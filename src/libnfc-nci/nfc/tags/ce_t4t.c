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
 *  This file contains the implementation for Type 4 tag in Card Emulation
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
#include "ce_api.h"
#include "ce_int.h"
#include "tags_int.h"
#include "gki.h"

#if (CE_TEST_INCLUDED == TRUE) /* test only */
BOOLEAN mapping_aid_test_enabled = FALSE;
UINT8   ce_test_tag_app_id[T4T_V20_NDEF_TAG_AID_LEN] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};
#endif

/*******************************************************************************
**
** Function         ce_t4t_send_to_lower
**
** Description      Send packet to lower layer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN ce_t4t_send_to_lower (BT_HDR *p_r_apdu)
{
#if (BT_TRACE_PROTOCOL == TRUE)
    DispCET4Tags (p_r_apdu, FALSE);
#endif

    if (NFC_SendData (NFC_RF_CONN_ID, p_r_apdu) != NFC_STATUS_OK)
    {
        CE_TRACE_ERROR0 ("ce_t4t_send_to_lower (): NFC_SendData () failed");
        return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
**
** Function         ce_t4t_send_status
**
** Description      Send status on R-APDU to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN ce_t4t_send_status (UINT16 status)
{
    BT_HDR      *p_r_apdu;
    UINT8       *p;

    CE_TRACE_DEBUG1 ("ce_t4t_send_status (): Status:0x%04X", status);

    p_r_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_CE_POOL_ID);

    if (!p_r_apdu)
    {
        CE_TRACE_ERROR0 ("ce_t4t_send_status (): Cannot allocate buffer");
        return FALSE;
    }

    p_r_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;

    UINT16_TO_BE_STREAM (p, status);

    p_r_apdu->len = T4T_RSP_STATUS_WORDS_SIZE;

    if (!ce_t4t_send_to_lower (p_r_apdu))
    {
        return FALSE;
    }
    return TRUE;
}

/*******************************************************************************
**
** Function         ce_t4t_select_file
**
** Description      Select a file
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN ce_t4t_select_file (UINT16 file_id)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;

    CE_TRACE_DEBUG1 ("ce_t4t_select_file (): FileID:0x%04X", file_id);

    if (file_id == T4T_CC_FILE_ID)
    {
        CE_TRACE_DEBUG0 ("ce_t4t_select_file (): Select CC file");

        p_t4t->status |= CE_T4T_STATUS_CC_FILE_SELECTED;
        p_t4t->status &= ~ (CE_T4T_STATUS_NDEF_SELECTED);

        return TRUE;
    }

    if (file_id == CE_T4T_MANDATORY_NDEF_FILE_ID)
    {
        CE_TRACE_DEBUG3 ("ce_t4t_select_file (): NLEN:0x%04X, MaxFileSize:0x%04X, WriteAccess:%s",
                          p_t4t->nlen,
                          p_t4t->max_file_size,
                          (p_t4t->status & CE_T4T_STATUS_NDEF_FILE_READ_ONLY ? "RW" : "RO"));

        p_t4t->status |= CE_T4T_STATUS_NDEF_SELECTED;
        p_t4t->status &= ~ (CE_T4T_STATUS_CC_FILE_SELECTED);

        return TRUE;
    }
    else
    {
        CE_TRACE_ERROR1 ("ce_t4t_select_file (): Cannot find file ID (0x%04X)", file_id);

        p_t4t->status &= ~ (CE_T4T_STATUS_CC_FILE_SELECTED);
        p_t4t->status &= ~ (CE_T4T_STATUS_NDEF_SELECTED);

        return FALSE;
    }
}

/*******************************************************************************
**
** Function         ce_t4t_read_binary
**
** Description      Read data from selected file and send R-APDU to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN ce_t4t_read_binary (UINT16 offset, UINT8 length)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       *p_src = NULL, *p_dst;
    BT_HDR      *p_r_apdu;

    CE_TRACE_DEBUG3 ("ce_t4t_read_binary (): Offset:0x%04X, Length:0x%04X, selected status = 0x%02X",
                      offset, length, p_t4t->status);

    if (p_t4t->status & CE_T4T_STATUS_CC_FILE_SELECTED)
    {
        p_src = p_t4t->cc_file;
    }
    else if (p_t4t->status & CE_T4T_STATUS_NDEF_SELECTED)
    {
        if (p_t4t->p_scratch_buf)
            p_src = p_t4t->p_scratch_buf;
        else
            p_src = p_t4t->p_ndef_msg;
    }

    if (p_src)
    {
        p_r_apdu = (BT_HDR *) GKI_getpoolbuf (NFC_CE_POOL_ID);

        if (!p_r_apdu)
        {
            CE_TRACE_ERROR0 ("ce_t4t_read_binary (): Cannot allocate buffer");
            return FALSE;
        }

        p_r_apdu->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
        p_dst = (UINT8 *) (p_r_apdu + 1) + p_r_apdu->offset;

        p_r_apdu->len = length;

        /* add NLEN before NDEF message and adjust offset             */
        /* if NDEF file is selected and offset < T4T_FILE_LENGTH_SIZE */
        if ((p_t4t->status & CE_T4T_STATUS_NDEF_SELECTED) && (length > 0))
        {
            if (offset == 0)
            {
                UINT16_TO_BE_STREAM (p_dst, p_t4t->nlen);

                if (length == 1)
                {
                    length = 0;
                    p_dst--;
                }
                else /* length >= 2 */
                    length -= T4T_FILE_LENGTH_SIZE;
            }
            else if (offset == 1)
            {
                UINT8_TO_BE_STREAM (p_dst, (UINT8) (p_t4t->nlen));

                offset = 0;
                length--;
            }
            else
            {
                offset -= T4T_FILE_LENGTH_SIZE;
            }
        }

        if (length > 0)
        {
            memcpy (p_dst, p_src + offset, length);
            p_dst += length;
        }

        UINT16_TO_BE_STREAM (p_dst, T4T_RSP_CMD_CMPLTED);
        p_r_apdu->len += T4T_RSP_STATUS_WORDS_SIZE;

        if (!ce_t4t_send_to_lower (p_r_apdu))
        {
            return FALSE;
        }
        return TRUE;
    }
    else
    {
        CE_TRACE_ERROR0 ("ce_t4t_read_binary (): No selected file");

        if (!ce_t4t_send_status (T4T_RSP_CMD_NOT_ALLOWED))
        {
            return FALSE;
        }
        return TRUE;
    }
}

