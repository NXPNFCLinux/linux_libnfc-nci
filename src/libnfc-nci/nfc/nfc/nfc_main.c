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
 *  This file contains functions that interface with the NFC NCI transport.
 *  On the receive side, it routes events to the appropriate handler
 *  (callback). On the transmit side, it manages the command transmission.
 *
 ******************************************************************************/
#include <string.h>
#include "gki.h"
#include "nfc_target.h"
#include "bt_types.h"
#include "hcidefs.h"
#include <phNxpNciHal.h>

#if (NFC_INCLUDED == TRUE)
#include "nfc_hal_api.h"
#include "nfc_api.h"
#include "nfc_int.h"
#include "nci_hmsgs.h"
#include "rw_int.h"
#include "ce_int.h"


#if (NFC_RW_ONLY == FALSE)
#include "ce_api.h"
#include "ce_int.h"
#include "llcp_int.h"

/* NFC mandates support for at least one logical connection;
 * Update max_conn to the NFCC capability on InitRsp */
#define NFC_SET_MAX_CONN_DEFAULT()    {nfc_cb.max_conn = 1;}

#else /* NFC_RW_ONLY */
#define ce_init()
#define llcp_init()

#define NFC_SET_MAX_CONN_DEFAULT()

#endif /* NFC_RW_ONLY */
/****************************************************************************
** Declarations
****************************************************************************/
#if NFC_DYNAMIC_MEMORY == FALSE
tNFC_CB nfc_cb;
#endif
UINT8 i2c_fragmentation_enabled = 0xff;
#if (NFC_RW_ONLY == FALSE)
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define NFC_NUM_INTERFACE_MAP   4
#else
#define NFC_NUM_INTERFACE_MAP   3
#endif /* (NFC_NXP_NOT_OPEN_INCLUDED == TRUE) */
#else
#define NFC_NUM_INTERFACE_MAP   1
#endif /* (NFC_RW_ONLY == FALSE) */

static const tNCI_DISCOVER_MAPS nfc_interface_mapping[NFC_NUM_INTERFACE_MAP] =
{
    /* Protocols that use Frame Interface do not need to be included in the interface mapping */
    {
        NCI_PROTOCOL_ISO_DEP,
        NCI_INTERFACE_MODE_POLL_N_LISTEN,
        NCI_INTERFACE_ISO_DEP
    }
#if (NFC_RW_ONLY == FALSE)
    ,
    /* this can not be set here due to 2079xB0 NFCC issues */
    {
        NCI_PROTOCOL_NFC_DEP,
        NCI_INTERFACE_MODE_POLL_N_LISTEN,
        NCI_INTERFACE_NFC_DEP
    }
#endif
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    ,
    {
        NCI_PROTOCOL_MIFARE,
        NCI_INTERFACE_MODE_POLL,
        NCI_INTERFACE_MIFARE
    }
    ,
    /* This mapping is for Felica on DH  */
    {
        NCI_PROTOCOL_T3T,
        NCI_INTERFACE_MODE_LISTEN,
        NCI_INTERFACE_FRAME
    }
#endif
};


#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         nfc_state_name
**
** Description      This function returns the state name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *nfc_state_name (UINT8 state)
{
    switch (state)
    {
    case NFC_STATE_NONE:
        return ("NONE");
    case NFC_STATE_W4_HAL_OPEN:
        return ("W4_HAL_OPEN");
    case NFC_STATE_CORE_INIT:
        return ("CORE_INIT");
    case NFC_STATE_W4_POST_INIT_CPLT:
        return ("W4_POST_INIT_CPLT");
    case NFC_STATE_IDLE:
        return ("IDLE");
    case NFC_STATE_OPEN:
        return ("OPEN");
    case NFC_STATE_CLOSING:
        return ("CLOSING");
    case NFC_STATE_W4_HAL_CLOSE:
        return ("W4_HAL_CLOSE");
    case NFC_STATE_NFCC_POWER_OFF_SLEEP:
        return ("NFCC_POWER_OFF_SLEEP");
    default:
        return ("???? UNKNOWN STATE");
    }
}

/*******************************************************************************
**
** Function         nfc_hal_event_name
**
** Description      This function returns the HAL event name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
static char *nfc_hal_event_name (UINT8 event)
{
    switch (event)
    {
    case HAL_NFC_OPEN_CPLT_EVT:
        return ("HAL_NFC_OPEN_CPLT_EVT");

    case HAL_NFC_CLOSE_CPLT_EVT:
        return ("HAL_NFC_CLOSE_CPLT_EVT");

    case HAL_NFC_POST_INIT_CPLT_EVT:
        return ("HAL_NFC_POST_INIT_CPLT_EVT");

    case HAL_NFC_PRE_DISCOVER_CPLT_EVT:
        return ("HAL_NFC_PRE_DISCOVER_CPLT_EVT");

    case HAL_NFC_REQUEST_CONTROL_EVT:
        return ("HAL_NFC_REQUEST_CONTROL_EVT");

    case HAL_NFC_RELEASE_CONTROL_EVT:
        return ("HAL_NFC_RELEASE_CONTROL_EVT");

    case HAL_NFC_ERROR_EVT:
        return ("HAL_NFC_ERROR_EVT");

    case HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT:
        return (" HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT ");

    default:
        return ("???? UNKNOWN EVENT");
    }
}
#endif /* BT_TRACE_VERBOSE == TRUE */

/*******************************************************************************
**
** Function         nfc_main_notify_enable_status
**
** Description      Notify status of Enable/PowerOffSleep/PowerCycle
**
*******************************************************************************/
static void nfc_main_notify_enable_status (tNFC_STATUS nfc_status)
{
    tNFC_RESPONSE   evt_data;

    evt_data.status = nfc_status;

    if (nfc_cb.p_resp_cback)
    {
        /* if getting out of PowerOffSleep mode or restarting NFCC */
        if (nfc_cb.flags & (NFC_FL_RESTARTING|NFC_FL_POWER_CYCLE_NFCC))
        {
            nfc_cb.flags &= ~(NFC_FL_RESTARTING|NFC_FL_POWER_CYCLE_NFCC);
            if (nfc_status != NFC_STATUS_OK)
            {
                nfc_cb.flags |= NFC_FL_POWER_OFF_SLEEP;
            }
            (*nfc_cb.p_resp_cback) (NFC_NFCC_RESTART_REVT, &evt_data);
        }
        else
        {
            (*nfc_cb.p_resp_cback) (NFC_ENABLE_REVT, &evt_data);
        }
    }
}



