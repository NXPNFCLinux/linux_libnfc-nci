/******************************************************************************
 *
 *  Copyright (C) 2003-2014 Broadcom Corporation
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
 *  This is the private interface file for the NFA DTA
 *
 ******************************************************************************/
#ifndef NFA_DTA_INT_H
#define NFA_DTA_INT_H

#include "nfa_dta_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfc_api.h"
#include "rw_api.h"
#include "ce_api.h"

#if (NFA_DTA_INCLUDED == TRUE)

/*****************************************************************************
**  DTA definitions
*****************************************************************************/
#define NFA_DTA_PATTERN_NUMBER_INVALID              0xFFFF

#define NFA_DTA_PATTERN_NUMBER_LLCP_CONNECT_BY_SAP  0x1200
#define NFA_DTA_PATTERN_NUMBER_LLCP_CONNECT_BY_SN   0x1240
#define NFA_DTA_PATTERN_NUMBER_LLCP_CONNECT_BY_SNL  0x1280

#define NFA_DTA_PATTERN_NUMBER_SNEP_SERVER_ONLY             0x1300
#define NFA_DTA_PATTERN_NUMBER_SNEP_DEFAULT_PUT_SHORT_NDEF  0x1301
#define NFA_DTA_PATTERN_NUMBER_SNEP_DEFAULT_PUT_LONG_NDEF   0x1302
#define NFA_DTA_PATTERN_NUMBER_SNEP_EXTENDED_GET            0x1303

#define NFA_DTA_DISCOVER_PARAMS_MAX     6

#define NDEF_WKT_TEXT_HDR_LEN   7               /* Header length for long NDEF text message */
#define NFA_DTA_T3T_WRITE_NDEF_SIZE     192     /* Size of NDEF message for T3T write-tests ([DTA] $5.5.6) */
#define NFA_DTA_T3T_LISTEN_SYSTEMCODE   0xBABE  /* System code to use for T3T Listen mode tests */

#define NFA_PROTOCOL_RANK_INVALID       0xFF    /* Maximum protocol preference rank */

#define NFA_DTA_SCRATCH_BUF_SIZE        T3T_MSG_BLOCKSIZE

#ifndef NFA_DTA_DEFAULT_CO_OUT_DSAP
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFA_DTA_DEFAULT_CO_OUT_DSAP     0x10    /* Default SAP[LT,CO-OUT-DEST] if SDP was not performed to get SAP from the LT */
#else
#define NFA_DTA_DEFAULT_CO_OUT_DSAP     0x12    /* Default SAP[LT,CO-OUT-DEST] if SDP was not performed to get SAP from the LT */
#endif
#endif

/*****************************************************************************
* DTA state machine definitions
*****************************************************************************/

typedef struct {
    BOOLEAN tp_continue;                    /* NFA_DTA_CFG_TP_CONTINUE    */
    tNFA_DTA_FL_POLL_LISTEN poll_listen;    /* NFA_DTA_CFG_POLL_LISTEN    */
    BOOLEAN t4at_nfcdep_priority;           /* NFA_DTA_CFG_T4AT_NFCDEP_PRIORITY */
    BOOLEAN reactivation;                   /* NFA_DTA_CFG_REACTIVATION   */
    UINT16  total_duration;                 /* NFA_DTA_CFG_TOTAL_DURATION */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    BOOLEAN enable_dta_llcp;                /* NFA_DTA_CFG_LLCP */
    tNFA_DTA_SNEP_MODE dta_snep_mode;       /* NFA_DTA_CFG_SNEP */
#endif
    tNFA_DTA_EMVCO_PCD_MODE emvco_pcd_mode; /* NFA_DTA_CFG_EMVCO_PCD */
} tNFA_DTA_CONFIG;

