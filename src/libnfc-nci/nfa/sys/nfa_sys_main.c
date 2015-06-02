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
 *  This is the main implementation file for the NFA system manager.
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfa_sys_ptim.h"
#include "nfa_dm_int.h"

/* protocol timer update period, in milliseconds */
#ifndef NFA_SYS_TIMER_PERIOD
#define NFA_SYS_TIMER_PERIOD            10
#endif

/* system manager control block definition */
#if NFA_DYNAMIC_MEMORY == FALSE
tNFA_SYS_CB nfa_sys_cb = {0};   /* nfa_sys control block. statically initialize 'flags' field to 0 */
#endif

/*******************************************************************************
**
** Function         nfa_sys_init
**
** Description      NFA initialization; called from task initialization.
**
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_init (void)
{
    memset (&nfa_sys_cb, 0, sizeof (tNFA_SYS_CB));
    nfa_sys_cb.flags |= NFA_SYS_FL_INITIALIZED;
    nfa_sys_ptim_init (&nfa_sys_cb.ptim_cb, NFA_SYS_TIMER_PERIOD, p_nfa_sys_cfg->timer);
    nfa_sys_cb.trace_level = p_nfa_sys_cfg->trace_level;
}




/*******************************************************************************
**
** Function         nfa_sys_event
**
** Description      BTA event handler; called from task event handler.
**
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_event (BT_HDR *p_msg)
{
    UINT8       id;
    BOOLEAN     freebuf = TRUE;

    NFA_TRACE_EVENT1 ("NFA got event 0x%04X", p_msg->event);

    /* get subsystem id from event */
    id = (UINT8) (p_msg->event >> 8);

    /* verify id and call subsystem event handler */
    if ((id < NFA_ID_MAX) && (nfa_sys_cb.is_reg[id]))
    {
        freebuf = (*nfa_sys_cb.reg[id]->evt_hdlr) (p_msg);
    }
    else
    {
        NFA_TRACE_WARNING1 ("NFA got unregistered event id %d", id);
    }

    if (freebuf)
    {
        GKI_freebuf (p_msg);
    }
}

/*******************************************************************************
**
** Function         nfa_sys_timer_update
**
** Description      Update the BTA timer list and handle expired timers.
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_timer_update (void)
{
    if (!nfa_sys_cb.timers_disabled)
    {
        nfa_sys_ptim_timer_update (&nfa_sys_cb.ptim_cb);
    }
}

/*******************************************************************************
**
** Function         nfa_sys_register
**
** Description      Called by other BTA subsystems to register their event
**                  handler.
**
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_register (UINT8 id, const tNFA_SYS_REG *p_reg)
{
    nfa_sys_cb.reg[id] = (tNFA_SYS_REG *) p_reg;
    nfa_sys_cb.is_reg[id] = TRUE;

    if ((id != NFA_ID_DM) && (id != NFA_ID_SYS))
        nfa_sys_cb.enable_cplt_mask |= (0x0001 << id);

    if (id != NFA_ID_SYS)
    {
        if (p_reg->proc_nfcc_pwr_mode)
            nfa_sys_cb.proc_nfcc_pwr_mode_cplt_mask |= (0x0001 << id);
    }

    NFA_TRACE_DEBUG2 ("nfa_sys_register () id=%i, enable_cplt_mask=0x%x",
                       id, nfa_sys_cb.enable_cplt_mask);
}


/*******************************************************************************
**
** Function         nfa_sys_check_disabled
**
** Description      If all subsystems above DM have been disabled, then
**                  disable DM. Called during NFA shutdown
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_check_disabled (void)
{
    UINT8 id;
    UINT8 done = TRUE;

    /* Check if all subsystems above DM have been disabled. */
    for (id = (NFA_ID_DM+1); id < NFA_ID_MAX; id++)
    {
        if (nfa_sys_cb.is_reg[id])
        {
            /* as long as one subsystem is not done */
            done = FALSE;
            break;
        }
    }

    /* All subsystems disabled. disable DM */
    if ((done) && (nfa_sys_cb.is_reg[NFA_ID_DM]))
    {
        (*nfa_sys_cb.reg[NFA_ID_DM]->disable) ();
    }
}


/*******************************************************************************
**
** Function         nfa_sys_deregister
**
** Description      Called by other BTA subsystems to de-register
**                  handler.
**
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_deregister (UINT8 id)
{
    NFA_TRACE_DEBUG1 ("nfa_sys: deregistering subsystem %i", id);

    nfa_sys_cb.is_reg[id] = FALSE;

    /* If not deregistering DM, then check if any other subsystems above DM are still  */
    /* registered.                                                                  */
    if (id != NFA_ID_DM)
    {
        /* If all subsystems above NFA_DM have been disabled, then okay to disable DM */
        nfa_sys_check_disabled ();
    }
    else
    {
        /* DM (the final sub-system) is deregistering. Clear pending timer events in nfa_sys. */
        nfa_sys_ptim_init (&nfa_sys_cb.ptim_cb, NFA_SYS_TIMER_PERIOD, p_nfa_sys_cfg->timer);
    }
}

