/******************************************************************************
 *
 *  Copyright (C) 2011-2014 Broadcom Corporation
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
 *  NFA interface for card emulation
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_api.h"
#include "nfa_sys.h"
#include "nfa_ce_int.h"
#include "nfa_sys_int.h"


/*******************************************************************************
**
** Function         nfa_ce_api_deregister_listen
**
** Description      Internal function called by listening for Felica system
**                  code, ISO-DEP AID, or UICC technology
**
** Returns:
**                  NFA_STATUS_OK,            if command accepted
**                  NFA_STATUS_BAD_HANDLE     invalid handle
**                  NFA_STATUS_FAILED:        otherwise
**
*******************************************************************************/
tNFA_STATUS nfa_ce_api_deregister_listen (tNFA_HANDLE handle, UINT32 listen_info)
{
    tNFA_CE_MSG      *p_ce_msg;

    /* Validate handle */
    if (  (listen_info != NFA_CE_LISTEN_INFO_UICC
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            && listen_info != NFA_CE_LISTEN_INFO_ESE
#endif
            )
        &&((handle & NFA_HANDLE_GROUP_MASK) != NFA_HANDLE_GROUP_CE)  )
    {
        NFA_TRACE_ERROR0 ("nfa_ce_api_reregister_listen: Invalid handle");
        return (NFA_STATUS_BAD_HANDLE);
    }

    if ((p_ce_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) (sizeof (tNFA_CE_MSG)))) != NULL)
    {
        p_ce_msg->hdr.event                     = NFA_CE_API_DEREG_LISTEN_EVT;
        p_ce_msg->dereg_listen.handle           = handle;
        p_ce_msg->dereg_listen.listen_info      = listen_info;

        nfa_sys_sendmsg (p_ce_msg);

        return (NFA_STATUS_OK);
    }
    else
    {
        NFA_TRACE_ERROR0 ("nfa_ce_api_reregister_listen: Out of buffers");
        return (NFA_STATUS_FAILED);
    }
}

/*****************************************************************************
**  APIs
*****************************************************************************/

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
**                  as long as the local NDEF tag is enabled.
**
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function. Also, Input parameters p_uid and
**                  uid_len are reserved for future use.
**
** Returns:
**                  NFA_STATUS_OK,            if command accepted
**                  NFA_STATUS_INVALID_PARAM,
**                      if protocol_maks is not 0 and p_ndef_data is NULL
**                  (or)uid_len is not 0
**                  (or)if protocol mask is set for Type 1 or Type 2
**
**                  NFA_STATUS_FAILED:        otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_CeConfigureLocalTag (tNFA_PROTOCOL_MASK protocol_mask,
                                     UINT8     *p_ndef_data,
                                     UINT16    ndef_cur_size,
                                     UINT16    ndef_max_size,
                                     BOOLEAN   read_only,
                                     UINT8     uid_len,
                                     UINT8     *p_uid)

{
    tNFA_CE_MSG *p_msg;

    NFA_TRACE_API0 ("NFA_CeConfigureLocalTag ()");

    if (protocol_mask)
    {
        /* If any protocols are specified, then NDEF buffer pointer must be non-NULL */
        if (p_ndef_data == NULL)
        {
            NFA_TRACE_ERROR0 ("NFA_CeConfigureLocalTag: NULL ndef data pointer");
            return (NFA_STATUS_INVALID_PARAM);
        }

        if ((protocol_mask & NFA_PROTOCOL_MASK_T1T) || (protocol_mask & NFA_PROTOCOL_MASK_T2T))
        {
            NFA_TRACE_ERROR0 ("NFA_CeConfigureLocalTag: Cannot emulate Type 1 / Type 2 tag");
            return (NFA_STATUS_INVALID_PARAM);
        }

        if (uid_len)
        {
            NFA_TRACE_ERROR1 ("NFA_CeConfigureLocalTag: Cannot Set UID for Protocol_mask: 0x%x", protocol_mask);
            return (NFA_STATUS_INVALID_PARAM);
        }
    }
    if ((p_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) sizeof(tNFA_CE_MSG))) != NULL)
    {
        p_msg->local_tag.hdr.event = NFA_CE_API_CFG_LOCAL_TAG_EVT;

        /* Copy ndef info */
        p_msg->local_tag.protocol_mask  = protocol_mask;
        p_msg->local_tag.p_ndef_data    = p_ndef_data;
        p_msg->local_tag.ndef_cur_size  = ndef_cur_size;
        p_msg->local_tag.ndef_max_size  = ndef_max_size;
        p_msg->local_tag.read_only      = read_only;
        p_msg->local_tag.uid_len        = uid_len;

        if (uid_len)
            memcpy (p_msg->local_tag.uid, p_uid, uid_len);

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}


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
tNFA_STATUS NFA_CeConfigureUiccListenTech (tNFA_HANDLE ee_handle,
                                           tNFA_TECHNOLOGY_MASK tech_mask)
{
#if (NFC_NFCEE_INCLUDED == TRUE)
    tNFA_CE_MSG *p_msg;

    NFA_TRACE_API1 ("NFA_CeConfigureUiccListenTech () ee_handle = 0x%x", ee_handle);

    /* If tech_mask is zero, then app is disabling listening for specified uicc */
    if (tech_mask == 0)
    {
        return (nfa_ce_api_deregister_listen (ee_handle, NFA_CE_LISTEN_INFO_UICC));
    }

    /* Otherwise then app is configuring uicc listen for the specificed technologies */
    if ((p_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) sizeof(tNFA_CE_MSG))) != NULL)
    {
        p_msg->reg_listen.hdr.event   = NFA_CE_API_REG_LISTEN_EVT;
        p_msg->reg_listen.listen_type = NFA_CE_REG_TYPE_UICC;

        p_msg->reg_listen.ee_handle   = ee_handle;
        p_msg->reg_listen.tech_mask   = tech_mask;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }
#else
    NFA_TRACE_ERROR0 ("NFA_CeConfigureUiccListenTech () NFCEE related functions are not enabled!");
#endif
    return (NFA_STATUS_FAILED);
}

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
tNFA_STATUS NFA_CeConfigureEseListenTech (tNFA_HANDLE ee_handle,
                                           tNFA_TECHNOLOGY_MASK tech_mask)
{
#if (NFC_NFCEE_INCLUDED == TRUE)
    tNFA_CE_MSG *p_msg;

    NFA_TRACE_API1 ("NFA_CeConfigureEseListenTech () ee_handle = 0x%x", ee_handle);

    /* If tech_mask is zero, then app is disabling listening for specified uicc */
    if (tech_mask == 0)
    {
        return (nfa_ce_api_deregister_listen (ee_handle, NFA_CE_LISTEN_INFO_ESE));
    }

    /* Otherwise then app is configuring ese listen for the specificed technologies */
    if ((p_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) sizeof(tNFA_CE_MSG))) != NULL)
    {
        p_msg->reg_listen.hdr.event   = NFA_CE_API_REG_LISTEN_EVT;
        p_msg->reg_listen.listen_type = NFA_CE_REG_TYPE_ESE;

        p_msg->reg_listen.ee_handle   = ee_handle;
        p_msg->reg_listen.tech_mask   = tech_mask;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }
#else
    NFA_TRACE_ERROR0 ("NFA_CeConfigureEseListenTech () NFCEE related functions are not enabled!");
#endif
    return (NFA_STATUS_FAILED);
}
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
tNFA_STATUS NFA_CeRegisterFelicaSystemCodeOnDH (UINT16 system_code,
                                                UINT8 nfcid2[NCI_RF_F_UID_LEN],
                                                tNFA_CONN_CBACK *p_conn_cback)
{
    tNFA_CE_MSG *p_msg;

    NFA_TRACE_API0 ("NFA_CeRegisterFelicaSystemCodeOnDH ()");

    /* Validate parameters */
    if (p_conn_cback==NULL)
        return (NFA_STATUS_INVALID_PARAM);

    if ((p_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) sizeof(tNFA_CE_MSG))) != NULL)
    {
        p_msg->reg_listen.hdr.event = NFA_CE_API_REG_LISTEN_EVT;
        p_msg->reg_listen.p_conn_cback = p_conn_cback;
        p_msg->reg_listen.listen_type = NFA_CE_REG_TYPE_FELICA;

        /* Listen info */
        memcpy (p_msg->reg_listen.nfcid2, nfcid2, NCI_RF_F_UID_LEN);
        p_msg->reg_listen.system_code = system_code;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

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
tNFA_STATUS NFA_CeDeregisterFelicaSystemCodeOnDH (tNFA_HANDLE handle)
{
    NFA_TRACE_API1 ("NFA_CeDeregisterFelicaSystemCodeOnDH (): handle:0x%X", handle);
    return (nfa_ce_api_deregister_listen (handle, NFA_CE_LISTEN_INFO_FELICA));
}

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
tNFA_STATUS NFA_CeRegisterAidOnDH (UINT8 aid[NFC_MAX_AID_LEN],
                                         UINT8           aid_len,
                                         tNFA_CONN_CBACK *p_conn_cback)
{
    tNFA_CE_MSG *p_msg;

    NFA_TRACE_API0 ("NFA_CeRegisterAidOnDH ()");

    /* Validate parameters */
    if (p_conn_cback==NULL)
        return (NFA_STATUS_INVALID_PARAM);

    if ((p_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) sizeof(tNFA_CE_MSG))) != NULL)
    {
        p_msg->reg_listen.hdr.event = NFA_CE_API_REG_LISTEN_EVT;
        p_msg->reg_listen.p_conn_cback = p_conn_cback;
        p_msg->reg_listen.listen_type = NFA_CE_REG_TYPE_ISO_DEP;

        /* Listen info */
        memcpy (p_msg->reg_listen.aid, aid, aid_len);
        p_msg->reg_listen.aid_len = aid_len;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

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
tNFA_STATUS NFA_CeDeregisterAidOnDH (tNFA_HANDLE handle)
{
    NFA_TRACE_API1 ("NFA_CeDeregisterAidOnDH (): handle:0x%X", handle);
    return (nfa_ce_api_deregister_listen (handle, NFA_CE_LISTEN_INFO_T4T_AID));
}

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
tNFA_STATUS NFA_CeSetIsoDepListenTech (tNFA_TECHNOLOGY_MASK tech_mask)
{
    tNFA_CE_MSG *p_msg;
    tNFA_TECHNOLOGY_MASK    use_mask = (NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B);

    NFA_TRACE_API1 ("NFA_CeSetIsoDepListenTech (): 0x%x", tech_mask);
    if (((tech_mask & use_mask) == 0) ||
        ((tech_mask & ~use_mask) != 0) )
    {
        NFA_TRACE_ERROR0 ("NFA_CeSetIsoDepListenTech: Invalid technology mask");
        return (NFA_STATUS_INVALID_PARAM);
    }

    if ((p_msg = (tNFA_CE_MSG *) GKI_getbuf ((UINT16) sizeof(tNFA_CE_MSG))) != NULL)
    {
        p_msg->hdr.event            = NFA_CE_API_CFG_ISODEP_TECH_EVT;
        p_msg->hdr.layer_specific   = tech_mask & use_mask;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

