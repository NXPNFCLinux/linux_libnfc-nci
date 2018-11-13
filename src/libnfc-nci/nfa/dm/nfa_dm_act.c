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
 *  This file contains the action functions for device manager state
 *  machine.
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_sys.h"
#include "nfa_api.h"
#include "nfa_dm_int.h"
#include "nfa_sys_int.h"
#include "nfa_ce_int.h"
#include "nfc_api.h"
#include "nfa_rw_int.h"
#include "nfa_rw_api.h"
#include "nfa_p2p_int.h"
#include "nci_hmsgs.h"

#if (defined (NFA_CHO_INCLUDED) && (NFA_CHO_INCLUDED==TRUE))
#include "nfa_cho_int.h"
#endif

#if (NFC_NFCEE_INCLUDED == TRUE)
#include "nfa_ee_int.h"
#include "nfa_hci_int.h"
#endif

#if (defined (NFA_SNEP_INCLUDED) && (NFA_SNEP_INCLUDED==TRUE))
#include "nfa_snep_int.h"
#endif

/* This is the timeout value to guarantee disable is performed within reasonable amount of time */
#ifndef NFA_DM_DISABLE_TIMEOUT_VAL
#define NFA_DM_DISABLE_TIMEOUT_VAL         1000
#endif

static void nfa_dm_set_init_nci_params (void);
static tNFA_STATUS nfa_dm_start_polling (void);
static BOOLEAN nfa_dm_deactivate_polling (void);
static void nfa_dm_excl_disc_cback (tNFA_DM_RF_DISC_EVT event, tNFC_DISCOVER *p_data);
static void nfa_dm_poll_disc_cback (tNFA_DM_RF_DISC_EVT event, tNFC_DISCOVER *p_data);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
extern BOOLEAN gFelicaReaderMode;
#endif

/*******************************************************************************
**
** Function         nfa_dm_module_init_cback
**
** Description      Processing initialization complete event from sub-modules
**
** Returns          None
**
*******************************************************************************/
static void nfa_dm_module_init_cback (void)
{
    tNFA_DM_CBACK_DATA dm_cback_data;

    nfa_dm_cb.flags &= ~NFA_DM_FLAGS_ENABLE_EVT_PEND;

    /* All subsystem are initialized */
    dm_cback_data.status = NFA_STATUS_OK;
    (*nfa_dm_cb.p_dm_cback) (NFA_DM_ENABLE_EVT, &dm_cback_data);
}

/*******************************************************************************
**
** Function         nfa_dm_nfcc_power_mode_proc_complete_cback
**
** Description      Processing complete of processing NFCC power state change
**                  from all sub-modules
**
** Returns          None
**
*******************************************************************************/
static void nfa_dm_nfcc_power_mode_proc_complete_cback (void)
{
    tNFA_DM_PWR_MODE_CHANGE power_mode_change;

    NFA_TRACE_DEBUG1 ("nfa_dm_nfcc_power_mode_proc_complete_cback () nfcc_pwr_mode = 0x%x",
                      nfa_dm_cb.nfcc_pwr_mode);

    /* if NFCC power state is change to full power */
    if (nfa_dm_cb.nfcc_pwr_mode != NFA_DM_PWR_MODE_OFF_SLEEP)
    {
        nfa_dm_cb.flags &= ~NFA_DM_FLAGS_NFCC_IS_RESTORING;

        /* reconfigure BRCM NFCC */
        nfa_dm_disc_sm_execute (NFA_DM_RF_DISCOVER_CMD, NULL);
    }

    nfa_dm_cb.flags &= ~NFA_DM_FLAGS_SETTING_PWR_MODE;

    power_mode_change.status     = NFA_STATUS_OK;
    power_mode_change.power_mode = nfa_dm_cb.nfcc_pwr_mode;
    (*nfa_dm_cb.p_dm_cback) (NFA_DM_PWR_MODE_CHANGE_EVT, (tNFA_DM_CBACK_DATA*) &power_mode_change);
}
/*******************************************************************************
**
** Function         nfa_dm_sys_enable
**
** Description      This function on enable
**
** Returns          void
**
*******************************************************************************/
void nfa_dm_sys_enable (void)
{
    nfa_dm_set_init_nci_params ();
}

/*******************************************************************************
**
** Function         nfa_dm_set_init_nci_params
**
** Description      Set initial NCI configuration parameters
**
** Returns          void
**
*******************************************************************************/
static void nfa_dm_set_init_nci_params (void)
{
    UINT8   xx;

    /* set NCI default value if other than zero */

    /* LF_T3T_IDENTIFIERS_1/2/.../16 */
    for (xx = 0; xx < NFA_CE_LISTEN_INFO_MAX; xx++)
    {
        nfa_dm_cb.params.lf_t3t_id[xx][0] = 0xFF;
        nfa_dm_cb.params.lf_t3t_id[xx][1] = 0xFF;
        nfa_dm_cb.params.lf_t3t_id[xx][2] = 0x02;
        nfa_dm_cb.params.lf_t3t_id[xx][3] = 0xFE;
    }

    /* LF_T3T_PMM */
    for (xx = 0; xx < NCI_PARAM_LEN_LF_T3T_PMM; xx++)
    {
        nfa_dm_cb.params.lf_t3t_pmm[xx] = 0xFF;
    }

    /* LF_T3T_FLAGS:
    ** DH needs to set this configuration, even if default value (not listening) is used,
    ** to let NFCC know of intention (not listening) of DH.
    */

    /* FWI */
    nfa_dm_cb.params.fwi[0] = 0x04;

    /* WT */
    nfa_dm_cb.params.wt[0] = 14;

    // LF_T3T_PMM is not supported.
    /* Set CE default configuration */
#if (NFC_NXP_NOT_OPEN_INCLUDED != TRUE)
    if (p_nfa_dm_ce_cfg[0])
    {
        nfa_dm_check_set_config (p_nfa_dm_ce_cfg[0], &p_nfa_dm_ce_cfg[1], FALSE);
    }
#endif

    /* Set optional general default configuration */
    if (p_nfa_dm_gen_cfg && p_nfa_dm_gen_cfg[0])
    {
        nfa_dm_check_set_config (p_nfa_dm_gen_cfg[0], &p_nfa_dm_gen_cfg[1], FALSE);
    }

    if (p_nfa_dm_interface_mapping && nfa_dm_num_dm_interface_mapping)
    {
        NFC_DiscoveryMap (nfa_dm_num_dm_interface_mapping, p_nfa_dm_interface_mapping, NULL);
    }
}

/*******************************************************************************
**
** Function         nfa_dm_proc_nfcc_power_mode
**
** Description      Processing NFCC power mode changes
**
** Returns          None
**
*******************************************************************************/
void nfa_dm_proc_nfcc_power_mode (UINT8 nfcc_power_mode)
{
    NFA_TRACE_DEBUG1 ("nfa_dm_proc_nfcc_power_mode (): nfcc_power_mode=%d",
                       nfcc_power_mode);

    /* if NFCC power mode is change to full power */
    if (nfcc_power_mode == NFA_DM_PWR_MODE_FULL)
    {
        memset (&nfa_dm_cb.params, 0x00, sizeof (tNFA_DM_PARAMS));
        NFA_TRACE_DEBUG2 ("setcfg_pending_mask=0x%x, setcfg_pending_num=%d",
            nfa_dm_cb.setcfg_pending_mask, nfa_dm_cb.setcfg_pending_num);
        nfa_dm_cb.setcfg_pending_mask = 0;
        nfa_dm_cb.setcfg_pending_num  = 0;

        nfa_dm_set_init_nci_params ();
        nfa_dm_cb.flags &= ~NFA_DM_FLAGS_POWER_OFF_SLEEP;
    }
    else if (nfcc_power_mode == NFA_DM_PWR_MODE_OFF_SLEEP)
    {
        nfa_dm_cb.flags |= NFA_DM_FLAGS_POWER_OFF_SLEEP;
    }

    nfa_sys_cback_notify_nfcc_power_mode_proc_complete (NFA_ID_DM);
}

/*******************************************************************************
**
** Function         nfa_dm_disable_event
**
** Description      report disable event
**
** Returns          void
**
*******************************************************************************/
static void nfa_dm_disable_event (void)
{
    /* Deregister DM from sys */
    nfa_sys_deregister (NFA_ID_DM);

    /* Notify app */
    nfa_dm_cb.flags &= ~(NFA_DM_FLAGS_DM_IS_ACTIVE
                        |NFA_DM_FLAGS_DM_DISABLING_NFC
                        |NFA_DM_FLAGS_ENABLE_EVT_PEND);
    (*nfa_dm_cb.p_dm_cback) (NFA_DM_DISABLE_EVT, NULL);
}