/*******************************************************************************
**
** Function         nfc_enabled
**
** Description      NFCC enabled, proceed with stack start up.
**
** Returns          void
**
*******************************************************************************/
void nfc_enabled (tNFC_STATUS nfc_status, BT_HDR *p_init_rsp_msg)
{
    tNFC_RESPONSE evt_data;
    tNFC_CONN_CB  *p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
    UINT8   *p;
    UINT8   num_interfaces = 0, xx;
    int     yy = 0;

    memset (&evt_data, 0, sizeof (tNFC_RESPONSE));

    if (nfc_status == NCI_STATUS_OK)
    {
        nfc_set_state (NFC_STATE_IDLE);

        p = (UINT8 *) (p_init_rsp_msg + 1) + p_init_rsp_msg->offset + NCI_MSG_HDR_SIZE + 1;
        /* we currently only support NCI of the same version.
        * We may need to change this, when we support multiple version of NFCC */
        evt_data.enable.nci_version = NCI_VERSION;
        STREAM_TO_UINT32 (evt_data.enable.nci_features, p);
        STREAM_TO_UINT8 (num_interfaces, p);

        evt_data.enable.nci_interfaces = 0;
        for (xx = 0; xx < num_interfaces; xx++)
        {
            if ((*p) <= NCI_INTERFACE_MAX)
                evt_data.enable.nci_interfaces |= (1 << (*p));
            #if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            else if (((*p) >= NCI_INTERFACE_FIRST_VS) && (yy < NFC_NFCC_MAX_NUM_VS_INTERFACE))
            #else
            else if (((*p) > NCI_INTERFACE_FIRST_VS) && (yy < NFC_NFCC_MAX_NUM_VS_INTERFACE))
            #endif
            {
                /* save the VS RF interface in control block, if there's still room */
                nfc_cb.vs_interface[yy++] = *p;
            }
            p++;
        }
        nfc_cb.nci_interfaces    = evt_data.enable.nci_interfaces;
        memcpy (evt_data.enable.vs_interface, nfc_cb.vs_interface, NFC_NFCC_MAX_NUM_VS_INTERFACE);
        evt_data.enable.max_conn = *p++;
        STREAM_TO_UINT16 (evt_data.enable.max_ce_table, p);
#if (NFC_RW_ONLY == FALSE)
        nfc_cb.max_ce_table          = evt_data.enable.max_ce_table;
        nfc_cb.nci_features          = evt_data.enable.nci_features;
        nfc_cb.max_conn              = evt_data.enable.max_conn;
#endif
        nfc_cb.nci_ctrl_size         = *p++; /* Max Control Packet Payload Length */
        p_cb->init_credits           = p_cb->num_buff = 0;
        STREAM_TO_UINT16 (evt_data.enable.max_param_size, p);
        nfc_set_conn_id (p_cb, NFC_RF_CONN_ID);
        evt_data.enable.manufacture_id   = *p++;
        STREAM_TO_ARRAY (evt_data.enable.nfcc_info, p, NFC_NFCC_INFO_LEN);
        NFC_DiscoveryMap (nfc_cb.num_disc_maps, (tNCI_DISCOVER_MAPS *) nfc_cb.p_disc_maps, NULL);
    }
    /* else not successful. the buffers will be freed in nfc_free_conn_cb () */
    else
    {
        if (nfc_cb.flags & NFC_FL_RESTARTING)
        {
            nfc_set_state (NFC_STATE_NFCC_POWER_OFF_SLEEP);
        }
        else
        {
            nfc_free_conn_cb (p_cb);

            /* if NFCC didn't respond to CORE_RESET or CORE_INIT */
            if (nfc_cb.nfc_state == NFC_STATE_CORE_INIT)
            {
                /* report status after closing HAL */
                nfc_cb.p_hal->close ();
                return;
            }
            else
                nfc_set_state (NFC_STATE_NONE);
        }
    }

    nfc_main_notify_enable_status (nfc_status);
}

/*******************************************************************************
**
** Function         nfc_set_state
**
** Description      Set the state of NFC stack
**
** Returns          void
**
*******************************************************************************/
void nfc_set_state (tNFC_STATE nfc_state)
{
#if (BT_TRACE_VERBOSE == TRUE)
    NFC_TRACE_DEBUG4 ("nfc_set_state %d (%s)->%d (%s)", nfc_cb.nfc_state, nfc_state_name (nfc_cb.nfc_state), nfc_state, nfc_state_name (nfc_state));
#else
    NFC_TRACE_DEBUG2 ("nfc_set_state %d->%d", nfc_cb.nfc_state, nfc_state);
#endif
    nfc_cb.nfc_state = nfc_state;
}

/*******************************************************************************
**
** Function         nfc_gen_cleanup
**
** Description      Clean up for both going into low power mode and disabling NFC
**
*******************************************************************************/
void nfc_gen_cleanup (void)
{
    nfc_cb.flags  &= ~NFC_FL_DEACTIVATING;

    /* the HAL pre-discover is still active - clear the pending flag/free the buffer */
    if (nfc_cb.flags & NFC_FL_DISCOVER_PENDING)
    {
        nfc_cb.flags &= ~NFC_FL_DISCOVER_PENDING;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if(nfc_cb.p_last_disc)
        {
            GKI_freebuf (nfc_cb.p_last_disc);
            nfc_cb.p_last_disc = NULL;
        }
        nfc_cb.p_last_disc = nfc_cb.p_disc_pending;
#else
        GKI_freebuf (nfc_cb.p_disc_pending);
#endif
        nfc_cb.p_disc_pending = NULL;
    }

    nfc_cb.flags &= ~(NFC_FL_CONTROL_REQUESTED | NFC_FL_CONTROL_GRANTED | NFC_FL_HAL_REQUESTED);

    nfc_stop_timer (&nfc_cb.deactivate_timer);

    /* Reset the connection control blocks */
    nfc_reset_all_conn_cbs ();

    if (nfc_cb.p_nci_init_rsp)
    {
        GKI_freebuf (nfc_cb.p_nci_init_rsp);
        nfc_cb.p_nci_init_rsp = NULL;
    }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(NULL != nfc_cb.p_last_disc)
    {
        GKI_freebuf(nfc_cb.p_last_disc);
        nfc_cb.p_last_disc = NULL;
    }
#endif
    /* clear any pending CMD/RSP */
    nfc_main_flush_cmd_queue ();
}

