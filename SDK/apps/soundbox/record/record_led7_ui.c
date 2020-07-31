#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
/* #include "app_api/record_api.h" */

extern u32 record_player_get_encoding_time();
extern int record_player_is_encoding(void);

#if (TCFG_UI_ENABLE && TCFG_LED_LCD_ENABLE && TCFG_APP_RECORD_EN)

void ui_record_temp_finsh(u8 menu)//子菜单被打断或者显示超时
{
    switch (menu) {
    default:
        break;
    }
}

static void led7_show_record(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" REC");
    ui_dis_var->dis->lock(0);
}

static void led_show_record_time(void *hd, int time)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    u8 tmp_buf[5] = {0};

    u8 Min = (u8)(time / 60 % 60);
    u8 Sec = (u8)(time % 60);
    printf("rec Min = %d, Sec = %d\n", Min, Sec);

    itoa2(Min, (u8 *)&tmp_buf[0]);
    itoa2(Sec, (u8 *)&tmp_buf[2]);

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string(tmp_buf);
    ui_dis_var->dis->show_icon(LED7_2POINT);
    ui_dis_var->dis->lock(0);
}


#if TCFG_APP_FM_EMITTER_EN
static void led7_fm_show_freq(void *hd, u32 arg)
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
#endif//TCFG_APP_FM_EMITTER_EN

static void *ui_open_record(void)
{
    ui_set_auto_reflash(500);
    return NULL;
}

static void ui_close_record(void *hd, void *private)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return ;
    }

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->lock(0);

    if (private) {
        free(private);
    }
}

static void ui_record_main(void *hd, void *private) //主界面显示
{
    /* printf("=====================\n"); */
    if (!hd) {
        return;
    }

#if TCFG_APP_FM_EMITTER_EN
    u16 fre = fm_emitter_manage_get_fre();
    if (fre != 0) {
        led7_fm_show_freq(hd, fre);
    }
#else
    if (record_player_is_encoding()) {
        int time = record_player_get_encoding_time();
        led_show_record_time(hd, time);
    } else {
        led7_show_record(hd);
    }
#endif
}


static int ui_record_user(void *hd, void *private, u8 menu, u32 arg)//子界面显示 //返回true不继续传递 ，返回false由common统一处理
{
    int ret = true;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return false;
    }
    switch (menu) {
    case MENU_RECORD:
        led7_show_record(hd);
        break;
    default:
        ret = false;
        break;
    }

    return ret;

}



REGISTER_UI_MAIN(record_main) = {
    .ui      = UI_RECORD_MENU_MAIN,
    .init    = ui_open_record,
    .ui_main = ui_record_main,
    .ui_user = ui_record_user,
    .uninit  = ui_close_record,
};


#endif//(TCFG_UI_ENABLE && TCFG_APP_RECORD_EN)
