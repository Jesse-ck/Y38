#include "app_api/app_debug_api.h"
#include "system/timer.h"
#include "system/includes.h"
#define  ENDLESS_LOOP_DEBUG_EN	0
#define  MUSIC_BP_DEBUG_EN		0

#if ENDLESS_LOOP_DEBUG_EN
static volatile u32 endless_loop_check_counter = 0;
static volatile u32 endless_loop_check_counter_last = (u32) - 1;
static volatile u8 endless_loop_flag = 0;
void endless_loop_debug(void)
{
    endless_loop_check_counter++;
}

static void endless_loop_debug_evt_notify(void *p)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_ENDLESS_LOOP_DEBUG;
    e.u.dev.event = 0;
    e.u.dev.value = 0;
    sys_event_notify(&e);
}

static void endless_loop_debug_show(void *p)
{
    u32 reti;//, reti;
    /* __asm__ volatile("%0 = rets":"=r"(rets)); */
    __asm__ volatile("%0 = reti":"=r"(reti));
    printf("warning endless loop!!! reti = %x\n", reti);
}

static void endless_loop_debug_check(void *p)
{
    if (endless_loop_flag == 1) {
        return ;
    }

    if (endless_loop_check_counter_last == endless_loop_check_counter) {
        endless_loop_flag = 1;
        sys_s_hi_timer_add(NULL, endless_loop_debug_show, 10); //4s
    }
    endless_loop_check_counter_last = endless_loop_check_counter;
}

void endless_loop_debug_int(void)
{
    sys_s_hi_timer_add(NULL, endless_loop_debug_evt_notify, 500); //4s
    sys_s_hi_timer_add(NULL, endless_loop_debug_check, 4000); //4s
}
#else//ENDLESS_LOOP_DEBUG_EN
void endless_loop_debug(void)
{

}
void endless_loop_debug_int(void)
{

}
#endif//ENDLESS_LOOP_DEBUG_EN

#if MUSIC_BP_DEBUG_EN

static u8 bp_debug_buf[2100];
static u32 bp_debug_size;
void pb_point_debug_save(void *buf, u32 size)
{
    if (size > sizeof(bp_debug_buf)) {
        return ;
    }
    /* printf("save bp buf &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n"); */
    memcpy(bp_debug_buf, (u8 *)buf, size);
    bp_debug_size = size;
}
void pb_point_debug_show(void)
{
    printf("bp_debug_size = %d\n", bp_debug_size);
    put_buf(bp_debug_buf, bp_debug_size);
}

#else
void pb_point_debug_save(void *buf, u32 size)
{

}

void pb_point_debug_show(void)
{

}
#endif//MUSIC_BP_DEBUG_EN



void exception_analyze_user()
{
    printf("exception_analyze_user:\n");
    pb_point_debug_show();
}
