#include "audio_digital_vol.h"


#define DIGITAL_FADE_EN 	1
#define DIGITAL_FADE_STEP 	4


static struct digital_volume d_volume;
/*
 *数字音量级数 DIGITAL_VOL_MAX
 *数组长度 DIGITAL_VOL_MAX + 1
 */
#define DIGITAL_VOL_MAX		31
const u16 dig_vol_table[DIGITAL_VOL_MAX + 1] = {
    0	, //0
    93	, //1
    111	, //2
    132	, //3
    158	, //4
    189	, //5
    226	, //6
    270	, //7
    323	, //8
    386	, //9
    462	, //10
    552	, //11
    660	, //12
    789	, //13
    943	, //14
    1127, //15
    1347, //16
    1610, //17
    1925, //18
    2301, //19
    2751, //20
    3288, //21
    3930, //22
    4698, //23
    5616, //24
    6713, //25
    8025, //26
    9592, //27
    11466,//28
    15200,//29
    16000,//30
    16384 //31
};

/*
 *fade_step一般不超过两级数字音量的最小差值
 *(1)通话如果用数字音量，一般步进小一点，音量调节的时候不会有杂音
 *(2)淡出的时候可以快一点，尽快淡出到0
 */
void audio_digital_vol_open(u8 vol, u8 vol_max, u16 fade_step)
{
    u8 vol_level;
    d_volume.fade 		= 0;
    d_volume.vol 		= vol;
    d_volume.vol_max 	= vol_max;
    vol_level 			= vol * DIGITAL_VOL_MAX / vol_max;
    d_volume.vol_target = dig_vol_table[vol_level];
    d_volume.vol_fade 	= d_volume.vol_target;
    d_volume.fade_step 	= fade_step;
    d_volume.toggle 	= 1;
    printf("digital_vol_open:%d-%d-%d\n", vol, vol_max, fade_step);
}

void audio_digital_vol_close(void)
{
    d_volume.toggle = 0;
    printf("digital_vol_close\n");
}

u8 audio_digital_vol_get(void)
{
    return d_volume.vol;
}

void audio_digital_vol_set(u8 vol)
{
    u8 vol_level;
    if (d_volume.toggle == 0) {
        return;
    }
    d_volume.vol = vol;
    d_volume.fade = DIGITAL_FADE_EN;
    vol_level = d_volume.vol * DIGITAL_VOL_MAX / d_volume.vol_max;
    d_volume.vol_target = dig_vol_table[vol_level];
    printf("digital_vol:%d-%d-%d-%d\n", vol, vol_level, d_volume.vol_fade, d_volume.vol_target);
}

void audio_digital_vol_reset_fade()
{
    d_volume.vol_fade = 0;
}
int audio_digital_vol_run(void *data, u32 len)
{
    s32 valuetemp;
    s16 *buf;

    if (d_volume.toggle == 0) {
        return -1;
    }

    buf = data;
    len >>= 1; //byte to point

    for (u32 i = 0; i < len; i += 2) {
        ///left channel
        valuetemp = buf[i];
        if (d_volume.fade) {
            if (d_volume.vol_fade > d_volume.vol_target) {
                d_volume.vol_fade -= d_volume.fade_step;
                if (d_volume.vol_fade < d_volume.vol_target) {
                    d_volume.vol_fade = d_volume.vol_target;
                }
            } else if (d_volume.vol_fade < d_volume.vol_target) {
                d_volume.vol_fade += d_volume.fade_step;
                if (d_volume.vol_fade > d_volume.vol_target) {
                    d_volume.vol_fade = d_volume.vol_target;
                }
            }
        } else {
            d_volume.vol_fade = d_volume.vol_target;
        }

        valuetemp = (valuetemp * d_volume.vol_fade) >> 14 ;
        if (valuetemp < -32768) {
            valuetemp = -32768;
        } else if (valuetemp > 32767) {
            valuetemp = 32767;
        }
        buf[i] = (s16)valuetemp;

        ///right channel
        valuetemp = buf[i + 1];
        valuetemp = (valuetemp * d_volume.vol_fade) >> 14 ;
        if (valuetemp < -32768) {
            valuetemp = -32768;
        } else if (valuetemp > 32767) {
            valuetemp = 32767;
        }
        buf[i + 1] = (s16)valuetemp;
    }
    return 0;
}


/*************************支持重入的数字音量调节****************************/



/*
 *fade_step一般不超过两级数字音量的最小差值
 *(1)通话如果用数字音量，一般步进小一点，音量调节的时候不会有杂音
 *(2)淡出的时候可以快一点，尽快淡出到0
 */
