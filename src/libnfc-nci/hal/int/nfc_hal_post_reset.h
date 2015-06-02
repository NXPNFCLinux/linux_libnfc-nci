/******************************************************************************
 *
 *  Copyright (C) 2009-2014 Broadcom Corporation
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
 *  Post NCI reset routines
 *
 ******************************************************************************/
#ifndef NFC_HAL_POST_RESET_H
#define NFC_HAL_POST_RESET_H


/*****************************************************************************
** Application control block definitions
******************************************************************************/
#define NFA_APP_PATCHFILE_MAX_PATH          255
#define NFA_APP_MAX_NUM_REINIT                5

typedef struct
{
    UINT8 prm_file[NFA_APP_PATCHFILE_MAX_PATH+1];   /* Filename of patchram */
    UINT8 *p_prm_buf;                               /* Pointer to buffer for holding patchram data */

    /* Patchfile for I2C fix */
    UINT8 prm_i2c_patchfile[NFA_APP_PATCHFILE_MAX_PATH+1];
    UINT8 *p_prm_i2c_buf;

    UINT8 userial_baud;

    tNFC_HAL_DEV_INIT_CFG dev_init_config;

    /* snooze mode setting */
    UINT8 snooze_mode;
    UINT8 idle_threshold_dh;
    UINT8 idle_threshold_nfcc;
    UINT8 nfc_wake_active_mode;
    UINT8 dh_wake_active_mode;

    /* NVM detection retry (some platforms require re-attempts to detect NVM) */
    UINT8 spd_nvm_detection_max_count;  /* max retry to get NVM type */
    UINT8 spd_nvm_detection_cur_count;  /* current retry count       */

    /* handling for failure to download patch */
    BOOLEAN spd_debug_mode;             /* debug mode for downloading patchram, report failure immediately and obviously */
    BOOLEAN spd_skip_on_power_cycle;    /* skip downloading patchram after power cycle because of patch download failure */
} tNFC_POST_RESET_CB;
extern tNFC_POST_RESET_CB nfc_post_reset_cb;

/*
** Post NCI reset handler
**
** This function is called to start device pre-initialization after NCI CORE-RESET.
** When pre-initialization is completed,
** HAL_NfcPreInitDone() must be called to proceed with stack start up.
*/
void nfc_hal_post_reset_init (UINT32 brcm_hw_id, UINT8 nvm_type);

#endif  /* NFC_HAL_POST_RESET_H */
