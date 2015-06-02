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
 *  This file contains the implementation for Type 1 tag in Reader/Writer
 *  mode.
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

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
extern unsigned char appl_dta_mode_flag;
#endif
/* Local Functions */
static tRW_EVENT rw_t1t_handle_rid_rsp (BT_HDR *p_pkt);
static void rw_t1t_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data);
static void rw_t1t_process_frame_error (void);
static void rw_t1t_process_error (void);
static void rw_t1t_handle_presence_check_rsp (tNFC_STATUS status);
#if (BT_TRACE_VERBOSE == TRUE)
static char *rw_t1t_get_state_name (UINT8 state);
static char *rw_t1t_get_sub_state_name (UINT8 sub_state);
static char *rw_t1t_get_event_name (UINT8 event);
#endif

/*******************************************************************************
**
** Function         rw_t1t_data_cback
**
** Description      This callback function handles data from NFCC.
**
** Returns          none
**
*******************************************************************************/
static void rw_t1t_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    tRW_T1T_CB              *p_t1t      = &rw_cb.tcb.t1t;
    tRW_EVENT               rw_event    = RW_RAW_FRAME_EVT;
    BOOLEAN                 b_notify    = TRUE;
    tRW_DATA                evt_data;
    BT_HDR                  *p_pkt;
    UINT8                   *p;
    tT1T_CMD_RSP_INFO       *p_cmd_rsp_info     = (tT1T_CMD_RSP_INFO *) rw_cb.tcb.t1t.p_cmd_rsp_info;
#if (BT_TRACE_VERBOSE == TRUE)
    UINT8                   begin_state         = p_t1t->state;
#endif

    p_pkt = (BT_HDR *) (p_data->data.p_data);
    if (p_pkt == NULL)
        return;
    /* Assume the data is just the response byte sequence */
    p = (UINT8 *) (p_pkt + 1) + p_pkt->offset;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("rw_t1t_data_cback (): state:%s (%d)", rw_t1t_get_state_name (p_t1t->state), p_t1t->state);
#else
    RW_TRACE_DEBUG1 ("rw_t1t_data_cback (): state=%d", p_t1t->state);
#endif

    evt_data.status = NFC_STATUS_OK;

    if(  (p_t1t->state == RW_T1T_STATE_IDLE)
       ||(!p_cmd_rsp_info)  )
    {
        /* If previous command was retransmitted and if response is pending to previous command retransmission,
         * check if lenght and ADD/ADD8/ADDS field matches the expected value of previous
         * retransmited command response. However, ignore ADD field if the command was RALL/RID
         */
        if (  (p_t1t->prev_cmd_rsp_info.pend_retx_rsp)
            &&(p_t1t->prev_cmd_rsp_info.rsp_len == p_pkt->len)
            &&((p_t1t->prev_cmd_rsp_info.op_code == T1T_CMD_RID) || (p_t1t->prev_cmd_rsp_info.op_code == T1T_CMD_RALL) || (p_t1t->prev_cmd_rsp_info.addr == *p))  )
        {
            /* Response to previous command retransmission */
            RW_TRACE_ERROR2 ("T1T Response to previous command in Idle state. command=0x%02x, Remaining max retx rsp:0x%02x ", p_t1t->prev_cmd_rsp_info.op_code, p_t1t->prev_cmd_rsp_info.pend_retx_rsp - 1);
            p_t1t->prev_cmd_rsp_info.pend_retx_rsp--;
            GKI_freebuf (p_pkt);
        }
        else
        {
            /* Raw frame event */
            evt_data.data.p_data = p_pkt;
            (*rw_cb.p_cback) (RW_T1T_RAW_FRAME_EVT, (tRW_DATA *) &evt_data);
        }
        return;
    }

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
    /* Update rx stats */
    rw_main_update_rx_stats (p_pkt->len);