/*******************************************************************************
**
** Function         ce_t4t_update_binary
**
** Description      Update file and send R-APDU to peer
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN ce_t4t_update_binary (UINT16 offset, UINT8 length, UINT8 *p_data)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       *p;
    UINT8        file_length[2];
    UINT16       starting_offset;
    tCE_DATA     ce_data;

    CE_TRACE_DEBUG3 ("ce_t4t_update_binary (): Offset:0x%04X, Length:0x%04X, selected status = 0x%02X",
                      offset, length, p_t4t->status);

    starting_offset = offset;

    /* update file size (NLEN) */
    if ((offset < T4T_FILE_LENGTH_SIZE) && (length > 0))
    {
        p = file_length;
        UINT16_TO_BE_STREAM (p, p_t4t->nlen);

        while ((offset < T4T_FILE_LENGTH_SIZE) && (length > 0))
        {
            *(file_length + offset++) = *(p_data++);
            length--;
        }

        p = file_length;
        BE_STREAM_TO_UINT16 (p_t4t->nlen, p);
    }

    if (length > 0)
        memcpy (p_t4t->p_scratch_buf + offset - T4T_FILE_LENGTH_SIZE, p_data, length);

    /* if this is the last step: writing non-zero length in NLEN */
    if ((starting_offset == 0) && (p_t4t->nlen > 0))
    {
        nfc_stop_quick_timer (&p_t4t->timer);

        if (ce_cb.p_cback)
        {
            ce_data.update_info.status = NFC_STATUS_OK;
            ce_data.update_info.length = p_t4t->nlen;
            ce_data.update_info.p_data = p_t4t->p_scratch_buf;

            (*ce_cb.p_cback) (CE_T4T_NDEF_UPDATE_CPLT_EVT, &ce_data);
            CE_TRACE_DEBUG0 ("ce_t4t_update_binary (): Sent CE_T4T_NDEF_UPDATE_CPLT_EVT");
        }

        p_t4t->status &= ~ (CE_T4T_STATUS_NDEF_FILE_UPDATING);
    }
    else if (!(p_t4t->status & CE_T4T_STATUS_NDEF_FILE_UPDATING))
    {
        /* starting of updating */
        p_t4t->status |= CE_T4T_STATUS_NDEF_FILE_UPDATING;

        nfc_start_quick_timer (&p_t4t->timer, NFC_TTYPE_CE_T4T_UPDATE,
                               (CE_T4T_TOUT_UPDATE * QUICK_TIMER_TICKS_PER_SEC) / 1000);

        if (ce_cb.p_cback)
            (*ce_cb.p_cback) (CE_T4T_NDEF_UPDATE_START_EVT, NULL);
    }

    if (!ce_t4t_send_status (T4T_RSP_CMD_CMPLTED))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/*******************************************************************************
**
** Function         ce_t4t_set_version_in_cc
**
** Description      update version in CC file
**                  If reader selects NDEF Tag Application with V1.0 AID then
**                  set V1.0 into CC file.
**                  If reader selects NDEF Tag Application with V2.0 AID then
**                  set V2.0 into CC file.
**
** Returns          None
**
*******************************************************************************/
static void ce_t4t_set_version_in_cc (UINT8 version)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       *p;

    CE_TRACE_DEBUG1 ("ce_t4t_set_version_in_cc (): version = 0x%02X", version);

    p = p_t4t->cc_file + T4T_VERSION_OFFSET_IN_CC;

    UINT8_TO_BE_STREAM (p, version);
}

