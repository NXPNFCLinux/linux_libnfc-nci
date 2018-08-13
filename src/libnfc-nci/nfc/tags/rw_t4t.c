/******************************************************************************
 *
 *  Copyright (C) 2010-2014 Broadcom Corporation
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
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2013-2014 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 *  This file contains the implementation for Type 4 tag in Reader/Writer
 *  mode.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_target.h"
#include "bt_types.h"
#include "trace_api.h"

#if (NFC_INCLUDED == TRUE)

#include "nfc_api.h"
#include "nfc_int.h"
#include "rw_api.h"
#include "rw_int.h"
#include "tags_int.h"
#include "gki.h"

/* main state */
#define RW_T4T_STATE_NOT_ACTIVATED              0x00    /* T4T is not activated                 */
#define RW_T4T_STATE_IDLE                       0x01    /* waiting for upper layer API          */
#define RW_T4T_STATE_DETECT_NDEF                0x02    /* performing NDEF detection precedure  */
#define RW_T4T_STATE_READ_NDEF                  0x03    /* performing read NDEF procedure       */
#define RW_T4T_STATE_UPDATE_NDEF                0x04    /* performing update NDEF procedure     */
#define RW_T4T_STATE_PRESENCE_CHECK             0x05    /* checking presence of tag             */
#define RW_T4T_STATE_SET_READ_ONLY              0x06    /* convert tag to read only             */

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define RW_T4T_STATE_NDEF_FORMAT                0x07    /* performing NDEF format               */
#define RW_T3BT_STATE_GET_PROP_DATA             0x08
#endif
/* sub state */
#define RW_T4T_SUBSTATE_WAIT_SELECT_APP         0x00    /* waiting for response of selecting AID    */
#define RW_T4T_SUBSTATE_WAIT_SELECT_CC          0x01    /* waiting for response of selecting CC     */
#define RW_T4T_SUBSTATE_WAIT_CC_FILE            0x02    /* waiting for response of reading CC       */
#define RW_T4T_SUBSTATE_WAIT_SELECT_NDEF_FILE   0x03    /* waiting for response of selecting NDEF   */
#define RW_T4T_SUBSTATE_WAIT_READ_NLEN          0x04    /* waiting for response of reading NLEN     */
#define RW_T4T_SUBSTATE_WAIT_READ_RESP          0x05    /* waiting for response of reading file     */
#define RW_T4T_SUBSTATE_WAIT_UPDATE_RESP        0x06    /* waiting for response of updating file    */
#define RW_T4T_SUBSTATE_WAIT_UPDATE_NLEN        0x07    /* waiting for response of updating NLEN    */
#define RW_T4T_SUBSTATE_WAIT_UPDATE_CC          0x08    /* waiting for response of updating CC      */

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define RW_T4T_SUBSTATE_WAIT_GET_HW_VERSION     0x09
#define RW_T4T_SUBSTATE_WAIT_GET_SW_VERSION     0x0A
#define RW_T4T_SUBSTATE_WAIT_GET_UID            0x0B
#define RW_T4T_SUBSTATE_WAIT_CREATE_APP         0x0C
#define RW_T4T_SUBSTATE_WAIT_CREATE_CC          0x0D
#define RW_T4T_SUBSTATE_WAIT_CREATE_NDEF        0x0E
#define RW_T4T_SUBSTATE_WAIT_WRITE_CC           0x0F
#define RW_T4T_SUBSTATE_WAIT_WRITE_NDEF         0x10
#define RW_T3BT_SUBSTATE_WAIT_GET_ATTRIB        0x11
#define RW_T3BT_SUBSTATE_WAIT_GET_PUPI          0x12
#endif

#if (BT_TRACE_VERBOSE == TRUE)
static char *rw_t4t_get_state_name (UINT8 state);
static char *rw_t4t_get_sub_state_name (UINT8 sub_state);
#endif

