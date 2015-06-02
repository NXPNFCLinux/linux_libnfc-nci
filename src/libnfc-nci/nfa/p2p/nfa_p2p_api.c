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
 *  NFA interface to LLCP
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "llcp_defs.h"
#include "llcp_api.h"
#include "nfa_p2p_api.h"
#include "nfa_p2p_int.h"

/*****************************************************************************
**  Constants
*****************************************************************************/

/*******************************************************************************
**
** Function         NFA_P2pRegisterServer
**
** Description      This function is called to listen to a SAP as server on LLCP.
**
**                  NFA_P2P_REG_SERVER_EVT will be returned with status and handle.
**
**                  If server_sap is set to NFA_P2P_ANY_SAP, then NFA will allocate
**                  a SAP between LLCP_LOWER_BOUND_SDP_SAP and LLCP_UPPER_BOUND_SDP_SAP
**                  Otherwise, server_sap must be between (LLCP_SDP_SAP + 1) and
**                  LLCP_UPPER_BOUND_SDP_SAP
**
**                  link_type : NFA_P2P_LLINK_TYPE and/or NFA_P2P_DLINK_TYPE
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pRegisterServer (UINT8              server_sap,
                                   tNFA_P2P_LINK_TYPE link_type,
                                   char               *p_service_name,
                                   tNFA_P2P_CBACK     *p_cback)
{
    tNFA_P2P_API_REG_SERVER *p_msg;

    P2P_TRACE_API3 ("NFA_P2pRegisterServer (): server_sap:0x%02x, link_type:0x%x, SN:<%s>",
                     server_sap, link_type, p_service_name);

    if (  (server_sap != NFA_P2P_ANY_SAP)
        &&((server_sap <= LLCP_SAP_SDP) ||(server_sap > LLCP_UPPER_BOUND_SDP_SAP))  )
    {
        P2P_TRACE_ERROR2 ("NFA_P2pRegisterServer (): server_sap must be between %d and %d",
                          LLCP_SAP_SDP + 1, LLCP_UPPER_BOUND_SDP_SAP);
        return (NFA_STATUS_FAILED);
    }
    else if (  ((link_type & NFA_P2P_LLINK_TYPE) == 0x00)
             &&((link_type & NFA_P2P_DLINK_TYPE) == 0x00)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pRegisterServer(): link type (0x%x) must be specified", link_type);
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_P2P_API_REG_SERVER *) GKI_getbuf (sizeof (tNFA_P2P_API_REG_SERVER))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_REG_SERVER_EVT;

        p_msg->server_sap = server_sap;
        p_msg->link_type  = link_type;

        BCM_STRNCPY_S (p_msg->service_name, sizeof (p_msg->service_name), p_service_name, LLCP_MAX_SN_LEN);
        p_msg->service_name[LLCP_MAX_SN_LEN] = 0;

        p_msg->p_cback = p_cback;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pRegisterClient
**
** Description      This function is called to register a client service on LLCP.
**
**                  NFA_P2P_REG_CLIENT_EVT will be returned with status and handle.
**
**                  link_type : NFA_P2P_LLINK_TYPE and/or NFA_P2P_DLINK_TYPE
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pRegisterClient (tNFA_P2P_LINK_TYPE link_type,
                                   tNFA_P2P_CBACK     *p_cback)
{
    tNFA_P2P_API_REG_CLIENT *p_msg;

    P2P_TRACE_API1 ("NFA_P2pRegisterClient (): link_type:0x%x", link_type);

    if (  ((link_type & NFA_P2P_LLINK_TYPE) == 0x00)
        &&((link_type & NFA_P2P_DLINK_TYPE) == 0x00)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pRegisterClient (): link type (0x%x) must be specified", link_type);
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_P2P_API_REG_CLIENT *) GKI_getbuf (sizeof (tNFA_P2P_API_REG_CLIENT))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_REG_CLIENT_EVT;

        p_msg->p_cback   = p_cback;
        p_msg->link_type = link_type;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pDeregister
**
** Description      This function is called to stop listening to a SAP as server
**                  or stop client service on LLCP.
**
** Note:            If this function is called to de-register a server and RF discovery
**                  is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pDeregister (tNFA_HANDLE handle)
{
    tNFA_P2P_API_DEREG *p_msg;
    tNFA_HANDLE         xx;

    P2P_TRACE_API1 ("NFA_P2pDeregister (): handle:0x%02X", handle);

    xx = handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pDeregister (): Handle is invalid or not registered");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_msg = (tNFA_P2P_API_DEREG *) GKI_getbuf (sizeof (tNFA_P2P_API_DEREG))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_DEREG_EVT;

        p_msg->handle    = handle;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pAcceptConn
**
** Description      This function is called to accept a request of data link
**                  connection to a listening SAP on LLCP after receiving
**                  NFA_P2P_CONN_REQ_EVT.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pAcceptConn (tNFA_HANDLE handle,
                               UINT16      miu,
                               UINT8       rw)
{
    tNFA_P2P_API_ACCEPT_CONN *p_msg;
    tNFA_HANDLE               xx;

    P2P_TRACE_API3 ("NFA_P2pAcceptConn (): handle:0x%02X, MIU:%d, RW:%d", handle, miu, rw);

    xx = handle & NFA_HANDLE_MASK;

    if (!(xx & NFA_P2P_HANDLE_FLAG_CONN))
    {
        P2P_TRACE_ERROR0 ("NFA_P2pAcceptConn (): Connection Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }
    else
    {
        xx &= ~NFA_P2P_HANDLE_FLAG_CONN;
    }

    if (  (xx >= LLCP_MAX_DATA_LINK)
        ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pAcceptConn (): Connection Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((miu < LLCP_DEFAULT_MIU) || (nfa_p2p_cb.local_link_miu < miu))
    {
        P2P_TRACE_ERROR3 ("NFA_P2pAcceptConn (): MIU(%d) must be between %d and %d",
                            miu, LLCP_DEFAULT_MIU, nfa_p2p_cb.local_link_miu);
    }
    else if ((p_msg = (tNFA_P2P_API_ACCEPT_CONN *) GKI_getbuf (sizeof (tNFA_P2P_API_ACCEPT_CONN))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_ACCEPT_CONN_EVT;

        p_msg->conn_handle  = handle;
        p_msg->miu          = miu;
        p_msg->rw           = rw;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pRejectConn
**
** Description      This function is called to reject a request of data link
**                  connection to a listening SAP on LLCP after receiving
**                  NFA_P2P_CONN_REQ_EVT.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pRejectConn (tNFA_HANDLE handle)
{
    tNFA_P2P_API_REJECT_CONN *p_msg;
    tNFA_HANDLE               xx;

    P2P_TRACE_API1 ("NFA_P2pRejectConn (): handle:0x%02X", handle);

    xx = handle & NFA_HANDLE_MASK;

    if (!(xx & NFA_P2P_HANDLE_FLAG_CONN))
    {
        P2P_TRACE_ERROR0 ("NFA_P2pRejectConn (): Connection Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }
    else
    {
        xx &= ~NFA_P2P_HANDLE_FLAG_CONN;
    }

    if (  (xx >= LLCP_MAX_DATA_LINK)
        ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pRejectConn (): Connection Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_msg = (tNFA_P2P_API_REJECT_CONN *) GKI_getbuf (sizeof (tNFA_P2P_API_REJECT_CONN))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_REJECT_CONN_EVT;

        p_msg->conn_handle  = handle;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pDisconnect
**
** Description      This function is called to disconnect an existing or
**                  connecting data link connection.
**
**                  discard any pending data on data link connection if flush is set to TRUE
**
**                  NFA_P2P_DISC_EVT will be returned after data link connection is disconnected
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pDisconnect (tNFA_HANDLE handle, BOOLEAN flush)
{
    tNFA_P2P_API_DISCONNECT *p_msg;
    tNFA_HANDLE              xx;

    P2P_TRACE_API2 ("NFA_P2pDisconnect (): handle:0x%02X, flush=%d", handle, flush);

    xx = handle & NFA_HANDLE_MASK;

    if (xx & NFA_P2P_HANDLE_FLAG_CONN)
    {
        xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

        if (  (xx >= LLCP_MAX_DATA_LINK)
            ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
        {
            P2P_TRACE_ERROR0 ("NFA_P2pDisconnect (): Connection Handle is not valid");
            return (NFA_STATUS_BAD_HANDLE);
        }
    }
    else
    {
        P2P_TRACE_ERROR0 ("NFA_P2pDisconnect (): Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_msg = (tNFA_P2P_API_DISCONNECT *) GKI_getbuf (sizeof (tNFA_P2P_API_DISCONNECT))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_DISCONNECT_EVT;

        p_msg->conn_handle  = handle;
        p_msg->flush        = flush;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pConnectByName
**
** Description      This function is called to create a connection-oriented transport
**                  by a service name.
**                  NFA_P2P_CONNECTED_EVT if success
**                  NFA_P2P_DISC_EVT if failed
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if client is not registered
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pConnectByName (tNFA_HANDLE client_handle,
                                  char        *p_service_name,
                                  UINT16      miu,
                                  UINT8       rw)
{
    tNFA_P2P_API_CONNECT *p_msg;
    tNFA_HANDLE           xx;

    P2P_TRACE_API4 ("NFA_P2pConnectByName (): client_handle:0x%x, SN:<%s>, MIU:%d, RW:%d",
                    client_handle, p_service_name, miu, rw);

    xx = client_handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pConnectByName (): Client Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if (  (miu < LLCP_DEFAULT_MIU)
        ||(nfa_p2p_cb.llcp_state != NFA_P2P_LLCP_STATE_ACTIVATED)
        ||(nfa_p2p_cb.local_link_miu < miu)  )
    {
        P2P_TRACE_ERROR3 ("NFA_P2pConnectByName (): MIU(%d) must be between %d and %d or LLCP link is not activated",
                            miu, LLCP_DEFAULT_MIU, nfa_p2p_cb.local_link_miu);
    }
    else if ((p_msg = (tNFA_P2P_API_CONNECT *) GKI_getbuf (sizeof (tNFA_P2P_API_CONNECT))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_CONNECT_EVT;

        BCM_STRNCPY_S (p_msg->service_name, sizeof (p_msg->service_name), p_service_name, LLCP_MAX_SN_LEN);
        p_msg->service_name[LLCP_MAX_SN_LEN] = 0;

        p_msg->dsap    = LLCP_INVALID_SAP;
        p_msg->miu     = miu;
        p_msg->rw      = rw;
        p_msg->client_handle = client_handle;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pConnectBySap
**
** Description      This function is called to create a connection-oriented transport
**                  by a SAP.
**                  NFA_P2P_CONNECTED_EVT if success
**                  NFA_P2P_DISC_EVT if failed
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if client is not registered
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pConnectBySap (tNFA_HANDLE client_handle,
                                 UINT8       dsap,
                                 UINT16      miu,
                                 UINT8       rw)
{
    tNFA_P2P_API_CONNECT *p_msg;
    tNFA_HANDLE           xx;

    P2P_TRACE_API4 ("NFA_P2pConnectBySap (): client_handle:0x%x, DSAP:0x%02X, MIU:%d, RW:%d",
                    client_handle, dsap, miu, rw);

    xx = client_handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pConnectBySap (): Client Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if (  (miu < LLCP_DEFAULT_MIU)
        ||(nfa_p2p_cb.llcp_state != NFA_P2P_LLCP_STATE_ACTIVATED)
        ||(nfa_p2p_cb.local_link_miu < miu)  )
    {
        P2P_TRACE_ERROR3 ("NFA_P2pConnectBySap (): MIU(%d) must be between %d and %d, or LLCP link is not activated",
                            miu, LLCP_DEFAULT_MIU, nfa_p2p_cb.local_link_miu);
    }
    else if ((p_msg = (tNFA_P2P_API_CONNECT *) GKI_getbuf (sizeof (tNFA_P2P_API_CONNECT))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_CONNECT_EVT;

        p_msg->service_name[LLCP_MAX_SN_LEN] = 0;

        p_msg->dsap    = dsap;
        p_msg->miu     = miu;
        p_msg->rw      = rw;
        p_msg->client_handle = client_handle;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pSendUI
**
** Description      This function is called to send data on connectionless
**                  transport.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_BAD_LENGTH if data length is more than remote link MIU
**                  NFA_STATUS_CONGESTED  if congested
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pSendUI (tNFA_HANDLE handle,
                           UINT8       dsap,
                           UINT16      length,
                           UINT8      *p_data)
{
    tNFA_P2P_API_SEND_UI *p_msg;
    tNFA_STATUS           ret_status = NFA_STATUS_FAILED;
    tNFA_HANDLE           xx;

    P2P_TRACE_API3 ("NFA_P2pSendUI (): handle:0x%X, DSAP:0x%02X, length:%d", handle, dsap, length);

    GKI_sched_lock ();

    xx = handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL))
    {
        P2P_TRACE_ERROR1 ("NFA_P2pSendUI (): Handle (0x%X) is not valid", handle);
        ret_status = NFA_STATUS_BAD_HANDLE;
    }
    else if (length > nfa_p2p_cb.remote_link_miu)
    {
        P2P_TRACE_ERROR3 ("NFA_P2pSendUI (): handle:0x%X, length(%d) must be less than remote link MIU(%d)",
                           handle, length, nfa_p2p_cb.remote_link_miu);
        ret_status = NFA_STATUS_BAD_LENGTH;
    }
    else if (nfa_p2p_cb.sap_cb[xx].flags & NFA_P2P_SAP_FLAG_LLINK_CONGESTED)
    {
        P2P_TRACE_WARNING1 ("NFA_P2pSendUI (): handle:0x%X, logical data link is already congested",
                             handle);
        ret_status = NFA_STATUS_CONGESTED;
    }
    else if (LLCP_IsLogicalLinkCongested ((UINT8)xx,
                                          nfa_p2p_cb.sap_cb[xx].num_pending_ui_pdu,
                                          nfa_p2p_cb.total_pending_ui_pdu,
                                          nfa_p2p_cb.total_pending_i_pdu))
    {
        nfa_p2p_cb.sap_cb[xx].flags |= NFA_P2P_SAP_FLAG_LLINK_CONGESTED;

        P2P_TRACE_WARNING1 ("NFA_P2pSendUI(): handle:0x%X, logical data link is congested",
                             handle);
        ret_status = NFA_STATUS_CONGESTED;
    }
    else if ((p_msg = (tNFA_P2P_API_SEND_UI *) GKI_getbuf (sizeof(tNFA_P2P_API_SEND_UI))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_SEND_UI_EVT;

        p_msg->handle  = handle;
        p_msg->dsap    = dsap;

        if ((p_msg->p_msg = (BT_HDR *) GKI_getpoolbuf (LLCP_POOL_ID)) != NULL)
        {
            p_msg->p_msg->len    = length;
            p_msg->p_msg->offset = LLCP_MIN_OFFSET;
            memcpy (((UINT8*) (p_msg->p_msg + 1) + p_msg->p_msg->offset), p_data, length);

            /* increase number of tx UI PDU which is not processed by NFA for congestion control */
            nfa_p2p_cb.sap_cb[xx].num_pending_ui_pdu++;
            nfa_p2p_cb.total_pending_ui_pdu++;
            nfa_sys_sendmsg (p_msg);

            ret_status = NFA_STATUS_OK;
        }
        else
        {
            GKI_freebuf (p_msg);

            nfa_p2p_cb.sap_cb[xx].flags |= NFA_P2P_SAP_FLAG_LLINK_CONGESTED;
            ret_status = NFA_STATUS_CONGESTED;
        }
    }

    GKI_sched_unlock ();

    return (ret_status);
}

