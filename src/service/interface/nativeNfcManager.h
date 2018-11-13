/******************************************************************************
 *
 *  Copyright (C) 2015 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License")
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

#ifndef __NATIVE_NFCMANAGER_TAG__H__
#define __NATIVE_NFCMANAGER_TAG__H__

#include "data_types.h"
#include "linux_nfc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
**
** Function:        isNfcActive
**
** Description:     Used externaly to determine if NFC is active or not.
**
** Returns:         'true' if the NFC stack is running, else 'false'.
**
*******************************************************************************/
BOOLEAN nativeNfcManager_isNfcActive();

/*******************************************************************************
**
** Function:        nfcManager_doInitialize
**
** Description:     Turn on NFC.
**                  e: JVM environment.
**                  o: Java object.
**
** Returns:         True if ok.
**
*******************************************************************************/
INT32 nativeNfcManager_doInitialize ();


/*******************************************************************************
**
** Function:        nfcManager_doDeinitialize
**
** Description:     Turn off NFC.
**
** Returns:         0 if ok.
**
*******************************************************************************/
INT32 nativeNfcManager_doDeinitialize ();


/*******************************************************************************
**
** Function:        nfcManager_enableDiscovery
**
** Description:     Start polling and listening for devices.
**                  technologies_mask: the bitmask of technologies for which to enable discovery
**                  reader mode: 
**                  enable_host_routing:
**                  restart:
**
** Returns:         0 if ok, error code otherwise
**
*******************************************************************************/
INT32 nativeNfcManager_enableDiscovery (INT32 technologies_mask,
    BOOLEAN reader_mode, INT32 enable_host_routing, BOOLEAN restart);


/*******************************************************************************
**
** Function:        nfcManager_disableDiscovery
**
** Description:     Stop polling and listening for devices.
**
** Returns:         0 if ok, error code otherwise
**
*******************************************************************************/
INT32 nativeNfcManager_disableDiscovery ();

void nativeNfcManager_registerTagCallback(nfcTagCallback_t *nfcTagCb);

void nativeNfcManager_deregisterTagCallback();

int nativeNfcManager_selectNextTag();

int nativeNfcManager_checkNextProtocol();

int nativeNfcManager_getNumTags();

void nativeNfcManager_registerHostCallback(nfcHostCardEmulationCallback_t *callback);
void nativeNfcManager_deregisterHostCallback();
    
/*******************************************************************************
**
** Function:        nativeNfcManager_sendRawFrame
**
** Description:     Send a raw frame.
**
** Returns:         True if ok.
**
*******************************************************************************/
INT32 nativeNfcManager_sendRawFrame (UINT8 *buf, UINT32 bufLen);

/*******************************************************************************
**
** Function:        nfcManager_doRegisterT3tIdentifier
**
** Description:     Registers LF_T3T_IDENTIFIER for NFC-F.
**                  t3tId: LF_T3T_IDENTIFIER value
**                  t3tIdsize: LF_T3T_IDENTIFIER size (10 or 18 bytes)
**
** Returns:         Handle retrieve from RoutingManager.
**
*******************************************************************************/
void nfcManager_registerT3tIdentifier(UINT8 *t3tId, UINT8 t3tIdsize);

/*******************************************************************************
**
** Function:        nfcManager_doDeregisterT3tIdentifier
**
** Description:     Deregisters LF_T3T_IDENTIFIER for NFC-F.
**
** Returns:         None
**
*******************************************************************************/
void nfcManager_doDeregisterT3tIdentifier(void);

#ifdef __cplusplus
}
#endif

#endif // __NATIVE_NFCMANAGER_TAG__H__
