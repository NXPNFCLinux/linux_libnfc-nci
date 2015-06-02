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
 *  This file contains source code for some utility functions to help parse
 *  and build NFC Data Exchange Format (NDEF) messages
 *
 ******************************************************************************/
#include <string.h>
#include "ndef_utils.h"

/*******************************************************************************
**
**              Static Local Functions
**
*******************************************************************************/


/*******************************************************************************
**
** Function         shiftdown
**
** Description      shift memory down (to make space to insert a record)
**
*******************************************************************************/
static void shiftdown (UINT8 *p_mem, UINT32 len, UINT32 shift_amount)
{
    register UINT8 *ps = p_mem + len - 1;
    register UINT8 *pd = ps + shift_amount;
    register UINT32 xx;

    for (xx = 0; xx < len; xx++)
        *pd-- = *ps--;
}

/*******************************************************************************
**
** Function         shiftup
**
** Description      shift memory up (to delete a record)
**
*******************************************************************************/
static void shiftup (UINT8 *p_dest, UINT8 *p_src, UINT32 len)
{
    register UINT8 *ps = p_src;
    register UINT8 *pd = p_dest;
    register UINT32 xx;

    for (xx = 0; xx < len; xx++)
        *pd++ = *ps++;
}

/*******************************************************************************
**
** Function         NDEF_MsgValidate
**
** Description      This function validates an NDEF message.
**
** Returns          TRUE if all OK, or FALSE if the message is invalid.
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgValidate (UINT8 *p_msg, UINT32 msg_len, BOOLEAN b_allow_chunks)
{
    UINT8   *p_rec = p_msg;
    UINT8   *p_end = p_msg + msg_len;
    UINT8   rec_hdr=0, type_len, id_len;
    int     count;
    UINT32  payload_len;
    BOOLEAN bInChunk = FALSE;

    if ( (p_msg == NULL) || (msg_len < 3) )
        return (NDEF_MSG_TOO_SHORT);

    /* The first record must have the MB bit set */
    if ((*p_msg & NDEF_MB_MASK) == 0)
        return (NDEF_MSG_NO_MSG_BEGIN);

    /* The first record cannot be a chunk */
    if ((*p_msg & NDEF_TNF_MASK) == NDEF_TNF_UNCHANGED)
        return (NDEF_MSG_UNEXPECTED_CHUNK);

    for (count = 0; p_rec < p_end; count++)
    {
        /* if less than short record header */
        if (p_rec + 3 > p_end)
            return (NDEF_MSG_TOO_SHORT);

        rec_hdr = *p_rec++;

        /* The second and all subsequent records must NOT have the MB bit set */
        if ( (count > 0) && (rec_hdr & NDEF_MB_MASK) )
            return (NDEF_MSG_EXTRA_MSG_BEGIN);

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
        {
            /* if less than 4 bytes payload length */
            if (p_rec + 4 > p_end)
                return (NDEF_MSG_TOO_SHORT);

            BE_STREAM_TO_UINT32 (payload_len, p_rec);
        }

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
        {
            /* if less than 1 byte ID field length */
            if (p_rec + 1 > p_end)
                return (NDEF_MSG_TOO_SHORT);

            id_len = *p_rec++;
        }
        else
            id_len = 0;

        /* A chunk must have type "unchanged", and no type or ID fields */
        if (rec_hdr & NDEF_CF_MASK)
        {
            if (!b_allow_chunks)
                return (NDEF_MSG_UNEXPECTED_CHUNK);

            /* Inside a chunk, the type must be unchanged and no type or ID field i sallowed */
            if (bInChunk)
            {
                if ( (type_len != 0) || (id_len != 0) || ((rec_hdr & NDEF_TNF_MASK) != NDEF_TNF_UNCHANGED) )
                    return (NDEF_MSG_INVALID_CHUNK);
            }
            else
            {
                /* First record of a chunk must NOT have type "unchanged" */
                if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNCHANGED)
                    return (NDEF_MSG_INVALID_CHUNK);

                bInChunk = TRUE;
            }
        }
        else
        {
            /* This may be the last guy in a chunk. */
            if (bInChunk)
            {
                if ( (type_len != 0) || (id_len != 0) || ((rec_hdr & NDEF_TNF_MASK) != NDEF_TNF_UNCHANGED) )
                    return (NDEF_MSG_INVALID_CHUNK);

                bInChunk = FALSE;
            }
            else
            {
                /* If not in a chunk, the record must NOT have type "unchanged" */
                if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNCHANGED)
                    return (NDEF_MSG_INVALID_CHUNK);
            }
        }

        /* An empty record must NOT have a type, ID or payload */
        if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_EMPTY)
        {
            if ( (type_len != 0) || (id_len != 0) || (payload_len != 0) )
                return (NDEF_MSG_INVALID_EMPTY_REC);
        }

        if ((rec_hdr & NDEF_TNF_MASK) == NDEF_TNF_UNKNOWN)
        {
            if (type_len != 0)
                return (NDEF_MSG_LENGTH_MISMATCH);
        }

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);

        if (rec_hdr & NDEF_ME_MASK)
            break;

        rec_hdr = 0;
    }

    /* The last record should have the ME bit set */
    if ((rec_hdr & NDEF_ME_MASK) == 0)
        return (NDEF_MSG_NO_MSG_END);

    /* p_rec should equal p_end if all the length fields were correct */
    if (p_rec != p_end)
        return (NDEF_MSG_LENGTH_MISMATCH);

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgGetNumRecs
**
** Description      This function gets the number of records in the given NDEF
**                  message.
**
** Returns          The record count, or 0 if the message is invalid.
**
*******************************************************************************/
INT32 NDEF_MsgGetNumRecs (UINT8 *p_msg)
{
    UINT8   *p_rec = p_msg;
    UINT8   rec_hdr, type_len, id_len;
    int     count;
    UINT32  payload_len;

    for (count = 0; ; )
    {
        count++;

        rec_hdr = *p_rec++;
        if (rec_hdr & NDEF_ME_MASK)
            break;

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);
    }

    /* Return the number of records found */
    return (count);
}

