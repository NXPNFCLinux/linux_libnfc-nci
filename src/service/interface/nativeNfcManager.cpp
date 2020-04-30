/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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

#include <semaphore.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "NfcTag.h"
#include "NfcDefs.h"
#include "NfcAdaptation.h"
#include "SyncEvent.h"
#include "OverrideLog.h"
#include "IntervalTimer.h"
#include "nativeNfcManager.h"
#include "nativeNfcSnep.h"
#include "RoutingManager.h"
#include "nativeNfcLlcp.h"

extern "C"
{
    #include "nfa_api.h"
    #include "nfa_p2p_api.h"
    #include "nfa_snep_api.h"
    #include "nfa_hci_api.h"
    #include "rw_api.h"
    #include "nfa_ee_api.h"
    #include "nfc_brcm_defs.h"
    #include "nci_config.h"
    #include "ce_api.h"
    #include "phNxpLog.h"
    #include "phNxpExtns.h"
    #include "phNxpConfig.h"
    #include "phNxpNciHal.h"
}

/*****************************************************************************
**
** public variables and functions
**
*****************************************************************************/
#define DEFAULT_TECH_MASK           (NFA_TECHNOLOGY_MASK_A \
                                     | NFA_TECHNOLOGY_MASK_B \
                                     | NFA_TECHNOLOGY_MASK_F \
                                     | NFA_TECHNOLOGY_MASK_ISO15693 \
                                     | NFA_TECHNOLOGY_MASK_A_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_F_ACTIVE \
                                     | NFA_TECHNOLOGY_MASK_KOVIO)
#define DEFAULT_DISCOVERY_DURATION       500
#define READER_MODE_DISCOVERY_DURATION    200
/* Transaction Events in order */
typedef enum transcation_events
{
    NFA_TRANS_DEFAULT = 0x00,
    NFA_TRANS_ACTIVATED_EVT,
    NFA_TRANS_EE_ACTION_EVT,
    NFA_TRANS_DM_RF_FIELD_EVT,
    NFA_TRANS_DM_RF_FIELD_EVT_ON,
    NFA_TRANS_DM_RF_TRANS_START,
    NFA_TRANS_DM_RF_FIELD_EVT_OFF,
    NFA_TRANS_DM_RF_TRANS_PROGRESS,
    NFA_TRANS_DM_RF_TRANS_END,
    NFA_TRANS_CE_ACTIVATED = 0x18,
    NFA_TRANS_CE_DEACTIVATED = 0x19,
}eTranscation_events_t;

/*Structure to store  discovery parameters*/
typedef struct discovery_Parameters
{
    int technologies_mask;
    BOOLEAN reader_mode;
    BOOLEAN enable_host_routing;
    BOOLEAN restart;
}discovery_Parameters_t;


/*Structure to store transcation result*/
typedef struct Transcation_Check
{
    BOOLEAN trans_in_progress;
    char last_request;
    eTranscation_events_t current_transcation_state;
    discovery_Parameters_t discovery_params;
}Transcation_Check_t;

/*****************************************************************************
**
** private variables and functions
**
*****************************************************************************/

BOOLEAN                        gActivated = false;
SyncEvent                      gDeactivatedEvent;
Mutex                          gSyncMutex;

static Transcation_Check_t     sTransaction_data;
static BOOLEAN                 sDiscCmdwhleNfcOff = false;
static BOOLEAN                 sIsNfaEnabled = false;
static BOOLEAN                 sRfEnabled = false;
static BOOLEAN                 sDiscoveryEnabled = false;  //is polling or listening
static BOOLEAN                 sPollingEnabled = false;  //is polling for tag?
static BOOLEAN                 sIsDisabling = false;
static BOOLEAN                 sReaderModeEnabled = false;
static BOOLEAN                 sP2pActive = false; // whether p2p was last active
static BOOLEAN                 sIsP2pListening;            // If P2P listening is enabled or not
static tNFA_TECHNOLOGY_MASK    sP2pListenTechMask; // P2P Listen mask
static UINT32                  sTech_mask;
static UINT32                  sDiscovery_duration;
static BOOLEAN                 sAbortConnlessWait = false;
//static UINT16                sCurrentConfigLen;
//static UINT8                 sConfig[256];

static SyncEvent               sNfaEnableEvent;  //event for NFA_Enable()
static SyncEvent               sNfaDisableEvent;  //event for NFA_Disable()
static SyncEvent               sNfaEnableDisablePollingEvent;//event for polling
static SyncEvent               sNfaNxpNtfEvent;
static SyncEvent               sP2PListenEvent;              // completion event for NFA_SetP2pListenTech()

static IntervalTimer           scleanupTimerProc_transaction;

static BOOLEAN                 sMultiProtocolSupport=true;
static BOOLEAN                 sSelectNext=false;

void startRfDiscovery (BOOLEAN isStart);
BOOLEAN isDiscoveryStarted();

static tNFA_STATUS stopPolling_rfDiscoveryDisabled();
static tNFA_STATUS startPolling_rfDiscoveryDisabled(tNFA_TECHNOLOGY_MASK tech_mask);
static void NxpResponsePropCmd_Cb(UINT8 event, UINT16 param_len, UINT8 *p_param);
static void nfaConnectionCallback (UINT8 event, tNFA_CONN_EVT_DATA *eventData);
static void nfaDeviceManagementCallback (UINT8 event, tNFA_DM_CBACK_DATA *eventData);
static bool isPeerToPeer (tNFA_ACTIVATED& activated);
static void notifyPollingEventwhileNfcOff();
static void sig_handler(int signo);
static void cleanupTimerProc_transaction(union sigval);
static void* enableThread(void *arg);
static void cleanup_timer();
static void handleRfDiscoveryEvent (tNFC_RESULT_DEVT* discoveredDevice);
static BOOLEAN isListenMode(tNFA_ACTIVATED& activated);

void checkforTranscation(UINT8 connEvent, void* eventData);

// flag for nfa callback indicating we are selecting for RF interface switch
extern BOOLEAN                 gIsSelectingRfInterface;

extern BOOLEAN                 gIsTagDeactivating;
extern nfcTagCallback_t        *gTagCallback;
extern tNFA_INTF_TYPE          sCurrentRfInterface;

extern BOOLEAN nativeNfcTag_getReconnectState(void);
extern BOOLEAN nativeNfcTag_setReconnectState(void);
extern void nativeNfcTag_doDeactivateStatus (INT32 status);
extern void nativeNfcTag_doConnectStatus (BOOLEAN isConnectOk);
extern void nativeNfcTag_doTransceiveStatus (tNFA_STATUS status, UINT8 * buf, UINT32 buflen);
extern void nativeNfcTag_doWriteStatus (BOOLEAN is_write_ok);
extern void nativeNfcTag_doCheckNdefResult (tNFA_STATUS status, UINT32 max_size, UINT32 current_size, UINT8 flags);
extern void nativeNfcTag_doMakeReadonlyResult (tNFA_STATUS status);
extern void nativeNfcTag_doPresenceCheckResult (tNFA_STATUS status);
extern void nativeNfcTag_formatStatus (BOOLEAN is_ok);
extern void nativeNfcTag_doReadCompleted (tNFA_STATUS status);
extern void nativeNfcTag_acquireRfInterfaceMutexLock();
extern void nativeNfcTag_releaseRfInterfaceMutexLock();
extern void nativeNfcTag_resetPresenceCheck ();
extern void nativeNfcTag_notifyRfTimeout ();
extern void nativeNfcTag_abortWaits ();
extern void nativeNfcTag_registerNdefTypeHandler ();
extern void nativeNfcTag_deregisterNdefTypeHandler ();
extern void nativeNfcTag_releasePresenceCheck();

static tNFA_STATUS SendAGCDebugCommand();
typedef struct enableAGC_debug
{
    long enableAGC; // config param
    bool AGCdebugstarted;// flag to indicate agc ongoing
    bool AGCdebugrunning;//flag to indicate agc running or stopped.
}enableAGC_debug_t;
static enableAGC_debug_t menableAGC_debug_t;
void *enableAGCThread(void *arg);
static void nfcManagerEnableAGCDebug(UINT8 connEvent);
void set_AGC_process_state(bool state);
bool get_AGC_process_state();

typedef struct nxp_feature_data
{
    SyncEvent    NxpFeatureConfigEvt;
    tNFA_STATUS  wstatus;
    UINT8        rsp_data[255];
    UINT8        rsp_len;
}Nxp_Feature_Data_t;
static Nxp_Feature_Data_t gnxpfeature_conf;
void SetCbStatus(tNFA_STATUS status);
tNFA_STATUS GetCbStatus(void);

/*******************************************************************************
**
** Function         sig_handler
**
** Description      This function is used to handle the different types of
**                  signal events.
**
** Returns          None
**
*******************************************************************************/
void sig_handler(int signo)
{
    switch (signo)
    {
        case SIGINT:
            NXPLOG_API_D("received SIGINT\n");
            break;
        case SIGABRT:
            NXPLOG_API_D("received SIGABRT\n");
            NFA_HciW4eSETransaction_Complete(Wait);
            break;
        case SIGSEGV:
            NXPLOG_API_D("received SIGSEGV\n");
            break;
        case SIGHUP:
            NXPLOG_API_D("received SIGHUP\n");
            break;
    }
}

