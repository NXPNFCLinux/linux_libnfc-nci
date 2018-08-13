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
#ifndef _PHNXPNCIHAL_H_
#define _PHNXPNCIHAL_H_

#include "nfc_hal_api.h"
#include "phNxpNciHal_utils.h"
#include <phNxpConfig.h>

/********************* Definitions and structures *****************************/


/*Mem alloc with 8 byte alignment*/
#define size_align(sz) ((((sz)-1) | 7) + 1)
#define nxp_malloc(size) malloc(size_align((size)))

#define MAX_RETRY_COUNT       5
#define NCI_MAX_DATA_LEN      300
#define NCI_POLL_DURATION     500
#define HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT    0x07
#undef P2P_PRIO_LOGIC_HAL_IMP

#define NCI_VERSION_2_0 0x20
#define NCI_VERSION_1_1 0x11
#define NCI_VERSION_1_0 0x10
#define NCI_VERSION_UNKNOWN 0x00

/* NCI Data */
#define NXP_NCI_MT_CMD  0x20
#define NXP_NCI_MT_RSP  0x40
#define NXP_NCI_MT_NTF  0x60

#define CORE_RESET_TRIGGER_TYPE_CORE_RESET_CMD_RECEIVED 0x02
#define CORE_RESET_TRIGGER_TYPE_POWERED_ON              0x01
#define NXP_NCI_MSG_CORE_RESET           0x00
#define NXP_NCI_MSG_CORE_INIT            0x01
#define NXP_NCI_MT_MASK                  0xE0
#define NXP_NCI_OID_MASK                 0x3F

/*ROM CODE VERSION FW*/
#define FW_MOBILE_ROM_VERSION_PN551 0x10
#define FW_MOBILE_ROM_VERSION_PN553 0x11
#define FW_MOBILE_ROM_VERSION_PN548AD 0x10
#define FW_MOBILE_ROM_VERSION_PN547C2 0x08
#define FW_MOBILE_ROM_VERSION_PN557 0x12

#define FW_DBG_REASON_AVAILABLE     (0xA3)

typedef enum Nfc_chipType
{
    pnUnknown    = 0x00U,
    pn547C2      = 0x01U,
    pn65T        = 0x02U,
    pn548C2      = 0x03U,
    pnDefault    = 0x03U,
    pn66T        = 0x04U,
    pn551        = 0x05U,
    pn67T        = 0x06U,
    pn553        = 0x07U,
    pn80T        = 0x08U,
    pn557        = 0x09U,
    pn81T        = 0x0AU,
    pnInvalid    = 0xFFU

} tNFC_chipType;

typedef void (phNxpNciHal_control_granted_callback_t)();

/* NCI Data */
typedef struct nci_data
{
    uint16_t len;
    uint8_t p_data[NCI_MAX_DATA_LEN];
} nci_data_t;

typedef enum
{
   HAL_STATUS_OPEN = 0,
   HAL_STATUS_CLOSE
} phNxpNci_HalStatus;

/* Macros to enable and disable extensions */
#define HAL_ENABLE_EXT()    (nxpncihal_ctrl.hal_ext_enabled = 1)
#define HAL_DISABLE_EXT()   (nxpncihal_ctrl.hal_ext_enabled = 0)

/* NCI Control structure */
typedef struct phNxpNciHal_Control
{
    tNFC_chipType nfcChipType;

    phNxpNci_HalStatus   halStatus;      /* Indicate if hal is open or closed */
    pthread_t client_thread;  /* Integration thread handle */
    phLibNfc_sConfig_t   gDrvCfg; /* Driver config data */
    uint8_t   thread_running; /* Thread running if set to 1, else set to 0 */
    /* HAL extensions */
    uint8_t hal_ext_enabled;
    uint8_t hal_boot_mode;

    /* retry count used to force download */
    uint8_t read_retry_cnt;
    uint16_t retry_cnt;

    /* Rx data */
    uint16_t rx_data_len;
    uint8_t  *p_rx_data;

    /* HAL open status */
    bool_t hal_open_status;

    /* libnfc-nci callbacks */
    nfc_stack_callback_t *p_nfc_stack_cback;
    nfc_stack_data_callback_t *p_nfc_stack_data_cback;

    /* control granted callback */
    phNxpNciHal_control_granted_callback_t *p_control_granted_cback;

    /* Waiting semaphore */
    phNxpNciHal_Sem_t ext_cb_data;

    uint8_t p_cmd_data[NCI_MAX_DATA_LEN];
    uint8_t p_rsp_data[NCI_MAX_DATA_LEN];
    uint16_t cmd_len;
    uint16_t rsp_len;

} phNxpNciHal_Control_t;

typedef struct phNxpNciClock{
    bool_t  isClockSet;
    uint8_t  p_rx_data[20];
    bool_t  issetConfig;
}phNxpNciClock_t;

typedef struct phNxpNciRfSetting{
    bool_t  isGetRfSetting;
    uint8_t  p_rx_data[20];
}phNxpNciRfSetting_t;


typedef enum {
    NFC_FORUM_PROFILE,
    EMV_CO_PROFILE,
    INVALID_PROFILe
}phNxpNciProfile_t;
/* NXP Poll Profile control structure */
typedef struct phNxpNciProfile_Control
{
    phNxpNciProfile_t profile_type;
    uint8_t                      bClkSrcVal;     /* Holds the System clock source read from config file */
    uint8_t                      bClkFreqVal;    /* Holds the System clock frequency read from config file */
    uint8_t                      bTimeout;       /* Holds the Timeout Value */
} phNxpNciProfile_Control_t;

/* Internal messages to handle callbacks */
#define NCI_HAL_OPEN_CPLT_MSG             0x411
#define NCI_HAL_CLOSE_CPLT_MSG            0x412
#define NCI_HAL_POST_INIT_CPLT_MSG        0x413
#define NCI_HAL_PRE_DISCOVER_CPLT_MSG     0x414
#define NCI_HAL_ERROR_MSG                 0x415
#define NCI_HAL_RX_MSG                    0xF01

#define NCIHAL_CMD_CODE_LEN_BYTE_OFFSET         (2U)
#define NCIHAL_CMD_CODE_BYTE_LEN                (3U)

/******************** NCI HAL exposed functions *******************************/

void phNxpNciHal_request_control (void);
void phNxpNciHal_release_control (void);
int phNxpNciHal_write_unlocked (uint16_t data_len, const uint8_t *p_data);

tNFC_chipType phNxpNciHal_getChipType(void);
tNFC_chipType phNxpNciHal_deriveChipType(uint8_t* msg, uint16_t msg_len);

#endif /* _PHNXPNCIHAL_H_ */