/*******************************************************************************
**
** Function         NFA_P2pReadUI
**
** Description      This function is called to read data on connectionless
**                  transport when receiving NFA_P2P_DATA_EVT with NFA_P2P_LLINK_TYPE.
**
**                  - Remote SAP who sent UI PDU is returned.
**                  - Information of UI PDU up to max_data_len is copied into p_data.
**                  - If more information of UI PDU or more UI PDU in queue then more
**                    is returned to TRUE.
**                  - Information of next UI PDU is not concatenated.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**
*******************************************************************************/
tNFA_STATUS NFA_P2pReadUI (tNFA_HANDLE handle,
                           UINT32      max_data_len,
                           UINT8       *p_remote_sap,
                           UINT32      *p_data_len,
                           UINT8       *p_data,
                           BOOLEAN     *p_more)
{
    tNFA_STATUS ret_status;
    tNFA_HANDLE xx;

    P2P_TRACE_API1 ("NFA_P2pReadUI (): handle:0x%X", handle);

    GKI_sched_lock ();

    xx = handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pReadUI (): Handle (0x%X) is not valid", handle);
        ret_status = NFA_STATUS_BAD_HANDLE;
    }
    else
    {
        *p_more = LLCP_ReadLogicalLinkData ((UINT8)xx,
                                            max_data_len,
                                            p_remote_sap,
                                            p_data_len,
                                            p_data);
        ret_status = NFA_STATUS_OK;
    }

    GKI_sched_unlock ();

    return (ret_status);
}