void *user_audio_digital_volume_open(u8 vol, u8 vol_max, u16 fade_step)
{
    struct digital_volume *d_volume = zalloc(sizeof(struct digital_volume));
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return NULL;
    }
    u8 vol_level;
    d_volume->fade 		= 0;
    d_volume->vol 		= vol;
    d_volume->vol_max 	= vol_max;
    vol_level 			= vol * DIGITAL_VOL_MAX / vol_max;
    d_volume->vol_target = dig_vol_table[vol_level];
    d_volume->vol_fade 	= d_volume->vol_target;
    d_volume->fade_step 	= fade_step;
    d_volume->toggle 	= 1;
    d_volume->ch_num    = 2;//默认双声道
    d_volume->user_vol_tab = NULL;
    d_volume->user_vol_max = 0;

    os_mutex_create(&d_volume->mutex);
    printf("digital_vol_open:%d-%d-%d\n", vol, vol_max, fade_step);
    return d_volume;
}

int user_audio_digital_volume_close(void *_d_volume)
{
    struct digital_volume *d_volume = (struct digital_volume *)_d_volume;
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return -1;
    }
    /* os_mutex_pend(&d_volume->mutex, 0); */
    d_volume->toggle = 0;
    d_volume->user_vol_tab = NULL;
    d_volume->user_vol_max = 0;

    if (d_volume) {
        free(d_volume);
        d_volume = NULL;
    }
    /* os_mutex_post(&d_volume->mutex); */
    printf("digital_vol_close\n");
    return 0;
}

u8 user_audio_digital_volume_get(void *_d_volume)
{
    struct digital_volume *d_volume = (struct digital_volume *)_d_volume;
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return 0;
    }
    /* os_mutex_pend(&d_volume->mutex, 0); */
    u8 vol = d_volume->vol;
    /* os_mutex_post(&d_volume->mutex); */
    return vol;
}

int user_audio_digital_volume_set(void *_d_volume, u8 vol)
{
    struct digital_volume *d_volume = (struct digital_volume *)_d_volume;
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return -1;
    }

    u8 vol_level;
    if (d_volume->toggle == 0) {
        return 0;
    }
    /* os_mutex_pend(&d_volume->mutex, 0); */
    d_volume->vol = vol;
    d_volume->fade = DIGITAL_FADE_EN;
    if (!d_volume->user_vol_tab) {
        vol_level = d_volume->vol * DIGITAL_VOL_MAX / d_volume->vol_max;
        d_volume->vol_target = dig_vol_table[vol_level];
    } else {
        u8 d_vol_max = d_volume->user_vol_max - 1;
        vol_level = d_volume->vol * d_vol_max / d_volume->vol_max;
        d_volume->vol_target = d_volume->user_vol_tab[vol_level];
    }

    /* os_mutex_post(&d_volume->mutex); */
    /* printf("digital_vol:%d-%d-%d-%d\n", vol, vol_level, d_volume->vol_fade, d_volume->vol_target); */
    return 0;
}

int user_audio_digital_volume_reset_fade(void *_d_volume)
{
    struct digital_volume *d_volume = (struct digital_volume *)_d_volume;
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return -1;
    }

    os_mutex_pend(&d_volume->mutex, 0);
    d_volume->vol_fade = 0;
    os_mutex_post(&d_volume->mutex);
    return 0;
}

