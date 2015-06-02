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
 *  This file contains the implementation for Type 1 tag NDEF operation in
 *  Reader/Writer mode.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_target.h"

#if (NFC_INCLUDED == TRUE)
#include "nfc_api.h"
#include "nci_hmsgs.h"
#include "rw_api.h"
#include "rw_int.h"
#include "nfc_int.h"
#include "gki.h"

#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))

/* Local Functions */
static tNFC_STATUS rw_t1t_handle_rall_rsp (BOOLEAN *p_notify,UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_dyn_read_rsp (BOOLEAN *p_notify, UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_write_rsp (BOOLEAN *p_notify, UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_read_rsp (BOOLEAN *p_callback, UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_tlv_detect_rsp (UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_ndef_read_rsp (UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_ndef_write_rsp (UINT8 *p_data);
static tNFC_STATUS rw_t1t_handle_ndef_rall_rsp (void);
static tNFC_STATUS rw_t1t_ndef_write_first_block (void);
static tNFC_STATUS rw_t1t_next_ndef_write_block (void);
static tNFC_STATUS rw_t1t_send_ndef_byte (UINT8 data, UINT8 block, UINT8 index, UINT8 msg_len);
static tNFC_STATUS rw_t1t_send_ndef_block (UINT8 *p_data, UINT8 block);
static UINT8 rw_t1t_prepare_ndef_bytes (UINT8 *p_data, UINT8 *p_length_field, UINT8 *p_index, BOOLEAN b_one_byte, UINT8 block, UINT8 lengthfield_len);
static UINT8 rw_t1t_get_ndef_flags (void);
static UINT16 rw_t1t_get_ndef_max_size (void);
static BOOLEAN rw_t1t_is_lock_reserved_otp_byte (UINT16 index);
static BOOLEAN rw_t1t_is_read_only_byte (UINT16 index);
static UINT8 rw_t1t_get_lock_bits_for_segment (UINT8 segment,UINT8 *p_start_byte, UINT8 *p_start_bit,UINT8 *p_end_byte);
static void rw_t1t_update_attributes (void);
static void rw_t1t_update_lock_attributes (void);
static void rw_t1t_extract_lock_bytes (UINT8 *p_data);
static void rw_t1t_update_tag_state (void);

const UINT8 rw_t1t_mask_bits[8] =
{0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};

/*******************************************************************************
**
** Function         rw_t1t_handle_rsp
**
** Description      This function handles the response received for all commands
**                  sent to tag
**
** Returns          event to be sent to application
**
*******************************************************************************/
tRW_EVENT rw_t1t_handle_rsp (const tT1T_CMD_RSP_INFO * p_info, BOOLEAN *p_notify, UINT8 *p_data, tNFC_STATUS *p_status)
{
    tRW_EVENT   rw_event;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    UINT8       adds;

    if(  (p_t1t->state == RW_T1T_STATE_READ)
       ||(p_t1t->state == RW_T1T_STATE_WRITE)  )
    {
        return t1t_info_to_evt (p_info);
    }

    rw_event = rw_t1t_info_to_event (p_info);

    if (p_info->opcode == T1T_CMD_RALL)
    {
        *p_status = rw_t1t_handle_rall_rsp (p_notify,p_data);
    }
    else if (p_info->opcode == T1T_CMD_RSEG)
    {
        adds = *p_data;
        if (adds == 0)
        {
            p_t1t->b_rseg   = TRUE;
            rw_t1t_update_tag_state ();
            rw_t1t_update_attributes ();
            rw_t1t_update_lock_attributes ();
            memcpy (p_t1t->mem, (UINT8 *) (p_data + T1T_ADD_LEN), T1T_SEGMENT_SIZE);
        }
        *p_status = rw_t1t_handle_dyn_read_rsp (p_notify,p_data);
    }
    else if (p_info->opcode == T1T_CMD_READ8)
    {
        *p_status = rw_t1t_handle_dyn_read_rsp (p_notify,p_data);
    }
    else
    {
        *p_status = rw_t1t_handle_write_rsp (p_notify,p_data);
    }
    return rw_event;
}

/*******************************************************************************
**
** Function         rw_t1t_info_to_event
**
** Description      This function returns RW event code based on the current state
**
** Returns          RW event code
**
*******************************************************************************/
tRW_EVENT rw_t1t_info_to_event (const tT1T_CMD_RSP_INFO * p_info)
{
    tRW_EVENT   rw_event;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;

    switch (p_t1t->state)
    {
    case RW_T1T_STATE_TLV_DETECT:
        if (p_t1t->tlv_detect == TAG_NDEF_TLV)
            rw_event = RW_T1T_NDEF_DETECT_EVT;
        else
            rw_event = RW_T1T_TLV_DETECT_EVT;
        break;

    case RW_T1T_STATE_READ_NDEF:
        rw_event = RW_T1T_NDEF_READ_EVT;
        break;

    case RW_T1T_STATE_WRITE_NDEF:
        rw_event = RW_T1T_NDEF_WRITE_EVT;
        break;

    case RW_T1T_STATE_SET_TAG_RO:
        rw_event = RW_T1T_SET_TAG_RO_EVT;
        break;

    case RW_T1T_STATE_CHECK_PRESENCE:
        rw_event = RW_T1T_PRESENCE_CHECK_EVT;
        break;

    case RW_T1T_STATE_FORMAT_TAG:
        rw_event = RW_T1T_FORMAT_CPLT_EVT;
        break;

    default:
        rw_event = t1t_info_to_evt (p_info);
        break;
    }
    return rw_event;
}

/*******************************************************************************
**
** Function         rw_t1t_extract_lock_bytes
**
** Description      This function will extract lock bytes if any present in the
**                  response data
**
** Parameters       p_data: Data bytes in the response of RSEG/READ8/RALL command
**
** Returns          None
**
*******************************************************************************/
void rw_t1t_extract_lock_bytes (UINT8 *p_data)
{
    UINT16              end;
    UINT16              start;
    UINT8               num_locks;
    UINT16              lock_offset = 0;
    UINT16              offset;
    tRW_T1T_CB          *p_t1t          = &rw_cb.tcb.t1t;
    tT1T_CMD_RSP_INFO   *p_cmd_rsp_info = (tT1T_CMD_RSP_INFO *) rw_cb.tcb.t1t.p_cmd_rsp_info;

    num_locks = 0;
    /* Based on the Command used to read Tag, calculate the offset of the tag read */
    if (p_cmd_rsp_info->opcode == T1T_CMD_RSEG)
    {
        start = p_t1t->segment * T1T_SEGMENT_SIZE;
        end   = start + T1T_SEGMENT_SIZE;
    }
    else if (p_cmd_rsp_info->opcode == T1T_CMD_READ8)
    {
        start = p_t1t->block_read * T1T_BLOCK_SIZE;
        end   = start + T1T_BLOCK_SIZE;
    }
    else if (p_cmd_rsp_info->opcode == T1T_CMD_RALL)
    {
        start = 0;
        end   = T1T_STATIC_SIZE;
    }
    else
        return;

    /* Collect lock bytes that are present in the part of the data read from Tag */
    while (num_locks < p_t1t->num_lockbytes)
    {
        if (p_t1t->lockbyte[num_locks].b_lock_read == FALSE)
        {
            /* Get the exact offset of the dynamic lock byte in the tag */
            offset = p_t1t->lock_tlv[p_t1t->lockbyte[num_locks].tlv_index].offset + p_t1t->lockbyte[num_locks].byte_index;
            if (  (offset <  end)
                &&(offset >= start)  )

            {
                /* This dynamic lock byte is in the response */
                if (p_cmd_rsp_info->opcode == T1T_CMD_RSEG)
                {
                    lock_offset = (offset % T1T_SEGMENT_SIZE);
                }
                else if (p_cmd_rsp_info->opcode == T1T_CMD_READ8)
                {
                    lock_offset = (offset % T1T_BLOCK_SIZE);
                }
                else if (p_cmd_rsp_info->opcode == T1T_CMD_RALL)
                {
                    lock_offset = offset;
                }

                p_t1t->lockbyte[num_locks].lock_byte    = p_data[lock_offset];
                p_t1t->lockbyte[num_locks].b_lock_read  = TRUE;
            }
            else
                break;
        }
        num_locks++;
    }
}

/*******************************************************************************
**
** Function         rw_t1t_update_tag_attributes
**
** Description      This function will update tag attributes based on cc, ndef
**                  message length
**
** Returns          None
**
*******************************************************************************/
void rw_t1t_update_tag_state (void)
{
    tRW_T1T_CB  *p_t1t          = &rw_cb.tcb.t1t;

    /* Set Tag state based on CC value and NDEF Message length */
    if (  ((p_t1t->mem[T1T_CC_NMN_BYTE] == T1T_CC_NMN) || (p_t1t->mem[T1T_CC_NMN_BYTE] == 0))
        &&((p_t1t->mem[T1T_CC_VNO_BYTE] == T1T_CC_VNO) || (p_t1t->mem[T1T_CC_VNO_BYTE] == T1T_CC_LEGACY_VNO))
        &&((p_t1t->mem[T1T_CC_RWA_BYTE] == T1T_CC_RWA_RW) || (p_t1t->mem[T1T_CC_RWA_BYTE] == T1T_CC_RWA_RO))  )
    {
        /* Valid CC value, so Tag is initialized */
        if (p_t1t->ndef_msg_len > 0)
        {
            if (p_t1t->mem[T1T_CC_RWA_BYTE] == T1T_CC_RWA_RO)
            {
                /* NDEF Message presence, CC indication sets Tag as READ ONLY  */
                p_t1t->tag_attribute = RW_T1_TAG_ATTRB_READ_ONLY;
            }
            else if (p_t1t->mem[T1T_CC_RWA_BYTE] == T1T_CC_RWA_RW)
            {
                /* NDEF Message presence, CC indication sets Tag as READ WRITE */
                p_t1t->tag_attribute = RW_T1_TAG_ATTRB_READ_WRITE;
            }
        }
        else
        {
            /* If NDEF is not yet detected then Tag remains in Initialized state
            *  after NDEF Detection the Tag state may be updated */
            p_t1t->tag_attribute = RW_T1_TAG_ATTRB_INITIALIZED;
        }
    }
    else
    {
        p_t1t->tag_attribute = RW_T1_TAG_ATTRB_UNKNOWN;
    }
}

/*******************************************************************************
**
** Function         rw_t1t_read_locks
**
** Description      This function will send command to read next unread locks
**
** Returns          NFC_STATUS_OK, if all locks are read successfully
**                  NFC_STATUS_FAILED, if reading locks failed
**                  NFC_STATUS_CONTINUE, if reading locks is in progress
**
*******************************************************************************/
tNFC_STATUS rw_t1t_read_locks (void)
{
    UINT8       num_locks   = 0;
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    tNFC_STATUS status      = NFC_STATUS_CONTINUE;
    UINT16      offset;

    while (num_locks < p_t1t->num_lockbytes)
    {
        if (p_t1t->lockbyte[num_locks].b_lock_read == FALSE)
        {
            offset = p_t1t->lock_tlv[p_t1t->lockbyte[num_locks].tlv_index].offset + p_t1t->lockbyte[num_locks].byte_index;
            if (offset < T1T_STATIC_SIZE)
            {
               p_t1t->lockbyte[num_locks].lock_byte   = p_t1t->mem[offset];
               p_t1t->lockbyte[num_locks].b_lock_read = TRUE;
            }
            else if (offset < (p_t1t->mem[T1T_CC_TMS_BYTE] + 1) * T1T_BLOCK_SIZE)
            {
                /* send READ8 command */
                p_t1t->block_read = (UINT8) (offset/T1T_BLOCK_SIZE);
                if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_READ8, p_t1t->block_read, NULL)) == NFC_STATUS_OK)
                {
                    /* Reading Locks */
                    status          = NFC_STATUS_CONTINUE;
                    p_t1t->substate = RW_T1T_SUBSTATE_WAIT_READ_LOCKS;
                }
                break;
            }
            else
            {
                /* Read locks failed */
                status = NFC_STATUS_FAILED;
                break;
            }
        }
        num_locks++;
    }
    if (num_locks == p_t1t->num_lockbytes)
    {
        /* All locks are read */
        status = NFC_STATUS_OK;
    }

    return status;
}

/*******************************************************************************
**
** Function         rw_t1t_handle_write_rsp
**
** Description      This function handles response received for WRITE_E8,
**                  WRITE_NE8, WRITE_E, WRITE_NE commands
**
** Returns          status of the current NDEF/TLV Operation
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_handle_write_rsp (BOOLEAN *p_notify, UINT8 *p_data)
{
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    tNFC_STATUS status      = NFC_STATUS_OK;
    UINT8       num_locks;
    UINT8       lock_count;
    UINT8       value;
    UINT8       addr;
    UINT8       write_block[T1T_BLOCK_SIZE];
    UINT16      offset;
    UINT16      next_offset;
    UINT8       num_bits;
    UINT8       next_num_bits;

    *p_notify = FALSE;

    switch (p_t1t->state)
    {
    case RW_T1T_STATE_WRITE:
        *p_notify = TRUE;
        break;

    case RW_T1T_STATE_FORMAT_TAG:
        if (p_t1t->substate == RW_T1T_SUBSTATE_WAIT_SET_NULL_NDEF)
        {
            if (rw_cb.tcb.t1t.hr[0] != T1T_STATIC_HR0 || rw_cb.tcb.t1t.hr[1] >= RW_T1T_HR1_MIN)
                *p_notify = TRUE;
            else
            {
                if (p_t1t->work_offset < (T1T_BLOCK_SIZE - 1))
                {
                    p_t1t->work_offset++;
                    /* send WRITE-E command */
                    RW_T1T_BLD_ADD ((addr), 1, (UINT8) p_t1t->work_offset);

                    if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, p_t1t->ndef_first_block[(UINT8) p_t1t->work_offset])) != NFC_STATUS_OK)
                        *p_notify = TRUE;
                }
                else
                    *p_notify = TRUE;
            }

        }
        else
        {
            /* send WRITE-E8 command */
            if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_E8, 2, p_t1t->ndef_final_block)) != NFC_STATUS_OK)
                *p_notify = TRUE;
            else
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_NULL_NDEF;
        }
        break;

    case RW_T1T_STATE_SET_TAG_RO:
        switch (p_t1t->substate)
        {
        case RW_T1T_SUBSTATE_WAIT_SET_CC_RWA_RO:

            if (!p_t1t->b_hard_lock)
            {
                status    = NFC_STATUS_OK;
                *p_notify = TRUE;
                break;
            }

            if ((p_t1t->hr[0] & 0x0F) != 1)
            {
                memset (write_block,0,T1T_BLOCK_SIZE);
                write_block[0] = 0xFF;
                write_block[1] = 0xFF;

                /* send WRITE-NE8 command */
                if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_NE8, T1T_LOCK_BLOCK, write_block)) != NFC_STATUS_OK)
                    *p_notify       = TRUE;
                else
                    p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS;
            }
            else
            {
                /* send WRITE-NE command */
                RW_T1T_BLD_ADD ((addr), (T1T_LOCK_BLOCK), (0));
                if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_NE, addr, 0xFF)) != NFC_STATUS_OK)
                    *p_notify       = TRUE;
                else
                    p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_ST_LOCK_BITS;
            }
            break;

        case RW_T1T_SUBSTATE_WAIT_SET_ST_LOCK_BITS:

            /* send WRITE-NE command */
            RW_T1T_BLD_ADD ((addr), (T1T_LOCK_BLOCK), (1));
            if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_NE, addr, 0xFF)) != NFC_STATUS_OK)
                *p_notify       = TRUE;
            else
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS;

            break;

        case RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS:
            num_locks = 0;
            while (num_locks < p_t1t->num_lockbytes)
            {
                if (p_t1t->lockbyte[num_locks].lock_status == RW_T1T_LOCK_UPDATE_INITIATED)
                {
                    p_t1t->lockbyte[num_locks].lock_status = RW_T1T_LOCK_UPDATED;
                }
                num_locks++;
            }

            num_locks = 0;
            while (num_locks < p_t1t->num_lockbytes)
            {
                if (p_t1t->lockbyte[num_locks].lock_status == RW_T1T_LOCK_NOT_UPDATED)
                {
                    offset = p_t1t->lock_tlv[p_t1t->lockbyte[num_locks].tlv_index].offset + p_t1t->lockbyte[num_locks].byte_index;
                    num_bits = ((p_t1t->lockbyte[num_locks].byte_index + 1)* 8 <= p_t1t->lock_tlv[p_t1t->lockbyte[num_locks].tlv_index].num_bits) ? 8 : p_t1t->lock_tlv[p_t1t->lockbyte[num_locks].tlv_index].num_bits % 8;

                    if ((p_t1t->hr[0] & 0x0F) != 1)
                    {
                        memset (write_block,0,T1T_BLOCK_SIZE);

                        write_block[(UINT8) (offset%T1T_BLOCK_SIZE)] |=  tags_pow (2,num_bits) - 1;
                        lock_count = num_locks + 1;
                        while (lock_count < p_t1t->num_lockbytes)
                        {
                            next_offset = p_t1t->lock_tlv[p_t1t->lockbyte[lock_count].tlv_index].offset + p_t1t->lockbyte[lock_count].byte_index;
                            next_num_bits = ((p_t1t->lockbyte[lock_count].byte_index + 1)* 8 <= p_t1t->lock_tlv[p_t1t->lockbyte[lock_count].tlv_index].num_bits) ? 8 : p_t1t->lock_tlv[p_t1t->lockbyte[lock_count].tlv_index].num_bits % 8;

                            if (next_offset/T1T_BLOCK_SIZE == offset/T1T_BLOCK_SIZE)
                            {
                                write_block[(UINT8) (next_offset%T1T_BLOCK_SIZE)] |=  tags_pow (2,next_num_bits) - 1;
                            }
                            else
                                break;
                            lock_count ++;
                        }

                        /* send WRITE-NE8 command */
                        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_NE8, (UINT8) (offset/T1T_BLOCK_SIZE), write_block)) == NFC_STATUS_OK)
                        {
                            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS;
                            while (lock_count >  num_locks)
                            {
                                p_t1t->lockbyte[lock_count - 1].lock_status = RW_T1T_LOCK_UPDATE_INITIATED;
                                lock_count --;
                            }
                        }
                        else
                            *p_notify       = TRUE;
                    }
                    else
                    {
                        /* send WRITE-NE command */
                        RW_T1T_BLD_ADD ((addr), ((UINT8) (offset/T1T_BLOCK_SIZE)), ((UINT8) (offset%T1T_BLOCK_SIZE)));
                        value = (UINT8) (tags_pow (2,num_bits) - 1);
                        if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_NE, addr, value)) == NFC_STATUS_OK)
                        {
                            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS;
                            p_t1t->lockbyte[num_locks].lock_status = RW_T1T_LOCK_UPDATE_INITIATED;
                        }
                        else
                            *p_notify       = TRUE;
                    }
                    break;
                }
                num_locks++;
            }
            if (num_locks == p_t1t->num_lockbytes)
            {
                rw_t1t_update_lock_attributes ();
                status    = NFC_STATUS_OK;
                *p_notify = TRUE;
            }
            break;
        }
        break;

    case RW_T1T_STATE_WRITE_NDEF:
        switch (p_t1t->substate)
        {
        case RW_T1T_SUBSTATE_WAIT_VALIDATE_NDEF:
            p_t1t->ndef_msg_len  = p_t1t->new_ndef_msg_len;
            p_t1t->tag_attribute = RW_T1_TAG_ATTRB_READ_WRITE;
            *p_notify = TRUE;
            break;

        case RW_T1T_SUBSTATE_WAIT_NDEF_UPDATED:
            status      = rw_t1t_handle_ndef_write_rsp (p_data);
            if (status == NFC_STATUS_OK)
            {
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_VALIDATE_NDEF;
            }
            else if (status == NFC_STATUS_FAILED)
            {
                /* Send Negative response to upper layer */
                *p_notify       = TRUE;
            }
            break;

        case RW_T1T_SUBSTATE_WAIT_NDEF_WRITE:
            status = rw_t1t_handle_ndef_write_rsp (p_data);

            if (status == NFC_STATUS_FAILED)
            {
                /* Send Negative response to upper layer */
                *p_notify       = TRUE;
            }
            else if (status == NFC_STATUS_OK)
            {
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_NDEF_UPDATED;
            }
            break;

        case RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF:
            status = rw_t1t_handle_ndef_write_rsp (p_data);
            if (status == NFC_STATUS_FAILED)
            {
                /* Send Negative response to upper layer */
                *p_notify   = TRUE;
            }
            else if (status == NFC_STATUS_CONTINUE)
            {
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_NDEF_WRITE;
            }
            else
            {
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_NDEF_UPDATED;
            }
            break;
        }
        break;
    }
    return status;
}

