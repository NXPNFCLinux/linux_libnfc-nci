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
 *  This is the private interface file for the NFA EE.
 *
 ******************************************************************************/
#ifndef NFA_EE_INT_H
#define NFA_EE_INT_H
#include "nfc_api.h"
#include "nfa_ee_api.h"
#include "nfa_sys.h"

#include <phNxpNciHal.h>

/*****************************************************************************
**  Constants and data types
*****************************************************************************/
#define NFA_EE_DEBUG            BT_TRACE_VERBOSE
#define NFA_EE_NUM_ECBS         (NFA_EE_MAX_EE_SUPPORTED+1) /* the number of tNFA_EE_ECBs (for NFCEEs and DH) */
#define NFA_EE_CB_4_DH          NFA_EE_MAX_EE_SUPPORTED     /* The index for DH in nfa_ee_cb.ee_cb[] */
#define NFA_EE_INVALID          0xFF
#define NFA_EE_MAX_TECH_ROUTE   4 /* only A, B, F, Bprime are supported by UICC now */

#ifndef NFA_EE_AID_CFG_TAG_NAME
#define NFA_EE_AID_CFG_TAG_NAME         0x4F /* AID                             */
#endif

/* NFA EE events */
enum
{
    NFA_EE_API_DISCOVER_EVT  = NFA_SYS_EVT_START(NFA_ID_EE),
    NFA_EE_API_REGISTER_EVT,
    NFA_EE_API_DEREGISTER_EVT,
    NFA_EE_API_MODE_SET_EVT,
    NFA_EE_API_SET_TECH_CFG_EVT,
    NFA_EE_API_SET_PROTO_CFG_EVT,
    NFA_EE_API_ADD_AID_EVT,
    NFA_EE_API_REMOVE_AID_EVT,
    NFA_EE_API_LMRT_SIZE_EVT,
    NFA_EE_API_UPDATE_NOW_EVT,
    NFA_EE_API_CONNECT_EVT,
    NFA_EE_API_SEND_DATA_EVT,
    NFA_EE_API_DISCONNECT_EVT,

    NFA_EE_NCI_DISC_RSP_EVT,
    NFA_EE_NCI_DISC_NTF_EVT,
    NFA_EE_NCI_MODE_SET_RSP_EVT,
    NFA_EE_NCI_CONN_EVT,
    NFA_EE_NCI_DATA_EVT,
    NFA_EE_NCI_ACTION_NTF_EVT,
    NFA_EE_NCI_DISC_REQ_NTF_EVT,
    NFA_EE_NCI_WAIT_RSP_EVT,

    NFA_EE_ROUT_TIMEOUT_EVT,
    NFA_EE_DISCV_TIMEOUT_EVT,
    NFA_EE_CFG_TO_NFCC_EVT,
    NFA_EE_MAX_EVT

};

typedef UINT16 tNFA_EE_INT_EVT;
#define NFA_EE_AE_ROUTE             0x80        /* for listen mode routing table*/
#define NFA_EE_AE_VS                0x40


/* NFA EE Management state */
enum
{
    NFA_EE_EM_STATE_INIT = 0,
    NFA_EE_EM_STATE_INIT_DONE,
    NFA_EE_EM_STATE_RESTORING,
    NFA_EE_EM_STATE_DISABLING,
    NFA_EE_EM_STATE_DISABLED,

    NFA_EE_EM_STATE_MAX
};
typedef UINT8 tNFA_EE_EM_STATE;

/* NFA EE connection status */
enum
{
    NFA_EE_CONN_ST_NONE,    /* not connected */
    NFA_EE_CONN_ST_WAIT,    /* connection is initiated; waiting for ack */
    NFA_EE_CONN_ST_CONN,    /* connected; can send/receive data */
    NFA_EE_CONN_ST_DISC,    /* disconnecting; waiting for ack */
    NFA_EE_CONN_ST_MAX
};
typedef UINT8 tNFA_EE_CONN_ST;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFA_EE_MAX_AID_CFG_LEN  (580)
#define NFA_EE_MAX_AID_CFG_LEN_PN547C2  (160)
#else
#define NFA_EE_MAX_AID_CFG_LEN  (510)
#endif
#define NFA_EE_7816_STATUS_LEN  (2)

