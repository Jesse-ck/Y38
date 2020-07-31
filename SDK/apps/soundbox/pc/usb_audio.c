/*****************************************************************
>file name : usb_audio.c
>author : lichao
>create time : Wed 22 May 2019 10:36:22 AM CST
*****************************************************************/
#if 0
#ifndef CONFIG_CPU_BR23
#ifndef CONFIG_CPU_BR25
#include "app_config.h"
#include "system/includes.h"
#include "uac_stream.h"
#include "media/audio_dev.h"
#include "media/effectrs_sync.h"
#include "usb/usb_config.h"
#include "usb/device/uac_audio.h"
#include "audio_config.h"
#include "tone_player.h"

#if TCFG_APP_PC_EN

#define USB_AUIDO_SPEAK_CTL_USE_SEM			1

struct usb_audio_handle {
    void *play_dev;
    void *play_priv;
    int top_size;
    int bottom_size;
    int begin_size;
    u16 resettop_size;
    u16 resetbottom_size;
    s16 delta_size;
    u8 bindex;
    u8 rec_tx_channels;
    u8 rec_begin : 1;
    u8 onoff : 1;
    u8 mutex_flag : 1;
    void *play_info;
    void *rec_dev;
    void *rec_priv;
    u16 rec_begin_size;
    u16 rec_top_size;
    u16 rec_bottom_size;
    u16 sys_event_id;
    OS_MUTEX mutex;
#if USB_AUIDO_SPEAK_CTL_USE_SEM
    OS_SEM sem;
#endif
    struct audio_reqbufs spk;
    struct audio_reqbufs mic;
};

enum usb_audio_msg {
    AUDIO_STREAM_RX = 0x0,
};

#define AUDIO_PLAY_DMA_SIZE        (6 * 1024)
#define AUDIO_REC_DMA_SIZE         (4 * 1024)

#if USB_MALLOC_ENABLE
static struct usb_audio_handle *uac_handle = NULL;
#define __this      (uac_handle)
#else
static u8 audio_play_dma_buffer[AUDIO_PLAY_DMA_SIZE] ALIGNED(4) sec(.usb_audio_play_dma);
static u8 audio_rec_dma_buffer[AUDIO_REC_DMA_SIZE] ALIGNED(4) sec(.usb_audio_rec_dma);
static struct usb_audio_handle uac_handle sec(.uac_var);
#define __this      (&uac_handle)
#endif


#define USB_AUDIO_MUTEX_INIT()		do{ \
										if (!__this->mutex_flag) { \
											__this->mutex_flag = 1; \
											os_mutex_create(&__this->mutex);\
										} \
									}while(0);
#define USB_AUDIO_MUTEX_PEND(to)	do {if (__this->mutex_flag) {os_sem_pend(&__this->mutex, to);}} while(0);
#define USB_AUDIO_MUTEX_POST()		do {if (__this->mutex_flag) {os_sem_post(&__this->mutex);}} while(0);


extern u16 uac_get_cur_vol(void);
extern u8 uac_get_mute(void);

int uac_vol_switch(int vol)
{
    u16 valsum = vol * (SYS_MAX_VOL + 1) / 100;

    if (valsum > SYS_MAX_VOL) {
        valsum = SYS_MAX_VOL;
    }
    return valsum;
}

static int usb_audio_play_sync(u32 data_size)
{
    int change_point = 0;

    if (data_size <= __this->resetbottom_size) {
        int need_size = data_size - __this->begin_size;
        __this->delta_size = need_size;
        log_d("d : %d\n", __this->delta_size);
    } else if (data_size >= __this->resettop_size) {
        int need_size = data_size - __this->begin_size;
        __this->delta_size = need_size;
        log_d("b : %d\n", __this->delta_size);
    } else if (data_size > __this->top_size) {
        change_point = -1;
        putchar('T');
    } else if (data_size < __this->bottom_size) {
        change_point = 1;
        putchar('B');
    }


    if (change_point && __this->play_dev) {
        struct audio_pcm_src src;
        src.resample = 0;
        src.ratio_i = (1024) * 2;
        src.ratio_o = (1024 + change_point) * 2;
        src.convert = 1;
        dev_ioctl(__this->play_dev, AUDIOC_PCM_RATE_CTL, (u32)&src);
    }
    return 0;
}

