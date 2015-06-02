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
 *  This file contains the implementation for Type 3 tag in Card Emulation
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

enum {
    CE_T3T_COMMAND_INVALID,
    CE_T3T_COMMAND_NFC_FORUM,
    CE_T3T_COMMAND_FELICA
};

/* T3T CE states */
enum {
    CE_T3T_STATE_NOT_ACTIVATED,
    CE_T3T_STATE_IDLE,
    CE_T3T_STATE_UPDATING
};

/* Bitmasks to indicate type of UPDATE */
#define CE_T3T_UPDATE_FL_NDEF_UPDATE_START  0x01
#define CE_T3T_UPDATE_FL_NDEF_UPDATE_CPLT   0x02
#define CE_T3T_UPDATE_FL_UPDATE             0x04

/*******************************************************************************
* Static constant definitions
*******************************************************************************/
static const UINT8 CE_DEFAULT_LF_PMM[NCI_T3T_PMM_LEN] = {0x20, 0x79, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   /* Default PMm param */

/*******************************************************************************
**
** Function         ce_t3t_init
**
** Description      Initialize tag-specific fields of ce control block
**
** Returns          none
**
*******************************************************************************/
void ce_t3t_init (void)
{
    memcpy (ce_cb.mem.t3t.local_pmm, CE_DEFAULT_LF_PMM, NCI_T3T_PMM_LEN);
    ce_cb.mem.t3t.ndef_info.nbr = CE_T3T_DEFAULT_CHECK_MAXBLOCKS;
    ce_cb.mem.t3t.ndef_info.nbw = CE_T3T_DEFAULT_UPDATE_MAXBLOCKS;
}

/*******************************************************************************
**
** Function         ce_t3t_send_to_lower
**
** Description      Send C-APDU to lower layer
**
** Returns          none
**
*******************************************************************************/
void ce_t3t_send_to_lower (BT_HDR *p_msg)
{
    UINT8 *p;

    /* Set NFC-F SoD field (payload len + 1) */
    p_msg->offset -= 1;         /* Point to SoD field */
    p = (UINT8 *) (p_msg+1) + p_msg->offset;
    UINT8_TO_STREAM (p, (p_msg->len+1));
    p_msg->len += 1;            /* Increment len to include SoD */

#if (BT_TRACE_PROTOCOL == TRUE)
    DispT3TagMessage (p_msg, FALSE);
#endif

    if (NFC_SendData (NFC_RF_CONN_ID, p_msg) != NFC_STATUS_OK)
    {
        CE_TRACE_ERROR0 ("ce_t3t_send_to_lower (): NFC_SendData () failed");
    }
}

/*******************************************************************************
**
** Function         ce_t3t_is_valid_opcode
**
** Description      Valid opcode
**
** Returns          Type of command
**
*******************************************************************************/
UINT8 ce_t3t_is_valid_opcode (UINT8 cmd_id)
{
    UINT8 retval = CE_T3T_COMMAND_INVALID;

    if (  (cmd_id == T3T_MSG_OPC_CHECK_CMD)
        ||(cmd_id == T3T_MSG_OPC_UPDATE_CMD)  )
    {
        retval = CE_T3T_COMMAND_NFC_FORUM;
    }
    else if (  (cmd_id == T3T_MSG_OPC_POLL_CMD)
             ||(cmd_id == T3T_MSG_OPC_REQ_SERVICE_CMD)
             ||(cmd_id == T3T_MSG_OPC_REQ_RESPONSE_CMD)
             ||(cmd_id == T3T_MSG_OPC_REQ_SYSTEMCODE_CMD)  )
    {
        retval = CE_T3T_COMMAND_FELICA;
    }

    return (retval);
}

/*****************************************************************************
**
** Function         ce_t3t_get_rsp_buf
**
** Description      Get a buffer for sending T3T messages
**
** Returns          BT_HDR *
**
*****************************************************************************/
BT_HDR *ce_t3t_get_rsp_buf (void)
{
    BT_HDR *p_cmd_buf;

    if ((p_cmd_buf = (BT_HDR *) GKI_getpoolbuf (NFC_CE_POOL_ID)) != NULL)
    {
        /* Reserve offset for NCI_DATA_HDR and NFC-F Sod (LEN) field */
        p_cmd_buf->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + 1;
        p_cmd_buf->len = 0;
    }

    return (p_cmd_buf);
}

/*******************************************************************************
**
** Function         ce_t3t_send_rsp
**
** Description      Send response to reader/writer
**
** Returns          none
**
*******************************************************************************/
void ce_t3t_send_rsp (tCE_CB *p_ce_cb, UINT8 *p_nfcid2, UINT8 opcode, UINT8 status1, UINT8 status2)
{
    tCE_T3T_MEM *p_cb = &p_ce_cb->mem.t3t;
    BT_HDR *p_rsp_msg;
    UINT8 *p_dst, *p_rsp_start;

    /* If p_nfcid2 is NULL, then used activated NFCID2 */
    if (p_nfcid2 == NULL)
    {
        p_nfcid2 = p_cb->local_nfcid2;
    }

    if ((p_rsp_msg = ce_t3t_get_rsp_buf ()) != NULL)
    {
        p_dst = p_rsp_start = (UINT8 *) (p_rsp_msg+1) + p_rsp_msg->offset;

        /* Response Code */
        UINT8_TO_STREAM (p_dst, opcode);

        /* Manufacturer ID */
        ARRAY_TO_STREAM (p_dst, p_nfcid2, NCI_RF_F_UID_LEN);

        /* Status1 and Status2 */
        UINT8_TO_STREAM (p_dst, status1);
        UINT8_TO_STREAM (p_dst, status2);

        p_rsp_msg->len = (UINT16) (p_dst - p_rsp_start);
        ce_t3t_send_to_lower (p_rsp_msg);
    }
    else
    {
        CE_TRACE_ERROR0 ("CE: Unable to allocat buffer for response message");
    }
}

/*******************************************************************************
**
** Function         ce_t3t_handle_update_cmd
**
** Description      Handle UPDATE command from reader/writer
**
** Returns          none
**
*******************************************************************************/
void ce_t3t_handle_update_cmd (tCE_CB *p_ce_cb, BT_HDR *p_cmd_msg)
{
    tCE_T3T_MEM *p_cb = &p_ce_cb->mem.t3t;
    UINT8 *p_temp;
    UINT8 *p_block_list = p_cb->cur_cmd.p_block_list_start;
    UINT8 *p_block_data = p_cb->cur_cmd.p_block_data_start;
    UINT8 i, j, bl0;
    UINT16 block_number, service_code, checksum, checksum_rx;
    UINT32 newlen_hiword;
    tCE_T3T_NDEF_INFO ndef_info;
    tNFC_STATUS nfc_status = NFC_STATUS_OK;
    UINT8 update_flags = 0;
    tCE_UPDATE_INFO update_info;

    /* If in idle state, notify app that update is starting */
    if (p_cb->state == CE_T3T_STATE_IDLE)
    {
        p_cb->state = CE_T3T_STATE_UPDATING;
    }

    for (i = 0; i < p_cb->cur_cmd.num_blocks; i++)
    {
        /* Read byte0 of block list */
        STREAM_TO_UINT8 (bl0, p_block_list);

        if (bl0 & T3T_MSG_MASK_TWO_BYTE_BLOCK_DESC_FORMAT)
        {
            STREAM_TO_UINT8 (block_number, p_block_list);
        }
        else
        {
            STREAM_TO_UINT16 (block_number, p_block_list);
        }

        /* Read the block from memory */
        service_code = p_cb->cur_cmd.service_code_list[bl0 & T3T_MSG_SERVICE_LIST_MASK];

        /* Reject UPDATE command if service code=T3T_MSG_NDEF_SC_RO */
        if (service_code == T3T_MSG_NDEF_SC_RO)
        {
            /* Error: invalid block number to update */
            CE_TRACE_ERROR0 ("CE: UPDATE request using read-only service");
            nfc_status = NFC_STATUS_FAILED;
            break;
        }

        /* Check for NDEF */
        if (service_code == T3T_MSG_NDEF_SC_RW)
        {
            if (p_cb->cur_cmd.num_blocks > p_cb->ndef_info.nbw)
            {
                CE_TRACE_ERROR2 ("CE: Requested too many blocks to update (requested: %i, max: %i)", p_cb->cur_cmd.num_blocks, p_cb->ndef_info.nbw);
                nfc_status = NFC_STATUS_FAILED;
                break;
            }
            else if (p_cb->ndef_info.rwflag == T3T_MSG_NDEF_RWFLAG_RO)
            {
                CE_TRACE_ERROR0 ("CE: error: write-request to read-only NDEF message.");
                nfc_status = NFC_STATUS_FAILED;
                break;
            }
            else if (block_number == 0)
            {
                CE_TRACE_DEBUG2 ("CE: Update sc 0x%04x block %i.", service_code, block_number);

                /* Special caes: NDEF block0 is the ndef attribute block */
                p_temp = p_block_data;
                STREAM_TO_UINT8 (ndef_info.version, p_block_data);
                p_block_data+=8;                                    /* Ignore nbr,nbw,maxb,and reserved (reader/writer not allowed to update this) */
                STREAM_TO_UINT8 (ndef_info.writef, p_block_data);
                p_block_data++;                                     /* Ignore rwflag (reader/writer not allowed to update this) */
                STREAM_TO_UINT8 (newlen_hiword, p_block_data);
                BE_STREAM_TO_UINT16 (ndef_info.ln, p_block_data);
                ndef_info.ln += (newlen_hiword<<16);
                BE_STREAM_TO_UINT16 (checksum_rx, p_block_data);

                checksum=0;
                for (j=0; j<T3T_MSG_NDEF_ATTR_INFO_SIZE; j++)
                {
                    checksum+=p_temp[j];
                }

                /* Compare calcuated checksum with received checksum */
                if (checksum != checksum_rx)
                {
                    CE_TRACE_ERROR0 ("CE: Checksum failed for NDEF attribute block.");
                    nfc_status = NFC_STATUS_FAILED;
                }
                else
                {
                    /* Update NDEF attribute block (only allowed to update current length and writef fields) */
                    p_cb->ndef_info.scratch_ln      = ndef_info.ln;
                    p_cb->ndef_info.scratch_writef  = ndef_info.writef;

                    /* If writef=0 indicates completion of NDEF update */
                    if (ndef_info.writef == 0)
                    {
                        update_flags |= CE_T3T_UPDATE_FL_NDEF_UPDATE_CPLT;
                    }
                    /* writef=1 indicates start of NDEF update */
                    else
                    {
                        update_flags |= CE_T3T_UPDATE_FL_NDEF_UPDATE_START;
                    }
                }
            }
            else
            {
                CE_TRACE_DEBUG2 ("CE: Udpate sc 0x%04x block %i.", service_code, block_number);

                /* Verify that block_number is within NDEF memory */
                if (block_number > p_cb->ndef_info.nmaxb)
                {
                    /* Error: invalid block number to update */
                    CE_TRACE_ERROR2 ("CE: Requested invalid NDEF block number to update %i (max is %i).", block_number, p_cb->ndef_info.nmaxb);
                    nfc_status = NFC_STATUS_FAILED;
                    break;
                }
                else
                {
                    /* Update NDEF memory block */
                    STREAM_TO_ARRAY ((&p_cb->ndef_info.p_scratch_buf[(block_number-1) * T3T_MSG_BLOCKSIZE]), p_block_data, T3T_MSG_BLOCKSIZE);
                }

                /* Set flag to indicate that this UPDATE contained at least one block */
                update_flags |= CE_T3T_UPDATE_FL_UPDATE;
            }
        }
        else
        {
            /* Error: invalid service code */
            CE_TRACE_ERROR1 ("CE: Requested invalid service code: 0x%04x.", service_code);
            nfc_status = NFC_STATUS_FAILED;
            break;
        }
    }

    /* Send appropriate response to reader/writer */
    if (nfc_status == NFC_STATUS_OK)
    {
        ce_t3t_send_rsp (p_ce_cb, NULL, T3T_MSG_OPC_UPDATE_RSP, T3T_MSG_RSP_STATUS_OK, T3T_MSG_RSP_STATUS_OK);
    }
    else
    {
        ce_t3t_send_rsp (p_ce_cb, NULL, T3T_MSG_OPC_UPDATE_RSP, T3T_MSG_RSP_STATUS_ERROR, T3T_MSG_RSP_STATUS2_ERROR_MEMORY);
        p_cb->state = CE_T3T_STATE_IDLE;
    }


    /* Notify the app of what got updated */
    if (update_flags & CE_T3T_UPDATE_FL_NDEF_UPDATE_START)
    {
        /* NDEF attribute got updated with WriteF=TRUE */
        p_ce_cb->p_cback (CE_T3T_NDEF_UPDATE_START_EVT, NULL);
    }

    if (update_flags & CE_T3T_UPDATE_FL_UPDATE)
    {
        /* UPDATE message contained at least one non-NDEF block */
        p_ce_cb->p_cback (CE_T3T_UPDATE_EVT, NULL);
    }


    if (update_flags & CE_T3T_UPDATE_FL_NDEF_UPDATE_CPLT)
    {
        /* NDEF attribute got updated with WriteF=FALSE */
        update_info.status = nfc_status;
        update_info.p_data = p_cb->ndef_info.p_scratch_buf;
        update_info.length = p_cb->ndef_info.scratch_ln;
        p_cb->state = CE_T3T_STATE_IDLE;
        p_ce_cb->p_cback (CE_T3T_NDEF_UPDATE_CPLT_EVT, (tCE_DATA *) &update_info);
    }

    GKI_freebuf (p_cmd_msg);
}

/*******************************************************************************
**
** Function         ce_t3t_handle_check_cmd
**
** Description      Handle CHECK command from reader/writer
**
** Returns          Nothing
**
*******************************************************************************/
void ce_t3t_handle_check_cmd (tCE_CB *p_ce_cb, BT_HDR *p_cmd_msg)
{
    tCE_T3T_MEM *p_cb = &p_ce_cb->mem.t3t;
    BT_HDR *p_rsp_msg;
    UINT8 *p_rsp_start;
    UINT8 *p_dst, *p_temp, *p_status;
    UINT8 *p_src = p_cb->cur_cmd.p_block_list_start;
    UINT8 i, bl0;
    UINT8 ndef_writef;
    UINT32 ndef_len;
    UINT16 block_number, service_code, checksum;

    if ((p_rsp_msg = ce_t3t_get_rsp_buf ()) != NULL)
    {
        p_dst = p_rsp_start = (UINT8 *) (p_rsp_msg+1) + p_rsp_msg->offset;

        /* Response Code */
        UINT8_TO_STREAM (p_dst, T3T_MSG_OPC_CHECK_RSP);

        /* Manufacturer ID */
        ARRAY_TO_STREAM (p_dst, p_cb->local_nfcid2, NCI_RF_F_UID_LEN);

        /* Save pointer to start of status field */
        p_status = p_dst;

        /* Status1 and Status2 (assume success initially */
        UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS_OK);
        UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS_OK);
        UINT8_TO_STREAM (p_dst, p_cb->cur_cmd.num_blocks);

        for (i = 0; i < p_cb->cur_cmd.num_blocks; i++)
        {
            /* Read byte0 of block list */
            STREAM_TO_UINT8 (bl0, p_src);

            if (bl0 & T3T_MSG_MASK_TWO_BYTE_BLOCK_DESC_FORMAT)
            {
                STREAM_TO_UINT8 (block_number, p_src);
            }
            else
            {
                STREAM_TO_UINT16 (block_number, p_src);
            }

            /* Read the block from memory */
            service_code = p_cb->cur_cmd.service_code_list[bl0 & T3T_MSG_SERVICE_LIST_MASK];

            /* Check for NDEF */
            if ((service_code == T3T_MSG_NDEF_SC_RO) || (service_code == T3T_MSG_NDEF_SC_RW))
            {
                /* Verify Nbr (NDEF only) */
                if (p_cb->cur_cmd.num_blocks > p_cb->ndef_info.nbr)
                {
                    /* Error: invalid number of blocks to check */
                    CE_TRACE_ERROR2 ("CE: Requested too many blocks to check (requested: %i, max: %i)", p_cb->cur_cmd.num_blocks, p_cb->ndef_info.nbr);

                    p_dst = p_status;
                    UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS_ERROR);
                    UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS2_ERROR_MEMORY);
                    break;
                }
                else if (block_number == 0)
                {
                    /* Special caes: NDEF block0 is the ndef attribute block */
                    p_temp = p_dst;

                    /* For rw ndef, use scratch buffer's attributes (in case reader/writer had previously updated NDEF) */
                    if ((p_cb->ndef_info.rwflag == T3T_MSG_NDEF_RWFLAG_RW) && (p_cb->ndef_info.p_scratch_buf))
                    {
                        ndef_writef = p_cb->ndef_info.scratch_writef;
                        ndef_len    = p_cb->ndef_info.scratch_ln;
                    }
                    else
                    {
                        ndef_writef = p_cb->ndef_info.writef;
                        ndef_len    = p_cb->ndef_info.ln;
                    }

                    UINT8_TO_STREAM (p_dst, p_cb->ndef_info.version);
                    UINT8_TO_STREAM (p_dst, p_cb->ndef_info.nbr);
                    UINT8_TO_STREAM (p_dst, p_cb->ndef_info.nbw);
                    UINT16_TO_BE_STREAM (p_dst, p_cb->ndef_info.nmaxb);
                    UINT32_TO_STREAM (p_dst, 0);
                    UINT8_TO_STREAM (p_dst, ndef_writef);
                    UINT8_TO_STREAM (p_dst, p_cb->ndef_info.rwflag);
                    UINT8_TO_STREAM (p_dst, (ndef_len >> 16 & 0xFF));
                    UINT16_TO_BE_STREAM (p_dst, (ndef_len & 0xFFFF));

                    checksum = 0;
                    for (i = 0; i < T3T_MSG_NDEF_ATTR_INFO_SIZE; i++)
                    {
                        checksum+=p_temp[i];
                    }
                    UINT16_TO_BE_STREAM (p_dst, checksum);
                }
                else
                {
                    /* Verify that block_number is within NDEF memory */
                    if (block_number > p_cb->ndef_info.nmaxb)
                    {
                        /* Invalid block number */
                        p_dst = p_status;

                        CE_TRACE_ERROR1 ("CE: Requested block number to check %i.", block_number);

                        /* Error: invalid number of blocks to check */
                        UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS_ERROR);
                        UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS2_ERROR_MEMORY);
                        break;
                    }
                    else
                    {
                        /* If card is RW, then read from the scratch buffer (so reader/write can read back what it had just written */
                        if ((p_cb->ndef_info.rwflag == T3T_MSG_NDEF_RWFLAG_RW) && (p_cb->ndef_info.p_scratch_buf))
                        {
                            ARRAY_TO_STREAM (p_dst, (&p_cb->ndef_info.p_scratch_buf[(block_number-1) * T3T_MSG_BLOCKSIZE]), T3T_MSG_BLOCKSIZE);
                        }
                        else
                        {
                            ARRAY_TO_STREAM (p_dst, (&p_cb->ndef_info.p_buf[(block_number-1) * T3T_MSG_BLOCKSIZE]), T3T_MSG_BLOCKSIZE);
                        }
                    }
                }
            }
            else
            {
                /* Error: invalid service code */
                CE_TRACE_ERROR1 ("CE: Requested invalid service code: 0x%04x.", service_code);

                p_dst = p_status;
                UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS_ERROR);
                UINT8_TO_STREAM (p_dst, T3T_MSG_RSP_STATUS2_ERROR_MEMORY);
                break;
            }
        }

        p_rsp_msg->len = (UINT16) (p_dst - p_rsp_start);
        ce_t3t_send_to_lower (p_rsp_msg);
    }
    else
    {
        CE_TRACE_ERROR0 ("CE: Unable to allocat buffer for response message");
    }

    GKI_freebuf (p_cmd_msg);
}


