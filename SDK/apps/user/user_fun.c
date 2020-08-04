#include "string.h"
#include "app_action.h"
#include "gpio.h"
#include "audio_dec.h"

#include "user_fun.h"
#include "user_fun_config.h"
#include "user_pa.h"
//#include "user_rgb.h"


static struct ex_dev_opr spk_dev = {
    //设置
    .dev_name = "spk",
    .enable = JL_SPK_EN,
    .port = USER_SPK_IO,            //检测IO
    .up = 1,                        // 检测IO上拉使能
    .down = 0,                      // 检测IO下拉使能
    .ad_channel = NO_CONFIG_PORT,   // 检测IO是否使用AD检测
    .ad_vol = 0,                    // AD检测时的阀值

    .scan_time = 50,
    .msg = USER_MSG_SYS_SPK_STATUS, // 发送的消息

    //状态
    .step = 0,
    .stu  = 0,//1
    .online = false,
};


void user_sys_auto_mute(bool cmd){
#if (defined(AUDIO_OUTPUT_AUTOMUTE) && (AUDIO_OUTPUT_AUTOMUTE == ENABLE))
    extern struct audio_automute *automute;
    extern void app_audio_output_ch_mute(u8 ch, u8 mute);
    
    u8 i = 0;
    u8 all_ch = 0;

    for (i = 0; i < audio_output_channel_num(); i++) {
        all_ch |= BIT(i);
    }

    if(!cmd){
        //调过 mute 通道
        audio_automute_skip(automute, 1);
        app_audio_output_ch_mute(all_ch, 0);
    }else{
        //开启 mute通道
        audio_automute_skip(automute, 0);
        app_audio_output_ch_mute(all_ch, 1);        
    }

#endif


}



void user_spk_init(void){
    gpio_set_direction(USER_SPK_IO,1);
    gpio_set_pull_down(USER_SPK_IO,0);
    gpio_set_pull_up(USER_SPK_IO,1);
    gpio_set_die(USER_SPK_IO,1);
}

u8 user_key_mapping(u8 key){
    u8 tp = NO_KEY;
    // u8 map_table[][2]={
    //     {0,3},{},
    // }

    return key;
}
void user_dac_pupu(void){
     u32 delay_10ms_cnt = 0;

    // pa_ctl(PA_MUTE,0);
    while (1) {
        // clr_wdt();
        os_time_dly(1);
        if(delay_10ms_cnt++>100){
            break;
        }
    }

	extern struct audio_dac_hdl dac_hdl;
	extern int audio_dac_start(struct audio_dac_hdl *dac);
	extern int audio_dac_stop(struct audio_dac_hdl *dac);
	audio_dac_start(&dac_hdl);
	audio_dac_stop(&dac_hdl);

    delay_10ms_cnt = 0;

    while (1) {
        // clr_wdt();
        os_time_dly(1);
        if(delay_10ms_cnt++>100){
            break;
        }
    }
    // pa_ctl(PA_UMUTE,0);
}

void user_power_io_keep(bool value){
    gpio_set_direction(USER_POWER_KEEP_IO,0);
    gpio_set_pull_down(USER_POWER_KEEP_IO,0);
    gpio_set_pull_up(USER_POWER_KEEP_IO,0);
    gpio_set_die(USER_POWER_KEEP_IO,1);
    gpio_set_output_value(USER_POWER_KEEP_IO,value);
}

void user_power_off(void){
    user_pa_strl(PA_POWER_OFF);
    user_power_io_keep(0);
}

void user_fun_io_init(void){
    user_power_io_keep(1);
    user_pa_strl(PA_INIT);//mute pa
}

void user_fun_init(void){
    user_spk_init();
    user_pa_init();
    user_dac_pupu();

    ex_dev_detect_init(&spk_dev);
    
}
