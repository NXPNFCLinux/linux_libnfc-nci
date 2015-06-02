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
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015 NXP Semiconductors
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
 *  This is the utilities implementation file for the NFA Connection
 *  Handover.
 *
 ******************************************************************************/

#include "string.h"
#include "nfa_sys.h"
#include "llcp_api.h"
#include "llcp_defs.h"
#include "nfa_p2p_int.h"
#include "nfa_cho_api.h"
#include "nfa_cho_int.h"
#include "trace_api.h"
#include "nfa_mem_co.h"

/*****************************************************************************
**  Constants
*****************************************************************************/
/* Handover server name on LLCP */
static char *p_cho_service_name = "urn:nfc:sn:handover";

/* Handover Request Record Type */
static UINT8 hr_rec_type[HR_REC_TYPE_LEN] = { 0x48, 0x72 }; /* "Hr" */

/* Handover Select Record Type */
static UINT8 hs_rec_type[HS_REC_TYPE_LEN] = { 0x48, 0x73 }; /* "Hs" */

/* Handover Carrier recrod Type */
/* static UINT8 hc_rec_type[HC_REC_TYPE_LEN] = { 0x48, 0x63 }; "Hc" */

/* Collision Resolution Record Type */
static UINT8 cr_rec_type[CR_REC_TYPE_LEN] = { 0x63, 0x72 }; /* "cr" */

/* Alternative Carrier Record Type */
static UINT8 ac_rec_type[AC_REC_TYPE_LEN] = { 0x61, 0x63 }; /* "ac" */

/* Error Record Type */
static UINT8 err_rec_type[ERR_REC_TYPE_LEN] = { 0x65, 0x72, 0x72 }; /* "err" */

/* Bluetooth OOB Data Type */
static UINT8 *p_bt_oob_rec_type = (UINT8 *) "application/vnd.bluetooth.ep.oob";

/* WiFi Data Type */
static UINT8 *p_wifi_rec_type = (UINT8 *) "application/vnd.wfa.wsc";

/*****************************************************************************
**  Global Variables
*****************************************************************************/

/*****************************************************************************
**  Static Functions
*****************************************************************************/
static void nfa_cho_ndef_cback (tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *p_data);

/*******************************************************************************
**
** Function         nfa_cho_ndef_cback
**
** Description      callback function from NDEF handler
**                  Post NDEF handler callback event to NFA Connection Handover module
**
** Returns          None
**
*******************************************************************************/
static void nfa_cho_ndef_cback (tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *p_data)
{
    tNFA_CHO_NDEF_TYPE_HDLR_EVT *p_msg;
    tNFA_CHO_MSG_TYPE           msg_type;

    CHO_TRACE_DEBUG1 ("nfa_cho_ndef_cback () event=%d", event);

    if ((p_msg = (tNFA_CHO_NDEF_TYPE_HDLR_EVT *) GKI_getbuf (sizeof (tNFA_CHO_NDEF_TYPE_HDLR_EVT))) != NULL)
    {
        p_msg->hdr.event = NFA_CHO_NDEF_TYPE_HANDLER_EVT;

        /* copy NDEF handler callback event and data */
        p_msg->event = event;
        memcpy (&(p_msg->data), p_data, sizeof (tNFA_NDEF_EVT_DATA));

        /* if it has NDEF message */
        if (event == NFA_NDEF_DATA_EVT)
        {
            if (p_data->ndef_data.ndef_type_handle == nfa_cho_cb.bt_ndef_type_handle )
            {
                msg_type = nfa_cho_get_msg_type (p_data->ndef_data.len,
                                                 p_data->ndef_data.p_data);
                if (msg_type != NFA_CHO_MSG_BT_OOB)
                {
                    /* This is not simplified BT OOB Message. It contains BT OOB Message. */
                    GKI_freebuf (p_msg);
                    return;
                }
            }
            else if (p_data->ndef_data.ndef_type_handle == nfa_cho_cb.wifi_ndef_type_handle )
            {
                msg_type = nfa_cho_get_msg_type (p_data->ndef_data.len,
                                                 p_data->ndef_data.p_data);
                if (msg_type != NFA_CHO_MSG_WIFI)
                {
                    /* This is not simplified WiFi Message. It contains WiFi Message. */
                    GKI_freebuf (p_msg);
                    return;
                }
            }

            /*
            ** NDEF message could be bigger than max GKI buffer
            ** so allocate memory from platform.
            */
            p_msg->data.ndef_data.p_data = (UINT8 *) nfa_mem_co_alloc (p_msg->data.ndef_data.len);

            if (p_msg->data.ndef_data.p_data)
            {
                memcpy (p_msg->data.ndef_data.p_data,
                        p_data->ndef_data.p_data,
                        p_msg->data.ndef_data.len);
            }
            else
            {
                CHO_TRACE_ERROR1 ("Failed nfa_mem_co_alloc () for %d bytes", p_msg->data.ndef_data.len);
                GKI_freebuf (p_msg);
                return;
            }
        }

        nfa_sys_sendmsg (p_msg);
    }
}

/*******************************************************************************
**
** Function         nfa_cho_proc_ndef_type_handler_evt
**
** Description      Process events (registration and NDEF data) from NFA NDEF
**                  Type Handler
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
void nfa_cho_proc_ndef_type_handler_evt (tNFA_CHO_INT_EVENT_DATA *p_evt_data)
{
    tNFA_CHO_MSG_TYPE msg_type;

    if (p_evt_data->ndef_type_hdlr.event == NFA_NDEF_REGISTER_EVT)
    {
        if (p_evt_data->ndef_type_hdlr.data.ndef_reg.status == NFA_STATUS_OK)
        {
            /* store handle for deregistration */
            if (nfa_cho_cb.hs_ndef_type_handle == NFA_HANDLE_INVALID)
            {
                nfa_cho_cb.hs_ndef_type_handle = p_evt_data->ndef_type_hdlr.data.ndef_reg.ndef_type_handle;
            }
            else if (nfa_cho_cb.bt_ndef_type_handle == NFA_HANDLE_INVALID)
            {
                nfa_cho_cb.bt_ndef_type_handle = p_evt_data->ndef_type_hdlr.data.ndef_reg.ndef_type_handle;
            }
            else if (nfa_cho_cb.wifi_ndef_type_handle == NFA_HANDLE_INVALID)
            {
                nfa_cho_cb.wifi_ndef_type_handle = p_evt_data->ndef_type_hdlr.data.ndef_reg.ndef_type_handle;
            }
        }
    }
    else if (p_evt_data->ndef_type_hdlr.event == NFA_NDEF_DATA_EVT)
    {
        /* if negotiated handover is on going, then ignore static handover */
        if (nfa_cho_cb.state != NFA_CHO_ST_CONNECTED)
        {
#if (BT_TRACE_PROTOCOL == TRUE)
            DispNDEFMsg (p_evt_data->ndef_type_hdlr.data.ndef_data.p_data,
                         p_evt_data->ndef_type_hdlr.data.ndef_data.len, TRUE);
#endif
            msg_type = nfa_cho_get_msg_type (p_evt_data->ndef_type_hdlr.data.ndef_data.len,
                                             p_evt_data->ndef_type_hdlr.data.ndef_data.p_data);

            if (msg_type == NFA_CHO_MSG_HS)
            {
                nfa_cho_proc_hs (p_evt_data->ndef_type_hdlr.data.ndef_data.len,
                                 p_evt_data->ndef_type_hdlr.data.ndef_data.p_data);
            }
            else if (  (msg_type == NFA_CHO_MSG_BT_OOB)
                     ||(msg_type == NFA_CHO_MSG_WIFI)  )
            {
                /* simplified BT OOB/Wifi Message */
                nfa_cho_proc_simplified_format (p_evt_data->ndef_type_hdlr.data.ndef_data.len,
                                                p_evt_data->ndef_type_hdlr.data.ndef_data.p_data);
            }
            else
            {
                CHO_TRACE_ERROR0 ("Unexpected CHO Message Type");
            }
        }

        nfa_mem_co_free (p_evt_data->ndef_type_hdlr.data.ndef_data.p_data);
    }
}

