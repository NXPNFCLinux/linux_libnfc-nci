/******************************************************************************
 *
 *  Copyright (C) 2010-2012 Broadcom Corporation
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
 *  This is the state implementation file for the NFA Connection Handover.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_api.h"
#include "llcp_api.h"
#include "llcp_defs.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfa_cho_api.h"
#include "nfa_cho_int.h"
#include "nfa_mem_co.h"

/*****************************************************************************
**  Global Variables
*****************************************************************************/

/*****************************************************************************
**  Static Functions
*****************************************************************************/
static void nfa_cho_sm_disabled (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data);
static void nfa_cho_sm_idle (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data);
static void nfa_cho_sm_w4_cc (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data);
static void nfa_cho_sm_connected (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data);
static void nfa_cho_proc_rx_handover_msg (void);

/* debug functions type */
#if (BT_TRACE_VERBOSE == TRUE)
static char *nfa_cho_state_code (tNFA_CHO_STATE state_code);
static char *nfa_cho_evt_code (tNFA_CHO_INT_EVT evt_code);
#endif

/*****************************************************************************
**  Constants
*****************************************************************************/

/*******************************************************************************
**
** Function         nfa_cho_sm_llcp_cback
**
** Description      Processing event from LLCP
**
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_sm_llcp_cback (tLLCP_SAP_CBACK_DATA *p_data)
{
    tNFA_CHO_RX_NDEF_STATUS rx_status;

    CHO_TRACE_DEBUG2 ("nfa_cho_sm_llcp_cback (): event:0x%02X, local_sap:0x%02X",
                       p_data->hdr.event, p_data->hdr.local_sap);

    switch (p_data->hdr.event)
    {
    case LLCP_SAP_EVT_DATA_IND:
        /* check if we received complete Handover Message */
        rx_status = nfa_cho_reassemble_ho_msg (p_data->data_ind.local_sap,
                                               p_data->data_ind.remote_sap);

        if (rx_status == NFA_CHO_RX_NDEF_COMPLETE)
        {
            nfa_cho_sm_execute (NFA_CHO_RX_HANDOVER_MSG_EVT, NULL);
        }
        break;

    case LLCP_SAP_EVT_CONNECT_IND:
        nfa_cho_sm_execute (NFA_CHO_LLCP_CONNECT_IND_EVT, (tNFA_CHO_INT_EVENT_DATA *) p_data);
        break;

    case LLCP_SAP_EVT_CONNECT_RESP:
        nfa_cho_sm_execute (NFA_CHO_LLCP_CONNECT_RESP_EVT, (tNFA_CHO_INT_EVENT_DATA *) p_data);
        break;

    case LLCP_SAP_EVT_DISCONNECT_IND:
        nfa_cho_sm_execute (NFA_CHO_LLCP_DISCONNECT_IND_EVT, (tNFA_CHO_INT_EVENT_DATA *) p_data);
        break;

    case LLCP_SAP_EVT_DISCONNECT_RESP:
        nfa_cho_sm_execute (NFA_CHO_LLCP_DISCONNECT_RESP_EVT, (tNFA_CHO_INT_EVENT_DATA *) p_data);
        break;

    case LLCP_SAP_EVT_CONGEST:
        nfa_cho_sm_execute (NFA_CHO_LLCP_CONGEST_EVT, (tNFA_CHO_INT_EVENT_DATA *) p_data);
        break;

    case LLCP_SAP_EVT_LINK_STATUS:
        nfa_cho_sm_execute (NFA_CHO_LLCP_LINK_STATUS_EVT, (tNFA_CHO_INT_EVENT_DATA *) p_data);
        break;

    default:
        CHO_TRACE_ERROR1 ("Unknown event:0x%02X", p_data->hdr.event);
        return;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_sm_disabled
