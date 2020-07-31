
#ifndef _BT_PROFILE_CFG_H_
#define _BT_PROFILE_CFG_H_

#include "app_config.h"
#include "btcontroller_modules.h"


#if (DUEROS_DMA_EN || TRANS_DATA_EN || RCSP_BTMATE_EN || RCSP_ADV_EN || GMA_EN)
#define    BT_FOR_APP_EN             1
#else
#define    BT_FOR_APP_EN             0
#endif

#if(DUEROS_DMA_EN)
#define USE_DMA_TONE  0  //使用DMA提示音
#else
#define USE_DMA_TONE  0
#endif

#if (DUEROS_DMA_EN && TRANS_DATA_EN)
#error "they can not enable at the same time!"
#endif

///---sdp service record profile- 用户选择支持协议--///
#if (BT_FOR_APP_EN)
#define USER_SUPPORT_PROFILE_SPP    1
#else
#define USER_SUPPORT_PROFILE_SPP    0
#endif
#define USER_SUPPORT_PROFILE_HFP    1
#define USER_SUPPORT_PROFILE_A2DP   1
#define USER_SUPPORT_PROFILE_AVCTP  1
#define USER_SUPPORT_PROFILE_HID    1
#define USER_SUPPORT_PROFILE_PNP    1
#define USER_SUPPORT_PROFILE_PBAP   0
#define USER_SUPPORT_PROFILE_HFP_AG 0


//ble demo的例子
#define DEF_BLE_DEMO_NULL                 0 //ble 没有使能
#define DEF_BLE_DEMO_ADV                  1 //only adv,can't connect
#define DEF_BLE_DEMO_STREAMER             2
#define DEF_BLE_DEMO_TRANS_DATA           3 //
#define DEF_BLE_DEMO_DUEROS_DMA           4 //
#define DEF_BLE_DEMO_RCSP_DEMO            5 //
#define DEF_BLE_DEMO_ADV_RCSP             6
#define DEF_BLE_DEMO_GMA                  7


//配置选择的demo
#if TCFG_USER_BLE_ENABLE

#if DUEROS_DMA_EN
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_DUEROS_DMA

#elif RCSP_BTMATE_EN
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_RCSP_DEMO

#elif TRANS_DATA_EN
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_TRANS_DATA

#elif RCSP_ADV_EN
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_ADV_RCSP

#elif GMA_EN
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_GMA

#else
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_ADV
#endif

#else
#define TCFG_BLE_DEMO_SELECT          DEF_BLE_DEMO_NULL//ble is closed
#endif

//配对加密使能
#define TCFG_BLE_SECURITY_EN          0



#endif
