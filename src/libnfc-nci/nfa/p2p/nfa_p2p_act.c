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
 *  This is the implementation file for the NFA P2P.
 *
 ******************************************************************************/

#include "string.h"
#include "nfc_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfa_dm_int.h"
#include "llcp_defs.h"
#include "llcp_api.h"
#include "nfa_p2p_api.h"
#include "nfa_p2p_int.h"

/*****************************************************************************
**  Global Variables
*****************************************************************************/

/*****************************************************************************
**  Static Functions
*****************************************************************************/

/*****************************************************************************
**  Constants
*****************************************************************************/

/*******************************************************************************
**
** Function         nfa_p2p_allocate_conn_cb
**
** Description      Allocate data link connection control block
**
**
** Returns          UINT8
**
*******************************************************************************/
static UINT8 nfa_p2p_allocate_conn_cb (UINT8 local_sap)
{
    UINT8 xx;

    for (xx = 0; xx < LLCP_MAX_DATA_LINK; xx++)
    {
        if (nfa_p2p_cb.conn_cb[xx].flags == 0)
        {
            nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_IN_USE;
            nfa_p2p_cb.conn_cb[xx].local_sap = local_sap;

            return (xx);
        }
    }

    P2P_TRACE_ERROR0 ("nfa_p2p_allocate_conn_cb (): No resource");

    return LLCP_MAX_DATA_LINK;
}

