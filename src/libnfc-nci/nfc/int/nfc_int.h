/******************************************************************************
 *
 *  Copyright (C) 2009-2014 Broadcom Corporation
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
 *  this file contains the main NFC Upper Layer internal definitions and
 *  functions.
 *
 ******************************************************************************/

#ifndef NFC_INT_H_
#define NFC_INT_H_

#include "nfc_target.h"
#include "gki.h"
#include "nci_defs.h"
#include "nfc_api.h"
#include "btu_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
** Internal NFC constants and definitions
****************************************************************************/

/****************************************************************************
** NFC_TASK definitions
****************************************************************************/

/* NFC_TASK event masks */
#define NFC_TASK_EVT_TRANSPORT_READY        EVENT_MASK (APPL_EVT_0)

/* NFC Timer events */
#define NFC_TTYPE_NCI_WAIT_RSP              0
#define NFC_TTYPE_WAIT_2_DEACTIVATE         1
#define NFC_TTYPE_NCI_WAIT_DATA_NTF         2
#define NFC_TTYPE_LLCP_LINK_MANAGER         100
#define NFC_TTYPE_LLCP_LINK_INACT           101
#define NFC_TTYPE_LLCP_DATA_LINK            102
#define NFC_TTYPE_LLCP_DELAY_FIRST_PDU      103
#define NFC_TTYPE_RW_T1T_RESPONSE           104
#define NFC_TTYPE_RW_T2T_RESPONSE           105
#define NFC_TTYPE_RW_T3T_RESPONSE           106
#define NFC_TTYPE_RW_T4T_RESPONSE           107
#define NFC_TTYPE_RW_I93_RESPONSE           108
#define NFC_TTYPE_CE_T4T_UPDATE             109
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFC_TTYPE_P2P_PRIO_RESPONSE         110  /* added for p2p prio logic timer */
#define NFC_TTYPE_LISTEN_ACTIVATION         111  /* added for listen activation timer */
#define NFC_TTYPE_P2P_PRIO_LOGIC_CLEANUP    112  /* added for p2p prio logic clenaup */
#endif
#define NFC_TTYPE_VS_BASE                   200
#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
#define NFC_TTYPE_LLCP_DPS_PDU              113
#endif


/* NFC Task event messages */

enum
{
    NFC_STATE_NONE,                 /* not start up yet                         */
    NFC_STATE_W4_HAL_OPEN,          /* waiting for HAL_NFC_OPEN_CPLT_EVT        */
    NFC_STATE_CORE_INIT,            /* sending CORE_RESET and CORE_INIT         */
    NFC_STATE_W4_POST_INIT_CPLT,    /* waiting for HAL_NFC_POST_INIT_CPLT_EVT   */
    NFC_STATE_IDLE,                 /* normal operation (discovery state)       */
    NFC_STATE_OPEN,                 /* NFC link is activated                    */
    NFC_STATE_CLOSING,              /* de-activating                            */
    NFC_STATE_W4_HAL_CLOSE,         /* waiting for HAL_NFC_POST_INIT_CPLT_EVT   */
    NFC_STATE_NFCC_POWER_OFF_SLEEP  /* NFCC is power-off sleep mode             */
};
typedef UINT8 tNFC_STATE;
enum
{
    I2C_FRAGMENATATION_ENABLED,     /*i2c fragmentation_enabled           */
    I2C_FRAGMENTATION_DISABLED      /*i2c_fragmentation_disabled          */
};
/* NFC control block flags */
#define NFC_FL_DEACTIVATING             0x0001  /* NFC_Deactivate () is called and the NCI cmd is not sent   */
#define NFC_FL_RESTARTING               0x0002  /* restarting NFCC after PowerOffSleep          */
#define NFC_FL_POWER_OFF_SLEEP          0x0004  /* enterning power off sleep mode               */
#define NFC_FL_POWER_CYCLE_NFCC         0x0008  /* Power cycle NFCC                             */
#define NFC_FL_CONTROL_REQUESTED        0x0010  /* HAL requested control on NCI command window  */
#define NFC_FL_CONTROL_GRANTED          0x0020  /* NCI command window is on the HAL side        */
#define NFC_FL_DISCOVER_PENDING         0x0040  /* NCI command window is on the HAL side        */
#define NFC_FL_HAL_REQUESTED            0x0080  /* NFC_FL_CONTROL_REQUESTED on HAL request      */

