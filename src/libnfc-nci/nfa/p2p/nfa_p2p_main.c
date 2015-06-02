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
 *  This is the main implementation file for the NFA P2P.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfa_dm_int.h"
#include "llcp_api.h"
#include "llcp_defs.h"
#include "nfa_p2p_api.h"
#include "nfa_p2p_int.h"

/*****************************************************************************
**  Global Variables
*****************************************************************************/

/* system manager control block definition */
#if NFA_DYNAMIC_MEMORY == FALSE
tNFA_P2P_CB nfa_p2p_cb;
#endif

/*****************************************************************************
**  Static Functions
*****************************************************************************/

/* event handler function type */
static BOOLEAN nfa_p2p_evt_hdlr (BT_HDR *p_msg);

/* disable function type */
static void nfa_p2p_sys_disable (void);
static void nfa_p2p_update_active_listen (void);

/* debug functions type */
#if (BT_TRACE_VERBOSE == TRUE)
static char *nfa_p2p_llcp_state_code (tNFA_P2P_LLCP_STATE state_code);
#endif

/*****************************************************************************
**  Constants
*****************************************************************************/
/* timeout to restore active listen mode if no RF activation on passive mode */
#define NFA_P2P_RESTORE_ACTIVE_LISTEN_TIMEOUT   5000

static const tNFA_SYS_REG nfa_p2p_sys_reg =
{
    NULL,
    nfa_p2p_evt_hdlr,
    nfa_p2p_sys_disable,
    NULL
};

#define NFA_P2P_NUM_ACTIONS  (NFA_P2P_LAST_EVT & 0x00ff)

/* type for action functions */
typedef BOOLEAN (*tNFA_P2P_ACTION) (tNFA_P2P_MSG *p_data);

/* action function list */
const tNFA_P2P_ACTION nfa_p2p_action[] =
{
    nfa_p2p_reg_server,                     /* NFA_P2P_API_REG_SERVER_EVT       */
    nfa_p2p_reg_client,                     /* NFA_P2P_API_REG_CLIENT_EVT       */
    nfa_p2p_dereg,                          /* NFA_P2P_API_DEREG_EVT            */
    nfa_p2p_accept_connection,              /* NFA_P2P_API_ACCEPT_CONN_EVT      */
    nfa_p2p_reject_connection,              /* NFA_P2P_API_REJECT_CONN_EVT      */
    nfa_p2p_disconnect,                     /* NFA_P2P_API_DISCONNECT_EVT       */
    nfa_p2p_create_data_link_connection,    /* NFA_P2P_API_CONNECT_EVT          */
    nfa_p2p_send_ui,                        /* NFA_P2P_API_SEND_UI_EVT          */
    nfa_p2p_send_data,                      /* NFA_P2P_API_SEND_DATA_EVT        */
    nfa_p2p_set_local_busy,                 /* NFA_P2P_API_SET_LOCAL_BUSY_EVT   */
    nfa_p2p_get_link_info,                  /* NFA_P2P_API_GET_LINK_INFO_EVT    */
    nfa_p2p_get_remote_sap,                 /* NFA_P2P_API_GET_REMOTE_SAP_EVT   */
    nfa_p2p_set_llcp_cfg,                   /* NFA_P2P_API_SET_LLCP_CFG_EVT     */
    nfa_p2p_restart_rf_discovery            /* NFA_P2P_INT_RESTART_RF_DISC_EVT  */
};