/*******************************************************************************
**
** Function         rw_t1t_handle_read_rsp
**
** Description      This function handle the data bytes excluding ADD(S)/ADD8 field
**                  received as part of RSEG, RALL, READ8 command response
**
** Returns          status of the current NDEF/TLV Operation
**
*******************************************************************************/
tNFC_STATUS rw_t1t_handle_read_rsp (BOOLEAN *p_notify, UINT8 *p_data)
{
    tRW_T1T_CB              *p_t1t  = &rw_cb.tcb.t1t;
    tNFC_STATUS             status  = NFC_STATUS_OK;
    tRW_DETECT_NDEF_DATA    ndef_data;
    tRW_DETECT_TLV_DATA     tlv_data;
    UINT8                   count;
    tRW_READ_DATA           evt_data;

    *p_notify = FALSE;
    /* Handle the response based on the current state */
    switch (p_t1t->state)
    {
    case RW_T1T_STATE_READ:
        *p_notify = TRUE;
        break;

    case RW_T1T_STATE_READ_NDEF:
        status = rw_t1t_handle_ndef_rall_rsp ();
        if (status != NFC_STATUS_CONTINUE)
        {
            evt_data.status = status;
            evt_data.p_data = NULL;
            rw_t1t_handle_op_complete ();
            (*rw_cb.p_cback) (RW_T1T_NDEF_READ_EVT, (tRW_DATA *) &evt_data);
        }
        break;

    case RW_T1T_STATE_TLV_DETECT:
        switch (p_t1t->substate)
        {
        case RW_T1T_SUBSTATE_WAIT_READ_LOCKS:
            status = rw_t1t_read_locks ();
            if (status != NFC_STATUS_CONTINUE)
            {
                rw_t1t_update_lock_attributes ();
                /* Send positive response to upper layer */
                if (p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV)
                {
                    tlv_data.protocol   = NFC_PROTOCOL_T1T;
                    tlv_data.num_bytes  = p_t1t->num_lockbytes;
                    tlv_data.status = status;
                    rw_t1t_handle_op_complete ();
                    (*rw_cb.p_cback) (RW_T1T_TLV_DETECT_EVT, (tRW_DATA *) &tlv_data);
                }
                else if (p_t1t->tlv_detect == TAG_NDEF_TLV)
                {
                    ndef_data.protocol  = NFC_PROTOCOL_T1T;
                    ndef_data.flags     = rw_t1t_get_ndef_flags ();
                    ndef_data.flags    |= RW_NDEF_FL_FORMATED;
                    ndef_data.max_size  = (UINT32) rw_t1t_get_ndef_max_size ();
                    ndef_data.cur_size  = p_t1t->ndef_msg_len;

                    if (ndef_data.max_size  < ndef_data.cur_size)
                    {
                        ndef_data.flags    |= RW_NDEF_FL_READ_ONLY;
                        ndef_data.max_size  = ndef_data.cur_size;
                    }

                    if (!(ndef_data.flags & RW_NDEF_FL_READ_ONLY))
                    {
                        ndef_data.flags    |= RW_NDEF_FL_SOFT_LOCKABLE;
                        if (status == NFC_STATUS_OK)
                            ndef_data.flags    |= RW_NDEF_FL_HARD_LOCKABLE;
                    }
                    ndef_data.status = status;
                    rw_t1t_handle_op_complete ();
                    (*rw_cb.p_cback) (RW_T1T_NDEF_DETECT_EVT, (tRW_DATA *)&ndef_data);
                }
            }
            break;

        case RW_T1T_SUBSTATE_NONE:
            if (p_t1t->tlv_detect == TAG_MEM_CTRL_TLV)
            {
                tlv_data.status    = rw_t1t_handle_tlv_detect_rsp (p_t1t->mem);
                tlv_data.protocol  = NFC_PROTOCOL_T1T;
                tlv_data.num_bytes = 0;
                count              = 0;
                while (count < p_t1t->num_mem_tlvs)
                {
                    tlv_data.num_bytes += p_t1t->mem_tlv[p_t1t->num_mem_tlvs].num_bytes;
                    count++;
                }
                rw_t1t_handle_op_complete ();
                /* Send response to upper layer */
                (*rw_cb.p_cback) (RW_T1T_TLV_DETECT_EVT, (tRW_DATA *) &tlv_data);
            }
            else if (p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV)
            {
                tlv_data.status    = rw_t1t_handle_tlv_detect_rsp (p_t1t->mem);
                tlv_data.protocol  = NFC_PROTOCOL_T1T;
                tlv_data.num_bytes = p_t1t->num_lockbytes;

                if (tlv_data.status == NFC_STATUS_FAILED)
                {
                    rw_t1t_handle_op_complete ();

                    /* Send Negative response to upper layer */
                    (*rw_cb.p_cback) (RW_T1T_TLV_DETECT_EVT, (tRW_DATA *)&tlv_data);
                }
                else
                {
                    rw_t1t_extract_lock_bytes (p_data);
                    status = rw_t1t_read_locks ();
                    if (status != NFC_STATUS_CONTINUE)
                    {
                        /* Send positive response to upper layer */
                        tlv_data.status = status;
                        rw_t1t_handle_op_complete ();

                        (*rw_cb.p_cback) (RW_T1T_TLV_DETECT_EVT, (tRW_DATA *) &tlv_data);
                    }
                }
            }
            else if (p_t1t->tlv_detect == TAG_NDEF_TLV)
            {
                ndef_data.protocol  = NFC_PROTOCOL_T1T;
                ndef_data.flags     = rw_t1t_get_ndef_flags ();

                if (p_t1t->mem[T1T_CC_NMN_BYTE] == T1T_CC_NMN)
                {
                    ndef_data.status    = rw_t1t_handle_tlv_detect_rsp (p_t1t->mem);

                    ndef_data.cur_size  = p_t1t->ndef_msg_len;
                    if (ndef_data.status == NFC_STATUS_FAILED)
                    {
                        ndef_data.max_size  = (UINT32) rw_t1t_get_ndef_max_size ();
                        if (ndef_data.max_size  < ndef_data.cur_size)
                        {
                            ndef_data.flags    |= RW_NDEF_FL_READ_ONLY;
                            ndef_data.max_size  = ndef_data.cur_size;
                        }
                        if (!(ndef_data.flags & RW_NDEF_FL_READ_ONLY))
                        {
                            ndef_data.flags    |= RW_NDEF_FL_SOFT_LOCKABLE;
                        }
                        /* Send Negative response to upper layer */
                        rw_t1t_handle_op_complete ();

                        (*rw_cb.p_cback) (RW_T1T_NDEF_DETECT_EVT, (tRW_DATA *) &ndef_data);
                    }
                    else
                    {
                        ndef_data.flags    |= RW_NDEF_FL_FORMATED;
                        rw_t1t_extract_lock_bytes (p_data);
                        status = rw_t1t_read_locks ();
                        if (status != NFC_STATUS_CONTINUE)
                        {
                            ndef_data.max_size  = (UINT32) rw_t1t_get_ndef_max_size ();
                            if (ndef_data.max_size  < ndef_data.cur_size)
                            {
                                ndef_data.flags    |= RW_NDEF_FL_READ_ONLY;
                                ndef_data.max_size  = ndef_data.cur_size;
                            }

                            if (!(ndef_data.flags & RW_NDEF_FL_READ_ONLY))
                            {
                                ndef_data.flags    |= RW_NDEF_FL_SOFT_LOCKABLE;
                                if (status == NFC_STATUS_OK)
                                    ndef_data.flags    |= RW_NDEF_FL_HARD_LOCKABLE;
                            }
                            /* Send positive response to upper layer */
                            ndef_data.status = status;
                            rw_t1t_handle_op_complete ();

                            (*rw_cb.p_cback) (RW_T1T_NDEF_DETECT_EVT, (tRW_DATA *)&ndef_data);
                        }
                    }
                }
                else
                {
                    /* Send Negative response to upper layer */
                    ndef_data.status    = NFC_STATUS_FAILED;
                    ndef_data.max_size  = (UINT32) rw_t1t_get_ndef_max_size ();
                    ndef_data.cur_size  = p_t1t->ndef_msg_len;
                    if (ndef_data.max_size  < ndef_data.cur_size)
                    {
                        ndef_data.flags    |= RW_NDEF_FL_READ_ONLY;
                        ndef_data.max_size  = ndef_data.cur_size;
                    }
                    if (!(ndef_data.flags & RW_NDEF_FL_READ_ONLY))
                    {
                        ndef_data.flags    |= RW_NDEF_FL_SOFT_LOCKABLE;
                        ndef_data.flags    |= RW_NDEF_FL_SOFT_LOCKABLE;
                    }
                    rw_t1t_handle_op_complete ();

                    (*rw_cb.p_cback) (RW_T1T_NDEF_DETECT_EVT, (tRW_DATA *) &ndef_data);
                }
            }
            break;
        }
        break;
    }
    return status;
}

