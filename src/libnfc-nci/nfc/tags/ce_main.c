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
 *  This file contains functions that interface with the NFC NCI transport.
 *  On the receive side, it routes events to the appropriate handler
 *  (callback). On the transmit side, it manages the command transmission.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_target.h"
#include "bt_types.h"

#if (NFC_INCLUDED == TRUE)
#include "nfc_api.h"
#include "nci_hmsgs.h"
#include "ce_api.h"
#include "ce_int.h"
#include "gki.h"

tCE_CB  ce_cb;

/*******************************************************************************
*******************************************************************************/
void ce_init (void)
{
    memset (&ce_cb, 0, sizeof (tCE_CB));
    ce_cb.trace_level = NFC_INITIAL_TRACE_LEVEL;

    /* Initialize tag-specific fields of ce control block */
    ce_t3t_init ();
}

/*******************************************************************************
**
** Function         CE_SendRawFrame
**
** Description      This function sends a raw frame to the peer device.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS CE_SendRawFrame (UINT8 *p_raw_data, UINT16 data_len)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;
    BT_HDR  *p_data;
    UINT8   *p;

    if (ce_cb.p_cback)
    {
        /* a valid opcode for RW */
        p_data = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);
        if (p_data)
        {
            p_data->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
            p = (UINT8 *) (p_data + 1) + p_data->offset;
            memcpy (p, p_raw_data, data_len);
            p_data->len = data_len;
            CE_TRACE_EVENT1 ("CE SENT raw frame (0x%x)", data_len);
            status = NFC_SendData (NFC_RF_CONN_ID, p_data);
        }

    }
    return status;
}

/*******************************************************************************
**
** Function         CE_SetActivatedTagType
**
** Description      This function selects the tag type for CE mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS CE_SetActivatedTagType (tNFC_ACTIVATE_DEVT *p_activate_params, UINT16 t3t_system_code, tCE_CBACK *p_cback)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;
    tNFC_PROTOCOL protocol = p_activate_params->protocol;

    CE_TRACE_API1 ("CE_SetActivatedTagType protocol:%d", protocol);

    switch (protocol)
    {
    case NFC_PROTOCOL_T1T:
    case NFC_PROTOCOL_T2T:
        return NFC_STATUS_FAILED;

    case NFC_PROTOCOL_T3T:   /* Type3Tag    - NFC-F */
        /* store callback function before NFC_SetStaticRfCback () */
        ce_cb.p_cback  = p_cback;
        status = ce_select_t3t (t3t_system_code, p_activate_params->rf_tech_param.param.lf.nfcid2);
        break;

    case NFC_PROTOCOL_ISO_DEP:     /* ISODEP/4A,4B- NFC-A or NFC-B */
        /* store callback function before NFC_SetStaticRfCback () */
        ce_cb.p_cback  = p_cback;
        status = ce_select_t4t ();
        break;

    default:
        CE_TRACE_ERROR0 ("CE_SetActivatedTagType Invalid protocol");
        return NFC_STATUS_FAILED;
    }

    if (status != NFC_STATUS_OK)
    {
        NFC_SetStaticRfCback (NULL);
        ce_cb.p_cback  = NULL;
    }
    return status;
}

/*******************************************************************************
**
** Function         CE_SetTraceLevel
**
** Description      This function sets the trace level for Card Emulation mode.
**                  If called with a value of 0xFF,
**                  it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 CE_SetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        ce_cb.trace_level = new_level;

    return (ce_cb.trace_level);
}

#endif /* NFC_INCLUDED == TRUE */