#define NFC_PEND_CONN_ID               0xFE
#define NFC_CONN_ID_INT_MASK           0xF0
#define NFC_CONN_ID_ID_MASK            NCI_CID_MASK
#define NFC_CONN_NO_FC                 0xFF /* set num_buff to this for no flow control */
#define NFC_NCI_CONN_NO_FC             0xFF

#if (NFC_RW_ONLY == FALSE)
/* only allow the entries that the NFCC can support */
#define NFC_CHECK_MAX_CONN()    {if (max > nfc_cb.max_conn) max = nfc_cb.max_conn;}
#else
#define NFC_CHECK_MAX_CONN()
#endif

typedef struct
{
    tNFC_CONN_CBACK *p_cback;   /* the callback function to receive the data        */
    BUFFER_Q    tx_q;           /* transmit queue                                   */
    BUFFER_Q    rx_q;           /* receive queue                                    */
    UINT8       id;             /* NFCEE ID or RF Discovery ID or NFC_TEST_ID       */
    UINT8       act_protocol;   /* the active protocol on this logical connection   */
    UINT8       conn_id;        /* the connection id assigned by NFCC for this conn */
    UINT8       buff_size;      /* the max buffer size for this connection.     .   */
    UINT8       num_buff;       /* num of buffers left to send on this connection   */
    UINT8       init_credits;   /* initial num of buffer credits                    */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    UINT8       act_interface;  /* the active interface on this logical i=connetion  */
    UINT8       sel_res;        /* the sel_res of the activated rf interface connection */
#endif
} tNFC_CONN_CB;

/* This data type is for NFC task to send a NCI VS command to NCIT task */
typedef struct
{
    BT_HDR          bt_hdr;     /* the NCI command          */
    tNFC_VS_CBACK   *p_cback;   /* the callback function to receive RSP   */
} tNFC_NCI_VS_MSG;

/* This data type is for HAL event */
typedef struct
{
    BT_HDR          hdr;
    UINT8           hal_evt;    /* HAL event code  */
    UINT8           status;     /* tHAL_NFC_STATUS */
} tNFC_HAL_EVT_MSG;

#define NFC_RECEIVE_MSGS_OFFSET     (10) /* callback function pointer(8; use 8 to be safe + NFC_SAVED_CMD_SIZE(2) */

/* NFCC power state change pending callback */
typedef void (tNFC_PWR_ST_CBACK) (void);
#define NFC_SAVED_HDR_SIZE          (2)
/* data Reassembly error (in BT_HDR.layer_specific) */
#define NFC_RAS_TOO_BIG             0x08
#define NFC_RAS_FRAGMENTED          0x01

/* NCI command buffer contains a VSC (in BT_HDR.layer_specific) */
#define NFC_WAIT_RSP_VSC            0x01
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFC_WAIT_RSP_NXP            0x02
#endif

typedef struct
{
    UINT8               data_ntf_timeout;           /* indicates credit ntf timeout occured         */
    UINT8               nci_cmd_channel_busy;      /*  indicates to the data queue that cmd is sent */
    UINT8               data_stored;               /*  indicates data is stored to be sent later  */
    UINT8               conn_id;                   /*  stores the conn_id of the data stored       */
}i2c_data;

