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
 *  Handle ndef messages
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_sys.h"
#include "nfa_api.h"
#include "nfa_dm_int.h"
#include "nfa_sys_int.h"
#include "nfc_api.h"
#include "ndef_utils.h"

/*******************************************************************************
* URI Well-known-type prefixes
*******************************************************************************/
const UINT8 *nfa_dm_ndef_wkt_uri_str_tbl[] = {
    NULL,           /* 0x00 */
    (const UINT8*) "http://www.",  /* 0x01 */
    (const UINT8*) "https://www.", /* 0x02 */
    (const UINT8*) "http://",      /* 0x03 */
    (const UINT8*) "https://",     /* 0x04 */
    (const UINT8*) "tel:",         /* 0x05 */
    (const UINT8*) "mailto:",      /* 0x06 */
    (const UINT8*) "ftp://anonymous:anonymous@",   /* 0x07 */
    (const UINT8*) "ftp://ftp.",   /* 0x08 */
    (const UINT8*) "ftps://",      /* 0x09 */
    (const UINT8*) "sftp://",      /* 0x0A */
    (const UINT8*) "smb://",       /* 0x0B */
    (const UINT8*) "nfs://",       /* 0x0C */
    (const UINT8*) "ftp://",       /* 0x0D */
    (const UINT8*) "dav://",       /* 0x0E */
    (const UINT8*) "news:",        /* 0x0F */
    (const UINT8*) "telnet://",    /* 0x10 */
    (const UINT8*) "imap:",        /* 0x11 */
    (const UINT8*) "rtsp://",      /* 0x12 */
    (const UINT8*) "urn:",         /* 0x13 */
    (const UINT8*) "pop:",         /* 0x14 */
    (const UINT8*) "sip:",         /* 0x15 */
    (const UINT8*) "sips:",        /* 0x16 */
    (const UINT8*) "tftp:",        /* 0x17 */
    (const UINT8*) "btspp://",     /* 0x18 */
    (const UINT8*) "btl2cap://",   /* 0x19 */
    (const UINT8*) "btgoep://",    /* 0x1A */
    (const UINT8*) "tcpobex://",   /* 0x1B */
    (const UINT8*) "irdaobex://",  /* 0x1C */
    (const UINT8*) "file://",      /* 0x1D */
    (const UINT8*) "urn:epc:id:",  /* 0x1E */
    (const UINT8*) "urn:epc:tag:", /* 0x1F */
    (const UINT8*) "urn:epc:pat:", /* 0x20 */
    (const UINT8*) "urn:epc:raw:", /* 0x21 */
    (const UINT8*) "urn:epc:",     /* 0x22 */
    (const UINT8*) "urn:nfc:"      /* 0x23 */
};
#define NFA_DM_NDEF_WKT_URI_STR_TBL_SIZE (sizeof (nfa_dm_ndef_wkt_uri_str_tbl) / sizeof (UINT8 *))

/*******************************************************************************
**
** Function         nfa_dm_ndef_dereg_hdlr_by_handle
**
** Description      Deregister NDEF record type handler
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
void nfa_dm_ndef_dereg_hdlr_by_handle (tNFA_HANDLE ndef_type_handle)
{
    tNFA_DM_CB *p_cb = &nfa_dm_cb;
    UINT16 hdlr_idx;
    hdlr_idx = (UINT16) (ndef_type_handle & NFA_HANDLE_MASK);

    if (p_cb->p_ndef_handler[hdlr_idx])
    {
        GKI_freebuf (p_cb->p_ndef_handler[hdlr_idx]);
        p_cb->p_ndef_handler[hdlr_idx] = NULL;
    }
}

/*******************************************************************************
**
** Function         nfa_dm_ndef_dereg_all
**
** Description      Deregister all NDEF record type handlers (called during
**                  shutdown(.
**
** Returns          Nothing
**
*******************************************************************************/
void nfa_dm_ndef_dereg_all (void)
{
    tNFA_DM_CB *p_cb = &nfa_dm_cb;
    UINT32 i;

    for (i = 0; i < NFA_NDEF_MAX_HANDLERS; i++)
    {
        /* If this is a free slot, then remember it */
        if (p_cb->p_ndef_handler[i] != NULL)
        {
            GKI_freebuf (p_cb->p_ndef_handler[i]);
            p_cb->p_ndef_handler[i] = NULL;
        }
    }
}


