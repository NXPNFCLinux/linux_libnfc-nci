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
 *  NFA interface to NFCEE
 *
 ******************************************************************************/
#ifndef NFA_EE_API_H
#define NFA_EE_API_H

#include "nfc_target.h"
#include "nfa_api.h"
#include "nfc_api.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/
#define NFA_MAX_AID_LEN             NFC_MAX_AID_LEN /* 16 per ISO 7816 specification    */
#define NFA_EE_HANDLE_DH            (NFA_HANDLE_GROUP_EE|NFC_DH_ID)
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
extern  UINT8 NFA_REMOVE_ALL_AID[];
#define NFA_REMOVE_ALL_AID_LEN      (0x08)
extern  UINT8 nfa_ee_ce_route_strict_disable;
#define NFA_EE_AE_NXP_PREFIX_MATCH      (0x10)
#endif

/* NFA EE callback events */
enum
{
    NFA_EE_DISCOVER_EVT,        /* The status for NFA_EeDiscover ()                      */
    NFA_EE_REGISTER_EVT,        /* The status for NFA_EeRegister ()                      */
    NFA_EE_DEREGISTER_EVT,      /* The status for NFA_EeDeregister ()                    */
    NFA_EE_MODE_SET_EVT,        /* The status for activating or deactivating an NFCEE    */
    NFA_EE_ADD_AID_EVT,         /* The status for adding an AID to a routing table entry */
    NFA_EE_REMOVE_AID_EVT,      /* The status for removing an AID from a routing table   */
    NFA_EE_REMAINING_SIZE_EVT,  /* The remaining size of the Listen Mode Routing Table   */
    NFA_EE_SET_TECH_CFG_EVT,    /* The status for setting the routing based on RF tech.  */
    NFA_EE_SET_PROTO_CFG_EVT,   /* The status for setting the routing based on protocols */
    NFA_EE_UPDATED_EVT,         /* The status for NFA_EeUpdateNow                        */
    NFA_EE_CONNECT_EVT,         /* Result of NFA_EeConnect                               */
    NFA_EE_DATA_EVT,            /* Received data from NFCEE.                             */
    NFA_EE_DISCONNECT_EVT,      /* NFCEE connection closed.                              */
    NFA_EE_NEW_EE_EVT,          /* A new NFCEE is discovered                             */
    NFA_EE_ACTION_EVT,          /* An action happened in NFCEE                           */
    NFA_EE_DISCOVER_REQ_EVT,    /* NFCEE Discover Request Notification                   */
    NFA_EE_ROUT_ERR_EVT,        /* Error - exceed NFCC CE Routing size                   */
    NFA_EE_NO_MEM_ERR_EVT,      /* Error - out of GKI buffers                            */
    NFA_EE_NO_CB_ERR_EVT        /* Error - Can not find control block or wrong state     */
};
typedef UINT8 tNFA_EE_EVT;

/* tNFA_NFCEE_INTERFACE values */
#define NFA_EE_INTERFACE_APDU         NFC_NFCEE_INTERFACE_APDU          /* APDU Interface       */
#define NFA_EE_INTERFACE_HCI_ACCESS   NFC_NFCEE_INTERFACE_HCI_ACCESS    /* HCI Access Interface*/
#define NFA_EE_INTERFACE_T3T          NFC_NFCEE_INTERFACE_T3T           /* T3T Command Interface*/
#define NFA_EE_INTERFACE_TRANSPARENT  NFC_NFCEE_INTERFACE_TRANSPARENT   /* Transparent Interface*/
#define NFA_EE_INTERFACE_PROPRIETARY  NFC_NFCEE_INTERFACE_PROPRIETARY   /* Proprietary          */
typedef UINT8 tNFA_EE_INTERFACE;

#define NFA_EE_TAG_HW_ID             NFC_NFCEE_TAG_HW_ID                /* HW/Registration ID   */
#define NFA_EE_TAG_ATR_BYTES         NFC_NFCEE_TAG_ATR_BYTES            /* ATR Bytes            */
#define NFA_EE_TAG_T3T_INFO          NFC_NFCEE_TAG_T3T_INFO             /* T3T Supplement. Info */
#define NFA_EE_TAG_HCI_HOST_ID       NFC_NFCEE_TAG_HCI_HOST_ID          /* Broadcom Proprietary */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
typedef UINT16 tNFA_EE_TAG;
#else
typedef UINT8 tNFA_EE_TAG;
#endif

