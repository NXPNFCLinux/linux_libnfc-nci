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
 *  This is the private interface file for NFA_RW
 *
 ******************************************************************************/
#ifndef NFA_RW_INT_H
#define NFA_RW_INT_H

#include "nfa_sys.h"
#include "nfa_api.h"
#include "nfa_rw_api.h"
#include "nfc_api.h"
#include "rw_api.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/

/* Interval for performing presence check (in ms) */
#ifndef NFA_RW_PRESENCE_CHECK_INTERVAL
#define NFA_RW_PRESENCE_CHECK_INTERVAL  750
#endif

/* TLV detection status */
#define NFA_RW_TLV_DETECT_ST_OP_NOT_STARTED         0x00 /* No Tlv detected */
#define NFA_RW_TLV_DETECT_ST_LOCK_TLV_OP_COMPLETE   0x01 /* Lock control tlv detected */
#define NFA_RW_TLV_DETECT_ST_MEM_TLV_OP_COMPLETE    0x02 /* Memory control tlv detected */
#define NFA_RW_TLV_DETECT_ST_COMPLETE               0x03 /* Both Lock and Memory control Tlvs are detected */

typedef UINT8 tNFA_RW_TLV_ST;


/* RW events */
enum
{
    NFA_RW_OP_REQUEST_EVT = NFA_SYS_EVT_START (NFA_ID_RW),
    NFA_RW_ACTIVATE_NTF_EVT,
    NFA_RW_DEACTIVATE_NTF_EVT,
    NFA_RW_PRESENCE_CHECK_TICK_EVT,
    NFA_RW_PRESENCE_CHECK_TIMEOUT_EVT,
    NFA_RW_MAX_EVT
};



/* BTA_RW operations */
enum
{
    NFA_RW_OP_DETECT_NDEF,
    NFA_RW_OP_READ_NDEF,
    NFA_RW_OP_WRITE_NDEF,
    NFA_RW_OP_PRESENCE_CHECK,
    NFA_RW_OP_FORMAT_TAG,
    NFA_RW_OP_SEND_RAW_FRAME,

    /* Exclusive Type-1,Type-2 tag operations */
    NFA_RW_OP_DETECT_LOCK_TLV,
    NFA_RW_OP_DETECT_MEM_TLV,
    NFA_RW_OP_SET_TAG_RO,

    /* Exclusive Type-1 tag operations */
    NFA_RW_OP_T1T_RID,
    NFA_RW_OP_T1T_RALL,
    NFA_RW_OP_T1T_READ,
    NFA_RW_OP_T1T_WRITE,
    NFA_RW_OP_T1T_RSEG,
    NFA_RW_OP_T1T_READ8,
    NFA_RW_OP_T1T_WRITE8,

    /* Exclusive Type-2 tag operations */
    NFA_RW_OP_T2T_READ,
    NFA_RW_OP_T2T_WRITE,
    NFA_RW_OP_T2T_SECTOR_SELECT,

    /* Exclusive Type-3 tag operations */
    NFA_RW_OP_T3T_READ,
    NFA_RW_OP_T3T_WRITE,
    NFA_RW_OP_T3T_GET_SYSTEM_CODES,

    /* Exclusive ISO 15693 tag operations */
    NFA_RW_OP_I93_INVENTORY,
    NFA_RW_OP_I93_STAY_QUIET,
    NFA_RW_OP_I93_READ_SINGLE_BLOCK,
    NFA_RW_OP_I93_WRITE_SINGLE_BLOCK,
    NFA_RW_OP_I93_LOCK_BLOCK,
    NFA_RW_OP_I93_READ_MULTI_BLOCK,
    NFA_RW_OP_I93_WRITE_MULTI_BLOCK,
    NFA_RW_OP_I93_SELECT,
    NFA_RW_OP_I93_RESET_TO_READY,
    NFA_RW_OP_I93_WRITE_AFI,
    NFA_RW_OP_I93_LOCK_AFI,
    NFA_RW_OP_I93_WRITE_DSFID,
    NFA_RW_OP_I93_LOCK_DSFID,
    NFA_RW_OP_I93_GET_SYS_INFO,
    NFA_RW_OP_I93_GET_MULTI_BLOCK_STATUS,

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    NFA_RW_OP_T3BT_PUPI,
#endif

    NFA_RW_OP_MAX
};
typedef UINT8 tNFA_RW_OP;

/* Enumeration of parameter structios for nfa_rw operations */

/* NFA_RW_OP_WRITE_NDEF params */
typedef struct
{
    UINT32          len;
    UINT8           *p_data;
} tNFA_RW_OP_PARAMS_WRITE_NDEF;

/* NFA_RW_OP_SEND_RAW_FRAME params */
typedef struct
{
    BT_HDR          *p_data;
} tNFA_RW_OP_PARAMS_SEND_RAW_FRAME;

/* NFA_RW_OP_SET_TAG_RO params */
typedef struct
{
    BOOLEAN         b_hard_lock;
} tNFA_RW_OP_PARAMS_CONFIG_READ_ONLY;

