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
 *  This file contains the LLCP API code
 *
 ******************************************************************************/

#include <string.h>
#include "gki.h"
#include "nfc_target.h"
#include "bt_types.h"
#include "llcp_api.h"
#include "llcp_int.h"
#include "llcp_defs.h"

#if (LLCP_TEST_INCLUDED == TRUE) /* this is for LLCP testing */

tLLCP_TEST_PARAMS llcp_test_params =
{
    LLCP_VERSION_VALUE,
    0,                             /* not override */
};

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
void LLCP_SetTestParams (UINT8 version, UINT16 wks)
{
    LLCP_TRACE_API2 ("LLCP_SetTestParams () version:0x%02X, wks:0x%04X",
                     version, wks);

    if (version != 0xFF)
        llcp_test_params.version = version;

    if (wks != 0xFFFF)
        llcp_test_params.wks = wks;
}
#endif

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
void LLCP_RegisterDtaCback (tLLCP_DTA_CBACK *p_dta_cback)
{
    LLCP_TRACE_API0 ("LLCP_RegisterDtaCback ()");

    llcp_cb.p_dta_cback = p_dta_cback;
}
#endif

/*******************************************************************************
**
** Function         LLCP_SetConfig
**
** Description      Set configuration parameters for LLCP
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
void LLCP_SetConfig (UINT16 link_miu,
                     UINT8  opt,
                     UINT8  wt,
                     UINT16 link_timeout,
                     UINT16 inact_timeout_init,
                     UINT16 inact_timeout_target,
                     UINT16 symm_delay,
                     UINT16 data_link_timeout,
                     UINT16 delay_first_pdu_timeout)
{
    LLCP_TRACE_API4 ("LLCP_SetConfig () link_miu:%d, opt:0x%02X, wt:%d, link_timeout:%d",
                     link_miu, opt, wt, link_timeout);
    LLCP_TRACE_API4 ("                 inact_timeout (init:%d,target:%d), symm_delay:%d, data_link_timeout:%d",
                     inact_timeout_init, inact_timeout_target, symm_delay, data_link_timeout);
    LLCP_TRACE_API1 ("                 delay_first_pdu_timeout:%d", delay_first_pdu_timeout);

    if (link_miu < LLCP_DEFAULT_MIU)
    {
        LLCP_TRACE_ERROR1 ("LLCP_SetConfig (): link_miu shall not be smaller than LLCP_DEFAULT_MIU (%d)",
                            LLCP_DEFAULT_MIU);
        link_miu = LLCP_DEFAULT_MIU;
    }
    else if (link_miu > LLCP_MAX_MIU)
    {
        LLCP_TRACE_ERROR1 ("LLCP_SetConfig (): link_miu shall not be bigger than LLCP_MAX_MIU (%d)",
                            LLCP_MAX_MIU);
        link_miu = LLCP_MAX_MIU;
    }

    /* if Link MIU is bigger than GKI buffer */
    if (link_miu > LLCP_MIU)
    {
        LLCP_TRACE_ERROR1 ("LLCP_SetConfig (): link_miu shall not be bigger than LLCP_MIU (%d)",
                            LLCP_MIU);
        llcp_cb.lcb.local_link_miu = LLCP_MIU;
    }
    else
        llcp_cb.lcb.local_link_miu = link_miu;

    llcp_cb.lcb.local_opt = opt;
    llcp_cb.lcb.local_wt  = wt;

    if (link_timeout < LLCP_LTO_UNIT)
    {
        LLCP_TRACE_ERROR1 ("LLCP_SetConfig (): link_timeout shall not be smaller than LLCP_LTO_UNIT (%d ms)",
                            LLCP_LTO_UNIT);
        llcp_cb.lcb.local_lto = LLCP_DEFAULT_LTO_IN_MS;
    }
    else if (link_timeout > LLCP_MAX_LTO_IN_MS)
    {
        LLCP_TRACE_ERROR1 ("LLCP_SetConfig (): link_timeout shall not be bigger than LLCP_MAX_LTO_IN_MS (%d ms)",
                            LLCP_MAX_LTO_IN_MS);
        llcp_cb.lcb.local_lto = LLCP_MAX_LTO_IN_MS;
    }
    else
        llcp_cb.lcb.local_lto = link_timeout;

    llcp_cb.lcb.inact_timeout_init   = inact_timeout_init;
    llcp_cb.lcb.inact_timeout_target = inact_timeout_target;
    llcp_cb.lcb.symm_delay           = symm_delay;
    llcp_cb.lcb.data_link_timeout    = data_link_timeout;
    llcp_cb.lcb.delay_first_pdu_timeout = delay_first_pdu_timeout;
}

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
void LLCP_GetConfig (UINT16 *p_link_miu,
                     UINT8  *p_opt,
                     UINT8  *p_wt,
                     UINT16 *p_link_timeout,
                     UINT16 *p_inact_timeout_init,
                     UINT16 *p_inact_timeout_target,
                     UINT16 *p_symm_delay,
                     UINT16 *p_data_link_timeout,
                     UINT16 *p_delay_first_pdu_timeout)
{
    *p_link_miu             = llcp_cb.lcb.local_link_miu;
    *p_opt                  = llcp_cb.lcb.local_opt;
    *p_wt                   = llcp_cb.lcb.local_wt;
    *p_link_timeout         = llcp_cb.lcb.local_lto;
    *p_inact_timeout_init   = llcp_cb.lcb.inact_timeout_init;
    *p_inact_timeout_target = llcp_cb.lcb.inact_timeout_target;
    *p_symm_delay           = llcp_cb.lcb.symm_delay;
    *p_data_link_timeout    = llcp_cb.lcb.data_link_timeout;
    *p_delay_first_pdu_timeout = llcp_cb.lcb.delay_first_pdu_timeout;

    LLCP_TRACE_API4 ("LLCP_GetConfig () link_miu:%d, opt:0x%02X, wt:%d, link_timeout:%d",
                     *p_link_miu, *p_opt, *p_wt, *p_link_timeout);
    LLCP_TRACE_API4 ("                 inact_timeout (init:%d, target:%d), symm_delay:%d, data_link_timeout:%d",
                     *p_inact_timeout_init, *p_inact_timeout_target, *p_symm_delay, *p_data_link_timeout);
    LLCP_TRACE_API1 ("                 delay_first_pdu_timeout:%d", *p_delay_first_pdu_timeout);
}

