#include "ui/ui_api.h"
#include "music/music_ui.h"
#include "fm_emitter/fm_emitter_manage.h"

#if TCFG_APP_MUSIC_EN
#if TCFG_UI_ENABLE
/* UI_DIS_VAR *ui_get_dis_var()//获取存储模式参数的结构体 */
/* { */
/* return ui_dis_var; */
/* } */


void ui_music_temp_finsh(u8 menu)//子菜单被打断或者显示超时
{
    switch (menu) {
    default:
        break;
    }
}

static void ui_led7_show_music_time(void *hd, int sencond)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    u8 tmp_buf[5] = {0};
    u8 min = 0;
    min = sencond / 60 % 60;
    sencond = sencond % 60;

    itoa2(min, (u8 *)&tmp_buf[0]);
    itoa2(sencond, (u8 *)&tmp_buf[2]);


    ui_dis_var->dis->lock(1);
    /* ui_dis_var->dis->clear(); */
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->clear_icon(0xffff);
    ui_dis_var->dis->show_string(tmp_buf);
    ui_dis_var->dis->flash_icon(LED7_2POINT);
    ui_dis_var->dis->show_icon(LED7_PLAY);
    ui_dis_var->dis->show_icon(LED7_MP3);
    ui_dis_var->dis->lock(0);

}


static void led7_show_filenumber(void *hd, u16 file_num)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    u8 bcd_number[5] = {0};	    ///<换算结果显示缓存
    itoa4(file_num, (u8 *)bcd_number);

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    if (file_num > 999 && file_num <= 1999) {
        bcd_number[0] = '1';
    } else {
        bcd_number[0] = ' ';
    }
    ui_dis_var->dis->show_string(bcd_number);
    ui_dis_var->dis->lock(0);
}


static void led7_show_pause(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" PAU");
    ui_dis_var->dis->show_icon(LED7_PAUSE);
    ui_dis_var->dis->lock(0);
}

static void led7_show_repeat_mode(void *hd, u32 val)
{
    if (!val) {
        return ;
    }
    u8 mode = (u8)val - 1;

    const u8 playmodestr[][5] = {
        " ALL",
        " ONE",
        "Fold",
        " rAn",
    };

    if (mode >= sizeof(playmodestr) / sizeof(playmodestr[0])) {
        printf("rpt mode display err !!\n");
        return ;
    }

    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)playmodestr[mode]);
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

static void *ui_open_music(void)
{
    MUSIC_DIS_VAR *ui_music = NULL;
    ui_music = (MUSIC_DIS_VAR *)malloc(sizeof(MUSIC_DIS_VAR));
    if (ui_music == NULL) {
        return NULL;
    }
    ui_set_auto_reflash(500);//设置主页500ms自动刷新
    return ui_music;
}

static void ui_close_music(void *hd, void *private)
{
    MUSIC_DIS_VAR *ui_music = (MUSIC_DIS_VAR *)private;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return ;
    }

    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->lock(0);

    if (ui_music) {
        free(ui_music);
    }
}

static void led7_show_music_dev(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;

    char *music_play_get_cur_dev(void);
    char *dev = music_play_get_cur_dev();

    if (dev) {
        if (!strcmp(dev, "udisk")) {
            ui_dis_var->dis->show_icon(LED7_USB);
        } else {
            ui_dis_var->dis->show_icon(LED7_SD);
        }
    }
}

static void ui_music_main(void *hd, void *private) //主界面显示
{
    if (!hd) {
        return;
    }
    MUSIC_DIS_VAR *ui_music = (MUSIC_DIS_VAR *)private;
#if TCFG_APP_FM_EMITTER_EN
    extern bool file_dec_is_pause(void);
    if (true == file_dec_is_pause()) {
        led7_show_pause(hd);
    } else {
        u16 fre = fm_emitter_manage_get_fre();
        if (fre != 0) {
            led7_fm_show_freq(hd, fre);
        }
    }
#else
    extern bool file_dec_is_play(void);
    extern bool file_dec_is_pause(void);
    if (true == file_dec_is_play()) {
        int music_play_get_cur_time(void);
        int sencond = music_play_get_cur_time();
        ui_led7_show_music_time(hd, sencond);
        led7_show_music_dev(hd);
        printf("sec = %d \n", sencond);
    } else if (file_dec_is_pause()) {
        led7_show_pause(hd);
    } else {
        printf("!!! %s %d\n", __FUNCTION__, __LINE__);

    }

#endif
}


static int ui_music_user(void *hd, void *private, u8 menu, u32 arg)//子界面显示 //返回true不继续传递 ，返回false由common统一处理
{
    int ret = true;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    MUSIC_DIS_VAR *ui_music = (MUSIC_DIS_VAR *)private;
    if (!ui_dis_var) {
        return false;
    }
    switch (menu) {
    case MENU_FILENUM:
        led7_show_filenumber(hd, arg);
        break;
    case MENU_MUSIC_REPEATMODE:
        led7_show_repeat_mode(hd, arg);
        break;
    default:
        ret = false;
        break;
    }

    return ret;

}



REGISTER_UI_MAIN(music_main) = {
    .ui      = UI_MUSIC_MENU_MAIN,
    .init    = ui_open_music,
    .ui_main = ui_music_main,
    .ui_user = ui_music_user,
    .uninit  = ui_close_music,
};

#endif
#endif
