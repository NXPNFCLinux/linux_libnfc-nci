/******************************************************************************
 *
 *  Copyright (C) 2011-2014 Broadcom Corporation
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
 *  Copyright (C) 2014 NXP Semiconductors
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
 *  This file contains the implementation for ISO 15693 in Reader/Writer
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

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define RW_I93_TOUT_RESP                        RW_I93_MAX_RSP_TIMEOUT    /* Response timeout     */
#endif
#define RW_I93_TOUT_STAY_QUIET                  200     /* stay quiet timeout   */
#define RW_I93_READ_MULTI_BLOCK_SIZE            128     /* max reading data if read multi block is supported */
#define RW_I93_FORMAT_DATA_LEN                  8       /* CC, zero length NDEF, Terminator TLV              */
#define RW_I93_GET_MULTI_BLOCK_SEC_SIZE         253     /* max getting lock status if get multi block sec is supported */

/* main state */
enum
{
    RW_I93_STATE_NOT_ACTIVATED,         /* ISO15693 is not activated            */
    RW_I93_STATE_IDLE,                  /* waiting for upper layer API          */
    RW_I93_STATE_BUSY,                  /* waiting for response from tag        */

    RW_I93_STATE_DETECT_NDEF,           /* performing NDEF detection precedure  */
    RW_I93_STATE_READ_NDEF,             /* performing read NDEF procedure       */
    RW_I93_STATE_UPDATE_NDEF,           /* performing update NDEF procedure     */
    RW_I93_STATE_FORMAT,                /* performing format procedure          */
    RW_I93_STATE_SET_READ_ONLY,         /* performing set read-only procedure   */

    RW_I93_STATE_PRESENCE_CHECK         /* checking presence of tag             */
};

/* sub state */
enum
{
    RW_I93_SUBSTATE_WAIT_UID,               /* waiting for response of inventory    */
    RW_I93_SUBSTATE_WAIT_SYS_INFO,          /* waiting for response of get sys info */
    RW_I93_SUBSTATE_WAIT_CC,                /* waiting for reading CC               */
    RW_I93_SUBSTATE_SEARCH_NDEF_TLV,        /* searching NDEF TLV                   */
    RW_I93_SUBSTATE_CHECK_LOCK_STATUS,      /* check if any NDEF TLV is locked      */

    RW_I93_SUBSTATE_RESET_LEN,              /* set length to 0 to update NDEF TLV   */
    RW_I93_SUBSTATE_WRITE_NDEF,             /* writing NDEF and Terminator TLV      */
    RW_I93_SUBSTATE_UPDATE_LEN,             /* set length into NDEF TLV             */

    RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI,   /* reset DSFID and AFI                  */
    RW_I93_SUBSTATE_CHECK_READ_ONLY,        /* check if any block is locked         */
    RW_I93_SUBSTATE_WRITE_CC_NDEF_TLV,      /* write CC and empty NDEF/Terminator TLV */

    RW_I93_SUBSTATE_WAIT_UPDATE_CC,         /* updating CC as read-only             */
    RW_I93_SUBSTATE_LOCK_NDEF_TLV,          /* lock blocks of NDEF TLV              */
    RW_I93_SUBSTATE_WAIT_LOCK_CC            /* lock block of CC                     */
};

#if (BT_TRACE_VERBOSE == TRUE)
static char *rw_i93_get_state_name (UINT8 state);
static char *rw_i93_get_sub_state_name (UINT8 sub_state);
static char *rw_i93_get_tag_name (UINT8 product_version);
#endif

static void rw_i93_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data);
void rw_i93_handle_error (tNFC_STATUS status);
tNFC_STATUS rw_i93_send_cmd_get_sys_info (UINT8 *p_uid, UINT8 extra_flag);

/*******************************************************************************
**
** Function         rw_i93_get_product_version
**
** Description      Get product version from UID
**
** Returns          void
**
*******************************************************************************/
void rw_i93_get_product_version (UINT8 *p_uid)
{
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;

    if (!memcmp (p_i93->uid, p_uid, I93_UID_BYTE_LEN))
    {
        return;
    }

    RW_TRACE_DEBUG0 ("rw_i93_get_product_version ()");

    memcpy (p_i93->uid, p_uid, I93_UID_BYTE_LEN);

    if (p_uid[1] == I93_UID_IC_MFG_CODE_NXP)
    {
        if (p_uid[2] == I93_UID_ICODE_SLI)
            p_i93->product_version = RW_I93_ICODE_SLI;
        else if (p_uid[2] == I93_UID_ICODE_SLI_S)
            p_i93->product_version = RW_I93_ICODE_SLI_S;
        else if (p_uid[2] == I93_UID_ICODE_SLI_L)
            p_i93->product_version = RW_I93_ICODE_SLI_L;
        else
            p_i93->product_version = RW_I93_UNKNOWN_PRODUCT;
    }
    else if (p_uid[1] == I93_UID_IC_MFG_CODE_TI)
    {
        if ((p_uid[2] & I93_UID_TAG_IT_HF_I_PRODUCT_ID_MASK) == I93_UID_TAG_IT_HF_I_PLUS_INLAY)
            p_i93->product_version = RW_I93_TAG_IT_HF_I_PLUS_INLAY;
        else if ((p_uid[2] & I93_UID_TAG_IT_HF_I_PRODUCT_ID_MASK) == I93_UID_TAG_IT_HF_I_PLUS_CHIP)
            p_i93->product_version = RW_I93_TAG_IT_HF_I_PLUS_CHIP;
        else if ((p_uid[2] & I93_UID_TAG_IT_HF_I_PRODUCT_ID_MASK) == I93_UID_TAG_IT_HF_I_STD_CHIP_INLAY)
            p_i93->product_version = RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY;
        else if ((p_uid[2] & I93_UID_TAG_IT_HF_I_PRODUCT_ID_MASK) == I93_UID_TAG_IT_HF_I_PRO_CHIP_INLAY)
            p_i93->product_version = RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY;
        else
            p_i93->product_version = RW_I93_UNKNOWN_PRODUCT;
    }
    else if (  (p_uid[1] == I93_UID_IC_MFG_CODE_STM)
             &&(p_i93->info_flags & I93_INFO_FLAG_IC_REF)  )
    {
        if (p_i93->ic_reference == I93_IC_REF_STM_M24LR04E_R)
            p_i93->product_version = RW_I93_STM_M24LR04E_R;
        else if (p_i93->ic_reference == I93_IC_REF_STM_M24LR16E_R)
            p_i93->product_version = RW_I93_STM_M24LR16E_R;
        else if (p_i93->ic_reference == I93_IC_REF_STM_M24LR64E_R)
            p_i93->product_version = RW_I93_STM_M24LR64E_R;
        else
        {
            switch (p_i93->ic_reference & I93_IC_REF_STM_MASK)
            {
            case I93_IC_REF_STM_LRI1K:
                p_i93->product_version = RW_I93_STM_LRI1K;
                break;
            case I93_IC_REF_STM_LRI2K:
                p_i93->product_version = RW_I93_STM_LRI2K;
                break;
            case I93_IC_REF_STM_LRIS2K:
                p_i93->product_version = RW_I93_STM_LRIS2K;
                break;
            case I93_IC_REF_STM_LRIS64K:
                p_i93->product_version = RW_I93_STM_LRIS64K;
                break;
            case I93_IC_REF_STM_M24LR64_R:
                p_i93->product_version = RW_I93_STM_M24LR64_R;
                break;
            default:
                p_i93->product_version = RW_I93_UNKNOWN_PRODUCT;
                break;
            }
        }
    }
    else
    {
        p_i93->product_version = RW_I93_UNKNOWN_PRODUCT;
    }

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG1 ("product_version = <%s>", rw_i93_get_tag_name(p_i93->product_version));
#else
    RW_TRACE_DEBUG1 ("product_version = %d", p_i93->product_version);
#endif

    switch (p_i93->product_version)
    {
    case RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY:
    case RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY:
        /* these don't support Get System Information Command */
        /* these support only Inventory, Stay Quiet, Read Single Block, Write Single Block, Lock Block */
        p_i93->block_size = I93_TAG_IT_HF_I_STD_PRO_CHIP_INLAY_BLK_SIZE;
        p_i93->num_block  = I93_TAG_IT_HF_I_STD_PRO_CHIP_INLAY_NUM_USER_BLK;
        break;
    default:
        break;
    }
}

