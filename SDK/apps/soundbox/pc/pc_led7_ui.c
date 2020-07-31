#include "ui/ui_api.h"
#include "system/includes.h"

#if TCFG_APP_PC_EN
#if ((TCFG_UI_ENABLE)&&(TCFG_LED_LCD_ENABLE))
void ui_pc_temp_finsh(u8 menu)//子菜单被打断或者显示超时
{
    switch (menu) {
    default:
        break;
    }
}

static void led7_show_pc(void *hd)
{
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    ui_dis_var->dis->lock(1);
    ui_dis_var->dis->clear();
    ui_dis_var->dis->setXY(0, 0);
    ui_dis_var->dis->show_string((u8 *)" PC");
    ui_dis_var->dis->show_icon(LED7_USB);
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

static void *ui_open_pc(void)
{
    /* ui_set_auto_reflash(500);//设置主页500ms自动刷新 */
    return NULL;
}

static void ui_close_pc(void *hd, void *private)
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

static void ui_pc_main(void *hd, void *private) //主界面显示
{
    if (!hd) {
        return;
    }
    led7_show_pc(hd);
}


static int ui_pc_user(void *hd, void *private, u8 menu, u32 arg)//子界面显示 //返回true不继续传递 ，返回false由common统一处理
{
    int ret = true;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (!ui_dis_var) {
        return false;
    }
    switch (menu) {
    default:
        ret = false;
        break;
    }

    return ret;

}



REGISTER_UI_MAIN(pc_main) = {
    .ui      = UI_PC_MENU_MAIN,
    .init    = ui_open_pc,
    .ui_main = ui_pc_main,
    .ui_user = ui_pc_user,
    .uninit  = ui_close_pc,
};













#endif
#endif
