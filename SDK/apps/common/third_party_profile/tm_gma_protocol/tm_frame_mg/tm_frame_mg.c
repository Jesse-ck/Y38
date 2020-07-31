#include "tm_frame_mg.h"
#include "generic/lbuf.h"
//#include "irq_api.h"
#include "tm_gma_hw_driver.h"
#include "uart.h"
//#include "sdk_cfg.h"
#include "btstack/frame_queque.h"
#include "gma_include.h"

#if (GMA_EN)

#define log_info printf
#define TM_MEM_USED_MALLOC		0

static void tm_send_process_init(void);
static void tm_send_process_exit(void);
static void tm_send_process_clear(void);
void tm_send_process_resume(void);

#define TM_AUDIO_POOL_SIZE (1024)
#define TM_CMD_POOL_SIZE (1024)

static volatile u8 tm_queque_busy = 0;
struct __frame_mg_ctl {
    //queque
    TM_QUEQUE audio_data_queque;
    TM_QUEQUE cmd_data_queque;
    //bt hardware interface
    int (*send_raw_audio_data)(_uint8 *buf, _uint16 len);
    int (*send_raw_cmd)(_uint8 *buf,  _uint16 len);
    int (*should_send)(_uint16 len);
#if (GMA_OTA_EN)
    //ota
    int (*ota_process)(int msg);
#endif
    //memory
///cmd
    _uint32 tm_cmd_pool[TM_CMD_POOL_SIZE / 4];
    struct lbuff_head  *tm_cmd_pool_ptr;
///audio
    _uint32 tm_audio_pool[TM_AUDIO_POOL_SIZE / 4];
    struct lbuff_head  *tm_audio_pool_ptr;
///queque busy
};
#if (TM_MEM_USED_MALLOC)
static struct __frame_mg_ctl *_frame_mg_ctl = NULL;
#define __this (_frame_mg_ctl)
#else
static struct __frame_mg_ctl _frame_mg_ctl;
#define __this (&_frame_mg_ctl)
#endif

/********************************************************
 * 					tm bt moudle
 * *****************************************************/

static void tm_should_send_register(int (*func)(_uint16 len))
{
    gma_hw_api.ENTER_CRITICAL();
    __this->should_send = func;
    gma_hw_api.EXIT_CRITICAL();
}

static void tm_send_raw_audio_data_register(int (*func)(uint8_t *buf, _uint16 len))
{
    gma_hw_api.ENTER_CRITICAL();
    __this->send_raw_audio_data = func;
    gma_hw_api.EXIT_CRITICAL();
}

static void tm_send_raw_cmd_register(int (*func)(uint8_t *buf, _uint16 len))
{
    gma_hw_api.ENTER_CRITICAL();
    __this->send_raw_cmd = func;
    gma_hw_api.EXIT_CRITICAL();
}

static int tm_should_send(_uint32 len)
{
    if (__this->should_send) {
        return __this->should_send(len);
    }

    return 0;
}

static int tm_send_raw_audio_data(uint8_t *buf, uint32_t len)
{
    //log_info(">>>>>bt audio send : \n");
    //printf_buf(buf, len);

    if (__this->send_raw_audio_data) {
        return __this->send_raw_audio_data(buf, len);
    }

    return (-1);
}

static int tm_send_raw_cmd(uint8_t *buf,  uint32_t len)
{
    //log_info(">>>>>bt cmd send : \n");
    //printf_buf(buf, len);
    if (__this->send_raw_cmd) {
        return __this->send_raw_cmd(buf, len);
    }

    return (-1);
}

/********************************************************
 * 					tm memory moudle
 * *****************************************************/

void tm_cmd_malloc_lbuf_init(void)
{
    gma_hw_api.ENTER_CRITICAL();
    __this->tm_cmd_pool_ptr = lbuf_init(__this->tm_cmd_pool, sizeof(__this->tm_cmd_pool), 4, 0);
    gma_hw_api.EXIT_CRITICAL();
}