/*******************************************************************************
**
** Function         nfa_p2p_discovery_cback
**
** Description      Processing event from discovery callback for listening
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_discovery_cback (tNFA_DM_RF_DISC_EVT event, tNFC_DISCOVER *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    P2P_TRACE_DEBUG1 ("nfa_p2p_discovery_cback (): event:0x%02X", event);

    switch (event)
    {
    case NFA_DM_RF_DISC_START_EVT:
        if (p_data->status == NFC_STATUS_OK)
        {
            nfa_p2p_cb.llcp_state    = NFA_P2P_LLCP_STATE_LISTENING;
            nfa_p2p_cb.rf_disc_state = NFA_DM_RFST_DISCOVERY;
        }
        break;

    case NFA_DM_RF_DISC_ACTIVATED_EVT:

        nfa_p2p_cb.rf_disc_state = NFA_DM_RFST_LISTEN_ACTIVE;

        /* notify NFC link activation */
        memcpy (&(evt_data.activated.activate_ntf),
                &(p_data->activate),
                sizeof (tNFC_ACTIVATE_DEVT));
        nfa_dm_conn_cback_event_notify (NFA_ACTIVATED_EVT, &evt_data);

        if ((p_data->activate.protocol        == NFC_PROTOCOL_NFC_DEP)
          &&((p_data->activate.intf_param.type == NFC_INTERFACE_NFC_DEP)))
        {
            nfa_p2p_activate_llcp (p_data);

            /* stop timer not to deactivate LLCP link on passive mode */
            nfa_sys_stop_timer (&nfa_p2p_cb.active_listen_restore_timer);
        }
        break;

    case NFA_DM_RF_DISC_DEACTIVATED_EVT:

        if (  (nfa_p2p_cb.rf_disc_state != NFA_DM_RFST_LISTEN_ACTIVE)
            &&(nfa_p2p_cb.rf_disc_state != NFA_DM_RFST_LISTEN_SLEEP)  )
        {
            /* this is not for P2P listen
            ** DM broadcasts deactivaiton event in listen sleep state.
            */
            break;
        }

        /* notify deactivation */
        if (  (p_data->deactivate.type == NFC_DEACTIVATE_TYPE_SLEEP)
            ||(p_data->deactivate.type == NFC_DEACTIVATE_TYPE_SLEEP_AF)  )
        {
            nfa_p2p_cb.rf_disc_state  = NFA_DM_RFST_LISTEN_SLEEP;
            evt_data.deactivated.type = NFA_DEACTIVATE_TYPE_SLEEP;
        }
        else
        {
            nfa_p2p_cb.rf_disc_state  = NFA_DM_RFST_DISCOVERY;
            evt_data.deactivated.type = NFA_DEACTIVATE_TYPE_IDLE;
        }
        nfa_dm_conn_cback_event_notify (NFA_DEACTIVATED_EVT, &evt_data);
        break;

    default:
        P2P_TRACE_ERROR0 ("Unexpected event");
        break;
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_update_active_listen_timeout_cback
**
** Description      Timeout while waiting for passive mode activation
**
** Returns          void
**
*******************************************************************************/
static void nfa_p2p_update_active_listen_timeout_cback (TIMER_LIST_ENT *p_tle)
{
    NFA_TRACE_ERROR0 ("nfa_p2p_update_active_listen_timeout_cback()");

    /* restore active listen mode */
    nfa_p2p_update_active_listen ();
}