/*******************************************************************************
**
** Function         rw_t1t_handle_dyn_read_rsp
**
** Description      This function handles response received for READ8, RSEG
**                  commands based on the current state
**
** Returns          status of the current NDEF/TLV Operation
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_handle_dyn_read_rsp (BOOLEAN *p_notify, UINT8 *p_data)
{
    tNFC_STATUS status  = NFC_STATUS_OK;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;

    *p_notify = FALSE;

    p_data += T1T_ADD_LEN;

    rw_t1t_extract_lock_bytes (p_data);

    if (p_t1t->state == RW_T1T_STATE_READ_NDEF)
    {
        status = rw_t1t_handle_ndef_read_rsp (p_data);
        if (  (status == NFC_STATUS_FAILED)
            ||(status == NFC_STATUS_OK)  )
        {
            /* Send response to upper layer */
            *p_notify = TRUE;
        }
    }
    else if (p_t1t->state == RW_T1T_STATE_WRITE_NDEF)
    {
        status = rw_t1t_handle_ndef_write_rsp (p_data);
        if (status == NFC_STATUS_FAILED)
        {
            /* Send response to upper layer */
            *p_notify = TRUE;
        }
        else if (status == NFC_STATUS_CONTINUE)
        {
            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF;
        }
    }
    else
    {
        status = rw_t1t_handle_read_rsp (p_notify, p_data);
    }
    return status;
}

/*****************************************************************************
**
** Function         rw_t1t_handle_rall_rsp
**
** Description      Handle response to RALL - Collect CC, set Tag state
**
** Returns          None
**
*****************************************************************************/
static tNFC_STATUS rw_t1t_handle_rall_rsp (BOOLEAN *p_notify,UINT8 *p_data)
{
    tRW_T1T_CB  *p_t1t   = &rw_cb.tcb.t1t;

    p_data      += T1T_HR_LEN; /* skip HR */
    memcpy (p_t1t->mem, (UINT8 *) p_data, T1T_STATIC_SIZE);
    p_t1t->segment  = 0;
    rw_t1t_extract_lock_bytes (p_data);

    p_data      += T1T_UID_LEN + T1T_RES_BYTE_LEN; /* skip Block 0, UID and Reserved byte */

    RW_TRACE_DEBUG0 ("rw_t1t_handle_rall_rsp ()");

    rw_t1t_update_tag_state ();
    rw_t1t_update_attributes ();
    rw_t1t_update_lock_attributes ();
    p_t1t->b_update = TRUE;
    return (rw_t1t_handle_read_rsp (p_notify, p_t1t->mem));
}

/*******************************************************************************
**
** Function         rw_t1t_handle_tlv_detect_rsp
**
** Description      Handle response to the last command sent while
**                  detecting tlv
**
** Returns          NFC_STATUS_OK, if tlv detect is complete & success
**                  NFC_STATUS_FAILED,if tlv detect failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_handle_tlv_detect_rsp (UINT8 *p_data)
{
    UINT16      offset;
    UINT16      len;
    UINT8       xx;
    UINT8       *p_readbytes;
    UINT8       index;
    UINT8       tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
    UINT8       found_tlv = TAG_NULL_TLV;
    tRW_T1T_CB  *p_t1t          = &rw_cb.tcb.t1t;
    BOOLEAN     failed          = FALSE;
    BOOLEAN     found           = FALSE;
    UINT8       count           = 0;
    tNFC_STATUS status          = NFC_STATUS_FAILED;
    UINT8       start_offset    = T1T_UID_LEN + T1T_CC_LEN + T1T_RES_BYTE_LEN;
    UINT8       end_offset      = T1T_STATIC_SIZE - (2*T1T_BLOCK_SIZE);
    UINT8       bytes_read      = T1T_STATIC_SIZE;
    UINT8       tlv_value[T1T_DEFAULT_TLV_LEN];
    UINT16      bytes_count = 0;

    p_readbytes = p_data;

    for (offset = start_offset; offset < end_offset  && !failed && !found;)
    {
        if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) (offset)) == TRUE)
        {
            offset++;
            continue;
        }
        switch (tlv_detect_state)
        {
        case RW_T1T_SUBSTATE_WAIT_TLV_DETECT:
            /* Search for the tag */
            found_tlv = p_readbytes[offset++];
            switch (found_tlv)
            {
            case TAG_NULL_TLV:         /* May be used for padding. SHALL ignore this */
                break;

            case TAG_NDEF_TLV:
                if (p_t1t->tlv_detect == TAG_NDEF_TLV)
                {
                    index = (offset % T1T_BLOCK_SIZE);
                    /* Backup ndef first block */
                    memcpy (&p_t1t->ndef_first_block[0],&p_readbytes[offset-index],index);
                    memcpy (&p_t1t->ndef_first_block[index],&p_readbytes[offset],T1T_BLOCK_SIZE - index);
                    tlv_detect_state = RW_T1T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN;
                }
                else if (p_t1t->tlv_detect == TAG_PROPRIETARY_TLV)
                {
                    tlv_detect_state = RW_T1T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN;
                }
                else if (  ((p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV) && (p_t1t->num_lockbytes > 0))
                         ||((p_t1t->tlv_detect == TAG_MEM_CTRL_TLV) && (p_t1t->num_mem_tlvs > 0))  )
                {
                    found = TRUE;
                }
                else
                {
                    failed = TRUE;
                }
                break;

            case TAG_LOCK_CTRL_TLV:
            case TAG_MEM_CTRL_TLV:
                tlv_detect_state = RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN0;
                break;

            case TAG_PROPRIETARY_TLV:
                if (p_t1t->tlv_detect == TAG_PROPRIETARY_TLV)
                {
                    index = (offset % T1T_BLOCK_SIZE);
                    /* Backup ndef first block */
                    tlv_detect_state = RW_T1T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN;
                }
                else
                {
                    /* NDEF/LOCK/MEM TLV can exist after Proprietary Tlv so we continue searching, skiping proprietary tlv */
                    tlv_detect_state = RW_T1T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN;
                }
                break;

            case TAG_TERMINATOR_TLV:   /* Last TLV block in the data area. Must be no NDEF nessage */
                if (  ((p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV) && (p_t1t->num_lockbytes > 0))
                    ||((p_t1t->tlv_detect == TAG_MEM_CTRL_TLV) && (p_t1t->num_mem_tlvs > 0))  )
                {
                    found = TRUE;
                }
                else
                {
                    failed = TRUE;
                }
                break;
            default:
                failed = TRUE;
            }
            break;

        case RW_T1T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN:
            len = p_readbytes[offset];
            switch (found_tlv)
            {
            case TAG_NDEF_TLV:
                p_t1t->ndef_header_offset = offset + p_t1t->work_offset;
                if (len == T1T_LONG_NDEF_LEN_FIELD_BYTE0)
                {
                    /* The next two bytes constitute length bytes */
                    tlv_detect_state     = RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN0;
                }
                else
                {
                    /* one byte length field */
                    p_t1t->ndef_msg_len = len;
                    bytes_count  = p_t1t->ndef_msg_len;
                    tlv_detect_state     = RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE;
                }
                break;

            case TAG_PROPRIETARY_TLV:
                if (len == 0xFF)
                {
                    /* The next two bytes constitute length bytes */
                    tlv_detect_state     = RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN0;
                }
                else
                {
                    /* one byte length field */
                    bytes_count  = len;
                    tlv_detect_state     = RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE;
                }
                break;
            }
            offset++;
            break;

        case RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN0:
            switch (found_tlv)
            {
            case TAG_LOCK_CTRL_TLV:
            case TAG_MEM_CTRL_TLV:

                len = p_readbytes[offset];
                if (len == T1T_DEFAULT_TLV_LEN)
                {
                    /* Valid Lock control TLV */
                    tlv_detect_state = RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE;
                    bytes_count = T1T_DEFAULT_TLV_LEN;
                }
                else if (  ((p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV) && (p_t1t->num_lockbytes > 0))
                         ||((p_t1t->tlv_detect == TAG_MEM_CTRL_TLV) && (p_t1t->num_mem_tlvs > 0))  )
                {
                    found = TRUE;
                }
                else
                {
                    failed = TRUE;
                }
                break;

            case TAG_NDEF_TLV:
            case TAG_PROPRIETARY_TLV:
                /* The first length byte */
                bytes_count  = (UINT8) p_readbytes[offset];
                tlv_detect_state     = RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN1;
                break;
            }
            offset++;
            break;

        case RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN1:
            bytes_count  = (bytes_count << 8) + p_readbytes[offset];
            if (found_tlv == TAG_NDEF_TLV)
            {
                p_t1t->ndef_msg_len = bytes_count;
            }
            tlv_detect_state     = RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE;
            offset++;
            break;

        case RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE:
            switch (found_tlv)
            {
            case TAG_NDEF_TLV:
                if ((bytes_count == p_t1t->ndef_msg_len) && (p_t1t->tlv_detect == TAG_NDEF_TLV))
                {
                    /* The first byte offset after length field */
                    p_t1t->ndef_msg_offset = offset + p_t1t->work_offset;
                }
                if (bytes_count > 0)
                    bytes_count--;

                if (p_t1t->tlv_detect == TAG_NDEF_TLV)
                {
                    if (p_t1t->ndef_msg_len > 0)
                    {
                        rw_t1t_update_tag_state ();
                    }
                    else
                    {
                        p_t1t->tag_attribute = RW_T1_TAG_ATTRB_INITIALIZED_NDEF;
                    }
                    found = TRUE;
                }
                else if (bytes_count == 0)
                {
                    tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
                }
                break;

            case TAG_LOCK_CTRL_TLV:
                bytes_count--;
                if (  (p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV)
                    ||(p_t1t->tlv_detect == TAG_NDEF_TLV)  )
                {
                    tlv_value[2 - bytes_count] = p_readbytes[offset];
                    if (bytes_count == 0)
                    {
                        if (p_t1t->num_lock_tlvs < RW_T1T_MAX_LOCK_TLVS)
                        {
                            p_t1t->lock_tlv[p_t1t->num_lock_tlvs].offset   = (tlv_value[0] >> 4) & 0x0F;
                            p_t1t->lock_tlv[p_t1t->num_lock_tlvs].offset  *= (UINT8) tags_pow (2, tlv_value[2] & 0x0F);
                            p_t1t->lock_tlv[p_t1t->num_lock_tlvs].offset  += tlv_value[0] & 0x0F;
                            p_t1t->lock_tlv[p_t1t->num_lock_tlvs].bytes_locked_per_bit = (UINT8) tags_pow (2, ((tlv_value[2] & 0xF0) >> 4));
                            p_t1t->lock_tlv[p_t1t->num_lock_tlvs].num_bits = tlv_value[1];
                            count = tlv_value[1] / 8 + ((tlv_value[1] % 8 != 0)? 1:0);
                            xx = 0;
                            while (xx < count)
                            {
                                if (p_t1t->num_lockbytes < RW_T1T_MAX_LOCK_BYTES)
                                {
                                    p_t1t->lockbyte[p_t1t->num_lockbytes].tlv_index = p_t1t->num_lock_tlvs;
                                    p_t1t->lockbyte[p_t1t->num_lockbytes].byte_index = xx;
                                    p_t1t->lockbyte[p_t1t->num_lockbytes].b_lock_read = FALSE;
                                    p_t1t->num_lockbytes++;
                                }
                                else
                                    RW_TRACE_ERROR1 ("T1T Buffer overflow error. Max supported lock bytes=0x%02X", RW_T1T_MAX_LOCK_BYTES);
                                xx++;
                            }
                            p_t1t->num_lock_tlvs++;
                            rw_t1t_update_attributes ();
                        }
                        else
                            RW_TRACE_ERROR1 ("T1T Buffer overflow error. Max supported lock tlvs=0x%02X", RW_T1T_MAX_LOCK_TLVS);

                        tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
                    }
                }
                else
                {
                    if (bytes_count == 0)
                    {
                        tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
                    }
                }
                break;

            case TAG_MEM_CTRL_TLV:
                bytes_count--;
                if (  (p_t1t->tlv_detect == TAG_MEM_CTRL_TLV)
                    ||(p_t1t->tlv_detect == TAG_NDEF_TLV)  )
                {
                    tlv_value[2 - bytes_count] = p_readbytes[offset];
                    if (bytes_count == 0)
                    {
                        if (p_t1t->num_mem_tlvs >= RW_T1T_MAX_MEM_TLVS)
                        {
                            RW_TRACE_ERROR0 ("rw_t1t_handle_tlv_detect_rsp - Maximum buffer allocated for Memory tlv has reached");
                            failed  = TRUE;
                        }
                        else
                        {
                            /* Extract dynamic reserved bytes */
                            p_t1t->mem_tlv[p_t1t->num_mem_tlvs].offset   = (tlv_value[0] >> 4) & 0x0F;
                            p_t1t->mem_tlv[p_t1t->num_mem_tlvs].offset  *= (UINT8) tags_pow (2, tlv_value[2] & 0x0F);
                            p_t1t->mem_tlv[p_t1t->num_mem_tlvs].offset  += tlv_value[0] & 0x0F;
                            p_t1t->mem_tlv[p_t1t->num_mem_tlvs].num_bytes = tlv_value[1];
                            p_t1t->num_mem_tlvs++;
                            rw_t1t_update_attributes ();
                            tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
                        }
                    }
                }
                else
                {
                    if (bytes_count == 0)
                    {
                        tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
                    }
                }
                break;

            case TAG_PROPRIETARY_TLV:
                bytes_count--;
                if (p_t1t->tlv_detect == TAG_PROPRIETARY_TLV)
                    found = TRUE;
                else
                {
                    if (bytes_count == 0)
                    {
                        tlv_detect_state = RW_T1T_SUBSTATE_WAIT_TLV_DETECT;
                    }
                }
                break;
            }
            offset++;
            break;
        }
    }

    p_t1t->work_offset += bytes_read;

    /* NDEF/Lock/Mem TLV to be found in segment 0, if not assume detection failed */
    if (!found && !failed)
    {
        if (  ((p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV) && (p_t1t->num_lockbytes > 0))
            ||((p_t1t->tlv_detect == TAG_MEM_CTRL_TLV) && (p_t1t->num_mem_tlvs > 0))  )
        {
            found = TRUE;
        }
        else
        {
            if (p_t1t->tlv_detect == TAG_NDEF_TLV)
            {
                p_t1t->tag_attribute = RW_T1_TAG_ATTRB_INITIALIZED;
            }
            failed = TRUE;
        }
    }


    status = failed ? NFC_STATUS_FAILED : NFC_STATUS_OK;
    return status;
}

