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
 *  This file contains LLCP internal definitions
 *
 ******************************************************************************/
#ifndef LLCP_INT_H
#define LLCP_INT_H

#include "llcp_api.h"
#include "nfc_api.h"

/*
** LLCP link states
*/
enum
{
    LLCP_LINK_STATE_DEACTIVATED,                /* llcp link is deactivated     */
    LLCP_LINK_STATE_ACTIVATED,                  /* llcp link has been activated */
    LLCP_LINK_STATE_DEACTIVATING,               /* llcp link is deactivating    */
    LLCP_LINK_STATE_ACTIVATION_FAILED           /* llcp link activation was failed */
};
typedef UINT8 tLLCP_LINK_STATE;

/*
** LLCP Symmetric state
*/

#define LLCP_LINK_SYMM_LOCAL_XMIT_NEXT   0
#define LLCP_LINK_SYMM_REMOTE_XMIT_NEXT  1

#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
typedef struct {
    TIMER_LIST_ENT    dps_timer;                /* dpspdu timer */
    UINT16            dps_delay;                /* Delay DPS response */
    BOOLEAN           dps_state;                /* dps state */
    BOOLEAN           p2p_flag;                 /* p2p flag */
    BT_HDR            *dps_msg;                 /* dps message */
} tLLCP_SECURED;
tLLCP_SECURED llcp_secured;
#endif


/*
** LLCP internal flags
*/
#define LLCP_LINK_FLAGS_RX_ANY_LLC_PDU      0x01    /* Received any LLC PDU in activated state */

/*
** LLCP link control block
*/
typedef struct
{
    tLLCP_LINK_STATE    link_state;             /* llcp link state                              */
    tLLCP_LINK_CBACK   *p_link_cback;           /* callback function to report llcp link status */
    UINT16              wks;                    /* well-known service bit-map                   */

    BOOLEAN             is_initiator;           /* TRUE if initiator role                       */
    BOOLEAN             is_sending_data;        /* TRUE if llcp_link_check_send_data() is excuting    */
    UINT8               flags;                  /* LLCP internal flags                          */
    BOOLEAN             received_first_packet;  /* TRUE if a packet has been received from remote */
    UINT8               agreed_major_version;   /* llcp major version used in activated state   */
    UINT8               agreed_minor_version;   /* llcp minor version used in activated state   */

    UINT8               peer_version;           /* llcp version of peer device                  */
    UINT16              peer_miu;               /* link MIU of peer device                      */
    UINT16              peer_wks;               /* WKS of peer device                           */
    UINT16              peer_lto;               /* link timeout of peer device in ms            */
    UINT8               peer_opt;               /* Option field of peer device                  */
    UINT16              effective_miu;          /* MIU to send PDU in activated state           */

    TIMER_LIST_ENT      timer;                  /* link timer for LTO and SYMM response         */
    UINT8               symm_state;             /* state of symmectric procedure                */
    BOOLEAN             ll_served;              /* TRUE if last transmisstion was for UI        */
    UINT8               ll_idx;                 /* for scheduler of logical link connection     */
    UINT8               dl_idx;                 /* for scheduler of data link connection        */

    TIMER_LIST_ENT      inact_timer;            /* inactivity timer                             */
    UINT16              inact_timeout;          /* inactivity timeout in ms                     */

    UINT8               link_deact_reason;      /* reason of LLCP link deactivated              */

    BUFFER_Q            sig_xmit_q;             /* tx signaling PDU queue                       */

    /* runtime configuration parameters */
    UINT16              local_link_miu;         /* Maximum Information Unit                     */
    UINT8               local_opt;              /* Option parameter                             */
    UINT8               local_wt;               /* Response Waiting Time Index                  */
    UINT16              local_lto;              /* Local Link Timeout                           */
    UINT16              inact_timeout_init;     /* Inactivity Timeout as initiator role         */
    UINT16              inact_timeout_target;   /* Inactivity Timeout as target role            */
    UINT16              symm_delay;             /* Delay SYMM response                          */
    UINT16              data_link_timeout;      /* data link conneciton timeout                 */
    UINT16              delay_first_pdu_timeout;/* delay timeout to send first PDU as initiator */
} tLLCP_LCB;

/*
** LLCP Application's registration control block on service access point (SAP)
*/