/*******************************************************************************
**
** Function         nfa_cho_proc_api_reg
**
** Description      Process registeration request from application
**                  Register Handover server on LLCP for negotiated handover
**                  Register handover select records on NDEF handler for static handover
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_proc_api_reg (tNFA_CHO_INT_EVENT_DATA *p_evt_data)
{
    CHO_TRACE_DEBUG1 ("nfa_cho_proc_api_reg (): enable_server=%d",
                      p_evt_data->api_reg.enable_server);

    if (p_evt_data->api_reg.enable_server == TRUE)
    {
        /* Register Handover server on LLCP for negotiated handover */
        nfa_cho_cb.server_sap = LLCP_RegisterServer (LLCP_INVALID_SAP,
                                                     LLCP_LINK_TYPE_DATA_LINK_CONNECTION,
                                                     p_cho_service_name,
                                                     nfa_cho_sm_llcp_cback);
        if (nfa_cho_cb.server_sap == LLCP_INVALID_SAP)
        {
            CHO_TRACE_ERROR0 ("Cannot register CHO server");
            return NFA_STATUS_FAILED;
        }
        else
        {
            nfa_p2p_enable_listening (NFA_ID_CHO, FALSE);
        }
    }
    else
    {
        /*
        ** Register Handover client on LLCP for negotiated handover
        ** LLCP will notify link status through callback
        */
        nfa_cho_cb.client_sap = LLCP_RegisterClient (LLCP_LINK_TYPE_DATA_LINK_CONNECTION,
                                                     nfa_cho_sm_llcp_cback);

        if (nfa_cho_cb.client_sap == LLCP_INVALID_SAP)
        {
            CHO_TRACE_ERROR0 ("Cannot register CHO client");
            return NFA_STATUS_FAILED;
        }

        /* set flag not to deregister client when disconnected */
        nfa_cho_cb.flags |= NFA_CHO_FLAGS_CLIENT_ONLY;
    }

    /* Register handover select record on NDEF handler for static handover */
    if (nfa_cho_cb.hs_ndef_type_handle == NFA_HANDLE_INVALID)
    {
        NFA_RegisterNDefTypeHandler (TRUE, NFA_TNF_WKT, hs_rec_type, HS_REC_TYPE_LEN,
                                     nfa_cho_ndef_cback);
    }
    if (nfa_cho_cb.bt_ndef_type_handle == NFA_HANDLE_INVALID)
    {
        NFA_RegisterNDefTypeHandler (TRUE, NFA_TNF_RFC2046_MEDIA,
                                     p_bt_oob_rec_type, (UINT8) strlen ((char *) p_bt_oob_rec_type),
                                     nfa_cho_ndef_cback);
    }
    if (nfa_cho_cb.wifi_ndef_type_handle == NFA_HANDLE_INVALID)
    {
        NFA_RegisterNDefTypeHandler (TRUE, NFA_TNF_RFC2046_MEDIA,
                                     p_wifi_rec_type, (UINT8) strlen ((char *) p_wifi_rec_type),
                                     nfa_cho_ndef_cback);
    }

    nfa_cho_cb.p_cback = p_evt_data->api_reg.p_cback;

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_cho_proc_api_dereg
**
** Description      Process deregisteration request from application
**                  Disconnect LLCP connection if any
**                  Deregister callback from NDEF handler and NFA P2P
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_proc_api_dereg (void)
{
    CHO_TRACE_DEBUG0 ("nfa_cho_proc_api_dereg ()");

    /* Deregister outgoing connection, data link will be disconnected if any */
    if (nfa_cho_cb.client_sap != LLCP_INVALID_SAP)
    {
        LLCP_Deregister (nfa_cho_cb.client_sap);
        nfa_cho_cb.client_sap = LLCP_INVALID_SAP;
    }

    /* Close Connection Handover server in LLCP, data link will be disconnected if any */
    if (nfa_cho_cb.server_sap != LLCP_INVALID_SAP)
    {
        LLCP_Deregister (nfa_cho_cb.server_sap);
        nfa_cho_cb.server_sap = LLCP_INVALID_SAP;
    }

    /* Deregister type handler if any */
    if (nfa_cho_cb.hs_ndef_type_handle != NFA_HANDLE_INVALID)
    {
        NFA_DeregisterNDefTypeHandler (nfa_cho_cb.hs_ndef_type_handle);
        nfa_cho_cb.hs_ndef_type_handle = NFA_HANDLE_INVALID;
    }

    if (nfa_cho_cb.bt_ndef_type_handle != NFA_HANDLE_INVALID)
    {
        NFA_DeregisterNDefTypeHandler (nfa_cho_cb.bt_ndef_type_handle);
        nfa_cho_cb.bt_ndef_type_handle = NFA_HANDLE_INVALID;
    }

    if (nfa_cho_cb.wifi_ndef_type_handle != NFA_HANDLE_INVALID)
    {
        NFA_DeregisterNDefTypeHandler (nfa_cho_cb.wifi_ndef_type_handle);
        nfa_cho_cb.wifi_ndef_type_handle = NFA_HANDLE_INVALID;
    }

    nfa_sys_stop_timer (&nfa_cho_cb.timer);
    nfa_cho_cb.p_cback = NULL;
    nfa_cho_cb.flags   = 0;

    nfa_p2p_disable_listening (NFA_ID_CHO, FALSE);
}

/*******************************************************************************
**
** Function         nfa_cho_create_connection
**
** Description      Create data link connection with handover server in remote
**
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_cho_create_connection (void)
{
    tLLCP_CONNECTION_PARAMS conn_params;
    tNFA_STATUS             status = NFA_STATUS_FAILED;

    CHO_TRACE_DEBUG0 ("nfa_cho_create_connection ()");

    if (nfa_cho_cb.client_sap == LLCP_INVALID_SAP)
    {
        nfa_cho_cb.client_sap = LLCP_RegisterClient (LLCP_LINK_TYPE_DATA_LINK_CONNECTION,
                                                     nfa_cho_sm_llcp_cback);
    }

    if (nfa_cho_cb.client_sap == LLCP_INVALID_SAP)
    {
        CHO_TRACE_ERROR0 ("Cannot register CHO client");
    }
    else
    {
        /* create data link connection with server name */
        conn_params.miu = (UINT16) (nfa_cho_cb.local_link_miu >= NFA_CHO_MIU ? NFA_CHO_MIU : nfa_cho_cb.local_link_miu);
        conn_params.rw  = NFA_CHO_RW;
        BCM_STRNCPY_S (conn_params.sn, sizeof (conn_params.sn),
                       p_cho_service_name, LLCP_MAX_SN_LEN);
        conn_params.sn[LLCP_MAX_SN_LEN] = 0;

        if (LLCP_ConnectReq (nfa_cho_cb.client_sap, LLCP_SAP_SDP, &conn_params) == LLCP_STATUS_SUCCESS)
            status = NFA_STATUS_OK;
    }

    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_process_disconnection
**
** Description      Clean up buffers and notify disconnection to application
**
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_process_disconnection (tNFA_CHO_DISC_REASON disc_reason)
{
    tNFA_CHO_EVT_DATA evt_data;

    nfa_sys_stop_timer (&nfa_cho_cb.timer);

    /* free buffer for Tx/Rx NDEF message */
    if (nfa_cho_cb.p_tx_ndef_msg)
    {
        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;
    }
    if (nfa_cho_cb.p_rx_ndef_msg)
    {
        GKI_freebuf (nfa_cho_cb.p_rx_ndef_msg);
        nfa_cho_cb.p_rx_ndef_msg = NULL;
    }

    /* if no server is registered on LLCP, do not deregister client to get link statue from LLCP */
    if (!(nfa_cho_cb.flags & NFA_CHO_FLAGS_CLIENT_ONLY))
    {
        if (nfa_cho_cb.client_sap != LLCP_INVALID_SAP)
        {
            LLCP_Deregister (nfa_cho_cb.client_sap);
            nfa_cho_cb.client_sap = LLCP_INVALID_SAP;
        }
    }

    nfa_cho_cb.flags &= ~NFA_CHO_FLAGS_CONN_COLLISION;

    evt_data.disconnected.reason = disc_reason;
    nfa_cho_cb.p_cback (NFA_CHO_DISCONNECTED_EVT, &evt_data);
}