/*******************************************************************************
**
** Function         LLCP_GetDiscoveryConfig
**
** Description      Returns discovery config for ISO 18092 MAC link activation
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
void LLCP_GetDiscoveryConfig (UINT8 *p_wt,
                              UINT8 *p_gen_bytes,
                              UINT8 *p_gen_bytes_len)
{
    UINT8      *p = p_gen_bytes;

    LLCP_TRACE_API0 ("LLCP_GetDiscoveryConfig ()");

    if (*p_gen_bytes_len < LLCP_MIN_GEN_BYTES)
    {
        LLCP_TRACE_ERROR1 ("LLCP_GetDiscoveryConfig (): GenBytes length shall not be smaller than LLCP_MIN_GEN_BYTES (%d)",
                            LLCP_MIN_GEN_BYTES);
        *p_gen_bytes_len = 0;
        return;
    }

    *p_wt = llcp_cb.lcb.local_wt;

    UINT8_TO_BE_STREAM (p, LLCP_MAGIC_NUMBER_BYTE0);
    UINT8_TO_BE_STREAM (p, LLCP_MAGIC_NUMBER_BYTE1);
    UINT8_TO_BE_STREAM (p, LLCP_MAGIC_NUMBER_BYTE2);

#if (LLCP_TEST_INCLUDED == TRUE) /* this is for LLCP testing */
    UINT8_TO_BE_STREAM (p, LLCP_VERSION_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_VERSION_LEN);
    UINT8_TO_BE_STREAM (p, llcp_test_params.version);

    UINT8_TO_BE_STREAM (p, LLCP_MIUX_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_MIUX_LEN);
    UINT16_TO_BE_STREAM (p, (llcp_cb.lcb.local_link_miu - LLCP_DEFAULT_MIU));

    UINT8_TO_BE_STREAM (p, LLCP_WKS_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_WKS_LEN);
    if (llcp_test_params.wks == 0)  /* not override */
    {
        UINT16_TO_BE_STREAM (p, llcp_cb.lcb.wks);
    }
    else
    {
        UINT16_TO_BE_STREAM (p, llcp_test_params.wks);
    }
#else
    UINT8_TO_BE_STREAM (p, LLCP_VERSION_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_VERSION_LEN);
    UINT8_TO_BE_STREAM (p, LLCP_VERSION_VALUE);

    UINT8_TO_BE_STREAM (p, LLCP_MIUX_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_MIUX_LEN);
    UINT16_TO_BE_STREAM (p, (llcp_cb.lcb.local_link_miu - LLCP_DEFAULT_MIU));

    UINT8_TO_BE_STREAM (p, LLCP_WKS_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_WKS_LEN);
    UINT16_TO_BE_STREAM (p, llcp_cb.lcb.wks);
