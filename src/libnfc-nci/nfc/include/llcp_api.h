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
 *  This file contains the LLCP API definitions
 *
 ******************************************************************************/
#ifndef LLCP_API_H
#define LLCP_API_H

#include "nfc_target.h"
#include "llcp_defs.h"

/*****************************************************************************
**  Constants
*****************************************************************************/
#define LLCP_STATUS_SUCCESS         0       /* Successfully done                */
#define LLCP_STATUS_FAIL            1       /* Failed without specific reason   */
#define LLCP_STATUS_CONGESTED       2       /* Data link is congested           */

typedef UINT8 tLLCP_STATUS;

#define LLCP_MIN_OFFSET             (NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + LLCP_PDU_HEADER_SIZE + LLCP_SEQUENCE_SIZE)

#define LLCP_INVALID_SAP            0xFF    /* indication of failure to allocate data link resource */

/*****************************************************************************
**  Type Definitions
*****************************************************************************/
typedef struct
{
    BOOLEAN is_initiator;       /* TRUE if we are POLL mode */
    UINT8   max_payload_size;   /* 64, 128, 192 or 254 */
    UINT8   waiting_time;
    UINT8  *p_gen_bytes;
    UINT8   gen_bytes_len;
} tLLCP_ACTIVATE_CONFIG;

typedef struct
{
    UINT16  miu;                        /* Local receiving MIU      */
    UINT8   rw;                         /* Local receiving window   */
    char    sn[LLCP_MAX_SN_LEN + 1];    /* Service name to connect  */
} tLLCP_CONNECTION_PARAMS;

/*********************************
**  Callback Functions Prototypes
**********************************/

/* Link Management Callback Events */

#define LLCP_LINK_ACTIVATION_FAILED_EVT     0x00    /* Fail to activate link    */
#define LLCP_LINK_ACTIVATION_COMPLETE_EVT   0x01    /* LLCP Link is activated   */
#define LLCP_LINK_DEACTIVATED_EVT           0x02    /* LLCP Link is deactivated */
#define LLCP_LINK_FIRST_PACKET_RECEIVED_EVT 0x03    /* First LLCP packet received from remote */

/* Link Management Callback Reasons */

#define LLCP_LINK_SUCCESS                   0x00    /* Success                                  */
#define LLCP_LINK_VERSION_FAILED            0x01    /* Failed to agree version                  */
#define LLCP_LINK_BAD_GEN_BYTES             0x02    /* Failed to parse received general bytes   */
#define LLCP_LINK_INTERNAL_ERROR            0x03    /* internal error                           */
#define LLCP_LINK_LOCAL_INITIATED           0x04    /* Link has been deactivated by local       */
#define LLCP_LINK_REMOTE_INITIATED          0x05    /* Link has been deactivated by remote      */
#define LLCP_LINK_TIMEOUT                   0x06    /* Link has been deactivated by timeout     */
#define LLCP_LINK_FRAME_ERROR               0x07    /* Link has been deactivated by frame error */
#define LLCP_LINK_RF_LINK_LOSS_NO_RX_LLC    0x08    /* RF link loss without any rx LLC PDU      */
#define LLCP_LINK_RF_TRANSMISSION_ERR       NFC_STATUS_RF_TRANSMISSION_ERR
#define LLCP_LINK_RF_PROTOCOL_ERR           NFC_STATUS_RF_PROTOCOL_ERR
#define LLCP_LINK_RF_TIMEOUT                NFC_STATUS_TIMEOUT
#define LLCP_LINK_RF_LINK_LOSS_ERR          NFC_STATUS_LINK_LOSS

typedef void (tLLCP_LINK_CBACK) (UINT8 event, UINT8 reason);

/* Minimum length of Gen Bytes for LLCP */
/* In CE4 low power mode, NFCC can store up to 21 bytes */
#define LLCP_MIN_GEN_BYTES                  20

/* Service Access Point (SAP) Callback Events */

#define LLCP_SAP_EVT_DATA_IND               0x00    /* Received data on SAP         */
#define LLCP_SAP_EVT_CONNECT_IND            0x01    /* Connection request from peer */
#define LLCP_SAP_EVT_CONNECT_RESP           0x02    /* Connection accepted by peer  */
#define LLCP_SAP_EVT_DISCONNECT_IND         0x03    /* Received disconnect request  */
#define LLCP_SAP_EVT_DISCONNECT_RESP        0x04    /* Received disconnect response */
#define LLCP_SAP_EVT_CONGEST                0x05    /* congested status is changed  */
#define LLCP_SAP_EVT_LINK_STATUS            0x06    /* Change of LLCP Link status   */
#define LLCP_SAP_EVT_TX_COMPLETE            0x07    /* tx queue is empty and all PDU is acked   */