/*******************************************************************************
**
** Function         ce_t4t_process_select_file_cmd
**
** Description      This function processes Select Command by file ID.
**
** Returns          TRUE if success
**
*******************************************************************************/
static BOOLEAN ce_t4t_process_select_file_cmd (UINT8 *p_cmd)
{
    UINT8  data_len;
    UINT16 file_id, status_words;

    CE_TRACE_DEBUG0 ("ce_t4t_process_select_file_cmd ()");

    p_cmd++; /* skip P2 */

    /* Lc Byte */
    BE_STREAM_TO_UINT8 (data_len, p_cmd);

    if (data_len == T4T_FILE_ID_SIZE)
    {
        /* File ID */
        BE_STREAM_TO_UINT16 (file_id, p_cmd);

        if (ce_t4t_select_file (file_id))
        {
            status_words = T4T_RSP_CMD_CMPLTED;
        }
        else
        {
            status_words = T4T_RSP_NOT_FOUND;
        }
    }
    else
    {
        status_words = T4T_RSP_WRONG_LENGTH;
    }

    if (!ce_t4t_send_status (status_words))
    {
        return FALSE;
    }

    if (status_words == T4T_RSP_CMD_CMPLTED)
    {
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         ce_t4t_process_select_app_cmd
**
** Description      This function processes Select Command by AID.
**
** Returns          none
**
*******************************************************************************/
static void ce_t4t_process_select_app_cmd (UINT8 *p_cmd, BT_HDR *p_c_apdu)
{
    UINT8    data_len;
    UINT16   status_words = 0x0000; /* invalid status words */
    tCE_DATA ce_data;
    UINT8    xx;

    CE_TRACE_DEBUG0 ("ce_t4t_process_select_app_cmd ()");

    p_cmd++; /* skip P2 */

    /* Lc Byte */
    BE_STREAM_TO_UINT8 (data_len, p_cmd);

#if (CE_TEST_INCLUDED == TRUE)
    if (mapping_aid_test_enabled)
    {
        if (  (data_len == T4T_V20_NDEF_TAG_AID_LEN)
            &&(!memcmp(p_cmd, ce_test_tag_app_id, data_len))
            &&(ce_cb.mem.t4t.p_ndef_msg)  )
        {
            GKI_freebuf (p_c_apdu);
            ce_t4t_send_status ((UINT16) T4T_RSP_CMD_CMPLTED);
            return;
        }
    }
#endif

    /*
    ** Compare AIDs registered by applications
    ** if found, use callback of the application
    ** otherwise, return error and maintain the same status
    */
    ce_cb.mem.t4t.selected_aid_idx = CE_T4T_MAX_REG_AID;
    for (xx = 0; xx < CE_T4T_MAX_REG_AID; xx++)
    {
        if (  (ce_cb.mem.t4t.reg_aid[xx].aid_len > 0)
            &&(ce_cb.mem.t4t.reg_aid[xx].aid_len == data_len)
            &&(!(memcmp(ce_cb.mem.t4t.reg_aid[xx].aid, p_cmd, data_len)))  )
        {
            ce_cb.mem.t4t.selected_aid_idx = xx;
            break;
        }
    }

    /* if found matched AID */
    if (ce_cb.mem.t4t.selected_aid_idx < CE_T4T_MAX_REG_AID)
    {
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_CC_FILE_SELECTED);
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_NDEF_SELECTED);
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_T4T_APP_SELECTED);
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_WILDCARD_AID_SELECTED);
        ce_cb.mem.t4t.status |= CE_T4T_STATUS_REG_AID_SELECTED;

        CE_TRACE_DEBUG4 ("ce_t4t_process_select_app_cmd (): Registered AID[%02X%02X%02X%02X...] is selected",
                         ce_cb.mem.t4t.reg_aid[ce_cb.mem.t4t.selected_aid_idx].aid[0],
                         ce_cb.mem.t4t.reg_aid[ce_cb.mem.t4t.selected_aid_idx].aid[1],
                         ce_cb.mem.t4t.reg_aid[ce_cb.mem.t4t.selected_aid_idx].aid[2],
                         ce_cb.mem.t4t.reg_aid[ce_cb.mem.t4t.selected_aid_idx].aid[3]);

        ce_data.raw_frame.status = NFC_STATUS_OK;
        ce_data.raw_frame.p_data = p_c_apdu;
        ce_data.raw_frame.aid_handle = ce_cb.mem.t4t.selected_aid_idx;

        p_c_apdu = NULL;

        (*(ce_cb.mem.t4t.reg_aid[ce_cb.mem.t4t.selected_aid_idx].p_cback)) (CE_T4T_RAW_FRAME_EVT, &ce_data);
    }
    else if (  (data_len == T4T_V20_NDEF_TAG_AID_LEN)
             &&(!memcmp(p_cmd, t4t_v20_ndef_tag_aid, data_len - 1))
             &&(ce_cb.mem.t4t.p_ndef_msg)  )
    {
        p_cmd += data_len - 1;

        /* adjust version if possible */
        if ((*p_cmd) == 0x00)
        {
            ce_t4t_set_version_in_cc (T4T_VERSION_1_0);
            status_words = T4T_RSP_CMD_CMPLTED;
        }
        else if ((*p_cmd) == 0x01)
        {
            ce_t4t_set_version_in_cc (T4T_VERSION_2_0);
            status_words = T4T_RSP_CMD_CMPLTED;
        }
        else
        {
            CE_TRACE_DEBUG0 ("ce_t4t_process_select_app_cmd (): Not found matched AID");
            status_words = T4T_RSP_NOT_FOUND;
        }
    }
    else if (ce_cb.mem.t4t.p_wildcard_aid_cback)
    {
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_CC_FILE_SELECTED);
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_NDEF_SELECTED);
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_T4T_APP_SELECTED);
        ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_REG_AID_SELECTED);
        ce_cb.mem.t4t.status |= CE_T4T_STATUS_WILDCARD_AID_SELECTED;

        ce_data.raw_frame.status = NFC_STATUS_OK;
        ce_data.raw_frame.p_data = p_c_apdu;
        ce_data.raw_frame.aid_handle = CE_T4T_WILDCARD_AID_HANDLE;
        p_c_apdu = NULL;

        CE_TRACE_DEBUG0 ("CET4T: Forward raw frame (SELECT APP) to wildcard AID handler");
        (*(ce_cb.mem.t4t.p_wildcard_aid_cback)) (CE_T4T_RAW_FRAME_EVT, &ce_data);
    }
    else
    {
        CE_TRACE_DEBUG0 ("ce_t4t_process_select_app_cmd (): Not found matched AID or not listening T4T NDEF");
        status_words = T4T_RSP_NOT_FOUND;
    }

    if (status_words)
    {
        /* if T4T CE can support */
        if (status_words == T4T_RSP_CMD_CMPLTED)
        {
            ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_CC_FILE_SELECTED);
            ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_NDEF_SELECTED);
            ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_REG_AID_SELECTED);
            ce_cb.mem.t4t.status &= ~ (CE_T4T_STATUS_WILDCARD_AID_SELECTED);
            ce_cb.mem.t4t.status |= CE_T4T_STATUS_T4T_APP_SELECTED;

            CE_TRACE_DEBUG0 ("ce_t4t_process_select_app_cmd (): T4T CE App selected");
        }

        ce_t4t_send_status (status_words);
        GKI_freebuf (p_c_apdu);
    }
    /* if status_words is not set then upper layer will send R-APDU */

    return;
}