/*****************************************************************************
* DTA state machine definitions
*****************************************************************************/
/* DTA events */
enum
{
    /* device manager local device API events */
    NFA_DTA_API_ENABLE_EVT = NFA_SYS_EVT_START (NFA_ID_DTA),
    NFA_DTA_API_DISABLE_EVT,
    NFA_DTA_API_CONFIG_EVT,
    NFA_DTA_API_START_EVT,
    NFA_DTA_API_STOP_EVT,
    NFA_DTA_ACTIVATE_EVT,
    NFA_DTA_DEACTIVATE_EVT,
    NFA_DTA_DATA_CBACK_EVT,

    NFA_DTA_MAX_EVT
};


/* data type for NFA_DTA_API_ENABLE_EVT */
typedef struct
{
    BT_HDR          hdr;
    BOOLEAN         auto_start;
    tNFA_DTA_CBACK  *p_cback;
} tNFA_DTA_API_ENABLE;

/* data type for NFA_DTA_API_START_EVT  */
typedef struct
{
    BT_HDR                  hdr;
    UINT16                  pattern_number;
    UINT8                   tlv_len;
    UINT8                   *p_tlv_params;
} tNFA_DTA_API_START;

/* data type for NFA_DTA_API_CONFIG  */
typedef struct
{
    BT_HDR                  hdr;
    tNFA_DTA_CFG_ITEM       item;
    tNFA_DTA_CFG            cfg_data;
} tNFA_DTA_API_CONFIG;

/* data type for NFA_DTA_DATA_CBACK_EVT */
typedef struct
{
    UINT8                   event;
    tRW_DATA                data;
} tNFA_DTA_RW_DATA;

typedef struct
{
    UINT8                   event;
    tCE_DATA                data;
} tNFA_DTA_CE_DATA;

typedef struct
{
    tNFC_CONN_EVT           event;
    tNFC_CONN               data;
} tNFA_DTA_NFCDEP_DATA;


enum
{
    NFA_DTA_LLCP_CONNECT_CO_ECHO_OUT,
    NFA_DTA_LLCP_DISCONNECT_CO_ECHO_OUT
};
typedef UINT8 tNFA_DTA_LLCP_EVT;

enum
{
    NFA_DTA_RW_DATA,
    NFA_DTA_CE_DATA,
    NFA_DTA_NFCDEP_DATA,
    NFA_DTA_LLCP_DATA
};

typedef UINT8 tNFA_DTA_DATA_TYPE;

typedef struct
{
    BT_HDR                  hdr;
    tNFA_DTA_DATA_TYPE      type;
    union
    {
        tNFA_DTA_RW_DATA        rw;
        tNFA_DTA_CE_DATA        ce;
        tNFA_DTA_NFCDEP_DATA    nfcdep;
        tNFA_DTA_LLCP_EVT       llcp_evt;
    } data;
} tNFA_DTA_DATA_CBACK;

/* All API message type */
typedef union
{
    BT_HDR              hdr;
    tNFA_DTA_API_ENABLE enable;
    tNFA_DTA_API_CONFIG cfg;
    tNFA_DTA_API_START  start;
    tNFA_DTA_DATA_CBACK data_cback;
}tNFA_DTA_MSG;



/* DTA states */
enum
{
    NFA_DTA_ST_IDLE,
    NFA_DTA_ST_DISCOVER,        /* Polling/Listening */
    NFA_DTA_ST_ACTIVATED        /* Activated, listen mode */
};
typedef UINT8 tNFA_DTA_STATE;

/* DTA Substates (while in ACTIVATED state) - substate enumerations are found in protocol-specific files (nfa_dta_XXX.c) */
#define NFA_DTA_SST_IDLE    0
typedef UINT8 tNFA_DTA_SUBSTATE;

/* DTA discovery states */
enum
{
    NFA_DTA_DISC_STATE_IDLE,
    NFA_DTA_DISC_STATE_DISCOVERY,
    NFA_DTA_DISC_STATE_POLL_ACTIVE,
    NFA_DTA_DISC_STATE_W4_ALL_DISCOVERIES,
    NFA_DTA_DISC_STATE_W4_HOST_SELECT,
    NFA_DTA_DISC_STATE_LISTEN_ACTIVE,
    NFA_DTA_DISC_STATE_LISTEN_SLEEP,
    NFA_DTA_DISC_STATE_MAX
};