#endif  /* RW_STATS_INCLUDED */


    if (  (p_pkt->len != p_cmd_rsp_info->rsp_len)
        ||((p_cmd_rsp_info->opcode != T1T_CMD_RALL) && (p_cmd_rsp_info->opcode != T1T_CMD_RID) && (*p != p_t1t->addr))  )

    {
        /* If previous command was retransmitted and if response is pending to previous command retransmission,
         * then check if lenght and ADD/ADD8/ADDS field matches the expected value of previous
         * retransmited command response. However, ignore ADD field if the command was RALL/RID
         */
        if (  (p_t1t->prev_cmd_rsp_info.pend_retx_rsp)
            &&(p_t1t->prev_cmd_rsp_info.rsp_len == p_pkt->len)
            &&((p_t1t->prev_cmd_rsp_info.op_code == T1T_CMD_RID) || (p_t1t->prev_cmd_rsp_info.op_code == T1T_CMD_RALL) || (p_t1t->prev_cmd_rsp_info.addr == *p))  )
        {
            RW_TRACE_ERROR2 ("T1T Response to previous command. command=0x%02x, Remaining max retx rsp:0x%02x", p_t1t->prev_cmd_rsp_info.op_code, p_t1t->prev_cmd_rsp_info.pend_retx_rsp - 1);
            p_t1t->prev_cmd_rsp_info.pend_retx_rsp--;
        }
        else
        {
            /* Stop timer as some response to current command is received */
            nfc_stop_quick_timer (&p_t1t->timer);
            /* Retrasmit the last sent command if retry-count < max retry */
#if (BT_TRACE_VERBOSE == TRUE)
            RW_TRACE_ERROR2 ("T1T Frame error. state=%s command (opcode) = 0x%02x", rw_t1t_get_state_name (p_t1t->state), p_cmd_rsp_info->opcode);
#else
            RW_TRACE_ERROR2 ("T1T Frame error. state=0x%02x command = 0x%02x ", p_t1t->state, p_cmd_rsp_info->opcode);
#endif
            rw_t1t_process_frame_error ();
        }
        GKI_freebuf (p_pkt);
        return;
    }

    /* Stop timer as response to current command is received */
    nfc_stop_quick_timer (&p_t1t->timer);

    RW_TRACE_EVENT2 ("RW RECV [%s]:0x%x RSP", t1t_info_to_str (p_cmd_rsp_info), p_cmd_rsp_info->opcode);

    /* If we did not receive response to all retransmitted previous command,
     * dont expect that as response have come for the current command itself.
     */
    if (p_t1t->prev_cmd_rsp_info.pend_retx_rsp)
        memset (&(p_t1t->prev_cmd_rsp_info), 0, sizeof (tRW_T1T_PREV_CMD_RSP_INFO));

    if (rw_cb.cur_retry)
    {
    /* If the current command was retransmitted to get this response, we might get
       response later to all or some of the retrasnmission of the current command
     */
        p_t1t->prev_cmd_rsp_info.addr          = ((p_cmd_rsp_info->opcode != T1T_CMD_RALL) && (p_cmd_rsp_info->opcode != T1T_CMD_RID))? p_t1t->addr:0;
        p_t1t->prev_cmd_rsp_info.rsp_len       = p_cmd_rsp_info->rsp_len;
        p_t1t->prev_cmd_rsp_info.op_code       = p_cmd_rsp_info->opcode;
        p_t1t->prev_cmd_rsp_info.pend_retx_rsp = (UINT8) rw_cb.cur_retry;
    }

    rw_cb.cur_retry = 0;

    if (p_cmd_rsp_info->opcode == T1T_CMD_RID)
    {
        rw_event = rw_t1t_handle_rid_rsp (p_pkt);
    }
    else
    {
        rw_event = rw_t1t_handle_rsp (p_cmd_rsp_info, &b_notify, p, &evt_data.status);
    }

    if (b_notify)
    {
        if(  (p_t1t->state != RW_T1T_STATE_READ)
           &&(p_t1t->state != RW_T1T_STATE_WRITE)  )
        {
            GKI_freebuf (p_pkt);
            evt_data.data.p_data = NULL;
        }
        else
        {
            evt_data.data.p_data = p_pkt;
        }
        rw_t1t_handle_op_complete ();
        (*rw_cb.p_cback) (rw_event, (tRW_DATA *) &evt_data);
    }
    else
        GKI_freebuf (p_pkt);

#if (BT_TRACE_VERBOSE == TRUE)
    if (begin_state != p_t1t->state)
    {
        RW_TRACE_DEBUG2 ("RW T1T state changed:<%s> -> <%s>",
                          rw_t1t_get_state_name (begin_state),
                          rw_t1t_get_state_name (p_t1t->state));
    }
#endif
}