/*******************************************************************************
**
** Function         nfa_p2p_update_active_listen
**
** Description      Remove active listen mode temporarily or restore it
**
**
** Returns          None
**
*******************************************************************************/
static void nfa_p2p_update_active_listen (void)
{
    tNFA_DM_DISC_TECH_PROTO_MASK p2p_listen_mask = 0;
    BT_HDR *p_msg;

    P2P_TRACE_DEBUG1 ("nfa_p2p_update_active_listen (): listen_tech_mask_to_restore:0x%x",
                       nfa_p2p_cb.listen_tech_mask_to_restore);

    /* if active listen mode was removed temporarily */
    if (nfa_p2p_cb.listen_tech_mask_to_restore)
    {
        /* restore listen technologies */
        nfa_p2p_cb.listen_tech_mask = nfa_p2p_cb.listen_tech_mask_to_restore;
        nfa_p2p_cb.listen_tech_mask_to_restore = 0;
        nfa_sys_stop_timer (&nfa_p2p_cb.active_listen_restore_timer);
    }
    else
    {
        /* start timer in case of no passive activation */
        nfa_p2p_cb.active_listen_restore_timer.p_cback = (TIMER_CBACK *)nfa_p2p_update_active_listen_timeout_cback;
        nfa_sys_start_timer (&nfa_p2p_cb.active_listen_restore_timer, 0, NFA_P2P_RESTORE_ACTIVE_LISTEN_TIMEOUT);

        /* save listen techonologies */
        nfa_p2p_cb.listen_tech_mask_to_restore = nfa_p2p_cb.listen_tech_mask;

        /* remove active listen mode */
        nfa_p2p_cb.listen_tech_mask &= ~( NFA_TECHNOLOGY_MASK_A_ACTIVE|NFA_TECHNOLOGY_MASK_F_ACTIVE);
    }

    if (nfa_p2p_cb.dm_disc_handle != NFA_HANDLE_INVALID)
    {
        nfa_dm_delete_rf_discover (nfa_p2p_cb.dm_disc_handle);
        nfa_p2p_cb.dm_disc_handle = NFA_HANDLE_INVALID;
    }

    /* collect listen technologies with NFC-DEP protocol */
    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_A)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LA_NFC_DEP;

    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_F)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LF_NFC_DEP;

    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_A_ACTIVE)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LAA_NFC_DEP;

    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_F_ACTIVE)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LFA_NFC_DEP;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    /*For P2P mode(Default DTA mode) open Raw channel to bypass LLCP layer. For LLCP DTA mode activate LLCP
     * Bypassing LLCP is handled in nfa_dm_poll_disc_cback*/
    if ((appl_dta_mode_flag == 1) && (nfa_dm_cb.eDtaMode == NFA_DTA_DEFAULT_MODE))
    {
        /* Configure listen technologies and protocols and register callback to NFA DM discovery */
        P2P_TRACE_DEBUG0 ("DTA mode1:Registering nfa_dm_poll_disc_cback to avoid LLCP in P2P ");
        nfa_p2p_cb.dm_disc_handle = nfa_dm_add_rf_discover (p2p_listen_mask,
                                                            NFA_DM_DISC_HOST_ID_DH,
                                                            nfa_dm_poll_disc_cback_dta_wrapper);
    }
    else
