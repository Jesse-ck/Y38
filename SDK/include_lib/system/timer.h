#ifndef SYS_TIMER_H
#define SYS_TIMER_H


#include "typedef.h"
#include "generic/list.h"





struct static_sys_timer {
    void (*func)(void *priv);
    void *priv;
    u32 msec;
    u32 jiffies;
};


#define SYS_HI_TIMER_ADD(_func, _priv, _msec) \
	static struct static_sys_timer hi_timer sec(.hi_timer) = { \
		.func = _func, \
		.priv = _priv, \
		.msec = _msec, \
	}

extern struct static_sys_timer static_hi_timer_begin[];
extern struct static_sys_timer static_hi_timer_end[];

#define list_for_each_static_hi_timer(p) \
	for (p=static_hi_timer_begin; p<static_hi_timer_end; p++)



struct sys_cpu_timer {
    u8 busy;
    void *priv;
    void (*set)(u32 usec);
    void (*unset)();
};

#define DEFINE_SYS_CPU_TIMER(t) \
    struct sys_cpu_timer t sec(.sys_cpu_timer);

#define REGISTER_SYS_CPU_TIMER(t) \
    struct sys_cpu_timer t sec(.sys_cpu_timer)


extern struct sys_cpu_timer sys_cpu_timer_begin[];
extern struct sys_cpu_timer sys_cpu_timer_end[];

#define list_for_each_cpu_usec_timer(p) \
    for (p = sys_cpu_timer_begin; p < sys_cpu_timer_end; p++)



u16 sys_timer_add(void *priv, void (*func)(void *priv), u32 msec);

void sys_timer_del(u16);

u16 sys_timeout_add(void *priv, void (*func)(void *priv), u32 msec);

void sys_timeout_del(u16);

void sys_timer_re_run(u16 id);

void sys_timer_set_user_data(u16 id, void *priv);

void *sys_timer_get_user_data(u16 id);

// void sys_timer_schedule();

int sys_timer_modify(u16 id, u32 msec);

void sys_hi_timer_schedule();

u16 sys_hi_timer_add(void *priv, void (*func)(void *priv), u32 msec);

u16 sys_hi_timeout_add(void *priv, void (*func)(void *priv), u32 msec);

int sys_hi_timer_modify(u16, u32 msec);

int sys_hi_timeout_modify(u16, u32 msec);

void sys_hi_timer_del(u16);

void sys_hi_timeout_del(u16);

int sys_s_hi_timer_add(void *priv, void (*func)(void *priv), u32 msec);

int sys_s_hi_timerout_add(void *priv, void (*func)(void *priv), u32 msec);

int sys_s_hi_timer_modify(u16 id, u32 msec);

int sys_s_hi_timeout_modify(u16 id, u32 msec);

void sys_s_hi_timer_del(u16 t);

void sys_s_hi_timeout_del(u16 t);

int sys_usec_timer_add(void *priv, void (*func)(void *priv), u32 usec);

void sys_usec_timer_schedule(struct sys_cpu_timer *);

void sys_usec_timer_set(int _t, u32 usec);

void sys_usec_timer_del(int);

void sys_timer_dump_time(void);

u32 sys_timer_get_ms(void);

#endif