typedef struct
{
    UINT8               link_type;              /* logical link and/or data link                */
    UINT8               *p_service_name;        /* GKI buffer containing service name           */
    tLLCP_APP_CBACK     *p_app_cback;           /* application's callback pointer               */

    BUFFER_Q            ui_xmit_q;              /* UI PDU queue for transmitting                */
    BUFFER_Q            ui_rx_q;                /* UI PDU queue for receiving                   */
    BOOLEAN             is_ui_tx_congested;     /* TRUE if transmitting UI PDU is congested     */

} tLLCP_APP_CB;

/*
** LLCP data link connection states
*/
enum
{
    LLCP_DLC_STATE_IDLE,            /* initial state                                    */
    LLCP_DLC_STATE_W4_REMOTE_RESP,  /* waiting for connection confirm from peer         */
    LLCP_DLC_STATE_W4_LOCAL_RESP,   /* waiting for connection confirm from upper layer  */
    LLCP_DLC_STATE_CONNECTED,       /* data link connection has been established        */
    LLCP_DLC_STATE_W4_REMOTE_DM,    /* waiting for disconnection confirm from peer      */
    LLCP_DLC_STATE_MAX
};
typedef UINT8 tLLCP_DLC_STATE;

/*
** LLCP data link connection events
*/
enum
{
    LLCP_DLC_EVENT_API_CONNECT_REQ,      /* connection request from upper layer  */
    LLCP_DLC_EVENT_API_CONNECT_CFM,      /* connection confirm from upper layer  */
    LLCP_DLC_EVENT_API_CONNECT_REJECT,   /* connection reject from upper layer   */
    LLCP_DLC_EVENT_PEER_CONNECT_IND,     /* connection request from peer         */
    LLCP_DLC_EVENT_PEER_CONNECT_CFM,     /* connection confirm from peer         */

    LLCP_DLC_EVENT_API_DATA_REQ,         /* data packet from upper layer         */
    LLCP_DLC_EVENT_PEER_DATA_IND,        /* data packet from peer                */

    LLCP_DLC_EVENT_API_DISCONNECT_REQ,   /* disconnect request from upper layer  */
    LLCP_DLC_EVENT_PEER_DISCONNECT_IND,  /* disconnect request from peer         */
    LLCP_DLC_EVENT_PEER_DISCONNECT_RESP, /* disconnect response from peer        */

    LLCP_DLC_EVENT_FRAME_ERROR,          /* received erroneous frame from peer   */
    LLCP_DLC_EVENT_LINK_ERROR,           /* llcp link has been deactivated       */

    LLCP_DLC_EVENT_TIMEOUT               /* timeout event                        */
};
typedef UINT8 tLLCP_DLC_EVENT;

/*
** LLCP data link connection control block
*/

#define LLCP_DATA_LINK_FLAG_PENDING_DISC     0x01 /* send DISC when tx queue is empty       */
#define LLCP_DATA_LINK_FLAG_PENDING_RR_RNR   0x02 /* send RR/RNR with valid sequence        */
#define LLCP_DATA_LINK_FLAG_NOTIFY_TX_DONE   0x04 /* notify upper later when tx complete    */


typedef struct
{
    tLLCP_DLC_STATE         state;              /* data link connection state               */
    UINT8                   flags;              /* specific action flags                    */
    tLLCP_APP_CB            *p_app_cb;          /* pointer of application registration      */
    TIMER_LIST_ENT          timer;              /* timer for connection complete            */

    UINT8                   local_sap;          /* SAP of local end point                   */
    UINT16                  local_miu;          /* MIU of local SAP                         */
    UINT8                   local_rw;           /* RW of local SAP                          */
    BOOLEAN                 local_busy;         /* TRUE if local SAP is busy                */

    UINT8                   remote_sap;         /* SAP of remote end point                  */
    UINT16                  remote_miu;         /* MIU of remote SAP                        */
    UINT8                   remote_rw;          /* RW of remote SAP                         */
    BOOLEAN                 remote_busy;        /* TRUE if remote SAP is busy               */

    UINT8                   next_tx_seq;        /* V(S), send state variable                */
    UINT8                   rcvd_ack_seq;       /* V(SA), send ack state variable           */
    UINT8                   next_rx_seq;        /* V(R), receive state variable             */
    UINT8                   sent_ack_seq;       /* V(RA), receive ack state variable        */

    BUFFER_Q                i_xmit_q;           /* tx queue of I PDU                        */
    BOOLEAN                 is_tx_congested;    /* TRUE if tx I PDU is congested            */

    BUFFER_Q                i_rx_q;             /* rx queue of I PDU                        */
    BOOLEAN                 is_rx_congested;    /* TRUE if rx I PDU is congested            */
    UINT8                   num_rx_i_pdu;       /* number of I PDU in rx queue              */
    UINT8                   rx_congest_threshold; /* dynamic congest threshold for rx I PDU */

} tLLCP_DLCB;

