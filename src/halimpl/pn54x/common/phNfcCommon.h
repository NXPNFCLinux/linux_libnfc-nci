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
#   define FW_BIN_ROOT_DIR "/vendor/firmware/"
#else
#   define FW_BIN_ROOT_DIR ""
#endif
#   define FW_BIN_EXTENSION ".so"


/* HAL Version number (Updated as per release) */
#define NXP_MW_VERSION_MAJ (0x02U)
#define NXP_MW_VERSION_MIN (0x04U)

#define GET_EEPROM_DATA (1U)
#define SET_EEPROM_DATA (2U)
#define BITWISE (1U)
#define BYTEWISE (2U)
#define GET_FW_DWNLD_FLAG (1U)
#define RESET_FW_DWNLD_FLAG (2U)
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
#define CLK_FREQ_32MHZ         5
#define CLK_FREQ_38_4MHZ       6
#define CLK_FREQ_52MHZ         7


#define SET_CONFIG_CMD_PLL_13MHZ        {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x20, 0x08, 0x08,\
                                        0x52, 0xA2, 0x02, 0x30, 0x01, 0xE1, 0x02}
#define SET_CONFIG_CMD_DPLL_13MHZ       {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x26, 0x08, 0x40,\
                                        0x42, 0xA3, 0x02, 0x88, 0x01, 0xE2, 0x02}
#define SET_CONFIG_CMD_PLL_19_2MHZ      {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x20, 0x08, 0x88,\
                                        0x51, 0xE3, 0x02, 0xB8, 0x21, 0xE1, 0x02}
#define SET_CONFIG_CMD_DPLL_19_2MHZ     {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x26, 0x08, 0x88,\
                                        0x01, 0xE2, 0x02, 0xF0, 0x00, 0xA2, 0x01}
#define SET_CONFIG_CMD_PLL_24MHZ        {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x20, 0x08, 0x28,\
                                         0xC2, 0xA2, 0x83, 0x88, 0x11, 0xE1, 0x02}
#define SET_CONFIG_CMD_DPLL_24MHZ       {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x26, 0x08, 0x38,\
                                         0x41, 0xD3, 0x02, 0x88, 0x01, 0xE2, 0x02}
#define SET_CONFIG_CMD_PLL_26MHZ        {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x20, 0x08, 0x08,\
                                         0x52, 0xA2, 0x82, 0x30, 0x01, 0xE1, 0x02}
#define SET_CONFIG_CMD_DPLL_26MHZ       {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x26, 0x08, 0x20,\
                                         0x41, 0xA3, 0x01, 0x88, 0x01, 0xE2, 0x02}
#define SET_CONFIG_CMD_PLL_32MHZ        {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x20, 0x08, 0xB8, 0x51, 0xA3, 0x82, 0x88, 0xF1, 0xF0, 0x02}
#define SET_CONFIG_CMD_DPLL_32MHZ       {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x26, 0x08, 0xB0,\
                                         0x01, 0xA3, 0x82, 0x88, 0x01, 0xE2, 0x02}
#define SET_CONFIG_CMD_PLL_38_4MHZ      {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x20, 0x08, 0x88,\
                                         0x51, 0xE3, 0x82, 0x88, 0x21, 0xE1, 0x02}
#define SET_CONFIG_CMD_DPLL_38_4MHZ     {0x20, 0x02, 0x0C, 0x01, 0xA0, 0x26, 0x08, 0x88,\
                                        0x01, 0xE2, 0x82, 0xF0, 0x00, 0xA2, 0x01}
										
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