/*******************************************************************************
**
** Function         rw_t1t_conn_cback
**
** Description      This callback function receives the events/data from NFCC.
**
** Returns          none
**
*******************************************************************************/
void rw_t1t_conn_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    tRW_T1T_CB          *p_t1t  = &rw_cb.tcb.t1t;
    tRW_READ_DATA       evt_data;

    RW_TRACE_DEBUG2 ("rw_t1t_conn_cback: conn_id=%i, evt=0x%x", conn_id, event);
    /* Only handle static conn_id */
    if (conn_id != NFC_RF_CONN_ID)
    {
        RW_TRACE_WARNING1 ("rw_t1t_conn_cback - Not static connection id: =%i", conn_id);
        return;
    }

    switch (event)
    {
    case NFC_CONN_CREATE_CEVT:
    case NFC_CONN_CLOSE_CEVT:
        break;

    case NFC_DEACTIVATE_CEVT:
#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
        /* Display stats */
        rw_main_log_stats ();
#endif  /* RW_STATS_INCLUDED */

        /* Stop t1t timer (if started) */
        nfc_stop_quick_timer (&p_t1t->timer);

        /* Free cmd buf for retransmissions */
        if (p_t1t->p_cur_cmd_buf)
        {
            GKI_freebuf (p_t1t->p_cur_cmd_buf);
            p_t1t->p_cur_cmd_buf = NULL;
        }

        p_t1t->state = RW_T1T_STATE_NOT_ACTIVATED;
        NFC_SetStaticRfCback (NULL);
        break;

    case NFC_DATA_CEVT:
        if (  (p_data != NULL)
            &&(p_data->data.status == NFC_STATUS_OK)  )
        {
            rw_t1t_data_cback (conn_id, event, p_data);
            break;
        }
        /* Data event with error status...fall through to NFC_ERROR_CEVT case */

    case NFC_ERROR_CEVT:
        if (  (p_t1t->state == RW_T1T_STATE_NOT_ACTIVATED)
            ||(p_t1t->state == RW_T1T_STATE_IDLE)  )
        {
#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
            rw_main_update_trans_error_stats ();
#endif  /* RW_STATS_INCLUDED */

            if (event == NFC_ERROR_CEVT)
                evt_data.status = (tNFC_STATUS) (*(UINT8*) p_data);
            else if (p_data)
                evt_data.status = p_data->status;
            else
                evt_data.status = NFC_STATUS_FAILED;

            evt_data.p_data = NULL;
            (*rw_cb.p_cback) (RW_T1T_INTF_ERROR_EVT, (tRW_DATA *) &evt_data);
            break;
        }
        nfc_stop_quick_timer (&p_t1t->timer);

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
        rw_main_update_trans_error_stats ();
#endif  /* RW_STATS_INCLUDED */

        if (p_t1t->state == RW_T1T_STATE_CHECK_PRESENCE)
        {
            rw_t1t_handle_presence_check_rsp (NFC_STATUS_FAILED);
        }
        else
        {
            rw_t1t_process_error ();
        }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        /* Free the response buffer in case of invalid response*/
        GKI_freebuf((BT_HDR *) (p_data->data.p_data));
#endif
        break;

    default:
        break;

    }
}

