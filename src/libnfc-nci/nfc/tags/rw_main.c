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
#include "rw_api.h"
#include "rw_int.h"

tRW_CB rw_cb;
/*******************************************************************************
*******************************************************************************/
void rw_init (void)
{
    memset (&rw_cb, 0, sizeof (tRW_CB));
    rw_cb.trace_level = NFC_INITIAL_TRACE_LEVEL;

}

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
/*******************************************************************************
* Internal functions for statistics
*******************************************************************************/
/*******************************************************************************
**
** Function         rw_main_reset_stats
**
** Description      Reset counters for statistics
**
** Returns          void
**
*******************************************************************************/
void rw_main_reset_stats (void)
{
    memset (&rw_cb.stats, 0, sizeof (tRW_STATS));

    /* Get current tick count */
    rw_cb.stats.start_tick = GKI_get_tick_count ();
}

/*******************************************************************************
**
** Function         rw_main_update_tx_stats
**
** Description      Update stats for tx
**
** Returns          void
**
*******************************************************************************/
void rw_main_update_tx_stats (UINT32 num_bytes, BOOLEAN is_retry)
{
    rw_cb.stats.bytes_sent+=num_bytes;
    rw_cb.stats.num_ops++;

    if (is_retry)
        rw_cb.stats.num_retries++;
}

/*******************************************************************************
**
** Function         rw_main_update_fail_stats
**
** Description      Increment failure count
**
** Returns          void
**
*******************************************************************************/
void rw_main_update_fail_stats (void)
{
    rw_cb.stats.num_fail++;
}

/*******************************************************************************
**
** Function         rw_main_update_crc_error_stats
**
** Description      Increment crc error count
**
** Returns          void
**
*******************************************************************************/
void rw_main_update_crc_error_stats (void)
{
    rw_cb.stats.num_crc++;
}

/*******************************************************************************
**
** Function         rw_main_update_trans_error_stats
**
** Description      Increment trans error count
**
** Returns          void
**
*******************************************************************************/
void rw_main_update_trans_error_stats (void)
{
    rw_cb.stats.num_trans_err++;
}

/*******************************************************************************
**
** Function         rw_main_update_rx_stats
**
** Description      Update stats for rx
**
** Returns          void
**
*******************************************************************************/
void rw_main_update_rx_stats (UINT32 num_bytes)
{
    rw_cb.stats.bytes_received+=num_bytes;
}

/*******************************************************************************
**
** Function         rw_main_log_stats
**
** Description      Dump stats
**
** Returns          void
**
*******************************************************************************/
void rw_main_log_stats (void)
{
    UINT32 ticks, elapsed_ms;

    ticks = GKI_get_tick_count () - rw_cb.stats.start_tick;
    elapsed_ms = GKI_TICKS_TO_MS (ticks);

    RW_TRACE_DEBUG5 ("NFC tx stats: cmds:%i, retries:%i, aborted: %i, tx_errs: %i, bytes sent:%i", rw_cb.stats.num_ops, rw_cb.stats.num_retries, rw_cb.stats.num_fail, rw_cb.stats.num_trans_err, rw_cb.stats.bytes_sent);
    RW_TRACE_DEBUG2 ("    rx stats: rx-crc errors %i, bytes received: %i", rw_cb.stats.num_crc, rw_cb.stats.bytes_received);
    RW_TRACE_DEBUG1 ("    time activated %i ms", elapsed_ms);
}
#endif  /* RW_STATS_INCLUDED */


/*******************************************************************************
**
** Function         RW_SendRawFrame
**
** Description      This function sends a raw frame to the peer device.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_SendRawFrame (UINT8 *p_raw_data, UINT16 data_len)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;
    BT_HDR  *p_data;
    UINT8   *p;

    if (rw_cb.p_cback)
    {
        /* a valid opcode for RW - remove */
        p_data = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID);
        if (p_data)
        {
            p_data->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
            p = (UINT8 *) (p_data + 1) + p_data->offset;
            memcpy (p, p_raw_data, data_len);
            p_data->len = data_len;

            RW_TRACE_EVENT1 ("RW SENT raw frame (0x%x)", data_len);
            status = NFC_SendData (NFC_RF_CONN_ID, p_data);
        }

    }
    return status;
}

/*******************************************************************************
**
** Function         RW_SetActivatedTagType
**
** Description      This function selects the tag type for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS RW_SetActivatedTagType (tNFC_ACTIVATE_DEVT *p_activate_params, tRW_CBACK *p_cback)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;

    /* check for null cback here / remove checks from rw_t?t */
    RW_TRACE_DEBUG3 ("RW_SetActivatedTagType protocol:%d, technology:%d, SAK:%d", p_activate_params->protocol, p_activate_params->rf_tech_param.mode, p_activate_params->rf_tech_param.param.pa.sel_rsp);

    if (p_cback == NULL)
    {
        RW_TRACE_ERROR0 ("RW_SetActivatedTagType called with NULL callback");
        return (NFC_STATUS_FAILED);
    }

    /* Reset tag-specific area of control block */
    memset (&rw_cb.tcb, 0, sizeof (tRW_TCB));

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
    /* Reset RW stats */
    rw_main_reset_stats ();
#endif  /* RW_STATS_INCLUDED */

    rw_cb.p_cback = p_cback;
    switch (p_activate_params->protocol)
    {
    /* not a tag NFC_PROTOCOL_NFCIP1:   NFCDEP/LLCP - NFC-A or NFC-F */
    case NFC_PROTOCOL_T1T:    /* Type1Tag    - NFC-A */
        if (p_activate_params->rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            status = rw_t1t_select (p_activate_params->rf_tech_param.param.pa.hr,
                                    p_activate_params->rf_tech_param.param.pa.nfcid1);
        }
        break;

    case NFC_PROTOCOL_T2T:   /* Type2Tag    - NFC-A */
        if (p_activate_params->rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            if (p_activate_params->rf_tech_param.param.pa.sel_rsp == NFC_SEL_RES_NFC_FORUM_T2T)
                status      = rw_t2t_select ();
        }
        break;

    case NFC_PROTOCOL_T3T:   /* Type3Tag    - NFC-F */
        if (p_activate_params->rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_F)
        {
            status = rw_t3t_select (p_activate_params->rf_tech_param.param.pf.nfcid2,
                                    p_activate_params->rf_tech_param.param.pf.mrti_check,
                                    p_activate_params->rf_tech_param.param.pf.mrti_update);
        }
        break;

    case NFC_PROTOCOL_ISO_DEP:     /* ISODEP/4A,4B- NFC-A or NFC-B */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    case NFC_PROTOCOL_T3BT:
#endif
        if (  (p_activate_params->rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B)
            ||(p_activate_params->rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A)  )
        {
            status          = rw_t4t_select ();
        }
        break;

    case NFC_PROTOCOL_15693:     /* ISO 15693 */
        if (p_activate_params->rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_ISO15693)
        {
            status          = rw_i93_select (p_activate_params->rf_tech_param.param.pi93.uid);
        }
        break;
    /* TODO set up callback for proprietary protocol */

    default:
        RW_TRACE_ERROR0 ("RW_SetActivatedTagType Invalid protocol");
    }

    if (status != NFC_STATUS_OK)
        rw_cb.p_cback = NULL;
    return status;
}

/*******************************************************************************
**
** Function         RW_SetTraceLevel
**
** Description      This function sets the trace level for Reader/Writer mode.
**                  If called with a value of 0xFF,
**                  it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 RW_SetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        rw_cb.trace_level = new_level;

    return (rw_cb.trace_level);
}

#endif /* NFC_INCLUDED == TRUE */