/*******************************************************************************
**
** Function         nfc_main_handle_hal_evt
**
** Description      Handle BT_EVT_TO_NFC_MSGS
**
*******************************************************************************/
void nfc_main_handle_hal_evt (tNFC_HAL_EVT_MSG *p_msg)
{
    UINT8  *ps;

    NFC_TRACE_DEBUG1 ("nfc_main_handle_hal_evt(): HAL event=0x%x", p_msg->hal_evt);

    switch (p_msg->hal_evt)
    {
    case HAL_NFC_OPEN_CPLT_EVT: /* only for failure case */
        nfc_enabled (NFC_STATUS_FAILED, NULL);
        break;

    case HAL_NFC_CLOSE_CPLT_EVT:
        if (nfc_cb.p_resp_cback)
        {
            if (nfc_cb.nfc_state == NFC_STATE_W4_HAL_CLOSE)
            {
                if (nfc_cb.flags & NFC_FL_POWER_OFF_SLEEP)
                {
                    nfc_cb.flags &= ~NFC_FL_POWER_OFF_SLEEP;
                    nfc_set_state (NFC_STATE_NFCC_POWER_OFF_SLEEP);
                    (*nfc_cb.p_resp_cback) (NFC_NFCC_POWER_OFF_REVT, NULL);
                }
                else
                {
                    nfc_set_state (NFC_STATE_NONE);
                    (*nfc_cb.p_resp_cback) (NFC_DISABLE_REVT, NULL);
                    nfc_cb.p_resp_cback = NULL;
                }
            }
            else
            {
                /* found error during initialization */
                nfc_set_state (NFC_STATE_NONE);
                nfc_main_notify_enable_status (NFC_STATUS_FAILED);
            }
        }
        break;

    case HAL_NFC_POST_INIT_CPLT_EVT:
        if (nfc_cb.p_nci_init_rsp)
        {
            /*
            ** if NFC_Disable() is called before receiving HAL_NFC_POST_INIT_CPLT_EVT,
            ** then wait for HAL_NFC_CLOSE_CPLT_EVT.
            */
            if (nfc_cb.nfc_state == NFC_STATE_W4_POST_INIT_CPLT)
            {
                if (p_msg->status == HAL_NFC_STATUS_OK)
                {
                    nfc_enabled (NCI_STATUS_OK, nfc_cb.p_nci_init_rsp);
                }
                else /* if post initailization failed */
                {
                    nfc_enabled (NCI_STATUS_FAILED, NULL);
                }
            }

            GKI_freebuf (nfc_cb.p_nci_init_rsp);
            nfc_cb.p_nci_init_rsp = NULL;
        }
        break;

    case HAL_NFC_PRE_DISCOVER_CPLT_EVT:
        /* restore the command window, no matter if the discover command is still pending */
        nfc_cb.nci_cmd_window = NCI_MAX_CMD_WINDOW;
        nfc_cb.flags         &= ~NFC_FL_CONTROL_GRANTED;
        if (nfc_cb.flags & NFC_FL_DISCOVER_PENDING)
        {
            /* issue the discovery command now, if it is still pending */
            nfc_cb.flags &= ~NFC_FL_DISCOVER_PENDING;
            ps            = (UINT8 *)nfc_cb.p_disc_pending;
            nci_snd_discover_cmd (*ps, (tNFC_DISCOVER_PARAMS *)(ps + 1));
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            if(nfc_cb.p_last_disc)
            {
                GKI_freebuf( nfc_cb.p_last_disc);
                nfc_cb.p_last_disc = NULL;
            }
            nfc_cb.p_last_disc = nfc_cb.p_disc_pending;
#else
            GKI_freebuf (nfc_cb.p_disc_pending);
#endif
            nfc_cb.p_disc_pending = NULL;
        }
        else
        {
            /* check if there's other pending commands */
            nfc_ncif_check_cmd_queue (NULL);
        }

        if (p_msg->status == HAL_NFC_STATUS_ERR_CMD_TIMEOUT)
            nfc_ncif_event_status (NFC_NFCC_TIMEOUT_REVT, NFC_STATUS_HW_TIMEOUT);
        break;

    case HAL_NFC_REQUEST_CONTROL_EVT:
        nfc_cb.flags    |= NFC_FL_CONTROL_REQUESTED;
        nfc_cb.flags    |= NFC_FL_HAL_REQUESTED;
        nfc_ncif_check_cmd_queue (NULL);
        break;

    case HAL_NFC_RELEASE_CONTROL_EVT:
        if (nfc_cb.flags & NFC_FL_CONTROL_GRANTED)
        {
            nfc_cb.flags &= ~NFC_FL_CONTROL_GRANTED;
            nfc_cb.nci_cmd_window = NCI_MAX_CMD_WINDOW;
            nfc_ncif_check_cmd_queue (NULL);

            if (p_msg->status == HAL_NFC_STATUS_ERR_CMD_TIMEOUT)
                nfc_ncif_event_status (NFC_NFCC_TIMEOUT_REVT, NFC_STATUS_HW_TIMEOUT);
        }
        break;

    case HAL_NFC_ERROR_EVT:
        switch (p_msg->status)
        {
        case HAL_NFC_STATUS_ERR_TRANSPORT:
            /* Notify app of transport error */
            if (nfc_cb.p_resp_cback)
            {
                (*nfc_cb.p_resp_cback) (NFC_NFCC_TRANSPORT_ERR_REVT, NULL);

                /* if enabling NFC, notify upper layer of failure after closing HAL */
                if (nfc_cb.nfc_state < NFC_STATE_IDLE)
                {
                    nfc_enabled (NFC_STATUS_FAILED, NULL);
                }
            }
            break;

        case HAL_NFC_STATUS_ERR_CMD_TIMEOUT:
            nfc_ncif_event_status (NFC_NFCC_TIMEOUT_REVT, NFC_STATUS_HW_TIMEOUT);

            /* if enabling NFC, notify upper layer of failure after closing HAL */
            if (nfc_cb.nfc_state < NFC_STATE_IDLE)
            {
                nfc_enabled (NFC_STATUS_FAILED, NULL);
                return;
            }
            break;

        default:
            break;
        }
        break;

    default:
        NFC_TRACE_ERROR1 ("nfc_main_handle_hal_evt (): unhandled event (0x%x).", p_msg->hal_evt);
        break;
    }
}


/*******************************************************************************
**
** Function         nfc_main_flush_cmd_queue
**
** Description      This function is called when setting power off sleep state.
**
** Returns          void
**
*******************************************************************************/
void nfc_main_flush_cmd_queue (void)
{
    BT_HDR *p_msg;

    NFC_TRACE_DEBUG0 ("nfc_main_flush_cmd_queue ()");

    /* initialize command window */
    nfc_cb.nci_cmd_window = NCI_MAX_CMD_WINDOW;

    /* Stop command-pending timer */
    nfc_stop_timer(&nfc_cb.nci_wait_rsp_timer);

    /* dequeue and free buffer */
    while ((p_msg = (BT_HDR *)GKI_dequeue (&nfc_cb.nci_cmd_xmit_q)) != NULL)
    {
        GKI_freebuf (p_msg);
    }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(NULL != nfc_cb.last_cmd_buf)
    {
        GKI_freebuf (nfc_cb.last_cmd_buf);
        nfc_cb.last_cmd_buf = NULL;
    }
#endif
}

