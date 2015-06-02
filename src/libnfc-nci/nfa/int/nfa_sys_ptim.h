/******************************************************************************
 *
 *  Copyright (C) 2003-2014 Broadcom Corporation
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
 *  Protocol timer services.
 *
 ******************************************************************************/
#ifndef NFA_SYS_PTIM_H
#define NFA_SYS_PTIM_H

#include "gki.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/

typedef struct
{
    TIMER_LIST_Q        timer_queue;        /* GKI timer queue */
    INT32               period;             /* Timer period in milliseconds */
    UINT32              last_gki_ticks;     /* GKI ticks since last time update called */
    UINT8               timer_id;           /* GKI timer id */
} tPTIM_CB;

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
**  Function Declarations
*****************************************************************************/

/*******************************************************************************
**
** Function         nfa_sys_ptim_init
**
** Description      Initialize a protocol timer service control block.
**
** Returns          void
**
*******************************************************************************/
extern void nfa_sys_ptim_init (tPTIM_CB *p_cb, UINT16 period, UINT8 timer_id);

/*******************************************************************************
**
** Function         nfa_sys_ptim_timer_update
**
** Description      Update the protocol timer list and handle expired timers.
**
** Returns          void
**
*******************************************************************************/
extern void nfa_sys_ptim_timer_update (tPTIM_CB *p_cb);

/*******************************************************************************
**
** Function         nfa_sys_ptim_start_timer
**
** Description      Start a protocol timer for the specified amount
**                  of time in milliseconds.
**
** Returns          void
**
*******************************************************************************/
extern void nfa_sys_ptim_start_timer (tPTIM_CB *p_cb, TIMER_LIST_ENT *p_tle, UINT16 type, INT32 timeout);

/*******************************************************************************
**
** Function         nfa_sys_ptim_stop_timer
**
** Description      Stop a protocol timer.
**
** Returns          void
**
*******************************************************************************/
extern void nfa_sys_ptim_stop_timer (tPTIM_CB *p_cb, TIMER_LIST_ENT *p_tle);

#ifdef __cplusplus
}
#endif

#endif /* NFA_SYS_PTIM_H */