**
** Description      Process event in disabled state
**
** Returns          None
**
*******************************************************************************/
static void nfa_cho_sm_disabled (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data)
{
    tNFA_CHO_EVT_DATA evt_data;
    UINT16            remote_link_miu;

    switch (event)
    {
    case NFA_CHO_API_REG_EVT:

        evt_data.status = nfa_cho_proc_api_reg (p_data);

        if (evt_data.status == NFA_STATUS_OK)
        {
            nfa_cho_cb.state = NFA_CHO_ST_IDLE;
        }
        p_data->api_reg.p_cback (NFA_CHO_REG_EVT, &evt_data);

        if (evt_data.status == NFA_STATUS_OK)
        {
            /* check if LLCP is already activated */
            LLCP_GetLinkMIU (&nfa_cho_cb.local_link_miu, &remote_link_miu);

            if (nfa_cho_cb.local_link_miu > 0)
            {
                nfa_cho_cb.flags |= NFA_CHO_FLAGS_LLCP_ACTIVATED;

                /* Notify application LLCP link activated */
                evt_data.activated.is_initiator = FALSE;
                nfa_cho_cb.p_cback (NFA_CHO_ACTIVATED_EVT, &evt_data);
            }
        }
        break;

    default:
        CHO_TRACE_ERROR0 ("Unknown event");
        break;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_sm_idle
**
** Description      Process event in idle state
**
** Returns          None
**
*******************************************************************************/
static void nfa_cho_sm_idle (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data)
{
    UINT16                  remote_link_miu;
    tNFA_CHO_EVT_DATA       evt_data;
    tLLCP_CONNECTION_PARAMS params;

    switch (event)
    {
    case NFA_CHO_API_REG_EVT:
        evt_data.status = NFA_STATUS_FAILED;
        p_data->api_reg.p_cback (NFA_CHO_REG_EVT, &evt_data);
        break;

    case NFA_CHO_API_DEREG_EVT:
        nfa_cho_proc_api_dereg ();
        nfa_cho_cb.state = NFA_CHO_ST_DISABLED;
        break;

    case NFA_CHO_API_CONNECT_EVT:
        /* if LLCP activated then create data link connection */
        if (nfa_cho_cb.flags & NFA_CHO_FLAGS_LLCP_ACTIVATED)
        {
            if (nfa_cho_create_connection () == NFA_STATUS_OK)
            {
                /* waiting for connection confirm */
                nfa_cho_cb.state = NFA_CHO_ST_W4_CC;
            }
            else
            {
                evt_data.disconnected.reason = NFA_CHO_DISC_REASON_CONNECTION_FAIL;
                nfa_cho_cb.p_cback (NFA_CHO_DISCONNECTED_EVT, &evt_data);
            }
        }
        else
        {
            evt_data.disconnected.reason = NFA_CHO_DISC_REASON_LINK_DEACTIVATED;
            nfa_cho_cb.p_cback (NFA_CHO_DISCONNECTED_EVT, &evt_data);
        }
        break;

    case NFA_CHO_API_DISCONNECT_EVT:
        /* Nothing to disconnect */
        nfa_cho_process_disconnection (NFA_CHO_DISC_REASON_API_REQUEST);
        break;

    case NFA_CHO_LLCP_CONNECT_IND_EVT:

        /* accept connection request */
        params.miu = (UINT16) (nfa_cho_cb.local_link_miu >= NFA_CHO_MIU ? NFA_CHO_MIU : nfa_cho_cb.local_link_miu);
        params.rw  = NFA_CHO_RW;
        params.sn[0] = 0;

        LLCP_ConnectCfm (p_data->llcp_cback_data.connect_ind.local_sap,
                         p_data->llcp_cback_data.connect_ind.remote_sap,
                         &params);

        nfa_cho_cb.remote_miu = p_data->llcp_cback_data.connect_ind.miu;
        nfa_cho_cb.remote_sap = p_data->llcp_cback_data.connect_ind.remote_sap;
        nfa_cho_cb.local_sap  = p_data->llcp_cback_data.connect_ind.local_sap;

        nfa_cho_cb.substate  = NFA_CHO_SUBSTATE_W4_REMOTE_HR;
        nfa_cho_cb.state     = NFA_CHO_ST_CONNECTED;
        nfa_cho_cb.congested = FALSE;

        evt_data.connected.initial_role = NFA_CHO_ROLE_SELECTOR;
        nfa_cho_cb.p_cback (NFA_CHO_CONNECTED_EVT, &evt_data);
        break;

    case NFA_CHO_LLCP_LINK_STATUS_EVT:
        /*
        **  LLCP sends NFA_CHO_LLCP_DISCONNECT_IND_EVT for all data link connection
        **  before sending NFA_CHO_LLCP_LINK_STATUS_EVT for deactivation.
        **  This event can be received only in this state.
        */

        if (p_data->llcp_cback_data.link_status.is_activated == TRUE)
        {
            nfa_cho_cb.flags |= NFA_CHO_FLAGS_LLCP_ACTIVATED;

            /* store local link MIU to decide MIU of data link connection later */
            LLCP_GetLinkMIU (&nfa_cho_cb.local_link_miu, &remote_link_miu);

            /* Notify application LLCP link activated */
            evt_data.activated.is_initiator = p_data->llcp_cback_data.link_status.is_initiator;
            nfa_cho_cb.p_cback (NFA_CHO_ACTIVATED_EVT, &evt_data);
        }
        else
        {
            /* the other flags had been cleared by NFA_CHO_LLCP_DISCONNECT_IND_EVT */
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_LLCP_ACTIVATED;

            /* Notify application LLCP link deactivated */
            evt_data.status = NFA_STATUS_OK;
            nfa_cho_cb.p_cback (NFA_CHO_DEACTIVATED_EVT, &evt_data);
        }
        break;

    case NFA_CHO_API_SEND_HR_EVT:
        GKI_freebuf (p_data->api_send_hr.p_ndef);
        break;

    case NFA_CHO_API_SEND_HS_EVT:
        GKI_freebuf (p_data->api_send_hs.p_ndef);
        break;

    case NFA_CHO_NDEF_TYPE_HANDLER_EVT:
        nfa_cho_proc_ndef_type_handler_evt (p_data);
        break;

    default:
        CHO_TRACE_ERROR0 ("Unknown event");
        break;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_sm_w4_cc
**
** Description      Process event in waiting for connection confirm state
**
** Returns          None
**
*******************************************************************************/
static void nfa_cho_sm_w4_cc (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data)
{
    tNFA_CHO_EVT_DATA       evt_data;
    tLLCP_CONNECTION_PARAMS params;

    switch (event)
    {
    case NFA_CHO_API_REG_EVT:
        evt_data.status = NFA_STATUS_FAILED;
        p_data->api_reg.p_cback (NFA_CHO_REG_EVT, &evt_data);
        break;

    case NFA_CHO_API_DEREG_EVT:
        nfa_cho_proc_api_dereg ();
        nfa_cho_cb.state = NFA_CHO_ST_DISABLED;
        break;

    case NFA_CHO_API_CONNECT_EVT:
        evt_data.disconnected.reason = NFA_CHO_DISC_REASON_ALEADY_CONNECTED;
        nfa_cho_cb.p_cback (NFA_CHO_DISCONNECTED_EVT, &evt_data);
        break;

    case NFA_CHO_API_DISCONNECT_EVT:
        /* disconnect collision connection accepted by local device */
        if (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
        {
            LLCP_DisconnectReq (nfa_cho_cb.collision_local_sap,
                                nfa_cho_cb.collision_remote_sap,
                                FALSE);

            /* clear collision flag */
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;
        }

        nfa_cho_cb.state = NFA_CHO_ST_IDLE;

        /* we cannot send DISC because we don't know remote SAP */
        nfa_cho_process_disconnection (NFA_CHO_DISC_REASON_API_REQUEST);
        break;

    case NFA_CHO_LLCP_CONNECT_RESP_EVT:
        /* peer accepted connection request */
        nfa_cho_cb.state     = NFA_CHO_ST_CONNECTED;
        nfa_cho_cb.substate  = NFA_CHO_SUBSTATE_W4_LOCAL_HR;
        nfa_cho_cb.congested = FALSE;

        /* store data link connection parameters */
        nfa_cho_cb.remote_miu = p_data->llcp_cback_data.connect_resp.miu;
        nfa_cho_cb.remote_sap = p_data->llcp_cback_data.connect_resp.remote_sap;
        nfa_cho_cb.local_sap  = nfa_cho_cb.client_sap;

        evt_data.connected.initial_role = NFA_CHO_ROLE_REQUESTER;
        nfa_cho_cb.p_cback (NFA_CHO_CONNECTED_EVT, &evt_data);
        break;

    case NFA_CHO_LLCP_CONNECT_IND_EVT:
        /* if already collision of connection */
        if (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
        {
            LLCP_ConnectReject (p_data->llcp_cback_data.connect_ind.local_sap,
                                p_data->llcp_cback_data.connect_ind.remote_sap,
                                LLCP_SAP_DM_REASON_TEMP_REJECT_THIS);
        }
        else
        {
            /*
            ** accept connection request and set collision flag
            ** wait for accepting connection request from peer or Hr message
            */
            params.miu = (UINT16) (nfa_cho_cb.local_link_miu >= NFA_CHO_MIU ? NFA_CHO_MIU : nfa_cho_cb.local_link_miu);
            params.rw  = NFA_CHO_RW;
            params.sn[0] = 0;

            LLCP_ConnectCfm (p_data->llcp_cback_data.connect_ind.local_sap,
                             p_data->llcp_cback_data.connect_ind.remote_sap,
                             &params);

            nfa_cho_cb.flags |= NFA_CHO_FLAGS_CONN_COLLISION;

            nfa_cho_cb.collision_remote_miu = p_data->llcp_cback_data.connect_ind.miu;
            nfa_cho_cb.collision_remote_sap = p_data->llcp_cback_data.connect_ind.remote_sap;
            nfa_cho_cb.collision_local_sap  = p_data->llcp_cback_data.connect_ind.local_sap;
            nfa_cho_cb.collision_congested  = FALSE;
        }
        break;

    case NFA_CHO_RX_HANDOVER_MSG_EVT:
        /* peer device sent handover message before accepting connection */
        /* clear collision flag */
        nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;

        nfa_cho_cb.remote_miu = nfa_cho_cb.collision_remote_miu;
        nfa_cho_cb.remote_sap = nfa_cho_cb.collision_remote_sap;
        nfa_cho_cb.local_sap  = nfa_cho_cb.collision_local_sap;
        nfa_cho_cb.congested  = nfa_cho_cb.collision_congested;

        nfa_cho_cb.substate  = NFA_CHO_SUBSTATE_W4_REMOTE_HR;
        nfa_cho_cb.state     = NFA_CHO_ST_CONNECTED;

        evt_data.connected.initial_role = NFA_CHO_ROLE_SELECTOR;
        nfa_cho_cb.p_cback (NFA_CHO_CONNECTED_EVT, &evt_data);

        /* process handover message in nfa_cho_cb.p_rx_ndef_msg */
        nfa_cho_proc_rx_handover_msg ();
        break;

    case NFA_CHO_LLCP_DISCONNECT_RESP_EVT:
        /*
        ** if peer rejected our connection request or there is no handover service in peer
        ** but we already accepted connection from peer
        */
        if (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
        {
            /* clear collision flag */
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;

            nfa_cho_cb.remote_miu = nfa_cho_cb.collision_remote_miu;
            nfa_cho_cb.remote_sap = nfa_cho_cb.collision_remote_sap;
            nfa_cho_cb.local_sap  = nfa_cho_cb.collision_local_sap;
            nfa_cho_cb.congested  = nfa_cho_cb.collision_congested;

            nfa_cho_cb.substate  = NFA_CHO_SUBSTATE_W4_REMOTE_HR;
            nfa_cho_cb.state     = NFA_CHO_ST_CONNECTED;

            evt_data.connected.initial_role = NFA_CHO_ROLE_SELECTOR;
            nfa_cho_cb.p_cback (NFA_CHO_CONNECTED_EVT, &evt_data);
        }
        else
        {
            nfa_cho_cb.state = NFA_CHO_ST_IDLE;
            nfa_cho_process_disconnection (NFA_CHO_DISC_REASON_CONNECTION_FAIL);
        }
        break;

    case NFA_CHO_LLCP_DISCONNECT_IND_EVT:
        /* if peer disconnects collision connection */
        if (  (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
            &&(p_data->llcp_cback_data.disconnect_ind.local_sap == nfa_cho_cb.collision_local_sap)
            &&(p_data->llcp_cback_data.disconnect_ind.remote_sap == nfa_cho_cb.collision_remote_sap)  )
        {
            /* clear collision flag */
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;
        }
        else    /* Link failure before peer accepts or rejects connection request */
        {
            nfa_cho_cb.state = NFA_CHO_ST_IDLE;
            nfa_cho_process_disconnection (NFA_CHO_DISC_REASON_CONNECTION_FAIL);
        }
        break;

    case NFA_CHO_LLCP_CONGEST_EVT:
        /* if collision connection is congested */
        if (  (p_data->llcp_cback_data.congest.link_type == LLCP_LINK_TYPE_DATA_LINK_CONNECTION)
            &&(nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION))
        {
            nfa_cho_cb.collision_congested = p_data->llcp_cback_data.congest.is_congested;
        }
        break;

    case NFA_CHO_API_SEND_HR_EVT:
        GKI_freebuf (p_data->api_send_hr.p_ndef);
        break;

    case NFA_CHO_API_SEND_HS_EVT:
        GKI_freebuf (p_data->api_send_hs.p_ndef);
        break;

    case NFA_CHO_NDEF_TYPE_HANDLER_EVT:
        nfa_cho_proc_ndef_type_handler_evt (p_data);
        break;

    default:
        CHO_TRACE_ERROR0 ("Unknown event");
        break;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_sm_connected
**
** Description      Process event in connected state
**
** Returns          None
**
*******************************************************************************/
static void nfa_cho_sm_connected (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data)
{
    tNFA_CHO_EVT_DATA evt_data;
    tNFA_STATUS       status;

    switch (event)
    {
    case NFA_CHO_API_REG_EVT:
        evt_data.status = NFA_STATUS_FAILED;
        p_data->api_reg.p_cback (NFA_CHO_REG_EVT, &evt_data);
        break;

    case NFA_CHO_API_DEREG_EVT:
        nfa_cho_proc_api_dereg ();
        nfa_cho_cb.state = NFA_CHO_ST_DISABLED;
        break;

    case NFA_CHO_API_CONNECT_EVT:
        /* it could be race condition, let app know outgoing connection failed */
        evt_data.disconnected.reason = NFA_CHO_DISC_REASON_ALEADY_CONNECTED;
        nfa_cho_cb.p_cback (NFA_CHO_DISCONNECTED_EVT, &evt_data);
        break;

    case NFA_CHO_API_DISCONNECT_EVT:
        /* disconnect collision connection accepted by local device */
        if (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
        {
            LLCP_DisconnectReq (nfa_cho_cb.collision_local_sap,
                                nfa_cho_cb.collision_remote_sap,
                                FALSE);

            /* clear collision flag */
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;
        }

        LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                            nfa_cho_cb.remote_sap,
                            FALSE);

        /* store disconnect reason */
        nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_API_REQUEST;
        break;

    case NFA_CHO_API_SEND_HR_EVT:
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_LOCAL_HR)
        {
            /* Send Handover Request Message */
            status = nfa_cho_send_hr (&p_data->api_send_hr);

            if (status == NFA_STATUS_OK)
            {
                nfa_cho_cb.substate = NFA_CHO_SUBSTATE_W4_REMOTE_HS;
                /* start timer for Handover Select Message */
                nfa_sys_start_timer (&nfa_cho_cb.timer, 0, NFA_CHO_TIMEOUT_FOR_HS);
            }
            else
            {
                CHO_TRACE_ERROR0 ("NFA CHO failed to send Hr");
                nfa_cho_notify_tx_fail_evt (status);
            }
        }
        else
        {
            CHO_TRACE_ERROR0 ("NFA CHO got unexpected NFA_CHO_API_SEND_HR_EVT");
            nfa_cho_notify_tx_fail_evt (NFA_STATUS_SEMANTIC_ERROR);
        }
        GKI_freebuf (p_data->api_send_hr.p_ndef);
        break;

    case NFA_CHO_API_SEND_HS_EVT:
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_LOCAL_HS)
        {
            /* send Handover Select Message */
            status = nfa_cho_send_hs (&p_data->api_send_hs);
            if (status == NFA_STATUS_OK)
            {
                nfa_cho_cb.substate = NFA_CHO_SUBSTATE_W4_REMOTE_HR;
            }
            else
            {
                CHO_TRACE_ERROR0 ("NFA CHO failed to send Hs");
                nfa_cho_notify_tx_fail_evt (status);
            }
        }
        else
        {
            CHO_TRACE_ERROR0 ("NFA CHO got unexpected NFA_CHO_API_SEND_HS_EVT");
            nfa_cho_notify_tx_fail_evt (NFA_STATUS_SEMANTIC_ERROR);
        }
        GKI_freebuf (p_data->api_send_hs.p_ndef);
        break;

    case NFA_CHO_API_SEL_ERR_EVT:
        /* application detected error */
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_LOCAL_HS)
        {
            /* Send Handover Select Error record */
            status = nfa_cho_send_hs_error (p_data->api_sel_err.error_reason,
                                            p_data->api_sel_err.error_data);
            if (status == NFA_STATUS_OK)
            {
                nfa_cho_cb.substate = NFA_CHO_SUBSTATE_W4_REMOTE_HR;
            }
            else
            {
                CHO_TRACE_ERROR0 ("Failed to send Hs Error record");
                nfa_cho_notify_tx_fail_evt (status);
            }
        }
        else
        {
            CHO_TRACE_ERROR0 ("NFA CHO got unexpected NFA_CHO_API_SEL_ERR_EVT");
            nfa_cho_notify_tx_fail_evt (NFA_STATUS_SEMANTIC_ERROR);
        }
        break;

    case NFA_CHO_LLCP_CONNECT_RESP_EVT:
        /* peer accepted connection after we accepted and received Hr */
        /* disconnect data link connection created by local device    */
        LLCP_DisconnectReq (p_data->llcp_cback_data.connect_resp.local_sap,
                            p_data->llcp_cback_data.connect_resp.remote_sap,
                            FALSE);
        break;

    case NFA_CHO_LLCP_CONNECT_IND_EVT:
        LLCP_ConnectReject (p_data->llcp_cback_data.connect_ind.local_sap,
                            p_data->llcp_cback_data.connect_ind.remote_sap,
                            LLCP_SAP_DM_REASON_TEMP_REJECT_THIS);
        break;

    case NFA_CHO_RX_HANDOVER_MSG_EVT:
        /* process handover message in nfa_cho_cb.p_rx_ndef_msg */
        nfa_cho_proc_rx_handover_msg ();
        break;

    case NFA_CHO_LLCP_DISCONNECT_IND_EVT:
        if (  (p_data->llcp_cback_data.disconnect_ind.local_sap  == nfa_cho_cb.local_sap)
            &&(p_data->llcp_cback_data.disconnect_ind.remote_sap == nfa_cho_cb.remote_sap)  )
        {
            nfa_cho_cb.state = NFA_CHO_ST_IDLE;
            nfa_cho_process_disconnection (NFA_CHO_DISC_REASON_PEER_REQUEST);
        }
        else  /* if disconnection of collision conneciton */
        {
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;
        }
        break;

    case NFA_CHO_LLCP_DISCONNECT_RESP_EVT:
        if (  (p_data->llcp_cback_data.disconnect_ind.local_sap  == nfa_cho_cb.local_sap)
            &&(p_data->llcp_cback_data.disconnect_ind.remote_sap == nfa_cho_cb.remote_sap)  )
        {
            nfa_cho_cb.state = NFA_CHO_ST_IDLE;
            nfa_cho_process_disconnection (nfa_cho_cb.disc_reason);
        }
        else  /* if disconnection of collision conneciton */
        {
            nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;
        }
        break;

    case NFA_CHO_LLCP_CONGEST_EVT:
        /* if data link connection is congested */
        if ( (p_data->llcp_cback_data.congest.link_type  == LLCP_LINK_TYPE_DATA_LINK_CONNECTION)
           &&(p_data->llcp_cback_data.congest.local_sap  == nfa_cho_cb.local_sap)
           &&(p_data->llcp_cback_data.congest.remote_sap == nfa_cho_cb.remote_sap)  )
        {
            nfa_cho_cb.congested = p_data->llcp_cback_data.congest.is_congested;

            if (!nfa_cho_cb.congested)
            {
                /* send remaining message if any */
                if (  (nfa_cho_cb.p_tx_ndef_msg)
                    &&(nfa_cho_cb.tx_ndef_sent_size < nfa_cho_cb.tx_ndef_cur_size)  )
                {
                    nfa_cho_send_handover_msg ();
                }
            }
        }
        break;

    case NFA_CHO_TIMEOUT_EVT:
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HS)
        {
            CHO_TRACE_ERROR0 ("Failed to receive Hs message");
        }
        else if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HR)
        {
            /* we didn't get complete Hr, don't need to notify application */
            CHO_TRACE_ERROR0 ("Failed to receive Hr message");
        }

        /* store disconnect reason and disconnect */
        nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_TIMEOUT;
        LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                            nfa_cho_cb.remote_sap,
                            FALSE);
        break;

    case NFA_CHO_NDEF_TYPE_HANDLER_EVT:
        nfa_cho_proc_ndef_type_handler_evt (p_data);
        break;

    default:
        CHO_TRACE_ERROR0 ("Unknown event");
        break;
    }
}
/*******************************************************************************
**
** Function         nfa_cho_sm_execute
**
** Description      Process event in state machine
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_sm_execute (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_data)
{
#if (BT_TRACE_VERBOSE == TRUE)
    CHO_TRACE_DEBUG2 ("nfa_cho_sm_execute (): State[%s], Event[%s]",
                       nfa_cho_state_code (nfa_cho_cb.state),
                       nfa_cho_evt_code (event));
#else
    CHO_TRACE_DEBUG2 ("nfa_cho_sm_execute (): State[%d], Event[%d]",
                       nfa_cho_cb.state, event);
#endif


    switch (nfa_cho_cb.state)
    {
    case NFA_CHO_ST_DISABLED:
        nfa_cho_sm_disabled (event, p_data);
        break;

    case NFA_CHO_ST_IDLE:
        nfa_cho_sm_idle (event, p_data);
        break;

    case NFA_CHO_ST_W4_CC:
        nfa_cho_sm_w4_cc (event, p_data);
        break;

    case NFA_CHO_ST_CONNECTED:
        nfa_cho_sm_connected (event, p_data);
        break;

    default:
        CHO_TRACE_ERROR0 ("Unknown state");
        break;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_resolve_collision
**
** Description      Resolve collision by random number in Hr
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_resolve_collision (BOOLEAN *p_free_hr)
{
    tNFA_CHO_ROLE_TYPE role;
    tNFA_STATUS        status;

    /* resolve collistion by random number */
    role = nfa_cho_get_local_device_role (nfa_cho_cb.rx_ndef_cur_size,
                                          nfa_cho_cb.p_rx_ndef_msg);

    /* if local device becomes selector */
    if (role == NFA_CHO_ROLE_SELECTOR)
    {
        /* peer device is winner so clean up any collision */
        if (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
        {
            /* disconnect data link connection created by local device */
            LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                                nfa_cho_cb.remote_sap,
                                FALSE);

            nfa_cho_cb.remote_miu = nfa_cho_cb.collision_remote_miu;
            nfa_cho_cb.remote_sap = nfa_cho_cb.collision_remote_sap;
            nfa_cho_cb.local_sap  = nfa_cho_cb.collision_local_sap;
            nfa_cho_cb.congested  = nfa_cho_cb.collision_congested;
        }

        nfa_cho_cb.substate = NFA_CHO_SUBSTATE_W4_LOCAL_HS;

        nfa_cho_proc_hr (nfa_cho_cb.rx_ndef_cur_size,
                         nfa_cho_cb.p_rx_ndef_msg);

        *p_free_hr = TRUE;
    }
    /* if both random numbers are equal */
    else if (role == NFA_CHO_ROLE_UNDECIDED)
    {
        /* send Hr with new random number */
        if (nfa_cho_cb.p_tx_ndef_msg)
        {
            status = nfa_cho_update_random_number (nfa_cho_cb.p_tx_ndef_msg);

            if (status == NFA_STATUS_OK)
            {
                nfa_cho_cb.tx_ndef_sent_size = 0;
                status = nfa_cho_send_handover_msg ();
            }
        }
        else
        {
            status = NFA_STATUS_FAILED;
        }

        if (status == NFA_STATUS_FAILED)
        {
            CHO_TRACE_ERROR0 ("Failed to send Hr record with new random number");

            nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_INTERNAL_ERROR;

            /* disconnect and notify application */
            LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                                nfa_cho_cb.remote_sap,
                                FALSE);
        }
        else
        {
            /* restart timer */
            nfa_sys_start_timer (&nfa_cho_cb.timer, 0, NFA_CHO_TIMEOUT_FOR_HS);

            /* Don't free previous tx NDEF message because we are reusing it */
            *p_free_hr = FALSE;
        }
    }
    else /* if (role == NFA_CHO_ROLE_REQUESTER) */
    {
        /* wait for "Hs" record */
        *p_free_hr = TRUE;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_check_disconnect_collision
**
** Description      Disconnect any collision connection
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_check_disconnect_collision (void)
{
    if (nfa_cho_cb.flags & NFA_CHO_FLAGS_CONN_COLLISION)
    {
        /* disconnect collision connection */
        LLCP_DisconnectReq (nfa_cho_cb.collision_local_sap,
                            nfa_cho_cb.collision_remote_sap,
                            FALSE);
    }
}

/*******************************************************************************
**
** Function         nfa_cho_proc_rx_handover_msg
**
** Description      Process received Handover Message
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_proc_rx_handover_msg (void)
{
    tNFA_CHO_MSG_TYPE msg_type;
    BOOLEAN           free_tx_ndef_msg = TRUE;

    /* get message type before processing to check collision */
    msg_type = nfa_cho_get_msg_type (nfa_cho_cb.rx_ndef_cur_size,
                                     nfa_cho_cb.p_rx_ndef_msg);

    if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HS)
    {
        /* if we sent "Hr" but received "Hr", collision */
        if (msg_type == NFA_CHO_MSG_HR)
        {
            nfa_cho_resolve_collision (&free_tx_ndef_msg);
        }
        else if (msg_type == NFA_CHO_MSG_HS)
        {
            /* parse and report application */
            nfa_cho_proc_hs (nfa_cho_cb.rx_ndef_cur_size,
                             nfa_cho_cb.p_rx_ndef_msg);

            nfa_cho_cb.substate = NFA_CHO_SUBSTATE_W4_LOCAL_HR;
        }
        else
        {
            CHO_TRACE_ERROR0 ("nfa_cho_proc_rx_handover_msg (): Unknown Message Type");

            nfa_cho_check_disconnect_collision ();

            nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_UNKNOWN_MSG;

            LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                                nfa_cho_cb.remote_sap,
                                FALSE);
        }
    }
    else if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HR)
    {
        if (msg_type == NFA_CHO_MSG_HR)
        {
            /* parse and notify NFA_CHO_REQ_EVT to application */
            nfa_cho_proc_hr (nfa_cho_cb.rx_ndef_cur_size,
                             nfa_cho_cb.p_rx_ndef_msg);

            /* In case of parsing error, let peer got timeout (1 sec) */

            /* wait for application selection */
            nfa_cho_cb.substate = NFA_CHO_SUBSTATE_W4_LOCAL_HS;
        }
        else
        {
            CHO_TRACE_ERROR0 ("nfa_cho_proc_rx_handover_msg (): Expecting Handover Request");

            nfa_cho_check_disconnect_collision ();

            nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_SEMANTIC_ERROR;

            LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                                nfa_cho_cb.remote_sap,
                                FALSE);
        }
    }
    else
    {
        CHO_TRACE_ERROR1 ("nfa_cho_proc_rx_handover_msg (): Unexpected data in substate (0x%x)", nfa_cho_cb.substate);

        nfa_cho_check_disconnect_collision ();

        nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_SEMANTIC_ERROR;

        LLCP_DisconnectReq (nfa_cho_cb.local_sap,
                            nfa_cho_cb.remote_sap,
                            FALSE);
    }

    if ((free_tx_ndef_msg) && (nfa_cho_cb.p_tx_ndef_msg))
    {
        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;
    }

    /* processing rx message is done, free buffer for rx handover message */
    if (nfa_cho_cb.p_rx_ndef_msg)
    {
        GKI_freebuf (nfa_cho_cb.p_rx_ndef_msg);
        nfa_cho_cb.p_rx_ndef_msg = NULL;
    }
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         nfa_cho_state_code
**
** Description
**
** Returns          string of state
**
*******************************************************************************/
static char *nfa_cho_state_code (tNFA_CHO_STATE state_code)
{
    switch (state_code)
    {
    case NFA_CHO_ST_DISABLED:
        return "DISABLED";
    case NFA_CHO_ST_IDLE:
        return "IDLE";
    case NFA_CHO_ST_CONNECTED:
        return "CONNECTED";
    default:
        return "unknown state";
    }
}

