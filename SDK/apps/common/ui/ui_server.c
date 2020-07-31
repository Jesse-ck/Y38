#include "includes.h"
#include "ui/ui_api.h"
#include "ui/ui.h"
#include "os/os_api.h"
#include "typedef.h"
#if TCFG_UI_ENABLE

#if (TCFG_SPI_LCD_ENABLE)

#define UI_AUTO_TEST 0

#define UI_NO_ARG (-1)
#define UI_TASK_NAME "ui"

struct ui_server_env {
    u8 init: 1;
    u8 key_lock : 1;
};

static struct ui_server_env __ui_display = {0};

int key_is_ui_takeover()
{
    return __ui_display.key_lock;
}

void key_ui_takeover(u8 on)
{
    __ui_display.key_lock = !!on;
}

int ui_show_main(int id)
{
    int ret = 0;
    u8 count = 0;
_wait:
    if (!__ui_display.init) {
        if (count > 100) {
            return -1;
        }
        count++;
        os_time_dly(1);
        goto _wait;

    }
    count = 0;

    if (__ui_display.init) {

        printf("~~~~~~~~~~~~~~ !!!!!!!! %s %d \n", __FUNCTION__, __LINE__);
__try:

            ret = os_taskq_post_type(UI_TASK_NAME, UI_MSG_SHOW, 1, &id);
        if (ret) {
            if (count > 10) {
                return -1;
            }
            count++;
            os_time_dly(1);
            goto __try;
        }
    }
    return 0;


}

int ui_hide_main(int id)
{


    int ret = 0;
    u8 count = 0;

_wait:
    if (!__ui_display.init) {
        if (count > 10) {
            return -1;
        }
        count++;
        os_time_dly(3);
        goto _wait;

    }
    count = 0;

    if (__ui_display.init) {

__try:
            ret = os_taskq_post_type(UI_TASK_NAME, UI_MSG_HIDE, 1, &id);
        if (ret) {
            if (count > 10) {
                return -1;
            }
            count++;
            os_time_dly(1);
            goto __try;
        }
    }
    return 0;

}


int ui_server_msg_post(int argc, ...)
{
    /* if (!strcmp(os_current_task(), UI_TASK_NAME)) { */
    /* return 1; */
    /* } */
    int ret = 0;
    int argv[8];
    u8 count = 0;

_wait:
    if (!__ui_display.init) {
        if (count > 10) {
            return -1;
        }
        count++;
        os_time_dly(1);
        goto _wait;

    }
    count = 0;
    if (__ui_display.init) {
        va_list argptr;
        va_start(argptr, argc);
        for (int i = 0; i < argc; i++) {
            argv[i] = va_arg(argptr, int);
        }
        va_end(argptr);

__try:
            ret = __os_taskq_post(UI_TASK_NAME, UI_MSG_OTHER, argc, argv);
        if (ret) {
            if (count > 10) {
                return -1;
            }
            count++;
            os_time_dly(1);
            goto __try;
        }
    }
    return 0;
}


int ui_key_msg_post(int msg)
{
    /* if (!strcmp(os_current_task(), UI_TASK_NAME)) { */
    /* return 1; */
    /* } */
    int ret = 0;
    u8 count = 0;
_wait:
    if (!__ui_display.init) {
        if (count > 10) {
            return -1;
        }
        count++;
        os_time_dly(1);
        goto _wait;

    }
    count = 0;
    if (__ui_display.init) {

__try:
            ret = __os_taskq_post(UI_TASK_NAME, UI_MSG_KEY, 1, &msg);
        if (ret) {
            if (count > 10) {
                return -1;
            }
            count++;
            os_time_dly(1);
            goto __try;
        }
    }
    return 0;
}


static void ui_event_handler(struct sys_event *event, void *priv)
{
    struct element_key_event e;
    struct element_touch_event t;

    switch (event->type) {
    case SYS_KEY_EVENT:
        /* e.event = event->u.key.event; */
        /* e.value = event->u.key.value; */
        /* e.private_data = &event->u.key; */
        /* ui_event_onkey(&e); */

        printf("\nevent=%d\n", t.event);
        break;
    case SYS_TOUCH_EVENT:
        /* t.event = event->u.touch.event; */
        /* t.pos.x = event->u.touch.pos.x; */
        /* t.pos.y = event->u.touch.pos.y; */
        /* printf("\nevent=%d\n",t.event); */
        /* ui_event_ontouch(&t); */
        break;
    case SYS_DEVICE_EVENT:
        break;
    default:
        break;
    }

}


