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
 *  This is the private interface file for the NFA P2P.
 *
 ******************************************************************************/
#ifndef NFA_P2P_INT_H
#define NFA_P2P_INT_H

#if (defined (NFA_P2P_INCLUDED) && (NFA_P2P_INCLUDED==TRUE))
#include "nfa_p2p_api.h"
#include "nfa_dm_int.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/
#define NFA_P2P_DEBUG   BT_TRACE_VERBOSE

/* NFA P2P LLCP link state */
enum
{
    NFA_P2P_LLCP_STATE_IDLE,
    NFA_P2P_LLCP_STATE_LISTENING,
    NFA_P2P_LLCP_STATE_ACTIVATED,

    NFA_P2P_LLCP_STATE_MAX
};

typedef UINT8 tNFA_P2P_LLCP_STATE;

/* NFA P2P events */
enum
{
    NFA_P2P_API_REG_SERVER_EVT  = NFA_SYS_EVT_START (NFA_ID_P2P),
    NFA_P2P_API_REG_CLIENT_EVT,
    NFA_P2P_API_DEREG_EVT,
    NFA_P2P_API_ACCEPT_CONN_EVT,
    NFA_P2P_API_REJECT_CONN_EVT,
    NFA_P2P_API_DISCONNECT_EVT,
    NFA_P2P_API_CONNECT_EVT,
    NFA_P2P_API_SEND_UI_EVT,
    NFA_P2P_API_SEND_DATA_EVT,
    NFA_P2P_API_SET_LOCAL_BUSY_EVT,
    NFA_P2P_API_GET_LINK_INFO_EVT,
    NFA_P2P_API_GET_REMOTE_SAP_EVT,
    NFA_P2P_API_SET_LLCP_CFG_EVT,
    NFA_P2P_INT_RESTART_RF_DISC_EVT,

    NFA_P2P_LAST_EVT
};

/* data type for NFA_P2P_API_REG_SERVER_EVT */
typedef struct
{
    BT_HDR              hdr;
    UINT8               server_sap;
    tNFA_P2P_LINK_TYPE  link_type;
    char                service_name[LLCP_MAX_SN_LEN + 1];
    tNFA_P2P_CBACK     *p_cback;
} tNFA_P2P_API_REG_SERVER;

/* data type for NFA_P2P_API_REG_CLIENT_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_P2P_LINK_TYPE  link_type;
    tNFA_P2P_CBACK     *p_cback;
} tNFA_P2P_API_REG_CLIENT;

/* data type for NFA_P2P_API_DEREG_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         handle;
} tNFA_P2P_API_DEREG;

/* data type for NFA_P2P_API_ACCEPT_CONN_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         conn_handle;
    UINT16              miu;
    UINT8               rw;
} tNFA_P2P_API_ACCEPT_CONN;

/* data type for NFA_P2P_API_REJECT_CONN_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         conn_handle;
} tNFA_P2P_API_REJECT_CONN;

/* data type for NFA_P2P_API_DISCONNECT_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         conn_handle;
    BOOLEAN             flush;
} tNFA_P2P_API_DISCONNECT;

/* data type for NFA_P2P_API_CONNECT_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         client_handle;
    char                service_name[LLCP_MAX_SN_LEN + 1];
    UINT8               dsap;
    UINT16              miu;
    UINT8               rw;
} tNFA_P2P_API_CONNECT;

/* data type for NFA_P2P_API_SEND_UI_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         handle;
    UINT8               dsap;
    BT_HDR             *p_msg;
} tNFA_P2P_API_SEND_UI;

/* data type for NFA_P2P_API_SEND_DATA_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         conn_handle;
    BT_HDR             *p_msg;
} tNFA_P2P_API_SEND_DATA;

/* data type for NFA_P2P_API_SET_LOCAL_BUSY_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         conn_handle;
    BOOLEAN             is_busy;
} tNFA_P2P_API_SET_LOCAL_BUSY;

/* data type for NFA_P2P_API_GET_LINK_INFO_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         handle;
} tNFA_P2P_API_GET_LINK_INFO;

/* data type for NFA_P2P_API_GET_REMOTE_SAP_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_HANDLE         handle;
    char                service_name[LLCP_MAX_SN_LEN + 1];
} tNFA_P2P_API_GET_REMOTE_SAP;

/* data type for NFA_P2P_API_SET_LLCP_CFG_EVT */
typedef struct
{
    BT_HDR              hdr;
    UINT16              link_miu;
    UINT8               opt;
    UINT8               wt;
    UINT16              link_timeout;
    UINT16              inact_timeout_init;
    UINT16              inact_timeout_target;
    UINT16              symm_delay;
    UINT16              data_link_timeout;
    UINT16              delay_first_pdu_timeout;
} tNFA_P2P_API_SET_LLCP_CFG;