/*******************************************************************************
**
** Function         nfa_dm_nfc_response_cback
**
** Description      Call DM event hanlder with NFC response callback data
**
** Returns          void
**
*******************************************************************************/
static void nfa_dm_nfc_response_cback (tNFC_RESPONSE_EVT event, tNFC_RESPONSE *p_data)
{
    tNFA_DM_CBACK_DATA dm_cback_data;
    tNFA_GET_CONFIG   *p_nfa_get_confg;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    tNFA_GET_ROUTING   *p_nfa_get_routing;
#endif
    tNFA_CONN_EVT_DATA conn_evt;
    UINT8 dm_cback_evt;
    UINT8 max_ee = 0;

#if (BT_TRACE_VERBOSE == TRUE)
    NFA_TRACE_DEBUG2 ("nfa_dm_nfc_response_cback () %s(0x%x)", nfa_dm_nfc_revt_2_str (event), event);
#else
    NFA_TRACE_DEBUG1 ("nfa_dm_nfc_response_cback () event=0x%x", event);
#endif

    switch (event)
    {
    case NFC_ENABLE_REVT:                        /* 0  Enable event */

        /* NFC stack enabled. Enable nfa sub-systems */
        if (p_data->enable.status == NFC_STATUS_OK)
        {
            if (nfa_ee_max_ee_cfg != 0)
            {
                if (nfa_dm_cb.get_max_ee)
                {
                    max_ee = nfa_dm_cb.get_max_ee ();
                    if (max_ee)
                    {
                        nfa_ee_max_ee_cfg = max_ee;
                    }
                }
            }
            /* Initialize NFA subsystems */
            nfa_sys_enable_subsystems ();
        }
        else if (nfa_dm_cb.flags & NFA_DM_FLAGS_ENABLE_EVT_PEND)
        {
            /* Notify app */
            nfa_dm_cb.flags &= ~(NFA_DM_FLAGS_ENABLE_EVT_PEND | NFA_DM_FLAGS_DM_IS_ACTIVE);
            dm_cback_data.status = p_data->enable.status;
            (*nfa_dm_cb.p_dm_cback) (NFA_DM_ENABLE_EVT, &dm_cback_data);
        }
        break;

    case NFC_DISABLE_REVT:                       /* 1  Disable event */
        nfa_dm_disable_event ();
        break;

    case NFC_SET_CONFIG_REVT:                    /* 2  Set Config Response */
        /* If this setconfig was due to NFA_SetConfig, then notify the app */
        if (nfa_dm_cb.setcfg_pending_mask & 1)      /* lsb=whether last NCI_SET_CONFIG was due to NFA_SetConfig */
        {
            dm_cback_data.set_config.status         = p_data->set_config.status;
            dm_cback_data.set_config.num_param_id   = p_data->set_config.num_param_id;
            memcpy (dm_cback_data.set_config.param_ids, p_data->set_config.param_ids, p_data->set_config.num_param_id);
            (*nfa_dm_cb.p_dm_cback) (NFA_DM_SET_CONFIG_EVT, &dm_cback_data);
        }

        /* Update the pending mask */
        if (nfa_dm_cb.setcfg_pending_num>0)
        {
            nfa_dm_cb.setcfg_pending_mask >>= 1;
            nfa_dm_cb.setcfg_pending_num--;
        }
        else
        {
            /* This should not occur (means we got a SET_CONFIG_NTF that's unaccounted for */
            NFA_TRACE_ERROR0 ("NFA received unexpected NFC_SET_CONFIG_REVT");
        }
        break;

    case NFC_GET_CONFIG_REVT:                    /* 3  Get Config Response */
        if (p_data->get_config.status == NFC_STATUS_OK)
        {
            if ((p_nfa_get_confg = (tNFA_GET_CONFIG *) GKI_getbuf ((UINT16) (sizeof (tNFA_GET_CONFIG) + p_data->get_config.tlv_size))) != NULL)
            {
                p_nfa_get_confg->status = NFA_STATUS_OK;
                p_nfa_get_confg->tlv_size = p_data->get_config.tlv_size;
                memcpy (p_nfa_get_confg->param_tlvs, p_data->get_config.p_param_tlvs, p_data->get_config.tlv_size);
                (*nfa_dm_cb.p_dm_cback) (NFA_DM_GET_CONFIG_EVT, (tNFA_DM_CBACK_DATA *) p_nfa_get_confg);

                GKI_freebuf (p_nfa_get_confg);
                return;
            }
            else
            {
                NFA_TRACE_DEBUG0 ("nfa_dm_nfc_response_cback unable to allocate buffer");
            }
        }

        /* Return result of getconfig to the app */
        dm_cback_data.get_config.status = NFA_STATUS_FAILED;
        (*nfa_dm_cb.p_dm_cback) (NFA_DM_GET_CONFIG_EVT, &dm_cback_data);
        break;

#if (NFC_NFCEE_INCLUDED == TRUE)
    case NFC_NFCEE_DISCOVER_REVT:                /* NFCEE Discover response */
    case NFC_NFCEE_INFO_REVT:                    /* NFCEE Discover Notification */
    case NFC_EE_ACTION_REVT:                     /* EE Action notification */
    case NFC_NFCEE_MODE_SET_REVT:                /* NFCEE Mode Set response */
    case NFC_SET_ROUTING_REVT:                   /* Configure Routing response */
        nfa_ee_proc_evt (event, p_data);
        break;

    case NFC_EE_DISCOVER_REQ_REVT:               /* EE Discover Req notification */
//        if (nfa_dm_is_active() &&
//            (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_DISCOVERY) )
//        {
//            nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_IDLE);
//        }
        nfa_ee_proc_evt (event, p_data);
        break;

#endif

    case NFC_RF_FIELD_REVT:                      /* RF Field information            */
        dm_cback_data.rf_field.status          = NFA_STATUS_OK;
        dm_cback_data.rf_field.rf_field_status = p_data->rf_field.rf_field;
        (*nfa_dm_cb.p_dm_cback) (NFA_DM_RF_FIELD_EVT, &dm_cback_data);
        break;


    case NFC_GET_ROUTING_REVT:                   /* Retrieve Routing response */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if ((p_nfa_get_routing = (tNFA_GET_ROUTING *) GKI_getbuf ((UINT16) (sizeof (tNFA_GET_ROUTING)))) != NULL)
        {
            p_nfa_get_routing->status          = p_data->get_routing.status;
            p_nfa_get_routing->num_tlvs        = p_data->get_routing.num_tlvs;
            p_nfa_get_routing->tlv_size        = p_data->get_routing.tlv_size;
            memcpy(p_nfa_get_routing->param_tlvs,p_data->get_routing.param_tlvs,p_data->get_routing.tlv_size);
            (*nfa_dm_cb.p_dm_cback) (NFA_DM_GET_ROUTE_CONFIG_REVT, (tNFA_DM_CBACK_DATA *) p_nfa_get_routing);
            GKI_freebuf (p_nfa_get_routing);
        }
        else
        {
            NFA_TRACE_DEBUG0 ("nfa_dm_nfc_response_cback unable to allocate buffer");
        }
#endif
        break;

    case NFC_GEN_ERROR_REVT:                     /* generic error command or notification */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if (p_data->status == 0xE4) //STATUS_EMVCO_PCD_COLLISION
        {
            dm_cback_data.status = p_data->status;
            (*nfa_dm_cb.p_dm_cback) (NFA_DM_EMVCO_PCD_COLLISION_EVT, &dm_cback_data);
        }
#endif
        break;

    case NFC_NFCC_RESTART_REVT:                  /* NFCC has been re-initialized */

        if (p_data->status == NFC_STATUS_OK)
        {
            nfa_dm_cb.nfcc_pwr_mode = NFA_DM_PWR_MODE_FULL;
            nfa_dm_cb.flags |= NFA_DM_FLAGS_NFCC_IS_RESTORING;

            /* NFCC will start from IDLE when turned on again */
            nfa_dm_cb.disc_cb.disc_flags &= ~NFA_DM_DISC_FLAGS_W4_RSP;
            nfa_dm_cb.disc_cb.disc_flags &= ~NFA_DM_DISC_FLAGS_W4_NTF;
            nfa_dm_cb.disc_cb.disc_state = NFA_DM_RFST_IDLE;
        }
        else
        {
            nfa_dm_cb.nfcc_pwr_mode = NFA_DM_PWR_MODE_OFF_SLEEP;
        }
        /* Notify NFA submodules change of NFCC power mode */
        nfa_sys_cback_reg_nfcc_power_mode_proc_complete (nfa_dm_nfcc_power_mode_proc_complete_cback);
        nfa_sys_notify_nfcc_power_mode (nfa_dm_cb.nfcc_pwr_mode);
        break;

    case NFC_NFCC_TIMEOUT_REVT:
        conn_evt.status = p_data->status;
        nfa_dm_conn_cback_event_notify (NFA_RECOVERY_EVT, &conn_evt);
        break;

    case NFC_NFCC_TRANSPORT_ERR_REVT:
        NFA_TRACE_DEBUG1 ("flags:0x%08x", nfa_dm_cb.flags);
        dm_cback_evt = (event == NFC_NFCC_TIMEOUT_REVT) ? NFA_DM_NFCC_TIMEOUT_EVT : NFA_DM_NFCC_TRANSPORT_ERR_EVT;
        (*nfa_dm_cb.p_dm_cback) (dm_cback_evt, NULL);
        break;

    case NFC_NFCC_POWER_OFF_REVT:
        nfa_dm_cb.nfcc_pwr_mode = NFA_DM_PWR_MODE_OFF_SLEEP;

        /* Notify NFA submodules change of NFCC power mode */
        nfa_sys_cback_reg_nfcc_power_mode_proc_complete (nfa_dm_nfcc_power_mode_proc_complete_cback);
        nfa_sys_notify_nfcc_power_mode (NFA_DM_PWR_MODE_OFF_SLEEP);
        break;

    case NFC_RF_COMM_PARAMS_UPDATE_REVT:
        conn_evt.status = p_data->status;
        nfa_dm_conn_cback_event_notify (NFA_UPDATE_RF_PARAM_RESULT_EVT, &conn_evt);
        break;


    default:
        break;
    }
}


