/******************************************************************************
 *
 *  Copyright (C) 2012-2014 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/


/******************************************************************************
 *
 *  This file contains the Broadcom-specific defintions that are shared
 *  between HAL, nfc stack, adaptation layer and applications.
 *
 ******************************************************************************/

#ifndef NFC_BRCM_DEFS_H
#define NFC_BRCM_DEFS_H

/*****************************************************************************
** Broadcom HW ID definitions
*****************************************************************************/
#define BRCM_20791B3_ID             0x20791b03
#define BRCM_20791B4_ID             0x20791b04
#define BRCM_20791B5_ID             0x20791b05
#define BRCM_43341B0_ID             0x43341b00
#define BRCM_20795T1_ID             0x20795a01
#define BRCM_20795A0_ID             0x20795a00
#define BRCM_20795A1_ID             0x20795a10

#define BRCM_NFC_GEN_MASK           0xFFFFF000  /* HW generation mask */
#define BRCM_NFC_REV_MASK           0x00000FFF  /* HW revision mask   */
#define BRCM_NFC_20791_GEN          0x20791000
#define BRCM_NFC_20791_GEN_MAX_EE   3           /* HCI access and 2 UICCs */
#define BRCM_NFC_43341_GEN          0x43341000
#define BRCM_NFC_43341_GEN_MAX_EE   3           /* HCI access and 2 UICCs */
#define BRCM_NFC_20795_GEN          0x20795000
#define BRCM_NFC_20795_GEN_MAX_EE   4           /* HCI access and 3 UICCs */

/*****************************************************************************
** Broadcom-specific NCI definitions
*****************************************************************************/

/**********************************************
 * NCI Message Proprietary  Group       - F
 **********************************************/
#define NCI_MSG_TAG_SET_MEM             0x00
#define NCI_MSG_TAG_GET_MEM             0x01
#define NCI_MSG_T1T_SET_HR              0x02
#define NCI_MSG_SET_CLF_REGISTERS       0x03
#define NCI_MSG_GET_BUILD_INFO          0x04
#define NCI_MSG_HCI_NETWK               0x05
#define NCI_MSG_SET_FWFSM               0x06
#define NCI_MSG_SET_UICCRDRF            0x07
#define NCI_MSG_POWER_LEVEL             0x08
#define NCI_MSG_FRAME_LOG               0x09
#define NCI_MSG_UICC_READER_ACTION      0x0A
#define NCI_MSG_SET_PPSE_RESPONSE       0x0B
#define NCI_MSG_PRBS_SET                0x0C
#define NCI_MSG_RESET_ALL_UICC_CFG      0x0D    /* reset HCI network/close all pipes (S,D) register */
#define NCI_MSG_GET_NFCEE_INFO          0x0E
#define NCI_MSG_DISABLE_INIT_CHECK      0x0F
#define NCI_MSG_ANTENNA_SELF_TEST       0x10
#define NCI_MSG_SET_MAX_PKT_SIZE        0x11
#define NCI_MSG_NCIP_CLK_REQ_OR_CAR_DET 0x12
#define NCI_MSG_NCIP_CONFIG_DBUART      0x13
#define NCI_MSG_NCIP_ENABLE_DVT_DRIVER  0x14
#define NCI_MSG_SET_ASWP                0x15
#define NCI_MSG_ENCAPSULATE_NCI         0x16
#define NCI_MSG_CONFIGURE_ARM_JTAG      0x17
#define NCI_MSG_STATISTICS              0x18
#define NCI_MSG_SET_DSP_TABLE           0x19
#define NCI_MSG_GET_DSP_TABLE           0x1a
#define NCI_MSG_READY_RX_CMD            0x1b
#define NCI_MSG_GET_VBAT                0x1c
#define NCI_MSG_GET_XTAL_INDEX_FROM_DH  0x1d
#define NCI_MSG_SWP_LOG                 0x1e
#define NCI_MSG_GET_PWRLEVEL            0x1f
#define NCI_MSG_SET_VBAT_MONITOR        0x20
#define NCI_MSG_SET_TINT_MODE           0x21
#define NCI_MSG_ACCESS_APP              0x22
#define NCI_MSG_SET_SECURE_MODE         0x23
#define NCI_MSG_GET_NV_DEVICE           0x24
#define NCI_MSG_LPTD                    0x25
#define NCI_MSG_SET_CE4_AS_SNOOZE       0x26
#define NCI_MSG_NFCC_SEND_HCI           0x27
#define NCI_MSG_CE4_PATCH_DOWNLOAD_DONE 0x28
#define NCI_MSG_EEPROM_RW               0x29
#define NCI_MSG_GET_CLF_REGISTERS       0x2A
#define NCI_MSG_RF_TEST                 0x2B
#define NCI_MSG_DEBUG_PRINT             0x2C
#define NCI_MSG_GET_PATCH_VERSION       0x2D
#define NCI_MSG_SECURE_PATCH_DOWNLOAD   0x2E
#define NCI_MSG_SPD_FORMAT_NVM          0x2F
#define NCI_MSG_SPD_READ_NVM            0x30
#define NCI_MSG_SWP_BIST                0x31
#define NCI_MSG_WLESS_DBG_MODE          0x32
#define NCI_MSG_I2C_REQ_POLARITY        0x33
#define NCI_MSG_AID_FILTER              0x39