/*******************************************************************************
**
** Function         rw_t1t_handle_ndef_rall_rsp
**
** Description      Handle response to RALL command sent while reading an
**                  NDEF message
**
** Returns          NFC_STATUS_CONTINUE, if NDEF read operation is not complete
**                  NFC_STATUS_OK, if NDEF read is successfull
**                  NFC_STATUS_FAILED,if NDEF read failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_handle_ndef_rall_rsp (void)
{
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    tNFC_STATUS status  = NFC_STATUS_CONTINUE;
    UINT8       count;
    UINT8       adds;

    count               = (UINT8) p_t1t->ndef_msg_offset;
    p_t1t->work_offset  = 0;
    p_t1t->segment      = 0;

    while (count < T1T_STATIC_SIZE && p_t1t->work_offset < p_t1t->ndef_msg_len)
    {
        if (rw_t1t_is_lock_reserved_otp_byte (count) == FALSE)
        {
            p_t1t->p_ndef_buffer[p_t1t->work_offset] = p_t1t->mem[count];
            p_t1t->work_offset++;
        }
        count++;
    }
    if (p_t1t->work_offset != p_t1t->ndef_msg_len)
    {
        if ((p_t1t->hr[0] & 0x0F) != 1)
        {
            if (p_t1t->work_offset == 0)
                return NFC_STATUS_FAILED;

            else
            {
                p_t1t->block_read   = T1T_STATIC_BLOCKS + 1;
                p_t1t->segment++;
            }
            if (p_t1t->ndef_msg_len - p_t1t->work_offset <= 8)
            {
                if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_READ8, p_t1t->block_read, NULL)) == NFC_STATUS_OK)
                {
                    p_t1t->tlv_detect  = TAG_NDEF_TLV;
                    p_t1t->state    = RW_T1T_STATE_READ_NDEF;
                    status          = NFC_STATUS_CONTINUE;
                }
            }
            else
            {
                /* send RSEG command */
                RW_T1T_BLD_ADDS ((adds), (p_t1t->segment));
                if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_RSEG, adds, NULL)) == NFC_STATUS_OK)
                {
                    p_t1t->state    = RW_T1T_STATE_READ_NDEF;
                    status          = NFC_STATUS_CONTINUE;
                }
            }
        }
        else
        {
            RW_TRACE_ERROR1 ("RW_T1tReadNDef - Invalid NDEF len: %u or NDEF corrupted", p_t1t->ndef_msg_len);
            status = NFC_STATUS_FAILED;
        }
    }
    else
    {
        status = NFC_STATUS_OK;
    }
    return status;
}

/*******************************************************************************
**
** Function         rw_t1t_handle_ndef_read_rsp
**
** Description      Handle response to commands sent while reading an
**                  NDEF message
**
** Returns          NFC_STATUS_CONTINUE, if tlv read is not yet complete
**                  NFC_STATUS_OK, if tlv read is complete & success
**                  NFC_STATUS_FAILED,if tlv read failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_handle_ndef_read_rsp (UINT8 *p_data)
{
    tNFC_STATUS         ndef_status = NFC_STATUS_CONTINUE;
    tRW_T1T_CB          *p_t1t      = &rw_cb.tcb.t1t;
    UINT8               index;
    UINT8               adds;
    tT1T_CMD_RSP_INFO   *p_cmd_rsp_info = (tT1T_CMD_RSP_INFO *) rw_cb.tcb.t1t.p_cmd_rsp_info;

    /* The Response received could be for Read8 or Read Segment command */
    switch(p_cmd_rsp_info->opcode)
    {
    case T1T_CMD_READ8:
        if (p_t1t->work_offset == 0)
        {
            index = p_t1t->ndef_msg_offset % T1T_BLOCK_SIZE;
        }
        else
        {
            index = 0;
        }
        p_t1t->segment = (p_t1t->block_read * T1T_BLOCK_SIZE)/T1T_SEGMENT_SIZE;
        while (index < T1T_BLOCK_SIZE && p_t1t->work_offset < p_t1t->ndef_msg_len)
        {
            if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) ((p_t1t->block_read * T1T_BLOCK_SIZE) + index)) == FALSE)
            {
                p_t1t->p_ndef_buffer[p_t1t->work_offset] = p_data[index];
                p_t1t->work_offset++;
            }
            index++;
        }
        break;

    case T1T_CMD_RSEG:
        if (p_t1t->work_offset == 0)
        {
            index = p_t1t->ndef_msg_offset % T1T_SEGMENT_SIZE;
        }
        else
        {
            index = 0;
        }
        p_t1t->block_read = ((p_t1t->segment + 1) * T1T_BLOCKS_PER_SEGMENT) - 1;

        while (index < T1T_SEGMENT_SIZE && p_t1t->work_offset < p_t1t->ndef_msg_len)
        {
            if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) (index)) == FALSE)
            {
                p_t1t->p_ndef_buffer[p_t1t->work_offset] = p_data[index];
                p_t1t->work_offset++;
            }
            index++;
        }
        break;

    default:
        break;
    }
    if (p_t1t->work_offset < p_t1t->ndef_msg_len)
    {
        if ((p_t1t->hr[0] & 0x0F) != 1)
        {
            if ((p_t1t->ndef_msg_len - p_t1t->work_offset) <= T1T_BLOCK_SIZE)
            {
                p_t1t->block_read++;
                if ((ndef_status = rw_t1t_send_dyn_cmd (T1T_CMD_READ8, (UINT8) (p_t1t->block_read), NULL)) == NFC_STATUS_OK)
                {
                    ndef_status  = NFC_STATUS_CONTINUE;
                }
            }
            else
            {
                p_t1t->segment++;
                /* send RSEG command */
                RW_T1T_BLD_ADDS ((adds), (p_t1t->segment));
                if ((ndef_status = rw_t1t_send_dyn_cmd (T1T_CMD_RSEG, adds, NULL)) == NFC_STATUS_OK)
                {
                    ndef_status  = NFC_STATUS_CONTINUE;
                }
            }
        }
    }
    else
    {
        ndef_status = NFC_STATUS_OK;
    }
    return ndef_status;
}