/*******************************************************************************
**
** Function         ce_t3t_handle_non_nfc_forum_cmd
**
** Description      Handle POLL command from reader/writer
**
** Returns          Nothing
**
*******************************************************************************/
void ce_t3t_handle_non_nfc_forum_cmd (tCE_CB *p_mem_cb, UINT8 cmd_id, BT_HDR *p_cmd_msg)
{
    tCE_T3T_MEM *p_cb = &p_mem_cb->mem.t3t;
    BT_HDR *p_rsp_msg;
    UINT8 *p_rsp_start;
    UINT8 *p_dst;
    UINT8 *p = (UINT8 *) (p_cmd_msg +1) + p_cmd_msg->offset;
    UINT16 sc;
    UINT8 rc;
    BOOLEAN send_response = TRUE;

    if ((p_rsp_msg = ce_t3t_get_rsp_buf ()) != NULL)
    {
        p_dst = p_rsp_start = (UINT8 *) (p_rsp_msg+1) + p_rsp_msg->offset;

        switch (cmd_id)
        {
        case T3T_MSG_OPC_POLL_CMD:
            /* Get system code and RC */
            /* Skip over sod and cmd_id */
            p+=2;
            BE_STREAM_TO_UINT16 (sc, p);
            STREAM_TO_UINT8 (rc, p);

            /* If requesting wildcard system code, or specifically our system code, then send POLL response */
            if ((sc == 0xFFFF) || (sc == p_cb->system_code))
            {
                /* Response Code */
                UINT8_TO_STREAM (p_dst, T3T_MSG_OPC_POLL_RSP);

                /* Manufacturer ID */
                ARRAY_TO_STREAM (p_dst, p_cb->local_nfcid2, NCI_RF_F_UID_LEN);

                /* Manufacturer Parameter PMm */
                ARRAY_TO_STREAM (p_dst, p_cb->local_pmm, NCI_T3T_PMM_LEN);

                /* If requesting system code */
                if (rc == T3T_POLL_RC_SC)
                {
                    UINT16_TO_BE_STREAM (p_dst, p_cb->system_code);
                }
            }
            else
            {
                send_response = FALSE;
            }
            break;


        case T3T_MSG_OPC_REQ_RESPONSE_CMD:
            /* Response Code */
            UINT8_TO_STREAM (p_dst, T3T_MSG_OPC_REQ_RESPONSE_RSP);

            /* Manufacturer ID */
            ARRAY_TO_STREAM (p_dst, p_cb->local_nfcid2, NCI_RF_F_UID_LEN);

            /* Mode */
            UINT8_TO_STREAM (p_dst, 0);
            break;

        case T3T_MSG_OPC_REQ_SYSTEMCODE_CMD:
            /* Response Code */
            UINT8_TO_STREAM (p_dst, T3T_MSG_OPC_REQ_SYSTEMCODE_RSP);

            /* Manufacturer ID */
            ARRAY_TO_STREAM (p_dst, p_cb->local_nfcid2, NCI_RF_F_UID_LEN);

            /* Number of system codes */
            UINT8_TO_STREAM (p_dst, 1);

            /* system codes */
            UINT16_TO_BE_STREAM (p_dst, T3T_SYSTEM_CODE_NDEF);
            break;


        case T3T_MSG_OPC_REQ_SERVICE_CMD:
        default:
            /* Unhandled command */
            CE_TRACE_ERROR1 ("Unhandled CE opcode: %02x", cmd_id);
            send_response = FALSE;
            break;
        }

        if (send_response)
        {
            p_rsp_msg->len = (UINT16) (p_dst - p_rsp_start);
            ce_t3t_send_to_lower (p_rsp_msg);
        }
        else
        {
            GKI_freebuf (p_rsp_msg);
        }
    }
    else
    {
        CE_TRACE_ERROR0 ("CE: Unable to allocat buffer for response message");
    }
    GKI_freebuf (p_cmd_msg);
}