#endif
    {
        /* Configure listen technologies and protocols and register callback to NFA DM discovery */
        nfa_p2p_cb.dm_disc_handle = nfa_dm_add_rf_discover (p2p_listen_mask,
                                                            NFA_DM_DISC_HOST_ID_DH,
                                                            nfa_p2p_discovery_cback);
    }

    /* restart RF discovery to update RF technologies */
    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof(BT_HDR))) != NULL)
    {
        p_msg->event = NFA_P2P_INT_RESTART_RF_DISC_EVT;
        nfa_sys_sendmsg (p_msg);
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_llcp_link_cback
**
** Description      Processing event from LLCP link management callback
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_llcp_link_cback (UINT8 event, UINT8 reason)
{
    tNFA_LLCP_ACTIVATED     llcp_activated;
    tNFA_LLCP_DEACTIVATED   llcp_deactivated;

    P2P_TRACE_DEBUG2 ("nfa_p2p_llcp_link_cback () event:0x%x, reason:0x%x", event, reason);

    if (event == LLCP_LINK_ACTIVATION_COMPLETE_EVT)
    {
        LLCP_GetLinkMIU (&nfa_p2p_cb.local_link_miu, &nfa_p2p_cb.remote_link_miu);
        nfa_p2p_cb.llcp_state = NFA_P2P_LLCP_STATE_ACTIVATED;

        if (nfa_p2p_cb.is_initiator)
        {
            /* notify NFA DM to send Activate Event to applicaiton with status  */
            nfa_dm_notify_activation_status (NFA_STATUS_OK, NULL);
        }

        llcp_activated.is_initiator    = nfa_p2p_cb.is_initiator;
        llcp_activated.local_link_miu  = nfa_p2p_cb.local_link_miu;
        llcp_activated.remote_link_miu = nfa_p2p_cb.remote_link_miu;
        llcp_activated.remote_lsc      = LLCP_GetRemoteLSC ();
        llcp_activated.remote_wks      = LLCP_GetRemoteWKS ();

        nfa_dm_act_conn_cback_notify (NFA_LLCP_ACTIVATED_EVT, (tNFA_CONN_EVT_DATA *) &llcp_activated);

    }
    else if (event == LLCP_LINK_ACTIVATION_FAILED_EVT)
    {
        nfa_p2p_cb.llcp_state = NFA_P2P_LLCP_STATE_IDLE;

        if (nfa_p2p_cb.is_initiator)
        {
            /* notify NFA DM to send Activate Event to applicaiton with status  */
            nfa_dm_notify_activation_status (NFA_STATUS_FAILED, NULL);
        }

        nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_DISCOVERY);
    }
    else if (event == LLCP_LINK_FIRST_PACKET_RECEIVED_EVT)
    {
        nfa_dm_act_conn_cback_notify (NFA_LLCP_FIRST_PACKET_RECEIVED_EVT, NULL);
    }
    else /* LLCP_LINK_DEACTIVATED_EVT       */
    {
        nfa_p2p_cb.llcp_state = NFA_P2P_LLCP_STATE_IDLE;

        /* if got RF link loss without any rx LLC PDU */
        if (reason == LLCP_LINK_RF_LINK_LOSS_NO_RX_LLC)
        {
            /* if it was active listen mode */
            if (  (nfa_p2p_cb.is_active_mode)
                &&(!nfa_p2p_cb.is_initiator))
            {
                /* if it didn't retry without active listen mode and passive mode is available */
                if (  (nfa_p2p_cb.listen_tech_mask_to_restore == 0x00)
                    &&(nfa_p2p_cb.listen_tech_mask & ( NFA_TECHNOLOGY_MASK_A
                                                      |NFA_TECHNOLOGY_MASK_F)))
                {
                    P2P_TRACE_DEBUG0 ("Retry without active listen mode");

                    /* retry without active listen mode */
                    nfa_p2p_update_active_listen ();
                }
            }
            else if (nfa_p2p_cb.listen_tech_mask_to_restore)
            {
                nfa_sys_start_timer (&nfa_p2p_cb.active_listen_restore_timer, 0, NFA_P2P_RESTORE_ACTIVE_LISTEN_TIMEOUT);
            }

            reason = LLCP_LINK_RF_LINK_LOSS_ERR;
        }
        else
        {
            if (nfa_p2p_cb.listen_tech_mask_to_restore)
            {
                /* restore active listen mode */
                nfa_p2p_update_active_listen ();
            }
        }

        llcp_deactivated.reason = reason;
        nfa_dm_act_conn_cback_notify (NFA_LLCP_DEACTIVATED_EVT, (tNFA_CONN_EVT_DATA *)&llcp_deactivated);

        if (reason != LLCP_LINK_RF_LINK_LOSS_ERR) /* if NFC link is still up */
        {
            if (nfa_p2p_cb.is_initiator)
            {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
               /*For LLCP DTA test, Deactivate to Sleep is needed to send DSL_REQ*/
               if((appl_dta_mode_flag == 1) && (nfa_dm_cb.eDtaMode == NFA_DTA_LLCP_MODE))
               {
                   nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_SLEEP);
               }
               else
               {
#endif
                   nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_DISCOVERY);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
               }
#endif
            }
            else if ((nfa_p2p_cb.is_active_mode) && (reason == LLCP_LINK_TIMEOUT))
            {
                /*
                ** target needs to trun off RF in case of receiving invalid frame from initiator
                */
                P2P_TRACE_DEBUG0 ("Got LLCP_LINK_TIMEOUT in active mode on target");
                nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_DISCOVERY);
            }
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_activate_llcp
**
** Description      Activate LLCP link
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_activate_llcp (tNFC_DISCOVER *p_data)
{
    tLLCP_ACTIVATE_CONFIG config;

    P2P_TRACE_DEBUG0 ("nfa_p2p_activate_llcp ()");

    if (  (p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A)
        ||(p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_F)
        ||(p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE)
        ||(p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE)  )
    {
        config.is_initiator = TRUE;
    }
    else
    {
        config.is_initiator = FALSE;
    }

    if (  (p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE)
        ||(p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE)
        ||(p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE)
        ||(p_data->activate.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE)  )
    {
        nfa_p2p_cb.is_active_mode = TRUE;
    }
    else
    {
        nfa_p2p_cb.is_active_mode = FALSE;
    }

    nfa_p2p_cb.is_initiator = config.is_initiator;

    config.max_payload_size = p_data->activate.intf_param.intf_param.pa_nfc.max_payload_size;
    config.waiting_time     = p_data->activate.intf_param.intf_param.pa_nfc.waiting_time;
    config.p_gen_bytes      = p_data->activate.intf_param.intf_param.pa_nfc.gen_bytes;
    config.gen_bytes_len    = p_data->activate.intf_param.intf_param.pa_nfc.gen_bytes_len;

    LLCP_ActivateLink (config, nfa_p2p_llcp_link_cback);
}

