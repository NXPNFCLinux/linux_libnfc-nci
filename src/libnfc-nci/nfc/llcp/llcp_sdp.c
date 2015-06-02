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
 *  This file contains the LLCP Service Discovery
 *
 ******************************************************************************/

#include <string.h>
#include "gki.h"
#include "nfc_target.h"
#include "bt_types.h"
#include "llcp_api.h"
#include "llcp_int.h"
#include "llcp_defs.h"

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#include "nfa_sys.h"
#include "nfa_dm_int.h"
#endif

/*******************************************************************************
**
** Function         llcp_sdp_proc_data
**
** Description      Do nothing
**
**
** Returns          void
**
*******************************************************************************/
void llcp_sdp_proc_data (tLLCP_SAP_CBACK_DATA *p_data)
{
    /*
    ** Do nothing
    ** llcp_sdp_proc_SNL () is called by link layer
    */
}

/*******************************************************************************
**
** Function         llcp_sdp_check_send_snl
**
** Description      Enqueue Service Name Lookup PDU into sig_xmit_q for transmitting
**
**
** Returns          void
**
*******************************************************************************/
void llcp_sdp_check_send_snl (void)
{
    UINT8 *p;

    if (llcp_cb.sdp_cb.p_snl)
    {
        LLCP_TRACE_DEBUG0 ("SDP: llcp_sdp_check_send_snl ()");

        llcp_cb.sdp_cb.p_snl->len     += LLCP_PDU_HEADER_SIZE;
        llcp_cb.sdp_cb.p_snl->offset  -= LLCP_PDU_HEADER_SIZE;

        p = (UINT8 *) (llcp_cb.sdp_cb.p_snl + 1) + llcp_cb.sdp_cb.p_snl->offset;
        UINT16_TO_BE_STREAM (p, LLCP_GET_PDU_HEADER (LLCP_SAP_SDP, LLCP_PDU_SNL_TYPE, LLCP_SAP_SDP ));

        GKI_enqueue (&llcp_cb.lcb.sig_xmit_q, llcp_cb.sdp_cb.p_snl);
        llcp_cb.sdp_cb.p_snl = NULL;
    }
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    else
    {
        /* Notify DTA after sending out SNL with SDRES not to send SNLs in AGF PDU */
        if ((llcp_cb.p_dta_cback) && (llcp_cb.dta_snl_resp))
        {
            llcp_cb.dta_snl_resp = FALSE;
            (*llcp_cb.p_dta_cback) ();
        }
    }
#endif
}

/*******************************************************************************
**
** Function         llcp_sdp_add_sdreq
**
** Description      Add Service Discovery Request into SNL PDU
**
**
** Returns          void
**
*******************************************************************************/
static void llcp_sdp_add_sdreq (UINT8 tid, char *p_name)
{
    UINT8  *p;
    UINT16 name_len = (UINT16) strlen (p_name);

    p = (UINT8 *) (llcp_cb.sdp_cb.p_snl + 1) + llcp_cb.sdp_cb.p_snl->offset + llcp_cb.sdp_cb.p_snl->len;

    UINT8_TO_BE_STREAM (p, LLCP_SDREQ_TYPE);
    UINT8_TO_BE_STREAM (p, (1 + name_len));
    UINT8_TO_BE_STREAM (p, tid);
    ARRAY_TO_BE_STREAM (p, p_name, name_len);

    llcp_cb.sdp_cb.p_snl->len += LLCP_SDREQ_MIN_LEN + name_len;
}

