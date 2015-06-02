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
 *  NFA card emulation API functions
 *
 ******************************************************************************/
#ifndef NFA_CE_API_H
#define NFA_CE_API_H

#include "nfc_target.h"
#include "nfa_api.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/

/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
**
** Function         NFA_CeConfigureLocalTag
**
** Description      Configure local NDEF tag.
**
**                  Tag events will be notifed using the tNFA_CONN_CBACK
**                  (registered during NFA_Enable)
**
**                  The NFA_CE_LOCAL_TAG_CONFIGURED_EVT reports the status of the
**                  operation.
**
**                  Activation and deactivation are reported using the
**                  NFA_ACTIVATED_EVT and NFA_DEACTIVATED_EVT events
**
**                  If a write-request is received to update the tag memory,
**                  an NFA_CE_NDEF_WRITE_CPLT_EVT will notify the application, along
**                  with a buffer containing the updated contents.
**
**                  To disable the local NDEF tag, set protocol_mask=0
**
**                  The NDEF data provided by p_ndef_data must be persistent
**                  as long as the local NDEF tag is enabled. Also, Input parameters p_uid and
**                  uid_len are reserved for future use.
**
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function.
**
** Returns:
**                  NFA_STATUS_OK,            if command accepted
**                  NFA_STATUS_INVALID_PARAM,
**                      if protocol_maks is not 0 and p_ndef_data is NULL
**                  (or) uid_len is not 0
**                  (or) if protocol mask is set for Type 1 or Type 2
**
**                  NFA_STATUS_FAILED:        otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeConfigureLocalTag (tNFA_PROTOCOL_MASK protocol_mask,
                                                    UINT8     *p_ndef_data,
                                                    UINT16    ndef_cur_size,
                                                    UINT16    ndef_max_size,
                                                    BOOLEAN   read_only,
                                                    UINT8     uid_len,
                                                    UINT8     *p_uid);

/*******************************************************************************
**
** Function         NFA_CeConfigureUiccListenTech
**
** Description      Configure listening for the UICC, using the specified
**                  technologies.
**
**                  Events will be notifed using the tNFA_CONN_CBACK
**                  (registered during NFA_Enable)
**
**                  The NFA_CE_UICC_LISTEN_CONFIGURED_EVT reports the status of the
**                  operation.
**
**                  Activation and deactivation are reported using the
**                  NFA_ACTIVATED_EVT and NFA_DEACTIVATED_EVT events
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns:
**                  NFA_STATUS_OK, if command accepted
**                  NFA_STATUS_FAILED: otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeConfigureUiccListenTech (tNFA_HANDLE          ee_handle,
                                                          tNFA_TECHNOLOGY_MASK tech_mask);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         NFA_CeConfigureEseListenTech
**
** Description      Configure listening for the Ese, using the specified
**                  technologies.
**
**                  Events will be notifed using the tNFA_CONN_CBACK
**                  (registered during NFA_Enable)
**
**                  The NFA_CE_ESE_LISTEN_CONFIGURED_EVT reports the status of the
**                  operation.
**
**                  Activation and deactivation are reported using the
**                  NFA_ACTIVATED_EVT and NFA_DEACTIVATED_EVT events
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns:
**                  NFA_STATUS_OK, if command accepted
**                  NFA_STATUS_FAILED: otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeConfigureEseListenTech (tNFA_HANDLE ee_handle,
                                           tNFA_TECHNOLOGY_MASK tech_mask);
#endif
/*******************************************************************************
**
** Function         NFA_CeRegisterFelicaSystemCodeOnDH
**
** Description      Register listening callback for Felica system code
**
**                  The NFA_CE_REGISTERED_EVT reports the status of the
**                  operation.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns:
**                  NFA_STATUS_OK, if command accepted
**                  NFA_STATUS_FAILED: otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeRegisterFelicaSystemCodeOnDH (UINT16           system_code,
                                                               UINT8            nfcid2[NCI_RF_F_UID_LEN],
                                                               tNFA_CONN_CBACK  *p_conn_cback);

/*******************************************************************************
**
** Function         NFA_CeDeregisterFelicaSystemCodeOnDH
**
** Description      Deregister listening callback for Felica
**                  (previously registered using NFA_CeRegisterFelicaSystemCodeOnDH)
**
**                  The NFA_CE_DEREGISTERED_EVT reports the status of the
**                  operation.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if invalid handle
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeDeregisterFelicaSystemCodeOnDH (tNFA_HANDLE handle);

/*******************************************************************************
**
** Function         NFA_CeRegisterAidOnDH
**
** Description      Register listening callback for the specified ISODEP AID
**
**                  The NFA_CE_REGISTERED_EVT reports the status of the
**                  operation.
**
**                  If no AID is specified (aid_len=0), then p_conn_cback will
**                  will get notifications for any AIDs routed to the DH. This
**                  over-rides callbacks registered for specific AIDs.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns:
**                  NFA_STATUS_OK, if command accepted
**                  NFA_STATUS_FAILED: otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeRegisterAidOnDH (UINT8           aid[NFC_MAX_AID_LEN],
                                                  UINT8           aid_len,
                                                  tNFA_CONN_CBACK *p_conn_cback);

/*******************************************************************************
**
** Function         NFA_CeDeregisterAidOnDH
**
** Description      Deregister listening callback for ISODEP AID
**                  (previously registered using NFA_CeRegisterAidOnDH)
**
**                  The NFA_CE_DEREGISTERED_EVT reports the status of the
**                  operation.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BAD_HANDLE if invalid handle
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeDeregisterAidOnDH (tNFA_HANDLE handle);

/*******************************************************************************
**
** Function         NFA_CeSetIsoDepListenTech
**
** Description      Set the technologies (NFC-A and/or NFC-B) to listen for when
**                  NFA_CeConfigureLocalTag or NFA_CeDeregisterAidOnDH are called.
**
**                  By default (if this API is not called), NFA will listen
**                  for both NFC-A and NFC-B for ISODEP.
**
** Note:            If listening for ISODEP on UICC, the DH listen callbacks
**                  may still get activate notifications for ISODEP if the reader/
**                  writer selects an AID that is not routed to the UICC (regardless
**                  of whether A or B was disabled using NFA_CeSetIsoDepListenTech)
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns:
**                  NFA_STATUS_OK, if command accepted
**                  NFA_STATUS_FAILED: otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_CeSetIsoDepListenTech (tNFA_TECHNOLOGY_MASK tech_mask);

#ifdef __cplusplus
}
#endif

#endif /* NFA_CE_API_H */