void *tm_cmd_malloc_lbuf(void *buf, _uint32 sz)
{
    /*json pool init*/
    if (__this->tm_cmd_pool_ptr == NULL) {
        gma_hw_api.ENTER_CRITICAL();
        __this->tm_cmd_pool_ptr = lbuf_init(__this->tm_cmd_pool, sizeof(__this->tm_cmd_pool), 4, 0);
        gma_hw_api.EXIT_CRITICAL();
    }

    if (__this->tm_cmd_pool_ptr == NULL) {
        return NULL;
    }

    void *object = lbuf_alloc(__this->tm_cmd_pool_ptr, sz);

    if (object) {
        memset((_uint8 *)object, 0, sz);
    } else {
        //ASSERT(0, "tm malloc null");
    }

    ///printf("malloc object:%x size:%d \n",(u32)object,sz);
    return object;
}

void tm_audio_malloc_lbuf_init(void)
{
    gma_hw_api.ENTER_CRITICAL();
    __this->tm_audio_pool_ptr = lbuf_init(__this->tm_audio_pool, sizeof(__this->tm_audio_pool), 4, 0);
    gma_hw_api.EXIT_CRITICAL();
}

void *tm_audio_malloc_lbuf(void *buf, _uint32 sz)
{
    /*json pool init*/
    if (__this->tm_audio_pool_ptr == NULL) {
        gma_hw_api.ENTER_CRITICAL();
        __this->tm_audio_pool_ptr = lbuf_init(__this->tm_audio_pool, sizeof(__this->tm_audio_pool), 4, 0);
        gma_hw_api.EXIT_CRITICAL();
    }

    if (__this->tm_audio_pool_ptr == NULL) {
        return NULL;
    }

    void *object = lbuf_alloc(__this->tm_audio_pool_ptr, sz);

    if (object) {
        memset((_uint8 *)object, 0, sz);
    } else {
        //ASSERT(0, "tm malloc null");
    }

    ///printf("malloc object:%x size:%d \n",(u32)object,sz);
    return object;
}


static void free_lbuf(void *ptr)
{
    //printf("free object:%x \n",(u32)ptr);
    lbuf_free(ptr);
}

/********************************************************
 * 					tm mutex moudle
 * *****************************************************/
#define semlock_read(v) (*(volatile int *)&(v)->counter)
#define semlock_set(v,i) (((v)->counter) == (i))
#define TM_MUTEX_P(mutex_id)      gma_hw_api.ENTER_CRITICAL()//tm_mute_p(mutex_id)
#define TM_MUTEX_V(mutex_id)      gma_hw_api.EXIT_CRITICAL()//tm_mute_v(mutex_id)
static void tm_mute_p(semlock_t mute)
{
    if (semlock_read(&mute) == TRUE) {
        ASSERT(0, "%s: tm mute fail!", __func__);
    }
    semlock_set(&mute, TRUE);
}

static void tm_mute_v(semlock_t mute)
{
    semlock_set(&mute, FALSE);
}

/********************************************************
 * 					tm queque moudle
 * *****************************************************/
/*queque 不为空调用者保证 */
static int tm_frame_queque_is_empty(TM_QUEQUE *queque)
{
    int ret = 1;

    if (queque->head == NULL && queque->tail == NULL) {
        ret = 1;
    } else {
        ret = 0;
    }

    return ret;
}

int tm_frame_push_queque(TM_QUEQUE *queque, TM_SEND_FRAME *frame)
{

    if (queque == NULL) {
        log_info("frame_push_queque\n");
        return -1;
    }

    TM_MUTEX_P(queque->mutex);
    if (tm_frame_queque_is_empty(queque)) {
        queque->head = frame;
        queque->tail = frame;
        queque->depth = 1;
        frame->next = NULL;

    } else {
        frame->next = queque->head;
        queque->head = frame;
        queque->depth++;

    }

    TM_MUTEX_V(queque->mutex);

    tm_send_process_resume();
    return 0;
}