/*******************************************************************************
**
** Function         nfa_sys_is_register
**
** Description      Called by other BTA subsystems to get registeration
**                  status.
**
**
** Returns          void
**
*******************************************************************************/
BOOLEAN nfa_sys_is_register (UINT8 id)
{
    return nfa_sys_cb.is_reg[id];
}

/*******************************************************************************
**
** Function         nfa_sys_is_graceful_disable
**
** Description      Called by other BTA subsystems to get disable
**                  parameter.
**
**
** Returns          void
**
*******************************************************************************/
BOOLEAN nfa_sys_is_graceful_disable (void)
{
    return nfa_sys_cb.graceful_disable;
}

/*******************************************************************************
**
** Function         nfa_sys_enable_subsystems
**
** Description      Call on NFA Start up
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_enable_subsystems (void)
{
    UINT8 id;

    NFA_TRACE_DEBUG0 ("nfa_sys: enabling subsystems");

    /* Enable all subsystems except SYS */
    for (id = NFA_ID_DM; id < NFA_ID_MAX; id++)
    {
        if (nfa_sys_cb.is_reg[id])
        {
            if (nfa_sys_cb.reg[id]->enable != NULL)
            {
                /* Subsytem has a Disable funciton. Call it now */
                (*nfa_sys_cb.reg[id]->enable) ();
            }
            else
            {
                /* Subsytem does not have a Enable function. Report Enable on behalf of subsystem */
                nfa_sys_cback_notify_enable_complete (id);
            }
        }
    }
}

/*******************************************************************************
**
** Function         nfa_sys_disable_subsystems
**
** Description      Call on NFA shutdown. Disable all subsystems above NFA_DM
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_disable_subsystems (BOOLEAN graceful)
{
    UINT8 id;
    BOOLEAN done = TRUE;

    NFA_TRACE_DEBUG1 ("nfa_sys: disabling subsystems:%d", graceful);
    nfa_sys_cb.graceful_disable = graceful;

    /* Disable all subsystems above NFA_DM. (NFA_DM and NFA_SYS will be disabled last) */
    for (id = (NFA_ID_DM+1); id < NFA_ID_MAX; id++)
    {
        if (nfa_sys_cb.is_reg[id])
        {
            done = FALSE;
            if (nfa_sys_cb.reg[id]->disable != NULL)
            {
                /* Subsytem has a Disable funciton. Call it now */
                (*nfa_sys_cb.reg[id]->disable) ();
            }
            else
            {
                /* Subsytem does not have a Disable function. Deregister on behalf of subsystem */
                nfa_sys_deregister (id);
            }
        }
    }

    /* If All subsystems disabled. disable DM */
    if ((done) && (nfa_sys_cb.is_reg[NFA_ID_DM]))
    {
        (*nfa_sys_cb.reg[NFA_ID_DM]->disable) ();
    }
}

/*******************************************************************************
**
** Function         nfa_sys_notify_nfcc_power_mode
**
** Description      Call to notify NFCC power mode to NFA sub-modules
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_notify_nfcc_power_mode (UINT8 nfcc_power_mode)
{
    UINT8 id;

    NFA_TRACE_DEBUG1 ("nfa_sys: notify NFCC power mode(%d) to subsystems", nfcc_power_mode);

    /* Notify NFCC power state to all subsystems except NFA_SYS */
    for (id = NFA_ID_DM; id < NFA_ID_MAX; id++)
    {
        if ((nfa_sys_cb.is_reg[id]) && (nfa_sys_cb.reg[id]->proc_nfcc_pwr_mode))
        {
            /* Subsytem has a funciton for processing NFCC power mode. Call it now */
            (*nfa_sys_cb.reg[id]->proc_nfcc_pwr_mode) (nfcc_power_mode);
        }
    }
}

/*******************************************************************************
**
** Function         nfa_sys_sendmsg
**
** Description      Send a GKI message to BTA.  This function is designed to
**                  optimize sending of messages to BTA.  It is called by BTA
**                  API functions and call-in functions.
**
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_sendmsg (void *p_msg)
{
    GKI_send_msg (NFC_TASK, p_nfa_sys_cfg->mbox, p_msg);
}

/*******************************************************************************
**
** Function         nfa_sys_start_timer
**
** Description      Start a protocol timer for the specified amount
**                  of time in milliseconds.
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_start_timer (TIMER_LIST_ENT *p_tle, UINT16 type, INT32 timeout)
{
    nfa_sys_ptim_start_timer (&nfa_sys_cb.ptim_cb, p_tle, type, timeout);
}

/*******************************************************************************
**
** Function         nfa_sys_stop_timer
**
** Description      Stop a BTA timer.
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_stop_timer (TIMER_LIST_ENT *p_tle)
{
    nfa_sys_ptim_stop_timer (&nfa_sys_cb.ptim_cb, p_tle);
}


/*******************************************************************************
**
** Function         nfa_sys_disable_timers
**
** Description      Disable sys timer event handling
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_disable_timers (void)
{
    nfa_sys_cb.timers_disabled = TRUE;
}

/*******************************************************************************
**
** Function         nfa_sys_set_trace_level
**
** Description      Set trace level for BTA
**
** Returns          void
**
*******************************************************************************/
void nfa_sys_set_trace_level (UINT8 level)
{
    nfa_sys_cb.trace_level = level;
}