/*******************************************************************************
**
** Function         nfa_dm_enable
**
** Description      Initialises the NFC device manager
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_enable (tNFA_DM_MSG *p_data)
{
    tNFA_DM_CBACK_DATA dm_cback_data;
    NFA_TRACE_DEBUG0 ("nfa_dm_enable ()");

    /* Check if NFA is already enabled */
    if (!(nfa_dm_cb.flags & NFA_DM_FLAGS_DM_IS_ACTIVE))
    {
        /* Initialize BRCM control block, it musb be called before setting any flags */
        nfa_dm_cb.flags |= (NFA_DM_FLAGS_DM_IS_ACTIVE | NFA_DM_FLAGS_ENABLE_EVT_PEND);
        nfa_sys_cback_reg_enable_complete (nfa_dm_module_init_cback);


        /* Store Enable parameters */
        nfa_dm_cb.p_dm_cback    = p_data->enable.p_dm_cback;
        nfa_dm_cb.p_conn_cback  = p_data->enable.p_conn_cback;

        /* Enable NFC stack */
        NFC_Enable (nfa_dm_nfc_response_cback);
    }
    else
    {
        NFA_TRACE_ERROR0 ("nfa_dm_enable: ERROR ALREADY ENABLED.");
        dm_cback_data.status = NFA_STATUS_ALREADY_STARTED;
        (*(p_data->enable.p_dm_cback)) (NFA_DM_ENABLE_EVT, &dm_cback_data);
    }

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_disable
**
** Description      Disables the NFC device manager
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_disable (tNFA_DM_MSG *p_data)
{
    tNFC_DEACT_TYPE deactivate_type = NFA_DEACTIVATE_TYPE_IDLE;

    NFA_TRACE_DEBUG1 ("nfa_dm_disable (): graceful:%d", p_data->disable.graceful);

    if (p_data->disable.graceful)
    {
        /* if RF discovery is enabled */
        if (nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_ENABLED)
        {
            nfa_dm_cb.disc_cb.disc_flags &= ~NFA_DM_DISC_FLAGS_ENABLED;

            if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE)
            {
                /* if waiting RSP in idle state */
                if (nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_W4_RSP)
                {
                    nfa_dm_cb.disc_cb.disc_flags |= NFA_DM_DISC_FLAGS_DISABLING;
                }
            }
            else
            {
                nfa_dm_cb.disc_cb.disc_flags |= NFA_DM_DISC_FLAGS_DISABLING;
                nfa_dm_disc_sm_execute (NFA_DM_RF_DEACTIVATE_CMD, (tNFA_DM_RF_DISC_DATA *) &deactivate_type);
                if ((nfa_dm_cb.disc_cb.disc_flags & (NFA_DM_DISC_FLAGS_W4_RSP|NFA_DM_DISC_FLAGS_W4_NTF)) == 0)
                {
                    /* not waiting to deactivate, clear the flag now */
                    nfa_dm_cb.disc_cb.disc_flags &= ~NFA_DM_DISC_FLAGS_DISABLING;
                }
            }
        }
        /* Start timeout for graceful shutdown. If timer expires, then force an ungraceful shutdown */
        nfa_sys_start_timer (&nfa_dm_cb.tle, NFA_DM_TIMEOUT_DISABLE_EVT, NFA_DM_DISABLE_TIMEOUT_VAL);
    }

    /* Disable all subsystems other than DM (DM will be disabled after all  */
    /* the other subsystem have been disabled)                              */
    nfa_sys_disable_subsystems (p_data->disable.graceful);
    return (TRUE);
}


/*******************************************************************************
**
** Function         nfa_dm_disable_complete
**
** Description      Called when all NFA subsytems are disabled.
**
**                  NFC core stack can now be disabled.
**
** Returns          void
**
*******************************************************************************/
void nfa_dm_disable_complete (void)
{
    NFA_TRACE_DEBUG0 ("nfa_dm_disable_complete ()");

    if ((nfa_dm_cb.flags & NFA_DM_FLAGS_DM_DISABLING_NFC) == 0)
    {
        NFA_TRACE_DEBUG0 ("nfa_dm_disable_complete (): proceeding with nfc core shutdown.");

        nfa_dm_cb.flags |= NFA_DM_FLAGS_DM_DISABLING_NFC;

        nfa_sys_stop_timer (&nfa_dm_cb.tle);

        /* Free all buffers for NDEF handlers */
        nfa_dm_ndef_dereg_all();

        /* Disable nfc core stack */
        NFC_Disable ();
    }
}

/*******************************************************************************
**
** Function         nfa_dm_set_config
**
** Description      Process set config command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_set_config (tNFA_DM_MSG *p_data)
{
    tNFC_STATUS status;
    UINT8       buff[255];
    UINT8      *p = buff;

    tNFA_DM_CBACK_DATA dm_cback_data;

    if (p_data->setconfig.length + 2 > 255)
    {
        /* Total length of TLV must be less than 256 (1 byte) */
        status = NFC_STATUS_FAILED;
    }
    else
    {
        UINT8_TO_STREAM (p, p_data->setconfig.param_id);
        UINT8_TO_STREAM (p, p_data->setconfig.length);
        ARRAY_TO_STREAM (p, p_data->setconfig.p_data, p_data->setconfig.length)
        status = nfa_dm_check_set_config ((UINT8) (p_data->setconfig.length + 2), buff, TRUE);
    }

    if (status != NFC_STATUS_OK)
    {
        dm_cback_data.set_config.status = NFA_STATUS_INVALID_PARAM;
        (*nfa_dm_cb.p_dm_cback) (NFA_DM_SET_CONFIG_EVT, &dm_cback_data);
    }

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_get_config
**
** Description      Process get config command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_get_config (tNFA_DM_MSG *p_data)
{
    NFC_GetConfig (p_data->getconfig.num_ids, p_data->getconfig.p_pmids);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_conn_cback_event_notify
**
** Description      Notify application of CONN_CBACK event, using appropriate
**                  callback
**
** Returns          nothing
**
*******************************************************************************/
void nfa_dm_conn_cback_event_notify (UINT8 event, tNFA_CONN_EVT_DATA *p_data)
{
    if (nfa_dm_cb.flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE)
    {
        /* Use exclusive RF mode callback */
        if (nfa_dm_cb.p_excl_conn_cback)
            (*nfa_dm_cb.p_excl_conn_cback) (event, p_data);
    }
    else
    {
        (*nfa_dm_cb.p_conn_cback) (event, p_data);
    }
}

/*******************************************************************************
**
** Function         nfa_dm_rel_excl_rf_control_and_notify
**
** Description      Stop exclusive RF control and notify app of
**                  NFA_EXCLUSIVE_RF_CONTROL_STOPPED_EVT
**
** Returns          void
**
*******************************************************************************/
void nfa_dm_rel_excl_rf_control_and_notify (void)
{
    tNFA_CONN_EVT_DATA conn_evt;

    NFA_TRACE_DEBUG0 ("nfa_dm_rel_excl_rf_control_and_notify ()");

    /* Exclusive RF control stopped. Notify app */
    nfa_dm_cb.flags &= ~NFA_DM_FLAGS_EXCL_RF_ACTIVE;

    /* Stop exclusive RF discovery for exclusive RF mode */
    nfa_dm_stop_excl_discovery ();

    /* Notify app that exclusive RF control has stopped */
    conn_evt.status = NFA_STATUS_OK;
    (*nfa_dm_cb.p_excl_conn_cback) (NFA_EXCLUSIVE_RF_CONTROL_STOPPED_EVT, &conn_evt);
    nfa_dm_cb.p_excl_conn_cback = NULL;
    nfa_dm_cb.p_excl_ndef_cback = NULL;
}

/*******************************************************************************
**
** Function         nfa_dm_act_request_excl_rf_ctrl
**
** Description      Request exclusive RF control
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_request_excl_rf_ctrl (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA conn_evt;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_request_excl_rf_ctrl ()");

    if (!nfa_dm_cb.p_excl_conn_cback)
    {
        if (nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_IDLE)
        {
            conn_evt.status = NFA_STATUS_FAILED;
            (*p_data->req_excl_rf_ctrl.p_conn_cback) (NFA_EXCLUSIVE_RF_CONTROL_STARTED_EVT, &conn_evt);
            return TRUE;
        }

        /* Store callbacks */
        nfa_dm_cb.p_excl_conn_cback = p_data->req_excl_rf_ctrl.p_conn_cback;
        nfa_dm_cb.p_excl_ndef_cback = p_data->req_excl_rf_ctrl.p_ndef_cback;

        nfa_dm_cb.flags |= NFA_DM_FLAGS_EXCL_RF_ACTIVE;

        /* start exclusive RF discovery */
        nfa_dm_start_excl_discovery (p_data->req_excl_rf_ctrl.poll_mask,
                                     &p_data->req_excl_rf_ctrl.listen_cfg,
                                     nfa_dm_excl_disc_cback);
    }
    else
    {
        NFA_TRACE_ERROR0 ("Exclusive rf control already requested");

        conn_evt.status = NFA_STATUS_FAILED;
        (*p_data->req_excl_rf_ctrl.p_conn_cback) (NFA_EXCLUSIVE_RF_CONTROL_STARTED_EVT, &conn_evt);
    }

    return TRUE;
}


