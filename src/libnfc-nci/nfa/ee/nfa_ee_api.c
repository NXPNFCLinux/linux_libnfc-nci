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
 *  NFA interface to NFCEE - API functions
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_ee_api.h"
#include "nfa_ee_int.h"
#include "nfa_sys_int.h"
#include "nfa_dm_int.h"


/*****************************************************************************
**  APIs
*****************************************************************************/
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
tNFA_STATUS NFA_EeDiscover(tNFA_EE_CBACK *p_cback)
{
    tNFA_EE_API_DISCOVER *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;

    NFA_TRACE_API0 ("NFA_EeDiscover()");

    if (nfa_ee_cb.em_state != NFA_EE_EM_STATE_INIT_DONE)
    {
        NFA_TRACE_ERROR1 ("NFA_EeDiscover bad em state: %d", nfa_ee_cb.em_state);
        status = NFA_STATUS_FAILED;
    }
    else if ((nfa_ee_cb.p_ee_disc_cback != NULL) || (p_cback == NULL))
    {
        NFA_TRACE_ERROR0 ("NFA_EeDiscover() in progress or NULL callback function");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_DISCOVER *) GKI_getbuf (sizeof(tNFA_EE_API_DISCOVER))) != NULL)
    {
        p_msg->hdr.event    = NFA_EE_API_DISCOVER_EVT;
        p_msg->p_cback      = p_cback;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeGetInfo(UINT8        *p_num_nfcee,
                          tNFA_EE_INFO *p_info)
{
    int   xx, ret = nfa_ee_cb.cur_ee;
    tNFA_EE_ECB  *p_cb = nfa_ee_cb.ecb;
    UINT8   max_ret;
    UINT8   num_ret  = 0;

    NFA_TRACE_DEBUG2 ("NFA_EeGetInfo em_state:%d cur_ee:%d", nfa_ee_cb.em_state, nfa_ee_cb.cur_ee);
    /* validate parameters */
    if (p_info == NULL || p_num_nfcee == NULL)
    {
        NFA_TRACE_ERROR0 ("NFA_EeGetInfo bad parameter");
        return (NFA_STATUS_INVALID_PARAM);
    }
    max_ret         = *p_num_nfcee;
    *p_num_nfcee = 0;
    if (nfa_ee_cb.em_state == NFA_EE_EM_STATE_INIT)
    {
        NFA_TRACE_ERROR1 ("NFA_EeGetInfo bad em state: %d", nfa_ee_cb.em_state);
        return (NFA_STATUS_FAILED);
    }

    /* compose output */
    for (xx = 0; (xx < ret) && (num_ret < max_ret); xx++, p_cb++)
    {
        NFA_TRACE_DEBUG4 ("xx:%d max_ret:%d, num_ret:%d ee_status:0x%x", xx, max_ret, num_ret, p_cb->ee_status);
        if ((p_cb->ee_status & NFA_EE_STATUS_INT_MASK) || (p_cb->ee_status == NFA_EE_STATUS_REMOVED))
        {
            continue;
        }
        p_info->ee_handle       = NFA_HANDLE_GROUP_EE | (tNFA_HANDLE)p_cb->nfcee_id;
        p_info->ee_status       = p_cb->ee_status;
        p_info->num_interface   = p_cb->num_interface;
        p_info->num_tlvs        = p_cb->num_tlvs;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        p_info->la_protocol =  p_cb->la_protocol;
        p_info->lb_protocol =  p_cb->lb_protocol;
        p_info->lf_protocol =  p_cb->lf_protocol;
        p_info->lbp_protocol =  p_cb->lbp_protocol;
#endif
        memcpy(p_info->ee_interface, p_cb->ee_interface, p_cb->num_interface);
        memcpy(p_info->ee_tlv, p_cb->ee_tlv, p_cb->num_tlvs * sizeof(tNFA_EE_TLV));
        p_info++;
        num_ret++;
    }
    NFA_TRACE_DEBUG1 ("num_ret:%d", num_ret);
    *p_num_nfcee = num_ret;
    return (NFA_STATUS_OK);
}

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         NFA_AllEeGetInfo
**
** Description      This function retrieves the NFCEE information from NFA.
**                  The actual number of NFCEE independent of their status
**                  is returned in p_num_nfcee and NFCEE information is returned
**                  in p_info
**
** Returns          NFA_STATUS_OK if information is retrieved successfully
**                  NFA_STATUS_FAILED If wrong state (retry later)
**                  NFA_STATUS_INVALID_PARAM If bad parameter
**
*******************************************************************************/
tNFA_STATUS NFA_AllEeGetInfo(UINT8        *p_num_nfcee,
                             tNFA_EE_INFO *p_info)
{
    int   xx, ret = nfa_ee_cb.cur_ee;
    tNFA_EE_ECB  *p_cb = nfa_ee_cb.ecb;
    UINT8   max_ret;
    UINT8   num_ret  = 0;

    NFA_TRACE_DEBUG2 ("NFA_AllEeGetInfo em_state:%d cur_ee:%d", nfa_ee_cb.em_state, nfa_ee_cb.cur_ee);
    /* validate parameters */
    if (p_info == NULL || p_num_nfcee == NULL)
    {
        NFA_TRACE_ERROR0 ("NFA_AllEeGetInfo bad parameter");
        return (NFA_STATUS_INVALID_PARAM);
    }
    max_ret         = *p_num_nfcee;
    *p_num_nfcee = 0;
    if (nfa_ee_cb.em_state == NFA_EE_EM_STATE_INIT)
    {
        NFA_TRACE_ERROR1 ("NFA_AllEeGetInfo bad em state: %d", nfa_ee_cb.em_state);
        return (NFA_STATUS_FAILED);
    }

    /* compose output */
    for (xx = 0; (xx < ret) && (num_ret < max_ret); xx++, p_cb++)
    {
        NFA_TRACE_DEBUG4 ("xx:%d max_ret:%d, num_ret:%d ee_status:0x%x", xx, max_ret, num_ret, p_cb->ee_status);
        if (p_cb->ee_status & NFA_EE_STATUS_INT_MASK)
        {
            continue;
        }
        p_info->ee_handle       = NFA_HANDLE_GROUP_EE | (tNFA_HANDLE)p_cb->nfcee_id;
        p_info->ee_status       = p_cb->ee_status;
        p_info->num_interface   = p_cb->num_interface;
        p_info->num_tlvs        = p_cb->num_tlvs;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        p_info->la_protocol =  p_cb->la_protocol;
        p_info->lb_protocol =  p_cb->lb_protocol;
        p_info->lf_protocol =  p_cb->lf_protocol;
        p_info->lbp_protocol =  p_cb->lbp_protocol;
#endif
        memcpy(p_info->ee_interface, p_cb->ee_interface, p_cb->num_interface);
        memcpy(p_info->ee_tlv, p_cb->ee_tlv, p_cb->num_tlvs * sizeof(tNFA_EE_TLV));
        p_info++;
        num_ret++;
    }
    NFA_TRACE_DEBUG1 ("num_ret:%d", num_ret);
    *p_num_nfcee = num_ret;
    return (NFA_STATUS_OK);
}
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
tNFA_STATUS NFA_EeRegister(tNFA_EE_CBACK *p_cback)
{
    tNFA_EE_API_REGISTER *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;

    NFA_TRACE_API0 ("NFA_EeRegister()");

    if (p_cback == NULL)
    {
        NFA_TRACE_ERROR0 ("NFA_EeRegister(): with NULL callback function");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_REGISTER *) GKI_getbuf (sizeof(tNFA_EE_API_REGISTER))) != NULL)
    {
        p_msg->hdr.event    = NFA_EE_API_REGISTER_EVT;
        p_msg->p_cback      = p_cback;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeDeregister(tNFA_EE_CBACK *p_cback)
{
    tNFA_EE_API_DEREGISTER *p_msg;
    tNFA_STATUS status = NFA_STATUS_INVALID_PARAM;
    int         index  = NFA_EE_MAX_CBACKS;
    int         xx;

    for (xx = 0; xx < NFA_EE_MAX_CBACKS; xx++)
    {
        if (nfa_ee_cb.p_ee_cback[xx] == p_cback)
        {
            index   = xx;
            status  = NFA_STATUS_FAILED;
            break;
        }
    }

    NFA_TRACE_API2 ("NFA_EeDeregister() %d, status:%d", index, status);
    if ((status != NFA_STATUS_INVALID_PARAM) &&
        (p_msg = (tNFA_EE_API_DEREGISTER *) GKI_getbuf (sizeof(tNFA_EE_API_DEREGISTER))) != NULL)
    {
        p_msg->hdr.event = NFA_EE_API_DEREGISTER_EVT;
        p_msg->index     = index;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeModeSet(tNFA_HANDLE    ee_handle,
                          tNFA_EE_MD     mode)
{
    tNFA_EE_API_MODE_SET *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    tNFA_EE_ECB *p_cb, *p_found = NULL;
    UINT32  xx;
    UINT8   nfcee_id = (ee_handle & 0xFF);

    p_cb = nfa_ee_cb.ecb;
    for (xx = 0; xx < nfa_ee_cb.cur_ee; xx++, p_cb++)
    {
        if (nfcee_id == p_cb->nfcee_id)
        {
            p_found = p_cb;
            break;
        }
    }
    NFA_TRACE_API2 ("NFA_EeModeSet(): handle:<0x%x>, mode:0x%02X", ee_handle, mode);

    if (p_found == NULL)
    {
        NFA_TRACE_ERROR1 ("NFA_EeModeSet() invalid NFCEE:0x%04x", ee_handle);
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_MODE_SET *) GKI_getbuf (sizeof(tNFA_EE_API_MODE_SET))) != NULL)
    {
        p_msg->hdr.event        = NFA_EE_API_MODE_SET_EVT;
        p_msg->nfcee_id         = nfcee_id;
        p_msg->mode             = mode;
        p_msg->p_cb             = p_found;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }


    return status;
}


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
tNFA_STATUS NFA_EeSetDefaultTechRouting(tNFA_HANDLE          ee_handle,
                                        tNFA_TECHNOLOGY_MASK technologies_switch_on,
                                        tNFA_TECHNOLOGY_MASK technologies_switch_off,
                                        tNFA_TECHNOLOGY_MASK technologies_battery_off
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                                       ,tNFA_TECHNOLOGY_MASK technologies_screen_lock,
                                        tNFA_TECHNOLOGY_MASK technologies_screen_off
#endif
)
{
    tNFA_EE_API_SET_TECH_CFG *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;

    NFA_TRACE_API6 ("NFA_EeSetDefaultTechRouting(): handle:<0x%x>technology_mask:<0x%x>/<0x%x>/<0x%x><0x%x><0x%x>",
        ee_handle, technologies_switch_on, technologies_switch_off, technologies_battery_off, technologies_screen_lock, technologies_screen_off );
    p_cb = nfa_ee_find_ecb (nfcee_id);

    if (p_cb == NULL)
    {
        NFA_TRACE_ERROR0 ("Bad ee_handle");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_SET_TECH_CFG *) GKI_getbuf (sizeof(tNFA_EE_API_SET_TECH_CFG))) != NULL)
    {
        p_msg->hdr.event                = NFA_EE_API_SET_TECH_CFG_EVT;
        p_msg->nfcee_id                 = nfcee_id;
        p_msg->p_cb                     = p_cb;
        p_msg->technologies_switch_on   = technologies_switch_on;
        p_msg->technologies_switch_off  = technologies_switch_off;
        p_msg->technologies_battery_off = technologies_battery_off;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        p_msg->technologies_screen_lock = technologies_screen_lock;
        p_msg->technologies_screen_off  = technologies_screen_off;
#endif
        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeSetDefaultProtoRouting(tNFA_HANDLE         ee_handle,
                                         tNFA_PROTOCOL_MASK  protocols_switch_on,
                                         tNFA_PROTOCOL_MASK  protocols_switch_off,
                                         tNFA_PROTOCOL_MASK  protocols_battery_off
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                                        ,tNFA_PROTOCOL_MASK  protocols_screen_lock,
                                         tNFA_PROTOCOL_MASK  protocols_screen_off
#endif
)
{
    tNFA_EE_API_SET_PROTO_CFG *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;

    NFA_TRACE_API6 ("NFA_EeSetDefaultProtoRouting(): handle:<0x%x>protocol_mask:<0x%x>/<0x%x>/<0x%x><0x%x><0x%x>",
        ee_handle, protocols_switch_on, protocols_switch_off, protocols_battery_off, protocols_screen_lock, protocols_screen_off);
    p_cb = nfa_ee_find_ecb (nfcee_id);

    if (p_cb == NULL)
    {
        NFA_TRACE_ERROR0 ("Bad ee_handle");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_SET_PROTO_CFG *) GKI_getbuf (sizeof(tNFA_EE_API_SET_PROTO_CFG))) != NULL)
    {
        p_msg->hdr.event                = NFA_EE_API_SET_PROTO_CFG_EVT;
        p_msg->nfcee_id                 = nfcee_id;
        p_msg->p_cb                     = p_cb;
        p_msg->protocols_switch_on      = protocols_switch_on;
        p_msg->protocols_switch_off     = protocols_switch_off;
        p_msg->protocols_battery_off    = protocols_battery_off;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        p_msg->protocols_screen_lock     = protocols_screen_lock;
        p_msg->protocols_screen_off    = protocols_screen_off;
#endif
        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeAddAidRouting(tNFA_HANDLE          ee_handle,
                                UINT8                aid_len,
                                UINT8               *p_aid,
                                tNFA_EE_PWR_STATE    power_state
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                                , UINT8              vs_info)
#else
)
#endif
{
    tNFA_EE_API_ADD_AID *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT16 size = sizeof(tNFA_EE_API_ADD_AID) + aid_len;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;

    NFA_TRACE_API1 ("NFA_EeAddAidRouting(): handle:<0x%x>", ee_handle);
    p_cb = nfa_ee_find_ecb (nfcee_id);

    /* validate parameters - make sure the AID is in valid length range */
    if ((p_cb == NULL) || (aid_len == 0) || (p_aid == NULL) || (aid_len < NFA_MIN_AID_LEN) || (aid_len > NFA_MAX_AID_LEN))
    {
        NFA_TRACE_ERROR1 ("Bad ee_handle or AID (len=%d)", aid_len);
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_ADD_AID *) GKI_getbuf (size)) != NULL)
    {
        NFA_TRACE_DEBUG2 ("aid:<%02x%02x>", p_aid[0], p_aid[1]);
        p_msg->hdr.event        = NFA_EE_API_ADD_AID_EVT;
        p_msg->nfcee_id         = nfcee_id;
        p_msg->p_cb             = p_cb;
        p_msg->aid_len          = aid_len;
        p_msg->power_state      = power_state;
        p_msg->p_aid            = (UINT8 *)(p_msg + 1);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        p_msg->vs_info          = vs_info;
#endif
        memcpy(p_msg->p_aid, p_aid, aid_len);

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_AddEePowerState(tNFA_HANDLE          ee_handle,
                                tNFA_EE_PWR_STATE    power_state_mask)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;
    UINT8 xx  = 0 ;

    NFA_TRACE_API1 ("NFA_AddEePowerState(): handle:<0x%x>", ee_handle);
    p_cb = nfa_ee_find_ecb (nfcee_id);

    /* validate parameters */
    if ((p_cb == NULL))
    {
        status = NFA_STATUS_INVALID_PARAM;
        return status;
    }

    if((power_state_mask & NFA_EE_PWR_STATE_SWITCH_OFF) != 0x00)
    {
        for(xx=0; xx < p_cb->aid_entries; xx++)
        {
            p_cb->aid_pwr_cfg[xx] |= power_state_mask;
            p_cb->ecb_flags |= NFA_EE_ECB_FLAGS_AID;
        }

        /* For tech and proto only enable power off mode*/
        p_cb->proto_switch_off |= p_cb->proto_switch_on;
        p_cb->tech_switch_off |= p_cb->tech_switch_on;
    }
    else
    {
        for(xx=0; xx < p_cb->aid_entries; xx++)
        {
            p_cb->aid_pwr_cfg[xx] &= power_state_mask;
            p_cb->ecb_flags |= NFA_EE_ECB_FLAGS_AID;
        }

        p_cb->proto_switch_off &= 0x00;
        p_cb->tech_switch_off &= 0x00;
    }

    p_cb->ecb_flags |= NFA_EE_ECB_FLAGS_TECH|NFA_EE_ECB_FLAGS_PROTO;

    return status;
}

