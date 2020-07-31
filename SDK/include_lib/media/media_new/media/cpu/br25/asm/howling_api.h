#ifndef HOWLING_API_H
#define HOWLING_API_H

typedef struct s_howling_para {
    int threshold;  //初始化阈值
    int depth;  //陷波器深度
    int bandwidth;//陷波器带宽
    int attack_time; //门限噪声启动时间
    int release_time;//门限噪声释放时间
    int noise_threshold;//门限噪声阈值
    int low_th_gain; //低于门限噪声阈值增益
    int sample_rate;
    int channel;
} HOWLING_PARM_SET;

typedef struct _HOWLING_API_STRUCT_ {
    HOWLING_PARM_SET 	parm;  //参数
    void				*ptr;    //运算buf指针
} HOWLING_API_STRUCT;

int get_howling_buf(void);
void howling_init(void *workbuf, int threshold, int depth, int bandwidth, int attackTime, int releaseTime, int Noise_threshold, int low_th_gain, int sampleRate, int channel);
int howling_run(void *workbuf, short *in, short *out, int len);

#endif