/*******************************************************************************
**
** Function         ce_t4t_process_timeout
**
** Description      process timeout event
**
** Returns          none
**
*******************************************************************************/
void ce_t4t_process_timeout (TIMER_LIST_ENT *p_tle)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    tCE_DATA     ce_data;

    CE_TRACE_DEBUG1 ("ce_t4t_process_timeout () event=%d", p_tle->event);

    if (p_tle->event == NFC_TTYPE_CE_T4T_UPDATE)
    {
        if (p_t4t->status & CE_T4T_STATUS_NDEF_FILE_UPDATING)
        {
            ce_data.status = NFC_STATUS_TIMEOUT;

            if (ce_cb.p_cback)
                (*ce_cb.p_cback) (CE_T4T_NDEF_UPDATE_ABORT_EVT, &ce_data);

            p_t4t->status &= ~ (CE_T4T_STATUS_NDEF_FILE_UPDATING);
        }
    }
    else
    {
        CE_TRACE_ERROR1 ("ce_t4t_process_timeout () unknown event=%d", p_tle->event);
    }
}

/*******************************************************************************
**
** Function         ce_t4t_data_cback
**
** Description      This callback function receives the data from NFCC.
**
** Returns          none
**
*******************************************************************************/
static void ce_t4t_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    BT_HDR  *p_c_apdu;
    UINT8   *p_cmd;
    UINT8    cla, instruct, select_type = 0, length;
    UINT16   offset, max_file_size;
    tCE_DATA ce_data;

    if (event == NFC_DEACTIVATE_CEVT)
    {
        NFC_SetStaticRfCback (NULL);
        return;
    }
    if (event != NFC_DATA_CEVT)
    {
        return;
    }

    p_c_apdu = (BT_HDR *) p_data->data.p_data;

#if (BT_TRACE_PROTOCOL == TRUE)
    DispCET4Tags (p_c_apdu, TRUE);