/*******************************************************************************
**
** Function         rw_t1t_next_ndef_write_block
**
** Description      This function prepare and writes ndef blocks
**
** Returns          NFC_STATUS_CONTINUE, if tlv write is not yet complete
**                  NFC_STATUS_OK, if tlv write is complete & success
**                  NFC_STATUS_FAILED,if tlv write failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_next_ndef_write_block (void)
{
    BOOLEAN     b_block_write_cmd   = FALSE;
    tRW_T1T_CB  *p_t1t              = &rw_cb.tcb.t1t;
    tNFC_STATUS ndef_status         = NFC_STATUS_CONTINUE;
    UINT8       write_block[8];
    UINT8       block;
    UINT8       index;
    UINT8       new_lengthfield_len;
    UINT8       length_field[3];
    UINT16      initial_offset;
    UINT8       count;
    /* Write NDEF Message */
    new_lengthfield_len = p_t1t->new_ndef_msg_len > 254 ? 3:1;

    /* Identify the command to use for NDEF write operation */
    if ((p_t1t->hr[0] & 0x0F) != 1)
    {
        /* Dynamic memory structure */
        b_block_write_cmd = FALSE;
        block           = p_t1t->ndef_block_written + 1;
        p_t1t->segment  = (block * T1T_BLOCK_SIZE) /T1T_SEGMENT_SIZE;

        count = 0;
        while (block <= p_t1t->mem[T1T_CC_TMS_BYTE])
        {
            index = 0;
            if (block == p_t1t->num_ndef_finalblock)
            {
                /* T1T_CMD_WRITE_E8 Command */
                b_block_write_cmd = TRUE;
                break;
            }
            while (index < T1T_BLOCK_SIZE && p_t1t->work_offset < (p_t1t->new_ndef_msg_len + new_lengthfield_len))
            {
                if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) ((block * T1T_BLOCK_SIZE) + index)) == FALSE)
                {
                    count++;
                }
                index++;
            }
            if (count == T1T_BLOCK_SIZE)
            {
                /* T1T_CMD_WRITE_E8 Command */
                b_block_write_cmd = TRUE;
                break;
            }
            else if (count == 0)
            {
                index = 0;
                block++;
                if (p_t1t->segment != (block * T1T_BLOCK_SIZE) /T1T_SEGMENT_SIZE)
                {
                    p_t1t->segment = (block * T1T_BLOCK_SIZE) /T1T_SEGMENT_SIZE;
                }
            }
            else
            {
                /* T1T_CMD_WRITE_E Command */
                b_block_write_cmd = FALSE;
                break;
            }
        }
    }
    else
    {
        /* Static memory structure */
        block       = p_t1t->ndef_block_written;
        b_block_write_cmd = FALSE;
    }

    new_lengthfield_len = p_t1t->new_ndef_msg_len > 254 ? 3:1;
    if (new_lengthfield_len == 3)
    {
        length_field[0] = T1T_LONG_NDEF_LEN_FIELD_BYTE0;
        length_field[1] = (UINT8) (p_t1t->new_ndef_msg_len >> 8);
        length_field[2] = (UINT8) (p_t1t->new_ndef_msg_len);
    }
    else
    {
        length_field[0] = (UINT8) (p_t1t->new_ndef_msg_len);
    }

    if (b_block_write_cmd)
    {
        /* Dynamic memory structure */
        index           = 0;
        p_t1t->segment  = (block * T1T_BLOCK_SIZE) /T1T_SEGMENT_SIZE;

        initial_offset  = p_t1t->work_offset;
        block = rw_t1t_prepare_ndef_bytes (write_block, length_field,  &index, FALSE, block, new_lengthfield_len);
        if (p_t1t->work_offset == initial_offset)
        {
            ndef_status = NFC_STATUS_FAILED;
        }
        else
        {
            /* Send WRITE_E8 command */
            ndef_status = rw_t1t_send_ndef_block (write_block, block);
        }
    }
    else
    {
        /* Static memory structure */
        if (p_t1t->write_byte + 1 >= T1T_BLOCK_SIZE)
        {
            index = 0;
            block++;
        }
        else
        {
            index       = p_t1t->write_byte + 1;
        }
        initial_offset  = p_t1t->work_offset;
        block = rw_t1t_prepare_ndef_bytes (write_block, length_field, &index, TRUE, block, new_lengthfield_len);
        if (p_t1t->work_offset == initial_offset)
        {
            ndef_status = NFC_STATUS_FAILED;
        }
        else
        {
            /* send WRITE-E command */
            ndef_status = rw_t1t_send_ndef_byte (write_block[index], block, index, new_lengthfield_len);
        }
    }
    return ndef_status;

}

/*******************************************************************************
**
** Function         rw_t1t_ndef_write_first_block
**
** Description      This function writes ndef first block
**
** Returns          NFC_STATUS_CONTINUE, if tlv write is not yet complete
**                  NFC_STATUS_OK, if tlv write is complete & success
**                  NFC_STATUS_FAILED,if tlv write failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_ndef_write_first_block (void)
{
    tNFC_STATUS ndef_status = NFC_STATUS_CONTINUE;
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    UINT8       block;
    UINT8       index;
    UINT8       new_lengthfield_len;
    UINT8       length_field[3];
    UINT8       write_block[8];

    /* handle positive response to invalidating existing NDEF Message */
    p_t1t->work_offset = 0;
    new_lengthfield_len = p_t1t->new_ndef_msg_len > 254 ? 3:1;
    if (new_lengthfield_len == 3)
    {
        length_field[0] = T1T_LONG_NDEF_LEN_FIELD_BYTE0;
        length_field[1] = (UINT8) (p_t1t->new_ndef_msg_len >> 8);
        length_field[2] = (UINT8) (p_t1t->new_ndef_msg_len);
    }
    else
    {
        length_field[0] = (UINT8) (p_t1t->new_ndef_msg_len);
    }
    /* updating ndef_first_block with new ndef message */
    memcpy(write_block,p_t1t->ndef_first_block,T1T_BLOCK_SIZE);
    index = p_t1t->ndef_header_offset % T1T_BLOCK_SIZE;
    block = (UINT8) (p_t1t->ndef_header_offset / T1T_BLOCK_SIZE);
    p_t1t->segment      = (UINT8) (p_t1t->ndef_header_offset/T1T_SEGMENT_SIZE);

    if ((p_t1t->hr[0] & 0x0F) != 1)
    {
        /* Dynamic Memory structure */
        block = rw_t1t_prepare_ndef_bytes (write_block, length_field, &index, FALSE, block, new_lengthfield_len);

        if (p_t1t->work_offset == 0)
        {
            ndef_status = NFC_STATUS_FAILED;
        }
        else
        {
            /* Send WRITE-E8 command based on the prepared write_block */
            ndef_status = rw_t1t_send_ndef_block (write_block, block);
        }
    }
    else
    {
        /* Static Memory structure */
        block = rw_t1t_prepare_ndef_bytes (write_block, length_field, &index, TRUE, block, new_lengthfield_len);
        if (p_t1t->work_offset == 0)
        {
            ndef_status = NFC_STATUS_FAILED;
        }
        else
        {
            /* send WRITE-E command */
            ndef_status = rw_t1t_send_ndef_byte (write_block[index], block, index, new_lengthfield_len);
        }
    }

    return ndef_status;
}

/*******************************************************************************
**
** Function         rw_t1t_send_ndef_byte
**
** Description      Sends ndef message or length field byte and update
**                  status
**
** Returns          NFC_STATUS_CONTINUE, if tlv write is not yet complete
**                  NFC_STATUS_OK, if tlv write is complete & success
**                  NFC_STATUS_FAILED,if tlv write failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_send_ndef_byte (UINT8 data, UINT8 block, UINT8 index, UINT8 msg_len)
{
    tNFC_STATUS ndef_status = NFC_STATUS_CONTINUE;
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    UINT8       addr;

    /* send WRITE-E command */
    RW_T1T_BLD_ADD ((addr), (block), (index));
    if (NFC_STATUS_OK == rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, data))
    {
        p_t1t->write_byte           = index;
        p_t1t->ndef_block_written   = block;
        if (p_t1t->work_offset == p_t1t->new_ndef_msg_len + msg_len)
        {
            ndef_status =  NFC_STATUS_OK;
        }
        else
        {
            ndef_status = NFC_STATUS_CONTINUE;
        }
    }
    else
    {
        ndef_status = NFC_STATUS_FAILED;
    }
    return ndef_status;
}

/*******************************************************************************
**
** Function         rw_t1t_prepare_ndef_bytes
**
** Description      prepares ndef block to write
**
** Returns          block number where to write
**
*******************************************************************************/
static UINT8 rw_t1t_prepare_ndef_bytes (UINT8 *p_data, UINT8 *p_length_field, UINT8 *p_index, BOOLEAN b_one_byte, UINT8 block, UINT8 lengthfield_len)
{
    tRW_T1T_CB  *p_t1t          = &rw_cb.tcb.t1t;
    UINT8       first_block     = (UINT8) (p_t1t->ndef_header_offset / T1T_BLOCK_SIZE);
    UINT16      initial_offset  = p_t1t->work_offset;

    while (p_t1t->work_offset == initial_offset && block <= p_t1t->mem[T1T_CC_TMS_BYTE])
    {
        if (  (block == p_t1t->num_ndef_finalblock)
            &&(block != first_block)  )
        {
            memcpy (p_data,p_t1t->ndef_final_block,T1T_BLOCK_SIZE);
        }
        /* Update length field */
        while (  (*p_index < T1T_BLOCK_SIZE)
               &&(p_t1t->work_offset < lengthfield_len)  )
        {
            if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) ((block * T1T_BLOCK_SIZE) + *p_index)) == FALSE)
            {
                p_data[*p_index] = p_length_field[p_t1t->work_offset];
                p_t1t->work_offset++;
                if (b_one_byte)
                    return block;
            }
            (*p_index)++;
            if (p_t1t->work_offset == lengthfield_len)
            {
                break;
            }
        }
        /* Update ndef message field */
        while (*p_index < T1T_BLOCK_SIZE && p_t1t->work_offset < (p_t1t->new_ndef_msg_len + lengthfield_len))
        {
            if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) ((block * T1T_BLOCK_SIZE) + *p_index)) == FALSE)
            {
                p_data[*p_index] = p_t1t->p_ndef_buffer[p_t1t->work_offset - lengthfield_len];
                p_t1t->work_offset++;
                if (b_one_byte)
                    return block;
            }
            (*p_index)++;
        }
        if (p_t1t->work_offset == initial_offset)
        {
            *p_index = 0;
            block++;
            if (p_t1t->segment != (block * T1T_BLOCK_SIZE) /T1T_SEGMENT_SIZE)
            {
                p_t1t->segment = (block * T1T_BLOCK_SIZE) /T1T_SEGMENT_SIZE;
            }
        }
    }
    return block;
}

/*******************************************************************************
**
** Function         rw_t1t_send_ndef_block
**
** Description      Sends ndef block and update status
**
** Returns          NFC_STATUS_CONTINUE, if tlv write is not yet complete
**                  NFC_STATUS_OK, if tlv write is complete & success
**                  NFC_STATUS_FAILED,if tlv write failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_send_ndef_block (UINT8 *p_data, UINT8 block)
{
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    tNFC_STATUS ndef_status = NFC_STATUS_CONTINUE;

    if (NFC_STATUS_OK == rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_E8, block, p_data))
    {
        p_t1t->ndef_block_written = block;
        if (p_t1t->ndef_block_written == p_t1t->num_ndef_finalblock)
        {
            ndef_status  =  NFC_STATUS_OK;
        }
        else
        {
             ndef_status =  NFC_STATUS_CONTINUE;
        }
    }
    else
    {
        ndef_status = NFC_STATUS_FAILED;
    }
    return ndef_status;
}

/*******************************************************************************
**
** Function         rw_t1t_get_ndef_flags
**
** Description      Prepare NDEF Flags
**
** Returns          NDEF Flag value
**
*******************************************************************************/
static UINT8 rw_t1t_get_ndef_flags (void)
{
    UINT8       flags   = 0;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;

    if ((p_t1t->hr[0] & 0xF0) == T1T_NDEF_SUPPORTED)
        flags |= RW_NDEF_FL_SUPPORTED;

    if (t1t_tag_init_data (p_t1t->hr[0]) != NULL)
        flags |= RW_NDEF_FL_FORMATABLE;

    if ((p_t1t->mem[T1T_CC_RWA_BYTE] & 0x0F) == T1T_CC_RWA_RO)
        flags |=RW_NDEF_FL_READ_ONLY;

    return flags;
}

