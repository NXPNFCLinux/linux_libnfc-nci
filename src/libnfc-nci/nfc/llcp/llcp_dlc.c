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
 *  This file contains the LLCP Data Link Connection Management
 *
 ******************************************************************************/

#include <string.h>
#include "gki.h"
#include "nfc_target.h"
#include "bt_types.h"
#include "llcp_int.h"
#include "llcp_defs.h"
#include "nfc_int.h"

static tLLCP_STATUS llcp_dlsm_idle (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data);
static tLLCP_STATUS llcp_dlsm_w4_remote_resp (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data);
static tLLCP_STATUS llcp_dlsm_w4_local_resp (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data);
static tLLCP_STATUS llcp_dlsm_connected (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data);
static tLLCP_STATUS llcp_dlsm_w4_remote_dm (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data);

#if (BT_TRACE_VERBOSE == TRUE)
static char *llcp_dlsm_get_state_name (tLLCP_DLC_STATE state);
static char *llcp_dlsm_get_event_name (tLLCP_DLC_EVENT event);
#endif

/*******************************************************************************
**
** Function         llcp_dlsm_execute
**
** Description      This function executes the state machine for data link connection.
**
** Returns          tLLCP_STATUS
**
*******************************************************************************/
tLLCP_STATUS llcp_dlsm_execute (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data)
{
    tLLCP_STATUS status;

#if (BT_TRACE_VERBOSE == TRUE)
    LLCP_TRACE_EVENT3 ("DLC (0x%02X) - state: %s, evt: %s",
                        p_dlcb->local_sap,
                        llcp_dlsm_get_state_name (p_dlcb->state),
                        llcp_dlsm_get_event_name (event));
#else
    LLCP_TRACE_EVENT3 ("DLC (0x%02X) - state: %d, evt: %d", p_dlcb->local_sap, p_dlcb->state, event);
#endif

    switch (p_dlcb->state)
    {
    case LLCP_DLC_STATE_IDLE:
        status = llcp_dlsm_idle (p_dlcb, event, p_data);
        break;

    case LLCP_DLC_STATE_W4_REMOTE_RESP:
        status = llcp_dlsm_w4_remote_resp (p_dlcb, event, p_data);
        break;

    case LLCP_DLC_STATE_W4_LOCAL_RESP:
        status = llcp_dlsm_w4_local_resp (p_dlcb, event, p_data);
        break;

    case LLCP_DLC_STATE_CONNECTED:
        status = llcp_dlsm_connected (p_dlcb, event, p_data);
        break;

    case LLCP_DLC_STATE_W4_REMOTE_DM:
        status = llcp_dlsm_w4_remote_dm (p_dlcb, event, p_data);
        break;

    default:
        status = LLCP_STATUS_FAIL;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         llcp_dlsm_idle
**
** Description      Data link connection is in idle state
**
** Returns          tLLCP_STATUS
**
*******************************************************************************/
static tLLCP_STATUS llcp_dlsm_idle (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data)
{
    tLLCP_STATUS            status = LLCP_STATUS_SUCCESS;
    tLLCP_SAP_CBACK_DATA    data;
    tLLCP_CONNECTION_PARAMS *p_params;

    switch (event)
    {
    case LLCP_DLC_EVENT_API_CONNECT_REQ:

        /* upper layer requests to create data link connection */
        p_params = (tLLCP_CONNECTION_PARAMS *)p_data;

        status = llcp_util_send_connect (p_dlcb, p_params);

        if (status == LLCP_STATUS_SUCCESS)
        {
            p_dlcb->local_miu = p_params->miu;
            p_dlcb->local_rw  = p_params->rw;

            /* wait for response from peer device */
            p_dlcb->state     = LLCP_DLC_STATE_W4_REMOTE_RESP;

            nfc_start_quick_timer (&p_dlcb->timer, NFC_TTYPE_LLCP_DATA_LINK,
                                   (UINT32) (llcp_cb.lcb.data_link_timeout * QUICK_TIMER_TICKS_PER_SEC) / 1000);
        }
        break;

    case LLCP_DLC_EVENT_PEER_CONNECT_IND:

        /* peer device requests to create data link connection */
        p_params = (tLLCP_CONNECTION_PARAMS *) p_data;

        if (p_params->miu > llcp_cb.lcb.peer_miu)
        {
            LLCP_TRACE_WARNING0 ("llcp_dlsm_idle (): Peer sent data link MIU bigger than peer's link MIU");
            p_params->miu = llcp_cb.lcb.peer_miu;
        }

        data.connect_ind.event          = LLCP_SAP_EVT_CONNECT_IND;
        data.connect_ind.remote_sap     = p_dlcb->remote_sap;
        data.connect_ind.local_sap      = p_dlcb->local_sap;
        data.connect_ind.miu            = p_params->miu;
        data.connect_ind.rw             = p_params->rw;
        data.connect_ind.p_service_name = p_params->sn;
        data.connect_ind.server_sap     = p_dlcb->local_sap;

        p_dlcb->remote_miu = p_params->miu;
        p_dlcb->remote_rw  = p_params->rw;

        LLCP_TRACE_DEBUG2 ("llcp_dlsm_idle (): Remote MIU:%d, RW:%d", p_dlcb->remote_miu, p_dlcb->remote_rw);

        /* wait for response from upper layer */
        p_dlcb->state = LLCP_DLC_STATE_W4_LOCAL_RESP;

        nfc_start_quick_timer (&p_dlcb->timer, NFC_TTYPE_LLCP_DATA_LINK,
                               (UINT32) (llcp_cb.lcb.data_link_timeout * QUICK_TIMER_TICKS_PER_SEC) / 1000);

        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        break;

    default:
        LLCP_TRACE_ERROR0 ("llcp_dlsm_idle (): Unexpected event");
        status = LLCP_STATUS_FAIL;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         llcp_dlsm_w4_remote_resp
**
** Description      data link connection is waiting for connection confirm from peer
**
** Returns          tLLCP_STATUS
**
*******************************************************************************/
static tLLCP_STATUS llcp_dlsm_w4_remote_resp (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data)
{
    tLLCP_STATUS            status = LLCP_STATUS_SUCCESS;
    tLLCP_SAP_CBACK_DATA    data;
    tLLCP_CONNECTION_PARAMS *p_params;

    switch (event)
    {
    case LLCP_DLC_EVENT_PEER_CONNECT_CFM:

        /* peer device accepted data link connection */
        nfc_stop_quick_timer (&p_dlcb->timer);

        p_params = (tLLCP_CONNECTION_PARAMS *) p_data;

        /* data link MIU must be up to link MIU */
        if (p_params->miu > llcp_cb.lcb.peer_miu)
        {
            LLCP_TRACE_WARNING0 ("llcp_dlsm_w4_remote_resp (): Peer sent data link MIU bigger than peer's link MIU");
            p_params->miu = llcp_cb.lcb.peer_miu;
        }

        p_dlcb->remote_miu = p_params->miu;
        p_dlcb->remote_rw  = p_params->rw;

        LLCP_TRACE_DEBUG2 ("llcp_dlsm_w4_remote_resp (): Remote MIU:%d, RW:%d", p_dlcb->remote_miu, p_dlcb->remote_rw);

        p_dlcb->state = LLCP_DLC_STATE_CONNECTED;
        llcp_util_adjust_dl_rx_congestion ();

        data.connect_resp.event          = LLCP_SAP_EVT_CONNECT_RESP;
        data.connect_resp.remote_sap     = p_dlcb->remote_sap;
        data.connect_resp.local_sap      = p_dlcb->local_sap;
        data.connect_resp.miu            = p_params->miu;
        data.connect_resp.rw             = p_params->rw;

        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        if (llcp_cb.overall_rx_congested)
        {
            p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_RR_RNR;
        }
        break;

    case LLCP_DLC_EVENT_PEER_DISCONNECT_RESP:
    case LLCP_DLC_EVENT_TIMEOUT:

        /* peer device rejected connection or didn't respond */
        data.disconnect_resp.event       = LLCP_SAP_EVT_DISCONNECT_RESP;
        data.disconnect_resp.local_sap   = p_dlcb->local_sap;
        data.disconnect_resp.remote_sap  = p_dlcb->remote_sap;
        data.disconnect_resp.reason      = *((UINT8*) p_data);
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        /* stop timer, flush any pending data in queue and deallocate control block */
        llcp_util_deallocate_data_link (p_dlcb);

        llcp_util_adjust_dl_rx_congestion ();
        break;

    case LLCP_DLC_EVENT_FRAME_ERROR:
    case LLCP_DLC_EVENT_LINK_ERROR:

        /* received bad frame or link is deactivated */
        data.disconnect_ind.event       = LLCP_SAP_EVT_DISCONNECT_IND;
        data.disconnect_ind.local_sap   = p_dlcb->local_sap;
        data.disconnect_ind.remote_sap  = p_dlcb->remote_sap;
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    default:
        LLCP_TRACE_ERROR0 ("llcp_dlsm_w4_remote_resp (): Unexpected event");
        status = LLCP_STATUS_FAIL;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         llcp_dlsm_w4_local_resp
**
** Description      data link connection is waiting for connection confirm from application
**
** Returns          tLLCP_STATUS
**
*******************************************************************************/
static tLLCP_STATUS llcp_dlsm_w4_local_resp (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data)
{
    tLLCP_STATUS             status = LLCP_STATUS_SUCCESS;
    tLLCP_CONNECTION_PARAMS *p_params;
    tLLCP_SAP_CBACK_DATA     data;
    UINT8                    reason;

    switch (event)
    {
    case LLCP_DLC_EVENT_API_CONNECT_CFM:

        /* upper layer accepted data link connection */
        nfc_stop_quick_timer (&p_dlcb->timer);

        p_params = (tLLCP_CONNECTION_PARAMS *) p_data;

        p_dlcb->local_miu = p_params->miu;
        p_dlcb->local_rw  = p_params->rw;

        p_dlcb->state = LLCP_DLC_STATE_CONNECTED;

        if (llcp_cb.overall_rx_congested)
        {
            p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_RR_RNR;
        }

        status = llcp_util_send_cc (p_dlcb, p_params);

        if (status == LLCP_STATUS_SUCCESS)
        {
            llcp_util_adjust_dl_rx_congestion ();
        }
        else
        {
            data.disconnect_ind.event       = LLCP_SAP_EVT_DISCONNECT_IND;
            data.disconnect_ind.local_sap   = p_dlcb->local_sap;
            data.disconnect_ind.remote_sap  = p_dlcb->remote_sap;
            (*p_dlcb->p_app_cb->p_app_cback) (&data);

            llcp_util_deallocate_data_link (p_dlcb);
        }
        break;

    case LLCP_DLC_EVENT_API_CONNECT_REJECT:
    case LLCP_DLC_EVENT_TIMEOUT:

        if (event == LLCP_DLC_EVENT_TIMEOUT)
            reason = LLCP_SAP_DM_REASON_TEMP_REJECT_THIS;
        else
            reason = *((UINT8*) p_data);

        /* upper layer rejected connection or didn't respond */
        llcp_util_send_dm (p_dlcb->remote_sap, p_dlcb->local_sap, reason);

        /* stop timer, flush any pending data in queue and deallocate control block */
        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    case LLCP_DLC_EVENT_FRAME_ERROR:
    case LLCP_DLC_EVENT_LINK_ERROR:

        /* received bad frame or link is deactivated */
        data.disconnect_ind.event       = LLCP_SAP_EVT_DISCONNECT_IND;
        data.disconnect_ind.local_sap   = p_dlcb->local_sap;
        data.disconnect_ind.remote_sap  = p_dlcb->remote_sap;
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    default:
        LLCP_TRACE_ERROR0 ("llcp_dlsm_w4_local_resp (): Unexpected event");
        status = LLCP_STATUS_FAIL;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         llcp_dlsm_connected
**
** Description      data link connection is connected
**
** Returns          tLLCP_STATUS
**
*******************************************************************************/
static tLLCP_STATUS llcp_dlsm_connected (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data)
{
    BOOLEAN              flush;
    tLLCP_STATUS         status = LLCP_STATUS_SUCCESS;
    tLLCP_SAP_CBACK_DATA data;

    switch (event)
    {
    case LLCP_DLC_EVENT_API_DISCONNECT_REQ:

        /* upper layer requests to disconnect */
        flush = *(BOOLEAN*) (p_data);

        /*
        ** if upper layer asks to discard any pending data
        ** or there is no pending data/ack to send and it is not waiting for ack
        */
        if (  (flush)
            ||(  (p_dlcb->i_xmit_q.count == 0)
               &&(p_dlcb->next_rx_seq == p_dlcb->sent_ack_seq)
               &&(p_dlcb->next_tx_seq == p_dlcb->rcvd_ack_seq)  )  )
        {
            /* wait for disconnect response */
            p_dlcb->state = LLCP_DLC_STATE_W4_REMOTE_DM;

            llcp_util_send_disc (p_dlcb->remote_sap, p_dlcb->local_sap );

            nfc_start_quick_timer (&p_dlcb->timer, NFC_TTYPE_LLCP_DATA_LINK,
                                   (UINT32) (llcp_cb.lcb.data_link_timeout * QUICK_TIMER_TICKS_PER_SEC) / 1000);
        }
        else
        {
            /* set flag to send DISC when tx queue is empty */
            p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_DISC;
        }
        break;

    case LLCP_DLC_EVENT_PEER_DISCONNECT_IND:

        /* peer device requests to disconnect */

        /* send disconnect response and notify upper layer */
        llcp_util_send_dm (p_dlcb->remote_sap, p_dlcb->local_sap, LLCP_SAP_DM_REASON_RESP_DISC );

        data.disconnect_ind.event       = LLCP_SAP_EVT_DISCONNECT_IND;
        data.disconnect_ind.local_sap   = p_dlcb->local_sap;
        data.disconnect_ind.remote_sap  = p_dlcb->remote_sap;
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    case LLCP_DLC_EVENT_API_DATA_REQ:

        /* upper layer requests to send data */

        /* if peer device can receive data */
        if (p_dlcb->remote_rw)
        {
            /* enqueue data and check if data can be sent */
            GKI_enqueue (&p_dlcb->i_xmit_q, p_data);
            llcp_cb.total_tx_i_pdu++;

            llcp_link_check_send_data ();

            if (  (p_dlcb->is_tx_congested)
                ||(llcp_cb.overall_tx_congested)
                ||(p_dlcb->remote_busy)
                ||(p_dlcb->i_xmit_q.count >= p_dlcb->remote_rw)  ) /*if enough data to send next round */
            {
                LLCP_TRACE_DEBUG3 ("llcp_dlsm_connected (): Data link (SSAP:DSAP=0x%X:0x%X) congested: xmit_q.count=%d",
                                    p_dlcb->local_sap, p_dlcb->remote_sap, p_dlcb->i_xmit_q.count);

                /* set congested here so overall congestion check routine will not report event again */
                p_dlcb->is_tx_congested = TRUE;
                status = LLCP_STATUS_CONGESTED;
            }
        }
        else
        {
            LLCP_TRACE_ERROR0 ("llcp_dlsm_connected (): Remote RW is zero: discard data");
            /* buffer will be freed when returned to API function */
            status = LLCP_STATUS_FAIL;
        }
        break;

    case LLCP_DLC_EVENT_PEER_DATA_IND:
        /* peer device sends data so notify upper layer to read data from data link connection */

        data.data_ind.event         = LLCP_SAP_EVT_DATA_IND;
        data.data_ind.local_sap     = p_dlcb->local_sap;
        data.data_ind.link_type     = LLCP_LINK_TYPE_DATA_LINK_CONNECTION;
        data.data_ind.remote_sap    = p_dlcb->remote_sap;

        (*p_dlcb->p_app_cb->p_app_cback) (&data);
        break;

    case LLCP_DLC_EVENT_FRAME_ERROR:
    case LLCP_DLC_EVENT_LINK_ERROR:

        /* received bad frame or link is deactivated */
        data.disconnect_ind.event       = LLCP_SAP_EVT_DISCONNECT_IND;
        data.disconnect_ind.local_sap   = p_dlcb->local_sap;
        data.disconnect_ind.remote_sap  = p_dlcb->remote_sap;
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    default:
        LLCP_TRACE_ERROR0 ("llcp_dlsm_connected (): Unexpected event");
        status = LLCP_STATUS_FAIL;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         llcp_dlsm_w4_remote_dm
**
** Description      data link connection is waiting for disconnection confirm from peer
**
** Returns          tLLCP_STATUS
**
*******************************************************************************/
static tLLCP_STATUS llcp_dlsm_w4_remote_dm (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data)
{
    tLLCP_STATUS         status = LLCP_STATUS_SUCCESS;
    tLLCP_SAP_CBACK_DATA data;

    switch (event)
    {
    case LLCP_DLC_EVENT_PEER_DISCONNECT_RESP:
    case LLCP_DLC_EVENT_TIMEOUT:

        /* peer device sends disconnect response or didn't responde */
        data.disconnect_resp.event       = LLCP_SAP_EVT_DISCONNECT_RESP;
        data.disconnect_resp.local_sap   = p_dlcb->local_sap;
        data.disconnect_resp.remote_sap  = p_dlcb->remote_sap;
        data.disconnect_resp.reason      = LLCP_SAP_DM_REASON_RESP_DISC;
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    case LLCP_DLC_EVENT_FRAME_ERROR:
    case LLCP_DLC_EVENT_LINK_ERROR:

        /* received bad frame or link is deactivated */
        data.disconnect_ind.event       = LLCP_SAP_EVT_DISCONNECT_IND;
        data.disconnect_ind.local_sap   = p_dlcb->local_sap;
        data.disconnect_ind.remote_sap  = p_dlcb->remote_sap;
        (*p_dlcb->p_app_cb->p_app_cback) (&data);

        llcp_util_deallocate_data_link (p_dlcb);
        llcp_util_adjust_dl_rx_congestion ();
        break;

    case LLCP_DLC_EVENT_PEER_DATA_IND:
        break;

    case LLCP_DLC_EVENT_PEER_DISCONNECT_IND:
        /* it's race condition, send disconnect response and wait for DM */
        llcp_util_send_dm (p_dlcb->remote_sap, p_dlcb->local_sap, LLCP_SAP_DM_REASON_RESP_DISC );
        break;

    default:
        LLCP_TRACE_ERROR0 ("llcp_dlsm_w4_remote_dm (): Unexpected event");
        status = LLCP_STATUS_FAIL;
        break;
    }

    return status;
}

/*******************************************************************************
**
** Function         llcp_dlc_find_dlcb_by_local_sap
**
** Description      Find tLLCP_DLCB by local SAP and remote SAP
**                  if remote_sap is LLCP_INVALID_SAP, it will return a DLCB which
**                  is waiting for CC from peer.
**
** Returns          tLLCP_DLCB *
**
*******************************************************************************/
tLLCP_DLCB *llcp_dlc_find_dlcb_by_sap (UINT8 local_sap, UINT8 remote_sap)
{
    int i;

    for (i = 0; i < LLCP_MAX_DATA_LINK; i++)
    {
        if (  (llcp_cb.dlcb[i].state != LLCP_DLC_STATE_IDLE)
            &&(llcp_cb.dlcb[i].local_sap == local_sap)  )
        {
            if ((remote_sap == LLCP_INVALID_SAP) && (llcp_cb.dlcb[i].state == LLCP_DLC_STATE_W4_REMOTE_RESP))
            {
                /* Remote SAP has not been finalized because we are watiing for CC */
                return (&llcp_cb.dlcb[i]);
            }
            else if (llcp_cb.dlcb[i].remote_sap == remote_sap)
            {
                return (&llcp_cb.dlcb[i]);
            }
        }
    }
    return NULL;
}

/*******************************************************************************
**
** Function         llcp_dlc_flush_q
**
** Description      Free buffers in tx and rx queue in data link
**
** Returns          void
**
*******************************************************************************/
void llcp_dlc_flush_q (tLLCP_DLCB *p_dlcb)
{
    if (p_dlcb)
    {
        LLCP_TRACE_DEBUG1 ("llcp_dlc_flush_q (): local SAP:0x%02X", p_dlcb->local_sap);

        /* Release any held buffers */
        while (p_dlcb->i_xmit_q.p_first)
        {
            GKI_freebuf (GKI_dequeue (&p_dlcb->i_xmit_q));
            llcp_cb.total_tx_i_pdu--;
        }

        /* discard any received I PDU on data link  including in AGF */
        LLCP_FlushDataLinkRxData (p_dlcb->local_sap, p_dlcb->remote_sap);
    }
    else
    {
        LLCP_TRACE_ERROR0 ("llcp_dlc_flush_q (): p_dlcb is NULL");
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_connect_pdu
**
** Description      Process CONNECT PDU
**
** Returns          void
**
*******************************************************************************/
static void llcp_dlc_proc_connect_pdu (UINT8 dsap, UINT8 ssap, UINT16 length, UINT8 *p_data)
{
    tLLCP_DLCB   *p_dlcb;
    tLLCP_STATUS  status;
    tLLCP_APP_CB *p_app_cb;

    tLLCP_CONNECTION_PARAMS  params;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_proc_connect_pdu ()");

    p_app_cb = llcp_util_get_app_cb (dsap);

    if (  (p_app_cb == NULL)
        ||(p_app_cb->p_app_cback == NULL)
        ||((p_app_cb->link_type & LLCP_LINK_TYPE_DATA_LINK_CONNECTION) == 0)  )
    {
        LLCP_TRACE_ERROR1 ("llcp_dlc_proc_connect_pdu (): Unregistered SAP:0x%x", dsap);
        llcp_util_send_dm (ssap, dsap, LLCP_SAP_DM_REASON_NO_SERVICE );
        return;
    }

    /* parse CONNECT PDU and get connection parameters */
    if (llcp_util_parse_connect (p_data, length, &params) != LLCP_STATUS_SUCCESS)
    {
        LLCP_TRACE_ERROR0 ("llcp_dlc_proc_connect_pdu (): Bad format CONNECT");
        llcp_util_send_dm (ssap, dsap, LLCP_SAP_DM_REASON_NO_SERVICE );
        return;
    }

    /* if this is connection by service name */
    if (dsap == LLCP_SAP_SDP)
    {
        /* find registered SAP with service name */
        if (strlen (params.sn))
            dsap = llcp_sdp_get_sap_by_name (params.sn, (UINT8) strlen (params.sn));
        else
        {
            /* if SN type is included without SN */
            if (params.sn[1] == LLCP_SN_TYPE)
            {
                llcp_util_send_dm (ssap, LLCP_SAP_SDP, LLCP_SAP_DM_REASON_NO_SERVICE );
            }
            else
            {
                /* SDP doesn't accept connection */
                llcp_util_send_dm (ssap, LLCP_SAP_SDP, LLCP_SAP_DM_REASON_PERM_REJECT_THIS );
            }
            return;
        }

        if (dsap == LLCP_SAP_SDP)
        {
            LLCP_TRACE_ERROR0 ("llcp_dlc_proc_connect_pdu (): SDP doesn't accept connection");

            llcp_util_send_dm (ssap, LLCP_SAP_SDP, LLCP_SAP_DM_REASON_PERM_REJECT_THIS );
            return;
        }
        else if (dsap == 0)
        {
            LLCP_TRACE_ERROR1 ("llcp_dlc_proc_connect_pdu (): Unregistered Service:%s", params.sn);

            llcp_util_send_dm (ssap, LLCP_SAP_SDP, LLCP_SAP_DM_REASON_NO_SERVICE );
            return;
        }
        else
        {
            /* check if this application can support connection-oriented transport */
            p_app_cb = llcp_util_get_app_cb (dsap);

            if (  (p_app_cb == NULL)
                ||(p_app_cb->p_app_cback == NULL)
                ||((p_app_cb->link_type & LLCP_LINK_TYPE_DATA_LINK_CONNECTION) == 0)  )
            {
                LLCP_TRACE_ERROR1 ("llcp_dlc_proc_connect_pdu (): SAP(0x%x) doesn't support connection-oriented", dsap);
                llcp_util_send_dm (ssap, dsap, LLCP_SAP_DM_REASON_NO_SERVICE );
                return;
            }
        }
    }

    /* check if any data link */
    p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);
    if (p_dlcb)
    {
        LLCP_TRACE_ERROR0 ("llcp_dlc_proc_connect_pdu (): Data link is aleady established");
        llcp_util_send_dm (ssap, dsap, LLCP_SAP_DM_REASON_TEMP_REJECT_THIS );
    }
    else
    {
        /* allocate data link connection control block and notify upper layer through state machine */
        p_dlcb = llcp_util_allocate_data_link (dsap, ssap);

        if (p_dlcb)
        {
            status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_PEER_CONNECT_IND, &params);
            if (status != LLCP_STATUS_SUCCESS)
            {
                LLCP_TRACE_ERROR0 ("llcp_dlc_proc_connect_pdu (): Error in state machine");
                llcp_util_deallocate_data_link (p_dlcb);
            }
        }
        else
        {
            LLCP_TRACE_ERROR0 ("llcp_dlc_proc_connect_pdu (): Out of resource");
            llcp_util_send_dm (ssap, dsap, LLCP_SAP_DM_REASON_TEMP_REJECT_ANY );
        }
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_disc_pdu
**
** Description      Process DISC PDU
**
** Returns          void
**
*******************************************************************************/
static void llcp_dlc_proc_disc_pdu (UINT8 dsap, UINT8 ssap, UINT16 length, UINT8 *p_data)
{
    tLLCP_DLCB *p_dlcb;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_proc_disc_pdu ()");

    p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);
    if (p_dlcb)
    {
        if (length > 0)
        {
            LLCP_TRACE_ERROR1 ("llcp_dlc_proc_disc_pdu (): Received extra data (%d bytes) in DISC PDU", length);

            llcp_util_send_frmr (p_dlcb, LLCP_FRMR_W_ERROR_FLAG|LLCP_FRMR_I_ERROR_FLAG, LLCP_PDU_DISC_TYPE, 0);
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_FRAME_ERROR, NULL);
        }
        else
        {
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_PEER_DISCONNECT_IND, NULL);
        }
    }
    else
    {
        LLCP_TRACE_ERROR2 ("llcp_dlc_proc_disc_pdu (): No data link for SAP (0x%x,0x%x)", dsap, ssap);
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_cc_pdu
**
** Description      Process CC PDU
**
** Returns          void
**
*******************************************************************************/
static void llcp_dlc_proc_cc_pdu (UINT8 dsap, UINT8 ssap, UINT16 length, UINT8 *p_data)
{
    tLLCP_DLCB              *p_dlcb;
    tLLCP_CONNECTION_PARAMS  params;
    tLLCP_STATUS             status;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_proc_cc_pdu ()");

    /* find a DLCB waiting for CC on this local SAP */
    p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, LLCP_INVALID_SAP);
    if (p_dlcb)
    {
        /* The CC may contain a SSAP that is different from the DSAP in the CONNECT */
        p_dlcb->remote_sap = ssap;

        if (llcp_util_parse_cc (p_data, length, &(params.miu), &(params.rw)) == LLCP_STATUS_SUCCESS)
        {
            status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_PEER_CONNECT_CFM, &params);
            if (status != LLCP_STATUS_SUCCESS)
            {
                LLCP_TRACE_ERROR0 ("llcp_dlc_proc_cc_pdu (): Error in state machine");
                llcp_util_deallocate_data_link (p_dlcb);
            }
        }
        else
        {
            llcp_util_send_frmr (p_dlcb, LLCP_FRMR_W_ERROR_FLAG|LLCP_FRMR_I_ERROR_FLAG, LLCP_PDU_DISC_TYPE, 0);
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_FRAME_ERROR, NULL);
        }
    }
    else
    {
        LLCP_TRACE_ERROR2 ("llcp_dlc_proc_cc_pdu (): No data link for SAP (0x%x,0x%x)", dsap, ssap);
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_dm_pdu
**
** Description      Process DM PDU
**
** Returns          void
**
*******************************************************************************/
static void llcp_dlc_proc_dm_pdu (UINT8 dsap, UINT8 ssap, UINT16 length, UINT8 *p_data)
{
    tLLCP_DLCB *p_dlcb;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_proc_dm_pdu ()");

    if (length != LLCP_PDU_DM_SIZE - LLCP_PDU_HEADER_SIZE)
    {
        LLCP_TRACE_ERROR0 ("llcp_dlc_proc_dm_pdu (): Received invalid DM PDU");
    }
    else
    {
        if (*p_data == LLCP_SAP_DM_REASON_RESP_DISC)
        {
            /* local device initiated disconnecting */
            p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);
        }
        else
        {
            /* peer device rejected connection with any reason */
            /* find a DLCB waiting for CC on this local SAP    */
            p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, LLCP_INVALID_SAP);
        }

        if (p_dlcb)
        {
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_PEER_DISCONNECT_RESP, p_data); /* passing reason */
        }
        else
        {
            LLCP_TRACE_ERROR2 ("llcp_dlc_proc_dm_pdu (): No data link for SAP (0x%x,0x%x)", dsap, ssap);
        }
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_i_pdu
**
** Description      Process I PDU
**
** Returns          void
**
*******************************************************************************/
void llcp_dlc_proc_i_pdu (UINT8 dsap, UINT8 ssap, UINT16 i_pdu_length, UINT8 *p_i_pdu, BT_HDR *p_msg)
{
    UINT8      *p, *p_dst, send_seq, rcv_seq, error_flags;
    UINT16      info_len, available_bytes;
    tLLCP_DLCB *p_dlcb;
    BOOLEAN     appended;
    BT_HDR     *p_last_buf;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_proc_i_pdu ()");

    p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);

    if ((p_dlcb)&&(p_dlcb->state == LLCP_DLC_STATE_CONNECTED))
    {
        error_flags = 0;

        if (p_msg)
        {
            i_pdu_length = p_msg->len;
            p_i_pdu = (UINT8 *) (p_msg + 1) + p_msg->offset;
        }

        info_len = i_pdu_length - LLCP_PDU_HEADER_SIZE - LLCP_SEQUENCE_SIZE;

        if (info_len > p_dlcb->local_miu)
        {
            LLCP_TRACE_ERROR2 ("llcp_dlc_proc_i_pdu (): exceeding local MIU (%d bytes): got %d bytes SDU",
                                p_dlcb->local_miu, info_len);

            error_flags |= LLCP_FRMR_I_ERROR_FLAG;
        }

        /* get sequence numbers */
        p = p_i_pdu + LLCP_PDU_HEADER_SIZE;

        send_seq = LLCP_GET_NS (*p);
        rcv_seq  = LLCP_GET_NR (*p);

#if (BT_TRACE_VERBOSE == TRUE)
        LLCP_TRACE_DEBUG6 ("LLCP RX I PDU - N(S,R):(%d,%d) V(S,SA,R,RA):(%d,%d,%d,%d)",
                            send_seq, rcv_seq,
                            p_dlcb->next_tx_seq, p_dlcb->rcvd_ack_seq,
                            p_dlcb->next_rx_seq, p_dlcb->sent_ack_seq);
#endif

        /* if send sequence number, N(S) is not expected one, V(R) */
        if (p_dlcb->next_rx_seq != send_seq)
        {
            LLCP_TRACE_ERROR2 ("llcp_dlc_proc_i_pdu (): Bad N(S) got:%d, expected:%d",
                                send_seq, p_dlcb->next_rx_seq);

            error_flags |= LLCP_FRMR_S_ERROR_FLAG;
        }
        else
        {
            /* if peer device sends more than our receiving window size */
            if ((UINT8) (send_seq - p_dlcb->sent_ack_seq) % LLCP_SEQ_MODULO >= p_dlcb->local_rw)
            {
                LLCP_TRACE_ERROR3 ("llcp_dlc_proc_i_pdu (): Bad N(S):%d >= V(RA):%d + RW(L):%d",
                                    send_seq, p_dlcb->sent_ack_seq, p_dlcb->local_rw);

                error_flags |= LLCP_FRMR_S_ERROR_FLAG;
            }
        }

        /* check N(R) is in valid range; V(SA) <= N(R) <= V(S) */
        if ((UINT8) (rcv_seq - p_dlcb->rcvd_ack_seq) % LLCP_SEQ_MODULO + (UINT8) (p_dlcb->next_tx_seq - rcv_seq) % LLCP_SEQ_MODULO
            != (UINT8) (p_dlcb->next_tx_seq - p_dlcb->rcvd_ack_seq) % LLCP_SEQ_MODULO)
        {
            error_flags |= LLCP_FRMR_R_ERROR_FLAG;
            LLCP_TRACE_ERROR3 ("llcp_dlc_proc_i_pdu (): Bad N(R):%d valid range [V(SA):%d, V(S):%d]",
                                rcv_seq, p_dlcb->rcvd_ack_seq, p_dlcb->next_tx_seq);
        }

        /* if any error is found */
        if (error_flags)
        {
            llcp_util_send_frmr (p_dlcb, error_flags, LLCP_PDU_I_TYPE, *p);
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_FRAME_ERROR, NULL);
        }
        else
        {
            /* update local sequence variables */
            p_dlcb->next_rx_seq  = (p_dlcb->next_rx_seq + 1) % LLCP_SEQ_MODULO;
            p_dlcb->rcvd_ack_seq = rcv_seq;

            appended = FALSE;

            /* get last buffer in rx queue */
            p_last_buf = (BT_HDR *) GKI_getlast (&p_dlcb->i_rx_q);

            if (p_last_buf)
            {
                /* get max length to append at the end of buffer */
                available_bytes = GKI_get_buf_size (p_last_buf) - BT_HDR_SIZE - p_last_buf->offset - p_last_buf->len;

                /* if new UI PDU with length can be attached at the end of buffer */
                if (available_bytes >= LLCP_PDU_AGF_LEN_SIZE + info_len)
                {
                    p_dst = (UINT8*) (p_last_buf + 1) + p_last_buf->offset + p_last_buf->len;

                    /* add length of information in I PDU */
                    UINT16_TO_BE_STREAM (p_dst, info_len);

                    /* copy information of I PDU */
                    p = p_i_pdu + LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE;

                    memcpy (p_dst, p, info_len);

                    p_last_buf->len += LLCP_PDU_AGF_LEN_SIZE + info_len;

                    if (p_msg)
                    {
                        GKI_freebuf (p_msg);
                        p_msg = NULL;
                    }

                    appended = TRUE;
                }
            }

            /* if it is not available to append */
            if (!appended)
            {
                /* if it's not from AGF PDU */
                if (p_msg)
                {
                    /* add length of information in front of information */
                    p = p_i_pdu + LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE - LLCP_PDU_AGF_LEN_SIZE;
                    UINT16_TO_BE_STREAM (p, info_len);

                    p_msg->offset += LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE - LLCP_PDU_AGF_LEN_SIZE;
                    p_msg->len    -= LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE - LLCP_PDU_AGF_LEN_SIZE;
                    p_msg->layer_specific = 0;
                }
                else
                {
                    p_msg = (BT_HDR *) GKI_getpoolbuf (LLCP_POOL_ID);

                    if (p_msg)
                    {
                        p_dst = (UINT8*) (p_msg + 1);

                        /* add length of information in front of information */
                        UINT16_TO_BE_STREAM (p_dst, info_len);

                        p = p_i_pdu + LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE;
                        memcpy (p_dst, p, info_len);

                        p_msg->offset = 0;
                        p_msg->len    = LLCP_PDU_AGF_LEN_SIZE + info_len;
                        p_msg->layer_specific = 0;
                    }
                    else
                    {
                        LLCP_TRACE_ERROR0 ("llcp_dlc_proc_i_pdu (): out of buffer");
                    }
                }

                /* insert I PDU in rx queue */
                if (p_msg)
                {
                    GKI_enqueue (&p_dlcb->i_rx_q, p_msg);
                    p_msg = NULL;
                    llcp_cb.total_rx_i_pdu++;

                    llcp_util_check_rx_congested_status ();
                }
            }

            p_dlcb->num_rx_i_pdu++;

            if (  (!p_dlcb->local_busy)
                &&(p_dlcb->num_rx_i_pdu == 1)  )
            {
                /* notify rx data is available so upper layer reads data until queue is empty */
                llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_PEER_DATA_IND, NULL);
            }

            if (  (!p_dlcb->is_rx_congested)
                &&(p_dlcb->num_rx_i_pdu >= p_dlcb->rx_congest_threshold)  )
            {
                LLCP_TRACE_DEBUG2 ("llcp_dlc_proc_i_pdu (): congested num_rx_i_pdu=%d, rx_congest_threshold=%d",
                                    p_dlcb->num_rx_i_pdu, p_dlcb->rx_congest_threshold);

                /* send RNR */
                p_dlcb->is_rx_congested = TRUE;
                p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_RR_RNR;
            }
        }
    }
    else
    {
        LLCP_TRACE_ERROR2 ("llcp_dlc_proc_i_pdu (): No data link for SAP (0x%x,0x%x)", dsap, ssap);
        llcp_util_send_dm (ssap, dsap, LLCP_SAP_DM_REASON_NO_ACTIVE_CONNECTION );
    }

    if (p_msg)
    {
        GKI_freebuf (p_msg);
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_rr_rnr_pdu
**
** Description      Process RR or RNR PDU
**
** Returns          void
**
*******************************************************************************/
static void llcp_dlc_proc_rr_rnr_pdu (UINT8 dsap, UINT8 ptype, UINT8 ssap, UINT16 length, UINT8 *p_data)
{
    UINT8      rcv_seq, error_flags;
    tLLCP_DLCB *p_dlcb;
    BOOLEAN     flush = TRUE;
    tLLCP_SAP_CBACK_DATA cback_data;
    BOOLEAN              old_remote_busy;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_proc_rr_rnr_pdu ()");

    p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);
    if (p_dlcb != NULL)
    {
        error_flags = 0;

        rcv_seq = LLCP_GET_NR (*p_data);

        if (length != LLCP_PDU_RR_SIZE - LLCP_PDU_HEADER_SIZE)
        {
            error_flags |= LLCP_FRMR_W_ERROR_FLAG|LLCP_FRMR_I_ERROR_FLAG;
        }

        /* check N(R) is in valid range; V(SA) <= N(R) <= V(S) */
        if ((UINT8) (rcv_seq - p_dlcb->rcvd_ack_seq) % LLCP_SEQ_MODULO + (UINT8) (p_dlcb->next_tx_seq - rcv_seq) % LLCP_SEQ_MODULO
            != (UINT8) (p_dlcb->next_tx_seq - p_dlcb->rcvd_ack_seq) % LLCP_SEQ_MODULO )
        {
            error_flags |= LLCP_FRMR_R_ERROR_FLAG;
            LLCP_TRACE_ERROR3 ("llcp_dlc_proc_rr_rnr_pdu (): Bad N(R):%d valid range [V(SA):%d, V(S):%d]",
                                rcv_seq, p_dlcb->rcvd_ack_seq, p_dlcb->next_tx_seq);
        }

        if (error_flags)
        {
            llcp_util_send_frmr (p_dlcb, error_flags, ptype, *p_data);
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_FRAME_ERROR, NULL);
        }
        else
        {
            p_dlcb->rcvd_ack_seq = rcv_seq;

#if (BT_TRACE_VERBOSE == TRUE)
            LLCP_TRACE_DEBUG5 ("LLCP RX - N(S,R):(NA,%d) V(S,SA,R,RA):(%d,%d,%d,%d)",
                                rcv_seq,
                                p_dlcb->next_tx_seq, p_dlcb->rcvd_ack_seq,
                                p_dlcb->next_rx_seq, p_dlcb->sent_ack_seq);
#endif
            old_remote_busy = p_dlcb->remote_busy;
            if (ptype == LLCP_PDU_RNR_TYPE)
            {
                p_dlcb->remote_busy = TRUE;
                /* if upper layer hasn't get congestion started notification */
                if (  (!old_remote_busy)
                    &&(!p_dlcb->is_tx_congested)  )
                {
                    LLCP_TRACE_WARNING3 ("llcp_dlc_proc_rr_rnr_pdu (): Data link (SSAP:DSAP=0x%X:0x%X) congestion start: i_xmit_q.count=%d",
                                          p_dlcb->local_sap, p_dlcb->remote_sap,
                                          p_dlcb->i_xmit_q.count);

                    cback_data.congest.event        = LLCP_SAP_EVT_CONGEST;
                    cback_data.congest.local_sap    = p_dlcb->local_sap;
                    cback_data.congest.remote_sap   = p_dlcb->remote_sap;
                    cback_data.congest.is_congested = TRUE;
                    cback_data.congest.link_type    = LLCP_LINK_TYPE_DATA_LINK_CONNECTION;

                    (*p_dlcb->p_app_cb->p_app_cback) (&cback_data);
                }
            }
            else
            {
                p_dlcb->remote_busy = FALSE;
                /* if upper layer hasn't get congestion ended notification and data link is not congested */
                if (  (old_remote_busy)
                    &&(!p_dlcb->is_tx_congested)  )
                {
                    LLCP_TRACE_WARNING3 ("llcp_dlc_proc_rr_rnr_pdu (): Data link (SSAP:DSAP=0x%X:0x%X) congestion end: i_xmit_q.count=%d",
                                          p_dlcb->local_sap, p_dlcb->remote_sap,
                                          p_dlcb->i_xmit_q.count);

                    cback_data.congest.event        = LLCP_SAP_EVT_CONGEST;
                    cback_data.congest.local_sap    = p_dlcb->local_sap;
                    cback_data.congest.remote_sap   = p_dlcb->remote_sap;
                    cback_data.congest.is_congested = FALSE;
                    cback_data.congest.link_type    = LLCP_LINK_TYPE_DATA_LINK_CONNECTION;

                    (*p_dlcb->p_app_cb->p_app_cback) (&cback_data);
                }
            }

            /* check flag to send DISC when tx queue is empty */
            if (p_dlcb->flags & LLCP_DATA_LINK_FLAG_PENDING_DISC)
            {
                /* if no pending data and all PDU is acked */
                if (  (p_dlcb->i_xmit_q.count == 0)
                    &&(p_dlcb->next_rx_seq == p_dlcb->sent_ack_seq)
                    &&(p_dlcb->next_tx_seq == p_dlcb->rcvd_ack_seq)  )
                {
                    p_dlcb->flags &= ~LLCP_DATA_LINK_FLAG_PENDING_DISC;
                    llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_DISCONNECT_REQ, &flush);
                }
            }
        }
    }
    else
    {
        LLCP_TRACE_ERROR2 ("llcp_dlc_proc_rr_rnr_pdu (): No data link for SAP (0x%x,0x%x)", dsap, ssap);
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_proc_rx_pdu
**
** Description      Process PDU for data link
**
** Returns          void
**
*******************************************************************************/
void llcp_dlc_proc_rx_pdu (UINT8 dsap, UINT8 ptype, UINT8 ssap, UINT16 length, UINT8 *p_data)
{
    tLLCP_DLCB *p_dlcb;

    LLCP_TRACE_DEBUG3 ("llcp_dlc_proc_rx_pdu (): DSAP:0x%x, PTYPE:0x%x, SSAP:0x%x",
                        dsap, ptype, ssap);

    if (dsap == LLCP_SAP_LM)
    {
        LLCP_TRACE_ERROR2 ("llcp_dlc_proc_rx_pdu (): Invalid SAP:0x%x for PTYPE:0x%x", dsap, ptype);
        return;
    }

    switch (ptype)
    {
    case LLCP_PDU_CONNECT_TYPE:
        llcp_dlc_proc_connect_pdu (dsap, ssap, length, p_data);
        break;

    case LLCP_PDU_DISC_TYPE:
        llcp_dlc_proc_disc_pdu (dsap, ssap, length, p_data);
        break;

    case LLCP_PDU_CC_TYPE:
        llcp_dlc_proc_cc_pdu (dsap, ssap, length, p_data);
        break;

    case LLCP_PDU_DM_TYPE:
        llcp_dlc_proc_dm_pdu (dsap, ssap, length, p_data);
        break;

    case LLCP_PDU_FRMR_TYPE:
        p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);
        if (p_dlcb)
        {
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_FRAME_ERROR, NULL);
        }
        break;

    case LLCP_PDU_RR_TYPE:
    case LLCP_PDU_RNR_TYPE:
        llcp_dlc_proc_rr_rnr_pdu (dsap, ptype, ssap, length, p_data);
        break;

    default:
        LLCP_TRACE_ERROR1 ("llcp_dlc_proc_rx_pdu (): Unexpected PDU type (0x%x)", ptype);

        p_dlcb = llcp_dlc_find_dlcb_by_sap (dsap, ssap);
        if (p_dlcb)
        {
            llcp_util_send_frmr (p_dlcb, LLCP_FRMR_W_ERROR_FLAG, ptype, 0);
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_FRAME_ERROR, NULL);
        }
        break;
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_check_to_send_rr_rnr
**
** Description      Send RR or RNR if necessary
**
** Returns          void
**
*******************************************************************************/
void llcp_dlc_check_to_send_rr_rnr (void)
{
    UINT8   idx;
    BOOLEAN flush = TRUE;

    LLCP_TRACE_DEBUG0 ("llcp_dlc_check_to_send_rr_rnr ()");

    /*
    ** DLC doesn't send RR PDU for each received I PDU because multiple I PDUs can be aggregated
    ** in a received AGF PDU. In this case, this is post processing of AGF PDU to send single RR
    ** or RNR after processing all I PDUs in received AGF if there was no I-PDU to carry N(R).
    **
    ** Send RR or RNR if any change of local busy condition or rx congestion status, or V(RA) is not
    ** V(R).
    */
    for (idx = 0; idx < LLCP_MAX_DATA_LINK; idx++)
    {
        if (llcp_cb.dlcb[idx].state == LLCP_DLC_STATE_CONNECTED)
        {
            llcp_util_send_rr_rnr (&(llcp_cb.dlcb[idx]));

            /* check flag to send DISC when tx queue is empty */
            if (llcp_cb.dlcb[idx].flags & LLCP_DATA_LINK_FLAG_PENDING_DISC)
            {
                /* if no pending data and all PDU is acked */
                if (  (llcp_cb.dlcb[idx].i_xmit_q.count == 0)
                    &&(llcp_cb.dlcb[idx].next_rx_seq == llcp_cb.dlcb[idx].sent_ack_seq)
                    &&(llcp_cb.dlcb[idx].next_tx_seq == llcp_cb.dlcb[idx].rcvd_ack_seq)  )
                {
                    llcp_cb.dlcb[idx].flags &= ~LLCP_DATA_LINK_FLAG_PENDING_DISC;
                    llcp_dlsm_execute (&(llcp_cb.dlcb[idx]), LLCP_DLC_EVENT_API_DISCONNECT_REQ, &flush);
                }
            }
        }
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_is_rw_open
**
** Description      check if receive window is open in remote
**
** Returns          TRUE if remote can receive more data
**
*******************************************************************************/
BOOLEAN llcp_dlc_is_rw_open (tLLCP_DLCB *p_dlcb)
{
    if ((UINT8) (p_dlcb->next_tx_seq - p_dlcb->rcvd_ack_seq) % LLCP_SEQ_MODULO < p_dlcb->remote_rw)
    {
        return TRUE;
    }
    else
    {
        LLCP_TRACE_DEBUG3 ("llcp_dlc_is_rw_open ():Flow Off, V(S):%d, V(SA):%d, RW(R):%d",
                           p_dlcb->next_tx_seq, p_dlcb->rcvd_ack_seq, p_dlcb->remote_rw);
        return FALSE;
    }
}

/*******************************************************************************
**
** Function         llcp_dlc_get_next_pdu
**
** Description      Get a PDU from tx queue of data link
**
** Returns          BT_HDR*
**
*******************************************************************************/
BT_HDR* llcp_dlc_get_next_pdu (tLLCP_DLCB *p_dlcb)
{
    BT_HDR *p_msg = NULL;
    BOOLEAN flush = TRUE;
    tLLCP_SAP_CBACK_DATA data;

#if (BT_TRACE_VERBOSE == TRUE)
    UINT8   send_seq = p_dlcb->next_tx_seq;
#endif

    /* if there is data to send and remote device can receive it */
    if (  (p_dlcb->i_xmit_q.count)
        &&(!p_dlcb->remote_busy)
        &&(llcp_dlc_is_rw_open (p_dlcb))  )
    {
        p_msg = (BT_HDR *) GKI_dequeue (&p_dlcb->i_xmit_q);
        llcp_cb.total_tx_i_pdu--;

        if (p_msg->offset >= LLCP_MIN_OFFSET)
        {
            /* add LLCP header, DSAP, PTYPE, SSAP, N(S), N(R) and update sent_ack_seq, V(RA) */
            llcp_util_build_info_pdu (p_dlcb, p_msg);

            p_dlcb->next_tx_seq  = (p_dlcb->next_tx_seq + 1) % LLCP_SEQ_MODULO;

#if (BT_TRACE_VERBOSE == TRUE)
            LLCP_TRACE_DEBUG6 ("LLCP TX - N(S,R):(%d,%d) V(S,SA,R,RA):(%d,%d,%d,%d)",
                                send_seq, p_dlcb->next_rx_seq,
                                p_dlcb->next_tx_seq, p_dlcb->rcvd_ack_seq,
                                p_dlcb->next_rx_seq, p_dlcb->sent_ack_seq);
#endif
        }
        else
        {
            LLCP_TRACE_ERROR2 ("LLCP - llcp_dlc_get_next_pdu (): offset (%d) must be %d at least",
                                p_msg->offset, LLCP_MIN_OFFSET );
            GKI_freebuf (p_msg);
            p_msg = NULL;
        }
    }

    /* if tx queue is empty and all PDU is acknowledged */
    if (  (p_dlcb->i_xmit_q.count == 0)
        &&(p_dlcb->next_rx_seq == p_dlcb->sent_ack_seq)
        &&(p_dlcb->next_tx_seq == p_dlcb->rcvd_ack_seq)  )
    {
        /* check flag to send DISC */
        if (p_dlcb->flags & LLCP_DATA_LINK_FLAG_PENDING_DISC)
        {
            p_dlcb->flags &= ~LLCP_DATA_LINK_FLAG_PENDING_DISC;
            llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_DISCONNECT_REQ, &flush);
        }

        /* check flag to notify upper layer */
        if (p_dlcb->flags & LLCP_DATA_LINK_FLAG_NOTIFY_TX_DONE)
        {
            p_dlcb->flags &= ~LLCP_DATA_LINK_FLAG_NOTIFY_TX_DONE;

            data.tx_complete.event      = LLCP_SAP_EVT_TX_COMPLETE;
            data.tx_complete.local_sap  = p_dlcb->local_sap;
            data.tx_complete.remote_sap = p_dlcb->remote_sap;

            (*p_dlcb->p_app_cb->p_app_cback) (&data);
        }
    }

    return p_msg;
}

/*******************************************************************************
**
** Function         llcp_dlc_get_next_pdu_length
**
** Description      return length of PDU which is top in tx queue of data link
**
** Returns          length of PDU
**
*******************************************************************************/
UINT16 llcp_dlc_get_next_pdu_length (tLLCP_DLCB *p_dlcb)
{
    BT_HDR *p_msg;

    /* if there is data to send and remote device can receive it */
    if (  (p_dlcb->i_xmit_q.count)
        &&(!p_dlcb->remote_busy)
        &&(llcp_dlc_is_rw_open (p_dlcb))  )
    {
        p_msg = (BT_HDR *) p_dlcb->i_xmit_q.p_first;

        return (p_msg->len + LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE);
    }
    return 0;
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         llcp_dlsm_get_state_name
**
** Description      This function returns the state name.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *llcp_dlsm_get_state_name (tLLCP_DLC_STATE state)
{
    switch (state)
    {
    case LLCP_DLC_STATE_IDLE:
        return ("IDLE");
    case LLCP_DLC_STATE_W4_REMOTE_RESP:
        return ("W4_REMOTE_RESP");
    case LLCP_DLC_STATE_W4_LOCAL_RESP:
        return ("W4_LOCAL_RESP");
    case LLCP_DLC_STATE_CONNECTED:
        return ("CONNECTED");
    case LLCP_DLC_STATE_W4_REMOTE_DM:
        return ("W4_REMOTE_DM");
    default:
        return ("???? UNKNOWN STATE");
    }
}

/*******************************************************************************
**
** Function         llcp_dlsm_get_event_name
**
** Description      This function returns the event name.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *llcp_dlsm_get_event_name (tLLCP_DLC_EVENT event)
{
    switch (event)
    {
    case LLCP_DLC_EVENT_API_CONNECT_REQ:
        return ("API_CONNECT_REQ");
    case LLCP_DLC_EVENT_API_CONNECT_CFM:
        return ("API_CONNECT_CFM");
    case LLCP_DLC_EVENT_API_CONNECT_REJECT:
        return ("API_CONNECT_REJECT");
    case LLCP_DLC_EVENT_PEER_CONNECT_IND:
        return ("PEER_CONNECT_IND");
    case LLCP_DLC_EVENT_PEER_CONNECT_CFM:
        return ("PEER_CONNECT_CFM");

    case LLCP_DLC_EVENT_API_DATA_REQ:
        return ("API_DATA_REQ");
    case LLCP_DLC_EVENT_PEER_DATA_IND:
        return ("PEER_DATA_IND");

    case LLCP_DLC_EVENT_API_DISCONNECT_REQ:
        return ("API_DISCONNECT_REQ");
    case LLCP_DLC_EVENT_PEER_DISCONNECT_IND:
        return ("PEER_DISCONNECT_IND");
    case LLCP_DLC_EVENT_PEER_DISCONNECT_RESP:
        return ("PEER_DISCONNECT_RESP");

    case LLCP_DLC_EVENT_FRAME_ERROR:
        return ("FRAME_ERROR");
    case LLCP_DLC_EVENT_LINK_ERROR:
        return ("LINK_ERROR");

    case LLCP_DLC_EVENT_TIMEOUT:
        return ("TIMEOUT");

    default:
        return ("???? UNKNOWN EVENT");
    }
}
#endif /* (BT_TRACE_VERBOSE == TRUE) */


