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
 *  This file contains function of the NFC unit to receive/process NCI
 *  commands.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_target.h"
#include "bt_types.h"
#include "gki.h"

#if NFC_INCLUDED == TRUE
#include "nci_defs.h"
#include "nci_hmsgs.h"
#include "nfc_api.h"
#include "nfc_int.h"

/*******************************************************************************
**
** Function         nci_proc_core_rsp
**
** Description      Process NCI responses in the CORE group
**
** Returns          TRUE-caller of this function to free the GKI buffer p_msg
**
*******************************************************************************/
BOOLEAN nci_proc_core_rsp (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *pp, len, op_code;
    BOOLEAN free = TRUE;
    UINT8   *p_old = nfc_cb.last_cmd;

    /* find the start of the NCI message and parse the NCI header */
    p   = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    NFC_TRACE_DEBUG1 ("nci_proc_core_rsp opcode:0x%x", op_code);
    len = *pp++;

    /* process the message based on the opcode and message type */
    switch (op_code)
    {
    case NCI_MSG_CORE_RESET:
        nfc_ncif_proc_reset_rsp (pp, FALSE);
        break;

    case NCI_MSG_CORE_INIT:
        nfc_ncif_proc_init_rsp (p_msg);
        free = FALSE;
        break;

    case NCI_MSG_CORE_GET_CONFIG:
        nfc_ncif_proc_get_config_rsp (p_msg);
        break;

    case NCI_MSG_CORE_SET_CONFIG:
        nfc_ncif_set_config_status (pp, len);
        break;

    case NCI_MSG_CORE_CONN_CREATE:
        nfc_ncif_proc_conn_create_rsp (p, p_msg->len, *p_old);
        break;

    case NCI_MSG_CORE_CONN_CLOSE:
        nfc_ncif_report_conn_close_evt (*p_old, *pp);
        break;

    default:
        NFC_TRACE_ERROR1 ("unknown opcode:0x%x", op_code);
        break;
    }

    return free;
}

/*******************************************************************************
**
** Function         nci_proc_core_ntf
**
** Description      Process NCI notifications in the CORE group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_core_ntf (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *pp, len, op_code;
    UINT8   conn_id;

    /* find the start of the NCI message and parse the NCI header */
    p   = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    NFC_TRACE_DEBUG1 ("nci_proc_core_ntf opcode:0x%x", op_code);
    len = *pp++;

    /* process the message based on the opcode and message type */
    switch (op_code)
    {
    case NCI_MSG_CORE_RESET:
        nfc_ncif_proc_reset_rsp (pp, TRUE);
        break;

    case NCI_MSG_CORE_GEN_ERR_STATUS:
        /* process the error ntf */
        /* in case of timeout: notify the static connection callback */
        nfc_ncif_event_status (NFC_GEN_ERROR_REVT, *pp);
        nfc_ncif_error_status (NFC_RF_CONN_ID, *pp);
        break;

    case NCI_MSG_CORE_INTF_ERR_STATUS:
        conn_id = *(pp+1);
        nfc_ncif_error_status (conn_id, *pp);
        break;

    case NCI_MSG_CORE_CONN_CREDITS:
        nfc_ncif_proc_credits(pp, len);
        break;

    default:
        NFC_TRACE_ERROR1 ("unknown opcode:0x%x", op_code);
        break;
    }
}