static void uac_speaker_stream_rx_handler(int event, void *data, int len)
{
    int msg[8];

    if ((!__this->play_dev) || (!__this->onoff)) {
        return ;
    }

    if (uac_speaker_stream_size() > __this->begin_size) {
#if USB_AUIDO_SPEAK_CTL_USE_SEM
        os_sem_set(&__this->sem, 0);
        os_sem_post(&__this->sem);
#else
        msg[0] = AUDIO_STREAM_RX;
        os_taskq_post_type("usb_audio", Q_MSG, 1, msg);
#endif
    }
}

static int uac_stream_handler(int msg)
{
    struct audio_buffer b;

    USB_AUDIO_MUTEX_PEND(0);

    if (!__this->play_dev) {
        USB_AUDIO_MUTEX_POST();
        return -EINVAL;
    }

    b.noblock = 0;
    b.timeout = 50;
    usb_audio_play_sync(uac_speaker_stream_size());

    dev_ioctl(__this->play_dev, AUDIOC_STREAM_ALLOC, (u32)&b);

    while (__this->delta_size > 0) {
        u32 rdlen;
        rdlen = uac_speaker_read(__this->play_priv, (void *)b.baddr, __this->delta_size > 1024 ? 1024 : __this->delta_size);
        __this->delta_size -= rdlen;
        log_d("v : %d\n", __this->delta_size);
    }

    if (__this->delta_size < 0) {
        u8 state = 0;
        dev_ioctl(__this->play_dev, AUDIOC_GET_SRC_STATS, (u32)&state);
        if (state == 0) {
            int deltav = 0 - __this->delta_size;
            deltav = (deltav > b.len) ? b.len : deltav;
            __this->delta_size += deltav;
            b.len -= deltav;
            memset((void *)b.baddr, 0, deltav);
            dev_write(__this->play_dev, (void *)b.baddr, deltav);
            log_d("w : %d\n", __this->delta_size);
        }
    }

    if (b.len) {
        b.len = uac_speaker_read(__this->play_priv, (void *)b.baddr, b.len > 1024 ? 1024 : b.len);

        dev_write(__this->play_dev, (void *)b.baddr, b.len);
    }

    USB_AUDIO_MUTEX_POST();
    return 0;
}