/*******************************************************************************
**
** Function         nfa_p2p_deactivate_llcp
**
** Description      Deactivate LLCP link
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_deactivate_llcp (void)
{
    P2P_TRACE_DEBUG0 ("nfa_p2p_deactivate_llcp ()");

    LLCP_DeactivateLink ();
}

/*******************************************************************************
**
** Function         nfa_p2p_init
**
** Description      Initialize NFA P2P
**
**
** Returns          None
**
*******************************************************************************/
void nfa_p2p_init (void)
{
    UINT8 xx;

    P2P_TRACE_DEBUG0 ("nfa_p2p_init ()");

    /* initialize control block */
    memset (&nfa_p2p_cb, 0, sizeof (tNFA_P2P_CB));
    nfa_p2p_cb.dm_disc_handle = NFA_HANDLE_INVALID;
    nfa_p2p_cb.trace_level    = APPL_INITIAL_TRACE_LEVEL;

    for (xx = 0; xx < LLCP_MAX_SDP_TRANSAC; xx++)
    {
        nfa_p2p_cb.sdp_cb[xx].local_sap = LLCP_INVALID_SAP;
    }

    /* register message handler on NFA SYS */
    nfa_sys_register (NFA_ID_P2P,  &nfa_p2p_sys_reg);
}


/*******************************************************************************
**
** Function         nfa_p2p_sys_disable
**
** Description      Deregister NFA P2P from NFA SYS/DM
**
**
** Returns          None
**
*******************************************************************************/
static void nfa_p2p_sys_disable (void)
{
    P2P_TRACE_DEBUG0 ("nfa_p2p_sys_disable()");

    nfa_sys_stop_timer (&nfa_p2p_cb.active_listen_restore_timer);

    /* deregister message handler on NFA SYS */
    nfa_sys_deregister (NFA_ID_P2P);
}

