#include "system/includes.h"
/*#include "btcontroller_config.h"*/
#include "btstack/btstack_task.h"
#include "app_config.h"
#include "app_action.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "ui_manage.h"
#include "gpio.h"
#include "app_main.h"
#include "asm/charge.h"
#include "update.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "app_charge.h"
#include "user_cfg.h"
#include "app_api/app_debug_api.h"
#include "bt_tws.h"
#include "charge_box/chargeIc_manage.h"
#include "charge_box/charge_det.h"
#include "charge_box/charge_ctrl.h"
#include "charge_box/wireless.h"
#include "clock_cfg.h"

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[APP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


/*任务列表 */
const struct task_info task_info_table[] = {
    {"app_core",            5,     1024,   512  },
    {"sys_event",           6,     256,   0    },
    {"btctrler",            4,     512,   384  },
    {"tws",                 5,     512,   128  },
    {"btstack",             3,     768,   256  },
#if (defined(TCFG_DEV_PREPROCESSOR_ENABLE) && TCFG_DEV_PREPROCESSOR_ENABLE)
    {"dev_pre",           	2,     256,   0		},
    {"audio_dec",           3,     768 + 32,   128  },
#else
#if (TCFG_REVERB_ENABLE)
    {"audio_dec",           3,     768 + 128,   128  },
#else
    {"audio_dec",           3,     768 + 32,   128  },
#endif
#endif
    {"dev_mg",           	3,     512,   512  },
    {"music_ply",           4,     1024,  256  },
    {"audio_enc",           3,     512,   128  },
    {"usb_msd",           	2,     512,   128   },
    {"aec",					2,	   768,   128	},
    {"aec_dbg",				3,	   128,   128	},
    {"update",				1,	   256,   0		},
    {"systimer",		    6,	   128,   0		},
    {"usb_audio",           5,     256,   256  },
    {"plnk_dbg",            5,     256,   256  },
    {"adc_linein",          2,     768,   128  },
    {"enc_write",           1,     768,   0 	},
    /* {"src_write",           1,     768,   256 	}, */
    {"fm_task",             3,     512,   128  },
#if (GMA_EN)
    {"tm_gma",              4,     768,   256  },
#endif
#if (RCSP_BTMATE_EN || RCSP_ADV_EN)
    {"rcsp_task",			4,	   768,   128	},
#endif
#if TCFG_SPI_LCD_ENABLE
    {"ui",           	    2,     768,   256  },
#else
    {"ui",                  3,     384,   64  },
#endif

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)
    {"chgbox_n",            6,     512,   128  },
#endif
    {0, 0},
};

APP_VAR app_var;

/*
 * 2ms timer中断回调函数
 */
void timer_2ms_handler()
{

}

void app_var_init(void)
{
    memset((u8 *)&bt_user_priv_var, 0, sizeof(BT_USER_PRIV_VAR));
    app_var.play_poweron_tone = 1;

}

void app_earphone_play_voice_file(const char *name);

void clr_wdt(void);

void check_power_on_key(void)
{
    u32 delay_10ms_cnt = 0;

    while (1) {
        clr_wdt();
        os_time_dly(1);

        extern u8 get_power_on_status(void);
        if (get_power_on_status()) {
            log_info("+");
            delay_10ms_cnt++;
            if (delay_10ms_cnt > 70) {
                return;
            }
        } else {
            log_info("-");
            delay_10ms_cnt = 0;
            log_info("enter softpoweroff\n");
            power_set_soft_poweroff();
        }
    }
}

static u8 poweron_charge_idle = 0;
void set_poweron_charge_idle(u8 flag)
{
    poweron_charge_idle = flag;
}

u8 get_poweron_charge_idle()
{
    return poweron_charge_idle;
}

void save_reset_source_to_vm(u8 reset_s)
{
    syscfg_write(CFG_USER_RESET_SOURCE, &reset_s, sizeof(reset_s));
}

u8 get_reset_from_vm(void)
{
    u8 reset_source = 0;
    syscfg_read(CFG_USER_RESET_SOURCE, &reset_source, sizeof(reset_source));
    return reset_source;
}

extern int cpu_reset_by_soft();
extern u32 timer_get_ms(void);
extern int audio_dec_init();
extern int audio_enc_init();


