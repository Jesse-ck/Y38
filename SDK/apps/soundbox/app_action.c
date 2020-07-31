#include "app_action.h"
#include "app_main.h"
#include "app_core.h"
#include "key_event_deal.h"
#include "system/device/vm.h"
#include "os/os_api.h"
#include "system/timer.h"
#include "tone_player.h"
#include "ui/ui_api.h"

#define LOG_TAG_CONST       APP_ACTION
#define LOG_TAG             "[APP_ACTION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define APP_MEM_STAS_DEBUG_EN		0

#define APP_SWITCH_MSG				(-1)
#define APP_NEXT_MSG				(-2)

struct app_handle {
    struct intent last_it;
    struct intent it;
    struct intent target_it;
    volatile u8 tone_busy;
    volatile u8 next_busy;
    u16	   switch_tm;
    u16	   next_tm;
    int (*user_msg)(int msg, int argc, int *argv);
    void *end_cb_priv;
    u8  end_cb_maigc;
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    u8  tone_tws_cmd;
    u16 tone_tws_to;
#endif
};
static struct app_handle g_app_hdl = {0};
#define __this	(&g_app_hdl)

/*此列表用户可以通过配置调整模式切换的顺序*/
static const char *app_next_list[] = {
    APP_NAME_BT		,
    APP_NAME_MUSIC	,
    APP_NAME_FM		,
    APP_NAME_RECORD	,
    APP_NAME_LINEIN	,
    APP_NAME_PC	,
    APP_NAME_RTC	,
    APP_NAME_SPDIF	,
};


static int app_switch_do(struct intent *it)
{
#if APP_MEM_STAS_DEBUG_EN
    printf("\n\t");
    printf("\n\t");
    extern void mem_stats(void);
    mem_stats();
    //内存使用情况:
    //Current free heap:当前剩余空间
    //mum ever free heapa:曾经剩余最少空间
    //physics memory size: 当前未使用的物理内存
    printf("\n\t");
    printf("\n\t");
#endif//APP_MEM_STAS_DEBUG_EN

    __this->next_busy = 0;
    __this->tone_busy = 0;

    return start_app(it);
}


int app_cur_task_check(char *name)
{
    if (__this->tone_busy == 0) {
        struct application *app = get_current_app();
        if (app && (!strcmp(app->name, name))) {
            return true;
        }
    }
    return false;
}

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
extern u8 get_total_connect_dev(void);
int app_mode_not_move(char *name)
{
    if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
        if ((get_total_connect_dev() == 0) && (0
                                               || (!strcmp(name, APP_NAME_MUSIC))
                                               || (!strcmp(name, APP_NAME_FM))
                                               || (!strcmp(name, APP_NAME_RECORD))
                                               || (!strcmp(name, APP_NAME_LINEIN))
                                               || (!strcmp(name, APP_NAME_RTC))
                                               || (!strcmp(name, APP_NAME_PC))
                                               || (!strcmp(name, APP_NAME_SPDIF))
                                              )
           ) {
            // 蓝牙没有连接，不切换
            return true;
        }
    }
    return false;
}
#endif

static void app_mode_tone_play_end(void *priv)
{
    if (__this->end_cb_maigc != (u8)priv) {
        printf("cb maigc:%d, priv:%d \n", __this->end_cb_maigc, (u8)priv);
        return ;
    }
    if (__this->tone_busy == 0) {
        printf("------------------------------------------------stop, when playing\n");
        /* return ;		 */

    }

    struct intent *it = (struct intent *)__this->end_cb_priv;
    /* ASSERT(it, "app mode tone param err!!\n"); */
    printf("notice end, start new app == %s\n", it->name);

    if (it == NULL) {
        return ;
    }

    if (it->name == NULL) {
        return ;
    }


    app_switch_do(it);

///再次检查下， 模式是否在线
    struct application *p;
    struct application_reg *app_reg = NULL;
    u8 find = 0;
    list_for_each_app(p) {
        if (!strcmp(p->name, it->name)) {
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
            if (app_mode_not_move(p->name) == true) {
                break;
            }
#endif
            app_reg = (struct application_reg *)p->private_data;
            ASSERT(app_reg, "app task name = %s, has no private_data err tone end\n", p->name);
            if (app_reg->enter_check) {
                if (!app_reg->enter_check()) {
                    printf("tone end, this app no online, name = %s\n", it->name);
                } else {
                    find = 1;
                }
            } else {
                //没有注册skip_check, 默认会进入该模式
                find = 1;
            }
            break;
        }
    }
///如果模式不在线， 自动切换到下一个模式
    if (find == 0) {
        printf("tone end, this app not online, name = %s\n", it->name);
        app_task_next();
    }
}

