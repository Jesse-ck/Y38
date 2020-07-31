#include "fm_server.h"
#include "loud_speaker.h"
#if TCFG_APP_FM_EN

#if (TCFG_SPI_LCD_ENABLE)
#include "ui/ui_style.h"
extern int ui_hide_main(int id);
extern int ui_show_main(int id);
extern int ui_server_msg_post(int argc, ...);
#endif

extern u8 app_get_audio_state(void);
extern void tone_event_to_user(u8 event);
extern void wdt_clear();
extern void bt_tws_sync_volume();
extern void save_scan_freq_org(u32 freq);
extern void fm_inside_trim(u32 freq);
extern struct dac_platform_data dac_data;
#define TCFG_FM_SC_REVERB_ENABLE 1

struct fm_opr {
    void *dev;
    u8 volume;
    u8 fm_dev_mute : 1;
    u8 scan_flag;
    u16 fm_freq_cur;		//  real_freq = fm_freq_cur + 875
    u16 fm_freq_channel_cur;
    u16 fm_total_channel;
    int scan_fre;
};

#define  SCANE_DOWN        (0x01)
#define  SCANE_UP          (0x02)

#define  SEMI_SCANE_DOWN   (0x03)//半自动搜索标志位
#define  SEMI_SCANE_UP     (0x04)


static struct fm_opr *fm_hdl = NULL;
#define __this 	(fm_hdl)


static void fm_app_mute(u8 mute)
{
    if (mute) {
        if (!__this->fm_dev_mute) {
            /* app_audio_mute(AUDIO_MUTE_DEFAULT); */
            fm_manage_mute(1);
            __this->fm_dev_mute = 1;
        }
    } else {
        if (__this->fm_dev_mute) {
            fm_manage_mute(0);
            /* app_audio_mute(AUDIO_UNMUTE_DEFAULT); */
            __this->fm_dev_mute = 0;
        }
    }
}


static void fm_read_info_init(void)
{
    FM_INFO fm_info = {0};

    void fm_vm_check(void);
    fm_vm_check();
    fm_read_info(&fm_info);

    __this->fm_freq_cur = fm_info.curFreq;//->dat[FM_FRE];
    printf("__this->fm_freq_cur = 0x%x\n", __this->fm_freq_cur);
    __this->fm_freq_channel_cur	= fm_info.curChanel;//->dat[FM_CHAN];
    printf("__this->fm_freq_channel_cur = 0x%x\n", __this->fm_freq_channel_cur);
    __this->fm_total_channel = fm_info.total_chanel;//get_total_mem_channel();
    printf("__this->fm_total_channel = 0x%x\n", __this->fm_total_channel);

    if (__this->fm_freq_cur == 0 && __this->fm_freq_channel_cur && __this->fm_total_channel) {
        __this->fm_freq_cur = get_fre_via_channel(__this->fm_freq_channel_cur);
        fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
    } else {
        fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
    }

}


// fm volume
static int fm_volume_set(u8 vol)
{

    return true;
}




static void fm_timer_handler_down(void)
{
    if ((!__this) || (__this->scan_flag != SCANE_DOWN)) {
        return;
    }

    wdt_clear();

    if (__this->scan_fre > VIRTUAL_FREQ(REAL_FREQ_MAX)) {
        __this->scan_fre = VIRTUAL_FREQ(REAL_FREQ_MIN);

        fm_app_mute(1);
        fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
        __this->scan_flag = 0;
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif

        fm_app_mute(0);


#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
        return;
    }

    fm_app_mute(1);

    if (fm_manage_set_fre(REAL_FREQ(__this->scan_fre))) {
        /* printf("~~~~~~~~~ FM FIND %d %d\n",  REAL_FREQ(__this->scan_fre),__this->scan_fre); */
        __this->fm_freq_cur  = __this->scan_fre;
        __this->fm_total_channel++;
        __this->fm_freq_channel_cur = __this->fm_total_channel;//++;
        save_fm_point(REAL_FREQ(__this->scan_fre));
        sys_timeout_add(NULL, fm_timer_handler_down, 1500); //播放一秒
#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_FM_STATION, 1000, __this->fm_total_channel, NULL);
        /* ui_menu_reflash(true); */
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif
        fm_app_mute(0);
    } else {
        sys_timeout_add(NULL, fm_timer_handler_down, 20);
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        /* ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL); */
#endif
#endif
    }
    __this->scan_fre++;

}

