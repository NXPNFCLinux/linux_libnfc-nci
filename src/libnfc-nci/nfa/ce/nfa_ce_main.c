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
 *  This is the main implementation file for the NFA_CE
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_ce_api.h"
#include "nfa_sys.h"
#include "nfa_ce_int.h"
#include "nfa_dm_int.h"
#include "nfa_sys_int.h"

/* NFA_CE control block */
tNFA_CE_CB nfa_ce_cb;

/*****************************************************************************
** Constants and types
*****************************************************************************/
#define NFA_CE_DEFAULT_ISODEP_DISC_MASK (NFA_DM_DISC_MASK_LA_ISO_DEP | NFA_DM_DISC_MASK_LB_ISO_DEP)
static void nfa_ce_proc_nfcc_power_mode (UINT8 nfcc_power_mode);

static const tNFA_SYS_REG nfa_ce_sys_reg =
{
    NULL,
    nfa_ce_hdl_event,
    nfa_ce_sys_disable,
    nfa_ce_proc_nfcc_power_mode
};

/* NFA_CE actions */
const tNFA_CE_ACTION nfa_ce_action_tbl[] =
{
    nfa_ce_api_cfg_local_tag,   /* NFA_CE_API_CFG_LOCAL_TAG_EVT */
    nfa_ce_api_reg_listen,      /* NFA_CE_API_REG_LISTEN_EVT    */
    nfa_ce_api_dereg_listen,    /* NFA_CE_API_DEREG_LISTEN_EVT  */
    nfa_ce_api_cfg_isodep_tech, /* NFA_CE_API_CFG_ISODEP_TECH_EVT*/
    nfa_ce_activate_ntf,        /* NFA_CE_ACTIVATE_NTF_EVT      */
    nfa_ce_deactivate_ntf,      /* NFA_CE_DEACTIVATE_NTF_EVT    */
};
#define NFA_CE_ACTION_TBL_SIZE  (sizeof (nfa_ce_action_tbl) / sizeof (tNFA_CE_ACTION))

/*****************************************************************************
** Local function prototypes
*****************************************************************************/
#if (BT_TRACE_VERBOSE == TRUE)
static char *nfa_ce_evt_2_str (UINT16 event);
#endif


/*******************************************************************************
**
** Function         nfa_ce_init
**
** Description      Initialize NFA CE
**
** Returns          None
**
*******************************************************************************/
void nfa_ce_init (void)
{
    NFA_TRACE_DEBUG0 ("nfa_ce_init ()");

    /* initialize control block */
    memset (&nfa_ce_cb, 0, sizeof (tNFA_CE_CB));

    /* Generate a random NFCID for Type-3 NDEF emulation (Type-3 tag NFCID2 must start with 02:FE) */
    nfa_ce_t3t_generate_rand_nfcid (nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].t3t_nfcid2);
    nfa_ce_cb.listen_info[NFA_CE_LISTEN_INFO_IDX_NDEF].rf_disc_handle = NFA_HANDLE_INVALID;
    nfa_ce_cb.isodep_disc_mask  = NFA_CE_DEFAULT_ISODEP_DISC_MASK;
    nfa_ce_cb.idx_wild_card     = NFA_CE_LISTEN_INFO_IDX_INVALID;

    /* register message handler on NFA SYS */
    nfa_sys_register ( NFA_ID_CE,  &nfa_ce_sys_reg);
}

/*******************************************************************************
**
** Function         nfa_ce_sys_disable
**
** Description      Clean up ce sub-system
**
**
** Returns          void
**
*******************************************************************************/
void nfa_ce_sys_disable (void)
{
    tNFA_CE_LISTEN_INFO *p_info;
    UINT8 xx;

    NFC_SetStaticRfCback (NULL);

    /* Free scratch buf if any */
    nfa_ce_free_scratch_buf ();

    /* Delete discovery handles */
    for (xx = 0, p_info = nfa_ce_cb.listen_info; xx < NFA_CE_LISTEN_INFO_MAX; xx++, p_info++)
    {
        if ((p_info->flags & NFA_CE_LISTEN_INFO_IN_USE) && (p_info->rf_disc_handle != NFA_HANDLE_INVALID))
        {
            nfa_dm_delete_rf_discover (p_info->rf_disc_handle);
            p_info->rf_disc_handle = NFA_HANDLE_INVALID;
        }
    }

    nfa_sys_deregister (NFA_ID_CE);
}