/*******************************************************************************
**
** Function         rw_i93_process_sys_info
**
** Description      Store system information of tag
**
** Returns          FALSE if retrying with protocol extension flag
**
*******************************************************************************/
BOOLEAN rw_i93_process_sys_info (UINT8* p_data)
{
    UINT8      *p     = p_data;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    UINT8      uid[I93_UID_BYTE_LEN], *p_uid;

    RW_TRACE_DEBUG0 ("rw_i93_process_sys_info ()");

    STREAM_TO_UINT8 (p_i93->info_flags, p);

    p_uid = uid;
    STREAM_TO_ARRAY8 (p_uid, p);

    if (p_i93->info_flags & I93_INFO_FLAG_DSFID)
    {
        STREAM_TO_UINT8 (p_i93->dsfid, p);
    }
    if (p_i93->info_flags & I93_INFO_FLAG_AFI)
    {
        STREAM_TO_UINT8 (p_i93->afi, p);
    }
    if (p_i93->info_flags & I93_INFO_FLAG_MEM_SIZE)
    {
        if (p_i93->intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
        {
            STREAM_TO_UINT16 (p_i93->num_block, p);
        }
        else
        {
            STREAM_TO_UINT8 (p_i93->num_block, p);
        }
        /* it is one less than actual number of bytes */
        p_i93->num_block += 1;

        STREAM_TO_UINT8 (p_i93->block_size, p);
        /* it is one less than actual number of blocks */
        p_i93->block_size = (p_i93->block_size & 0x1F) + 1;
    }
    if (p_i93->info_flags & I93_INFO_FLAG_IC_REF)
    {
        STREAM_TO_UINT8 (p_i93->ic_reference, p);

        /* clear existing UID to set product version */
        p_i93->uid[0] = 0x00;

        /* store UID and get product version */
        rw_i93_get_product_version (p_uid);

        if (p_i93->uid[0] == I93_UID_FIRST_BYTE)
        {
            if (  (p_i93->uid[1] == I93_UID_IC_MFG_CODE_NXP)
                &&(p_i93->ic_reference == I93_IC_REF_ICODE_SLI_L)  )
            {
                p_i93->num_block  = 8;
                p_i93->block_size = 4;
            }
            else if (p_i93->uid[1] == I93_UID_IC_MFG_CODE_STM)
            {
                /*
                **  LRI1K:      010000xx(b), blockSize: 4, numberBlocks: 0x20
                **  LRI2K:      001000xx(b), blockSize: 4, numberBlocks: 0x40
                **  LRIS2K:     001010xx(b), blockSize: 4, numberBlocks: 0x40
                **  LRIS64K:    010001xx(b), blockSize: 4, numberBlocks: 0x800
                **  M24LR64-R:  001011xx(b), blockSize: 4, numberBlocks: 0x800
                **  M24LR04E-R: 01011010(b), blockSize: 4, numberBlocks: 0x80
                **  M24LR16E-R: 01001110(b), blockSize: 4, numberBlocks: 0x200
                **  M24LR64E-R: 01011110(b), blockSize: 4, numberBlocks: 0x800
                */
                if (  (p_i93->product_version == RW_I93_STM_M24LR16E_R)
                    ||(p_i93->product_version == RW_I93_STM_M24LR64E_R)  )
                {
                    /*
                    ** M24LR16E-R or M24LR64E-R returns system information without memory size,
                    ** if option flag is not set.
                    ** LRIS64K and M24LR64-R return error if option flag is not set.
                    */
                    if (!(p_i93->intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK))
                    {
                        /* get memory size with protocol extension flag */
                        if (rw_i93_send_cmd_get_sys_info (NULL, I93_FLAG_PROT_EXT_YES) == NFC_STATUS_OK)
                        {
                            /* STM supports more than 2040 bytes */
                            p_i93->intl_flags |= RW_I93_FLAG_16BIT_NUM_BLOCK;

                            return FALSE;
                        }
                    }
                    return TRUE;
                }
                else if (  (p_i93->product_version == RW_I93_STM_LRI2K)
                         &&(p_i93->ic_reference    == 0x21)  )
                {
                    /* workaround of byte order in memory size information */
                    p_i93->num_block = 64;
                    p_i93->block_size = 4;
                }
            }
        }
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_i93_check_sys_info_prot_ext
**
** Description      Check if need to set protocol extension flag to get system info
**
** Returns          TRUE if sent Get System Info with protocol extension flag
**
*******************************************************************************/
BOOLEAN rw_i93_check_sys_info_prot_ext (UINT8 error_code)
{
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;

    RW_TRACE_DEBUG0 ("rw_i93_check_sys_info_prot_ext ()");

    if (  (p_i93->uid[1] == I93_UID_IC_MFG_CODE_STM)
        &&(p_i93->sent_cmd == I93_CMD_GET_SYS_INFO)
        &&(error_code == I93_ERROR_CODE_OPTION_NOT_SUPPORTED)
        &&(rw_i93_send_cmd_get_sys_info (NULL, I93_FLAG_PROT_EXT_YES) == NFC_STATUS_OK)  )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_to_upper
**
** Description      Send response to upper layer
**
** Returns          void
**
*******************************************************************************/
void rw_i93_send_to_upper (BT_HDR *p_resp)
{
    UINT8      *p = (UINT8 *) (p_resp + 1) + p_resp->offset, *p_uid;
    UINT16     length = p_resp->len;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    tRW_DATA   rw_data;
    UINT8      event = RW_I93_MAX_EVT;
    UINT8      flags;
    BT_HDR     *p_buff;

    RW_TRACE_DEBUG0 ("rw_i93_send_to_upper ()");

    STREAM_TO_UINT8 (flags, p);
    length--;

    if (flags & I93_FLAG_ERROR_DETECTED)
    {
        if ((length) && (rw_i93_check_sys_info_prot_ext(*p)))
        {
            /* getting system info with protocol extension flag */
            /* This STM tag supports more than 2040 bytes */
            p_i93->intl_flags |= RW_I93_FLAG_16BIT_NUM_BLOCK;
            p_i93->state = RW_I93_STATE_BUSY;
        }
        else
        {
            /* notify error to upper layer */
            rw_data.i93_cmd_cmpl.status  = NFC_STATUS_FAILED;
            rw_data.i93_cmd_cmpl.command = p_i93->sent_cmd;
            STREAM_TO_UINT8 (rw_data.i93_cmd_cmpl.error_code, p);

            rw_cb.tcb.i93.sent_cmd = 0;
            (*(rw_cb.p_cback)) (RW_I93_CMD_CMPL_EVT, &rw_data);
        }
        return;
    }

    switch (p_i93->sent_cmd)
    {
    case I93_CMD_INVENTORY:

        /* forward inventory response */
        rw_data.i93_inventory.status = NFC_STATUS_OK;
        STREAM_TO_UINT8 (rw_data.i93_inventory.dsfid, p);

        p_uid = rw_data.i93_inventory.uid;
        STREAM_TO_ARRAY8 (p_uid, p);

        /* store UID and get product version */
        rw_i93_get_product_version (p_uid);

        event = RW_I93_INVENTORY_EVT;
        break;

    case I93_CMD_READ_SINGLE_BLOCK:
    case I93_CMD_READ_MULTI_BLOCK:
    case I93_CMD_GET_MULTI_BLK_SEC:

        /* forward tag data or security status */
        p_buff = (BT_HDR*) GKI_getbuf ((UINT16) (length + BT_HDR_SIZE));

        if (p_buff)
        {
            p_buff->offset = 0;
            p_buff->len    = length;

            memcpy ((p_buff + 1), p, length);

            rw_data.i93_data.status  = NFC_STATUS_OK;
            rw_data.i93_data.command = p_i93->sent_cmd;
            rw_data.i93_data.p_data  = p_buff;

            event = RW_I93_DATA_EVT;
        }
        else
        {
            rw_data.i93_cmd_cmpl.status     = NFC_STATUS_NO_BUFFERS;
            rw_data.i93_cmd_cmpl.command    = p_i93->sent_cmd;
            rw_data.i93_cmd_cmpl.error_code = 0;

            event = RW_I93_CMD_CMPL_EVT;
        }
        break;

    case I93_CMD_WRITE_SINGLE_BLOCK:
    case I93_CMD_LOCK_BLOCK:
    case I93_CMD_WRITE_MULTI_BLOCK:
    case I93_CMD_SELECT:
    case I93_CMD_RESET_TO_READY:
    case I93_CMD_WRITE_AFI:
    case I93_CMD_LOCK_AFI:
    case I93_CMD_WRITE_DSFID:
    case I93_CMD_LOCK_DSFID:

        /* notify the complete of command */
        rw_data.i93_cmd_cmpl.status     = NFC_STATUS_OK;
        rw_data.i93_cmd_cmpl.command    = p_i93->sent_cmd;
        rw_data.i93_cmd_cmpl.error_code = 0;

        event = RW_I93_CMD_CMPL_EVT;
        break;

    case I93_CMD_GET_SYS_INFO:

        if (rw_i93_process_sys_info (p))
        {
            rw_data.i93_sys_info.status     = NFC_STATUS_OK;
            rw_data.i93_sys_info.info_flags = p_i93->info_flags;
            rw_data.i93_sys_info.dsfid      = p_i93->dsfid;
            rw_data.i93_sys_info.afi        = p_i93->afi;
            rw_data.i93_sys_info.num_block  = p_i93->num_block;
            rw_data.i93_sys_info.block_size = p_i93->block_size;
            rw_data.i93_sys_info.IC_reference = p_i93->ic_reference;

            memcpy (rw_data.i93_sys_info.uid, p_i93->uid, I93_UID_BYTE_LEN);

            event = RW_I93_SYS_INFO_EVT;
        }
        else
        {
            /* retrying with protocol extension flag */
            p_i93->state = RW_I93_STATE_BUSY;
            return;
        }
        break;

    default:
        break;
    }

    rw_cb.tcb.i93.sent_cmd = 0;
    if (event != RW_I93_MAX_EVT)
    {
        (*(rw_cb.p_cback)) (event, &rw_data);
    }
    else
    {
        RW_TRACE_ERROR0 ("rw_i93_send_to_upper (): Invalid response");
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_to_lower
**
** Description      Send Request frame to lower layer
**
** Returns          TRUE if success
**
*******************************************************************************/
BOOLEAN rw_i93_send_to_lower (BT_HDR *p_msg)
{
#if (BT_TRACE_PROTOCOL == TRUE)
    DispRWI93Tag (p_msg, FALSE, 0x00);
#endif

    /* store command for retransmitting */
    if (rw_cb.tcb.i93.p_retry_cmd)
    {
        GKI_freebuf (rw_cb.tcb.i93.p_retry_cmd);
        rw_cb.tcb.i93.p_retry_cmd = NULL;
    }

    rw_cb.tcb.i93.p_retry_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (rw_cb.tcb.i93.p_retry_cmd)
    {
        memcpy (rw_cb.tcb.i93.p_retry_cmd, p_msg, sizeof (BT_HDR) + p_msg->offset + p_msg->len);
    }

    if (NFC_SendData (NFC_RF_CONN_ID, p_msg) != NFC_STATUS_OK)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_to_lower (): NFC_SendData () failed");
        return FALSE;
    }

    nfc_start_quick_timer (&rw_cb.tcb.i93.timer, NFC_TTYPE_RW_I93_RESPONSE,
                           (RW_I93_TOUT_RESP*QUICK_TIMER_TICKS_PER_SEC)/1000);

    return TRUE;
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_inventory
**
** Description      Send Inventory Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_inventory (UINT8 *p_uid, BOOLEAN including_afi, UINT8 afi)
{
    BT_HDR      *p_cmd;
    UINT8       *p, flags;

    RW_TRACE_DEBUG2 ("rw_i93_send_cmd_inventory () including_afi:%d, AFI:0x%02X", including_afi, afi);

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_inventory (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 3;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    flags = (I93_FLAG_SLOT_ONE | I93_FLAG_INVENTORY_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE);
    if (including_afi)
    {
        flags |= I93_FLAG_AFI_PRESENT;
    }

    UINT8_TO_STREAM (p, flags);

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_INVENTORY);

    if (including_afi)
    {
        /* Parameters */
        UINT8_TO_STREAM (p, afi);    /* Optional AFI */
        p_cmd->len++;
    }

    if (p_uid)
    {
        UINT8_TO_STREAM  (p, I93_UID_BYTE_LEN*8);         /* Mask Length */
        ARRAY8_TO_STREAM (p, p_uid);                      /* UID */
        p_cmd->len += I93_UID_BYTE_LEN;
    }
    else
    {
        UINT8_TO_STREAM (p, 0x00);   /* Mask Length */
    }

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_INVENTORY;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_stay_quiet
**
** Description      Send Stay Quiet Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_stay_quiet (void)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_stay_quiet ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_stay_quiet (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 10;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_STAY_QUIET);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_STAY_QUIET;

        /* restart timer for stay quiet */
        nfc_start_quick_timer (&rw_cb.tcb.i93.timer, NFC_TTYPE_RW_I93_RESPONSE,
                               (RW_I93_TOUT_STAY_QUIET * QUICK_TIMER_TICKS_PER_SEC) / 1000);
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_read_single_block
**
** Description      Send Read Single Block Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_read_single_block (UINT16 block_number, BOOLEAN read_security)
{
    BT_HDR      *p_cmd;
    UINT8       *p, flags;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_read_single_block ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_read_single_block (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 11;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    flags = (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE);

    if (read_security)
        flags |= I93_FLAG_OPTION_SET;

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
        flags |= I93_FLAG_PROT_EXT_YES;

    UINT8_TO_STREAM (p, flags);

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_READ_SINGLE_BLOCK);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);     /* UID */

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
    {
        UINT16_TO_STREAM (p, block_number);          /* Block number */
        p_cmd->len++;
    }
    else
    {
        UINT8_TO_STREAM (p, block_number);          /* Block number */
    }

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_READ_SINGLE_BLOCK;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_write_single_block
**
** Description      Send Write Single Block Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_write_single_block (UINT16 block_number, UINT8 *p_data)
{
    BT_HDR      *p_cmd;
    UINT8       *p, flags;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_write_single_block ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_write_single_block (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 11 + rw_cb.tcb.i93.block_size;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    if (  (rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PLUS_INLAY)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PLUS_CHIP)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
    {
        /* Option must be set for TI tag */
        flags = (I93_FLAG_ADDRESS_SET | I93_FLAG_OPTION_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE);
    }
    else
    {
        flags = (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE);
    }

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
        flags |= I93_FLAG_PROT_EXT_YES;

    UINT8_TO_STREAM (p, flags);

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_WRITE_SINGLE_BLOCK);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
    {
        UINT16_TO_STREAM (p, block_number);          /* Block number */
        p_cmd->len++;
    }
    else
    {
        UINT8_TO_STREAM (p, block_number);          /* Block number */
    }


    /* Data */
    ARRAY_TO_STREAM (p, p_data, rw_cb.tcb.i93.block_size);

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_WRITE_SINGLE_BLOCK;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_lock_block
**
** Description      Send Lock Block Request to VICC
**
**                  STM LRIS64K, M24LR64-R, M24LR04E-R, M24LR16E-R, M24LR64E-R
**                  do not support.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_lock_block (UINT8 block_number)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_lock_block ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_lock_block (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 11;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    if (  (rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PLUS_INLAY)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PLUS_CHIP)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
    {
        /* Option must be set for TI tag */
        UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | I93_FLAG_OPTION_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));
    }
    else
    {
        UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));
    }

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_LOCK_BLOCK);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */
    UINT8_TO_STREAM (p, block_number);         /* Block number */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_LOCK_BLOCK;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_read_multi_blocks
**
** Description      Send Read Multiple Blocks Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_read_multi_blocks (UINT16 first_block_number, UINT16 number_blocks)
{
    BT_HDR      *p_cmd;
    UINT8       *p, flags;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_read_multi_blocks ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_read_multi_blocks (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 12;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    flags = (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE);

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
        flags |= I93_FLAG_PROT_EXT_YES;

    UINT8_TO_STREAM (p, flags);

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_READ_MULTI_BLOCK);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
    {
        UINT16_TO_STREAM (p, first_block_number);   /* First block number */
        p_cmd->len++;
    }
    else
    {
        UINT8_TO_STREAM (p, first_block_number);   /* First block number */
    }

    UINT8_TO_STREAM (p, number_blocks - 1);    /* Number of blocks, 0x00 to read one block */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_READ_MULTI_BLOCK;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_write_multi_blocks
**
** Description      Send Write Multiple Blocks Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_write_multi_blocks (UINT8  first_block_number,
                                                UINT16 number_blocks,
                                                UINT8 *p_data)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_write_multi_blocks ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_write_multi_blocks (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 12 + number_blocks * rw_cb.tcb.i93.block_size;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_WRITE_MULTI_BLOCK);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);   /* UID */
    UINT8_TO_STREAM (p, first_block_number);   /* First block number */
    UINT8_TO_STREAM (p, number_blocks - 1);    /* Number of blocks, 0x00 to read one block */

    /* Data */
    ARRAY_TO_STREAM (p, p_data, number_blocks * rw_cb.tcb.i93.block_size);

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_WRITE_MULTI_BLOCK;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_select
**
** Description      Send Select Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_select (UINT8 *p_uid)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_select ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_select (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 10 ;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_SELECT);

    /* Parameters */
    ARRAY8_TO_STREAM (p, p_uid);    /* UID */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_SELECT;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_reset_to_ready
**
** Description      Send Reset to Ready Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_reset_to_ready (void)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_reset_to_ready ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_reset_to_ready (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 10 ;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_RESET_TO_READY);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_RESET_TO_READY;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_write_afi
**
** Description      Send Write AFI Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_write_afi (UINT8 afi)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_write_afi ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_write_afi (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 11;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_WRITE_AFI);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */
    UINT8_TO_STREAM (p, afi);                  /* AFI */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_WRITE_AFI;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_lock_afi
**
** Description      Send Lock AFI Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_lock_afi (void)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_lock_afi ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_lock_afi (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 10;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_LOCK_AFI);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_LOCK_AFI;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_write_dsfid
**
** Description      Send Write DSFID Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_write_dsfid (UINT8 dsfid)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_write_dsfid ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_write_dsfid (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 11;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_WRITE_DSFID);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */
    UINT8_TO_STREAM (p, dsfid);                /* DSFID */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_WRITE_DSFID;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_lock_dsfid
**
** Description      Send Lock DSFID Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_lock_dsfid (void)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_lock_dsfid ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_lock_dsfid (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 10;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_LOCK_DSFID);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_LOCK_DSFID;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_get_sys_info
**
** Description      Send Get System Information Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_get_sys_info (UINT8 *p_uid, UINT8 extra_flags)
{
    BT_HDR      *p_cmd;
    UINT8       *p;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_get_sys_info ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_get_sys_info (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 10;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    UINT8_TO_STREAM (p, (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE | extra_flags));

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_GET_SYS_INFO);

    /* Parameters */
    if (p_uid)
    {
        ARRAY8_TO_STREAM (p, p_uid);               /* UID */
    }
    else
    {
        ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);   /* UID */
    }

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_GET_SYS_INFO;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_send_cmd_get_multi_block_sec
**
** Description      Send Get Multiple Block Security Status Request to VICC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_send_cmd_get_multi_block_sec (UINT16 first_block_number,
                                                 UINT16 number_blocks)
{
    BT_HDR      *p_cmd;
    UINT8       *p, flags;

    RW_TRACE_DEBUG0 ("rw_i93_send_cmd_get_multi_block_sec ()");

    p_cmd = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);

    if (!p_cmd)
    {
        RW_TRACE_ERROR0 ("rw_i93_send_cmd_get_multi_block_sec (): Cannot allocate buffer");
        return NFC_STATUS_NO_BUFFERS;
    }

    p_cmd->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
    p_cmd->len    = 12;
    p = (UINT8 *) (p_cmd + 1) + p_cmd->offset;

    /* Flags */
    flags = (I93_FLAG_ADDRESS_SET | RW_I93_FLAG_SUB_CARRIER | RW_I93_FLAG_DATA_RATE);

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
        flags |= I93_FLAG_PROT_EXT_YES;

    UINT8_TO_STREAM (p, flags);

    /* Command Code */
    UINT8_TO_STREAM (p, I93_CMD_GET_MULTI_BLK_SEC);

    /* Parameters */
    ARRAY8_TO_STREAM (p, rw_cb.tcb.i93.uid);    /* UID */

    if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_16BIT_NUM_BLOCK)
    {
        UINT16_TO_STREAM (p, first_block_number);   /* First block number */
        UINT16_TO_STREAM (p, number_blocks - 1);    /* Number of blocks, 0x00 to read one block */
        p_cmd->len += 2;
    }
    else
    {
        UINT8_TO_STREAM (p, first_block_number);   /* First block number */
        UINT8_TO_STREAM (p, number_blocks - 1);    /* Number of blocks, 0x00 to read one block */
    }

    if (rw_i93_send_to_lower (p_cmd))
    {
        rw_cb.tcb.i93.sent_cmd  = I93_CMD_GET_MULTI_BLK_SEC;
        return NFC_STATUS_OK;
    }
    else
    {
        return NFC_STATUS_FAILED;
    }
}

/*******************************************************************************
**
** Function         rw_i93_get_next_blocks
**
** Description      Read as many blocks as possible (up to RW_I93_READ_MULTI_BLOCK_SIZE)
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_get_next_blocks (UINT16 offset)
{
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    UINT16     first_block;
    UINT16     num_block;

    RW_TRACE_DEBUG0 ("rw_i93_get_next_blocks ()");

    first_block = offset / p_i93->block_size;

    /* more blocks, more efficent but more error rate */

    if (p_i93->intl_flags & RW_I93_FLAG_READ_MULTI_BLOCK)
    {
        num_block = RW_I93_READ_MULTI_BLOCK_SIZE / p_i93->block_size;

        if (num_block + first_block > p_i93->num_block)
            num_block = p_i93->num_block - first_block;

        if (p_i93->uid[1] == I93_UID_IC_MFG_CODE_STM)
        {
            /* LRIS64K, M24LR64-R, M24LR04E-R, M24LR16E-R, M24LR64E-R requires
            **      The max number of blocks is 32 and they are all located in the same sector.
            **      The sector is 32 blocks of 4 bytes.
            */
            if (  (p_i93->product_version == RW_I93_STM_LRIS64K)
                ||(p_i93->product_version == RW_I93_STM_M24LR64_R)
                ||(p_i93->product_version == RW_I93_STM_M24LR04E_R)
                ||(p_i93->product_version == RW_I93_STM_M24LR16E_R)
                ||(p_i93->product_version == RW_I93_STM_M24LR64E_R)  )
            {
                if (num_block > I93_STM_MAX_BLOCKS_PER_READ)
                    num_block = I93_STM_MAX_BLOCKS_PER_READ;

                if ((first_block / I93_STM_BLOCKS_PER_SECTOR)
                    != ((first_block + num_block - 1) / I93_STM_BLOCKS_PER_SECTOR))
                {
                    num_block = I93_STM_BLOCKS_PER_SECTOR - (first_block % I93_STM_BLOCKS_PER_SECTOR);
                }
            }
        }

        return rw_i93_send_cmd_read_multi_blocks (first_block, num_block);
    }
    else
    {
        return rw_i93_send_cmd_read_single_block (first_block, FALSE);
    }
}

/*******************************************************************************
**
** Function         rw_i93_get_next_block_sec
**
** Description      Get as many security of blocks as possible from p_i93->rw_offset
**                  (up to RW_I93_GET_MULTI_BLOCK_SEC_SIZE)
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS rw_i93_get_next_block_sec (void)
{
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    UINT16     num_blocks;

    RW_TRACE_DEBUG0 ("rw_i93_get_next_block_sec ()");

    if (p_i93->num_block <= p_i93->rw_offset)
    {
        RW_TRACE_ERROR2 ("rw_offset(0x%x) must be less than num_block(0x%x)",
                         p_i93->rw_offset, p_i93->num_block);
        return NFC_STATUS_FAILED;
    }

    num_blocks = p_i93->num_block - p_i93->rw_offset;

    if (num_blocks > RW_I93_GET_MULTI_BLOCK_SEC_SIZE)
        num_blocks = RW_I93_GET_MULTI_BLOCK_SEC_SIZE;

    return rw_i93_send_cmd_get_multi_block_sec (p_i93->rw_offset, num_blocks);
}

/*******************************************************************************
**
** Function         rw_i93_sm_detect_ndef
**
** Description      Process NDEF detection procedure
**
**                  1. Get UID if not having yet
**                  2. Get System Info if not having yet
**                  3. Read first block for CC
**                  4. Search NDEF Type and length
**                  5. Get block status to get max NDEF size and read-only status
**
** Returns          void
**
*******************************************************************************/
void rw_i93_sm_detect_ndef (BT_HDR *p_resp)
{
    UINT8      *p = (UINT8 *) (p_resp + 1) + p_resp->offset, *p_uid;
    UINT8       flags, u8 = 0, cc[4];
    UINT16      length = p_resp->len, xx, block, first_block, last_block, num_blocks;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    tRW_DATA    rw_data;
    tNFC_STATUS status = NFC_STATUS_FAILED;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_i93_sm_detect_ndef () sub_state:%s (0x%x)",
                      rw_i93_get_sub_state_name (p_i93->sub_state), p_i93->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_i93_sm_detect_ndef () sub_state:0x%x", p_i93->sub_state);
#endif

    STREAM_TO_UINT8 (flags, p);
    length--;

    if (flags & I93_FLAG_ERROR_DETECTED)
    {
        if ((length) && (rw_i93_check_sys_info_prot_ext(*p)))
        {
            /* getting system info with protocol extension flag */
            /* This STM tag supports more than 2040 bytes */
            p_i93->intl_flags |= RW_I93_FLAG_16BIT_NUM_BLOCK;
        }
        else
        {
            RW_TRACE_DEBUG1 ("Got error flags (0x%02x)", flags);
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        return;
    }

    switch (p_i93->sub_state)
    {
    case RW_I93_SUBSTATE_WAIT_UID:

        STREAM_TO_UINT8 (u8, p); /* DSFID */
        p_uid = p_i93->uid;
        STREAM_TO_ARRAY8 (p_uid, p);

        if (u8 != I93_DFS_UNSUPPORTED)
        {
            /* if Data Storage Format is unknown */
            RW_TRACE_DEBUG1 ("Got unknown DSFID (0x%02x)", u8);
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        else
        {
            /* get system information to get memory size */
            if (rw_i93_send_cmd_get_sys_info (NULL, I93_FLAG_PROT_EXT_NO) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_SYS_INFO;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        break;

    case RW_I93_SUBSTATE_WAIT_SYS_INFO:

        p_i93->block_size = 0;
        p_i93->num_block  = 0;

        if (!rw_i93_process_sys_info (p))
        {
            /* retrying with protocol extension flag */
            break;
        }

        if ((p_i93->block_size == 0)||(p_i93->num_block == 0))
        {
            RW_TRACE_DEBUG0 ("Unable to get tag memory size");
            rw_i93_handle_error (status);
        }
        else
        {
            /* read CC in the first block */
            if (rw_i93_send_cmd_read_single_block (0x0000, FALSE) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_CC;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        break;

    case RW_I93_SUBSTATE_WAIT_CC:

        /* assume block size is more than 4 */
        STREAM_TO_ARRAY (cc, p, 4);

        status = NFC_STATUS_FAILED;

        /*
        ** Capability Container (CC)
        **
        ** CC[0] : magic number (0xE1)
        ** CC[1] : Bit 7-6:Major version number
        **       : Bit 5-4:Minor version number
        **       : Bit 3-2:Read access condition    (00b: read access granted without any security)
        **       : Bit 1-0:Write access condition   (00b: write access granted without any security)
        ** CC[2] : Memory size in 8 bytes (Ex. 0x04 is 32 bytes) [STM, set to 0xFF if more than 2040bytes]
        ** CC[3] : Bit 0:Read multiple blocks is supported [NXP, STM]
        **       : Bit 1:Inventory page read is supported [NXP]
        **       : Bit 2:More than 2040 bytes are supported [STM]
        */

        RW_TRACE_DEBUG4 ("rw_i93_sm_detect_ndef (): cc: 0x%02X 0x%02X 0x%02X 0x%02X", cc[0], cc[1], cc[2], cc[3]);
        RW_TRACE_DEBUG2 ("rw_i93_sm_detect_ndef (): Total blocks:0x%04X, Block size:0x%02X", p_i93->num_block, p_i93->block_size );

        if (  (cc[0] == I93_ICODE_CC_MAGIC_NUMER)
            &&(  (cc[3] & I93_STM_CC_OVERFLOW_MASK)
               ||(cc[2] * 8) == (p_i93->num_block * p_i93->block_size)  )  )
        {
            if ((cc[1] & I93_ICODE_CC_READ_ACCESS_MASK) == I93_ICODE_CC_READ_ACCESS_GRANTED)
            {
                if ((cc[1] & I93_ICODE_CC_WRITE_ACCESS_MASK) != I93_ICODE_CC_WRITE_ACCESS_GRANTED)
                {
                    /* read-only or password required to write */
                    p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
                }
                if (cc[3] & I93_ICODE_CC_MBREAD_MASK)
                {
                    /* tag supports read multi blocks command */
                    p_i93->intl_flags |= RW_I93_FLAG_READ_MULTI_BLOCK;
                }
                status = NFC_STATUS_OK;
            }
        }

        if (status == NFC_STATUS_OK)
        {
            /* seach NDEF TLV from offset 4 */
            p_i93->rw_offset = 4;

            if (rw_i93_get_next_blocks (p_i93->rw_offset) == NFC_STATUS_OK)
            {
                p_i93->sub_state        = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
                p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_SEARCH_NDEF_TLV:

        /* search TLV within read blocks */
        for (xx = 0; xx < length; xx++)
        {
            /* if looking for type */
            if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_TYPE)
            {
                if (*(p + xx) == I93_ICODE_TLV_TYPE_NULL)
                {
                    continue;
                }
                else if (  (*(p + xx) == I93_ICODE_TLV_TYPE_NDEF)
                         ||(*(p + xx) == I93_ICODE_TLV_TYPE_PROP)  )
                {
                    /* store found type and get length field */
                    p_i93->tlv_type = *(p + xx);
                    p_i93->ndef_tlv_start_offset = p_i93->rw_offset + xx;

                    p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_1;
                }
                else if (*(p + xx) == I93_ICODE_TLV_TYPE_TERM)
                {
                    /* no NDEF TLV found */
                    p_i93->tlv_type = I93_ICODE_TLV_TYPE_TERM;
                    break;
                }
                else
                {
                    RW_TRACE_DEBUG1 ("Invalid type: 0x%02x", *(p + xx));
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                    return;
                }
            }
            else if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_LENGTH_1)
            {
                /* if 3 bytes length field */
                if (*(p + xx) == 0xFF)
                {
                    /* need 2 more bytes for length field */
                    p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_2;
                }
                else
                {
                    p_i93->tlv_length = *(p + xx);
                    p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_VALUE;

                    if (p_i93->tlv_type == I93_ICODE_TLV_TYPE_NDEF)
                    {
                        p_i93->ndef_tlv_last_offset = p_i93->ndef_tlv_start_offset + 1 + p_i93->tlv_length;
                        break;
                    }
                }
            }
            else if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_LENGTH_2)
            {
                /* the second byte of 3 bytes length field */
                p_i93->tlv_length = *(p + xx);
                p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_LENGTH_3;
            }
            else if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_LENGTH_3)
            {
                /* the last byte of 3 bytes length field */
                p_i93->tlv_length = (p_i93->tlv_length << 8) + *(p + xx);
                p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_VALUE;

                if (p_i93->tlv_type == I93_ICODE_TLV_TYPE_NDEF)
                {
                    p_i93->ndef_tlv_last_offset = p_i93->ndef_tlv_start_offset + 3 + p_i93->tlv_length;
                    break;
                }
            }
            else if (p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_VALUE)
            {
                /* this is other than NDEF TLV */
                if (p_i93->tlv_length <= length - xx)
                {
                    /* skip value field */
                    xx += (UINT8)p_i93->tlv_length;
                    p_i93->tlv_detect_state = RW_I93_TLV_DETECT_STATE_TYPE;
                }
                else
                {
                    /* read more data */
                    p_i93->tlv_length -= (length - xx);
                    break;
                }
            }
        }

        /* found NDEF TLV and read length field */
        if (  (p_i93->tlv_type == I93_ICODE_TLV_TYPE_NDEF)
            &&(p_i93->tlv_detect_state == RW_I93_TLV_DETECT_STATE_VALUE)  )
        {
            p_i93->ndef_length = p_i93->tlv_length;

            /* get lock status to see if read-only */
            if (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
                ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)
                ||((p_i93->uid[1] == I93_UID_IC_MFG_CODE_NXP) && (p_i93->ic_reference & I93_ICODE_IC_REF_MBREAD_MASK))  )
            {
                /* these doesn't support GetMultiBlockSecurityStatus */

                p_i93->rw_offset = p_i93->ndef_tlv_start_offset;
                first_block = p_i93->ndef_tlv_start_offset / p_i93->block_size;

                /* read block to get lock status */
                rw_i93_send_cmd_read_single_block (first_block, TRUE);
                p_i93->sub_state = RW_I93_SUBSTATE_CHECK_LOCK_STATUS;
            }
            else
            {
                /* block offset for read-only check */
                p_i93->rw_offset = 0;

                if (rw_i93_get_next_block_sec () == NFC_STATUS_OK)
                {
                    p_i93->sub_state = RW_I93_SUBSTATE_CHECK_LOCK_STATUS;
                }
                else
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
        }
        else
        {
            /* read more data */
            p_i93->rw_offset += length;

            if (p_i93->rw_offset >= p_i93->block_size * p_i93->num_block)
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
            else if (rw_i93_get_next_blocks (p_i93->rw_offset) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_SEARCH_NDEF_TLV;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        break;

    case RW_I93_SUBSTATE_CHECK_LOCK_STATUS:

        if (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
            ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)
            ||((p_i93->uid[1] == I93_UID_IC_MFG_CODE_NXP) && (p_i93->ic_reference & I93_ICODE_IC_REF_MBREAD_MASK))  )
        {
            /* these doesn't support GetMultiBlockSecurityStatus */

            block = (p_i93->rw_offset / p_i93->block_size);
            last_block  = (p_i93->ndef_tlv_last_offset / p_i93->block_size);

            if ((*p) & I93_BLOCK_LOCKED)
            {
                if (block <= last_block)
                {
                    p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
                }
            }
            else
            {
                /* if we need to check more user blocks */
                if (block + 1 < p_i93->num_block)
                {
                    p_i93->rw_offset += p_i93->block_size;

                    /* read block to get lock status */
                    rw_i93_send_cmd_read_single_block ((UINT16)(p_i93->rw_offset / p_i93->block_size), TRUE);
                    break;
                }
            }

            p_i93->max_ndef_length = p_i93->ndef_length
                                     /* add available bytes including the last block of NDEF TLV */
                                     + (p_i93->block_size * (block - last_block) + 1)
                                     - (p_i93->ndef_tlv_last_offset % p_i93->block_size)
                                     - 1;
        }
        else
        {
            if (p_i93->rw_offset == 0)
            {
                p_i93->max_ndef_length = p_i93->ndef_length
                                         /* add available bytes in the last block of NDEF TLV */
                                         + p_i93->block_size
                                         - (p_i93->ndef_tlv_last_offset % p_i93->block_size)
                                         - 1;

                first_block = (p_i93->ndef_tlv_start_offset / p_i93->block_size);
            }
            else
            {
                first_block = 0;
            }

            last_block  = (p_i93->ndef_tlv_last_offset / p_i93->block_size);
            num_blocks  = length;

            for (block = first_block; block < num_blocks; block++)
            {
                /* if any block of NDEF TLV is locked */
                if ((block + p_i93->rw_offset) <= last_block)
                {
                    if (*(p + block) & I93_BLOCK_LOCKED)
                    {
                        p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
                        break;
                    }
                }
                else
                {
                    if (*(p + block) & I93_BLOCK_LOCKED)
                    {
                        /* no more consecutive unlocked block */
                        break;
                    }
                    else
                    {
                        /* add block size if not locked */
                        p_i93->max_ndef_length += p_i93->block_size;
                    }
                }
            }

            /* update next security of block to check */
            p_i93->rw_offset += num_blocks;

            /* if need to check more */
            if (p_i93->num_block > p_i93->rw_offset)
            {
                if (rw_i93_get_next_block_sec () != NFC_STATUS_OK)
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
                break;
            }
        }

        /* check if need to adjust max NDEF length */
        if (  (p_i93->ndef_length < 0xFF)
            &&(p_i93->max_ndef_length >= 0xFF)  )
        {
            /* 3 bytes length field must be used */
            p_i93->max_ndef_length -= 2;
        }

        rw_data.ndef.status     = NFC_STATUS_OK;
        rw_data.ndef.protocol   = NFC_PROTOCOL_15693;
        rw_data.ndef.flags      = 0;
        rw_data.ndef.flags      |= RW_NDEF_FL_SUPPORTED;
        rw_data.ndef.flags      |= RW_NDEF_FL_FORMATED;
        rw_data.ndef.flags      |= RW_NDEF_FL_FORMATABLE;
        rw_data.ndef.cur_size   = p_i93->ndef_length;

        if (p_i93->intl_flags & RW_I93_FLAG_READ_ONLY)
        {
            rw_data.ndef.flags    |= RW_NDEF_FL_READ_ONLY;
            rw_data.ndef.max_size  = p_i93->ndef_length;
        }
        else
        {
            rw_data.ndef.flags    |= RW_NDEF_FL_HARD_LOCKABLE;
            rw_data.ndef.max_size  = p_i93->max_ndef_length;
        }

        p_i93->state    = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;

        RW_TRACE_DEBUG3 ("NDEF cur_size(%d),max_size (%d), flags (0x%x)",
                         rw_data.ndef.cur_size,
                         rw_data.ndef.max_size,
                         rw_data.ndef.flags);

        (*(rw_cb.p_cback)) (RW_I93_NDEF_DETECT_EVT, &rw_data);
        break;

    default:
        break;
    }
}

/*******************************************************************************
**
** Function         rw_i93_sm_read_ndef
**
** Description      Process NDEF read procedure
**
** Returns          void
**
*******************************************************************************/
void rw_i93_sm_read_ndef (BT_HDR *p_resp)
{
    UINT8      *p = (UINT8 *) (p_resp + 1) + p_resp->offset;
    UINT8       flags;
    UINT16      offset, length = p_resp->len;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    tRW_DATA    rw_data;

    RW_TRACE_DEBUG0 ("rw_i93_sm_read_ndef ()");

    STREAM_TO_UINT8 (flags, p);
    length--;

    if (flags & I93_FLAG_ERROR_DETECTED)
    {
        RW_TRACE_DEBUG1 ("Got error flags (0x%02x)", flags);
        rw_i93_handle_error (NFC_STATUS_FAILED);
        return;
    }

    /* if this is the first block */
    if (p_i93->rw_length == 0)
    {
        /* get start of NDEF in the first block */
        offset = p_i93->ndef_tlv_start_offset % p_i93->block_size;

        if (p_i93->ndef_length < 0xFF)
        {
            offset += 2;
        }
        else
        {
            offset += 4;
        }

        /* adjust offset if read more blocks because the first block doesn't have NDEF */
        offset -= (p_i93->rw_offset - p_i93->ndef_tlv_start_offset);
    }
    else
    {
        offset = 0;
    }

    /* if read enough data to skip type and length field for the beginning */
    if (offset < length)
    {
        offset++; /* flags */
        p_resp->offset += offset;
        p_resp->len    -= offset;

        rw_data.data.status = NFC_STATUS_OK;
        rw_data.data.p_data = p_resp;

        p_i93->rw_length += p_resp->len;
    }

    /* if read all of NDEF data */
    if (p_i93->rw_length >= p_i93->ndef_length)
    {
        /* remove extra btyes in the last block */
        p_resp->len -= (p_i93->rw_length - p_i93->ndef_length);

        p_i93->state    = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;

        RW_TRACE_DEBUG2 ("NDEF read complete read (%d)/total (%d)",
                         p_resp->len,
                         p_i93->ndef_length);

        (*(rw_cb.p_cback)) (RW_I93_NDEF_READ_CPLT_EVT, &rw_data);
    }
    else
    {
        RW_TRACE_DEBUG2 ("NDEF read segment read (%d)/total (%d)",
                         p_resp->len,
                         p_i93->ndef_length);

        (*(rw_cb.p_cback)) (RW_I93_NDEF_READ_EVT, &rw_data);

        /* this will make read data from next block */
        p_i93->rw_offset += length;

        if (rw_i93_get_next_blocks (p_i93->rw_offset) != NFC_STATUS_OK)
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
    }
}

/*******************************************************************************
**
** Function         rw_i93_sm_update_ndef
**
** Description      Process NDEF update procedure
**
**                  1. Set length field to zero
**                  2. Write NDEF and Terminator TLV
**                  3. Set length field to NDEF length
**
** Returns          void
**
*******************************************************************************/
void rw_i93_sm_update_ndef (BT_HDR *p_resp)
{
    UINT8      *p = (UINT8 *) (p_resp + 1) + p_resp->offset;
    UINT8       flags, xx, length_offset, buff[I93_MAX_BLOCK_LENGH];
    UINT16      length = p_resp->len, block_number;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_i93_sm_update_ndef () sub_state:%s (0x%x)",
                      rw_i93_get_sub_state_name (p_i93->sub_state), p_i93->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_i93_sm_update_ndef () sub_state:0x%x", p_i93->sub_state);
#endif

    STREAM_TO_UINT8 (flags, p);
    length--;

    if (flags & I93_FLAG_ERROR_DETECTED)
    {
        if (  (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_INLAY)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_CHIP)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
                                         &&
              (*p == I93_ERROR_CODE_BLOCK_FAIL_TO_WRITE)  )
        {
            /* ignore error */
        }
        else
        {
            RW_TRACE_DEBUG1 ("Got error flags (0x%02x)", flags);
            rw_i93_handle_error (NFC_STATUS_FAILED);
            return;
        }
    }

    switch (p_i93->sub_state)
    {
    case RW_I93_SUBSTATE_RESET_LEN:

        /* get offset of length field */
        length_offset = (p_i93->ndef_tlv_start_offset + 1) % p_i93->block_size;

        /* set length to zero */
        *(p + length_offset) = 0x00;

        if (p_i93->ndef_length > 0)
        {
            /* if 3 bytes length field is needed */
            if (p_i93->ndef_length >= 0xFF)
            {
                xx = length_offset + 3;
            }
            else
            {
                xx = length_offset + 1;
            }

            /* write the first part of NDEF in the same block */
            for ( ; xx < p_i93->block_size; xx++)
            {
                if (p_i93->rw_length < p_i93->ndef_length)
                {
                    *(p + xx) = *(p_i93->p_update_data + p_i93->rw_length++);
                }
                else
                {
                    *(p + xx) = I93_ICODE_TLV_TYPE_NULL;
                }
            }
        }

        block_number = (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

        if (rw_i93_send_cmd_write_single_block (block_number, p) == NFC_STATUS_OK)
        {
            /* update next writing offset */
            p_i93->rw_offset = (block_number + 1) * p_i93->block_size;
            p_i93->sub_state = RW_I93_SUBSTATE_WRITE_NDEF;
        }
        else
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_WRITE_NDEF:

        /* if it's not the end of tag memory */
        if (p_i93->rw_offset < p_i93->block_size * p_i93->num_block)
        {
            block_number = p_i93->rw_offset / p_i93->block_size;

            /* if we have more data to write */
            if (p_i93->rw_length < p_i93->ndef_length)
            {
                p = p_i93->p_update_data + p_i93->rw_length;

                p_i93->rw_offset += p_i93->block_size;
                p_i93->rw_length += p_i93->block_size;

                /* if this is the last block of NDEF TLV */
                if (p_i93->rw_length > p_i93->ndef_length)
                {
                    /* length of NDEF TLV in the block */
                    xx = (UINT8) (p_i93->block_size - (p_i93->rw_length - p_i93->ndef_length));

                    /* set NULL TLV in the unused part of block */
                    memset (buff, I93_ICODE_TLV_TYPE_NULL, p_i93->block_size);
                    memcpy (buff, p, xx);
                    p = buff;

                    /* if it's the end of tag memory */
                    if (  (p_i93->rw_offset >= p_i93->block_size * p_i93->num_block)
                        &&(xx < p_i93->block_size)  )
                    {
                        buff[xx] = I93_ICODE_TLV_TYPE_TERM;
                    }

                    p_i93->ndef_tlv_last_offset = p_i93->rw_offset - p_i93->block_size + xx - 1;
                }

                if (rw_i93_send_cmd_write_single_block (block_number, p) != NFC_STATUS_OK)
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
            else
            {
                /* if this is the very next block of NDEF TLV */
                if (block_number == (p_i93->ndef_tlv_last_offset / p_i93->block_size) + 1)
                {
                    p_i93->rw_offset += p_i93->block_size;

                    /* write Terminator TLV and NULL TLV */
                    memset (buff, I93_ICODE_TLV_TYPE_NULL, p_i93->block_size);
                    buff[0] = I93_ICODE_TLV_TYPE_TERM;
                    p = buff;

                    if (rw_i93_send_cmd_write_single_block (block_number, p) != NFC_STATUS_OK)
                    {
                        rw_i93_handle_error (NFC_STATUS_FAILED);
                    }
                }
                else
                {
                    /* finished writing NDEF and Terminator TLV */
                    /* read length field to update length       */
                    block_number = (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

                    if (rw_i93_send_cmd_read_single_block (block_number, FALSE) == NFC_STATUS_OK)
                    {
                        /* set offset to length field */
                        p_i93->rw_offset = p_i93->ndef_tlv_start_offset + 1;

                        /* get size of length field */
                        if (p_i93->ndef_length >= 0xFF)
                        {
                            p_i93->rw_length = 3;
                        }
                        else if (p_i93->ndef_length > 0)
                        {
                            p_i93->rw_length = 1;
                        }
                        else
                        {
                            p_i93->rw_length = 0;
                        }

                        p_i93->sub_state = RW_I93_SUBSTATE_UPDATE_LEN;
                    }
                    else
                    {
                        rw_i93_handle_error (NFC_STATUS_FAILED);
                    }
                }
            }
        }
        else
        {
            /* if we have no more data to write */
            if (p_i93->rw_length >= p_i93->ndef_length)
            {
                /* finished writing NDEF and Terminator TLV */
                /* read length field to update length       */
                block_number = (p_i93->ndef_tlv_start_offset + 1) / p_i93->block_size;

                if (rw_i93_send_cmd_read_single_block (block_number, FALSE) == NFC_STATUS_OK)
                {
                    /* set offset to length field */
                    p_i93->rw_offset = p_i93->ndef_tlv_start_offset + 1;

                    /* get size of length field */
                    if (p_i93->ndef_length >= 0xFF)
                    {
                        p_i93->rw_length = 3;
                    }
                    else if (p_i93->ndef_length > 0)
                    {
                        p_i93->rw_length = 1;
                    }
                    else
                    {
                        p_i93->rw_length = 0;
                    }

                    p_i93->sub_state = RW_I93_SUBSTATE_UPDATE_LEN;
                    break;
                }
            }
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_UPDATE_LEN:

        /* if we have more length field to write */
        if (p_i93->rw_length > 0)
        {
            /* if we got ack for writing, read next block to update rest of length field */
            if (length == 0)
            {
                block_number = p_i93->rw_offset / p_i93->block_size;

                if (rw_i93_send_cmd_read_single_block (block_number, FALSE) != NFC_STATUS_OK)
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
            else
            {
                length_offset = p_i93->rw_offset % p_i93->block_size;

                /* update length field within the read block */
                for (xx = length_offset; xx < p_i93->block_size; xx++)
                {
                    if (p_i93->rw_length == 3)
                        *(p + xx) = 0xFF;
                    else if (p_i93->rw_length == 2)
                        *(p + xx) = (UINT8) ((p_i93->ndef_length >> 8) & 0xFF);
                    else if (p_i93->rw_length == 1)
                        *(p + xx) = (UINT8) (p_i93->ndef_length & 0xFF);

                    p_i93->rw_length--;
                    if (p_i93->rw_length == 0)
                        break;
                }

                block_number = (p_i93->rw_offset / p_i93->block_size);

                if (rw_i93_send_cmd_write_single_block (block_number, p) == NFC_STATUS_OK)
                {
                    /* set offset to the beginning of next block */
                    p_i93->rw_offset += p_i93->block_size - (p_i93->rw_offset % p_i93->block_size);
                }
                else
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
        }
        else
        {
            RW_TRACE_DEBUG3 ("NDEF update complete, %d bytes, (%d-%d)",
                             p_i93->ndef_length,
                             p_i93->ndef_tlv_start_offset,
                             p_i93->ndef_tlv_last_offset);

            p_i93->state         = RW_I93_STATE_IDLE;
            p_i93->sent_cmd      = 0;
            p_i93->p_update_data = NULL;

            rw_data.status = NFC_STATUS_OK;
            (*(rw_cb.p_cback)) (RW_I93_NDEF_UPDATE_CPLT_EVT, &rw_data);
        }
        break;

    default:
        break;
    }
}

/*******************************************************************************
**
** Function         rw_i93_sm_format
**
** Description      Process format procedure
**
**                  1. Get UID
**                  2. Get sys info for memory size (reset AFI/DSFID)
**                  3. Get block status to get read-only status
**                  4. Write CC and empty NDEF
**
** Returns          void
**
*******************************************************************************/
void rw_i93_sm_format (BT_HDR *p_resp)
{
    UINT8      *p = (UINT8 *) (p_resp + 1) + p_resp->offset, *p_uid;
    UINT8       flags;
    UINT16      length = p_resp->len, xx, block_number;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    tRW_DATA    rw_data;
    tNFC_STATUS status = NFC_STATUS_FAILED;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_i93_sm_format () sub_state:%s (0x%x)",
                      rw_i93_get_sub_state_name (p_i93->sub_state), p_i93->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_i93_sm_format () sub_state:0x%x", p_i93->sub_state);
#endif

    STREAM_TO_UINT8 (flags, p);
    length--;

    if (flags & I93_FLAG_ERROR_DETECTED)
    {
        if (  (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_INLAY)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_CHIP)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
                                         &&
              (*p == I93_ERROR_CODE_BLOCK_FAIL_TO_WRITE)  )
        {
            /* ignore error */
        }
        else if ((length) && (rw_i93_check_sys_info_prot_ext(*p)))
        {
            /* getting system info with protocol extension flag */
            /* This STM tag supports more than 2040 bytes */
            p_i93->intl_flags |= RW_I93_FLAG_16BIT_NUM_BLOCK;
            return;
        }
        else
        {
            RW_TRACE_DEBUG1 ("Got error flags (0x%02x)", flags);
            rw_i93_handle_error (NFC_STATUS_FAILED);
            return;
        }
    }

    switch (p_i93->sub_state)
    {
    case RW_I93_SUBSTATE_WAIT_UID:

        p++; /* skip DSFID */
        p_uid = p_i93->uid;
        STREAM_TO_ARRAY8 (p_uid, p);     /* store UID */

        /* get system information to get memory size */
        if (rw_i93_send_cmd_get_sys_info (NULL, I93_FLAG_PROT_EXT_NO) == NFC_STATUS_OK)
        {
            p_i93->sub_state = RW_I93_SUBSTATE_WAIT_SYS_INFO;
        }
        else
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_WAIT_SYS_INFO:

        p_i93->block_size = 0;
        p_i93->num_block  = 0;

        if (!rw_i93_process_sys_info (p))
        {
            /* retrying with protocol extension flag */
            break;
        }

        if (p_i93->info_flags & I93_INFO_FLAG_DSFID)
        {
            /* DSFID, if any DSFID then reset */
            if (p_i93->dsfid != I93_DFS_UNSUPPORTED)
            {
                p_i93->intl_flags |= RW_I93_FLAG_RESET_DSFID;
            }
        }
        if (p_i93->info_flags & I93_INFO_FLAG_AFI)
        {
            /* AFI, reset to 0 */
            if (p_i93->afi != 0x00)
            {
                p_i93->intl_flags |= RW_I93_FLAG_RESET_AFI;
            }
        }

        if ((p_i93->block_size == 0)||(p_i93->num_block == 0))
        {
            RW_TRACE_DEBUG0 ("Unable to get tag memory size");
            rw_i93_handle_error (status);
        }
        else if (p_i93->intl_flags & RW_I93_FLAG_RESET_DSFID)
        {
            if (rw_i93_send_cmd_write_dsfid (I93_DFS_UNSUPPORTED) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else if (p_i93->intl_flags & RW_I93_FLAG_RESET_AFI)
        {
            if (rw_i93_send_cmd_write_afi (0x00) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else
        {
            /* get lock status to see if read-only */
            if ((p_i93->uid[1] == I93_UID_IC_MFG_CODE_NXP) && (p_i93->ic_reference & I93_ICODE_IC_REF_MBREAD_MASK))
            {
                /* these doesn't support GetMultiBlockSecurityStatus */

                rw_cb.tcb.i93.rw_offset = 0;

                /* read blocks with option flag to get block security status */
                if (rw_i93_send_cmd_read_single_block (0x0000, TRUE) == NFC_STATUS_OK)
                {
                    p_i93->sub_state = RW_I93_SUBSTATE_CHECK_READ_ONLY;
                }
                else
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
            else
            {
                /* block offset for read-only check */
                p_i93->rw_offset = 0;

                if (rw_i93_get_next_block_sec () == NFC_STATUS_OK)
                {
                    p_i93->sub_state = RW_I93_SUBSTATE_CHECK_READ_ONLY;
                }
                else
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
        }

        break;

    case RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI:

        if (p_i93->sent_cmd == I93_CMD_WRITE_DSFID)
        {
            p_i93->intl_flags &= ~RW_I93_FLAG_RESET_DSFID;
        }
        else if (p_i93->sent_cmd == I93_CMD_WRITE_AFI)
        {
            p_i93->intl_flags &= ~RW_I93_FLAG_RESET_AFI;
        }

        if (p_i93->intl_flags & RW_I93_FLAG_RESET_DSFID)
        {
            if (rw_i93_send_cmd_write_dsfid (I93_DFS_UNSUPPORTED) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else if (p_i93->intl_flags & RW_I93_FLAG_RESET_AFI)
        {
            if (rw_i93_send_cmd_write_afi (0x00) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else
        {
            /* get lock status to see if read-only */
            if ((p_i93->uid[1] == I93_UID_IC_MFG_CODE_NXP) && (p_i93->ic_reference & I93_ICODE_IC_REF_MBREAD_MASK))
            {
                /* these doesn't support GetMultiBlockSecurityStatus */

                rw_cb.tcb.i93.rw_offset = 0;

                /* read blocks with option flag to get block security status */
                if (rw_i93_send_cmd_read_single_block (0x0000, TRUE) == NFC_STATUS_OK)
                {
                    p_i93->sub_state = RW_I93_SUBSTATE_CHECK_READ_ONLY;
                }
                else
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
            else
            {
                /* block offset for read-only check */
                p_i93->rw_offset = 0;

                if (rw_i93_get_next_block_sec () == NFC_STATUS_OK)
                {
                    p_i93->sub_state = RW_I93_SUBSTATE_CHECK_READ_ONLY;
                }
                else
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
            }
        }
        break;

    case RW_I93_SUBSTATE_CHECK_READ_ONLY:

        if (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
            ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)
            ||((p_i93->uid[1] == I93_UID_IC_MFG_CODE_NXP) && (p_i93->ic_reference & I93_ICODE_IC_REF_MBREAD_MASK))  )
        {
            if ((*p) & I93_BLOCK_LOCKED)
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
                break;
            }

            /* if we checked all of user blocks */
            if ((p_i93->rw_offset / p_i93->block_size) + 1 == p_i93->num_block)
            {
                if (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
                    ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
                {
                    /* read the block which has AFI */
                    p_i93->rw_offset = I93_TAG_IT_HF_I_STD_PRO_CHIP_INLAY_AFI_LOCATION;
                    rw_i93_send_cmd_read_single_block ((UINT16)(p_i93->rw_offset/p_i93->block_size), TRUE);
                    break;
                }
            }
            else if (p_i93->rw_offset == I93_TAG_IT_HF_I_STD_PRO_CHIP_INLAY_AFI_LOCATION)
            {
                /* no block is locked */
            }
            else
            {
                p_i93->rw_offset += p_i93->block_size;
                rw_i93_send_cmd_read_single_block ((UINT16)(p_i93->rw_offset/p_i93->block_size), TRUE);
                break;
            }
        }
        else
        {
            /* if any block is locked, we cannot format it */
            for (xx = 0; xx < length; xx++)
            {
                if (*(p + xx) & I93_BLOCK_LOCKED)
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                    break;
                }
            }

            /* update block offset for read-only check */
            p_i93->rw_offset += length;

            /* if need to get more lock status of blocks */
            if (p_i93->num_block > p_i93->rw_offset)
            {
                if (rw_i93_get_next_block_sec () != NFC_STATUS_OK)
                {
                    rw_i93_handle_error (NFC_STATUS_FAILED);
                }
                break;
            }
        }

        /* get buffer to store CC, zero length NDEF TLV and Terminator TLV */
        p_i93->p_update_data = (UINT8*) GKI_getbuf (RW_I93_FORMAT_DATA_LEN);

        if (!p_i93->p_update_data)
        {
            RW_TRACE_ERROR0 ("rw_i93_sm_format (): Cannot allocate buffer");
            rw_i93_handle_error (NFC_STATUS_FAILED);
            break;
        }

        p = p_i93->p_update_data;

        /* Capability Container */
        *(p++) = I93_ICODE_CC_MAGIC_NUMER;                      /* magic number */
        *(p++) = 0x40;                                          /* version 1.0, read/write */

        /* if memory size is less than 2048 bytes */
        if (((p_i93->num_block * p_i93->block_size) / 8) < 0x100)
            *(p++) = (UINT8) ((p_i93->num_block * p_i93->block_size) / 8);    /* memory size */
        else
            *(p++) = 0xFF;

        if (  (p_i93->product_version == RW_I93_ICODE_SLI)
            ||(p_i93->product_version == RW_I93_ICODE_SLI_S)
            ||(p_i93->product_version == RW_I93_ICODE_SLI_L)  )
        {
            if (p_i93->ic_reference & I93_ICODE_IC_REF_MBREAD_MASK)
                *(p++) = I93_ICODE_CC_IPREAD_MASK;  /* IPREAD */
            else
                *(p++) = I93_ICODE_CC_MBREAD_MASK;  /* MBREAD, read multi block command supported */
        }
        else if (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_INLAY)
                 ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_CHIP)  )
        {
            *(p++) = I93_ICODE_CC_MBREAD_MASK;      /* MBREAD, read multi block command supported */
        }
        else if (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
                 ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
        {
            *(p++) = 0;
        }
        else
        {
            /* STM except LRIS2K, Broadcom supports read multi block command */

            /* if memory size is more than 2040 bytes (which is not LRIS2K) */
            if (((p_i93->num_block * p_i93->block_size) / 8) > 0xFF)
                *(p++) = (I93_ICODE_CC_MBREAD_MASK | I93_STM_CC_OVERFLOW_MASK);
            else if (p_i93->product_version == RW_I93_STM_LRIS2K)
                *(p++) = 0x00;
            else
                *(p++) = I93_ICODE_CC_MBREAD_MASK;
        }

        /* zero length NDEF and Terminator TLV */
        *(p++) = I93_ICODE_TLV_TYPE_NDEF;
        *(p++) = 0x00;
        *(p++) = I93_ICODE_TLV_TYPE_TERM;
        *(p++) = I93_ICODE_TLV_TYPE_NULL;

        /* start from block 0 */
        p_i93->rw_offset = 0;

        if (rw_i93_send_cmd_write_single_block (0, p_i93->p_update_data) == NFC_STATUS_OK)
        {
            p_i93->sub_state = RW_I93_SUBSTATE_WRITE_CC_NDEF_TLV;
            p_i93->rw_offset += p_i93->block_size;
        }
        else
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_WRITE_CC_NDEF_TLV:

        /* if we have more data to write */
        if (p_i93->rw_offset < RW_I93_FORMAT_DATA_LEN)
        {
            block_number = (p_i93->rw_offset / p_i93->block_size);
            p = p_i93->p_update_data + p_i93->rw_offset;

            if (rw_i93_send_cmd_write_single_block (block_number, p) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WRITE_CC_NDEF_TLV;
                p_i93->rw_offset += p_i93->block_size;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else
        {
            GKI_freebuf (p_i93->p_update_data);
            p_i93->p_update_data = NULL;

            p_i93->state = RW_I93_STATE_IDLE;
            p_i93->sent_cmd = 0;

            rw_data.status = NFC_STATUS_OK;
            (*(rw_cb.p_cback)) (RW_I93_FORMAT_CPLT_EVT, &rw_data);
        }
        break;

    default:
        break;
    }
}

/*******************************************************************************
**
** Function         rw_i93_sm_set_read_only
**
** Description      Process read-only procedure
**
**                  1. Update CC as read-only
**                  2. Lock all block of NDEF TLV
**                  3. Lock block of CC
**
** Returns          void
**
*******************************************************************************/
void rw_i93_sm_set_read_only (BT_HDR *p_resp)
{
    UINT8      *p = (UINT8 *) (p_resp + 1) + p_resp->offset;
    UINT8       flags, block_number;
    UINT16      length = p_resp->len;
    tRW_I93_CB *p_i93 = &rw_cb.tcb.i93;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_i93_sm_set_read_only () sub_state:%s (0x%x)",
                      rw_i93_get_sub_state_name (p_i93->sub_state), p_i93->sub_state);
#else
    RW_TRACE_DEBUG1 ("rw_i93_sm_set_read_only () sub_state:0x%x", p_i93->sub_state);
#endif

    STREAM_TO_UINT8 (flags, p);
    length--;

    if (flags & I93_FLAG_ERROR_DETECTED)
    {
        if (  (  (p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_INLAY)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PLUS_CHIP)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
               ||(p_i93->product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
                                         &&
              (*p == I93_ERROR_CODE_BLOCK_FAIL_TO_WRITE)  )
        {
            /* ignore error */
        }
        else
        {
            RW_TRACE_DEBUG1 ("Got error flags (0x%02x)", flags);
            rw_i93_handle_error (NFC_STATUS_FAILED);
            return;
        }
    }

    switch (p_i93->sub_state)
    {
    case RW_I93_SUBSTATE_WAIT_CC:

        /* mark CC as read-only */
        *(p+1) |= I93_ICODE_CC_READ_ONLY;

        if (rw_i93_send_cmd_write_single_block (0, p) == NFC_STATUS_OK)
        {
            p_i93->sub_state = RW_I93_SUBSTATE_WAIT_UPDATE_CC;
        }
        else
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_WAIT_UPDATE_CC:

        /* successfully write CC then lock all blocks of NDEF TLV */
        p_i93->rw_offset = p_i93->ndef_tlv_start_offset;
        block_number     = (UINT8) (p_i93->rw_offset / p_i93->block_size);

        if (rw_i93_send_cmd_lock_block (block_number) == NFC_STATUS_OK)
        {
            p_i93->rw_offset += p_i93->block_size;
            p_i93->sub_state = RW_I93_SUBSTATE_LOCK_NDEF_TLV;
        }
        else
        {
            rw_i93_handle_error (NFC_STATUS_FAILED);
        }
        break;

    case RW_I93_SUBSTATE_LOCK_NDEF_TLV:

        /* if we need to lock more blocks */
        if (p_i93->rw_offset < p_i93->ndef_tlv_last_offset)
        {
            /* get the next block of NDEF TLV */
            block_number = (UINT8) (p_i93->rw_offset / p_i93->block_size);

            if (rw_i93_send_cmd_lock_block (block_number) == NFC_STATUS_OK)
            {
                p_i93->rw_offset += p_i93->block_size;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        /* if the first block of NDEF TLV is different from block of CC */
        else if (p_i93->ndef_tlv_start_offset / p_i93->block_size != 0)
        {
            /* lock block of CC */
            if (rw_i93_send_cmd_lock_block (0) == NFC_STATUS_OK)
            {
                p_i93->sub_state = RW_I93_SUBSTATE_WAIT_LOCK_CC;
            }
            else
            {
                rw_i93_handle_error (NFC_STATUS_FAILED);
            }
        }
        else
        {
            p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
            p_i93->state       = RW_I93_STATE_IDLE;
            p_i93->sent_cmd    = 0;

            rw_data.status = NFC_STATUS_OK;
            (*(rw_cb.p_cback)) (RW_I93_SET_TAG_RO_EVT, &rw_data);
        }
        break;

    case RW_I93_SUBSTATE_WAIT_LOCK_CC:

        p_i93->intl_flags |= RW_I93_FLAG_READ_ONLY;
        p_i93->state      = RW_I93_STATE_IDLE;
        p_i93->sent_cmd   = 0;

        rw_data.status = NFC_STATUS_OK;
        (*(rw_cb.p_cback)) (RW_I93_SET_TAG_RO_EVT, &rw_data);
        break;

    default:
        break;
    }
}

/*******************************************************************************
**
** Function         rw_i93_handle_error
**
** Description      notify error to application and clean up
**
** Returns          none
**
*******************************************************************************/
void rw_i93_handle_error (tNFC_STATUS status)
{
    tRW_I93_CB *p_i93  = &rw_cb.tcb.i93;
    tRW_DATA    rw_data;
    tRW_EVENT   event;

    RW_TRACE_DEBUG2 ("rw_i93_handle_error (): status:0x%02X, state:0x%X",
                      status, p_i93->state);

    nfc_stop_quick_timer (&p_i93->timer);

    if (rw_cb.p_cback)
    {
        rw_data.status = status;

        switch (p_i93->state)
        {
        case RW_I93_STATE_IDLE:   /* in case of RawFrame */
            event = RW_I93_INTF_ERROR_EVT;
            break;

        case RW_I93_STATE_BUSY:
            if (p_i93->sent_cmd == I93_CMD_STAY_QUIET)
            {
                /* There is no response to Stay Quiet command */
                rw_data.i93_cmd_cmpl.status     = NFC_STATUS_OK;
                rw_data.i93_cmd_cmpl.command    = I93_CMD_STAY_QUIET;
                rw_data.i93_cmd_cmpl.error_code = 0;
                event = RW_I93_CMD_CMPL_EVT;
            }
            else
            {
                event = RW_I93_INTF_ERROR_EVT;
            }
            break;

        case RW_I93_STATE_DETECT_NDEF:
            rw_data.ndef.protocol = NFC_PROTOCOL_15693;
            rw_data.ndef.cur_size = 0;
            rw_data.ndef.max_size = 0;
            rw_data.ndef.flags    = 0;
            rw_data.ndef.flags   |= RW_NDEF_FL_FORMATABLE;
            rw_data.ndef.flags   |= RW_NDEF_FL_UNKNOWN;
            event = RW_I93_NDEF_DETECT_EVT;
            break;

        case RW_I93_STATE_READ_NDEF:
            event = RW_I93_NDEF_READ_FAIL_EVT;
            break;

        case RW_I93_STATE_UPDATE_NDEF:
            p_i93->p_update_data = NULL;
            event = RW_I93_NDEF_UPDATE_FAIL_EVT;
            break;

        case RW_I93_STATE_FORMAT:
            if (p_i93->p_update_data)
            {
                GKI_freebuf (p_i93->p_update_data);
                p_i93->p_update_data = NULL;
            }
            event = RW_I93_FORMAT_CPLT_EVT;
            break;

        case RW_I93_STATE_SET_READ_ONLY:
            event = RW_I93_SET_TAG_RO_EVT;
            break;

        case RW_I93_STATE_PRESENCE_CHECK:
            event = RW_I93_PRESENCE_CHECK_EVT;
            break;

        default:
            event = RW_I93_MAX_EVT;
            break;
        }

        p_i93->state    = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;

        if (event != RW_I93_MAX_EVT)
        {
            (*(rw_cb.p_cback)) (event, &rw_data);
        }
    }
    else
    {
        p_i93->state = RW_I93_STATE_IDLE;
    }
}

/*******************************************************************************
**
** Function         rw_i93_process_timeout
**
** Description      process timeout event
**
** Returns          none
**
*******************************************************************************/
void rw_i93_process_timeout (TIMER_LIST_ENT *p_tle)
{
    BT_HDR *p_buf;

    RW_TRACE_DEBUG1 ("rw_i93_process_timeout () event=%d", p_tle->event);

    if (p_tle->event == NFC_TTYPE_RW_I93_RESPONSE)
    {
        if (  (rw_cb.tcb.i93.retry_count < RW_MAX_RETRIES)
            &&(rw_cb.tcb.i93.p_retry_cmd)
            &&(rw_cb.tcb.i93.sent_cmd != I93_CMD_STAY_QUIET))
        {
            rw_cb.tcb.i93.retry_count++;
            RW_TRACE_ERROR1 ("rw_i93_process_timeout (): retry_count = %d", rw_cb.tcb.i93.retry_count);

            p_buf = rw_cb.tcb.i93.p_retry_cmd;
            rw_cb.tcb.i93.p_retry_cmd = NULL;

            if (rw_i93_send_to_lower (p_buf))
            {
                return;
            }
        }

        /* all retrial is done or failed to send command to lower layer */
        if (rw_cb.tcb.i93.p_retry_cmd)
        {
            GKI_freebuf (rw_cb.tcb.i93.p_retry_cmd);
            rw_cb.tcb.i93.p_retry_cmd = NULL;
            rw_cb.tcb.i93.retry_count = 0;
        }
        rw_i93_handle_error (NFC_STATUS_TIMEOUT);
    }
    else
    {
        RW_TRACE_ERROR1 ("rw_i93_process_timeout () unknown event=%d", p_tle->event);
    }
}

/*******************************************************************************
**
** Function         rw_i93_data_cback
**
** Description      This callback function receives the data from NFCC.
**
** Returns          none
**
*******************************************************************************/
static void rw_i93_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    tRW_I93_CB *p_i93  = &rw_cb.tcb.i93;
    BT_HDR     *p_resp;
    tRW_DATA    rw_data;

#if (BT_TRACE_VERBOSE == TRUE)
    UINT8  begin_state   = p_i93->state;
#endif

    RW_TRACE_DEBUG1 ("rw_i93_data_cback () event = 0x%X", event);

    if (  (event == NFC_DEACTIVATE_CEVT)
        ||(event == NFC_ERROR_CEVT)  )
    {
        nfc_stop_quick_timer (&p_i93->timer);

        if (event == NFC_ERROR_CEVT)
        {
            if (  (p_i93->retry_count < RW_MAX_RETRIES)
                &&(p_i93->p_retry_cmd)  )
            {
                p_i93->retry_count++;

                RW_TRACE_ERROR1 ("rw_i93_data_cback (): retry_count = %d", p_i93->retry_count);

                p_resp = p_i93->p_retry_cmd;
                p_i93->p_retry_cmd = NULL;
                if (rw_i93_send_to_lower (p_resp))
                {
                    return;
                }
            }

            /* all retrial is done or failed to send command to lower layer */
            if (p_i93->p_retry_cmd)
            {
                GKI_freebuf (p_i93->p_retry_cmd);
                p_i93->p_retry_cmd = NULL;
                p_i93->retry_count = 0;
            }

            rw_i93_handle_error ((tNFC_STATUS) (*(UINT8*) p_data));
        }
        else
        {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            /* free retry buffer */
            if ((event == NFC_DEACTIVATE_CEVT )&& p_i93->p_retry_cmd)
            {
                GKI_freebuf (p_i93->p_retry_cmd);
                p_i93->p_retry_cmd = NULL;
                p_i93->retry_count = 0;
            }
#endif
            NFC_SetStaticRfCback (NULL);
            p_i93->state = RW_I93_STATE_NOT_ACTIVATED;
        }
        return;
    }

    if (event != NFC_DATA_CEVT)
    {
        return;
    }

    p_resp = (BT_HDR *) p_data->data.p_data;

    nfc_stop_quick_timer (&p_i93->timer);

    /* free retry buffer */
    if (p_i93->p_retry_cmd)
    {
        GKI_freebuf (p_i93->p_retry_cmd);
        p_i93->p_retry_cmd = NULL;
        p_i93->retry_count = 0;
    }

#if (BT_TRACE_PROTOCOL == TRUE)
    DispRWI93Tag (p_resp, TRUE, p_i93->sent_cmd);
#endif

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("RW I93 state: <%s (%d)>",
                        rw_i93_get_state_name (p_i93->state), p_i93->state);
#else
    RW_TRACE_DEBUG1 ("RW I93 state: %d", p_i93->state);
#endif

    switch (p_i93->state)
    {
    case RW_I93_STATE_IDLE:
        /* Unexpected Response from VICC, it should be raw frame response */
        /* forward to upper layer without parsing */
        p_i93->sent_cmd = 0;
        if (rw_cb.p_cback)
        {
            rw_data.raw_frame.status = p_data->data.status;
            rw_data.raw_frame.p_data = p_resp;
            (*(rw_cb.p_cback)) (RW_I93_RAW_FRAME_EVT, &rw_data);
            p_resp = NULL;
        }
        else
        {
            GKI_freebuf (p_resp);
        }
        break;
    case RW_I93_STATE_BUSY:
        p_i93->state = RW_I93_STATE_IDLE;
        rw_i93_send_to_upper (p_resp);
        GKI_freebuf (p_resp);
        break;

    case RW_I93_STATE_DETECT_NDEF:
        rw_i93_sm_detect_ndef (p_resp);
        GKI_freebuf (p_resp);
        break;

    case RW_I93_STATE_READ_NDEF:
        rw_i93_sm_read_ndef (p_resp);
        /* p_resp may send upper lyaer */
        break;

    case RW_I93_STATE_UPDATE_NDEF:
        rw_i93_sm_update_ndef (p_resp);
        GKI_freebuf (p_resp);
        break;

    case RW_I93_STATE_FORMAT:
        rw_i93_sm_format (p_resp);
        GKI_freebuf (p_resp);
        break;

    case RW_I93_STATE_SET_READ_ONLY:
        rw_i93_sm_set_read_only (p_resp);
        GKI_freebuf (p_resp);
        break;

    case RW_I93_STATE_PRESENCE_CHECK:
        p_i93->state    = RW_I93_STATE_IDLE;
        p_i93->sent_cmd = 0;

        /* depending of response length, send presence check with ok or failed */
        if (p_resp->len > 1) rw_data.status  = NFC_STATUS_OK;
        else rw_data.status  = NFC_STATUS_FAILED;
        (*(rw_cb.p_cback)) (RW_I93_PRESENCE_CHECK_EVT, &rw_data);
        GKI_freebuf (p_resp);
        break;

    default:
        RW_TRACE_ERROR1 ("rw_i93_data_cback (): invalid state=%d", p_i93->state);
        GKI_freebuf (p_resp);
        break;
    }

#if (BT_TRACE_VERBOSE == TRUE)
    if (begin_state != p_i93->state)
    {
        RW_TRACE_DEBUG2 ("RW I93 state changed:<%s> -> <%s>",
                          rw_i93_get_state_name (begin_state),
                          rw_i93_get_state_name (p_i93->state));
    }
#endif
}

/*******************************************************************************
**
** Function         rw_i93_select
**
** Description      Initialise ISO 15693 RW
**
** Returns          NFC_STATUS_OK if success
**
*******************************************************************************/
tNFC_STATUS rw_i93_select (UINT8 *p_uid)
{
    tRW_I93_CB  *p_i93 = &rw_cb.tcb.i93;
    UINT8       uid[I93_UID_BYTE_LEN], *p;

    RW_TRACE_DEBUG0 ("rw_i93_select ()");

    NFC_SetStaticRfCback (rw_i93_data_cback);

    p_i93->state = RW_I93_STATE_IDLE;

    /* convert UID to big endian format - MSB(0xE0) in first byte */
    p = uid;
    STREAM_TO_ARRAY8 (p, p_uid);

    rw_i93_get_product_version (uid);

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         RW_I93Inventory
**
** Description      This function send Inventory command with/without AFI
**                  If UID is provided then set UID[0]:MSB, ... UID[7]:LSB
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93Inventory (BOOLEAN including_afi, UINT8 afi, UINT8 *p_uid)
{
    tNFC_STATUS status;

    RW_TRACE_API2 ("RW_I93Inventory (), including_afi:%d, AFI:0x%02X", including_afi, afi);

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93Inventory ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_inventory (p_uid, including_afi, afi);

    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return (status);
}

/*******************************************************************************
**
** Function         RW_I93StayQuiet
**
** Description      This function send Inventory command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93StayQuiet (void)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93StayQuiet ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93StayQuiet ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_stay_quiet ();
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93ReadSingleBlock
**
** Description      This function send Read Single Block command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93ReadSingleBlock (UINT16 block_number)
{
    tNFC_STATUS status;

    RW_TRACE_API1 ("RW_I93ReadSingleBlock () block_number:0x%02X", block_number);

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93ReadSingleBlock ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_read_single_block (block_number, FALSE);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93WriteSingleBlock
**
** Description      This function send Write Single Block command
**                  Application must get block size first by calling RW_I93GetSysInfo().
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93WriteSingleBlock (UINT16 block_number,
                                    UINT8  *p_data)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93WriteSingleBlock ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93WriteSingleBlock ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    if (rw_cb.tcb.i93.block_size == 0)
    {
        RW_TRACE_ERROR0 ("RW_I93WriteSingleBlock ():Block size is unknown");
        return NFC_STATUS_FAILED;
    }

    status = rw_i93_send_cmd_write_single_block (block_number, p_data);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93LockBlock
**
** Description      This function send Lock Block command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93LockBlock (UINT8 block_number)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93LockBlock ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93LockBlock ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_lock_block (block_number);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93ReadMultipleBlocks
**
** Description      This function send Read Multiple Blocks command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93ReadMultipleBlocks (UINT16 first_block_number,
                                      UINT16 number_blocks)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93ReadMultipleBlocks ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93ReadMultipleBlocks ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_read_multi_blocks (first_block_number, number_blocks);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93WriteMultipleBlocks
**
** Description      This function send Write Multiple Blocks command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93WriteMultipleBlocks (UINT8  first_block_number,
                                       UINT16 number_blocks,
                                       UINT8  *p_data)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93WriteMultipleBlocks ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93WriteMultipleBlocks ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    if (rw_cb.tcb.i93.block_size == 0)
    {
        RW_TRACE_ERROR0 ("RW_I93WriteSingleBlock ():Block size is unknown");
        return NFC_STATUS_FAILED;
    }

    status = rw_i93_send_cmd_write_multi_blocks (first_block_number, number_blocks, p_data);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93Select
**
** Description      This function send Select command
**
**                  UID[0]: 0xE0, MSB
**                  UID[1]: IC Mfg Code
**                  ...
**                  UID[7]: LSB
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93Select (UINT8 *p_uid)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93Select ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93Select ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    if (p_uid)
    {
        status = rw_i93_send_cmd_select (p_uid);
        if (status == NFC_STATUS_OK)
        {
            rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
        }
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_I93Select ():UID shall be provided");
        status = NFC_STATUS_FAILED;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93ResetToReady
**
** Description      This function send Reset To Ready command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93ResetToReady (void)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93ResetToReady ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93ResetToReady ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_reset_to_ready ();
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93WriteAFI
**
** Description      This function send Write AFI command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93WriteAFI (UINT8 afi)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93WriteAFI ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93WriteAFI ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_write_afi (afi);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93LockAFI
**
** Description      This function send Lock AFI command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93LockAFI (void)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93LockAFI ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93LockAFI ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_lock_afi ();
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93WriteDSFID
**
** Description      This function send Write DSFID command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93WriteDSFID (UINT8 dsfid)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93WriteDSFID ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93WriteDSFID ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_write_dsfid (dsfid);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93LockDSFID
**
** Description      This function send Lock DSFID command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93LockDSFID (void)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93LockDSFID ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93LockDSFID ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_lock_dsfid ();
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93GetSysInfo
**
** Description      This function send Get System Information command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93GetSysInfo (UINT8 *p_uid)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93GetSysInfo ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93GetSysInfo ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    if (p_uid)
    {
        status = rw_i93_send_cmd_get_sys_info (p_uid, I93_FLAG_PROT_EXT_NO);
    }
    else
    {
        status = rw_i93_send_cmd_get_sys_info (NULL, I93_FLAG_PROT_EXT_NO);
    }

    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93GetMultiBlockSecurityStatus
**
** Description      This function send Get Multiple Block Security Status command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
tNFC_STATUS RW_I93GetMultiBlockSecurityStatus (UINT16 first_block_number,
                                               UINT16 number_blocks)
{
    tNFC_STATUS status;

    RW_TRACE_API0 ("RW_I93GetMultiBlockSecurityStatus ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93GetMultiBlockSecurityStatus ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_BUSY;
    }

    status = rw_i93_send_cmd_get_multi_block_sec (first_block_number, number_blocks);
    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state = RW_I93_STATE_BUSY;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_I93DetectNDef
**
** Description      This function performs NDEF detection procedure
**
**                  RW_I93_NDEF_DETECT_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_I93DetectNDef (void)
{
    tNFC_STATUS status;
    tRW_I93_RW_SUBSTATE sub_state;

    RW_TRACE_API0 ("RW_I93DetectNDef ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93DetectNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_FAILED;
    }

    if (rw_cb.tcb.i93.uid[0] != I93_UID_FIRST_BYTE)
    {
        status = rw_i93_send_cmd_inventory (NULL, FALSE, 0x00);
        sub_state = RW_I93_SUBSTATE_WAIT_UID;
    }
    else if (  (rw_cb.tcb.i93.num_block == 0)
             ||(rw_cb.tcb.i93.block_size == 0)  )
    {
        status = rw_i93_send_cmd_get_sys_info (rw_cb.tcb.i93.uid, I93_FLAG_PROT_EXT_NO);
        sub_state = RW_I93_SUBSTATE_WAIT_SYS_INFO;

        /* clear all flags */
        rw_cb.tcb.i93.intl_flags = 0;
    }
    else
    {
        /* read CC in the first block */
        status = rw_i93_send_cmd_read_single_block (0x0000, FALSE);
        sub_state = RW_I93_SUBSTATE_WAIT_CC;
    }

    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state      = RW_I93_STATE_DETECT_NDEF;
        rw_cb.tcb.i93.sub_state  = sub_state;

        /* clear flags except flag for 2 bytes of number of blocks */
        rw_cb.tcb.i93.intl_flags &= RW_I93_FLAG_16BIT_NUM_BLOCK;
    }

    return (status);
}

/*******************************************************************************
**
** Function         RW_I93ReadNDef
**
** Description      This function performs NDEF read procedure
**                  Note: RW_I93DetectNDef () must be called before using this
**
**                  The following event will be returned
**                      RW_I93_NDEF_READ_EVT for each segmented NDEF message
**                      RW_I93_NDEF_READ_CPLT_EVT for the last segment or complete NDEF
**                      RW_I93_NDEF_READ_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if I93 is busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_I93ReadNDef (void)
{
    RW_TRACE_API0 ("RW_I93ReadNDef ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93ReadNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_FAILED;
    }

    if (  (rw_cb.tcb.i93.tlv_type == I93_ICODE_TLV_TYPE_NDEF)
        &&(rw_cb.tcb.i93.ndef_length > 0)  )
    {
        rw_cb.tcb.i93.rw_offset = rw_cb.tcb.i93.ndef_tlv_start_offset;
        rw_cb.tcb.i93.rw_length = 0;

        if (rw_i93_get_next_blocks (rw_cb.tcb.i93.rw_offset) == NFC_STATUS_OK)
        {
            rw_cb.tcb.i93.state = RW_I93_STATE_READ_NDEF;
        }
        else
        {
            return NFC_STATUS_FAILED;
        }
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_I93ReadNDef ():No NDEF detected");
        return NFC_STATUS_FAILED;
    }

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         RW_I93UpdateNDef
**
** Description      This function performs NDEF update procedure
**                  Note: RW_I93DetectNDef () must be called before using this
**                        Updating data must not be removed until returning event
**
**                  The following event will be returned
**                      RW_I93_NDEF_UPDATE_CPLT_EVT for complete
**                      RW_I93_NDEF_UPDATE_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if I93 is busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_I93UpdateNDef (UINT16 length, UINT8 *p_data)
{
    UINT16 block_number;

    RW_TRACE_API1 ("RW_I93UpdateNDef () length:%d", length);

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93UpdateNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_FAILED;
    }

    if (rw_cb.tcb.i93.tlv_type == I93_ICODE_TLV_TYPE_NDEF)
    {
        if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_READ_ONLY)
        {
            RW_TRACE_ERROR0 ("RW_I93UpdateNDef ():NDEF is read-only");
            return NFC_STATUS_FAILED;
        }
        if (rw_cb.tcb.i93.max_ndef_length < length)
        {
            RW_TRACE_ERROR2 ("RW_I93UpdateNDef ():data (%d bytes) is more than max NDEF length (%d)",
                              length, rw_cb.tcb.i93.max_ndef_length);
            return NFC_STATUS_FAILED;
        }

        rw_cb.tcb.i93.ndef_length   = length;
        rw_cb.tcb.i93.p_update_data = p_data;

        /* read length field */
        rw_cb.tcb.i93.rw_offset = rw_cb.tcb.i93.ndef_tlv_start_offset + 1;
        rw_cb.tcb.i93.rw_length = 0;

        block_number = rw_cb.tcb.i93.rw_offset / rw_cb.tcb.i93.block_size;

        if (rw_i93_send_cmd_read_single_block (block_number, FALSE) == NFC_STATUS_OK)
        {
            rw_cb.tcb.i93.state     = RW_I93_STATE_UPDATE_NDEF;
            rw_cb.tcb.i93.sub_state = RW_I93_SUBSTATE_RESET_LEN;
        }
        else
        {
            return NFC_STATUS_FAILED;
        }
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_I93ReadNDef ():No NDEF detected");
        return NFC_STATUS_FAILED;
    }

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         RW_I93FormatNDef
**
** Description      This function performs formatting procedure
**
**                  RW_I93_FORMAT_CPLT_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_I93FormatNDef (void)
{
    tNFC_STATUS status;
    tRW_I93_RW_SUBSTATE sub_state;

    RW_TRACE_API0 ("RW_I93FormatNDef ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93FormatNDef ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_FAILED;
    }

    if (  (rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY)
        ||(rw_cb.tcb.i93.product_version == RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY)  )
    {
        /* These don't support GetSystemInformation and GetMultiBlockSecurityStatus */
        rw_cb.tcb.i93.rw_offset = 0;

        /* read blocks with option flag to get block security status */
        status = rw_i93_send_cmd_read_single_block (0x0000, TRUE);
        sub_state = RW_I93_SUBSTATE_CHECK_READ_ONLY;
    }
    else
    {
        status = rw_i93_send_cmd_inventory (rw_cb.tcb.i93.uid, FALSE, 0x00);
        sub_state = RW_I93_SUBSTATE_WAIT_UID;
    }

    if (status == NFC_STATUS_OK)
    {
        rw_cb.tcb.i93.state      = RW_I93_STATE_FORMAT;
        rw_cb.tcb.i93.sub_state  = sub_state;
        rw_cb.tcb.i93.intl_flags = 0;
    }

    return (status);
}

/*******************************************************************************
**
** Function         RW_I93SetTagReadOnly
**
** Description      This function performs NDEF read-only procedure
**                  Note: RW_I93DetectNDef () must be called before using this
**                        Updating data must not be removed until returning event
**
**                  The RW_I93_SET_TAG_RO_EVT event will be returned.
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if I93 is busy or other error
**
*******************************************************************************/
tNFC_STATUS RW_I93SetTagReadOnly (void)
{
    RW_TRACE_API0 ("RW_I93SetTagReadOnly ()");

    if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        RW_TRACE_ERROR1 ("RW_I93SetTagReadOnly ():Unable to start command at state (0x%X)",
                          rw_cb.tcb.i93.state);
        return NFC_STATUS_FAILED;
    }

    if (rw_cb.tcb.i93.tlv_type == I93_ICODE_TLV_TYPE_NDEF)
    {
        if (rw_cb.tcb.i93.intl_flags & RW_I93_FLAG_READ_ONLY)
        {
            RW_TRACE_ERROR0 ("RW_I93SetTagReadOnly ():NDEF is already read-only");
            return NFC_STATUS_FAILED;
        }

        /* get CC in the first block */
        if (rw_i93_send_cmd_read_single_block (0, FALSE) == NFC_STATUS_OK)
        {
            rw_cb.tcb.i93.state     = RW_I93_STATE_SET_READ_ONLY;
            rw_cb.tcb.i93.sub_state = RW_I93_SUBSTATE_WAIT_CC;
        }
        else
        {
            return NFC_STATUS_FAILED;
        }
    }
    else
    {
        RW_TRACE_ERROR0 ("RW_I93SetTagReadOnly ():No NDEF detected");
        return NFC_STATUS_FAILED;
    }

    return NFC_STATUS_OK;
}

/*****************************************************************************
**
** Function         RW_I93PresenceCheck
**
** Description      Check if the tag is still in the field.
**
**                  The RW_I93_PRESENCE_CHECK_EVT w/ status is used to indicate
**                  presence or non-presence.
**
** Returns          NFC_STATUS_OK, if raw data frame sent
**                  NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**                  NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_I93PresenceCheck (void)
{

    tNFC_STATUS status;
    tRW_DATA    evt_data;

    RW_TRACE_API0 ("RW_I93PresenceCheck ()");

    if (!rw_cb.p_cback)
    {
        return NFC_STATUS_FAILED;
    }
    else if (rw_cb.tcb.i93.state == RW_I93_STATE_NOT_ACTIVATED)
    {
        evt_data.status = NFC_STATUS_FAILED;
        (*rw_cb.p_cback) (RW_T4T_PRESENCE_CHECK_EVT, &evt_data);

        return NFC_STATUS_OK;
    }
    else if (rw_cb.tcb.i93.state != RW_I93_STATE_IDLE)
    {
        return NFC_STATUS_BUSY;
    }
    else
    {
        /* The support of AFI by the VICC is optional, so do not include AFI */
        status = rw_i93_send_cmd_inventory (rw_cb.tcb.i93.uid, FALSE, 0x00);

        if (status == NFC_STATUS_OK)
        {
            /* do not retry during presence check */
            rw_cb.tcb.i93.retry_count = RW_MAX_RETRIES;
            rw_cb.tcb.i93.state = RW_I93_STATE_PRESENCE_CHECK;
        }
    }

    return (status);
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         rw_i93_get_state_name
**
** Description      This function returns the state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_i93_get_state_name (UINT8 state)
{
    switch (state)
    {
    case RW_I93_STATE_NOT_ACTIVATED:
        return ("NOT_ACTIVATED");
    case RW_I93_STATE_IDLE:
        return ("IDLE");
    case RW_I93_STATE_BUSY:
        return ("BUSY");

    case RW_I93_STATE_DETECT_NDEF:
        return ("NDEF_DETECTION");
    case RW_I93_STATE_READ_NDEF:
        return ("READ_NDEF");
    case RW_I93_STATE_UPDATE_NDEF:
        return ("UPDATE_NDEF");
    case RW_I93_STATE_FORMAT:
        return ("FORMAT");
    case RW_I93_STATE_SET_READ_ONLY:
        return ("SET_READ_ONLY");

    case RW_I93_STATE_PRESENCE_CHECK:
        return ("PRESENCE_CHECK");
    default:
        return ("???? UNKNOWN STATE");
    }
}

/*******************************************************************************
**
** Function         rw_i93_get_sub_state_name
**
** Description      This function returns the sub_state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_i93_get_sub_state_name (UINT8 sub_state)
{
    switch (sub_state)
    {
    case RW_I93_SUBSTATE_WAIT_UID:
        return ("WAIT_UID");
    case RW_I93_SUBSTATE_WAIT_SYS_INFO:
        return ("WAIT_SYS_INFO");
    case RW_I93_SUBSTATE_WAIT_CC:
        return ("WAIT_CC");
    case RW_I93_SUBSTATE_SEARCH_NDEF_TLV:
        return ("SEARCH_NDEF_TLV");
    case RW_I93_SUBSTATE_CHECK_LOCK_STATUS:
        return ("CHECK_LOCK_STATUS");
    case RW_I93_SUBSTATE_RESET_LEN:
        return ("RESET_LEN");
    case RW_I93_SUBSTATE_WRITE_NDEF:
        return ("WRITE_NDEF");
    case RW_I93_SUBSTATE_UPDATE_LEN:
        return ("UPDATE_LEN");
    case RW_I93_SUBSTATE_WAIT_RESET_DSFID_AFI:
        return ("WAIT_RESET_DSFID_AFI");
    case RW_I93_SUBSTATE_CHECK_READ_ONLY:
        return ("CHECK_READ_ONLY");
    case RW_I93_SUBSTATE_WRITE_CC_NDEF_TLV:
        return ("WRITE_CC_NDEF_TLV");
    case RW_I93_SUBSTATE_WAIT_UPDATE_CC:
        return ("WAIT_UPDATE_CC");
    case RW_I93_SUBSTATE_LOCK_NDEF_TLV:
        return ("LOCK_NDEF_TLV");
    case RW_I93_SUBSTATE_WAIT_LOCK_CC:
        return ("WAIT_LOCK_CC");
    default:
        return ("???? UNKNOWN SUBSTATE");
    }
}

/*******************************************************************************
**
** Function         rw_i93_get_tag_name
**
** Description      This function returns the tag name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_i93_get_tag_name (UINT8 product_version)
{
    switch (product_version)
    {
    case RW_I93_ICODE_SLI:
        return ("SLI/SLIX");
    case RW_I93_ICODE_SLI_S:
        return ("SLI-S/SLIX-S");
    case RW_I93_ICODE_SLI_L:
        return ("SLI-L/SLIX-L");
    case RW_I93_TAG_IT_HF_I_PLUS_INLAY:
        return ("Tag-it HF-I Plus Inlay");
    case RW_I93_TAG_IT_HF_I_PLUS_CHIP:
        return ("Tag-it HF-I Plus Chip");
    case RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY:
        return ("Tag-it HF-I Standard Chip/Inlyas");
    case RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY:
        return ("Tag-it HF-I Pro Chip/Inlays");
    case RW_I93_STM_LRI1K:
        return ("LRi1K");
    case RW_I93_STM_LRI2K:
        return ("LRi2K");
    case RW_I93_STM_LRIS2K:
        return ("LRiS2K");
    case RW_I93_STM_LRIS64K:
        return ("LRiS64K");
    case RW_I93_STM_M24LR64_R:
        return ("M24LR64");
    case RW_I93_STM_M24LR04E_R:
        return ("M24LR04E");
    case RW_I93_STM_M24LR16E_R:
        return ("M24LR16E");
    case RW_I93_STM_M24LR64E_R:
        return ("M24LR64E");
    case RW_I93_UNKNOWN_PRODUCT:
    default:
        return ("UNKNOWN");
    }
}

#endif

#endif /* (NFC_INCLUDED == TRUE) */