/* NFA_RW_OP_T1T_READ params */
typedef struct
{
    UINT8           segment_number;
    UINT8           block_number;
    UINT8           index;
} tNFA_RW_OP_PARAMS_T1T_READ;

/* NFA_RW_OP_T1T_WRITE_E8,NFA_RW_OP_T1T_WRITE_NE8
   NFA_RW_OP_T1T_WRITE_E, NFA_RW_OP_T1T_WRITE_NE params  */
typedef struct
{
    BOOLEAN         b_erase;
    UINT8           block_number;
    UINT8           index;
    UINT8           p_block_data[8];
} tNFA_RW_OP_PARAMS_T1T_WRITE;

/* NFA_RW_OP_T2T_READ params */
typedef struct
{
    UINT8           block_number;
} tNFA_RW_OP_PARAMS_T2T_READ;

/* NFA_RW_OP_T2T_WRITE params */
typedef struct
{
    UINT8           block_number;
    UINT8           p_block_data[4];
} tNFA_RW_OP_PARAMS_T2T_WRITE;

/* NFA_RW_OP_T2T_SECTOR_SELECT params */
typedef struct
{
    UINT8           sector_number;
} tNFA_RW_OP_PARAMS_T2T_SECTOR_SELECT;

/* NFA_RW_OP_T3T_READ params */
typedef struct
{
    UINT8              num_blocks;
    tNFA_T3T_BLOCK_DESC *p_block_desc;
} tNFA_RW_OP_PARAMS_T3T_READ;

/* NFA_RW_OP_T3T_WRITE params */
typedef struct
{
    UINT8               num_blocks;
    tNFA_T3T_BLOCK_DESC *p_block_desc;
    UINT8               *p_block_data;
} tNFA_RW_OP_PARAMS_T3T_WRITE;

/* NFA_RW_OP_I93_XXX params */
typedef struct
{
    BOOLEAN             uid_present;
    UINT8               uid[I93_UID_BYTE_LEN];
    BOOLEAN             afi_present;
    UINT8               afi;
    UINT8               dsfid;
    UINT16              first_block_number;
    UINT16              number_blocks;
    UINT8              *p_data;
} tNFA_RW_OP_PARAMS_I93_CMD;

/* Union of params for all reader/writer operations */
typedef union
{
    /* params for NFA_RW_OP_WRITE_NDEF */
    tNFA_RW_OP_PARAMS_WRITE_NDEF        write_ndef;

    /* params for NFA_RW_OP_SEND_RAW_FRAME */
    tNFA_RW_OP_PARAMS_SEND_RAW_FRAME    send_raw_frame;

    /* params for NFA_RW_OP_SET_TAG_RO */
    tNFA_RW_OP_PARAMS_CONFIG_READ_ONLY  set_readonly;

    /* params for NFA_RW_OP_T2T_READ and NFA_RW_OP_T1T_WRITE */
    tNFA_RW_OP_PARAMS_T1T_READ          t1t_read;
    tNFA_RW_OP_PARAMS_T1T_WRITE         t1t_write;

    /* params for NFA_RW_OP_T2T_READ,NFA_RW_OP_T2T_WRITE and NFA_RW_OP_T2T_SECTOR_SELECT */
    tNFA_RW_OP_PARAMS_T2T_READ          t2t_read;
    tNFA_RW_OP_PARAMS_T2T_WRITE         t2t_write;
    tNFA_RW_OP_PARAMS_T2T_SECTOR_SELECT t2t_sector_select;

    /* params for NFA_RW_OP_T3T_READ and NFA_RW_OP_T3T_WRITE */
    tNFA_RW_OP_PARAMS_T3T_READ          t3t_read;
    tNFA_RW_OP_PARAMS_T3T_WRITE         t3t_write;

    /* params for NFA_RW_OP_PRESENCE_CHECK */
    tNFA_RW_PRES_CHK_OPTION             option;

    /* params for ISO 15693 */
    tNFA_RW_OP_PARAMS_I93_CMD           i93_cmd;

} tNFA_RW_OP_PARAMS;

/* data type for NFA_RW_op_req_EVT */
typedef struct
{
    BT_HDR              hdr;
    tNFA_RW_OP          op;     /* NFA RW operation */
    tNFA_RW_OP_PARAMS   params;
} tNFA_RW_OPERATION;

/* data type for NFA_RW_ACTIVATE_NTF */
typedef struct
{
    BT_HDR              hdr;
    tNFC_ACTIVATE_DEVT  *p_activate_params; /* Data from NFC_ACTIVATE_DEVT      */
    BOOLEAN             excl_rf_not_active; /* TRUE if not in exclusive RF mode */
} tNFA_RW_ACTIVATE_NTF;

/* union of all data types */
typedef union
{
    /* GKI event buffer header */
    BT_HDR                  hdr;
    tNFA_RW_OPERATION       op_req;
    tNFA_RW_ACTIVATE_NTF    activate_ntf;
} tNFA_RW_MSG;

