#include "gpio.h"
#include "app_config.h"
#include "system/includes.h"
#include "device/wireless_charge.h"
#include "charge_box/charge_ctrl.h"
#include "asm/adc_api.h"
#include "charge_box/charge_box.h"
#include "device/chargebox.h"

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHG_WL]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

extern void delay_2ms(int cnt);
static void irq_disable_backup();
static void irq_enable_revert();

#if(defined WIRELESS_ENABLE) && (WIRELESS_ENABLE)

_wireless_hdl wl_hdl;
#define __this (&wl_hdl)

/* volatile struct _wireless_info  info; */
static u16 wl_send_buff[10] __attribute__((aligned(4))); //10 * =20byte

//控制充电口电压范围
#define VOLTAGE_MIN     6100
#define VOLTAGE_MAX     6200

//取中间 (AD_OK_COUNTS-AD_CUT_COUNTS*2)计算均值（去掉头部与尾部）
#define AD_OK_COUNTS    5
#define AD_CUT_COUNTS   1

//dcdc en IO
#define DCDC_EN_IO          IO_PORTB_09
#define DCDC_CTRL_EN        1            //是否使用dcdc 控制

//wpc io
#define WPC_IO              IO_PORTB_10

//ad 选择
#define WL_AD_DET_IO        IO_PORTB_08
#define WL_AD_DET_CH        AD_CH_PB8

//无线充上线检测
#define WL_ONLINE_VOLT     3500  ///电压大于3.5才认为是有充电器放上来
#define WL_ONLINE_TIMES    12
#define WL_OFFLINE_TIMES   15

static u16  power_table[AD_OK_COUNTS];
static u16  power_table_tmp[AD_OK_COUNTS];
static u8  power_cnt = 0;
static volatile u8 data_ok = 0;
static volatile u8 timer_100ms = 0;

static u16 get_wireless_voltage(void);
void wl_timer2_open(void);
static void wl_timer2_close(void);

///dcdc en IO初始化
static void dcdc_io_init(void)
{
#if DCDC_CTRL_EN
    gpio_set_die(DCDC_EN_IO, 0);
    gpio_set_pull_down(DCDC_EN_IO, 0);
    gpio_set_pull_up(DCDC_EN_IO, 0);
    gpio_direction_output(DCDC_EN_IO, 0);
#endif
}

static void dcdc_set_en(u8 en)
{
#if DCDC_CTRL_EN
    if (en) {
        gpio_direction_output(DCDC_EN_IO, 1);
    } else {
        gpio_direction_output(DCDC_EN_IO, 0);
    }
#endif
}
//
static void wpc_io_init(void)
{
    gpio_set_die(WPC_IO, 1);
    gpio_set_pull_down(WPC_IO, 0);
    gpio_set_pull_up(WPC_IO, 0);
    gpio_direction_output(WPC_IO, 0);
}


//10ms
void wireless_10ms_scan(void)
{
    u16 power_tmp;
    if ((!__this->info.open) || __this->info.busy) {
        ///不在线或发数据中不动作
        timer_100ms = 0;
        return;
    }
    power_tmp = get_wireless_voltage();
    power_table[power_cnt++] = power_tmp;
    if (power_cnt >= AD_OK_COUNTS) {
        power_cnt = 0;
        data_ok = 1;
    }

    timer_100ms++;
    if (timer_100ms % 10 == 0) {
        timer_100ms = 0;
        app_chargebox_event_to_user(CHGBOX_EVENT_WL_100MS);
        /* put_event(EVENT_100MS); */
    }
}

void wireless_100ms_run_app(void)
{
    wl_timer2_open();
    wireless_100ms_run();
}

u16 get_wireless_power(void)
{
    u8 i, j, k;
    u16 ad_sum, ad_min;
    while (!data_ok);
    memcpy(power_table_tmp, power_table, sizeof(power_table));
    for (i = 1; i < AD_OK_COUNTS; i++) {
        for (j = i; j > 0; j--) {
            if (power_table_tmp[j] < power_table_tmp[j - 1]) {
                ad_min = power_table_tmp[j];
                power_table_tmp[j] = power_table_tmp[j - 1];
                power_table_tmp[j - 1] = ad_min;
            }
        }
    }
    ad_sum = 0;
    for (k = AD_CUT_COUNTS; k < (AD_OK_COUNTS - AD_CUT_COUNTS); k++) {
        ad_sum = power_table_tmp[k] + ad_sum;
    }
    ad_sum = (ad_sum / (AD_OK_COUNTS - (AD_CUT_COUNTS * 2)));
    return ad_sum;
    //去掉去掉最大与最小 AD_CUT_COUNTS*2 个
}