/* NFA EE control block flags:
 * use to indicate an API function has changed the configuration of the associated NFCEE
 * The flags are cleared when the routing table/VS is updated */
#define NFA_EE_ECB_FLAGS_TECH       0x02      /* technology routing changed         */
#define NFA_EE_ECB_FLAGS_PROTO      0x04      /* protocol routing changed           */
#define NFA_EE_ECB_FLAGS_AID        0x08      /* AID routing changed                */
#define NFA_EE_ECB_FLAGS_VS         0x10      /* VS changed                         */
#define NFA_EE_ECB_FLAGS_RESTORE    0x20      /* Restore related                    */
#define NFA_EE_ECB_FLAGS_ROUTING    0x0E      /* routing flags changed              */
#define NFA_EE_ECB_FLAGS_DISC_REQ   0x40      /* NFCEE Discover Request NTF is set  */
#define NFA_EE_ECB_FLAGS_ORDER      0x80      /* DISC_REQ N reported before DISC N  */
typedef UINT8 tNFA_EE_ECB_FLAGS;

/* part of tNFA_EE_STATUS; for internal use only  */
#define NFA_EE_STATUS_RESTORING 0x20      /* waiting for restore to full power mode to complete */
#define NFA_EE_STATUS_INT_MASK  0x20      /* this bit is in ee_status for internal use only */

/* NFA-EE information for a particular NFCEE Entity (including DH) */
typedef struct
{
    tNFA_TECHNOLOGY_MASK    tech_switch_on;     /* default routing - technologies switch_on  */
    tNFA_TECHNOLOGY_MASK    tech_switch_off;    /* default routing - technologies switch_off */
    tNFA_TECHNOLOGY_MASK    tech_battery_off;   /* default routing - technologies battery_off*/
    tNFA_PROTOCOL_MASK      proto_switch_on;    /* default routing - protocols switch_on     */
    tNFA_PROTOCOL_MASK      proto_switch_off;   /* default routing - protocols switch_off    */
    tNFA_PROTOCOL_MASK      proto_battery_off;  /* default routing - protocols battery_off   */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_PROTOCOL_MASK      proto_screen_lock;   /* default routing - protocols screen_lock    */
    tNFA_PROTOCOL_MASK      proto_screen_off;  /* default routing - protocols screen_off  */
    tNFA_TECHNOLOGY_MASK    tech_screen_lock;    /* default routing - technologies screen_lock*/
    tNFA_TECHNOLOGY_MASK    tech_screen_off;   /* default routing - technologies screen_off*/
#endif
    tNFA_EE_CONN_ST         conn_st;            /* connection status */
    UINT8                   conn_id;            /* connection id */
    tNFA_EE_CBACK           *p_ee_cback;        /* the callback function */

    /* Each AID entry has an ssociated aid_len, aid_pwr_cfg, aid_rt_info.
     * aid_cfg[] contains AID and maybe some other VS information in TLV format
     * The first T is always NFA_EE_AID_CFG_TAG_NAME, the L is the actual AID length
     * the aid_len is the total length of all the TLVs associated with this AID entry
     */
    UINT8                   aid_len[NFA_EE_MAX_AID_ENTRIES];/* the actual lengths in aid_cfg */
    UINT8                   aid_pwr_cfg[NFA_EE_MAX_AID_ENTRIES];/* power configuration of this AID entry */
    UINT8                   aid_rt_info[NFA_EE_MAX_AID_ENTRIES];/* route/vs info for this AID entry */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    UINT8                   aid_rt_loc[NFA_EE_MAX_AID_ENTRIES];/* route location info for this AID entry */
#endif
    UINT8                   aid_cfg[NFA_EE_MAX_AID_CFG_LEN];/* routing entries based on AID */
    UINT8                   aid_entries;        /* The number of AID entries in aid_cfg */
    UINT8                   nfcee_id;           /* ID for this NFCEE */
    UINT8                   ee_status;          /* The NFCEE status */
    UINT8                   ee_old_status;      /* The NFCEE status before going to low power mode */
    tNFA_EE_INTERFACE       ee_interface[NFC_MAX_EE_INTERFACE];/* NFCEE supported interface */
    tNFA_EE_TLV             ee_tlv[NFC_MAX_EE_TLVS];/* the TLV */
    UINT8                   num_interface;      /* number of Target interface */
    UINT8                   num_tlvs;           /* number of TLVs */
    tNFA_EE_ECB_FLAGS       ecb_flags;          /* the flags of this control block */
    tNFA_EE_INTERFACE       use_interface;      /* NFCEE interface used for the connection */
    tNFA_NFC_PROTOCOL       la_protocol;        /* Listen A protocol    */
    tNFA_NFC_PROTOCOL       lb_protocol;        /* Listen B protocol    */
    tNFA_NFC_PROTOCOL       lf_protocol;        /* Listen F protocol    */
    tNFA_NFC_PROTOCOL       lbp_protocol;       /* Listen B' protocol   */
    UINT8                   size_mask;          /* the size for technology and protocol routing */
    UINT16                  size_aid;           /* the size for aid routing */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_NFC_PROTOCOL       pa_protocol;        /* Passive poll A SWP Reader   */
    tNFA_NFC_PROTOCOL       pb_protocol;        /* Passive poll B SWP Reader   */
    UINT8                   ee_req_op;          /* add or remove req ntf*/
#endif
} tNFA_EE_ECB;