/*******************************************************************************
**
** Function         nfa_dm_act_release_excl_rf_ctrl
**
** Description      Release exclusive RF control
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_release_excl_rf_ctrl (tNFA_DM_MSG *p_data)
{
    NFA_TRACE_DEBUG0 ("nfa_dm_act_release_excl_rf_ctrl ()");

    /* nfa_dm_rel_excl_rf_control_and_notify() is called when discovery state goes IDLE */
    nfa_dm_cb.disc_cb.disc_flags |= NFA_DM_DISC_FLAGS_STOPPING;

    /* if discover command has been sent in IDLE state and waiting for response
    ** then just wait for responose. Otherwise initiate deactivating.
    */
    if (!(  (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE)
          &&(nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_W4_RSP)  ))
    {
        nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_IDLE);
    }

    if (nfa_dm_cb.disc_cb.kovio_tle.in_use)
        nfa_sys_stop_timer (&nfa_dm_cb.disc_cb.kovio_tle);

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_dm_act_deactivate
**
** Description      Process deactivate command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_deactivate (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA conn_evt;
    tNFA_DEACTIVATE_TYPE deact_type;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_deactivate ()");

    if (  (p_data->deactivate.sleep_mode == FALSE)                 /* Always allow deactivate to IDLE */
        ||(  (nfa_dm_cb.disc_cb.activated_protocol != NFA_PROTOCOL_T1T)      /* Do not allow deactivate to SLEEP for T1T,NFCDEP */
           &&
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
             (
#endif
             (nfa_dm_cb.disc_cb.activated_protocol != NFA_PROTOCOL_NFC_DEP)
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            || gFelicaReaderMode || appl_dta_mode_flag)
#endif
           &&(nfa_dm_cb.disc_cb.activated_protocol != NFC_PROTOCOL_KOVIO)  )  )
    {
        deact_type  = NFA_DEACTIVATE_TYPE_DISCOVERY;
        if (p_data->deactivate.sleep_mode)
        {
            if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_W4_HOST_SELECT)
            {
                /* Deactivate to sleep mode not allowed in this state. */
                deact_type = NFA_DEACTIVATE_TYPE_IDLE;
            }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            else if(appl_dta_mode_flag==TRUE && (nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_LISTEN_SLEEP || nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE))
            {
                deact_type = NFA_DEACTIVATE_TYPE_SLEEP;
            }
#endif
            else if (nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_LISTEN_SLEEP)
            {
                deact_type = NFA_DEACTIVATE_TYPE_SLEEP;
            }
        }
        if ((nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_W4_ALL_DISCOVERIES)
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            ||(nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_T3BT)
#endif
           )
        {
            /* Only deactivate to IDLE is allowed in this state. */
            deact_type = NFA_DEACTIVATE_TYPE_IDLE;
        }

        if (  (nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_NFC_DEP)
            &&((nfa_dm_cb.flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE) == 0x00)
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            && appl_dta_mode_flag != TRUE
#endif
            )
        {
            /* Exclusive RF control doesn't use NFA P2P */
            /* NFA P2P will deactivate NFC link after deactivating LLCP link */
            if (!(nfa_dm_cb.flags & NFA_DM_FLAGS_P2P_PAUSED))
            {
                nfa_p2p_deactivate_llcp ();
            }
            else
            {
                nfa_dm_rf_deactivate (deact_type);
            }
            return (TRUE);
        }
        else
        {

            if (nfa_dm_rf_deactivate (deact_type) == NFA_STATUS_OK)
            {
                if (nfa_dm_cb.disc_cb.kovio_tle.in_use)
                    nfa_sys_stop_timer (&nfa_dm_cb.disc_cb.kovio_tle);
                nfa_rw_stop_presence_check_timer ();
                return (TRUE);
            }
        }
    }

    NFA_TRACE_ERROR0 ("nfa_dm_act_deactivate (): invalid protocol, mode or state");

    /* Notify error to application */
    conn_evt.status = NFA_STATUS_FAILED;
    nfa_dm_conn_cback_event_notify (NFA_DEACTIVATE_FAIL_EVT, &conn_evt);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_power_off_sleep
**
** Description      Process power off sleep mode request
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_power_off_sleep (tNFA_DM_MSG *p_data)
{
    NFA_TRACE_DEBUG0 ("nfa_dm_act_power_off_sleep ()");

    NFC_SetPowerOffSleep ((BOOLEAN) (p_data->hdr.layer_specific));

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_reg_vsc
**
** Description      Process registers VSC callback
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_reg_vsc(tNFA_DM_MSG *p_data)
{
    if (NFC_RegVSCback(p_data->reg_vsc.is_register, p_data->reg_vsc.p_cback) != NFC_STATUS_OK)
    {
        NFA_TRACE_ERROR0 ("NFC_RegVSCback failed");
    }
    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_send_vsc
**
** Description      Send the NCI Vendor Specific command to the NCI command queue
**
** Returns          FALSE (message buffer is NOT freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_send_vsc(tNFA_DM_MSG *p_data)
{
    BT_HDR  *p_cmd = (BT_HDR *)p_data;

    p_cmd->offset   = sizeof (tNFA_DM_API_SEND_VSC) - BT_HDR_SIZE;
    p_cmd->len      = p_data->send_vsc.cmd_params_len;
    NFC_SendVsCommand (p_data->send_vsc.oid, p_cmd, p_data->send_vsc.p_cback);

    /* Most dm action functions return TRUE, so nfa-sys frees the GKI buffer carrying the message,
     * This action function re-use the GKI buffer to send the VSC, so the GKI buffer can not be freed by nfa-sys */
    return (FALSE);
}

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         nfa_dm_act_send_nxp
**
** Description      Send the NXP NCI command to the NCI command queue
**
** Returns          FALSE (message buffer is NOT freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_send_nxp(tNFA_DM_MSG *p_data)
{
    BT_HDR  *p_cmd = (BT_HDR *)p_data;

    p_cmd->offset   = sizeof (tNFA_DM_API_SEND_VSC) - BT_HDR_SIZE;
    p_cmd->len      = p_data->send_vsc.cmd_params_len;
    NFC_SendNxpNciCommand (p_cmd, p_data->send_vsc.p_cback);

    /* Most dm action functions return TRUE, so nfa-sys frees the GKI buffer carrying the message,
     * This action function re-use the GKI buffer to send the VSC, so the GKI buffer can not be freed by nfa-sys */
    return (FALSE);
}
#endif
/*******************************************************************************
**
** Function         nfa_dm_start_polling
**
** Description      Start polling
**
** Returns          tNFA_STATUS
**
*******************************************************************************/
tNFA_STATUS nfa_dm_start_polling (void)
{
    tNFA_STATUS             status;
    tNFA_TECHNOLOGY_MASK    poll_tech_mask;
    tNFA_DM_DISC_TECH_PROTO_MASK poll_disc_mask = 0;

    NFA_TRACE_DEBUG0 ("nfa_dm_start_polling ()");

    poll_tech_mask = nfa_dm_cb.poll_mask;

    /* start RF discovery with discovery callback */
    if (nfa_dm_cb.poll_disc_handle == NFA_HANDLE_INVALID)
    {
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_A)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_PA_T1T;
            poll_disc_mask |= NFA_DM_DISC_MASK_PA_T2T;
            poll_disc_mask |= NFA_DM_DISC_MASK_PA_ISO_DEP;
            poll_disc_mask |= NFA_DM_DISC_MASK_PA_NFC_DEP;
            poll_disc_mask |= NFA_DM_DISC_MASK_P_LEGACY;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_A_ACTIVE)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_PAA_NFC_DEP;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_B)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_PB_ISO_DEP;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_F)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_PF_T3T;
            poll_disc_mask |= NFA_DM_DISC_MASK_PF_NFC_DEP;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_F_ACTIVE)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_PFA_NFC_DEP;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_ISO15693)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_P_ISO15693;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_B_PRIME)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_P_B_PRIME;
        }
        if (poll_tech_mask & NFA_TECHNOLOGY_MASK_KOVIO)
        {
            poll_disc_mask |= NFA_DM_DISC_MASK_P_KOVIO;
        }

        nfa_dm_cb.poll_disc_handle = nfa_dm_add_rf_discover (poll_disc_mask,
                                                             NFA_DM_DISC_HOST_ID_DH,
                                                             nfa_dm_poll_disc_cback);

        if (nfa_dm_cb.poll_disc_handle != NFA_HANDLE_INVALID)
            status = NFA_STATUS_OK;
        else
            status = NFA_STATUS_FAILED;
    }
    else
    {
        status = NFA_STATUS_OK;
    }

    return (status);
}