#endif

    CE_TRACE_DEBUG1 ("ce_t4t_data_cback (): conn_id = 0x%02X", conn_id);

    p_cmd = (UINT8 *) (p_c_apdu + 1) + p_c_apdu->offset;

    /* Class Byte */
    BE_STREAM_TO_UINT8 (cla, p_cmd);

    /* Don't check class if registered AID has been selected */
    if (  (cla != T4T_CMD_CLASS)
        &&((ce_cb.mem.t4t.status & CE_T4T_STATUS_REG_AID_SELECTED) == 0)
        &&((ce_cb.mem.t4t.status & CE_T4T_STATUS_WILDCARD_AID_SELECTED) == 0)  )
    {
        GKI_freebuf (p_c_apdu);
        ce_t4t_send_status (T4T_RSP_CLASS_NOT_SUPPORTED);
        return;
    }

    /* Instruction Byte */
    BE_STREAM_TO_UINT8 (instruct, p_cmd);

    if ((cla == T4T_CMD_CLASS) && (instruct == T4T_CMD_INS_SELECT))
    {
        /* P1 Byte */
        BE_STREAM_TO_UINT8 (select_type, p_cmd);

        if (select_type == T4T_CMD_P1_SELECT_BY_NAME)
        {
            ce_t4t_process_select_app_cmd (p_cmd, p_c_apdu);
            return;
        }
    }

    /* if registered AID is selected */
    if (ce_cb.mem.t4t.status & CE_T4T_STATUS_REG_AID_SELECTED)
    {
        CE_TRACE_DEBUG0 ("CET4T: Forward raw frame to registered AID");

        /* forward raw frame to upper layer */
        if (ce_cb.mem.t4t.selected_aid_idx < CE_T4T_MAX_REG_AID)
        {
            ce_data.raw_frame.status = p_data->data.status;
            ce_data.raw_frame.p_data = p_c_apdu;
            ce_data.raw_frame.aid_handle = ce_cb.mem.t4t.selected_aid_idx;
            p_c_apdu = NULL;

            (*(ce_cb.mem.t4t.reg_aid[ce_cb.mem.t4t.selected_aid_idx].p_cback)) (CE_T4T_RAW_FRAME_EVT, &ce_data);
        }
        else
        {
            GKI_freebuf (p_c_apdu);
            ce_t4t_send_status (T4T_RSP_NOT_FOUND);
        }
    }
    else if (ce_cb.mem.t4t.status & CE_T4T_STATUS_WILDCARD_AID_SELECTED)
    {
        CE_TRACE_DEBUG0 ("CET4T: Forward raw frame to wildcard AID handler");

        /* forward raw frame to upper layer */
        ce_data.raw_frame.status = p_data->data.status;
        ce_data.raw_frame.p_data = p_c_apdu;
        ce_data.raw_frame.aid_handle = CE_T4T_WILDCARD_AID_HANDLE;
        p_c_apdu = NULL;

        (*(ce_cb.mem.t4t.p_wildcard_aid_cback)) (CE_T4T_RAW_FRAME_EVT, &ce_data);
    }
    else if (ce_cb.mem.t4t.status & CE_T4T_STATUS_T4T_APP_SELECTED)
    {
        if (instruct == T4T_CMD_INS_SELECT)
        {
            /* P1 Byte is already parsed */
            if (select_type == T4T_CMD_P1_SELECT_BY_FILE_ID)
            {
                ce_t4t_process_select_file_cmd (p_cmd);
            }
            else
            {
                CE_TRACE_ERROR1 ("CET4T: Bad P1 byte (0x%02X)", select_type);
                ce_t4t_send_status (T4T_RSP_WRONG_PARAMS);
            }
        }
        else if (instruct == T4T_CMD_INS_READ_BINARY)
        {
            if (  (ce_cb.mem.t4t.status & CE_T4T_STATUS_CC_FILE_SELECTED)
                ||(ce_cb.mem.t4t.status & CE_T4T_STATUS_NDEF_SELECTED)  )
            {
                if (ce_cb.mem.t4t.status & CE_T4T_STATUS_CC_FILE_SELECTED)
                {
                    max_file_size = T4T_FC_TLV_OFFSET_IN_CC + T4T_FILE_CONTROL_TLV_SIZE;
                }
                else
                {
                    max_file_size = ce_cb.mem.t4t.max_file_size;
                }

                BE_STREAM_TO_UINT16 (offset, p_cmd); /* Offset */
                BE_STREAM_TO_UINT8 (length, p_cmd); /* Le     */

                /* check if valid parameters */
                if (length <= CE_T4T_MAX_LE)
                {
                    /* CE allows to read more than current file size but not max file size */
                    if (length + offset > max_file_size)
                    {
                        if (offset < max_file_size)
                        {
                            length = (UINT8) (max_file_size - offset);

                            CE_TRACE_DEBUG2 ("CET4T: length is reduced to %d by max_file_size (%d)",
                                              length, max_file_size);
                        }
                        else
                        {
                            CE_TRACE_ERROR2 ("CET4T: offset (%d) must be less than max_file_size (%d)",
                                              offset, max_file_size);
                            length = 0;
                        }
                    }
                }
                else
                {
                    CE_TRACE_ERROR2 ("CET4T: length (%d) must be less than MLe (%d)",
                                      length, CE_T4T_MAX_LE);
                    length = 0;
                }

                if (length > 0)
                    ce_t4t_read_binary (offset, length);
                else
                    ce_t4t_send_status (T4T_RSP_WRONG_PARAMS);
            }
            else
            {
                CE_TRACE_ERROR0 ("CET4T: File has not been selected");
                ce_t4t_send_status (T4T_RSP_CMD_NOT_ALLOWED);
            }
        }
        else if (instruct == T4T_CMD_INS_UPDATE_BINARY)
        {
            if (ce_cb.mem.t4t.status & CE_T4T_STATUS_NDEF_FILE_READ_ONLY)
            {
                CE_TRACE_ERROR0 ("CET4T: No access right");
                ce_t4t_send_status (T4T_RSP_CMD_NOT_ALLOWED);
            }
            else if (ce_cb.mem.t4t.status & CE_T4T_STATUS_NDEF_SELECTED)
            {
                BE_STREAM_TO_UINT16 (offset, p_cmd); /* Offset */
                BE_STREAM_TO_UINT8 (length, p_cmd); /* Lc     */

                /* check if valid parameters */
                if (length <= CE_T4T_MAX_LC)
                {
                    if (length + offset > ce_cb.mem.t4t.max_file_size)
                    {
                        CE_TRACE_ERROR3 ("CET4T: length (%d) + offset (%d) must be less than max_file_size (%d)",
                                          length, offset, ce_cb.mem.t4t.max_file_size);
                        length = 0;
                    }
                }
                else
                {
                    CE_TRACE_ERROR2 ("CET4T: length (%d) must be less than MLc (%d)",
                                      length, CE_T4T_MAX_LC);
                    length = 0;
                }

                if (length > 0)
                    ce_t4t_update_binary (offset, length, p_cmd);
                else
                    ce_t4t_send_status (T4T_RSP_WRONG_PARAMS);
            }
            else
            {
                CE_TRACE_ERROR0 ("CET4T: NDEF File has not been selected");
                ce_t4t_send_status (T4T_RSP_CMD_NOT_ALLOWED);
            }
        }
        else
        {
            CE_TRACE_ERROR1 ("CET4T: Unsupported Instruction byte (0x%02X)", instruct);
            ce_t4t_send_status (T4T_RSP_INSTR_NOT_SUPPORTED);
        }
    }
    else
    {
        CE_TRACE_ERROR0 ("CET4T: Application has not been selected");
        ce_t4t_send_status (T4T_RSP_CMD_NOT_ALLOWED);
    }

    if (p_c_apdu)
        GKI_freebuf (p_c_apdu);
}