/*******************************************************************************
**
** Function         nfa_p2p_deallocate_conn_cb
**
** Description      Deallocate data link connection control block
**
**
** Returns          void
**
*******************************************************************************/
static void nfa_p2p_deallocate_conn_cb (UINT8 xx)
{
    if (xx < LLCP_MAX_DATA_LINK)
    {
        nfa_p2p_cb.conn_cb[xx].flags = 0;
    }
    else
    {
        P2P_TRACE_ERROR1 ("nfa_p2p_deallocate_conn_cb (): Invalid index (%d)", xx);
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_find_conn_cb
**
** Description      Find data link connection control block by local/remote SAP
**
**
** Returns          UINT8
**
*******************************************************************************/
static UINT8 nfa_p2p_find_conn_cb (UINT8 local_sap, UINT8 remote_sap)
{
    UINT8 xx;

    for (xx = 0; xx < LLCP_MAX_DATA_LINK; xx++)
    {
        if (  (nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_IN_USE)
            &&(nfa_p2p_cb.conn_cb[xx].local_sap == local_sap)
            &&(nfa_p2p_cb.conn_cb[xx].remote_sap == remote_sap)  )
        {
            return (xx);
        }
    }

    return (LLCP_MAX_DATA_LINK);
}

/*******************************************************************************
**
** Function         nfa_p2p_llcp_cback
**
** Description      Processing SAP callback events from LLCP
**
**
** Returns          None
**
*******************************************************************************/
static void nfa_p2p_llcp_cback (tLLCP_SAP_CBACK_DATA *p_data)
{
    P2P_TRACE_DEBUG2 ("nfa_p2p_llcp_cback (): event:0x%02X, local_sap:0x%02X", p_data->hdr.event, p_data->hdr.local_sap);

    switch (p_data->hdr.event)
    {
    case LLCP_SAP_EVT_DATA_IND:
         nfa_p2p_proc_llcp_data_ind (p_data);
        break;

    case LLCP_SAP_EVT_CONNECT_IND:
        nfa_p2p_proc_llcp_connect_ind (p_data);
        break;

    case LLCP_SAP_EVT_CONNECT_RESP:
        nfa_p2p_proc_llcp_connect_resp (p_data);
        break;

    case LLCP_SAP_EVT_DISCONNECT_IND:
        nfa_p2p_proc_llcp_disconnect_ind (p_data);
        break;

    case LLCP_SAP_EVT_DISCONNECT_RESP:
        nfa_p2p_proc_llcp_disconnect_resp (p_data);
        break;

    case LLCP_SAP_EVT_CONGEST:
        nfa_p2p_proc_llcp_congestion (p_data);
        break;

    case LLCP_SAP_EVT_LINK_STATUS:
        nfa_p2p_proc_llcp_link_status (p_data);
        break;

    default:
        P2P_TRACE_ERROR1 ("nfa_p2p_llcp_cback (): Unknown event:0x%02X", p_data->hdr.event);
        return;
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_sdp_cback
**
** Description      Process SDP callback event from LLCP
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_sdp_cback (UINT8 tid, UINT8 remote_sap)
{
    UINT8             local_sap;
    UINT8             xx;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG2 ("nfa_p2p_sdp_cback (): tid:0x%02X, remote_sap:0x%02X", tid, remote_sap);

    /* search for callback function to process */
    for (xx = 0; xx < LLCP_MAX_SDP_TRANSAC; xx++)
    {
        if (  (nfa_p2p_cb.sdp_cb[xx].local_sap != LLCP_INVALID_SAP)
            &&(nfa_p2p_cb.sdp_cb[xx].tid == tid)  )
        {
            local_sap = nfa_p2p_cb.sdp_cb[xx].local_sap;

            evt_data.sdp.handle     = (NFA_HANDLE_GROUP_P2P | local_sap);
            evt_data.sdp.remote_sap = remote_sap;
            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_SDP_EVT, &evt_data);

            nfa_p2p_cb.sdp_cb[xx].local_sap = LLCP_INVALID_SAP;
            break;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_start_sdp
**
** Description      Initiate SDP
**
**
** Returns          TRUE if success
**
*******************************************************************************/
BOOLEAN nfa_p2p_start_sdp (char *p_service_name, UINT8 local_sap)
{
    int xx;

    P2P_TRACE_DEBUG1 ("nfa_p2p_start_sdp (): SN:<%s>", p_service_name);

    /* search for empty slot */
    for (xx = 0; xx < LLCP_MAX_SDP_TRANSAC; xx++)
    {
        if (nfa_p2p_cb.sdp_cb[xx].local_sap == LLCP_INVALID_SAP)
        {
            if (LLCP_DiscoverService (p_service_name,
                                      nfa_p2p_sdp_cback,
                                      &(nfa_p2p_cb.sdp_cb[xx].tid)) == LLCP_STATUS_SUCCESS)
            {
                nfa_p2p_cb.sdp_cb[xx].local_sap    = local_sap;
                return TRUE;
            }
            else
            {
                /* failure of SDP */
                return FALSE;
            }
        }
    }
    return FALSE;
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_data_ind
**
** Description      Processing incoming data event from LLCP
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_proc_llcp_data_ind (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             local_sap, xx;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG0 ("nfa_p2p_proc_llcp_data_ind ()");

    local_sap = p_data->data_ind.local_sap;

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        evt_data.data.handle    = 0;
        /* if connectionless */
        if (p_data->data_ind.link_type == NFA_P2P_LLINK_TYPE)
        {
            evt_data.data.handle = (NFA_HANDLE_GROUP_P2P | local_sap);
        }
        else
        {
            xx = nfa_p2p_find_conn_cb (p_data->data_ind.local_sap,
                                       p_data->data_ind.remote_sap);

            if (xx != LLCP_MAX_DATA_LINK)
            {
                evt_data.data.handle = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
            }
        }

        evt_data.data.remote_sap = p_data->data_ind.remote_sap;
        evt_data.data.link_type  = p_data->data_ind.link_type;

        /* notify upper layer that there are data at LLCP */
        nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DATA_EVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_connect_ind
**
** Description      Processing connection request from peer
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_proc_llcp_connect_ind (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             server_sap, local_sap;
    tNFA_P2P_EVT_DATA evt_data;
    UINT8             xx;

    P2P_TRACE_DEBUG1 ("nfa_p2p_proc_llcp_connect_ind () server_sap:0x%x",
                       p_data->connect_ind.server_sap);

    server_sap = p_data->connect_ind.server_sap;
    local_sap  = p_data->connect_ind.local_sap;

    if (nfa_p2p_cb.sap_cb[server_sap].p_cback)
    {
        xx = nfa_p2p_allocate_conn_cb (server_sap);

        if (xx != LLCP_MAX_DATA_LINK)
        {
            nfa_p2p_cb.conn_cb[xx].remote_sap = p_data->connect_ind.remote_sap;
            nfa_p2p_cb.conn_cb[xx].remote_miu = p_data->connect_ind.miu;

            /* peer will not receive any data */
            if (p_data->connect_ind.rw == 0)
                nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_REMOTE_RW_ZERO;

            evt_data.conn_req.server_handle = (NFA_HANDLE_GROUP_P2P | server_sap);
            evt_data.conn_req.conn_handle   = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
            evt_data.conn_req.remote_sap    = p_data->connect_ind.remote_sap;
            evt_data.conn_req.remote_miu    = p_data->connect_ind.miu;
            evt_data.conn_req.remote_rw     = p_data->connect_ind.rw;

            nfa_p2p_cb.sap_cb[server_sap].p_cback (NFA_P2P_CONN_REQ_EVT, &evt_data);
        }
    }
    else
    {
        P2P_TRACE_ERROR0 ("nfa_p2p_proc_llcp_connect_ind (): Not registered");
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_connect_resp
**
** Description      Processing connection response from peer
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_proc_llcp_connect_resp (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             local_sap, xx;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG0 ("nfa_p2p_proc_llcp_connect_resp ()");

    local_sap  = p_data->connect_resp.local_sap;

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        xx = nfa_p2p_allocate_conn_cb (local_sap);

        if (xx != LLCP_MAX_DATA_LINK)
        {
            nfa_p2p_cb.conn_cb[xx].remote_sap = p_data->connect_resp.remote_sap;
            nfa_p2p_cb.conn_cb[xx].remote_miu = p_data->connect_resp.miu;

            /* peer will not receive any data */
            if (p_data->connect_resp.rw == 0)
                nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_REMOTE_RW_ZERO;

            evt_data.connected.client_handle = (NFA_HANDLE_GROUP_P2P | local_sap);
            evt_data.connected.conn_handle   = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
            evt_data.connected.remote_sap    = p_data->connect_resp.remote_sap;
            evt_data.connected.remote_miu    = p_data->connect_resp.miu;
            evt_data.connected.remote_rw     = p_data->connect_resp.rw;

            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_CONNECTED_EVT, &evt_data);
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_disconnect_ind
**
** Description      Processing disconnection request from peer
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_proc_llcp_disconnect_ind (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             local_sap, xx;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG0 ("nfa_p2p_proc_llcp_disconnect_ind ()");

    local_sap  = p_data->disconnect_ind.local_sap;

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        xx = nfa_p2p_find_conn_cb (p_data->disconnect_ind.local_sap,
                                   p_data->disconnect_ind.remote_sap);

        if (xx != LLCP_MAX_DATA_LINK)
        {
            evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
            evt_data.disc.reason = NFA_P2P_DISC_REASON_REMOTE_INITIATE;

            nfa_p2p_deallocate_conn_cb (xx);

            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);
        }
        else
        {
            /*
            ** LLCP link has been deactivated before receiving CC or DM.
            ** Return NFA_P2P_DISC_EVT to indicate failure of creating connection
            */

            evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | local_sap);
            evt_data.disc.reason = NFA_P2P_DISC_REASON_LLCP_DEACTIVATED;

            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);

            P2P_TRACE_ERROR0 ("nfa_p2p_proc_llcp_disconnect_ind (): Link deactivated");
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_disconnect_resp
**
** Description      Processing rejected connection from peer
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_proc_llcp_disconnect_resp (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             local_sap, xx;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG0 ("nfa_p2p_proc_llcp_disconnect_resp ()");

    local_sap  = p_data->disconnect_resp.local_sap;

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        if (p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_RESP_DISC)
        {
            evt_data.disc.reason = NFA_P2P_DISC_REASON_LOCAL_INITITATE;
        }
        else if (  (p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_APP_REJECTED)
                 ||(p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_PERM_REJECT_THIS)
                 ||(p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_PERM_REJECT_ANY)
                 ||(p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_TEMP_REJECT_THIS)
                 ||(p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_TEMP_REJECT_ANY)  )
        {
            evt_data.disc.reason = NFA_P2P_DISC_REASON_REMOTE_REJECT;
        }
        else if (p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_NO_SERVICE)
        {
            evt_data.disc.reason = NFA_P2P_DISC_REASON_NO_SERVICE;
        }
        else if (p_data->disconnect_resp.reason == LLCP_SAP_DM_REASON_NO_ACTIVE_CONNECTION)
        {
            evt_data.disc.reason = NFA_P2P_DISC_REASON_LLCP_DEACTIVATED;
        }
        else
        {
            evt_data.disc.reason = NFA_P2P_DISC_REASON_NO_INFORMATION;
        }

        if (evt_data.disc.reason == NFA_P2P_DISC_REASON_LOCAL_INITITATE)
        {
            xx = nfa_p2p_find_conn_cb (p_data->disconnect_resp.local_sap,
                                       p_data->disconnect_resp.remote_sap);

            if (xx != LLCP_MAX_DATA_LINK)
            {
                evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);

                nfa_p2p_deallocate_conn_cb (xx);

                nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);
            }
            else
            {
                P2P_TRACE_ERROR0 ("nfa_p2p_proc_llcp_disconnect_resp (): No connection found");
            }
        }
        else
        {
            evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | local_sap);
            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_congest
**
** Description      Processing LLCP congestion event
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_proc_llcp_congestion (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             local_sap, remote_sap, xx;
    tNFA_P2P_EVT_DATA evt_data;

    local_sap  = p_data->congest.local_sap;
    remote_sap = p_data->congest.remote_sap;

    evt_data.congest.link_type    = p_data->congest.link_type;
    evt_data.congest.is_congested = p_data->congest.is_congested;

    if (p_data->congest.is_congested)
    {
        P2P_TRACE_DEBUG2 ("nfa_p2p_proc_llcp_congestion () START SAP=(0x%x,0x%x)",
                          local_sap, remote_sap);

    }
    else
    {
        P2P_TRACE_DEBUG2 ("nfa_p2p_proc_llcp_congestion () END SAP=(0x%x,0x%x)",
                          local_sap, remote_sap);
    }

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        if (evt_data.congest.link_type == NFA_P2P_LLINK_TYPE)
        {
            evt_data.congest.handle = (NFA_HANDLE_GROUP_P2P | local_sap);

            if (  (evt_data.congest.is_congested == FALSE)
                &&(nfa_p2p_cb.sap_cb[local_sap].flags & NFA_P2P_SAP_FLAG_LLINK_CONGESTED)  )
            {
                nfa_p2p_cb.sap_cb[local_sap].flags &= ~NFA_P2P_SAP_FLAG_LLINK_CONGESTED;
                nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_CONGEST_EVT, &evt_data);
            }
            else if (  (evt_data.congest.is_congested == TRUE)
                     &&(!(nfa_p2p_cb.sap_cb[local_sap].flags & NFA_P2P_SAP_FLAG_LLINK_CONGESTED))  )
            {
                /* this is overall congestion due to high usage of buffer pool */
                nfa_p2p_cb.sap_cb[local_sap].flags |= NFA_P2P_SAP_FLAG_LLINK_CONGESTED;
                nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_CONGEST_EVT, &evt_data);
            }
        }
        else
        {
            xx = nfa_p2p_find_conn_cb (local_sap, remote_sap);

            if (xx != LLCP_MAX_DATA_LINK)
            {
                evt_data.congest.handle = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);

                if (  (evt_data.congest.is_congested == FALSE)
                    &&(nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_CONGESTED)  )
                {
                    nfa_p2p_cb.conn_cb[xx].flags &= ~NFA_P2P_CONN_FLAG_CONGESTED;
                    nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_CONGEST_EVT, &evt_data);
                }
                else if (  (evt_data.congest.is_congested == TRUE)
                         &&(!(nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_CONGESTED))  )
                {
                    /* this is overall congestion due to high usage of buffer pool */
                    nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_CONGESTED;
                    nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_CONGEST_EVT, &evt_data);
                }
            }
            else
            {
                P2P_TRACE_ERROR0 ("nfa_p2p_proc_llcp_congestion (): No connection found");
            }
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_proc_llcp_link_status
**
** Description      Processing LLCP link status
**
**
** Returns          next state after processing this event
**
*******************************************************************************/
void nfa_p2p_proc_llcp_link_status (tLLCP_SAP_CBACK_DATA  *p_data)
{
    UINT8             local_sap, xx;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG1 ("nfa_p2p_proc_llcp_link_status () is_activated:%d",
                       p_data->link_status.is_activated);

    local_sap  = p_data->link_status.local_sap;

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        if (p_data->link_status.is_activated)
        {
            /* only for server */
            evt_data.activated.handle           = (NFA_HANDLE_GROUP_P2P | local_sap);
            evt_data.activated.local_link_miu   = nfa_p2p_cb.local_link_miu;
            evt_data.activated.remote_link_miu  = nfa_p2p_cb.remote_link_miu;

            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_ACTIVATED_EVT, &evt_data);
        }
        else /* if LLCP link is deactivated */
        {
            for (xx = 0; xx < LLCP_MAX_DATA_LINK; xx++)
            {
                if (  (nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_IN_USE)
                    &&(nfa_p2p_cb.conn_cb[xx].local_sap == local_sap))
                {
                    evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
                    evt_data.disc.reason = NFA_P2P_DISC_REASON_LLCP_DEACTIVATED;

                    nfa_p2p_deallocate_conn_cb (xx);
                    nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);
                }
            }

            /* notify deactivation and clear flags */
            if (nfa_p2p_cb.sap_cb[local_sap].flags & NFA_P2P_SAP_FLAG_SERVER)
            {
                evt_data.deactivated.handle = (NFA_HANDLE_GROUP_P2P | local_sap);
                nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DEACTIVATED_EVT, &evt_data);

                nfa_p2p_cb.sap_cb[local_sap].flags = NFA_P2P_SAP_FLAG_SERVER;
            }
            else if (nfa_p2p_cb.sap_cb[local_sap].flags & NFA_P2P_SAP_FLAG_CLIENT)
            {
                evt_data.deactivated.handle = (NFA_HANDLE_GROUP_P2P | local_sap);
                nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DEACTIVATED_EVT, &evt_data);

                nfa_p2p_cb.sap_cb[local_sap].flags = NFA_P2P_SAP_FLAG_CLIENT;
            }
            else /* if this is not registered service */
            {
                nfa_p2p_cb.sap_cb[local_sap].p_cback = NULL;
            }
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_reg_server
**
** Description      Allocate a service as server and register to LLCP
**
**
** Returns          FALSE if need to keep buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_reg_server (tNFA_P2P_MSG *p_msg)
{
    tNFA_P2P_EVT_DATA  evt_data;
    UINT8              server_sap;

    P2P_TRACE_DEBUG0 ("nfa_p2p_reg_server ()");

    server_sap = LLCP_RegisterServer (p_msg->api_reg_server.server_sap,
                                      p_msg->api_reg_server.link_type,
                                      p_msg->api_reg_server.service_name,
                                      nfa_p2p_llcp_cback);

    if (server_sap == LLCP_INVALID_SAP)
    {
        evt_data.reg_server.server_handle = NFA_HANDLE_INVALID;
        evt_data.reg_server.server_sap    = NFA_P2P_INVALID_SAP;
        BCM_STRNCPY_S (evt_data.reg_server.service_name, sizeof (evt_data.reg_server.service_name),
                       p_msg->api_reg_server.service_name, LLCP_MAX_SN_LEN);
        evt_data.reg_server.service_name[LLCP_MAX_SN_LEN] = 0;

        p_msg->api_reg_server.p_cback (NFA_P2P_REG_SERVER_EVT, &evt_data);

        return TRUE;
    }

    /* if need to update WKS in LLCP Gen bytes */
    if (server_sap <= LLCP_UPPER_BOUND_WK_SAP)
    {
        nfa_p2p_enable_listening (NFA_ID_P2P, TRUE);
    }
    else if (!nfa_p2p_cb.is_p2p_listening)
    {
        nfa_p2p_enable_listening (NFA_ID_P2P, FALSE);
    }

    nfa_p2p_cb.sap_cb[server_sap].p_cback    = p_msg->api_reg_server.p_cback;
    nfa_p2p_cb.sap_cb[server_sap].flags      = NFA_P2P_SAP_FLAG_SERVER;

    evt_data.reg_server.server_handle = (NFA_HANDLE_GROUP_P2P | server_sap);
    evt_data.reg_server.server_sap    = server_sap;
    BCM_STRNCPY_S (evt_data.reg_server.service_name, sizeof (evt_data.reg_server.service_name),
                   p_msg->api_reg_server.service_name, LLCP_MAX_SN_LEN);
    evt_data.reg_server.service_name[LLCP_MAX_SN_LEN] = 0;

    /* notify NFA_P2P_REG_SERVER_EVT to server */
    nfa_p2p_cb.sap_cb[server_sap].p_cback (NFA_P2P_REG_SERVER_EVT, &evt_data);

    /* if LLCP is already activated */
    if (nfa_p2p_cb.llcp_state == NFA_P2P_LLCP_STATE_ACTIVATED)
    {
        evt_data.activated.handle          = (NFA_HANDLE_GROUP_P2P | server_sap);
        evt_data.activated.local_link_miu  = nfa_p2p_cb.local_link_miu;
        evt_data.activated.remote_link_miu = nfa_p2p_cb.remote_link_miu;

        /* notify NFA_P2P_ACTIVATED_EVT to server */
        nfa_p2p_cb.sap_cb[server_sap].p_cback (NFA_P2P_ACTIVATED_EVT, &evt_data);
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_reg_client
**
** Description      Allocate a service as client and register to LLCP
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_reg_client (tNFA_P2P_MSG *p_msg)
{
    tNFA_P2P_EVT_DATA  evt_data;
    UINT8              local_sap;

    P2P_TRACE_DEBUG0 ("nfa_p2p_reg_client ()");

    local_sap = LLCP_RegisterClient (p_msg->api_reg_client.link_type,
                                     nfa_p2p_llcp_cback);

    if (local_sap == LLCP_INVALID_SAP)
    {
        evt_data.reg_client.client_handle = NFA_HANDLE_INVALID;
        p_msg->api_reg_client.p_cback (NFA_P2P_REG_CLIENT_EVT, &evt_data);
        return TRUE;
    }

    nfa_p2p_cb.sap_cb[local_sap].p_cback = p_msg->api_reg_client.p_cback;
    nfa_p2p_cb.sap_cb[local_sap].flags   = NFA_P2P_SAP_FLAG_CLIENT;

    evt_data.reg_client.client_handle = (NFA_HANDLE_GROUP_P2P | local_sap);
    nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_REG_CLIENT_EVT, &evt_data);

    /* if LLCP is already activated */
    if (nfa_p2p_cb.llcp_state == NFA_P2P_LLCP_STATE_ACTIVATED)
    {
        evt_data.activated.handle           = (NFA_HANDLE_GROUP_P2P | local_sap);
        evt_data.activated.local_link_miu   = nfa_p2p_cb.local_link_miu;
        evt_data.activated.remote_link_miu  = nfa_p2p_cb.remote_link_miu;

        /* notify NFA_P2P_ACTIVATED_EVT to client */
        nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_ACTIVATED_EVT, &evt_data);
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_dereg
**
** Description      Deallocate a service as server or client and deregister to LLCP
**                  LLCP will deallocate data link connection created by this server
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_dereg (tNFA_P2P_MSG *p_msg)
{
    UINT8 local_sap, xx;

    P2P_TRACE_DEBUG0 ("nfa_p2p_dereg ()");

    local_sap = (UINT8) (p_msg->api_dereg.handle & NFA_HANDLE_MASK);

    if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
    {
        for (xx = 0; xx < LLCP_MAX_DATA_LINK; xx++)
        {
            if (  (nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_IN_USE)
                &&(nfa_p2p_cb.conn_cb[xx].local_sap == local_sap)  )
            {
                nfa_p2p_deallocate_conn_cb (xx);
            }
        }
    }

    LLCP_Deregister (local_sap);
    nfa_p2p_cb.sap_cb[local_sap].p_cback = NULL;

    if (nfa_p2p_cb.is_p2p_listening)
    {
        /* check if this is the last server on NFA P2P */
        for (xx = 0; xx < NFA_P2P_NUM_SAP; xx++)
        {
            if (  (nfa_p2p_cb.sap_cb[xx].p_cback)
                &&(nfa_p2p_cb.sap_cb[xx].flags & NFA_P2P_SAP_FLAG_SERVER)  )
            {
                break;
            }
        }

        if (xx >= NFA_P2P_NUM_SAP)
        {
            /* if need to update WKS in LLCP Gen bytes */
            if (local_sap <= LLCP_UPPER_BOUND_WK_SAP)
                nfa_p2p_disable_listening (NFA_ID_P2P, TRUE);
            else
                nfa_p2p_disable_listening (NFA_ID_P2P, FALSE);
        }
        /* if need to update WKS in LLCP Gen bytes */
        else if (local_sap <= LLCP_UPPER_BOUND_WK_SAP)
        {
            nfa_p2p_enable_listening (NFA_ID_P2P, TRUE);
        }
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_accept_connection
**
** Description      Connection Confirm from local application
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_accept_connection (tNFA_P2P_MSG *p_msg)
{
    UINT8                   xx;
    tLLCP_CONNECTION_PARAMS params;

    P2P_TRACE_DEBUG0 ("nfa_p2p_accept_connection ()");

    xx  = (UINT8) (p_msg->api_accept.conn_handle & NFA_HANDLE_MASK);
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    params.miu   = p_msg->api_accept.miu;
    params.rw    = p_msg->api_accept.rw;
    params.sn[0] = 0;

    LLCP_ConnectCfm (nfa_p2p_cb.conn_cb[xx].local_sap, nfa_p2p_cb.conn_cb[xx].remote_sap, &params);

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_reject_connection
**
** Description      Reject connection by local application
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_reject_connection (tNFA_P2P_MSG *p_msg)
{
    UINT8 xx;

    P2P_TRACE_DEBUG0 ("nfa_p2p_reject_connection ()");

    xx  = (UINT8) (p_msg->api_reject.conn_handle & NFA_HANDLE_MASK);
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    LLCP_ConnectReject (nfa_p2p_cb.conn_cb[xx].local_sap, nfa_p2p_cb.conn_cb[xx].remote_sap,
                        LLCP_SAP_DM_REASON_APP_REJECTED);

    /* no need to deregister service on LLCP */
    nfa_p2p_deallocate_conn_cb (xx);

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_disconnect
**
** Description      Disconnect data link connection by local application
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_disconnect (tNFA_P2P_MSG *p_msg)
{
    UINT8             local_sap, xx;
    tLLCP_STATUS      status;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG0 ("nfa_p2p_disconnect ()");

    xx = (UINT8) (p_msg->api_disconnect.conn_handle & NFA_HANDLE_MASK);

    /* if this is for data link connection */
    if (xx & NFA_P2P_HANDLE_FLAG_CONN)
    {
        xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

        status = LLCP_DisconnectReq (nfa_p2p_cb.conn_cb[xx].local_sap, nfa_p2p_cb.conn_cb[xx].remote_sap,
                                     p_msg->api_disconnect.flush);

        if (status == LLCP_STATUS_SUCCESS)
        {
            /* wait for disconnect response if successful */
            return TRUE;
        }
        else
        {
            /*
            ** while we are waiting for connect confirm,
            ** we cannot sent DISC because we don't know DSAP yet
            */
            local_sap = nfa_p2p_cb.conn_cb[xx].local_sap;

            if (nfa_p2p_cb.sap_cb[local_sap].p_cback)
            {
                evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
                evt_data.disc.reason = NFA_P2P_DISC_REASON_LOCAL_INITITATE;

                nfa_p2p_deallocate_conn_cb (xx);
                nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);
            }
        }
    }
    else
    {
        P2P_TRACE_ERROR0 ("Handle is not for Data link connection");
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_create_data_link_connection
**
** Description      Create data link connection
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_create_data_link_connection (tNFA_P2P_MSG *p_msg)
{
    UINT8                   local_sap;
    tNFA_P2P_EVT_DATA       evt_data;
    tLLCP_CONNECTION_PARAMS conn_params;
    tLLCP_STATUS            status;

    P2P_TRACE_DEBUG0 ("nfa_p2p_create_data_link_connection ()");

    local_sap = (UINT8) (p_msg->api_connect.client_handle & NFA_HANDLE_MASK);

    conn_params.miu = p_msg->api_connect.miu;
    conn_params.rw  = p_msg->api_connect.rw;

    /* NFA_P2pConnectBySap () */
    if (p_msg->api_connect.dsap != LLCP_INVALID_SAP)
    {
        conn_params.sn[0] = 0;
        status = LLCP_ConnectReq (local_sap, p_msg->api_connect.dsap, &conn_params);
    }
    /* NFA_P2pConnectByName () */
    else
    {
        BCM_STRNCPY_S (conn_params.sn, sizeof (conn_params.sn),
                       p_msg->api_connect.service_name, LLCP_MAX_SN_LEN);
        conn_params.sn[LLCP_MAX_SN_LEN] = 0;

        status = LLCP_ConnectReq (local_sap, LLCP_SAP_SDP, &conn_params);
    }

    if (status != LLCP_STATUS_SUCCESS)
    {
        evt_data.disc.handle = (NFA_HANDLE_GROUP_P2P | local_sap);
        evt_data.disc.reason = NFA_P2P_DISC_REASON_NO_INFORMATION;

        nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_DISC_EVT, &evt_data);
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_send_ui
**
** Description      Send UI PDU
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_send_ui (tNFA_P2P_MSG *p_msg)
{
    UINT8             local_sap;
    tLLCP_STATUS      status;
    tNFA_P2P_EVT_DATA evt_data;

    P2P_TRACE_DEBUG0 ("nfa_p2p_send_ui ()");

    local_sap = (UINT8) (p_msg->api_send_ui.handle & NFA_HANDLE_MASK);

    /* decrease number of tx UI PDU which is not processed by NFA for congestion control */
    if (nfa_p2p_cb.sap_cb[local_sap].num_pending_ui_pdu)
        nfa_p2p_cb.sap_cb[local_sap].num_pending_ui_pdu--;

    if (nfa_p2p_cb.total_pending_ui_pdu)
        nfa_p2p_cb.total_pending_ui_pdu--;

    status = LLCP_SendUI (local_sap,
                          p_msg->api_send_ui.dsap,
                          p_msg->api_send_ui.p_msg);

    if (status == LLCP_STATUS_CONGESTED)
    {
        if (!(nfa_p2p_cb.sap_cb[local_sap].flags & NFA_P2P_SAP_FLAG_LLINK_CONGESTED))
        {
            nfa_p2p_cb.sap_cb[local_sap].flags |= NFA_P2P_SAP_FLAG_LLINK_CONGESTED;

            /* notify that this logical link is congested */
            evt_data.congest.link_type    = NFA_P2P_LLINK_TYPE;
            evt_data.congest.handle       = (NFA_HANDLE_GROUP_P2P | local_sap);
            evt_data.congest.is_congested = TRUE;

            nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_CONGEST_EVT, &evt_data);
        }
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_send_data
**
** Description      Send I PDU
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_send_data (tNFA_P2P_MSG *p_msg)
{
    tNFA_P2P_EVT_DATA evt_data;
    tLLCP_STATUS      status;
    UINT8             xx;

    P2P_TRACE_DEBUG0 ("nfa_p2p_send_data ()");

    xx = (UINT8) (p_msg->api_send_data.conn_handle & NFA_HANDLE_MASK);
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    /* decrease number of tx I PDU which is not processed by NFA for congestion control */
    if (nfa_p2p_cb.conn_cb[xx].num_pending_i_pdu)
        nfa_p2p_cb.conn_cb[xx].num_pending_i_pdu--;

    if (nfa_p2p_cb.total_pending_i_pdu)
        nfa_p2p_cb.total_pending_i_pdu--;

    status = LLCP_SendData (nfa_p2p_cb.conn_cb[xx].local_sap,
                            nfa_p2p_cb.conn_cb[xx].remote_sap,
                            p_msg->api_send_data.p_msg);

    if (status == LLCP_STATUS_CONGESTED)
    {
        if (!(nfa_p2p_cb.conn_cb[xx].flags & NFA_P2P_CONN_FLAG_CONGESTED))
        {
            nfa_p2p_cb.conn_cb[xx].flags |= NFA_P2P_CONN_FLAG_CONGESTED;

            /* notify that this data link is congested */
            evt_data.congest.link_type    = NFA_P2P_DLINK_TYPE;
            evt_data.congest.handle       = (NFA_HANDLE_GROUP_P2P | NFA_P2P_HANDLE_FLAG_CONN | xx);
            evt_data.congest.is_congested = TRUE;

            nfa_p2p_cb.sap_cb[nfa_p2p_cb.conn_cb[xx].local_sap].p_cback (NFA_P2P_CONGEST_EVT, &evt_data);
        }
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_set_local_busy
**
** Description      Set or reset local busy
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_set_local_busy (tNFA_P2P_MSG *p_msg)
{
    UINT8 xx;

    P2P_TRACE_DEBUG0 ("nfa_p2p_set_local_busy ()");

    xx = (UINT8) (p_msg->api_local_busy.conn_handle & NFA_HANDLE_MASK);
    xx &= ~NFA_P2P_HANDLE_FLAG_CONN;

    LLCP_SetLocalBusyStatus (nfa_p2p_cb.conn_cb[xx].local_sap,
                             nfa_p2p_cb.conn_cb[xx].remote_sap,
                             p_msg->api_local_busy.is_busy);

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_get_link_info
**
** Description      Get WKS of remote and link MIU
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_get_link_info (tNFA_P2P_MSG *p_msg)
{
    tNFA_P2P_EVT_DATA evt_data;
    UINT8             local_sap;

    P2P_TRACE_DEBUG0 ("nfa_p2p_get_link_info ()");

    evt_data.link_info.handle          = p_msg->api_link_info.handle;
    evt_data.link_info.wks             = LLCP_GetRemoteWKS ();
    evt_data.link_info.local_link_miu  = nfa_p2p_cb.local_link_miu;
    evt_data.link_info.remote_link_miu = nfa_p2p_cb.remote_link_miu;

    local_sap =  (UINT8) (p_msg->api_link_info.handle & NFA_HANDLE_MASK);
    nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_LINK_INFO_EVT, &evt_data);

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_get_remote_sap
**
** Description      Get remote SAP
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_get_remote_sap (tNFA_P2P_MSG *p_msg)
{
    tNFA_P2P_EVT_DATA evt_data;
    UINT8             local_sap;

    P2P_TRACE_DEBUG0 ("nfa_p2p_get_remote_sap ()");

    local_sap =  (UINT8) (p_msg->api_remote_sap.handle & NFA_HANDLE_MASK);

    if (!nfa_p2p_start_sdp (p_msg->api_remote_sap.service_name,
                            local_sap))
    {
        evt_data.sdp.handle     = p_msg->api_remote_sap.handle;
        evt_data.sdp.remote_sap = 0x00;
        nfa_p2p_cb.sap_cb[local_sap].p_cback (NFA_P2P_SDP_EVT, &evt_data);
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_set_llcp_cfg
**
** Description      Set LLCP configuration
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_set_llcp_cfg (tNFA_P2P_MSG *p_msg)
{
    LLCP_SetConfig (p_msg->api_set_llcp_cfg.link_miu,
                    p_msg->api_set_llcp_cfg.opt,
                    p_msg->api_set_llcp_cfg.wt,
                    p_msg->api_set_llcp_cfg.link_timeout,
                    p_msg->api_set_llcp_cfg.inact_timeout_init,
                    p_msg->api_set_llcp_cfg.inact_timeout_target,
                    p_msg->api_set_llcp_cfg.symm_delay,
                    p_msg->api_set_llcp_cfg.data_link_timeout,
                    p_msg->api_set_llcp_cfg.delay_first_pdu_timeout);

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_p2p_restart_rf_discovery
**
** Description      Restart RF discovery by deactivating to IDLE
**
**
** Returns          TRUE to deallocate buffer
**
*******************************************************************************/
BOOLEAN nfa_p2p_restart_rf_discovery (tNFA_P2P_MSG *p_msg)
{
    P2P_TRACE_DEBUG0 ("nfa_p2p_restart_rf_discovery ()");

    nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_IDLE);

    return TRUE;
}