/* NDEF detection status */
enum
{
    NFA_RW_NDEF_ST_UNKNOWN =0,      /* NDEF detection not performed yet */
    NFA_RW_NDEF_ST_TRUE,            /* Tag is NDEF */
    NFA_RW_NDEF_ST_FALSE            /* Tag is not NDEF */
};
typedef UINT8 tNFA_RW_NDEF_ST;

/* flags for RW control block */
#define NFA_RW_FL_NOT_EXCL_RF_MODE              0x01    /* Activation while not in exclusive RF mode                                */
#define NFA_RW_FL_AUTO_PRESENCE_CHECK_BUSY      0x02    /* Waiting for response from tag for auto-presence check                    */
#define NFA_RW_FL_TAG_IS_READONLY               0x04    /* Read only tag                                                            */
#define NFA_RW_FL_ACTIVATION_NTF_PENDING        0x08    /* Busy retrieving additional tag information                               */
#define NFA_RW_FL_API_BUSY                      0x10    /* Tag operation is in progress                                             */
#define NFA_RW_FL_ACTIVATED                     0x20    /* Tag is been activated                                                    */
#define NFA_RW_FL_NDEF_OK                       0x40    /* NDEF DETECTed OK                                                         */

/* NFA RW control block */
typedef struct
{
    tNFA_RW_OP      cur_op;         /* Current operation */

    TIMER_LIST_ENT  tle;            /* list entry for nfa_rw timer */
    tNFA_RW_MSG     *p_pending_msg; /* Pending API (if busy performing presence check) */

    /* Tag info */
    tNFC_PROTOCOL   protocol;
    tNFC_INTF_TYPE  intf_type;
    UINT8           pa_sel_res;
    tNFC_RF_TECH_N_MODE  activated_tech_mode;    /* activated technology and mode */

    BOOLEAN         b_hard_lock;

    tNFA_RW_MSG     *p_buffer_rw_msg; /* Buffer to hold incoming cmd while reading tag id */

    /* TLV info */
    tNFA_RW_TLV_ST  tlv_st;         /* TLV detection status */

    /* NDEF info */
    tNFA_RW_NDEF_ST ndef_st;        /* NDEF detection status */
    UINT32          ndef_max_size;  /* max number of bytes available for NDEF data */
    UINT32          ndef_cur_size;  /* current size of stored NDEF data (in bytes) */
    UINT8           *p_ndef_buf;
    UINT32          ndef_rd_offset; /* current read-offset of incoming NDEF data */

    /* Current NDEF Write info */
    UINT8           *p_ndef_wr_buf; /* Pointer to NDEF data being written */
    UINT32          ndef_wr_len;    /* Length of NDEF data being written */

    /* Reactivating type 2 tag after NACK rsp */
    tRW_EVENT       halt_event;     /* Event ID from stack after NACK response */
    tRW_DATA        rw_data;        /* Event Data from stack after NACK response */
    BOOLEAN         skip_dyn_locks; /* To skip reading dynamic locks during NDEF Detect */

    /* Flags (see defintions for NFA_RW_FL_* ) */
    UINT8           flags;

    /* ISO 15693 tag memory information */
    UINT16          i93_afi_location;
    UINT8           i93_dsfid;
    UINT8           i93_block_size;
    UINT16          i93_num_block;
    UINT8           i93_uid[I93_UID_BYTE_LEN];
} tNFA_RW_CB;
extern tNFA_RW_CB nfa_rw_cb;



/* type definition for action functions */
typedef BOOLEAN (*tNFA_RW_ACTION) (tNFA_RW_MSG *p_data);

/* Internal nfa_rw function prototypes */
extern void    nfa_rw_stop_presence_check_timer (void);

/* Action function prototypes */
extern BOOLEAN nfa_rw_handle_op_req (tNFA_RW_MSG *p_data);
extern BOOLEAN nfa_rw_activate_ntf (tNFA_RW_MSG *p_data);
extern BOOLEAN nfa_rw_deactivate_ntf (tNFA_RW_MSG *p_data);
extern BOOLEAN nfa_rw_presence_check_tick (tNFA_RW_MSG *p_data);
extern BOOLEAN nfa_rw_presence_check_timeout (tNFA_RW_MSG *p_data);
extern void    nfa_rw_handle_sleep_wakeup_rsp (tNFC_STATUS status);
extern void    nfa_rw_handle_presence_check_rsp (tNFC_STATUS status);
extern void    nfa_rw_command_complete (void);
extern BOOLEAN nfa_rw_handle_event (BT_HDR *p_msg);

extern void    nfa_rw_free_ndef_rx_buf (void);
extern void    nfa_rw_sys_disable (void);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
extern void nfa_rw_set_cback(tNFC_DISCOVER *p_data);
extern void nfa_rw_update_pupi_id(UINT8 *p, UINT8 len);
#endif

#endif /* NFA_DM_INT_H */