/*******************************************************************************
**
** Function         ce_t3t_data_cback
**
** Description      This callback function receives the data from NFCC.
**
** Returns          none
**
*******************************************************************************/
void ce_t3t_data_cback (UINT8 conn_id, tNFC_DATA_CEVT *p_data)
{
    tCE_CB *p_ce_cb = &ce_cb;
    tCE_T3T_MEM *p_cb = &p_ce_cb->mem.t3t;
    BT_HDR *p_msg = p_data->p_data;
    tCE_DATA     ce_data;
    UINT8 cmd_id, bl0, entry_len, i;
    UINT8 *p_nfcid2 = NULL;
    UINT8 *p = (UINT8 *) (p_msg +1) + p_msg->offset;
    UINT8 cmd_nfcid2[NCI_RF_F_UID_LEN];
    UINT16 block_list_start_offset, remaining;
    BOOLEAN msg_processed = FALSE;
    BOOLEAN block_list_ok;
    UINT8 sod;
    UINT8 cmd_type;

#if (BT_TRACE_PROTOCOL == TRUE)
    DispT3TagMessage (p_msg, TRUE);
#endif

    /* If activate system code is not NDEF, or if no local NDEF contents was set, then pass data up to the app */
    if ((p_cb->system_code != T3T_SYSTEM_CODE_NDEF) || (!p_cb->ndef_info.initialized))
    {
        ce_data.raw_frame.status = p_data->status;
        ce_data.raw_frame.p_data = p_msg;
        p_ce_cb->p_cback (CE_T3T_RAW_FRAME_EVT, &ce_data);
        return;
    }

    /* Verify that message contains at least Sod and cmd_id */
    if (p_msg->len < 2)
    {
        CE_TRACE_ERROR1 ("CE: received invalid T3t message (invalid length: %i)", p_msg->len);
    }
    else
    {

        /* Get and validate command opcode */
        STREAM_TO_UINT8 (sod, p);
        STREAM_TO_UINT8 (cmd_id, p);

        /* Valid command and message length */
        cmd_type = ce_t3t_is_valid_opcode (cmd_id);
        if (cmd_type == CE_T3T_COMMAND_INVALID)
        {
            CE_TRACE_ERROR1 ("CE: received invalid T3t message (invalid command: 0x%02X)", cmd_id);
        }
        else if (cmd_type == CE_T3T_COMMAND_FELICA)
        {
            ce_t3t_handle_non_nfc_forum_cmd (p_ce_cb, cmd_id, p_msg);
            msg_processed = TRUE;
        }
        else
        {
            /* Verify that message contains at least NFCID2 and NUM services */
            if (p_msg->len < T3T_MSG_CMD_COMMON_HDR_LEN)
            {
                CE_TRACE_ERROR1 ("CE: received invalid T3t message (invalid length: %i)", p_msg->len);
            }
            else
            {
                /* Handle NFC_FORUM command (UPDATE or CHECK) */
                STREAM_TO_ARRAY (cmd_nfcid2, p, NCI_RF_F_UID_LEN);
                STREAM_TO_UINT8 (p_cb->cur_cmd.num_services, p);

                /* Calculate offset of block-list-start */
                block_list_start_offset = T3T_MSG_CMD_COMMON_HDR_LEN + 2*p_cb->cur_cmd.num_services + 1;

                if (p_cb->state == CE_T3T_STATE_NOT_ACTIVATED)
                {
                    CE_TRACE_ERROR2 ("CE: received command 0x%02X while in bad state (%i))", cmd_id, p_cb->state);
                }
                else if (memcmp (cmd_nfcid2, p_cb->local_nfcid2, NCI_RF_F_UID_LEN) != 0)
                {
                    CE_TRACE_ERROR0 ("CE: received invalid T3t message (invalid NFCID2)");
                    p_nfcid2 = cmd_nfcid2;      /* respond with ERROR using the NFCID2 from the command message */
                }
                else if (p_msg->len < block_list_start_offset)
                {
                    /* Does not have minimum (including number_of_blocks field) */
                    CE_TRACE_ERROR0 ("CE: incomplete message");
                }
                else
                {
                    /* Parse service code list */
                    for (i = 0; i < p_cb->cur_cmd.num_services; i++)
                    {
                        STREAM_TO_UINT16 (p_cb->cur_cmd.service_code_list[i], p);
                    }

                    /* Verify that block list */
                    block_list_ok = TRUE;
                    STREAM_TO_UINT8 (p_cb->cur_cmd.num_blocks, p);
                    remaining = p_msg->len - block_list_start_offset;
                    p_cb->cur_cmd.p_block_list_start = p;
                    for (i = 0; i < p_cb->cur_cmd.num_blocks; i++)
                    {
                        /* Each entry is at lease 2 bytes long */
                        if (remaining < 2)
                        {
                            /* Unexpected end of message (while reading block-list) */
                            CE_TRACE_ERROR0 ("CE: received invalid T3t message (unexpected end of block-list)");
                            block_list_ok = FALSE;
                            break;
                        }

                        /* Get byte0 of block-list entry */
                        bl0 = *p;

                        /* Validate service code index and size of block-list */
                        if ((bl0 & T3T_MSG_SERVICE_LIST_MASK) >= p_cb->cur_cmd.num_services)
                        {
                            /* Invalid service code */
                            CE_TRACE_ERROR1 ("CE: received invalid T3t message (invalid service index: %i)", (bl0 & T3T_MSG_SERVICE_LIST_MASK));
                            block_list_ok = FALSE;
                            break;
                        }
                        else if ((!(bl0 & T3T_MSG_MASK_TWO_BYTE_BLOCK_DESC_FORMAT)) && (remaining < 3))
                        {
                            /* Unexpected end of message (while reading 3-byte entry) */
                            CE_TRACE_ERROR0 ("CE: received invalid T3t message (unexpected end of block-list)");
                            block_list_ok = FALSE;
                            break;
                        }

                        /* Advance pointers to next block-list entry */
                        entry_len = (bl0 & T3T_MSG_MASK_TWO_BYTE_BLOCK_DESC_FORMAT) ? 2 : 3;
                        p+=entry_len;
                        remaining-=entry_len;
                    }

                    /* Block list is verified. Call CHECK or UPDATE handler */
                    if (block_list_ok)
                    {
                        p_cb->cur_cmd.p_block_data_start = p;
                        if (cmd_id == T3T_MSG_OPC_CHECK_CMD)
                        {
                            /* This is a CHECK command. Sanity check: there shouldn't be any more data remaining after reading block list */
                            if (remaining)
                            {
                                CE_TRACE_ERROR1 ("CE: unexpected data after after CHECK command (#i bytes)", remaining);
                            }
                            ce_t3t_handle_check_cmd (p_ce_cb, p_msg);
                            msg_processed = TRUE;
                        }
                        else
                        {
                            /* This is an UPDATE command. See if message contains all the expected block data */
                            if (remaining < p_cb->cur_cmd.num_blocks*T3T_MSG_BLOCKSIZE)
                            {
                                CE_TRACE_ERROR0 ("CE: unexpected end of block-data");
                            }
                            else
                            {
                                ce_t3t_handle_update_cmd (p_ce_cb, p_msg);
                                msg_processed = TRUE;
                            }
                        }
                    }
                }
            }
        }
    }

    if (!msg_processed)
    {
        ce_t3t_send_rsp (p_ce_cb, p_nfcid2, T3T_MSG_OPC_CHECK_RSP, T3T_MSG_RSP_STATUS_ERROR, T3T_MSG_RSP_STATUS2_ERROR_PROCESSING);
        GKI_freebuf (p_msg);
    }


}

