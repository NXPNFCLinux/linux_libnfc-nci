/*
 * Copyright (C) 2012 The Android Open Source Project
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
#pragma once

/* Discovery modes -- keep in sync with NFCManager.DISCOVERY_MODE_* */
#define DISCOVERY_MODE_TAG_READER         0
#define DISCOVERY_MODE_NFCIP1             1
#define DISCOVERY_MODE_CARD_EMULATION     2
#define DISCOVERY_MODE_TABLE_SIZE         3

#define DISCOVERY_MODE_DISABLED           0
#define DISCOVERY_MODE_ENABLED            1

#define MODE_P2P_TARGET                   0
#define MODE_P2P_INITIATOR                1


/* Properties values */
#define PROPERTY_LLCP_LTO                 0
#define PROPERTY_LLCP_MIU                 1
#define PROPERTY_LLCP_WKS                 2
#define PROPERTY_LLCP_OPT                 3
#define PROPERTY_NFC_DISCOVERY_A          4
#define PROPERTY_NFC_DISCOVERY_B          5
#define PROPERTY_NFC_DISCOVERY_F          6
#define PROPERTY_NFC_DISCOVERY_15693      7
#define PROPERTY_NFC_DISCOVERY_NCFIP      8


/* Error codes */
#define ERROR_BUFFER_TOO_SMALL            -12
#define ERROR_INSUFFICIENT_RESOURCES      -9


/* Pre-defined tag type values. These must match the values in
 * Ndef.java in the framework.
 */
#define NDEF_UNKNOWN_TYPE                -1
#define NDEF_TYPE1_TAG                   1
#define NDEF_TYPE2_TAG                   2
#define NDEF_TYPE3_TAG                   3
#define NDEF_TYPE4_TAG                   4
#define NDEF_MIFARE_CLASSIC_TAG          101


/* Pre-defined card read/write state values. These must match the values in
 * Ndef.java in the framework.
 */
#define NDEF_MODE_READ_ONLY              1
#define NDEF_MODE_READ_WRITE             2
#define NDEF_MODE_UNKNOWN                3

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define VEN_POWER_STATE_ON                   6
#define VEN_POWER_STATE_OFF                  7
#endif

/* Setting VEN_CFG  */
#define VEN_CFG_NFC_ON_POWER_ON           3
#define VEN_CFG_NFC_OFF_POWER_OFF         2


//define a few NXP error codes that NFC service expects;
//see external/libnfc-nxp/src/phLibNfcStatus.h;
//see external/libnfc-nxp/inc/phNfcStatus.h
#define NFCSTATUS_SUCCESS (0x0000)
#define NFCSTATUS_FAILED (0x00FF)

//default general trasceive timeout in millisecond
#define DEFAULT_GENERAL_TRANS_TIMEOUT  2000