#endif

    UINT8_TO_BE_STREAM (p, LLCP_LTO_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_LTO_LEN);
    UINT8_TO_BE_STREAM (p, (llcp_cb.lcb.local_lto/LLCP_LTO_UNIT));

    UINT8_TO_BE_STREAM (p, LLCP_OPT_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_OPT_LEN);
    UINT8_TO_BE_STREAM (p, llcp_cb.lcb.local_opt);

    *p_gen_bytes_len = (UINT8) (p - p_gen_bytes);
}

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
tLLCP_STATUS LLCP_ActivateLink (tLLCP_ACTIVATE_CONFIG config,
                                tLLCP_LINK_CBACK     *p_link_cback)
{
    LLCP_TRACE_API1 ("LLCP_ActivateLink () link_state = %d", llcp_cb.lcb.link_state);

    if (  (llcp_cb.lcb.link_state == LLCP_LINK_STATE_DEACTIVATED)
        &&(p_link_cback)  )
    {
        llcp_cb.lcb.p_link_cback = p_link_cback;
        return (llcp_link_activate (&config));
    }
    else
        return LLCP_STATUS_FAIL;
}

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
tLLCP_STATUS LLCP_DeactivateLink (void)
{
    LLCP_TRACE_API1 ("LLCP_DeactivateLink () link_state = %d", llcp_cb.lcb.link_state);

    if (llcp_cb.lcb.link_state != LLCP_LINK_STATE_DEACTIVATED)
    {
        llcp_link_deactivate (LLCP_LINK_LOCAL_INITIATED);
        return LLCP_STATUS_SUCCESS;
    }
    else
        return LLCP_STATUS_FAIL;
}

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
UINT8 LLCP_RegisterServer (UINT8           reg_sap,
                           UINT8           link_type,
                           char            *p_service_name,
                           tLLCP_APP_CBACK *p_app_cback)
{
    UINT8  sap;
    UINT16 length;
    tLLCP_APP_CB *p_app_cb;

    LLCP_TRACE_API3 ("LLCP_RegisterServer (): SAP:0x%x, link_type:0x%x, ServiceName:<%s>",
                     reg_sap, link_type, ((p_service_name == NULL) ? "" : p_service_name));

    if (!p_app_cback)
    {
        LLCP_TRACE_ERROR0 ("LLCP_RegisterServer (): Callback must be provided");
        return LLCP_INVALID_SAP;
    }
    else if (  ((link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK) == 0x00)
             &&((link_type & LLCP_LINK_TYPE_DATA_LINK_CONNECTION) == 0x00)  )
    {
        LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): link type (0x%x) must be specified", link_type);
        return LLCP_INVALID_SAP;
    }

    if (reg_sap == LLCP_INVALID_SAP)
    {
        /* allocate a SAP between 0x10 and 0x1F */
        for (sap = 0; sap < LLCP_MAX_SERVER; sap++)
        {
            if (llcp_cb.server_cb[sap].p_app_cback == NULL)
            {
                p_app_cb = &llcp_cb.server_cb[sap];
                reg_sap  = LLCP_LOWER_BOUND_SDP_SAP + sap;
                break;
            }
        }

        if (reg_sap == LLCP_INVALID_SAP)
        {
            LLCP_TRACE_ERROR0 ("LLCP_RegisterServer (): out of resource");
            return LLCP_INVALID_SAP;
        }
    }
    else if (reg_sap == LLCP_SAP_LM)
    {
        LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): SAP (0x%x) is for link manager", reg_sap);
        return LLCP_INVALID_SAP;
    }
    else if (reg_sap <= LLCP_UPPER_BOUND_WK_SAP)
    {
        if (reg_sap >= LLCP_MAX_WKS)
        {
            LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): out of resource for SAP (0x%x)", reg_sap);
            return LLCP_INVALID_SAP;
        }
        else if (llcp_cb.wks_cb[reg_sap].p_app_cback)
        {
            LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): SAP (0x%x) is already registered", reg_sap);
            return LLCP_INVALID_SAP;
        }
        else
        {
            p_app_cb = &llcp_cb.wks_cb[reg_sap];
        }
    }
    else if (reg_sap <= LLCP_UPPER_BOUND_SDP_SAP)
    {
        if (reg_sap - LLCP_LOWER_BOUND_SDP_SAP >= LLCP_MAX_SERVER)
        {
            LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): out of resource for SAP (0x%x)", reg_sap);
            return LLCP_INVALID_SAP;
        }
        else if (llcp_cb.server_cb[reg_sap - LLCP_LOWER_BOUND_SDP_SAP].p_app_cback)
        {
            LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): SAP (0x%x) is already registered", reg_sap);
            return LLCP_INVALID_SAP;
        }
        else
        {
            p_app_cb = &llcp_cb.server_cb[reg_sap - LLCP_LOWER_BOUND_SDP_SAP];
        }
    }
    else if (reg_sap >= LLCP_LOWER_BOUND_LOCAL_SAP)
    {
        LLCP_TRACE_ERROR2 ("LLCP_RegisterServer (): SAP (0x%x) must be less than 0x%x",
                            reg_sap, LLCP_LOWER_BOUND_LOCAL_SAP);
        return LLCP_INVALID_SAP;
    }

    memset (p_app_cb, 0x00, sizeof (tLLCP_APP_CB));

    if (p_service_name)
    {
        length = (UINT8) strlen (p_service_name);
        if (length > LLCP_MAX_SN_LEN)
        {
            LLCP_TRACE_ERROR1 ("LLCP_RegisterServer (): Service Name (%d bytes) is too long", length);
            return LLCP_INVALID_SAP;
        }

        p_app_cb->p_service_name = (UINT8 *) GKI_getbuf ((UINT16) (length + 1));
        if (p_app_cb->p_service_name == NULL)
        {
            LLCP_TRACE_ERROR0 ("LLCP_RegisterServer (): Out of resource");
            return LLCP_INVALID_SAP;
        }

        BCM_STRNCPY_S ((char *) p_app_cb->p_service_name, length + 1, (char *) p_service_name, length + 1);
        p_app_cb->p_service_name[length] = 0;
    }
    else
        p_app_cb->p_service_name = NULL;

    p_app_cb->p_app_cback = p_app_cback;
    p_app_cb->link_type   = link_type;

    if (reg_sap <= LLCP_UPPER_BOUND_WK_SAP)
    {
        llcp_cb.lcb.wks |= (1 << reg_sap);
    }

    LLCP_TRACE_DEBUG1 ("LLCP_RegisterServer (): Registered SAP = 0x%02X", reg_sap);

    if (link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK)
    {
        llcp_cb.num_logical_data_link++;
        llcp_util_adjust_ll_congestion ();
    }

    return reg_sap;
}

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
UINT8 LLCP_RegisterClient (UINT8           link_type,
                           tLLCP_APP_CBACK *p_app_cback)
{
    UINT8 reg_sap = LLCP_INVALID_SAP;
    UINT8 sap;
    tLLCP_APP_CB *p_app_cb;

    LLCP_TRACE_API1 ("LLCP_RegisterClient (): link_type = 0x%x", link_type);

    if (!p_app_cback)
    {
        LLCP_TRACE_ERROR0 ("LLCP_RegisterClient (): Callback must be provided");
        return LLCP_INVALID_SAP;
    }
    else if (  ((link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK) == 0x00)
             &&((link_type & LLCP_LINK_TYPE_DATA_LINK_CONNECTION) == 0x00)  )
    {
        LLCP_TRACE_ERROR1 ("LLCP_RegisterClient (): link type (0x%x) must be specified", link_type);
        return LLCP_INVALID_SAP;
    }

    /* allocate a SAP between 0x20 and 0x3F */
    for (sap = 0; sap < LLCP_MAX_CLIENT; sap++)
    {
        if (llcp_cb.client_cb[sap].p_app_cback == NULL)
        {
            p_app_cb = &llcp_cb.client_cb[sap];
            memset (p_app_cb, 0x00, sizeof (tLLCP_APP_CB));
            reg_sap = LLCP_LOWER_BOUND_LOCAL_SAP + sap;
            break;
        }
    }

    if (reg_sap == LLCP_INVALID_SAP)
    {
        LLCP_TRACE_ERROR0 ("LLCP_RegisterClient (): out of resource");
        return LLCP_INVALID_SAP;
    }

    p_app_cb->p_app_cback    = p_app_cback;
    p_app_cb->p_service_name = NULL;
    p_app_cb->link_type      = link_type;

    LLCP_TRACE_DEBUG1 ("LLCP_RegisterClient (): Registered SAP = 0x%02X", reg_sap);

    if (link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK)
    {
        llcp_cb.num_logical_data_link++;
        llcp_util_adjust_ll_congestion ();
    }

    return reg_sap;
}

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
tLLCP_STATUS LLCP_Deregister (UINT8 local_sap)
{
    UINT8 idx;
    tLLCP_APP_CB *p_app_cb;

    LLCP_TRACE_API1 ("LLCP_Deregister () SAP:0x%x", local_sap);

    p_app_cb = llcp_util_get_app_cb (local_sap);

    if ((!p_app_cb) || (p_app_cb->p_app_cback == NULL))
    {
        LLCP_TRACE_ERROR1 ("LLCP_Deregister (): SAP (0x%x) is not registered", local_sap);
        return LLCP_STATUS_FAIL;
    }

    if (p_app_cb->p_service_name)
        GKI_freebuf (p_app_cb->p_service_name);

    /* update WKS bit map */
    if (local_sap <= LLCP_UPPER_BOUND_WK_SAP)
    {
        llcp_cb.lcb.wks &= ~ (1 << local_sap);
    }

    /* discard any received UI PDU on this SAP */
    LLCP_FlushLogicalLinkRxData (local_sap);
    llcp_cb.total_rx_ui_pdu = 0;

    /* deallocate any data link connection on this SAP */
    for (idx = 0; idx < LLCP_MAX_DATA_LINK; idx++)
    {
        if (  (llcp_cb.dlcb[idx].state != LLCP_DLC_STATE_IDLE)
            &&(llcp_cb.dlcb[idx].local_sap == local_sap)  )
        {
            llcp_util_deallocate_data_link (&llcp_cb.dlcb[idx]);
        }
    }

    p_app_cb->p_app_cback = NULL;

    /* discard any pending tx UI PDU from this SAP */
    while (p_app_cb->ui_xmit_q.p_first)
    {
        GKI_freebuf (GKI_dequeue (&p_app_cb->ui_xmit_q));
        llcp_cb.total_tx_ui_pdu--;
    }

    if (p_app_cb->link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK)
    {
        llcp_cb.num_logical_data_link--;
        llcp_util_adjust_ll_congestion ();
    }

    /* check rx congestion status */
    llcp_util_check_rx_congested_status ();

    return LLCP_STATUS_SUCCESS;
}

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
BOOLEAN LLCP_IsLogicalLinkCongested (UINT8 local_sap,
                                     UINT8 num_pending_ui_pdu,
                                     UINT8 total_pending_ui_pdu,
                                     UINT8 total_pending_i_pdu)
{
    tLLCP_APP_CB *p_app_cb;

    LLCP_TRACE_API4 ("LLCP_IsLogicalLinkCongested () Local SAP:0x%x, pending = (%d, %d, %d)",
                     local_sap, num_pending_ui_pdu, total_pending_ui_pdu, total_pending_i_pdu);

    p_app_cb = llcp_util_get_app_cb (local_sap);

    if (  (llcp_cb.lcb.link_state != LLCP_LINK_STATE_ACTIVATED)
        ||(p_app_cb == NULL)
        ||(p_app_cb->p_app_cback == NULL)
        ||((p_app_cb->link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK) == 0)
        ||(p_app_cb->is_ui_tx_congested)  )
    {
        return (TRUE);
    }
    else if (  (num_pending_ui_pdu + p_app_cb->ui_xmit_q.count >= llcp_cb.ll_tx_congest_start)
             ||(total_pending_ui_pdu + llcp_cb.total_tx_ui_pdu >= llcp_cb.max_num_ll_tx_buff)
             ||(total_pending_ui_pdu + total_pending_i_pdu + llcp_cb.total_tx_ui_pdu + llcp_cb.total_tx_i_pdu >= llcp_cb.max_num_tx_buff)  )
    {
        /* set flag so LLCP can notify uncongested status later */
        p_app_cb->is_ui_tx_congested = TRUE;

        return (TRUE);
    }
    return (FALSE);
}

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
tLLCP_STATUS LLCP_SendUI (UINT8   ssap,
                          UINT8   dsap,
                          BT_HDR *p_buf)
{
    tLLCP_STATUS status = LLCP_STATUS_FAIL;
    tLLCP_APP_CB *p_app_cb;

    LLCP_TRACE_API2 ("LLCP_SendUI () SSAP=0x%x, DSAP=0x%x", ssap, dsap);

    p_app_cb = llcp_util_get_app_cb (ssap);

    if (  (p_app_cb == NULL)
        ||(p_app_cb->p_app_cback == NULL)  )
    {
        LLCP_TRACE_ERROR1 ("LLCP_SendUI (): SSAP (0x%x) is not registered", ssap);
    }
    else if ((p_app_cb->link_type & LLCP_LINK_TYPE_LOGICAL_DATA_LINK) == 0)
    {
        LLCP_TRACE_ERROR1 ("LLCP_SendUI (): Logical link on SSAP (0x%x) is not enabled", ssap);
    }
    else if (llcp_cb.lcb.link_state != LLCP_LINK_STATE_ACTIVATED)
    {
        LLCP_TRACE_ERROR0 ("LLCP_SendUI (): LLCP link is not activated");
    }
    else if (  (llcp_cb.lcb.peer_opt == LLCP_LSC_UNKNOWN)
             ||(llcp_cb.lcb.peer_opt & LLCP_LSC_1)  )
    {
        if (p_buf->len <= llcp_cb.lcb.peer_miu)
        {
            if (p_buf->offset >= LLCP_MIN_OFFSET)
            {
                status = llcp_util_send_ui (ssap, dsap, p_app_cb, p_buf);
            }
            else
            {
                LLCP_TRACE_ERROR2 ("LLCP_SendUI (): offset (%d) must be %d at least",
                                    p_buf->offset, LLCP_MIN_OFFSET );
            }
        }
        else
        {
            LLCP_TRACE_ERROR0 ("LLCP_SendUI (): Data length shall not be bigger than peer's link MIU");
        }
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_SendUI (): Peer doesn't support connectionless link");
    }

    if (status == LLCP_STATUS_FAIL)
    {
        GKI_freebuf (p_buf);
    }

    return status;
}

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
BOOLEAN LLCP_ReadLogicalLinkData (UINT8  local_sap,
                                  UINT32 max_data_len,
                                  UINT8  *p_remote_sap,
                                  UINT32 *p_data_len,
                                  UINT8  *p_data)
{
    tLLCP_APP_CB *p_app_cb;
    BT_HDR       *p_buf;
    UINT8        *p_ui_pdu;
    UINT16       pdu_hdr, ui_pdu_length;

    LLCP_TRACE_API1 ("LLCP_ReadLogicalLinkData () Local SAP:0x%x", local_sap);

    *p_data_len = 0;

    p_app_cb = llcp_util_get_app_cb (local_sap);

    /* if application is registered */
    if ((p_app_cb) && (p_app_cb->p_app_cback))
    {
        /* if any UI PDU in rx queue */
        if (p_app_cb->ui_rx_q.p_first)
        {
            p_buf    = (BT_HDR *) p_app_cb->ui_rx_q.p_first;
            p_ui_pdu = (UINT8*) (p_buf + 1) + p_buf->offset;

            /* get length of UI PDU */
            BE_STREAM_TO_UINT16 (ui_pdu_length, p_ui_pdu);

            /* get remote SAP from LLCP header */
            BE_STREAM_TO_UINT16 (pdu_hdr, p_ui_pdu);
            *p_remote_sap = LLCP_GET_SSAP (pdu_hdr);

            /* layer_specific has the offset to read within UI PDU */
            p_ui_pdu += p_buf->layer_specific;

            /* copy data up to max_data_len */
            if (max_data_len >= (UINT32) (ui_pdu_length - LLCP_PDU_HEADER_SIZE - p_buf->layer_specific))
            {
                /* copy information without LLCP header */
                *p_data_len = (UINT32) (ui_pdu_length - LLCP_PDU_HEADER_SIZE - p_buf->layer_specific);

                /* move to next UI PDU if any */
                p_buf->layer_specific = 0;  /* reset offset to read from the first byte of next UI PDU */
                p_buf->offset += LLCP_PDU_AGF_LEN_SIZE + ui_pdu_length;
                p_buf->len    -= LLCP_PDU_AGF_LEN_SIZE + ui_pdu_length;
            }
            else
            {
                *p_data_len = max_data_len;

                /* update offset to read from remaining UI PDU next time */
                p_buf->layer_specific += max_data_len;
            }

            memcpy (p_data, p_ui_pdu, *p_data_len);

            /* if read all of UI PDU */
            if (p_buf->len == 0)
            {
                GKI_dequeue (&p_app_cb->ui_rx_q);
                GKI_freebuf (p_buf);

                /* decrease number of received UI PDU in in all of ui_rx_q and check rx congestion status */
                llcp_cb.total_rx_ui_pdu--;
                llcp_util_check_rx_congested_status ();
            }
        }

        /* if there is more UI PDU in rx queue */
        if (p_app_cb->ui_rx_q.p_first)
        {
            return (TRUE);
        }
        else
        {
            return (FALSE);
        }
    }
    else
    {
        LLCP_TRACE_ERROR1 ("LLCP_ReadLogicalLinkData (): Unregistered SAP:0x%x", local_sap);

        return (FALSE);
    }
}

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
UINT32 LLCP_FlushLogicalLinkRxData (UINT8 local_sap)
{
    BT_HDR       *p_buf;
    UINT32       flushed_length = 0;
    tLLCP_APP_CB *p_app_cb;
    UINT8        *p_ui_pdu;
    UINT16       ui_pdu_length;

    LLCP_TRACE_API1 ("LLCP_FlushLogicalLinkRxData () Local SAP:0x%x", local_sap);

    p_app_cb = llcp_util_get_app_cb (local_sap);

    /* if application is registered */
    if ((p_app_cb) && (p_app_cb->p_app_cback))
    {
        /* if any UI PDU in rx queue */
        while (p_app_cb->ui_rx_q.p_first)
        {
            p_buf    = (BT_HDR *) p_app_cb->ui_rx_q.p_first;
            p_ui_pdu = (UINT8*) (p_buf + 1) + p_buf->offset;

            /* get length of UI PDU */
            BE_STREAM_TO_UINT16 (ui_pdu_length, p_ui_pdu);

            flushed_length += (UINT32) (ui_pdu_length - LLCP_PDU_HEADER_SIZE - p_buf->layer_specific);

            /* move to next UI PDU if any */
            p_buf->layer_specific = 0;  /* offset */
            p_buf->offset += LLCP_PDU_AGF_LEN_SIZE + ui_pdu_length;
            p_buf->len    -= LLCP_PDU_AGF_LEN_SIZE + ui_pdu_length;

            /* if read all of UI PDU */
            if (p_buf->len == 0)
            {
                GKI_dequeue (&p_app_cb->ui_rx_q);
                GKI_freebuf (p_buf);
                llcp_cb.total_rx_ui_pdu--;
            }
        }

        /* number of received UI PDU is decreased so check rx congestion status */
        llcp_util_check_rx_congested_status ();
    }
    else
    {
        LLCP_TRACE_ERROR1 ("LLCP_FlushLogicalLinkRxData (): Unregistered SAP:0x%x", local_sap);
    }

    return (flushed_length);
}

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
tLLCP_STATUS LLCP_ConnectReq (UINT8                    reg_sap,
                              UINT8                    dsap,
                              tLLCP_CONNECTION_PARAMS *p_params)
{
    tLLCP_DLCB   *p_dlcb;
    tLLCP_STATUS status;
    tLLCP_APP_CB *p_app_cb;
    tLLCP_CONNECTION_PARAMS params;

    LLCP_TRACE_API2 ("LLCP_ConnectReq () reg_sap=0x%x, DSAP=0x%x", reg_sap, dsap);

    if (  (llcp_cb.lcb.peer_opt != LLCP_LSC_UNKNOWN)
        &&((llcp_cb.lcb.peer_opt & LLCP_LSC_2) == 0)  )
    {
        LLCP_TRACE_ERROR0 ("LLCP_ConnectReq (): Peer doesn't support connection-oriented link");
        return LLCP_STATUS_FAIL;
    }

    if (!p_params)
    {
        params.miu   = LLCP_DEFAULT_MIU;
        params.rw    = LLCP_DEFAULT_RW;
        params.sn[0] = 0;
        p_params     = &params;
    }

    p_app_cb = llcp_util_get_app_cb (reg_sap);

    /* if application is registered */
    if (  (p_app_cb == NULL)
        ||(p_app_cb->p_app_cback == NULL)  )
    {
        LLCP_TRACE_ERROR1 ("LLCP_ConnectReq (): SSAP (0x%x) is not registered", reg_sap);
        return LLCP_STATUS_FAIL;
    }

    if (dsap == LLCP_SAP_LM)
    {
        LLCP_TRACE_ERROR1 ("LLCP_ConnectReq (): DSAP (0x%x) must not be link manager SAP", dsap);
        return LLCP_STATUS_FAIL;
    }

    if (dsap == LLCP_SAP_SDP)
    {
        if (strlen (p_params->sn) > LLCP_MAX_SN_LEN)
        {
            LLCP_TRACE_ERROR1 ("LLCP_ConnectReq (): Service Name (%d bytes) is too long",
                               strlen (p_params->sn));
            return LLCP_STATUS_FAIL;
        }
    }

    if ((p_params) && (p_params->miu > llcp_cb.lcb.local_link_miu))
    {
        LLCP_TRACE_ERROR0 ("LLCP_ConnectReq (): Data link MIU shall not be bigger than local link MIU");
        return LLCP_STATUS_FAIL;
    }

    /* check if any pending connection request on this reg_sap */
    p_dlcb = llcp_dlc_find_dlcb_by_sap (reg_sap, LLCP_INVALID_SAP);
    if (p_dlcb)
    {
        /*
        ** Accepting LLCP may change SAP in CC, so we cannot find right data link connection
        ** if there is multiple pending connection request on the same local SAP.
        */
        LLCP_TRACE_ERROR0 ("LLCP_ConnectReq (): There is pending connect request on this reg_sap");
        return LLCP_STATUS_FAIL;
    }

    p_dlcb = llcp_util_allocate_data_link (reg_sap, dsap);

    if (p_dlcb)
    {
        status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_CONNECT_REQ, p_params);
        if (status != LLCP_STATUS_SUCCESS)
        {
            LLCP_TRACE_ERROR0 ("LLCP_ConnectReq (): Error in state machine");
            llcp_util_deallocate_data_link (p_dlcb);
            return LLCP_STATUS_FAIL;
        }
    }
    else
    {
        return LLCP_STATUS_FAIL;
    }

    return LLCP_STATUS_SUCCESS;
}

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
tLLCP_STATUS LLCP_ConnectCfm (UINT8                    local_sap,
                              UINT8                    remote_sap,
                              tLLCP_CONNECTION_PARAMS *p_params)
{
    tLLCP_STATUS  status;
    tLLCP_DLCB   *p_dlcb;
    tLLCP_CONNECTION_PARAMS params;

    LLCP_TRACE_API2 ("LLCP_ConnectCfm () Local SAP:0x%x, Remote SAP:0x%x)",
                     local_sap, remote_sap);

    if (!p_params)
    {
        params.miu   = LLCP_DEFAULT_MIU;
        params.rw    = LLCP_DEFAULT_RW;
        params.sn[0] = 0;
        p_params     = &params;
    }
    if (p_params->miu > llcp_cb.lcb.local_link_miu)
    {
        LLCP_TRACE_ERROR0 ("LLCP_ConnectCfm (): Data link MIU shall not be bigger than local link MIU");
        return LLCP_STATUS_FAIL;
    }

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_CONNECT_CFM, p_params);
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_ConnectCfm (): No data link");
        status = LLCP_STATUS_FAIL;
    }

    return status;
}

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
tLLCP_STATUS LLCP_ConnectReject (UINT8 local_sap,
                                 UINT8 remote_sap,
                                 UINT8 reason)
{
    tLLCP_STATUS  status;
    tLLCP_DLCB   *p_dlcb;

    LLCP_TRACE_API3 ("LLCP_ConnectReject () Local SAP:0x%x, Remote SAP:0x%x, reason:0x%x",
                     local_sap, remote_sap, reason);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_CONNECT_REJECT, &reason);
        llcp_util_deallocate_data_link (p_dlcb);
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_ConnectReject (): No data link");
        status = LLCP_STATUS_FAIL;
    }

    return status;
}