/*******************************************************************************
**
** Function         NDEF_MsgGetRecLength
**
** Description      This function returns length of the current record in the given
**                  NDEF message.
**
** Returns          Length of record
**
*******************************************************************************/
UINT32 NDEF_MsgGetRecLength (UINT8 *p_cur_rec)
{
    UINT8   rec_hdr, type_len, id_len;
    UINT32  rec_len = 0;
    UINT32  payload_len;

    /* Get the current record's header */
    rec_hdr = *p_cur_rec++;
    rec_len++;

    /* Type field length */
    type_len = *p_cur_rec++;
    rec_len++;

    /* Payload length - can be 1 or 4 bytes */
    if (rec_hdr & NDEF_SR_MASK)
    {
        payload_len = *p_cur_rec++;
        rec_len++;
    }
    else
    {
        BE_STREAM_TO_UINT32 (payload_len, p_cur_rec);
        rec_len += 4;
    }

    /* ID field Length */
    if (rec_hdr & NDEF_IL_MASK)
    {
        id_len = *p_cur_rec++;
        rec_len++;
    }
    else
        id_len = 0;

    /* Total length of record */
    rec_len += (payload_len + type_len + id_len);

    return (rec_len);
}

/*******************************************************************************
**
** Function         NDEF_MsgGetNextRec
**
** Description      This function gets a pointer to the next record in the given
**                  NDEF message. If the current record pointer is NULL, a pointer
**                  to the first record is returned.
**
** Returns          Pointer to the start of the record, or NULL if no more
**
*******************************************************************************/
UINT8 *NDEF_MsgGetNextRec (UINT8 *p_cur_rec)
{
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    /* Get the current record's header */
    rec_hdr = *p_cur_rec++;

    /* If this is the last record, return NULL */
    if (rec_hdr & NDEF_ME_MASK)
        return (NULL);

    /* Type field length */
    type_len = *p_cur_rec++;

    /* Payload length - can be 1 or 4 bytes */
    if (rec_hdr & NDEF_SR_MASK)
        payload_len = *p_cur_rec++;
    else
        BE_STREAM_TO_UINT32 (payload_len, p_cur_rec);

    /* ID field Length */
    if (rec_hdr & NDEF_IL_MASK)
        id_len = *p_cur_rec++;
    else
        id_len = 0;

    /* Point to next record */
    p_cur_rec += (payload_len + type_len + id_len);

    return (p_cur_rec);
}

/*******************************************************************************
**
** Function         NDEF_MsgGetRecByIndex
**
** Description      This function gets a pointer to the record with the given
**                  index (0-based index) in the given NDEF message.
**
** Returns          Pointer to the start of the record, or NULL
**
*******************************************************************************/
UINT8 *NDEF_MsgGetRecByIndex (UINT8 *p_msg, INT32 index)
{
    UINT8   *p_rec = p_msg;
    UINT8   rec_hdr, type_len, id_len;
    INT32   count;
    UINT32  payload_len;

    for (count = 0; ; count++)
    {
        if (count == index)
            return (p_rec);

        rec_hdr = *p_rec++;

        if (rec_hdr & NDEF_ME_MASK)
            return (NULL);

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);
    }

    /* If here, there is no record of that index */
    return (NULL);
}


/*******************************************************************************
**
** Function         NDEF_MsgGetLastRecInMsg
**
** Description      This function gets a pointer to the last record in the
**                  given NDEF message.
**
** Returns          Pointer to the start of the last record, or NULL if some problem
**
*******************************************************************************/
UINT8 *NDEF_MsgGetLastRecInMsg (UINT8 *p_msg)
{
    UINT8   *p_rec = p_msg;
    UINT8   *pRecStart;
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    for ( ; ; )
    {
        pRecStart = p_rec;
        rec_hdr = *p_rec++;

        if (rec_hdr & NDEF_ME_MASK)
            break;

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);
    }

    return (pRecStart);
}