/*******************************************************************************
**
** Function         nfa_dm_ndef_reg_hdlr
**
** Description      Register NDEF record type handler
**
** Returns          TRUE if message buffer is to be freed by caller
**
*******************************************************************************/
BOOLEAN nfa_dm_ndef_reg_hdlr (tNFA_DM_MSG *p_data)
{
    tNFA_DM_CB *p_cb = &nfa_dm_cb;
    UINT32 hdlr_idx, i;
    tNFA_DM_API_REG_NDEF_HDLR *p_reg_info = (tNFA_DM_API_REG_NDEF_HDLR *) p_data;
    tNFA_NDEF_REGISTER ndef_register;

    /* If registering default handler, check to see if one is already registered */
    if (p_reg_info->tnf == NFA_TNF_DEFAULT)
    {
        /* check if default handler is already registered */
        if (p_cb->p_ndef_handler[NFA_NDEF_DEFAULT_HANDLER_IDX])
        {
            NFA_TRACE_WARNING0 ("Default NDEF handler being changed.");

            /* Free old registration info */
            nfa_dm_ndef_dereg_hdlr_by_handle ((tNFA_HANDLE) NFA_NDEF_DEFAULT_HANDLER_IDX);
        }
        NFA_TRACE_DEBUG0 ("Default NDEF handler successfully registered.");
        hdlr_idx = NFA_NDEF_DEFAULT_HANDLER_IDX;
    }
    /* Get available entry in ndef_handler table, and check if requested type is already registered */
    else
    {
        hdlr_idx = NFA_HANDLE_INVALID;

        /* Check if this type is already registered */
        for (i = (NFA_NDEF_DEFAULT_HANDLER_IDX+1); i < NFA_NDEF_MAX_HANDLERS; i++)
        {
            /* If this is a free slot, then remember it */
            if (p_cb->p_ndef_handler[i] == NULL)
            {
                hdlr_idx = i;
                break;
            }
        }
    }

    if (hdlr_idx != NFA_HANDLE_INVALID)
    {
        /* Update the table */
        p_cb->p_ndef_handler[hdlr_idx] = p_reg_info;

        p_reg_info->ndef_type_handle = (tNFA_HANDLE) (NFA_HANDLE_GROUP_NDEF_HANDLER | hdlr_idx);

        ndef_register.ndef_type_handle = p_reg_info->ndef_type_handle;
        ndef_register.status = NFA_STATUS_OK;

        NFA_TRACE_DEBUG1 ("NDEF handler successfully registered. Handle=0x%08x", p_reg_info->ndef_type_handle);
        (*(p_reg_info->p_ndef_cback)) (NFA_NDEF_REGISTER_EVT, (tNFA_NDEF_EVT_DATA *) &ndef_register);

        return FALSE;       /* indicate that we will free message buffer when type_handler is deregistered */
    }
    else
    {
        /* Error */
        NFA_TRACE_ERROR0 ("NDEF handler failed to register.");
        ndef_register.ndef_type_handle = NFA_HANDLE_INVALID;
        ndef_register.status = NFA_STATUS_FAILED;
        (*(p_reg_info->p_ndef_cback)) (NFA_NDEF_REGISTER_EVT, (tNFA_NDEF_EVT_DATA *) &ndef_register);

        return TRUE;
    }
}

/*******************************************************************************
**
** Function         nfa_dm_ndef_dereg_hdlr
**
** Description      Deregister NDEF record type handler
**
** Returns          TRUE (message buffer to be freed by caller)
**
*******************************************************************************/
BOOLEAN nfa_dm_ndef_dereg_hdlr (tNFA_DM_MSG *p_data)
{
    tNFA_DM_API_DEREG_NDEF_HDLR *p_dereginfo = (tNFA_DM_API_DEREG_NDEF_HDLR *) p_data;

    /* Make sure this is a NDEF_HDLR handle */
    if (  ((p_dereginfo->ndef_type_handle & NFA_HANDLE_GROUP_MASK) != NFA_HANDLE_GROUP_NDEF_HANDLER)
        ||((p_dereginfo->ndef_type_handle & NFA_HANDLE_MASK) >= NFA_NDEF_MAX_HANDLERS)  )
    {
        NFA_TRACE_ERROR1 ("Invalid handle for NDEF type handler: 0x%08x", p_dereginfo->ndef_type_handle);
    }
    else
    {
        nfa_dm_ndef_dereg_hdlr_by_handle (p_dereginfo->ndef_type_handle);
    }


    return TRUE;
}