static BOOLEAN rw_t4t_send_to_lower (BT_HDR *p_c_apdu);
static BOOLEAN rw_t4t_select_file (UINT16 file_id);
static BOOLEAN rw_t4t_read_file (UINT16 offset, UINT16 length, BOOLEAN is_continue);
static BOOLEAN rw_t4t_update_nlen (UINT16 ndef_len);
static BOOLEAN rw_t4t_update_file (void);
static BOOLEAN rw_t4t_update_cc_to_readonly (void);
static BOOLEAN rw_t4t_select_application (UINT8 version);
static BOOLEAN rw_t4t_validate_cc_file (void);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
static BOOLEAN rw_t4t_get_hw_version (void);
static BOOLEAN rw_t4t_get_sw_version (void);
static BOOLEAN rw_t4t_create_app (void);
static BOOLEAN rw_t4t_select_app (void);
static BOOLEAN rw_t4t_create_ccfile (void);
static BOOLEAN rw_t4t_create_ndef (void);
static BOOLEAN rw_t4t_write_cc (void);
static BOOLEAN rw_t4t_write_ndef (void);
static BOOLEAN rw_t3bt_get_pupi (void);
#endif
static void rw_t4t_handle_error (tNFC_STATUS status, UINT8 sw1, UINT8 sw2);
static void rw_t4t_sm_detect_ndef (BT_HDR *p_r_apdu);
static void rw_t4t_sm_read_ndef (BT_HDR *p_r_apdu);
static void rw_t4t_sm_update_ndef (BT_HDR  *p_r_apdu);
static void rw_t4t_sm_set_readonly (BT_HDR  *p_r_apdu);
static void rw_t4t_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
static void rw_t4t_sm_ndef_format (BT_HDR  *p_r_apdu);
static void rw_t3Bt_sm_get_card_id(BT_HDR *p_r_apdu);
#endif
/*******************************************************************************
**
** Function         rw_t4t_send_to_lower
**
** Description      Send C-APDU to lower layer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_send_to_lower (BT_HDR *p_c_apdu)
{
#if (BT_TRACE_PROTOCOL == TRUE)
    DispRWT4Tags (p_c_apdu, FALSE);
#endif

    if (NFC_SendData (NFC_RF_CONN_ID, p_c_apdu) != NFC_STATUS_OK)
    {
        RW_TRACE_ERROR0 ("rw_t4t_send_to_lower (): NFC_SendData () failed");
        return FALSE;
    }

    nfc_start_quick_timer (&rw_cb.tcb.t4t.timer, NFC_TTYPE_RW_T4T_RESPONSE,
                           (RW_T4T_TOUT_RESP * QUICK_TIMER_TICKS_PER_SEC) / 1000);

    return TRUE;
}

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         rw_t4t_get_hw_version
**
** Description      Send get hw version cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_get_hw_version (void)
{
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_get_hw_version (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_GET_HW_VERSION);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_FIELD(p,0x00);

    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_get_sw_version
**
** Description      Send get sw version cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_get_sw_version (void)
{
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_get_sw_version (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_ADDI_FRAME_RESP);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_FIELD(p,0x00);

    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_update_version_details
**
** Description      Updates the size of the card
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_update_version_details(BT_HDR *p_r_apdu)
{
    tRW_T4T_CB      *p_t4t = &rw_cb.tcb.t4t;
    UINT8 *p;
    UINT16 major_version, minor_version;

    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;
    major_version = *(p+3);
    minor_version = *(p+4);

    if((T4T_DESEV0_MAJOR_VERSION == major_version) &&
            (T4T_DESEV0_MINOR_VERSION == minor_version))
    {
            p_t4t->card_size = 0xEDE;
    }
    else if(major_version >= T4T_DESEV1_MAJOR_VERSION)
    {
            p_t4t->card_type = T4T_TYPE_DESFIRE_EV1;
        switch (*(p + 5))
        {
        case T4T_SIZE_IDENTIFIER_2K:
            p_t4t->card_size = 2048;
            break;
        case T4T_SIZE_IDENTIFIER_4K:
            p_t4t->card_size = 4096;
            break;
        case T4T_SIZE_IDENTIFIER_8K:
            p_t4t->card_size = 7680;
            break;
        default:
            return FALSE;
            break;
        }
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_get_uid_details
**
** Description      Send get uid cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_get_uid_details (void)
{
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_get_uid_details (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_ADDI_FRAME_RESP);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_FIELD(p,0x00);

    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_create_app
**
** Description      Send create application cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_create_app (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;
        UINT8        df_name[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_create_app (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_CREATE_AID);
    UINT16_TO_BE_STREAM (p, 0x0000);
    if(p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)
    {
            UINT8_TO_BE_STREAM(p,(T4T_CMD_MAX_HDR_SIZE + sizeof(df_name)+2));
            UINT24_TO_BE_STREAM(p, T4T_DES_EV1_NFC_APP_ID);
        UINT16_TO_BE_STREAM(p, 0x0F21);                  /*Key settings and no.of keys */
        UINT16_TO_BE_STREAM(p, 0x05E1);                  /* ISO file ID */
        ARRAY_TO_BE_STREAM(p,df_name,sizeof(df_name));   /*DF file name */
        UINT8_TO_BE_STREAM(p,0x00);                      /* Le */
        p_c_apdu->len = 20;
    }
    else
    {
            UINT8_TO_BE_STREAM(p, T4T_CMD_MAX_HDR_SIZE);
            UINT24_TO_BE_STREAM(p, T4T_DES_EV0_NFC_APP_ID);
            UINT16_TO_BE_STREAM(p, 0x0F01);                  /*Key settings and no.of keys */
            UINT8_TO_BE_STREAM(p,0x00);                      /* Le */
            p_c_apdu->len = 11;
    }

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_select_app
**
** Description      Select application cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_select_app (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_select_app (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_SELECT_APP);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_STREAM(p,0x03);                      /* Lc: length of wrapped data */
    if(p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)
    {
        UINT24_TO_BE_STREAM(p, T4T_DES_EV1_NFC_APP_ID);
    }
    else
    {
            UINT24_TO_BE_STREAM(p, T4T_DES_EV0_NFC_APP_ID);
    }
    UINT8_TO_BE_STREAM(p,0x00);                      /* Le */

    p_c_apdu->len = 9;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_create_ccfile
**
** Description      create capability container file cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_create_ccfile (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_create_ccfile (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_CREATE_DATAFILE);
    UINT16_TO_BE_STREAM (p, 0x0000);
    if(p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)
    {
        UINT8_TO_BE_STREAM(p,0x09);                    /* Lc: length of wrapped data */
            UINT8_TO_BE_STREAM(p,0x01);                    /* EV1 CC file id             */
        UINT16_TO_BE_STREAM(p,0x03E1);                 /* ISO file id                */
    }
    else
    {
            UINT8_TO_BE_STREAM(p,0x07);               /* Lc: length of wrapped data */
            UINT8_TO_BE_STREAM(p,0x03);               /* DESFire CC file id         */
    }
    UINT8_TO_BE_STREAM(p,0x00);                   /* COMM settings              */
    UINT16_TO_BE_STREAM(p,0xEEEE);                /* Access rights              */
    UINT24_TO_BE_STREAM(p,0x0F0000);              /* Set file size              */
    UINT8_TO_BE_STREAM(p,0x00);                   /* Le                         */

    p_c_apdu->len = (p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)?15:13;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_create_ndef
**
** Description      creates an ndef file cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_create_ndef (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_create_ndef (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_CREATE_DATAFILE);
    UINT16_TO_BE_STREAM (p, 0x0000);
    if(p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)
    {
        UINT8_TO_BE_STREAM(p,0x09);                   /* Lc: length of wrapped data */
            UINT8_TO_BE_STREAM(p,0x02);                   /* DESFEv1 NDEF file id       */
        UINT16_TO_BE_STREAM(p,0x04E1);                /* ISO file id                */
    }
    else
    {
            UINT8_TO_BE_STREAM(p,0x07);
            UINT8_TO_BE_STREAM(p,0x04);                   /* DESF4 NDEF file id        */
    }

    UINT8_TO_BE_STREAM(p,0x00);                       /* COMM settings              */
    UINT16_TO_BE_STREAM(p,0xEEEE);                    /* Access rights              */
    UINT16_TO_STREAM(p,p_t4t->card_size);
        UINT8_TO_BE_STREAM(p,0x00);                       /* Set card size              */
    UINT8_TO_BE_STREAM(p,0x00);                       /* Le                         */

    p_c_apdu->len = (p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)?15:13;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_write_cc
**
** Description      sends write cc file cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_write_cc (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;
    UINT8        CCFileBytes[]  = {0x00,0x0f,0x10,0x00,0x3B,0x00,0x34,0x04,0x06,0xE1,0x04,0x04,0x00,0x00,0x00};

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_write_cc (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_WRITE);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_STREAM(p,0x16);                    /* Lc: length of wrapped data  */
    if(p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)
    {
            CCFileBytes[2]  = 0x20;
            CCFileBytes[11] = p_t4t->card_size >> 8;
            CCFileBytes[12] = (UINT8) p_t4t->card_size;
        UINT8_TO_BE_STREAM(p, 0x01);               /* CC file id                  */
    }
    else
    {
            UINT8_TO_BE_STREAM(p, 0x03);
    }
    UINT24_TO_BE_STREAM(p, 0x000000);              /* Set the offset              */
    UINT24_TO_BE_STREAM(p, 0x0F0000);              /* Set available length        */
    ARRAY_TO_BE_STREAM(p, CCFileBytes, sizeof(CCFileBytes));
    UINT8_TO_BE_STREAM(p,0x00);                    /* Le                         */

    p_c_apdu->len = 28;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_write_ndef
**
** Description      sends write ndef file cmd to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_write_ndef (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_write_ndef (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_DES_WRITE);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_STREAM(p,0x09);                    /* Lc: length of wrapped data  */
    if(p_t4t->card_type == T4T_TYPE_DESFIRE_EV1)
    {
        UINT8_TO_BE_STREAM(p, 0x02);               /* DESFEv1 Ndef file id        */
    }
    else
    {
            UINT8_TO_BE_STREAM(p, 0x04);
    }
    UINT24_TO_BE_STREAM(p, 0x000000);              /* Set the offset              */
    UINT24_TO_BE_STREAM(p, 0x020000);              /* Set available length        */
    UINT16_TO_BE_STREAM(p, 0x0000);                /* Ndef file bytes             */
    UINT8_TO_BE_STREAM(p,0x00);                    /* Le                          */

    p_c_apdu->len = 15;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }
    return TRUE;
}
#endif

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
static BOOLEAN rw_t3bt_get_pupi (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t3bt_get_pupi (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, 0x00);
    UINT8_TO_BE_STREAM (p, 0x36);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT8_TO_BE_STREAM(p,0x08);                    /* Lc: length of wrapped data  */

    p_c_apdu->len = 0x05;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }
    return TRUE;
}
#endif
/*******************************************************************************
**
** Function         rw_t4t_select_file
**
** Description      Send Select Command (by File ID) to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_select_file (UINT16 file_id)
{
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    RW_TRACE_DEBUG1 ("rw_t4t_select_file (): File ID:0x%04X", file_id);

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_select_file (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_SELECT);
    UINT8_TO_BE_STREAM (p, T4T_CMD_P1_SELECT_BY_FILE_ID);

    /* if current version mapping is V2.0 */
    if (rw_cb.tcb.t4t.version == T4T_VERSION_2_0)
    {
        UINT8_TO_BE_STREAM (p, T4T_CMD_P2_FIRST_OR_ONLY_0CH);
    }
    else /* version 1.0 */
    {
        UINT8_TO_BE_STREAM (p, T4T_CMD_P2_FIRST_OR_ONLY_00H);
    }

    UINT8_TO_BE_STREAM (p, T4T_FILE_ID_SIZE);
    UINT16_TO_BE_STREAM (p, file_id);

    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE + T4T_FILE_ID_SIZE;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_read_file
**
** Description      Send ReadBinary Command to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_read_file (UINT16 offset, UINT16 length, BOOLEAN is_continue)
{
    tRW_T4T_CB      *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR          *p_c_apdu;
    UINT8           *p;

    RW_TRACE_DEBUG3 ("rw_t4t_read_file () offset:%d, length:%d, is_continue:%d, ",
                      offset, length, is_continue);

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_read_file (): Cannot allocate buffer");
        return FALSE;
    }

    /* if this is the first reading */
    if (is_continue == FALSE)
    {
        /* initialise starting offset and total length */
        /* these will be updated when receiving response */
        p_t4t->rw_offset = offset;
        p_t4t->rw_length = length;
    }

    /* adjust reading length if payload is bigger than max size per single command */
    if (length > p_t4t->max_read_size)
    {
        length = (UINT8) (p_t4t->max_read_size);
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, (T4T_CMD_CLASS | rw_cb.tcb.t4t.channel));
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_READ_BINARY);
    UINT16_TO_BE_STREAM (p, offset);
    UINT8_TO_BE_STREAM (p, length); /* Le */

    p_c_apdu->len = T4T_CMD_MIN_HDR_SIZE + 1; /* adding Le */

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_update_nlen
**
** Description      Send UpdateBinary Command to update NLEN to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_update_nlen (UINT16 ndef_len)
{
    BT_HDR          *p_c_apdu;
    UINT8           *p;

    RW_TRACE_DEBUG1 ("rw_t4t_update_nlen () NLEN:%d", ndef_len);

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_update_nlen (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_UPDATE_BINARY);
    UINT16_TO_BE_STREAM (p, 0x0000);                    /* offset for NLEN */
    UINT8_TO_BE_STREAM (p, T4T_FILE_LENGTH_SIZE);
    UINT16_TO_BE_STREAM (p, ndef_len);

    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE + T4T_FILE_LENGTH_SIZE;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_update_file
**
** Description      Send UpdateBinary Command to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_update_file (void)
{
    tRW_T4T_CB      *p_t4t = &rw_cb.tcb.t4t;
    BT_HDR          *p_c_apdu;
    UINT8           *p;
    UINT16          length;

    RW_TRACE_DEBUG2 ("rw_t4t_update_file () rw_offset:%d, rw_length:%d",
                      p_t4t->rw_offset, p_t4t->rw_length);

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_write_file (): Cannot allocate buffer");
        return FALSE;
    }

    /* try to send all of remaining data */
    length = p_t4t->rw_length;

    /* adjust updating length if payload is bigger than max size per single command */
    if (length > p_t4t->max_update_size)
    {
        length = (UINT8) (p_t4t->max_update_size);
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_UPDATE_BINARY);
    UINT16_TO_BE_STREAM (p, p_t4t->rw_offset);
    UINT8_TO_BE_STREAM (p, length);

    memcpy (p, p_t4t->p_update_data, length);

    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE + length;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    /* adjust offset, length and pointer for remaining data */
    p_t4t->rw_offset     += length;
    p_t4t->rw_length     -= length;
    p_t4t->p_update_data += length;

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_update_cc_to_readonly
**
** Description      Send UpdateBinary Command for changing Write access
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_update_cc_to_readonly (void)
{
    BT_HDR          *p_c_apdu;
    UINT8           *p;

    RW_TRACE_DEBUG0 ("rw_t4t_update_cc_to_readonly (): Remove Write access from CC");

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_update_cc_to_readonly (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    /* Add Command Header */
    UINT8_TO_BE_STREAM (p, T4T_CMD_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_UPDATE_BINARY);
    UINT16_TO_BE_STREAM (p, (T4T_FC_TLV_OFFSET_IN_CC + T4T_FC_WRITE_ACCESS_OFFSET_IN_TLV)); /* Offset for Read Write access byte of CC */
    UINT8_TO_BE_STREAM (p, 1); /* Length of write access field in cc interms of bytes */

    /* Remove Write access */
    UINT8_TO_BE_STREAM (p, T4T_FC_NO_WRITE_ACCESS);


    p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE + 1;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_select_application
**
** Description      Select Application
**
**                  NDEF Tag Application Select - C-APDU
**
**                        CLA INS P1 P2 Lc Data(AID)      Le
**                  V1.0: 00  A4  04 00 07 D2760000850100 -
**                  V2.0: 00  A4  04 00 07 D2760000850101 00
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_select_application (UINT8 version)
{
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    RW_TRACE_DEBUG1 ("rw_t4t_select_application () version:0x%X", version);

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("rw_t4t_select_application (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, T4T_CMD_CLASS);
    UINT8_TO_BE_STREAM (p, T4T_CMD_INS_SELECT);
    UINT8_TO_BE_STREAM (p, T4T_CMD_P1_SELECT_BY_NAME);
    UINT8_TO_BE_STREAM (p, T4T_CMD_P2_FIRST_OR_ONLY_00H);

    if (version == T4T_VERSION_1_0)   /* this is for V1.0 */
    {
        UINT8_TO_BE_STREAM (p, T4T_V10_NDEF_TAG_AID_LEN);

        memcpy (p, t4t_v10_ndef_tag_aid, T4T_V10_NDEF_TAG_AID_LEN);

        p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE + T4T_V10_NDEF_TAG_AID_LEN;
    }
    else if (version == T4T_VERSION_2_0)   /* this is for V2.0 */
    {
        UINT8_TO_BE_STREAM (p, T4T_V20_NDEF_TAG_AID_LEN);

        memcpy (p, t4t_v20_ndef_tag_aid, T4T_V20_NDEF_TAG_AID_LEN);
        p += T4T_V20_NDEF_TAG_AID_LEN;

        UINT8_TO_BE_STREAM (p, 0x00); /* Le set to 0x00 */

        p_c_apdu->len = T4T_CMD_MAX_HDR_SIZE + T4T_V20_NDEF_TAG_AID_LEN + 1;
    }
    else
    {
        return FALSE;
    }

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_validate_cc_file
**
** Description      Validate CC file and mandatory NDEF TLV
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN rw_t4t_validate_cc_file (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;

    RW_TRACE_DEBUG0 ("rw_t4t_validate_cc_file ()");

    if (p_t4t->cc_file.cclen < T4T_CC_FILE_MIN_LEN)
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): CCLEN (%d) is too short",
                         p_t4t->cc_file.cclen);
        return FALSE;
    }

    if (T4T_GET_MAJOR_VERSION (p_t4t->cc_file.version) != T4T_GET_MAJOR_VERSION (p_t4t->version))
    {
        RW_TRACE_ERROR2 ("rw_t4t_validate_cc_file (): Peer version (0x%02X) is matched to ours (0x%02X)",
                         p_t4t->cc_file.version, p_t4t->version);
        return FALSE;
    }

    if (p_t4t->cc_file.max_le < 0x000F)
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): MaxLe (%d) is too small",
                         p_t4t->cc_file.max_le);
        return FALSE;
    }

    if (p_t4t->cc_file.max_lc < 0x0001)
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): MaxLc (%d) is too small",
                         p_t4t->cc_file.max_lc);
        return FALSE;
    }

    if (  (p_t4t->cc_file.ndef_fc.file_id == T4T_CC_FILE_ID)
        ||(p_t4t->cc_file.ndef_fc.file_id == 0xE102)
        ||(p_t4t->cc_file.ndef_fc.file_id == 0xE103)
        ||((p_t4t->cc_file.ndef_fc.file_id == 0x0000) && (p_t4t->cc_file.version == 0x20))
        ||(p_t4t->cc_file.ndef_fc.file_id == 0x3F00)
        ||(p_t4t->cc_file.ndef_fc.file_id == 0x3FFF)
        ||(p_t4t->cc_file.ndef_fc.file_id == 0xFFFF)  )
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): File ID (0x%04X) is invalid",
                          p_t4t->cc_file.ndef_fc.file_id);
        return FALSE;
    }

    if (  (p_t4t->cc_file.ndef_fc.max_file_size < 0x0005)
        ||(p_t4t->cc_file.ndef_fc.max_file_size == 0xFFFF)  )
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): max_file_size (%d) is reserved",
                         p_t4t->cc_file.ndef_fc.max_file_size);
        return FALSE;
    }

    if (p_t4t->cc_file.ndef_fc.read_access != T4T_FC_READ_ACCESS)
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): Read Access (0x%02X) is invalid",
                          p_t4t->cc_file.ndef_fc.read_access);
        return FALSE;
    }

    if (  (p_t4t->cc_file.ndef_fc.write_access != T4T_FC_WRITE_ACCESS)
        &&(p_t4t->cc_file.ndef_fc.write_access < 0x80)  )
    {
        RW_TRACE_ERROR1 ("rw_t4t_validate_cc_file (): Write Access (0x%02X) is invalid",
                          p_t4t->cc_file.ndef_fc.write_access);
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_t4t_handle_error
**
** Description      notify error to application and clean up
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_handle_error (tNFC_STATUS status, UINT8 sw1, UINT8 sw2)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    tRW_DATA    rw_data;
    tRW_EVENT   event;

    RW_TRACE_DEBUG4 ("rw_t4t_handle_error (): status:0x%02X, sw1:0x%02X, sw2:0x%02X, state:0x%X",
                      status, sw1, sw2, p_t4t->state);

    nfc_stop_quick_timer (&p_t4t->timer);

    if (rw_cb.p_cback)
    {
        rw_data.status = status;

        rw_data.t4t_sw.sw1    = sw1;
        rw_data.t4t_sw.sw2    = sw2;

        switch (p_t4t->state)
        {
        case RW_T4T_STATE_DETECT_NDEF:
            rw_data.ndef.flags  = RW_NDEF_FL_UNKNOWN;
            event = RW_T4T_NDEF_DETECT_EVT;
            break;

        case RW_T4T_STATE_READ_NDEF:
            event = RW_T4T_NDEF_READ_FAIL_EVT;
            break;

        case RW_T4T_STATE_UPDATE_NDEF:
            event = RW_T4T_NDEF_UPDATE_FAIL_EVT;
            break;

        case RW_T4T_STATE_PRESENCE_CHECK:
            event = RW_T4T_PRESENCE_CHECK_EVT;
            rw_data.status = NFC_STATUS_FAILED;
            break;

        case RW_T4T_STATE_SET_READ_ONLY:
            event = RW_T4T_SET_TO_RO_EVT;
            break;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        case RW_T4T_STATE_NDEF_FORMAT:
            event = RW_T4T_NDEF_FORMAT_CPLT_EVT;
            rw_data.status = NFC_STATUS_FAILED;
            break;
        case RW_T3BT_STATE_GET_PROP_DATA:
            event = RW_T3BT_RAW_READ_CPLT_EVT;
            rw_data.status = NFC_STATUS_FAILED;
            break;
#endif
        default:
            event = RW_T4T_MAX_EVT;
            break;
        }

        p_t4t->state = RW_T4T_STATE_IDLE;

        if (event != RW_T4T_MAX_EVT)
        {
            (*(rw_cb.p_cback)) (event, &rw_data);
        }
    }
    else
    {
        p_t4t->state = RW_T4T_STATE_IDLE;
    }
}
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         rw_t4t_sm_ndef_format
**
** Description      State machine for NDEF format procedure
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_sm_ndef_format(BT_HDR *p_r_apdu)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    UINT8       *p, type, length;
    UINT16      status_words, nlen;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t4t_sm_ndef_format (): sub_state:%s (%d)",
                      rw_t4t_get_sub_state_name (p_t4t->sub_state), p_t4t->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_t4t_sm_ndef_format (): sub_state=%d", p_t4t->sub_state);