/*******************************************************************************
**
** Function         nfc_main_post_hal_evt
**
** Description      This function posts HAL event to NFC_TASK
**
** Returns          void
**
*******************************************************************************/
void nfc_main_post_hal_evt (UINT8 hal_evt, tHAL_NFC_STATUS status)
{
    tNFC_HAL_EVT_MSG *p_msg;

    if ((p_msg = (tNFC_HAL_EVT_MSG *) GKI_getbuf (sizeof(tNFC_HAL_EVT_MSG))) != NULL)
    {
        /* Initialize BT_HDR */
        p_msg->hdr.len    = 0;
        p_msg->hdr.event  = BT_EVT_TO_NFC_MSGS;
        p_msg->hdr.offset = 0;
        p_msg->hdr.layer_specific = 0;
        p_msg->hal_evt = hal_evt;
        p_msg->status  = status;
        GKI_send_msg (NFC_TASK, NFC_MBOX_ID, p_msg);
    }
    else
    {
        NFC_TRACE_ERROR0 ("nfc_main_post_hal_evt (): No buffer");
    }
}


/*******************************************************************************
**
** Function         nfc_main_hal_cback
**
** Description      HAL event handler
**
** Returns          void
**
*******************************************************************************/
static void nfc_main_hal_cback(UINT8 event, tHAL_NFC_STATUS status)
{
#if (BT_TRACE_VERBOSE == TRUE)
    NFC_TRACE_DEBUG3 ("nfc_main_hal_cback event: %s(0x%x), status=%d",
                       nfc_hal_event_name (event), event, status);
#else
    NFC_TRACE_DEBUG2 ("nfc_main_hal_cback event: 0x%x, status=%d", event, status);
#endif

    switch (event)
    {
    case HAL_NFC_OPEN_CPLT_EVT:
        /*
        ** if NFC_Disable() is called before receiving HAL_NFC_OPEN_CPLT_EVT,
        ** then wait for HAL_NFC_CLOSE_CPLT_EVT.
        */
        if (nfc_cb.nfc_state == NFC_STATE_W4_HAL_OPEN)
        {
            if (status == HAL_NFC_STATUS_OK)
            {
                /* Notify NFC_TASK that NCI tranport is initialized */
                GKI_send_event (NFC_TASK, NFC_TASK_EVT_TRANSPORT_READY);
            }
            else
            {
                nfc_main_post_hal_evt (event, status);
            }
        }
        break;

    case HAL_NFC_CLOSE_CPLT_EVT:
    case HAL_NFC_POST_INIT_CPLT_EVT:
    case HAL_NFC_PRE_DISCOVER_CPLT_EVT:
    case HAL_NFC_REQUEST_CONTROL_EVT:
    case HAL_NFC_RELEASE_CONTROL_EVT:
    case HAL_NFC_ERROR_EVT:
        nfc_main_post_hal_evt (event, status);
        break;
    case HAL_NFC_ENABLE_I2C_FRAGMENTATION_EVT:
    {
        NFC_TRACE_DEBUG1 ("nfc_main_hal_cback handled  event  %x", event);
        set_i2c_fragmentation_enabled(I2C_FRAGMENATATION_ENABLED);
    }
        break;
    default:
        NFC_TRACE_DEBUG1 ("nfc_main_hal_cback unhandled event %x", event);
        break;
    }
}

/*******************************************************************************
**
** Function         nfc_main_hal_data_cback
**
** Description      HAL data event handler
**
** Returns          void
**
*******************************************************************************/
static void nfc_main_hal_data_cback(UINT16 data_len, UINT8   *p_data)
{
    BT_HDR *p_msg;

    /* ignore all data while shutting down NFCC */
    if (nfc_cb.nfc_state == NFC_STATE_W4_HAL_CLOSE)
    {
        return;
    }

    if (p_data)
    {
        if ((p_msg = (BT_HDR *) GKI_getpoolbuf (NFC_NCI_POOL_ID)) != NULL)
        {
            /* Initialize BT_HDR */
            p_msg->len    = data_len;
            p_msg->event  = BT_EVT_TO_NFC_NCI;
            p_msg->offset = NFC_RECEIVE_MSGS_OFFSET;

            /* no need to check length, it always less than pool size */
            memcpy ((UINT8 *)(p_msg + 1) + p_msg->offset, p_data, p_msg->len);

            GKI_send_msg (NFC_TASK, NFC_MBOX_ID, p_msg);
        }
        else
        {
            NFC_TRACE_ERROR0 ("nfc_main_hal_data_cback (): No buffer");
        }
    }
}

/*******************************************************************************
**
** Function         NFC_Enable
**
** Description      This function enables NFC. Prior to calling NFC_Enable:
**                  - the NFCC must be powered up, and ready to receive commands.
**                  - GKI must be enabled
**                  - NFC_TASK must be started
**                  - NCIT_TASK must be started (if using dedicated NCI transport)
**
**                  This function opens the NCI transport (if applicable),
**                  resets the NFC controller, and initializes the NFC subsystems.
**
**                  When the NFC startup procedure is completed, an
**                  NFC_ENABLE_REVT is returned to the application using the
**                  tNFC_RESPONSE_CBACK.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_Enable (tNFC_RESPONSE_CBACK *p_cback)
{
    NFC_TRACE_API0 ("NFC_Enable ()");

    /* Validate callback */
    if (!p_cback)
    {
        return (NFC_STATUS_INVALID_PARAM);
    }
    nfc_cb.p_resp_cback = p_cback;

    /* Open HAL transport. */
    nfc_set_state (NFC_STATE_W4_HAL_OPEN);
    nfc_cb.p_hal->open (nfc_main_hal_cback, nfc_main_hal_data_cback);
    /* Moved from the init to Enable as the HAL open is just performed above */
    if (phNxpNciHal_getChipType() == pn547C2)
    {
        /* Reduce the interface mapping size since Felica interface
         * is not supported in pn547
         */
        (nfc_cb.num_disc_maps)--;
        NFC_TRACE_API1 ("NFC_Enable (): nfc_cb.num_disc_maps = %d", nfc_cb.num_disc_maps);
    }

    return (NFC_STATUS_OK);
}

/*******************************************************************************
**
** Function         NFC_Disable
**
** Description      This function performs clean up routines for shutting down
**                  NFC and closes the NCI transport (if using dedicated NCI
**                  transport).
**
**                  When the NFC shutdown procedure is completed, an
**                  NFC_DISABLED_REVT is returned to the application using the
**                  tNFC_RESPONSE_CBACK.
**
** Returns          nothing
**
*******************************************************************************/
void NFC_Disable (void)
{
    NFC_TRACE_API1 ("NFC_Disable (): nfc_state = %d", nfc_cb.nfc_state);

    if ((nfc_cb.nfc_state == NFC_STATE_NONE) || (nfc_cb.nfc_state == NFC_STATE_NFCC_POWER_OFF_SLEEP))
    {
        nfc_set_state (NFC_STATE_NONE);
        if (nfc_cb.p_resp_cback)
        {
            (*nfc_cb.p_resp_cback) (NFC_DISABLE_REVT, NULL);
            nfc_cb.p_resp_cback = NULL;
        }
        return;
    }

    /* Close transport and clean up */
    nfc_task_shutdown_nfcc ();
}

