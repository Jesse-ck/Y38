#include "spdif/hdmi_cec.h"
#include "spdif/hdmi_cec_api.h"
#include "app_config.h"
#include "asm/clock.h"
#include "app_config.h"
#include "gpio.h"
#include "system/timer.h"
#if ((TCFG_SPDIF_ENABLE) && (TCFG_HDMI_ARC_ENABLE))


/*declare variables*/
volatile u32 timer0_int_cnt = 0;//counter
volatile u8  CEC_State  = 0;
volatile u8  CEC_Stb_Ready  = 0;//Start bit is ready
volatile u8  CEC_Head  = 0;//receive header
volatile u8  CEC_Data  = 0;//receive data

volatile s8  CEC_EOM_H = -1; //End of message for header
volatile s8  CEC_EOM_D = -1;//End of message for data
volatile s8  CEC_ACK_H = -1; //ACK for header block
volatile s8  CEC_ACK_D = -1;//ACK for data block

volatile u8  initiator_device = 0;
volatile u8  Tx_Retry_Cnt = 0; //retry count

volatile u8 CEC_Head_Dest_flag = 0;  //cec header dest is correct
volatile u8 CEC_Broadcast_flag = 0; // 1= CEC broadcast Command ,0 = none
volatile u8 Tx_flag = 0;        // 1= Tx,0 = Rx
volatile u8 Tx_NACK_H = 0;     //Tx receive NACK For Header Block
volatile u8 Tx_NACK_D = 0;     //Tx receive NACK For Data Block
volatile u8 Rx_CEC_Data_flag = 0; //1= receive CEC data ,0 = none
volatile u8 Rx_CEC_Data_Len = 0;  //receive data length
volatile u8 RX_DATA[17] = {0}; //Receive data
volatile u8 TMR2_flag = 0;  // 1 = TMR2 ISR,0=No TMR2_ISR



u8 CEC_Version_cmd[3] = {0x00, CEC_VERSION, 0x04}; //0x04 :1.3a ????
u8 Report_Power_Status_Cmd[3] = {0x00, 0x90, 0x00}; //0x00:ON ; 0x01:OFF
u8 Standby_Cmd[2] = {0x00, STANDBY};

u8 PollingMessage_cmd[1] = {0x55};
u8 Report_Physical_Add_Cmd[7] = {0x5F, REPORT_PHYSICAL_ADDRESS, 0x20, 0x00, 0x05};

u8 Device_Vendor_Id_Cmd[5] = {0x5f, DEVICE_VENDOR_ID, 0x45, 0x58, 0x50};
u8 Set_System_Audio_Mode_cmd[] = {0x50, SET_SYSTEM_AUDIO_MODE, 0x01};
u8 Set_Osd_Name_cmd[] = {0x50, CECOP_SET_OSD_NAME, 0x41, 0x75, 0x64, 0x69, 0x6f, 0x20, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d}; //["Audio System"]

u8 System_Audio_Mode_Status_cmd[] = {0x50, CECOP_SYSTEM_AUDIO_MODE_STATUS, 0x01};
u8 Report_Audio_Status_cmd[] = {0x50, CECOP_REPORT_AUDIO_STATUS, 0x32};

u8 Initiate_Arc_cmd[2] = {0x50, INITIATE_ARC};
u8 Report_Short_Audio_Desc_cmd[8] = {0x50, REPORT_SHORT_AUDIO_DESCRIPTOR, 0x15, 0x17, 0x50, 0x3E, 0x06, 0xC0}; //AC-3 6Ch (48 44 32k) 640kbps/ DTS 7Ch (48 44K)  1536kbps

struct _CEC_MESSAGE {
    u8 cmd_data[17];
    u8 cmd_len;
    u8 cmd_status;
};
struct _CEC_MESSAGE s_cec_message;

typedef enum {
    ARC_STATUS_REQ_ADD,
    ARC_STATUS_WAIT_ONLINE,
    ARC_STATUS_REPORT_ADD,
    ARC_STATUS_STB,
    ARC_STATUS_INIT,
    ARC_STATUS_REQ_INIT,
    ARC_STATUS_INIT_SUCCESS,
    ARC_STATUS_TERMINATE,
    ARC_STATUS_REQ_TERMINATE,
    ARC_STATUS_REPORT_TERMINATE,
} ARC_STATUS;