/*******************************************************************************
**
** Function:        handleRfDiscoveryEvent
**
** Description:     Handle RF-discovery events from the stack.
**                  discoveredDevice: Discovered device.
**
** Returns:         None
**
*******************************************************************************/
static void handleRfDiscoveryEvent (tNFC_RESULT_DEVT* discoveredDevice)
{
    NfcTag::getInstance ().mNumDiscNtf++;

#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(discoveredDevice->more == NCI_DISCOVER_NTF_MORE)
    {
        //there is more discovery notification coming
        NXPLOG_API_D ("%s: numDiscovery=%u (Expect More??)", __FUNCTION__, NfcTag::getInstance ().mNumDiscNtf);
        return;
    }
#endif
    BOOLEAN isP2p = NfcTag::getInstance ().isP2pDiscovered ();
    if (!sReaderModeEnabled && isP2p)
    {
        //select the peer that supports P2P
        NfcTag::getInstance ().selectP2p();
    }
    else
    {
        NXPLOG_API_D ("%s: numDiscovery=%u", __FUNCTION__, NfcTag::getInstance ().mNumDiscNtf);


        if (!sReaderModeEnabled)
        {
             /* Commented below statement as it is not clear why this notification
              * is ignored */
            //NfcTag::getInstance ().mNumDiscNtf--;
            NXPLOG_API_D ("%s: sReaderModeEnabled=false ", __FUNCTION__);
        }

        if(!sMultiProtocolSupport)
        {
            NfcTag::getInstance ().mNumDiscNtf = 0;
            NfcTag::getInstance ().mNumTags++;
            NXPLOG_API_D ("%s: sMultiProtocolSupport=false", __FUNCTION__);
        }
        else
        {
            NfcTag::getInstance ().mNumTags =
                    NfcTag::getInstance ().mNumDiscNtf;
        }

        //select the first of multiple tags that is discovered
        NfcTag::getInstance ().selectFirstTag();

    }
}

/*******************************************************************************
**
** Function:        notifyPollingEventwhileNfcOff
**
** Description:     Notifies sNfaEnableDisablePollingEvent if tag operations
**                  is in progress at the time Nfc Off is in progress to avoid
**                  NFC off thread infinite block.
**
** Returns:         None
**
*******************************************************************************/
static void notifyPollingEventwhileNfcOff()
{
    NXPLOG_API_D ("%s: sDiscCmdwhleNfcOff=%x", __FUNCTION__, sDiscCmdwhleNfcOff);
    if(sDiscCmdwhleNfcOff == true)
    {
        SyncEventGuard guard (sNfaEnableDisablePollingEvent);
        sNfaEnableDisablePollingEvent.notifyOne ();
    }
}

static void NxpResponsePropCmd_Cb(UINT8 event, UINT16 param_len, UINT8 *p_param)
{
    (void)event;
    (void)param_len;
    (void)p_param;
    NXPLOG_API_D("NxpResponsePropCmd_Cb Received length data = 0x%x status = 0x%x", param_len, p_param[3]);
    SyncEventGuard guard (sNfaNxpNtfEvent);
    sNfaNxpNtfEvent.notifyOne ();

}