/*******************************************************************************
**
** Function         NFC_Init
**
** Description      This function initializes control block for NFC
**
** Returns          nothing
**
*******************************************************************************/
void NFC_Init (tHAL_NFC_ENTRY *p_hal_entry_tbl)
{
    int xx;

    NFC_TRACE_API0 ("NFC_Init ()");

    /* Clear nfc control block */
    memset (&nfc_cb, 0, sizeof (tNFC_CB));

    /* Reset the nfc control block */
    for (xx = 0; xx < NCI_MAX_CONN_CBS; xx++)
    {
        nfc_cb.conn_cb[xx].conn_id = NFC_ILLEGAL_CONN_ID;
    }

    /* NCI init */
    nfc_cb.p_hal            = p_hal_entry_tbl;
    nfc_cb.nfc_state        = NFC_STATE_NONE;
    nfc_cb.nci_cmd_window   = NCI_MAX_CMD_WINDOW;
    nfc_cb.nci_wait_rsp_tout= NFC_CMD_CMPL_TIMEOUT;
    nfc_cb.p_disc_maps      = nfc_interface_mapping;
    nfc_cb.trace_level      = NFC_INITIAL_TRACE_LEVEL;
    nfc_cb.nci_ctrl_size    = NCI_CTRL_INIT_SIZE;
    nfc_cb.reassembly       = TRUE;
    nfc_cb.num_disc_maps    = NFC_NUM_INTERFACE_MAP;

    rw_init ();
    ce_init ();
    llcp_init ();
    NFC_SET_MAX_CONN_DEFAULT ();
}

/*******************************************************************************
**
** Function         NFC_GetLmrtSize
**
** Description      Called by application wto query the Listen Mode Routing
**                  Table size supported by NFCC
**
** Returns          Listen Mode Routing Table size
**
*******************************************************************************/
UINT16 NFC_GetLmrtSize (void)
{
    UINT16 size = 0;
#if (NFC_RW_ONLY == FALSE)
    size = nfc_cb.max_ce_table;
#endif
    return size;
}