/*******************************************************************************
**
** Function         rw_t1t_get_ndef_max_size
**
** Description      Calculate maximum size of NDEF message that can be written
**                  on to the tag
**
** Returns          Maximum size of NDEF Message
**
*******************************************************************************/
static UINT16 rw_t1t_get_ndef_max_size (void)
{
    UINT16              offset;
    tRW_T1T_CB          *p_t1t   = &rw_cb.tcb.t1t;
    UINT16              tag_size = (p_t1t->mem[T1T_CC_TMS_BYTE] +1)* T1T_BLOCK_SIZE;
    const tT1T_INIT_TAG *p_ret;
    UINT8               init_segment = p_t1t->segment;

    p_t1t->max_ndef_msg_len = 0;
    offset                  = p_t1t->ndef_msg_offset;
    p_t1t->segment          = (UINT8) (p_t1t->ndef_msg_offset/T1T_SEGMENT_SIZE);

    if (  (tag_size < T1T_STATIC_SIZE)
        ||(tag_size > (T1T_SEGMENT_SIZE * T1T_MAX_SEGMENTS))
        ||((p_t1t->mem[T1T_CC_NMN_BYTE] != T1T_CC_NMN) && (p_t1t->mem[T1T_CC_NMN_BYTE] != 0))  )
    {
        /* Tag not formated, determine maximum NDEF size from HR */
        if  (  ((p_t1t->hr[0] & 0xF0) == T1T_NDEF_SUPPORTED)
             &&((p_ret = t1t_tag_init_data (p_t1t->hr[0])) != NULL)  )
        {
            p_t1t->max_ndef_msg_len = ((p_ret->tms +1)* T1T_BLOCK_SIZE) - T1T_OTP_LOCK_RES_BYTES - T1T_UID_LEN - T1T_ADD_LEN - T1T_CC_LEN - T1T_TLV_TYPE_LEN - T1T_SHORT_NDEF_LEN_FIELD_LEN;
            if (p_ret->b_dynamic)
            {
                p_t1t->max_ndef_msg_len -= (T1T_TLV_TYPE_LEN + T1T_DEFAULT_TLV_LEN_FIELD_LEN + T1T_DEFAULT_TLV_LEN + T1T_TLV_TYPE_LEN + T1T_DEFAULT_TLV_LEN_FIELD_LEN + T1T_DEFAULT_TLV_LEN);
                p_t1t->max_ndef_msg_len -= T1T_DYNAMIC_LOCK_BYTES;
            }
            offset = tag_size;
        }
        else
        {
            p_t1t->segment = init_segment;
            return p_t1t->max_ndef_msg_len;
        }
    }

    /* Starting from NDEF Message offset find the first locked data byte */
    while (offset < tag_size)
    {
        if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) (offset)) == FALSE)
        {
            if (rw_t1t_is_read_only_byte ((UINT16) offset) == TRUE)
                break;
            p_t1t->max_ndef_msg_len++;
        }
        offset++;
        if (offset % T1T_SEGMENT_SIZE == 0)
        {
            p_t1t->segment = (UINT8) (offset / T1T_SEGMENT_SIZE);
        }
    }
    /* NDEF Length field length changes based on NDEF size */
    if (  (p_t1t->max_ndef_msg_len >= T1T_LONG_NDEF_LEN_FIELD_BYTE0)
        &&((p_t1t->ndef_msg_offset - p_t1t->ndef_header_offset) == T1T_SHORT_NDEF_LEN_FIELD_LEN)  )
    {
        p_t1t->max_ndef_msg_len -=  (p_t1t->max_ndef_msg_len == T1T_LONG_NDEF_LEN_FIELD_BYTE0)? 1 : (T1T_LONG_NDEF_LEN_FIELD_LEN - T1T_SHORT_NDEF_LEN_FIELD_LEN);
    }

    p_t1t->segment = init_segment;
    return p_t1t->max_ndef_msg_len;
}

/*******************************************************************************
**
** Function         rw_t1t_handle_ndef_write_rsp
**
** Description      Handle response to commands sent while writing an
**                  NDEF message
**
** Returns          NFC_STATUS_CONTINUE, if tlv write is not yet complete
**                  NFC_STATUS_OK, if tlv write is complete & success
**                  NFC_STATUS_FAILED,if tlv write failed
**
*******************************************************************************/
static tNFC_STATUS rw_t1t_handle_ndef_write_rsp (UINT8 *p_data)
{
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    tNFC_STATUS ndef_status = NFC_STATUS_CONTINUE;
    UINT8       addr;

    switch (p_t1t->substate)
    {
    case RW_T1T_SUBSTATE_WAIT_READ_NDEF_BLOCK:
        /* Backup ndef_final_block */
        memcpy (p_t1t->ndef_final_block, p_data, T1T_BLOCK_SIZE);
        /* Invalidate existing NDEF Message */
        RW_T1T_BLD_ADD ((addr), (T1T_CC_BLOCK), (T1T_CC_NMN_OFFSET));
        if (NFC_STATUS_OK == rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, 0))
        {
            ndef_status     = NFC_STATUS_CONTINUE;
            p_t1t->state    = RW_T1T_STATE_WRITE_NDEF;
            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF;
        }
        else
        {
            ndef_status = NFC_STATUS_FAILED;
        }
        break;

    case RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF:
        ndef_status = rw_t1t_ndef_write_first_block ();
        break;

    case RW_T1T_SUBSTATE_WAIT_NDEF_WRITE:
        ndef_status = rw_t1t_next_ndef_write_block ();
        break;

    case RW_T1T_SUBSTATE_WAIT_NDEF_UPDATED:
        /* Validate new NDEF Message */
        RW_T1T_BLD_ADD ((addr), (T1T_CC_BLOCK), (T1T_CC_NMN_OFFSET));
        if (NFC_STATUS_OK == rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, T1T_CC_NMN))
        {
            ndef_status     = NFC_STATUS_OK;
        }
        else
        {
            ndef_status     = NFC_STATUS_FAILED;
        }
        break;
    default:
        break;
    }

    return ndef_status;
}

/*******************************************************************************
**
** Function         rw_t1t_update_attributes
**
** Description      This function will prepare attributes for the current
**                  segment. Every bit in the attribute refers to one byte of
**                  tag content.The bit corresponding to a tag byte will be set
**                  to '1' when the Tag byte is read only,otherwise will be set
**                  to '0'
**
** Returns          None
**
*******************************************************************************/
static void rw_t1t_update_attributes (void)
{
    UINT8       count       = 0;
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    UINT16      lower_offset;
    UINT16      upper_offset;
    UINT8       num_bytes;
    UINT16      offset;
    UINT8       bits_per_byte  = 8;

    count = 0;
    while (count < T1T_BLOCKS_PER_SEGMENT)
    {
        p_t1t->attr[count] = 0x00;
        count++;
    }

    lower_offset  = p_t1t->segment * T1T_SEGMENT_SIZE;
    upper_offset  = (p_t1t->segment + 1)* T1T_SEGMENT_SIZE;

    if (p_t1t->segment == 0)
    {
        /* UID/Lock/Reserved/OTP bytes */
        p_t1t->attr[0x00] = 0xFF; /* Uid bytes */
        p_t1t->attr[0x0D] = 0xFF; /* Reserved bytes */
        p_t1t->attr[0x0E] = 0xFF; /* lock/otp bytes */
        p_t1t->attr[0x0F] = 0xFF; /* lock/otp bytes */
    }

    /* update attr based on lock control and mem control tlvs */
    count = 0;
    while (count < p_t1t->num_lockbytes)
    {
        offset = p_t1t->lock_tlv[p_t1t->lockbyte[count].tlv_index].offset + p_t1t->lockbyte[count].byte_index;
        if (offset >= lower_offset && offset < upper_offset)
        {
            /* Set the corresponding bit in attr to indicate - lock byte */
            p_t1t->attr[(offset % T1T_SEGMENT_SIZE) / bits_per_byte] |= rw_t1t_mask_bits[(offset % T1T_SEGMENT_SIZE) % bits_per_byte];
        }
        count++;
    }
    count = 0;
    while (count < p_t1t->num_mem_tlvs)
    {
        num_bytes = 0;
        while (num_bytes < p_t1t->mem_tlv[count].num_bytes)
        {
            offset = p_t1t->mem_tlv[count].offset + num_bytes;
            if (offset >= lower_offset && offset < upper_offset)
            {
                /* Set the corresponding bit in attr to indicate - reserved byte */
                p_t1t->attr[(offset % T1T_SEGMENT_SIZE) / bits_per_byte] |= rw_t1t_mask_bits[(offset % T1T_SEGMENT_SIZE) % bits_per_byte];
            }
            num_bytes++;
        }
        count++;
    }
}

/*******************************************************************************
**
** Function         rw_t1t_get_lock_bits_for_segment
**
** Description      This function will identify the index of the dynamic lock
**                  byte that covers the current segment
**
** Parameters:      segment, segment number
**                  p_start_byte, pointer to hold the first lock byte index
**                  p_start_bit, pointer to hold the first lock bit index
**                  p_end_byte, pointer to hold the last lock byte index
**
** Returns          Total lock bits that covers the specified segment
**
*******************************************************************************/
static UINT8 rw_t1t_get_lock_bits_for_segment (UINT8 segment,UINT8 *p_start_byte, UINT8 *p_start_bit,UINT8 *p_end_byte)
{
    tRW_T1T_CB  *p_t1t              = &rw_cb.tcb.t1t;
    UINT16      byte_count          = T1T_SEGMENT_SIZE;
    UINT8       total_bits          = 0;
    UINT8       num_dynamic_locks   = 0;
    UINT8       bit_count           = 0;
    UINT16      tag_size            = (p_t1t->mem[T1T_CC_TMS_BYTE] +1) * T1T_BLOCK_SIZE;
    UINT16      lower_offset;
    UINT16      upper_offset;
    BOOLEAN     b_all_bits_are_locks = TRUE;
    UINT8       bytes_locked_per_bit;
    UINT8       num_bits;

    upper_offset    = (segment + 1) * T1T_SEGMENT_SIZE;

    if (upper_offset > tag_size)
        upper_offset = tag_size;

    lower_offset    = segment * T1T_SEGMENT_SIZE;
    *p_start_byte   = num_dynamic_locks;
    *p_start_bit    = 0;

    while (  (byte_count <= lower_offset)
           &&(num_dynamic_locks < p_t1t->num_lockbytes)  )
    {
        bytes_locked_per_bit = p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_locks].tlv_index].bytes_locked_per_bit;
        /* Number of bits in the current lock byte */
        b_all_bits_are_locks = ((p_t1t->lockbyte[num_dynamic_locks].byte_index + 1) * TAG_BITS_PER_BYTE <= p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_locks].tlv_index].num_bits);
        num_bits = b_all_bits_are_locks ? TAG_BITS_PER_BYTE : p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_locks].tlv_index].num_bits % TAG_BITS_PER_BYTE;

        /* Skip lock bits that covers all previous segments */
        if (bytes_locked_per_bit * num_bits + byte_count <= lower_offset)
        {
            byte_count += bytes_locked_per_bit * num_bits;
            num_dynamic_locks++;
        }
        else
        {
            /* The first lock bit that covers this segment is present in this segment */
            bit_count = 0;
            while (bit_count < num_bits)
            {
                byte_count += bytes_locked_per_bit;
                if (byte_count > lower_offset)
                {
                    *p_start_byte = num_dynamic_locks;
                    *p_end_byte = num_dynamic_locks;
                    *p_start_bit  = bit_count;
                    bit_count++;
                    total_bits = 1;
                    break;
                }
                bit_count++;
            }
        }
    }
    if (num_dynamic_locks == p_t1t->num_lockbytes)
    {
        return 0;
    }
    while (  (byte_count < upper_offset)
           &&(num_dynamic_locks < p_t1t->num_lockbytes)  )
    {
        bytes_locked_per_bit = p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_locks].tlv_index].bytes_locked_per_bit;

        /* Number of bits in the current lock byte */
        b_all_bits_are_locks = ((p_t1t->lockbyte[num_dynamic_locks].byte_index + 1) * TAG_BITS_PER_BYTE <= p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_locks].tlv_index].num_bits);
        num_bits             =  b_all_bits_are_locks ? TAG_BITS_PER_BYTE : p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_locks].tlv_index].num_bits % TAG_BITS_PER_BYTE;

        /* Collect all lock bits that covers the current segment */
        if ((bytes_locked_per_bit * (num_bits - bit_count)) + byte_count < upper_offset)
        {
            byte_count       += bytes_locked_per_bit * (num_bits - bit_count);
            total_bits       += num_bits - bit_count;
            bit_count         = 0;
            *p_end_byte       = num_dynamic_locks;
            num_dynamic_locks++;
        }
        else
        {
            /* The last lock byte that covers the current segment */
            bit_count = 0;
            while (bit_count < num_bits)
            {
                byte_count += bytes_locked_per_bit;
                if (byte_count >= upper_offset)
                {
                    *p_end_byte = num_dynamic_locks;
                    total_bits += (bit_count + 1);
                    break;
                }
                bit_count++;
            }
        }
    }
    return total_bits;
}