#endif

    /* get status words */
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;

    switch (p_t4t->sub_state)
    {
    case RW_T4T_SUBSTATE_WAIT_GET_HW_VERSION:
        p += (p_r_apdu->len - 1);
        if(*(p) == T4T_ADDI_FRAME_RESP)
        {
            if(!rw_t4t_get_sw_version())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_GET_SW_VERSION;
            }
        }
        else
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_GET_SW_VERSION:
        p += (p_r_apdu->len - 1);
        if(*(p) == T4T_ADDI_FRAME_RESP)
        {
            if(!rw_t4t_update_version_details(p_r_apdu))
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            if(!rw_t4t_get_uid_details())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_GET_UID;
        }
        else
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_GET_UID:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            if(!rw_t4t_create_app())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_CREATE_APP;
            }

        }
        break;

    case RW_T4T_SUBSTATE_WAIT_CREATE_APP:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words == 0x91DE) /* DUPLICATE_ERROR, file already exist*/
        {
            status_words = 0x9100;
        }
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            if(!rw_t4t_select_app())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_APP;
            }

        }
        break;

    case RW_T4T_SUBSTATE_WAIT_SELECT_APP:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            if(!rw_t4t_create_ccfile())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_CREATE_CC;
            }

        }
        break;

    case RW_T4T_SUBSTATE_WAIT_CREATE_CC:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words == 0x91DE) /* DUPLICATE_ERROR, file already exist*/
        {
            status_words = 0x9100;
        }
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            if(!rw_t4t_create_ndef())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_CREATE_NDEF;
            }

        }
        break;

    case RW_T4T_SUBSTATE_WAIT_CREATE_NDEF:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words == 0x91DE) /* DUPLICATE_ERROR, file already exist*/
        {
            status_words = 0x9100;
        }
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            if(!rw_t4t_write_cc())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_WRITE_CC;
            }

        }
        break;

    case RW_T4T_SUBSTATE_WAIT_WRITE_CC:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            if(!rw_t4t_write_ndef())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_WRITE_NDEF;
            }
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_WRITE_NDEF:
        p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words != 0x9100)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            p_t4t->state       = RW_T4T_STATE_IDLE;
            if (rw_cb.p_cback)
            {
                rw_data.ndef.status   = NFC_STATUS_OK;
                rw_data.ndef.protocol = NFC_PROTOCOL_ISO_DEP;
                rw_data.ndef.max_size = p_t4t->card_size;
                rw_data.ndef.cur_size = 0x00;

                (*(rw_cb.p_cback)) (RW_T4T_NDEF_FORMAT_CPLT_EVT, &rw_data);

                RW_TRACE_DEBUG0 ("rw_t4t_ndef_format (): Sent RW_T4T_NDEF_FORMAT_CPLT_EVT");
            }
        }
        break;

    default:
        RW_TRACE_ERROR1 ("rw_t4t_sm_ndef_format (): unknown sub_state=%d", p_t4t->sub_state);
        rw_t4t_handle_error(NFC_STATUS_FAILED,0,0);
        break;
    }
}
#endif

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
static void rw_t3Bt_sm_get_card_id(BT_HDR *p_r_apdu)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    UINT8       *p, type, length;
    UINT16      status_words, nlen;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t3Bt_sm_get_id (): sub_state:%s (%d)",
                      rw_t4t_get_sub_state_name (p_t4t->sub_state), p_t4t->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_t3Bt_sm_get_id (): sub_state=%d", p_t4t->sub_state);