/**********************************************
 * Proprietary  NCI status codes
 **********************************************/
#define NCI_STATUS_SPD_ERROR_ORDER          0xE0
#define NCI_STATUS_SPD_ERROR_DEST           0xE1
#define NCI_STATUS_SPD_ERROR_PROJECTID      0xE2
#define NCI_STATUS_SPD_ERROR_CHIPVER        0xE3
#define NCI_STATUS_SPD_ERROR_MAJORVER       0xE4
#define NCI_STATUS_SPD_ERROR_INVALID_PARAM  0xE5
#define NCI_STATUS_SPD_ERROR_INVALID_SIG    0xE6
#define NCI_STATUS_SPD_ERROR_NVM_CORRUPTED  0xE7
#define NCI_STATUS_SPD_ERROR_PWR_MODE       0xE8
#define NCI_STATUS_SPD_ERROR_MSG_LEN        0xE9
#define NCI_STATUS_SPD_ERROR_PATCHSIZE      0xEA



#define NCI_NV_DEVICE_NONE              0x00
#define NCI_NV_DEVICE_EEPROM            0x08
#define NCI_NV_DEVICE_UICC1             0x10

/* The events reported on tNFC_VS_CBACK */
/* The event is (NCI_NTF_BIT|oid) or (NCI_RSP_BIT|oid) */
#define NFC_VS_HCI_NETWK_EVT            (NCI_NTF_BIT|NCI_MSG_HCI_NETWK)
#define NFC_VS_HCI_NETWK_RSP            (NCI_RSP_BIT|NCI_MSG_HCI_NETWK)
#define NFC_VS_UICC_READER_ACTION_EVT   (NCI_NTF_BIT|NCI_MSG_UICC_READER_ACTION)
#define NFC_VS_POWER_LEVEL_RSP          (NCI_RSP_BIT|NCI_MSG_POWER_LEVEL)
#define NFC_VS_GET_NV_DEVICE_EVT        (NCI_RSP_BIT|NCI_MSG_GET_NV_DEVICE)
#define NFC_VS_LPTD_EVT                 (NCI_NTF_BIT|NCI_MSG_LPTD)
#define NFC_VS_GET_BUILD_INFO_EVT       (NCI_RSP_BIT|NCI_MSG_GET_BUILD_INFO)
#define NFC_VS_GET_PATCH_VERSION_EVT    (NCI_RSP_BIT|NCI_MSG_GET_PATCH_VERSION)
#define NFC_VS_SEC_PATCH_DOWNLOAD_EVT   (NCI_RSP_BIT|NCI_MSG_SECURE_PATCH_DOWNLOAD)
#define NFC_VS_SEC_PATCH_AUTH_EVT       (NCI_NTF_BIT|NCI_MSG_SECURE_PATCH_DOWNLOAD)
#define NFC_VS_EEPROM_RW_EVT            (NCI_RSP_BIT|NCI_MSG_EEPROM_RW)