/*******************************************************************************
**
** Function         rw_t1t_update_lock_attributes
**
** Description      This function will check if the tag index passed as
**                  argument is a locked byte and return
**                  TRUE or FALSE
**
** Parameters:      index, the index of the byte in the tag
**
**
** Returns          TRUE, if the specified index in the tag is a locked or
**                        reserved or otp byte
**                  FALSE, otherwise
**
*******************************************************************************/
static void rw_t1t_update_lock_attributes (void)
{
    UINT8       xx = 0;
    UINT8       bytes_locked_per_lock_bit;
    UINT8       num_static_lock_bytes       = 0;
    UINT8       num_dynamic_lock_bytes      = 0;
    UINT8       bits_covered                = 0;
    UINT8       bytes_covered               = 0;
    UINT8       block_count                 = 0;
    tRW_T1T_CB  *p_t1t = &rw_cb.tcb.t1t;
    UINT8       start_lock_byte;
    UINT8       start_lock_bit;
    UINT8       end_lock_byte;
    UINT8       num_lock_bits;
    UINT8       total_bits;


    block_count = 0;
    while (block_count < T1T_BLOCKS_PER_SEGMENT)
    {
        p_t1t->lock_attr[block_count] = 0x00;
        block_count++;
    }

    /* update lock_attr based on static lock bytes */
    if (p_t1t->segment == 0)
    {
        xx                      = 0;
        num_static_lock_bytes   = 0;
        block_count             = 0;
        num_lock_bits           = 8;

        while (num_static_lock_bytes < T1T_NUM_STATIC_LOCK_BYTES)
        {
            /* Update lock attribute based on 2 static locks */
            while (xx < num_lock_bits)
            {
                p_t1t->lock_attr[block_count] = 0x00;

                if (p_t1t->mem[T1T_LOCK_0_OFFSET + num_static_lock_bytes] & rw_t1t_mask_bits[xx++])
                {
                    /* If the bit is set then 1 block is locked */
                    p_t1t->lock_attr[block_count] = 0xFF;
                }

                block_count++;
            }
            num_static_lock_bytes++;
            xx = 0;
        }
        /* Locked bytes */
        p_t1t->lock_attr[0x00] = 0xFF;
        p_t1t->lock_attr[0x0D] = 0xFF;
    }
    else
    {
        /* update lock_attr based on segment and using dynamic lock bytes */
        if ((total_bits = rw_t1t_get_lock_bits_for_segment (p_t1t->segment,&start_lock_byte, &start_lock_bit,&end_lock_byte)) != 0)
        {
            xx                       = start_lock_bit;
            num_dynamic_lock_bytes   = start_lock_byte;
            bits_covered             = 0;
            bytes_covered            = 0;
            block_count              = 0;
            num_lock_bits            = 8;

            p_t1t->lock_attr[block_count] = 0;

            while (num_dynamic_lock_bytes <= end_lock_byte)
            {
                bytes_locked_per_lock_bit   = p_t1t->lock_tlv[p_t1t->lockbyte[num_dynamic_lock_bytes].tlv_index].bytes_locked_per_bit;
                if (num_dynamic_lock_bytes == end_lock_byte)
                {
                    num_lock_bits = (total_bits % 8 == 0)? 8:total_bits % 8;
                }
                while (xx < num_lock_bits)
                {
                    bytes_covered = 0;
                    while (bytes_covered < bytes_locked_per_lock_bit)
                    {
                        /* Set/clear lock_attr byte bits based on whether a particular lock bit is set or not
                         * each bit in lock_attr represents one byte in Tag read only attribute */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                        if ((p_t1t->lockbyte[num_dynamic_lock_bytes].lock_byte & rw_t1t_mask_bits[xx]) && (block_count < T1T_BLOCKS_PER_SEGMENT))
#else
                        if (p_t1t->lockbyte[num_dynamic_lock_bytes].lock_byte & rw_t1t_mask_bits[xx])
#endif
                        {
                            p_t1t->lock_attr[block_count] |= 0x01 << bits_covered;
                        }
                        bytes_covered++;
                        bits_covered++;
                        if (bits_covered == 8)
                        {
                            bits_covered = 0;
                            block_count++;
                            if (block_count < T1T_BLOCKS_PER_SEGMENT)
                                p_t1t->lock_attr[block_count] = 0;
                        }
                    }
                    xx++;
                }
                num_dynamic_lock_bytes++;
                xx = 0;
            }
        }
    }
}

/*******************************************************************************
**
** Function         rw_t1t_is_lock_reserved_otp_byte
**
** Description      This function will check if the tag index passed as
**                  argument is a lock or reserved or otp byte
**
** Parameters:      index, the index of the byte in the tag's current segment
**
**
** Returns          TRUE, if the specified index in the tag is a locked or
**                        reserved or otp byte
**                  FALSE, otherwise
**
*******************************************************************************/
static BOOLEAN rw_t1t_is_lock_reserved_otp_byte (UINT16 index)
{
    tRW_T1T_CB  *p_t1t = &rw_cb.tcb.t1t;

    if (p_t1t->attr_seg != p_t1t->segment)
    {
        /* Update p_t1t->attr to reflect the current segment */
        rw_t1t_update_attributes ();
        p_t1t->attr_seg = p_t1t->segment;
    }
    index = index % T1T_SEGMENT_SIZE;

    /* Every bit in p_t1t->attr indicates one specific byte of the tag is either a lock/reserved/otp byte or not
     * So, each array element in p_t1t->attr covers one block in the tag as T1 block size and array element size is 8
     * Find the block and offset for the index (passed as argument) and Check if the offset bit in the
     * p_t1t->attr[block] is set or not. If the bit is set then it is a lock/reserved/otp byte, otherwise not */

    return ((p_t1t->attr[index /8] & rw_t1t_mask_bits[index % 8]) == 0) ? FALSE:TRUE;
}

/*******************************************************************************
**
** Function         rw_t1t_is_read_only_byte
**
** Description      This function will check if the tag index passed as
**                  argument is a read only byte
**
** Parameters:      index, the index of the byte in the tag's current segment
**
**
** Returns          TRUE, if the specified index in the tag is a locked or
**                        reserved or otp byte
**                  FALSE, otherwise
**
*******************************************************************************/
static BOOLEAN rw_t1t_is_read_only_byte (UINT16 index)
{
    tRW_T1T_CB  *p_t1t = &rw_cb.tcb.t1t;

    if (p_t1t->lock_attr_seg != p_t1t->segment)
    {
        /* Update p_t1t->lock_attr to reflect the current segment */
        rw_t1t_update_lock_attributes ();
        p_t1t->lock_attr_seg = p_t1t->segment;
    }

    index = index % T1T_SEGMENT_SIZE;
    /* Every bit in p_t1t->lock_attr indicates one specific byte of the tag is a read only byte or read write byte
     * So, each array element in p_t1t->lock_attr covers one block in the tag as T1 block size and array element size is 8
     * Find the block and offset for the index (passed as argument) and Check if the offset bit in the
     * p_t1t->lock_attr[block] is set or not. If the bit is set then it is a read only byte, otherwise read write byte */

    return ((p_t1t->lock_attr[index /8] & rw_t1t_mask_bits[index % 8]) == 0) ? FALSE:TRUE;
}

/*****************************************************************************
**
** Function         RW_T1tFormatNDef
**
** Description
**      Format Tag content
**
** Returns
**      NFC_STATUS_OK, Command sent to format Tag
**      NFC_STATUS_REJECTED: Invalid HR0 and cannot format the tag
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_T1tFormatNDef (void)
{
    tRW_T1T_CB          *p_t1t  = &rw_cb.tcb.t1t;
    tNFC_STATUS         status  = NFC_STATUS_FAILED;
    const tT1T_INIT_TAG *p_ret;
    UINT8               addr;
    UINT8               *p;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tFormatNDef - Tag not initialized/ Busy! State: %u", p_t1t->state);
        return (NFC_STATUS_FAILED);
    }

    if ((p_t1t->hr[0] & 0xF0) != T1T_NDEF_SUPPORTED)
    {
        RW_TRACE_WARNING1 ("RW_T1tFormatNDef - Cannot format tag as NDEF not supported. HR0: %u", p_t1t->hr[0]);
        return (NFC_STATUS_REJECTED);
    }

    if ((p_ret = t1t_tag_init_data (p_t1t->hr[0])) == NULL)
    {
        RW_TRACE_WARNING2 ("RW_T1tFormatNDef - Invalid HR - HR0: %u, HR1: %u", p_t1t->hr[0], p_t1t->hr[1]);
        return (NFC_STATUS_REJECTED);
    }

    memset (p_t1t->ndef_first_block, 0, T1T_BLOCK_SIZE);
    memset (p_t1t->ndef_final_block, 0, T1T_BLOCK_SIZE);
    p = p_t1t->ndef_first_block;

    /* Prepare Capability Container */
    UINT8_TO_BE_STREAM (p, T1T_CC_NMN);
    UINT8_TO_BE_STREAM (p, T1T_CC_VNO);
    UINT8_TO_BE_STREAM (p, p_ret->tms);
    UINT8_TO_BE_STREAM (p, T1T_CC_RWA_RW);
    if (p_ret->b_dynamic)
    {
        /* Prepare Lock and Memory TLV */
        UINT8_TO_BE_STREAM (p, TAG_LOCK_CTRL_TLV);
        UINT8_TO_BE_STREAM (p, T1T_DEFAULT_TLV_LEN);
        UINT8_TO_BE_STREAM (p, p_ret->lock_tlv[0]);
        UINT8_TO_BE_STREAM (p, p_ret->lock_tlv[1]);
        p = p_t1t->ndef_final_block;
        UINT8_TO_BE_STREAM (p, p_ret->lock_tlv[2]);
        UINT8_TO_BE_STREAM (p, TAG_MEM_CTRL_TLV);
        UINT8_TO_BE_STREAM (p, T1T_DEFAULT_TLV_LEN);
        UINT8_TO_BE_STREAM (p, p_ret->mem_tlv[0]);
        UINT8_TO_BE_STREAM (p, p_ret->mem_tlv[1]);
        UINT8_TO_BE_STREAM (p, p_ret->mem_tlv[2]);
    }
    /* Prepare NULL NDEF TLV */
    UINT8_TO_BE_STREAM (p, TAG_NDEF_TLV);
    UINT8_TO_BE_STREAM (p, 0);

    if (rw_cb.tcb.t1t.hr[0] != T1T_STATIC_HR0 || rw_cb.tcb.t1t.hr[1] >= RW_T1T_HR1_MIN)
    {
        /* send WRITE-E8 command */
        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_E8, 1, p_t1t->ndef_first_block)) == NFC_STATUS_OK)
        {
            p_t1t->state    = RW_T1T_STATE_FORMAT_TAG;
            p_t1t->b_update = FALSE;
            p_t1t->b_rseg   = FALSE;
            if (p_ret->b_dynamic)
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_CC;
            else
                p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_NULL_NDEF;
        }
    }
    else
    {
        /* send WRITE-E command */
        RW_T1T_BLD_ADD ((addr), 1, 0);

        if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, p_t1t->ndef_first_block[0])) == NFC_STATUS_OK)
        {
            p_t1t->work_offset  = 0;
            p_t1t->state        = RW_T1T_STATE_FORMAT_TAG;
            p_t1t->substate     = RW_T1T_SUBSTATE_WAIT_SET_NULL_NDEF;
            p_t1t->b_update     = FALSE;
            p_t1t->b_rseg       = FALSE;
        }
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_T1tLocateTlv
**
** Description      This function is called to find the start of the given TLV
**
** Parameters:      tlv_type, Type of TLV to find
**
** Returns          NCI_STATUS_OK, if detection was started. Otherwise, error status.
**
*******************************************************************************/
tNFC_STATUS RW_T1tLocateTlv (UINT8 tlv_type)
{
    tNFC_STATUS     status = NFC_STATUS_FAILED;
    tRW_T1T_CB      *p_t1t= &rw_cb.tcb.t1t;
    UINT8           adds;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tLocateTlv - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_FAILED);
    }
    p_t1t->tlv_detect = tlv_type;

    if(  (p_t1t->tlv_detect == TAG_NDEF_TLV)
       &&(((p_t1t->hr[0]) & 0xF0) != T1T_NDEF_SUPPORTED)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tLocateTlv - Error: NDEF not supported by the tag");
        return (NFC_STATUS_REFUSED);
    }

    if (  (p_t1t->tlv_detect == TAG_MEM_CTRL_TLV)
        ||(p_t1t->tlv_detect == TAG_NDEF_TLV)  )
    {
        p_t1t->num_mem_tlvs = 0;
    }

    if (  (p_t1t->tlv_detect == TAG_LOCK_CTRL_TLV)
        ||(p_t1t->tlv_detect == TAG_NDEF_TLV)  )
    {
        p_t1t->num_lockbytes = 0;
        p_t1t->num_lock_tlvs = 0;
    }

    /* Start reading memory, looking for the TLV */
    p_t1t->segment = 0;
    if ((p_t1t->hr[0] & 0x0F) != 1)
    {
        /* send RSEG command */
        RW_T1T_BLD_ADDS ((adds), (p_t1t->segment));
        status = rw_t1t_send_dyn_cmd (T1T_CMD_RSEG, adds, NULL);
    }
    else
    {
        status = rw_t1t_send_static_cmd (T1T_CMD_RALL, 0, 0);
    }
    if (status == NFC_STATUS_OK)
    {
        p_t1t->tlv_detect   = tlv_type;
        p_t1t->work_offset  = 0;
        p_t1t->state        = RW_T1T_STATE_TLV_DETECT;
        p_t1t->substate     = RW_T1T_SUBSTATE_NONE;
    }

    return status;
}