SEC(.chargebox_code)
void wpc_io_set(u8 mode)
{
    u8 io_num;
    switch (mode) {
    case IO_HIGH:
        gpio_direction_output(WPC_IO, 1);
        break;
    case IO_LOW:
        gpio_direction_output(WPC_IO, 0);
        wl_timer2_close(); //最后一个动作后关闭
        break;
    case IO_INIT:
        gpio_set_die(WPC_IO, 1);
        gpio_set_pull_down(WPC_IO, 0);
        gpio_set_pull_up(WPC_IO, 0);
        gpio_direction_output(WPC_IO, 0);
        break;
    case IO_OVERTURN://翻转
        io_num = WPC_IO % 16;
#if(WPC_IO <= IO_PORTA_15)
        JL_PORTA->OUT ^= BIT(io_num);
#elif(WPC_IO <= IO_PORTB_15)
        JL_PORTB->OUT ^= BIT(io_num);
#elif(WPC_IO <= IO_PORTC_15)
        JL_PORTC->OUT ^= BIT(io_num);
#elif(WPC_IO <= IO_PORTD_7)
        JL_PORTD->OUT ^= BIT(io_num);
#endif
        break;
    case IO_DIR_IN: //高阻
        gpio_direction_input(WPC_IO);
        break;
    }
}

//检测到无线充电上线时调用
void wireless_api_open(void)
{
    wl_timer2_open();            //初始化好timer
    irq_disable_backup();             //关其他中断
    irq_enable(IRQ_TIME2_IDX);   //开本中断

    power_cnt = 0;
    data_ok = 0;
    wireless_open(VOLTAGE_MIN, VOLTAGE_MAX);
    //发送信号强度包
    get_signal_value();
    __this->info.busy = 1;
    while (__this->info.busy) {
        asm("nop");
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) {
            wireless_close();
            irq_enable_revert();
            return;
        }
    }
    irq_enable_revert();//恢复中断（注意只打开原来开着的中断）


    //发送身份包
    delay_2ms(3);
    get_identification();
    wl_timer2_open();
    irq_disable_backup();
    irq_enable(IRQ_TIME2_IDX);
    __this->info.busy = 1;
    while (__this->info.busy) {
        asm("nop");
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) {
            wireless_close();
            irq_enable_revert();
            return;
        }
    }
    irq_enable_revert();

    //发送配置包
    delay_2ms(3);
    get_configuration();
    wl_timer2_open();
    irq_disable_backup();
    irq_enable(IRQ_TIME2_IDX);
    __this->info.busy = 1;
    while (__this->info.busy) {
        asm("nop");
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) {
            wireless_close();
            irq_enable_revert();
            return;
        }
    }
    irq_enable_revert();
    timer_100ms = 0;
    /* clear_one_event(EVENT_100MS); */
}

//检测到无线充电掉线是调用
void wireless_api_close(void)
{
    wireless_close();
    data_ok = 0;
    power_cnt = 0;
    dcdc_set_en(0);
}


static const u32 timer_div[] = {
    /*0000*/    1,
    /*0001*/    4,
    /*0010*/    16,
    /*0011*/    64,
    /*0100*/    2,
    /*0101*/    8,
    /*0110*/    32,
    /*0111*/    128,
    /*1000*/    256,
    /*1001*/    4 * 256,
    /*1010*/    16 * 256,
    /*1011*/    64 * 256,
    /*1100*/    2 * 256,
    /*1101*/    8 * 256,
    /*1110*/    32 * 256,
    /*1111*/    128 * 256,
};

#define MAX_TIME_CNT            0x7fff
#define MIN_TIME_CNT            0x100
#define WL_TIMER_UNIT_US        250  //单位us