/*******************************************************************************
**
** Function:        nfaConnectionCallback
**
** Description:     Receive connection-related events from stack.
**                  connEvent: Event code.
**                  eventData: Event data.
**
** Returns:         None
**
*******************************************************************************/
static void nfaConnectionCallback (UINT8 connEvent, tNFA_CONN_EVT_DATA* eventData)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    static UINT8 prev_more_val = 0x00;
    UINT8 cur_more_val=0x00;
    NXPLOG_API_D("%s: event= %u", __FUNCTION__, connEvent);

    switch (connEvent)
    {
        case NFA_POLL_ENABLED_EVT: // whether polling successfully started
        {
            NXPLOG_API_D("%s: NFA_POLL_ENABLED_EVT: status = %u", __FUNCTION__, eventData->status);

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
        }
        break;

        case NFA_POLL_DISABLED_EVT: // Listening/Polling stopped
        {
            NXPLOG_API_D("%s: NFA_POLL_DISABLED_EVT: status = %u", __FUNCTION__, eventData->status);

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
        }
        break;

        case NFA_RF_DISCOVERY_STARTED_EVT: // RF Discovery started
        {
            NXPLOG_API_D("%s: NFA_RF_DISCOVERY_STARTED_EVT: status = %u", __FUNCTION__, eventData->status);

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
            NfcTag::getInstance ().resetDiscInfo();
        }
        break;

        case NFA_RF_DISCOVERY_STOPPED_EVT: // RF Discovery stopped event
        {
            NXPLOG_API_D("%s: NFA_RF_DISCOVERY_STOPPED_EVT: status = %u", __FUNCTION__, eventData->status);
            notifyPollingEventwhileNfcOff();
            if (nativeNfcTag_getReconnectState() == true)
            {
               eventData->deactivated.type = NFA_DEACTIVATE_TYPE_SLEEP;
               NfcTag::getInstance().setDeactivationState (eventData->deactivated);
               if (gIsTagDeactivating)
               {
                    NfcTag::getInstance().setActive(false);
                    nativeNfcTag_doDeactivateStatus(0);
               }
            }
            else
            {
                SyncEventGuard guard (sNfaEnableDisablePollingEvent);
                sNfaEnableDisablePollingEvent.notifyOne ();
                NfcTag::getInstance ().resetDiscInfo();
            }
        }
        break;

        case NFA_DISC_RESULT_EVT: // NFC link/protocol discovery notificaiton
        {
            status = eventData->disc_result.status;
            cur_more_val = eventData->disc_result.discovery_ntf.more;
            NXPLOG_API_D("%s: NFA_DISC_RESULT_EVT: status = %d", __FUNCTION__, status);
            if((cur_more_val == 0x01) && (prev_more_val != 0x02))
            {
                NXPLOG_API_D("NFA_DISC_RESULT_EVT failed");
                status = NFA_STATUS_FAILED;
            }
            else
            {
                NXPLOG_API_D("NFA_DISC_RESULT_EVT success");
                status = NFA_STATUS_OK;
                prev_more_val = cur_more_val;
            }

            if (status != NFA_STATUS_OK)
            {
                NfcTag::getInstance ().resetDiscInfo();
                NXPLOG_API_D("%s: NFA_DISC_RESULT_EVT error: status = %d", __FUNCTION__, status);
            }
            else
            {
                NfcTag::getInstance().connectionEventHandler(connEvent, eventData);
                handleRfDiscoveryEvent(&eventData->disc_result.discovery_ntf);
            }
        }
        break;

        case NFA_SELECT_RESULT_EVT: // NFC link/protocol discovery select response
        {
            NXPLOG_API_D("%s: NFA_SELECT_RESULT_EVT: status = %d, gIsSelectingRfInterface = %d, sIsDisabling=%d", __FUNCTION__, eventData->status, gIsSelectingRfInterface, sIsDisabling);

            if (sIsDisabling)
                break;

            if (eventData->status != NFA_STATUS_OK)
            {
                nativeNfcTag_doConnectStatus(false);
                NfcTag::getInstance ().resetDiscInfo();
                NXPLOG_API_D("%s: NFA_SELECT_RESULT_EVT error: status = %d", __FUNCTION__, eventData->status);
                NFA_Deactivate (FALSE);
            }
        }
        break;

        case NFA_DEACTIVATE_FAIL_EVT:
        {
            NXPLOG_API_D("%s: NFA_DEACTIVATE_FAIL_EVT: status = %d", __FUNCTION__, eventData->status);
            SyncEventGuard g (gDeactivatedEvent);
            gDeactivatedEvent.notifyOne ();

            SyncEventGuard guard (sNfaEnableDisablePollingEvent);
            sNfaEnableDisablePollingEvent.notifyOne ();
            NfcTag::getInstance ().resetDiscInfo();
        }
        break;

        case NFA_ACTIVATED_EVT: // NFC link/protocol activated
        {
            checkforTranscation(NFA_ACTIVATED_EVT, (void *)eventData);
            NXPLOG_API_D("%s: NFA_ACTIVATED_EVT: gIsSelectingRfInterface=%d, sIsDisabling=%d", __FUNCTION__, gIsSelectingRfInterface, sIsDisabling);
            if((eventData->activated.activate_ntf.protocol != NFA_PROTOCOL_NFC_DEP) && (!isListenMode(eventData->activated)))
            {
                sCurrentRfInterface = (tNFA_INTF_TYPE) eventData->activated.activate_ntf.intf_param.type;
            }
            if (EXTNS_GetConnectFlag() == TRUE)
            {
                NfcTag::getInstance().setActivationState ();
                nativeNfcTag_doConnectStatus(true);
                break;
            }
            NfcTag::getInstance().setActive(true);
            if (sIsDisabling || !sIsNfaEnabled)
                break;
            gActivated = true;

            NfcTag::getInstance().setActivationState ();
            if (gIsSelectingRfInterface)
            {
                nativeNfcTag_doConnectStatus(true);
                if (NfcTag::getInstance ().isCashBeeActivated() == true || NfcTag::getInstance ().isEzLinkTagActivated() == true)
                {
                    NfcTag::getInstance().connectionEventHandler (NFA_ACTIVATED_UPDATE_EVT, eventData);
                }
                break;
            }

            nativeNfcTag_resetPresenceCheck();
            if (isPeerToPeer(eventData->activated))
            {
                sP2pActive = true;
                NXPLOG_API_D("%s: NFA_ACTIVATED_EVT; is p2p", __FUNCTION__);
            }
            else
            {
                if(!NfcTag::getInstance ().mNumTags)
                {
                    /*
                    * Since Single Card is Activated consider
                    * the number of tags to be single
                    *
                    * */
                    NfcTag::getInstance ().mNumTags++;
                }

                NfcTag::getInstance().connectionEventHandler (connEvent, eventData);

                if(NfcTag::getInstance ().mNumDiscNtf)
                {
                    //NXPLOG_API_D("%s: Multiple Tags Deactivating", __FUNCTION__);
                    //NFA_Deactivate (TRUE);
                }
            }
        }
        break;

        case NFA_DEACTIVATED_EVT: // NFC link/protocol deactivated
        {
            NXPLOG_API_D("%s: NFA_DEACTIVATED_EVT   Type: %u, gIsTagDeactivating: %d",
                            __FUNCTION__, eventData->deactivated.type,gIsTagDeactivating);
            notifyPollingEventwhileNfcOff();
            if (true == nativeNfcTag_getReconnectState())
            {
                NXPLOG_API_D("Reconnect in progress : Do nothing");
                break;
            }
            NfcTag::getInstance().setDeactivationState (eventData->deactivated);

            if (eventData->deactivated.type != NFA_DEACTIVATE_TYPE_SLEEP)
            {
                {
                    SyncEventGuard g (gDeactivatedEvent);
                    gActivated = false; //guard this variable from multi-threaded access
                    gDeactivatedEvent.notifyOne ();
                }
                NfcTag::getInstance ().resetDiscInfo();
                nativeNfcTag_resetPresenceCheck();
                NfcTag::getInstance().connectionEventHandler (connEvent, eventData);
                nativeNfcTag_abortWaits();
                NfcTag::getInstance().abort ();
            }
            else if (gIsTagDeactivating)
            {
                NfcTag::getInstance().setActive(false);
                nativeNfcTag_doDeactivateStatus(0);
            }
            else if (EXTNS_GetDeactivateFlag() == TRUE)
            {
                NfcTag::getInstance().setActive(false);
                nativeNfcTag_doDeactivateStatus(0);
            }

            if( sSelectNext && NfcTag::getInstance ().mNumTags)
            {
                nativeNfcTag_releasePresenceCheck();
                {
                    SyncEventGuard g (gDeactivatedEvent);
                    gActivated = false; //guard this variable from multi-threaded access
                    gDeactivatedEvent.notifyOne ();
                }
                //NfcTag::getInstance ().mNumDiscNtf--;

                NXPLOG_API_D("%s: Multiple Tags=%d Select Next", __FUNCTION__
                				, NfcTag::getInstance ().mNumTags);

                NfcTag::getInstance().selectNextTag();

                sSelectNext = false;
            }

            // If RF is activated for what we think is a Secure Element transaction
            // and it is deactivated to either IDLE or DISCOVERY mode, notify w/event.
            if ((eventData->deactivated.type == NFA_DEACTIVATE_TYPE_IDLE)
                    || (eventData->deactivated.type == NFA_DEACTIVATE_TYPE_DISCOVERY))
            {
                if (sP2pActive) {
                    sP2pActive = false;
                    // Make sure RF field events are re-enabled
                    NXPLOG_API_D("%s: NFA_DEACTIVATED_EVT; is p2p", __FUNCTION__);
                }
            }
        }

        break;

        case NFA_TLV_DETECT_EVT: // TLV Detection complete
        status = eventData->tlv_detect.status;
        NXPLOG_API_D("%s: NFA_TLV_DETECT_EVT: status = %d, protocol = %d, num_tlvs = %d, num_bytes = %d",
             __FUNCTION__, status, eventData->tlv_detect.protocol,
             eventData->tlv_detect.num_tlvs, eventData->tlv_detect.num_bytes);
        if (status != NFA_STATUS_OK)
        {
            NXPLOG_API_D("%s: NFA_TLV_DETECT_EVT error: status = %d", __FUNCTION__, status);
        }
        break;

        case NFA_NDEF_DETECT_EVT: // NDEF Detection complete;
        //if status is failure, it means the tag does not contain any or valid NDEF data;
        //pass the failure status to the NFC Service;
        status = eventData->ndef_detect.status;
        NXPLOG_API_D("%s: NFA_NDEF_DETECT_EVT: status = 0x%X, protocol = %u, "
             "max_size = %lu, cur_size = %lu, flags = 0x%X", __FUNCTION__,
             status,
             eventData->ndef_detect.protocol, eventData->ndef_detect.max_size,
             eventData->ndef_detect.cur_size, eventData->ndef_detect.flags);
        NfcTag::getInstance().connectionEventHandler (connEvent, eventData);
        nativeNfcTag_doCheckNdefResult(status,
            eventData->ndef_detect.max_size, eventData->ndef_detect.cur_size,
            eventData->ndef_detect.flags);
        break;

        case NFA_DATA_EVT: // Data message received (for non-NDEF reads)
        NXPLOG_API_D("%s: NFA_DATA_EVT: status = 0x%X, len = %d", __FUNCTION__, eventData->status, eventData->data.len);
        nativeNfcTag_doTransceiveStatus(eventData->status, eventData->data.p_data, eventData->data.len);
        break;

        case NFA_RW_INTF_ERROR_EVT:
        NXPLOG_API_D("%s: NFC_RW_INTF_ERROR_EVT", __FUNCTION__);
        nativeNfcTag_notifyRfTimeout();
        nativeNfcTag_doReadCompleted (NFA_STATUS_TIMEOUT);
        break;

        case NFA_SELECT_CPLT_EVT: // Select completed
        status = eventData->status;
        NXPLOG_API_D("%s: NFA_SELECT_CPLT_EVT: status = %d", __FUNCTION__, status);
        if (status != NFA_STATUS_OK)
        {
            NXPLOG_API_D("%s: NFA_SELECT_CPLT_EVT error: status = %d", __FUNCTION__, status);
        }
        break;

        case NFA_READ_CPLT_EVT: // NDEF-read or tag-specific-read completed
        NXPLOG_API_D("%s: NFA_READ_CPLT_EVT: status = 0x%X", __FUNCTION__, eventData->status);
        nativeNfcTag_doReadCompleted (eventData->status);
        NfcTag::getInstance().connectionEventHandler (connEvent, eventData);
        break;

        case NFA_WRITE_CPLT_EVT: // Write completed
        NXPLOG_API_D("%s: NFA_WRITE_CPLT_EVT: status = %d", __FUNCTION__, eventData->status);
        nativeNfcTag_doWriteStatus (eventData->status == NFA_STATUS_OK);
        break;

        case NFA_SET_TAG_RO_EVT: // Tag set as Read only
        NXPLOG_API_D("%s: NFA_SET_TAG_RO_EVT: status = %d", __FUNCTION__, eventData->status);
        nativeNfcTag_doMakeReadonlyResult(eventData->status);
        break;

        case NFA_CE_NDEF_WRITE_START_EVT: // NDEF write started
        NXPLOG_API_D("%s: NFA_CE_NDEF_WRITE_START_EVT: status: %d", __FUNCTION__, eventData->status);

        if (eventData->status != NFA_STATUS_OK)
        {
            NXPLOG_API_D("%s: NFA_CE_NDEF_WRITE_START_EVT error: status = %d", __FUNCTION__, eventData->status);
        }
        break;

        case NFA_CE_NDEF_WRITE_CPLT_EVT: // NDEF write completed
        NXPLOG_API_D("%s: FA_CE_NDEF_WRITE_CPLT_EVT: len = %lu", __FUNCTION__, eventData->ndef_write_cplt.len);
        break;

        case NFA_LLCP_ACTIVATED_EVT: // LLCP link is activated
        NXPLOG_API_D("%s: NFA_LLCP_ACTIVATED_EVT: is_initiator: %d  remote_wks: %d, remote_lsc: %d, remote_link_miu: %d, local_link_miu: %d",
             __FUNCTION__,
             eventData->llcp_activated.is_initiator,
             eventData->llcp_activated.remote_wks,
             eventData->llcp_activated.remote_lsc,
             eventData->llcp_activated.remote_link_miu,
             eventData->llcp_activated.local_link_miu);
        break;

        case NFA_LLCP_DEACTIVATED_EVT: // LLCP link is deactivated
        NXPLOG_API_D("%s: NFA_LLCP_DEACTIVATED_EVT", __FUNCTION__);
        break;

        case NFA_LLCP_FIRST_PACKET_RECEIVED_EVT: // Received first packet over llcp
        NXPLOG_API_D("%s: NFA_LLCP_FIRST_PACKET_RECEIVED_EVT", __FUNCTION__);
        break;

        case NFA_PRESENCE_CHECK_EVT:
        NXPLOG_API_D("%s: NFA_PRESENCE_CHECK_EVT", __FUNCTION__);
        nativeNfcTag_doPresenceCheckResult (eventData->status);
        break;

        case NFA_FORMAT_CPLT_EVT:
        NXPLOG_API_D("%s: NFA_FORMAT_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
        nativeNfcTag_formatStatus (eventData->status == NFA_STATUS_OK);
        break;

        case NFA_I93_CMD_CPLT_EVT:
        NXPLOG_API_D("%s: NFA_I93_CMD_CPLT_EVT: status=0x%X", __FUNCTION__, eventData->status);
        break;

        case NFA_SET_P2P_LISTEN_TECH_EVT:
        {
            NXPLOG_API_D("%s: NFA_SET_P2P_LISTEN_TECH_EVT", __FUNCTION__);
            SyncEventGuard guard (sP2PListenEvent);
            sP2PListenEvent.notifyOne(); //unblock NFA_SetP2pListenTech()
            break;
        }
        break;

        case NFA_CE_LOCAL_TAG_CONFIGURED_EVT:
        {
            NXPLOG_API_D("%s: NFA_CE_LOCAL_TAG_CONFIGURED_EVT", __FUNCTION__);
            break;
        }

        default:
        {
            NXPLOG_API_D("%s: unknown event ????", __FUNCTION__);
            break;
        }
    }
}

