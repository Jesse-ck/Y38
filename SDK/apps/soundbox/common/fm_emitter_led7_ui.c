#include "common/fm_emitter_led7_ui.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"

#if (TCFG_UI_ENABLE && TCFG_APP_FM_EMITTER_EN)

void fm_emitter_enter_ui_menu(void)
{
    u16 tmp_fmtx_fre;
    tmp_fmtx_fre = fm_emitter_manage_get_fre();

    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        ui_menu_reflash(true);//刷新主页
        printf("fm menu exit !\n");
    } else if (ui_get_app_menu(GRT_CUR_MENU) == MENU_MAIN) {
        printf("fm menu in !\n");
        ui_set_tmp_menu(MENU_FM_SET_FRE, 10000, tmp_fmtx_fre, NULL);
    } else {
        printf("fm menu ui not ready %d!\n", __LINE__);
    }
}

void fm_emitter_enter_ui_next_fre(void)
{
    u16 tmp_fmtx_fre;
    tmp_fmtx_fre = fm_emitter_manage_get_fre();
    if (tmp_fmtx_fre < 1080) {
        tmp_fmtx_fre++;
    } else {
        tmp_fmtx_fre = 875;
    }
    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        ui_set_tmp_menu(MENU_FM_SET_FRE,  10000, tmp_fmtx_fre, NULL);
        fm_emitter_manage_set_fre(tmp_fmtx_fre);
    } else {
        printf("fm menu ui not ready %d!\n", __LINE__);
    }
}

void fm_emitter_enter_ui_prev_fre(void)
{
    u16 tmp_fmtx_fre;
    tmp_fmtx_fre = fm_emitter_manage_get_fre();
    if (tmp_fmtx_fre > 875) {
        tmp_fmtx_fre--;
    } else {
        tmp_fmtx_fre = 1080;
    }
#if TCFG_UI_ENABLE
    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        ui_set_tmp_menu(MENU_FM_SET_FRE,  10000, tmp_fmtx_fre, NULL);
        fm_emitter_manage_set_fre(tmp_fmtx_fre);
    } else {
        printf("fm menu ui not ready %d!\n", __LINE__);
    }
#endif
}


static u16 fre_num_tmp = 0;
static void set_fm_fre_timeout(u8 menu)
{
    if (menu == MENU_IR_FM_SET_FRE) {
        fre_num_tmp = 0;
        /* printf("set_fm_fre_timeout\n"); */
    }
}

void fm_emitter_fre_set_by_number(u8 num)
{
    fre_num_tmp = fre_num_tmp * 10 + num;
#if TCFG_UI_ENABLE
    /* printf("fre_num_tmp =%d \n",fre_num_tmp); */
    ui_set_tmp_menu(MENU_IR_FM_SET_FRE, 4 * 1000, fre_num_tmp, set_fm_fre_timeout);
#endif
    if (fre_num_tmp > 875) {
        fre_num_tmp = 0;
    }
}

#endif //(TCFG_UI_ENABLE && TCFG_APP_FM_EMITTER_EN)