/*******************************************************************************
**
** Function         nfa_cho_notify_tx_fail_evt
**
** Description      Notify application of NFA_CHO_TX_FAIL_EVT
**
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_notify_tx_fail_evt (tNFA_STATUS status)
{
    tNFA_CHO_EVT_DATA evt_data;

    CHO_TRACE_DEBUG0 ("nfa_cho_notify_tx_fail_evt ()");

    evt_data.status = status;

    if (nfa_cho_cb.p_cback)
        nfa_cho_cb.p_cback (NFA_CHO_TX_FAIL_EVT, &evt_data);
}

/*******************************************************************************
**
** Function         nfa_cho_reassemble_ho_msg
**
** Description      Reassemble received data for handover message
**
**
** Returns          tNFA_CHO_RX_NDEF_STATUS
**
*******************************************************************************/
tNFA_CHO_RX_NDEF_STATUS nfa_cho_reassemble_ho_msg (UINT8 local_sap, UINT8 remote_sap)
{
    tNFA_CHO_RX_NDEF_STATUS rx_status;

    nfa_sys_stop_timer (&nfa_cho_cb.timer);

    /*
    ** allocate memory for NDEF message for the first segment
    ** validate NDEF message to check if received complete message
    */
    rx_status = nfa_cho_read_ndef_msg (local_sap, remote_sap);

    /* if Temporary Memory Constraint */
    if (rx_status == NFA_CHO_RX_NDEF_TEMP_MEM)
    {
        CHO_TRACE_ERROR0 ("Failed due to Temporary Memory Constraint");

        /* if we are expecting Hr then send Hs Error record */
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HR)
        {
            /* ask retry later, handover request will disconnect */
            nfa_cho_send_hs_error (NFA_CHO_ERROR_TEMP_MEM, NFA_CHO_TIMEOUT_FOR_RETRY);
        }
        else
        {
            /* we cannot send error record, so disconnect */
            nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_INTERNAL_ERROR;
            LLCP_DisconnectReq (nfa_cho_cb.local_sap, nfa_cho_cb.remote_sap, FALSE);
        }
    }
    /* Permanent Memory Constraint */
    else if (rx_status == NFA_CHO_RX_NDEF_PERM_MEM)
    {
        CHO_TRACE_ERROR0 ("Failed due to Permanent Memory Constraint");

        /* if we are expecting Hr then send Hs Error record */
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HR)
        {
            /*
            ** notify our buffer size and ask retry with modified message later
            ** handover request will disconnect
            */
            nfa_cho_send_hs_error (NFA_CHO_ERROR_PERM_MEM, nfa_cho_cb.rx_ndef_buf_size);
        }
        else
        {
            /* we cannot send error record, so disconnect */
            nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_INTERNAL_ERROR;
            LLCP_DisconnectReq (nfa_cho_cb.local_sap, nfa_cho_cb.remote_sap, FALSE);
        }
    }
    /* Invalid NDEF message */
    else if (rx_status == NFA_CHO_RX_NDEF_INVALID)
    {
        CHO_TRACE_ERROR0 ("Failed due to invalid NDEF message");

        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HR)
        {
            /* let Handover Requester got timeout */
        }
        else
        {
            /* we cannot send error record, so disconnect */
            nfa_cho_cb.disc_reason = NFA_CHO_DISC_REASON_INVALID_MSG;
            LLCP_DisconnectReq (nfa_cho_cb.local_sap, nfa_cho_cb.remote_sap, FALSE);
        }
    }
    /* need more segment */
    else if (rx_status == NFA_CHO_RX_NDEF_INCOMPLTE)
    {
        /* wait for next segment */
        if (nfa_cho_cb.substate == NFA_CHO_SUBSTATE_W4_REMOTE_HR)
        {
            nfa_sys_start_timer (&nfa_cho_cb.timer, 0, NFA_CHO_TIMEOUT_SEGMENTED_HR);
        }
        /* don't update running timer if we are waiting Hs */
    }
    else /* NFA_CHO_RX_NDEF_COMPLETE */
    {
        /* Received complete NDEF message */
    }

    return rx_status;
}

/*******************************************************************************
**
** Function         nfa_cho_send_handover_msg
**
** Description      Send segmented or whole Handover Message on LLCP
**                  if congested then wait for uncongested event from LLCP
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_send_handover_msg (void)
{
    tNFA_STATUS  status = NFA_STATUS_FAILED;
    tLLCP_STATUS llcp_status;
    UINT16       tx_size;
    BT_HDR       *p_msg;
    UINT8        *p_src, *p_dst;

    CHO_TRACE_DEBUG2 ("nfa_cho_send_handover_msg () size=%d, sent=%d",
                      nfa_cho_cb.tx_ndef_cur_size, nfa_cho_cb.tx_ndef_sent_size);

    /* while data link connection is not congested */
    while ((!nfa_cho_cb.congested) && (nfa_cho_cb.tx_ndef_sent_size < nfa_cho_cb.tx_ndef_cur_size))
    {
        /* select segment size as min (MIU of remote, remaining NDEF size) */
        if (nfa_cho_cb.tx_ndef_cur_size - nfa_cho_cb.tx_ndef_sent_size > nfa_cho_cb.remote_miu)
        {
            tx_size = nfa_cho_cb.remote_miu;
        }
        else
        {
            tx_size = (UINT16) (nfa_cho_cb.tx_ndef_cur_size - nfa_cho_cb.tx_ndef_sent_size);
        }

        /* transmit a segment on LLCP */
        if ((p_msg = (BT_HDR *) GKI_getpoolbuf (LLCP_POOL_ID)) != NULL)
        {
            p_msg->len    = (UINT16) tx_size;
            p_msg->offset = LLCP_MIN_OFFSET;

            p_dst = (UINT8*) (p_msg + 1) + p_msg->offset;
            p_src = nfa_cho_cb.p_tx_ndef_msg + nfa_cho_cb.tx_ndef_sent_size;

            memcpy (p_dst, p_src, tx_size);

            llcp_status = LLCP_SendData (nfa_cho_cb.local_sap, nfa_cho_cb.remote_sap, p_msg);

            nfa_cho_cb.tx_ndef_sent_size += tx_size;
        }
        else
        {
            llcp_status = LLCP_STATUS_FAIL;
        }

        if (llcp_status == LLCP_STATUS_SUCCESS)
        {
            status = NFA_STATUS_OK;
        }
        else if (llcp_status == LLCP_STATUS_CONGESTED)
        {
            status = NFA_STATUS_CONGESTED;
            CHO_TRACE_DEBUG0 ("Data link connection is congested");
            /* wait for uncongested event */
            nfa_cho_cb.congested = TRUE;
            break;
        }
        else
        {
            status = NFA_STATUS_FAILED;
            GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
            nfa_cho_cb.p_tx_ndef_msg = NULL;
            break;
        }
    }

    /*
    ** free buffer when receiving response or disconnected because we may need to send
    ** Hr message again due to collision
    */

    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_read_ndef_msg
**
** Description      allocate memory for NDEF message for the first segment
**                  validate NDEF message to check if received complete message
**
** Returns          None
**
*******************************************************************************/
tNFA_CHO_RX_NDEF_STATUS nfa_cho_read_ndef_msg (UINT8 local_sap, UINT8 remote_sap)
{
    tNDEF_STATUS            ndef_status;
    tNFA_CHO_RX_NDEF_STATUS rx_status;
    BOOLEAN                 more;
    UINT32                  length;

    CHO_TRACE_DEBUG2 ("nfa_cho_read_ndef_msg () local_sap=0x%x, remote_sap=0x%x",
                      local_sap, remote_sap);

    /* if this is the first segment */
    if (!nfa_cho_cb.p_rx_ndef_msg)
    {
        nfa_cho_cb.p_rx_ndef_msg = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);

        if (!nfa_cho_cb.p_rx_ndef_msg)
        {
            CHO_TRACE_ERROR0 ("Failed to allocate buffer");
            return NFA_CHO_RX_NDEF_TEMP_MEM;
        }

        nfa_cho_cb.rx_ndef_buf_size = LLCP_POOL_BUF_SIZE;
        nfa_cho_cb.rx_ndef_cur_size = 0;
    }

    more = TRUE;
    while (more)
    {
        more = LLCP_ReadDataLinkData (local_sap,
                                      remote_sap,
                                      (UINT16)(nfa_cho_cb.rx_ndef_buf_size - nfa_cho_cb.rx_ndef_cur_size),
                                      &length,
                                      nfa_cho_cb.p_rx_ndef_msg + nfa_cho_cb.rx_ndef_cur_size);

        nfa_cho_cb.rx_ndef_cur_size += length;

        /* if it doesn't fit into allocated memory */
        if ((nfa_cho_cb.rx_ndef_cur_size >= nfa_cho_cb.rx_ndef_buf_size)
          &&(more))
        {
            CHO_TRACE_ERROR0 ("Failed to store too much data");

            LLCP_FlushDataLinkRxData (local_sap, remote_sap);

            GKI_freebuf (nfa_cho_cb.p_rx_ndef_msg);
            nfa_cho_cb.p_rx_ndef_msg = NULL;

            return NFA_CHO_RX_NDEF_PERM_MEM;
        }
    }

    /* check NDEF message */
    ndef_status = NDEF_MsgValidate (nfa_cho_cb.p_rx_ndef_msg, nfa_cho_cb.rx_ndef_cur_size, FALSE);

    switch (ndef_status)
    {
    case NDEF_OK:
        rx_status = NFA_CHO_RX_NDEF_COMPLETE;
        break;

    case NDEF_MSG_TOO_SHORT:
    case NDEF_MSG_NO_MSG_END:
    case NDEF_MSG_LENGTH_MISMATCH:
        rx_status = NFA_CHO_RX_NDEF_INCOMPLTE;
        break;

    default:
        rx_status = NFA_CHO_RX_NDEF_INVALID;
        break;
    }

    if (rx_status == NFA_CHO_RX_NDEF_COMPLETE)
    {
#if (BT_TRACE_PROTOCOL == TRUE)
        DispCHO (nfa_cho_cb.p_rx_ndef_msg, nfa_cho_cb.rx_ndef_cur_size, TRUE);
#endif
    }
    else if (rx_status == NFA_CHO_RX_NDEF_INCOMPLTE)
    {
        CHO_TRACE_DEBUG0 ("Need more data to complete NDEF message");
    }
    else /* if (rx_status == NFA_CHO_RX_NDEF_INVALID) */
    {
        CHO_TRACE_ERROR1 ("Failed to validate NDEF message error=0x%x", ndef_status);
        GKI_freebuf (nfa_cho_cb.p_rx_ndef_msg);
        nfa_cho_cb.p_rx_ndef_msg = NULL;
    }

    return rx_status;
}

