#ifndef _USER_PA_H_
#define _USER_PA_H_

#define USER_PA_MUTE_IO  IO_PORTA_00
#define USER_PA_CLASS_IO  IO_PORTA_00

struct USER_PA_CTL_IO {
    char port_mute;
    char port_abd;
    void (*mute)(char mute);
    void (*abd)(char abd);
} ;

enum {
    PA_INIT,
    PA_CLASS_AB,
    PA_CLASS_D,
    PA_MUTE,
    PA_UMUTE,
    PA_POWER_OFF,
};

void user_pa_init(void);
void user_pa_strl(u8 cmd);
void user_pa_set_auto_mute(volatile u8 sys_automute);
void user_manual_mute(u8 cmd);
void user_mic_online(u8 cmd);
#endif