int user_audio_digital_volume_run(void *_d_volume, void *data, u32 len, u8 ch_num)
{

    struct digital_volume *d_volume = (struct digital_volume *)_d_volume;
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return -1;
    }

    s32 valuetemp;
    s16 *buf;

    if (d_volume->toggle == 0) {
        return -1;
    }
    if (ch_num > 4) {
        return -1;
    }
    os_mutex_pend(&d_volume->mutex, 0);

    if (ch_num) {
        d_volume->ch_num = ch_num;
    }
    buf = data;
    len >>= 1; //byte to point

    /* printf("d_volume->vol_target %d %d\n", d_volume->vol_target, ch_num); */
    for (u32 i = 0; i < len; i += d_volume->ch_num) {//ch_num 1/2/3/4
        ///FL channel
        if (d_volume->fade) {
            if (d_volume->vol_fade > d_volume->vol_target) {
                d_volume->vol_fade -= d_volume->fade_step;
                if (d_volume->vol_fade < d_volume->vol_target) {
                    d_volume->vol_fade = d_volume->vol_target;
                }
            } else if (d_volume->vol_fade < d_volume->vol_target) {
                d_volume->vol_fade += d_volume->fade_step;
                if (d_volume->vol_fade > d_volume->vol_target) {
                    d_volume->vol_fade = d_volume->vol_target;
                }
            }
        } else {
            d_volume->vol_fade = d_volume->vol_target;
        }

        valuetemp = buf[i];
        valuetemp = (valuetemp * d_volume->vol_fade) >> 14 ;
        if (valuetemp < -32768) {
            valuetemp = -32768;
        } else if (valuetemp > 32767) {
            valuetemp = 32767;
        }
        buf[i] = (s16)valuetemp;

        if (d_volume->ch_num > 1) { //双声道
            ///FR channel
            valuetemp = buf[i + 1];
            valuetemp = (valuetemp * d_volume->vol_fade) >> 14 ;
            if (valuetemp < -32768) {
                valuetemp = -32768;
            } else if (valuetemp > 32767) {
                valuetemp = 32767;
            }
            buf[i + 1] = (s16)valuetemp;

            if (d_volume->ch_num > 2) { //三声道
                //RL channel
                valuetemp = buf[i + 2];
                valuetemp = (valuetemp * d_volume->vol_fade) >> 14 ;
                if (valuetemp < -32768) {
                    valuetemp = -32768;
                } else if (valuetemp > 32767) {
                    valuetemp = 32767;
                }
                buf[i + 2] = (s16)valuetemp;


                if (d_volume->ch_num > 3) { //四声道
                    ///RR channel
                    valuetemp = buf[i + 3];
                    valuetemp = (valuetemp * d_volume->vol_fade) >> 14 ;
                    if (valuetemp < -32768) {
                        valuetemp = -32768;
                    } else if (valuetemp > 32767) {
                        valuetemp = 32767;
                    }
                    buf[i + 3] = (s16)valuetemp;
                }
            }
        }
    }
    os_mutex_post(&d_volume->mutex);
    return 0;
}




/*
 *user_vol_max:音量级数
 *user_vol_tab:自定义音量表,自定义表长user_vol_max+1
 */
void user_audio_digital_set_volume_tab(void *_d_volume, u16 *user_vol_tab, u8 user_vol_max)
{
    struct digital_volume *d_volume = (struct digital_volume *)_d_volume;
    if (!d_volume) {
        log_e("d_volume NULL\n");
        return ;
    }

    os_mutex_pend(&d_volume->mutex, 0);
    if (user_vol_tab) {
        d_volume->user_vol_tab = user_vol_tab;
        d_volume->user_vol_max = user_vol_max;
    }
    os_mutex_post(&d_volume->mutex);
}



/*
 *priv:用户自定义指针
 *void (*handler)(void *priv, void *data, int len, u8 ch_num):用户自定义回调
 * */
void *user_audio_process_open(void *parm, void *priv, void (*handler)(void *priv, void *data, int len, u8 ch_num))
{
    struct user_audio_parm *user_hdl = zalloc(sizeof(struct user_audio_parm));
    if (!user_hdl) {
        log_e("user_hdl NULL\n");
        return NULL;
    }
    user_hdl->priv = priv;
    user_hdl->handler = handler;

    struct user_audio_digital_parm *dvol_parm = (struct user_audio_digital_parm *)parm;
    if (dvol_parm->en) {
        log_i("vol :%d vol_max:%d fade_step:%d\n");
        user_hdl->dvol_hdl = user_audio_digital_volume_open(dvol_parm->vol, dvol_parm->vol_max, dvol_parm->fade_step);
    }
    log_i("%s ok\n", __FUNCTION__);
    return user_hdl;
}


int user_audio_process_close(void *_uparm_hdl)
{
    struct user_audio_parm *user_hdl = (struct user_audio_parm *)_uparm_hdl;
    if (!user_hdl) {
        log_e("user_hdl NULL\n");
        return -1;
    }
    if (user_hdl->dvol_hdl) {
        user_audio_digital_volume_close(user_hdl->dvol_hdl);
        user_hdl->dvol_hdl = NULL;
    }
    free(user_hdl);
    user_hdl = NULL;
    log_i("%s ok\n", __FUNCTION__);
    return 0;
}

void user_audio_process_handler_run(void *_uparm_hdl, void *data, u32 len, u8 ch_num)
{
    struct user_audio_parm *user_hdl = (struct user_audio_parm *)_uparm_hdl;
    if (!user_hdl) {
        log_e("user_hdl NULL\n");
        return;
    }


    if (user_hdl->handler) {
        user_hdl->handler(user_hdl->priv, data, len, ch_num);
    }

    if (user_hdl->dvol_hdl) {
        user_audio_digital_volume_run(user_hdl->dvol_hdl, data, len, ch_num);
    }

}