/* for NFA_EeModeSet () */
#define NFA_EE_MD_ACTIVATE          NFC_MODE_ACTIVATE
#define NFA_EE_MD_DEACTIVATE        NFC_MODE_DEACTIVATE
typedef UINT8 tNFA_EE_MD;

#define NFA_EE_PWR_STATE_ON         0x01    /* The device is on                 */
#define NFA_EE_PWR_STATE_SWITCH_OFF 0x02    /* The device is switched off       */
#define NFA_EE_PWR_STATE_BATT_OFF   0x04    /* The device's battery is removed  */
#define NFA_EE_PWR_STATE_NONE       0       /* used to remove a particular technology or protocol based routing cfg of a handle from the routing table. */
typedef UINT8 tNFA_EE_PWR_STATE;


#define NFA_EE_STATUS_INACTIVE          NFC_NFCEE_STATUS_INACTIVE /* NFCEE connected and inactive */
#define NFA_EE_STATUS_ACTIVE            NFC_NFCEE_STATUS_ACTIVE   /* NFCEE connected and active   */
#define NFA_EE_STATUS_REMOVED           NFC_NFCEE_STATUS_REMOVED  /* NFCEE removed                */
#define NFA_EE_STATUS_PENDING           0x10                      /* waiting for response from NFCC */
#define NFA_EE_STATUS_ACTIVATING        (NFA_EE_STATUS_PENDING+NFC_NFCEE_STATUS_ACTIVE)
#define NFA_EE_STATUS_DEACTIVATING      (NFA_EE_STATUS_PENDING+NFC_NFCEE_STATUS_INACTIVE)
typedef UINT8 tNFA_EE_STATUS;



/* additional NFCEE Info */
typedef struct
{
    tNFA_EE_TAG             tag;
    UINT8                   len;
    UINT8                   info[NFC_MAX_EE_INFO];
} tNFA_EE_TLV;

typedef struct
{
    tNFA_HANDLE         ee_handle;              /* handle for NFCEE oe DH   */
    tNFA_EE_STATUS      ee_status;              /* The NFCEE status         */
    UINT8               num_interface;          /* number of NFCEE interface*/
    tNFA_EE_INTERFACE   ee_interface[NFC_MAX_EE_INTERFACE];/* NFCEE supported interface */
    UINT8               num_tlvs;               /* number of TLVs           */
    tNFA_EE_TLV         ee_tlv[NFC_MAX_EE_TLVS];/* the TLV                  */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_NFC_PROTOCOL       la_protocol;        /* Listen A protocol    */
    tNFA_NFC_PROTOCOL       lb_protocol;        /* Listen B protocol    */
    tNFA_NFC_PROTOCOL       lf_protocol;        /* Listen F protocol    */
    tNFA_NFC_PROTOCOL       lbp_protocol;       /* Listen B' protocol   */
#endif
} tNFA_EE_INFO;



typedef struct
{
    tNFA_STATUS         status;                         /* NFA_STATUS_OK is successful      */
    UINT8               num_ee;                         /* number of NFCEEs found           */
    tNFA_EE_INFO        ee_info[NFA_EE_MAX_EE_SUPPORTED];/*NFCEE information                */
} tNFA_EE_DISCOVER;

typedef struct
{
    tNFA_HANDLE         ee_handle;      /* Handle of NFCEE                                  */
    tNFA_STATUS         status;         /* NFA_STATUS_OK is successful                      */
    tNFA_EE_INTERFACE   ee_interface;   /* NFCEE interface associated with this connection  */
} tNFA_EE_CONNECT;

#define NFA_EE_TRGR_SELECT              NFC_EE_TRIG_SELECT       /* ISO 7816-4 SELECT command */
#define NFA_EE_TRGR_RF_PROTOCOL         NFC_EE_TRIG_RF_PROTOCOL  /* RF Protocol changed       */
#define NFA_EE_TRGR_RF_TECHNOLOGY       NFC_EE_TRIG_RF_TECHNOLOGY/* RF Technology changed     */
#define NFA_EE_TRGR_APP_INIT            NFC_EE_TRIG_APP_INIT     /* Application initiation    */
typedef tNFC_EE_TRIGGER tNFA_EE_TRIGGER;

