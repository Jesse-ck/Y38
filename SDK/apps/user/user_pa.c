#include "string.h"
#include "gpio.h"
#include "app_action.h"
#include "btstack/avctp_user.h"
#include "app_main.h"

#include "user_fun.h"
#include "user_pa.h"
//#include "user_rgb.h"

extern APP_VAR app_var;
extern int tone_get_status();

static volatile bool only_pin = 0;

static volatile u8 automute=0;
static volatile u8 manual_mute=0;
static volatile bool mic_online = 0;

void user_pa_set_auto_mute(volatile u8 sys_automute){
    automute = sys_automute;
}
volatile u8 user_is_auto_mute(void){
    return automute;
}

void user_manual_mute(u8 cmd){
    manual_mute = cmd;
}
volatile u8 user_is_manual_mute(void){
    return manual_mute;
}

void user_mic_online(u8 cmd){
    mic_online = cmd?1:0;
}
volatile bool user_is_mic_online(void){
    return mic_online;
}



static void user_pa_ab(void){
    puts(">> pa ab\n");

    gpio_set_direction(USER_PA_MUTE_IO,1);
    gpio_set_pull_down(USER_PA_MUTE_IO,0);
    gpio_set_pull_up(USER_PA_MUTE_IO,1);
}
static void user_pa_d(void){
    puts(">> pa d\n");
    gpio_set_direction(USER_PA_MUTE_IO,0);
    gpio_set_pull_down(USER_PA_MUTE_IO,0);
    gpio_set_pull_up(USER_PA_MUTE_IO,0);
    gpio_set_die(USER_PA_MUTE_IO,1);
    gpio_set_output_value(USER_PA_MUTE_IO,1);
}

static void user_pa_mute(void){
    puts(">> pa mute\n");

    gpio_set_direction(USER_PA_MUTE_IO,0);
    gpio_set_pull_down(USER_PA_MUTE_IO,0);
    gpio_set_pull_up(USER_PA_MUTE_IO,0);
    gpio_set_die(USER_PA_MUTE_IO,1);
    gpio_set_output_value(USER_PA_MUTE_IO,0);
    
}
static void user_pa_umute(void){
    puts(">> pa umute\n");
    if(only_pin){
        if(true == app_cur_task_check(APP_NAME_FM)){
            user_pa_ab();
        }else{
            user_pa_d();
        }
    }else{
        gpio_set_output_value(USER_PA_MUTE_IO,1);
    }
    
}

static void user_init(void){
    puts(">> pa init\n");
    if(USER_PA_MUTE_IO == USER_PA_CLASS_IO){
        only_pin = 1;
    }

    gpio_set_direction(USER_PA_MUTE_IO,0);
    gpio_set_pull_down(USER_PA_MUTE_IO,0);
    gpio_set_pull_up(USER_PA_MUTE_IO,0);
    gpio_set_die(USER_PA_MUTE_IO,1);
    gpio_set_output_value(USER_PA_MUTE_IO,0);
}

void user_pa_strl(u8 cmd){

    static u8 pa_strl_mute_flag = 0;
    static bool power_off_flag = 0;
    if(power_off_flag){
        return;
    }

    if(PA_MUTE == cmd && PA_MUTE !=pa_strl_mute_flag){
        pa_strl_mute_flag = PA_MUTE;
        user_pa_mute();
    }else if(PA_UMUTE == cmd && PA_UMUTE !=pa_strl_mute_flag){
        pa_strl_mute_flag = PA_UMUTE;
        user_pa_umute();
    }else if(PA_CLASS_AB == cmd){
        user_pa_ab();
    }else if(PA_CLASS_D == cmd){
        user_pa_d();
    }else if(PA_INIT == cmd){
        user_init();
    }else if(PA_POWER_OFF == cmd){    
        puts(">>>>>>>>>>>>>>> power off <<<<<<<<<<<<<<<<<<<<\n");
        user_pa_mute();
        power_off_flag = 1;
    }
}

void user_pa_service(void){
    u8 tp_flag =0;

    //自动mute
    if(user_is_auto_mute()){
        // puts(">> H\n");
        tp_flag = 1;
    }

    //各个模式特有mute
    if (true == app_cur_task_check(APP_NAME_BT)){
        if ((get_call_status() == BT_CALL_ACTIVE) ||
            (get_call_status() == BT_CALL_OUTGOING) ||
            (get_call_status() == BT_CALL_ALERT) ||
            (get_call_status() == BT_CALL_INCOMING)) {
            //通话过程不允许mute pa
            tp_flag = 0;
        }

    }else if (true == app_cur_task_check(APP_NAME_MUSIC)){
        extern bool user_file_dec_is_pause(void);
        tp_flag = user_file_dec_is_pause();
    }else if(true == app_cur_task_check(APP_NAME_FM)){
        tp_flag = 0;
    }else if(true == app_cur_task_check(APP_NAME_LINEIN)){
        tp_flag = user_is_manual_mute();
    }

    //音量为0
    if(!app_var.music_volume){
        // puts(">> L\n");
        tp_flag = 1;
    }

    //提示音
    if(tone_get_status()){
        tp_flag = 0;
    }

    //mic 插入
    if(user_is_mic_online()){
        tp_flag = 0;
    }
    
    // printf(">>>>>>>>>>>  mute flag %d\n",tp_flag);
    if(tp_flag){
        user_pa_strl(PA_MUTE);
    }else{
        user_pa_strl(PA_UMUTE);
    }
    
    sys_hi_timeout_add(NULL,user_pa_service,tp_flag?10:1000);
}



void user_pa_init(void){
    // sys_timer_add(NULL, user_pa_service, 10);
    user_pa_service();
}