/*******************************************************************************
**
** Function         nfa_p2p_set_config
**
** Description      Set General bytes and WT parameters for LLCP
**
**
** Returns          void
**
*******************************************************************************/
void nfa_p2p_set_config (tNFA_DM_DISC_TECH_PROTO_MASK disc_mask)
{
    UINT8 wt, gen_bytes_len = LLCP_MAX_GEN_BYTES;
    UINT8 params[LLCP_MAX_GEN_BYTES + 5], *p, length;

    P2P_TRACE_DEBUG0 ("nfa_p2p_set_config ()");

    LLCP_GetDiscoveryConfig (&wt, params + 2, &gen_bytes_len);
    if (nfa_dm_is_p2p_paused ())
    {
        gen_bytes_len = 0;
    }

    if (disc_mask & ( NFA_DM_DISC_MASK_PA_NFC_DEP
                     |NFA_DM_DISC_MASK_PF_NFC_DEP
                     |NFA_DM_DISC_MASK_PAA_NFC_DEP
                     |NFA_DM_DISC_MASK_PFA_NFC_DEP) )
    {
        p = params;

        UINT8_TO_BE_STREAM (p, NFC_PMID_ATR_REQ_GEN_BYTES);
        UINT8_TO_BE_STREAM (p, gen_bytes_len);

        p += gen_bytes_len;
        length = gen_bytes_len + 2;

        nfa_dm_check_set_config (length, params, FALSE);
    }

    if (disc_mask & ( NFA_DM_DISC_MASK_LA_NFC_DEP
                     |NFA_DM_DISC_MASK_LF_NFC_DEP
                     |NFA_DM_DISC_MASK_LAA_NFC_DEP
                     |NFA_DM_DISC_MASK_LFA_NFC_DEP) )
    {
        p = params;

        UINT8_TO_BE_STREAM (p, NFC_PMID_ATR_RES_GEN_BYTES);
        UINT8_TO_BE_STREAM (p, gen_bytes_len);

        p += gen_bytes_len;
        length = gen_bytes_len + 2;

        UINT8_TO_BE_STREAM (p, NFC_PMID_WT);
        UINT8_TO_BE_STREAM (p, NCI_PARAM_LEN_WT);
        UINT8_TO_BE_STREAM (p, wt);

        length += 3;

        nfa_dm_check_set_config (length, params, FALSE);
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_enable_listening
**
** Description      Configure listen technologies and protocols for LLCP
**                  If LLCP WKS is changed then LLCP Gen bytes will be updated.
**
** Returns          void
**
*******************************************************************************/
void nfa_p2p_enable_listening (tNFA_SYS_ID sys_id, BOOLEAN update_wks)
{
    tNFA_DM_DISC_TECH_PROTO_MASK p2p_listen_mask = 0;

    P2P_TRACE_DEBUG2 ("nfa_p2p_enable_listening () sys_id = %d, update_wks = %d",
                       sys_id, update_wks);

    if (sys_id == NFA_ID_P2P)
        nfa_p2p_cb.is_p2p_listening = TRUE;
    else if (sys_id == NFA_ID_CHO)
        nfa_p2p_cb.is_cho_listening = TRUE;
    else if (sys_id == NFA_ID_SNEP)
        nfa_p2p_cb.is_snep_listening = TRUE;

    if (nfa_p2p_cb.dm_disc_handle != NFA_HANDLE_INVALID)
    {
        /* if need to update WKS in LLCP Gen bytes */
        if (update_wks)
        {
            /* update LLCP Gen Bytes */
            nfa_p2p_set_config (NFA_DM_DISC_MASK_PA_NFC_DEP|NFA_DM_DISC_MASK_LA_NFC_DEP);
        }
        return;
    }

    /* collect listen technologies with NFC-DEP protocol */
    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_A)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LA_NFC_DEP;

    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_F)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LF_NFC_DEP;

    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_A_ACTIVE)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LAA_NFC_DEP;

    if (nfa_p2p_cb.listen_tech_mask & NFA_TECHNOLOGY_MASK_F_ACTIVE)
        p2p_listen_mask |= NFA_DM_DISC_MASK_LFA_NFC_DEP;

    if (p2p_listen_mask)
    {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        /*For P2P mode(Default DTA mode) open Raw channel to bypass LLCP layer. For LLCP DTA mode activate LLCP
         * Bypassing LLCP is handled in nfa_dm_poll_disc_cback*/
        if ((appl_dta_mode_flag == 1) && (nfa_dm_cb.eDtaMode == NFA_DTA_DEFAULT_MODE))
        {
            /* Configure listen technologies and protocols and register callback to NFA DM discovery */
            P2P_TRACE_DEBUG0 ("DTA mode2:Registering nfa_dm_poll_disc_cback to avoid LLCP in P2P ");
            nfa_p2p_cb.dm_disc_handle = nfa_dm_add_rf_discover (p2p_listen_mask,
                                                                NFA_DM_DISC_HOST_ID_DH,
                                                                nfa_dm_poll_disc_cback_dta_wrapper);
        }
        else
#endif
        {
            /* Configure listen technologies and protocols and register callback to NFA DM discovery */
            nfa_p2p_cb.dm_disc_handle = nfa_dm_add_rf_discover (p2p_listen_mask,
                                                                NFA_DM_DISC_HOST_ID_DH,
                                                                nfa_p2p_discovery_cback);
        }

    }
}