/*******************************************************************************
**
** Function:        nfaDeviceManagementCallback
**
** Description:     Receive device management events from stack.
**                  dmEvent: Device-management event ID.
**                  eventData: Data associated with event ID.
**
** Returns:         None
**
*******************************************************************************/
void nfaDeviceManagementCallback (UINT8 dmEvent, tNFA_DM_CBACK_DATA* eventData)
{
    NXPLOG_API_D ("%s: enter; event=0x%X", __FUNCTION__, dmEvent);

    switch (dmEvent)
    {
    case NFA_DM_ENABLE_EVT: /* Result of NFA_Enable */
        {
            SyncEventGuard guard (sNfaEnableEvent);
            NXPLOG_API_D ("%s: NFA_DM_ENABLE_EVT; status=0x%X",
                    __FUNCTION__, eventData->status);
            sIsNfaEnabled = eventData->status == NFA_STATUS_OK;
            sIsDisabling = false;
            sNfaEnableEvent.notifyOne ();
        }
        break;

    case NFA_DM_DISABLE_EVT: /* Result of NFA_Disable */
        {
            SyncEventGuard guard (sNfaDisableEvent);
            NXPLOG_API_D ("%s: NFA_DM_DISABLE_EVT", __FUNCTION__);
            sIsNfaEnabled = false;
            sIsDisabling = false;
            sNfaDisableEvent.notifyOne ();
        }
        break;

    case NFA_DM_SET_CONFIG_EVT: //result of NFA_SetConfig
        NXPLOG_API_D ("%s: NFA_DM_SET_CONFIG_EVT", __FUNCTION__);
        break;
#if 0
    case NFA_DM_GET_CONFIG_EVT: /* Result of NFA_GetConfig */
        NXPLOG_API_D ("%s: NFA_DM_GET_CONFIG_EVT", __FUNCTION__);
        {
            HciRFParams::getInstance().connectionEventHandler(dmEvent,eventData);
            SyncEventGuard guard (sNfaGetConfigEvent);
            if (eventData->status == NFA_STATUS_OK &&
                    eventData->get_config.tlv_size <= sizeof(sConfig))
            {
                sCurrentConfigLen = eventData->get_config.tlv_size;
                memcpy(sConfig, eventData->get_config.param_tlvs, eventData->get_config.tlv_size);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#ifdef CHECK_FOR_NFCEE_CONFIGURATION
                if(sCheckNfceeFlag)
                    checkforNfceeBuffer();
#endif
#endif
            }
            else
            {
                ALOGE("%s: NFA_DM_GET_CONFIG failed", __FUNCTION__);
                sCurrentConfigLen = 0;
            }
            sNfaGetConfigEvent.notifyOne();
        }
        break;
#endif //if 0
    case NFA_DM_RF_FIELD_EVT:
        checkforTranscation(NFA_TRANS_DM_RF_FIELD_EVT, (void *)eventData);
        NXPLOG_API_D ("%s: NFA_DM_RF_FIELD_EVT; status=0x%X; field status=%u", __FUNCTION__,
              eventData->rf_field.status, eventData->rf_field.rf_field_status);
        if (sIsDisabling || !sIsNfaEnabled)
            break;

        if (!sP2pActive && eventData->rf_field.status == NFA_STATUS_OK)
        {
//            RoutingManager::getInstance().notifyRfFieldEvent (
//                eventData->rf_field.rf_field_status == NFA_DM_RF_FIELD_ON);
        }
        break;
    case NFA_DM_NFCC_TRANSPORT_ERR_EVT:
    case NFA_DM_NFCC_TIMEOUT_EVT:
        {
            if (dmEvent == NFA_DM_NFCC_TIMEOUT_EVT)
            {
                NXPLOG_API_D ("%s: NFA_DM_NFCC_TIMEOUT_EVT; abort", __FUNCTION__);
            }
            else if (dmEvent == NFA_DM_NFCC_TRANSPORT_ERR_EVT)
            {
                NXPLOG_API_D ("%s: NFA_DM_NFCC_TRANSPORT_ERR_EVT; abort", __FUNCTION__);
            }
            NFA_HciW4eSETransaction_Complete(Wait);
            nativeNfcTag_abortWaits();
            NfcTag::getInstance().abort ();
            sAbortConnlessWait = true;
            {
                NXPLOG_API_D ("%s: aborting  sNfaEnableDisablePollingEvent", __FUNCTION__);
                SyncEventGuard guard (sNfaEnableDisablePollingEvent);
                sNfaEnableDisablePollingEvent.notifyOne();
            }
            {
                NXPLOG_API_D ("%s: aborting  sNfaEnableEvent", __FUNCTION__);
                SyncEventGuard guard (sNfaEnableEvent);
                sNfaEnableEvent.notifyOne();
            }
            {
                NXPLOG_API_D ("%s: aborting  sNfaDisableEvent", __FUNCTION__);
                SyncEventGuard guard (sNfaDisableEvent);
                sNfaDisableEvent.notifyOne();
            }
            sDiscoveryEnabled = false;
            sPollingEnabled = false;

            if (!sIsDisabling && sIsNfaEnabled)
            {
                EXTNS_Close();
                NFA_Disable(FALSE);
                sIsDisabling = true;
            }
            else
            {
                sIsNfaEnabled = false;
                sIsDisabling = false;
            }
            NXPLOG_API_D ("%s: crash NFC service", __FUNCTION__);
            //////////////////////////////////////////////
            //crash the NFC service process so it can restart automatically
            //TODO: abort ();
            //////////////////////////////////////////////
        }
        break;
#if 0
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    case NFA_DM_SET_ROUTE_CONFIG_REVT:
        NXPLOG_API_D ("%s: NFA_DM_SET_ROUTE_CONFIG_REVT; status=0x%X",
                __FUNCTION__, eventData->status);
        if(eventData->status != NFA_STATUS_OK)
        {
            NXPLOG_API_D("AID Routing table configuration Failed!!!");
        }
        else
        {
            NXPLOG_API_D("AID Routing Table configured.");
        }
        break;
#endif

    case NFA_DM_EMVCO_PCD_COLLISION_EVT:
        ALOGE("STATUS_EMVCO_PCD_COLLISION - Multiple card detected");
        SecureElement::getInstance().notifyEmvcoMultiCardDetectedListeners();
        break;
#endif
    default:
        NXPLOG_API_D ("%s: unhandled event", __FUNCTION__);
        break;
    }
}

/*******************************************************************************
**
** Function:        isPeerToPeer
**
** Description:     Whether the activation data indicates the peer supports NFC-DEP.
**                  activated: Activation data.
**
** Returns:         True if the peer supports NFC-DEP.
**
*******************************************************************************/
static bool isPeerToPeer (tNFA_ACTIVATED& activated)
{
    return activated.activate_ntf.protocol == NFA_PROTOCOL_NFC_DEP;
}

/*******************************************************************************
**
** Function:        isListenMode
**
** Description:     Indicates whether the activation data indicates it is
**                  listen mode.
**
** Returns:         True if this listen mode.
**
*******************************************************************************/
static BOOLEAN isListenMode(tNFA_ACTIVATED& activated)
{
    return ((NFC_DISCOVERY_TYPE_LISTEN_A == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_DISCOVERY_TYPE_LISTEN_B == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_DISCOVERY_TYPE_LISTEN_F == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_DISCOVERY_TYPE_LISTEN_ISO15693 == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_DISCOVERY_TYPE_LISTEN_B_PRIME == activated.activate_ntf.rf_tech_param.mode)
            || (NFC_INTERFACE_EE_DIRECT_RF == activated.activate_ntf.intf_param.type));
}