/*******************************************************************************
**
** Function         NFC_SetConfig
**
** Description      This function is called to send the configuration parameter
**                  TLV to NFCC. The response from NFCC is reported by
**                  tNFC_RESPONSE_CBACK as NFC_SET_CONFIG_REVT.
**
** Parameters       tlv_size - the length of p_param_tlvs.
**                  p_param_tlvs - the parameter ID/Len/Value list
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_SetConfig (UINT8     tlv_size,
                           UINT8    *p_param_tlvs)
{
    return nci_snd_core_set_config (p_param_tlvs, tlv_size);
}

/*******************************************************************************
**
** Function         NFC_GetConfig
**
** Description      This function is called to retrieve the parameter TLV from NFCC.
**                  The response from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_GET_CONFIG_REVT.
**
** Parameters       num_ids - the number of parameter IDs
**                  p_param_ids - the parameter ID list.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_GetConfig (UINT8     num_ids,
                           UINT8    *p_param_ids)
{
    return nci_snd_core_get_config (p_param_ids, num_ids);
}

/*******************************************************************************
**
** Function         NFC_DiscoveryMap
**
** Description      This function is called to set the discovery interface mapping.
**                  The response from NFCC is reported by tNFC_DISCOVER_CBACK as.
**                  NFC_MAP_DEVT.
**
** Parameters       num - the number of items in p_params.
**                  p_maps - the discovery interface mappings
**                  p_cback - the discovery callback function
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_DiscoveryMap (UINT8 num, tNFC_DISCOVER_MAPS *p_maps,
                                        tNFC_DISCOVER_CBACK *p_cback)
{
    UINT8   num_disc_maps = num;
    UINT8   xx, yy, num_intf, intf_mask;
    tNFC_DISCOVER_MAPS  max_maps[NFC_NFCC_MAX_NUM_VS_INTERFACE + NCI_INTERFACE_MAX];
    BOOLEAN is_supported;
    #if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    nfc_cb.num_disc_maps = num;
    #endif
    nfc_cb.p_discv_cback = p_cback;
    num_intf             = 0;
    NFC_TRACE_DEBUG1 ("nci_interfaces supported by NFCC: 0x%x", nfc_cb.nci_interfaces);

    for(xx = 0; xx < NFC_NFCC_MAX_NUM_VS_INTERFACE + NCI_INTERFACE_MAX; xx++)
    {
        memset(&max_maps[xx], 0x00, sizeof(tNFC_DISCOVER_MAPS));
    }

    for (xx = 0; xx < num_disc_maps; xx++)
    {
        is_supported = FALSE;
        if (p_maps[xx].intf_type > NCI_INTERFACE_MAX)
        {
            for (yy = 0; yy < NFC_NFCC_MAX_NUM_VS_INTERFACE; yy++)
            {
                if (nfc_cb.vs_interface[yy] == p_maps[xx].intf_type)
                    is_supported    = TRUE;
            }
            NFC_TRACE_DEBUG3 ("[%d]: vs intf_type:0x%x is_supported:%d", xx, p_maps[xx].intf_type, is_supported);
        }
        else
        {
            intf_mask = (1 << (p_maps[xx].intf_type));
            if (intf_mask & nfc_cb.nci_interfaces)
            {
                is_supported    = TRUE;
            }
            NFC_TRACE_DEBUG4 ("[%d]: intf_type:%d intf_mask: 0x%x is_supported:%d", xx, p_maps[xx].intf_type, intf_mask, is_supported);
        }
        if (is_supported)
            memcpy (&max_maps[num_intf++], &p_maps[xx], sizeof (tNFC_DISCOVER_MAPS));
        else
        {
            NFC_TRACE_WARNING1 ("NFC_DiscoveryMap interface=0x%x is not supported by NFCC", p_maps[xx].intf_type);
        }
    }

    NFC_TRACE_WARNING1 ("num_intf = 0x%2x",num_intf);

    for(xx = 0; xx < NFC_NFCC_MAX_NUM_VS_INTERFACE + NCI_INTERFACE_MAX; xx++)
    {
        NFC_TRACE_WARNING2 ("max_maps[%d].intf_type = 0x%2x",xx,max_maps[xx].intf_type);
        NFC_TRACE_WARNING2 ("max_maps[%d].mode = 0x%2x",xx,max_maps[xx].mode);
        NFC_TRACE_WARNING2 ("max_maps[%d].protocol = 0x%2x",xx,max_maps[xx].protocol);
    }
    return nci_snd_discover_map_cmd (num_intf, (tNCI_DISCOVER_MAPS *) max_maps);
}

/*******************************************************************************
**
** Function         NFC_DiscoveryStart
**
** Description      This function is called to start Polling and/or Listening.
**                  The response from NFCC is reported by tNFC_DISCOVER_CBACK as.
**                  NFC_START_DEVT. The notification from NFCC is reported by
**                  tNFC_DISCOVER_CBACK as NFC_RESULT_DEVT.
**
** Parameters       num_params - the number of items in p_params.
**                  p_params - the discovery parameters
**                  p_cback - the discovery callback function
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_DiscoveryStart (UINT8                 num_params,
                                tNFC_DISCOVER_PARAMS *p_params,
                                tNFC_DISCOVER_CBACK  *p_cback)
{
    UINT8   *p;
    int     params_size;
    tNFC_STATUS status = NFC_STATUS_NO_BUFFERS;

    NFC_TRACE_API0 ("NFC_DiscoveryStart");
    if (nfc_cb.p_disc_pending)
    {
        NFC_TRACE_ERROR0 ("There's pending NFC_DiscoveryStart");
        status = NFC_STATUS_BUSY;
    }
    else
    {
        nfc_cb.p_discv_cback = p_cback;
        nfc_cb.flags        |= NFC_FL_DISCOVER_PENDING;
        nfc_cb.flags        |= NFC_FL_CONTROL_REQUESTED;
        params_size          = sizeof (tNFC_DISCOVER_PARAMS) * num_params;
        nfc_cb.p_disc_pending = GKI_getbuf ((UINT16)(BT_HDR_SIZE + 1 + params_size));
        if (nfc_cb.p_disc_pending)
        {
            p       = (UINT8 *)nfc_cb.p_disc_pending;
            *p++    = num_params;
            memcpy (p, p_params, params_size);
            status = NFC_STATUS_CMD_STARTED;
            nfc_ncif_check_cmd_queue (NULL);
        }
    }

    NFC_TRACE_API1 ("NFC_DiscoveryStart status: 0x%x", status);
    return status;
}

/*******************************************************************************
**
** Function         NFC_DiscoverySelect
**
** Description      If tNFC_DISCOVER_CBACK reports status=NFC_MULTIPLE_PROT,
**                  the application needs to use this function to select the
**                  the logical endpoint to continue. The response from NFCC is
**                  reported by tNFC_DISCOVER_CBACK as NFC_SELECT_DEVT.
**
** Parameters       rf_disc_id - The ID identifies the remote device.
**                  protocol - the logical endpoint on the remote devide
**                  rf_interface - the RF interface to communicate with NFCC
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_DiscoverySelect (UINT8    rf_disc_id,
                                 UINT8    protocol,
                                 UINT8    rf_interface)
{
    return nci_snd_discover_select_cmd (rf_disc_id, protocol, rf_interface);
}

/*******************************************************************************
**
** Function         NFC_ConnCreate
**
** Description      This function is called to create a logical connection with
**                  NFCC for data exchange.
**
** Parameters       dest_type - the destination type
**                  id   - the NFCEE ID or RF Discovery ID .
**                  protocol   - the protocol.
**                  p_cback - the connection callback function
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_ConnCreate (UINT8            dest_type,
                            UINT8            id,
                            UINT8            protocol,
                            tNFC_CONN_CBACK *p_cback)
{
    tNFC_STATUS     status = NFC_STATUS_FAILED;
    tNFC_CONN_CB    *p_cb;
    UINT8           num_tlv=0, tlv_size=0;
    UINT8           param_tlvs[4], *pp;

    p_cb = nfc_alloc_conn_cb (p_cback);
    if (p_cb)
    {
        p_cb->id = id;
        pp = param_tlvs;
        if (dest_type == NCI_DEST_TYPE_NFCEE)
        {
            num_tlv = 1;
            UINT8_TO_STREAM (pp, NCI_CON_CREATE_TAG_NFCEE_VAL);
            UINT8_TO_STREAM (pp, 2);
            UINT8_TO_STREAM (pp, id);
            UINT8_TO_STREAM (pp, protocol);
            tlv_size = 4;
        }
        else if (dest_type == NCI_DEST_TYPE_REMOTE)
        {
            num_tlv = 1;
            UINT8_TO_STREAM (pp, NCI_CON_CREATE_TAG_RF_DISC_ID);
            UINT8_TO_STREAM (pp, 1);
            UINT8_TO_STREAM (pp, id);
            tlv_size = 3;
        }
        else if (dest_type == NCI_DEST_TYPE_NFCC)
        {
            p_cb->id = NFC_TEST_ID;
        }
        /* Add handling of NCI_DEST_TYPE_REMOTE when more RF interface definitions are added */
        p_cb->act_protocol = protocol;
        p_cb->p_cback = p_cback;
        status = nci_snd_core_conn_create (dest_type, num_tlv, tlv_size, param_tlvs);
        if (status == NFC_STATUS_FAILED)
            nfc_free_conn_cb (p_cb);
    }
    return status;
}

/*******************************************************************************
**
** Function         NFC_ConnClose
**
** Description      This function is called to close a logical connection with
**                  NFCC.
**
** Parameters       conn_id - the connection id.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_ConnClose (UINT8 conn_id)
{
    tNFC_CONN_CB *p_cb = nfc_find_conn_cb_by_conn_id (conn_id);
    tNFC_STATUS     status = NFC_STATUS_FAILED;

    if (p_cb)
    {
        status = nci_snd_core_conn_close (conn_id);
    }
    return status;
}

/*******************************************************************************
**
** Function         NFC_SetStaticRfCback
**
** Description      This function is called to update the data callback function
**                  to receive the data for the given connection id.
**
** Parameters       p_cback - the connection callback function
**
** Returns          Nothing
**
*******************************************************************************/
void NFC_SetStaticRfCback (tNFC_CONN_CBACK    *p_cback)
{
    tNFC_CONN_CB * p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];

    p_cb->p_cback = p_cback;
    /* just in case DH has received NCI data before the data callback is set
     * check if there's any data event to report on this connection id */
    nfc_data_event (p_cb);
}

/*******************************************************************************
**
** Function         NFC_SetReassemblyFlag
**
** Description      This function is called to set if nfc will reassemble
**                  nci packet as much as its buffer can hold or it should not
**                  reassemble but forward the fragmented nci packet to layer above.
**                  If nci data pkt is fragmented, nfc may send multiple
**                  NFC_DATA_CEVT with status NFC_STATUS_CONTINUE before sending
**                  NFC_DATA_CEVT with status NFC_STATUS_OK based on reassembly
**                  configuration and reassembly buffer size
**
** Parameters       reassembly - flag to indicate if nfc may reassemble or not
**
** Returns          Nothing
**
*******************************************************************************/
void NFC_SetReassemblyFlag (BOOLEAN    reassembly)
{
    nfc_cb.reassembly = reassembly;
}