SEC(.chargebox_code)
___interrupt
static void wl_timer2_isr(void)
{
    JL_TIMER2->CON |= BIT(14);
    wireless_250us_run();
}

void wl_timer2_open(void)
{
    u32 prd_cnt;
    u8 index;

    JL_TIMER2->CON = BIT(14);//清pending
    for (index = 0; index < (sizeof(timer_div) / sizeof(timer_div[0])); index++) {
        prd_cnt = WL_TIMER_UNIT_US * (clk_get("timer") / 1000000) / timer_div[index];
        if (prd_cnt > MIN_TIME_CNT && prd_cnt < MAX_TIME_CNT) {
            break;
        }
    }

    JL_TIMER2->CNT = 0;
    JL_TIMER2->PRD = prd_cnt;
    request_irq(IRQ_TIME2_IDX, 3, wl_timer2_isr, 0);
    /* JL_TIMER2->CON = (index << 4) | BIT(0);//osc lsb */
    /* JL_TIMER2->CON |= BIT(14);//清pending */
    JL_TIMER2->CON = (index << 4) | BIT(0) | BIT(3); //osc clk

    /* log_info("PRD : 0x%x / %d", JL_TIMER2->PRD, clk_get("timer")); */
}

static void wl_timer2_close(void)
{
    JL_TIMER2->CON = 0;//osc clk
}

static u16 get_wireless_voltage(void)
{
    u16 volt;
//注意分压,上下拉及外部路相关， 默认开下拉四分一
    volt = adc_get_voltage(WL_AD_DET_CH) * 4; //四分一分压
    return volt;
}

void wl_ad_det_init(void)
{

    adc_add_sample_ch(WL_AD_DET_CH);          //注意：初始化AD_KEY之前，先初始化ADC

    gpio_set_die(WL_AD_DET_IO, 0);
    gpio_set_direction(WL_AD_DET_IO, 1);
    gpio_set_pull_down(WL_AD_DET_IO, 1);
    gpio_set_pull_up(WL_AD_DET_IO, 0);
}



void wireless_online_det_2ms(void)
{
    static u16 wl_detect_cnt = 0;
    if (sys_info.status[WIRELESS_DET] == STATUS_ONLINE) {
        if (get_wireless_voltage() < WL_ONLINE_VOLT) {
            wl_detect_cnt++;
            sys_info.life_cnt = 0;
            if (wl_detect_cnt >= WL_OFFLINE_TIMES) {
                wl_detect_cnt = 0;
                sys_info.status[WIRELESS_DET] = STATUS_OFFLINE;
                app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_OFFLINE);
            }
        }
    } else {
        if (get_wireless_voltage() >= WL_ONLINE_VOLT) {
            wl_detect_cnt++;
            sys_info.life_cnt = 0;
            if (wl_detect_cnt >= WL_ONLINE_TIMES) {
                wl_detect_cnt = 0;
                sys_info.status[WIRELESS_DET] = STATUS_ONLINE;
                app_chargebox_event_to_user(CHGBOX_EVENT_WIRELESS_ONLINE);
            }
        }
    }
}

void wl_2ms_scan()
{
    static u16 cnt = 0;
    wireless_online_det_2ms();
    cnt++;
    if ((cnt % 5) == 0) { //10ms
        wireless_10ms_scan();
        cnt = 0;
    }
}



void chgbox_handshake_init();
void wireless_init_api(void)
{
    ///注意顺序不能改变
    memset(__this, 0x0, sizeof(_wireless_hdl));
    //AD采集初始化
    wl_ad_det_init();

    dcdc_io_init();
    wpc_io_init();
    __this->get_wl_power = get_wireless_power;
    __this->dcdc_en_set = dcdc_set_en;
    __this->wpc_set = wpc_io_set;
    __this->send_buff = wl_send_buff;
    wireless_lib_init(__this, VOLTAGE_MIN, VOLTAGE_MAX);
    //2msscan注册
    sys_s_hi_timer_add(NULL, wl_2ms_scan, 2);


}
#endif//end of WIRELESS_ENABLE