/*******************************************************************************
**
** Function         nfa_cho_add_cr_record
**
** Description      Adding Collision Resolution record
**
**
** Returns          NDEF_OK if success
**
*******************************************************************************/
tNDEF_STATUS nfa_cho_add_cr_record (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size)
{
    tNDEF_STATUS status;
    UINT32       temp32;

    CHO_TRACE_DEBUG1 ("nfa_cho_add_cr_record () cur_size = %d", *p_cur_size);

    /* Get random number from timer */
    temp32 = GKI_get_tick_count ();
    nfa_cho_cb.tx_random_number = (UINT16) ((temp32 >> 16) ^ (temp32));

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
    if (nfa_cho_cb.test_enabled & NFA_CHO_TEST_RANDOM)
    {
        nfa_cho_cb.tx_random_number = nfa_cho_cb.test_random_number;
    }
#endif

    CHO_TRACE_DEBUG1 ("tx_random_number = 0x%04x", nfa_cho_cb.tx_random_number);

    /* Add Well-Known Type:Collistion Resolution Record */
    status = NDEF_MsgAddWktCr (p_msg, max_size, p_cur_size,
                               nfa_cho_cb.tx_random_number);

    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_add_ac_record
**
** Description      Adding Alternative Carrier record
**
**
** Returns          NDEF_OK if success
**
*******************************************************************************/
tNDEF_STATUS nfa_cho_add_ac_record (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                    UINT8 num_ac_info, tNFA_CHO_AC_INFO *p_ac_info,
                                    UINT8 *p_ndef, UINT32 max_ndef_size, UINT32 *p_cur_ndef_size)
{
    tNDEF_STATUS status = NDEF_OK;
    UINT8        xx, yy;
    UINT8       *p_rec, *p_id, id_len;
    char         carrier_data_ref_str[NFA_CHO_MAX_REF_NAME_LEN];
    char        *aux_data_ref[NFA_CHO_MAX_AUX_DATA_COUNT];
    char         aux_data_ref_str[NFA_CHO_MAX_AUX_DATA_COUNT][NFA_CHO_MAX_REF_NAME_LEN];

    CHO_TRACE_DEBUG1 ("nfa_cho_add_ac_record (): num_ac_info = %d", num_ac_info);

    /* initialize auxilary data reference */
    for (xx = 0; xx < NFA_CHO_MAX_AUX_DATA_COUNT; xx++)
    {
        aux_data_ref[xx] = aux_data_ref_str[xx];
    }

    p_rec = p_ndef;

    /* Alternative Carrier Records */
    for (xx = 0; (xx < num_ac_info) && (status == NDEF_OK); xx++)
    {
        if (!p_rec)
        {
            status = NDEF_REC_NOT_FOUND;
            break;
        }

        p_id = NDEF_RecGetId (p_rec, &id_len);

        if ((p_id) && (id_len > 0) && (id_len <= NFA_CHO_MAX_REF_NAME_LEN))
        {
            memcpy (carrier_data_ref_str, p_id, id_len);
            carrier_data_ref_str[id_len] = 0x00;
        }
        else
        {
            CHO_TRACE_ERROR1 ("nfa_cho_add_ac_record ():id_len=%d", id_len);
            status = NDEF_REC_NOT_FOUND;
            break;
        }

        p_rec = NDEF_MsgGetNextRec (p_rec);

        /* auxilary data reference */
        for (yy = 0; yy < p_ac_info[xx].num_aux_data; yy++)
        {
            if (!p_rec)
            {
                status = NDEF_REC_NOT_FOUND;
                break;
            }

            p_id = NDEF_RecGetId (p_rec, &id_len);

            if ((p_id) && (id_len > 0) && (id_len <= NFA_CHO_MAX_REF_NAME_LEN))
            {
                memcpy (aux_data_ref_str[yy], p_id, id_len);
                aux_data_ref_str[yy][id_len] = 0x00;
            }
            else
            {
                CHO_TRACE_ERROR1 ("nfa_cho_add_ac_record ():id_len=%d", id_len);
                status = NDEF_REC_NOT_FOUND;
                break;
            }

            p_rec = NDEF_MsgGetNextRec (p_rec);
        }

        if (status == NDEF_OK)
        {
            /* Add Well-Known Type:Alternative Carrier Record */
            status = NDEF_MsgAddWktAc (p_msg, max_size, p_cur_size,
                                       p_ac_info[xx].cps, carrier_data_ref_str,
                                       p_ac_info[xx].num_aux_data, aux_data_ref);
        }

        if (status != NDEF_OK)
        {
            break;
        }
    }

    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_send_hr
**
** Description      Sending Handover Request Message
**                  It may send one from AC list to select a specific AC.
**
** Returns          NFA_STATUS_OK if success
**
*******************************************************************************/
tNFA_STATUS nfa_cho_send_hr (tNFA_CHO_API_SEND_HR *p_api_send_hr)
{
    tNDEF_STATUS    status;
    UINT8          *p_msg_cr_ac;
    UINT32          cur_size_cr_ac, max_size;
    UINT8           version;

    CHO_TRACE_DEBUG0 ("nfa_cho_send_hr ()");

    /* Collistion Resolution Record and Alternative Carrier Records */

    p_msg_cr_ac = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);
    if (!p_msg_cr_ac)
    {
        CHO_TRACE_ERROR0 ("Failed to allocate buffer");
        return NFA_STATUS_NO_BUFFERS;
    }

    max_size = LLCP_POOL_BUF_SIZE;
    NDEF_MsgInit (p_msg_cr_ac, max_size, &cur_size_cr_ac);

    /* Collistion Resolution Record */
    if (NDEF_OK != nfa_cho_add_cr_record (p_msg_cr_ac, max_size, &cur_size_cr_ac))
    {
        CHO_TRACE_ERROR0 ("Failed to add cr record");
        GKI_freebuf (p_msg_cr_ac);
        return NFA_STATUS_FAILED;
    }

    /* Alternative Carrier Records */
    if (NDEF_OK != nfa_cho_add_ac_record (p_msg_cr_ac, max_size, &cur_size_cr_ac,
                                          p_api_send_hr->num_ac_info, p_api_send_hr->p_ac_info,
                                          p_api_send_hr->p_ndef, p_api_send_hr->max_ndef_size,
                                          &(p_api_send_hr->cur_ndef_size)))
    {
        CHO_TRACE_ERROR0 ("Failed to add ac record");
        GKI_freebuf (p_msg_cr_ac);
        return NFA_STATUS_FAILED;
    }

    /* Handover Request Message */

    nfa_cho_cb.p_tx_ndef_msg = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);
    if (!nfa_cho_cb.p_tx_ndef_msg)
    {
        CHO_TRACE_ERROR0 ("Failed to allocate buffer");
        GKI_freebuf (p_msg_cr_ac);
        return NFA_STATUS_FAILED;
    }

    max_size = LLCP_POOL_BUF_SIZE;

    /* Handover Request Record */
    version = NFA_CHO_VERSION;

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
    if (nfa_cho_cb.test_enabled & NFA_CHO_TEST_VERSION)
    {
        version = nfa_cho_cb.test_version;
    }
#endif

    status = NDEF_MsgCreateWktHr (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                  version);
    if (status != NDEF_OK)
    {
        CHO_TRACE_ERROR0 ("Failed to create Hr");
        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;
        GKI_freebuf (p_msg_cr_ac);
        return NFA_STATUS_FAILED;
    }

    /* Append Collistion Resolution Record and Alternative Carrier Records */
    status = NDEF_MsgAppendPayload (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                    nfa_cho_cb.p_tx_ndef_msg, p_msg_cr_ac, cur_size_cr_ac);

    GKI_freebuf (p_msg_cr_ac);

    if (status != NDEF_OK)
    {
        CHO_TRACE_ERROR0 ("Failed to add cr/ac record");
        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;
        return NFA_STATUS_FAILED;
    }


    /* Append Alternative Carrier Reference Data or Handover Carrier Record */
    status = NDEF_MsgAppendRec (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                p_api_send_hr->p_ndef, p_api_send_hr->cur_ndef_size);

    if (status != NDEF_OK)
    {
        CHO_TRACE_ERROR0 ("Failed to add ac reference data or Hc record");
        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;
        return NFA_STATUS_FAILED;
    }

#if (BT_TRACE_PROTOCOL == TRUE)
    DispCHO (nfa_cho_cb.p_tx_ndef_msg, nfa_cho_cb.tx_ndef_cur_size, FALSE);
#endif

    /* Send it to peer */
    nfa_cho_cb.tx_ndef_sent_size = 0;

    status = nfa_cho_send_handover_msg ();

    if (status == NFA_STATUS_CONGESTED)
    {
        status = NFA_STATUS_OK;
    }

    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_send_hs
**
** Description      Send Handover Select Message
**
**
** Returns          NFA_STATUS_OK if success
**
*******************************************************************************/
tNFA_STATUS nfa_cho_send_hs (tNFA_CHO_API_SEND_HS *p_api_select)
{
    tNDEF_STATUS    status;
    UINT8          *p_msg_ac;
    UINT32          cur_size_ac = 0, max_size;
    UINT8           version;

    CHO_TRACE_DEBUG1 ("nfa_cho_send_hs () num_ac_info=%d", p_api_select->num_ac_info);

    if (p_api_select->num_ac_info > 0)
    {
        /* Alternative Carrier Records */

        p_msg_ac = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);

        if (!p_msg_ac)
        {
            CHO_TRACE_ERROR0 ("Failed to allocate buffer");
            return NFA_STATUS_FAILED;
        }

        max_size = LLCP_POOL_BUF_SIZE;
        NDEF_MsgInit (p_msg_ac, max_size, &cur_size_ac);

        if (NDEF_OK != nfa_cho_add_ac_record (p_msg_ac, max_size, &cur_size_ac,
                                              p_api_select->num_ac_info, p_api_select->p_ac_info,
                                              p_api_select->p_ndef, p_api_select->max_ndef_size,
                                              &(p_api_select->cur_ndef_size)))
        {
            CHO_TRACE_ERROR0 ("Failed to add ac record");
            GKI_freebuf (p_msg_ac);
            return NFA_STATUS_FAILED;
        }
    }
    else
    {
        p_msg_ac = NULL;
    }

    /* Handover Select Message */
    nfa_cho_cb.p_tx_ndef_msg = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);

    if (!nfa_cho_cb.p_tx_ndef_msg)
    {
        CHO_TRACE_ERROR0 ("Failed to allocate buffer");

        if (p_msg_ac)
            GKI_freebuf (p_msg_ac);

        return NFA_STATUS_FAILED;
    }
    max_size = LLCP_POOL_BUF_SIZE;

    /* Handover Select Record */
    version = NFA_CHO_VERSION;

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
    if (nfa_cho_cb.test_enabled & NFA_CHO_TEST_VERSION)
    {
        version = nfa_cho_cb.test_version;
    }