/*******************************************************************************
**
** Function         NFC_SendData
**
** Description      This function is called to send the given data packet
**                  to the connection identified by the given connection id.
**
** Parameters       conn_id - the connection id.
**                  p_data - the data packet.
**                  p_data->offset must be >= NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE
**                  The data payload starts at ((UINT8 *) (p_data + 1) + p_data->offset)
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_SendData (UINT8       conn_id,
                          BT_HDR     *p_data)
{
    tNFC_STATUS     status = NFC_STATUS_FAILED;
    tNFC_CONN_CB *p_cb = nfc_find_conn_cb_by_conn_id (conn_id);

    if (p_cb && p_data && p_data->offset >= NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE)
    {
        status = nfc_ncif_send_data (p_cb, p_data);
    }

    if (status != NFC_STATUS_OK)
        GKI_freebuf (p_data);

    return status;
}

/*******************************************************************************
**
** Function         NFC_FlushData
**
** Description      This function is called to discard the tx data queue of
**                  the given connection id.
**
** Parameters       conn_id - the connection id.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_FlushData (UINT8       conn_id)
{
    tNFC_STATUS     status = NFC_STATUS_FAILED;
    tNFC_CONN_CB    *p_cb = nfc_find_conn_cb_by_conn_id (conn_id);
    void            *p_buf;

    if (p_cb)
    {
        status  = NFC_STATUS_OK;
        while ((p_buf = GKI_dequeue (&p_cb->tx_q)) != NULL)
            GKI_freebuf (p_buf);
    }

    return status;
}

/*******************************************************************************
**
** Function         NFC_Deactivate
**
** Description      This function is called to stop the discovery process or
**                  put the listen device in sleep mode or terminate the NFC link.
**
**                  The response from NFCC is reported by tNFC_DISCOVER_CBACK
**                  as NFC_DEACTIVATE_DEVT.
**
** Parameters       deactivate_type - NFC_DEACTIVATE_TYPE_IDLE, to IDLE mode.
**                                    NFC_DEACTIVATE_TYPE_SLEEP to SLEEP mode.
**                                    NFC_DEACTIVATE_TYPE_SLEEP_AF to SLEEP_AF mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_Deactivate (tNFC_DEACT_TYPE deactivate_type)
{
    tNFC_CONN_CB * p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
    tNFC_STATUS  status = NFC_STATUS_OK;

#if (BT_TRACE_VERBOSE == TRUE)
    NFC_TRACE_API3 ("NFC_Deactivate %d (%s) deactivate_type:%d", nfc_cb.nfc_state, nfc_state_name (nfc_cb.nfc_state), deactivate_type);
#else
    NFC_TRACE_API2 ("NFC_Deactivate %d deactivate_type:%d", nfc_cb.nfc_state, deactivate_type);
#endif

    if (nfc_cb.flags & NFC_FL_DISCOVER_PENDING)
    {
        /* the HAL pre-discover is still active - clear the pending flag */
        nfc_cb.flags &= ~NFC_FL_DISCOVER_PENDING;
        if (!(nfc_cb.flags & NFC_FL_HAL_REQUESTED))
        {
            /* if HAL did not request for control, clear this bit now */
            nfc_cb.flags &= ~NFC_FL_CONTROL_REQUESTED;
        }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if( nfc_cb.p_last_disc)
        {
            GKI_freebuf (nfc_cb.p_last_disc);
            nfc_cb.p_last_disc = NULL;
        }
        nfc_cb.p_last_disc = nfc_cb.p_disc_pending;
#else
        GKI_freebuf (nfc_cb.p_disc_pending);
#endif
        nfc_cb.p_disc_pending = NULL;
        return NFC_STATUS_OK;
    }

    if (nfc_cb.nfc_state == NFC_STATE_OPEN)
    {
        nfc_set_state (NFC_STATE_CLOSING);
        NFC_TRACE_DEBUG3 ( "act_protocol %d credits:%d/%d", p_cb->act_protocol, p_cb->init_credits, p_cb->num_buff);
        if ((p_cb->act_protocol == NCI_PROTOCOL_NFC_DEP) &&
            (p_cb->init_credits != p_cb->num_buff))
        {
            nfc_cb.flags           |= NFC_FL_DEACTIVATING;
            nfc_cb.deactivate_timer.param = (TIMER_PARAM_TYPE) deactivate_type;
            nfc_start_timer (&nfc_cb.deactivate_timer , (UINT16) (NFC_TTYPE_WAIT_2_DEACTIVATE), NFC_DEACTIVATE_TIMEOUT);
            return status;
        }
    }

    status = nci_snd_deactivate_cmd (deactivate_type);
    return status;
}

/*******************************************************************************
**
** Function         NFC_UpdateRFCommParams
**
** Description      This function is called to update RF Communication parameters
**                  once the Frame RF Interface has been activated.
**
**                  The response from NFCC is reported by tNFC_RESPONSE_CBACK
**                  as NFC_RF_COMM_PARAMS_UPDATE_REVT.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_UpdateRFCommParams (tNFC_RF_COMM_PARAMS *p_params)
{
    UINT8 tlvs[12];
    UINT8 *p = tlvs;
    UINT8 data_exch_config;

    /* RF Technology and Mode */
    if (p_params->include_rf_tech_mode)
    {
        UINT8_TO_STREAM (p, NCI_RF_PARAM_ID_TECH_N_MODE);
        UINT8_TO_STREAM (p, 1);
        UINT8_TO_STREAM (p, p_params->rf_tech_n_mode);
    }

    /* Transmit Bit Rate */
    if (p_params->include_tx_bit_rate)
    {
        UINT8_TO_STREAM (p, NCI_RF_PARAM_ID_TX_BIT_RATE);
        UINT8_TO_STREAM (p, 1);
        UINT8_TO_STREAM (p, p_params->tx_bit_rate);
    }

    /* Receive Bit Rate */
    if (p_params->include_tx_bit_rate)
    {
        UINT8_TO_STREAM (p, NCI_RF_PARAM_ID_RX_BIT_RATE);
        UINT8_TO_STREAM (p, 1);
        UINT8_TO_STREAM (p, p_params->rx_bit_rate);
    }

    /* NFC-B Data Exchange Configuration */
    if (p_params->include_nfc_b_config)
    {
        UINT8_TO_STREAM (p, NCI_RF_PARAM_ID_B_DATA_EX_PARAM);
        UINT8_TO_STREAM (p, 1);

        data_exch_config =  (p_params->min_tr0 & 0x03) << 6;          /* b7b6 : Mininum TR0 */
        data_exch_config |= (p_params->min_tr1 & 0x03) << 4;          /* b5b4 : Mininum TR1 */
        data_exch_config |= (p_params->suppression_eos & 0x01) << 3;  /* b3 :   Suppression of EoS */
        data_exch_config |= (p_params->suppression_sos & 0x01) << 2;  /* b2 :   Suppression of SoS */
        data_exch_config |= (p_params->min_tr2 & 0x03);               /* b1b0 : Mininum TR2 */

        UINT8_TO_STREAM (p, data_exch_config);
    }

    return nci_snd_parameter_update_cmd (tlvs, (UINT8) (p - tlvs));
}