#define NCI_GET_PATCH_VERSION_NVM_OFFSET    37


#define NCI_NFCC_PIPE_INFO_NV_SIZE      24  /* Static and dynamic pipe id and status for each pipe to uicc0 and uicc1. */
#define NCI_PERSONALITY_SLOT_SIZE       19
#define NCI_DYNAMIC_PIPE_SIZE           8

#define NCI_SWP_INTERFACE_TYPE          0xFF    /* Type of TLV in NCI_MSG_HCI_NETWK */
#define NCI_HCI_GATE_TYPE               0xFE    /* Type of TLV in NCI_MSG_HCI_NETWK */

/* Secure Patch Download definitions (patch type definitions) */
#define NCI_SPD_TYPE_HEADER             0x00
#define NCI_SPD_TYPE_SRAM               0x01
#define NCI_SPD_TYPE_AON                0x02
#define NCI_SPD_TYPE_PATCH_TABLE        0x03
#define NCI_SPD_TYPE_SECURE_CONFIG      0x04
#define NCI_SPD_TYPE_CONTROLLED_CONFIG  0x05
#define NCI_SPD_TYPE_SIGNATURE          0x06
#define NCI_SPD_TYPE_SIGCHEK            0x07

/* Secure Patch Download definitions (NCI_SPD_TYPE_HEADER definitions) */
#define NCI_SPD_HEADER_OFFSET_CHIPVERLEN    0x18
#define NCI_SPD_HEADER_CHIPVER_LEN          16

/* NVM Type (in GET_PATCH_VERSION RSP) */
#define NCI_SPD_NVM_TYPE_NONE           0x00
#define NCI_SPD_NVM_TYPE_EEPROM         0x01
#define NCI_SPD_NVM_TYPE_UICC           0x02

/**********************************************
 * NCI NFCC proprietary features in octet 3
 **********************************************/
#define NCI_FEAT_SIGNED_PATCH           0x01000000

/**********************************************
 * NCI Interface Types
 **********************************************/
#define NCI_INTERFACE_VS_CALYPSO_CE     0x81
#define NCI_INTERFACE_VS_T2T_CE         0x82    /* for Card Emulation side */
#define NCI_INTERFACE_VS_15693          0x83    /* for both Reader/Writer and Card Emulation side */
#define NCI_INTERFACE_VS_T1T_CE         0x84    /* for Card Emulation side */

/**********************************************
 * NCI Proprietary Parameter IDs
 **********************************************/