static void fm_timer_handler_up(void)
{
    if ((!__this) || (__this->scan_flag != SCANE_UP)) {
        return;
    }

    wdt_clear();

    if (__this->scan_fre <  VIRTUAL_FREQ(REAL_FREQ_MIN)) {
        __this->scan_fre = VIRTUAL_FREQ(REAL_FREQ_MAX);
        fm_app_mute(1);
        fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif
#endif
        fm_app_mute(0);
        __this->scan_flag = 0;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
        return;
    }

    fm_app_mute(1);

    if (fm_manage_set_fre(REAL_FREQ(__this->scan_fre))) {
        __this->fm_freq_cur  = __this->scan_fre;
        __this->fm_total_channel++;
        __this->fm_freq_channel_cur = 1;//++;
        save_fm_point(REAL_FREQ(__this->scan_fre));
        sys_timeout_add(NULL, fm_timer_handler_up, 1500); //播放一秒
#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_FM_STATION, 1000, __this->fm_total_channel, NULL);
        /* ui_menu_reflash(true); */
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif
        fm_app_mute(0);
    } else {
        sys_timeout_add(NULL, fm_timer_handler_up, 20); //
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        /* ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL); */
#endif

#endif
    }
    __this->scan_fre--;

}


static void __set_fm_station()
{
    fm_app_mute(1);
    __this->fm_freq_cur = get_fre_via_channel(__this->fm_freq_channel_cur);
    fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
    fm_last_ch_save(__this->fm_freq_channel_cur);
    fm_app_mute(0);
}

static void __set_fm_frq()
{
    fm_app_mute(1);
    fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
    fm_last_freq_save(REAL_FREQ(__this->fm_freq_cur));
    fm_app_mute(0);
}

static void fm_timer_scan_up(void)//半自动收台
{
    if ((!__this) || (__this->scan_flag != SEMI_SCANE_UP)) {
        return;
    }
    wdt_clear();

    if (__this->scan_fre <=  VIRTUAL_FREQ(REAL_FREQ_MIN)) {
        __this->scan_fre = VIRTUAL_FREQ(REAL_FREQ_MAX);
#if TCFG_FM_INSIDE_ENABLE
		save_scan_freq_org(REAL_FREQ(__this->scan_fre) * 10);
#endif
    }
	else{
		__this->scan_fre--;
	}

	if(__this->scan_fre == __this->fm_freq_cur){
		fm_app_mute(1);
		fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
#if TCFG_UI_ENABLE
		ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
		ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif
#endif
		fm_app_mute(0);
		__this->scan_flag = 0;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
		reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
		return;
	}

    fm_app_mute(1);

    if (fm_manage_set_fre(REAL_FREQ(__this->scan_fre))) {
        __this->fm_freq_cur  = __this->scan_fre;
        save_fm_point(REAL_FREQ(__this->scan_fre));//保存当前频点
        __this->fm_freq_channel_cur = get_channel_via_fre(REAL_FREQ(__this->scan_fre));//获取当前台号
        __this->fm_total_channel = get_total_mem_channel();//获取新的总台数
#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_FM_STATION, 1000, __this->fm_freq_channel_cur, NULL);
        /* ui_menu_reflash(true); */
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif
        fm_app_mute(0);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
        __this->scan_flag = 0;
        return;
    } else {
        sys_timeout_add(NULL, fm_timer_scan_up, 20); //
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        /* ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL); */
#endif

#endif
    }

}