static TM_SEND_FRAME *tm_frame_pop_queque(TM_QUEQUE *queque)
{

    if (queque == NULL) {
        return NULL;
    }

    TM_MUTEX_P(queque->mutex);

    TM_SEND_FRAME *frame = queque->head;
    TM_SEND_FRAME *ret = NULL;

    if (tm_frame_queque_is_empty(queque)) {
        ret = NULL;
        goto __END;
    }

    if (queque->head == queque->tail) {
        ret = queque->head;
        queque->head = NULL;
        queque->tail = NULL;
        queque->depth = 0;
        goto __END ;
    }

    while (frame->next != queque->tail) {
        frame = frame->next;
    }

    ret = queque->tail ;
    frame->next = NULL;
    queque->tail = frame;
    queque->depth--;

__END:
    TM_MUTEX_V(queque->mutex);
    return ret;
}

static TM_SEND_FRAME *tm_frame_pop_queque_alloc(TM_QUEQUE *queque)
{

    if (queque == NULL) {
        return NULL;
    }

    TM_MUTEX_P(queque->mutex);

    TM_SEND_FRAME *frame = queque->head;
    TM_SEND_FRAME *ret = NULL;

    if (tm_frame_queque_is_empty(queque)) {
        ret = NULL;
        goto __END;
    }

    if (queque->head == queque->tail) {
        ret = queque->head;
        /* queque->head = NULL; */
        /* queque->tail = NULL; */
        /* queque->depth = 0; */
        goto __END ;
    }

    /* while (frame->next != queque->tail) { */
    /* frame = frame->next; */
    /* } */

    ret = queque->tail ;
    /* frame->next = NULL; */
    /* queque->tail = frame; */
    /* queque->depth--; */

__END:
    TM_MUTEX_V(queque->mutex);
    return ret;
}

int tm_clear_queque(TM_QUEQUE *queque)
{
    TM_SEND_FRAME *send_frame = NULL;

    if (queque == NULL) {
        return -1;
    }

    TM_MUTEX_P(queque->mutex);

    queque->depth = 0;

    while (queque->head != NULL) {
        send_frame = queque->head;
        queque->head = queque->head->next;
        lbuf_free(send_frame->buffer);
        lbuf_free(send_frame);
    }

    queque->tail = queque->head;

    TM_MUTEX_V(queque->mutex);

    return 0;
}

u8 tm_queque_is_busy(void)
{
    return tm_queque_busy;
}

void tm_init_queque(void)
{

    /*init queque*/
    __this->audio_data_queque.head = NULL;
    __this->audio_data_queque.tail = NULL;
    __this->audio_data_queque.depth = 0;
    __this->cmd_data_queque.head  = NULL;
    __this->cmd_data_queque.tail = NULL;
    __this->cmd_data_queque.depth  = 0;

}

int tm_cmd_send_data_to_queue(_uint8 *in_buff, _uint32 len)
{
    TM_QUEQUE *p_queque = NULL;
    TM_SEND_FRAME *frame = NULL;

    frame  = tm_cmd_malloc_lbuf(NULL, sizeof(TM_SEND_FRAME));
    if (frame == NULL) {
        log_info("****error tm_send_data_to_queue\n");
        return -1;
    }

    (void)memset((void *)frame, 0, sizeof(TM_SEND_FRAME));

    frame->buffer = (void *)in_buff;
    frame->len   =  len;

    p_queque = &(__this->cmd_data_queque);

    tm_frame_push_queque(p_queque, frame);
    tm_queque_busy = 1;
    return 0;
}

int tm_audio_send_data_to_queue(_uint8 *in_buff, _uint32 len)
{
    TM_QUEQUE *p_queque = NULL;
    TM_SEND_FRAME *frame = NULL;

    frame  = tm_audio_malloc_lbuf(NULL, sizeof(TM_SEND_FRAME));
    if (frame == NULL) {
        log_info("****error tm_send_data_to_queue\n");
        return -1;
    }

    (void)memset((void *)frame, 0, sizeof(TM_SEND_FRAME));

    frame->buffer = (void *)in_buff;
    frame->len   =  len;

    p_queque = &(__this->audio_data_queque);

    tm_frame_push_queque(p_queque, frame);
    tm_queque_busy = 1;
    return 0;
}

static int tm_stop_send(void)
{
    /*rec stop*/
    gma_hw_api.stop_speech();
    return 0;
}

