/*
 *********************************************************************************************************
 *                                            br16
 *                                            btstack
 *                                             CODE
 *
 *                          (c) Copyright 2016-2016, ZHUHAI JIELI
 *                                           All Rights Reserved
 *
 * File : *
 * By   : jamin.li
 * DATE : 2016-04-12 10:17AM    build this file
 *********************************************************************************************************
 */

#ifndef _DUEROS_PLATFORM_H_
#define _DUEROS_PLATFORM_H_

//#include "semlock.h"

#define TRUE				1
#define FALSE				0

#define DUEROS_TID
#define DUEROS_MDELAY

// #define DUEROS_THREAD_DEF
// #define DUEROS_CREAT_THREAD
// #define DUEROS_OS_THREAD

// #define AF_FADE_OUT_SIGNAL_ID  (1<<15)

//#define DUEROS_WAIT(x)  osSignalWait((1 << AF_FADE_OUT_SIGNAL_ID), osWaitForever)
// #define DUEROS_WAKEUP(tid) osSignalSet(tid, AF_FADE_OUT_SIGNAL_ID)

//#define   DMA_DEBUG_RUN()   printf("\n--%s--%d--\n",__FUNCTION__,__LINE__);

#endif