#define NCI_PARAM_ID_LA_FSDI            0xA0
#define NCI_PARAM_ID_LB_FSDI            0xA1
#define NCI_PARAM_ID_HOST_LISTEN_MASK   0xA2
#define NCI_PARAM_ID_CHIP_TYPE          0xA3 /* NFCDEP */
#define NCI_PARAM_ID_PA_ANTICOLL        0xA4
#define NCI_PARAM_ID_CONTINUE_MODE      0xA5
#define NCI_PARAM_ID_LBP                0xA6
#define NCI_PARAM_ID_T1T_RDR_ONLY       0xA7
#define NCI_PARAM_ID_LA_SENS_RES        0xA8
#define NCI_PARAM_ID_PWR_SETTING_BITMAP 0xA9
#define NCI_PARAM_ID_WI_NTF_ENABLE      0xAA
#define NCI_PARAM_ID_LN_BITRATE         0xAB /* NFCDEP Listen Bitrate */
#define NCI_PARAM_ID_LF_BITRATE         0xAC /* FeliCa */
#define NCI_PARAM_ID_SWP_BITRATE_MASK   0xAD
#define NCI_PARAM_ID_KOVIO              0xAE
#define NCI_PARAM_ID_UICC_NTF_TO        0xAF
#define NCI_PARAM_ID_NFCDEP             0xB0
#define NCI_PARAM_ID_CLF_REGS_CFG       0xB1
#define NCI_PARAM_ID_NFCDEP_TRANS_TIME  0xB2
#define NCI_PARAM_ID_CREDIT_TIMER       0xB3
#define NCI_PARAM_ID_CORRUPT_RX         0xB4
#define NCI_PARAM_ID_ISODEP             0xB5
#define NCI_PARAM_ID_LF_CONFIG          0xB6
#define NCI_PARAM_ID_I93_DATARATE       0xB7
#define NCI_PARAM_ID_CREDITS_THRESHOLD  0xB8
#define NCI_PARAM_ID_TAGSNIFF_CFG       0xB9
#define NCI_PARAM_ID_PA_FSDI            0xBA /* ISODEP */
#define NCI_PARAM_ID_PB_FSDI            0xBB /* ISODEP */
#define NCI_PARAM_ID_FRAME_INTF_RETXN   0xBC

#define NCI_PARAM_ID_UICC_RDR_PRIORITY  0xBD
#define NCI_PARAM_ID_GUARD_TIME         0xBE
#define NCI_PARAM_ID_STDCONFIG          0xBF /* dont not use this config item */
#define NCI_PARAM_ID_PROPCFG            0xC0 /* dont not use this config item  */
#define NCI_PARAM_ID_MAXTRY2ACTIVATE    0xC1
#define NCI_PARAM_ID_SWPCFG             0xC2
#define NCI_PARAM_ID_CLF_LPM_CFG        0xC3
#define NCI_PARAM_ID_DCLB               0xC4
#define NCI_PARAM_ID_ACT_ORDER          0xC5
#define NCI_PARAM_ID_DEP_DELAY_ACT      0xC6
#define NCI_PARAM_ID_DH_PARITY_CRC_CTL  0xC7
#define NCI_PARAM_ID_PREINIT_DSP_CFG    0xC8
#define NCI_PARAM_ID_FW_WORKAROUND      0xC9
#define NCI_PARAM_ID_RFU_CONFIG         0xCA
#define NCI_PARAM_ID_EMVCO_ENABLE       0xCB
#define NCI_PARAM_ID_ANTDRIVER_PARAM    0xCC
#define NCI_PARAM_ID_PLL325_CFG_PARAM   0xCD
#define NCI_PARAM_ID_OPNLP_ADPLL_ENABLE 0xCE
#define NCI_PARAM_ID_CONFORMANCE_MODE   0xCF

#define NCI_PARAM_ID_LPO_ON_OFF_ENABLE  0xD0
#define NCI_PARAM_ID_FORCE_VANT         0xD1
#define NCI_PARAM_ID_COEX_CONFIG        0xD2
#define NCI_PARAM_ID_INTEL_MODE         0xD3

#define NCI_PARAM_ID_AID                0xFF

/**********************************************
 * NCI Parameter ID Lens
 **********************************************/
#define NCI_PARAM_LEN_PWR_SETTING_BITMAP    3
#define NCI_PARAM_LEN_HOST_LISTEN_MASK      2
#define NCI_PARAM_LEN_PLL325_CFG_PARAM      14

/**********************************************
 * Snooze Mode
 **********************************************/
#define NFC_SNOOZE_MODE_NONE      0x00    /* Snooze mode disabled    */
#define NFC_SNOOZE_MODE_UART      0x01    /* Snooze mode for UART    */
#define NFC_SNOOZE_MODE_SPI_I2C   0x08    /* Snooze mode for SPI/I2C */

