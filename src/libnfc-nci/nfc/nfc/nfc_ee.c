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
 *  This file contains functions that interface with the NFCEEs.
 *
 ******************************************************************************/
#include <string.h>
#include "gki.h"
#include "nfc_target.h"
#include "bt_types.h"

#if (NFC_INCLUDED == TRUE)
#include "nfc_api.h"
#include "nfc_int.h"
#include "nci_hmsgs.h"


/*******************************************************************************
**
** Function         NFC_NfceeDiscover
**
** Description      This function is called to enable or disable NFCEE Discovery.
**                  The response from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_NFCEE_DISCOVER_REVT.
**                  The notification from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_NFCEE_INFO_REVT.
**
** Parameters       discover - 1 to enable discover, 0 to disable.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_NfceeDiscover (BOOLEAN discover)
{
    return nci_snd_nfcee_discover ((UINT8) (discover ? NCI_DISCOVER_ACTION_ENABLE : NCI_DISCOVER_ACTION_DISABLE));
}

/*******************************************************************************
**
** Function         NFC_NfceeModeSet
**
** Description      This function is called to activate or de-activate an NFCEE
**                  connected to the NFCC.
**                  The response from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_NFCEE_MODE_SET_REVT.
**
** Parameters       nfcee_id - the NFCEE to activate or de-activate.
**                  mode - NFC_MODE_ACTIVATE to activate NFCEE,
**                         NFC_MODE_DEACTIVATE to de-activate.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_NfceeModeSet (UINT8              nfcee_id,
                              tNFC_NFCEE_MODE    mode)
{
    if (mode >= NCI_NUM_NFCEE_MODE)
    {
        NFC_TRACE_ERROR1 ("NFC_NfceeModeSet bad mode:%d", mode);
        return NFC_STATUS_FAILED;
    }

    return nci_snd_nfcee_mode_set (nfcee_id, mode);
}




/*******************************************************************************
**
** Function         NFC_SetRouting
**
** Description      This function is called to configure the CE routing table.
**                  The response from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_SET_ROUTING_REVT.
**
** Parameters
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_SetRouting (BOOLEAN more,
                             UINT8  num_tlv,
                             UINT8  tlv_size,
                             UINT8  *p_param_tlvs)
{
    return nci_snd_set_routing_cmd (more, num_tlv, tlv_size, p_param_tlvs);
}

/*******************************************************************************
**
** Function         NFC_GetRouting
**
** Description      This function is called to retrieve the CE routing table from
**                  NFCC. The response from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_GET_ROUTING_REVT.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_GetRouting (void)
{
    return nci_snd_get_routing_cmd ();
}


#endif /* NFC_INCLUDED == TRUE */