/*
** LLCP service discovery control block
*/

typedef struct
{
    UINT8           tid;        /* transaction ID                           */
    tLLCP_SDP_CBACK *p_cback;   /* callback function for service discovery  */
} tLLCP_SDP_TRANSAC;

typedef struct
{
    UINT8               next_tid;                       /* next TID to use         */
    tLLCP_SDP_TRANSAC   transac[LLCP_MAX_SDP_TRANSAC];  /* active SDP transactions */
    BT_HDR              *p_snl;                         /* buffer for SNL PDU      */
} tLLCP_SDP_CB;


/*
** LLCP control block
*/

typedef struct
{
    UINT8           trace_level;                    /* LLCP trace level                             */

    tLLCP_SDP_CB    sdp_cb;                         /* SDP control block                            */
    tLLCP_LCB       lcb;                            /* LLCP link control block                      */
    tLLCP_APP_CB    wks_cb[LLCP_MAX_WKS];           /* Application's registration for well-known services */
    tLLCP_APP_CB    server_cb[LLCP_MAX_SERVER];     /* Application's registration for SDP services  */
    tLLCP_APP_CB    client_cb[LLCP_MAX_CLIENT];     /* Application's registration for client        */
    tLLCP_DLCB      dlcb[LLCP_MAX_DATA_LINK];       /* Data link connection control block           */

    UINT8           max_num_ll_tx_buff;             /* max number of tx UI PDU in queue             */
    UINT8           max_num_tx_buff;                /* max number of tx UI/I PDU in queue           */

    UINT8           num_logical_data_link;          /* number of logical data link                  */
    UINT8           num_data_link_connection;       /* number of established data link connection   */

    /* these two thresholds (number of tx UI PDU) are dynamically adjusted based on number of logical links */
    UINT8           ll_tx_congest_start;            /* congest start threshold for each logical link*/
    UINT8           ll_tx_congest_end;              /* congest end threshold for each logical link  */

    UINT8           total_tx_ui_pdu;                /* total number of tx UI PDU in all of ui_xmit_q*/
    UINT8           total_tx_i_pdu;                 /* total number of tx I PDU in all of i_xmit_q  */
    BOOLEAN         overall_tx_congested;           /* TRUE if tx link is congested                 */

    /* start point of uncongested status notification is in round robin */
    UINT8           ll_tx_uncongest_ntf_start_sap;  /* next start of logical data link              */
    UINT8           dl_tx_uncongest_ntf_start_idx;  /* next start of data link connection           */

    /*
    ** when overall rx link congestion starts, RNR is sent to remote end point of data link connection
    ** while rx link is congested, UI PDU is discarded.
    */
    UINT8           num_rx_buff;                    /* reserved number of rx UI/I PDU in queue      */
    UINT8           overall_rx_congest_start;       /* threshold of overall rx congestion start     */
    UINT8           overall_rx_congest_end;         /* threshold of overall rx congestion end       */
    UINT8           max_num_ll_rx_buff;             /* max number of rx UI PDU in queue             */

    /*
    ** threshold (number of rx UI PDU) is dynamically adjusted based on number of logical links
    ** when number of rx UI PDU is more than ll_rx_congest_start, the oldest UI PDU is discarded
    */
    UINT8           ll_rx_congest_start;            /* rx congest start threshold for each logical link */

    UINT8           total_rx_ui_pdu;                /* total number of rx UI PDU in all of ui_rx_q  */
    UINT8           total_rx_i_pdu;                 /* total number of rx I PDU in all of i_rx_q    */
    BOOLEAN         overall_rx_congested;           /* TRUE if overall rx link is congested         */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tLLCP_DTA_CBACK *p_dta_cback;                   /* callback to notify DTA when respoding SNL    */
    BOOLEAN         dta_snl_resp;                   /* TRUE if need to notify DTA when respoding SNL*/
#endif
} tLLCP_CB;

#if (LLCP_TEST_INCLUDED == TRUE) /* this is for LLCP testing */

typedef struct {
    UINT8  version;
    UINT16 wks;
} tLLCP_TEST_PARAMS;

#endif