/*******************************************************************************
**
** Function         nfa_dm_act_enable_polling
**
** Description      Process enable polling command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_enable_polling (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_enable_polling ()");

    if (  (!(nfa_dm_cb.flags & NFA_DM_FLAGS_POLLING_ENABLED))
        &&(!(nfa_dm_cb.flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE)  )  )
    {
        nfa_dm_cb.poll_mask = p_data->enable_poll.poll_mask;

        if (nfa_dm_start_polling () == NFA_STATUS_OK)
        {
            nfa_dm_cb.flags |= NFA_DM_FLAGS_POLLING_ENABLED;

            evt_data.status = NFA_STATUS_OK;
            nfa_dm_conn_cback_event_notify (NFA_POLL_ENABLED_EVT, &evt_data);
            return (TRUE);
        }
    }
    else
    {
        NFA_TRACE_ERROR0 ("nfa_dm_act_enable_polling (): already started");
    }

    /* send NFA_POLL_ENABLED_EVT with NFA_STATUS_FAILED */
    evt_data.status = NFA_STATUS_FAILED;
    nfa_dm_conn_cback_event_notify (NFA_POLL_ENABLED_EVT, &evt_data);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_deactivate_polling
**
** Description      Deactivate any polling state
**
** Returns          TRUE if need to wait for deactivation
**
*******************************************************************************/
static BOOLEAN nfa_dm_deactivate_polling (void)
{
    NFA_TRACE_DEBUG0 ("nfa_dm_deactivate_polling ()");

    if (  (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_W4_ALL_DISCOVERIES)
        ||(nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_W4_HOST_SELECT)  )
    {
        nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_IDLE);
        return FALSE;
    }
    else if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE)
    {
        if (nfa_dm_cb.disc_cb.activated_protocol == NFC_PROTOCOL_NFC_DEP)
        {
            /* NFA P2P will deactivate NFC link after deactivating LLCP link */
            nfa_p2p_deactivate_llcp ();
        }
        else
        {
            nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_IDLE);
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*******************************************************************************
**
** Function         nfa_dm_act_disable_polling
**
** Description      Process disable polling command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_disable_polling (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_disable_polling ()");

    if (nfa_dm_cb.poll_disc_handle != NFA_HANDLE_INVALID)
    {
        nfa_dm_cb.flags &= ~NFA_DM_FLAGS_POLLING_ENABLED;

        if (nfa_dm_deactivate_polling () == FALSE)
        {
            nfa_dm_delete_rf_discover (nfa_dm_cb.poll_disc_handle);
            nfa_dm_cb.poll_disc_handle = NFA_HANDLE_INVALID;

            evt_data.status = NFA_STATUS_OK;
            nfa_dm_conn_cback_event_notify (NFA_POLL_DISABLED_EVT, &evt_data);
        }
        else
        {
            nfa_dm_cb.flags |= NFA_DM_FLAGS_SEND_POLL_STOP_EVT;
        }
    }
    else
    {
        evt_data.status = NFA_STATUS_FAILED;
        nfa_dm_conn_cback_event_notify (NFA_POLL_DISABLED_EVT, &evt_data);
    }

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_enable_listening
**
** Description      Process enable listening command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_enable_listening (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_enable_listening ()");

    nfa_dm_cb.flags &= ~NFA_DM_FLAGS_LISTEN_DISABLED;
    evt_data.status = NFA_STATUS_OK;
    nfa_dm_conn_cback_event_notify (NFA_LISTEN_ENABLED_EVT, &evt_data);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_disable_listening
**
** Description      Process disable listening command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_disable_listening (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_disable_listening ()");

    nfa_dm_cb.flags |= NFA_DM_FLAGS_LISTEN_DISABLED;
    evt_data.status = NFA_STATUS_OK;
    nfa_dm_conn_cback_event_notify (NFA_LISTEN_DISABLED_EVT, &evt_data);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_pause_p2p
**
** Description      Process Pause P2P command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_pause_p2p (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_pause_p2p ()");

    nfa_dm_cb.flags |= NFA_DM_FLAGS_P2P_PAUSED;
    evt_data.status = NFA_STATUS_OK;
    nfa_dm_conn_cback_event_notify (NFA_P2P_PAUSED_EVT, &evt_data);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_resume_p2p
**
** Description      Process resume P2P command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_resume_p2p (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_resume_p2p ()");

    nfa_dm_cb.flags &= ~NFA_DM_FLAGS_P2P_PAUSED;
    evt_data.status = NFA_STATUS_OK;
    nfa_dm_conn_cback_event_notify (NFA_P2P_RESUMED_EVT, &evt_data);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_send_raw_frame
**
** Description      Send an raw frame on RF link
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_send_raw_frame (tNFA_DM_MSG *p_data)
{
    tNFC_STATUS status = NFC_STATUS_FAILED;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_send_raw_frame ()");

    /* If NFC link is activated */
    if (  (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_POLL_ACTIVE)
        ||(nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_LISTEN_ACTIVE)  )
    {
        nfa_dm_cb.flags |= NFA_DM_FLAGS_RAW_FRAME;
        NFC_SetReassemblyFlag (FALSE);
        /* If not in exclusive mode, and not activated for LISTEN, then forward raw data to NFA_RW to send */
        if (  !(nfa_dm_cb.flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE)
            &&!(nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_LISTEN_ACTIVE)
            &&(  (nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_T1T)
               ||(nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_T2T)
               ||(nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_T3T)
               ||(nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_ISO_DEP)
               ||(nfa_dm_cb.disc_cb.activated_protocol == NFA_PROTOCOL_ISO15693)  )  )
        {
            /* if RW is checking presence then it will put into pending queue */
            status = nfa_rw_send_raw_frame ((BT_HDR*)p_data);
        }
        else
        {
            status = NFC_SendData (NFC_RF_CONN_ID, (BT_HDR*) p_data);
            if (status != NFC_STATUS_OK)
            {
                NFC_SetReassemblyFlag (TRUE);
            }
            /* Already freed or NCI layer will free buffer */
            return FALSE;
        }
    }

    if (status == NFC_STATUS_FAILED)
    {
        NFC_SetReassemblyFlag (TRUE);
        /* free the buffer */
        return TRUE;
    }
    else
    {
        /* NCI layer will free buffer */
        return FALSE;
    }
}

/*******************************************************************************
**
** Function         nfa_dm_set_p2p_listen_tech
**
** Description      Notify change of P2P listen technologies to NFA P2P
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_set_p2p_listen_tech (tNFA_DM_MSG *p_data)
{
    NFA_TRACE_DEBUG1 ("nfa_dm_set_p2p_listen_tech ()  tech_mask = %d",
                      p_data->set_p2p_listen_tech.tech_mask);

    nfa_p2p_update_listen_tech (p_data->set_p2p_listen_tech.tech_mask);
    nfa_dm_conn_cback_event_notify (NFA_SET_P2P_LISTEN_TECH_EVT, NULL);

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_start_rf_discovery
**
** Description      Process start RF discovery command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_start_rf_discovery (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_start_rf_discovery ()");

    if (nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_ENABLED)
    {
        evt_data.status = NFA_STATUS_OK;
        nfa_dm_conn_cback_event_notify (NFA_RF_DISCOVERY_STARTED_EVT, &evt_data);
    }
    else if (nfa_dm_cb.disc_cb.disc_state != NFA_DM_RFST_IDLE)
    {
        evt_data.status = NFA_STATUS_SEMANTIC_ERROR;
        nfa_dm_conn_cback_event_notify (NFA_RF_DISCOVERY_STARTED_EVT, &evt_data);
    }
    else
    {
        nfa_dm_cb.disc_cb.disc_flags |= (NFA_DM_DISC_FLAGS_ENABLED|NFA_DM_DISC_FLAGS_NOTIFY);
        nfa_dm_start_rf_discover ();
    }

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_stop_rf_discovery
**
** Description      Process stop RF discovery command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_stop_rf_discovery (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_stop_rf_discovery ()");

    if (!(nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_ENABLED) ||
        (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_IDLE) )
    {
        nfa_dm_cb.disc_cb.disc_flags &= ~NFA_DM_DISC_FLAGS_ENABLED;

        /* if discover command has been sent in IDLE state and waiting for response */
        if (nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_W4_RSP)
        {
            nfa_dm_cb.disc_cb.disc_flags |= NFA_DM_DISC_FLAGS_STOPPING;
        }
        else
        {
            evt_data.status = NFA_STATUS_OK;
            nfa_dm_conn_cback_event_notify (NFA_RF_DISCOVERY_STOPPED_EVT, &evt_data);
        }
    }
    else
    {
        nfa_dm_cb.disc_cb.disc_flags &= ~NFA_DM_DISC_FLAGS_ENABLED;
        nfa_dm_cb.disc_cb.disc_flags |= NFA_DM_DISC_FLAGS_STOPPING;

        if (nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_IDLE) == NFA_STATUS_OK)
        {
            if (nfa_dm_cb.disc_cb.kovio_tle.in_use)
                nfa_sys_stop_timer (&nfa_dm_cb.disc_cb.kovio_tle);
            nfa_rw_stop_presence_check_timer ();
        }
    }
    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_set_rf_disc_duration
**
** Description      Set duration for RF discovery
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_set_rf_disc_duration (tNFA_DM_MSG *p_data)
{
    nfa_dm_cb.disc_cb.disc_duration = p_data->disc_duration.rf_disc_dur_ms;
    return (TRUE);
}
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         nfa_dm_act_get_rf_disc_duration
**
** Description      Get duration for RF discovery
**
** Returns          UINT16
**
*******************************************************************************/
UINT16 nfa_dm_act_get_rf_disc_duration ( )
{
    return (nfa_dm_cb.disc_cb.disc_duration);
}
#endif
/*******************************************************************************
**
** Function         nfa_dm_act_select
**
** Description      Process RF select command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_select (tNFA_DM_MSG *p_data)
{
    NFA_TRACE_DEBUG0 ("nfa_dm_act_select ()");

    nfa_dm_rf_discover_select (p_data->select.rf_disc_id,
                               p_data->select.protocol,
                               p_data->select.rf_interface);
    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_update_rf_params
**
** Description      Process update RF communication parameters command
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_update_rf_params (tNFA_DM_MSG *p_data)
{
    tNFA_CONN_EVT_DATA conn_evt;

    NFA_TRACE_DEBUG0 ("nfa_dm_act_update_rf_params ()");

    if (NFC_UpdateRFCommParams (&p_data->update_rf_params.params) != NFC_STATUS_OK)
    {
        conn_evt.status = NFA_STATUS_FAILED;
        nfa_dm_conn_cback_event_notify (NFA_UPDATE_RF_PARAM_RESULT_EVT, &conn_evt);
    }

    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_disable_timeout
**
** Description      timeout on disable process. Shutdown immediately
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_act_disable_timeout (tNFA_DM_MSG *p_data)
{
    tNFA_DM_API_DISABLE disable;

    disable.graceful = FALSE;
    nfa_dm_disable ((tNFA_DM_MSG *) &disable);
    return (TRUE);
}

/*******************************************************************************
**
** Function         nfa_dm_act_conn_cback_notify
**
** Description      Notify app of reader/writer/ndef events
**
** Returns          nothing
**
*******************************************************************************/
void nfa_dm_act_conn_cback_notify (UINT8 event, tNFA_CONN_EVT_DATA *p_data)
{
    NFA_TRACE_DEBUG1 ("nfa_dm_act_conn_cback_notify (): event:0x%X", event);

    /* Notify event using appropriate CONN_CBACK */
    nfa_dm_conn_cback_event_notify (event, p_data);

    /* If not in exclusive RF mode, then read NDEF message from tag (if automatic reading is enabled) */
    if (!(nfa_dm_cb.flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE))
    {
        if (  (event == NFA_NDEF_DETECT_EVT)
            &&(nfa_dm_cb.flags & NFA_DM_FLAGS_AUTO_READING_NDEF)  )
        {
            /* read NDEF message from tag */
            if (p_data->ndef_detect.status == NFA_STATUS_OK)
            {
                NFA_RwReadNDef ();
            }
            else if (p_data->ndef_detect.status == NFA_STATUS_FAILED)
            {
                nfa_dm_cb.flags &= ~NFA_DM_FLAGS_AUTO_READING_NDEF;
            }
            /* ignore NFA_STATUS_BUSY */
        }
        else if (  (event == NFA_READ_CPLT_EVT)
                 &&(nfa_dm_cb.flags & NFA_DM_FLAGS_AUTO_READING_NDEF))
        {
            /* reading NDEF message is done */
            nfa_dm_cb.flags &= ~NFA_DM_FLAGS_AUTO_READING_NDEF;
        }
    }
}

/*******************************************************************************
**
** Function         nfa_dm_act_data_cback
**
** Description      Processing data from RF link
**
** Returns          None
**
*******************************************************************************/
static void nfa_dm_act_data_cback (UINT8 conn_id, tNFC_CONN_EVT event, tNFC_CONN *p_data)
{
    BT_HDR             *p_msg;
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG1 ("nfa_dm_act_data_cback (): event = 0x%X", event);

    if (event == NFC_DATA_CEVT)
    {
        p_msg = (BT_HDR *) p_data->data.p_data;

        if (p_msg)
        {
            evt_data.data.status = p_data->data.status;
            evt_data.data.p_data = (UINT8 *) (p_msg + 1) + p_msg->offset;
            evt_data.data.len    = p_msg->len;

            nfa_dm_conn_cback_event_notify (NFA_DATA_EVT, &evt_data);

            GKI_freebuf (p_msg);
        }
        else
        {
            NFA_TRACE_ERROR0 ("nfa_dm_act_data_cback (): received NFC_DATA_CEVT with NULL data pointer");
        }
    }
    else if (event == NFC_DEACTIVATE_CEVT)
    {
        NFC_SetStaticRfCback (NULL);
    }
}

/*******************************************************************************
**
** Function         nfa_dm_excl_disc_cback
**
** Description      Processing event from discovery callback
**
** Returns          None
**
*******************************************************************************/
static void nfa_dm_excl_disc_cback (tNFA_DM_RF_DISC_EVT event, tNFC_DISCOVER *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG1 ("nfa_dm_excl_disc_cback (): event:0x%02X", event);

    switch (event)
    {
    case NFA_DM_RF_DISC_START_EVT:
        evt_data.status = NFA_STATUS_OK;
        nfa_dm_conn_cback_event_notify (NFA_EXCLUSIVE_RF_CONTROL_STARTED_EVT, &evt_data);
        break;

    case NFA_DM_RF_DISC_ACTIVATED_EVT:
        if (nfa_dm_cb.disc_cb.activated_tech_mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            /* store SEL_RES response */
            nfa_dm_cb.disc_cb.activated_sel_res = p_data->activate.rf_tech_param.param.pa.sel_rsp;
        }

        if (nfa_dm_cb.disc_cb.disc_state == NFA_DM_RFST_LISTEN_ACTIVE)
        {
            /* Set data callback to receive raw frame */
            NFC_SetStaticRfCback (nfa_dm_act_data_cback);

            memset (&(evt_data.activated.params), 0x00, sizeof (tNFA_TAG_PARAMS));
            memcpy (&(evt_data.activated.activate_ntf), &(p_data->activate), sizeof (tNFC_ACTIVATE_DEVT));

            nfa_dm_conn_cback_event_notify (NFA_ACTIVATED_EVT, &evt_data);
        }
        else
        {
            /* holding activation notification until sub-module is ready */
            nfa_dm_cb.p_activate_ntf = (UINT8*) GKI_getbuf (sizeof (tNFC_ACTIVATE_DEVT));

            if (nfa_dm_cb.p_activate_ntf)
            {
                memcpy (nfa_dm_cb.p_activate_ntf,
                        &(p_data->activate),
                        sizeof (tNFC_ACTIVATE_DEVT));

                if (  (nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T1T)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T2T)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T3T)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_ISO_DEP)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFA_PROTOCOL_ISO15693)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_KOVIO)  )
                {
                    /* Notify NFA tag sub-system */
                    nfa_rw_proc_disc_evt (NFA_DM_RF_DISC_ACTIVATED_EVT, p_data, FALSE);
                }
                else /* if NFC-DEP, ISO-DEP with frame interface or others */
                {
                    /* Set data callback to receive raw frame */
                    NFC_SetStaticRfCback (nfa_dm_act_data_cback);
                    nfa_dm_notify_activation_status (NFA_STATUS_OK, NULL);
                }
            }
            else
            {
                /* deactivate and restart RF discovery */
                nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_DISCOVERY);
            }
        }
        break;

    case NFA_DM_RF_DISC_DEACTIVATED_EVT:

        /* if deactivated to idle or discovery */
        if (  (p_data->deactivate.type == NFC_DEACTIVATE_TYPE_IDLE)
            ||(p_data->deactivate.type == NFC_DEACTIVATE_TYPE_DISCOVERY)  )
        {
            /* clear stored NFCID/UID/KOVIO bar code */
            nfa_dm_cb.activated_nfcid_len = 0;
        }

        if (nfa_dm_cb.disc_cb.activated_protocol != NFC_PROTOCOL_NFC_DEP)
        {
            /* Notify NFA RW sub-systems */
            nfa_rw_proc_disc_evt (NFA_DM_RF_DISC_DEACTIVATED_EVT, NULL, FALSE);
        }

        /* if deactivated as sleep mode */
        if (  (p_data->deactivate.type == NFC_DEACTIVATE_TYPE_SLEEP)
            ||(p_data->deactivate.type == NFC_DEACTIVATE_TYPE_SLEEP_AF)  )
        {
            evt_data.deactivated.type = NFA_DEACTIVATE_TYPE_SLEEP;
        }
        else
        {
            evt_data.deactivated.type = NFA_DEACTIVATE_TYPE_IDLE;
        }

        /* notify deactivation to upper layer */
        nfa_dm_conn_cback_event_notify (NFA_DEACTIVATED_EVT, &evt_data);

        /* clean up SEL_RES response */
        nfa_dm_cb.disc_cb.activated_sel_res = 0;
        break;

    default:
        NFA_TRACE_ERROR0 ("Unexpected event");
        break;
    }
}