/*******************************************************************************
**
** Function         NFA_P2pFlushUI
**
** Description      This function is called to flush data on connectionless
**                  transport.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**
*******************************************************************************/
tNFA_STATUS NFA_P2pFlushUI (tNFA_HANDLE handle,
                            UINT32      *p_length)
{
    tNFA_STATUS ret_status;
    tNFA_HANDLE xx;

    P2P_TRACE_API1 ("NFA_P2pReadUI (): handle:0x%X", handle);

    GKI_sched_lock ();

    xx = handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pFlushUI (): Handle (0x%X) is not valid", handle);
        ret_status = NFA_STATUS_BAD_HANDLE;
        *p_length  = 0;
    }
    else
    {
        *p_length  = LLCP_FlushLogicalLinkRxData ((UINT8)xx);
        ret_status = NFA_STATUS_OK;
    }

    GKI_sched_unlock ();

    return (ret_status);
}

/*******************************************************************************
**
** Function         NFA_P2pSendData
**
** Description      This function is called to send data on connection-oriented
**                  transport.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_BAD_LENGTH if data length is more than remote MIU
**                  NFA_STATUS_CONGESTED  if congested
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pSendData (tNFA_HANDLE handle,
                             UINT16      length,
                             UINT8      *p_data)
{
    tNFA_P2P_API_SEND_DATA *p_msg;
    tNFA_STATUS            ret_status = NFA_STATUS_FAILED;
    tNFA_HANDLE            xx;

    P2P_TRACE_API2 ("NFA_P2pSendData (): handle:0x%X, length:%d", handle, length);

    GKI_sched_lock ();

    xx = handle & NFA_HANDLE_MASK;
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    if (  (!(handle & NFA_P2P_HANDLE_FLAG_CONN))
        ||(xx >= LLCP_MAX_DATA_LINK)
        ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pSendData (): Handle(0x%X) is not valid", handle);
        ret_status = NFA_STATUS_BAD_HANDLE;
    }
    else if (nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_REMOTE_RW_ZERO)
    {
        P2P_TRACE_ERROR1 ("NFA_P2pSendData (): handle:0x%X, Remote set RW to 0 (flow off)", handle);
        ret_status = NFA_STATUS_FAILED;
    }
    else if (nfa_p2p_cb.conn_cb[xx].remote_miu < length)
    {
        P2P_TRACE_ERROR2 ("NFA_P2pSendData (): handle:0x%X, Data more than remote MIU(%d)",
                           handle, nfa_p2p_cb.conn_cb[xx].remote_miu);
        ret_status = NFA_STATUS_BAD_LENGTH;
    }
    else if (nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_CONGESTED)
    {
        P2P_TRACE_WARNING1 ("NFA_P2pSendData (): handle:0x%X, data link connection is already congested",
                            handle);
        ret_status = NFA_STATUS_CONGESTED;
    }
    else if (LLCP_IsDataLinkCongested (nfa_p2p_cb.conn_cb[xx].local_sap,
                                       nfa_p2p_cb.conn_cb[xx].remote_sap,
                                       nfa_p2p_cb.conn_cb[xx].num_pending_i_pdu,
                                       nfa_p2p_cb.total_pending_ui_pdu,
                                       nfa_p2p_cb.total_pending_i_pdu))
    {
        nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_CONGESTED;

        P2P_TRACE_WARNING1 ("NFA_P2pSendData (): handle:0x%X, data link connection is congested",
                            handle);
        ret_status = NFA_STATUS_CONGESTED;
    }
    else if ((p_msg = (tNFA_P2P_API_SEND_DATA *) GKI_getbuf (sizeof(tNFA_P2P_API_SEND_DATA))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_SEND_DATA_EVT;

        p_msg->conn_handle  = handle;

        if ((p_msg->p_msg = (BT_HDR *) GKI_getpoolbuf (LLCP_POOL_ID)) != NULL)
        {
            p_msg->p_msg->len    = length;
            p_msg->p_msg->offset = LLCP_MIN_OFFSET;
            memcpy (((UINT8*) (p_msg->p_msg + 1) + p_msg->p_msg->offset), p_data, length);

            /* increase number of tx I PDU which is not processed by NFA for congestion control */
            nfa_p2p_cb.conn_cb[xx].num_pending_i_pdu++;
            nfa_p2p_cb.total_pending_i_pdu++;
            nfa_sys_sendmsg (p_msg);

            ret_status = NFA_STATUS_OK;
        }
        else
        {
            GKI_freebuf (p_msg);
            nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_CONGESTED;
            ret_status = NFA_STATUS_CONGESTED;
        }
    }

    GKI_sched_unlock ();

    return (ret_status);
}