/*******************************************************************************
**
** Function         llcp_sdp_send_sdreq
**
** Description      Send Service Discovery Request
**
**
** Returns          LLCP_STATUS
**
*******************************************************************************/
tLLCP_STATUS llcp_sdp_send_sdreq (UINT8 tid, char *p_name)
{
    tLLCP_STATUS status;
    UINT16       name_len;
    UINT16       available_bytes;

    LLCP_TRACE_DEBUG2 ("llcp_sdp_send_sdreq (): tid=0x%x, ServiceName=%s", tid, p_name);

    /* if there is no pending SNL */
    if (!llcp_cb.sdp_cb.p_snl)
    {
        llcp_cb.sdp_cb.p_snl = (BT_HDR*) GKI_getpoolbuf (LLCP_POOL_ID);

        if (llcp_cb.sdp_cb.p_snl)
        {
            llcp_cb.sdp_cb.p_snl->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + LLCP_PDU_HEADER_SIZE;
            llcp_cb.sdp_cb.p_snl->len    = 0;
        }
    }

    if (llcp_cb.sdp_cb.p_snl)
    {
        available_bytes = GKI_get_buf_size (llcp_cb.sdp_cb.p_snl)
                          - BT_HDR_SIZE - llcp_cb.sdp_cb.p_snl->offset
                          - llcp_cb.sdp_cb.p_snl->len;

        name_len = (UINT16) strlen (p_name);

        /* if SDREQ parameter can be added in SNL */
        if (  (available_bytes >= LLCP_SDREQ_MIN_LEN + name_len)
            &&(llcp_cb.sdp_cb.p_snl->len + LLCP_SDREQ_MIN_LEN + name_len <= llcp_cb.lcb.effective_miu)  )
        {
            llcp_sdp_add_sdreq (tid, p_name);
            status = LLCP_STATUS_SUCCESS;
        }
        else
        {
            /* send pending SNL PDU to LM */
            llcp_sdp_check_send_snl ();

            llcp_cb.sdp_cb.p_snl = (BT_HDR*) GKI_getpoolbuf (LLCP_POOL_ID);

            if (llcp_cb.sdp_cb.p_snl)
            {
                llcp_cb.sdp_cb.p_snl->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + LLCP_PDU_HEADER_SIZE;
                llcp_cb.sdp_cb.p_snl->len    = 0;

                llcp_sdp_add_sdreq (tid, p_name);

                status = LLCP_STATUS_SUCCESS;
            }
            else
            {
                status = LLCP_STATUS_FAIL;
            }
        }
    }
    else
    {
        status = LLCP_STATUS_FAIL;
    }
    /* if LM is waiting for PDUs from upper layer */
    if (  (status == LLCP_STATUS_SUCCESS)
        &&(llcp_cb.lcb.symm_state == LLCP_LINK_SYMM_LOCAL_XMIT_NEXT)  )
    {
        llcp_link_check_send_data ();
    }
    return status;
}

/*******************************************************************************
**
** Function         llcp_sdp_add_sdres
**
** Description      Add Service Discovery Response into SNL PDU
**
**
** Returns          void
**
*******************************************************************************/
static void llcp_sdp_add_sdres (UINT8 tid, UINT8 sap)
{
    UINT8  *p;

    p = (UINT8 *) (llcp_cb.sdp_cb.p_snl + 1) + llcp_cb.sdp_cb.p_snl->offset + llcp_cb.sdp_cb.p_snl->len;

    UINT8_TO_BE_STREAM (p, LLCP_SDRES_TYPE);
    UINT8_TO_BE_STREAM (p, LLCP_SDRES_LEN);
    UINT8_TO_BE_STREAM (p, tid);
    UINT8_TO_BE_STREAM (p, sap);

    llcp_cb.sdp_cb.p_snl->len += 2 + LLCP_SDRES_LEN;   /* type and length */
}