/*******************************************************************************
**
** Function         nfa_dm_poll_disc_cback
**
** Description      Processing event from discovery callback
**
** Returns          None
**
*******************************************************************************/
static void nfa_dm_poll_disc_cback (tNFA_DM_RF_DISC_EVT event, tNFC_DISCOVER *p_data)
{
    tNFA_CONN_EVT_DATA evt_data;

    NFA_TRACE_DEBUG1 ("nfa_dm_poll_disc_cback (): event:0x%02X", event);

    switch (event)
    {
    case NFA_DM_RF_DISC_START_EVT:
        break;

    case NFA_DM_RF_DISC_ACTIVATED_EVT:

        if (nfa_dm_cb.disc_cb.activated_tech_mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            /* store SEL_RES response */
            nfa_dm_cb.disc_cb.activated_sel_res = p_data->activate.rf_tech_param.param.pa.sel_rsp;
        }

        /* holding activation notification until sub-module is ready */
        nfa_dm_cb.p_activate_ntf = (UINT8*) GKI_getbuf (sizeof (tNFC_ACTIVATE_DEVT));

        if (nfa_dm_cb.p_activate_ntf)
        {
            memcpy (nfa_dm_cb.p_activate_ntf,
                    &(p_data->activate),
                    sizeof (tNFC_ACTIVATE_DEVT));

            if ((nfa_dm_cb.disc_cb.activated_protocol     == NFC_PROTOCOL_NFC_DEP)
                &&((nfa_dm_cb.disc_cb.activated_rf_interface == NFC_INTERFACE_NFC_DEP)))
            {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                /*For P2P mode(Default DTA mode) open Raw channel to bypass LLCP layer. For LLCP DTA mode activate LLCP*/
                if ((appl_dta_mode_flag == 1) && (nfa_dm_cb.eDtaMode == NFA_DTA_DEFAULT_MODE))
                {
                    /* Open raw channel in case of p2p for DTA testing */
                    NFC_SetStaticRfCback (nfa_dm_act_data_cback);
                    nfa_dm_notify_activation_status (NFA_STATUS_OK, NULL);

                }
                else
                {
#endif
                    if (!(nfa_dm_cb.flags & NFA_DM_FLAGS_P2P_PAUSED))
                    {
                        /* activate LLCP */
                        nfa_p2p_activate_llcp (p_data);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                        if (nfa_dm_cb.p_activate_ntf)
                        {
                            GKI_freebuf (nfa_dm_cb.p_activate_ntf);
                            nfa_dm_cb.p_activate_ntf = NULL;
                        }
#endif
                    }
                    else
                    {
                        NFA_TRACE_DEBUG0 ("P2P is paused");
                        nfa_dm_notify_activation_status (NFA_STATUS_OK, NULL);
                    }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                }
#endif
            }
            else if (  (nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T1T)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T2T)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T3T)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_ISO_DEP)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_15693)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_KOVIO)
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_MIFARE)
                     ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T3BT)