/*******************************************************************************
**
** Function         NFA_P2pReadData
**
** Description      This function is called to read data on connection-oriented
**                  transport when receiving NFA_P2P_DATA_EVT with NFA_P2P_DLINK_TYPE.
**
**                  - Information of I PDU is copied into p_data up to max_data_len.
**                  - If more information of I PDU or more I PDU in queue, then more
**                    is returned to TRUE.
**                  - Information of next I PDU is not concatenated.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**
*******************************************************************************/
tNFA_STATUS NFA_P2pReadData (tNFA_HANDLE handle,
                             UINT32      max_data_len,
                             UINT32      *p_data_len,
                             UINT8       *p_data,
                             BOOLEAN     *p_more)
{
    tNFA_STATUS ret_status;
    tNFA_HANDLE xx;

    P2P_TRACE_API1 ("NFA_P2pReadData (): handle:0x%X", handle);

    GKI_sched_lock ();

    xx = handle & NFA_HANDLE_MASK;
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    if (  (!(handle & NFA_P2P_HANDLE_FLAG_CONN))
        ||(xx >= LLCP_MAX_DATA_LINK)
        ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pReadData (): Handle(0x%X) is not valid", handle);
        ret_status = NFA_STATUS_BAD_HANDLE;
    }
    else
    {
        *p_more = LLCP_ReadDataLinkData (nfa_p2p_cb.conn_cb[xx].local_sap,
                                         nfa_p2p_cb.conn_cb[xx].remote_sap,
                                         max_data_len,
                                         p_data_len,
                                         p_data);
        ret_status = NFA_STATUS_OK;
    }

    GKI_sched_unlock ();

    return (ret_status);
}