/*******************************************************************************
**
** Function:        enableP2pListening
**
** Description:     Start/stop polling/listening to peer that supports P2P.
**                  isEnable: Is enable polling/listening?
**
** Returns:         None
**
*******************************************************************************/
static void enableP2pListening (bool isEnable)
{
    tNFA_STATUS nfaStat = NFA_STATUS_FAILED;

    NXPLOG_API_D ("%s: enter isEnable: %u  mIsP2pListening: %u",
                  "enableP2pListening", isEnable, sIsP2pListening);

    // If request to enable P2P listening, and we were not already listening
    if ( (isEnable == true) && (sIsP2pListening == false) && (sP2pListenTechMask != 0) )
    {
        SyncEventGuard guard (sP2PListenEvent);
        if ((nfaStat = NFA_SetP2pListenTech (sP2pListenTechMask)) == NFA_STATUS_OK)
        {
            sP2PListenEvent.wait ();
            sIsP2pListening = true;
        }
        else
        {
            NXPLOG_API_E ("%s: fail enable listen; error=0x%X",
                          "enableP2pListening", nfaStat);
        }
    }
    else if ( (isEnable == false) && (sIsP2pListening == true) )
    {
        SyncEventGuard guard (sP2PListenEvent);
        // Request to disable P2P listening, check if it was enabled
        if ((nfaStat = NFA_SetP2pListenTech(0)) == NFA_STATUS_OK)
        {
            sP2PListenEvent.wait ();
            sIsP2pListening = false;
        }
        else
        {
            NXPLOG_API_E ("%s: fail disable listen; error=0x%X",
                          "enableP2pListening", nfaStat);
        }
    }
    NXPLOG_API_D ("%s: exit; mIsP2pListening: %u", "enableP2pListening",
                  sIsP2pListening);
}


/*******************************************************************************
**
** Function:        startRfDiscovery
**
** Description:     Ask stack to start polling and listening for devices.
**                  isStart: Whether to start.
**
** Returns:         None
**
*******************************************************************************/
void startRfDiscovery(BOOLEAN isStart)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;

    NXPLOG_API_D ("%s: is start=%d", __FUNCTION__, isStart);
    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    status  = isStart ? NFA_StartRfDiscovery () : NFA_StopRfDiscovery ();
    if (status == NFA_STATUS_OK)
    {
        //if(gGeneralPowershutDown == VEN_CFG_NFC_OFF_POWER_OFF)
        //{
        //    sDiscCmdwhleNfcOff = true;
        //}
        sNfaEnableDisablePollingEvent.wait (); //wait for NFA_RF_DISCOVERY_xxxx_EVT
        sRfEnabled = isStart;
        sDiscCmdwhleNfcOff = false;
    }
    else
    {
        NXPLOG_API_D ("%s: Failed to start/stop RF discovery; error=0x%X", __FUNCTION__, status);
    }
}

BOOLEAN isDiscoveryStarted()
{
    return sRfEnabled;
}

static tNFA_STATUS startPolling_rfDiscoveryDisabled(tNFA_TECHNOLOGY_MASK tech_mask) {
    tNFA_STATUS stat = NFA_STATUS_FAILED;

    unsigned long num = 0;

    if (tech_mask == 0 && GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
        tech_mask = num;
    else if (tech_mask == 0) tech_mask = DEFAULT_TECH_MASK;

    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    NXPLOG_API_D ("%s: enable polling", __FUNCTION__);
    stat = NFA_EnablePolling (tech_mask);
    if (stat == NFA_STATUS_OK)
    {
        NXPLOG_API_D ("%s: wait for enable event", __FUNCTION__);
        sPollingEnabled = true;
        sNfaEnableDisablePollingEvent.wait (); //wait for NFA_POLL_ENABLED_EVT
    }
    else
    {
        NXPLOG_API_D ("%s: fail enable polling; error=0x%X", __FUNCTION__, stat);
    }

    return stat;
}

static tNFA_STATUS stopPolling_rfDiscoveryDisabled() {
    tNFA_STATUS stat = NFA_STATUS_FAILED;

    SyncEventGuard guard (sNfaEnableDisablePollingEvent);
    NXPLOG_API_D ("%s: disable polling", __FUNCTION__);
    stat = NFA_DisablePolling ();
    if (stat == NFA_STATUS_OK) {
        sPollingEnabled = false;
        sNfaEnableDisablePollingEvent.wait (); //wait for NFA_POLL_DISABLED_EVT
    } else {
        NXPLOG_API_D ("%s: fail disable polling; error=0x%X", __FUNCTION__, stat);
    }

    return stat;
}

/*******************************************************************************
 **
 ** Function:        checkforTranscation
 **
 ** Description:     Receive connection-related events from stack.
 **                  connEvent: Event code.
 **                  eventData: Event data.
 **
 ** Returns:         None
 **
 *******************************************************************************/
void checkforTranscation(UINT8 connEvent, void* eventData)
{
    tNFA_CONN_EVT_DATA *eventAct_Data = (tNFA_CONN_EVT_DATA*) eventData;
    tNFA_DM_CBACK_DATA* eventDM_Conn_data = (tNFA_DM_CBACK_DATA *) eventData;
    tNFA_EE_CBACK_DATA* ee_action_data = (tNFA_EE_CBACK_DATA *) eventData;
    tNFA_EE_ACTION& action = ee_action_data->action;
    NXPLOG_API_D ("%s: enter; event=0x%X transaction_data.current_transcation_state = 0x%x", __FUNCTION__, connEvent,
            sTransaction_data.current_transcation_state);
    switch(connEvent)
    {
    case NFA_ACTIVATED_EVT:
        if((eventAct_Data->activated.activate_ntf.protocol != NFA_PROTOCOL_NFC_DEP) && (isListenMode(eventAct_Data->activated)))
        {
            NXPLOG_API_D("ACTIVATED_EVT setting flag");
            sTransaction_data.current_transcation_state = NFA_TRANS_ACTIVATED_EVT;
            sTransaction_data.trans_in_progress = true;
        }else{
            NXPLOG_API_D("other event clearing flag ");
            memset(&sTransaction_data, 0x00, sizeof(Transcation_Check_t));
        }
        break;
    case NFA_EE_ACTION_EVT:
        if (sTransaction_data.current_transcation_state == NFA_TRANS_DEFAULT
                || sTransaction_data.current_transcation_state == NFA_TRANS_ACTIVATED_EVT )
        {
//            if(getScreenState() == NFA_SCREEN_STATE_OFF)
//            {
//                if (!sP2pActive && eventDM_Conn_data->rf_field.status == NFA_STATUS_OK)
//                   RoutingManager::getInstance().notifyRfFieldEvent (true);
//            }

#if (NFC_NXP_CHIP_PN548AD == FALSE)
            if((action.param.technology == NFC_RF_TECHNOLOGY_A))
            {
                sTransaction_data.current_transcation_state = NFA_TRANS_DM_RF_TRANS_END;
                memset(&sTransaction_data, 0x00, sizeof(Transcation_Check_t));
            }
            else
#endif
            {
                sTransaction_data.current_transcation_state = NFA_TRANS_EE_ACTION_EVT;
                sTransaction_data.trans_in_progress = true;
            }
        }
        break;

    case NFA_TRANS_CE_ACTIVATED:
        if (sTransaction_data.current_transcation_state == NFA_TRANS_DEFAULT || sTransaction_data.current_transcation_state == NFA_TRANS_ACTIVATED_EVT)
        {
//            if(getScreenState() == NFA_SCREEN_STATE_OFF)
//            {
//                if (!sP2pActive && eventDM_Conn_data->rf_field.status == NFA_STATUS_OK)
//                    RoutingManager::getInstance().notifyRfFieldEvent (true);
//            }
                sTransaction_data.current_transcation_state = NFA_TRANS_CE_ACTIVATED;
                sTransaction_data.trans_in_progress = true;
            }
        break;
    case NFA_TRANS_CE_DEACTIVATED:
        if (sTransaction_data.current_transcation_state == NFA_TRANS_CE_ACTIVATED)
            {
                sTransaction_data.current_transcation_state = NFA_TRANS_CE_DEACTIVATED;
            }
        break;

    case NFA_TRANS_DM_RF_FIELD_EVT:
        if (eventDM_Conn_data->rf_field.status == NFA_STATUS_OK &&
            eventDM_Conn_data->rf_field.rf_field_status == 1)
        {
            if (phNxpNciHal_getChipType() != pn547C2)
            {
                NXPLOG_API_D("NFA_TRANS_DM_RF_FIELD_EVT AGC debug enabled");
                nfcManagerEnableAGCDebug(connEvent);
            }
        }
        else if (eventDM_Conn_data->rf_field.status == NFA_STATUS_OK &&
                eventDM_Conn_data->rf_field.rf_field_status == 0)
        {
            NXPLOG_API_D("NFA_TRANS_DM_RF_FIELD_EVT AGC debug stopped");
            if (phNxpNciHal_getChipType() != pn547C2)
            {
                set_AGC_process_state(false);
            }
        }
        break;

    default:
        break;
    }

    NXPLOG_API_D ("%s: exit; event=0x%X transaction_data.current_transcation_state = 0x%x", __FUNCTION__, connEvent,
            sTransaction_data.current_transcation_state);
}

/*******************************************************************************
**
** Function:        cleanupTimerProc_transaction
**
** Description:     Callback function for interval timer.
**
** Returns:         None
**
*******************************************************************************/
static void cleanupTimerProc_transaction(union sigval)
{
    NXPLOG_API_D("Inside cleanupTimerProc");
    cleanup_timer();
}

void cleanup_timer()
{
    NXPLOG_API_D("Inside cleanup");
    //set_transcation_stat(false);
    pthread_t transaction_thread;
    int irret = -1;
    sTransaction_data.trans_in_progress = FALSE;
    NXPLOG_API_D ("%s", __FUNCTION__);

    /* Transcation is done process the last request*/
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    irret = pthread_create(&transaction_thread, &attr, enableThread, NULL);
    if(irret != 0)
    {
        NXPLOG_API_E("Unable to create the thread");
    }
    else
    {
        if(pthread_setname_np(transaction_thread,"NFCMGR_TRANS"))
        {
        	NXPLOG_API_E("pthread_setname_np in %s failed", __FUNCTION__);
        }
    }
    sTransaction_data.current_transcation_state = NFA_TRANS_DM_RF_TRANS_END;
}

/*******************************************************************************
 **
 ** Function:       enableThread
 **
 ** Description:    thread to trigger enable/disable discovery related events
 **
 ** Returns:        None .
 **
 *******************************************************************************/
void* enableThread(void *arg)
{
    (void)arg;
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    //eScreenState_t last_screen_state_request = get_lastScreenStateRequest();
    sTransaction_data.trans_in_progress = FALSE;
    BOOLEAN screen_lock_flag = false;
    BOOLEAN disable_discovery = false;

    if(sIsNfaEnabled != true || sIsDisabling == true)
        goto TheEnd;

#if 0
    if (last_screen_state_request != NFA_SCREEN_STATE_DEFAULT)
    {
        NXPLOG_API_D("update last screen state request: %d", last_screen_state_request);
        nfcManager_doSetScreenState(NULL, NULL, last_screen_state_request);
        if( last_screen_state_request == NFA_SCREEN_STATE_LOCKED)
            screen_lock_flag = true;
    }
    else
    {
        ALOGD("No request pending");
    }
#endif

    if (sTransaction_data.last_request == 1)
    {
        NXPLOG_API_D("send the last request enable");
        sDiscoveryEnabled = false;
        sPollingEnabled = false;
        nfcManager_enableDiscovery(
                sTransaction_data.discovery_params.technologies_mask,
                sTransaction_data.discovery_params.reader_mode,
                sTransaction_data.discovery_params.enable_host_routing,
                sTransaction_data.discovery_params.restart);
    }
    else if (sTransaction_data.last_request == 2)
    {
        NXPLOG_API_D("send the last request disable");
        nfcManager_disableDiscovery();
        disable_discovery = true;
    }

    if(screen_lock_flag && disable_discovery)
    {

        startRfDiscovery(TRUE);
    }
    screen_lock_flag = false;
    disable_discovery = false;
    memset(&sTransaction_data, 0x00, sizeof(Transcation_Check_t));

TheEnd:
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    pthread_exit(NULL);
}

/*******************************************************************************
**
** Function:        nativeNfcManager_sendRawFrame
**
** Description:     Send a raw frame.
**                  e: JVM environment.
**                  o: Java object.
**
** Returns:         True if ok.
**
*******************************************************************************/
INT32 nativeNfcManager_sendRawFrame (UINT8 *buf, UINT32 bufLen)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_D ("%s: Nfc not initialized.", __FUNCTION__);
        goto End;
    }
    status = NFA_SendRawFrame (buf, bufLen, 0);