#endif
                    )
            {
                /* Notify NFA tag sub-system */
                nfa_rw_proc_disc_evt (NFA_DM_RF_DISC_ACTIVATED_EVT, p_data, TRUE);
            }
            else /* if NFC-DEP/ISO-DEP with frame interface */
            {
                /* Set data callback to receive raw frame */
                NFC_SetStaticRfCback (nfa_dm_act_data_cback);
                nfa_dm_notify_activation_status (NFA_STATUS_OK, NULL);
            }
        }
        else
        {
            /* deactivate and restart RF discovery */
            nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_DISCOVERY);
        }
        break;

    case NFA_DM_RF_DISC_DEACTIVATED_EVT:

        /* if deactivated to idle or discovery */
        if (  (p_data->deactivate.type == NFC_DEACTIVATE_TYPE_IDLE)
            ||(p_data->deactivate.type == NFC_DEACTIVATE_TYPE_DISCOVERY)  )
        {
            /* clear stored NFCID/UID/KOVIO bar code */
            nfa_dm_cb.activated_nfcid_len = 0;
        }

        if (  (nfa_dm_cb.disc_cb.activated_protocol     == NFC_PROTOCOL_NFC_DEP)
            && ((nfa_dm_cb.disc_cb.activated_rf_interface == NFC_INTERFACE_NFC_DEP)))
        {
            /*
            ** If LLCP link is not deactivated yet,
            ** LLCP will receive deactivation ntf through data callback.
            ** NFA P2P will receive callback event from LLCP.
            */
        }
        else
        {
            /* Notify NFA RW sub-systems */
            nfa_rw_proc_disc_evt (NFA_DM_RF_DISC_DEACTIVATED_EVT, NULL, TRUE);
        }

        /* if NFA sent NFA_ACTIVATED_EVT earlier */
        if (nfa_dm_cb.flags & NFA_DM_FLAGS_SEND_DEACTIVATED_EVT)
        {
            nfa_dm_cb.flags &= ~NFA_DM_FLAGS_SEND_DEACTIVATED_EVT;

            /* if deactivated as sleep mode */
            if (  (p_data->deactivate.type == NFC_DEACTIVATE_TYPE_SLEEP)
                ||(p_data->deactivate.type == NFC_DEACTIVATE_TYPE_SLEEP_AF)  )
            {
                evt_data.deactivated.type = NFA_DEACTIVATE_TYPE_SLEEP;
            }
            else
            {
                evt_data.deactivated.type = NFA_DEACTIVATE_TYPE_IDLE;
            }
            /* notify deactivation to application */
            nfa_dm_conn_cback_event_notify (NFA_DEACTIVATED_EVT, &evt_data);
        }

        /* clean up SEL_RES response */
        nfa_dm_cb.disc_cb.activated_sel_res = 0;

        if (!(nfa_dm_cb.flags & NFA_DM_FLAGS_POLLING_ENABLED))
        {
            /* deregister discovery callback from NFA DM Discovery */
            nfa_dm_delete_rf_discover (nfa_dm_cb.poll_disc_handle);
            nfa_dm_cb.poll_disc_handle = NFA_HANDLE_INVALID;

            /* this is for disable polling */
            if (nfa_dm_cb.flags & NFA_DM_FLAGS_SEND_POLL_STOP_EVT)
            {
                nfa_dm_cb.flags &= ~NFA_DM_FLAGS_SEND_POLL_STOP_EVT;

                evt_data.status = NFA_STATUS_OK;
                nfa_dm_conn_cback_event_notify (NFA_POLL_DISABLED_EVT, &evt_data);
            }
        }
        break;
    }
}

/*******************************************************************************
** Function         nfa_dm_poll_disc_cback_dta_wrapper
**
** Description      Accessing the nfa_dm_poll_disc_cback for DTA wrapper
**
** Returns          None
**
*******************************************************************************/
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
void nfa_dm_poll_disc_cback_dta_wrapper(tNFA_DM_RF_DISC_EVT event, tNFC_DISCOVER *p_data)
{
    nfa_dm_poll_disc_cback(event, p_data);
}
#endif