#define LLCP_LINK_TYPE_LOGICAL_DATA_LINK      0x01
#define LLCP_LINK_TYPE_DATA_LINK_CONNECTION   0x02

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_DATA_IND        */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
    UINT8   link_type;          /* link type                    */
} tLLCP_SAP_DATA_IND;

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_CONNECT_IND     */
    UINT8   server_sap;         /* SAP of local server          */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
    UINT16  miu;                /* MIU of peer device           */
    UINT8   rw;                 /* RW of peer device            */
    char   *p_service_name;     /* Service name (only for SDP)  */
} tLLCP_SAP_CONNECT_IND;

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_CONNECT_RESP    */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
    UINT16  miu;                /* MIU of peer device           */
    UINT8   rw;                 /* RW of peer device            */
} tLLCP_SAP_CONNECT_RESP;

#define LLCP_SAP_DISCONNECT_REASON_TIMEOUT  0x80
typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_DISCONNECT_IND  */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
} tLLCP_SAP_DISCONNECT_IND;

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_DISCONNECT_RESP */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
    UINT8   reason;             /* Reason of DM PDU if not timeout */
} tLLCP_SAP_DISCONNECT_RESP;

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_CONGEST         */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
    BOOLEAN is_congested;       /* TRUE if congested            */
    UINT8   link_type;          /* congested link type          */
} tLLCP_SAP_CONGEST;

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_LINK_STATUS     */
    UINT8   local_sap;          /* SAP of local device          */
    BOOLEAN is_activated;       /* TRUE if LLCP link is activated  */
    BOOLEAN is_initiator;       /* TRUE if local LLCP is initiator */
} tLLCP_SAP_LINK_STATUS;

typedef struct
{
    UINT8   event;              /* LLCP_SAP_EVT_TX_COMPLETE     */
    UINT8   local_sap;          /* SAP of local device          */
    UINT8   remote_sap;         /* SAP of remote device         */
} tLLCP_SAP_TX_COMPLETE;

typedef struct
{
    UINT8   event;              /* event                        */
    UINT8   local_sap;          /* SAP of local device          */
} tLLCP_SAP_HEADER;

typedef union
{
    tLLCP_SAP_HEADER            hdr;                /* common header                */
    tLLCP_SAP_DATA_IND          data_ind;           /* LLCP_SAP_EVT_DATA_IND        */
    tLLCP_SAP_CONNECT_IND       connect_ind;        /* LLCP_SAP_EVT_CONNECT_IND     */
    tLLCP_SAP_CONNECT_RESP      connect_resp;       /* LLCP_SAP_EVT_CONNECT_RESP    */
    tLLCP_SAP_DISCONNECT_IND    disconnect_ind;     /* LLCP_SAP_EVT_DISCONNECT_IND  */
    tLLCP_SAP_DISCONNECT_RESP   disconnect_resp;    /* LLCP_SAP_EVT_DISCONNECT_RESP */
    tLLCP_SAP_CONGEST           congest;            /* LLCP_SAP_EVT_CONGEST         */
    tLLCP_SAP_LINK_STATUS       link_status;        /* LLCP_SAP_EVT_LINK_STATUS     */
    tLLCP_SAP_TX_COMPLETE       tx_complete;        /* LLCP_SAP_EVT_TX_COMPLETE     */
} tLLCP_SAP_CBACK_DATA;

typedef void (tLLCP_APP_CBACK) (tLLCP_SAP_CBACK_DATA *p_data);

/* Service Discovery Callback */

typedef void (tLLCP_SDP_CBACK) (UINT8 tid, UINT8 remote_sap);

/* LLCP DTA Callback - notify DTA responded SNL for connectionless echo service */

typedef void (tLLCP_DTA_CBACK) (void);