#endif

    /* get status words */
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;

    switch (p_t4t->sub_state)
    {
    case RW_T3BT_SUBSTATE_WAIT_GET_ATTRIB:
        if((p_r_apdu->len == 0x00) &&
           ((*p != 0x00) && (*p++ != 0x00)))
           {
               rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
           }
        else
        {
            if(!rw_t3bt_get_pupi())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T3BT_SUBSTATE_WAIT_GET_PUPI;
            }
        }
        break;

    case RW_T3BT_SUBSTATE_WAIT_GET_PUPI:
        p += (p_r_apdu->len - 3);
        BE_STREAM_TO_UINT16 (status_words, p);
        if(status_words != 0x9000)
        {
            rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        }
        else
        {
            UINT8 rsp_len = p_r_apdu->len - 3;
            p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset; //"p" points to start of response
            p_t4t->state       = RW_T4T_STATE_IDLE;
            nfa_rw_update_pupi_id(p, rsp_len);
            if (rw_cb.p_cback)
            {
                (*(rw_cb.p_cback)) (RW_T3BT_RAW_READ_CPLT_EVT, &rw_data);
            }
            else
            {
                RW_TRACE_ERROR0 ("rw_t3Bt_sm_get_id (): NULL callback");
            }
        }
        break;

    default:
        RW_TRACE_ERROR1 ("rw_t3Bt_sm_get_id (): unknown sub_state=%d", p_t4t->sub_state);
        rw_t4t_handle_error(NFC_STATUS_FAILED,0,0);
        break;
    }
}
#endif
/*******************************************************************************
**
** Function         rw_t4t_sm_detect_ndef
**
** Description      State machine for NDEF detection procedure
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_sm_detect_ndef (BT_HDR *p_r_apdu)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    UINT8       *p, type, length;
    UINT16      status_words, nlen;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t4t_sm_detect_ndef (): sub_state:%s (%d)",
                      rw_t4t_get_sub_state_name (p_t4t->sub_state), p_t4t->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_t4t_sm_detect_ndef (): sub_state=%d", p_t4t->sub_state);
#endif

    /* get status words */
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;
    p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
    BE_STREAM_TO_UINT16 (status_words, p);

    if (status_words != T4T_RSP_CMD_CMPLTED)
    {
        /* try V1.0 after failing of V2.0 */
        if (  (p_t4t->sub_state == RW_T4T_SUBSTATE_WAIT_SELECT_APP)
            &&(p_t4t->version   == T4T_VERSION_2_0)  )
        {
            p_t4t->version = T4T_VERSION_1_0;

            RW_TRACE_DEBUG1 ("rw_t4t_sm_detect_ndef (): retry with version=0x%02X",
                              p_t4t->version);

            if (!rw_t4t_select_application (T4T_VERSION_1_0))
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            return;
        }

        p_t4t->ndef_status &= ~ (RW_T4T_NDEF_STATUS_NDEF_DETECTED);
        rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        return;
    }

    switch (p_t4t->sub_state)
    {
    case RW_T4T_SUBSTATE_WAIT_SELECT_APP:

        /* NDEF Tag application has been selected then select CC file */
        if (!rw_t4t_select_file (T4T_CC_FILE_ID))
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        else
        {
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_CC;
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_SELECT_CC:

        /* CC file has been selected then read mandatory part of CC file */
        if (!rw_t4t_read_file (0x00, T4T_CC_FILE_MIN_LEN, FALSE))
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        else
        {
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_CC_FILE;
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_CC_FILE:

        /* CC file has been read then validate and select mandatory NDEF file */
        if (p_r_apdu->len >= T4T_CC_FILE_MIN_LEN + T4T_RSP_STATUS_WORDS_SIZE)
        {
            p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;

            BE_STREAM_TO_UINT16 (p_t4t->cc_file.cclen, p);
            BE_STREAM_TO_UINT8 (p_t4t->cc_file.version, p);
            BE_STREAM_TO_UINT16 (p_t4t->cc_file.max_le, p);
            BE_STREAM_TO_UINT16 (p_t4t->cc_file.max_lc, p);

            BE_STREAM_TO_UINT8 (type, p);
            BE_STREAM_TO_UINT8 (length, p);

            if (  (type == T4T_NDEF_FILE_CONTROL_TYPE)
                &&(length == T4T_FILE_CONTROL_LENGTH)  )
            {
                BE_STREAM_TO_UINT16 (p_t4t->cc_file.ndef_fc.file_id, p);
                BE_STREAM_TO_UINT16 (p_t4t->cc_file.ndef_fc.max_file_size, p);
                BE_STREAM_TO_UINT8 (p_t4t->cc_file.ndef_fc.read_access, p);
                BE_STREAM_TO_UINT8 (p_t4t->cc_file.ndef_fc.write_access, p);

#if (BT_TRACE_VERBOSE == TRUE)
                RW_TRACE_DEBUG0 ("Capability Container (CC) file");
                RW_TRACE_DEBUG1 ("  CCLEN:  0x%04X",    p_t4t->cc_file.cclen);
                RW_TRACE_DEBUG1 ("  Version:0x%02X",    p_t4t->cc_file.version);
                RW_TRACE_DEBUG1 ("  MaxLe:  0x%04X",    p_t4t->cc_file.max_le);
                RW_TRACE_DEBUG1 ("  MaxLc:  0x%04X",    p_t4t->cc_file.max_lc);
                RW_TRACE_DEBUG0 ("  NDEF File Control TLV");
                RW_TRACE_DEBUG1 ("    FileID:      0x%04X", p_t4t->cc_file.ndef_fc.file_id);
                RW_TRACE_DEBUG1 ("    MaxFileSize: 0x%04X", p_t4t->cc_file.ndef_fc.max_file_size);
                RW_TRACE_DEBUG1 ("    ReadAccess:  0x%02X", p_t4t->cc_file.ndef_fc.read_access);
                RW_TRACE_DEBUG1 ("    WriteAccess: 0x%02X", p_t4t->cc_file.ndef_fc.write_access);
#endif

                if (rw_t4t_validate_cc_file ())
                {
                    if (!rw_t4t_select_file (p_t4t->cc_file.ndef_fc.file_id))
                    {
                        rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
                    }
                    else
                    {
                        p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_NDEF_FILE;
                    }
                    break;
                }
            }
        }

        /* invalid response or CC file */
        p_t4t->ndef_status &= ~ (RW_T4T_NDEF_STATUS_NDEF_DETECTED);
        rw_t4t_handle_error (NFC_STATUS_BAD_RESP, 0, 0);
        break;

    case RW_T4T_SUBSTATE_WAIT_SELECT_NDEF_FILE:

        /* NDEF file has been selected then read the first 2 bytes (NLEN) */
        if (!rw_t4t_read_file (0, T4T_FILE_LENGTH_SIZE, FALSE))
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        else
        {
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_READ_NLEN;
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_READ_NLEN:

        /* NLEN has been read then report upper layer */
        if (p_r_apdu->len == T4T_FILE_LENGTH_SIZE + T4T_RSP_STATUS_WORDS_SIZE)
        {
            /* get length of NDEF */
            p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;
            BE_STREAM_TO_UINT16 (nlen, p);

            if (nlen <= p_t4t->cc_file.ndef_fc.max_file_size - T4T_FILE_LENGTH_SIZE)
            {
                p_t4t->ndef_status = RW_T4T_NDEF_STATUS_NDEF_DETECTED;

                if (p_t4t->cc_file.ndef_fc.write_access != T4T_FC_WRITE_ACCESS)
                {
                    p_t4t->ndef_status |= RW_T4T_NDEF_STATUS_NDEF_READ_ONLY;
                }

                /* Get max bytes to read per command */
                if (p_t4t->cc_file.max_le >= RW_T4T_MAX_DATA_PER_READ)
                {
                    p_t4t->max_read_size = RW_T4T_MAX_DATA_PER_READ;
                }
                else
                {
                    p_t4t->max_read_size = p_t4t->cc_file.max_le;
                }

                /* Le: valid range is 0x01 to 0xFF */
                if (p_t4t->max_read_size >= T4T_MAX_LENGTH_LE)
                {
                    p_t4t->max_read_size = T4T_MAX_LENGTH_LE;
                }

                /* Get max bytes to update per command */
                if (p_t4t->cc_file.max_lc >= RW_T4T_MAX_DATA_PER_WRITE)
                {
                    p_t4t->max_update_size = RW_T4T_MAX_DATA_PER_WRITE;
                }
                else
                {
                    p_t4t->max_update_size = p_t4t->cc_file.max_lc;
                }

                /* Lc: valid range is 0x01 to 0xFF */
                if (p_t4t->max_update_size >= T4T_MAX_LENGTH_LC)
                {
                    p_t4t->max_update_size = T4T_MAX_LENGTH_LC;
                }

                p_t4t->ndef_length = nlen;
                p_t4t->state       = RW_T4T_STATE_IDLE;

                if (rw_cb.p_cback)
                {
                    rw_data.ndef.status   = NFC_STATUS_OK;
                    rw_data.ndef.protocol = NFC_PROTOCOL_ISO_DEP;
                    rw_data.ndef.max_size = (UINT32) (p_t4t->cc_file.ndef_fc.max_file_size - (UINT16) T4T_FILE_LENGTH_SIZE);
                    rw_data.ndef.cur_size = nlen;
                    rw_data.ndef.flags    = RW_NDEF_FL_SUPPORTED | RW_NDEF_FL_FORMATED;
                    if (p_t4t->cc_file.ndef_fc.write_access != T4T_FC_WRITE_ACCESS)
                    {
                        rw_data.ndef.flags    |= RW_NDEF_FL_READ_ONLY;
                    }

                    (*(rw_cb.p_cback)) (RW_T4T_NDEF_DETECT_EVT, &rw_data);

                    RW_TRACE_DEBUG0 ("rw_t4t_sm_detect_ndef (): Sent RW_T4T_NDEF_DETECT_EVT");
                }
            }
            else
            {
                /* NLEN should be less than max file size */
                RW_TRACE_ERROR2 ("rw_t4t_sm_detect_ndef (): NLEN (%d) + 2 must be <= max file size (%d)",
                                 nlen, p_t4t->cc_file.ndef_fc.max_file_size);

                p_t4t->ndef_status &= ~ (RW_T4T_NDEF_STATUS_NDEF_DETECTED);
                rw_t4t_handle_error (NFC_STATUS_BAD_RESP, 0, 0);
            }
        }
        else
        {
            /* response payload size should be T4T_FILE_LENGTH_SIZE */
            RW_TRACE_ERROR2 ("rw_t4t_sm_detect_ndef (): Length (%d) of R-APDU must be %d",
                             p_r_apdu->len, T4T_FILE_LENGTH_SIZE + T4T_RSP_STATUS_WORDS_SIZE);

            p_t4t->ndef_status &= ~ (RW_T4T_NDEF_STATUS_NDEF_DETECTED);
            rw_t4t_handle_error (NFC_STATUS_BAD_RESP, 0, 0);
        }
        break;

    default:
        RW_TRACE_ERROR1 ("rw_t4t_sm_detect_ndef (): unknown sub_state=%d", p_t4t->sub_state);
        rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        break;
    }
}

/*******************************************************************************
**
** Function         rw_t4t_sm_read_ndef
**
** Description      State machine for NDEF read procedure
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_sm_read_ndef (BT_HDR *p_r_apdu)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    UINT8       *p;
    UINT16      status_words;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t4t_sm_read_ndef (): sub_state:%s (%d)",
                      rw_t4t_get_sub_state_name (p_t4t->sub_state), p_t4t->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_t4t_sm_read_ndef (): sub_state=%d", p_t4t->sub_state);
#endif

    /* get status words */
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;
    p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
    BE_STREAM_TO_UINT16 (status_words, p);

    if (status_words != T4T_RSP_CMD_CMPLTED)
    {
        rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        GKI_freebuf (p_r_apdu);
        return;
    }

    switch (p_t4t->sub_state)
    {
    case RW_T4T_SUBSTATE_WAIT_READ_RESP:

        /* Read partial or complete data */
        p_r_apdu->len -= T4T_RSP_STATUS_WORDS_SIZE;

        if ((p_r_apdu->len > 0) && (p_r_apdu->len <= p_t4t->rw_length))
        {
            p_t4t->rw_length -= p_r_apdu->len;
            p_t4t->rw_offset += p_r_apdu->len;

            if (rw_cb.p_cback)
            {
                rw_data.data.status = NFC_STATUS_OK;
                rw_data.data.p_data = p_r_apdu;

                /* if need to read more data */
                if (p_t4t->rw_length > 0)
                {
                    (*(rw_cb.p_cback)) (RW_T4T_NDEF_READ_EVT, &rw_data);

                    if (!rw_t4t_read_file (p_t4t->rw_offset, p_t4t->rw_length, TRUE))
                    {
                        rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
                    }
                }
                else
                {
                    p_t4t->state = RW_T4T_STATE_IDLE;

                    (*(rw_cb.p_cback)) (RW_T4T_NDEF_READ_CPLT_EVT, &rw_data);

                    RW_TRACE_DEBUG0 ("rw_t4t_sm_read_ndef (): Sent RW_T4T_NDEF_READ_CPLT_EVT");

                }

                p_r_apdu = NULL;
            }
            else
            {
                p_t4t->rw_length = 0;
                p_t4t->state = RW_T4T_STATE_IDLE;
            }
        }
        else
        {
            RW_TRACE_ERROR2 ("rw_t4t_sm_read_ndef (): invalid payload length (%d), rw_length (%d)",
                             p_r_apdu->len, p_t4t->rw_length);
            rw_t4t_handle_error (NFC_STATUS_BAD_RESP, 0, 0);
        }
        break;

    default:
        RW_TRACE_ERROR1 ("rw_t4t_sm_read_ndef (): unknown sub_state = %d", p_t4t->sub_state);
        rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        break;
    }

    if (p_r_apdu)
        GKI_freebuf (p_r_apdu);
}

/*******************************************************************************
**
** Function         rw_t4t_sm_update_ndef
**
** Description      State machine for NDEF update procedure
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_sm_update_ndef (BT_HDR  *p_r_apdu)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    UINT8       *p;
    UINT16      status_words;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t4t_sm_update_ndef (): sub_state:%s (%d)",
                      rw_t4t_get_sub_state_name (p_t4t->sub_state), p_t4t->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_t4t_sm_update_ndef (): sub_state=%d", p_t4t->sub_state);
#endif

    /* Get status words */
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;
    p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
    BE_STREAM_TO_UINT16 (status_words, p);

    if (status_words != T4T_RSP_CMD_CMPLTED)
    {
        rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        return;
    }

    switch (p_t4t->sub_state)
    {
    case RW_T4T_SUBSTATE_WAIT_UPDATE_NLEN:

        /* NLEN has been updated */
        /* if need to update data */
        if (p_t4t->p_update_data)
        {
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_UPDATE_RESP;

            if (!rw_t4t_update_file ())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
                p_t4t->p_update_data = NULL;
            }
        }
        else
        {
            p_t4t->state = RW_T4T_STATE_IDLE;

            /* just finished last step of updating (updating NLEN) */
            if (rw_cb.p_cback)
            {
                rw_data.status = NFC_STATUS_OK;

                (*(rw_cb.p_cback)) (RW_T4T_NDEF_UPDATE_CPLT_EVT, &rw_data);
                RW_TRACE_DEBUG0 ("rw_t4t_sm_update_ndef (): Sent RW_T4T_NDEF_UPDATE_CPLT_EVT");
            }
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_UPDATE_RESP:

        /* if updating is not completed */
        if (p_t4t->rw_length > 0)
        {
            if (!rw_t4t_update_file ())
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
                p_t4t->p_update_data = NULL;
            }
        }
        else
        {
            p_t4t->p_update_data = NULL;

            /* update NLEN as last step of updating file */
            if (!rw_t4t_update_nlen (p_t4t->ndef_length))
            {
                rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
            }
            else
            {
                p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_UPDATE_NLEN;
            }
        }
        break;

    default:
        RW_TRACE_ERROR1 ("rw_t4t_sm_update_ndef (): unknown sub_state = %d", p_t4t->sub_state);
        rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        break;
    }
}