/*******************************************************************************
**
** Function         ce_select_t4t
**
** Description      Select Type 4 Tag
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS ce_select_t4t (void)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;

    CE_TRACE_DEBUG0 ("ce_select_t4t ()");

    nfc_stop_quick_timer (&p_t4t->timer);

    /* clear other than read-only flag */
    p_t4t->status &= CE_T4T_STATUS_NDEF_FILE_READ_ONLY;

    NFC_SetStaticRfCback (ce_t4t_data_cback);

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         CE_T4tSetLocalNDEFMsg
**
** Description      Initialise CE Type 4 Tag with mandatory NDEF message
**
**                  The following event may be returned
**                      CE_T4T_UPDATE_START_EVT for starting update
**                      CE_T4T_UPDATE_CPLT_EVT for complete update
**                      CE_T4T_UPDATE_ABORT_EVT for failure of update
**                      CE_T4T_RAW_FRAME_EVT for raw frame
**
**                  read_only:      TRUE if read only
**                  ndef_msg_max:   Max NDEF message size
**                  ndef_msg_len:   NDEF message size
**                  p_ndef_msg:     NDEF message (excluding NLEN)
**                  p_scratch_buf:  temp storage for update
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T4tSetLocalNDEFMsg (BOOLEAN    read_only,
                                   UINT16     ndef_msg_max,
                                   UINT16     ndef_msg_len,
                                   UINT8      *p_ndef_msg,
                                   UINT8      *p_scratch_buf)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       *p;

    CE_TRACE_API3 ("CE_T4tSetLocalNDEFMsg () read_only=%d, ndef_msg_max=%d, ndef_msg_len=%d",
                   read_only, ndef_msg_max, ndef_msg_len);

    if (!p_ndef_msg)
    {
        p_t4t->p_ndef_msg = NULL;

        CE_TRACE_DEBUG0 ("CE_T4tSetLocalNDEFMsg (): T4T is disabled");
        return NFC_STATUS_OK;
    }

    if ((!read_only) && (!p_scratch_buf))
    {
        CE_TRACE_ERROR0 ("CE_T4tSetLocalNDEFMsg (): p_scratch_buf cannot be NULL if not read-only");
        return NFC_STATUS_FAILED;
    }

#if (CE_TEST_INCLUDED == TRUE)
    mapping_aid_test_enabled = FALSE;