/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
**
** Function         LLCP_SetConfig
**
** Description      Set configuration parameters for LLCP
**                  - Local Link MIU
**                  - Option parameter
**                  - Waiting Time Index
**                  - Local Link Timeout
**                  - Inactivity Timeout as initiator role
**                  - Inactivity Timeout as target role
**                  - Delay SYMM response
**                  - Data link connection timeout
**                  - Delay timeout to send first PDU as initiator
**
** Returns          void
**
*******************************************************************************/
LLCP_API extern void LLCP_SetConfig (UINT16 link_miu,
                                     UINT8  opt,
                                     UINT8  wt,
                                     UINT16 link_timeout,
                                     UINT16 inact_timeout_init,
                                     UINT16 inact_timeout_target,
                                     UINT16 symm_delay,
                                     UINT16 data_link_timeout,
                                     UINT16 delay_first_pdu_timeout);

/*******************************************************************************
**
** Function         LLCP_GetConfig
**
** Description      Get configuration parameters for LLCP
**                  - Local Link MIU
**                  - Option parameter
**                  - Response Waiting Time Index
**                  - Local Link Timeout
**                  - Inactivity Timeout as initiator role
**                  - Inactivity Timeout as target role
**                  - Delay SYMM response
**                  - Data link connection timeout
**                  - Delay timeout to send first PDU as initiator
**
** Returns          void
**
*******************************************************************************/
LLCP_API extern void LLCP_GetConfig (UINT16 *p_link_miu,
                                     UINT8  *p_opt,
                                     UINT8  *p_wt,
                                     UINT16 *p_link_timeout,
                                     UINT16 *p_inact_timeout_init,
                                     UINT16 *p_inact_timeout_target,
                                     UINT16 *p_symm_delay,
                                     UINT16 *p_data_link_timeout,
                                     UINT16 *p_delay_first_pdu_timeout);

/*******************************************************************************
**
** Function         LLCP_GetDiscoveryConfig
**
** Description      Returns discovery config for LLCP MAC link activation
**                  This function is called to get general bytes for NFC_PMID_ATR_REQ_GEN_BYTES
**                  or NFC_PMID_ATR_RES_GEN_BYTES before starting discovery.
**
**                  wt:Waiting time 0 - 8, only for listen
**                  p_gen_bytes: pointer to store LLCP magic number and paramters
**                  p_gen_bytes_len: length of buffer for gen bytes as input
**                                   (NOTE:it must be bigger than LLCP_MIN_GEN_BYTES)
**                                   actual gen bytes size as output
**
**                  Restrictions on the use of ISO 18092
**                  1. The DID features shall not be used.
**                  2. the NAD features shall not be used.
**                  3. Frame waiting time extentions (WTX) shall not be used.
**
** Returns          None
**
*******************************************************************************/
LLCP_API extern void LLCP_GetDiscoveryConfig (UINT8 *p_wt,
                                              UINT8 *p_gen_bytes,
                                              UINT8 *p_gen_bytes_len);

/*******************************************************************************
**
** Function         LLCP_ActivateLink
**
** Description      This function will activate LLCP link with LR, WT and Gen Bytes
**                  in activation NTF from NFCC.
**
**                  LLCP_LINK_ACTIVATION_COMPLETE_EVT will be returned through
**                  callback function if successful.
**                  Otherwise, LLCP_LINK_ACTIVATION_FAILED_EVT will be returned.
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_ActivateLink (tLLCP_ACTIVATE_CONFIG config,
                                                tLLCP_LINK_CBACK     *p_link_cback);

/*******************************************************************************
**
** Function         LLCP_DeactivateLink
**
** Description      Deactivate LLCP link
**
**                  LLCP_LINK_DEACTIVATED_EVT will be returned through callback
**                  when LLCP link is deactivated. Then NFC link may be deactivated.
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_DeactivateLink (void);

/*******************************************************************************
**
** Function         LLCP_RegisterServer
**
** Description      Register server and callback function
**
**                  reg_sap : Well-Known SAP except LM and SDP (0x02 - 0x0F)
**                            Advertized by SDP (0x10 - 0x1F)
**                            LLCP_INVALID_SAP, LLCP will allocate between 0x10 and 0x1F
**                  link_type : LLCP_LINK_TYPE_LOGICAL_DATA_LINK
**                              and/or LLCP_LINK_TYPE_DATA_LINK_CONNECTION
**                  p_service_name : Null-terminated string up to LLCP_MAX_SN_LEN
**
** Returns          SAP between 0x02 and 0x1F, if success
**                  LLCP_INVALID_SAP, otherwise
**
*******************************************************************************/
LLCP_API extern UINT8 LLCP_RegisterServer (UINT8           reg_sap,
                                           UINT8           link_type,
                                           char            *p_service_name,
                                           tLLCP_APP_CBACK *p_sap_cback);