static void fm_timer_scan_down(void)
{
    if ((!__this) || (__this->scan_flag != SEMI_SCANE_DOWN)) {
        return;
    }

    wdt_clear();

    if (__this->scan_fre >= VIRTUAL_FREQ(REAL_FREQ_MAX)) {
        __this->scan_fre = VIRTUAL_FREQ(REAL_FREQ_MIN);
#if TCFG_FM_INSIDE_ENABLE
		save_scan_freq_org(REAL_FREQ(__this->scan_fre) * 10);
#endif
	}
	else{
		__this->scan_fre++;
	}

	if(__this->scan_fre == __this->fm_freq_cur){

        fm_app_mute(1);
		fm_manage_set_fre(REAL_FREQ(__this->fm_freq_cur));
        __this->scan_flag = 0;
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif
        fm_app_mute(0);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
        return;
    }

    fm_app_mute(1);

    if (fm_manage_set_fre(REAL_FREQ(__this->scan_fre))) {
        __this->fm_freq_cur  = __this->scan_fre;
        save_fm_point(REAL_FREQ(__this->scan_fre));//保存当前频点
        __this->fm_freq_channel_cur = get_channel_via_fre(REAL_FREQ(__this->scan_fre));//获取当前台号
        __this->fm_total_channel = get_total_mem_channel();//获取新的总台数

#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_FM_STATION, 1000, __this->fm_freq_channel_cur, NULL);
        /* ui_menu_reflash(true); */
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif

        fm_app_mute(0);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
        __this->scan_flag = 0;
        return;

    } else {
        sys_timeout_add(NULL, fm_timer_scan_down, 20);
#if TCFG_UI_ENABLE
        ui_menu_reflash(true);
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
        /* ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL); */
#endif
#endif

    }

}




static void fm_scan_up()//半自动收台
{
    log_info("KEY_FM_SCAN_UP\n");
    if (__this->scan_flag) {
        return;
    }
    __this->scan_fre =  __this->fm_freq_cur;
    __this->scan_flag = SEMI_SCANE_UP;
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
    reverb_pause();
#endif

#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
	loud_speaker_pause();
#endif	
    fm_app_mute(1);

#if TCFG_FM_INSIDE_ENABLE
	fm_inside_trim(REAL_FREQ(__this->scan_fre) * 10);
#endif

    sys_timeout_add(NULL, fm_timer_scan_up, 20);
}



static void fm_scan_down()//半自动收台
{
    log_info("KEY_FM_SCAN_DOWN\n");
    if (__this->scan_flag) {
        return;
    }
    __this->scan_fre = __this->fm_freq_cur;
    __this->scan_flag = SEMI_SCANE_DOWN;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
    reverb_pause();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
	loud_speaker_pause();
#endif	
    fm_app_mute(1);

#if TCFG_FM_INSIDE_ENABLE
	fm_inside_trim(REAL_FREQ(__this->scan_fre) * 10);
#endif

    sys_timeout_add(NULL, fm_timer_scan_down, 20);

}



static void fm_scan_all_up()
{
    log_info("KEY_FM_SCAN_ALL_UP\n");
    if (__this->scan_flag) {
        log_info("KEY_FM_SCAN_ALL_DOWN STOP\n");
        __this->scan_flag = 0;
        os_time_dly(1);
        fm_app_mute(0);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
        return;
    }
    clear_all_fm_point();
    __this->fm_freq_cur  = 1;
    __this->fm_total_channel = 0;
    __this->fm_freq_channel_cur = 0;


    __this->scan_fre = VIRTUAL_FREQ(REAL_FREQ_MAX);
    __this->scan_flag = SCANE_UP;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
    reverb_pause();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
	loud_speaker_pause();
#endif	

    fm_app_mute(1);

#if TCFG_FM_INSIDE_ENABLE
	fm_inside_trim(REAL_FREQ(__this->scan_fre) * 10);
#endif

    sys_timeout_add(NULL, fm_timer_handler_up, 20);
}



static void fm_scan_all_down()
{
    log_info("KEY_FM_SCAN_ALL_DOWN\n");
    if (__this->scan_flag) {
        log_info("KEY_FM_SCAN_ALL STOP\n");
        __this->scan_flag = 0;
        os_time_dly(1);
        fm_app_mute(0);

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
        reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
        return;
    }

    clear_all_fm_point();

    __this->fm_freq_cur  = 1;//__this->scan_fre;
    __this->fm_total_channel = 0;//++;
    __this->fm_freq_channel_cur = 0;//++;

    __this->scan_fre = VIRTUAL_FREQ(REAL_FREQ_MIN);
    __this->scan_flag = SCANE_DOWN;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
    reverb_pause();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
	loud_speaker_pause();
#endif	

    fm_app_mute(1);

#if TCFG_FM_INSIDE_ENABLE
	fm_inside_trim(REAL_FREQ(__this->scan_fre) * 10);
#endif

    sys_timeout_add(NULL, fm_timer_handler_down, 20);
}

static void fm_volume_pp(void)
{

    log_info("KEY_MUSIC_PP\n");
    if (__this->scan_flag) {
        return ;
    }
    if (__this->fm_dev_mute == 0) {
        fm_app_mute(1);
    } else {
        fm_app_mute(0);
    }
}