#endif
#if 0
    status = NDEF_MsgCreateWktHs (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                  version);

    if (status != NDEF_OK)
    {
        CHO_TRACE_ERROR0 ("Failed to create Hs");

        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;

        if (p_msg_ac)
            GKI_freebuf (p_msg_ac);

        return NFA_STATUS_FAILED;
    }

    if (p_api_select->num_ac_info > 0)
    {
        /* Append Alternative Carrier Records */
        status = NDEF_MsgAppendPayload (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                        nfa_cho_cb.p_tx_ndef_msg, p_msg_ac, cur_size_ac);

        if (p_msg_ac)
            GKI_freebuf (p_msg_ac);

        if (status != NDEF_OK)
        {
            CHO_TRACE_ERROR0 ("Failed to add cr record");
            GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
            nfa_cho_cb.p_tx_ndef_msg = NULL;
            return NFA_STATUS_FAILED;
        }

        /* Append Alternative Carrier Reference Data */
        status = NDEF_MsgAppendRec (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                    p_api_select->p_ndef, p_api_select->cur_ndef_size);

        if (status != NDEF_OK)
        {
            CHO_TRACE_ERROR0 ("Failed to add ac reference data record");
            GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
            nfa_cho_cb.p_tx_ndef_msg = NULL;
            return NFA_STATUS_FAILED;
        }
    }
#endif
    memcpy(nfa_cho_cb.p_tx_ndef_msg, p_api_select->p_ndef, p_api_select->cur_ndef_size);
    nfa_cho_cb.tx_ndef_cur_size = p_api_select->cur_ndef_size;
#if (BT_TRACE_PROTOCOL == TRUE)
    DispCHO (nfa_cho_cb.p_tx_ndef_msg, nfa_cho_cb.tx_ndef_cur_size, FALSE);
#endif

    /* Send it to peer */
    nfa_cho_cb.tx_ndef_sent_size = 0;

    status = nfa_cho_send_handover_msg ();

    if (status == NFA_STATUS_CONGESTED)
    {
        status = NFA_STATUS_OK;
    }
    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_send_hs_error
**
** Description      Sending Handover Select Message with error record
**
**
** Returns          NFA_STATUS_OK if success
**
*******************************************************************************/
tNFA_STATUS nfa_cho_send_hs_error (UINT8 error_reason, UINT32 error_data)
{
    tNDEF_STATUS    status;
    UINT8           version;
    UINT32          max_size;

    CHO_TRACE_DEBUG2 ("nfa_cho_send_hs_error () error_reason=0x%x, error_data=0x%x",
                       error_reason, error_data);

    /* Handover Select Message */
    nfa_cho_cb.p_tx_ndef_msg = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);

    if (!nfa_cho_cb.p_tx_ndef_msg)
    {
        CHO_TRACE_ERROR0 ("Failed to allocate buffer");
        return NFA_STATUS_FAILED;
    }

    max_size = LLCP_POOL_BUF_SIZE;

    /* Handover Select Record with Version */
    version = NFA_CHO_VERSION;

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
    if (nfa_cho_cb.test_enabled & NFA_CHO_TEST_VERSION)
    {
        version = nfa_cho_cb.test_version;
    }
#endif
    status = NDEF_MsgCreateWktHs (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                  version);

    if (status != NDEF_OK)
    {
        CHO_TRACE_ERROR0 ("Failed to create Hs");

        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;

        return NFA_STATUS_FAILED;
    }

    /* Add Error Records */
    status = NDEF_MsgAddWktErr (nfa_cho_cb.p_tx_ndef_msg, max_size, &nfa_cho_cb.tx_ndef_cur_size,
                                error_reason, error_data);

    if (status != NDEF_OK)
    {
        CHO_TRACE_ERROR0 ("Failed to add err record");

        GKI_freebuf (nfa_cho_cb.p_tx_ndef_msg);
        nfa_cho_cb.p_tx_ndef_msg = NULL;

        return NFA_STATUS_FAILED;
    }

#if (BT_TRACE_PROTOCOL == TRUE)
    DispCHO (nfa_cho_cb.p_tx_ndef_msg, nfa_cho_cb.tx_ndef_cur_size, FALSE);
#endif

    /* Send it to peer */
    nfa_cho_cb.tx_ndef_sent_size = 0;

    status = nfa_cho_send_handover_msg ();

    if (status == NFA_STATUS_CONGESTED)
    {
        status = NFA_STATUS_OK;
    }
    return status;
}

/*******************************************************************************
**
** Function         nfa_cho_get_random_number
**
** Description      Return random number in Handover Request message
**
**
** Returns          random number in "cr" record
**
*******************************************************************************/
UINT16 nfa_cho_get_random_number (UINT8 *p_ndef_msg)
{
    UINT16 random_number;
    UINT8 *p_cr_record, *p_cr_payload;
    UINT32 cr_payload_len;

    CHO_TRACE_DEBUG0 ("nfa_cho_get_random_number ()");

    /* find Collision Resolution record */
    p_cr_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, cr_rec_type, CR_REC_TYPE_LEN);

    if (!p_cr_record)
    {
        CHO_TRACE_ERROR0 ("Failed to find cr record");
        return 0;
    }

    /* get start of payload in Collision Resolution record */
    p_cr_payload = NDEF_RecGetPayload (p_cr_record, &cr_payload_len);

    if ((!p_cr_payload) || (cr_payload_len != 2))
    {
        CHO_TRACE_ERROR0 ("Failed to get cr payload (random number)");
        return 0;
    }

    /* get random number from payload */
    BE_STREAM_TO_UINT16 (random_number, p_cr_payload);

    return random_number;
}