/*******************************************************************************
**
** Function         rw_t1t_send_static_cmd
**
** Description      This function composes a Type 1 Tag command for static
**                  memory and send through NCI to NFCC.
**
** Returns          NFC_STATUS_OK if the command is successfuly sent to NCI
**                  otherwise, error status
**
*******************************************************************************/
tNFC_STATUS rw_t1t_send_static_cmd (UINT8 opcode, UINT8 add, UINT8 dat)
{
    tNFC_STATUS             status  = NFC_STATUS_FAILED;
    tRW_T1T_CB              *p_t1t  = &rw_cb.tcb.t1t;
    const tT1T_CMD_RSP_INFO *p_cmd_rsp_info = t1t_cmd_to_rsp_info (opcode);
    BT_HDR                  *p_data;
    UINT8                   *p;

    if (p_cmd_rsp_info)
    {
        /* a valid opcode for RW */
        p_data = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);
        if (p_data)
        {
            p_t1t->p_cmd_rsp_info   = (tT1T_CMD_RSP_INFO *) p_cmd_rsp_info;
            p_t1t->addr             = add;
            p_data->offset          = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
            p                       = (UINT8 *) (p_data + 1) + p_data->offset;
            UINT8_TO_BE_STREAM (p, opcode);
            UINT8_TO_BE_STREAM (p, add);
            UINT8_TO_BE_STREAM (p, dat);

            ARRAY_TO_STREAM (p, p_t1t->mem, T1T_CMD_UID_LEN);
            p_data->len     = p_cmd_rsp_info->cmd_len;

            /* Indicate first attempt to send command, back up cmd buffer in case needed for retransmission */
            rw_cb.cur_retry = 0;
            memcpy (p_t1t->p_cur_cmd_buf, p_data, sizeof (BT_HDR) + p_data->offset + p_data->len);

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
            /* Update stats */
            rw_main_update_tx_stats (p_data->len, FALSE);
#endif  /* RW_STATS_INCLUDED */

            RW_TRACE_EVENT2 ("RW SENT [%s]:0x%x CMD", t1t_info_to_str (p_cmd_rsp_info), p_cmd_rsp_info->opcode);
            if ((status = NFC_SendData (NFC_RF_CONN_ID, p_data)) == NFC_STATUS_OK)
            {
                nfc_start_quick_timer (&p_t1t->timer, NFC_TTYPE_RW_T1T_RESPONSE,
                       (RW_T1T_TOUT_RESP * QUICK_TIMER_TICKS_PER_SEC) / 1000);
            }
        }
        else
        {
            status = NFC_STATUS_NO_BUFFERS;
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         rw_t1t_send_dyn_cmd
**
** Description      This function composes a Type 1 Tag command for dynamic memory
**                  and send through NCI to NFCC.
**
** Returns          NFC_STATUS_OK if the command is successfuly sent to NCI
**                  otherwise, error status
**
*******************************************************************************/
tNFC_STATUS rw_t1t_send_dyn_cmd (UINT8 opcode, UINT8 add, UINT8 *p_dat)
{
    tNFC_STATUS             status  = NFC_STATUS_FAILED;
    tRW_T1T_CB              *p_t1t  = &rw_cb.tcb.t1t;
    const tT1T_CMD_RSP_INFO *p_cmd_rsp_info = t1t_cmd_to_rsp_info (opcode);
    BT_HDR                  *p_data;
    UINT8                   *p;

    if (p_cmd_rsp_info)
    {
        /* a valid opcode for RW */
        p_data = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);
        if (p_data)
        {
            p_t1t->p_cmd_rsp_info   = (tT1T_CMD_RSP_INFO *) p_cmd_rsp_info;
            p_t1t->addr             = add;
            p_data->offset          = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
            p                       = (UINT8 *) (p_data + 1) + p_data->offset;
            UINT8_TO_BE_STREAM (p, opcode);
            UINT8_TO_BE_STREAM (p, add);

            if (p_dat)
            {
                ARRAY_TO_STREAM (p, p_dat, 8);
            }
            else
            {
                memset (p, 0, 8);
                p += 8;
            }
            ARRAY_TO_STREAM (p, p_t1t->mem, T1T_CMD_UID_LEN);
            p_data->len     = p_cmd_rsp_info->cmd_len;

            /* Indicate first attempt to send command, back up cmd buffer in case needed for retransmission */
            rw_cb.cur_retry = 0;
            memcpy (p_t1t->p_cur_cmd_buf, p_data, sizeof (BT_HDR) + p_data->offset + p_data->len);

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
            /* Update stats */
            rw_main_update_tx_stats (p_data->len, FALSE);
#endif  /* RW_STATS_INCLUDED */

            RW_TRACE_EVENT2 ("RW SENT [%s]:0x%x CMD", t1t_info_to_str (p_cmd_rsp_info), p_cmd_rsp_info->opcode);

            if ((status = NFC_SendData (NFC_RF_CONN_ID, p_data)) == NFC_STATUS_OK)
            {
                nfc_start_quick_timer (&p_t1t->timer, NFC_TTYPE_RW_T1T_RESPONSE,
                       (RW_T1T_TOUT_RESP * QUICK_TIMER_TICKS_PER_SEC) / 1000);
            }
        }
        else
        {
            status = NFC_STATUS_NO_BUFFERS;
        }
    }
    return status;
}

/*****************************************************************************
**
** Function         rw_t1t_handle_rid_rsp
**
** Description      Handles response to RID: Collects HR, UID, notify up the
**                  stack
**
** Returns          event to notify application
**
*****************************************************************************/
static tRW_EVENT rw_t1t_handle_rid_rsp (BT_HDR *p_pkt)
{
    tRW_T1T_CB  *p_t1t   = &rw_cb.tcb.t1t;
    tRW_DATA    evt_data;
    UINT8       *p_rid_rsp;

    evt_data.status      = NFC_STATUS_OK;
    evt_data.data.p_data = p_pkt;

    /* Assume the data is just the response byte sequence */
    p_rid_rsp = (UINT8 *) (p_pkt + 1) + p_pkt->offset;

    /* Response indicates tag is present */
    if (p_t1t->state == RW_T1T_STATE_CHECK_PRESENCE)
    {
        /* If checking for the presence of the tag then just notify */
        return RW_T1T_PRESENCE_CHECK_EVT;
    }

    /* Extract HR and UID from response */
    STREAM_TO_ARRAY (p_t1t->hr,  p_rid_rsp, T1T_HR_LEN);

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_DEBUG2 ("hr0:0x%x, hr1:0x%x", p_t1t->hr[0], p_t1t->hr[1]);
    RW_TRACE_DEBUG4 ("rw_t1t_handle_rid_rsp (): UID0-3=%02x%02x%02x%02x", p_rid_rsp[0], p_rid_rsp[1], p_rid_rsp[2], p_rid_rsp[3]);
#else
    RW_TRACE_DEBUG0 ("rw_t1t_handle_rid_rsp ()");
#endif

    /* Fetch UID0-3 from RID response message */
    STREAM_TO_ARRAY (p_t1t->mem,  p_rid_rsp, T1T_CMD_UID_LEN);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    /* Free the RID response buffer */
    GKI_freebuf (p_pkt);
#endif

    /* Notify RID response Event */
    return RW_T1T_RID_EVT;
}

/*******************************************************************************
**
** Function         rw_t1t_select
**
** Description      This function will set the callback function to
**                  receive data from lower layers and also send rid command
**
** Returns          none
**
*******************************************************************************/
tNFC_STATUS rw_t1t_select (UINT8 hr[T1T_HR_LEN], UINT8 uid[T1T_CMD_UID_LEN])
{
    tNFC_STATUS status  = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;

    p_t1t->state = RW_T1T_STATE_NOT_ACTIVATED;

    /* Alloc cmd buf for retransmissions */
    if (p_t1t->p_cur_cmd_buf ==  NULL)
    {
        if ((p_t1t->p_cur_cmd_buf = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID)) == NULL)
        {
            RW_TRACE_ERROR0 ("rw_t1t_select: unable to allocate buffer for retransmission");
            return status;
        }
    }

    memcpy (p_t1t->hr, hr, T1T_HR_LEN);
    memcpy (p_t1t->mem, uid, T1T_CMD_UID_LEN);

    NFC_SetStaticRfCback (rw_t1t_conn_cback);

    p_t1t->state    = RW_T1T_STATE_IDLE;

    return NFC_STATUS_OK;
}