static void fm_prev_freq()
{
    log_info("KEY_FM_PREV_FREQ\n");
    if (__this->scan_flag) {
        return;
    }

    if (__this->fm_freq_cur <= VIRTUAL_FREQ(REAL_FREQ_MIN)) {
        __this->fm_freq_cur = VIRTUAL_FREQ(REAL_FREQ_MAX);
    } else {
        __this->fm_freq_cur -= 1;
    }

    __set_fm_frq();
#if TCFG_UI_ENABLE
    ui_menu_reflash(true);
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
    ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif
#endif
}

static void fm_next_freq()
{
    log_info("KEY_FM_NEXT_FREQ\n");
    if (__this->scan_flag) {
        return;
    }

    if (__this->fm_freq_cur >= VIRTUAL_FREQ(REAL_FREQ_MAX)) {
        __this->fm_freq_cur = VIRTUAL_FREQ(REAL_FREQ_MIN);
    } else {
        __this->fm_freq_cur += 1;
    }

    __set_fm_frq();

#if TCFG_UI_ENABLE
    ui_menu_reflash(true);

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
    ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif

#endif
}

static void fm_volume_up()
{
    u8 vol = 0;
    log_info("KEY_VOL_UP\n");
    if (__this->scan_flag) {
        return;
    }

    app_audio_volume_up(1);
    log_info("fm vol+: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    bt_tws_sync_volume();
#endif

#if TCFG_UI_ENABLE
    vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
    ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE

}

static void fm_volume_down()
{
    u8 vol = 0;
    log_info("KEY_VOL_DOWN\n");
    if (__this->scan_flag) {
        return;
    }
    app_audio_volume_down(1);
    log_info("fm vol-: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    bt_tws_sync_volume();
#endif

#if TCFG_UI_ENABLE
    vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
    ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE

}


static void fm_prev_station()
{
    log_info("KEY_FM_PREV_STATION\n");

    if (__this->scan_flag || (!__this->fm_total_channel)) {
        return;
    }

    if (__this->fm_freq_channel_cur <= 1) {
        __this->fm_freq_channel_cur = __this->fm_total_channel;
    } else {
        __this->fm_freq_channel_cur -= 1;
    }
    __set_fm_station();
#if TCFG_UI_ENABLE
    ui_set_tmp_menu(MENU_FM_STATION, 2000, __this->fm_freq_channel_cur, NULL);
    /* ui_menu_reflash(true); */
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
    ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif
#endif
}


static void fm_next_station()
{
    log_info("KEY_FM_NEXT_STATION\n");
    if (__this->scan_flag || (!__this->fm_total_channel)) {
        return;
    }


    if (__this->fm_freq_channel_cur >= __this->fm_total_channel) {
        __this->fm_freq_channel_cur = 1;
    } else {
        __this->fm_freq_channel_cur += 1;
    }

    __set_fm_station();
#if TCFG_UI_ENABLE
    ui_set_tmp_menu(MENU_FM_STATION, 2000, __this->fm_freq_channel_cur, NULL);
    /* ui_menu_reflash(true); */
#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用
    ui_server_msg_post(3, ID_WINDOW_FM, "fm_fre", NULL);
#endif
#endif
}

static void fm_msg_deal(int msg)
{
    switch (msg) {
    //action
    case FM_MUSIC_PP:
        fm_volume_pp();
        break;
    case FM_PREV_FREQ:
        fm_prev_freq();
        break;
    case FM_NEXT_FREQ:
        fm_next_freq();
        break;
    case FM_VOLUME_UP:
        fm_volume_up();
        break;
    case FM_VOLUME_DOWN:
        fm_volume_down();
        break;
    case FM_PREV_STATION:
        fm_prev_station();
        break;
    case FM_NEXT_STATION:
        fm_next_station();
        break;
    case FM_SCAN_ALL_DOWN:
        fm_scan_all_down();
        break;
    case FM_SCAN_ALL_UP:
        fm_scan_all_up();
        break;
    case FM_SCAN_DOWN:
        fm_scan_down();
        break;
    case FM_SCAN_UP:
        fm_scan_up();
        break;

    }
#if (defined(SMART_BOX_EN) && (SMART_BOX_EN))
	extern void smartbot_fm_msg_deal(int msg);
	smartbot_fm_msg_deal(msg);
#endif
}

static void fm_task_init()
{
    fm_hdl = (struct fm_opr *)malloc(sizeof(struct fm_opr));
    memset(fm_hdl, 0x00, sizeof(struct fm_opr));
    if (fm_hdl == NULL) {
        puts("fm_state_machine fm_hdl malloc err !\n");
    }
    __this->fm_dev_mute = 0;
    fm_app_mute(1);
    fm_read_info_init();
    os_time_dly(1);
    fm_app_mute(0);
#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_FM_MENU_MAIN);
#endif

#if (TCFG_SPI_LCD_ENABLE)
    ui_show_main(ID_WINDOW_FM);
#endif
}


int fm_server_msg_post(int msg)
{
    int ret = 0;
    int msg_pool[8];
    u8 count = 0;
    if (!__this) {
        return -1;
    }
    msg_pool[0] = msg;
__try:
        os_taskq_post_type("fm_task", FM_MSG_USER, 1, msg_pool);
    if (ret) {
        if (count > 20) {
            return -1;
        }
        count++;
        os_time_dly(1);
        goto __try;
    }
    return 0;
}


static void fm_task(void *p)
{
    int msg[32];
    int ret;
    fm_task_init();
    os_sem_post((OS_SEM *)p);
    while (1) {
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)); //500ms_reflash
        if (ret != OS_TASKQ) {
            continue;
        }
        switch (msg[0]) { //action
        case FM_MSG_EXIT:
            if (__this->scan_flag) {
                __this->scan_flag = 0;
                os_time_dly(5);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE) &&(TCFG_FM_SC_REVERB_ENABLE))
                reverb_resume();
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))	
		loud_speaker_resume();
