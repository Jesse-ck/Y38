#ifndef __USER_MSG_H__
#define __USER_MSG_H__


///用户自动消息定义, 注意此处是非按键消息

enum {
    USER_MSG_SYS_START = -0x1000,
    USER_MSG_SYS_MIXER_RECORD_SWITCH,
    USER_MSG_SYS_MIXER_RECORD_STOP,

    ///用户自定义消息从以下开始定义
    USER_MSG_TEST = 0x0,
    USER_MSG_SLIDE1 = 0x1,//滑动变阻器消息
    USER_MSG_SLIDE2 = 0x2//滑动变阻器消息
};


#endif//__USER_MSG_H__