#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
extern int tws_api_sync_call_by_uuid(int uuid, int priv, int delay_ms);

#ifdef CONFIG_TONE_LOCK_BY_BT_TIME

extern void tone_out_start_by_bt_time(u32 bt_time);
extern u32 audio_bt_time_read(u16 *bt_phase, s32 *bt_bitoff);

void app_mode_tone_play_by_tws(int cmd);
void app_mode_tone_tws_to(void *priv);

static void app_mode_tws_callback_func(int cmd, u32 tmr)
{
    u32 cur_tmr = audio_bt_time_read(NULL, NULL);
    printf("cur bt:%d, tmr:%d, cmd:%d \n", cur_tmr, tmr, cmd);
    tone_out_start_by_bt_time(tmr);
#if 0
    app_mode_tone_play_by_tws(cmd);
#else
    local_irq_disable();
    if (__this->tone_tws_to) {
        sys_timeout_del(__this->tone_tws_to);
        __this->tone_tws_to = 0;
    }
    __this->tone_tws_to = sys_timeout_add((void *)cmd, app_mode_tone_tws_to, 50);
    local_irq_enable();
    if (!__this->tone_tws_to) {
        app_mode_tone_play_by_tws(cmd);
    }
#endif
}

struct app_mode_tws {
    u8  cmd;
    u32 tmr;
};

static void app_mode_tws_func(void *_data, u16 len, bool rx)
{
    struct app_mode_tws tws;
    if (rx) {
        memcpy(&tws, _data, sizeof(tws));
        /* printf("........... %d,%d     \n\n", tws.cmd, tws.tmr); */

        int argv[4];
        argv[0] = (int)app_mode_tws_callback_func;
        argv[1] = 2;
        argv[2] = (int)tws.cmd;
        argv[3] = (int)tws.tmr;
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(argv), argv);
    }
}

REGISTER_TWS_FUNC_STUB(app_mode_stub) = {
    .func_id = TWS_FUNC_ID_APP_MODE,
    .func    = app_mode_tws_func,
};
#endif /* CONFIG_TONE_LOCK_BY_BT_TIME */

void app_mode_tone_play_by_tws(int cmd)
{
    if ((is_tws_active_device()) && (__this->tone_tws_cmd != cmd)) {
        log_e("twscmd:%d, cmd:%d ", __this->tone_tws_cmd, cmd);
        return ;
    }
    u8 play_err = 0;
    struct application *p;
    struct application_reg *app_reg = NULL;
    list_for_each_app(p) {
        app_reg = (struct application_reg *)p->private_data;
        if (app_reg && (app_reg->tone_tws_cmd == cmd)) {
#ifdef CONFIG_TONE_LOCK_BY_BT_TIME
            __this->end_cb_maigc++;
            __this->end_cb_priv = (void *)&__this->it;
#endif
            if (!is_tws_active_device()) {
                /* tone_play(app_reg->tone_name, 1); */
                mode_tone_play(app_reg->tone_name, NULL, NULL);
                break;
            }
            if (__this->tone_busy) {
#if 0
                mode_tone_stop();
#else
                mode_tone_play_set_no_end();
#endif
            }
            __this->tone_busy = 1;
            if (!mode_tone_play(app_reg->tone_name, app_mode_tone_play_end, (void *)__this->end_cb_maigc)) {
                if (app_reg->tone_prepare) {
                    app_reg->tone_prepare();
                } else {
#if TCFG_UI_ENABLE
                    ui_set_tmp_menu(MENU_WAIT, 0, 0, NULL);
#endif
                }
                printf("tone play start ok\n");
            } else {
                play_err = 1;
                printf("tone play start fail\n");
            }
            local_irq_disable();
            if (__this->tone_tws_to) {
                sys_timeout_del(__this->tone_tws_to);
                __this->tone_tws_to = 0;
            }
            local_irq_enable();
            break;
        }
    }
    if (play_err) {
        app_switch_do(&__this->it);
    }
}
void app_mode_tone_tws_to(void *priv)
{
    __this->tone_tws_to = 0;
    app_mode_tone_play_by_tws((int)priv);
}
#endif