static u8 irq_ie_tab[10];
static void irq_disable_backup()
{
    u8 i;
    u8 x, y;
    local_irq_disable();
    memset(irq_ie_tab, 0, sizeof(irq_ie_tab) / sizeof(irq_ie_tab[0]));
    for (i = 0; i < MAX_IRQ_ENTRY_NUM; i++) {
        if (irq_read(i)) {
            x = i / 8;
            y = i % 8;
            irq_ie_tab[x] |= BIT(y);
        }
    }

    for (i = 0; i < MAX_IRQ_ENTRY_NUM; i++) {
        irq_disable(i);
    }
    local_irq_enable();
}

static void irq_enable_revert()
{
    u8 i;
    u8 x, y;
    local_irq_disable();
    for (i = 0; i < MAX_IRQ_ENTRY_NUM; i++) {
        x = i / 8;
        y = i % 8;
        if (irq_ie_tab[x] & BIT(y)) {
            irq_enable(i);
        }
    }
    local_irq_enable();
}

//关于handshake部分
//////////////////////////////////////////////////////////////////////////

#define CHGBOX_HANDSHAKE_IO  IO_PORTA_01



void chgbox_timer2_delay_us(u8 us);

struct _hs_hdl hs_ctrl = {
    .port = CHGBOX_HANDSHAKE_IO,
    .send_delay_us = chgbox_timer2_delay_us,
};

void chgbox_handshake_init(void)
{
    //部分封装与其他IO绑定在一起,注意设置成高阻
    gpio_direction_input(IO_PORTC_07);
    gpio_set_die(IO_PORTC_07, 0);
    gpio_set_pull_down(IO_PORTC_07, 0);
    gpio_set_pull_up(IO_PORTC_07, 0);

    handshake_ctrl_init(&hs_ctrl);
}

SEC(.chargebox_code)
void chgbox_timer2_delay_us(u8 us)
{
    ////delay 值要根据不同的频率去调试,本参数为48m参数
    switch (us) {
    case HS_DELAY_2US:
        JL_TIMER2->PRD = 19;//24*2;
        break;
    case HS_DELAY_3US:
        JL_TIMER2->PRD = 40;//24*3;
        break;
    case HS_DELAY_4US:
        JL_TIMER2->PRD = 62;//24*4;
        break;
    case HS_DELAY_7US:
        JL_TIMER2->PRD = 144;//24*7;
        break;
    case HS_DELAY_8US:
        JL_TIMER2->PRD = 158;//24*8;
        break;
    case HS_DELAY_14US:
        JL_TIMER2->PRD = 300;//24*14;
        break;
    case HS_DELAY_16US:
        JL_TIMER2->PRD = 355;//24*16;
        break;
    }
    JL_TIMER2->CON = BIT(0) | BIT(3) | BIT(14); //1分频,osc时钟,24m，24次就1us
    while (!(JL_TIMER2->CON & BIT(15))); //等pending
    JL_TIMER2->CON = 0;
}


void chgbox_handshake_run_app(void)
{
    delay_2ms(2);
    irq_disable_backup();
    handshake_send_app(HS_CMD0);
    irq_enable_revert();

    delay_2ms(2);

    irq_disable_backup();
    handshake_send_app(HS_CMD1);
    irq_enable_revert();

    delay_2ms(2);

    irq_disable_backup();
    handshake_send_app(HS_CMD2);
    irq_enable_revert();

    delay_2ms(2);

    irq_disable_backup();
    handshake_send_app(HS_CMD3);
    irq_enable_revert();
}

//多握手n次处理
#define  HS_REPEAT_MAX_TIMES  10
static u8 chgbox_hs_repeat_times = 0;
static u8 hs_repeat_cnt = 0;
void chgbox_handshake_set_repeat(u8 times)
{
    chgbox_hs_repeat_times = times;
    hs_repeat_cnt = 0;
}

void chgbox_handshake_repeatedly(void)
{
    if (chgbox_hs_repeat_times) {
        hs_repeat_cnt++;
        if (hs_repeat_cnt == HS_REPEAT_MAX_TIMES) {
            hs_repeat_cnt = 0;
            chgbox_hs_repeat_times--;
            chgbox_handshake_run_app();
        }
    }
}

#endif