/*******************************************************************************
**
** Function         nfa_dm_notify_activation_status
**
** Description      Processing activation status from sub-modules
**
** Returns          None
**
*******************************************************************************/
void nfa_dm_notify_activation_status (tNFA_STATUS status, tNFA_TAG_PARAMS *p_params)
{
    tNFA_CONN_EVT_DATA  evt_data;
    tNFC_RF_TECH_PARAMS *p_tech_params;
    UINT8               *p_nfcid = NULL, nfcid_len;

    NFA_TRACE_DEBUG1 ("nfa_dm_notify_activation_status (): status:0x%X", status);

    if (!nfa_dm_cb.p_activate_ntf)
    {
        /* this is for NFA P2P listen */
        return;
    }

    if (status == NFA_STATUS_OK)
    {
        /* notify NFC link activation */
        memcpy ( &(evt_data.activated.activate_ntf),
                 nfa_dm_cb.p_activate_ntf,
                 sizeof (tNFC_ACTIVATE_DEVT));

        p_tech_params = &evt_data.activated.activate_ntf.rf_tech_param;

        memset (&(evt_data.activated.params), 0x00, sizeof (tNFA_TAG_PARAMS));
        if (p_params)
        {
            memcpy (&(evt_data.activated.params),
                    p_params,
                    sizeof (tNFA_TAG_PARAMS));
        }

        /* get length of NFCID and location */
        if (p_tech_params->mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            if(p_tech_params->param.pa.nfcid1_len == 0)
            {
                nfcid_len = sizeof(p_params->t1t.uid);
                p_nfcid   = p_params->t1t.uid;
                evt_data.activated.activate_ntf.rf_tech_param.param.pa.nfcid1_len = nfcid_len;
                memcpy (evt_data.activated.activate_ntf.rf_tech_param.param.pa.nfcid1,p_nfcid,nfcid_len);
            }
            else
            {
#endif
                nfcid_len = p_tech_params->param.pa.nfcid1_len;
                p_nfcid   = p_tech_params->param.pa.nfcid1;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            }
#endif
        }
        else if (p_tech_params->mode == NFC_DISCOVERY_TYPE_POLL_B)
        {
            nfcid_len = NFC_NFCID0_MAX_LEN;
            p_nfcid   = p_tech_params->param.pb.nfcid0;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            if(nfa_dm_cb.disc_cb.activated_protocol == NFC_PROTOCOL_T3BT)
            {
                if(p_tech_params->param.pb.pupiid_len != 0)
                {
                    tNFC_ACTIVATE_DEVT *activate_ntf = (tNFC_ACTIVATE_DEVT*)nfa_dm_cb.p_activate_ntf;
                    p_nfcid = activate_ntf->rf_tech_param.param.pb.pupiid;
                    nfcid_len = activate_ntf->rf_tech_param.param.pb.pupiid_len;
                    NFA_TRACE_DEBUG1 ("nfa_dm_notify_activation_status (): update pupi_len=%x", nfcid_len);
                    memcpy (evt_data.activated.activate_ntf.rf_tech_param.param.pb.pupiid, p_nfcid, nfcid_len);
                }
            }
#endif
        }
        else if (p_tech_params->mode == NFC_DISCOVERY_TYPE_POLL_F)
        {
            nfcid_len = NFC_NFCID2_LEN;
            p_nfcid   = p_tech_params->param.pf.nfcid2;
        }
        else if (p_tech_params->mode == NFC_DISCOVERY_TYPE_POLL_ISO15693)
        {
            nfcid_len = NFC_ISO15693_UID_LEN;
            p_nfcid   = p_tech_params->param.pi93.uid;
        }
        else if (p_tech_params->mode == NFC_DISCOVERY_TYPE_POLL_KOVIO)
        {
            nfcid_len = p_tech_params->param.pk.uid_len;
            p_nfcid   = p_tech_params->param.pk.uid;
        }
        else
        {
            nfcid_len = 0;
        }

        /*
        ** If not in exlusive RF mode, and
        **      P2P activation, then push default NDEF message through SNEP
        **      TAG activation, then read NDEF message
        */
        if (nfa_dm_cb.disc_cb.activated_protocol == NFC_PROTOCOL_NFC_DEP)
        {
            /*
            ** Default NDEF message will be put to NFC Forum defualt SNEP server
            ** after receiving NFA_LLCP_ACTIVATED_EVT.
            */
        }
        else if (!(nfa_dm_cb.flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE))
        {
            /*
            ** if the same tag is activated then do not perform auto NDEF detection.
            ** Application may put a tag into sleep mode and reactivate the same tag.
            */

            if (  (p_tech_params->mode != nfa_dm_cb.activated_tech_mode)
                ||(nfcid_len != nfa_dm_cb.activated_nfcid_len)
                ||(memcmp (p_nfcid, nfa_dm_cb.activated_nfcid, nfcid_len)))
            {
                if (  (nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T1T)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T2T)
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFC_PROTOCOL_T3T)
                    ||(  (nfa_dm_cb.disc_cb.activated_protocol == NFC_PROTOCOL_ISO_DEP)
                       &&(nfa_dm_cb.disc_cb.activated_rf_interface == NFC_INTERFACE_ISO_DEP)  )
                    ||(nfa_dm_cb.disc_cb.activated_protocol  == NFA_PROTOCOL_ISO15693)  )
                {
                    if (p_nfa_dm_cfg->auto_detect_ndef)
                    {
                        if (p_nfa_dm_cfg->auto_read_ndef)
                        {
                            nfa_dm_cb.flags |= NFA_DM_FLAGS_AUTO_READING_NDEF;
                        }
                        NFA_RwDetectNDef ();
                    }
                    else if (p_nfa_dm_cfg->auto_read_ndef)
                    {
                        NFA_RwReadNDef ();
                    }
                }
            }
        }

        /* store activated tag information */
        nfa_dm_cb.activated_tech_mode = p_tech_params->mode;
        nfa_dm_cb.activated_nfcid_len = nfcid_len;
        if (nfcid_len)
            memcpy (nfa_dm_cb.activated_nfcid, p_nfcid, nfcid_len);

        nfa_dm_cb.flags |= NFA_DM_FLAGS_SEND_DEACTIVATED_EVT;
        if (!(nfa_dm_cb.disc_cb.disc_flags & NFA_DM_DISC_FLAGS_CHECKING))
            nfa_dm_conn_cback_event_notify (NFA_ACTIVATED_EVT, &evt_data);
    }
    else
    {
        /* if NFC_DEP, NFA P2P will deactivate */
        if (nfa_dm_cb.disc_cb.activated_protocol != NFC_PROTOCOL_NFC_DEP)
        {
            nfa_dm_rf_deactivate (NFA_DEACTIVATE_TYPE_DISCOVERY);
        }
    }

    GKI_freebuf (nfa_dm_cb.p_activate_ntf);
    nfa_dm_cb.p_activate_ntf = NULL;
}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         nfa_dm_nfc_revt_2_str
**
** Description      convert nfc revt to string
**
*******************************************************************************/
char *nfa_dm_nfc_revt_2_str (tNFC_RESPONSE_EVT event)
{
    switch (event) {
    case NFC_ENABLE_REVT:
        return "NFC_ENABLE_REVT";

    case NFC_DISABLE_REVT:
        return "NFC_DISABLE_REVT";

    case NFC_SET_CONFIG_REVT:
        return "NFC_SET_CONFIG_REVT";

    case NFC_GET_CONFIG_REVT:
        return "NFC_GET_CONFIG_REVT";

    case NFC_NFCEE_DISCOVER_REVT:
        return "NFC_NFCEE_DISCOVER_REVT";

    case NFC_NFCEE_INFO_REVT:
        return "NFC_NFCEE_INFO_REVT";

    case NFC_NFCEE_MODE_SET_REVT:
        return "NFC_NFCEE_MODE_SET_REVT";

    case NFC_RF_FIELD_REVT:
        return "NFC_RF_FIELD_REVT";

    case NFC_EE_ACTION_REVT:
        return "NFC_EE_ACTION_REVT";

    case NFC_EE_DISCOVER_REQ_REVT:
        return "NFC_EE_DISCOVER_REQ_REVT";

    case NFC_SET_ROUTING_REVT:
        return "NFC_SET_ROUTING_REVT";

    case NFC_GET_ROUTING_REVT:
        return "NFC_GET_ROUTING_REVT";

    case NFC_GEN_ERROR_REVT:
        return "NFC_GEN_ERROR_REVT";

    case NFC_NFCC_RESTART_REVT:
        return "NFC_NFCC_RESTART_REVT";

    case NFC_NFCC_TIMEOUT_REVT:
        return "NFC_NFCC_TIMEOUT_REVT";

    case NFC_NFCC_TRANSPORT_ERR_REVT:
        return "NFC_NFCC_TRANSPORT_ERR_REVT";

    case NFC_NFCC_POWER_OFF_REVT:
        return "NFC_NFCC_POWER_OFF_REVT";

    default:
        return "unknown revt";
        break;
    }
}
#endif /* BT_VERBOSE */
