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
 *  This file contains the main LLCP entry points
 *
 ******************************************************************************/

#include <string.h>
#include "gki.h"
#include "nfc_target.h"
#include "bt_types.h"
#include "llcp_api.h"
#include "llcp_int.h"
#include "llcp_defs.h"
#include "nfc_int.h"

#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
#include "nci_config.h"
#endif

#if (LLCP_DYNAMIC_MEMORY == FALSE)
tLLCP_CB llcp_cb;
#endif

/*******************************************************************************
**
** Function         llcp_init
**
** Description      This function is called once at startup to initialize
**                  all the LLCP structures
**
** Returns          void
**
*******************************************************************************/
void llcp_init (void)
{
    UINT32 pool_count;
    UINT32 p2p_flag;

    memset (&llcp_cb, 0, sizeof (tLLCP_CB));

    llcp_cb.trace_level = LLCP_INITIAL_TRACE_LEVEL;

    LLCP_TRACE_DEBUG0 ("LLCP - llcp_init ()");

    llcp_cb.lcb.local_link_miu = (LLCP_MIU <= LLCP_MAX_MIU ? LLCP_MIU : LLCP_MAX_MIU);
#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
    GetNumValue(NAME_LLCP_SECURED_P2P_ENABLE, &p2p_flag, sizeof(p2p_flag));
    if(p2p_flag)
        llcp_cb.lcb.local_opt      = LLCP_LSC_3|(LLCP_DPC_1 << 2);
    else
        llcp_cb.lcb.local_opt       = LLCP_LSC_3;
#else
    llcp_cb.lcb.local_opt      = LLCP_OPT_VALUE;
#endif
    llcp_cb.lcb.local_wt       = LLCP_WAITING_TIME;
    llcp_cb.lcb.local_lto      = LLCP_LTO_VALUE;

    llcp_cb.lcb.inact_timeout_init   = LLCP_INIT_INACTIVITY_TIMEOUT;
    llcp_cb.lcb.inact_timeout_target = LLCP_TARGET_INACTIVITY_TIMEOUT;
    llcp_cb.lcb.symm_delay           = LLCP_DELAY_RESP_TIME;
    llcp_cb.lcb.data_link_timeout    = LLCP_DATA_LINK_CONNECTION_TOUT;
    llcp_cb.lcb.delay_first_pdu_timeout = LLCP_DELAY_TIME_TO_SEND_FIRST_PDU;

    llcp_cb.lcb.wks  = LLCP_WKS_MASK_LM;

    /* total number of buffers for LLCP */
    pool_count = GKI_poolcount (LLCP_POOL_ID);

    /* number of buffers for receiving data */
    llcp_cb.num_rx_buff = (pool_count * LLCP_RX_BUFF_RATIO) / 100;

    /* rx congestion start/end threshold */
    llcp_cb.overall_rx_congest_start = (UINT8) ((llcp_cb.num_rx_buff * LLCP_RX_CONGEST_START) / 100);
    llcp_cb.overall_rx_congest_end   = (UINT8) ((llcp_cb.num_rx_buff * LLCP_RX_CONGEST_END) / 100);

    /* max number of buffers for receiving data on logical data link */
    llcp_cb.max_num_ll_rx_buff = (UINT8) ((llcp_cb.num_rx_buff * LLCP_LL_RX_BUFF_LIMIT) / 100);

    LLCP_TRACE_DEBUG4 ("num_rx_buff = %d, rx_congest_start = %d, rx_congest_end = %d, max_num_ll_rx_buff = %d",
                        llcp_cb.num_rx_buff, llcp_cb.overall_rx_congest_start,
                        llcp_cb.overall_rx_congest_end, llcp_cb.max_num_ll_rx_buff);

    /* max number of buffers for transmitting data */
    llcp_cb.max_num_tx_buff    = (UINT8) (pool_count - llcp_cb.num_rx_buff);

    /* max number of buffers for transmitting data on logical data link */
    llcp_cb.max_num_ll_tx_buff = (UINT8) ((llcp_cb.max_num_tx_buff * LLCP_LL_TX_BUFF_LIMIT) / 100);

    LLCP_TRACE_DEBUG2 ("max_num_tx_buff = %d, max_num_ll_tx_buff = %d",
                        llcp_cb.max_num_tx_buff, llcp_cb.max_num_ll_tx_buff);

    llcp_cb.ll_tx_uncongest_ntf_start_sap = LLCP_SAP_SDP + 1;

    LLCP_RegisterServer (LLCP_SAP_SDP, LLCP_LINK_TYPE_DATA_LINK_CONNECTION, "urn:nfc:sn:sdp", llcp_sdp_proc_data);
}

/*******************************************************************************
**
** Function         llcp_cleanup
**
** Description      This function is called once at closing to clean up
**
** Returns          void
**
*******************************************************************************/
void llcp_cleanup (void)
{
    UINT8 sap;
    tLLCP_APP_CB *p_app_cb;

    LLCP_TRACE_DEBUG0 ("LLCP - llcp_cleanup ()");

    for (sap = LLCP_SAP_SDP; sap < LLCP_NUM_SAPS; sap++)
    {
        p_app_cb = llcp_util_get_app_cb (sap);

        if (  (p_app_cb)
            &&(p_app_cb->p_app_cback)  )
        {
            LLCP_Deregister (sap);
        }
    }

    nfc_stop_quick_timer (&llcp_cb.lcb.inact_timer);
    nfc_stop_quick_timer (&llcp_cb.lcb.timer);
}

/*******************************************************************************
**
** Function         llcp_process_timeout
**
** Description      This function is called when an LLCP-related timeout occurs
**
** Returns          void
**
*******************************************************************************/
void llcp_process_timeout (TIMER_LIST_ENT *p_tle)
{
    UINT8 reason;

    LLCP_TRACE_DEBUG1 ("llcp_process_timeout: event=%d", p_tle->event);

    switch (p_tle->event)
    {
    case NFC_TTYPE_LLCP_LINK_MANAGER:
        /* Link timeout or Symm timeout */
        llcp_link_process_link_timeout ();
        break;

    case NFC_TTYPE_LLCP_LINK_INACT:
        /* inactivity timeout */
        llcp_link_deactivate (LLCP_LINK_LOCAL_INITIATED);
        break;

    case NFC_TTYPE_LLCP_DATA_LINK:
        reason = LLCP_SAP_DISCONNECT_REASON_TIMEOUT;
        llcp_dlsm_execute ((tLLCP_DLCB *) (p_tle->param), LLCP_DLC_EVENT_TIMEOUT, &reason);
        break;

    case NFC_TTYPE_LLCP_DELAY_FIRST_PDU:
            llcp_link_check_send_data ();
        break;
#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
    case NFC_TTYPE_LLCP_DPS_PDU:
         LLCP_TRACE_DEBUG0("dps pdu timeout");//need to be defined
        break;
#endif
    default:
        break;
    }
}

/*******************************************************************************
**
** Function         LLCP_SetTraceLevel
**
** Description      This function sets the trace level for LLCP.  If called with
**                  a value of 0xFF, it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 LLCP_SetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        llcp_cb.trace_level = new_level;

    return (llcp_cb.trace_level);
}