/*******************************************************************************
**
** Function         NFA_GetAidTableSize
**
** Description      This function is called to get the Maximum AID routing table size.
**
** Returns          AID routing table maximum size
**
*******************************************************************************/
UINT16 NFA_GetAidTableSize()
{
    unsigned int nfa_ee_max_aid_cfg_len = NFA_EE_MAX_AID_CFG_LEN;

    if (phNxpNciHal_getChipType() == pn547C2)
    {
        /* AID Entry and the Configuration size allocated is less
         * in pn547
         */
    	nfa_ee_max_aid_cfg_len = NFA_EE_MAX_AID_CFG_LEN_PN547C2;
    }

    return nfa_ee_max_aid_cfg_len;
}

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
UINT16 NFA_GetRemainingAidTableSize()
{
    UINT16 size = 0;

    size = NFA_GetAidTableSize() - nfa_ee_lmrt_size();

    return size ;
}
/*******************************************************************************
**
** Function         NFA_SetCEStrictDisable
**
** Description      This function is called to set the flag for Strict CE.
**
** Returns          None.
**
*******************************************************************************/
void NFA_SetCEStrictDisable(UINT32 state)
{
    nfa_ee_ce_route_strict_disable = (UINT8)state;
}
#endif

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
tNFA_STATUS NFA_EeRemoveAidRouting(UINT8     aid_len,
                                   UINT8    *p_aid)
{
    tNFA_EE_API_REMOVE_AID *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT16 size = sizeof(tNFA_EE_API_REMOVE_AID) + aid_len;

    NFA_TRACE_API0 ("NFA_EeRemoveAidRouting()");
    if ((aid_len == 0) || (p_aid == NULL) || (aid_len > NFA_MAX_AID_LEN))
    {
        NFA_TRACE_ERROR0 ("Bad AID");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_REMOVE_AID *) GKI_getbuf (size)) != NULL)
    {
        p_msg->hdr.event        = NFA_EE_API_REMOVE_AID_EVT;
        p_msg->aid_len          = aid_len;
        p_msg->p_aid            = (UINT8 *)(p_msg + 1);
        memcpy(p_msg->p_aid, p_aid, aid_len);

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeGetLmrtRemainingSize (void)
{
    tNFA_EE_API_LMRT_SIZE *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;

    NFA_TRACE_API0 ("NFA_EeGetLmrtRemainingSize()");
    if ((p_msg = (tNFA_EE_API_LMRT_SIZE *) GKI_getbuf (sizeof(tNFA_EE_API_LMRT_SIZE))) != NULL)
    {
        p_msg->event    = NFA_EE_API_LMRT_SIZE_EVT;
        nfa_sys_sendmsg (p_msg);
        status = NFA_STATUS_OK;
    }

    return status;
}

/******************************************************************************
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
tNFA_STATUS NFA_EeUpdateNow(void)
{
    BT_HDR *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;

    NFA_TRACE_API0 ("NFA_EeUpdateNow()");
    if (nfa_ee_cb.ee_wait_evt & NFA_EE_WAIT_UPDATE_ALL)
    {
        NFA_TRACE_ERROR0 ("update in progress");
        status = NFA_STATUS_SEMANTIC_ERROR;
    }
    else if ((p_msg = (BT_HDR *) GKI_getbuf (BT_HDR_SIZE)) != NULL)
    {
        p_msg->event    = NFA_EE_API_UPDATE_NOW_EVT;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}


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
tNFA_STATUS NFA_EeConnect(tNFA_HANDLE    ee_handle,
                          UINT8          ee_interface,
                          tNFA_EE_CBACK *p_cback)
{
    tNFA_EE_API_CONNECT *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;

    NFA_TRACE_API2 ("NFA_EeConnect(): handle:<0x%x> ee_interface:0x%x", ee_handle, ee_interface);
    p_cb = nfa_ee_find_ecb (nfcee_id);

    if ((p_cb == NULL) || (p_cback == NULL))
    {
        NFA_TRACE_ERROR0 ("Bad ee_handle or NULL callback function");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_CONNECT *) GKI_getbuf (sizeof(tNFA_EE_API_CONNECT))) != NULL)
    {
        p_msg->hdr.event        = NFA_EE_API_CONNECT_EVT;
        p_msg->nfcee_id         = nfcee_id;
        p_msg->p_cb             = p_cb;
        p_msg->ee_interface     = ee_interface;
        p_msg->p_cback          = p_cback;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeSendData (tNFA_HANDLE  ee_handle,
                            UINT16       data_len,
                            UINT8       *p_data)
{
    tNFA_EE_API_SEND_DATA *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;

    NFA_TRACE_API1 ("NFA_EeSendData(): handle:<0x%x>", ee_handle);

    p_cb = nfa_ee_find_ecb (nfcee_id);

    if ((p_cb == NULL) || (p_cb->conn_st != NFA_EE_CONN_ST_CONN) || (p_data == NULL))
    {
        NFA_TRACE_ERROR0 ("Bad ee_handle or NULL data");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_SEND_DATA *) GKI_getbuf ((UINT16)(sizeof(tNFA_EE_API_SEND_DATA) + data_len))) != NULL)
    {
        p_msg->hdr.event        = NFA_EE_API_SEND_DATA_EVT;
        p_msg->nfcee_id         = nfcee_id;
        p_msg->p_cb             = p_cb;
        p_msg->data_len         = data_len;
        p_msg->p_data           = (UINT8 *)(p_msg + 1);
        memcpy(p_msg->p_data, p_data, data_len);

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}

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
tNFA_STATUS NFA_EeDisconnect(tNFA_HANDLE ee_handle)
{
    tNFA_EE_API_DISCONNECT *p_msg;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT8       nfcee_id = (UINT8)(ee_handle & 0xFF);
    tNFA_EE_ECB *p_cb;

    NFA_TRACE_API1 ("NFA_EeDisconnect(): handle:<0x%x>", ee_handle);
    p_cb = nfa_ee_find_ecb (nfcee_id);

    if ((p_cb == NULL) || (p_cb->conn_st != NFA_EE_CONN_ST_CONN))
    {
        NFA_TRACE_ERROR0 ("NFA_EeDisconnect() Bad ee_handle");
        status = NFA_STATUS_INVALID_PARAM;
    }
    else if ((p_msg = (tNFA_EE_API_DISCONNECT *) GKI_getbuf (sizeof(tNFA_EE_API_DISCONNECT))) != NULL)
    {
        p_msg->hdr.event        = NFA_EE_API_DISCONNECT_EVT;
        p_msg->nfcee_id         = nfcee_id;
        p_msg->p_cb             = p_cb;

        nfa_sys_sendmsg (p_msg);

        status = NFA_STATUS_OK;
    }

    return status;
}