/*******************************************************************************
**
** Function         rw_t1t_process_timeout
**
** Description      process timeout event
**
** Returns          none
**
*******************************************************************************/
void rw_t1t_process_timeout (TIMER_LIST_ENT *p_tle)
{
    tRW_T1T_CB        *p_t1t  = &rw_cb.tcb.t1t;

#if (BT_TRACE_VERBOSE == TRUE)
    RW_TRACE_ERROR2 ("T1T timeout. state=%s command (opcode)=0x%02x ", rw_t1t_get_state_name (p_t1t->state), (rw_cb.tcb.t1t.p_cmd_rsp_info)->opcode);
#else
    RW_TRACE_ERROR2 ("T1T timeout. state=0x%02x command=0x%02x ", p_t1t->state, (rw_cb.tcb.t1t.p_cmd_rsp_info)->opcode);
#endif

    if (p_t1t->state == RW_T1T_STATE_CHECK_PRESENCE)
    {
        /* Tag has moved from range */
        rw_t1t_handle_presence_check_rsp (NFC_STATUS_FAILED);
    }
    else if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        rw_t1t_process_error ();
    }
}


/*******************************************************************************
**
** Function         rw_t1t_process_frame_error
**
** Description      Process frame crc error
**
** Returns          none
**
*******************************************************************************/
static void rw_t1t_process_frame_error (void)
{
#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
    /* Update stats */
    rw_main_update_crc_error_stats ();
#endif  /* RW_STATS_INCLUDED */

    /* Process the error */
    rw_t1t_process_error ();
}

/*******************************************************************************
**
** Function         rw_t1t_process_error
**
** Description      process timeout event
**
** Returns          none
**
*******************************************************************************/
static void rw_t1t_process_error (void)
{
    tRW_READ_DATA           evt_data;
    tRW_EVENT               rw_event;
    BT_HDR                  *p_cmd_buf;
    tRW_T1T_CB              *p_t1t  = &rw_cb.tcb.t1t;
    tT1T_CMD_RSP_INFO       *p_cmd_rsp_info = (tT1T_CMD_RSP_INFO *) rw_cb.tcb.t1t.p_cmd_rsp_info;
    tRW_DETECT_NDEF_DATA    ndef_data;

    RW_TRACE_DEBUG1 ("rw_t1t_process_error () State: %u", p_t1t->state);

    /* Retry sending command if retry-count < max */
    if (rw_cb.cur_retry < RW_MAX_RETRIES)
    {
        /* retry sending the command */
        rw_cb.cur_retry++;

        RW_TRACE_DEBUG2 ("T1T retransmission attempt %i of %i", rw_cb.cur_retry, RW_MAX_RETRIES);

        /* allocate a new buffer for message */
        if ((p_cmd_buf = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID)) != NULL)
        {
            memcpy (p_cmd_buf, p_t1t->p_cur_cmd_buf, sizeof (BT_HDR) + p_t1t->p_cur_cmd_buf->offset + p_t1t->p_cur_cmd_buf->len);

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
            /* Update stats */
            rw_main_update_tx_stats (p_cmd_buf->len, TRUE);
#endif  /* RW_STATS_INCLUDED */

            if (NFC_SendData (NFC_RF_CONN_ID, p_cmd_buf) == NFC_STATUS_OK)
            {
                /* Start timer for waiting for response */
                nfc_start_quick_timer (&p_t1t->timer, NFC_TTYPE_RW_T1T_RESPONSE,
                                       (RW_T1T_TOUT_RESP * QUICK_TIMER_TICKS_PER_SEC)/1000);

                return;
            }
        }
    }
    else
    {
    /* we might get response later to all or some of the retrasnmission
     * of the current command, update previous command response information */
        RW_TRACE_DEBUG1 ("T1T maximum retransmission attempts reached (%i)", RW_MAX_RETRIES);
        p_t1t->prev_cmd_rsp_info.addr          = ((p_cmd_rsp_info->opcode != T1T_CMD_RALL) && (p_cmd_rsp_info->opcode != T1T_CMD_RID))? p_t1t->addr:0;
        p_t1t->prev_cmd_rsp_info.rsp_len       = p_cmd_rsp_info->rsp_len;
        p_t1t->prev_cmd_rsp_info.op_code       = p_cmd_rsp_info->opcode;
        p_t1t->prev_cmd_rsp_info.pend_retx_rsp = RW_MAX_RETRIES;
    }

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
    /* update failure count */
    rw_main_update_fail_stats ();
