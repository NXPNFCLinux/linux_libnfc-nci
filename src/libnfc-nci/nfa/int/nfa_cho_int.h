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
 *  This is the private interface file for the NFA Connection Handover.
 *
 ******************************************************************************/
#ifndef NFA_CHO_INT_H
#define NFA_CHO_INT_H

#if (defined (NFA_CHO_INCLUDED) && (NFA_CHO_INCLUDED==TRUE))
#include "llcp_api.h"
#include "llcp_defs.h"
#include "nfa_cho_api.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/

/* NFA Connection Handover state */
enum
{
    NFA_CHO_ST_DISABLED,        /* Application has not registered      */
    NFA_CHO_ST_IDLE,            /* No data link connection             */
    NFA_CHO_ST_W4_CC,           /* Waiting for connection confirm      */
    NFA_CHO_ST_CONNECTED,       /* Data link connected                 */

    NFA_CHO_ST_MAX
};

typedef UINT8 tNFA_CHO_STATE;

/* NFA Connection Handover substate in NFA_CHO_ST_CONNECTED */
enum
{
    NFA_CHO_SUBSTATE_W4_LOCAL_HR,   /* Waiting for Hs record from local     */
    NFA_CHO_SUBSTATE_W4_LOCAL_HS,   /* Waiting for Hs record from local     */
    NFA_CHO_SUBSTATE_W4_REMOTE_HR,  /* Waiting for Hr record from remote    */
    NFA_CHO_SUBSTATE_W4_REMOTE_HS,  /* Waiting for Hs record from remote    */

    NFA_CHO_SUBSTATE_MAX
};

typedef UINT8 tNFA_CHO_SUBSTATE;

/* Handover Message receiving status for SAR */
#define NFA_CHO_RX_NDEF_COMPLETE    0   /* received complete NDEF message */
#define NFA_CHO_RX_NDEF_TEMP_MEM    1   /* Cannot process due to temporary memory constraint */
#define NFA_CHO_RX_NDEF_PERM_MEM    2   /* Cannot process due to permanent memory constraint */
#define NFA_CHO_RX_NDEF_INCOMPLTE   3   /* Need more date       */
#define NFA_CHO_RX_NDEF_INVALID     4   /* Invalid NDEF message */

typedef UINT8 tNFA_CHO_RX_NDEF_STATUS;

/* Handover Message Type */
#define NFA_CHO_MSG_UNKNOWN         0   /* Unknown Message           */
#define NFA_CHO_MSG_HR              1   /* Handover Request Message  */
#define NFA_CHO_MSG_HS              2   /* Handover Select Message   */
#define NFA_CHO_MSG_BT_OOB          3   /* Simplified BT OOB message */
#define NFA_CHO_MSG_WIFI            4   /* Simplified WIFI message   */

typedef UINT8 tNFA_CHO_MSG_TYPE;

/* Timeout */
#define NFA_CHO_TIMEOUT_FOR_HS          1000    /* ms, waiting for Hs record */
#define NFA_CHO_TIMEOUT_FOR_RETRY       1000    /* ms, retry because of temp memory constrain */
#define NFA_CHO_TIMEOUT_SEGMENTED_HR    500     /* ms, waiting for next segmented Hr */

#define NFA_CHO_EXCLUDING_PAYLOAD_ID    0xFF    /* don't include payload ID string */

/* NFA Connection Handover internal events */
enum
{
    NFA_CHO_API_REG_EVT     = NFA_SYS_EVT_START (NFA_ID_CHO), /* NFA_ChoRegister () */
    NFA_CHO_API_DEREG_EVT,            /* NFA_ChoDeregister ()       */
    NFA_CHO_API_CONNECT_EVT,          /* NFA_ChoConnect ()          */
    NFA_CHO_API_DISCONNECT_EVT,       /* NFA_ChoDisconnect ()       */
    NFA_CHO_API_SEND_HR_EVT,          /* NFA_ChoSendHr ()           */
    NFA_CHO_API_SEND_HS_EVT,          /* NFA_ChoSendHs ()           */
    NFA_CHO_API_SEL_ERR_EVT,          /* NFA_ChoSendSelectError ()  */

    NFA_CHO_RX_HANDOVER_MSG_EVT,      /* Received Handover Message  */

    NFA_CHO_LLCP_CONNECT_IND_EVT,     /* LLCP_SAP_EVT_CONNECT_IND       */
    NFA_CHO_LLCP_CONNECT_RESP_EVT,    /* LLCP_SAP_EVT_CONNECT_RESP      */
    NFA_CHO_LLCP_DISCONNECT_IND_EVT,  /* LLCP_SAP_EVT_DISCONNECT_IND    */
    NFA_CHO_LLCP_DISCONNECT_RESP_EVT, /* LLCP_SAP_EVT_DISCONNECT_RESP   */
    NFA_CHO_LLCP_CONGEST_EVT,         /* LLCP_SAP_EVT_CONGEST           */
    NFA_CHO_LLCP_LINK_STATUS_EVT,     /* LLCP_SAP_EVT_LINK_STATUS       */