const char *str_substr_iter(const char *str, char delim, int *iter)
{
    const char *substr;

    ASSERT(str != NULL);

    substr = str + *iter;
    if (*substr == '\0') {
        return NULL;
    }

    for (str = substr; *str != '\0'; str++) {
        (*iter)++;
        if (*str == delim) {
            break;
        }
    }

    return substr;
}


static int do_msg_handler(const char *msg, va_list *pargptr, int (*handler)(const char *, u32))
{
    int ret = 0;
    int width;
    int step = 0;
    u32 arg = 0x5678;
    int m[16];
    char *t = (char *)&m[3];
    va_list argptr = *pargptr;

    if (*msg == '\0') {
        handler((const char *)' ', 0);
        /* os_taskq_post_msg("ui_task", 4, UI_MSG_MSG, handler, 0, '\0'); */
        return 0;
    }

    while (*msg && *msg != ',') {
        switch (step) {
        case 0:
            if (*msg == ':') {
                step = 1;
            }
            break;
        case 1:
            switch (*msg) {
            case '%':
                msg++;
                if (*msg >= '0' && *msg <= '9') {
                    if (*msg == '1') {
                        arg = va_arg(argptr, int) & 0xff;
                    } else if (*msg == '2') {
                        arg = va_arg(argptr, int) & 0xffff;
                    } else if (*msg == '4') {
                        arg = va_arg(argptr, int);
                    }
                } else if (*msg == 'p') {
                    arg = va_arg(argptr, int);
                }
                /* m[0] = UI_MSG_MSG; */
                /* m[1] = (int)handler; */
                m[2] = arg;

                handler((char *)&m[3], m[2]);
                /* os_taskq_post_type("ui_task", Q_MSG, 3 + ((t - (char *)&m[3]) + 3) / 4, m); */
                t = (char *)&m[3];
                break;
            case '=':
                *t = '\0';
                break;
            case ' ':
                break;
            default:
                *t++ = *msg;
                break;
            }
            break;
        }

        msg++;
    }

    *pargptr = argptr;

    return ret;
}


int ui_message_handler(int id, const char *msg, va_list argptr)
{
    int iter = 0;
    const char *str;
    const struct uimsg_handl *handler;
    struct window *window = (struct window *)ui_core_get_element_by_id(id);

    if (!window || !window->private_data) {
        return -EINVAL;
    }

    handler = (const struct uimsg_handl *)window->private_data;

    while ((str = str_substr_iter(msg, ',', &iter)) != NULL) {
        for (; handler->msg != NULL; handler++) {
            if (!memcmp(str, handler->msg, strlen(handler->msg))) {
                do_msg_handler(str + strlen(handler->msg), &argptr, handler->handler);
                break;
            }
        }
    }

    return 0;
}




static void ui_task(void *p)
{
    int msg[32];
    int ret;
    struct element_key_event e = {0};
    struct ui_style style;
    style.file = NULL;


    ui_framework_init(p);
    ui_set_style_file(&style);

#if(UI_AUTO_TEST == 1)
    void lcd_ui_auto_test();
    lcd_ui_auto_test();
#endif
    /* register_sys_event_handler(SYS_KEY_EVENT | SYS_TOUCH_EVENT, */
    /* 100, NULL, ui_event_handler); */


    __ui_display.init = 1;

    while (1) {
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)); //500ms_reflash
        if (ret != OS_TASKQ) {
            continue;
        }

#if(UI_AUTO_TEST == 1)
#else
        switch (msg[0]) { //action
        case UI_MSG_EXIT:
            os_sem_post((OS_SEM *)msg[1]);
            os_time_dly(10000);
            break;
        case UI_MSG_OTHER:
            ui_message_handler(msg[1], (const char *)msg[2], (void *)&msg[3]);
            break;
        case UI_MSG_KEY:
            e.value = msg[1];
            ui_event_onkey(&e);
            break;
        case UI_MSG_SHOW:
            printf("~~~~~~~~~~~~~~ !!!!!!!! %s %d %x\n", __FUNCTION__, __LINE__, msg[1]);
            ui_show(msg[1]);
            break;
        case UI_MSG_HIDE:
            printf("~~~~~~~~~~~~~~ !!!!!!!! %s %d %x\n", __FUNCTION__, __LINE__, msg[1]);
            ui_hide(msg[1]);
            break;
        default:
            break;
        }
#endif

    }
}

int ui_lcd_init(void *arg)
{
    int err = 0;
    err = task_create(ui_task, arg, UI_TASK_NAME);
    return err;
}


#endif
#endif
