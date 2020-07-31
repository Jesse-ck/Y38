#ifndef reverb_api_h__
#define reverb_api_h__


typedef struct _EF_REVERB_PARM_ {
    unsigned int deepval;
    unsigned int decayval;
    unsigned int filtsize;
    unsigned int wetgain;
    unsigned int drygain;
    unsigned int sr;
    unsigned int max_ms;
    unsigned int centerfreq_bandQ;
} REVERB_PARM_SET;


#define  REVERB_H_PARM_STRUCT REVERB_PARM_SET

/*open 跟 run 都是 成功 返回 RET_OK，错误返回 RET_ERR*/
/*魔音结构体*/
typedef struct __REVERB_FUNC_API_ {
    unsigned int (*need_buf)(unsigned int *ptr, REVERB_PARM_SET *reverb_parm);
    int (*open)(unsigned int *ptr, REVERB_PARM_SET *reverb_parm);
    int (*init)(unsigned int *ptr, REVERB_PARM_SET *reverb_parm);
    int (*run)(unsigned int *ptr, short *inbuf, int len);
} REVERB_FUNC_API;

typedef struct _REVERB_API_STRUCT_ {
    REVERB_PARM_SET parm;	//参数
    unsigned int 	*ptr;           	//运算buf指针
    REVERB_FUNC_API *func_api;          //函数指针
} REVERB_API_STRUCT;

extern REVERB_FUNC_API  *get_reverb_func_api();

#endif // reverb_api_h__