    NFA_CHO_NDEF_TYPE_HANDLER_EVT,    /* Callback event from NDEF Type handler */
    NFA_CHO_TIMEOUT_EVT,              /* Timeout event              */

    NFA_CHO_LAST_EVT
};

typedef UINT16 tNFA_CHO_INT_EVT;

/* data type for NFA_CHO_API_REG_EVT */
typedef struct
{
    BT_HDR              hdr;
    BOOLEAN             enable_server;
    tNFA_CHO_CBACK     *p_cback;
} tNFA_CHO_API_REG;

/* data type for NFA_CHO_API_DEREG_EVT */
typedef BT_HDR tNFA_CHO_API_DEREG;

/* data type for NFA_CHO_API_CONNECT_EVT */
typedef BT_HDR tNFA_CHO_API_CONNECT;

/* data type for NFA_CHO_API_DISCONNECT_EVT */
typedef BT_HDR tNFA_CHO_API_DISCONNECT;

/* data type for NFA_CHO_API_SEND_HR_EVT */
typedef struct
{
    BT_HDR              hdr;
    UINT8               num_ac_info;
    tNFA_CHO_AC_INFO   *p_ac_info;
    UINT8              *p_ndef;
    UINT32              max_ndef_size;
    UINT32              cur_ndef_size;
} tNFA_CHO_API_SEND_HR;

/* data type for NFA_CHO_API_SEND_HS_EVT */
typedef struct
{
    BT_HDR              hdr;
    UINT8               num_ac_info;
    tNFA_CHO_AC_INFO   *p_ac_info;
    UINT8              *p_ndef;
    UINT32              max_ndef_size;
    UINT32              cur_ndef_size;
} tNFA_CHO_API_SEND_HS;

/* data type for NFA_CHO_API_STOP_EVT */
typedef BT_HDR tNFA_CHO_API_STOP;

/* data type for NFA_CHO_API_SEL_ERR_EVT */
typedef struct
{
    BT_HDR              hdr;
    UINT8               error_reason;
    UINT32              error_data;
} tNFA_CHO_API_SEL_ERR;

/* data type for NFA_CHO_NDEF_TYPE_HANDLER_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_NDEF_EVT       event;
    tNFA_NDEF_EVT_DATA  data;
} tNFA_CHO_NDEF_TYPE_HDLR_EVT;

/* union of all event data types */
typedef union
{
    BT_HDR                      hdr;                /* NFA_CHO_TIMEOUT_EVT        */
    tNFA_CHO_API_REG            api_reg;            /* NFA_CHO_API_REG_EVT        */
    tNFA_CHO_API_DEREG          api_dereg;          /* NFA_CHO_API_DEREG_EVT      */
    tNFA_CHO_API_CONNECT        api_connect;        /* NFA_CHO_API_CONNECT_EVT    */
    tNFA_CHO_API_DISCONNECT     api_disconnect;     /* NFA_CHO_API_DISCONNECT_EVT */
    tNFA_CHO_API_SEND_HR        api_send_hr;        /* NFA_CHO_API_SEND_HR_EVT    */
    tNFA_CHO_API_SEND_HS        api_send_hs;        /* NFA_CHO_API_SEND_HS_EVT    */
    tNFA_CHO_API_SEL_ERR        api_sel_err;        /* NFA_CHO_API_SEL_ERR_EVT    */
    tNFA_CHO_NDEF_TYPE_HDLR_EVT ndef_type_hdlr;     /* NFA_CHO_NDEF_TYPE_HANDLER_EVT */
    tLLCP_SAP_CBACK_DATA        llcp_cback_data;    /* LLCP callback data         */
} tNFA_CHO_INT_EVENT_DATA;

/*****************************************************************************
**  control block
*****************************************************************************/

#define NFA_CHO_FLAGS_LLCP_ACTIVATED    0x01
#define NFA_CHO_FLAGS_CLIENT_ONLY       0x02    /* Handover server is not enabled       */
#define NFA_CHO_FLAGS_CONN_COLLISION    0x04    /* collision when creating data link    */

