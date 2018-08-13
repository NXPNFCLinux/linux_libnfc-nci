/*
 * Copyright (C) 2010-2014 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _PHNXPEXTNS_H_
#define _PHNXPEXTNS_H_

#include <nfa_api.h>
#include <sys/types.h>
#include <errno.h>
#include <phNfcStatus.h>
#ifdef __cplusplus
extern "C" {
#endif

NFCSTATUS EXTNS_Init(tNFA_DM_CBACK        *p_dm_cback,
                     tNFA_CONN_CBACK      *p_conn_cback);
void EXTNS_Close(void);
NFCSTATUS EXTNS_MfcInit(tNFA_ACTIVATED activationData);
NFCSTATUS EXTNS_MfcCheckNDef(void);
NFCSTATUS EXTNS_MfcReadNDef(void);
NFCSTATUS EXTNS_MfcPresenceCheck(void);
NFCSTATUS EXTNS_MfcWriteNDef(uint8_t *pBuf, uint32_t len);
NFCSTATUS EXTNS_MfcFormatTag(uint8_t *key, uint8_t len);
NFCSTATUS EXTNS_MfcDisconnect(void);
NFCSTATUS EXTNS_MfcActivated(void);
NFCSTATUS EXTNS_MfcTransceive(uint8_t *p_data, uint32_t len);
NFCSTATUS EXTNS_MfcRegisterNDefTypeHandler (tNFA_NDEF_CBACK *ndefHandlerCallback);
NFCSTATUS EXTNS_MfcCallBack(uint8_t *buf, uint32_t buflen);
NFCSTATUS EXTNS_MfcSetReadOnly (uint8_t *key, uint8_t len);
void   EXTNS_SetConnectFlag(bool_t flagval);
bool_t EXTNS_GetConnectFlag(void);
void   EXTNS_SetDeactivateFlag(bool_t flagval);
bool_t EXTNS_GetDeactivateFlag(void);
void   EXTNS_SetCallBackFlag(bool_t flagval);
bool_t EXTNS_GetCallBackFlag(void);
NFCSTATUS EXTNS_CheckMfcResponse(uint8_t** sTransceiveData, uint32_t *sTransceiveDataLen);
void MfcPresenceCheckResult(NFCSTATUS status);
void MfcResetPresenceCheckStatus(void);
NFCSTATUS EXTNS_GetPresenceCheckStatus(void);
/*NFC-SPI synchronization related apis*/
NFCSTATUS EXTNS_EseNfcSyncInit(void);
NFCSTATUS EXTNS_EseNfcSyncDeInit(void);
NFCSTATUS EXTNS_NfcReqResourceIoctl(double time_limit);
NFCSTATUS EXTNS_NfcReleaseResourceIoctl(double time_limit);

#ifdef __cplusplus
}
#endif

/*
 * Events from JNI for NXP Extensions
 */
#define PH_NXPEXTNS_MIFARE_CHECK_NDEF   0x01   /* MIFARE Check Ndef */
#define PH_NXPEXTNS_MIFARE_READ_NDEF    0x02   /* MIFARE Read Ndef */
#define PH_NXPEXTNS_MIFARE_WRITE_NDEF   0x03   /* MIFARE Write Ndef */
#define PH_NXPEXTNS_MIFARE_FORMAT_NDEF  0x04   /* MIFARE Format */
#define PH_NXPEXTNS_DISCONNECT          0x05   /* Target Disconnect */
#define PH_NXPEXTNS_ACTIVATED           0x06   /* Target Activated */
#define PH_NXPEXTNS_MIFARE_TRANSCEIVE   0x07   /* MIFARE Raw Transceive */
#define PH_NXPEXTNS_MIFARE_READ_ONLY    0x08   /* MIFARE Read Only */
#define PH_NXPEXTNS_MIFARE_PRESENCE_CHECK    0x09   /* MIFARE Presence Check */
#define PH_NXPEXTNS_RX_DATA             0xF1   /* Receive Data */

#endif /* _PHNXPEXTNS_H_ */