#endif  /* RW_STATS_INCLUDED */

    rw_event        = rw_t1t_info_to_event (p_cmd_rsp_info);
    if (p_t1t->state != RW_T1T_STATE_NOT_ACTIVATED)
        rw_t1t_handle_op_complete ();

    evt_data.status = NFC_STATUS_TIMEOUT;
    if (rw_event == RW_T2T_NDEF_DETECT_EVT)
    {
        ndef_data.status    = evt_data.status;
        ndef_data.protocol  = NFC_PROTOCOL_T1T;
        ndef_data.flags     = RW_NDEF_FL_UNKNOWN;
        ndef_data.max_size  = 0;
        ndef_data.cur_size  = 0;
        (*rw_cb.p_cback) (rw_event, (tRW_DATA *) &ndef_data);
    }
    else
    {
        evt_data.p_data = NULL;
        (*rw_cb.p_cback) (rw_event, (tRW_DATA *) &evt_data);
    }
}

/*****************************************************************************
**
** Function         rw_t1t_handle_presence_check_rsp
**
** Description      Handle response to presence check
**
** Returns          Nothing
**
*****************************************************************************/
void rw_t1t_handle_presence_check_rsp (tNFC_STATUS status)
{
    tRW_READ_DATA   evt_data;

    /* Notify, Tag is present or not */
    evt_data.status = status;
    rw_t1t_handle_op_complete ();

    (*(rw_cb.p_cback)) (RW_T1T_PRESENCE_CHECK_EVT, (tRW_DATA *) &evt_data);
}

/*****************************************************************************
**
** Function         rw_t1t_handle_op_complete
**
** Description      Reset to IDLE state
**
** Returns          Nothing
**
*****************************************************************************/
void rw_t1t_handle_op_complete (void)
{
    tRW_T1T_CB      *p_t1t  = &rw_cb.tcb.t1t;

    p_t1t->state    = RW_T1T_STATE_IDLE;
#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if ((appl_dta_mode_flag == 0) && (p_t1t->state != RW_T1T_STATE_READ_NDEF))
#else
    if (p_t1t->state != RW_T1T_STATE_READ_NDEF)
#endif
    {
        p_t1t->b_update = FALSE;
        p_t1t->b_rseg   = FALSE;
    }
    p_t1t->substate = RW_T1T_SUBSTATE_NONE;
#endif
    return;
}

/*****************************************************************************
**
** Function         RW_T1tPresenceCheck
**
** Description
**      Check if the tag is still in the field.
**
**      The RW_T1T_PRESENCE_CHECK_EVT w/ status is used to indicate presence
**      or non-presence.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_T1tPresenceCheck (void)
{
    tNFC_STATUS retval = NFC_STATUS_OK;
    tRW_DATA evt_data;
    tRW_CB *p_rw_cb = &rw_cb;

    RW_TRACE_API0 ("RW_T1tPresenceCheck");

    /* If RW_SelectTagType was not called (no conn_callback) return failure */
    if (!p_rw_cb->p_cback)
    {
        retval = NFC_STATUS_FAILED;
    }
    /* If we are not activated, then RW_T1T_PRESENCE_CHECK_EVT status=FAIL */
    else if (p_rw_cb->tcb.t1t.state == RW_T1T_STATE_NOT_ACTIVATED)
    {
        evt_data.status = NFC_STATUS_FAILED;
        (*p_rw_cb->p_cback) (RW_T1T_PRESENCE_CHECK_EVT, &evt_data);
    }
    /* If command is pending, assume tag is still present */
    else if (p_rw_cb->tcb.t1t.state != RW_T1T_STATE_IDLE)
    {
        evt_data.status = NFC_STATUS_OK;
        (*p_rw_cb->p_cback) (RW_T1T_PRESENCE_CHECK_EVT, &evt_data);
    }
    else
    {
        /* IDLE state: send a RID command to the tag to see if it responds */
        if((retval = rw_t1t_send_static_cmd (T1T_CMD_RID, 0, 0))== NFC_STATUS_OK)
        {
            p_rw_cb->tcb.t1t.state = RW_T1T_STATE_CHECK_PRESENCE;
        }
    }

    return (retval);
}

/*****************************************************************************
**
** Function         RW_T1tRid
**
** Description
**      This function sends a RID command for Reader/Writer mode.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_T1tRid (void)
{
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    tNFC_STATUS status  = NFC_STATUS_FAILED;

    RW_TRACE_API0 ("RW_T1tRid");

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tRid - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    /* send a RID command */
    if((status = rw_t1t_send_static_cmd (T1T_CMD_RID, 0, 0))== NFC_STATUS_OK)
    {
        p_t1t->state = RW_T1T_STATE_READ;
    }

    return (status);
}

