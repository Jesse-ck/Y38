#include "includes.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "btstack/avctp_user.h"

#if TCFG_UI_ENABLE
static void led7_show_wait(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" Lod");
    ui_dis_var->dis->lock(0);
}

static void led7_show_hi(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" HI");
    ui_dis_var->dis->lock(0);
}
static void led7_show_bt(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" bt");
    ui_dis_var->dis->lock(0);
}

static void led7_show_call(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" CAL");
    ui_dis_var->dis->lock(0);
}

#if TCFG_APP_FM_EMITTER_EN
static void led7_fm_ir_set_freq(void *hd, u16 freq)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    u8 bcd_number[5] = {0};	  ///<换算结果显示缓存

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    sprintf((char *)bcd_number, "%4d", freq);
    /* itoa4(freq,bcd_number); */
    if (freq > 1080) {
        ui_dis_var->dis->show_string((u8 *)" Err");
    } else if (freq >= 875) {
        ui_dis_var->dis->show_string(bcd_number);
        /* os_time_dly(100); */
        fm_emitter_manage_set_fre(freq);
        ui_menu_reflash(TRUE);//设置回主页
    } else {
        ui_dis_var->dis->FlashChar(BIT(0) | BIT(1) | BIT(2) | BIT(3)); //设置闪烁
        ui_dis_var->dis->show_string(bcd_number);
    }
    ui_dis_var->dis->lock(0);

}
#endif

static void led7_show_volume(void *hd, u8 vol)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_char(' ');
    ui_dis_var->dis->show_char('V');
    ui_dis_var->dis->show_number(vol / 10);
    ui_dis_var->dis->show_number(vol % 10);
    ui_dis_var->dis->lock(0);
}

static void led7_fm_show_freq(void *hd, void *private, u32 arg)
{
    u8 bcd_number[5] = {0};
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    u16 freq = 0;
    if (arg) {
        freq = arg;
        ui_dis_var->fmtx_freq = arg;
    } else {
        freq = ui_dis_var->fmtx_freq;
    }

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    itoa4(freq, (u8 *)bcd_number);
    if (freq > 999 && freq <= 1999) {
        bcd_number[0] = '1';
    } else {
        bcd_number[0] = ' ';
    }
    ui_dis_var->dis->show_string(bcd_number);
    ui_dis_var->dis->show_icon(LED7_DOT);
    ui_dis_var->dis->lock(0);
}

static void led7_fm_set_freq(void *hd, u32 arg)
{
    u8 bcd_number[5] = {0};
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    u16 freq = 0;

    if (arg) {
        freq = arg;
        ui_dis_var->fmtx_freq = arg;
    } else {
        freq = ui_dis_var->fmtx_freq;
    }


    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->FlashChar(BIT(0) | BIT(1) | BIT(2) | BIT(3)); //设置闪烁
    itoa4(freq, (u8 *)bcd_number);
    if (freq > 999 && freq <= 1999) {
        bcd_number[0] = '1';
    } else {
        bcd_number[0] = ' ';
    }
    ui_dis_var->dis->show_string(bcd_number);
    ui_dis_var->dis->show_icon(LED7_DOT);
    ui_dis_var->dis->lock(0);
}


void ui_common(void *hd, void *private, u8 menu, u32 arg)//公共显示
{
    u16 fre = 0;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;

    if (!hd) {
        return;
    }

    switch (menu) {
    case MENU_POWER_UP:
        led7_show_hi(hd);
        break;
    case MENU_MAIN_VOL:
        led7_show_volume(hd, arg & 0xff);
        break;
    case MENU_WAIT:
        led7_show_wait(hd);
        break;
    case MENU_BT:
        led7_show_bt(hd);
        break;
    case MENU_IR_FM_SET_FRE:
#if TCFG_APP_FM_EMITTER_EN
        led7_fm_ir_set_freq(hd, arg);
#endif
        break;
    case MENU_FM_SET_FRE:

#if TCFG_APP_FM_EMITTER_EN
        fre = fm_emitter_manage_get_fre();
        led7_fm_set_freq(hd, arg);
#endif
        break;
    default:
        break;
    }
}



static void *ui_open_bt(void)
{
    void *private = NULL;

    ui_set_auto_reflash(500);//设置主页500ms自动刷新
    return private;
}

static void ui_close_bt(void *hd, void *private)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return;
    }

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->lock(0);

    if (private) {
        free(private);
    }
}

static void ui_bt_main(void *hd, void *private) //主界面显示
{
    if (!hd) {
        return;
    }

#if TCFG_APP_FM_EMITTER_EN

    if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) {
        led7_show_call(hd);
    } else {
        u16 fre = fm_emitter_manage_get_fre();
        if (fre != 0) {
            led7_fm_show_freq(hd, private, fre);
        } else {
            led7_show_wait(hd);
        }
    }
#else
    if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) {
        led7_show_call(hd);
    } else {
        led7_show_bt(hd);
    }
#endif
}


static int ui_bt_user(void *hd, void *private, u8 menu, u32 arg)//子界面显示 //返回true不继续传递 ，返回false由common统一处理
{
    int ret = true;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return false;
    }
    switch (menu) {
    case MENU_BT:
        led7_show_bt(hd);
        break;

    default:
        ret = false;
    }

    return ret;

}



REGISTER_UI_MAIN(bt_main) = {
    .ui      = UI_BT_MENU_MAIN,
    .init    = ui_open_bt,
    .ui_main = ui_bt_main,
    .ui_user = ui_bt_user,
    .uninit  = ui_close_bt,
};



#endif