/*******************************************************************************
**
** Function         llcp_sdp_send_sdres
**
** Description      Send Service Discovery Response
**
**
** Returns          LLCP_STATUS
**
*******************************************************************************/
static tLLCP_STATUS llcp_sdp_send_sdres (UINT8 tid, UINT8 sap)
{
    tLLCP_STATUS status;
    UINT16       available_bytes;

    LLCP_TRACE_DEBUG2 ("llcp_sdp_send_sdres (): tid=0x%x, SAP=0x%x", tid, sap);

    /* if there is no pending SNL */
    if (!llcp_cb.sdp_cb.p_snl)
    {
        llcp_cb.sdp_cb.p_snl = (BT_HDR*) GKI_getpoolbuf (LLCP_POOL_ID);

        if (llcp_cb.sdp_cb.p_snl)
        {
            llcp_cb.sdp_cb.p_snl->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + LLCP_PDU_HEADER_SIZE;
            llcp_cb.sdp_cb.p_snl->len    = 0;
        }
    }

    if (llcp_cb.sdp_cb.p_snl)
    {
        available_bytes = GKI_get_buf_size (llcp_cb.sdp_cb.p_snl)
                          - BT_HDR_SIZE - llcp_cb.sdp_cb.p_snl->offset
                          - llcp_cb.sdp_cb.p_snl->len;

        /* if SDRES parameter can be added in SNL */
        if (  (available_bytes >= 2 + LLCP_SDRES_LEN)
            &&(llcp_cb.sdp_cb.p_snl->len + 2 + LLCP_SDRES_LEN <= llcp_cb.lcb.effective_miu)  )
        {
            llcp_sdp_add_sdres (tid, sap);
            status = LLCP_STATUS_SUCCESS;
        }
        else
        {
            /* send pending SNL PDU to LM */
            llcp_sdp_check_send_snl ();

            llcp_cb.sdp_cb.p_snl = (BT_HDR*) GKI_getpoolbuf (LLCP_POOL_ID);

            if (llcp_cb.sdp_cb.p_snl)
            {
                llcp_cb.sdp_cb.p_snl->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + LLCP_PDU_HEADER_SIZE;
                llcp_cb.sdp_cb.p_snl->len    = 0;

                llcp_sdp_add_sdres (tid, sap);

                status = LLCP_STATUS_SUCCESS;
            }
            else
            {
                status = LLCP_STATUS_FAIL;
            }
        }
    }
    else
    {
        status = LLCP_STATUS_FAIL;
    }
    /* if LM is waiting for PDUs from upper layer */
    if (  (status == LLCP_STATUS_SUCCESS)
        &&(llcp_cb.lcb.symm_state == LLCP_LINK_SYMM_LOCAL_XMIT_NEXT)  )
    {
        llcp_link_check_send_data ();
    }
    return status;
}

/*******************************************************************************
**
** Function         llcp_sdp_get_sap_by_name
**
** Description      Search SAP by service name
**
**
** Returns          SAP if success
**
*******************************************************************************/
UINT8 llcp_sdp_get_sap_by_name (char *p_name, UINT8 length)
{
    UINT8        sap;
    tLLCP_APP_CB *p_app_cb;

    for (sap = LLCP_SAP_SDP; sap <= LLCP_UPPER_BOUND_SDP_SAP; sap++)
    {
        p_app_cb = llcp_util_get_app_cb (sap);

        if (  (p_app_cb)
            &&(p_app_cb->p_app_cback)
            &&(strlen((char*)p_app_cb->p_service_name) == length)
            &&(!strncmp((char*)p_app_cb->p_service_name, p_name, length))  )
        {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            /* if device is under LLCP DTA testing */
            if (  (llcp_cb.p_dta_cback)
                &&(!strncmp((char*)p_app_cb->p_service_name, "urn:nfc:sn:cl-echo-in", length))  )
            {
                llcp_cb.dta_snl_resp = TRUE;
            }
#endif
            return (sap);
        }
    }
    return 0;
}

/*******************************************************************************
**
** Function         llcp_sdp_return_sap
**
** Description      Report TID and SAP to requester
**
**
** Returns          void
**
*******************************************************************************/
static void llcp_sdp_return_sap (UINT8 tid, UINT8 sap)
{
    UINT8 i;

    LLCP_TRACE_DEBUG2 ("llcp_sdp_return_sap (): tid=0x%x, SAP=0x%x", tid, sap);

    for (i = 0; i < LLCP_MAX_SDP_TRANSAC; i++)
    {
        if (  (llcp_cb.sdp_cb.transac[i].p_cback)
            &&(llcp_cb.sdp_cb.transac[i].tid == tid)  )
        {
            (*llcp_cb.sdp_cb.transac[i].p_cback) (tid, sap);

            llcp_cb.sdp_cb.transac[i].p_cback = NULL;
        }
    }
}