/*******************************************************************************
**
** Function         rw_t4t_sm_set_readonly
**
** Description      State machine for CC update procedure
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_sm_set_readonly (BT_HDR  *p_r_apdu)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;
    UINT8       *p;
    UINT16      status_words;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t4t_sm_set_readonly (): sub_state:%s (%d)",
                      rw_t4t_get_sub_state_name (p_t4t->sub_state), p_t4t->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_t4t_sm_set_readonly (): sub_state=%d", p_t4t->sub_state);
#endif

    /* Get status words */
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;
    p += (p_r_apdu->len - T4T_RSP_STATUS_WORDS_SIZE);
    BE_STREAM_TO_UINT16 (status_words, p);

    if (status_words != T4T_RSP_CMD_CMPLTED)
    {
        rw_t4t_handle_error (NFC_STATUS_CMD_NOT_CMPLTD, *(p-2), *(p-1));
        return;
    }

    switch (p_t4t->sub_state)
    {
    case RW_T4T_SUBSTATE_WAIT_SELECT_CC:

        /* CC file has been selected then update write access to read-only in CC file */
        if (!rw_t4t_update_cc_to_readonly ())
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        else
        {
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_UPDATE_CC;
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_UPDATE_CC:
        /* CC Updated, Select NDEF File to allow NDEF operation */
        p_t4t->cc_file.ndef_fc.write_access = T4T_FC_NO_WRITE_ACCESS;
        p_t4t->ndef_status |= RW_T4T_NDEF_STATUS_NDEF_READ_ONLY;

        if (!rw_t4t_select_file (p_t4t->cc_file.ndef_fc.file_id))
        {
            rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        }
        else
        {
            p_t4t->sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_NDEF_FILE;
        }
        break;

    case RW_T4T_SUBSTATE_WAIT_SELECT_NDEF_FILE:
        p_t4t->state = RW_T4T_STATE_IDLE;
        /* just finished last step of configuring tag read only (Selecting NDEF file CC) */
        if (rw_cb.p_cback)
        {
            rw_data.status = NFC_STATUS_OK;

            RW_TRACE_DEBUG0 ("rw_t4t_sm_set_readonly (): Sent RW_T4T_SET_TO_RO_EVT");
            (*(rw_cb.p_cback)) (RW_T4T_SET_TO_RO_EVT, &rw_data);
        }
        break;

    default:
        RW_TRACE_ERROR1 ("rw_t4t_sm_set_readonly (): unknown sub_state = %d", p_t4t->sub_state);
        rw_t4t_handle_error (NFC_STATUS_FAILED, 0, 0);
        break;
    }
}