/* data type for NFA_EE_API_DISCOVER_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_CBACK       *p_cback;
} tNFA_EE_API_DISCOVER;

/* data type for NFA_EE_API_REGISTER_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_CBACK       *p_cback;
} tNFA_EE_API_REGISTER;

/* data type for NFA_EE_API_DEREGISTER_EVT */
typedef struct
{
    BT_HDR              hdr;
    int                 index;
} tNFA_EE_API_DEREGISTER;

/* data type for NFA_EE_API_MODE_SET_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
    UINT8               mode;
} tNFA_EE_API_MODE_SET;

/* data type for NFA_EE_API_SET_TECH_CFG_EVT */
typedef struct
{
    BT_HDR                  hdr;
    tNFA_EE_ECB            *p_cb;
    UINT8                   nfcee_id;
    tNFA_TECHNOLOGY_MASK    technologies_switch_on;
    tNFA_TECHNOLOGY_MASK    technologies_switch_off;
    tNFA_TECHNOLOGY_MASK    technologies_battery_off;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_TECHNOLOGY_MASK    technologies_screen_lock;
    tNFA_TECHNOLOGY_MASK    technologies_screen_off;
#endif
} tNFA_EE_API_SET_TECH_CFG;

/* data type for NFA_EE_API_SET_PROTO_CFG_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
    tNFA_PROTOCOL_MASK  protocols_switch_on;
    tNFA_PROTOCOL_MASK  protocols_switch_off;
    tNFA_PROTOCOL_MASK  protocols_battery_off;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_PROTOCOL_MASK  protocols_screen_lock;
    tNFA_PROTOCOL_MASK  protocols_screen_off;
#endif

} tNFA_EE_API_SET_PROTO_CFG;

/* data type for NFA_EE_API_ADD_AID_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
    UINT8               aid_len;
    UINT8               *p_aid;
    tNFA_EE_PWR_STATE   power_state;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    UINT8               vs_info;
#endif
} tNFA_EE_API_ADD_AID;

/* data type for NFA_EE_API_REMOVE_AID_EVT */
typedef struct
{
    BT_HDR              hdr;
    UINT8               aid_len;
    UINT8               *p_aid;
} tNFA_EE_API_REMOVE_AID;

/* data type for NFA_EE_API_LMRT_SIZE_EVT */
typedef  BT_HDR tNFA_EE_API_LMRT_SIZE;

/* data type for NFA_EE_API_CONNECT_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
    UINT8               ee_interface;
    tNFA_EE_CBACK       *p_cback;
} tNFA_EE_API_CONNECT;

/* data type for NFA_EE_API_SEND_DATA_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
    UINT16              data_len;
    UINT8               *p_data;
} tNFA_EE_API_SEND_DATA;

/* data type for NFA_EE_API_DISCONNECT_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
} tNFA_EE_API_DISCONNECT;


typedef struct
{
    BT_HDR              hdr;
    tNFC_STATUS         status;                 /* The event status. */
} tNFA_EE_MSG_STATUS;