static void audio_module_probe(void)
{
#if 0       //debug
    /*解码器*/
    AUDIO_DECODER_PROBE(sbc);
    AUDIO_DECODER_PROBE(msbc);
    AUDIO_DECODER_PROBE(cvsd);
    /*AUDIO_DECODER_PROBE(mty);*/
#if TCFG_DEC_WAV_ENABLE
    AUDIO_DECODER_PROBE(wav);
#endif
#if TCFG_DEC_WMA_ENABLE
    AUDIO_DECODER_PROBE(wma);
#endif
#if TCFG_DEC_MP3_ENABLE
    AUDIO_DECODER_PROBE(mp3);
#endif
#if TCFG_BT_SUPPORT_AAC
    AUDIO_DECODER_PROBE(aac);
#endif

    /*编码器*/
    AUDIO_ENCODER_PROBE(msbc);
    AUDIO_ENCODER_PROBE(cvsd);
#endif
}

void app_charge_box_ctrl_init(void);
void app_main()
{
    int update;
    u32 addr = 0, size = 0;
    STATUS *p_tone;

    log_info("app_main\n");


    audio_enc_init();
    audio_dec_init();

    /* audio_module_probe(); */
#ifdef CONFIG_UPDATA_ENABLE
    update = update_result_deal();
#endif


#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)
    clock_add_set(CHARGE_BOX_CLK);
    chgbox_init_app();
#endif

    audio_module_probe();
    app_var_init();

#if (TCFG_MC_BIAS_AUTO_ADJUST == MC_BIAS_ADJUST_POWER_ON)
    extern u8 power_reset_src;
    u8 por_flag = 0;
    u8 cur_por_flag = 0;
    /*
     *1.update
     *2.power_on_reset(BIT0:上电复位)
     *3.pin reset(BIT4:长按复位)
     */
    if (update || (power_reset_src & BIT(0)) || (power_reset_src & BIT(4))) {
        //log_info("reset_flag:0x%x",power_reset_src);
        cur_por_flag = 0xA5;
    }
    int ret = syscfg_read(CFG_POR_FLAG, &por_flag, 1);
    log_info("POR flag:%x-%x", cur_por_flag, por_flag);
    if ((cur_por_flag == 0xA5) && (por_flag != cur_por_flag)) {
        //log_info("update POR flag");
        ret = syscfg_write(CFG_POR_FLAG, &cur_por_flag, 1);
    }
#endif

#if (TCFG_CHARGE_ENABLE && TCFG_CHARGE_POWERON_ENABLE)
    if (is_ldo5v_wakeup()) { //LDO5V唤醒
        if (get_charge_online_flag()) { //关机时，充电插入
            set_poweron_charge_idle(1);
        } else { //关机时，充电拔出
            power_set_soft_poweroff();
        }
    }
#endif

    if (get_charge_online_flag()) {
        set_charge_idle_flag(0);

#if (TCFG_CHARGE_ENABLE && TCFG_CHARGE_POWERON_ENABLE)
        set_poweron_charge_idle(1);
#endif

#if(TCFG_SYS_LVD_EN == 1)
        vbat_check_init();
#endif
        app_var.start_time = timer_get_ms();
        app_task_switch(APP_NAME_IDLE, ACTION_APP_MAIN, NULL);
    } else {
        check_power_on_voltage();

#if TCFG_POWER_ON_NEED_KEY
#if (CONFIG_BT_MODE == BT_NORMAL)
        /*充电拔出,CPU软件复位, 不检测按键，直接开机*/
#if TCFG_CHARGE_OFF_POWERON_NE
        if ((!update && cpu_reset_by_soft()) || is_ldo5v_wakeup()) {
#else
        if (!update && cpu_reset_by_soft()) {
#endif
            app_var.play_poweron_tone = 0;
        } else {
            check_power_on_key();
        }
#endif
#endif
        /* endless_loop_debug_int(); */
        ui_manage_init();
        ui_update_status(STATUS_POWERON);
        app_var.start_time = timer_get_ms();

        app_task_switch(APP_NAME_POWERON, ACTION_APP_MAIN, NULL);


    }
}

int eSystemConfirmStopStatus(void)
{
    /* 系统进入在未来时间里，无任务超时唤醒，可根据用户选择系统停止，或者系统定时唤醒(100ms) */
    //1:Endless Sleep
    //0:100 ms wakeup
    if (get_charge_full_flag()) {
        log_i("Endless Sleep");
        power_set_soft_poweroff();
        return 1;
    } else {
        log_i("100 ms wakeup");
        return 0;
    }

}