/*******************************************************************************
**
** Function         ce_t3t_conn_cback
**
** Description      This callback function receives the events/data from NFCC.
**
** Returns          none
**
*******************************************************************************/
void ce_t3t_conn_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    tCE_T3T_MEM *p_cb = &ce_cb.mem.t3t;

    CE_TRACE_DEBUG2 ("ce_t3t_conn_cback: conn_id=%i, evt=%i", conn_id, event);

    switch (event)
    {
    case NFC_CONN_CREATE_CEVT:
        break;

    case NFC_CONN_CLOSE_CEVT:
        p_cb->state = CE_T3T_STATE_NOT_ACTIVATED;
        break;

    case NFC_DATA_CEVT:
        if (p_data->data.status == NFC_STATUS_OK)
        {
            ce_t3t_data_cback (conn_id, &p_data->data);
        }
        break;

    case NFC_DEACTIVATE_CEVT:
        p_cb->state = CE_T3T_STATE_NOT_ACTIVATED;
        NFC_SetStaticRfCback (NULL);
        break;

    default:
        break;

    }
}

/*******************************************************************************
**
** Function         ce_select_t3t
**
** Description      Select Type 3 Tag
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS ce_select_t3t (UINT16 system_code, UINT8 nfcid2[NCI_RF_F_UID_LEN])
{
    tCE_T3T_MEM *p_cb = &ce_cb.mem.t3t;

    CE_TRACE_DEBUG0 ("ce_select_t3t ()");

    p_cb->state = CE_T3T_STATE_IDLE;
    p_cb->system_code = system_code;
    memcpy (p_cb->local_nfcid2, nfcid2, NCI_RF_F_UID_LEN);

    NFC_SetStaticRfCback (ce_t3t_conn_cback);
    return NFC_STATUS_OK;
}


/*******************************************************************************
**
** Function         CE_T3tSetLocalNDEFMsg
**
** Description      Initialise CE Type 3 Tag with mandatory NDEF message
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T3tSetLocalNDEFMsg (BOOLEAN read_only,
                                   UINT32  size_max,
                                   UINT32  size_current,
                                   UINT8   *p_buf,
                                   UINT8   *p_scratch_buf)
{
    tCE_T3T_MEM *p_cb = &ce_cb.mem.t3t;

    CE_TRACE_API3 ("CE_T3tSetContent: ro=%i, size_max=%i, size_current=%i", read_only, size_max, size_current);

    /* Verify scratch buffer was provided if NDEF message is read/write */
    if ((!read_only) && (!p_scratch_buf))
    {
        CE_TRACE_ERROR0 ("CE_T3tSetLocalNDEFMsg (): p_scratch_buf cannot be NULL if not read-only");
        return NFC_STATUS_FAILED;
    }

    /* Check if disabling the local NDEF */
    if (!p_buf)
    {
        p_cb->ndef_info.initialized = FALSE;
    }
    /* Save ndef attributes */
    else
    {
        p_cb->ndef_info.initialized = TRUE;
        p_cb->ndef_info.ln = size_current;                          /* Current length */
        p_cb->ndef_info.nmaxb = (UINT16) ((size_max + 15) / T3T_MSG_BLOCKSIZE);  /* Max length (in blocks) */
        p_cb->ndef_info.rwflag = (read_only) ? T3T_MSG_NDEF_RWFLAG_RO : T3T_MSG_NDEF_RWFLAG_RW;
        p_cb->ndef_info.writef = T3T_MSG_NDEF_WRITEF_OFF;
        p_cb->ndef_info.version = 0x10;
        p_cb->ndef_info.p_buf = p_buf;
        p_cb->ndef_info.p_scratch_buf = p_scratch_buf;

        /* Initiate scratch buffer with same contents as read-buffer */
        if (p_scratch_buf)
        {
            p_cb->ndef_info.scratch_ln      = p_cb->ndef_info.ln;
            p_cb->ndef_info.scratch_writef  = T3T_MSG_NDEF_WRITEF_OFF;
            memcpy (p_scratch_buf, p_buf, p_cb->ndef_info.ln);
        }
    }

    return (NFC_STATUS_OK);
}