/* common data type for internal events with nfa_ee_use_cfg_cb[] as TRUE */
typedef struct
{
    BT_HDR              hdr;
    tNFA_EE_ECB        *p_cb;
    UINT8               nfcee_id;
} tNFA_EE_CFG_HDR;

/* data type for tNFC_RESPONSE_EVT */
typedef struct
{
    BT_HDR                      hdr;
    void                        *p_data;
} tNFA_EE_NCI_RESPONSE;

/* data type for NFA_EE_NCI_DISC_RSP_EVT */
typedef struct
{
    BT_HDR                      hdr;
    tNFC_NFCEE_DISCOVER_REVT    *p_data;
} tNFA_EE_NCI_DISC_RSP;

/* data type for NFA_EE_NCI_DISC_NTF_EVT */
typedef struct
{
    BT_HDR                      hdr;
    tNFC_NFCEE_INFO_REVT        *p_data;
} tNFA_EE_NCI_DISC_NTF;

/* data type for NFA_EE_NCI_MODE_SET_RSP_EVT */
typedef struct
{
    BT_HDR                      hdr;
    tNFC_NFCEE_MODE_SET_REVT    *p_data;
} tNFA_EE_NCI_MODE_SET;

/* data type for NFA_EE_NCI_WAIT_RSP_EVT */
typedef struct
{
    BT_HDR                      hdr;
    void                        *p_data;
    UINT8                       opcode;
} tNFA_EE_NCI_WAIT_RSP;

/* data type for NFA_EE_NCI_CONN_EVT and NFA_EE_NCI_DATA_EVT */
typedef struct
{
    BT_HDR                      hdr;
    UINT8                       conn_id;
    tNFC_CONN_EVT               event;
    tNFC_CONN                   *p_data;
} tNFA_EE_NCI_CONN;

/* data type for NFA_EE_NCI_ACTION_NTF_EVT */
typedef struct
{
    BT_HDR                      hdr;
    tNFC_EE_ACTION_REVT         *p_data;
} tNFA_EE_NCI_ACTION;

/* data type for NFA_EE_NCI_DISC_REQ_NTF_EVT */
typedef struct
{
    BT_HDR                      hdr;
    tNFC_EE_DISCOVER_REQ_REVT   *p_data;
} tNFA_EE_NCI_DISC_REQ;

/* union of all event data types */
typedef union
{
    BT_HDR                      hdr;
    tNFA_EE_CFG_HDR             cfg_hdr;
    tNFA_EE_API_DISCOVER        ee_discover;
    tNFA_EE_API_REGISTER        ee_register;
    tNFA_EE_API_DEREGISTER      deregister;
    tNFA_EE_API_MODE_SET        mode_set;
    tNFA_EE_API_SET_TECH_CFG    set_tech;
    tNFA_EE_API_SET_PROTO_CFG   set_proto;
    tNFA_EE_API_ADD_AID         add_aid;
    tNFA_EE_API_REMOVE_AID      rm_aid;
    tNFA_EE_API_LMRT_SIZE       lmrt_size;
    tNFA_EE_API_CONNECT         connect;
    tNFA_EE_API_SEND_DATA       send_data;
    tNFA_EE_API_DISCONNECT      disconnect;
    tNFA_EE_NCI_DISC_RSP        disc_rsp;
    tNFA_EE_NCI_DISC_NTF        disc_ntf;
    tNFA_EE_NCI_MODE_SET        mode_set_rsp;
    tNFA_EE_NCI_WAIT_RSP        wait_rsp;
    tNFA_EE_NCI_CONN            conn;
    tNFA_EE_NCI_ACTION          act;
    tNFA_EE_NCI_DISC_REQ        disc_req;
} tNFA_EE_MSG;

/* type for State Machine (SM) action functions */
typedef void (*tNFA_EE_SM_ACT)(tNFA_EE_MSG *p_data);