/*******************************************************************************
**
** Function         rw_t4t_process_timeout
**
** Description      process timeout event
**
** Returns          none
**
*******************************************************************************/
void rw_t4t_process_timeout (TIMER_LIST_ENT *p_tle)
{
    RW_TRACE_DEBUG1 ("rw_t4t_process_timeout () event=%d", p_tle->event);

    if (p_tle->event == NFC_TTYPE_RW_T4T_RESPONSE)
    {
        rw_t4t_handle_error (NFC_STATUS_TIMEOUT, 0, 0);
    }
    else
    {
        RW_TRACE_ERROR1 ("rw_t4t_process_timeout () unknown event=%d", p_tle->event);
    }
}

/*******************************************************************************
**
** Function         rw_t4t_data_cback
**
** Description      This callback function receives the data from NFCC.
**
** Returns          none
**
*******************************************************************************/
static void rw_t4t_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    tRW_T4T_CB *p_t4t    = &rw_cb.tcb.t4t;
    BT_HDR     *p_r_apdu;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    UINT8  begin_state   = p_t4t->state;
#endif

    RW_TRACE_DEBUG1 ("rw_t4t_data_cback () event = 0x%X", event);
    nfc_stop_quick_timer (&p_t4t->timer);

    switch (event)
    {
    case NFC_DEACTIVATE_CEVT:
        NFC_SetStaticRfCback (NULL);
        p_t4t->state = RW_T4T_STATE_NOT_ACTIVATED;
        return;

    case NFC_ERROR_CEVT:
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if (p_t4t->state == RW_T4T_STATE_PRESENCE_CHECK)
        {
            p_t4t->state   = RW_T4T_STATE_IDLE;
            rw_data.status = NFC_STATUS_FAILED;
            (*(rw_cb.p_cback)) (RW_T4T_PRESENCE_CHECK_EVT, &rw_data);
        }
        else if (p_t4t->state == RW_T4T_STATE_NDEF_FORMAT)
        {
            p_t4t->state   = RW_T4T_STATE_IDLE;
            rw_data.status = NFC_STATUS_FAILED;
            (*(rw_cb.p_cback)) (RW_T4T_NDEF_FORMAT_CPLT_EVT, &rw_data);
        }
        else if (p_t4t->state != RW_T4T_STATE_IDLE)
        {
            rw_t4t_handle_error (rw_data.status, 0, 0);
        }
        else
        {
            p_t4t->state   = RW_T4T_STATE_IDLE;
            rw_data.status = (tNFC_STATUS) (*(UINT8*) p_data);
            (*(rw_cb.p_cback)) (RW_T4T_INTF_ERROR_EVT, &rw_data);
        }
#else
        rw_data.status = (tNFC_STATUS) (*(UINT8*) p_data);

        if (p_t4t->state != RW_T4T_STATE_IDLE)
        {
            rw_t4t_handle_error (rw_data.status, 0, 0);
        }
        else
        {
            (*(rw_cb.p_cback)) (RW_T4T_INTF_ERROR_EVT, &rw_data);
        }
#endif
        return;

    case NFC_DATA_CEVT:
        p_r_apdu = (BT_HDR *) p_data->data.p_data;
        break;

#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    case NFC_RF_WTX_CEVT:
        if(p_t4t->state == RW_T4T_STATE_IDLE)
        {
            /* WTX received for raw frame sent
             * forward to upper layer without parsing */
            if (rw_cb.p_cback)
            {
                (*(rw_cb.p_cback)) (RW_T4T_RAW_FRAME_RF_WTX_EVT, &rw_data);
            }
        }
        else
        {
            nfc_start_quick_timer (&p_t4t->timer, NFC_TTYPE_RW_T4T_RESPONSE,
                                  (RW_T4T_TOUT_RESP * QUICK_TIMER_TICKS_PER_SEC) / 1000);
        }
        return;
#endif

    default:
        return;
    }

#if (BT_TRACE_PROTOCOL == TRUE)
    if (p_t4t->state != RW_T4T_STATE_IDLE)
        DispRWT4Tags (p_r_apdu, TRUE);
#endif

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("RW T4T state: <%s (%d)>",
                        rw_t4t_get_state_name (p_t4t->state), p_t4t->state);
#else
    RW_TRACE_DEBUG1 ("RW T4T state: %d", p_t4t->state);
