#ifndef  __USB_CONFIG_H__
#define  __USB_CONFIG_H__

#include "typedef.h"
#include "asm/usb.h"
#include "usb/device/usb_stack.h"
#include "usb/host/usb_host.h"


#define     USB_MALLOC_ENABLE           1
#define     USB_HOST_ASYNC              1


///////////MassStorage Class

#define     MSD_BULK_EP_OUT             1
#define     MSD_BULK_EP_IN              1


#define     MAXP_SIZE_BULKOUT           64
#define     MAXP_SIZE_BULKIN            64


///////////HID class
#define     HID_EP_IN                   2
#define     HID_EP_OUT                  2

#define     MAXP_SIZE_HIDOUT            8
#define     MAXP_SIZE_HIDIN             8
/* #define     MAXP_SIZE_HIDOUT            64 */
/* #define     MAXP_SIZE_HIDIN             64 */



/////////////Audio Class
#define     UAC_ISO_INTERVAL            1
//speaker class
#define     SPK_AUDIO_RATE              (480)//*100KHz
#define     SPK_AUDIO_RES               16

#define     SPK_CHANNEL                 2
#define     SPK_FRAME_LEN               (((SPK_AUDIO_RATE) * SPK_AUDIO_RES / 8 * SPK_CHANNEL)/10)

#define     SPK_PCM_Type                (SPK_AUDIO_RES >> 4)                // 0=8 ,1=16
#define     SPK_AUDIO_TYPE              (0x02 - SPK_PCM_Type)           // TYPE1_PCM16


#define     SPK_ISO_EP_OUT              3

#define     SPEAKER_STR_INDEX           8

#define     SPK_INPUT_TERMINAL_ID       1
#define     SPK_FEATURE_UNIT_ID         2
#define     SPK_OUTPUT_TERMINAL_ID      3

/////////////Microphone Class
#define     MIC_AUDIO_RATE              480//*100KHz
#define     MIC_AUDIO_RES               16

#define     MIC_CHANNEL                 2
#define     MIC_FRAME_LEN               ((MIC_AUDIO_RATE * MIC_AUDIO_RES / 8 * MIC_CHANNEL)/10)

#define     MIC_PCM_TYPE                (MIC_AUDIO_RES >> 4)                // 0=8 ,1=16
#define     MIC_AUDIO_TYPE              (0x02 - MIC_PCM_TYPE)



#define     MIC_ISO_EP_IN               3

#define     MIC_STR_INDEX               9

#define     MIC_INPUT_TERMINAL_ID       4
#define     MIC_FEATURE_UNIT_ID         5
#define     MIC_OUTPUT_TERMINAL_ID      6


void usb_host_config(usb_dev usb_id);
void usb_host_free(usb_dev usb_id);
void *usb_h_get_ep_buffer(const usb_dev usb_id, u32 ep);
void usb_h_isr_reg(const usb_dev usb_id, u8 priority, u8 cpu_id);
void usb_g_isr_reg(const usb_dev usb_id, u8 priority, u8 cpu_id);



void usb_sof_isr_reg(const usb_dev usb_id, u8 priority, u8 cpu_id);
void *usb_get_ep_buffer(const usb_dev usb_id, u32 ep);
u32 usb_config(const usb_dev usb_id);
u32 usb_release(const usb_dev usb_id);

#endif  /*USB_CONFIG_H*/