/* NFC control blocks */
typedef struct
{
    UINT16              flags;                      /* NFC control block flags - NFC_FL_* */
    tNFC_CONN_CB        conn_cb[NCI_MAX_CONN_CBS];
    UINT8               conn_id[NFC_MAX_CONN_ID+1]; /* index: conn_id; conn_id[]: index(1 based) to conn_cb[] */
    tNFC_DISCOVER_CBACK *p_discv_cback;
    tNFC_RESPONSE_CBACK *p_resp_cback;
    tNFC_TEST_CBACK     *p_test_cback;
    tNFC_VS_CBACK       *p_vs_cb[NFC_NUM_VS_CBACKS];/* Register for vendor specific events  */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    UINT8               nxpCbflag;
#endif

#if (NFC_RW_ONLY == FALSE)
    /* NFCC information at init rsp */
    UINT32              nci_features;               /* the NCI features supported by NFCC */
    UINT16              max_ce_table;               /* the max routing table size       */
    UINT8               max_conn;                   /* the num of connections supported by NFCC */
#endif
    UINT8               nci_ctrl_size;              /* Max Control Packet Payload Size */

    const tNCI_DISCOVER_MAPS  *p_disc_maps;         /* NCI RF Discovery interface mapping */
    UINT8               vs_interface[NFC_NFCC_MAX_NUM_VS_INTERFACE];  /* the NCI VS interfaces of NFCC    */
    UINT16              nci_interfaces;             /* the NCI interfaces of NFCC       */
    UINT8               num_disc_maps;              /* number of RF Discovery interface mappings */
    void               *p_disc_pending;            /* the parameters associated with pending NFC_DiscoveryStart */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    void               *p_last_disc;            /* the parameters associated with pending NFC_DiscoveryStart */
#endif

    /* NFC_TASK timer management */
    TIMER_LIST_Q        timer_queue;                /* 1-sec timer event queue */
    TIMER_LIST_Q        quick_timer_queue;

    TIMER_LIST_ENT      deactivate_timer;           /* Timer to wait for deactivation */

    tNFC_STATE          nfc_state;
    BOOLEAN             reassembly;         /* Reassemble fragmented data pkt */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFC_STATE          old_nfc_state;
#endif
    UINT8               trace_level;
    UINT8               last_hdr[NFC_SAVED_HDR_SIZE];/* part of last NCI command header */
    UINT8               last_cmd[NFC_SAVED_CMD_SIZE];/* part of last NCI command payload */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    UINT8              *last_cmd_buf;
    UINT8               cmd_size;
#endif
    void                *p_vsc_cback;       /* the callback function for last VSC command */
    BUFFER_Q            nci_cmd_xmit_q;     /* NCI command queue */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    BUFFER_Q            nci_cmd_recov_xmit_q;     /* NCI recovery command queue */
    TIMER_LIST_ENT      listen_activation_timer_list;           /* Timer for monitoring listen activation */
#endif
    TIMER_LIST_ENT      nci_wait_rsp_timer; /* Timer for waiting for nci command response */
    TIMER_LIST_ENT      nci_wait_data_ntf_timer; /* Timer for waiting for core credit ntf*/
    UINT16              nci_wait_rsp_tout;  /* NCI command timeout (in ms) */
    UINT8               nci_wait_rsp;       /* layer_specific for last NCI message */

    UINT8               nci_cmd_window;     /* Number of commands the controller can accecpt without waiting for response */

    BT_HDR              *p_nci_init_rsp;    /* holding INIT_RSP until receiving HAL_NFC_POST_INIT_CPLT_EVT */
    tHAL_NFC_ENTRY      *p_hal;
    i2c_data            i2c_data_t;         /* holding i2c fragmentation data */
} tNFC_CB;


/*****************************************************************************
**  EXTERNAL FUNCTION DECLARATIONS
*****************************************************************************/

/* Global NFC data */
#if NFC_DYNAMIC_MEMORY == FALSE
NFC_API extern tNFC_CB  nfc_cb;
#else
NFC_API extern tNFC_CB *nfc_cb_ptr;
#define nfc_cb (*nfc_cb_ptr)
#endif

/****************************************************************************
** Internal nfc functions
****************************************************************************/

NFC_API extern void nfc_init(void);

/* from nfc_utils.c */
NFC_API extern tNFC_CONN_CB * nfc_alloc_conn_cb ( tNFC_CONN_CBACK *p_cback);
NFC_API extern tNFC_CONN_CB * nfc_find_conn_cb_by_conn_id (UINT8 conn_id);
NFC_API extern tNFC_CONN_CB * nfc_find_conn_cb_by_handle (UINT8 target_handle);
NFC_API extern void nfc_set_conn_id (tNFC_CONN_CB * p_cb, UINT8 conn_id);
NFC_API extern void nfc_free_conn_cb (tNFC_CONN_CB *p_cb);
NFC_API extern void nfc_reset_all_conn_cbs (void);
NFC_API extern void nfc_data_event (tNFC_CONN_CB * p_cb);