/*******************************************************************************
**
** Function         nfa_dm_ndef_find_next_handler
**
** Description      Find next ndef handler for a given record type
**
** Returns          void
**
*******************************************************************************/
tNFA_DM_API_REG_NDEF_HDLR *nfa_dm_ndef_find_next_handler (tNFA_DM_API_REG_NDEF_HDLR *p_init_handler,
                                                          UINT8                     tnf,
                                                          UINT8                     *p_type_name,
                                                          UINT8                     type_name_len,
                                                          UINT8                     *p_payload,
                                                          UINT32                    payload_len)
{
    tNFA_DM_CB *p_cb = &nfa_dm_cb;
    UINT8 i;

    /* if init_handler is NULL, then start with the first non-default handler */
    if (!p_init_handler)
        i=NFA_NDEF_DEFAULT_HANDLER_IDX+1;
    else
    {
        /* Point to handler index after p_init_handler */
        i = (p_init_handler->ndef_type_handle & NFA_HANDLE_MASK) + 1;
    }


    /* Look for next handler */
    for (; i < NFA_NDEF_MAX_HANDLERS; i++)
    {
        /* Check if TNF matches */
        if (  (p_cb->p_ndef_handler[i])
            &&(p_cb->p_ndef_handler[i]->tnf == tnf)  )
        {
            /* TNF matches. */
            /* If handler is for a specific URI type, check if type is WKT URI, */
            /* and that the URI prefix abrieviation for this handler matches */
            if (p_cb->p_ndef_handler[i]->flags & NFA_NDEF_FLAGS_WKT_URI)
            {
                /* This is a handler for a specific URI type */
                /* Check if this recurd is WKT URI */
                if ((p_payload) && (type_name_len == 1) && (*p_type_name == 'U'))
                {
                    /* Check if URI prefix abrieviation matches */
                    if ((payload_len>1) && (p_payload[0] == p_cb->p_ndef_handler[i]->uri_id))
                    {
                        /* URI prefix abrieviation matches */
                        /* If handler does not specify an absolute URI, then match found. */
                        /* If absolute URI, then compare URI for match (skip over uri_id in ndef payload) */
                        if (  (p_cb->p_ndef_handler[i]->uri_id != NFA_NDEF_URI_ID_ABSOLUTE)
                            ||(memcmp (&p_payload[1], p_cb->p_ndef_handler[i]->name, p_cb->p_ndef_handler[i]->name_len) == 0)  )
                        {
                            /* Handler found. */
                            break;
                        }
                    }
                    /* Check if handler is absolute URI but NDEF is using prefix abrieviation */
                    else if ((p_cb->p_ndef_handler[i]->uri_id == NFA_NDEF_URI_ID_ABSOLUTE) && (p_payload[0] != NFA_NDEF_URI_ID_ABSOLUTE))
                    {
                        /* Handler is absolute URI but NDEF is using prefix abrieviation. Compare URI prefix */
                        if (  (p_payload[0]<NFA_DM_NDEF_WKT_URI_STR_TBL_SIZE)
                            &&(memcmp (p_cb->p_ndef_handler[i]->name, (char *) nfa_dm_ndef_wkt_uri_str_tbl[p_payload[0]], p_cb->p_ndef_handler[i]->name_len) == 0)  )
                        {
                            /* Handler found. */
                            break;
                        }
                    }
                    /* Check if handler is using prefix abrieviation, but NDEF is using absolute URI */
                    else if ((p_cb->p_ndef_handler[i]->uri_id != NFA_NDEF_URI_ID_ABSOLUTE) && (p_payload[0] == NFA_NDEF_URI_ID_ABSOLUTE))
                    {
                        /* Handler is using prefix abrieviation, but NDEF is using absolute URI. Compare URI prefix */
                        if (  (p_cb->p_ndef_handler[i]->uri_id<NFA_DM_NDEF_WKT_URI_STR_TBL_SIZE)
                            &&(memcmp (&p_payload[1], nfa_dm_ndef_wkt_uri_str_tbl[p_cb->p_ndef_handler[i]->uri_id], strlen ((const char*) nfa_dm_ndef_wkt_uri_str_tbl[p_cb->p_ndef_handler[i]->uri_id])) == 0)  )
                        {
                            /* Handler found. */
                            break;
                        }
                    }
                }
            }
            /* Not looking for specific URI. Check if type_name for this handler matches the NDEF record's type_name */
            else if (p_cb->p_ndef_handler[i]->name_len == type_name_len)
            {
                if (  (type_name_len == 0)
                    ||(memcmp(p_cb->p_ndef_handler[i]->name, p_type_name, type_name_len) == 0)  )
                {
                    /* Handler found */
                    break;
                }
            }
        }

    }

    if (i < NFA_NDEF_MAX_HANDLERS)
        return (p_cb->p_ndef_handler[i]);
    else
        return (NULL);
}