/*******************************************************************************
**
** Function         nci_proc_rf_management_rsp
**
** Description      Process NCI responses in the RF Management group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_rf_management_rsp (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *pp, len, op_code;
    UINT8   *p_old = nfc_cb.last_cmd;

    /* find the start of the NCI message and parse the NCI header */
    p   = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    len = *pp++;

    switch (op_code)
    {
    case NCI_MSG_RF_DISCOVER:
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        nfa_dm_p2p_prio_logic(op_code, pp, 1);
#endif
        nfc_ncif_rf_management_status (NFC_START_DEVT, *pp);
        break;

    case NCI_MSG_RF_DISCOVER_SELECT:
        nfc_ncif_rf_management_status (NFC_SELECT_DEVT, *pp);
        break;

    case NCI_MSG_RF_T3T_POLLING:
        break;

    case NCI_MSG_RF_DISCOVER_MAP:
        nfc_ncif_rf_management_status (NFC_MAP_DEVT, *pp);
        break;

    case NCI_MSG_RF_DEACTIVATE:
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if (FALSE == nfa_dm_p2p_prio_logic(op_code, pp, 1))
        {
            NFC_TRACE_ERROR0("Dont process the Response");
            return;
        }
#endif
        nfc_ncif_proc_deactivate (*pp, *p_old, FALSE);
        break;

#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)

    case NCI_MSG_RF_SET_ROUTING:
        nfc_ncif_event_status (NFC_SET_ROUTING_REVT, *pp);
        break;

    case NCI_MSG_RF_GET_ROUTING:
        if (*pp != NFC_STATUS_OK)
            nfc_ncif_event_status (NFC_GET_ROUTING_REVT, *pp);
        break;
#endif
#endif

    case NCI_MSG_RF_PARAMETER_UPDATE:
        nfc_ncif_event_status (NFC_RF_COMM_PARAMS_UPDATE_REVT, *pp);
        break;

    default:
        NFC_TRACE_ERROR1 ("unknown opcode:0x%x", op_code);
        break;
    }
}

/*******************************************************************************
**
** Function         nci_proc_rf_management_ntf
**
** Description      Process NCI notifications in the RF Management group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_rf_management_ntf (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *pp, len, op_code;

    /* find the start of the NCI message and parse the NCI header */
    p   = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    len = *pp++;

    switch (op_code)
    {
    case NCI_MSG_RF_DISCOVER :
        nfc_ncif_proc_discover_ntf (p, p_msg->len);
        break;

    case NCI_MSG_RF_DEACTIVATE:
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if (FALSE == nfa_dm_p2p_prio_logic(op_code, pp, 2))
        {
            NFC_TRACE_ERROR0("Dont process the Event");
            return;
        }
#endif
        nfc_ncif_proc_deactivate (NFC_STATUS_OK, *pp, TRUE);
        break;

    case NCI_MSG_RF_INTF_ACTIVATED:
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if (FALSE == nfa_dm_p2p_prio_logic(op_code, pp, 2))
        {
            NFC_TRACE_ERROR0("Dont process the Event");
            return;
        }
#endif
        nfc_ncif_proc_activate (pp, len);
        break;

    case NCI_MSG_RF_FIELD:
        nfc_ncif_proc_rf_field_ntf (*pp);
        break;

    case NCI_MSG_RF_T3T_POLLING:
        nfc_ncif_proc_t3t_polling_ntf (pp, len);
        break;

#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)

    case NCI_MSG_RF_GET_ROUTING:
        nfc_ncif_proc_get_routing (pp, len);
        break;

    case NCI_MSG_RF_EE_ACTION:
        nfc_ncif_proc_ee_action (pp, len);
        break;

    case NCI_MSG_RF_EE_DISCOVERY_REQ:
        nfc_ncif_proc_ee_discover_req (pp, len);
        break;
#endif
#endif

    default:
        NFC_TRACE_ERROR1 ("unknown opcode:0x%x", op_code);
        break;
    }
}

#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)