/*****************************************************************************
**
** Function         RW_T1tDetectNDef
**
** Description
**      This function is used to perform NDEF detection on a Type 1 tag, and
**      retrieve the tag's NDEF attribute information (block 0).
**
**      Before using this API, the application must call RW_SelectTagType to
**      indicate that a Type 1 tag has been activated.
**
** Returns
**      NFC_STATUS_OK: ndef detection procedure started
**      NFC_STATUS_WRONG_PROTOCOL: type 1 tag not activated
**      NFC_STATUS_BUSY: another command is already in progress
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_T1tDetectNDef (void)
{
    return RW_T1tLocateTlv (TAG_NDEF_TLV);
}

/*******************************************************************************
**
** Function         RW_T1tReadNDef
**
** Description      This function can be called to read the NDEF message on the tag.
**
** Parameters:      p_buffer:   The buffer into which to read the NDEF message
**                  buf_len:    The length of the buffer
**
** Returns          NCI_STATUS_OK, if read was started. Otherwise, error status.
**
*******************************************************************************/
tNFC_STATUS RW_T1tReadNDef (UINT8 *p_buffer, UINT16 buf_len)
{
    tNFC_STATUS     status = NFC_STATUS_FAILED;
    tRW_T1T_CB      *p_t1t = &rw_cb.tcb.t1t;
    BOOLEAN         b_notify;
    UINT8           adds;
    const tT1T_CMD_RSP_INFO *p_cmd_rsp_info_rall = t1t_cmd_to_rsp_info (T1T_CMD_RALL);
    const tT1T_CMD_RSP_INFO *p_cmd_rsp_info_rseg = t1t_cmd_to_rsp_info (T1T_CMD_RSEG);



    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tReadNDef - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_FAILED);
    }

    /* Check HR0 if NDEF supported by the tag */
    if (((p_t1t->hr[0]) & 0xF0) != T1T_NDEF_SUPPORTED)
    {
        RW_TRACE_ERROR0 ("RW_T1tReadNDef - Error: NDEF not supported by the tag");
        return (NFC_STATUS_REFUSED);
    }

    if (p_t1t->tag_attribute  == RW_T1_TAG_ATTRB_INITIALIZED_NDEF)
    {
        RW_TRACE_WARNING1 ("RW_T1tReadNDef - NDEF Message length is zero, NDEF Length : %u ", p_t1t->ndef_msg_len);
        return (NFC_STATUS_NOT_INITIALIZED);
    }

    if (  (p_t1t->tag_attribute != RW_T1_TAG_ATTRB_READ_WRITE)
        &&(p_t1t->tag_attribute != RW_T1_TAG_ATTRB_READ_ONLY)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tReadNDef - Error: NDEF detection not performed yet/ Tag is in Initialized state");
        return (NFC_STATUS_FAILED);
    }

    if (buf_len < p_t1t->ndef_msg_len)
    {
        RW_TRACE_WARNING2 ("RW_T1tReadNDef - buffer size: %u  less than NDEF msg sise: %u", buf_len, p_t1t->ndef_msg_len);
        return (NFC_STATUS_FAILED);
    }
    p_t1t->p_ndef_buffer = p_buffer;

    if (p_t1t->b_rseg == TRUE)
    {
        /* If already got response to RSEG 0 */
        p_t1t->state = RW_T1T_STATE_READ_NDEF;
        p_t1t->p_cmd_rsp_info = (tT1T_CMD_RSP_INFO *)p_cmd_rsp_info_rseg;

        rw_t1t_handle_read_rsp (&b_notify,p_t1t->mem);
        status       = NFC_STATUS_OK;
    }
    else if (p_t1t->b_update == TRUE)
    {
        /* If already got response to RALL */
        p_t1t->state = RW_T1T_STATE_READ_NDEF;
        p_t1t->p_cmd_rsp_info = (tT1T_CMD_RSP_INFO *) p_cmd_rsp_info_rall;

        rw_t1t_handle_read_rsp (&b_notify,p_t1t->mem);
        status       = NFC_STATUS_OK;

    }
    else
    {
        p_t1t->segment      = 0;
        p_t1t->work_offset  = 0;
        if ((p_t1t->hr[0] & 0x0F) != 1)
        {
            /* send RSEG command */
            RW_T1T_BLD_ADDS ((adds), (p_t1t->segment));
            status = rw_t1t_send_dyn_cmd (T1T_CMD_RSEG, adds, NULL);
        }
        else
        {
            status = rw_t1t_send_static_cmd (T1T_CMD_RALL, 0, 0);
        }
        if (status == NFC_STATUS_OK)
            p_t1t->state = RW_T1T_STATE_READ_NDEF;

    }

    return status;
}

/*******************************************************************************
**
** Function         RW_T1tWriteNDef
**
** Description      This function can be called to write an NDEF message to the tag.
**
** Parameters:      msg_len:    The length of the buffer
**                  p_msg:      The NDEF message to write
**
** Returns          NCI_STATUS_OK, if write was started. Otherwise, error status.
**
*******************************************************************************/
tNFC_STATUS RW_T1tWriteNDef (UINT16 msg_len, UINT8 *p_msg)
{
    tNFC_STATUS status          = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t          = &rw_cb.tcb.t1t;
    UINT16      num_ndef_bytes;
    UINT16      offset;
    UINT8       addr;
    UINT8       init_lengthfield_len;
    UINT8       new_lengthfield_len;
    UINT16      init_ndef_msg_offset;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteNDef - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_FAILED);
    }

    /* Check HR0 if NDEF supported by the tag */
    if (((p_t1t->hr[0]) & 0xF0) != T1T_NDEF_SUPPORTED)
    {
        RW_TRACE_ERROR0 ("RW_T1tWriteNDef - Error: NDEF not supported by the tag");
        return (NFC_STATUS_REFUSED);
    }

    if (  (p_t1t->tag_attribute  != RW_T1_TAG_ATTRB_READ_WRITE)
        &&(p_t1t->tag_attribute  != RW_T1_TAG_ATTRB_INITIALIZED_NDEF)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tWriteNDef - Tag cannot update NDEF");
        return (NFC_STATUS_REFUSED);
    }

    if (msg_len > p_t1t->max_ndef_msg_len)
    {
        RW_TRACE_ERROR1 ("RW_T1tWriteNDef - Cannot write NDEF of size greater than %u bytes", p_t1t->max_ndef_msg_len);
        return (NFC_STATUS_REFUSED);
    }

    p_t1t->p_ndef_buffer        = p_msg;
    p_t1t->new_ndef_msg_len     = msg_len;
    new_lengthfield_len         = p_t1t->new_ndef_msg_len > 254 ? 3:1;
    init_lengthfield_len        = (UINT8) (p_t1t->ndef_msg_offset - p_t1t->ndef_header_offset);
    init_ndef_msg_offset        = p_t1t->ndef_msg_offset;

    /* ndef_msg_offset should reflect the new ndef message offset */
    if (init_lengthfield_len > new_lengthfield_len)
    {
        p_t1t->ndef_msg_offset  =  init_ndef_msg_offset - (T1T_LONG_NDEF_LEN_FIELD_LEN - T1T_SHORT_NDEF_LEN_FIELD_LEN);
    }
    else if (init_lengthfield_len < new_lengthfield_len)
    {
        p_t1t->ndef_msg_offset  =  init_ndef_msg_offset + (T1T_LONG_NDEF_LEN_FIELD_LEN - T1T_SHORT_NDEF_LEN_FIELD_LEN);
    }

    num_ndef_bytes              = 0;
    offset                      = p_t1t->ndef_msg_offset;
    p_t1t->segment              = (UINT8) (p_t1t->ndef_msg_offset/T1T_SEGMENT_SIZE);

    /* Locate NDEF final block based on the size of new NDEF Message */
    while (num_ndef_bytes < p_t1t->new_ndef_msg_len)
    {
        if (rw_t1t_is_lock_reserved_otp_byte ((UINT16) offset) == FALSE)
            num_ndef_bytes++;

        offset++;
        if (offset % T1T_SEGMENT_SIZE == 0)
        {
            p_t1t->segment      = (UINT8) (offset / T1T_SEGMENT_SIZE);
        }
    }

    p_t1t->b_update = FALSE;
    p_t1t->b_rseg   = FALSE;

    if ((p_t1t->hr[0] & 0x0F) != 1)
    {
        /* Dynamic data structure */
        p_t1t->block_read = (UINT8) ((offset - 1)/T1T_BLOCK_SIZE);
        /* Read NDEF final block before updating */
        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_READ8, p_t1t->block_read, NULL)) == NFC_STATUS_OK)
        {
            p_t1t->num_ndef_finalblock = p_t1t->block_read;
            p_t1t->state    = RW_T1T_STATE_WRITE_NDEF;
            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_READ_NDEF_BLOCK;
        }
    }
    else
    {
        /* NDEF detected and Static memory structure so send WRITE-E command */
        RW_T1T_BLD_ADD ((addr), (T1T_CC_BLOCK), (T1T_CC_NMN_OFFSET));
        if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, 0)) == NFC_STATUS_OK)
        {
            p_t1t->state    = RW_T1T_STATE_WRITE_NDEF;
            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF;
        }

    }

    if (status != NFC_STATUS_OK)
    {
        /* if status failed, reset ndef_msg_offset to initial message */
        p_t1t->ndef_msg_offset = init_ndef_msg_offset;
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tSetTagReadOnly
**
** Description      This function can be called to set t1 tag as read only.
**
** Parameters:      None
**
** Returns          NCI_STATUS_OK, if setting tag as read only was started.
**                  Otherwise, error status.
**
*******************************************************************************/
tNFC_STATUS RW_T1tSetTagReadOnly (BOOLEAN b_hard_lock)
{
    tNFC_STATUS status      = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t      = &rw_cb.tcb.t1t;
    UINT8       addr;
    UINT8       num_locks;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tSetTagReadOnly - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    p_t1t->b_hard_lock = b_hard_lock;

    if (  (p_t1t->tag_attribute == RW_T1_TAG_ATTRB_READ_WRITE)
        ||(p_t1t->tag_attribute == RW_T1_TAG_ATTRB_INITIALIZED)
        ||(p_t1t->tag_attribute == RW_T1_TAG_ATTRB_INITIALIZED_NDEF)  )
    {
        /* send WRITE-NE command */
        RW_T1T_BLD_ADD ((addr), (T1T_CC_BLOCK), (T1T_CC_RWA_OFFSET));
        if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_NE, addr, 0x0F)) == NFC_STATUS_OK)
        {
            p_t1t->b_update = FALSE;
            p_t1t->b_rseg   = FALSE;

            if (p_t1t->b_hard_lock)
            {
                num_locks = 0;
                while (num_locks < p_t1t->num_lockbytes)
                {
                    p_t1t->lockbyte[num_locks].lock_status = RW_T1T_LOCK_NOT_UPDATED;
                    num_locks++;
                }
            }
            p_t1t->state    = RW_T1T_STATE_SET_TAG_RO;
            p_t1t->substate = RW_T1T_SUBSTATE_WAIT_SET_CC_RWA_RO;
        }
    }

    return status;
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         rw_t1t_get_sub_state_name
**
** Description      This function returns the sub_state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_t1t_get_sub_state_name (UINT8 sub_state)
{
    switch (sub_state)
    {
    case RW_T1T_SUBSTATE_NONE:
        return ("NONE");
    case RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE:
        return ("EXTRACT_TLV_VALUE");
    case RW_T1T_SUBSTATE_WAIT_READ_LOCKS:
        return ("READING_LOCKS");
    case RW_T1T_SUBSTATE_WAIT_READ_NDEF_BLOCK:
        return ("READ_NDEF_FINAL_BLOCK");
    case RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF:
        return ("INVALIDATING_NDEF");
    case RW_T1T_SUBSTATE_WAIT_NDEF_WRITE:
        return ("WRITE_NDEF_TLV_MESSAGE");
    case RW_T1T_SUBSTATE_WAIT_NDEF_UPDATED:
        return ("WAITING_RSP_FOR_LAST_NDEF_WRITE");
    case RW_T1T_SUBSTATE_WAIT_VALIDATE_NDEF:
        return ("VALIDATING_NDEF");
    case RW_T1T_SUBSTATE_WAIT_SET_CC_RWA_RO:
        return ("SET_RWA_RO");
    case RW_T1T_SUBSTATE_WAIT_SET_ST_LOCK_BITS:
        return ("SET_STATIC_LOCK_BITS");
    case RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS:
        return ("SET_DYNAMIC_LOCK_BITS");

    default:
        return ("???? UNKNOWN SUBSTATE");
    }
}
#endif /* (BT_TRACE_VERBOSE == TRUE) */

#endif /* (defined ((RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE)) */

#endif /* (NFC_INCLUDED == TRUE) */