/*****************************************************************************
* DTA control block definitions
*****************************************************************************/

/* NDEF buffer definitions */
enum {
    NFA_DTA_BUF_READ,               /* Buffer for RW Read requests */
    NFA_DTA_BUF_WRITE,              /* Buffer for RW Write requests */
    NFA_DTA_BUF_MAX
};

typedef struct {
    UINT8   *p_data;
    UINT32  max_size;
    UINT32  cur_size;
    UINT32  offset;     /* current read/write offset */
} tNFA_DTA_BUF_CB;


/* T4T listen mode test application */
enum {
    NFA_DTA_T4T_CE_APP_NONE,        /* Not selected */
    NFA_DTA_T4T_CE_APP_LOOPBACK,    /* loopback test applicaiton */
    NFA_DTA_T4T_CE_APP_PROP         /* propretary test application */
};

/* DTA test step command */
typedef tNFC_STATUS (*tNFA_DTA_CMD_FCN) (void *);

/* dta control block flags */
#define NFA_DTA_FL_ENABLED                      0x00000001  /* DTA is enabled */
#define NFA_DTA_FL_AUTOSTART                    0x00000002  /* Automatically start discovery when NFC is enabled */
#define NFA_DTA_FL_STOPPING                     0x00000004  /* DTA is stopping (NFA_DtaStop called) */
#define NFA_DTA_FL_DISABLING                    0x00000008  /* DTA is being disabled (NFA_DtaDisable called) */
#define NFA_DTA_FL_T4T_DESELECT_DEACT           0x00000010  /* T4T/NFCDEP is deactivating to IDLE (need to DESELECT first) */