/*******************************************************************************
**
** Function         RW_T1tReadAll
**
** Description      This function sends a RALL command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tReadAll (void)
{
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    tNFC_STATUS status  = NFC_STATUS_FAILED;

    RW_TRACE_API0 ("RW_T1tReadAll");

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tReadAll - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    /* send RALL command */
    if ((status = rw_t1t_send_static_cmd (T1T_CMD_RALL, 0, 0)) == NFC_STATUS_OK)
    {
        p_t1t->state = RW_T1T_STATE_READ;
    }

    return status;
}

/*******************************************************************************
**
** Function         RW_T1tRead
**
** Description      This function sends a READ command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tRead (UINT8 block, UINT8 byte)
{
    tNFC_STATUS status  = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    UINT8       addr;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tRead - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    /* send READ command */
    RW_T1T_BLD_ADD ((addr), (block), (byte));
    if ((status = rw_t1t_send_static_cmd (T1T_CMD_READ, addr, 0)) == NFC_STATUS_OK)
    {
        p_t1t->state = RW_T1T_STATE_READ;
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tWriteErase
**
** Description      This function sends a WRITE-E command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tWriteErase (UINT8 block, UINT8 byte, UINT8 new_byte)
{
    tNFC_STATUS status  = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    UINT8       addr;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteErase - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }
    if (  (p_t1t->tag_attribute == RW_T1_TAG_ATTRB_READ_ONLY)
        &&(block != T1T_CC_BLOCK)
        &&(byte  != T1T_CC_RWA_OFFSET)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tWriteErase - Tag is in Read only state");
        return (NFC_STATUS_REFUSED);
    }
    if (  (block >= T1T_STATIC_BLOCKS)
        ||(byte  >= T1T_BLOCK_SIZE   )  )
    {
        RW_TRACE_ERROR2 ("RW_T1tWriteErase - Invalid Block/byte: %u / %u", block, byte);
        return (NFC_STATUS_REFUSED);
    }
    if(  (block == T1T_UID_BLOCK)
       ||(block == T1T_RES_BLOCK)  )
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteErase - Cannot write to Locked block: %u", block);
        return (NFC_STATUS_REFUSED);
    }
    /* send WRITE-E command */
    RW_T1T_BLD_ADD ((addr), (block), (byte));
    if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_E, addr, new_byte)) == NFC_STATUS_OK)
    {
        p_t1t->state = RW_T1T_STATE_WRITE;
        if (block < T1T_BLOCKS_PER_SEGMENT)
        {
            p_t1t->b_update = FALSE;
            p_t1t->b_rseg   = FALSE;
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tWriteNoErase
**
** Description      This function sends a WRITE-NE command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tWriteNoErase (UINT8 block, UINT8 byte, UINT8 new_byte)
{
    tNFC_STATUS status  = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    UINT8       addr;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteNoErase - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }
    if (  (p_t1t->tag_attribute == RW_T1_TAG_ATTRB_READ_ONLY)
        &&(block != T1T_CC_BLOCK)
        &&(byte  != T1T_CC_RWA_OFFSET)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tWriteErase - Tag is in Read only state");
        return (NFC_STATUS_REFUSED);
    }
    if (  (block >= T1T_STATIC_BLOCKS)
        ||(byte  >= T1T_BLOCK_SIZE   )  )
    {
        RW_TRACE_ERROR2 ("RW_T1tWriteErase - Invalid Block/byte: %u / %u", block, byte);
        return (NFC_STATUS_REFUSED);
    }
    if(  (block == T1T_UID_BLOCK)
       ||(block == T1T_RES_BLOCK)  )
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteNoErase - Cannot write to Locked block: %u", block);
        return (NFC_STATUS_REFUSED);
    }
    /* send WRITE-NE command */
    RW_T1T_BLD_ADD ((addr), (block), (byte));
    if ((status = rw_t1t_send_static_cmd (T1T_CMD_WRITE_NE, addr, new_byte)) == NFC_STATUS_OK)
    {
        p_t1t->state = RW_T1T_STATE_WRITE;
        if (block < T1T_BLOCKS_PER_SEGMENT)
        {
            p_t1t->b_update = FALSE;
            p_t1t->b_rseg   = FALSE;
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tReadSeg
**
** Description      This function sends a RSEG command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tReadSeg (UINT8 segment)
{
    tNFC_STATUS status  = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t  = &rw_cb.tcb.t1t;
    UINT8       adds;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tReadSeg - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }
    if (segment >=  T1T_MAX_SEGMENTS)
    {
        RW_TRACE_ERROR1 ("RW_T1tReadSeg - Invalid Segment: %u", segment);
        return (NFC_STATUS_REFUSED);
    }
    if (rw_cb.tcb.t1t.hr[0] != T1T_STATIC_HR0)
    {
        /* send RSEG command */
        RW_T1T_BLD_ADDS ((adds), (segment));
        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_RSEG, adds, NULL)) == NFC_STATUS_OK)
        {
            p_t1t->state = RW_T1T_STATE_READ;
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tRead8
**
** Description      This function sends a READ8 command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tRead8 (UINT8 block)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t= &rw_cb.tcb.t1t;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tRead8 - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    if (rw_cb.tcb.t1t.hr[0] != T1T_STATIC_HR0 || rw_cb.tcb.t1t.hr[1] >= RW_T1T_HR1_MIN)
    {
        /* send READ8 command */
        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_READ8, block, NULL)) == NFC_STATUS_OK)
        {
            p_t1t->state = RW_T1T_STATE_READ;
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tWriteErase8
**
** Description      This function sends a WRITE-E8 command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tWriteErase8 (UINT8 block, UINT8 *p_new_dat)
{
    tRW_T1T_CB  *p_t1t= &rw_cb.tcb.t1t;
    tNFC_STATUS status = NFC_STATUS_FAILED;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteErase8 - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    if (  (p_t1t->tag_attribute == RW_T1_TAG_ATTRB_READ_ONLY)
        &&(block != T1T_CC_BLOCK)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tWriteErase8 - Tag is in Read only state");
        return (NFC_STATUS_REFUSED);
    }

    if(  (block == T1T_UID_BLOCK)
       ||(block == T1T_RES_BLOCK)  )
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteErase8 - Cannot write to Locked block: %u", block);
        return (NFC_STATUS_REFUSED);
    }

    if (rw_cb.tcb.t1t.hr[0] != T1T_STATIC_HR0 || rw_cb.tcb.t1t.hr[1] >= RW_T1T_HR1_MIN)
    {
        /* send WRITE-E8 command */
        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_E8, block, p_new_dat)) == NFC_STATUS_OK)
        {
            p_t1t->state = RW_T1T_STATE_WRITE;
            if (block < T1T_BLOCKS_PER_SEGMENT)
            {
                p_t1t->b_update = FALSE;
                p_t1t->b_rseg   = FALSE;
            }
        }
    }
    return status;
}