/*******************************************************************************
**
** Function         NFA_P2pFlushData
**
** Description      This function is called to flush data on connection-oriented
**                  transport.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**
*******************************************************************************/
tNFA_STATUS NFA_P2pFlushData (tNFA_HANDLE handle,
                              UINT32      *p_length)
{
    tNFA_STATUS ret_status;
    tNFA_HANDLE xx;

    P2P_TRACE_API1 ("NFA_P2pFlushData (): handle:0x%X", handle);

    GKI_sched_lock ();

    xx = handle & NFA_HANDLE_MASK;
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    if (  (!(handle & NFA_P2P_HANDLE_FLAG_CONN))
        ||(xx >= LLCP_MAX_DATA_LINK)
        ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
    {
        P2P_TRACE_ERROR1 ("NFA_P2pFlushData (): Handle(0x%X) is not valid", handle);
        ret_status = NFA_STATUS_BAD_HANDLE;
    }
    else
    {
        *p_length = LLCP_FlushDataLinkRxData (nfa_p2p_cb.conn_cb[xx].local_sap,
                                              nfa_p2p_cb.conn_cb[xx].remote_sap);
        ret_status = NFA_STATUS_OK;
    }

    GKI_sched_unlock ();

    return (ret_status);
}

/*******************************************************************************
**
** Function         NFA_P2pSetLocalBusy
**
** Description      This function is called to stop or resume incoming data on
**                  connection-oriented transport.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if handle is not valid
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pSetLocalBusy (tNFA_HANDLE conn_handle,
                                 BOOLEAN     is_busy)
{
    tNFA_P2P_API_SET_LOCAL_BUSY *p_msg;
    tNFA_HANDLE                  xx;

    P2P_TRACE_API2 ("NFA_P2pSetLocalBusy (): conn_handle:0x%02X, is_busy:%d", conn_handle, is_busy);

    xx = conn_handle & NFA_HANDLE_MASK;

    if (!(xx & NFA_P2P_HANDLE_FLAG_CONN))
    {
        P2P_TRACE_ERROR0 ("NFA_P2pSetLocalBusy (): Connection Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }
    else
    {
        xx &= ~NFA_P2P_HANDLE_FLAG_CONN;
    }

    if (  (xx >= LLCP_MAX_DATA_LINK)
        ||(nfa_p2p_cb.conn_cb[xx].flags == 0)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pSetLocalBusy (): Connection Handle is not valid");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_msg = (tNFA_P2P_API_SET_LOCAL_BUSY *) GKI_getbuf (sizeof (tNFA_P2P_API_SET_LOCAL_BUSY))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_SET_LOCAL_BUSY_EVT;

        p_msg->conn_handle = conn_handle;
        p_msg->is_busy     = is_busy;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pGetLinkInfo
**
** Description      This function is called to get local/remote link MIU and
**                  Well-Known Service list encoded as a 16-bit field of connected LLCP.
**                  NFA_P2P_LINK_INFO_EVT will be returned.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if server or client is not registered
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pGetLinkInfo (tNFA_HANDLE handle)
{
    tNFA_P2P_API_GET_LINK_INFO *p_msg;
    tNFA_HANDLE                 xx;

    P2P_TRACE_API1 ("NFA_P2pGetLinkInfo (): handle:0x%x", handle);

    if (nfa_p2p_cb.llcp_state != NFA_P2P_LLCP_STATE_ACTIVATED)
    {
        P2P_TRACE_ERROR0 ("NFA_P2pGetLinkInfo (): LLCP link is not activated");
        return (NFA_STATUS_FAILED);
    }

    xx = handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pGetLinkInfo (): Handle is invalid or not registered");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_msg = (tNFA_P2P_API_GET_LINK_INFO *) GKI_getbuf (sizeof (tNFA_P2P_API_GET_LINK_INFO))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_GET_LINK_INFO_EVT;

        p_msg->handle = handle;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pGetRemoteSap
**
** Description      This function is called to get SAP associated by service name
**                  on connected remote LLCP.
**                  NFA_P2P_SDP_EVT will be returned.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if server or client is not registered
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pGetRemoteSap (tNFA_HANDLE handle,
                                 char        *p_service_name)
{
    tNFA_P2P_API_GET_REMOTE_SAP *p_msg;
    tNFA_HANDLE                  xx;

    P2P_TRACE_API2 ("NFA_P2pGetRemoteSap(): handle:0x%x, SN:<%s>", handle, p_service_name);

    if (nfa_p2p_cb.llcp_state != NFA_P2P_LLCP_STATE_ACTIVATED)
    {
        P2P_TRACE_ERROR0 ("NFA_P2pGetRemoteSap(): LLCP link is not activated");
        return (NFA_STATUS_FAILED);
    }

    xx = handle & NFA_HANDLE_MASK;

    if (  (xx >= NFA_P2P_NUM_SAP)
        ||(nfa_p2p_cb.sap_cb[xx].p_cback == NULL)  )
    {
        P2P_TRACE_ERROR0 ("NFA_P2pGetRemoteSap (): Handle is invalid or not registered");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_msg = (tNFA_P2P_API_GET_REMOTE_SAP *) GKI_getbuf (sizeof (tNFA_P2P_API_GET_REMOTE_SAP))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_GET_REMOTE_SAP_EVT;

        p_msg->handle = handle;

        BCM_STRNCPY_S (p_msg->service_name, sizeof (p_msg->service_name), p_service_name, LLCP_MAX_SN_LEN);
        p_msg->service_name[LLCP_MAX_SN_LEN] = 0;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pSetLLCPConfig
**
** Description      This function is called to change LLCP config parameters.
**                  Application must call while LLCP is not activated.
**
**                  Parameters descriptions (default value)
**                  - Local Link MIU (LLCP_MIU)
**                  - Option parameter (LLCP_OPT_VALUE)
**                  - Response Waiting Time Index (LLCP_WAITING_TIME)
**                  - Local Link Timeout (LLCP_LTO_VALUE)
**                  - Inactivity Timeout as initiator role (LLCP_INIT_INACTIVITY_TIMEOUT)
**                  - Inactivity Timeout as target role (LLCP_TARGET_INACTIVITY_TIMEOUT)
**                  - Delay SYMM response (LLCP_DELAY_RESP_TIME)
**                  - Data link connection timeout (LLCP_DATA_LINK_CONNECTION_TOUT)
**                  - Delay timeout to send first PDU as initiator (LLCP_DELAY_TIME_TO_SEND_FIRST_PDU)
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_P2pSetLLCPConfig (UINT16  link_miu,
                                  UINT8   opt,
                                  UINT8   wt,
                                  UINT16  link_timeout,
                                  UINT16  inact_timeout_init,
                                  UINT16  inact_timeout_target,
                                  UINT16  symm_delay,
                                  UINT16  data_link_timeout,
                                  UINT16  delay_first_pdu_timeout)
{
    tNFA_P2P_API_SET_LLCP_CFG *p_msg;

    P2P_TRACE_API4 ("NFA_P2pSetLLCPConfig ():link_miu:%d, opt:0x%02X, wt:%d, link_timeout:%d",
                     link_miu, opt, wt, link_timeout);
    P2P_TRACE_API4 ("                       inact_timeout(init:%d, target:%d), symm_delay:%d, data_link_timeout:%d",
                     inact_timeout_init, inact_timeout_target, symm_delay, data_link_timeout);
    P2P_TRACE_API1 ("                       delay_first_pdu_timeout:%d", delay_first_pdu_timeout);

    if (nfa_p2p_cb.llcp_state == NFA_P2P_LLCP_STATE_ACTIVATED)
    {
        P2P_TRACE_ERROR0 ("NFA_P2pSetLLCPConfig (): LLCP link is activated");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_P2P_API_SET_LLCP_CFG *) GKI_getbuf (sizeof (tNFA_P2P_API_SET_LLCP_CFG))) != NULL)
    {
        p_msg->hdr.event = NFA_P2P_API_SET_LLCP_CFG_EVT;

        p_msg->link_miu             = link_miu;
        p_msg->opt                  = opt;
        p_msg->wt                   = wt;
        p_msg->link_timeout         = link_timeout;
        p_msg->inact_timeout_init   = inact_timeout_init;
        p_msg->inact_timeout_target = inact_timeout_target;
        p_msg->symm_delay           = symm_delay;
        p_msg->data_link_timeout    = data_link_timeout;
        p_msg->delay_first_pdu_timeout = delay_first_pdu_timeout;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_P2pGetLLCPConfig
**
** Description      This function is called to read LLCP config parameters.
**
**                  Parameters descriptions
**                  - Local Link MIU
**                  - Option parameter
**                  - Response Waiting Time Index
**                  - Local Link Timeout
**                  - Inactivity Timeout as initiator role
**                  - Inactivity Timeout as target role
**                  - Delay SYMM response
**                  - Data link connection timeout
**                  - Delay timeout to send first PDU as initiator
**
** Returns          None
**
*******************************************************************************/
void NFA_P2pGetLLCPConfig (UINT16 *p_link_miu,
                           UINT8  *p_opt,
                           UINT8  *p_wt,
                           UINT16 *p_link_timeout,
                           UINT16 *p_inact_timeout_init,
                           UINT16 *p_inact_timeout_target,
                           UINT16 *p_symm_delay,
                           UINT16 *p_data_link_timeout,
                           UINT16 *p_delay_first_pdu_timeout)
{
    LLCP_GetConfig (p_link_miu,
                    p_opt,
                    p_wt,
                    p_link_timeout,
                    p_inact_timeout_init,
                    p_inact_timeout_target,
                    p_symm_delay,
                    p_data_link_timeout,
                    p_delay_first_pdu_timeout);

    P2P_TRACE_API4 ("NFA_P2pGetLLCPConfig () link_miu:%d, opt:0x%02X, wt:%d, link_timeout:%d",
                     *p_link_miu, *p_opt, *p_wt, *p_link_timeout);
    P2P_TRACE_API4 ("                       inact_timeout(init:%d, target:%d), symm_delay:%d, data_link_timeout:%d",
                     *p_inact_timeout_init, *p_inact_timeout_target, *p_symm_delay, *p_data_link_timeout);
    P2P_TRACE_API1 ("delay_first_pdu_timeout:%d",*p_delay_first_pdu_timeout);
}

/*******************************************************************************
**
** Function         NFA_P2pSetTraceLevel
**
** Description      This function sets the trace level for P2P.  If called with
**                  a value of 0xFF, it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 NFA_P2pSetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        nfa_p2p_cb.trace_level = new_level;

    return (nfa_p2p_cb.trace_level);
}
