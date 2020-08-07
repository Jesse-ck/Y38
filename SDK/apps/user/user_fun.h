#ifndef _USER_FUN_H_
#define _USER_FUN_H_
#include "system/includes.h"
#include "user_fun_config.h"
#include "app_config.h"
#include "user_dev_check.h"



void user_fun_io_init(void);
void user_power_off(void);
void user_fun_init(void);
u8 user_key_mapping(u8 key);
void user_sys_auto_mute(bool cmd);
void jl_power_on_task_set(const u8 * name,const u8 * logo);
u8 jl_power_on_task_goto(void);
#endif