/*******************************************************************************
**
** Function         CE_T3tSetLocalNDefParams
**
** Description      Sets T3T-specific NDEF parameters. (Optional - if not
**                  called, then CE will use default parameters)
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T3tSetLocalNDefParams (UINT8 nbr, UINT8 nbw)
{
    tCE_T3T_MEM *p_cb = &ce_cb.mem.t3t;

    CE_TRACE_API2 ("CE_T3tSetLocalNDefParams: nbr=%i, nbw=%i", nbr, nbw);

    /* Validate */
    if ((nbr > T3T_MSG_NUM_BLOCKS_CHECK_MAX) || (nbw>T3T_MSG_NUM_BLOCKS_UPDATE_MAX) || (nbr < 1) || (nbw < 1))
    {
        CE_TRACE_ERROR0 ("CE_T3tSetLocalNDefParams: invalid params");
        return NFC_STATUS_FAILED;
    }

    p_cb->ndef_info.nbr = nbr;
    p_cb->ndef_info.nbw = nbw;

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         CE_T3tSendCheckRsp
**
** Description      Send CHECK response message
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T3tSendCheckRsp (UINT8 status1, UINT8 status2, UINT8 num_blocks, UINT8 *p_block_data)
{
    tCE_T3T_MEM *p_cb = &ce_cb.mem.t3t;
    tNFC_STATUS retval = NFC_STATUS_OK;
    BT_HDR *p_rsp_msg;
    UINT8 *p_dst, *p_rsp_start;

    CE_TRACE_API3 ("CE_T3tCheckRsp: status1=0x%02X, status2=0x%02X, num_blocks=%i", status1, status2, num_blocks);

    /* Validate num_blocks */
    if (num_blocks > T3T_MSG_NUM_BLOCKS_CHECK_MAX)
    {
        CE_TRACE_ERROR2 ("CE_T3tCheckRsp num_blocks (%i) exceeds maximum (%i)", num_blocks, T3T_MSG_NUM_BLOCKS_CHECK_MAX);
        return (NFC_STATUS_FAILED);
    }

    if ((p_rsp_msg = ce_t3t_get_rsp_buf ()) != NULL)
    {
        p_dst = p_rsp_start = (UINT8 *) (p_rsp_msg+1) + p_rsp_msg->offset;

        /* Response Code */
        UINT8_TO_STREAM (p_dst, T3T_MSG_OPC_CHECK_RSP);

        /* Manufacturer ID */
        ARRAY_TO_STREAM (p_dst, p_cb->local_nfcid2, NCI_RF_F_UID_LEN);

        /* Status1 and Status2 */
        UINT8_TO_STREAM (p_dst, status1);
        UINT8_TO_STREAM (p_dst, status2);

        if (status1 == T3T_MSG_RSP_STATUS_OK)
        {
            UINT8_TO_STREAM (p_dst, num_blocks);
            ARRAY_TO_STREAM (p_dst, p_block_data, (num_blocks * T3T_MSG_BLOCKSIZE));
        }

        p_rsp_msg->len = (UINT16) (p_dst - p_rsp_start);
        ce_t3t_send_to_lower (p_rsp_msg);
    }
    else
    {
        CE_TRACE_ERROR0 ("CE: Unable to allocate buffer for response message");
    }

    return (retval);
}

/*******************************************************************************
**
** Function         CE_T3tSendUpdateRsp
**
** Description      Send UPDATE response message
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS CE_T3tSendUpdateRsp (UINT8 status1, UINT8 status2)
{
    tNFC_STATUS retval = NFC_STATUS_OK;
    tCE_CB *p_ce_cb = &ce_cb;

    CE_TRACE_API2 ("CE_T3tUpdateRsp: status1=0x%02X, status2=0x%02X", status1, status2);
    ce_t3t_send_rsp (p_ce_cb, NULL, T3T_MSG_OPC_UPDATE_RSP, status1, status2);

    return (retval);
}

#endif /* NFC_INCLUDED == TRUE */
