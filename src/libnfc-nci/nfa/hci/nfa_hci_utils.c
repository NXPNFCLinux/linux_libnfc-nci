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
 *  Copyright (C) 2014 NXP Semiconductors
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
 *  This file contains the utility functions for the NFA HCI.
 *
 ******************************************************************************/
#include <string.h>
#include "trace_api.h"
#include "nfc_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfa_dm_int.h"
#include "nfa_hci_api.h"
#include "nfa_hci_int.h"
#include "nfa_nv_co.h"
#include "nfa_mem_co.h"
#include "nfa_hci_defs.h"

static void handle_debug_loopback (BT_HDR *p_buf, UINT8 pipe, UINT8 type, UINT8 instruction);
BOOLEAN HCI_LOOPBACK_DEBUG = FALSE;

/*******************************************************************************
**
** Function         nfa_hciu_find_pipe_by_pid
**
** Description      look for the pipe control block based on pipe id
**
** Returns          pointer to the pipe control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE *nfa_hciu_find_pipe_by_pid (UINT8 pipe_id)
{
    tNFA_HCI_DYN_PIPE   *pp = nfa_hci_cb.cfg.dyn_pipes;
    int                 xx  = 0;

    /* Loop through looking for a match */
    for ( ; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (pp->pipe_id == pipe_id)
            return (pp);
    }

    /* If here, not found */
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_gate_by_gid
**
** Description      Find the gate control block for the given gate id
**
** Returns          pointer to the gate control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_GATE *nfa_hciu_find_gate_by_gid (UINT8 gate_id)
{
    tNFA_HCI_DYN_GATE *pg = nfa_hci_cb.cfg.dyn_gates;
    int               xx  = 0;

    for ( ; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++)
    {
        if (pg->gate_id == gate_id)
            return (pg);
    }

    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_gate_by_owner
**
** Description      Find the the first gate control block for the given owner
**
** Returns          pointer to the gate control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_GATE *nfa_hciu_find_gate_by_owner (tNFA_HANDLE app_handle)
{
    tNFA_HCI_DYN_GATE *pg = nfa_hci_cb.cfg.dyn_gates;
    int               xx  = 0;

    for ( ; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++)
    {
        if (pg->gate_owner == app_handle)
            return (pg);
    }

    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_gate_with_nopipes_by_owner
**
** Description      Find the the first gate control block with no pipes
**                  for the given owner
**
** Returns          pointer to the gate control block, or NULL if not found
**
*******************************************************************************/
tNFA_HCI_DYN_GATE *nfa_hciu_find_gate_with_nopipes_by_owner (tNFA_HANDLE app_handle)
{
    tNFA_HCI_DYN_GATE *pg = nfa_hci_cb.cfg.dyn_gates;
    int               xx  = 0;

    for ( ; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++)
    {
        if (  (pg->gate_owner    == app_handle)
            &&(pg->pipe_inx_mask == 0)  )
            return (pg);
    }

    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_count_pipes_on_gate
**
** Description      Count the number of pipes on the given gate
**
** Returns          the number of pipes on the gate
**
*******************************************************************************/
UINT8 nfa_hciu_count_pipes_on_gate (tNFA_HCI_DYN_GATE *p_gate)
{
    int               xx    = 0;
    UINT32            mask  = 1;
    UINT8             count = 0;

    for ( ; xx < NFA_HCI_MAX_PIPE_CB; xx++)
    {
        if ( p_gate->pipe_inx_mask & mask )
            count++;

        mask = mask << 1;
    }

    return (count);
}

/*******************************************************************************
**
** Function         nfa_hciu_count_open_pipes_on_gate
**
** Description      Count the number of opened pipes on the given gate
**
** Returns          the number of pipes in OPENED state on the gate
**
*******************************************************************************/
UINT8 nfa_hciu_count_open_pipes_on_gate (tNFA_HCI_DYN_GATE *p_gate)
{
    tNFA_HCI_DYN_PIPE *pp   = nfa_hci_cb.cfg.dyn_pipes;
    int               xx    = 0;
    UINT32            mask  = 1;
    UINT8             count = 0;

    for ( ; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        /* For each pipe on this gate, check if it is open */
        if ((p_gate->pipe_inx_mask & mask) && (pp->pipe_state == NFA_HCI_PIPE_OPENED))
            count++;

        mask = mask << 1;
    }

    return (count);
}

/*******************************************************************************
**
** Function         nfa_hciu_get_gate_owner
**
** Description      Find the application that owns a gate
**
** Returns          application handle
**
*******************************************************************************/
tNFA_HANDLE nfa_hciu_get_gate_owner (UINT8 gate_id)
{
    tNFA_HCI_DYN_GATE   *pg;

    if ((pg = nfa_hciu_find_gate_by_gid (gate_id)) == NULL)
        return (NFA_HANDLE_INVALID);

    return (pg->gate_owner);
}

/*******************************************************************************
**
** Function         nfa_hciu_get_pipe_owner
**
** Description      Find the application that owns a pipe
**
** Returns          application handle
**
*******************************************************************************/
tNFA_HANDLE nfa_hciu_get_pipe_owner (UINT8 pipe_id)
{
    tNFA_HCI_DYN_PIPE   *pp;
    tNFA_HCI_DYN_GATE   *pg;

    if ((pp = nfa_hciu_find_pipe_by_pid (pipe_id)) == NULL)
        return (NFA_HANDLE_INVALID);

    if ((pg = nfa_hciu_find_gate_by_gid (pp->local_gate)) == NULL)
        return (NFA_HANDLE_INVALID);

    return (pg->gate_owner);
}


/*******************************************************************************
**
** Function         nfa_hciu_alloc_gate
**
** Description      Allocate an gate control block
**
** Returns          pointer to the allocated gate, or NULL if cannot allocate
**
*******************************************************************************/
tNFA_HCI_DYN_GATE *nfa_hciu_alloc_gate (UINT8 gate_id, tNFA_HANDLE app_handle)
{
    tNFA_HCI_DYN_GATE   *pg;
    int                 xx;
    UINT8               app_inx = app_handle & NFA_HANDLE_MASK;


    /* First, check if the application handle is valid */
    if (  (gate_id != NFA_HCI_CONNECTIVITY_GATE)
        &&(gate_id < NFA_HCI_FIRST_PROP_GATE)
        &&((  (app_handle & NFA_HANDLE_GROUP_MASK) != NFA_HANDLE_GROUP_HCI)
            ||(app_inx >= NFA_HCI_MAX_APP_CB)
            ||(nfa_hci_cb.p_app_cback[app_inx] == NULL))  )
    {
        return (NULL);
    }

    if (gate_id != 0)
    {
        if ((pg = nfa_hciu_find_gate_by_gid (gate_id)) != NULL)
            return (pg);
    }
    else
    {
        /* If gate_id is 0, we need to assign a free one */
        /* Loop through all possible gate IDs checking if they are already used */
        for (gate_id = NFA_HCI_FIRST_HOST_SPECIFIC_GENERIC_GATE; gate_id <= NFA_HCI_LAST_PROP_GATE; gate_id++)
        {
            /* Skip connectivity gate */
            if (gate_id == NFA_HCI_CONNECTIVITY_GATE) gate_id++;

            /* Check if the gate is already allocated */
            if (nfa_hciu_find_gate_by_gid (gate_id) == NULL)
                break;
        }
        if (gate_id > NFA_HCI_LAST_PROP_GATE)
        {
            NFA_TRACE_ERROR2 ("nfa_hci_alloc_gate - no free Gate ID: %u  App Handle: 0x%04x", gate_id, app_handle);
            return (NULL);
        }
    }

    /* Now look for a free control block */
    for (xx = 0, pg = nfa_hci_cb.cfg.dyn_gates; xx < NFA_HCI_MAX_GATE_CB; xx++, pg++)
    {
        if (pg->gate_id == 0)
        {
            /* Found a free gate control block */
            pg->gate_id       = gate_id;
            pg->gate_owner    = app_handle;
            pg->pipe_inx_mask = 0;

            NFA_TRACE_DEBUG2 ("nfa_hciu_alloc_gate id:%d  app_handle: 0x%04x", gate_id, app_handle);

            nfa_hci_cb.nv_write_needed = TRUE;
            return (pg);
        }
    }

    /* If here, no free gate control block */
    NFA_TRACE_ERROR2 ("nfa_hci_alloc_gate - no CB  Gate ID: %u  App Handle: 0x%04x", gate_id, app_handle);
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_send_msg
**
** Description      This function will fragment the given packet, if necessary
**                  and send it on the given pipe.
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_msg (UINT8 pipe_id, UINT8 type, UINT8 instruction, UINT16 msg_len, UINT8 *p_msg)
{
    BT_HDR          *p_buf;
    UINT8           *p_data;
    BOOLEAN          first_pkt = TRUE;
    UINT16          data_len;
    tNFA_STATUS     status = NFA_STATUS_OK;
    UINT16          max_seg_hcp_pkt_size = nfa_hci_cb.buff_size;

#if (BT_TRACE_VERBOSE == TRUE)
    char    buff[100];

    NFA_TRACE_DEBUG3 ("nfa_hciu_send_msg pipe_id:%d   %s  len:%d",
                      pipe_id, nfa_hciu_get_type_inst_names (pipe_id, type, instruction, buff), msg_len);
#else
    NFA_TRACE_DEBUG4 ("nfa_hciu_send_msg pipe_id:%d   Type: %u  Inst: %u  len: %d",
                      pipe_id, type, instruction, msg_len);
#endif

    if (instruction == NFA_HCI_ANY_GET_PARAMETER)
        nfa_hci_cb.param_in_use = *p_msg;

    while ((first_pkt == TRUE) || (msg_len != 0))
    {
        if ((p_buf = (BT_HDR *) GKI_getpoolbuf (NFC_RW_POOL_ID)) != NULL)
        {
            p_buf->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;

            /* First packet has a 2-byte header, subsequent fragments have a 1-byte header */
            data_len = first_pkt ? (max_seg_hcp_pkt_size - 2) : (max_seg_hcp_pkt_size - 1);

            p_data = (UINT8 *) (p_buf + 1) + p_buf->offset;

            /* Last or only segment has "no fragmentation" bit set */
            if (msg_len > data_len)
            {
                *p_data++ = (NFA_HCI_MESSAGE_FRAGMENTATION << 7) | (pipe_id & 0x7F);
            }
            else
            {
                data_len = msg_len;
                *p_data++ = (NFA_HCI_NO_MESSAGE_FRAGMENTATION << 7) | (pipe_id & 0x7F);
            }

            p_buf->len = 1;

            /* Message header only goes in the first segment */
            if (first_pkt)
            {
                first_pkt = FALSE;
                *p_data++ = (type << 6) | instruction;
                p_buf->len++;
            }

            if (data_len != 0)
            {
                memcpy (p_data, p_msg, data_len);

                p_buf->len += data_len;
                msg_len    -= data_len;
                if (msg_len > 0)
                    p_msg      += data_len;
            }

#if (BT_TRACE_PROTOCOL == TRUE)
            DispHcp (((UINT8 *) (p_buf + 1) + p_buf->offset), p_buf->len, FALSE, (BOOLEAN) ((p_buf->len - data_len) == 2));
#endif

            if (HCI_LOOPBACK_DEBUG)
                handle_debug_loopback (p_buf, pipe_id, type, instruction);
            else
                status = NFC_SendData (nfa_hci_cb.conn_id, p_buf);
        }
        else
        {
            NFA_TRACE_ERROR0 ("nfa_hciu_send_data_packet no buffers");
            status = NFA_STATUS_NO_BUFFERS;
            break;
        }
    }

    /* Start timer if response to wait for a particular time for the response  */
    if (type == NFA_HCI_COMMAND_TYPE)
    {
        nfa_hci_cb.cmd_sent = instruction;

        if (nfa_hci_cb.hci_state == NFA_HCI_STATE_IDLE)
            nfa_hci_cb.hci_state = NFA_HCI_STATE_WAIT_RSP;

        nfa_sys_start_timer (&nfa_hci_cb.timer, NFA_HCI_RSP_TIMEOUT_EVT, p_nfa_hci_cfg->hcp_response_timeout);
    }

    return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_get_allocated_gate_list
**
** Description      fills in a list of allocated gates
**
** Returns          the number of gates
**
*******************************************************************************/
UINT8 nfa_hciu_get_allocated_gate_list (UINT8 *p_gate_list)
{
    tNFA_HCI_DYN_GATE   *p_cb;
    int                 xx;
    UINT8               count = 0;

    for (xx = 0, p_cb = nfa_hci_cb.cfg.dyn_gates; xx < NFA_HCI_MAX_GATE_CB; xx++, p_cb++)
    {
        if (p_cb->gate_id != 0)
        {
            *p_gate_list++ = p_cb->gate_id;
            count++;
        }
    }

    NFA_TRACE_DEBUG1 ("nfa_hciu_get_allocated_gate_list () returns: %u", count);

    return (count);
}

/*******************************************************************************
**
** Function         nfa_hciu_alloc_pipe
**
** Description      Allocate a pipe control block
**
** Returns          pointer to the pipe control block, or NULL if
**                  cannot allocate
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE *nfa_hciu_alloc_pipe (UINT8 pipe_id)
{
    UINT8               xx;
    tNFA_HCI_DYN_PIPE   *pp;

    /* If we already have a pipe of the same ID, release it first it */
    if ((pp = nfa_hciu_find_pipe_by_pid (pipe_id)) != NULL)
    {
        if (pipe_id > NFA_HCI_LAST_DYNAMIC_PIPE)
            return pp;
        nfa_hciu_release_pipe (pipe_id);
    }

    /* Look for a free pipe control block */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes ; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (pp->pipe_id == 0)
        {
            NFA_TRACE_DEBUG2 ("nfa_hciu_alloc_pipe:%d, index:%d", pipe_id, xx);
            pp->pipe_id = pipe_id;

            nfa_hci_cb.nv_write_needed = TRUE;
            return (pp);
        }
    }

    NFA_TRACE_DEBUG1 ("nfa_hciu_alloc_pipe:%d, NO free entries !!", pipe_id);
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_release_gate
**
** Description      Remove a generic gate from gate list
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_release_gate (UINT8 gate_id)
{
    tNFA_HCI_DYN_GATE   *p_gate = nfa_hciu_find_gate_by_gid (gate_id);

    if (p_gate != NULL)
    {
        NFA_TRACE_DEBUG3 ("nfa_hciu_release_gate () ID: %d  owner: 0x%04x  pipe_inx_mask: 0x%04x",
                          gate_id, p_gate->gate_owner, p_gate->pipe_inx_mask);

        p_gate->gate_id       = 0;
        p_gate->gate_owner    = 0;
        p_gate->pipe_inx_mask = 0;

        nfa_hci_cb.nv_write_needed = TRUE;
    }
    else
    {
        NFA_TRACE_WARNING1 ("nfa_hciu_release_gate () ID: %d  NOT FOUND", gate_id);
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_add_pipe_to_gate
**
** Description      Add pipe to generic gate
**
** Returns          NFA_STATUS_OK, if successfully add the pipe on to the gate
**                  NFA_HCI_ADM_E_NO_PIPES_AVAILABLE, otherwise
**
*******************************************************************************/
tNFA_HCI_RESPONSE nfa_hciu_add_pipe_to_gate (UINT8 pipe_id,   UINT8 local_gate,
                                             UINT8 dest_host, UINT8 dest_gate)
{
    tNFA_HCI_DYN_GATE   *p_gate;
    tNFA_HCI_DYN_PIPE   *p_pipe;
    UINT8               pipe_index;

    p_gate = nfa_hciu_find_gate_by_gid (local_gate);

    if (p_gate != NULL)
    {
        /* Allocate a pipe control block */
        if ((p_pipe = nfa_hciu_alloc_pipe (pipe_id)) != NULL)
        {
            p_pipe->pipe_id     = pipe_id;
            p_pipe->pipe_state  = NFA_HCI_PIPE_CLOSED;
            p_pipe->dest_host   = dest_host;
            p_pipe->dest_gate   = dest_gate;
            p_pipe->local_gate  = local_gate;

            /* Save the pipe in the gate that it belongs to */
            pipe_index = (UINT8) (p_pipe - nfa_hci_cb.cfg.dyn_pipes);
            p_gate->pipe_inx_mask |= (UINT32) (1 << pipe_index);

            NFA_TRACE_DEBUG4 ("nfa_hciu_add_pipe_to_gate  Gate ID: 0x%02x  Pipe ID: 0x%02x  pipe_index: %u  App Handle: 0x%08x",
                              local_gate, pipe_id, pipe_index, p_gate->gate_owner);
            return (NFA_HCI_ANY_OK);
        }
    }

    NFA_TRACE_DEBUG1 ("nfa_hciu_add_pipe_to_gate: 0x%02x  NOT FOUND", local_gate);

    return (NFA_HCI_ADM_E_NO_PIPES_AVAILABLE);
}

/*******************************************************************************
**
** Function         nfa_hciu_add_pipe_to_static_gate
**
** Description      Add pipe to identity management gate
**
** Returns          NFA_HCI_ANY_OK, if successfully add the pipe on to the gate
**                  NFA_HCI_ADM_E_NO_PIPES_AVAILABLE, otherwise
**
*******************************************************************************/
tNFA_HCI_RESPONSE nfa_hciu_add_pipe_to_static_gate (UINT8 local_gate, UINT8 pipe_id, UINT8 dest_host, UINT8 dest_gate)
{
    tNFA_HCI_DYN_PIPE   *p_pipe;
    UINT8               pipe_index;

    NFA_TRACE_EVENT4 ("nfa_hciu_add_pipe_to_static_gate (%u)  Pipe: 0x%02x  Dest Host: 0x%02x  Dest Gate: 0x%02x)",
                      local_gate, pipe_id, dest_host, dest_gate);

    /* Allocate a pipe control block */
    if ((p_pipe = nfa_hciu_alloc_pipe (pipe_id)) != NULL)
    {
        p_pipe->pipe_id     = pipe_id;
        p_pipe->pipe_state  = NFA_HCI_PIPE_CLOSED;
        p_pipe->dest_host   = dest_host;
        p_pipe->dest_gate   = dest_gate;
        p_pipe->local_gate  = local_gate;

        /* If this is the ID gate, save the pipe index in the ID gate info     */
        /* block. Note that for loopback, it is enough to just create the pipe */
        if (local_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE)
        {
            pipe_index = (UINT8) (p_pipe - nfa_hci_cb.cfg.dyn_pipes);
            nfa_hci_cb.cfg.id_mgmt_gate.pipe_inx_mask  |= (UINT32) (1 << pipe_index);
        }
        return NFA_HCI_ANY_OK;
    }

    return NFA_HCI_ADM_E_NO_PIPES_AVAILABLE;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_active_pipe_by_owner
**
** Description      Find the first pipe associated with the given app
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE *nfa_hciu_find_active_pipe_by_owner (tNFA_HANDLE app_handle)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;

    NFA_TRACE_DEBUG1 ("nfa_hciu_find_pipe_by_owner () app_handle:0x%x", app_handle);

    /* Loop through all pipes looking for the owner */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (  (pp->pipe_id != 0)
            &&(pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE)
            &&(pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE)
            &&(nfa_hciu_is_active_host (pp->dest_host))  )
        {
            if (  ((pg = nfa_hciu_find_gate_by_gid (pp->local_gate)) != NULL)
                &&(pg->gate_owner == app_handle) )
                return (pp);
        }
    }

    /* If here, not found */
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_check_pipe_between_gates
**
** Description      Check if there is a pipe between specified Terminal host
**                  gate and and the specified UICC gate
**
** Returns          TRUE, if there exists a pipe between the two specified gated
**                  FALSE, otherwise
**
*******************************************************************************/
BOOLEAN nfa_hciu_check_pipe_between_gates (UINT8 local_gate, UINT8 dest_host, UINT8 dest_gate)
{
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;

    NFA_TRACE_DEBUG3 ("nfa_hciu_check_pipe_between_gates () Local gate: 0x%02X, Host[0x%02X] gate: 0x%02X", local_gate, dest_host, dest_gate);

    /* Loop through all pipes looking for the owner */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (  (pp->pipe_id != 0)
            &&(pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE)
            &&(pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE)
            &&(pp->local_gate == local_gate)
            &&(pp->dest_host  == dest_host)
            &&(pp->dest_gate  == dest_gate)  )
        {
            return (TRUE);
        }
    }

    /* If here, not found */
    return (FALSE);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_pipe_by_owner
**
** Description      Find the first pipe associated with the given app
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE *nfa_hciu_find_pipe_by_owner (tNFA_HANDLE app_handle)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;

    NFA_TRACE_DEBUG1 ("nfa_hciu_find_pipe_by_owner () app_handle:0x%x", app_handle);

    /* Loop through all pipes looking for the owner */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (pp->pipe_id != 0)
        {
            if (  ((pg = nfa_hciu_find_gate_by_gid (pp->local_gate)) != NULL)
                &&(pg->gate_owner == app_handle) )
                return (pp);
        }
    }

    /* If here, not found */
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_find_pipe_on_gate
**
** Description      Find the first pipe associated with the given gate
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE *nfa_hciu_find_pipe_on_gate (UINT8 gate_id)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;

    NFA_TRACE_DEBUG1 ("nfa_hciu_find_pipe_on_gate () Gate:0x%x", gate_id);

    /* Loop through all pipes looking for the owner */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (pp->pipe_id != 0)
        {
            if (  ((pg = nfa_hciu_find_gate_by_gid (pp->local_gate)) != NULL)
                &&(pg->gate_id == gate_id) )
                return (pp);
        }
    }

    /* If here, not found */
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_is_active_host
**
** Description      Check if the host is currently active
**
** Returns          TRUE, if the host is active in the host network
**                  FALSE, if the host is not active in the host network
**
*******************************************************************************/
BOOLEAN nfa_hciu_is_active_host (UINT8 host_id)
{
    UINT8   xx;

    for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++)
    {
        if (nfa_hci_cb.inactive_host[xx] == host_id)
            return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_hciu_is_host_reseting
**
** Description      Check if the host is currently reseting
**
** Returns          TRUE, if the host is reseting
**                  FALSE, if the host is not reseting
**
*******************************************************************************/
BOOLEAN nfa_hciu_is_host_reseting (UINT8 host_id)
{
    UINT8   xx;

    for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++)
    {
        if (nfa_hci_cb.reset_host[xx] == host_id)
            return TRUE;
    }

    return FALSE;
}

/*******************************************************************************
**
** Function         nfa_hciu_is_no_host_resetting
**
** Description      Check if no host is reseting
**
** Returns          TRUE, if no host is resetting at this time
**                  FALSE, if one or more host is resetting
**
*******************************************************************************/
BOOLEAN nfa_hciu_is_no_host_resetting (void)
{
    UINT8   xx;

    for (xx = 0; xx < NFA_HCI_MAX_HOST_IN_NETWORK; xx++)
    {
        if (nfa_hci_cb.reset_host[xx] != 0)
            return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_hciu_find_active_pipe_on_gate
**
** Description      Find the first active pipe associated with the given gate
**
** Returns          pointer to pipe, or NULL if none found
**
*******************************************************************************/
tNFA_HCI_DYN_PIPE *nfa_hciu_find_active_pipe_on_gate (UINT8 gate_id)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;

    NFA_TRACE_DEBUG1 ("nfa_hciu_find_active_pipe_on_gate () Gate:0x%x", gate_id);

    /* Loop through all pipes looking for the owner */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (  (pp->pipe_id != 0)
            &&(pp->pipe_id >= NFA_HCI_FIRST_DYNAMIC_PIPE)
            &&(pp->pipe_id <= NFA_HCI_LAST_DYNAMIC_PIPE)
            &&(nfa_hciu_is_active_host (pp->dest_host))  )
        {
            if (  ((pg = nfa_hciu_find_gate_by_gid (pp->local_gate)) != NULL)
                &&(pg->gate_id == gate_id) )
                return (pp);
        }
    }

    /* If here, not found */
    return (NULL);
}

/*******************************************************************************
**
** Function         nfa_hciu_release_pipe
**
** Description      remove the specified pipe
**
** Returns          NFA_HCI_ANY_OK, if removed
**                  NFA_HCI_ANY_E_NOK, if otherwise
**
*******************************************************************************/
tNFA_HCI_RESPONSE nfa_hciu_release_pipe (UINT8 pipe_id)
{
    tNFA_HCI_DYN_GATE   *p_gate;
    tNFA_HCI_DYN_PIPE   *p_pipe;
    UINT8               pipe_index;

    NFA_TRACE_EVENT1 ("nfa_hciu_release_pipe: %u", pipe_id);

    if ((p_pipe = nfa_hciu_find_pipe_by_pid (pipe_id)) == NULL)
        return (NFA_HCI_ANY_E_NOK);

    if (pipe_id > NFA_HCI_LAST_DYNAMIC_PIPE)
    {
        NFA_TRACE_DEBUG1 ("ignore pipe: %d", pipe_id);
        return (NFA_HCI_ANY_E_NOK);
    }

    pipe_index = (UINT8) (p_pipe - nfa_hci_cb.cfg.dyn_pipes);

    if (p_pipe->local_gate == NFA_HCI_IDENTITY_MANAGEMENT_GATE)
    {
        /* Remove pipe from ID management gate */
        nfa_hci_cb.cfg.id_mgmt_gate.pipe_inx_mask &= ~ (UINT32) (1 << pipe_index);
    }
    else
    {
        if ((p_gate = nfa_hciu_find_gate_by_gid (p_pipe->local_gate)) == NULL)
        {
            /* Mark the pipe control block as free */
            p_pipe->pipe_id = 0;
            return (NFA_HCI_ANY_E_NOK);
        }

        /* Remove pipe from gate */
        p_gate->pipe_inx_mask &= ~ (UINT32) (1 << pipe_index);
    }

    /* Reset pipe control block */
    memset (p_pipe,0,sizeof (tNFA_HCI_DYN_PIPE));
    nfa_hci_cb.nv_write_needed = TRUE;
    return NFA_HCI_ANY_OK;
}

/*******************************************************************************
**
** Function         nfa_hciu_remove_all_pipes_from_host
**
** Description      remove all the pipes that are connected to a specific host
**
** Returns          None
**
*******************************************************************************/
void nfa_hciu_remove_all_pipes_from_host (UINT8 host)
{
    tNFA_HCI_DYN_GATE   *pg;
    tNFA_HCI_DYN_PIPE   *pp;
    int                 xx;
    tNFA_HCI_EVT_DATA   evt_data;

    NFA_TRACE_EVENT1 ("nfa_hciu_remove_all_pipes_from_host (0x%02x)", host);

    /* Remove all pipes from the specified host connected to all generic gates */
    for (xx = 0, pp = nfa_hci_cb.cfg.dyn_pipes; xx < NFA_HCI_MAX_PIPE_CB; xx++, pp++)
    {
        if (  (pp->pipe_id == 0)
                    ||
              (  (host != 0)
               &&((pp->dest_host != host) || (pp->pipe_id > NFA_HCI_LAST_DYNAMIC_PIPE)))  )
            continue;

        if ((pg = nfa_hciu_find_gate_by_gid (pp->local_gate)) != NULL)
        {
            evt_data.deleted.status = NFA_STATUS_OK;
            evt_data.deleted.pipe   = pp->pipe_id;

            nfa_hciu_send_to_app (NFA_HCI_DELETE_PIPE_EVT, &evt_data, pg->gate_owner);
        }
        nfa_hciu_release_pipe (pp->pipe_id);
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_send_create_pipe_cmd
**
** Description      Create dynamic pipe between the specified gates
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_create_pipe_cmd (UINT8 source_gate, UINT8 dest_host, UINT8 dest_gate)
{
    tNFA_STATUS         status;
    UINT8               data[3];

    data[0] = source_gate;
    data[1] = dest_host;
    data[2] = dest_gate;

    NFA_TRACE_DEBUG3 ("nfa_hciu_send_create_pipe_cmd source_gate:%d, dest_host:%d, dest_gate:%d", source_gate, dest_host, dest_gate);

    status = nfa_hciu_send_msg (NFA_HCI_ADMIN_PIPE, NFA_HCI_COMMAND_TYPE, NFA_HCI_ADM_CREATE_PIPE, 3, data);

    return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_delete_pipe_cmd
**
** Description      Delete the dynamic pipe
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_delete_pipe_cmd (UINT8 pipe)
{
    tNFA_STATUS status;

    NFA_TRACE_DEBUG1 ("nfa_hciu_send_delete_pipe_cmd: %d", pipe);

    if (pipe > NFA_HCI_LAST_DYNAMIC_PIPE)
    {
        NFA_TRACE_DEBUG1 ("ignore pipe: %d", pipe);
        return (NFA_HCI_ANY_E_NOK);
    }
    nfa_hci_cb.pipe_in_use = pipe;

    status = nfa_hciu_send_msg (NFA_HCI_ADMIN_PIPE, NFA_HCI_COMMAND_TYPE, NFA_HCI_ADM_DELETE_PIPE, 1, &pipe);

    return status;
}


/*******************************************************************************
**
** Function         nfa_hciu_send_clear_all_pipe_cmd
**
** Description      delete all the dynamic pipe connected to device host,
**                  to close all static pipes connected to device host,
**                  and to set registry values related to static pipes to
**                  theri default values.
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_clear_all_pipe_cmd (void)
{
    tNFA_STATUS status;
    UINT16      id_ref_data = 0x0102;

    NFA_TRACE_DEBUG0 ("nfa_hciu_send_clear_all_pipe_cmd");

    status = nfa_hciu_send_msg (NFA_HCI_ADMIN_PIPE, NFA_HCI_COMMAND_TYPE, NFA_HCI_ADM_CLEAR_ALL_PIPE, 2, (UINT8 *) &id_ref_data);

    return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_open_pipe_cmd
**
** Description      Open a closed pipe
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_open_pipe_cmd (UINT8 pipe)
{
    tNFA_STATUS status;

    nfa_hci_cb.pipe_in_use = pipe;

    status = nfa_hciu_send_msg (pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_OPEN_PIPE, 0, NULL);

    return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_close_pipe_cmd
**
** Description      Close an opened pipe
**
** Returns          status
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_close_pipe_cmd (UINT8 pipe)
{
    tNFA_STATUS status;

    nfa_hci_cb.pipe_in_use = pipe;

    status = nfa_hciu_send_msg (pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_CLOSE_PIPE, 0, NULL);

    return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_get_param_cmd
**
** Description      Read a parameter value from gate registry
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_get_param_cmd (UINT8 pipe, UINT8 index)
{
    tNFA_STATUS status;

    if ((status = nfa_hciu_send_msg (pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_GET_PARAMETER, 1, &index)) == NFC_STATUS_OK)
        nfa_hci_cb.param_in_use = index;

    return status;
}

/*******************************************************************************
**
** Function         nfa_hciu_send_set_param_cmd
**
** Description      Set a parameter value in a gate registry
**
** Returns          None
**
*******************************************************************************/
tNFA_STATUS nfa_hciu_send_set_param_cmd (UINT8 pipe, UINT8 index, UINT8 length, UINT8 *p_data)
{
    tNFA_STATUS status;
    UINT8       data[255];

    data[0] = index;

    memcpy (&data[1], p_data, length);

    if ((status = nfa_hciu_send_msg (pipe, NFA_HCI_COMMAND_TYPE, NFA_HCI_ANY_SET_PARAMETER, (UINT16) (length + 1), data)) == NFC_STATUS_OK)
        nfa_hci_cb.param_in_use = index;

    return status;
}


/*******************************************************************************
**
** Function         nfa_hciu_send_to_app
**
** Description      Send an event back to an application
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_send_to_app (tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA *p_evt, tNFA_HANDLE app_handle)
{
    UINT8   app_inx = app_handle & NFA_HANDLE_MASK;

    /* First, check if the application handle is valid */
    if (  ((app_handle & NFA_HANDLE_GROUP_MASK) == NFA_HANDLE_GROUP_HCI)
        &&(app_inx < NFA_HCI_MAX_APP_CB) )
    {
        if (nfa_hci_cb.p_app_cback[app_inx] != NULL)
        {
            nfa_hci_cb.p_app_cback[app_inx] (event, p_evt);
            return;
        }
    }

    if (app_handle != NFA_HANDLE_INVALID)
    {
        NFA_TRACE_WARNING2 ("nfa_hciu_send_to_app no callback,  event: 0x%04x  app_handle: 0x%04x",
                            event, app_handle);
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_send_to_all_apps
**
** Description      Send an event back to all applications
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_send_to_all_apps (tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA *p_evt)
{
    UINT8   app_inx;

    for (app_inx = 0; app_inx < NFA_HCI_MAX_APP_CB; app_inx++)
    {
        if (nfa_hci_cb.p_app_cback[app_inx] != NULL)
            nfa_hci_cb.p_app_cback[app_inx] (event, p_evt);
    }

}

/*******************************************************************************
**
** Function         nfa_hciu_send_to_apps_handling_connectivity_evts
**
** Description      Send a connectivity event to all the application interested
**                  in connectivity events
**
** Returns          none
**
*******************************************************************************/
void nfa_hciu_send_to_apps_handling_connectivity_evts (tNFA_HCI_EVT event, tNFA_HCI_EVT_DATA *p_evt)
{
    UINT8   app_inx;

    for (app_inx = 0; app_inx < NFA_HCI_MAX_APP_CB; app_inx++)
    {
        if (  (nfa_hci_cb.p_app_cback[app_inx] != NULL)
            &&(nfa_hci_cb.cfg.b_send_conn_evts[app_inx]))

            nfa_hci_cb.p_app_cback[app_inx] (event, p_evt);
    }

}

#if (BT_TRACE_VERBOSE == TRUE)
/*******************************************************************************
**
** Function         nfa_hciu_get_response_name
**
** Description      This function returns the error code name.
**
** NOTE             conditionally compiled to save memory.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *nfa_hciu_get_response_name (UINT8 rsp_code)
{
    switch (rsp_code)
    {
    case NFA_HCI_ANY_OK:
        return ("ANY_OK");
    case NFA_HCI_ANY_E_NOT_CONNECTED:
        return ("ANY_E_NOT_CONNECTED");
    case NFA_HCI_ANY_E_CMD_PAR_UNKNOWN:
        return ("ANY_E_CMD_PAR_UNKNOWN");
    case NFA_HCI_ANY_E_NOK:
        return ("ANY_E_NOK");
    case NFA_HCI_ADM_E_NO_PIPES_AVAILABLE:
        return ("ADM_E_NO_PIPES_AVAILABLE");
    case NFA_HCI_ANY_E_REG_PAR_UNKNOWN:
        return ("ANY_E_REG_PAR_UNKNOWN");
    case NFA_HCI_ANY_E_PIPE_NOT_OPENED:
        return ("ANY_E_PIPE_NOT_OPENED");
    case NFA_HCI_ANY_E_CMD_NOT_SUPPORTED:
        return ("ANY_E_CMD_NOT_SUPPORTED");
    case NFA_HCI_ANY_E_INHIBITED:
        return ("ANY_E_INHIBITED");
    case NFA_HCI_ANY_E_TIMEOUT:
        return ("ANY_E_TIMEOUT");
    case NFA_HCI_ANY_E_REG_ACCESS_DENIED:
        return ("ANY_E_REG_ACCESS_DENIED");
    case NFA_HCI_ANY_E_PIPE_ACCESS_DENIED:
        return ("ANY_E_PIPE_ACCESS_DENIED");
    default:
        return ("UNKNOWN");
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_type_2_str
**
** Description      This function returns the type name.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *nfa_hciu_type_2_str(UINT8 type)
{
    switch (type)
    {
    case NFA_HCI_COMMAND_TYPE:
        return ("COMMAND");
    case NFA_HCI_EVENT_TYPE:
        return ("EVENT");
    case NFA_HCI_RESPONSE_TYPE:
        return ("RESPONSE");
    default:
        return ("UNKNOWN");
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_instr_2_str
**
** Description      This function returns the instruction name.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *nfa_hciu_instr_2_str (UINT8 instruction)
{
    switch (instruction)
    {
    case NFA_HCI_ANY_SET_PARAMETER:
        return ("ANY_SET_PARAMETER");
    case NFA_HCI_ANY_GET_PARAMETER:
        return ("ANY_GET_PARAMETER");
    case NFA_HCI_ANY_OPEN_PIPE:
        return ("ANY_OPEN_PIPE");
    case NFA_HCI_ANY_CLOSE_PIPE:
        return ("ANY_CLOSE_PIPE");
    case NFA_HCI_ADM_CREATE_PIPE:
        return ("ADM_CREATE_PIPE");
    case NFA_HCI_ADM_DELETE_PIPE:
        return ("ADM_DELETE_PIPE");
    case NFA_HCI_ADM_NOTIFY_PIPE_CREATED:
        return ("ADM_NOTIFY_PIPE_CREATED");
    case NFA_HCI_ADM_NOTIFY_PIPE_DELETED:
        return ("ADM_NOTIFY_PIPE_DELETED");
    case NFA_HCI_ADM_CLEAR_ALL_PIPE:
        return ("ADM_CLEAR_ALL_PIPE");
    case NFA_HCI_ADM_NOTIFY_ALL_PIPE_CLEARED:
        return ("ADM_NOTIFY_ALL_PIPE_CLEARED");
    default:
        return ("UNKNOWN");
    }
}


/*******************************************************************************
**
** Function         nfa_hciu_get_event_name
**
** Description      This function returns the event code name.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *nfa_hciu_get_event_name (UINT16 event)
{
    switch (event)
    {
    case NFA_HCI_API_REGISTER_APP_EVT:        return ("API_REGISTER");
    case NFA_HCI_API_DEREGISTER_APP_EVT:      return ("API_DEREGISTER");
    case NFA_HCI_API_GET_APP_GATE_PIPE_EVT:   return ("API_GET_GATE_LIST");
    case NFA_HCI_API_ALLOC_GATE_EVT:          return ("API_ALLOC_GATE");
    case NFA_HCI_API_DEALLOC_GATE_EVT:        return ("API_DEALLOC_GATE");
    case NFA_HCI_API_GET_HOST_LIST_EVT:       return ("API_GET_HOST_LIST");
    case NFA_HCI_API_GET_REGISTRY_EVT:        return ("API_GET_REG_VALUE");
    case NFA_HCI_API_SET_REGISTRY_EVT:        return ("API_SET_REG_VALUE");
    case NFA_HCI_API_CREATE_PIPE_EVT:         return ("API_CREATE_PIPE");
    case NFA_HCI_API_OPEN_PIPE_EVT:           return ("API_OPEN_PIPE");
    case NFA_HCI_API_CLOSE_PIPE_EVT:          return ("API_CLOSE_PIPE");
    case NFA_HCI_API_DELETE_PIPE_EVT:         return ("API_DELETE_PIPE");
    case NFA_HCI_API_SEND_CMD_EVT:            return ("API_SEND_COMMAND_EVT");
    case NFA_HCI_API_SEND_RSP_EVT:            return ("API_SEND_RESPONSE_EVT");
    case NFA_HCI_API_SEND_EVENT_EVT:          return ("API_SEND_EVENT_EVT");
    case NFA_HCI_RSP_NV_READ_EVT:             return ("NV_READ_EVT");
    case NFA_HCI_RSP_NV_WRITE_EVT:            return ("NV_WRITE_EVT");
    case NFA_HCI_RSP_TIMEOUT_EVT:             return ("RESPONSE_TIMEOUT_EVT");
    case NFA_HCI_CHECK_QUEUE_EVT:             return ("CHECK_QUEUE");

    default:
        return ("UNKNOWN");
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_get_state_name
**
** Description      This function returns the state name.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *nfa_hciu_get_state_name (UINT8 state)
{
    switch (state)
    {
    case NFA_HCI_STATE_DISABLED:             return ("DISABLED");
    case NFA_HCI_STATE_STARTUP:              return ("STARTUP");
    case NFA_HCI_STATE_WAIT_NETWK_ENABLE:    return ("WAIT_NETWK_ENABLE");
    case NFA_HCI_STATE_IDLE:                 return ("IDLE");
    case NFA_HCI_STATE_WAIT_RSP:             return ("WAIT_RSP");
    case NFA_HCI_STATE_REMOVE_GATE:          return ("REMOVE_GATE");
    case NFA_HCI_STATE_APP_DEREGISTER:       return ("APP_DEREGISTER");
    case NFA_HCI_STATE_RESTORE:              return ("RESTORE");
    case NFA_HCI_STATE_RESTORE_NETWK_ENABLE: return ("WAIT_NETWK_ENABLE_AFTER_RESTORE");

    default:
        return ("UNKNOWN");
    }
}

/*******************************************************************************
**
** Function         nfa_hciu_get_type_inst_names
**
** Description      This function returns command/response/event name.
**
** Returns          none
**
*******************************************************************************/
char *nfa_hciu_get_type_inst_names (UINT8 pipe, UINT8 type, UINT8 inst, char *p_buff)
{
    int   xx;

    xx = sprintf (p_buff, "Type: %s [0x%02x] ", nfa_hciu_type_2_str (type), type);

    switch (type)
    {
    case NFA_HCI_COMMAND_TYPE:
        sprintf (&p_buff[xx], "Inst: %s [0x%02x] ", nfa_hciu_instr_2_str (inst), inst);
        break;
    case NFA_HCI_EVENT_TYPE:
        sprintf (&p_buff[xx], "Evt: %s [0x%02x] ", nfa_hciu_evt_2_str (pipe, inst), inst);
        break;
    case NFA_HCI_RESPONSE_TYPE:
        sprintf (&p_buff[xx], "Resp: %s [0x%02x] ", nfa_hciu_get_response_name (inst), inst);
        break;
    default:
        sprintf (&p_buff[xx], "Inst: %u ", inst);
        break;
    }
    return (p_buff);
}

/*******************************************************************************
**
** Function         nfa_hciu_evt_2_str
**
** Description      This function returns the event name.
**
** Returns          pointer to the name
**
*******************************************************************************/
char *nfa_hciu_evt_2_str (UINT8 pipe_id, UINT8 evt)
{
    tNFA_HCI_DYN_PIPE   *p_pipe;

    if (  (pipe_id != NFA_HCI_ADMIN_PIPE)
        &&(pipe_id != NFA_HCI_LINK_MANAGEMENT_PIPE)
        &&((p_pipe = nfa_hciu_find_pipe_by_pid (pipe_id)) != NULL)  )
    {
        if (p_pipe->local_gate == NFA_HCI_CONNECTIVITY_GATE)
        {
            switch (evt)
            {
            case NFA_HCI_EVT_CONNECTIVITY:
                return ("EVT_CONNECTIVITY");
            case NFA_HCI_EVT_TRANSACTION:
                return ("EVT_TRANSACTION");
            case NFA_HCI_EVT_OPERATION_ENDED:
                return ("EVT_OPERATION_ENDED");
            default:
                return ("UNKNOWN");
            }
        }
    }

    switch (evt)
    {
    case NFA_HCI_EVT_HCI_END_OF_OPERATION:
        return ("EVT_END_OF_OPERATION");
    case NFA_HCI_EVT_POST_DATA:
        return ("EVT_POST_DATA");
    case NFA_HCI_EVT_HOT_PLUG:
        return ("EVT_HOT_PLUG");
    default:
        return ("UNKNOWN");
    }
}
#endif


static void handle_debug_loopback (BT_HDR *p_buf, UINT8 pipe, UINT8 type, UINT8 instruction)
{
    UINT8 *p = (UINT8 *) (p_buf + 1) + p_buf->offset;
    static UINT8  next_pipe = 0x10;

    if (type == NFA_HCI_COMMAND_TYPE)
    {
        switch (instruction)
        {
        case NFA_HCI_ADM_CREATE_PIPE:
            p[6] = next_pipe++;
            p[5] = p[4];
            p[4] = p[3];
            p[3] = p[2];
            p[2] = 3;
            p[1] = (NFA_HCI_RESPONSE_TYPE << 6) | NFA_HCI_ANY_OK;
            p_buf->len = p_buf->offset + 7;
            break;

        case NFA_HCI_ANY_GET_PARAMETER:
            p[1] = (NFA_HCI_RESPONSE_TYPE << 6) | NFA_HCI_ANY_OK;
            memcpy (&p[2], (UINT8 *) nfa_hci_cb.cfg.admin_gate.session_id, NFA_HCI_SESSION_ID_LEN);
            p_buf->len = p_buf->offset + 2 + NFA_HCI_SESSION_ID_LEN;
            break;

        default:
            p[1] = (NFA_HCI_RESPONSE_TYPE << 6) | NFA_HCI_ANY_OK;
            p_buf->len = p_buf->offset + 2;
            break;
        }
    }
    else if (type == NFA_HCI_RESPONSE_TYPE)
    {
        GKI_freebuf (p_buf);
        return;
    }

    p_buf->event = NFA_HCI_CHECK_QUEUE_EVT;
    nfa_sys_sendmsg (p_buf);
}