/*******************************************************************************
**
** Function         nci_proc_ee_management_rsp
**
** Description      Process NCI responses in the NFCEE Management group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_ee_management_rsp (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *pp, len, op_code;
    tNFC_RESPONSE_CBACK *p_cback = nfc_cb.p_resp_cback;
    tNFC_NFCEE_DISCOVER_REVT    nfcee_discover;
    tNFC_NFCEE_INFO_REVT        nfcee_info;
    tNFC_NFCEE_MODE_SET_REVT    mode_set;
    tNFC_RESPONSE   *p_evt = (tNFC_RESPONSE *) &nfcee_info;
    tNFC_RESPONSE_EVT event = NFC_NFCEE_INFO_REVT;
    UINT8   *p_old = nfc_cb.last_cmd;

    /* find the start of the NCI message and parse the NCI header */
    p   = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    NFC_TRACE_DEBUG1 ("nci_proc_ee_management_rsp opcode:0x%x", op_code);
    len = *pp++;

    switch (op_code)
    {
    case NCI_MSG_NFCEE_DISCOVER:
        p_evt                       = (tNFC_RESPONSE *) &nfcee_discover;
        nfcee_discover.status       = *pp++;
        nfcee_discover.num_nfcee    = *pp++;

        if (nfcee_discover.status != NFC_STATUS_OK)
            nfcee_discover.num_nfcee    = 0;

        event                       = NFC_NFCEE_DISCOVER_REVT;
        break;

    case NCI_MSG_NFCEE_MODE_SET:
        p_evt                   = (tNFC_RESPONSE *) &mode_set;
        mode_set.status         = *pp;
        mode_set.nfcee_id       = 0;
        event                   = NFC_NFCEE_MODE_SET_REVT;
        mode_set.nfcee_id       = *p_old++;
        mode_set.mode           = *p_old++;
        break;

    default:
        p_cback = NULL;
        NFC_TRACE_ERROR1 ("unknown opcode:0x%x", op_code);
        break;
    }

    if (p_cback)
        (*p_cback) (event, p_evt);
}

/*******************************************************************************
**
** Function         nci_proc_ee_management_ntf
**
** Description      Process NCI notifications in the NFCEE Management group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_ee_management_ntf (BT_HDR *p_msg)
{
    UINT8                 *p;
    UINT8                 *pp, len, op_code;
    tNFC_RESPONSE_CBACK   *p_cback = nfc_cb.p_resp_cback;
    tNFC_NFCEE_INFO_REVT  nfcee_info;
    tNFC_RESPONSE         *p_evt   = (tNFC_RESPONSE *) &nfcee_info;
    tNFC_RESPONSE_EVT     event    = NFC_NFCEE_INFO_REVT;
    UINT8                 xx;
    UINT8                 yy;
    UINT8                 ee_status;
    tNFC_NFCEE_TLV        *p_tlv;

    /* find the start of the NCI message and parse the NCI header */
    p   = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    NFC_TRACE_DEBUG1 ("nci_proc_ee_management_ntf opcode:0x%x", op_code);
    len = *pp++;

    if (op_code == NCI_MSG_NFCEE_DISCOVER)
    {
        nfcee_info.nfcee_id    = *pp++;
        ee_status                   = *pp++;

        nfcee_info.ee_status        = ee_status;
        yy                          = *pp;
        nfcee_info.num_interface    = *pp++;
        p                           = pp;

        if (nfcee_info.num_interface > NFC_MAX_EE_INTERFACE)
            nfcee_info.num_interface = NFC_MAX_EE_INTERFACE;

        for (xx = 0; xx < nfcee_info.num_interface; xx++)
        {
            nfcee_info.ee_interface[xx] = *pp++;
        }

        pp                              = p + yy;
        nfcee_info.num_tlvs             = *pp++;
        NFC_TRACE_DEBUG4 ("nfcee_id: 0x%x num_interface:0x%x/0x%x, num_tlvs:0x%x",
            nfcee_info.nfcee_id, nfcee_info.num_interface, yy, nfcee_info.num_tlvs);

        if (nfcee_info.num_tlvs > NFC_MAX_EE_TLVS)
            nfcee_info.num_tlvs = NFC_MAX_EE_TLVS;

        p_tlv = &nfcee_info.ee_tlv[0];

        for (xx = 0; xx < nfcee_info.num_tlvs; xx++, p_tlv++)
        {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            if(*pp < 0xA0)
            {
                p_tlv->tag = *pp++;
            }
            else
            {
                p_tlv->tag  = *pp++;
                p_tlv->tag  = (p_tlv->tag << 8) | *pp++;
            }
#else
            p_tlv->tag  = *pp++;
#endif
            p_tlv->len  = yy = *pp++;
            NFC_TRACE_DEBUG2 ("tag:0x%x, len:0x%x", p_tlv->tag, p_tlv->len);
            if (p_tlv->len > NFC_MAX_EE_INFO)
                p_tlv->len = NFC_MAX_EE_INFO;
            p   = pp;
            STREAM_TO_ARRAY (p_tlv->info, pp, p_tlv->len);
            pp  = p += yy;
        }
    }
    else
    {
        p_cback = NULL;
        NFC_TRACE_ERROR1 ("unknown opcode:0x%x", op_code);
    }

    if (p_cback)
        (*p_cback) (event, p_evt);
}