struct HDMI_ARC_HDL {
    u8  arc_statu;
    u8  decode_mark;
    u16 time_prd;
    u16 code_var;
    u8  bit_index;
    u8  edge_type;
    volatile u8 recv_ok;
    u16 timer_hdl;
};
struct HDMI_ARC_HDL arc_handler;

#define __this  (&arc_handler)




#if 0
#define CEC_BUS_INIT()             do{JL_PORTB->PU &= ~BIT(3);JL_PORTB->PD &= ~BIT(3);JL_PORTB->DIR |=  BIT(3);}while(0)
#define CEC_BUS_OUT_PU()           do{JL_PORTB->PU |= BIT(3);JL_PORTB->PD &= ~BIT(3);}while(0)
#define CEC_BUS_OUT_LOW()	       do{JL_PORTB->PU &= ~BIT(3);JL_PORTB->PD |= BIT(3);JL_PORTB->DIR &=  ~BIT(3);JL_PORTB->OUT &=~BIT(3); }while(0)
#define CEC_BUS_OUT_HIGH()	       do{JL_PORTB->PD &= ~BIT(3);JL_PORTB->PU |= BIT(3);JL_PORTB->DIR &=  ~BIT(3);JL_PORTB->OUT |=BIT(3); }while(0)
#define CEC_BUS_OUT_PD()	       do{JL_PORTB->PU &= ~BIT(3);JL_PORTB->PD |= BIT(3);}while(0)
#define CEC_BUS_STATUS()           ((JL_PORTB->IN&BIT(3))? 1:0)
#else
#define CEC_BUS_INIT()            do{gpio_set_pull_up(TCFG_HDMI_CEC_PORT,0);gpio_set_pull_down(TCFG_HDMI_CEC_PORT,0);gpio_direction_input(TCFG_HDMI_CEC_PORT);}while(0)
#define CEC_BUS_OUT_PU()          //do{JL_PORTB->PU |= BIT(3);JL_PORTB->PD &= ~BIT(3);}while(0)
#define CEC_BUS_OUT_LOW()	      do{gpio_set_pull_up(TCFG_HDMI_CEC_PORT,0);gpio_set_pull_down(TCFG_HDMI_CEC_PORT,1);gpio_direction_output(TCFG_HDMI_CEC_PORT,0); }while(0)
#define CEC_BUS_OUT_HIGH()	      do{gpio_set_pull_down(TCFG_HDMI_CEC_PORT,0);gpio_set_pull_up(TCFG_HDMI_CEC_PORT,1);gpio_direction_output(TCFG_HDMI_CEC_PORT,1); }while(0)
#define CEC_BUS_OUT_PD()	       //
#define CEC_BUS_STATUS()          (gpio_read(TCFG_HDMI_CEC_PORT)?1:0)
#endif

volatile u8 time2_capture_mode = 0;

static u32 timer2_pad;

static s8 send_cec_command(u8 *data, u8 byte_len);
void decode_CEC_command(void);

/******
 *需要用到2个定时器
 *1个用于输出 时序控制
 *1个用于接收 capture
 *********/
/*****************************/
// CEC_PIN   JL_PORTB_3

#define MAX_TIME_CNT 0x7fff
#define MIN_TIME_CNT 0x0100
/* #define MIN_TIME_CNT1 0x0010 */

enum {
    TIMER_CLK_SRC_SYSCLK          = 0,
    TIMER_CLK_SRC_IOSIGN,
    TIMER_CLK_SRC_OSC,
    TIMER_CLK_SRC_RC,
};


