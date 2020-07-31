#include "key_driver.h"
#include "slidekey.h"
#include "gpio.h"
#include "system/event.h"
#include "app_config.h"
#include "asm/clock.h"
#include "app_action.h"

#if TCFG_SLIDE_KEY_ENABLE

s16 slide_data = 0;

#define SLIDE_FILTER_CNT	3
#define MAX_CH   10
static const struct slidekey_platform_data *__this = NULL;

static u8 slide_old_level[MAX_CH];


/*
  en == 0 需要跟上一次的等级不一致才会post msg
  en == 1 则不需要跟上一次的等级比较，可以直接post msg,可供外部调用重新获取level
 */
void get_slide_level(u8 en)
{
    u8 i = 0, j = 0;
    u8 slide_new_level[MAX_CH] = {0};

    for (i = 0; i < __this->num; i++) {
        for (j = 0; j < SLIDE_FILTER_CNT; j++) {
            slide_data += adc_get_value(__this->port[i].ad_channel);
        }
        slide_data = slide_data / SLIDE_FILTER_CNT;
        //printf("i = %d, slide_data = %d\n",i,slide_data);

        if (slide_data > __this->port[i].max_ad) {	//由于滑动变阻器拉到最大会断开电路，所以最大级数赋给这个状态
            slide_new_level[i] = __this->port[i].max_level;
        } else {
            slide_new_level[i] = (slide_data - __this->port[i].min_ad) / ((__this->port[i].max_ad - __this->port[i].min_ad) / (__this->port[i].max_level - 1));
        }

        if (en == 0) {
            if (slide_new_level[i] != slide_old_level[i]) {
                printf("i = %d,nl = %d,nd = %d\n", i, slide_new_level[i], adc_get_value(__this->port[i].ad_channel));
                slide_old_level[i] = slide_new_level[i];
                app_task_msg_post(__this->port[i].msg, 1, slide_new_level[i]);
            }
        } else {
            app_task_msg_post(__this->port[i].msg, 1, slide_new_level[i]);
        }
        slide_data = 0;
    }
}
void slidekey_scan(void *ad)
{

    if (!__this->enable) {
        return;
    }
    get_slide_level(0);
}



int slidekey_init(const struct slidekey_platform_data *slidekey_data)
{
    printf("%s", __func__);
    u8 i = 0;

    __this = slidekey_data;
    if (__this == NULL) {
        return -EINVAL;
    }

    for (i = 0; i < __this->num; i++) {

        slide_old_level[i] = 100;

        adc_add_sample_ch(__this->port[i].ad_channel);

        gpio_set_die(__this->port[i].io, 0);
        gpio_set_direction(__this->port[i].io, 1);
        gpio_set_pull_down(__this->port[i].io, 0);
        gpio_set_pull_up(__this->port[i].io, 1);

    }
    return 1;
}


#endif  /* #if TCFG_SLIDE_KEY_ENABLE */