/*******************************************************************************
**
** Function         NFC_SetPowerOffSleep
**
** Description      This function closes/opens transport and turns off/on NFCC.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_SetPowerOffSleep (BOOLEAN enable)
{
    NFC_TRACE_API1 ("NFC_SetPowerOffSleep () enable = %d", enable);

    if (  (enable == FALSE)
        &&(nfc_cb.nfc_state == NFC_STATE_NFCC_POWER_OFF_SLEEP)  )
    {
        nfc_cb.flags |= NFC_FL_RESTARTING;

        /* open transport */
        nfc_set_state (NFC_STATE_W4_HAL_OPEN);
        nfc_cb.p_hal->open (nfc_main_hal_cback, nfc_main_hal_data_cback);

        return NFC_STATUS_OK;
    }
    else if (  (enable == TRUE)
             &&(nfc_cb.nfc_state == NFC_STATE_IDLE)  )
    {
        /* close transport to turn off NFCC and clean up */
        nfc_cb.flags |= NFC_FL_POWER_OFF_SLEEP;
        nfc_task_shutdown_nfcc ();

        return NFC_STATUS_OK;
    }

    NFC_TRACE_ERROR1 ("NFC_SetPowerOffSleep () invalid state = %d", nfc_cb.nfc_state);
    return NFC_STATUS_FAILED;
}

/*******************************************************************************
**
** Function         NFC_PowerCycleNFCC
**
** Description      This function turns off and then on NFCC.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFC_PowerCycleNFCC (void)
{
    NFC_TRACE_API0 ("NFC_PowerCycleNFCC ()");

    if (nfc_cb.nfc_state == NFC_STATE_IDLE)
    {
        /* power cycle NFCC */
        nfc_cb.flags |= NFC_FL_POWER_CYCLE_NFCC;
        nfc_task_shutdown_nfcc ();

        return NFC_STATUS_OK;
    }

    NFC_TRACE_ERROR1 ("NFC_PowerCycleNFCC () invalid state = %d", nfc_cb.nfc_state);
    return NFC_STATUS_FAILED;
}


/*******************************************************************************
**
** Function         NFC_SetTraceLevel
**
** Description      This function sets the trace level for NFC.  If called with
**                  a value of 0xFF, it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 NFC_SetTraceLevel (UINT8 new_level)
{
    NFC_TRACE_API1 ("NFC_SetTraceLevel () new_level = %d", new_level);

    if (new_level != 0xFF)
        nfc_cb.trace_level = new_level;

    return (nfc_cb.trace_level);
}
void set_i2c_fragmentation_enabled(int value)
{
    i2c_fragmentation_enabled = value;
}

int  get_i2c_fragmentation_enabled()
{
    return i2c_fragmentation_enabled;
}
#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         NFC_GetStatusName
**
** Description      This function returns the status name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *NFC_GetStatusName (tNFC_STATUS status)
{
    switch (status)
    {
    case NFC_STATUS_OK:
        return "OK";
    case NFC_STATUS_REJECTED:
        return "REJECTED";
    case NFC_STATUS_MSG_CORRUPTED:
        return "CORRUPTED";
    case NFC_STATUS_BUFFER_FULL:
        return "BUFFER_FULL";
    case NFC_STATUS_FAILED:
        return "FAILED";
    case NFC_STATUS_NOT_INITIALIZED:
        return "NOT_INITIALIZED";
    case NFC_STATUS_SYNTAX_ERROR:
        return "SYNTAX_ERROR";
    case NFC_STATUS_SEMANTIC_ERROR:
        return "SEMANTIC_ERROR";
    case NFC_STATUS_UNKNOWN_GID:
        return "UNKNOWN_GID";
    case NFC_STATUS_UNKNOWN_OID:
        return "UNKNOWN_OID";
    case NFC_STATUS_INVALID_PARAM:
        return "INVALID_PARAM";
    case NFC_STATUS_MSG_SIZE_TOO_BIG:
        return "MSG_SIZE_TOO_BIG";
    case NFC_STATUS_ALREADY_STARTED:
        return "ALREADY_STARTED";
    case NFC_STATUS_ACTIVATION_FAILED:
        return "ACTIVATION_FAILED";
    case NFC_STATUS_TEAR_DOWN:
        return "TEAR_DOWN";
    case NFC_STATUS_RF_TRANSMISSION_ERR:
        return "RF_TRANSMISSION_ERR";
    case NFC_STATUS_RF_PROTOCOL_ERR:
        return "RF_PROTOCOL_ERR";
    case NFC_STATUS_TIMEOUT:
        return "TIMEOUT";
    case NFC_STATUS_EE_INTF_ACTIVE_FAIL:
        return "EE_INTF_ACTIVE_FAIL";
    case NFC_STATUS_EE_TRANSMISSION_ERR:
        return "EE_TRANSMISSION_ERR";
    case NFC_STATUS_EE_PROTOCOL_ERR:
        return "EE_PROTOCOL_ERR";
    case NFC_STATUS_EE_TIMEOUT:
        return "EE_TIMEOUT";
    case NFC_STATUS_CMD_STARTED:
        return "CMD_STARTED";
    case NFC_STATUS_HW_TIMEOUT:
        return "HW_TIMEOUT";
    case NFC_STATUS_CONTINUE:
        return "CONTINUE";
    case NFC_STATUS_REFUSED:
        return "REFUSED";
    case NFC_STATUS_BAD_RESP:
        return "BAD_RESP";
    case NFC_STATUS_CMD_NOT_CMPLTD:
        return "CMD_NOT_CMPLTD";
    case NFC_STATUS_NO_BUFFERS:
        return "NO_BUFFERS";
    case NFC_STATUS_WRONG_PROTOCOL:
        return "WRONG_PROTOCOL";
    case NFC_STATUS_BUSY:
        return "BUSY";
    case NFC_STATUS_LINK_LOSS:
        return "LINK_LOSS";
    case NFC_STATUS_BAD_LENGTH:
        return "BAD_LENGTH";
    case NFC_STATUS_BAD_HANDLE:
        return "BAD_HANDLE";
    case NFC_STATUS_CONGESTED:
        return "CONGESTED";
    default:
        return"UNKNOWN";
    }
}
#endif

#endif /* NFC_INCLUDED == TRUE */