#endif

    /* Initialise CC file */
    p = p_t4t->cc_file;

    UINT16_TO_BE_STREAM (p, T4T_CC_FILE_MIN_LEN);
    UINT8_TO_BE_STREAM (p, T4T_MY_VERSION);
    UINT16_TO_BE_STREAM (p, CE_T4T_MAX_LE);
    UINT16_TO_BE_STREAM (p, CE_T4T_MAX_LC);

    /* Mandatory NDEF File Control TLV */
    UINT8_TO_BE_STREAM (p, T4T_NDEF_FILE_CONTROL_TYPE);            /* type */
    UINT8_TO_BE_STREAM (p, T4T_FILE_CONTROL_LENGTH);               /* length */
    UINT16_TO_BE_STREAM (p, CE_T4T_MANDATORY_NDEF_FILE_ID);         /* file ID */
    UINT16_TO_BE_STREAM (p, ndef_msg_max + T4T_FILE_LENGTH_SIZE);   /* max NDEF file size */
    UINT8_TO_BE_STREAM (p, T4T_FC_READ_ACCESS);                    /* read access */

    if (read_only)
    {
        UINT8_TO_BE_STREAM (p, T4T_FC_NO_WRITE_ACCESS);    /* read only */
        p_t4t->status |= CE_T4T_STATUS_NDEF_FILE_READ_ONLY;
    }
    else
    {
        UINT8_TO_BE_STREAM (p, T4T_FC_WRITE_ACCESS);       /* write access */
        p_t4t->status &= ~ (CE_T4T_STATUS_NDEF_FILE_READ_ONLY);
    }

    /* set mandatory NDEF file */
    p_t4t->p_ndef_msg    = p_ndef_msg;
    p_t4t->nlen          = ndef_msg_len;
    p_t4t->max_file_size = ndef_msg_max + T4T_FILE_LENGTH_SIZE;

    /* Initialize scratch buffer */
    p_t4t->p_scratch_buf = p_scratch_buf;

    if (p_scratch_buf)
    {
        memcpy (p_scratch_buf, p_ndef_msg, ndef_msg_len);
    }

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         CE_T4tRegisterAID
**
** Description      Register AID in CE T4T
**
**                  aid_len: length of AID (up to NFC_MAX_AID_LEN)
**                  p_aid:   AID
**                  p_cback: Raw frame will be forwarded with CE_RAW_FRAME_EVT
**
** Returns          tCE_T4T_AID_HANDLE if successful,
**                  CE_T4T_AID_HANDLE_INVALID otherwisse
**
*******************************************************************************/
tCE_T4T_AID_HANDLE CE_T4tRegisterAID (UINT8 aid_len, UINT8 *p_aid, tCE_CBACK *p_cback)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       xx;

    /* Handle registering callback for wildcard AID (all AIDs) */
    if (aid_len == 0)
    {
        CE_TRACE_API0 ("CE_T4tRegisterAID (): registering callback for wildcard AID ");

        /* Check if a wildcard callback is already registered (only one is allowed) */
        if (p_t4t->p_wildcard_aid_cback != NULL)
        {
            CE_TRACE_ERROR0 ("CE_T4tRegisterAID (): only one wildcard AID can be registered at time.");
            return CE_T4T_AID_HANDLE_INVALID;
        }

        CE_TRACE_DEBUG1 ("CE_T4tRegisterAID (): handle 0x%02x registered (for wildcard AID)", CE_T4T_WILDCARD_AID_HANDLE);
        p_t4t->p_wildcard_aid_cback = p_cback;
        return CE_T4T_WILDCARD_AID_HANDLE;
    }


    CE_TRACE_API5 ("CE_T4tRegisterAID () AID [%02X%02X%02X%02X...], %d bytes",
                   *p_aid, *(p_aid+1), *(p_aid+2), *(p_aid+3), aid_len);

    if (aid_len > NFC_MAX_AID_LEN)
    {
        CE_TRACE_ERROR1 ("CE_T4tRegisterAID (): AID is up to %d bytes", NFC_MAX_AID_LEN);
        return CE_T4T_AID_HANDLE_INVALID;
    }

    if (p_cback == NULL)
    {
        CE_TRACE_ERROR0 ("CE_T4tRegisterAID (): callback must be provided");
        return CE_T4T_AID_HANDLE_INVALID;
    }

    for (xx = 0; xx < CE_T4T_MAX_REG_AID; xx++)
    {
        if (  (p_t4t->reg_aid[xx].aid_len == aid_len)
            &&(!(memcmp(p_t4t->reg_aid[xx].aid, p_aid, aid_len)))  )
        {
            CE_TRACE_ERROR0 ("CE_T4tRegisterAID (): already registered");
            return CE_T4T_AID_HANDLE_INVALID;
        }
    }

    for (xx = 0; xx < CE_T4T_MAX_REG_AID; xx++)
    {
        if (p_t4t->reg_aid[xx].aid_len == 0)
        {
            p_t4t->reg_aid[xx].aid_len = aid_len;
            p_t4t->reg_aid[xx].p_cback = p_cback;
            memcpy (p_t4t->reg_aid[xx].aid, p_aid, aid_len);
            break;
        }
    }

    if (xx >= CE_T4T_MAX_REG_AID)
    {
        CE_TRACE_ERROR0 ("CE_T4tRegisterAID (): No resource");
        return CE_T4T_AID_HANDLE_INVALID;
    }
    else
    {
        CE_TRACE_DEBUG1 ("CE_T4tRegisterAID (): handle 0x%02x registered", xx);
    }

    return (xx);
}