/* DTA control block */
typedef struct {
    UINT32                  dta_flags;      /* dta_flags must be first item in structure (statically intialized to 0 on startup) */

    /* Configuration */
    tNFA_DTA_CONFIG         cfg;

    /* DTA State Machine */
    tNFA_DTA_STATE          state;
    tNFA_DTA_SUBSTATE       substate;       /* Current protocol-specific sub-state */
    tNFA_DTA_CBACK          *p_cback;       /* Applicatation for DTA event notification */

    /* DTA test parameters */
    UINT16                  pattern_number;
    UINT16                  pattern_number_old;

    /* Discovery Parameters */
    UINT8                   disc_state;
    UINT8                   disc_params_num;
    tNFC_DISCOVER_PARAMS    disc_params[NFA_DTA_DISCOVER_PARAMS_MAX];

    /* Activation parameters */
    tNFC_ACTIVATE_DEVT      activate_params;
    UINT8                   cur_protocol_rank;  /* perference ranking of currently discovered protocol */

    tRW_CBACK              *p_rw_cback;
    tCE_CBACK              *p_ce_cback;

    TIMER_LIST_ENT          protocol_timer;     /* timer for the activated protocol if needed */

    UINT8                   t4t_ce_app;         /* T4T listen mode test application */
    tCE_T4T_AID_HANDLE      t4t_dta_aid_hdl;    /* T4T registration handle for proprietary dta aid */
    tCE_T4T_AID_HANDLE      t4t_prop_aid_hdl;   /* T4T registration handle for proprietary aid */
    UINT8                   nfc_dep_wt;

    BOOLEAN                 llcp_cl_more_to_read;    /* TRUE if there is more to read in llcp cl link*/
    BOOLEAN                 llcp_co_more_to_read;    /* TRUE if there is more to read in llcp recieve window*/
    BOOLEAN                 llcp_is_initiator;      /* TURE if IUT is LLCP initiator */
    UINT16                  llcp_local_link_miu;    /* link MIU of IUT               */
    UINT16                  llcp_remote_link_miu;   /* link MIU of LT                */

    UINT8                   llcp_pattern_num_sap;   /* SAP of pattern number exchange */

    UINT8                   llcp_cl_in_local_sap;   /* SAP of IUT-CL-IN-DEST */
    UINT8                   llcp_cl_out_local_sap;  /* SAP of IUT-CL-OUT-SRC */
    UINT8                   llcp_cl_out_remote_sap; /* SAP of LT-CL-OUT-DEST */

    UINT8                   llcp_co_in_local_sap;   /* SAP of IUT-CO-IN-DEST */
    UINT8                   llcp_co_in_remote_sap;  /* SAP of LT-CO-IN-SRC   */
    UINT8                   llcp_co_out_local_sap;  /* SAP of IUT-CO-OUT-SRC */
    UINT8                   llcp_co_out_remote_sap; /* SAP of LT-CO-OUT-DEST */

    UINT16                  llcp_co_out_remote_miu; /* MIU of LT-CO-OUT-DEST */
    UINT8                   llcp_co_out_remote_rw;  /* RW of LT-CO-OUT-DEST  */

#define NFA_DTA_LLCP_FLAGS_CO_OUT_CONNECTING    0x01    /* establishing outbound on connection-oriented */
#define NFA_DTA_LLCP_FLAGS_CO_OUT_CONNECTED     0x02    /* established outbound on connection-oriented  */

    UINT8                   llcp_flags;             /* internal flags for LLCP echo test */
    UINT8                   llcp_sdp_tid_cl;        /* SDP transaction ID for outbound connectionless */
    UINT8                   llcp_sdp_tid_co;        /* SDP transaction ID for outbound connection-oriented */

    TIMER_LIST_ENT          llcp_cl_echo_timer;     /* timer for the connectionless echo test application      */
    TIMER_LIST_ENT          llcp_co_echo_timer;     /* timer for the connection-oriented echo test application */
    BUFFER_Q                llcp_cl_buffer;         /* buffer for the connectionless echo test application     */
    BUFFER_Q                llcp_co_buffer;         /* buffer for the connection-oriented echo test application*/

    tNFA_HANDLE             snep_server_handle;
    tNFA_HANDLE             snep_server_conn_handle;
    tNFA_HANDLE             snep_client_handle;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFA_DTA_SNEP_CLIENT_TEST_FLAGS_DEFAULT_SERVER    0x01
#define NFA_DTA_SNEP_CLIENT_TEST_FLAGS_EXTENDED_SERVER   0x02
#define NFA_DTA_SNEP_CLIENT_TEST_FLAGS_PUT_SHORT_NDEF    0x04
#define NFA_DTA_SNEP_CLIENT_TEST_FLAGS_PUT_LONG_NDEF     0x08
#define NFA_DTA_SNEP_CLIENT_TEST_FLAGS_GET               0x10

    UINT8                   snep_client_test_flags;
#endif
    UINT8                   *p_snep_short_ndef;
    UINT32                  snep_short_ndef_size;
    UINT8                   *p_snep_long_ndef;
    UINT32                  snep_long_ndef_size;

    /* DTA buffer for NDEF read/write */
    tNFA_DTA_BUF_CB buf_cb[NFA_DTA_BUF_MAX];
    UINT32              ndef_size;      /* Size of NDEF message from NDEF detection */

    /* Scratch buffer for miscelaneous use */
    UINT8               scratch_buf[NFA_DTA_SCRATCH_BUF_SIZE];

    /* DTA Test command table */
    tNFA_DTA_CMD_FCN    *p_cur_cmd_tbl; /* Current table of commands for current test */
    UINT8               cur_cmd_idx;
} tNFA_DTA_CB;
extern tNFA_DTA_CB nfa_dta_cb;

/* NFA_SYS info for DTA */
extern const tNFA_SYS_REG nfa_dta_sys_reg;

/* DTA startup setconfig parameters */
extern UINT8 *p_nfa_dta_start_up_cfg;
extern UINT8 nfa_dta_start_up_cfg_len;