/*******************************************************************************
**
** Function         nfa_cho_parse_ac_records
**
** Description      Parsing NDEF message to retrieve Alternative Carrier records
**                  and store into tNFA_CHO_AC_REC
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_parse_ac_records (UINT8 *p_ndef_msg, UINT8 *p_num_ac_rec, tNFA_CHO_AC_REC *p_ac_rec)
{
    UINT8 *p_ac_record, *p_ac_payload;
    UINT32 ac_payload_len;

    UINT8  xx, yy;

    CHO_TRACE_DEBUG0 ("nfa_cho_parse_ac_records ()");

    /* get Alternative Carrier record */
    p_ac_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, ac_rec_type, AC_REC_TYPE_LEN);

    xx = 0;

    while ((p_ac_record) && (xx < NFA_CHO_MAX_AC_INFO))
    {
        /* get payload */
        p_ac_payload = NDEF_RecGetPayload (p_ac_record, &ac_payload_len);

        if ((!p_ac_payload) || (ac_payload_len < 3))
        {
            CHO_TRACE_ERROR0 ("Failed to get ac payload");
            return NFA_STATUS_FAILED;
        }

        /* Carrier Power State */
        BE_STREAM_TO_UINT8 (p_ac_rec->cps, p_ac_payload);

        /* Carrier Data Reference Length and Characters */
        BE_STREAM_TO_UINT8 (p_ac_rec->carrier_data_ref.ref_len, p_ac_payload);

        ac_payload_len -= 2;

        /* remaining must have carrier data ref and Auxiliary Data Reference Count at least */
        if (ac_payload_len > p_ac_rec->carrier_data_ref.ref_len)
        {
            if (p_ac_rec->carrier_data_ref.ref_len > NFA_CHO_MAX_REF_NAME_LEN)
            {
                CHO_TRACE_ERROR1 ("Too many bytes for carrier_data_ref, ref_len = %d",
                                   p_ac_rec->carrier_data_ref.ref_len);
                return NFA_STATUS_FAILED;
            }

            BE_STREAM_TO_ARRAY (p_ac_payload,
                                p_ac_rec->carrier_data_ref.ref_name,
                                p_ac_rec->carrier_data_ref.ref_len);
            ac_payload_len -= p_ac_rec->carrier_data_ref.ref_len;
        }
        else
        {
            CHO_TRACE_ERROR0 ("Failed to parse carrier_data_ref.ref_len");
            return NFA_STATUS_FAILED;
        }

        /* Auxiliary Data Reference Count */
        BE_STREAM_TO_UINT8 (p_ac_rec->aux_data_ref_count, p_ac_payload);
        ac_payload_len--;

        /* Auxiliary Data Reference Length and Characters */
        for (yy = 0; (yy < p_ac_rec->aux_data_ref_count) && (yy < NFA_CHO_MAX_AUX_DATA_COUNT); yy++)
        {
            if (ac_payload_len > 0)
            {
                BE_STREAM_TO_UINT8 (p_ac_rec->aux_data_ref[yy].ref_len, p_ac_payload);
                ac_payload_len--;

                if (ac_payload_len >= p_ac_rec->aux_data_ref[yy].ref_len)
                {
                    if (p_ac_rec->aux_data_ref[yy].ref_len > NFA_CHO_MAX_REF_NAME_LEN)
                    {
                        CHO_TRACE_ERROR2 ("Too many bytes for aux_data_ref[%d], ref_len=%d",
                                           yy, p_ac_rec->aux_data_ref[yy].ref_len);
                        return NFA_STATUS_FAILED;
                    }

                    BE_STREAM_TO_ARRAY (p_ac_payload,
                                        p_ac_rec->aux_data_ref[yy].ref_name,
                                        p_ac_rec->aux_data_ref[yy].ref_len);
                    ac_payload_len -= p_ac_rec->aux_data_ref[yy].ref_len;
                }
                else
                {
                    CHO_TRACE_ERROR1 ("Failed to parse ref_name for aux_data_ref[%d]", yy);
                    return NFA_STATUS_FAILED;
                }
            }
            else
            {
                CHO_TRACE_ERROR1 ("Failed to parse ref_len for aux_data_ref[%d]", yy);
                return NFA_STATUS_FAILED;
            }
        }

        if (ac_payload_len != 0)
        {
            CHO_TRACE_WARNING1 ("Found extra data in AC record[%d]", xx);
        }

        xx++;
        p_ac_rec++;

        /* get next Alternative Carrier record */
        p_ac_record = NDEF_MsgGetNextRecByType (p_ac_record, NDEF_TNF_WKT, ac_rec_type, AC_REC_TYPE_LEN);
    }

    *p_num_ac_rec = xx;

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_cho_parse_hr_record
**
** Description      Parsing Handover Request message to retrieve version, random number,
**                  Alternative Carrier records and store into tNFA_CHO_AC_REC
**
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_parse_hr_record (UINT8  *p_ndef_msg,
                                     UINT8  *p_version,
                                     UINT16 *p_random_number,
                                     UINT8  *p_num_ac_rec,
                                     tNFA_CHO_AC_REC *p_ac_rec)
{
    UINT8 *p_hr_record, *p_hr_payload;
    UINT32 hr_payload_len;

    CHO_TRACE_DEBUG0 ("nfa_cho_parse_hr_record ()");

    /* get Handover Request record */
    p_hr_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, hr_rec_type, HR_REC_TYPE_LEN);

    if (!p_hr_record)
    {
        CHO_TRACE_ERROR0 ("Failed to find Hr record");
        return NFA_STATUS_FAILED;
    }

    p_hr_payload = NDEF_RecGetPayload (p_hr_record, &hr_payload_len);

    if ((!p_hr_payload) || (hr_payload_len < 7))
    {
        CHO_TRACE_ERROR0 ("Failed to get Hr payload (version, cr/ac record)");
        return NFA_STATUS_FAILED;
    }

    /* Version */
    STREAM_TO_UINT8 ((*p_version), p_hr_payload);
    hr_payload_len--;

    /* NDEF message for Collision Resolution record and Alternative Carrier records */

    if (NDEF_OK != NDEF_MsgValidate (p_hr_payload, hr_payload_len, FALSE))
    {
        CHO_TRACE_ERROR0 ("Failed to validate NDEF message for cr/ac records");
        return NFA_STATUS_FAILED;
    }

    /* Collision Resolution record */
    if (p_random_number)
    {
        *p_random_number = nfa_cho_get_random_number (p_hr_payload);
    }

    /* Alternative Carrier records */
    if (p_ac_rec)
    {
        return (nfa_cho_parse_ac_records (p_hr_payload, p_num_ac_rec, p_ac_rec));
    }

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_cho_parse_carrier_config
**
** Description      Parse Alternative Carrier Configuration and Aux Data
**
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_parse_carrier_config (UINT8 *p_ndef_msg, UINT8 num_ac_rec, tNFA_CHO_AC_REC *p_ac_rec)
{
    UINT8  *p_record;
    UINT8   xx, yy;

    CHO_TRACE_DEBUG1 ("nfa_cho_parse_carrier_config () num_ac_rec = %d", num_ac_rec);

    /* Parse Alternative Carrier Configuration and Aux Data */
    for (xx = 0; xx < num_ac_rec; xx++)
    {
        p_record = NDEF_MsgGetFirstRecById (p_ndef_msg,
                                            p_ac_rec->carrier_data_ref.ref_name,
                                            p_ac_rec->carrier_data_ref.ref_len);

        if (!p_record)
        {
            CHO_TRACE_ERROR2 ("Failed to find Payload ID: len=%d, [0x%x ...]",
                              p_ac_rec->carrier_data_ref.ref_len,
                              p_ac_rec->carrier_data_ref.ref_name[0]);
            return NFA_STATUS_FAILED;
        }

        for (yy = 0; yy < p_ac_rec->aux_data_ref_count; yy++)
        {
            /* Get aux data record by Payload ID */
            p_record = NDEF_MsgGetFirstRecById (p_ndef_msg,
                                                p_ac_rec->aux_data_ref[yy].ref_name,
                                                p_ac_rec->aux_data_ref[yy].ref_len);

            if (!p_record)
            {
                CHO_TRACE_ERROR2 ("Failed to find Payload ID for Aux: len=%d, [0x%x ...]",
                                  p_ac_rec->aux_data_ref[yy].ref_len,
                                  p_ac_rec->aux_data_ref[yy].ref_name[0]);
                return NFA_STATUS_FAILED;
            }
        }

        p_ac_rec++;
    }

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_cho_proc_hr
**
** Description      Parse Handover Request Message
**                  In case of parsing error, let peer got timeout (1 sec).
**
**
** Returns          None
**
*******************************************************************************/
void nfa_cho_proc_hr (UINT32 length, UINT8 *p_ndef_msg)
{
    tNFA_CHO_EVT_DATA    evt_data;
    tNFA_CHO_API_SEND_HS select;
    UINT8  version;
    UINT16 random_number;

    CHO_TRACE_DEBUG1 ("nfa_cho_proc_hr () length=%d", length);

    /* Parse Handover Request record */
    if (NDEF_OK != nfa_cho_parse_hr_record (p_ndef_msg, &version, &random_number,
                                            &evt_data.request.num_ac_rec,
                                            &evt_data.request.ac_rec[0]))
    {
        CHO_TRACE_ERROR0 ("Failed to parse hr record");
        return;
    }

    if (version != NFA_CHO_VERSION)
    {
        CHO_TRACE_DEBUG1 ("Version (0x%02x) not matched", version);
        /* For the future, */
        /* if we have higher major then support peer's version */
        /* if we have lower major then send empty handover select message */
        if (NFA_CHO_GET_MAJOR_VERSION (version) > NFA_CHO_GET_MAJOR_VERSION (NFA_CHO_VERSION))
        {
            select.num_ac_info = 0;
            nfa_cho_send_hs (&select);
            return;
        }
    }

    if (NFA_STATUS_OK != nfa_cho_parse_carrier_config (p_ndef_msg,
                                                       evt_data.request.num_ac_rec,
                                                       &evt_data.request.ac_rec[0]))
    {
        CHO_TRACE_ERROR0 ("Failed to parse carrier configuration");

        evt_data.request.status       = NFA_STATUS_FAILED;
        evt_data.request.num_ac_rec   = 0;
        evt_data.request.p_ref_ndef   = NULL;
        evt_data.request.ref_ndef_len = 0;

        nfa_cho_cb.p_cback (NFA_CHO_REQUEST_EVT, &evt_data);
        return;
    }

    if (evt_data.request.num_ac_rec)
    {
        /* passing alternative carrier references */
        evt_data.request.p_ref_ndef   = p_ndef_msg;
        evt_data.request.ref_ndef_len = length;
    }
    else
    {
        evt_data.request.p_ref_ndef   = NULL;
        evt_data.request.ref_ndef_len = 0;
    }

    evt_data.request.status = NFA_STATUS_OK;

    nfa_cho_cb.p_cback (NFA_CHO_REQUEST_EVT, &evt_data);
}

/*******************************************************************************
**
** Function         nfa_cho_get_error
**
** Description      Search Error record and parse it
**
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_get_error (UINT8 *p_ndef_msg, UINT8 *p_error_reason, UINT32 *p_error_data)
{
    UINT8 *p_err_record, *p_err_payload, u8;
    UINT32 err_payload_len;

    CHO_TRACE_DEBUG0 ("nfa_cho_get_error ()");

    p_err_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, err_rec_type, ERR_REC_TYPE_LEN);

    if (!p_err_record)
    {
        CHO_TRACE_DEBUG0 ("Found no err record");
        return NFA_STATUS_FAILED;
    }

    p_err_payload = NDEF_RecGetPayload (p_err_record, &err_payload_len);

    if (!p_err_payload)
    {
        CHO_TRACE_ERROR0 ("Failed to get err payload");
        return NFA_STATUS_SYNTAX_ERROR;
    }

    BE_STREAM_TO_UINT8 (*p_error_reason, p_err_payload);

    if (  (err_payload_len == 2)
        &&(  (*p_error_reason == NFA_CHO_ERROR_TEMP_MEM)
           ||(*p_error_reason == NFA_CHO_ERROR_CARRIER)  )  )
    {
        BE_STREAM_TO_UINT8 (u8, p_err_payload);
        *p_error_data = (UINT32)u8;
    }
    else if (  (err_payload_len == 5)
             &&(*p_error_reason == NFA_CHO_ERROR_PERM_MEM)  )
    {
        BE_STREAM_TO_UINT32 (*p_error_data, p_err_payload);
    }
    else
    {
        CHO_TRACE_ERROR2 ("Unknown error reason = %d, err_payload_len = %d",
                          *p_error_reason, err_payload_len);
        return NFA_STATUS_SYNTAX_ERROR;
    }

    CHO_TRACE_DEBUG2 ("error_reason=0x%x, error_data=0x%x", *p_error_reason, *p_error_data);

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_cho_parse_hs_record
**
** Description      Parse Handover Select record
**
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_parse_hs_record (UINT8  *p_ndef_msg,
                                     UINT8  *p_version,
                                     UINT8  *p_num_ac_rec,
                                     tNFA_CHO_AC_REC *p_ac_rec,
                                     UINT8  *p_error_reason,
                                     UINT32 *p_error_data)
{
    tNFA_STATUS status;
    UINT8 *p_hs_record, *p_hs_payload;
    UINT32 hs_payload_len;

    CHO_TRACE_DEBUG0 ("nfa_cho_parse_hs_record ()");

    /* get Handover Select record */
    p_hs_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, hs_rec_type, HS_REC_TYPE_LEN);

    if (!p_hs_record)
    {
        CHO_TRACE_ERROR0 ("Failed to find Hs record");
        return NFA_STATUS_FAILED;
    }

    p_hs_payload = NDEF_RecGetPayload (p_hs_record, &hs_payload_len);

    if ((!p_hs_payload) || (hs_payload_len < 1))  /* at least version */
    {
        CHO_TRACE_ERROR0 ("Failed to get Hs payload (version, ac record)");
        return NFA_STATUS_FAILED;
    }

    STREAM_TO_UINT8 ((*p_version), p_hs_payload);
    hs_payload_len--;

    /* Check if error record is sent */
    status = nfa_cho_get_error (p_ndef_msg, p_error_reason, p_error_data);

    if (status == NFA_STATUS_SYNTAX_ERROR)
    {
        return NFA_STATUS_FAILED;
    }
    else if (status == NFA_STATUS_OK)
    {
        return NFA_STATUS_OK;
    }

    if (hs_payload_len >= 3 )
    {
        /* NDEF message for Alternative Carrier records */
        if (NDEF_OK != NDEF_MsgValidate (p_hs_payload, hs_payload_len, FALSE))
        {
            CHO_TRACE_ERROR0 ("Failed to validate NDEF message for ac records");
            return NFA_STATUS_FAILED;
        }

        /* Alternative Carrier records */
        if (p_ac_rec)
        {
            if (NFA_STATUS_OK != nfa_cho_parse_ac_records (p_hs_payload, p_num_ac_rec, p_ac_rec))
            {
                return NFA_STATUS_FAILED;
            }

            CHO_TRACE_DEBUG1 ("Found %d ac record", *p_num_ac_rec);
        }
    }
    else
    {
        CHO_TRACE_DEBUG0 ("Empty Handover Select Message");
        *p_num_ac_rec = 0;
    }

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function         nfa_cho_proc_hs
**
** Description      Parse Handover Select Message
**
**
** Returns          FALSE if we need to select one from inactive ACs
**
*******************************************************************************/
void nfa_cho_proc_hs (UINT32 length, UINT8 *p_ndef_msg)
{
    tNFA_CHO_EVT_DATA evt_data;
    UINT8  version, error_reason = 0;
    UINT32 error_data;

    CHO_TRACE_DEBUG0 ("nfa_cho_proc_hs ()");

    evt_data.select.status = NFA_STATUS_OK;

    /* Parse Handover Select record */
    if (NFA_STATUS_OK != nfa_cho_parse_hs_record (p_ndef_msg, &version,
                                                  &evt_data.select.num_ac_rec,
                                                  &evt_data.select.ac_rec[0],
                                                  &error_reason, &error_data))
    {
        CHO_TRACE_ERROR0 ("Failed to parse hs record");

        evt_data.select.status = NFA_STATUS_FAILED;
    }

    if (  (evt_data.select.status == NFA_STATUS_OK)
        &&(error_reason != 0)  )
    {
        /* We got error records */
        evt_data.sel_err.error_reason = error_reason;
        evt_data.sel_err.error_data   = error_data;

        nfa_cho_cb.p_cback (NFA_CHO_SEL_ERR_EVT, &evt_data);
        return;
    }

    if (  (evt_data.select.status == NFA_STATUS_OK)
        &&(version != NFA_CHO_VERSION)  )
    {
        CHO_TRACE_ERROR1 ("Version (0x%02x) not matched", version);

        evt_data.select.status = NFA_STATUS_FAILED;
    }

    /* parse Alternative Carrier information */

    if (  (evt_data.select.status == NFA_STATUS_OK)
        &&(NFA_STATUS_OK != nfa_cho_parse_carrier_config (p_ndef_msg,
                                                          evt_data.select.num_ac_rec,
                                                          &evt_data.select.ac_rec[0]))  )
    {
        CHO_TRACE_ERROR0 ("Failed to parse carrier configuration");

        evt_data.select.status = NFA_STATUS_FAILED;
    }

    if (evt_data.select.status == NFA_STATUS_OK)
    {
        if (evt_data.select.num_ac_rec)
        {
            /* passing alternative carrier references */
            evt_data.select.p_ref_ndef   = NDEF_MsgGetNextRec (p_ndef_msg);
            *evt_data.select.p_ref_ndef |= NDEF_MB_MASK;
            evt_data.select.ref_ndef_len = (UINT32)(length - (evt_data.select.p_ref_ndef - p_ndef_msg));
        }
        else
        {
            evt_data.select.p_ref_ndef   = NULL;
            evt_data.select.ref_ndef_len = 0;
        }
    }
    else
    {
        evt_data.select.num_ac_rec   = 0;
        evt_data.select.p_ref_ndef   = NULL;
        evt_data.select.ref_ndef_len = 0;
    }

    nfa_cho_cb.p_cback (NFA_CHO_SELECT_EVT, &evt_data);
}