/*****************************************************************************
**  control block
*****************************************************************************/
#define NFA_EE_CFGED_UPDATE_NOW         0x80
#define NFA_EE_CFGED_OFF_ROUTING        0x40    /* either switch off or battery off is configured */

/* the following status are the definition used in ee_cfg_sts */
#define NFA_EE_STS_CHANGED_ROUTING      0x01
#define NFA_EE_STS_CHANGED_VS           0x02
#define NFA_EE_STS_CHANGED              0x0f
#define NFA_EE_STS_PREV_ROUTING         0x10
#define NFA_EE_STS_PREV                 0xf0


#define NFA_EE_WAIT_UPDATE              0x10    /* need to report NFA_EE_UPDATED_EVT */
#define NFA_EE_WAIT_UPDATE_RSP          0x20    /* waiting for the rsp of set routing commands */
#define NFA_EE_WAIT_UPDATE_ALL          0xF0

typedef UINT8 tNFA_EE_WAIT;

#define NFA_EE_FLAG_WAIT_HCI            0x01    /* set this bit when waiting for HCI to finish the initialization process in NFA_EE_EM_STATE_RESTORING */
#define NFA_EE_FLAG_NOTIFY_HCI          0x02    /* set this bit when EE needs to notify the p_enable_cback at the end of NFCEE discover process in NFA_EE_EM_STATE_RESTORING */
#define NFA_EE_FLAG_WAIT_DISCONN        0x04    /* set this bit when gracefully disable with outstanding NCI connections */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFA_EE_FLAG_CFG_NFC_DEP         0x05    /* set this bit when NFC-DEP is configured in the routing table */
#endif
typedef UINT8 tNFA_EE_FLAGS;


#define NFA_EE_DISC_STS_ON              0x00    /* NFCEE DISCOVER in progress       */
#define NFA_EE_DISC_STS_OFF             0x01    /* disable NFCEE DISCOVER           */
#define NFA_EE_DISC_STS_REQ             0x02    /* received NFCEE DISCOVER REQ NTF  */
typedef UINT8 tNFA_EE_DISC_STS;

typedef void (tNFA_EE_ENABLE_DONE_CBACK)(tNFA_EE_DISC_STS status);

/* NFA EE Management control block */
typedef struct
{
    tNFA_EE_ECB          ecb[NFA_EE_NUM_ECBS];   /* control block for DH and NFCEEs  */
    TIMER_LIST_ENT       timer;                  /* timer to send info to NFCC       */
    TIMER_LIST_ENT       discv_timer;            /* timer to end NFCEE discovery     */
    tNFA_EE_CBACK        *p_ee_cback[NFA_EE_MAX_CBACKS];/* to report EE events       */
    tNFA_EE_CBACK        *p_ee_disc_cback;       /* to report EE discovery result    */
    tNFA_EE_ENABLE_DONE_CBACK *p_enable_cback;   /* callback to notify on enable done*/
    tNFA_EE_EM_STATE     em_state;               /* NFA-EE state initialized or not  */
    UINT8                wait_rsp;               /* num of NCI rsp expected (update) */
    UINT8                num_ee_expecting;       /* number of ee_info still expecting*/
    UINT8                cur_ee;                 /* the number of ee_info in cb      */
    UINT8                ee_cfged;               /* the bit mask of configured ECBs  */
    UINT8                ee_cfg_sts;             /* configuration status             */
    tNFA_EE_WAIT         ee_wait_evt;            /* Pending event(s) to be reported  */
    tNFA_EE_FLAGS        ee_flags;               /* flags                            */
} tNFA_EE_CB;

/*****************************************************************************
**  External variables
*****************************************************************************/

/* NFA EE control block */
#if NFA_DYNAMIC_MEMORY == FALSE
extern tNFA_EE_CB nfa_ee_cb;
#else
extern tNFA_EE_CB *nfa_ee_cb_ptr;
#define nfa_ee_cb (*nfa_ee_cb_ptr)
#endif

/*****************************************************************************
**  External functions
*****************************************************************************/
/* function prototypes - exported from nfa_ee_main.c */
void nfa_ee_sys_enable (void);
void nfa_ee_sys_disable (void);