/*******************************************************************************
**
** Function         llcp_sdp_proc_deactivation
**
** Description      Report SDP failure for any pending request because of deactivation
**
**
** Returns          void
**
*******************************************************************************/
void llcp_sdp_proc_deactivation (void)
{
    UINT8 i;

    LLCP_TRACE_DEBUG0 ("llcp_sdp_proc_deactivation ()");

    for (i = 0; i < LLCP_MAX_SDP_TRANSAC; i++)
    {
        if (llcp_cb.sdp_cb.transac[i].p_cback)
        {
            (*llcp_cb.sdp_cb.transac[i].p_cback) (llcp_cb.sdp_cb.transac[i].tid, 0x00);

            llcp_cb.sdp_cb.transac[i].p_cback = NULL;
        }
    }

    /* free any pending SNL PDU */
    if (llcp_cb.sdp_cb.p_snl)
    {
        GKI_freebuf (llcp_cb.sdp_cb.p_snl);
        llcp_cb.sdp_cb.p_snl = NULL;
    }

    llcp_cb.sdp_cb.next_tid = 0;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    llcp_cb.dta_snl_resp = FALSE;
#endif
}

/*******************************************************************************
**
** Function         llcp_sdp_proc_snl
**
** Description      Process SDREQ and SDRES in SNL
**
**
** Returns          LLCP_STATUS
**
*******************************************************************************/
tLLCP_STATUS llcp_sdp_proc_snl (UINT16 sdu_length, UINT8 *p)
{
    UINT8  type, length, tid, sap, *p_value;

    LLCP_TRACE_DEBUG0 ("llcp_sdp_proc_snl ()");

    if ((llcp_cb.lcb.agreed_major_version < LLCP_MIN_SNL_MAJOR_VERSION)||
       ((llcp_cb.lcb.agreed_major_version == LLCP_MIN_SNL_MAJOR_VERSION)&&(llcp_cb.lcb.agreed_minor_version < LLCP_MIN_SNL_MINOR_VERSION)))
    {
        LLCP_TRACE_DEBUG0 ("llcp_sdp_proc_snl(): version number less than 1.1, SNL not supported.");
        return LLCP_STATUS_FAIL;
    }
    while (sdu_length >= 2) /* at least type and length */
    {
        BE_STREAM_TO_UINT8 (type, p);
        BE_STREAM_TO_UINT8 (length, p);

        switch (type)
        {
        case LLCP_SDREQ_TYPE:
            if (  (length > 1)                /* TID and sevice name */
                &&(sdu_length >= 2 + length)  ) /* type, length, TID and service name */
            {
                p_value = p;
                BE_STREAM_TO_UINT8 (tid, p_value);
                sap = llcp_sdp_get_sap_by_name ((char*) p_value, (UINT8) (length - 1));
                llcp_sdp_send_sdres (tid, sap);
            }
            else
            {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                /*For P2P in LLCP mode TC_CTO_TAR_BI_03_x(x=3) fix*/
                if ((appl_dta_mode_flag == 1) && (nfa_dm_cb.eDtaMode == NFA_DTA_LLCP_MODE))
                {
                    LLCP_TRACE_ERROR0 ("DEBUG> llcp_sdp_proc_snl () Calling llcp_sdp_send_sdres");
                    tid = 0x01;
                    sap = 0x00;
                    llcp_sdp_send_sdres (tid, sap);
                }
#endif
                LLCP_TRACE_ERROR1 ("llcp_sdp_proc_snl (): bad length (%d) in LLCP_SDREQ_TYPE", length);
            }
            break;

        case LLCP_SDRES_TYPE:
            if (  (length == LLCP_SDRES_LEN)  /* TID and SAP */
                &&(sdu_length >= 2 + length)  ) /* type, length, TID and SAP */
            {
                p_value = p;
                BE_STREAM_TO_UINT8 (tid, p_value);
                BE_STREAM_TO_UINT8 (sap, p_value);
                llcp_sdp_return_sap (tid, sap);
            }
            else
            {
                LLCP_TRACE_ERROR1 ("llcp_sdp_proc_snl (): bad length (%d) in LLCP_SDRES_TYPE", length);
            }
            break;

        default:
            LLCP_TRACE_WARNING1 ("llcp_sdp_proc_snl (): Unknown type (0x%x) is ignored", type);
            break;
        }

        if (sdu_length >= 2 + length)   /* type, length, value */
        {
            sdu_length -= 2 + length;
            p += length;
        }
        else
        {
            break;
        }
    }

    if (sdu_length)
    {
        LLCP_TRACE_ERROR0 ("llcp_sdp_proc_snl (): Bad format of SNL");
        return LLCP_STATUS_FAIL;
    }
    else
    {
        return LLCP_STATUS_SUCCESS;
    }
}