#define NFC_SNOOZE_ACTIVE_LOW     0x00    /* high to low voltage is asserting */
#define NFC_SNOOZE_ACTIVE_HIGH    0x01    /* low to high voltage is asserting */


/**********************************************
 * HCI definitions
 **********************************************/
#define NFC_HAL_HCI_SESSION_ID_LEN                  8
#define NFC_HAL_HCI_SYNC_ID_LEN                     2

/* HCI Network command definitions */
#define NFC_HAL_HCI_NETWK_INFO_SIZE                 250
#define NFC_HAL_HCI_NO_RW_MODE_NETWK_INFO_SIZE      184
#define NFC_HAL_HCI_DH_NETWK_INFO_SIZE              111
#define NFC_HAL_HCI_MIN_NETWK_INFO_SIZE             12
#define NFC_HAL_HCI_MIN_DH_NETWK_INFO_SIZE          11

/* Card emulation RF Gate A definitions */
#define NFC_HAL_HCI_CE_RF_A_UID_REG_LEN             10
#define NFC_HAL_HCI_CE_RF_A_ATQA_RSP_CODE_LEN       2
#define NFC_HAL_HCI_CE_RF_A_MAX_HIST_DATA_LEN       15
#define NFC_HAL_HCI_CE_RF_A_MAX_DATA_RATE_LEN       3

/* Card emulation RF Gate B definitions */
#define NFC_HAL_HCI_CE_RF_B_PUPI_LEN                4
#define NFC_HAL_HCI_CE_RF_B_ATQB_LEN                4
#define NFC_HAL_HCI_CE_RF_B_HIGHER_LAYER_RSP_LEN    61
#define NFC_HAL_HCI_CE_RF_B_MAX_DATA_RATE_LEN       3

/* Card emulation RF Gate BP definitions */
#define NFC_HAL_HCI_CE_RF_BP_MAX_PAT_IN_LEN         8
#define NFC_HAL_HCI_CE_RF_BP_DATA_OUT_LEN           40

/* Reader RF Gate A definitions */
#define NFC_HAL_HCI_RD_RF_B_HIGHER_LAYER_DATA_LEN   61

/* DH HCI Network command definitions */
#define NFC_HAL_HCI_DH_MAX_DYN_PIPES                20

/* Target handle for different host in the network */
#define NFC_HAL_HCI_DH_TARGET_HANDLE                0xF2
#define NFC_HAL_HCI_UICC0_TARGET_HANDLE             0xF3
#define NFC_HAL_HCI_UICC1_TARGET_HANDLE             0xF4
#define NFC_HAL_HCI_UICC2_TARGET_HANDLE             0xF5

/* Card emulation RF Gate A registry information */
typedef struct
{
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
    UINT8   mode;                                                       /* Type A card emulation enabled indicator, 0x02:enabled    */
    UINT8   sak;
    UINT8   uid_reg_len;
    UINT8   uid_reg[NFC_HAL_HCI_CE_RF_A_UID_REG_LEN];
    UINT8   atqa[NFC_HAL_HCI_CE_RF_A_ATQA_RSP_CODE_LEN];                /* ATQA response code */
    UINT8   app_data_len;
    UINT8   app_data[NFC_HAL_HCI_CE_RF_A_MAX_HIST_DATA_LEN];            /* 15 bytes optional storage for historic data, use 2 slots */
    UINT8   fwi_sfgi;                                                   /* FRAME WAITING TIME, START-UP FRAME GUARD TIME            */
    UINT8   cid_support;
    UINT8   datarate_max[NFC_HAL_HCI_CE_RF_A_MAX_DATA_RATE_LEN];
    UINT8   clt_support;
} tNCI_HCI_CE_RF_A;