#define PROCESS_USED_OS  1
#if (!PROCESS_USED_OS)
SET_INTERRUPT
#endif
static void tm_data_send_process_thread(void)
{
    int ret = -1;
    TM_SEND_FRAME *frame = NULL;
    u8 timeout = 0;

    tm_send_process_clear();
    timeout = 0;
    while (1) {
//		putchar('T');
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
__SLAVE_SEND:
            printf("gma process send error !!! ");
            frame  = tm_frame_pop_queque_alloc(&(__this->cmd_data_queque));
            if (frame != NULL) {
                if (tm_should_send(frame->len)) {
                    log_info("---slave role cmd snd \n");
                    frame  = tm_frame_pop_queque(&(__this->cmd_data_queque));
                    lbuf_free(frame->buffer);
                    lbuf_free(frame);
                    goto __SLAVE_SEND;
                }
            }

            frame  = tm_frame_pop_queque_alloc(&(__this->audio_data_queque));
            if (frame != NULL) {
                if (tm_should_send(frame->len)) {
                    frame  = tm_frame_pop_queque(&(__this->audio_data_queque));
                    //log_info("--data snd\n");
                    lbuf_free(frame->buffer);
                    lbuf_free(frame);
                }
            } else {
                break;
            }
        } else {
__MASTER_SEND:
            frame  = tm_frame_pop_queque_alloc(&(__this->cmd_data_queque));
            if (frame != NULL) {
                if (tm_should_send(frame->len)) {
                    log_info("---cmd snd \n");
                    frame  = tm_frame_pop_queque(&(__this->cmd_data_queque));
                    ret = tm_send_raw_cmd(frame->buffer, frame->len);
                    lbuf_free(frame->buffer);
                    lbuf_free(frame);
                    if (ret) {
                        goto __EXCEPTION;
                    } else {
                        goto  __MASTER_SEND;
                    }
                } else {
                    printf("cmd wait \n");
                    timeout ++;
                    if (timeout >= 10) {
                        printf("wait timeout exit \n");
                        goto __EXCEPTION;
                    }
                    os_time_dly(1);
                }
            }

            frame  = tm_frame_pop_queque_alloc(&(__this->audio_data_queque));
            if (frame != NULL) {
                if (tm_should_send(frame->len)) {
                    frame  = tm_frame_pop_queque(&(__this->audio_data_queque));
                    //log_info("--data snd\n");
                    ret = tm_send_raw_audio_data(frame->buffer, frame->len);
                    lbuf_free(frame->buffer);
                    lbuf_free(frame);
                    if (ret) {
                        /*处理连接disconnect问题*/
                        goto __EXCEPTION;
                    } else {
                        os_time_dly(1);
                    }
                } else {
                    printf("audio wait \n");
                    timeout ++;
                    if (timeout >= 10) {
                        printf("wait timeout exit \n");
                        goto __EXCEPTION;
                    }
                    os_time_dly(1);
                }
            } else {
                break;
            }

        }
    }

    tm_queque_busy = 0;
    return;

__EXCEPTION:
    log_info("-------------- stop\n");
    (void)tm_stop_send();
    tm_queque_busy = 0;
}

#define PROCESS_USED_MSG 	1
#if PROCESS_USED_OS
#if (!PROCESS_USED_MSG)
static OS_SEM tm_sem;
#endif
#define PROCESS_NAME "tm_gma"
static void tm_process_task(void *p)
{
    int msg[8];
    int ret;

    while (1) {
#if (!PROCESS_USED_MSG)
        os_sem_pend(&tm_sem, 0);
        tm_data_send_process_thread();
#else
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (ret != OS_TASKQ) {
            continue;
        }

        switch (msg[0]) {
        case TM_MSG_START_DATA_PROCESS:
            tm_data_send_process_thread();
            break;
#if (GMA_OTA_EN)
        case TM_MSG_OTA_DATA_WRITE_TO_FLASH:
        case TM_MGS_OTA_CRC16_CHECK:
            if (__this && (__this->ota_process)) {
                __this->ota_process(msg[0]);
            }
            break;
#endif
        }
#endif
    }
}
#endif

static void tm_send_process_init(void)
{
#if PROCESS_USED_OS
#if (!PROCESS_USED_MSG)
    os_sem_create(&tm_sem, 0);
#endif
    int err = task_create(tm_process_task, NULL, PROCESS_NAME);
#else
    irq_handler_register(IRQ_TM_PRO_IDX, tm_data_send_process_thread, irq_index_to_prio(IRQ_TM_PRO_IDX));
#endif
}