/*******************************************************************************
**
** Function         nfa_p2p_disable_listening
**
** Description      Remove listen technologies and protocols for LLCP and
**                  deregister callback from NFA DM discovery if all of P2P/CHO/SNEP
**                  doesn't listen LLCP any more.
**                  If LLCP WKS is changed then ATR_RES will be updated.
**
** Returns          void
**
*******************************************************************************/
void nfa_p2p_disable_listening (tNFA_SYS_ID sys_id, BOOLEAN update_wks)
{

    P2P_TRACE_DEBUG2 ("nfa_p2p_disable_listening ()  sys_id = %d, update_wks = %d",
                       sys_id, update_wks);

    if (sys_id == NFA_ID_P2P)
        nfa_p2p_cb.is_p2p_listening = FALSE;
    else if (sys_id == NFA_ID_CHO)
        nfa_p2p_cb.is_cho_listening = FALSE;
    else if (sys_id == NFA_ID_SNEP)
        nfa_p2p_cb.is_snep_listening = FALSE;

    if (nfa_p2p_cb.dm_disc_handle != NFA_HANDLE_INVALID)
    {
        if (  (nfa_p2p_cb.is_p2p_listening == FALSE)
            &&(nfa_p2p_cb.is_cho_listening == FALSE)
            &&(nfa_p2p_cb.is_snep_listening == FALSE)  )
        {
            nfa_p2p_cb.llcp_state    = NFA_P2P_LLCP_STATE_IDLE;
            nfa_p2p_cb.rf_disc_state = NFA_DM_RFST_IDLE;

            nfa_dm_delete_rf_discover (nfa_p2p_cb.dm_disc_handle);
            nfa_p2p_cb.dm_disc_handle = NFA_HANDLE_INVALID;
        }
        else if (update_wks)
        {
            /* update LLCP Gen Bytes */
            nfa_p2p_set_config (NFA_DM_DISC_MASK_PA_NFC_DEP|NFA_DM_DISC_MASK_LA_NFC_DEP);
        }
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_update_listen_tech
**
** Description      Update P2P listen technologies. If there is change then
**                  restart or stop P2P listen.
**
** Returns          void
**
*******************************************************************************/
void nfa_p2p_update_listen_tech (tNFA_TECHNOLOGY_MASK tech_mask)
{
    P2P_TRACE_DEBUG1 ("nfa_p2p_update_listen_tech ()  tech_mask = 0x%x", tech_mask);

    if (nfa_p2p_cb.listen_tech_mask_to_restore)
    {
        nfa_p2p_cb.listen_tech_mask_to_restore = 0;
        nfa_sys_stop_timer (&nfa_p2p_cb.active_listen_restore_timer);
    }

    if (nfa_p2p_cb.listen_tech_mask != tech_mask)
    {
        nfa_p2p_cb.listen_tech_mask = tech_mask;

        if (nfa_p2p_cb.dm_disc_handle != NFA_HANDLE_INVALID)
        {
            nfa_p2p_cb.rf_disc_state = NFA_DM_RFST_IDLE;

            nfa_dm_delete_rf_discover (nfa_p2p_cb.dm_disc_handle);
            nfa_p2p_cb.dm_disc_handle = NFA_HANDLE_INVALID;
        }

        /* restart discovery without updating sub-module status */
        if (nfa_p2p_cb.is_p2p_listening
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                || appl_dta_mode_flag
#endif
            )
            nfa_p2p_enable_listening (NFA_ID_P2P, FALSE);
        else if (nfa_p2p_cb.is_cho_listening)
            nfa_p2p_enable_listening (NFA_ID_CHO, FALSE);
        else if (nfa_p2p_cb.is_snep_listening)
            nfa_p2p_enable_listening (NFA_ID_SNEP, FALSE);
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_evt_hdlr
**
** Description      Processing event for NFA P2P
**
**
** Returns          TRUE if p_msg needs to be deallocated
**
*******************************************************************************/
static BOOLEAN nfa_p2p_evt_hdlr (BT_HDR *p_hdr)
{
    BOOLEAN delete_msg = TRUE;
    UINT16  event;

    tNFA_P2P_MSG *p_msg = (tNFA_P2P_MSG *)p_hdr;

#if (BT_TRACE_VERBOSE == TRUE)
    P2P_TRACE_DEBUG2 ("nfa_p2p_evt_hdlr (): LLCP State [%s], Event [%s]",
                       nfa_p2p_llcp_state_code (nfa_p2p_cb.llcp_state),
                       nfa_p2p_evt_code (p_msg->hdr.event));
#else
    P2P_TRACE_DEBUG2 ("nfa_p2p_evt_hdlr (): State 0x%02x, Event 0x%02x",
                       nfa_p2p_cb.llcp_state, p_msg->hdr.event);
#endif

    event = p_msg->hdr.event & 0x00ff;

    /* execute action functions */
    if (event < NFA_P2P_NUM_ACTIONS)
    {
        delete_msg = (*nfa_p2p_action[event]) (p_msg);
    }
    else
    {
        P2P_TRACE_ERROR0 ("Unhandled event");
    }

    return delete_msg;
}


#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         nfa_p2p_llcp_state_code
**
** Description
**
** Returns          string of state
**
*******************************************************************************/
static char *nfa_p2p_llcp_state_code (tNFA_P2P_LLCP_STATE state_code)
{
    switch (state_code)
    {
    case NFA_P2P_LLCP_STATE_IDLE:
        return "Link IDLE";
    case NFA_P2P_LLCP_STATE_LISTENING:
        return "Link LISTENING";
    case NFA_P2P_LLCP_STATE_ACTIVATED:
        return "Link ACTIVATED";
    default:
        return "Unknown state";
    }
}

/*******************************************************************************
**
** Function         nfa_p2p_evt_code
**
** Description
**
** Returns          string of event
**
*******************************************************************************/
char *nfa_p2p_evt_code (UINT16 evt_code)
{
    switch (evt_code)
    {
    case NFA_P2P_API_REG_SERVER_EVT:
        return "API_REG_SERVER";
    case NFA_P2P_API_REG_CLIENT_EVT:
        return "API_REG_CLIENT";
    case NFA_P2P_API_DEREG_EVT:
        return "API_DEREG";
    case NFA_P2P_API_ACCEPT_CONN_EVT:
        return "API_ACCEPT_CONN";
    case NFA_P2P_API_REJECT_CONN_EVT:
        return "API_REJECT_CONN";
    case NFA_P2P_API_DISCONNECT_EVT:
        return "API_DISCONNECT";
    case NFA_P2P_API_CONNECT_EVT:
        return "API_CONNECT";
    case NFA_P2P_API_SEND_UI_EVT:
        return "API_SEND_UI";
    case NFA_P2P_API_SEND_DATA_EVT:
        return "API_SEND_DATA";
    case NFA_P2P_API_SET_LOCAL_BUSY_EVT:
        return "API_SET_LOCAL_BUSY";
    case NFA_P2P_API_GET_LINK_INFO_EVT:
        return "API_GET_LINK_INFO";
    case NFA_P2P_API_GET_REMOTE_SAP_EVT:
        return "API_GET_REMOTE_SAP";
    case NFA_P2P_API_SET_LLCP_CFG_EVT:
        return "API_SET_LLCP_CFG_EVT";
    case NFA_P2P_INT_RESTART_RF_DISC_EVT:
        return "RESTART_RF_DISC_EVT";
    default:
        return "Unknown event";
    }
}
#endif  /* Debug Functions */