/*******************************************************************************
**
** Function         LLCP_RegisterClient
**
** Description      Register client and callback function
**
**                  link_type : LLCP_LINK_TYPE_LOGICAL_DATA_LINK
**                              and/or LLCP_LINK_TYPE_DATA_LINK_CONNECTION
**
** Returns          SAP between 0x20 and 0x3F, if success
**                  LLCP_INVALID_SAP, otherwise
**
*******************************************************************************/
LLCP_API extern UINT8 LLCP_RegisterClient (UINT8           link_type,
                                           tLLCP_APP_CBACK *p_sap_cback);

/*******************************************************************************
**
** Function         LLCP_Deregister
**
** Description      Deregister server or client
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_Deregister (UINT8 sap);

/*******************************************************************************
**
** Function         LLCP_IsLogicalLinkCongested
**
** Description      Check if logical link is congested
**
**
** Returns          TRUE if congested
**
*******************************************************************************/
LLCP_API extern BOOLEAN LLCP_IsLogicalLinkCongested (UINT8 local_sap,
                                                     UINT8 num_pending_ui_pdu,
                                                     UINT8 total_pending_ui_pdu,
                                                     UINT8 total_pending_i_pdu);

/*******************************************************************************
**
** Function         LLCP_SendUI
**
** Description      Send connnectionless data to DSAP
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**                  LLCP_STATUS_CONGESTED if logical link is congested
**                  LLCP_STATUS_FAIL, otherwise
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_SendUI (UINT8 ssap, UINT8 dsap, BT_HDR *p_buf);

/*******************************************************************************
**
** Function         LLCP_ReadLogicalLinkData
**
** Description      Read information of UI PDU for local SAP
**
**                  - Remote SAP who sent UI PDU is returned.
**                  - Information of UI PDU up to max_data_len is copied into p_data.
**                  - Information of next UI PDU is not concatenated.
**                  - Recommended max_data_len is link MIU of local device
**
** Returns          TRUE if more information of UI PDU or more UI PDU in queue
**
*******************************************************************************/
LLCP_API extern BOOLEAN LLCP_ReadLogicalLinkData (UINT8  local_sap,
                                                  UINT32 max_data_len,
                                                  UINT8  *p_remote_sap,
                                                  UINT32 *p_data_len,
                                                  UINT8  *p_data);

/*******************************************************************************
**
** Function         LLCP_FlushLogicalLinkRxData
**
** Description      Discard received data in logical data link of local SAP
**
**
** Returns          length of data flushed
**
*******************************************************************************/
LLCP_API extern UINT32 LLCP_FlushLogicalLinkRxData (UINT8 local_sap);

/*******************************************************************************
**
** Function         LLCP_ConnectReq
**
** Description      Create data link connection between registered SAP and DSAP
**                  in peer LLCP,
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**                  LLCP_STATUS_FAIL, otherwise
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_ConnectReq (UINT8 reg_sap, UINT8 dsap,
                                              tLLCP_CONNECTION_PARAMS *p_params);

/*******************************************************************************
**
** Function         LLCP_ConnectCfm
**
** Description      Accept connection request from peer LLCP
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**                  LLCP_STATUS_FAIL, otherwise
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_ConnectCfm (UINT8 local_sap,
                                              UINT8 remote_sap,
                                              tLLCP_CONNECTION_PARAMS *p_params);

/*******************************************************************************
**
** Function         LLCP_ConnectReject
**
** Description      Reject connection request from peer LLCP
**
**                  reason : LLCP_SAP_DM_REASON_APP_REJECTED
**                           LLCP_SAP_DM_REASON_PERM_REJECT_THIS
**                           LLCP_SAP_DM_REASON_PERM_REJECT_ANY
**                           LLCP_SAP_DM_REASON_TEMP_REJECT_THIS
**                           LLCP_SAP_DM_REASON_TEMP_REJECT_ANY
**
** Returns          LLCP_STATUS_SUCCESS if success
**                  LLCP_STATUS_FAIL, otherwise
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_ConnectReject (UINT8 local_sap,
                                                 UINT8 remote_sap,
                                                 UINT8 reason);

/*******************************************************************************
**
** Function         LLCP_IsDataLinkCongested
**
** Description      Check if data link is congested
**
**
** Returns          TRUE if congested
**
*******************************************************************************/
LLCP_API extern BOOLEAN LLCP_IsDataLinkCongested (UINT8 local_sap,
                                                  UINT8 remote_sap,
                                                  UINT8 num_pending_i_pdu,
                                                  UINT8 total_pending_ui_pdu,
                                                  UINT8 total_pending_i_pdu);