static void tm_send_process_exit(void)
{
#if PROCESS_USED_OS
    task_kill(PROCESS_NAME);
#if (!PROCESS_USED_MSG)
    os_sem_del(&tm_sem, 0) ;
#endif
#else
    irq_handler_unregister(IRQ_TM_PRO_IDX);
#endif
}

static void tm_send_process_clear(void)
{
#if PROCESS_USED_OS
#else
    irq_common_handler(IRQ_TM_PRO_IDX);
#endif
}

void tm_send_process_resume(void)
{
#if PROCESS_USED_OS
#if (!PROCESS_USED_MSG)
    os_sem_post(&tm_sem);
#else
    int err = os_taskq_post_type(PROCESS_NAME, TM_MSG_START_DATA_PROCESS, 0, NULL);

    if (err != OS_NO_ERR) {
        log_info("OS_ERR : 0x%x", err);
    }
#endif
#else
    irq_set_pending(IRQ_TM_PRO_IDX);
#endif
}

/********************************************************
 * 					tm ota
 * *****************************************************/
#if (GMA_OTA_EN)
int tm_ota_process_register(int (*process)(int msg))
{
    if (__this == NULL) {
        return -1;
    }

    __this->ota_process = process;
    return 0;
}

void tm_ota_data_writes_resume(void)
{
#if PROCESS_USED_OS
#if (!PROCESS_USED_MSG)
    os_sem_post(&tm_sem);
#else
    int err = os_taskq_post_type(PROCESS_NAME, TM_MSG_OTA_DATA_WRITE_TO_FLASH, 0, NULL);

    if (err != OS_NO_ERR) {
        log_info("OS_ERR : 0x%x", err);
    }
#endif
#else
    irq_set_pending(IRQ_TM_PRO_IDX);
#endif
}

void tm_ota_data_crc16_resume(void)
{
#if PROCESS_USED_OS
#if (!PROCESS_USED_MSG)
    os_sem_post(&tm_sem);
#else
    int err = os_taskq_post_type(PROCESS_NAME, TM_MGS_OTA_CRC16_CHECK, 0, NULL);

    if (err != OS_NO_ERR) {
        log_info("OS_ERR : 0x%x", err);
    }
#endif
#else
    irq_set_pending(IRQ_TM_PRO_IDX);
#endif
}
#endif

int tm_frame_mg_init(int (*should_send)(_uint16 len), int (*send_data)(uint8_t *buf,  _uint16 len), \
                     int (*send_audio_data)(uint8_t *buf,  _uint16 len), int (*ota_process)(int msg))
{
#if (TM_MEM_USED_MALLOC)
    if (_frame_mg_ctl == NULL) {
        _frame_mg_ctl = malloc(sizeof(struct __frame_mg_ctl));
        if (_frame_mg_ctl == NULL) {
            log_info("tm memory alloc error !!! line:%d \n", __LINE__);
            return (-1);
        }
    }
#endif

    tm_init_queque();

#if (GMA_OTA_EN)
    tm_ota_process_register(ota_process);
#endif
    tm_should_send_register(should_send);
    tm_send_raw_audio_data_register(send_audio_data);
    tm_send_raw_cmd_register(send_data);

    tm_audio_malloc_lbuf_init();
    tm_cmd_malloc_lbuf_init();
    tm_send_process_init();
    tm_queque_busy = 0;
    log_info("===========tm frame init ok \n");
    return 0;
}

void tm_frame_mg_close(void)
{

    //close data send process
    tm_send_process_exit();

    gma_hw_api.ENTER_CRITICAL();
    tm_should_send_register(NULL);
    tm_send_raw_audio_data_register(NULL);
    tm_send_raw_cmd_register(NULL);
    gma_hw_api.EXIT_CRITICAL();

#if (TM_MEM_USED_MALLOC)
    if (_frame_mg_ctl != NULL) {
        free(_frame_mg_ctl);
        _frame_mg_ctl = NULL;
    }
#endif

    tm_queque_busy = 0;
}

#endif