/*******************************************************************************
**
** Function         nfa_dm_ndef_clear_notified_flag
**
** Description      Clear 'whole_message_notified' flag for all the handlers
**                  (flag used to indicate that this handler has already
**                  handled the entire incoming NDEF message)
**
** Returns          void
**
*******************************************************************************/
void nfa_dm_ndef_clear_notified_flag (void)
{
    tNFA_DM_CB *p_cb = &nfa_dm_cb;
    UINT8 i;

    for (i = 0; i < NFA_NDEF_MAX_HANDLERS; i++)
    {
        if (p_cb->p_ndef_handler[i])
        {
            p_cb->p_ndef_handler[i]->flags &= ~NFA_NDEF_FLAGS_WHOLE_MESSAGE_NOTIFIED;
        }
    }


}

/*******************************************************************************
**
** Function         nfa_dm_ndef_handle_message
**
** Description      Handle incoming ndef message
**
** Returns          void
**
*******************************************************************************/
void nfa_dm_ndef_handle_message (tNFA_STATUS status, UINT8 *p_msg_buf, UINT32 len)
{
    tNFA_DM_CB *p_cb = &nfa_dm_cb;
    tNDEF_STATUS ndef_status;
    UINT8 *p_rec, *p_ndef_start, *p_type, *p_payload, *p_rec_end;
    UINT32 payload_len;
    UINT8 tnf, type_len, rec_hdr_flags, id_len;
    tNFA_DM_API_REG_NDEF_HDLR *p_handler;
    tNFA_NDEF_DATA ndef_data;
    UINT8 rec_count = 0;
    BOOLEAN record_handled, entire_message_handled;

    NFA_TRACE_DEBUG3 ("nfa_dm_ndef_handle_message status=%i, msgbuf=%08x, len=%i", status, p_msg_buf, len);

    if (status != NFA_STATUS_OK)
    {
        /* If problem reading NDEF message, then exit (no action required) */
        return;
    }

    /* If in exclusive RF mode is activer, then route NDEF message callback registered with NFA_StartExclusiveRfControl */
    if ((p_cb->flags & NFA_DM_FLAGS_EXCL_RF_ACTIVE) && (p_cb->p_excl_ndef_cback))
    {
        ndef_data.ndef_type_handle = 0;     /* No ndef-handler handle, since this callback is not from RegisterNDefHandler */
        ndef_data.p_data = p_msg_buf;
        ndef_data.len = len;
        (*p_cb->p_excl_ndef_cback) (NFA_NDEF_DATA_EVT, (tNFA_NDEF_EVT_DATA *) &ndef_data);
        return;
    }

    /* Handle zero length - notify default handler */
    if (len == 0)
    {
        if ((p_handler = p_cb->p_ndef_handler[NFA_NDEF_DEFAULT_HANDLER_IDX]) != NULL)
        {
            NFA_TRACE_DEBUG0 ("Notifying default handler of zero-length NDEF message...");
            ndef_data.ndef_type_handle = p_handler->ndef_type_handle;
            ndef_data.p_data = NULL;   /* Start of record */
            ndef_data.len = 0;
            (*p_handler->p_ndef_cback) (NFA_NDEF_DATA_EVT, (tNFA_NDEF_EVT_DATA *) &ndef_data);
        }
        return;
    }

    /* Validate the NDEF message */
    if ((ndef_status = NDEF_MsgValidate (p_msg_buf, len, TRUE)) != NDEF_OK)
    {
        NFA_TRACE_ERROR1 ("Received invalid NDEF message. NDEF status=0x%x", ndef_status);
        return;
    }

    /* NDEF message received from backgound polling. Pass the NDEF message to the NDEF handlers */

    /* New NDEF message. Clear 'notified' flag for all the handlers */
    nfa_dm_ndef_clear_notified_flag ();

    /* Indicate that no handler has handled this entire NDEF message (e.g. connection-handover handler *) */
    entire_message_handled = FALSE;

    /* Get first record in message */
    p_rec = p_ndef_start = p_msg_buf;

    /* Check each record in the NDEF message */
    while (p_rec != NULL)
    {
        /* Get record type */
        p_type = NDEF_RecGetType (p_rec, &tnf, &type_len);

        /* Indicate record not handled yet */
        record_handled = FALSE;

        /* Get pointer to record payload */
        p_payload = NDEF_RecGetPayload (p_rec, &payload_len);

        /* Find first handler for this type */
        if ((p_handler = nfa_dm_ndef_find_next_handler (NULL, tnf, p_type, type_len, p_payload, payload_len)) == NULL)
        {
            /* Not a registered NDEF type. Use default handler */
            if ((p_handler = p_cb->p_ndef_handler[NFA_NDEF_DEFAULT_HANDLER_IDX]) != NULL)
            {
                NFA_TRACE_DEBUG0 ("No handler found. Using default handler...");
            }
        }

        while (p_handler)
        {
            /* If handler is for whole NDEF message, and it has already been notified, then skip notification */
            if (p_handler->flags & NFA_NDEF_FLAGS_WHOLE_MESSAGE_NOTIFIED)
            {
                /* Look for next handler */
                p_handler = nfa_dm_ndef_find_next_handler (p_handler, tnf, p_type, type_len, p_payload, payload_len);
                continue;
            }

            /* Get pointer to record payload */
            NFA_TRACE_DEBUG1 ("Calling ndef type handler (%x)", p_handler->ndef_type_handle);

            ndef_data.ndef_type_handle = p_handler->ndef_type_handle;
            ndef_data.p_data = p_rec;   /* Start of record */

            /* Calculate length of NDEF record */
            if (p_payload != NULL)
                ndef_data.len = payload_len + (UINT32) (p_payload - p_rec);
            else
            {
                /* If no payload, calculate length of ndef record header */
                p_rec_end = p_rec;

                /* First byte is the header flags */
                rec_hdr_flags = *p_rec_end++;

                /* Next byte is the type field length */
                type_len = *p_rec_end++;

                /* Next is the payload length (1 or 4 bytes) */
                if (rec_hdr_flags & NDEF_SR_MASK)
                {
                    p_rec_end++;
                }
                else
                {
                    p_rec_end+=4;
                }

                /* ID field Length */
                if (rec_hdr_flags & NDEF_IL_MASK)
                    id_len = *p_rec_end++;
                else
                    id_len = 0;
                p_rec_end+=id_len;

                ndef_data.len = (UINT32) (p_rec_end - p_rec);
            }

            /* If handler wants entire ndef message, then pass pointer to start of message and  */
            /* set 'notified' flag so handler won't get notified on subsequent records for this */
            /* NDEF message.                                                                    */
            if (p_handler->flags & NFA_NDEF_FLAGS_HANDLE_WHOLE_MESSAGE)
            {
                ndef_data.p_data = p_ndef_start;   /* Start of NDEF message */
                ndef_data.len = len;
                p_handler->flags |= NFA_NDEF_FLAGS_WHOLE_MESSAGE_NOTIFIED;

                /* Indicate that at least one handler has received entire NDEF message */
                entire_message_handled = TRUE;
            }

            /* Notify NDEF type handler */
            (*p_handler->p_ndef_cback) (NFA_NDEF_DATA_EVT, (tNFA_NDEF_EVT_DATA *) &ndef_data);

            /* Indicate that at lease one handler has received this record */
            record_handled = TRUE;

            /* Look for next handler */
            p_handler = nfa_dm_ndef_find_next_handler (p_handler, tnf, p_type, type_len, p_payload, payload_len);
        }


        /* Check if at least one handler was notified of this record (only happens if no default handler was register) */
        if ((!record_handled) && (!entire_message_handled))
        {
            /* Unregistered NDEF record type; no default handler */
            NFA_TRACE_WARNING1 ("Unhandled NDEF record (#%i)", rec_count);
        }

        rec_count++;
        p_rec = NDEF_MsgGetNextRec (p_rec);
    }
}