/*****************************************************************************
* DTA internal funciton protoytpes
*****************************************************************************/
/* Internal function prototypes */
void nfa_dta_deactivate (UINT8 deactivate_type);
void nfa_dta_shutdown (void);
void nfa_dta_discover_start (void);

/* nfa_sys handler for DTA */
BOOLEAN nfa_dta_evt_hdlr (BT_HDR *p_msg);
void nfa_dta_sys_disable (void);

/* State machine action functions */
BOOLEAN nfa_dta_enable (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_disable (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_config (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_start (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_handle_deact (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_stop (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_run_test (tNFA_DTA_MSG *p_data);
BOOLEAN nfa_dta_proc_data (tNFA_DTA_MSG *p_msg_data);

/* Utility functions */
void nfa_dta_test_set_state (tNFA_DTA_STATE state);
void nfa_dta_test_set_substate (tNFA_DTA_SUBSTATE substate);
void nfa_dta_free_ndef_buf (UINT8 ndef_idx);
UINT8 *nfa_dta_realloc_buf (UINT8 ndef_idx, UINT32 size);
void nfa_dta_t3t_nfcid_rand (UINT8 nfcid2[NCI_RF_F_UID_LEN]);

/* Test function entry points (in nfa_dta_XXX.c) */
void nfa_dta_nfcdep_poll_test_start (void);
void nfa_dta_nfcdep_proc_data (tNFC_CONN_EVT event, tNFC_CONN *p_data);
void nfa_dta_t1t_poll_test_start (void);
void nfa_dta_t2t_poll_test_start (void);
void nfa_dta_t3t_poll_test_start (void);
void nfa_dta_t4t_poll_test_start (void);

void nfa_dta_nfcdep_listen_test_start (void);
void nfa_dta_t3t_listen_test_start (void);
void nfa_dta_t4t_listen_test_start (void);

void nfa_dta_t1t_rw_cback (UINT8 event, tRW_DATA *p_data);
void nfa_dta_t2t_rw_cback (UINT8 event, tRW_DATA *p_data);
void nfa_dta_t3t_rw_cback (UINT8 event, tRW_DATA *p_data);
void nfa_dta_t4t_rw_cback (UINT8 event, tRW_DATA *p_data);

void nfa_dta_t3t_ce_cback (UINT8 event, tCE_DATA *p_data);
void nfa_dta_t4t_ce_cback (UINT8 event, tCE_DATA *p_data);

void nfa_dta_ce_cback (UINT8 event, tCE_DATA *p_ce_data);

void nfa_dta_t4t_register_apps (void);
void nfa_dta_t4t_deregister_apps (void);

void nfa_dta_llcp_init (void);
void nfa_dta_llcp_set_gen_bytes (void);
void nfa_dta_llcp_clear_gen_bytes (void);
void nfa_dta_llcp_register_pattern_number_service (void);
void nfa_dta_llcp_deregister_pattern_number_service (void);
void nfa_dta_llcp_register_echo (void);
void nfa_dta_llcp_deregister_echo (void);
void nfa_dta_llcp_activate_link (void);
void nfa_dta_llcp_connect_co_echo_out (void);
void nfa_dta_llcp_disconnect_co_echo_out (void);

void nfa_dta_snep_init (void);
void nfa_dta_snep_register (void);
void nfa_dta_snep_deregister (void);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
void nfa_dta_snep_mode (tNFA_DTA_SNEP_MODE mode);
#endif

void nfa_dta_emvco_pcd_config_nfcc (BOOLEAN enable);
void nfa_dta_emvco_pcd_start (void);
void nfa_dta_emvco_pcd_cback (UINT8 event, tRW_DATA *p_data);

extern UINT8 *p_nfa_dta_brcm_start_up_cfg;
extern UINT8 nfa_dta_brcm_start_up_cfg_len;
extern UINT8 *p_nfa_dta_start_up_vsc_cfg;

#endif /* (NFA_DTA_INCLUDED == TRUE) */
#endif /* NFA_DTA_INT_H */
