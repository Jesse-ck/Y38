#include "system/includes.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"

#if TCFG_APP_LINEIN_EN
#if ((TCFG_UI_ENABLE)&&(TCFG_LED_LCD_ENABLE))

void ui_linein_temp_finsh(u8 menu)//子菜单被打断或者显示超时
{
    switch (menu) {
    default:
        break;
    }
}

static void led7_show_aux(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" AUX");
    ui_dis_var->dis->lock(0);
}


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

static void led7_show_pause(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" PAU");
    ui_dis_var->dis->lock(0);
}

static void *ui_open_linein(void)
{
    /* ui_set_auto_reflash(500);//设置主页500ms自动刷新 */
    return NULL;
}

static void ui_close_linein(void *hd, void *private)
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

static void ui_linein_main(void *hd, void *private) //主界面显示
{
    if (!hd) {
        return;
    }

#if TCFG_APP_FM_EMITTER_EN
    extern u8 linein_get_status(void);
    if (linein_get_status()) {
        u16 fre = fm_emitter_manage_get_fre();
        if (fre != 0) {
            led7_fm_show_freq(hd, fre);
        } else {
            printf("ui get fmtx fre err !\n");
        }
    } else {
        led7_show_pause(hd);
    }
#else
    extern u8 linein_get_status(void);
    if (linein_get_status()) {
        led7_show_aux(hd);
    } else {
        led7_show_pause(hd);
    }
#endif
}


static int ui_linein_user(void *hd, void *private, u8 menu, u32 arg)//子界面显示 //返回true不继续传递 ，返回false由common统一处理
{
    int ret = true;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return false;
    }
    switch (menu) {
    case MENU_AUX:
        led7_show_aux(hd);
        break;
    default:
        ret = false;
        break;
    }

    return ret;

}



REGISTER_UI_MAIN(linein_main) = {
    .ui      = UI_AUX_MENU_MAIN,
    .init    = ui_open_linein,
    .ui_main = ui_linein_main,
    .ui_user = ui_linein_user,
    .uninit  = ui_close_linein,
};






#endif
#endif