/* Card emulation RF Gate B registry information */
typedef struct
{
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
    UINT8   mode;                                                       /* Type B card emulation enabled indicator, 0x02:enabled    */
    UINT8   pupi_len;
    UINT8   pupi_reg[NFC_HAL_HCI_CE_RF_B_PUPI_LEN];
    UINT8   afi;
    UINT8   atqb[NFC_HAL_HCI_CE_RF_B_ATQB_LEN];                         /* 4 bytes ATQB application data                            */
    UINT8   higherlayer_resp[NFC_HAL_HCI_CE_RF_B_HIGHER_LAYER_RSP_LEN]; /* 0~ 61 bytes ATRB_INF use 1~4 personality slots     */
    UINT8   datarate_max[NFC_HAL_HCI_CE_RF_B_MAX_DATA_RATE_LEN];
    UINT8   natrb;
} tNCI_HCI_CE_RF_B;

/* Card emulation RF Gate BP registry information */
typedef struct
{
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
    UINT8   mode;                                                       /* Type B prime card emulation enabled indicator, 0x02:enabled */
    UINT8   pat_in_len;
    UINT8   pat_in[NFC_HAL_HCI_CE_RF_BP_MAX_PAT_IN_LEN];
    UINT8   dat_out_len;
    UINT8   dat_out[NFC_HAL_HCI_CE_RF_BP_DATA_OUT_LEN];                 /* ISO7816-3 <=64 byte, and other fields are 9 bytes        */
    UINT8   natr;
} tNCI_HCI_CE_RF_BP;

/* Card emulation RF Gate F registry information */
typedef struct
{
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
    UINT8   mode;                                                       /* Type F card emulation enabled indicator, 0x02:enabled    */
    UINT8   speed_cap;
    UINT8   clt_support;
} tNCI_HCI_CE_RF_F;

/* Reader RF Gate A registry information */
typedef struct
{
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
    UINT8   datarate_max;
} tNCI_HCI_RD_RF_A;

/* Reader RF Gate B registry information */
typedef struct
{
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
    UINT8   afi;
    UINT8   hldata_len;
    UINT8   high_layer_data[NFC_HAL_HCI_RD_RF_B_HIGHER_LAYER_DATA_LEN]; /* INF field in ATTRIB command                        */
} tNCI_HCI_RD_RF_B;

/* Dynamic pipe information */
typedef struct
{
    UINT8   source_host;
    UINT8   dest_host;
    UINT8   source_gate;
    UINT8   dest_gate;
    UINT8   pipe_id;                                                    /* if MSB is set then valid, 7 bits for Pipe ID             */
} tNCI_HCI_DYN_PIPE_INFO;

/*************************************************************
 * HCI Network CMD/NTF structure for UICC host in the network
 *************************************************************/
typedef struct
{
    UINT8                target_handle;
    UINT8                session_id[NFC_HAL_HCI_SESSION_ID_LEN];
    UINT8                sync_id[NFC_HAL_HCI_SYNC_ID_LEN];
    UINT8                static_pipe_info;
    tNCI_HCI_CE_RF_A     ce_rf_a;
    tNCI_HCI_CE_RF_B     ce_rf_b;
    tNCI_HCI_CE_RF_BP    ce_rf_bp;
    tNCI_HCI_CE_RF_F     ce_rf_f;
    tNCI_HCI_RD_RF_A     rw_rf_a;
    tNCI_HCI_RD_RF_B     rw_rf_b;
} tNCI_HCI_NETWK;

/************************************************
 * HCI Network CMD/NTF structure for Device host
 ************************************************/
typedef struct
{
    UINT8                   target_handle;
    UINT8                   session_id[NFC_HAL_HCI_SESSION_ID_LEN];
    UINT8                   static_pipe_info;
    UINT8                   num_dyn_pipes;
    tNCI_HCI_DYN_PIPE_INFO  dyn_pipe_info[NFC_HAL_HCI_DH_MAX_DYN_PIPES];
} tNCI_HCI_NETWK_DH;

#endif  /* NFC_BRCM_DEFS_H */