/* union of all event data types */
typedef union
{
    BT_HDR                      hdr;
    tNFA_P2P_API_REG_SERVER     api_reg_server;
    tNFA_P2P_API_REG_CLIENT     api_reg_client;
    tNFA_P2P_API_DEREG          api_dereg;
    tNFA_P2P_API_ACCEPT_CONN    api_accept;
    tNFA_P2P_API_REJECT_CONN    api_reject;
    tNFA_P2P_API_DISCONNECT     api_disconnect;
    tNFA_P2P_API_CONNECT        api_connect;
    tNFA_P2P_API_SEND_UI        api_send_ui;
    tNFA_P2P_API_SEND_DATA      api_send_data;
    tNFA_P2P_API_SET_LOCAL_BUSY api_local_busy;
    tNFA_P2P_API_GET_LINK_INFO  api_link_info;
    tNFA_P2P_API_GET_REMOTE_SAP api_remote_sap;
    tNFA_P2P_API_SET_LLCP_CFG   api_set_llcp_cfg;
} tNFA_P2P_MSG;

/*****************************************************************************
**  control block
*****************************************************************************/
#define NFA_P2P_HANDLE_FLAG_CONN             0x80   /* Bit flag for connection handle           */

/* NFA P2P Connection block */
#define NFA_P2P_CONN_FLAG_IN_USE             0x01   /* Connection control block is used         */
#define NFA_P2P_CONN_FLAG_REMOTE_RW_ZERO     0x02   /* Remote set RW to 0 (flow off)            */
#define NFA_P2P_CONN_FLAG_CONGESTED          0x04   /* data link connection is congested        */

typedef struct
{
    UINT8               flags;                      /* internal flags for data link connection  */
    UINT8               local_sap;                  /* local SAP of data link connection        */
    UINT8               remote_sap;                 /* remote SAP of data link connection       */
    UINT16              remote_miu;                 /* MIU of remote end point                  */
    UINT8               num_pending_i_pdu;          /* number of tx I PDU not processed by NFA  */
} tNFA_P2P_CONN_CB;

/* NFA P2P SAP control block */
#define NFA_P2P_SAP_FLAG_SERVER             0x01    /* registered server                        */
#define NFA_P2P_SAP_FLAG_CLIENT             0x02    /* registered client                        */
#define NFA_P2P_SAP_FLAG_LLINK_CONGESTED    0x04    /* logical link connection is congested     */

typedef struct
{
    UINT8              flags;                       /* internal flags for local SAP             */
    tNFA_P2P_CBACK     *p_cback;                    /* callback function for local SAP          */
    UINT8              num_pending_ui_pdu;          /* number of tx UI PDU not processed by NFA */
} tNFA_P2P_SAP_CB;

/* NFA P2P SDP control block */
typedef struct
{
    UINT8            tid;                           /* transaction ID */
    UINT8            local_sap;
} tNFA_P2P_SDP_CB;

#define NFA_P2P_NUM_SAP         64

