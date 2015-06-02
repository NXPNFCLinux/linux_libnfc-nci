/******************************************************************************
 *
 *  Copyright (C) 2010-2012 Broadcom Corporation
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
 *  Copyright (C) 2015 NXP Semiconductors
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
 *  NFA interface for connection handover
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_api.h"
#include "nfa_sys.h"
#include "nfa_sys_int.h"
#include "nfa_p2p_api.h"
#include "nfa_cho_api.h"
#include "nfa_cho_int.h"
#include "nfa_mem_co.h"

/*****************************************************************************
**  Constants
*****************************************************************************/

/*******************************************************************************
**
** Function         NFA_ChoRegister
**
** Description      This function is called to register callback function to receive
**                  connection handover events.
**
**                  On this registration, "urn:nfc:sn:handover" server will be
**                  registered on LLCP if enable_server is TRUE.
**
**                  The result of the registration is reported with NFA_CHO_REG_EVT.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoRegister (BOOLEAN        enable_server,
                             tNFA_CHO_CBACK *p_cback)
{
    tNFA_CHO_API_REG *p_msg;

    CHO_TRACE_API1 ("NFA_ChoRegister (): enable_server=%d", enable_server);

    if (  (nfa_cho_cb.state != NFA_CHO_ST_DISABLED)
        ||(nfa_cho_cb.p_cback != NULL)  )
    {
        CHO_TRACE_ERROR0 ("NFA_ChoRegister (): Already registered or callback is not provided");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_CHO_API_REG *) GKI_getbuf (sizeof (tNFA_CHO_API_REG))) != NULL)
    {
        p_msg->hdr.event = NFA_CHO_API_REG_EVT;

        p_msg->enable_server = enable_server;
        p_msg->p_cback       = p_cback;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ChoDeregister
**
** Description      This function is called to deregister callback function from NFA
**                  Connection Handover Application.
**
**                  If this is the valid deregistration, NFA Connection Handover
**                  Application will close the service with "urn:nfc:sn:handover"
**                  on LLCP and deregister NDEF type handler if any.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoDeregister (void)
{
    tNFA_CHO_API_DEREG *p_msg;

    CHO_TRACE_API0 ("NFA_ChoDeregister ()");

    if (nfa_cho_cb.state == NFA_CHO_ST_DISABLED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoDeregister (): Not registered");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_CHO_API_DEREG *) GKI_getbuf (sizeof (tNFA_CHO_API_DEREG))) != NULL)
    {
        p_msg->event = NFA_CHO_API_DEREG_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ChoConnect
**
** Description      This function is called to create data link connection to
**                  Connection Handover server on peer device.
**
**                  It must be called after receiving NFA_CHO_ACTIVATED_EVT.
**                  NFA_CHO_CONNECTED_EVT will be returned if successful.
**                  Otherwise, NFA_CHO_DISCONNECTED_EVT will be returned.
**
**                  If NFA_CHO_ROLE_REQUESTER is returned in NFA_CHO_CONNECTED_EVT,
**                  Handover Request Message can be sent.
**                  If NFA_CHO_ROLE_SELECTOR is returned in NFA_CHO_CONNECTED_EVT
**                  because of collision, application must wait for Handover
**                  Request Message.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoConnect (void)
{
    tNFA_CHO_API_CONNECT *p_msg;

    CHO_TRACE_API0 ("NFA_ChoConnect ()");

    if (nfa_cho_cb.state == NFA_CHO_ST_DISABLED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoConnect (): Not registered");
        return (NFA_STATUS_FAILED);
    }
    else if (nfa_cho_cb.state == NFA_CHO_ST_CONNECTED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoConnect (): Already connected");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_CHO_API_CONNECT *) GKI_getbuf (sizeof (tNFA_CHO_API_CONNECT))) != NULL)
    {
        p_msg->event = NFA_CHO_API_CONNECT_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ChoDisconnect
**
** Description      This function is called to disconnect data link connection with
**                  Connection Handover server on peer device.
**
**                  NFA_CHO_DISCONNECTED_EVT will be returned.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoDisconnect (void)
{
    tNFA_CHO_API_DISCONNECT *p_msg;

    CHO_TRACE_API0 ("NFA_ChoDisconnect ()");

    if (nfa_cho_cb.state == NFA_CHO_ST_DISABLED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoDisconnect (): Not registered");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_CHO_API_DISCONNECT *) GKI_getbuf (sizeof (tNFA_CHO_API_DISCONNECT))) != NULL)
    {
        p_msg->event = NFA_CHO_API_DISCONNECT_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ChoSendHr
**
** Description      This function is called to send Handover Request Message with
**                  Handover Carrier records or Alternative Carrier records.
**
**                  It must be called after receiving NFA_CHO_CONNECTED_EVT.
**
**                  NDEF may include one or more Handover Carrier records or Alternative
**                  Carrier records with auxiliary data.
**                  The records in NDEF must be matched with tNFA_CHO_AC_INFO in order.
**                  Payload ID must be unique and Payload ID length must be less than
**                  or equal to NFA_CHO_MAX_REF_NAME_LEN.
**
**                  The alternative carrier information of Handover Select record
**                  will be sent to application by NFA_CHO_SELECT_EVT. Application
**                  may receive NFA_CHO_REQUEST_EVT because of handover collision.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoSendHr (UINT8             num_ac_info,
                           tNFA_CHO_AC_INFO *p_ac_info,
                           UINT8            *p_ndef,
                           UINT32            ndef_len)
{
    tNFA_CHO_API_SEND_HR *p_msg;
    UINT16               msg_size;
    UINT8                *p_ndef_buf;

    CHO_TRACE_API2 ("NFA_ChoSendHr (): num_ac_info=%d, ndef_len=%d", num_ac_info, ndef_len);

    if (nfa_cho_cb.state != NFA_CHO_ST_CONNECTED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoSendHr (): Not connected");
        return (NFA_STATUS_FAILED);
    }

    if (num_ac_info > NFA_CHO_MAX_AC_INFO)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoSendHr (): Too many AC information");
        return (NFA_STATUS_FAILED);
    }

    p_ndef_buf = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);

    if (!p_ndef_buf)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoSendHr (): Failed to allocate buffer for NDEF");
        return NFA_STATUS_FAILED;
    }
    else if (ndef_len > LLCP_POOL_BUF_SIZE)
    {
        CHO_TRACE_ERROR1 ("NFA_ChoSendHr (): Failed to allocate buffer for %d bytes", ndef_len);
        GKI_freebuf (p_ndef_buf);
        return NFA_STATUS_FAILED;
    }

    msg_size = sizeof (tNFA_CHO_API_SEND_HR) + num_ac_info * sizeof (tNFA_CHO_AC_INFO);

    if ((p_msg = (tNFA_CHO_API_SEND_HR *) GKI_getbuf (msg_size)) != NULL)
    {
        p_msg->hdr.event = NFA_CHO_API_SEND_HR_EVT;

        memcpy (p_ndef_buf, p_ndef, ndef_len);
        p_msg->p_ndef        = p_ndef_buf;
        p_msg->max_ndef_size = LLCP_POOL_BUF_SIZE;
        p_msg->cur_ndef_size = ndef_len;

        p_msg->num_ac_info   = num_ac_info;
        p_msg->p_ac_info     = (tNFA_CHO_AC_INFO *) (p_msg + 1);
        memcpy (p_msg->p_ac_info, p_ac_info, num_ac_info * sizeof (tNFA_CHO_AC_INFO));

        nfa_sys_sendmsg (p_msg);
        return (NFA_STATUS_OK);
    }
    else
    {
        GKI_freebuf (p_ndef_buf);
        return (NFA_STATUS_FAILED);
    }
}

/*******************************************************************************
**
** Function         NFA_ChoSendHs
**
** Description      This function is called to send Handover Select message with
**                  Alternative Carrier records as response to Handover Request
**                  message.
**
**                  NDEF may include one or more Alternative Carrier records with
**                  auxiliary data.
**                  The records in NDEF must be matched with tNFA_CHO_AC_INFO in order.
**                  Payload ID must be unique and Payload ID length must be less than
**                  or equal to NFA_CHO_MAX_REF_NAME_LEN.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoSendHs (
                           UINT8            *p_ndef,
                           UINT32            ndef_len)
{
    tNFA_CHO_API_SEND_HS *p_msg;
    UINT16               msg_size;
    UINT8                *p_ndef_buf;

    CHO_TRACE_API1 ("NFA_ChoSendHs(): ndef_len=%d", ndef_len);

    if (nfa_cho_cb.state != NFA_CHO_ST_CONNECTED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoSendHs (): Not connected");
        return (NFA_STATUS_FAILED);
    }

    p_ndef_buf = (UINT8 *) GKI_getpoolbuf (LLCP_POOL_ID);
    if (!p_ndef_buf)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoSendHs (): Failed to allocate buffer for NDEF");
        return NFA_STATUS_FAILED;
    }
    else if (ndef_len > LLCP_POOL_BUF_SIZE)
    {
        CHO_TRACE_ERROR1 ("NFA_ChoSendHs (): Failed to allocate buffer for %d bytes", ndef_len);
        GKI_freebuf (p_ndef_buf);
        return NFA_STATUS_FAILED;
    }

    msg_size = sizeof (tNFA_CHO_API_SEND_HS);

    if ((p_msg = (tNFA_CHO_API_SEND_HS *) GKI_getbuf (msg_size)) != NULL)
    {
        p_msg->hdr.event = NFA_CHO_API_SEND_HS_EVT;

        memcpy (p_ndef_buf, p_ndef, ndef_len);
        p_msg->p_ndef        = p_ndef_buf;
        p_msg->max_ndef_size = LLCP_POOL_BUF_SIZE;
        p_msg->cur_ndef_size = ndef_len;

        p_msg->num_ac_info   = 0;
        p_msg->p_ac_info     = NULL;

        nfa_sys_sendmsg (p_msg);
        return (NFA_STATUS_OK);
    }
    else
    {
        GKI_freebuf (p_ndef_buf);
        return (NFA_STATUS_FAILED);
    }
}

/*******************************************************************************
**
** Function         NFA_ChoSendSelectError
**
** Description      This function is called to send Error record to indicate failure
**                  to process the most recently received Handover Request message.
**
**                  error_reason : NFA_CHO_ERROR_TEMP_MEM
**                                 NFA_CHO_ERROR_PERM_MEM
**                                 NFA_CHO_ERROR_CARRIER
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ChoSendSelectError (UINT8  error_reason,
                                    UINT32 error_data)
{
    tNFA_CHO_API_SEL_ERR *p_msg;

    CHO_TRACE_API2 ("NFA_ChoSendSelectError (): error_reason=0x%x, error_data=0x%x",
                     error_reason, error_data);

    if (nfa_cho_cb.state == NFA_CHO_ST_DISABLED)
    {
        CHO_TRACE_ERROR0 ("NFA_ChoSendSelectError (): Not registered");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_CHO_API_SEL_ERR *) GKI_getbuf (sizeof (tNFA_CHO_API_SEL_ERR))) != NULL)
    {
        p_msg->hdr.event = NFA_CHO_API_SEL_ERR_EVT;

        p_msg->error_reason = error_reason;
        p_msg->error_data   = error_data;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ChoSetTraceLevel
**
** Description      This function sets the trace level for CHO.  If called with
**                  a value of 0xFF, it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 NFA_ChoSetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        nfa_cho_cb.trace_level = new_level;

    return (nfa_cho_cb.trace_level);
}

#if (defined (NFA_CHO_TEST_INCLUDED) && (NFA_CHO_TEST_INCLUDED == TRUE))
/*******************************************************************************
**
** Function         NFA_ChoSetTestParam
**
** Description      This function is called to set test parameters.
**
*******************************************************************************/
void NFA_ChoSetTestParam (UINT8        test_enable,
                          UINT8        test_version,
                          UINT16       test_random_number)
{
    nfa_cho_cb.test_enabled         = test_enable;
    nfa_cho_cb.test_version         = test_version;
    nfa_cho_cb.test_random_number   = test_random_number;
}
#endif