#ifdef __cplusplus
extern "C" {
#endif


/*
** LLCP global data
*/

#if (!defined LLCP_DYNAMIC_MEMORY) || (LLCP_DYNAMIC_MEMORY == FALSE)
LLCP_API extern tLLCP_CB  llcp_cb;
#else
LLCP_API extern tLLCP_CB *llcp_cb_ptr;
#define llcp_cb (*llcp_cb_ptr)
#endif

/*
** Functions provided by llcp_main.c
*/
void llcp_init (void);
void llcp_cleanup (void);
void llcp_process_timeout (TIMER_LIST_ENT *p_tle);

/*
** Functions provided by llcp_link.c
*/
tLLCP_STATUS llcp_link_activate (tLLCP_ACTIVATE_CONFIG *p_config);
void llcp_link_process_link_timeout (void);
void llcp_link_deactivate (UINT8 reason);

void llcp_link_check_send_data (void);
void llcp_link_connection_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data);

/*
**  Functions provided by llcp_util.c
*/
void         llcp_util_adjust_ll_congestion (void);
void         llcp_util_adjust_dl_rx_congestion (void);
void         llcp_util_check_rx_congested_status (void);
BOOLEAN      llcp_util_parse_link_params (UINT16 length, UINT8 *p_bytes);
tLLCP_STATUS llcp_util_send_ui (UINT8 ssap, UINT8 dsap, tLLCP_APP_CB *p_app_cb, BT_HDR *p_msg);
void         llcp_util_send_disc (UINT8 dsap, UINT8 ssap);
tLLCP_DLCB  *llcp_util_allocate_data_link (UINT8 reg_sap, UINT8 remote_sap);
void         llcp_util_deallocate_data_link (tLLCP_DLCB *p_dlcb);
tLLCP_STATUS llcp_util_send_connect (tLLCP_DLCB *p_dlcb, tLLCP_CONNECTION_PARAMS *p_params);
tLLCP_STATUS llcp_util_parse_connect (UINT8 *p_bytes, UINT16 length, tLLCP_CONNECTION_PARAMS *p_params);
tLLCP_STATUS llcp_util_send_cc (tLLCP_DLCB *p_dlcb, tLLCP_CONNECTION_PARAMS *p_params);
tLLCP_STATUS llcp_util_parse_cc (UINT8 *p_bytes, UINT16 length, UINT16 *p_miu, UINT8 *p_rw);
void         llcp_util_send_dm (UINT8 dsap, UINT8 ssap, UINT8 reason);
void         llcp_util_build_info_pdu (tLLCP_DLCB *p_dlcb, BT_HDR *p_msg);
tLLCP_STATUS llcp_util_send_frmr (tLLCP_DLCB *p_dlcb, UINT8 flags, UINT8 ptype, UINT8 sequence);
void         llcp_util_send_rr_rnr (tLLCP_DLCB *p_dlcb);
tLLCP_APP_CB *llcp_util_get_app_cb (UINT8 sap);
/*
** Functions provided by llcp_dlc.c
*/
tLLCP_STATUS llcp_dlsm_execute (tLLCP_DLCB *p_dlcb, tLLCP_DLC_EVENT event, void *p_data);
tLLCP_DLCB  *llcp_dlc_find_dlcb_by_sap (UINT8 local_sap, UINT8 remote_sap);
void         llcp_dlc_flush_q (tLLCP_DLCB *p_dlcb);
void         llcp_dlc_proc_i_pdu (UINT8 dsap, UINT8 ssap, UINT16 i_pdu_length, UINT8 *p_i_pdu, BT_HDR *p_msg);
void         llcp_dlc_proc_rx_pdu (UINT8 dsap, UINT8 ptype, UINT8 ssap, UINT16 length, UINT8 *p_data);
void         llcp_dlc_check_to_send_rr_rnr (void);
BOOLEAN      llcp_dlc_is_rw_open (tLLCP_DLCB *p_dlcb);
BT_HDR      *llcp_dlc_get_next_pdu (tLLCP_DLCB *p_dlcb);
UINT16       llcp_dlc_get_next_pdu_length (tLLCP_DLCB *p_dlcb);

/*
** Functions provided by llcp_sdp.c
*/
void         llcp_sdp_proc_data (tLLCP_SAP_CBACK_DATA *p_data);
tLLCP_STATUS llcp_sdp_send_sdreq (UINT8 tid, char *p_name);
UINT8        llcp_sdp_get_sap_by_name (char *p_name, UINT8 length);
tLLCP_STATUS llcp_sdp_proc_snl (UINT16 sdu_length, UINT8 *p);
void         llcp_sdp_check_send_snl (void);
void         llcp_sdp_proc_deactivation (void);
#ifdef __cplusplus
}
#endif

#endif