#endif
#endif

/*******************************************************************************
**
** Function         nci_proc_prop_rsp
**
** Description      Process NCI responses in the Proprietary group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_prop_rsp (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *p_evt;
    UINT8   *pp, len, op_code;
    tNFC_VS_CBACK   *p_cback = (tNFC_VS_CBACK *)nfc_cb.p_vsc_cback;

    /* find the start of the NCI message and parse the NCI header */
    p   = p_evt = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    len = *pp++;

    /*If there's a pending/stored command, restore the associated address of the callback function */
    if (p_cback)
        (*p_cback) ((tNFC_VS_EVT) (NCI_RSP_BIT|op_code), p_msg->len, p_evt);
}

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         nci_proc_prop_nxp_rsp
**
** Description      Process NXP NCI responses
**
** Returns          void
**
*******************************************************************************/
void nci_proc_prop_nxp_rsp (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *p_evt;
    UINT8   *pp, len, op_code;
    tNFC_VS_CBACK   *p_cback = (tNFC_VS_CBACK *)nfc_cb.p_vsc_cback;

    /* find the start of the NCI message and parse the NCI header */
    p   = p_evt = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    len = *pp++;

    /*If there's a pending/stored command, restore the associated address of the callback function */
    if (p_cback)
    {
        (*p_cback) ((tNFC_VS_EVT) (NCI_RSP_BIT|op_code), p_msg->len, p_evt);
        nfc_cb.p_vsc_cback = NULL;
    }
    nfc_ncif_update_window ();
}
#endif

/*******************************************************************************
**
** Function         nci_proc_prop_ntf
**
** Description      Process NCI notifications in the Proprietary group
**
** Returns          void
**
*******************************************************************************/
void nci_proc_prop_ntf (BT_HDR *p_msg)
{
    UINT8   *p;
    UINT8   *p_evt;
    UINT8   *pp, len, op_code;
    int i;

    /* find the start of the NCI message and parse the NCI header */
    p   = p_evt = (UINT8 *) (p_msg + 1) + p_msg->offset;
    pp  = p+1;
    NCI_MSG_PRS_HDR1 (pp, op_code);
    len = *pp++;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    NFC_TRACE_DEBUG1 ("nci_proc_prop_ntf:op_code =0x%x", op_code);
    switch(op_code)
    {
    case NCI_MSG_RF_WTX:
        nfc_ncif_proc_rf_wtx_ntf (p, p_msg->len);
        return;
    default:
        break;
    }
#endif

    for (i = 0; i < NFC_NUM_VS_CBACKS; i++)
    {
        if (nfc_cb.p_vs_cb[i])
        {
            (*nfc_cb.p_vs_cb[i]) ((tNFC_VS_EVT) (NCI_NTF_BIT|op_code), p_msg->len, p_evt);
        }
    }
}

#endif /* NFC_INCLUDED == TRUE*/