/*******************************************************************************
**
** Function         nfa_cho_evt_code
**
** Description
**
** Returns          string of event
**
*******************************************************************************/
char *nfa_cho_evt_code (tNFA_CHO_INT_EVT evt_code)
{
    switch (evt_code)
    {
    case NFA_CHO_API_REG_EVT:
        return "API_REG";
    case NFA_CHO_API_DEREG_EVT:
        return "API_DEREG";
    case NFA_CHO_API_CONNECT_EVT:
        return "API_CONNECT";
    case NFA_CHO_API_DISCONNECT_EVT:
        return "API_DISCONNECT";
    case NFA_CHO_API_SEND_HR_EVT:
        return "API_SEND_HR";
    case NFA_CHO_API_SEND_HS_EVT:
        return "API_SEND_HS";
    case NFA_CHO_API_SEL_ERR_EVT:
        return "API_SEL_ERR";

    case NFA_CHO_RX_HANDOVER_MSG_EVT:
        return "RX_HANDOVER_MSG";

    case NFA_CHO_LLCP_CONNECT_IND_EVT:
        return "LLCP_CONNECT_IND";
    case NFA_CHO_LLCP_CONNECT_RESP_EVT:
        return "LLCP_CONNECT_RESP";
    case NFA_CHO_LLCP_DISCONNECT_IND_EVT:
        return "LLCP_DISCONNECT_IND";
    case NFA_CHO_LLCP_DISCONNECT_RESP_EVT:
        return "LLCP_DISCONNECT_RESP";
    case NFA_CHO_LLCP_CONGEST_EVT:
        return "LLCP_CONGEST";
    case NFA_CHO_LLCP_LINK_STATUS_EVT:
        return "LLCP_LINK_STATUS";

    case NFA_CHO_NDEF_TYPE_HANDLER_EVT:
        return "NDEF_TYPE_HANDLER";
    case NFA_CHO_TIMEOUT_EVT:
        return "TIMEOUT";

    default:
        return "unknown event";
    }
}
#endif  /* Debug Functions */