/*******************************************************************************
**
** Function         nfa_cho_proc_simplified_format
**
** Description      Parse simplified BT OOB/Wifi Message
**
**
** Returns          void
**
*******************************************************************************/
void nfa_cho_proc_simplified_format (UINT32 length, UINT8 *p_ndef_msg)
{
    tNFA_CHO_EVT_DATA evt_data;

    CHO_TRACE_DEBUG0 ("nfa_cho_proc_simplified_format ()");

    evt_data.select.status = NFA_STATUS_OK;

    evt_data.select.num_ac_rec = 1;

    evt_data.select.ac_rec[0].cps = NFA_CHO_CPS_UNKNOWN;
    evt_data.select.ac_rec[0].carrier_data_ref.ref_len = 0;
    evt_data.select.ac_rec[0].aux_data_ref_count       = 0;

    evt_data.select.p_ref_ndef   = p_ndef_msg;
    evt_data.select.ref_ndef_len = length;

    nfa_cho_cb.p_cback (NFA_CHO_SELECT_EVT, &evt_data);
}

/*******************************************************************************
**
** Function         nfa_cho_get_msg_type
**
** Description      Get handover message type to check collision
**
**
** Returns          NFA_CHO_MSG_HR if it has Handover Request record
**                  NFA_CHO_MSG_HS if it has Handover Select record
**                  NFA_CHO_MSG_BT_OOB if it has simplified BT OOB record
**                  NFA_CHO_MSG_WIFI if it has simplified WiFi record
**                  NFA_CHO_MSG_UNKNOWN, otherwise
**
*******************************************************************************/
tNFA_CHO_MSG_TYPE nfa_cho_get_msg_type (UINT32 length, UINT8 *p_ndef_msg)
{
    UINT8 *p_record;

    CHO_TRACE_DEBUG1 ("nfa_cho_get_msg_type () length=%d", length);

    p_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, hr_rec_type, HR_REC_TYPE_LEN);

    if (p_record)
    {
        CHO_TRACE_DEBUG0 ("Found Hr record");
        return NFA_CHO_MSG_HR;
    }

    p_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, hs_rec_type, HS_REC_TYPE_LEN);

    if (p_record)
    {
        CHO_TRACE_DEBUG0 ("Found Hs record");
        return NFA_CHO_MSG_HS;
    }

    p_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_MEDIA,
                                          p_bt_oob_rec_type, BT_OOB_REC_TYPE_LEN);

    if (p_record)
    {
        CHO_TRACE_DEBUG0 ("Found simplified BT OOB record");
        return NFA_CHO_MSG_BT_OOB;
    }

    p_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_MEDIA,
                                          p_wifi_rec_type, (UINT8) strlen ((char *) p_wifi_rec_type));

    if (p_record)
    {
        CHO_TRACE_DEBUG0 ("Found simplified WiFi record");
        return NFA_CHO_MSG_WIFI;
    }

    CHO_TRACE_ERROR0 ("Failed to find Hr/Hs record");

    return NFA_CHO_MSG_UNKNOWN;
}