#endif

    switch (p_t4t->state)
    {
    case RW_T4T_STATE_IDLE:
        /* Unexpected R-APDU, it should be raw frame response */
        /* forward to upper layer without parsing */
#if (BT_TRACE_VERBOSE == TRUE)
        RW_TRACE_DEBUG2 ("RW T4T Raw Frame: Len [0x%X] Status [%s]", p_r_apdu->len, NFC_GetStatusName (p_data->data.status));
#else
        RW_TRACE_DEBUG2 ("RW T4T Raw Frame: Len [0x%X] Status [0x%X]", p_r_apdu->len, p_data->data.status);
#endif
        if (rw_cb.p_cback)
        {
            rw_data.raw_frame.status = p_data->data.status;
            rw_data.raw_frame.p_data = p_r_apdu;
            (*(rw_cb.p_cback)) (RW_T4T_RAW_FRAME_EVT, &rw_data);
            p_r_apdu = NULL;
        }
        else
        {
            GKI_freebuf (p_r_apdu);
        }
        break;
    case RW_T4T_STATE_DETECT_NDEF:
        rw_t4t_sm_detect_ndef (p_r_apdu);
        GKI_freebuf (p_r_apdu);
        break;
    case RW_T4T_STATE_READ_NDEF:
        rw_t4t_sm_read_ndef (p_r_apdu);
        /* p_r_apdu may send upper lyaer */
        break;
    case RW_T4T_STATE_UPDATE_NDEF:
        rw_t4t_sm_update_ndef (p_r_apdu);
        GKI_freebuf (p_r_apdu);
        break;
    case RW_T4T_STATE_PRESENCE_CHECK:
        /* if any response, send presence check with ok */
        rw_data.status = NFC_STATUS_OK;
        p_t4t->state = RW_T4T_STATE_IDLE;
        (*(rw_cb.p_cback)) (RW_T4T_PRESENCE_CHECK_EVT, &rw_data);
        GKI_freebuf (p_r_apdu);
        break;
    case RW_T4T_STATE_SET_READ_ONLY:
        rw_t4t_sm_set_readonly (p_r_apdu);
        GKI_freebuf (p_r_apdu);
        break;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    case RW_T4T_STATE_NDEF_FORMAT:
        rw_t4t_sm_ndef_format(p_r_apdu);
        GKI_freebuf (p_r_apdu);
        break;
    case RW_T3BT_STATE_GET_PROP_DATA:
        rw_t3Bt_sm_get_card_id(p_r_apdu);
        GKI_freebuf (p_r_apdu);
        break;
#endif
    default:
        RW_TRACE_ERROR1 ("rw_t4t_data_cback (): invalid state=%d", p_t4t->state);
        GKI_freebuf (p_r_apdu);
        break;
    }

#if (BT_TRACE_VERBOSE == TRUE)
    if (begin_state != p_t4t->state)
    {
        RW_TRACE_DEBUG2 ("RW T4T state changed:<%s> -> <%s>",
                          rw_t4t_get_state_name (begin_state),
                          rw_t4t_get_state_name (p_t4t->state));
    }
#endif
}

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         RW_T4tFormatNDef
**
** Description      format T4T tag
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS RW_T4tFormatNDef(void)
{
    RW_TRACE_API0 ("RW_T4tFormatNDef ()");

    if (rw_cb.tcb.t4t.state != RW_T4T_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_T4tFormatNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.t4t.state);
        return NFC_STATUS_FAILED;
    }

        rw_cb.tcb.t4t.card_type = 0x00;
    if(!rw_t4t_get_hw_version())
    {
        return NFC_STATUS_FAILED;
    }
    rw_cb.tcb.t4t.state = RW_T4T_STATE_NDEF_FORMAT;
    rw_cb.tcb.t4t.sub_state = RW_T4T_SUBSTATE_WAIT_GET_HW_VERSION;

    return NFC_STATUS_OK;
}
#endif
/*******************************************************************************
**
** Function         rw_t4t_select
**
** Description      Initialise T4T
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS rw_t4t_select (void)
{
    tRW_T4T_CB  *p_t4t = &rw_cb.tcb.t4t;

    RW_TRACE_DEBUG0 ("rw_t4t_select ()");

    NFC_SetStaticRfCback (rw_t4t_data_cback);

    p_t4t->state   = RW_T4T_STATE_IDLE;
    p_t4t->version = T4T_MY_VERSION;

    /* set it min of max R-APDU data size before reading CC file */
    p_t4t->cc_file.max_le = T4T_MIN_MLE;

    /* These will be udated during NDEF detection */
    p_t4t->max_read_size   = T4T_MAX_LENGTH_LE;
    p_t4t->max_update_size = T4T_MAX_LENGTH_LC;

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         RW_T4tDetectNDef
**
** Description      This function performs NDEF detection procedure
**
**                  RW_T4T_NDEF_DETECT_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_T4tDetectNDef (void)
{
    RW_TRACE_API0 ("RW_T4tDetectNDef ()");

    if (rw_cb.tcb.t4t.state != RW_T4T_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_T4tDetectNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.t4t.state);
        return NFC_STATUS_FAILED;
    }

    if (rw_cb.tcb.t4t.ndef_status & RW_T4T_NDEF_STATUS_NDEF_DETECTED)
    {
        /* NDEF Tag application has been selected then select CC file */
        if (!rw_t4t_select_file (T4T_CC_FILE_ID))
        {
            return NFC_STATUS_FAILED;
        }
        rw_cb.tcb.t4t.sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_CC;
    }
    else
    {
        /* Select NDEF Tag Application */
        if (!rw_t4t_select_application (rw_cb.tcb.t4t.version))
        {
            return NFC_STATUS_FAILED;
        }
        rw_cb.tcb.t4t.sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_APP;
    }

    rw_cb.tcb.t4t.state     = RW_T4T_STATE_DETECT_NDEF;

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         RW_T4tReadNDef
**
** Description      This function performs NDEF read procedure
**                  Note: RW_T4tDetectNDef () must be called before using this
**
**                  The following event will be returned
**                      RW_T4T_NDEF_READ_EVT for each segmented NDEF message
**                      RW_T4T_NDEF_READ_CPLT_EVT for the last segment or complete NDEF
**                      RW_T4T_NDEF_READ_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_T4tReadNDef (void)
{
    RW_TRACE_API0 ("RW_T4tReadNDef ()");

    if (rw_cb.tcb.t4t.state != RW_T4T_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_T4tReadNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.t4t.state);
        return NFC_STATUS_FAILED;
    }

    /* if NDEF has been detected */
    if (rw_cb.tcb.t4t.ndef_status & RW_T4T_NDEF_STATUS_NDEF_DETECTED)
    {
        /* start reading NDEF */
        if (!rw_t4t_read_file (T4T_FILE_LENGTH_SIZE, rw_cb.tcb.t4t.ndef_length, FALSE))
        {
            return NFC_STATUS_FAILED;
        }

        rw_cb.tcb.t4t.state     = RW_T4T_STATE_READ_NDEF;
        rw_cb.tcb.t4t.sub_state = RW_T4T_SUBSTATE_WAIT_READ_RESP;

        return NFC_STATUS_OK;
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_T4tReadNDef ():No NDEF detected");
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         RW_T4tUpdateNDef
**
** Description      This function performs NDEF update procedure
**                  Note: RW_T4tDetectNDef () must be called before using this
**                        Updating data must not be removed until returning event
**
**                  The following event will be returned
**                      RW_T4T_NDEF_UPDATE_CPLT_EVT for complete
**                      RW_T4T_NDEF_UPDATE_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_T4tUpdateNDef (UINT16 length, UINT8 *p_data)
{
    RW_TRACE_API1 ("RW_T4tUpdateNDef () length:%d", length);

    if (rw_cb.tcb.t4t.state != RW_T4T_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_T4tUpdateNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.t4t.state);
        return NFC_STATUS_FAILED;
    }

    /* if NDEF has been detected */
    if (rw_cb.tcb.t4t.ndef_status & RW_T4T_NDEF_STATUS_NDEF_DETECTED)
    {
        /* if read-only */
        if (rw_cb.tcb.t4t.ndef_status & RW_T4T_NDEF_STATUS_NDEF_READ_ONLY)
        {
            RW_TRACE_ERROR0 ("RW_T4tUpdateNDef ():NDEF is read-only");
            return NFC_STATUS_FAILED;
        }

        if (rw_cb.tcb.t4t.cc_file.ndef_fc.max_file_size < length + T4T_FILE_LENGTH_SIZE)
        {
            RW_TRACE_ERROR2 ("RW_T4tUpdateNDef ():data (%d bytes) plus NLEN is more than max file size (%d)",
                              length, rw_cb.tcb.t4t.cc_file.ndef_fc.max_file_size);
            return NFC_STATUS_FAILED;
        }

        /* store NDEF length and data */
        rw_cb.tcb.t4t.ndef_length   = length;
        rw_cb.tcb.t4t.p_update_data = p_data;

        rw_cb.tcb.t4t.rw_offset     = T4T_FILE_LENGTH_SIZE;
        rw_cb.tcb.t4t.rw_length     = length;

        /* set NLEN to 0x0000 for the first step */
        if (!rw_t4t_update_nlen (0x0000))
        {
            return NFC_STATUS_FAILED;
        }

        rw_cb.tcb.t4t.state     = RW_T4T_STATE_UPDATE_NDEF;
        rw_cb.tcb.t4t.sub_state = RW_T4T_SUBSTATE_WAIT_UPDATE_NLEN;

        return NFC_STATUS_OK;
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_T4tUpdateNDef ():No NDEF detected");
        return NFC_STATUS_FAILED;
    }
}