int __app_task_switch(const char *name, int action, void *param)
{
    int ret = -EFAULT;
    struct application *p;
    struct application *cur;
    u8 i, j;
    u8 find = 0;
    u8 app_max = sizeof(app_next_list) / sizeof(app_next_list[0]);
    struct application_reg *app_reg = NULL;

    if (!name) {
        return ret;
    }

    list_for_each_app(p) {
        if (!strcmp(p->name, name)) {
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
            if (app_mode_not_move(p->name) == true) {
                break;
            }
#endif
            app_reg = (struct application_reg *)p->private_data;
            ASSERT(app_reg, "app task name = %s, has no private_data err\n", p->name);
            if (app_reg->enter_check) {
                if (!app_reg->enter_check()) {
                    printf("this app no online, name = %s\n", name);
                } else {
                    find = 1;
                }
            } else {
                //没有注册skip_check, 默认会进入该模式
                find = 1;
            }
            break;
        }
    }

    if (find == 0) {
        printf("no this app, name = %s\n", name);
        return ret;
    }


    if (__this->tone_busy) {
        printf("break app notice ____________________________________________________________\n");
        __this->tone_busy = 0;
#if 0
        mode_tone_stop();//这里停止提示音， 不应该发出结束回调
#else
        mode_tone_play_set_no_end();
#endif
    } else {
        init_intent(&__this->it);
        cur = get_current_app();
        if (cur) {
            if (!strcmp(cur->name, name)) {
                printf("the same app , name = %s\n", name);
                __this->next_busy = 0;
                return ret;
            }
            //退出当前模式
            __this->last_it.name = cur->name;
            __this->last_it.action = ACTION_APP_MAIN;
            __this->last_it.exdata = 0;


            struct application_reg *cur_reg = (struct application_reg *)cur->private_data;
            if (cur_reg->exit_check) {
                __this->target_it.name = name;
                __this->target_it.action = action;
                __this->target_it.exdata = (u32)param;
                if (0 == cur_reg->exit_check()) {
                    //可以退出模式了，但是不需要跑ACTION_BACK流程
                } else {
                    ///暂时还不能退出当前模式
                    __this->next_busy = 0;
                    return 0;
                }
            } else {
                __this->it.action = ACTION_BACK;
                __this->it.name = cur->name;
                __this->it.exdata = 0;
                start_app(&__this->it);
            }
        }
    }

//整理VM
    vm_check_all(0);

    __this->it.name = name;
    __this->it.action = action;
    __this->it.exdata = (u32)param;
    //切换到下一个模式
    if (app_reg) {
        local_irq_disable();
        __this->user_msg = app_reg->user_msg;
        local_irq_enable();

        if (app_reg->tone_name) {
            u8 need_tone = 1;
            if ((app_reg->tone_play_check != NULL) && (app_reg->tone_play_check() == 0)) {
                need_tone = 0;
            }

            if (need_tone) {
                __this->end_cb_maigc++;
                __this->end_cb_priv = (void *)&__this->it;
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
                int state = tws_api_get_tws_state();
                if ((state & TWS_STA_SIBLING_CONNECTED) && (app_reg->tone_tws_cmd)) {
                    if (is_tws_active_device()) {
                        local_irq_disable();
                        if (__this->tone_tws_to) {
                            sys_timeout_del(__this->tone_tws_to);
                            __this->tone_tws_to = 0;
                        }
                        local_irq_enable();
                        __this->tone_tws_cmd = app_reg->tone_tws_cmd;
#ifdef CONFIG_TONE_LOCK_BY_BT_TIME
                        struct app_mode_tws tws = {0};
                        tws.cmd = __this->tone_tws_cmd;
                        tws.tmr = audio_bt_time_read(NULL, NULL) + 800;
                        tone_out_start_by_bt_time(tws.tmr);
                        printf("bt_time:%d,\n", tws.tmr);
                        int err = tws_api_send_data_to_sibling((u8 *)&tws, sizeof(struct app_mode_tws), TWS_FUNC_ID_APP_MODE);
                        if (err) {
                            printf("app mode tws send err \n");
                        }
#else
                        /* __this->tone_tws_to = sys_timeout_add((void *)__this->tone_tws_cmd, app_mode_tone_tws_to, TWS_SYNC_TIME_DO / 2 + 100); */
                        tws_api_sync_call_by_uuid('T', __this->tone_tws_cmd, TWS_SYNC_TIME_DO / 4);
                        return 0;
#endif
                    }
                }
#endif
                __this->tone_busy = 1;
                if (!mode_tone_play(app_reg->tone_name, app_mode_tone_play_end, (void *)__this->end_cb_maigc)) {
                    if (app_reg->tone_prepare) {
                        app_reg->tone_prepare();
                    } else {
#if TCFG_UI_ENABLE
                        ui_set_tmp_menu(MENU_WAIT, 0, 0, NULL);
#endif
                    }

                    printf("tone play start ok\n");
                    return 0;
                } else {
                    printf("tone play start fail\n");
                }

            }
        }
    }
    return app_switch_do(&__this->it);
}