static const u16 timer_div[] = {
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

/*************** T3 计时*************************/
___interrupt
/* AT_VOLATILE_RAM_CODE */
static void timer3_irq_handler(void)
{
    JL_TIMER3->CON |= BIT(14);
    timer0_int_cnt++;
}

static void timer3_init(void)
{
    u32 prd_cnt, clk, tmp_tick;
    u8 index;
    u8 catch_flag = 0;

    JL_TIMER3->CON = 0;
    printf("\n--func=%s\n", __FUNCTION__);
    /* clk = TIMER_CLK; */
    clk = clk_get("timer");
    clk /= 10000;
    clk *= 1; //100us
    for (index = 0; index < (sizeof(timer_div) / sizeof(timer_div[0])); index++) {
        prd_cnt = clk / timer_div[index];
        if (prd_cnt > MIN_TIME_CNT && prd_cnt < MAX_TIME_CNT) {
            catch_flag = 1;
            break;
        }
    }

    if (catch_flag == 0) {
        puts("warning:timer_err\n");
        return;
    }
    timer0_int_cnt = 0;
    JL_TIMER3->CNT = 0;
    JL_TIMER3->PRD = prd_cnt - 1;
    JL_TIMER3->CON = BIT(3) | (index << 4);
    request_irq(IRQ_TIME3_IDX, 2, timer3_irq_handler, 0);
    /* JL_TIMER3->CON = (clk_src << 2) | (index << 4); */
}

void start_time3_cnt(void)
{
    JL_TIMER3->CON &= ~(0x03);
    timer0_int_cnt = 0;
    JL_TIMER3->CON |= BIT(0);
}

u32 get_timer3_cnt_value(void)
{
    JL_TIMER3->CON &= ~(0x03);
    return timer0_int_cnt;
}

void timer3_delay(u32 N)
{
    JL_TIMER3->CON &= ~(0x03);
    timer0_int_cnt = 0;
    JL_TIMER3->CON |= BIT(0);
    while (1) {
        if (timer0_int_cnt >= N) {
            break;
        }
    }
    JL_TIMER3->CON &= ~(0x03);
}

/********************T3 END***************************/

/*********************T2 CEC CAP ******************/
#if 10
static void enable_timer2_capture(u8 mark)
{
    //10 上升 11 下降
//    if(mark ==0)
    time2_capture_mode = mark;
    JL_TIMER2->CON &= ~(BIT(1) | BIT(0));//disable
    if (mark == 2) { //下降
        JL_TIMER2->CON |= (BIT(1) | BIT(0));
    }
    if (mark == 1) { //上升
        JL_TIMER2->CON |= BIT(1);
    }
}

static void disable_timer2_capture(void)
{
    JL_TIMER2->CON &= ~(BIT(1) | BIT(0));
}

___interrupt
static void timer2_irq_handler(void)
{
    u16 TCAP2_tmp = 0;
    u32 CAP2_C = 0;
    JL_TIMER2->CON |= BIT(14);
    TMR2_flag = 1;
    JL_TIMER2->CNT = 0;
#if 0  //for test
    TCAP2_tmp = JL_TIMER2->PRD;

    CAP2_C = ((TCAP2_tmp + 0xffff) / timer2_pad);

    printf("\n TCAP %d;%d %d \n", TCAP2_tmp, time2_capture_mode, CAP2_C);
    if (time2_capture_mode == 2) {
        enable_timer2_capture(1);
    } else {
        enable_timer2_capture(2);
    }
    return;
#endif


    //下降触发 CEC Bus is Low
    if (time2_capture_mode == 2) {
        // printf("%d",CEC_BUS_STATUS());
        //sys_global_value.t3_cnt1++;
        if (((CEC_State == 10) || (CEC_State == 20)) && (CEC_Stb_Ready == 1)) {
            if (CEC_State == 10) {
                RX_DATA[Rx_CEC_Data_Len] = CEC_Head;
            } else {
                RX_DATA[Rx_CEC_Data_Len] = CEC_Data;
            }
            Rx_CEC_Data_Len++;

            //Rx
            if (Tx_flag == 0) {
                if (CEC_Head_Dest_flag == 1) {
                    /*Set T2 at falling-edge interrupt and to reset counter */
                    enable_timer2_capture(0);
                    /*ACK：CEC Bus output low 1.5ms */
#if 10
                    CEC_BUS_OUT_LOW();
                    timer3_delay(CEC_BIT_0_L);
                    //CEC_BUS_OUT_HIGH();
                    CEC_BUS_INIT();
#endif // 0

                    enable_timer2_capture(2);
                }

                //Next Data
                if (((CEC_State == 10) && (CEC_EOM_H == 0)) || ((CEC_State == 20) && (CEC_EOM_D == 0))) {
                    CEC_State = 11;
                } else { // The end
                    // CEC Bus output High
//                    printf("\n\n\n hdmi receive data: %d\n\n",Rx_CEC_Data_Len);
                    if (CEC_Head_Dest_flag == 1) {
                        if (__this->arc_statu >= ARC_STATUS_STB) {
                            if ((s_cec_message.cmd_status == 0) && (Rx_CEC_Data_Len > 1)) {
                                //  printf_buf((u8*)RX_DATA,Rx_CEC_Data_Len);
                                memcpy(&s_cec_message.cmd_data[0], (u8 *)RX_DATA, Rx_CEC_Data_Len);
//                              s_cec_message.cmd_data[1] = RX_DATA[1];
//                              s_cec_message.cmd_data[0] = RX_DATA[0];
                                //  printf_buf(&s_cec_message.cmd_data[0],Rx_CEC_Data_Len);
                                s_cec_message.cmd_len = Rx_CEC_Data_Len;
                                s_cec_message.cmd_status = 1;
                                /* hdmi_cec_decode_irq_resume(); */
                            }
                        }
                    } else {

                        //  printf("\n\n\n hdmi receive data: %d\n\n",Rx_CEC_Data_Len);
                    }
                    CEC_BUS_INIT();
                    CEC_State = 0;
                    CEC_Stb_Ready = 0;
                    Rx_CEC_Data_flag = 1;
                    // Rx_CEC_Data_Len = 0;
                }
            } else {
                //Tx
                /*Set T2 at rising-edge interrupt and to capture counter */
                enable_timer2_capture(1);
            }
        } else { // The Other non-Ack Fields
            /*Set T2 at rising-edge interrupt and to capture counter */
            enable_timer2_capture(1);

        }
    }
    //上升触发 CEC Bus is High
    else if (time2_capture_mode == 1) {

        TCAP2_tmp = JL_TIMER2->PRD;
        JL_TIMER2->CNT = 0;

        TCAP2_tmp = TCAP2_tmp * 1000 / timer2_pad;
        /*Set T2 at falling-edge interrupt and to reset counter */
        enable_timer2_capture(2);

        //Start Bit
        if (CEC_State == 0) {
//            puts("H");
            CEC_State++;
            if (((TCAP2_tmp + 27000) >= STB_MIN) && ((TCAP2_tmp + 27000) <= STB_MAX)) { //Start bit
                CEC_Stb_Ready = 1; //Set Start Bit Ready
                CEC_Head = 0;
                CEC_EOM_H = -1;
                CEC_ACK_H = -1;
                CEC_EOM_D = -1;
                CEC_ACK_D = -1;
                CEC_Head_Dest_flag = 0;
                Rx_CEC_Data_flag = 0;
                Rx_CEC_Data_Len = 0;
            } else {
                // printf("\n C:%d,%d,%d \n",TCAP2_tmp+CAP2_C,STB_MIN,STB_MAX);
                CEC_State = 0;
                CEC_Stb_Ready = 0;
            }
        }
        //header block
        else if ((CEC_State >= 1 && CEC_State < 9) && (CEC_Stb_Ready == 1)) {
//            puts("h");
            CEC_State++;
            CEC_Head = CEC_Head << 1;
            if ((TCAP2_tmp >= BIT_1_MIN) && (TCAP2_tmp <= BIT_1_MAX)) { //1
                CEC_Head |= 0x01;
            } else if ((TCAP2_tmp >= BIT_0_MIN) && (TCAP2_tmp <= BIT_0_MAX)) { //0
                CEC_Head &= 0xFE;
            } else {
                CEC_State = 0;
                CEC_Stb_Ready = 0;
            }
        }
        //End of Message for header block
        else if ((CEC_State == 9) && (CEC_Stb_Ready == 1)) {
//            puts("D");
            CEC_State++;
            // Rx
            if (Tx_flag == 0) {
                /*check the destination whether is belong to this device */
                if ((CEC_Head & 0x0F) == CEC_HEAD_DEST) {
                    CEC_Head_Dest_flag = 1;
                    //  puts("\n----\n");
                }
            }

            if ((TCAP2_tmp >= BIT_1_MIN) && (TCAP2_tmp <= BIT_1_MAX)) { //1
                CEC_EOM_H = 1;
            } else if ((TCAP2_tmp >= BIT_0_MIN) && (TCAP2_tmp <= BIT_0_MAX)) { //0
                CEC_EOM_H = 0;
            } else {
                CEC_State = 0;
                CEC_Stb_Ready = 0;
            }
        }
        //ACK for Header block
        else if ((CEC_State == 10) && (CEC_Stb_Ready == 1)) {
            //Tx
            if (Tx_flag == 1) {
                if ((TCAP2_tmp >= BIT_1_MIN) && (TCAP2_tmp <= BIT_1_MAX)) { //1
                    CEC_ACK_H = 1;
                } else if ((TCAP2_tmp >= BIT_0_MIN) && (TCAP2_tmp <= BIT_0_MAX)) { //0
                    CEC_ACK_H = 0;
                }
                /*receive not ACK for Header block*/
                if (CEC_ACK_H != 0) {
                    // CEC Bus output High
                    CEC_State = 0;
                    CEC_Stb_Ready = 0;
                    Tx_NACK_H = 0;
                } else {
                    CEC_State = 11;
                }
            }
        }
        // Data Block
        else if (((CEC_State >= 11) && (CEC_State <= 19)) && (CEC_Stb_Ready == 1)) {
            CEC_State++;
            CEC_Data = CEC_Data << 1;
            if ((TCAP2_tmp >= BIT_1_MIN) && (TCAP2_tmp <= BIT_1_MAX)) { //1
                CEC_Data |= 0x01;
            } else if ((TCAP2_tmp >= BIT_0_MIN) && (TCAP2_tmp <= BIT_0_MAX)) { //0
                CEC_Data &= 0xFE;
            } else {
                CEC_State = 0;
                CEC_Stb_Ready = 0;
            }
        }
        //End of Message for data block
        else if ((CEC_State == 19) && (CEC_Stb_Ready == 1)) {
            CEC_State++;
            if ((TCAP2_tmp >= BIT_1_MIN) && (TCAP2_tmp <= BIT_1_MAX)) { //1
                CEC_EOM_D = 1;
            } else if ((TCAP2_tmp >= BIT_0_MIN) && (TCAP2_tmp <= BIT_0_MAX)) { //0
                CEC_EOM_D = 0;
            } else {
                CEC_State = 0;
                CEC_Stb_Ready = 0;
            }
        }
        //ACK for data block
        else if ((CEC_State == 20) && (CEC_Stb_Ready == 1)) {
            // Tx
            if (Tx_flag == 1) {
                if ((TCAP2_tmp >= BIT_1_MIN) && (TCAP2_tmp <= BIT_1_MAX)) { //1
                    CEC_ACK_D = 1;
                } else if ((TCAP2_tmp >= BIT_0_MIN) && (TCAP2_tmp <= BIT_0_MAX)) { //0
                    CEC_ACK_D = 0;
                }
                /*receive not ACK for Data block*/
                if (CEC_ACK_D != 0) {
                    // CEC Bus output High
                    CEC_State = 0;
                    CEC_Stb_Ready = 0;
                    Tx_NACK_D = 0;
                } else {
                    CEC_State = 11;
                }
            }
        }
    }
}


static void timer2_capture_init(void)
{
    JL_TIMER2->CON = 0;
    u32 clk;
    u32 prd_cnt;
    u8 index;

    clk = clk_get("timer");
    clk /= 10000;
    clk *= 1; //100us for cnt

    for (index = 0; index < (sizeof(timer_div) / sizeof(timer_div[0])); index++) {
        prd_cnt = (clk + timer_div[index]) / timer_div[index];
        if (prd_cnt > MIN_TIME_CNT && prd_cnt < MAX_TIME_CNT) {
            break;
        }
    }
    timer2_pad = prd_cnt;
    printf("\n\n\n\n prd_cnt %x \n", prd_cnt);
    JL_TIMER2->CON = ((index << 4) | BIT(3));
    JL_TIMER2->CNT = 0;
    JL_TIMER2->PRD = 0;

    request_irq(IRQ_TIME2_IDX, 2, timer2_irq_handler, 0);
    // 0:PA15;1 input channel 2
#if 0
    JL_IOMAP->CON1 &= ~BIT(30);
#else
    //input channel 需另外配 //
    JL_IOMAP->CON1 |= BIT(30);
    JL_IOMAP->CON2 &= ~(0x7F << 16);
    printf("TCFG_HDMI_CEC_PORT:[%d]", TCFG_HDMI_CEC_PORT);
    JL_IOMAP->CON2 |= (TCFG_HDMI_CEC_PORT << 16);
#endif
    gpio_set_die(TCFG_HDMI_CEC_PORT, 1);
}



void start_cec_receive(void)
{
    Tx_flag = 0;
    enable_timer2_capture(1);
}
void stop_cec_receive(void)
{
    Tx_flag = 1;
    enable_timer2_capture(0);
}


void hdmi_cec_decode_irq_loop()
{
#if 0
    //for test
    CEC_BUS_INIT();
    u8 temp = CEC_BUS_STATUS();
    printf("CEC STATUS %d", temp);
    return;
#endif
    if (s_cec_message.cmd_status) {
        decode_CEC_command();
        s_cec_message.cmd_status = 0;
        return;
    }
    /* printf("temp [%d]\n",__this->arc_statu); */
    static int init_cnt = 0;
    switch (__this->arc_statu) {
    case ARC_STATUS_REPORT_ADD:
        putchar('A');
        if (send_cec_command(Report_Physical_Add_Cmd, 5) == 0) {
            init_cnt++;
            if (init_cnt >= 2) {
                init_cnt = 0;
                __this->arc_statu = ARC_STATUS_WAIT_ONLINE;
            }
        }
        putchar('a');
        break;
    case ARC_STATUS_WAIT_ONLINE:
        putchar('O');
        if (send_cec_command(Device_Vendor_Id_Cmd, 5) == 0) {
            init_cnt++;
            if (init_cnt >= 2) {
                init_cnt = 0;
                __this->arc_statu = ARC_STATUS_STB;
            }
        }
        putchar('o');
        break;
    case ARC_STATUS_REQ_ADD:
        putchar('G');
        init_cnt++;
//           if(init_cnt<2)
//                break;
        if (send_cec_command(PollingMessage_cmd, 1) == 0) {
            //init_cnt++;
            if (init_cnt > 2) {
                __this->arc_statu = ARC_STATUS_REPORT_ADD;
                init_cnt = 0;
            }
        } else {
            init_cnt = 0;
        }
        putchar('g');
        break;
    case ARC_STATUS_STB:

        if (init_cnt % 4 == 0) {
            if (send_cec_command(Set_System_Audio_Mode_cmd, 3) == 0) {
                init_cnt += 3;
                if (init_cnt > 8) {
                    init_cnt = 0;
                    __this->arc_statu = ARC_STATUS_INIT;
                }
            } else {
                init_cnt = 0;
            }
        }
        init_cnt++;
        //    puts("\n ARC_STATUS_STB\n");
        break;
    case ARC_STATUS_INIT:
        if ((init_cnt % 12) == 0) {
            if (send_cec_command(Initiate_Arc_cmd, 2) == 0) {
            }
        }
        init_cnt++;
        break;
    case ARC_STATUS_INIT_SUCCESS:
        break;
    case ARC_STATUS_REQ_INIT:
        break;
    default:
        break;
    }

    if (__this->decode_mark) {
        __this->decode_mark--;
    }
}


void hdmi_cec_init(void)
{
    timer3_init();
    memset(&s_cec_message, 0, sizeof(struct _CEC_MESSAGE));
    CEC_BUS_INIT();
    timer2_capture_init();
    /* start_cec_receive(); */
    puts("\n hdmi_cec_init \n");
    __this->arc_statu = ARC_STATUS_REQ_ADD;
    __this->decode_mark = 0;

    __this->timer_hdl = sys_timer_add(NULL, hdmi_cec_decode_irq_loop, 500);
}

void hdmi_cec_close(void)
{
    if (__this->timer_hdl) {
        sys_timer_del(__this->timer_hdl);
        stop_cec_receive();
        get_timer3_cnt_value();
        __this->timer_hdl = 0;
    }
}
#if 0
void hdmi_cec_decode_irq_resume()
{
    if (__this->decode_mark == 0) {
        //__this->decode_mark = 1;
        // if(__this->arc_statu != ARC_STATUS_INIT_SUCCESS)
        /* irq_set_pending(IRQ_HDMI_CEC_IDX); */

    }

}
#endif

u8 _ACK_read(void)
{
    return 0;
}

static s8 _send_cec(u8 *data, u8 byte_len)
{
    u32 time_cnt_10us;
    u8 nbyte;
    s8 nbit;
    Tx_flag = 1;   // 1=transmit
    Tx_NACK_H = 0; //clear Tx_NACK_H
    Tx_NACK_D = 0; //clear Tx_NACK_D
    stop_cec_receive();
    CEC_Broadcast_flag = ((data[0] & 0x0f) == 0x0f);
    if ((data[0] & 0x0f) == CEC_HEAD_DEST) {
        CEC_Broadcast_flag = 1;
    }
    //make sure CEC BUS to release Hight
    /* puts("\n _send_cec \n"); */
    CEC_BUS_INIT();
    printf("\n CEC_BUS_STATUS():%d \n", CEC_BUS_STATUS());
    /* while ((CEC_BUS_STATUS()) == 0); */
    if ((CEC_BUS_STATUS()) == 0) {
        return -1;
    }
    // printf("\n CEC_BUS_STATUS():%d \n",CEC_BUS_STATUS());
    /* puts("\n _send_cec 2\n"); */
    //check CEC BUS = hight that over free time >=5*2.4ms
    TMR2_flag = 0;

    //delay free time >= 5*2.4 ms
    timer3_delay(CEC_FREE_TIME_NI);
    if (TMR2_flag != 0) {
        printf("\n detect CEC Bus is low !!\n");
        Tx_flag = 0;
        timer3_delay(CEC_FREE_TIME_RS);
        return -1;
    }

    // Send out start bit
    CEC_BUS_OUT_LOW();
    timer3_delay(CEC_START_BIT_L);
    // CEC_BUS_OUT_HIGH();
    CEC_BUS_INIT();
#if 0
    if ((CEC_BUS_STATUS()) == 0) {
        printf("\n CEC_BUS_STATUS():%d \n", CEC_BUS_STATUS());
        printf("\n arbitration loss at start bit !!\n");
        Tx_flag = 0;
        timer3_delay(CEC_FREE_TIME_RS);
        return -1;
    }
#endif
    timer3_delay(CEC_START_BIT_H);


    /*send out Header Code and Data Code */
    for (nbyte = 0; nbyte < byte_len; nbyte++) {
        for (nbit = 7; nbit >= 0; nbit--) {
            if (data[nbyte] & (1 << nbit)) {
                CEC_BUS_OUT_LOW();
                timer3_delay(CEC_BIT_1_L);
                CEC_BUS_OUT_HIGH();
                timer3_delay(CEC_BIT_1_H);
            } else {
                CEC_BUS_OUT_LOW();
                timer3_delay(CEC_BIT_0_L);
                CEC_BUS_OUT_HIGH();
                timer3_delay(CEC_BIT_0_H);
            }
#if 0
            if ((nbyte == 0) && (nbit >= 4)) {
                if (((data[0] >> nbit) & 0x01) != (CEC_Head & 0x01)) {
                    printf("\n arbitration loss at initiator bit !!\n");
                    Tx_flag = 0;
                    timer3_delay(CEC_FREE_TIME_RS);
                    return -1;
                }
            }
#endif
        }
        /*send out EOM Bit*/
        if (nbyte == byte_len - 1) {
            CEC_BUS_OUT_LOW();
            timer3_delay(CEC_BIT_1_L);
            CEC_BUS_OUT_HIGH();
            timer3_delay(CEC_BIT_1_H);
        } else {
            CEC_BUS_OUT_LOW();
            timer3_delay(CEC_BIT_0_L);
            CEC_BUS_OUT_HIGH();
            timer3_delay(CEC_BIT_0_H);
        }
        /*send out ACK Bit*/
        CEC_BUS_OUT_LOW();
        timer3_delay(CEC_BIT_1_L);
        // CEC_BUS_OUT_HIGH();
        start_time3_cnt();
        //  timer3_delay(CEC_BIT_1_H);
        CEC_BUS_INIT();

        /*check ACK for Header block*/
        while ((CEC_BUS_STATUS()) == 0);
        time_cnt_10us = get_timer3_cnt_value();

        //  printf("time_cnt_10us:%d \n",time_cnt_10us);
        if (time_cnt_10us > 6 && time_cnt_10us < 12) { //0
            if (nbyte == 0) {
                Tx_NACK_H = 0;
            } else {
                Tx_NACK_D = 0;
            }
            timer3_delay(CEC_BIT_0_H);
        } else { //1
            timer3_delay(CEC_BIT_1_H);
            if (nbyte == 0) {
                Tx_NACK_H = 1;
            } else {
                Tx_NACK_D = 1;
            }
        }


#if 10
        if (nbyte == 0 && Tx_NACK_H == 1) {
            if (CEC_Broadcast_flag == 0) {
                // printf("\n No Found destination Device %x !!!\n",data[0]);
                Tx_flag = 0;
                timer3_delay(CEC_FREE_TIME_RS);
                return -1;
            }
        } else if (nbyte != 0 && Tx_NACK_D == 1) {
            if (CEC_Broadcast_flag == 0) {
                // printf("\n No ACK for Data Block !!! \n");
                Tx_flag = 0;
                timer3_delay(CEC_FREE_TIME_RS);
                return -1;

            }
        }
#endif // 0
    }
    //turn back as a follower
    Tx_flag = 0;
    //delay free time
    timer3_delay(CEC_FREE_TIME_PI);
    start_cec_receive();
    return 0;
}

static s8 send_cec_command(u8 *data, u8 byte_len)
{
    u8 i;
    for (Tx_Retry_Cnt = 0; Tx_Retry_Cnt < CEC_MAX_RETRY; Tx_Retry_Cnt++) {
        if (_send_cec(data, byte_len) == 0) {
            /* printf("\n transmitted CEC Data \n"); */
            //            for(i=0;i<byte_len;i++){
            //                printf("%02x ",data[i]);
            //            }
            // break;
            CEC_BUS_INIT();
            return 0;
        } else {
            start_cec_receive();
            /* printf("\n Re-transmitted <%d> \n",Tx_Retry_Cnt); */
        }
    }
    CEC_BUS_INIT();
    /* printf("CEC Command OUT"); */
    return -1;
}

void decode_CEC_command(void)
{
    //  initiator_device = (RX_DATA[0]&0xf0)>>4;
    initiator_device = (s_cec_message.cmd_data[0] & 0xf0) >> 4;
    printf("\n decode cmd:%x\n", s_cec_message.cmd_data[1]);
    switch (s_cec_message.cmd_data[1]) {
    case CECOP_FEATURE_ABORT:
        //        if(__this->arc_statu <= ARC_STATUS_STB){
        if (__this->arc_statu <= ARC_STATUS_INIT) {
            printf("cmd len%d \n", s_cec_message.cmd_len);
            __this->arc_statu = ARC_STATUS_REQ_ADD;
        }
        break;
    case CECOP_GIVE_PHYSICAL_ADDRESS:
        send_cec_command(Report_Physical_Add_Cmd, 5);
        break;
    case CECOP_GIVE_DEVICE_VENDOR_ID:
        send_cec_command(Device_Vendor_Id_Cmd, 5);
        break;
    case GET_CEC_VERSION:
        CEC_Version_cmd[0] = initiator_device;
        send_cec_command(CEC_Version_cmd, 3);
        break;
    case CECOP_ARC_REPORT_INITIATED:
        printf("\n CECOP_ARC_REPORT_INITIATED \n");
        break;
    case CECOP_SET_STREAM_PATH:
        break;
    //case 0x48: //??
    case CECOP_GIVE_OSD_NAME:
        send_cec_command(Set_Osd_Name_cmd, sizeof(Set_Osd_Name_cmd) / sizeof(Set_Osd_Name_cmd[0]));
        break;
    case CECOP_REPORT_SHORT_AUDIO:
        send_cec_command(Report_Short_Audio_Desc_cmd, sizeof(Report_Short_Audio_Desc_cmd) / sizeof(Report_Short_Audio_Desc_cmd[0]));
        break;
    case CECOP_SYSTEM_AUDIO_MODE_REQUEST:// 70 72
        send_cec_command(Set_System_Audio_Mode_cmd, sizeof(Set_System_Audio_Mode_cmd) / sizeof(Set_System_Audio_Mode_cmd[0]));
        break;
    case CECOP_GIVE_SYSTEM_AUDIO_MODE_STATUS:
        send_cec_command(System_Audio_Mode_Status_cmd, sizeof(System_Audio_Mode_Status_cmd) / sizeof(System_Audio_Mode_Status_cmd[0]));
        break;
    default:
        //  printf("\n CEC command <%x> unknow len:%d\n",s_cec_message.cmd_data[1],s_cec_message.cmd_len);
        break;
    }
}






#endif
#endif