/*****************************************************************************
**
** Function         RW_T4tPresenceCheck
**
** Description
**      Check if the tag is still in the field.
**
**      The RW_T4T_PRESENCE_CHECK_EVT w/ status is used to indicate presence
**      or non-presence.
**      option is RW_T4T_CHK_EMPTY_I_BLOCK, use empty I block for presence check.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_T4tPresenceCheck (UINT8 option)
{
    tNFC_STATUS retval = NFC_STATUS_OK;
    tRW_DATA    evt_data;
    BOOLEAN     status;
    BT_HDR      *p_data;

    RW_TRACE_API1 ("RW_T4tPresenceCheck () %d", option);

    /* If RW_SelectTagType was not called (no conn_callback) return failure */
    if (!rw_cb.p_cback)
    {
        retval = NFC_STATUS_FAILED;
    }
    /* If we are not activated, then RW_T4T_PRESENCE_CHECK_EVT with NFC_STATUS_FAILED */
    else if (rw_cb.tcb.t4t.state == RW_T4T_STATE_NOT_ACTIVATED)
    {
        evt_data.status = NFC_STATUS_FAILED;
        (*rw_cb.p_cback) (RW_T4T_PRESENCE_CHECK_EVT, &evt_data);
    }
    /* If command is pending, assume tag is still present */
    else if (rw_cb.tcb.t4t.state != RW_T4T_STATE_IDLE)
    {
        evt_data.status = NFC_STATUS_OK;
        (*rw_cb.p_cback) (RW_T4T_PRESENCE_CHECK_EVT, &evt_data);
    }
    else
    {
        status = FALSE;
        if (option == RW_T4T_CHK_EMPTY_I_BLOCK)
        {
            /* use empty I block for presence check */
            if ((p_data = (BT_HDR *) GKI_getbuf (NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE)) != NULL)
            {
                p_data->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
                p_data->len    = 0;
                if (NFC_SendData (NFC_RF_CONN_ID, (BT_HDR*) p_data) == NFC_STATUS_OK)
                    status = TRUE;
            }
        }
        else
        {
            /* use read binary on the given channel */
            rw_cb.tcb.t4t.channel = 0;
            if (option <= RW_T4T_CHK_READ_BINARY_CH3)
                rw_cb.tcb.t4t.channel = option;
            status = rw_t4t_read_file (0, 1, FALSE);
            rw_cb.tcb.t4t.channel = 0;
        }

        if (status == TRUE)
        {
            rw_cb.tcb.t4t.state = RW_T4T_STATE_PRESENCE_CHECK;
        }
        else
        {
            retval = NFC_STATUS_NO_BUFFERS;
        }
    }

    return (retval);
}

/*****************************************************************************
**
** Function         RW_T4tSetNDefReadOnly
**
** Description      This function performs NDEF read-only procedure
**                  Note: RW_T4tDetectNDef() must be called before using this
**
**                  The RW_T4T_SET_TO_RO_EVT event will be returned.
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*****************************************************************************/
tNFC_STATUS RW_T4tSetNDefReadOnly (void)
{
    tNFC_STATUS retval = NFC_STATUS_OK;
    tRW_DATA    evt_data;

    RW_TRACE_API0 ("RW_T4tSetNDefReadOnly ()");

    if (rw_cb.tcb.t4t.state != RW_T4T_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_T4tSetNDefReadOnly ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.t4t.state);
        return NFC_STATUS_FAILED;
    }

    /* if NDEF has been detected */
    if (rw_cb.tcb.t4t.ndef_status & RW_T4T_NDEF_STATUS_NDEF_DETECTED)
    {
        /* if read-only */
        if (rw_cb.tcb.t4t.ndef_status & RW_T4T_NDEF_STATUS_NDEF_READ_ONLY)
        {
            RW_TRACE_API0 ("RW_T4tSetNDefReadOnly (): NDEF is already read-only");

            evt_data.status = NFC_STATUS_OK;
            (*rw_cb.p_cback) (RW_T4T_SET_TO_RO_EVT, &evt_data);
            return (retval);
        }

        /* NDEF Tag application has been selected then select CC file */
        if (!rw_t4t_select_file (T4T_CC_FILE_ID))
        {
            return NFC_STATUS_FAILED;
        }

        rw_cb.tcb.t4t.state     = RW_T4T_STATE_SET_READ_ONLY;
        rw_cb.tcb.t4t.sub_state = RW_T4T_SUBSTATE_WAIT_SELECT_CC;

        return NFC_STATUS_OK;
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_T4tSetNDefReadOnly ():No NDEF detected");
        return NFC_STATUS_FAILED;
    }
    return (retval);
}

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
tNFC_STATUS RW_T3BtGetPupiID(void)
{
    BT_HDR      *p_c_apdu;
    UINT8       *p;

    p_c_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_c_apdu)
    {
        RW_TRACE_ERROR0 ("RW_T3BtGetPupiID (): Cannot allocate buffer");
        return FALSE;
    }

    p_c_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    UINT8_TO_BE_STREAM (p, 0x1D);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT16_TO_BE_STREAM (p, 0x0000);
    UINT16_TO_BE_STREAM(p,0x0008);
    UINT16_TO_BE_STREAM (p, 0x0100);

    p_c_apdu->len = 0x09;

    if (!rw_t4t_send_to_lower (p_c_apdu))
    {
        return FALSE;
    }

    rw_cb.tcb.t4t.state = RW_T3BT_STATE_GET_PROP_DATA;
    rw_cb.tcb.t4t.sub_state = RW_T3BT_SUBSTATE_WAIT_GET_ATTRIB;
    return TRUE;
}
#endif

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         rw_t4t_get_state_name
**
** Description      This function returns the state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_t4t_get_state_name (UINT8 state)
{
    switch (state)
    {
    case RW_T4T_STATE_NOT_ACTIVATED:
        return ("NOT_ACTIVATED");
    case RW_T4T_STATE_IDLE:
        return ("IDLE");
    case RW_T4T_STATE_DETECT_NDEF:
        return ("NDEF_DETECTION");
    case RW_T4T_STATE_READ_NDEF:
        return ("READ_NDEF");
    case RW_T4T_STATE_UPDATE_NDEF:
        return ("UPDATE_NDEF");
    case RW_T4T_STATE_PRESENCE_CHECK:
        return ("PRESENCE_CHECK");
    case RW_T4T_STATE_SET_READ_ONLY:
        return ("SET_READ_ONLY");

    default:
        return ("???? UNKNOWN STATE");
    }
}

/*******************************************************************************
**
** Function         rw_t4t_get_sub_state_name
**
** Description      This function returns the sub_state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_t4t_get_sub_state_name (UINT8 sub_state)
{
    switch (sub_state)
    {
    case RW_T4T_SUBSTATE_WAIT_SELECT_APP:
        return ("WAIT_SELECT_APP");
    case RW_T4T_SUBSTATE_WAIT_SELECT_CC:
        return ("WAIT_SELECT_CC");
    case RW_T4T_SUBSTATE_WAIT_CC_FILE:
        return ("WAIT_CC_FILE");
    case RW_T4T_SUBSTATE_WAIT_SELECT_NDEF_FILE:
        return ("WAIT_SELECT_NDEF_FILE");
    case RW_T4T_SUBSTATE_WAIT_READ_NLEN:
        return ("WAIT_READ_NLEN");

    case RW_T4T_SUBSTATE_WAIT_READ_RESP:
        return ("WAIT_READ_RESP");
    case RW_T4T_SUBSTATE_WAIT_UPDATE_RESP:
        return ("WAIT_UPDATE_RESP");
    case RW_T4T_SUBSTATE_WAIT_UPDATE_NLEN:
        return ("WAIT_UPDATE_NLEN");
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    case RW_T4T_SUBSTATE_WAIT_GET_HW_VERSION:
        return ("WAIT_GET_HW_VERSION");
    case RW_T4T_SUBSTATE_WAIT_GET_SW_VERSION :
        return ("WAIT_GET_SW_VERSION");
    case RW_T4T_SUBSTATE_WAIT_GET_UID:
        return ("WAIT_GET_UID");
    case RW_T4T_SUBSTATE_WAIT_CREATE_APP:
        return ("WAIT_CREATE_APP");
    case RW_T4T_SUBSTATE_WAIT_CREATE_CC:
        return ("WAIT_CREATE_CC");
    case RW_T4T_SUBSTATE_WAIT_CREATE_NDEF:
        return ("WAIT_CREATE_NDEF");
    case RW_T4T_SUBSTATE_WAIT_WRITE_CC :
        return ("WAIT_WRITE_CC");
    case RW_T4T_SUBSTATE_WAIT_WRITE_NDEF:
        return ("WAIT_WRITE_NDEF");
    case RW_T3BT_SUBSTATE_WAIT_GET_ATTRIB:
        return ("WAIT_GET_ATTRIB");
    case RW_T3BT_SUBSTATE_WAIT_GET_PUPI:
        return ("WAIT_GET_PUPI");
#endif
    default:
        return ("???? UNKNOWN SUBSTATE");
    }
}
#endif

#endif /* (NFC_INCLUDED == TRUE) */