/*******************************************************************************
**
** Function         NDEF_MsgGetFirstRecByType
**
** Description      This function gets a pointer to the first record with the given
**                  record type in the given NDEF message.
**
** Returns          Pointer to the start of the record, or NULL
**
*******************************************************************************/
UINT8 *NDEF_MsgGetFirstRecByType (UINT8 *p_msg, UINT8 tnf, UINT8 *p_type, UINT8 tlen)
{
    UINT8   *p_rec = p_msg;
    UINT8   *pRecStart;
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    for ( ; ; )
    {
        pRecStart = p_rec;

        rec_hdr = *p_rec++;

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* At this point, p_rec points to the start of the type field. We need to */
        /* compare the type of the type, the length of the type and the data     */
        if ( ((rec_hdr & NDEF_TNF_MASK) == tnf)
         &&  (type_len == tlen)
         &&  (!memcmp (p_rec, p_type, tlen)) )
             return (pRecStart);

        /* If this was the last record, return NULL */
        if (rec_hdr & NDEF_ME_MASK)
            return (NULL);

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);
    }

    /* If here, there is no record of that type */
    return (NULL);
}

/*******************************************************************************
**
** Function         NDEF_MsgGetNextRecByType
**
** Description      This function gets a pointer to the next record with the given
**                  record type in the given NDEF message.
**
** Returns          Pointer to the start of the record, or NULL
**
*******************************************************************************/
UINT8 *NDEF_MsgGetNextRecByType (UINT8 *p_cur_rec, UINT8 tnf, UINT8 *p_type, UINT8 tlen)
{
    UINT8   *p_rec;
    UINT8   *pRecStart;
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    /* If this is the last record in the message, return NULL */
    if ((p_rec = NDEF_MsgGetNextRec (p_cur_rec)) == NULL)
        return (NULL);

    for ( ; ; )
    {
        pRecStart = p_rec;

        rec_hdr = *p_rec++;

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* At this point, p_rec points to the start of the type field. We need to */
        /* compare the type of the type, the length of the type and the data     */
        if ( ((rec_hdr & NDEF_TNF_MASK) == tnf)
         &&  (type_len == tlen)
         &&  (!memcmp (p_rec, p_type, tlen)) )
             return (pRecStart);

        /* If this was the last record, return NULL */
        if (rec_hdr & NDEF_ME_MASK)
            break;

        /* Point to next record */
        p_rec += (payload_len + type_len + id_len);
    }

    /* If here, there is no record of that type */
    return (NULL);
}


/*******************************************************************************
**
** Function         NDEF_MsgGetFirstRecById
**
** Description      This function gets a pointer to the first record with the given
**                  record id in the given NDEF message.
**
** Returns          Pointer to the start of the record, or NULL
**
*******************************************************************************/
UINT8 *NDEF_MsgGetFirstRecById (UINT8 *p_msg, UINT8 *p_id, UINT8 ilen)
{
    UINT8   *p_rec = p_msg;
    UINT8   *pRecStart;
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    for ( ; ; )
    {
        pRecStart = p_rec;

        rec_hdr = *p_rec++;

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* At this point, p_rec points to the start of the type field. Skip it */
        p_rec += type_len;

        /* At this point, p_rec points to the start of the ID field. Compare length and data */
        if ( (id_len == ilen) && (!memcmp (p_rec, p_id, ilen)) )
             return (pRecStart);

        /* If this was the last record, return NULL */
        if (rec_hdr & NDEF_ME_MASK)
            return (NULL);

        /* Point to next record */
        p_rec += (id_len + payload_len);
    }

    /* If here, there is no record of that ID */
    return (NULL);
}

/*******************************************************************************
**
** Function         NDEF_MsgGetNextRecById
**
** Description      This function gets a pointer to the next record with the given
**                  record id in the given NDEF message.
**
** Returns          Pointer to the start of the record, or NULL
**
*******************************************************************************/
UINT8 *NDEF_MsgGetNextRecById (UINT8 *p_cur_rec, UINT8 *p_id, UINT8 ilen)
{
    UINT8   *p_rec;
    UINT8   *pRecStart;
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    /* If this is the last record in the message, return NULL */
    if ((p_rec = NDEF_MsgGetNextRec (p_cur_rec)) == NULL)
        return (NULL);

    for ( ; ; )
    {
        pRecStart = p_rec;

        rec_hdr = *p_rec++;

        /* Type field length */
        type_len = *p_rec++;

        /* Payload length - can be 1 or 4 bytes */
        if (rec_hdr & NDEF_SR_MASK)
            payload_len = *p_rec++;
        else
            BE_STREAM_TO_UINT32 (payload_len, p_rec);

        /* ID field Length */
        if (rec_hdr & NDEF_IL_MASK)
            id_len = *p_rec++;
        else
            id_len = 0;

        /* At this point, p_rec points to the start of the type field. Skip it */
        p_rec += type_len;

        /* At this point, p_rec points to the start of the ID field. Compare length and data */
        if ( (id_len == ilen) && (!memcmp (p_rec, p_id, ilen)) )
             return (pRecStart);

        /* If this was the last record, return NULL */
        if (rec_hdr & NDEF_ME_MASK)
            break;

        /* Point to next record */
        p_rec += (id_len + payload_len);
    }

    /* If here, there is no record of that ID */
    return (NULL);
}

