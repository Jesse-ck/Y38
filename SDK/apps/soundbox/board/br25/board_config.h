#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 *  板级配置选择
 */

#define CONFIG_BOARD_AC696X_DEMO
// #define CONFIG_BOARD_AC6969D_DEMO
// #define CONFIG_BOARD_AC696X_LIGHTER
// #define CONFIG_BOARD_AC696X_TWS_BOX   //暂不支持 ！！！！！！！
// #define CONFIG_BOARD_AC696X_TWS

#include "board_ac696x_demo_cfg.h"
#include "board_ac6969d_demo_cfg.h"
#include "board_ac696x_lighter_cfg.h"
#include "board_ac696x_tws_box.h"   //转发对箱
#include "board_ac696x_tws.h"   //纯对箱

#endif