/* NFA P2P control block */
typedef struct
{
    tNFA_HANDLE         dm_disc_handle;

    tNFA_DM_RF_DISC_STATE rf_disc_state;
    tNFA_P2P_LLCP_STATE llcp_state;
    BOOLEAN             is_initiator;
    BOOLEAN             is_active_mode;
    UINT16              local_link_miu;
    UINT16              remote_link_miu;

    tNFA_TECHNOLOGY_MASK listen_tech_mask;          /* for P2P listening */
    tNFA_TECHNOLOGY_MASK listen_tech_mask_to_restore;/* to retry without active listen mode */
    TIMER_LIST_ENT      active_listen_restore_timer; /* timer to restore active listen mode */
    BOOLEAN             is_p2p_listening;
    BOOLEAN             is_cho_listening;
    BOOLEAN             is_snep_listening;

    tNFA_P2P_SAP_CB     sap_cb[NFA_P2P_NUM_SAP];
    tNFA_P2P_CONN_CB    conn_cb[LLCP_MAX_DATA_LINK];
    tNFA_P2P_SDP_CB     sdp_cb[LLCP_MAX_SDP_TRANSAC];

    UINT8               total_pending_ui_pdu;       /* total number of tx UI PDU not processed by NFA */
    UINT8               total_pending_i_pdu;        /* total number of tx I PDU not processed by NFA */

    UINT8               trace_level;
} tNFA_P2P_CB;

/*****************************************************************************
**  External variables
*****************************************************************************/

/* NFA P2P control block */
#if NFA_DYNAMIC_MEMORY == FALSE
extern tNFA_P2P_CB nfa_p2p_cb;
#else
extern tNFA_P2P_CB *nfa_p2p_cb_ptr;
#define nfa_p2p_cb (*nfa_p2p_cb_ptr)
#endif

/*****************************************************************************
**  External functions
*****************************************************************************/
/*
**  nfa_p2p_main.c
*/
void nfa_p2p_init (void);
void nfa_p2p_update_listen_tech (tNFA_TECHNOLOGY_MASK tech_mask);
void nfa_p2p_enable_listening (tNFA_SYS_ID sys_id, BOOLEAN update_wks);
void nfa_p2p_disable_listening (tNFA_SYS_ID sys_id, BOOLEAN update_wks);
void nfa_p2p_activate_llcp (tNFC_DISCOVER *p_data);
void nfa_p2p_deactivate_llcp (void);
void nfa_p2p_set_config (tNFA_DM_DISC_TECH_PROTO_MASK disc_mask);

/*
**  nfa_p2p_act.c
*/
void nfa_p2p_proc_llcp_data_ind (tLLCP_SAP_CBACK_DATA  *p_data);
void nfa_p2p_proc_llcp_connect_ind (tLLCP_SAP_CBACK_DATA  *p_data);
void nfa_p2p_proc_llcp_connect_resp (tLLCP_SAP_CBACK_DATA  *p_data);
void nfa_p2p_proc_llcp_disconnect_ind (tLLCP_SAP_CBACK_DATA  *p_data);
void nfa_p2p_proc_llcp_disconnect_resp (tLLCP_SAP_CBACK_DATA  *p_data);
void nfa_p2p_proc_llcp_congestion (tLLCP_SAP_CBACK_DATA  *p_data);
void nfa_p2p_proc_llcp_link_status (tLLCP_SAP_CBACK_DATA  *p_data);

BOOLEAN nfa_p2p_start_sdp (char *p_service_name, UINT8 local_sap);

BOOLEAN nfa_p2p_reg_server (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_reg_client (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_dereg (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_accept_connection (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_reject_connection (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_disconnect (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_create_data_link_connection (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_send_ui (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_send_data (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_set_local_busy (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_get_link_info (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_get_remote_sap (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_set_llcp_cfg (tNFA_P2P_MSG *p_msg);
BOOLEAN nfa_p2p_restart_rf_discovery (tNFA_P2P_MSG *p_msg);

#if (BT_TRACE_VERBOSE == TRUE)
char *nfa_p2p_evt_code (UINT16 evt_code);
#endif

#else

#define nfa_p2p_init ()
#define nfa_p2p_activate_llcp (a) {};
#define nfa_p2p_deactivate_llcp ()
#define nfa_p2p_set_config ()

#endif /* (defined (NFA_P2P_INCLUDED) && (NFA_P2P_INCLUDED==TRUE)) */
#endif /* NFA_P2P_INT_H */