static void usb_audio_task(void *arg)
{
    int res;
    int msg[8];


    while (1) {
#if USB_AUIDO_SPEAK_CTL_USE_SEM
        res = os_sem_pend(&__this->sem, 5);
        if (!res) {
            uac_stream_handler(0);
        }
        res = os_taskq_accept(ARRAY_SIZE(msg), msg);
#else
        res = os_task_pend("taskq", msg, ARRAY_SIZE(msg));

        switch (res) {
        case OS_TASKQ:
            switch (msg[0]) {
            case Q_EVENT:
                break;
            case Q_MSG:
                uac_stream_handler(msg[1]);
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
#endif
    }
}


static int usb_audio_play_open(void *_info)
{
    struct uac_stream_info *info = (struct uac_stream_info *)_info;
    int err = 0;
    struct audio_format f = {0};

    USB_AUDIO_MUTEX_PEND(0);

    if (__this->play_dev) {
        USB_AUDIO_MUTEX_POST();
        return 0;
    }

    __this->play_dev = dev_open("audio", "play");

    if (!__this->play_dev) {
        USB_AUDIO_MUTEX_POST();
        return -ENODEV;
    }

    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());

    f.volume        = uac_vol_switch(uac_get_cur_vol());
    if (uac_get_mute()) {
        f.volume    = 0;
    }
    /*
     * channel为2并且DAC配置差分或单端输出时，需要将pcm合成为单声道写入audio设备
     *
     * */
    f.channel       = info->channel;
    f.sample_rate   = info->sample_rate;
    f.priority      = 4;
    f.src_on        = 1;
    f.fade_on       = 1;
    err = dev_ioctl(__this->play_dev, AUDIOC_SET_FMT, (u32)&f);
    if (err) {
        log_e("set format_err\n");
        goto __err;
    }

    /* struct audio_reqbufs breq; */
#if USB_MALLOC_ENABLE
    if (!(__this->spk.buf)) {
        __this->spk.buf = (u8 *)malloc(AUDIO_PLAY_DMA_SIZE);
        if (!(__this->spk.buf)) {
            goto __err;
        }
    }
#else
    __this->spk.buf = audio_play_dma_buffer;
#endif
    __this->spk.size = AUDIO_PLAY_DMA_SIZE;

    dev_ioctl(__this->play_dev, AUDIOC_REQBUFS, (unsigned int)(&(__this->spk)));

    err = dev_ioctl(__this->play_dev, AUDIOC_PLAY, 1);

    __this->play_priv = info->priv;

    __this->top_size = uac_speaker_stream_length() * 70 / 100;
    __this->bottom_size = uac_speaker_stream_length() * 40 / 100;
    __this->begin_size = uac_speaker_stream_length() * 50 / 100;
    __this->resetbottom_size = 48 * 2 * 2 * 3;
    __this->resettop_size = uac_speaker_stream_length() - 48 * 2 * 2 * 3;

    set_uac_speaker_rx_handler(__this->play_priv, uac_speaker_stream_rx_handler);

    /* if (uac_get_mute()) { */
    /* printf("mute"); */
    /* app_audio_mute(AUDIO_MUTE_DEFAULT); */
    /* } else { */
    /* printf("unmute"); */
    /* app_audio_mute(AUDIO_UNMUTE_DEFAULT); */
    /* } */

    USB_AUDIO_MUTEX_POST();
    return 0;

__err2:
    if (__this->play_dev) {
        dev_ioctl(__this->play_dev, AUDIOC_PLAY, 0);
    }

__err:
    if (__this->play_dev) {
        dev_close(__this->play_dev);
        __this->play_dev = NULL;
    }
    USB_AUDIO_MUTEX_POST();
    return -EINVAL;
}

static int usb_audio_play_close(void *arg)
{
    int err = 0;

    USB_AUDIO_MUTEX_PEND(0);

    if (!__this->play_dev) {
        USB_AUDIO_MUTEX_POST();
        return 0;
    }

    err = dev_ioctl(__this->play_dev, AUDIOC_PLAY, 0);

    dev_close(__this->play_dev);
    __this->play_dev = NULL;

    if (__this->spk.buf) {
#if USB_MALLOC_ENABLE
        free(__this->spk.buf);
#endif
        __this->spk.buf = NULL;
    }

    USB_AUDIO_MUTEX_POST();
    return 0;
}


static int usb_audio_mic_sync(u32 data_size)
{
    int change_point = 0;

    if (data_size > __this->rec_top_size) {
        change_point = -1;
    } else if (data_size < __this->rec_bottom_size) {
        change_point = 1;
    }

    if (change_point) {
        struct audio_pcm_src src;
        src.resample = 0;
        src.ratio_i = (1024) * 2;
        src.ratio_o = (1024 + change_point) * 2;
        src.convert = 1;
        dev_ioctl(__this->rec_dev, AUDIOC_PCM_RATE_CTL, (u32)&src);
    }

    return 0;
}

static int usb_audio_mic_tx_handler(int event, void *data, int len)
{
    struct audio_buffer b = {0};
    int i = 0;
    int r_len = 0;
    u8 ch = 0;
    u8 double_read = 0;

    /* USB_AUDIO_MUTEX_PEND(0); */

    if (!__this->rec_dev) {
        /* USB_AUDIO_MUTEX_POST(); */
        return 0;
    }

    u32 size;
    dev_ioctl(__this->rec_dev, AUDIOC_STREAM_SIZE, (u32)&size);
    if (!__this->rec_begin) {
        if (size < __this->rec_begin_size) {
            memset(data, 0x0, len);
            /* USB_AUDIO_MUTEX_POST(); */
            return 0;
        }
        __this->rec_begin = 1;
    }

    usb_audio_mic_sync(size);

    b.noblock = 1;
    b.timeout = 0;

    b.index = __this->bindex; //多路编码时候需要调整audio的多路缓存
    dev_ioctl(__this->rec_dev, AUDIOC_DQBUF, (u32)&b);

    int points = len / 2 / __this->rec_tx_channels;
    if (points * 2 > b.len) {
        double_read = 1;
        points = b.len / 2;
    }
    s16 *mic_pcm = (s16 *)b.baddr;
    s16 *tx_pcm = (s16 *)data;
    for (i = 0; i < points; i++, mic_pcm++) {
        for (ch = 0; ch < __this->rec_tx_channels; ch++) {
            *tx_pcm++ = *mic_pcm;
        }
    }

    r_len += points * 2 * __this->rec_tx_channels;
    b.len = points * 2;

    dev_ioctl(__this->rec_dev, AUDIOC_QBUF, (u32)&b);

    if (double_read) {
        dev_ioctl(__this->rec_dev, AUDIOC_DQBUF, (u32)&b);
        points = (len / 2 / __this->rec_tx_channels - points);
        if (points * 2 > b.len) {
            double_read = 1;
            points = b.len / 2;
        }
        mic_pcm = (s16 *)b.baddr;
        for (i = 0; i < points; i++, mic_pcm++) {
            for (ch = 0; ch < __this->rec_tx_channels; ch++) {
                *tx_pcm++ = *mic_pcm;
            }
        }

        r_len += points * 2 * __this->rec_tx_channels;
        b.len = points * 2;
        dev_ioctl(__this->rec_dev, AUDIOC_QBUF, (u32)&b);
    }

    /*putchar('t');*/
    /* USB_AUDIO_MUTEX_POST(); */
    return r_len;
}

static int usb_audio_mic_open(void *_info)
{
    struct uac_stream_info *info = (struct uac_stream_info *)_info;
    int err = 0;
    struct audio_format f = {0};
    void *mic_hdl = NULL;

    /* USB_AUDIO_MUTEX_PEND(0); */

    if (__this->rec_dev) {
        /* USB_AUDIO_MUTEX_POST(); */
        return 0;
    }

    __this->rec_begin = 0;
    __this->rec_begin_size = AUDIO_REC_DMA_SIZE * 50 / 100;
    __this->rec_bottom_size = AUDIO_REC_DMA_SIZE * 30 / 100;
    __this->rec_top_size = AUDIO_REC_DMA_SIZE * 80 / 100;
    __this->rec_tx_channels = info->channel;
    __this->rec_priv = info->priv;
    /*audio rec设备*/
    log_d("open audio rec dev\n");
    mic_hdl = dev_open("audio", (void *)"rec");
    if (!mic_hdl) {
        log_e("---audio rec open: faild\n");
        goto __err;
    }

    f.sample_source = "mic";
    f.sample_rate = info->sample_rate;
    f.volume = 25;
    f.channel = 1;
    f.src_on = 1;
    err = dev_ioctl(mic_hdl, AUDIOC_SET_FMT, (u32)&f);
    if (err) {
        log_e("audio set fmt err\n");
        goto __err;
    }

    /* struct audio_reqbufs breq; */
#if USB_MALLOC_ENABLE
    if (!(__this->mic.buf)) {
        __this->mic.buf = (u8 *)malloc(AUDIO_REC_DMA_SIZE);
        if (!(__this->mic.buf)) {
            goto __err;
        }
    }
#else
    __this->mic.buf = audio_rec_dma_buffer;
#endif
    __this->mic.size = AUDIO_REC_DMA_SIZE;
    dev_ioctl(mic_hdl, AUDIOC_REQBUFS, (unsigned int)(&(__this->mic)));

    err = dev_ioctl(mic_hdl, AUDIOC_STREAM_ON, (u32)&__this->bindex);
    if (err) {
        log_e("audio rec stream on err\n");
        goto __err;
    }
    log_d("bindex %d\n", __this->bindex);

    set_uac_mic_tx_handler(__this->rec_priv, usb_audio_mic_tx_handler);

    __this->rec_dev = mic_hdl;
    /* USB_AUDIO_MUTEX_POST(); */
    return 0;

__err:
    if (mic_hdl) {
        dev_close(mic_hdl);
    }

    /* USB_AUDIO_MUTEX_POST(); */
    return -EFAULT;
}

static int usb_audio_mic_close(void *arg)
{
    int err = 0;

    /* USB_AUDIO_MUTEX_PEND(0); */

    if (!__this->rec_dev) {
        /* USB_AUDIO_MUTEX_POST(); */
        return -EINVAL;
    }

    local_irq_disable();
    void *mic_hdl = __this->rec_dev;
    __this->rec_begin = 0;
    __this->rec_dev = NULL;
    local_irq_enable();

    err = dev_ioctl(mic_hdl, AUDIOC_STREAM_OFF, __this->bindex);

    dev_close(mic_hdl);

    if (__this->mic.buf) {
#if USB_MALLOC_ENABLE
        free(__this->mic.buf);
#endif
        __this->mic.buf = NULL;
    }

    /* USB_AUDIO_MUTEX_POST(); */
    return 0;
}

static int usb_device_event_handler(u8 event, int value)
{
    switch (event) {
    case USB_AUDIO_PLAY_OPEN:
        tone_play_stop();
        __this->onoff = 1;
        __this->play_info = (void *)value;
        usb_audio_play_open((void *)value);
        break;
    case USB_AUDIO_PLAY_CLOSE:
        __this->onoff = 0;
        __this->play_info = (void *)value;
        usb_audio_play_close((void *)value);
        break;
    case USB_AUDIO_MIC_OPEN:
        usb_audio_mic_open((void *)value);
        break;
    case USB_AUDIO_MIC_CLOSE:
        usb_audio_mic_close((void *)value);
        break;
    default:
        break;
    }
    return 0;
}

static void usb_audio_event_handler(struct sys_event *event, void *priv)
{
    switch (event->type) {
    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_UAC) {
            log_d("usb device event : %d\n", event->u.dev.event);
            usb_device_event_handler(event->u.dev.event, event->u.dev.value);
        }
        return;
    default:
        break;
    }
    return;
}