/*******************************************************************************
**
** Function         RW_T1tWriteNoErase8
**
** Description      This function sends a WRITE-NE8 command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_T1tWriteNoErase8 (UINT8 block, UINT8 *p_new_dat)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;
    tRW_T1T_CB  *p_t1t= &rw_cb.tcb.t1t;

    if (p_t1t->state != RW_T1T_STATE_IDLE)
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteNoErase8 - Busy - State: %u", p_t1t->state);
        return (NFC_STATUS_BUSY);
    }

    if (  (p_t1t->tag_attribute == RW_T1_TAG_ATTRB_READ_ONLY)
        &&(block != T1T_CC_BLOCK)  )
    {
        RW_TRACE_ERROR0 ("RW_T1tWriteNoErase8 - Tag is in Read only state");
        return (NFC_STATUS_REFUSED);
    }

    if(  (block == T1T_UID_BLOCK)
       ||(block == T1T_RES_BLOCK)  )
    {
        RW_TRACE_WARNING1 ("RW_T1tWriteNoErase8 - Cannot write to Locked block: %u", block);
        return (NFC_STATUS_REFUSED);
    }

    if (rw_cb.tcb.t1t.hr[0] != T1T_STATIC_HR0 || rw_cb.tcb.t1t.hr[1] >= RW_T1T_HR1_MIN)
    {
        /* send WRITE-NE command */
        if ((status = rw_t1t_send_dyn_cmd (T1T_CMD_WRITE_NE8, block, p_new_dat)) == NFC_STATUS_OK)
        {
            p_t1t->state    = RW_T1T_STATE_WRITE;
            if (block < T1T_BLOCKS_PER_SEGMENT)
            {
                p_t1t->b_update = FALSE;
                p_t1t->b_rseg   = FALSE;
            }
        }
    }
    return status;
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         rw_t1t_get_state_name
**
** Description      This function returns the state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *rw_t1t_get_state_name (UINT8 state)
{
    switch (state)
    {
    case RW_T1T_STATE_IDLE:
        return ("IDLE");
    case RW_T1T_STATE_NOT_ACTIVATED:
        return ("NOT_ACTIVATED");
    case RW_T1T_STATE_READ:
        return ("APP_READ");
    case RW_T1T_STATE_WRITE:
        return ("APP_WRITE");
    case RW_T1T_STATE_TLV_DETECT:
        return ("TLV_DETECTION");
    case RW_T1T_STATE_READ_NDEF:
        return ("READING_NDEF");
    case RW_T1T_STATE_WRITE_NDEF:
        return ("WRITING_NDEF");
    case RW_T1T_STATE_SET_TAG_RO:
        return ("SET_TAG_RO");
    case RW_T1T_STATE_CHECK_PRESENCE:
        return ("CHECK_PRESENCE");
    case RW_T1T_STATE_FORMAT_TAG:
        return ("FORMAT_TAG");
    default:
        return ("???? UNKNOWN STATE");
    }
}

#endif /* (BT_TRACE_VERBOSE == TRUE) */

#endif /* (NFC_INCLUDED == TRUE) */