/*******************************************************************************
**
** Function         NDEF_RecGetType
**
** Description      This function gets a pointer to the record type for the given NDEF record.
**
** Returns          Pointer to Type (NULL if none). TNF and len are filled in.
**
*******************************************************************************/
UINT8 *NDEF_RecGetType (UINT8 *p_rec, UINT8 *p_tnf, UINT8 *p_type_len)
{
    UINT8   rec_hdr, type_len;

    /* First byte is the record header */
    rec_hdr = *p_rec++;

    /* Next byte is the type field length */
    type_len = *p_rec++;

    /* Skip the payload length */
    if (rec_hdr & NDEF_SR_MASK)
        p_rec += 1;
    else
        p_rec += 4;

    /* Skip ID field Length, if present */
    if (rec_hdr & NDEF_IL_MASK)
        p_rec++;

    /* At this point, p_rec points to the start of the type field.  */
    *p_type_len = type_len;
    *p_tnf      = rec_hdr & NDEF_TNF_MASK;

    if (type_len == 0)
        return (NULL);
    else
        return (p_rec);
}

/*******************************************************************************
**
** Function         NDEF_RecGetId
**
** Description      This function gets a pointer to the record id for the given NDEF record.
**
** Returns          Pointer to Id (NULL if none). ID Len is filled in.
**
*******************************************************************************/
UINT8 *NDEF_RecGetId (UINT8 *p_rec, UINT8 *p_id_len)
{
    UINT8   rec_hdr, type_len;

    /* First byte is the record header */
    rec_hdr = *p_rec++;

    /* Next byte is the type field length */
    type_len = *p_rec++;

    /* Skip the payload length */
    if (rec_hdr & NDEF_SR_MASK)
        p_rec++;
    else
        p_rec += 4;

    /* ID field Length */
    if (rec_hdr & NDEF_IL_MASK)
        *p_id_len = *p_rec++;
    else
        *p_id_len = 0;

    /* p_rec now points to the start of the type field. The ID field follows it */
    if (*p_id_len == 0)
        return (NULL);
    else
        return (p_rec + type_len);
}


/*******************************************************************************
**
** Function         NDEF_RecGetPayload
**
** Description      This function gets a pointer to the payload for the given NDEF record.
**
** Returns          a pointer to the payload (or NULL none). Payload len filled in.
**
*******************************************************************************/
UINT8 *NDEF_RecGetPayload (UINT8 *p_rec, UINT32 *p_payload_len)
{
    UINT8   rec_hdr, type_len, id_len;
    UINT32  payload_len;

    /* First byte is the record header */
    rec_hdr = *p_rec++;

    /* Next byte is the type field length */
    type_len = *p_rec++;

    /* Next is the payload length (1 or 4 bytes) */
    if (rec_hdr & NDEF_SR_MASK)
        payload_len = *p_rec++;
    else
        BE_STREAM_TO_UINT32 (payload_len, p_rec);

    *p_payload_len = payload_len;

    /* ID field Length */
    if (rec_hdr & NDEF_IL_MASK)
        id_len = *p_rec++;
    else
        id_len = 0;

    /* p_rec now points to the start of the type field. The ID field follows it, then the payload */
    if (payload_len == 0)
        return (NULL);
    else
        return (p_rec + type_len + id_len);
}


/*******************************************************************************
**
** Function         NDEF_MsgInit
**
** Description      This function initializes an NDEF message.
**
** Returns          void
**                  *p_cur_size is initialized to 0
**
*******************************************************************************/
void NDEF_MsgInit (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size)
{
    *p_cur_size = 0;
    memset (p_msg, 0, max_size);
}