/*******************************************************************************
**
** Function         CE_T4tDeregisterAID
**
** Description      Deregister AID in CE T4T
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
NFC_API extern void CE_T4tDeregisterAID (tCE_T4T_AID_HANDLE aid_handle)
{
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;

    CE_TRACE_API1 ("CE_T4tDeregisterAID () handle 0x%02x", aid_handle);

    /* Check if deregistering wildcard AID */
    if (aid_handle == CE_T4T_WILDCARD_AID_HANDLE)
    {
        if (p_t4t->p_wildcard_aid_cback != NULL)
        {
            p_t4t->p_wildcard_aid_cback = NULL;
        }
        else
        {
            CE_TRACE_ERROR0 ("CE_T4tDeregisterAID (): Invalid handle");
        }
        return;
    }

    /* Deregister AID */
    if ((aid_handle >= CE_T4T_MAX_REG_AID) || (p_t4t->reg_aid[aid_handle].aid_len==0))
    {
        CE_TRACE_ERROR0 ("CE_T4tDeregisterAID (): Invalid handle");
    }
    else
    {
        p_t4t->reg_aid[aid_handle].aid_len = 0;
        p_t4t->reg_aid[aid_handle].p_cback = NULL;
    }
}

/*******************************************************************************
**
** Function         CE_T4TTestSetCC
**
** Description      Set fields in Capability Container File for testing
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T4TTestSetCC (UINT16 cc_len,
                             UINT8  version,
                             UINT16 max_le,
                             UINT16 max_lc)
{
#if (CE_TEST_INCLUDED == TRUE)
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       *p;

    CE_TRACE_DEBUG4 ("CE_T4TTestSetCC (): CCLen:0x%04X, Ver:0x%02X, MaxLe:0x%04X, MaxLc:0x%04X",
                      cc_len, version, max_le, max_lc);

    /* CC file */
    p = p_t4t->cc_file;

    if (cc_len != 0xFFFF)
    {
        UINT16_TO_BE_STREAM (p, cc_len);
    }
    else
        p += 2;

    if (version != 0xFF)
    {
        mapping_aid_test_enabled = TRUE;
        if (version == T4T_VERSION_1_0)
            ce_test_tag_app_id[T4T_V20_NDEF_TAG_AID_LEN - 1] = 0x00;
        else if (version == T4T_VERSION_2_0)
            ce_test_tag_app_id[T4T_V20_NDEF_TAG_AID_LEN - 1] = 0x01;
        else /* Undefined version */
            ce_test_tag_app_id[T4T_V20_NDEF_TAG_AID_LEN - 1] = 0xFF;

        UINT8_TO_BE_STREAM (p, version);
    }
    else
    {
        mapping_aid_test_enabled = FALSE;
        p += 1;
    }

    if (max_le != 0xFFFF)
    {
        UINT16_TO_BE_STREAM (p, max_le);
    }
    else
        p += 2;

    if (max_lc != 0xFFFF)
    {
        UINT16_TO_BE_STREAM (p, max_lc);
    }
    else
        p += 2;

    return NFC_STATUS_OK;
#else
    return NFC_STATUS_FAILED;
#endif
}

/*******************************************************************************
**
** Function         CE_T4TTestSetNDEFCtrlTLV
**
** Description      Set fields in NDEF File Control TLV for testing
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T4TTestSetNDEFCtrlTLV (UINT8  type,
                                      UINT8  length,
                                      UINT16 file_id,
                                      UINT16 max_file_size,
                                      UINT8  read_access,
                                      UINT8  write_access)
{
#if (CE_TEST_INCLUDED == TRUE)
    tCE_T4T_MEM *p_t4t = &ce_cb.mem.t4t;
    UINT8       *p;

    CE_TRACE_DEBUG6 ("CE_T4TTestSetNDEFCtrlTLV (): type:0x%02X, len:0x%02X, FileID:0x%04X, MaxFile:0x%04X, RdAcc:0x%02X, WrAcc:0x%02X",
                      type, length, file_id, max_file_size, read_access, write_access);

    /* NDEF File control TLV */
    p = p_t4t->cc_file + T4T_FC_TLV_OFFSET_IN_CC;

    if (type != 0xFF)
    {
        UINT8_TO_BE_STREAM (p, type);
    }
    else
        p += 1;

    if (length != 0xFF)
    {
        UINT8_TO_BE_STREAM (p, length);
    }
    else
        p += 1;

    if (file_id != 0xFFFF)
    {
        UINT16_TO_BE_STREAM (p, file_id);
    }
    else
        p += 2;

    if (max_file_size != 0xFFFF)
    {
        UINT16_TO_BE_STREAM (p, max_file_size);
    }
    else
        p += 2;

    if (read_access != 0xFF)
    {
        UINT8_TO_BE_STREAM (p, read_access);
    }
    else
        p += 1;

    if (write_access != 0xFF)
    {
        UINT8_TO_BE_STREAM (p, write_access);
    }
    else
        p += 1;

    return NFC_STATUS_OK;
#else
    return NFC_STATUS_FAILED;
#endif
}
#endif /* NFC_INCLUDED == TRUE */