/* event handler function type */
BOOLEAN nfa_ee_evt_hdlr (BT_HDR *p_msg);
void nfa_ee_proc_nfcc_power_mode (UINT8 nfcc_power_mode);
#if (NFC_NFCEE_INCLUDED == TRUE)
void nfa_ee_get_tech_route (UINT8 power_state, UINT8 *p_handles);
#endif
void nfa_ee_proc_evt(tNFC_RESPONSE_EVT event, void *p_data);
tNFA_EE_ECB * nfa_ee_find_ecb (UINT8 nfcee_id);
tNFA_EE_ECB * nfa_ee_find_ecb_by_conn_id (UINT8 conn_id);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
UINT16 nfa_ee_lmrt_size();
UINT8 nfa_ee_get_supported_tech_list(UINT8 nfcee_id);
#endif
UINT8 nfa_ee_ecb_to_mask (tNFA_EE_ECB *p_cb);
void nfa_ee_restore_one_ecb (tNFA_EE_ECB *p_cb);
BOOLEAN nfa_ee_is_active (tNFA_HANDLE nfcee_id);

/* Action function prototypes - nfa_ee_act.c */
void nfa_ee_api_discover(tNFA_EE_MSG *p_data);
void nfa_ee_api_register(tNFA_EE_MSG *p_data);
void nfa_ee_api_deregister(tNFA_EE_MSG *p_data);
void nfa_ee_api_mode_set(tNFA_EE_MSG *p_data);
void nfa_ee_api_set_tech_cfg(tNFA_EE_MSG *p_data);
void nfa_ee_api_set_proto_cfg(tNFA_EE_MSG *p_data);
void nfa_ee_api_add_aid(tNFA_EE_MSG *p_data);
void nfa_ee_api_remove_aid(tNFA_EE_MSG *p_data);
void nfa_ee_api_lmrt_size(tNFA_EE_MSG *p_data);
void nfa_ee_api_update_now(tNFA_EE_MSG *p_data);
void nfa_ee_api_connect(tNFA_EE_MSG *p_data);
void nfa_ee_api_send_data(tNFA_EE_MSG *p_data);
void nfa_ee_api_disconnect(tNFA_EE_MSG *p_data);
void nfa_ee_report_disc_done(BOOLEAN notify_sys);
void nfa_ee_nci_disc_rsp(tNFA_EE_MSG *p_data);
void nfa_ee_nci_disc_ntf(tNFA_EE_MSG *p_data);
void nfa_ee_nci_mode_set_rsp(tNFA_EE_MSG *p_data);
void nfa_ee_nci_wait_rsp(tNFA_EE_MSG *p_data);
void nfa_ee_nci_conn(tNFA_EE_MSG *p_data);
void nfa_ee_nci_action_ntf(tNFA_EE_MSG *p_data);
void nfa_ee_nci_disc_req_ntf(tNFA_EE_MSG *p_data);
void nfa_ee_rout_timeout(tNFA_EE_MSG *p_data);
void nfa_ee_discv_timeout(tNFA_EE_MSG *p_data);
void nfa_ee_lmrt_to_nfcc(tNFA_EE_MSG *p_data);
void nfa_ee_update_rout(void);
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
BOOLEAN nfa_ee_nfeeid_active(UINT8 nfee_id);
#endif
void nfa_ee_report_event(tNFA_EE_CBACK *p_cback, tNFA_EE_EVT event, tNFA_EE_CBACK_DATA *p_data);
tNFA_EE_ECB * nfa_ee_find_aid_offset(UINT8 aid_len, UINT8 *p_aid, int *p_offset, int *p_entry);
void nfa_ee_remove_labels(void);
int nfa_ee_find_total_aid_len(tNFA_EE_ECB *p_cb, int start_entry);
void nfa_ee_start_timer(void);
void nfa_ee_reg_cback_enable_done (tNFA_EE_ENABLE_DONE_CBACK *p_cback);
void nfa_ee_report_update_evt (void);

extern void nfa_ee_proc_hci_info_cback (void);
void nfa_ee_check_disable (void);
BOOLEAN nfa_ee_restore_ntf_done(void);
void nfa_ee_check_restore_complete(void);


#endif /* NFA_P2P_INT_H */