/* NFA Connection Handover control block */
typedef struct
{
    tNFA_CHO_STATE      state;                  /* main state                           */
    tNFA_CHO_SUBSTATE   substate;               /* substate in connected state          */
    TIMER_LIST_ENT      timer;                  /* timer for rx handover message        */

    UINT8               server_sap;             /* SAP for local handover server        */
    UINT8               client_sap;             /* SAP for connection to remote handover server */
    UINT8               local_sap;              /* SSAP for connection, either server_sap or client_sap */
    UINT8               remote_sap;             /* DSAP for connection                  */

    UINT8               flags;                  /* internal flags                       */
    tNFA_CHO_DISC_REASON disc_reason;           /* disconnection reason                 */

    tNFA_HANDLE         hs_ndef_type_handle;    /* handle for HS NDEF Type handler      */
    tNFA_HANDLE         bt_ndef_type_handle;    /* handle for BT OOB NDEF Type handler  */
    tNFA_HANDLE         wifi_ndef_type_handle;  /* handle for WiFi NDEF Type handler    */

    UINT16              local_link_miu;         /* MIU of local LLCP                    */
    UINT16              remote_miu;             /* peer's MIU of data link connection   */
    BOOLEAN             congested;              /* TRUE if data link is congested       */

    UINT8               collision_local_sap;    /* SSAP for collision connection        */
    UINT8               collision_remote_sap;   /* DSAP for collision connection        */
    UINT16              collision_remote_miu;   /* peer's MIU of collision  connection  */
    BOOLEAN             collision_congested;    /* TRUE if collision connection is congested */

    UINT16              tx_random_number;       /* it has been sent in Hr for collision */

    UINT8              *p_tx_ndef_msg;          /* allocate buffer for tx NDEF msg      */
    UINT32              tx_ndef_cur_size;       /* current size of NDEF message         */
    UINT32              tx_ndef_sent_size;      /* transmitted size of NDEF message     */

    UINT8              *p_rx_ndef_msg;          /* allocate buffer for rx NDEF msg      */
    UINT32              rx_ndef_buf_size;       /* allocate buffer size for rx NDEF msg */
    UINT32              rx_ndef_cur_size;       /* current rx size of NDEF message      */

    tNFA_CHO_CBACK     *p_cback;                /* callback registered by application   */

    UINT8               trace_level;

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
    UINT8               test_enabled;
    UINT8               test_version;
    UINT16              test_random_number;
#endif
} tNFA_CHO_CB;

/*****************************************************************************
**  External variables
*****************************************************************************/

/* NFA Connection Handover control block */
#if NFA_DYNAMIC_MEMORY == FALSE
extern tNFA_CHO_CB nfa_cho_cb;
#else
extern tNFA_CHO_CB *nfa_cho_cb_ptr;
#define nfa_cho_cb (*nfa_cho_cb_ptr)
#endif

/*****************************************************************************
**  External functions
*****************************************************************************/
/* nfa_cho_main.c */
void nfa_cho_init (void);

/* nfa_cho_sm.c */
void nfa_cho_sm_llcp_cback (tLLCP_SAP_CBACK_DATA *p_data);
void nfa_cho_sm_execute (tNFA_CHO_INT_EVT event, tNFA_CHO_INT_EVENT_DATA *p_evt_data);

/* nfa_cho_util.c */
void nfa_cho_proc_ndef_type_handler_evt (tNFA_CHO_INT_EVENT_DATA *p_evt_data);
tNFA_STATUS nfa_cho_proc_api_reg (tNFA_CHO_INT_EVENT_DATA *p_evt_data);
void        nfa_cho_proc_api_dereg (void);
tNFA_STATUS nfa_cho_create_connection (void);
void nfa_cho_process_disconnection (tNFA_CHO_DISC_REASON disc_reason);
void nfa_cho_notify_tx_fail_evt (tNFA_STATUS status);

tNFA_STATUS nfa_cho_send_handover_msg (void);
tNFA_CHO_RX_NDEF_STATUS nfa_cho_read_ndef_msg (UINT8 local_sap, UINT8 remote_sap);
tNFA_CHO_RX_NDEF_STATUS nfa_cho_reassemble_ho_msg (UINT8 local_sap, UINT8 remote_sap);

tNFA_STATUS nfa_cho_send_hr (tNFA_CHO_API_SEND_HR *p_api_send_hr);
tNFA_STATUS nfa_cho_send_hs (tNFA_CHO_API_SEND_HS *p_api_select);
tNFA_STATUS nfa_cho_send_hs_error (UINT8 error_reason, UINT32 error_data);

void nfa_cho_proc_hr (UINT32 length, UINT8 *p_ndef_msg);
void nfa_cho_proc_hs (UINT32 length, UINT8 *p_ndef_msg);
void nfa_cho_proc_simplified_format (UINT32 length, UINT8 *p_ndef_msg);

tNFA_CHO_MSG_TYPE  nfa_cho_get_msg_type (UINT32 length, UINT8 *p_ndef_msg);
tNFA_CHO_ROLE_TYPE nfa_cho_get_local_device_role (UINT32 length, UINT8 *p_ndef_msg);
tNFA_STATUS nfa_cho_update_random_number (UINT8 *p_ndef_msg);
#endif /* (defined (NFA_CHO_INCLUDED) && (NFA_CHO_INCLUDED==TRUE)) */
#endif /* NFA_CHO_INT_H */