End:
    gSyncMutex.unlock();
    return status;
}

/*******************************************************************************
**
** Function:        isNfcActive
**
** Description:     Used externaly to determine if NFC is active or not.
**
** Returns:         'true' if the NFC stack is running, else 'false'.
**
*******************************************************************************/
BOOLEAN nativeNfcManager_isNfcActive()
{
    BOOLEAN ret;
    ret = (sIsNfaEnabled && !sIsDisabling);
    return ret ;
}

/*******************************************************************************
**
** Function:        nfcManager_doInitialize
**
** Description:     Turn on NFC.
**                  e: JVM environment.
**                  o: Java object.
**
** Returns:         True if ok.
**
*******************************************************************************/
INT32 nativeNfcManager_doInitialize ()
{
    NXPLOG_API_D ("%s: enter; NCI_VERSION=0x%02X",
        __FUNCTION__, NCI_VERSION);
    tNFA_STATUS stat = NFA_STATUS_OK;
    unsigned long num = 0;

    gSyncMutex.lock();
    NfcTag::getInstance ().mNfcDisableinProgress = false;
    if (sIsNfaEnabled)
    {
        NXPLOG_API_D ("%s: already enabled", __FUNCTION__);
        gSyncMutex.unlock();
        return sIsNfaEnabled;
    }
    if (!isNxpConfigValid(NXP_CONFIG_TYPE_INIT))
    {
        NXPLOG_API_E ("%s: can't find libnfc-nxp-init.conf file", __FUNCTION__);
        gSyncMutex.unlock();
        return sIsNfaEnabled;
    }

    if ((signal(SIGABRT, sig_handler) == SIG_ERR) &&
            (signal(SIGSEGV, sig_handler) == SIG_ERR) &&
            (signal(SIGINT, sig_handler) == SIG_ERR) &&
            (signal(SIGHUP, sig_handler) == SIG_ERR))
    {
        NXPLOG_API_E("Failed to register signal handler");
    }

    NfcAdaptation& NfcAdaptInstance = NfcAdaptation::GetInstance();

    NfcAdaptInstance.Initialize(); //start GKI, NCI task, NFC task
    {
        SyncEventGuard guard (sNfaEnableEvent);
        tHAL_NFC_ENTRY* halFuncEntries = NfcAdaptInstance.GetHalEntryFuncs ();

        NFA_Init (halFuncEntries);

        stat = NFA_Enable (nfaDeviceManagementCallback, nfaConnectionCallback);
        if (stat == NFA_STATUS_OK)
        {
        	/* The Global App Log already initialised during Adaptation
        	 *  Initialise */
            num = initializeGlobalAppLogLevel ();
            CE_SetTraceLevel (num);
            LLCP_SetTraceLevel (num);
            NFC_SetTraceLevel (num);
            RW_SetTraceLevel (num);
            NFA_SetTraceLevel (num);
            NFA_P2pSetTraceLevel (num);
            NFA_SnepSetTraceLevel(num);
            sNfaEnableEvent.wait(); //wait for NFA command to finish
        }
        EXTNS_Init(nfaDeviceManagementCallback, nfaConnectionCallback);
        NfcAdaptInstance.Configure();
    }

    if (stat == NFA_STATUS_OK)
    {
        //sIsNfaEnabled indicates whether stack started successfully
        if (sIsNfaEnabled)
        {
            RoutingManager::getInstance().initialize();
            nativeNfcTag_registerNdefTypeHandler ();
            NfcTag::getInstance().initialize ();

            nativeNfcSnep_handleNfcOnOff(true);
            sTransaction_data.trans_in_progress = FALSE;

            if (GetNumValue(NAME_POLLING_TECH_MASK, &num, sizeof(num)))
            {
                sTech_mask = num;
                NXPLOG_API_D ("%s: NAME_POLLING_TECH_MASK mask=0x%X", __FUNCTION__, sTech_mask);
            }
            else
            {
                sTech_mask = DEFAULT_TECH_MASK;
            }
            NXPLOG_API_D ("%s: tag polling tech mask=0x%X", __FUNCTION__, sTech_mask);
            if (GetNumValue ("P2P_LISTEN_TECH_MASK", &num, sizeof (num)))
            {
                sP2pListenTechMask = num;
            }
            else
            {
                sP2pListenTechMask = (NFA_TECHNOLOGY_MASK_A
                        | NFA_TECHNOLOGY_MASK_F
                        | NFA_TECHNOLOGY_MASK_A_ACTIVE
                        | NFA_TECHNOLOGY_MASK_F_ACTIVE);
            }
            NXPLOG_API_D ("%s: P2P listen tech mask=0x%X", __FUNCTION__, sP2pListenTechMask);
            // if this value exists, set polling interval.
            if (GetNumValue(NAME_NFA_DM_DISC_DURATION_POLL, &num, sizeof(num)))
                sDiscovery_duration = num;
            else
                sDiscovery_duration = DEFAULT_DISCOVERY_DURATION;

            NFA_SetRfDiscoveryDuration(sDiscovery_duration);
            goto TheEnd;
        }
    }

    NXPLOG_API_E ("%s: fail nfa enable; error=0x%X", __FUNCTION__, stat);

    if (sIsNfaEnabled)
    {
        EXTNS_Close();
        stat = NFA_Disable (FALSE /* ungraceful */);
    }

    NfcAdaptInstance.Finalize();

TheEnd:
    NXPLOG_API_D ("%s: nfc enabled = %x", __FUNCTION__, sIsNfaEnabled);
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    gSyncMutex.unlock();
    return sIsNfaEnabled ? 0 : -1;
}