static void app_task_switch_timout(void *priv)
{
    struct intent *target = (struct intent *)priv;

    if (__this->switch_tm) {
        sys_timeout_del(__this->switch_tm);
        __this->switch_tm = 0;
    }

    if (target == NULL) {
        return ;
    }

    bool ret = app_task_msg_post(APP_SWITCH_MSG, 3, (int)target->name, (int)target->action, (int)target->exdata);
    if (ret == false) {
        __this->switch_tm = sys_timeout_add((void *)target, app_task_switch_timout, 10);
    } else {
        free(target);
    }
}

int app_task_switch(const char *name, int action, void *param)
{
    bool ret = app_task_msg_post(APP_SWITCH_MSG, 3, (int)name, (int)action, (int)param);
    if (ret == false) {
        struct intent *target = (struct intent *)zalloc(sizeof(struct intent));
        ASSERT(target);

        target->name = name;
        target->action = action;
        target->exdata = (u32)param;
        __this->switch_tm = sys_timeout_add((void *)target, app_task_switch_timout, 10);
    }
    return 0;
}

int app_task_switch_last(void)
{
    return app_task_switch(__this->last_it.name, __this->last_it.action, NULL);
}

///有注册exit_check才调用， 一般模式不调用， 属于蓝牙模式特殊处理
int app_task_switch_target(void)
{
    return app_task_switch(__this->target_it.name, __this->target_it.action, (void *)__this->target_it.exdata);
}

int app_task_target_check(const char *name)
{
    if (__this->target_it.name && strcmp(__this->target_it.name, name)) {
        return 0;
    }
    return -1;
}