// usb audio 过程中播放提示音
int usb_audio_and_tone_play(u8 start)
{
    if (__this->onoff) {
        if (start) { // 开始播放提示音
            usb_audio_play_close((void *)__this->play_info);
        } else { // 提示音播放完成
            USB_AUDIO_MUTEX_PEND(0);
            uac_speaker_stream_buf_clear();
            USB_AUDIO_MUTEX_POST();
            usb_audio_play_open((void *)__this->play_info);
        }
    }
    return __this->onoff;
}

int usb_audio_demo_init(void)
{
    int err = 0;

#if USB_MALLOC_ENABLE
    if (!uac_handle) {
        uac_handle = (struct usb_audio_handle *)zalloc(sizeof(struct usb_audio_handle));
        if (!uac_handle) {
            return -1;
        }
    }
#else
    memset(&uac_handle, 0, sizeof(uac_handle));
#endif

    USB_AUDIO_MUTEX_INIT();

#if USB_AUIDO_SPEAK_CTL_USE_SEM
    os_sem_create(&__this->sem, 0);
#endif

    __this->onoff = 0;
    __this->sys_event_id = register_sys_event_handler(SYS_ALL_EVENT, 2, NULL, usb_audio_event_handler);

    err = task_create(usb_audio_task, NULL, "usb_audio");
    if (err) {
        return err;
    }
    return 0;
}

void usb_audio_demo_exit(void)
{
    if (__this->sys_event_id) {
        unregister_sys_event_handler(__this->sys_event_id);
        __this->sys_event_id = 0;
    }
    usb_audio_play_close(NULL);
    usb_audio_mic_close(NULL);

#if USB_MALLOC_ENABLE
    if (uac_handle) {
        free(uac_handle);
        uac_handle = NULL;
    }
#endif
    task_kill("usb_audio");
}

#endif /* TCFG_APP_PC_EN */

#endif
#endif
#endif