/*******************************************************************************
**
** Function         LLCP_IsDataLinkCongested
**
** Description      Check if data link connection is congested
**
**
** Returns          TRUE if congested
**
*******************************************************************************/
BOOLEAN LLCP_IsDataLinkCongested (UINT8 local_sap,
                                  UINT8 remote_sap,
                                  UINT8 num_pending_i_pdu,
                                  UINT8 total_pending_ui_pdu,
                                  UINT8 total_pending_i_pdu)
{
    tLLCP_DLCB   *p_dlcb;

    LLCP_TRACE_API5 ("LLCP_IsDataLinkCongested () Local SAP:0x%x, Remote SAP:0x%x, pending = (%d, %d, %d)",
                     local_sap, remote_sap, num_pending_i_pdu, total_pending_ui_pdu, total_pending_i_pdu);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        if (  (p_dlcb->is_tx_congested)
            ||(p_dlcb->remote_busy)  )
        {
            return (TRUE);
        }
        else if (  (num_pending_i_pdu + p_dlcb->i_xmit_q.count >= p_dlcb->remote_rw)
                 ||(total_pending_ui_pdu + total_pending_i_pdu + llcp_cb.total_tx_ui_pdu + llcp_cb.total_tx_i_pdu >= llcp_cb.max_num_tx_buff)  )
        {
            /* set flag so LLCP can notify uncongested status later */
            p_dlcb->is_tx_congested = TRUE;
            return (TRUE);
        }
        return (FALSE);
    }
    return (TRUE);
}

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
tLLCP_STATUS LLCP_SendData (UINT8   local_sap,
                            UINT8   remote_sap,
                            BT_HDR *p_buf)
{
    tLLCP_STATUS  status = LLCP_STATUS_FAIL;
    tLLCP_DLCB   *p_dlcb;

    LLCP_TRACE_API2 ("LLCP_SendData () Local SAP:0x%x, Remote SAP:0x%x",
                     local_sap, remote_sap);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        if (p_dlcb->remote_miu >= p_buf->len)
        {
            if (p_buf->offset >= LLCP_MIN_OFFSET)
            {
                status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_DATA_REQ, p_buf);
            }
            else
            {
                LLCP_TRACE_ERROR2 ("LLCP_SendData (): offset (%d) must be %d at least",
                                    p_buf->offset, LLCP_MIN_OFFSET );
            }
        }
        else
        {
            LLCP_TRACE_ERROR2 ("LLCP_SendData (): Information (%d bytes) cannot be more than peer MIU (%d bytes)",
                                p_buf->len, p_dlcb->remote_miu);
        }
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_SendData (): No data link");
    }

    if (status == LLCP_STATUS_FAIL)
    {
        GKI_freebuf (p_buf);
    }

    return status;
}

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
BOOLEAN LLCP_ReadDataLinkData (UINT8  local_sap,
                               UINT8  remote_sap,
                               UINT32 max_data_len,
                               UINT32 *p_data_len,
                               UINT8  *p_data)
{
    tLLCP_DLCB *p_dlcb;
    BT_HDR     *p_buf;
    UINT8      *p_i_pdu;
    UINT16     i_pdu_length;

    LLCP_TRACE_API2 ("LLCP_ReadDataLinkData () Local SAP:0x%x, Remote SAP:0x%x",
                      local_sap, remote_sap);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    *p_data_len = 0;
    if (p_dlcb)
    {
        /* if any I PDU in rx queue */
        if (p_dlcb->i_rx_q.p_first)
        {
            p_buf   = (BT_HDR *) p_dlcb->i_rx_q.p_first;
            p_i_pdu = (UINT8*) (p_buf + 1) + p_buf->offset;

            /* get length of I PDU */
            BE_STREAM_TO_UINT16 (i_pdu_length, p_i_pdu);

            /* layer_specific has the offset to read within I PDU */
            p_i_pdu += p_buf->layer_specific;

            /* copy data up to max_data_len */
            if (max_data_len >= (UINT32) (i_pdu_length - p_buf->layer_specific))
            {
                /* copy information */
                *p_data_len = (UINT32) (i_pdu_length - p_buf->layer_specific);

                /* move to next I PDU if any */
                p_buf->layer_specific = 0;  /* reset offset to read from the first byte of next I PDU */
                p_buf->offset += LLCP_PDU_AGF_LEN_SIZE + i_pdu_length;
                p_buf->len    -= LLCP_PDU_AGF_LEN_SIZE + i_pdu_length;
            }
            else
            {
                *p_data_len = max_data_len;

                /* update offset to read from remaining I PDU next time */
                p_buf->layer_specific += max_data_len;
            }

            memcpy (p_data, p_i_pdu, *p_data_len);

            if (p_buf->layer_specific == 0)
            {
                p_dlcb->num_rx_i_pdu--;
            }

            /* if read all of I PDU */
            if (p_buf->len == 0)
            {
                GKI_dequeue (&p_dlcb->i_rx_q);
                GKI_freebuf (p_buf);

                /* decrease number of received I PDU in in all of ui_rx_q and check rx congestion status */
                llcp_cb.total_rx_i_pdu--;
                llcp_util_check_rx_congested_status ();
            }
        }

        /* if getting out of rx congestion */
        if (  (!p_dlcb->local_busy)
            &&(p_dlcb->is_rx_congested)
            &&(p_dlcb->num_rx_i_pdu <= p_dlcb->rx_congest_threshold / 2)  )
        {
            /* send RR */
            p_dlcb->is_rx_congested = FALSE;
            p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_RR_RNR;
        }

        /* if there is more I PDU in rx queue */
        if (p_dlcb->i_rx_q.p_first)
        {
            return (TRUE);
        }
        else
        {
            return (FALSE);
        }
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_ReadDataLinkData (): No data link connection");

        return (FALSE);
    }
}

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
UINT32 LLCP_FlushDataLinkRxData (UINT8  local_sap,
                                 UINT8  remote_sap)
{
    tLLCP_DLCB *p_dlcb;
    BT_HDR     *p_buf;
    UINT32     flushed_length = 0;
    UINT8      *p_i_pdu;
    UINT16     i_pdu_length;

    LLCP_TRACE_API2 ("LLCP_FlushDataLinkRxData () Local SAP:0x%x, Remote SAP:0x%x",
                      local_sap, remote_sap);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
        if(llcp_secured.p2p_flag == TRUE)
        {
            while (p_dlcb->i_rx_q.p_first)
            {
                GKI_freebuf (GKI_dequeue (&p_dlcb->i_rx_q));
                llcp_cb.total_rx_i_pdu--;
             }
         }
   else
   {
#endif
        /* if any I PDU in rx queue */
        while (p_dlcb->i_rx_q.p_first)
        {
            p_buf   = (BT_HDR *) p_dlcb->i_rx_q.p_first;
            p_i_pdu = (UINT8*) (p_buf + 1) + p_buf->offset;

            /* get length of I PDU */
            BE_STREAM_TO_UINT16 (i_pdu_length, p_i_pdu);

            flushed_length += (UINT32) (i_pdu_length - p_buf->layer_specific);

            /* move to next I PDU if any */
            p_buf->layer_specific = 0;  /* offset */
            p_buf->offset += LLCP_PDU_AGF_LEN_SIZE + i_pdu_length;
            p_buf->len    -= LLCP_PDU_AGF_LEN_SIZE + i_pdu_length;

            /* if read all of I PDU */
            if (p_buf->len == 0)
            {
                GKI_dequeue (&p_dlcb->i_rx_q);
                GKI_freebuf (p_buf);
                llcp_cb.total_rx_i_pdu--;
            }
        }
#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
    }
#endif

        p_dlcb->num_rx_i_pdu = 0;

        /* if getting out of rx congestion */
        if (  (!p_dlcb->local_busy)
            &&(p_dlcb->is_rx_congested)  )
        {
            /* send RR */
            p_dlcb->is_rx_congested = FALSE;
            p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_RR_RNR;
        }

        /* number of received I PDU is decreased so check rx congestion status */
        llcp_util_check_rx_congested_status ();
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_FlushDataLinkRxData (): No data link connection");
    }

    return (flushed_length);
}

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
tLLCP_STATUS LLCP_DisconnectReq (UINT8 local_sap,
                                 UINT8 remote_sap,
                                 BOOLEAN flush)
{
    tLLCP_STATUS  status;
    tLLCP_DLCB   *p_dlcb;

    LLCP_TRACE_API3 ("LLCP_DisconnectReq () Local SAP:0x%x, Remote SAP:0x%x, flush=%d",
                     local_sap, remote_sap, flush);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        status = llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_API_DISCONNECT_REQ, &flush);
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_DisconnectReq (): No data link");
        status = LLCP_STATUS_FAIL;
    }

    return status;
}