/*******************************************************************************
**
** Function:        nfcManager_doDeinitialize
**
** Description:     Turn off NFC.
**
** Returns:         0 if ok.
**
*******************************************************************************/
INT32 nativeNfcManager_doDeinitialize ()
{
    tNFA_STATUS stat = NFA_STATUS_OK;
    NXPLOG_API_D ("%s: enter", __FUNCTION__);

    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_D ("%s: Nfc not initialized.", __FUNCTION__);
        gSyncMutex.unlock();
        return NFA_STATUS_OK;
    }
    sIsDisabling = true;
    NFA_HciW4eSETransaction_Complete(Wait);

    RoutingManager::getInstance().disableRoutingToHost();
    //Stop the discovery before calling NFA_Disable.
    if(sRfEnabled)
    {
        startRfDiscovery(FALSE);
    }

    if (sIsNfaEnabled)
    {
        SyncEventGuard guard (sNfaDisableEvent);
        EXTNS_Close();
        stat = NFA_Disable (TRUE /* graceful */);
        if (stat == NFA_STATUS_OK)
        {
            NXPLOG_API_D ("%s: wait for completion", __FUNCTION__);
            sNfaDisableEvent.wait (); //wait for NFA command to finish
            nativeNfcSnep_handleNfcOnOff(false);
        }
        else
        {
            NXPLOG_API_D ("%s: fail disable; error=0x%X", __FUNCTION__, stat);
        }
    }
    NfcTag::getInstance ().mNfcDisableinProgress = true;
    nativeNfcTag_abortWaits();
    NfcTag::getInstance().abort ();
    RoutingManager::getInstance().finalize();
    sIsNfaEnabled = false;
    sDiscoveryEnabled = false;
    sIsDisabling = false;
    sPollingEnabled = false;
    gActivated = false;

    {
        //unblock NFA_EnablePolling() and NFA_DisablePolling()
        SyncEventGuard guard (sNfaEnableDisablePollingEvent);
        sNfaEnableDisablePollingEvent.notifyOne ();
    }

    NfcAdaptation& theInstance = NfcAdaptation::GetInstance();
    theInstance.Finalize();

    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    gSyncMutex.unlock();
    return stat;
}

/*******************************************************************************
**
** Function:        nfcManager_enableDiscovery
**
** Description:     Start polling and listening for devices.
** Description:     Start polling and listening for devices.
**                  technologies_mask: the bitmask of technologies for which to enable discovery
**                  reader mode:
**                  enable_host_routing:
**                  restart:
**
** Returns:         0 if ok, error code otherwise
**
*******************************************************************************/
INT32 nativeNfcManager_enableDiscovery (INT32 technologies_mask,
    BOOLEAN reader_mode, INT32 enable_host_routing, BOOLEAN restart)
{
    tNFA_STATUS status = NFA_STATUS_OK;
    tNFA_TECHNOLOGY_MASK tech_mask = DEFAULT_TECH_MASK;
    unsigned long num = 0;
    static UINT8   sProprietaryCmdBuf[]={0xFE,0xFE,0xFE,0x00};

    gSyncMutex.lock();

    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        gSyncMutex.unlock();
        return NFA_STATUS_NOT_INITIALIZED;
    }

    if(sTransaction_data.trans_in_progress)
    {
        NXPLOG_API_D("Transcation is in progress store the requst");
        sTransaction_data.last_request = 1;
        sTransaction_data.discovery_params.technologies_mask = technologies_mask;
        sTransaction_data.discovery_params.reader_mode = reader_mode;
        sTransaction_data.discovery_params.enable_host_routing = enable_host_routing;
        sTransaction_data.discovery_params.restart = restart;
        gSyncMutex.unlock();
        return NFA_STATUS_OK;
    }
    if (technologies_mask == DEFAULT_NFA_TECH_MASK)
    {
        tech_mask = (tNFA_TECHNOLOGY_MASK)sTech_mask;
   }
    else
    {
        tech_mask = sTech_mask = (tNFA_TECHNOLOGY_MASK) technologies_mask;
    }
    NXPLOG_API_D ("%s: enter; tech_mask = %02x", __FUNCTION__, tech_mask);

    if( sDiscoveryEnabled && !restart)
    {
        NXPLOG_API_D ("%s: already discovering", __FUNCTION__);
        gSyncMutex.unlock();
        return NFA_STATUS_ALREADY_STARTED;
    }

    nativeNfcTag_acquireRfInterfaceMutexLock();

    if (sRfEnabled) {
        // Stop RF discovery to reconfigure
        startRfDiscovery(FALSE);
    }

    // Check polling configuration
    if (tech_mask != 0)
    {
        NXPLOG_API_D ("%s: Disable p2pListening", __FUNCTION__);
        enableP2pListening (false);
        stopPolling_rfDiscoveryDisabled();
        startPolling_rfDiscoveryDisabled(tech_mask);

        // Start P2P listening if tag polling was enabled
        if (sPollingEnabled)
        {
            NXPLOG_API_D ("%s: Enable p2pListening", __FUNCTION__);
            enableP2pListening (!reader_mode);

            if (reader_mode && !sReaderModeEnabled)
            {
                sReaderModeEnabled = true;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                NFA_SetReaderMode(true, 0);
                /*Send the state of readmode flag to Hal using proprietary command*/
                sProprietaryCmdBuf[3]=0x01;
                status |= NFA_SendNxpNciCommand(sizeof(sProprietaryCmdBuf),sProprietaryCmdBuf,NxpResponsePropCmd_Cb);
                if (status == NFA_STATUS_OK)
                {
                    SyncEventGuard guard (sNfaNxpNtfEvent);
                    sNfaNxpNtfEvent.wait(500); //wait for callback
                }
                else
                {
                    NXPLOG_API_D ("%s: Failed NFA_SendNxpNciCommand", __FUNCTION__);
                }
                NXPLOG_API_D ("%s: FRM Enable", __FUNCTION__);
#endif
                NFA_PauseP2p();
                NFA_DisableListening();
                NFA_SetRfDiscoveryDuration(READER_MODE_DISCOVERY_DURATION);
            }
            else if (sReaderModeEnabled)
            {
                sReaderModeEnabled = false;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                NFA_SetReaderMode(false, 0);
                //gFelicaReaderState = STATE_IDLE;
                /*Send the state of readmode flag to Hal using proprietary command*/
                sProprietaryCmdBuf[3]=0x00;
                status |= NFA_SendNxpNciCommand(sizeof(sProprietaryCmdBuf),sProprietaryCmdBuf,NxpResponsePropCmd_Cb);
                if (status == NFA_STATUS_OK)
                {
                    SyncEventGuard guard (sNfaNxpNtfEvent);
                    sNfaNxpNtfEvent.wait(500); //wait for callback
                }
                else
                {
                    NXPLOG_API_D ("%s: Failed NFA_SendNxpNciCommand", __FUNCTION__);
                }
                NXPLOG_API_D ("%s: FRM Disable", __FUNCTION__);
#endif
                NFA_ResumeP2p();
                NFA_EnableListening();
                NFA_SetRfDiscoveryDuration(sDiscovery_duration);
            }
            else
            {
 #if(NXP_NFC_NATIVE_ENABLE_HCE ==  TRUE)
                if (enable_host_routing && FLAG_HCE_ENABLE_HCE)
                {
                   NXPLOG_API_D ("Host Card Emulation Enabled");
                   RoutingManager::getInstance().enableRoutingToHost(enable_host_routing & FLAG_HCE_SKIP_NDEF_CHECK);
                }
                else
#endif
                {
                    RoutingManager::getInstance().disableRoutingToHost();
                }
            }
        }
    }
    else
    {
        // No technologies configured, stop polling
        stopPolling_rfDiscoveryDisabled();

 #if(NXP_NFC_NATIVE_ENABLE_HCE ==  TRUE)
        if (enable_host_routing && FLAG_HCE_ENABLE_HCE)
        {
               NXPLOG_API_D ("Host Card Emulation Enabled");
               RoutingManager::getInstance().enableRoutingToHost(enable_host_routing & FLAG_HCE_SKIP_NDEF_CHECK);
        }
        else
#endif
        {
               RoutingManager::getInstance().disableRoutingToHost();
        }
	}

    // Start P2P listening if tag polling was enabled or the mask was 0.
    if (sDiscoveryEnabled || (tech_mask == 0))
    {
        NXPLOG_API_D ("%s: Enable p2pListening", __FUNCTION__);
        enableP2pListening (true);
    }
    // Actually start discovery.
    usleep(100*1000);
    startRfDiscovery (TRUE);
    sDiscoveryEnabled = true;

    nativeNfcTag_releaseRfInterfaceMutexLock();
    gSyncMutex.unlock();
    NXPLOG_API_D ("%s: exit", __FUNCTION__);

    return NFA_STATUS_OK;
}