/* Union of NFCEE action parameter depending on the associated trigger */
typedef union
{
    tNFA_NFC_PROTOCOL   protocol;   /* NFA_EE_TRGR_RF_PROTOCOL: the protocol that triggers this event */
    tNFC_RF_TECH        technology; /* NFA_EE_TRGR_RF_TECHNOLOGY:the technology that triggers this event */
    tNFC_AID            aid;        /* NFA_EE_TRGR_SELECT      : the AID in the received SELECT AID command */
    tNFC_APP_INIT       app_init;   /* NFA_EE_TRGR_APP_INIT:     The information for the application initiated trigger */
} tNFA_EE_ACTION_PARAM;

typedef struct
{
    tNFA_HANDLE             ee_handle;      /* Handle of NFCEE                  */
    tNFA_EE_TRIGGER         trigger;        /* the trigger of this event        */
    tNFA_EE_ACTION_PARAM    param;
} tNFA_EE_ACTION;

typedef struct
{
    tNFA_HANDLE         ee_handle;  /* Handle of NFCEE              */
    tNFA_STATUS         status;     /* NFA_STATUS_OK is successful  */
    tNFA_EE_STATUS      ee_status;  /* The NFCEE status             */
} tNFA_EE_MODE_SET;

typedef struct
{
    tNFA_HANDLE         ee_handle;          /* Handle of MFCEE      */
    tNFA_NFC_PROTOCOL   la_protocol;        /* Listen A protocol    */
    tNFA_NFC_PROTOCOL   lb_protocol;        /* Listen B protocol    */
    tNFA_NFC_PROTOCOL   lf_protocol;        /* Listen F protocol    */
    tNFA_NFC_PROTOCOL   lbp_protocol;       /* Listen B' protocol   */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_NFC_PROTOCOL   pa_protocol;        /* Passive poll A SWP Reader   */
    tNFA_NFC_PROTOCOL   pb_protocol;        /* Passive poll B SWP Reader   */
    UINT8               ee_req_op;          /* add or remove req ntf*/
#endif
} tNFA_EE_DISCOVER_INFO;

/* Data for NFA_EE_DISCOVER_REQ_EVT */
typedef struct
{
    UINT8                   status;                         /* NFA_STATUS_OK if successful   */
    UINT8                   num_ee;                         /* number of MFCEE information   */
    tNFA_EE_DISCOVER_INFO   ee_disc_info[NFA_EE_MAX_EE_SUPPORTED - 1]; /* NFCEE DISCOVER Request info   */
} tNFA_EE_DISCOVER_REQ;

/* Data for NFA_EE_DATA_EVT */
typedef struct
{
    tNFA_HANDLE handle;     /* Connection handle */
    UINT16      len;        /* Length of data    */
    UINT8       *p_buf;     /* Data buffer       */
} tNFA_EE_DATA;

/* Union of all EE callback structures */
typedef union
{
    tNFA_STATUS             status;             /* NFA_STATUS_OK is successful; otherwise NFA_STATUS_FAILED */
    tNFA_EE_DATA            data;
    tNFA_HANDLE             handle;
    tNFA_EE_DISCOVER        ee_discover;
    tNFA_STATUS             ee_register;
    tNFA_STATUS             deregister;
    tNFA_STATUS             add_aid;
    tNFA_STATUS             remove_aid;
    tNFA_STATUS             set_tech;
    tNFA_STATUS             set_proto;
    UINT16                  size;
    tNFA_EE_CONNECT         connect;
    tNFA_EE_ACTION          action;
    tNFA_EE_MODE_SET        mode_set;
    tNFA_EE_INFO            new_ee;
    tNFA_EE_DISCOVER_REQ    discover_req;
} tNFA_EE_CBACK_DATA;


/* EE callback */
typedef void (tNFA_EE_CBACK) (tNFA_EE_EVT event, tNFA_EE_CBACK_DATA *p_data);