/*******************************************************************************
**
** Function         nfa_ce_proc_nfcc_power_mode
**
** Description      Processing NFCC power mode changes
**
** Returns          None
**
*******************************************************************************/
static void nfa_ce_proc_nfcc_power_mode (UINT8 nfcc_power_mode)
{
    tNFA_CE_CB *p_cb = &nfa_ce_cb;
    UINT8 listen_info_idx;

    NFA_TRACE_DEBUG1 ("nfa_ce_proc_nfcc_power_mode (): nfcc_power_mode=%d",
                       nfcc_power_mode);

    /* if NFCC power mode is change to full power */
    if (nfcc_power_mode == NFA_DM_PWR_MODE_FULL)
    {
        nfa_ce_restart_listen_check ();
    }
    else
    {
        for (listen_info_idx=0; listen_info_idx<NFA_CE_LISTEN_INFO_IDX_INVALID; listen_info_idx++)
        {
            /* add RF discovery to DM only if it is not added yet */
            if (  (p_cb->listen_info[listen_info_idx].flags & NFA_CE_LISTEN_INFO_IN_USE)
                &&(p_cb->listen_info[listen_info_idx].rf_disc_handle != NFA_HANDLE_INVALID))
            {
                nfa_dm_delete_rf_discover (p_cb->listen_info[listen_info_idx].rf_disc_handle);
                p_cb->listen_info[listen_info_idx].rf_disc_handle = NFA_HANDLE_INVALID;
            }
        }
    }

    nfa_sys_cback_notify_nfcc_power_mode_proc_complete (NFA_ID_CE);
}

/*******************************************************************************
**
** Function         nfa_ce_hdl_event
**
** Description      nfa rw main event handling function.
**
** Returns          BOOLEAN
**
*******************************************************************************/
BOOLEAN nfa_ce_hdl_event (BT_HDR *p_msg)
{
    UINT16 act_idx;
    BOOLEAN freebuf = TRUE;

#if (BT_TRACE_VERBOSE == TRUE)
    NFA_TRACE_EVENT3 ("nfa_ce_handle_event event: %s (0x%02x), flags: %08x", nfa_ce_evt_2_str (p_msg->event), p_msg->event, nfa_ce_cb.flags);
#else
    NFA_TRACE_EVENT2 ("nfa_ce_handle_event event: 0x%x, flags: %08x",p_msg->event, nfa_ce_cb.flags);
#endif

    /* Get NFA_RW sub-event */
    if ((act_idx = (p_msg->event & 0x00FF)) < NFA_CE_ACTION_TBL_SIZE)
    {
        freebuf = (*nfa_ce_action_tbl[act_idx]) ((tNFA_CE_MSG*) p_msg);
    }

    /* if vendor specific event handler is registered */
    if (nfa_ce_cb.p_vs_evt_hdlr)
    {
        (*nfa_ce_cb.p_vs_evt_hdlr) (p_msg);
    }

    return freebuf;
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         nfa_ce_evt_2_str
**
** Description      convert nfc evt to string
**
*******************************************************************************/
static char *nfa_ce_evt_2_str (UINT16 event)
{
    switch (event)
    {
    case NFA_CE_API_CFG_LOCAL_TAG_EVT:
        return "NFA_CE_API_CFG_LOCAL_TAG_EVT";

    case NFA_CE_API_REG_LISTEN_EVT:
        return "NFA_CE_API_REG_LISTEN_EVT";

    case NFA_CE_API_DEREG_LISTEN_EVT:
        return "NFA_CE_API_DEREG_LISTEN_EVT";

    case NFA_CE_API_CFG_ISODEP_TECH_EVT:
        return "NFA_CE_API_CFG_ISODEP_TECH_EVT";

    case NFA_CE_ACTIVATE_NTF_EVT:
        return "NFA_CE_ACTIVATE_NTF_EVT";

    case NFA_CE_DEACTIVATE_NTF_EVT:
        return "NFA_CE_DEACTIVATE_NTF_EVT";

    default:
        return "Unknown";
    }
}
#endif /* BT_TRACE_VERBOSE */
