/*
 * Copyright (C) 2010-2014 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 *  OSAL header files related to memory, debug, random, semaphore and mutex functions.
 */

#ifndef PHNFCCOMMON_H
#define PHNFCCOMMON_H

/*
************************* Include Files ****************************************
*/

#include <phNfcStatus.h>
#include <semaphore.h>
#include <phOsalNfc_Timer.h>
#include <pthread.h>
#include <phDal4Nfc_messageQueueLib.h>
#include <phNfcCompId.h>

#ifdef ANDROID
#   define FW_DLL_ROOT_DIR "/vendor/firmware/"
#else
#   define FW_DLL_ROOT_DIR ""
#endif
#   define FW_DLL_EXTENSION ".so"

#if(NFC_NXP_CHIP_TYPE != PN547C2)

/* Actual FW library name*/
#define FW_LIB_PATH       FW_DLL_ROOT_DIR "lib" NXP_CHIP_NAME "_fw"          FW_DLL_EXTENSION
/* Restore Currupted PLL Setttings/etc */
#define PLATFORM_LIB_PATH FW_DLL_ROOT_DIR "lib" NXP_CHIP_NAME "_fw_platform" FW_DLL_EXTENSION
/* Upgrade the public Key */
#define PKU_LIB_PATH      FW_DLL_ROOT_DIR "lib" NXP_CHIP_NAME "_fw_pku"      FW_DLL_EXTENSION
#else
/* Actual FW library name*/
#define FW_LIB_PATH       FW_DLL_ROOT_DIR "lib" NXP_CHIP_NAME "_fw"          FW_DLL_EXTENSION
/* Restore Currupted PLL Setttings/etc */
#define PLATFORM_LIB_PATH FW_DLL_ROOT_DIR "lib" NXP_CHIP_NAME "_fw_platform" FW_DLL_EXTENSION
/* Upgrade the public Key */
#define PKU_LIB_PATH      FW_DLL_ROOT_DIR "lib" NXP_CHIP_NAME "_fw_pku"      FW_DLL_EXTENSION

#endif

/* HAL Version number (Updated as per release) */
#define NXP_MW_VERSION_MAJ  (1U)
#define NXP_MW_VERSION_MIN  (0U)

/*
 *****************************************************************
 ***********  System clock source selection configuration ********
 *****************************************************************
 */

#define CLK_SRC_UNDEF      0
#define CLK_SRC_XTAL       1
#define CLK_SRC_PLL        2
#define CLK_SRC_PADDIRECT  3

/*Extern crystal clock source*/
#define NXP_SYS_CLK_SRC_SEL         CLK_SRC_PLL  /* Use one of CLK_SRC_<value> */
/*Direct clock*/

/*
 *****************************************************************
 ***********  System clock frequency selection configuration ****************
 * If Clk_Src is set to PLL, make sure to set the Clk_Freq also*
 *****************************************************************
 */
#define CLK_FREQ_UNDEF         0
#define CLK_FREQ_13MHZ         1
#define CLK_FREQ_19_2MHZ       2
#define CLK_FREQ_24MHZ         3
#define CLK_FREQ_26MHZ         4
#define CLK_FREQ_38_4MHZ       5
#define CLK_FREQ_52MHZ         6

#define NXP_SYS_CLK_FREQ_SEL  CLK_FREQ_19_2MHZ /* Set to one of CLK_FREQ_<value> */

#define CLK_TO_CFG_DEF         1
#define CLK_TO_CFG_MAX         26
/*
 *  information to configure OSAL
 */
typedef struct phOsalNfc_Config
{
    uint8_t *pLogFile; /* Log File Name*/
    uintptr_t dwCallbackThreadId; /* Client ID to which message is posted */
}phOsalNfc_Config_t, *pphOsalNfc_Config_t /* Pointer to #phOsalNfc_Config_t */;


/*
 * Deferred call declaration.
 * This type of API is called from ClientApplication (main thread) to notify
 * specific callback.
 */
typedef  void (*pphOsalNfc_DeferFuncPointer_t) (void*);


/*
 * Deferred message specific info declaration.
 */
typedef struct phOsalNfc_DeferedCallInfo
{
        pphOsalNfc_DeferFuncPointer_t   pDeferedCall;/* pointer to Deferred callback */
        void                            *pParam;    /* contains timer message specific details*/
}phOsalNfc_DeferedCallInfo_t;


/*
 * States in which a OSAL timer exist.
 */
typedef enum
{
    eTimerIdle = 0,         /* Indicates Initial state of timer */
    eTimerRunning = 1,      /* Indicate timer state when started */
    eTimerStopped = 2       /* Indicates timer state when stopped */
}phOsalNfc_TimerStates_t;   /* Variable representing State of timer */

/*
 **Timer Handle structure containing details of a timer.
 */
typedef struct phOsalNfc_TimerHandle
{
    uint32_t TimerId;                                   /* ID of the timer */
    timer_t hTimerHandle;                               /* Handle of the timer */
    pphOsalNfc_TimerCallbck_t   Application_callback;   /* Timer callback function to be invoked */
    void *pContext;                                     /* Parameter to be passed to the callback function */
    phOsalNfc_TimerStates_t eState;                     /* Timer states */
    phLibNfc_Message_t tOsalMessage;                    /* Osal Timer message posted on User Thread */
    phOsalNfc_DeferedCallInfo_t tDeferedCallInfo;       /* Deferred Call structure to Invoke Callback function */
}phOsalNfc_TimerHandle_t,*pphOsalNfc_TimerHandle_t;     /* Variables for Structure Instance and Structure Ptr */

#endif /*  PHOSALNFC_H  */