/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
**
** Function         NFA_EeDiscover
**
** Description      This function retrieves the NFCEE information from NFCC.
**                  The NFCEE information is reported in NFA_EE_DISCOVER_EVT.
**
**                  This function may be called when a system supports removable
**                  NFCEEs,
**
** Returns          NFA_STATUS_OK if information is retrieved successfully
**                  NFA_STATUS_FAILED If wrong state (retry later)
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeDiscover (tNFA_EE_CBACK *p_cback);

/*******************************************************************************
**
** Function         NFA_EeGetInfo
**
** Description      This function retrieves the NFCEE information from NFA.
**                  The actual number of NFCEE is returned in p_num_nfcee
**                  and NFCEE information is returned in p_info
**
** Returns          NFA_STATUS_OK if information is retrieved successfully
**                  NFA_STATUS_FAILED If wrong state (retry later)
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeGetInfo (UINT8        *p_num_nfcee,
                                          tNFA_EE_INFO *p_info);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         NFA_AllEeGetInfo
**
** Description      This function retrieves the All NFCEE's independent of
**                  their status information from NFA.
**                  The actual number of NFCEE is returned in p_num_nfcee
**                  and NFCEE information is returned in p_info
**
** Returns          NFA_STATUS_OK if information is retrieved successfully
**                  NFA_STATUS_FAILED If wrong state (retry later)
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_AllEeGetInfo (UINT8        *p_num_nfcee,
                                             tNFA_EE_INFO *p_info);
#endif

/*******************************************************************************
**
** Function         NFA_EeRegister
**
** Description      This function registers a callback function to receive the
**                  events from NFA-EE module.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeRegister (tNFA_EE_CBACK *p_cback);

/*******************************************************************************
**
** Function         NFA_EeDeregister
**
** Description      This function de-registers the callback function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeDeregister (tNFA_EE_CBACK *p_cback);

/*******************************************************************************
**
** Function         NFA_EeModeSet
**
** Description      This function is called to activate (mode = NFA_EE_MD_ACTIVATE)
**                  or deactivate (mode = NFA_EE_MD_DEACTIVATE) the NFCEE
**                  identified by the given ee_handle. The result of this
**                  operation is reported with the NFA_EE_MODE_SET_EVT.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeModeSet (tNFA_HANDLE    ee_handle,
                                          tNFA_EE_MD     mode);


/*******************************************************************************
**
** Function         NFA_EeSetDefaultTechRouting
**
** Description      This function is called to add, change or remove the
**                  default routing based on RF technology in the listen mode
**                  routing table for the given ee_handle. The status of this
**                  operation is reported as the NFA_EE_SET_TECH_CFG_EVT.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Note:            NFA_EeUpdateNow() should be called after last NFA-EE function
**                  to change the listen mode routing is called.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeSetDefaultTechRouting (tNFA_HANDLE          ee_handle,
                                                        tNFA_TECHNOLOGY_MASK technologies_switch_on,
                                                        tNFA_TECHNOLOGY_MASK technologies_switch_off,
                                                        tNFA_TECHNOLOGY_MASK technologies_battery_off
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                                                       ,tNFA_TECHNOLOGY_MASK technologies_screen_lock,
                                                        tNFA_TECHNOLOGY_MASK technologies_screen_off
#endif

);

/*******************************************************************************
**
** Function         NFA_EeSetDefaultProtoRouting
**
** Description      This function is called to add, change or remove the
**                  default routing based on Protocol in the listen mode routing
**                  table for the given ee_handle. The status of this
**                  operation is reported as the NFA_EE_SET_PROTO_CFG_EVT.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Note:            NFA_EeUpdateNow() should be called after last NFA-EE function
**                  to change the listen mode routing is called.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeSetDefaultProtoRouting (tNFA_HANDLE         ee_handle,
                                                         tNFA_PROTOCOL_MASK  protocols_switch_on,
                                                         tNFA_PROTOCOL_MASK  protocols_switch_off,
                                                         tNFA_PROTOCOL_MASK  protocols_battery_off
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                                                        ,tNFA_PROTOCOL_MASK  protocols_screen_lock,
                                                         tNFA_PROTOCOL_MASK  protocols_screen_off
#endif
);

/*******************************************************************************
**
** Function         NFA_EeAddAidRouting
**
** Description      This function is called to add an AID entry in the
**                  listen mode routing table in NFCC. The status of this
**                  operation is reported as the NFA_EE_ADD_AID_EVT.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Note:            NFA_EeUpdateNow() should be called after last NFA-EE function
**                  to change the listen mode routing is called.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeAddAidRouting (tNFA_HANDLE          ee_handle,
                                                UINT8                aid_len,
                                                UINT8               *p_aid,
                                                tNFA_EE_PWR_STATE     power_state
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                                                , UINT8              vs_info
#endif
);

/*******************************************************************************
**
** Function         NFA_EeRemoveAidRouting
**
** Description      This function is called to remove the given AID entry from the
**                  listen mode routing table. If the entry configures VS,
**                  it is also removed. The status of this operation is reported
**                  as the NFA_EE_REMOVE_AID_EVT.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Note:            NFA_EeUpdateNow() should be called after last NFA-EE function
**                  to change the listen mode routing is called.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeRemoveAidRouting (UINT8     aid_len,
                                                   UINT8    *p_aid);

/*******************************************************************************
**
** Function         NFA_EeGetLmrtRemainingSize
**
** Description      This function is called to get remaining size of the
**                  Listen Mode Routing Table.
**                  The remaining size is reported in NFA_EE_REMAINING_SIZE_EVT
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeGetLmrtRemainingSize (void);

/*******************************************************************************
**
** Function         NFA_EeUpdateNow
**
** Description      This function is called to send the current listen mode
**                  routing table and VS configuration to the NFCC (without waiting
**                  for NFA_EE_ROUT_TIMEOUT_VAL).
**
**                  The status of this operation is
**                  reported with the NFA_EE_UPDATED_EVT.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_SEMANTIC_ERROR is update is currently in progress
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeUpdateNow (void);

/*******************************************************************************
**
** Function         NFA_EeConnect
**
** Description      Open connection to an NFCEE attached to the NFCC
**
**                  The status of this operation is
**                  reported with the NFA_EE_CONNECT_EVT.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeConnect (tNFA_HANDLE    ee_handle,
                                          UINT8          ee_interface,
                                          tNFA_EE_CBACK *p_cback);

/*******************************************************************************
**
** Function         NFA_EeSendData
**
** Description      Send data to the given NFCEE.
**                  This function shall be called after NFA_EE_CONNECT_EVT is reported
**                  and before NFA_EeDisconnect is called on the given ee_handle.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeSendData (tNFA_HANDLE  ee_handle,
                                           UINT16       data_len,
                                           UINT8       *p_data);

/*******************************************************************************
**
** Function         NFA_EeDisconnect
**
** Description      Disconnect (if a connection is currently open) from an
**                  NFCEE interface. The result of this operation is reported
**                  with the NFA_EE_DISCONNECT_EVT.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_EeDisconnect (tNFA_HANDLE ee_handle);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         NFA_AddEePowerState
**
** Description      This function is called to add power state in the
**                  listen mode routing table in NFCC.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Note:            NFA_EeUpdateNow() should be called after last NFA-EE function
**                  to change the listen mode routing is called.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_AddEePowerState(tNFA_HANDLE          ee_handle,
                                tNFA_EE_PWR_STATE    power_state_mask);

/*******************************************************************************
**
** Function         NFA_GetAidTableSize
**
** Description      This function is called to get the AID routing table size.
**
** Returns          Maximum AID routing table size.
**
*******************************************************************************/
NFC_API extern UINT16 NFA_GetAidTableSize();

/*******************************************************************************
**
** Function         NFA_GetRemainingAidTableSize
**
** Description      This function is called to get the remaining AID routing
**                  table size.
**
** Returns          Remaining AID routing table size.
**
*******************************************************************************/
NFC_API extern UINT16 NFA_GetRemainingAidTableSize();

/*******************************************************************************
**
** Function         NFA_SetCEStrictDisable
**
** Description      This function is called to set the flag for Strict CE.
**
** Returns          None.
**
*******************************************************************************/
NFC_API extern void NFA_SetCEStrictDisable(UINT32 state);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NFA_EE_API_H */