/*******************************************************************************
**
** Function         NDEF_MsgAddRec
**
** Description      This function adds an NDEF record to the end of an NDEF message.
**
** Returns          OK, or error if the record did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
extern tNDEF_STATUS  NDEF_MsgAddRec (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                     UINT8 tnf, UINT8 *p_type, UINT8 type_len,
                                     UINT8 *p_id, UINT8  id_len,
                                     UINT8 *p_payload, UINT32 payload_len)
{
    UINT8   *p_rec = p_msg + *p_cur_size;
    UINT32  recSize;
    int     plen = (payload_len < 256) ? 1 : 4;
    int     ilen = (id_len == 0) ? 0 : 1;

    if (tnf > NDEF_TNF_RESERVED)
    {
        tnf = NDEF_TNF_UNKNOWN;
        type_len  = 0;
    }

    /* First, make sure the record will fit. we need at least 2 bytes for header and type length */
    recSize = payload_len + 2 + type_len + plen + ilen + id_len;

    if ((*p_cur_size + recSize) > max_size)
        return (NDEF_MSG_INSUFFICIENT_MEM);

    /* Construct the record header. For the first record, set both begin and end bits */
    if (*p_cur_size == 0)
        *p_rec = tnf | NDEF_MB_MASK | NDEF_ME_MASK;
    else
    {
        /* Find the previous last and clear his 'Message End' bit */
        UINT8  *pLast = NDEF_MsgGetLastRecInMsg (p_msg);

        if (!pLast)
            return (NDEF_MSG_NO_MSG_END);

        *pLast &= ~NDEF_ME_MASK;
        *p_rec   = tnf | NDEF_ME_MASK;
    }

    if (plen == 1)
        *p_rec |= NDEF_SR_MASK;

    if (ilen != 0)
        *p_rec |= NDEF_IL_MASK;

    p_rec++;

    /* The next byte is the type field length */
    *p_rec++ = type_len;

    /* Payload length - can be 1 or 4 bytes */
    if (plen == 1)
        *p_rec++ = (UINT8)payload_len;
    else
         UINT32_TO_BE_STREAM (p_rec, payload_len);

    /* ID field Length (optional) */
    if (ilen > 0)
        *p_rec++ = id_len;

    /* Next comes the type */
    if (type_len)
    {
        if (p_type)
            memcpy (p_rec, p_type, type_len);

        p_rec += type_len;
    }

    /* Next comes the ID */
    if (id_len)
    {
        if (p_id)
            memcpy (p_rec, p_id, id_len);

        p_rec += id_len;
    }

    /* And lastly the payload. If NULL, the app just wants to reserve memory */
    if (p_payload)
        memcpy (p_rec, p_payload, payload_len);

    *p_cur_size += recSize;

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgInsertRec
**
** Description      This function inserts a record at a specific index into the
**                  given NDEF message
**
** Returns          OK, or error if the record did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
extern tNDEF_STATUS NDEF_MsgInsertRec (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size, INT32 index,
                                       UINT8 tnf, UINT8 *p_type, UINT8 type_len,
                                       UINT8 *p_id, UINT8  id_len,
                                       UINT8 *p_payload, UINT32 payload_len)
{
    UINT8   *p_rec;
    UINT32  recSize;
    INT32   plen = (payload_len < 256) ? 1 : 4;
    INT32   ilen = (id_len == 0) ? 0 : 1;

    /* First, make sure the record will fit. we need at least 2 bytes for header and type length */
    recSize = payload_len + 2 + type_len + plen + ilen + id_len;

    if ((*p_cur_size + recSize) > max_size)
        return (NDEF_MSG_INSUFFICIENT_MEM);

    /* See where the new record goes. If at the end, call the 'AddRec' function */
    if ( (index >= NDEF_MsgGetNumRecs (p_msg))
      || ((p_rec = NDEF_MsgGetRecByIndex(p_msg, index)) == NULL) )
    {
        return NDEF_MsgAddRec (p_msg, max_size, p_cur_size, tnf, p_type, type_len,
                               p_id, id_len, p_payload, payload_len);
    }

    /* If we are inserting at the beginning, remove the MB bit from the current first */
    if (index == 0)
        *p_msg &= ~NDEF_MB_MASK;

    /* Make space for the new record */
    shiftdown (p_rec, (UINT32)(*p_cur_size - (p_rec - p_msg)), recSize);

    /* If adding at the beginning, set begin bit */
    if (index == 0)
        *p_rec = tnf | NDEF_MB_MASK;
    else
        *p_rec = tnf;

    if (plen == 1)
        *p_rec |= NDEF_SR_MASK;

    if (ilen != 0)
        *p_rec |= NDEF_IL_MASK;

    p_rec++;

    /* The next byte is the type field length */
    *p_rec++ = type_len;

    /* Payload length - can be 1 or 4 bytes */
    if (plen == 1)
        *p_rec++ = (UINT8)payload_len;
    else
         UINT32_TO_BE_STREAM (p_rec, payload_len);

    /* ID field Length (optional) */
    if (ilen != 0)
        *p_rec++ = id_len;

    /* Next comes the type */
    if (type_len)
    {
        if (p_type)
            memcpy (p_rec, p_type, type_len);

        p_rec += type_len;
    }

    /* Next comes the ID */
    if (ilen != 0)
    {
        if (p_id)
            memcpy (p_rec, p_id, id_len);

        p_rec += id_len;
    }

    /* And lastly the payload. If NULL, the app just wants to reserve memory */
    if (p_payload)
        memcpy (p_rec, p_payload, payload_len);

    *p_cur_size += recSize;

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgAppendRec
**
** Description      This function adds NDEF records to the end of an NDEF message.
**
** Returns          OK, or error if the record did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
extern tNDEF_STATUS  NDEF_MsgAppendRec (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                        UINT8 *p_new_rec, UINT32 new_rec_len)
{
    UINT8   *p_rec;
    tNDEF_STATUS    status;

    /* First, validate new records */
    if ((status = NDEF_MsgValidate(p_new_rec, new_rec_len, FALSE)) != NDEF_OK)
        return (status);

    /* First, make sure the record will fit */
    if ((*p_cur_size + new_rec_len) > max_size)
        return (NDEF_MSG_INSUFFICIENT_MEM);

    /* Find where to copy new record */
    if (*p_cur_size == 0)
        p_rec = p_msg;
    else
    {
        /* Find the previous last and clear his 'Message End' bit */
        UINT8  *pLast = NDEF_MsgGetLastRecInMsg (p_msg);

        if (!pLast)
            return (NDEF_MSG_NO_MSG_END);

        *pLast &= ~NDEF_ME_MASK;
        p_rec   = p_msg + *p_cur_size;

        /* clear 'Message Begin' bit of new record */
        *p_new_rec &= ~NDEF_MB_MASK;
    }

    /* append new records */
    memcpy (p_rec, p_new_rec, new_rec_len);

    *p_cur_size += new_rec_len;

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgAppendPayload
**
** Description      This function appends extra payload to a specific record in the
**                  given NDEF message
**
** Returns          OK, or error if the extra payload did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgAppendPayload (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                    UINT8 *p_rec, UINT8 *p_add_pl, UINT32 add_pl_len)
{
    UINT32      prev_paylen, new_paylen;
    UINT8       *p_prev_pl, *pp;
    UINT8       incr_lenfld = 0;
    UINT8       type_len, id_len;

    /* Skip header */
    pp = p_rec + 1;

    /* Next byte is the type field length */
    type_len = *pp++;

    /* Next is the payload length (1 or 4 bytes) */
    if (*p_rec & NDEF_SR_MASK)
        prev_paylen = *pp++;
    else
        BE_STREAM_TO_UINT32 (prev_paylen, pp);

    /* ID field Length */
    if (*p_rec & NDEF_IL_MASK)
        id_len = *pp++;
    else
        id_len = 0;

    p_prev_pl = pp + type_len + id_len;

    new_paylen = prev_paylen + add_pl_len;

    /* Previous payload may be < 256, and this addition may make it larger than 256 */
    /* If that were to happen, the payload length field goes from 1 byte to 4 bytes */
    if ( (prev_paylen < 256) && (new_paylen > 255) )
        incr_lenfld = 3;

    /* Check that it all fits */
    if ((*p_cur_size + add_pl_len + incr_lenfld) > max_size)
        return (NDEF_MSG_INSUFFICIENT_MEM);

    /* Point to payload length field */
    pp = p_rec + 2;

    /* If we need to increase the length field from 1 to 4 bytes, do it first */
    if (incr_lenfld)
    {
        shiftdown (pp + 1, (UINT32)(*p_cur_size - (pp - p_msg) - 1), 3);
        p_prev_pl += 3;
    }

    /* Store in the new length */
    if (new_paylen > 255)
    {
        *p_rec &= ~NDEF_SR_MASK;
        UINT32_TO_BE_STREAM (pp, new_paylen);
    }
    else
        *pp = (UINT8)new_paylen;

    /* Point to the end of the previous payload */
    pp = p_prev_pl + prev_paylen;

    /* If we are not the last record, make space for the extra payload */
    if ((*p_rec & NDEF_ME_MASK) == 0)
        shiftdown (pp, (UINT32)(*p_cur_size - (pp - p_msg)), add_pl_len);

    /* Now copy in the additional payload data */
    memcpy (pp, p_add_pl, add_pl_len);

    *p_cur_size += add_pl_len + incr_lenfld;

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgReplacePayload
**
** Description      This function replaces the payload of a specific record in the
**                  given NDEF message
**
** Returns          OK, or error if the new payload did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgReplacePayload (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                     UINT8 *p_rec, UINT8 *p_new_pl, UINT32 new_pl_len)
{
    UINT32      prev_paylen;
    UINT8       *p_prev_pl, *pp;
    UINT32      paylen_delta;
    UINT8       type_len, id_len;

    /* Skip header */
    pp = p_rec + 1;

    /* Next byte is the type field length */
    type_len = *pp++;

    /* Next is the payload length (1 or 4 bytes) */
    if (*p_rec & NDEF_SR_MASK)
        prev_paylen = *pp++;
    else
        BE_STREAM_TO_UINT32 (prev_paylen, pp);

    /* ID field Length */
    if (*p_rec & NDEF_IL_MASK)
        id_len = *pp++;
    else
        id_len = 0;

    p_prev_pl = pp + type_len + id_len;

    /* Point to payload length field again */
    pp = p_rec + 2;

    if (new_pl_len > prev_paylen)
    {
        /* New payload is larger than the previous */
        paylen_delta = new_pl_len - prev_paylen;

        /* If the previous payload length was < 256, and new is > 255 */
        /* the payload length field goes from 1 byte to 4 bytes       */
        if ( (prev_paylen < 256) && (new_pl_len > 255) )
        {
            if ((*p_cur_size + paylen_delta + 3) > max_size)
                return (NDEF_MSG_INSUFFICIENT_MEM);

            shiftdown (pp + 1, (UINT32)(*p_cur_size - (pp - p_msg) - 1), 3);
            p_prev_pl   += 3;
            *p_cur_size += 3;
            *p_rec      &= ~NDEF_SR_MASK;
        }
        else if ((*p_cur_size + paylen_delta) > max_size)
            return (NDEF_MSG_INSUFFICIENT_MEM);

        /* Store in the new length */
        if (new_pl_len > 255)
        {
            UINT32_TO_BE_STREAM (pp, new_pl_len);
        }
        else
            *pp = (UINT8)new_pl_len;

        /* Point to the end of the previous payload */
        pp = p_prev_pl + prev_paylen;

        /* If we are not the last record, make space for the extra payload */
        if ((*p_rec & NDEF_ME_MASK) == 0)
            shiftdown (pp, (UINT32)(*p_cur_size - (pp - p_msg)), paylen_delta);

        *p_cur_size += paylen_delta;
    }
    else if (new_pl_len < prev_paylen)
    {
        /* New payload is smaller than the previous */
        paylen_delta = prev_paylen - new_pl_len;

        /* If the previous payload was > 256, and new is less than 256 */
        /* the payload length field goes from 4 bytes to 1 byte        */
        if ( (prev_paylen > 255) && (new_pl_len < 256) )
        {
            shiftup (pp + 1, pp + 4, (UINT32)(*p_cur_size - (pp - p_msg) - 3));
            p_prev_pl   -= 3;
            *p_cur_size -= 3;
            *p_rec      |= NDEF_SR_MASK;
        }

        /* Store in the new length */
        if (new_pl_len > 255)
        {
            UINT32_TO_BE_STREAM (pp, new_pl_len);
        }
        else
            *pp = (UINT8)new_pl_len;

        /* Point to the end of the previous payload */
        pp = p_prev_pl + prev_paylen;

        /* If we are not the last record, remove the extra space from the previous payload */
        if ((*p_rec & NDEF_ME_MASK) == 0)
            shiftup (pp - paylen_delta, pp, (UINT32)(*p_cur_size - (pp - p_msg)));

        *p_cur_size -= paylen_delta;
    }

    /* Now copy in the new payload data */
    if (p_new_pl)
        memcpy (p_prev_pl, p_new_pl, new_pl_len);

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgReplaceType
**
** Description      This function replaces the type field of a specific record in the
**                  given NDEF message
**
** Returns          OK, or error if the new type field did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgReplaceType (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                  UINT8 *p_rec, UINT8 *p_new_type, UINT8 new_type_len)
{
    UINT8       typelen_delta;
    UINT8       *p_prev_type, prev_type_len;
    UINT8       *pp;

    /* Skip header */
    pp = p_rec + 1;

    /* Next byte is the type field length */
    prev_type_len = *pp++;

    /* Skip the payload length */
    if (*p_rec & NDEF_SR_MASK)
        pp += 1;
    else
        pp += 4;

    if (*p_rec & NDEF_IL_MASK)
        pp++;

    /* Save pointer to the start of the type field */
    p_prev_type = pp;

    if (new_type_len > prev_type_len)
    {
        /* New type is larger than the previous */
        typelen_delta = new_type_len - prev_type_len;

        if ((*p_cur_size + typelen_delta) > max_size)
            return (NDEF_MSG_INSUFFICIENT_MEM);

        /* Point to the end of the previous type, and make space for the extra data */
        pp = p_prev_type + prev_type_len;
        shiftdown (pp, (UINT32)(*p_cur_size - (pp - p_msg)), typelen_delta);

        *p_cur_size += typelen_delta;
    }
    else if (new_type_len < prev_type_len)
    {
        /* New type field is smaller than the previous */
        typelen_delta = prev_type_len - new_type_len;

        /* Point to the end of the previous type, and shift up to fill the the unused space */
        pp = p_prev_type + prev_type_len;
        shiftup (pp - typelen_delta, pp, (UINT32)(*p_cur_size - (pp - p_msg)));

        *p_cur_size -= typelen_delta;
    }

    /* Save in new type length */
    p_rec[1] = new_type_len;

    /* Now copy in the new type field data */
    if (p_new_type)
        memcpy (p_prev_type, p_new_type, new_type_len);

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgReplaceId
**
** Description      This function replaces the ID field of a specific record in the
**                  given NDEF message
**
** Returns          OK, or error if the new ID field did not fit
**                  *p_cur_size is updated
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgReplaceId (UINT8 *p_msg, UINT32 max_size, UINT32 *p_cur_size,
                                UINT8 *p_rec, UINT8 *p_new_id, UINT8 new_id_len)
{
    UINT8       idlen_delta;
    UINT8       *p_prev_id, *p_idlen_field;
    UINT8       prev_id_len, type_len;
    UINT8       *pp;

    /* Skip header */
    pp = p_rec + 1;

    /* Next byte is the type field length */
    type_len = *pp++;

    /* Skip the payload length */
    if (*p_rec & NDEF_SR_MASK)
        pp += 1;
    else
        pp += 4;

    p_idlen_field = pp;

    if (*p_rec & NDEF_IL_MASK)
        prev_id_len = *pp++;
    else
        prev_id_len = 0;

    /* Save pointer to the start of the ID field (right after the type field) */
    p_prev_id = pp + type_len;

    if (new_id_len > prev_id_len)
    {
        /* New ID field is larger than the previous */
        idlen_delta = new_id_len - prev_id_len;

        /* If the previous ID length was 0, we need to add a 1-byte ID length */
        if (prev_id_len == 0)
        {
            if ((*p_cur_size + idlen_delta + 1) > max_size)
                return (NDEF_MSG_INSUFFICIENT_MEM);

            shiftdown (p_idlen_field, (UINT32)(*p_cur_size - (p_idlen_field - p_msg)), 1);
            p_prev_id   += 1;
            *p_cur_size += 1;
            *p_rec      |= NDEF_IL_MASK;
        }
        else if ((*p_cur_size + idlen_delta) > max_size)
            return (NDEF_MSG_INSUFFICIENT_MEM);

        /* Point to the end of the previous ID field, and make space for the extra data */
        pp = p_prev_id + prev_id_len;
        shiftdown (pp, (UINT32)(*p_cur_size - (pp - p_msg)), idlen_delta);

        *p_cur_size += idlen_delta;
    }
    else if (new_id_len < prev_id_len)
    {
        /* New ID field is smaller than the previous */
        idlen_delta = prev_id_len - new_id_len;

        /* Point to the end of the previous ID, and shift up to fill the the unused space */
        pp = p_prev_id + prev_id_len;
        shiftup (pp - idlen_delta, pp, (UINT32)(*p_cur_size - (pp - p_msg)));

        *p_cur_size -= idlen_delta;

        /* If removing the ID, make sure that length field is also removed */
        if (new_id_len == 0)
        {
            shiftup (p_idlen_field, p_idlen_field + 1, (UINT32)(*p_cur_size - (p_idlen_field - p_msg - (UINT32)1)));
            *p_rec      &= ~NDEF_IL_MASK;
            *p_cur_size -= 1;
        }
    }

    /* Save in new ID length and data */
    if (new_id_len)
    {
        *p_idlen_field = new_id_len;

        if (p_new_id)
            memcpy (p_prev_id, p_new_id, new_id_len);
    }

    return (NDEF_OK);
}

/*******************************************************************************
**
** Function         NDEF_MsgRemoveRec
**
** Description      This function removes the record at the given
**                  index in the given NDEF message.
**
** Returns          TRUE if OK, FALSE if the index was invalid
**                  *p_cur_size is updated
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgRemoveRec (UINT8 *p_msg, UINT32 *p_cur_size, INT32 index)
{
    UINT8   *p_rec = NDEF_MsgGetRecByIndex (p_msg, index);
    UINT8   *pNext, *pPrev;

    if (!p_rec)
        return (NDEF_REC_NOT_FOUND);

    /* If this is the first record in the message... */
    if (*p_rec & NDEF_MB_MASK)
    {
        /* Find the second record (if any) and set his 'Message Begin' bit */
        if ((pNext = NDEF_MsgGetRecByIndex(p_msg, 1)) != NULL)
        {
            *pNext |= NDEF_MB_MASK;

            *p_cur_size -= (UINT32)(pNext - p_msg);

            shiftup (p_msg, pNext, *p_cur_size);
        }
        else
            *p_cur_size = 0;              /* No more records, lenght must be zero */

        return (NDEF_OK);
    }

    /* If this is the last record in the message... */
    if (*p_rec & NDEF_ME_MASK)
    {
        if (index > 0)
        {
            /* Find the previous record and set his 'Message End' bit */
            if ((pPrev = NDEF_MsgGetRecByIndex(p_msg, index - 1)) == NULL)
                return (FALSE);

            *pPrev |= NDEF_ME_MASK;
        }
        *p_cur_size = (UINT32)(p_rec - p_msg);

        return (NDEF_OK);
    }

    /* Not the first or the last... get the address of the next record */
    if ((pNext = NDEF_MsgGetNextRec (p_rec)) == NULL)
        return (FALSE);

    /* We are removing p_rec, so shift from pNext to the end */
    shiftup (p_rec, pNext, (UINT32)(*p_cur_size - (pNext - p_msg)));

    *p_cur_size -= (UINT32)(pNext - p_rec);

    return (NDEF_OK);
}


/*******************************************************************************
**
** Function         NDEF_MsgCopyAndDechunk
**
** Description      This function copies and de-chunks an NDEF message.
**                  It is assumed that the destination is at least as large
**                  as the source, since the source may not actually contain
**                  any chunks.
**
** Returns          The output byte count
**
*******************************************************************************/
tNDEF_STATUS NDEF_MsgCopyAndDechunk (UINT8 *p_src, UINT32 src_len, UINT8 *p_dest, UINT32 *p_out_len)
{
    UINT32          out_len, max_out_len;
    UINT8           *p_rec;
    UINT8           *p_prev_rec = p_dest;
    UINT8           *p_type, *p_id, *p_pay;
    UINT8           type_len, id_len, tnf;
    UINT32          pay_len;
    tNDEF_STATUS    status;

    /* First, validate the source */
    if ((status = NDEF_MsgValidate(p_src, src_len, TRUE)) != NDEF_OK)
        return (status);

    /* The output buffer must be at least as large as the input buffer */
    max_out_len = src_len;

    /* Initialize output */
    NDEF_MsgInit (p_dest, max_out_len, &out_len);

    p_rec = p_src;

    /* Now, copy record by record */
    while ((p_rec != NULL) && (status == NDEF_OK))
    {
        p_type = NDEF_RecGetType (p_rec, &tnf, &type_len);
        p_id   = NDEF_RecGetId (p_rec, &id_len);
        p_pay  = NDEF_RecGetPayload (p_rec, &pay_len);

        /* If this is the continuation of a chunk, append the payload to the previous */
        if (tnf == NDEF_TNF_UNCHANGED)
        {
            if (p_pay)
            {
                status = NDEF_MsgAppendPayload (p_dest, max_out_len, &out_len, p_prev_rec, p_pay, pay_len);
            }
        }
        else
        {
            p_prev_rec = p_dest + out_len;

            status = NDEF_MsgAddRec (p_dest, max_out_len, &out_len, tnf, p_type, type_len,
                            p_id, id_len, p_pay, pay_len);
        }

        p_rec = NDEF_MsgGetNextRec (p_rec);
    }

    *p_out_len = out_len;

    return (status);
}