/*******************************************************************************
**
** Function         nfa_cho_get_local_device_role
**
** Description      Resolve collision and get role of local device
**
**
** Returns          tNFA_CHO_ROLE_TYPE
**
*******************************************************************************/
tNFA_CHO_ROLE_TYPE nfa_cho_get_local_device_role (UINT32 length, UINT8 *p_ndef_msg)
{
    UINT16 rx_random_number;
    UINT8  version;

    CHO_TRACE_DEBUG1 ("nfa_cho_get_local_device_role () length=%d", length);

    /* Get random number in Handover Request record */
    if (NDEF_OK != nfa_cho_parse_hr_record (p_ndef_msg, &version, &rx_random_number, NULL, NULL))
    {
        CHO_TRACE_ERROR0 ("Failed to parse hr record");
        return NFA_CHO_ROLE_UNDECIDED;
    }

    CHO_TRACE_DEBUG2 ("tx_random_number=0x%x, rx_random_number=0x%x",
                       nfa_cho_cb.tx_random_number, rx_random_number);

    if (nfa_cho_cb.tx_random_number == rx_random_number)
    {
        return NFA_CHO_ROLE_UNDECIDED;
    }
    /* if the least significant bits are same */
    else if (((nfa_cho_cb.tx_random_number ^ rx_random_number) & 0x0001) == 0)
    {
        if (nfa_cho_cb.tx_random_number > rx_random_number)
            return NFA_CHO_ROLE_SELECTOR;
        else
            return NFA_CHO_ROLE_REQUESTER;
    }
    else
    {
        if (nfa_cho_cb.tx_random_number > rx_random_number)
            return NFA_CHO_ROLE_REQUESTER;
        else
            return NFA_CHO_ROLE_SELECTOR;
    }
}

/*******************************************************************************
**
** Function         nfa_cho_update_random_number
**
** Description      Replace random number
**
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_cho_update_random_number (UINT8 *p_ndef_msg)
{
    UINT8 *p_hr_record, *p_hr_payload;
    UINT8 *p_cr_record, *p_cr_payload;
    UINT32 hr_payload_len, cr_payload_len;
    UINT32 temp32;

    CHO_TRACE_DEBUG0 ("nfa_cho_update_random_number ()");

    /* get Handover Request record */
    p_hr_record = NDEF_MsgGetFirstRecByType (p_ndef_msg, NDEF_TNF_WKT, hr_rec_type, HR_REC_TYPE_LEN);

    if (!p_hr_record)
    {
        CHO_TRACE_ERROR0 ("Failed to find Hr record");
        return NFA_STATUS_FAILED;
    }

    p_hr_payload = NDEF_RecGetPayload (p_hr_record, &hr_payload_len);

    /* Skip Version */
    p_hr_payload++;
    hr_payload_len--;

    /* NDEF message for Collision Resolution record and Alternative Carrier records */

    /* find Collision Resolution record */
    p_cr_record = NDEF_MsgGetFirstRecByType (p_hr_payload, NDEF_TNF_WKT, cr_rec_type, CR_REC_TYPE_LEN);

    if (!p_cr_record)
    {
        CHO_TRACE_ERROR0 ("Failed to find cr record");
        return NFA_STATUS_FAILED;
    }

    /* get start of payload in Collision Resolution record */
    p_cr_payload = NDEF_RecGetPayload (p_cr_record, &cr_payload_len);

    /* Get random number from timer */
    temp32 = GKI_get_tick_count ();
    nfa_cho_cb.tx_random_number = (UINT16) ((temp32 >> 16) ^ (temp32));

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
    if (nfa_cho_cb.test_enabled & NFA_CHO_TEST_RANDOM)
    {
        nfa_cho_cb.tx_random_number = nfa_cho_cb.test_random_number;
    }
#endif

    CHO_TRACE_DEBUG1 ("tx_random_number = 0x%04x", nfa_cho_cb.tx_random_number);

    /* update random number in payload */
    UINT16_TO_BE_STREAM (p_cr_payload, nfa_cho_cb.tx_random_number);

    return NFA_STATUS_OK;
}