/*******************************************************************************
**
** Function:        nfcManager_disableDiscovery
**
** Description:     Stop polling and listening for devices.
**
** Returns:         0 if ok, error code otherwise
**
*******************************************************************************/
INT32 nativeNfcManager_disableDiscovery ()
{
    tNFA_STATUS status = NFA_STATUS_OK;

    NXPLOG_API_D ("%s: enter;", __FUNCTION__);

    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        status = NFA_STATUS_NOT_INITIALIZED;
        goto TheEnd;
    }

    if(sTransaction_data.trans_in_progress == TRUE)
    {
        NXPLOG_API_D("Transcatin is in progress store the request");
        sTransaction_data.last_request =2;
        goto TheEnd;
    }
    if (sDiscoveryEnabled == false)
    {
        NXPLOG_API_D ("%s: already disabled", __FUNCTION__);
        status = NFA_STATUS_FAILED;
        goto TheEnd;
    }
    nativeNfcTag_acquireRfInterfaceMutexLock();
    // Stop RF Discovery.
    startRfDiscovery (FALSE);

    if (sPollingEnabled)
    {
        stopPolling_rfDiscoveryDisabled();
    }
    sDiscoveryEnabled = false;
    sReaderModeEnabled = false;
    enableP2pListening (false);

    // We may have had RF field notifications that did not cause
    // any activate/deactive events. For example, caused by wireless
    // charging orbs. Those may cause us to go to sleep while the last
    // field event was indicating a field. To prevent sticking in that
    // state, always reset the rf field status when we disable discovery.
    //SecureElement::getInstance().resetRfFieldStatus();
    nativeNfcTag_releaseRfInterfaceMutexLock();
TheEnd:
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    gSyncMutex.unlock();
    return status;
}

void nativeNfcManager_registerTagCallback(nfcTagCallback_t *nfcTagCb)
{
    gTagCallback = nfcTagCb;
}

void nativeNfcManager_deregisterTagCallback()
{
    gTagCallback = NULL;
}

int nativeNfcManager_selectNextTag()
{
    int status = NFA_STATUS_FAILED;
    if(NfcTag::getInstance ().mNumTags > 1)
    {
        NXPLOG_API_W("%s: Deactivating Selected Tag to Select Next ", __FUNCTION__
                        , NfcTag::getInstance ().mNumTags);

        NFA_Deactivate (TRUE);
        sSelectNext = true;
        status = NFA_STATUS_OK;
    }
    return status;
}

int nativeNfcManager_checkNextProtocol()
{
    return NfcTag::getInstance ().checkNextValidProtocol();
}

int nativeNfcManager_getNumTags()
{
    return NfcTag::getInstance ().mNumTags;
}

void nativeNfcManager_registerHostCallback(nfcHostCardEmulationCallback_t *callback)
{
    RoutingManager::getInstance().registerHostCallback(callback);
}

void nativeNfcManager_deregisterHostCallback()
{
    RoutingManager::getInstance().deregisterHostCallback();
}

/*******************************************************************************
**
** Function:        nfcManagerEnableAGCDebug
**
** Description:     Enable/Disable Dynamic RSSI feature.
**
** Returns:         None
**
*******************************************************************************/
static void nfcManagerEnableAGCDebug(UINT8 connEvent)
{
    unsigned long enableAGCDebug = 0;
    int retvalue = 0xFF;
    GetNxpNumValue (NAME_NXP_AGC_DEBUG_ENABLE, (void*)&enableAGCDebug, sizeof(enableAGCDebug));
    menableAGC_debug_t.enableAGC = enableAGCDebug;
    NXPLOG_API_D ("%s ,%lu:", __FUNCTION__, enableAGCDebug);
    if(sIsNfaEnabled != true || sIsDisabling == true)
        return;
    if(!menableAGC_debug_t.enableAGC)
    {
        NXPLOG_API_D ("%s AGCDebug not enabled", __FUNCTION__);
        return;
    }

    NXPLOG_API_D ("%s connEvent=%d, menableAGC_debug_t.AGCdebugstarted=%d", __FUNCTION__, connEvent, menableAGC_debug_t.AGCdebugrunning);

    if(connEvent == NFA_TRANS_DM_RF_FIELD_EVT &&
       menableAGC_debug_t.AGCdebugrunning == false)
    {
        pthread_t agcThread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        retvalue = pthread_create(&agcThread, &attr, enableAGCThread, NULL);
        pthread_attr_destroy(&attr);
        if(retvalue == 0)
        {
            if(pthread_setname_np(agcThread,"NFC_MANAGER_AGC_TASK"))
            {
                NXPLOG_API_E("pthread_setname_np in %s failed", __FUNCTION__);
            }
            menableAGC_debug_t.AGCdebugrunning = true;
            set_AGC_process_state(true);
        }
    }
}

void *enableAGCThread(void *arg)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    while( menableAGC_debug_t.AGCdebugrunning == true )
    {
        if(get_AGC_process_state() == false)
        {
            sleep(10000);
            continue;
        }
        status = SendAGCDebugCommand();
        if(status == NFA_STATUS_OK)
        {
            NXPLOG_API_D ("%s:  enable success exit", __FUNCTION__);
        }
        usleep(500000);
    }
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    pthread_exit(NULL);
    return NULL;
}
/*******************************************************************************
 **
 ** Function:       set_AGC_process_state
 **
 ** Description:    sets the AGC process to stop
 **
 ** Returns:        None .
 **
 *******************************************************************************/
void set_AGC_process_state(bool state)
{
    menableAGC_debug_t.AGCdebugrunning = state;
}

/*******************************************************************************
 **
 ** Function:       get_AGC_process_state
 **
 ** Description:    returns the AGC process state.
 **
 ** Returns:        true/false .
 **
 *******************************************************************************/
bool get_AGC_process_state()
{
    return menableAGC_debug_t.AGCdebugrunning;
}

/*******************************************************************************
 **
 ** Function:        NxpResponse_EnableAGCDebug_Cb()
 **
 ** Description:     Cb to handle the response of AGC command
 **
 ** Returns:         success/failure
 **
 *******************************************************************************/
static void NxpResponse_EnableAGCDebug_Cb(UINT8 event, UINT16 param_len, UINT8 *p_param)
{
    NXPLOG_API_D("NxpResponse_EnableAGCDebug_Cb Received length data = 0x%x", param_len);
    SetCbStatus(NFA_STATUS_FAILED);
    if(param_len > 0)
    {
        gnxpfeature_conf.rsp_len = param_len;
        memcpy(gnxpfeature_conf.rsp_data, p_param, gnxpfeature_conf.rsp_len);
        SetCbStatus(NFA_STATUS_OK);
    }
    SyncEventGuard guard(gnxpfeature_conf.NxpFeatureConfigEvt);
    gnxpfeature_conf.NxpFeatureConfigEvt.notifyOne ();
}
/*******************************************************************************
 **
 ** Function:        printDataByte()
 **
 ** Description:     Prints the AGC values
 **
 ** Returns:         success/failure
 **
 *******************************************************************************/
static void printDataByte(UINT16 param_len, UINT8 *p_param)
{
    char print_buffer[param_len * 3 + 1];
    memset (print_buffer, 0, sizeof(print_buffer));
    for (int i = 0; i < param_len; i++)
    {
        snprintf(&print_buffer[i * 2], 3 ,"%02X", p_param[i]);
    }
    NXPLOG_API_D("AGC Dynamic RSSI values  = %s", print_buffer);
}
/*******************************************************************************
 **
 ** Function:        SendAGCDebugCommand()
 **
 ** Description:     Sends the AGC Debug command.This enables dynamic RSSI
 **                  look up table filling for different "TX RF settings" and enables
 **                  MWdebug prints.
 **
 ** Returns:         success/failure
 **
 *******************************************************************************/
tNFA_STATUS SendAGCDebugCommand()
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    uint8_t cmd_buf[] = {0x2F, 0x33, 0x04, 0x40, 0x00, 0x40, 0xD8};
    uint8_t dynamic_rssi_buf[50];
    NXPLOG_API_D("%s: enter", __FUNCTION__);
    SetCbStatus(NFA_STATUS_FAILED);
    gnxpfeature_conf.rsp_len = 0;
    memset(gnxpfeature_conf.rsp_data, 0, 50);
    SyncEventGuard guard (gnxpfeature_conf.NxpFeatureConfigEvt);
    status = NFA_SendNxpNciCommand(sizeof(cmd_buf), cmd_buf, NxpResponse_EnableAGCDebug_Cb);
    if (status == NFA_STATUS_OK)
    {
        NXPLOG_API_D ("%s: Success NFA_SendNxpNciCommand", __FUNCTION__);
        gnxpfeature_conf.NxpFeatureConfigEvt.wait(1000); /* wait for callback */
    }
    else
    {    tNFA_STATUS status = NFA_STATUS_FAILED;
        NXPLOG_API_D ("%s: Failed NFA_SendNxpNciCommand", __FUNCTION__);
    }
    status = GetCbStatus();
    if(status == NFA_STATUS_OK && gnxpfeature_conf.rsp_len > 0)
    {
        printDataByte(gnxpfeature_conf.rsp_len, gnxpfeature_conf.rsp_data);
    }
    return status;
}

void SetCbStatus(tNFA_STATUS status)
{
    gnxpfeature_conf.wstatus = status;
}

tNFA_STATUS GetCbStatus(void)
{
    return gnxpfeature_conf.wstatus;
}

void nfcManager_registerT3tIdentifier(UINT8 *t3tId, UINT8 t3tIdsize)
{
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    RoutingManager::getInstance().registerT3tIdentifier(t3tId, t3tIdsize);
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
}

void nfcManager_doDeregisterT3tIdentifier(void)
{
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    RoutingManager::getInstance().deregisterT3tIdentifier();
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
}