/*******************************************************************************
**
** Function         LLCP_SendData
**
** Description      Send connection-oriented data
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**                  LLCP_STATUS_CONGESTED if data link is congested
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_SendData (UINT8  local_sap,
                                            UINT8  remote_sap,
                                            BT_HDR *p_buf);

/*******************************************************************************
**
** Function         LLCP_ReadDataLinkData
**
** Description      Read information of I PDU for data link connection
**
**                  - Information of I PDU up to max_data_len is copied into p_data.
**                  - Information of next I PDU is not concatenated.
**                  - Recommended max_data_len is data link connection MIU of local
**                    end point
**
** Returns          TRUE if more data in queue
**
*******************************************************************************/
LLCP_API extern BOOLEAN LLCP_ReadDataLinkData (UINT8  local_sap,
                                               UINT8  remote_sap,
                                               UINT32 max_data_len,
                                               UINT32 *p_data_len,
                                               UINT8  *p_data);

/*******************************************************************************
**
** Function         LLCP_FlushDataLinkRxData
**
** Description      Discard received data in data link connection
**
**
** Returns          length of rx data flushed
**
*******************************************************************************/
LLCP_API extern UINT32 LLCP_FlushDataLinkRxData (UINT8  local_sap,
                                                 UINT8  remote_sap);

/*******************************************************************************
**
** Function         LLCP_DisconnectReq
**
** Description      Disconnect data link
**                  discard any pending data if flush is set to TRUE
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_DisconnectReq (UINT8   local_sap,
                                                 UINT8   remote_sap,
                                                 BOOLEAN flush);

/*******************************************************************************
**
** Function         LLCP_SetTxCompleteNtf
**
** Description      This function is called to get LLCP_SAP_EVT_TX_COMPLETE
**                  when Tx queue is empty and all PDU is acked.
**                  This is one time event, so upper layer shall call this function
**                  again to get next LLCP_SAP_EVT_TX_COMPLETE.
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_SetTxCompleteNtf (UINT8 local_sap,
                                                    UINT8 remote_sap);

/*******************************************************************************
**
** Function         LLCP_SetLocalBusyStatus
**
** Description      Set local busy status
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_SetLocalBusyStatus (UINT8   local_sap,
                                                      UINT8   remote_sap,
                                                      BOOLEAN is_busy);

/*******************************************************************************
**
** Function         LLCP_GetRemoteWKS
**
** Description      Return well-known service bitmap of connected device
**
**
** Returns          WKS bitmap if success
**
*******************************************************************************/
LLCP_API extern UINT16 LLCP_GetRemoteWKS (void);

/*******************************************************************************
**
** Function         LLCP_GetRemoteLSC
**
** Description      Return link service class of connected device
**
**
** Returns          link service class
**
*******************************************************************************/
LLCP_API extern UINT8 LLCP_GetRemoteLSC (void);

/*******************************************************************************
**
** Function         LLCP_GetLinkMIU
**
** Description      Return local and remote link MIU
**
**
** Returns          None
**
*******************************************************************************/
LLCP_API extern void LLCP_GetLinkMIU (UINT16 *p_local_link_miu, UINT16 *p_remote_link_miu);

/*******************************************************************************
**
** Function         LLCP_DiscoverService
**
** Description      Return SAP of service name in connected device through callback
**
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
LLCP_API extern tLLCP_STATUS LLCP_DiscoverService (char            *p_name,
                                                   tLLCP_SDP_CBACK *p_cback,
                                                   UINT8           *p_tid);

/*******************************************************************************
**
** Function         LLCP_SetTraceLevel
**
** Description      This function sets the trace level for LLCP.  If called with
**                  a value of 0xFF, it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
LLCP_API extern UINT8 LLCP_SetTraceLevel (UINT8 new_level);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         LLCP_RegisterDtaCback
**
** Description      Register callback function for LLCP DTA testing
**
**
** Returns          void
**
*******************************************************************************/
LLCP_API extern void LLCP_RegisterDtaCback (tLLCP_DTA_CBACK *p_dta_cback);
#endif

#if (LLCP_TEST_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         LLCP_SetTestParams
**
** Description      Set test parameters for LLCP
**
**
** Returns          void
**
*******************************************************************************/
LLCP_API extern void LLCP_SetTestParams (UINT8 version, UINT16 wks);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* LLCP_API_H */