#endif	
            }

            os_sem_post((OS_SEM *)msg[1]);
            os_time_dly(10000);
            break;
        case FM_MSG_USER:
            fm_msg_deal(msg[1]);
            break;
        }
    }
}


void fm_server_create()
{
    int err = 0;
    if (__this) {
        return;
    }
    OS_SEM sem;
    os_sem_create(&sem, 0);
    err = task_create(fm_task, (void *)&sem, "fm_task");
    os_sem_pend(&sem, 0);
}


void fm_sever_kill()
{
    int err = 0;
    int msg[8];
    if (!__this) {
        return;
    }
    OS_SEM sem;// = zalloc(sizeof(OS_SEM));
    os_sem_create(&sem, 0);
    msg[0] = (int)&sem;
    do {
        err = os_taskq_post_type("fm_task", FM_MSG_EXIT, 1, msg);
        if (!err) {
            break;
        }
        printf("fm err = %x\n", err);
        os_time_dly(5);
    } while (err);
    os_time_dly(3);
    os_sem_pend(&sem, 0);
    err = task_kill("fm_task");
    if (__this != NULL) {
        free(__this);
        __this = NULL;
    }
#if (TCFG_SPI_LCD_ENABLE)
    ui_hide_main(ID_WINDOW_FM);
#endif
}

u8 fm_get_scan_flag(void)
{
	return __this->scan_flag;
}

u8 fm_get_fm_dev_mute(void)
{
	return __this->fm_dev_mute;
}

u8 fm_get_cur_channel(void)
{
	return (u8)__this->fm_freq_channel_cur;
}

u16 fm_get_cur_fre(void)
{
	if (__this->fm_freq_cur > 1080) {
		__this->fm_freq_cur /= 10;
	}
	return 	(__this->fm_freq_cur % 874) + 874;
}

u8 fm_get_mode(void)
{
	u32 freq_min = REAL_FREQ_MIN/VIRTUAL_FREQ_STEP;
	if (freq_min < 875) {
		return 1;	
	} else {
		return 0;	
	}
}

void fm_sel_station(u8 channel)
{
	if (channel > __this->fm_total_channel)	{
		printf("channel sel err!\n");	
		return;
	}
	__this->fm_freq_channel_cur = channel;
	__set_fm_station();
}

u8 fm_set_fre(u16 fre)
{
	u8 ret = 0;
	if ((fre < 875) || (fre > 1080)) {
		return ret;	
	}
	__this->fm_freq_cur = fre;
	__set_fm_frq();
	return ret;	
}

u8 get_fm_scan_status()
{
	return __this->scan_flag;
}

#endif