static int __app_task_next(void)
{
    int ret = -EFAULT;
    u8 i, j;
    u8 find_next = 0;
    u8 app_max = sizeof(app_next_list) / sizeof(app_next_list[0]);
    if (app_max < 2) {
        printf("no more app can switch\n");
        return ret;
    }
    struct intent it;
    struct application *p;
    const char *cur_app_name = NULL;



    init_intent(&it);
    if (__this->tone_busy) {
        cur_app_name = __this->it.name;
    } else {
        p = get_current_app();
        if (!p) {
            return ret;
        }
        cur_app_name = p->name;
    }


    //确定当前模式位置
    for (i = 0; i < app_max; i++) {
        if (!strcmp(cur_app_name, app_next_list[i])) {
            break;
        }
    }

    //确定下一个模式的位置
    j = 0;
    do {
        i++;
        if (i >= app_max) {
            i = 0;
        }

        list_for_each_app(p) {
            if (!strcmp(p->name, app_next_list[i])) {
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
                if (app_mode_not_move(p->name) == true) {
                    break;
                }
#endif
                struct application_reg *app_reg = (struct application_reg *)p->private_data;
                ASSERT(app_reg, "app task name = %s, has no private_data err\n", p->name);
                if (app_reg->enter_check) {
                    if (!app_reg->enter_check()) {
                        j++;
                        if (j >= (app_max - 1)) {
                            break;
                        } else {
                            ///跳过此模式
                            continue;
                        }
                    } else {
                        find_next = 1;
                        break;
                    }
                } else {
                    //没有注册skip_check, 默认会进入该模式
                    find_next = 1;
                    break;
                }
            }
        }
    } while (find_next == 0);

    if (find_next) {
        it.name = app_next_list[i];
        it.action = ACTION_APP_MAIN;
        return __app_task_switch(it.name, it.action, NULL);
    }

    printf("no other app next!!, cur app name = %s\n", p->name);
    return false;
}


static void app_task_next_do(void *priv)
{
    bool ret;
    if (__this->next_tm) {
        sys_timeout_del(__this->next_tm);
        __this->next_tm = 0;
    }

    ret = app_task_msg_post(APP_NEXT_MSG, 1, 0);
    if (ret == false) {
        __this->next_tm = sys_timeout_add(NULL, app_task_next_do, 10);
    }
}

int app_task_next(void)
{
    if (__this->next_busy) {
        printf("last app next not finish!!!!\n");
        return -1;
    }
    __this->next_busy = 1;
    printf("app_task_next >>>>>>>>>>>>>>>>\n");

    app_task_next_do(NULL);
    return 0;
}


void __attribute__((weak))app_common_user_msg_deal(int msg, int argc, int *argv)
{

}

static int app_action_event_handler(int msg, int argc, int *argv)
{
    /* printf("msg = %d, argc = %d, argv:", msg, argc); */
    /* for(int i=0; i<argc; i++) */
    /* { */
    /* printf("%d ", argv[i]);		 */
    /* } */
    if (msg == APP_SWITCH_MSG) {
        printf("__app_task_switch， name = %s, action = %d, param = %x\n", (const char *)argv[0], (int)argv[1], argv[2]);
        __app_task_switch((const char *)argv[0], (int)argv[1], (void *)argv[2]);
        return 0;
    }

    if (msg == APP_NEXT_MSG) {
        __app_task_next();
        return 0;
    }

    u8 filter = 0;
    if (__this->user_msg) {
        if (__this->user_msg(msg, argc, argv)) {
            filter = 1;
        }
    }

    if (!filter) {
        app_common_user_msg_deal(msg, argc, argv);
    }

    return 0;
}

#define  APP_ATICON_MSG_VAL_MAX			8
bool app_task_msg_post(int msg, int argc, ...)
{
    int argv[APP_ATICON_MSG_VAL_MAX] = {0};
    bool ret = true;
    va_list argptr;
    va_start(argptr, argc);

    if (argc > (APP_ATICON_MSG_VAL_MAX - 3)) {
        printf("%s, msg argc err\n", __FUNCTION__);
        ret = false;
    } else {
        argv[0] = (int)app_action_event_handler;
        argv[2] = msg;
        for (int i = 0; i < argc; i++) {
            argv[i + 3] = va_arg(argptr, int);
        }

        if (argc >= 2) {
            argv[1] = argc + 1;
        } else {
            argv[1] = 3;
            argc = 3;//不够的， 发够三个参数
        }
        int r = os_taskq_post_type("app_core", Q_CALLBACK, argc + 3, argv);
        if (r) {
            printf("app_next post msg err %x\n", r);
            ret = false;
        }
    }

    va_end(argptr);

    return ret;
}