void nfc_ncif_send (BT_HDR *p_buf, BOOLEAN is_cmd);
extern UINT8 nfc_ncif_send_data (tNFC_CONN_CB *p_cb, BT_HDR *p_data);
NFC_API extern void nfc_ncif_cmd_timeout (void);
NFC_API extern void nfc_wait_2_deactivate_timeout (void);
NFC_API extern void nfc_ncif_credit_ntf_timeout (void);
NFC_API extern BOOLEAN nfc_ncif_process_event (BT_HDR *p_msg);
NFC_API extern void nfc_ncif_check_cmd_queue (BT_HDR *p_buf);
NFC_API extern void nfc_ncif_send_cmd (BT_HDR *p_buf);
NFC_API extern void nfc_ncif_proc_discover_ntf (UINT8 *p, UINT16 plen);
NFC_API extern void nfc_ncif_rf_management_status (tNFC_DISCOVER_EVT event, UINT8 status);
NFC_API extern void nfc_ncif_set_config_status (UINT8 *p, UINT8 len);
NFC_API extern void nfc_ncif_event_status (tNFC_RESPONSE_EVT event, UINT8 status);
NFC_API extern void nfc_ncif_error_status (UINT8 conn_id, UINT8 status);
NFC_API extern void nfc_ncif_proc_credits(UINT8 *p, UINT16 plen);
NFC_API extern void nfc_ncif_proc_activate (UINT8 *p, UINT8 len);
NFC_API extern void nfc_ncif_proc_deactivate (UINT8 status, UINT8 deact_type, BOOLEAN is_ntf);
#if ((NFC_NFCEE_INCLUDED == TRUE) && (NFC_RW_ONLY == FALSE))
NFC_API extern void nfc_ncif_proc_ee_action (UINT8 *p, UINT16 plen);
NFC_API extern void nfc_ncif_proc_ee_discover_req (UINT8 *p, UINT16 plen);
NFC_API extern void nfc_ncif_proc_get_routing (UINT8 *p, UINT8 len);
#endif
NFC_API extern void nfc_ncif_proc_conn_create_rsp (UINT8 *p, UINT16 plen, UINT8 dest_type);
NFC_API extern void nfc_ncif_report_conn_close_evt (UINT8 conn_id, tNFC_STATUS status);
NFC_API extern void nfc_ncif_proc_t3t_polling_ntf (UINT8 *p, UINT16 plen);
NFC_API extern void nfc_ncif_proc_reset_rsp (UINT8 *p, BOOLEAN is_ntf);
NFC_API extern void nfc_ncif_proc_init_rsp (BT_HDR *p_msg);
NFC_API extern void nfc_ncif_proc_get_config_rsp (BT_HDR *p_msg);
NFC_API extern void nfc_ncif_proc_data (BT_HDR *p_msg);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
NFC_API extern tNFC_STATUS nfc_ncif_store_FWVersion(UINT8 * p_buf);
NFC_API extern BOOLEAN nfa_dm_p2p_prio_logic(UINT8 event, UINT8 *p, UINT8 ntf_rsp);
NFC_API extern void nfc_ncif_update_window (void);
NFC_API extern void nfa_dm_p2p_timer_event ();
NFC_API extern void nfc_ncif_empty_cmd_queue ();
NFC_API extern BOOLEAN nfc_ncif_proc_proprietary_rsp (UINT8 mt, UINT8 gid, UINT8 oid);
NFC_API extern void nfc_ncif_proc_rf_wtx_ntf (UINT8 *p, UINT16 plen);
#endif

#if (NFC_RW_ONLY == FALSE)
NFC_API extern void nfc_ncif_proc_rf_field_ntf (UINT8 rf_status);
#else
#define nfc_ncif_proc_rf_field_ntf(rf_status)
#endif

/* From nfc_task.c */
NFC_API extern UINT32 nfc_task (UINT32 param);
void nfc_task_shutdown_nfcc (void);

/* From nfc_main.c */
void nfc_enabled (tNFC_STATUS nfc_status, BT_HDR *p_init_rsp_msg);
void nfc_set_state (tNFC_STATE nfc_state);
void nfc_main_flush_cmd_queue (void);
void nfc_gen_cleanup (void);
void nfc_main_handle_hal_evt (tNFC_HAL_EVT_MSG *p_msg);

/* Timer functions */
void nfc_start_timer (TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout);
UINT32 nfc_remaining_time (TIMER_LIST_ENT *p_tle);
void nfc_stop_timer (TIMER_LIST_ENT *p_tle);

void nfc_start_quick_timer (TIMER_LIST_ENT *p_tle, UINT16 type, UINT32 timeout);
void nfc_stop_quick_timer (TIMER_LIST_ENT *p_tle);
void nfc_process_quick_timer_evt (void);
void set_i2c_fragmentation_enabled(int value);
int  get_i2c_fragmentation_enabled();

#ifdef __cplusplus
}
#endif

#endif /* NFC_INT_H_ */