/*******************************************************************************
**
** Function         LLCP_SetTxCompleteNtf
**
** Description      This function is called to get LLCP_SERVICE_TX_COMPLETE
**                  when Tx queue is empty and all PDU is acked.
**                  This is one time event, so upper layer shall call this function
**                  again to get next LLCP_SERVICE_TX_COMPLETE.
**
** Returns          LLCP_STATUS_SUCCESS if success
**
*******************************************************************************/
tLLCP_STATUS LLCP_SetTxCompleteNtf (UINT8   local_sap,
                                    UINT8   remote_sap)
{
    tLLCP_STATUS  status;
    tLLCP_DLCB   *p_dlcb;

    LLCP_TRACE_API2 ("LLCP_SetTxCompleteNtf () Local SAP:0x%x, Remote SAP:0x%x",
                      local_sap, remote_sap);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        /* set flag to notify upper later when tx complete */
        p_dlcb->flags |= LLCP_DATA_LINK_FLAG_NOTIFY_TX_DONE;
        status = LLCP_STATUS_SUCCESS;
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_SetTxCompleteNtf (): No data link");
        status = LLCP_STATUS_FAIL;
    }

    return status;
}

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
tLLCP_STATUS LLCP_SetLocalBusyStatus (UINT8   local_sap,
                                      UINT8   remote_sap,
                                      BOOLEAN is_busy)
{
    tLLCP_STATUS  status;
    tLLCP_DLCB   *p_dlcb;

    LLCP_TRACE_API2 ("LLCP_SetLocalBusyStatus () Local SAP:0x%x, is_busy=%d",
                      local_sap, is_busy);

    p_dlcb = llcp_dlc_find_dlcb_by_sap (local_sap, remote_sap);

    if (p_dlcb)
    {
        if (p_dlcb->local_busy != is_busy)
        {
            p_dlcb->local_busy = is_busy;

            /* send RR or RNR with valid sequence */
            p_dlcb->flags |= LLCP_DATA_LINK_FLAG_PENDING_RR_RNR;

            if (is_busy == FALSE)
            {
                if (p_dlcb->i_rx_q.count)
                {
                    llcp_dlsm_execute (p_dlcb, LLCP_DLC_EVENT_PEER_DATA_IND, NULL);
                }
            }
        }
        status = LLCP_STATUS_SUCCESS;
    }
    else
    {
        LLCP_TRACE_ERROR0 ("LLCP_SetLocalBusyStatus (): No data link");
        status = LLCP_STATUS_FAIL;
    }

    return status;
}

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
UINT16 LLCP_GetRemoteWKS (void)
{
    LLCP_TRACE_API1 ("LLCP_GetRemoteWKS () WKS:0x%04x",
                     (llcp_cb.lcb.link_state == LLCP_LINK_STATE_ACTIVATED) ? llcp_cb.lcb.peer_wks :0);

    if (llcp_cb.lcb.link_state == LLCP_LINK_STATE_ACTIVATED)
        return (llcp_cb.lcb.peer_wks);
    else
        return (0);
}

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
UINT8 LLCP_GetRemoteLSC (void)
{
    LLCP_TRACE_API1 ("LLCP_GetRemoteLSC () LSC:0x%x",
                     (llcp_cb.lcb.link_state == LLCP_LINK_STATE_ACTIVATED)
                     ? llcp_cb.lcb.peer_opt & (LLCP_LSC_1 | LLCP_LSC_2) :0);

    if (llcp_cb.lcb.link_state == LLCP_LINK_STATE_ACTIVATED)
        return (llcp_cb.lcb.peer_opt & (LLCP_LSC_1 | LLCP_LSC_2));
    else
        return (LLCP_LSC_UNKNOWN);
}

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
LLCP_API void LLCP_GetLinkMIU (UINT16 *p_local_link_miu,
                               UINT16 *p_remote_link_miu)
{
    LLCP_TRACE_API0 ("LLCP_GetLinkMIU ()");

    if (llcp_cb.lcb.link_state == LLCP_LINK_STATE_ACTIVATED)
    {
        *p_local_link_miu  = llcp_cb.lcb.local_link_miu;
        *p_remote_link_miu = llcp_cb.lcb.effective_miu;
    }
    else
    {
        *p_local_link_miu  = 0;
        *p_remote_link_miu = 0;
    }

    LLCP_TRACE_DEBUG2 ("LLCP_GetLinkMIU (): local_link_miu = %d, remote_link_miu = %d",
                       *p_local_link_miu, *p_remote_link_miu);
}

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
tLLCP_STATUS LLCP_DiscoverService (char            *p_name,
                                   tLLCP_SDP_CBACK *p_cback,
                                   UINT8           *p_tid)
{
    tLLCP_STATUS  status;
    UINT8         i;

    LLCP_TRACE_API1 ("LLCP_DiscoverService () Service Name:%s",
                      p_name);

    if (llcp_cb.lcb.link_state != LLCP_LINK_STATE_ACTIVATED)
    {
        LLCP_TRACE_ERROR0 ("LLCP_DiscoverService (): Link is not activated");
        return LLCP_STATUS_FAIL;
    }

    if (!p_cback)
    {
        LLCP_TRACE_ERROR0 ("LLCP_DiscoverService (): Callback must be provided.");
        return LLCP_STATUS_FAIL;
    }

    /* if peer version is less than V1.1 then SNL is not supported */
    if ((llcp_cb.lcb.agreed_major_version == 0x01) && (llcp_cb.lcb.agreed_minor_version < 0x01))
    {
        LLCP_TRACE_ERROR0 ("LLCP_DiscoverService (): Peer doesn't support SNL");
        return LLCP_STATUS_FAIL;
    }

    for (i = 0; i < LLCP_MAX_SDP_TRANSAC; i++)
    {
        if (!llcp_cb.sdp_cb.transac[i].p_cback)
        {
            llcp_cb.sdp_cb.transac[i].tid = llcp_cb.sdp_cb.next_tid;
            llcp_cb.sdp_cb.next_tid++;
            llcp_cb.sdp_cb.transac[i].p_cback = p_cback;

            status = llcp_sdp_send_sdreq (llcp_cb.sdp_cb.transac[i].tid, p_name);

            if (status == LLCP_STATUS_FAIL)
            {
                llcp_cb.sdp_cb.transac[i].p_cback = NULL;
            }

            *p_tid = llcp_cb.sdp_cb.transac[i].tid;
            return (status);
        }
    }

    LLCP_TRACE_ERROR0 ("LLCP_DiscoverService (): Out of resource");

    return LLCP_STATUS_FAIL;
}
