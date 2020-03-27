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

#include <signal.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <malloc.h>

#include "nativeNfcTag.h"
#include "nativeNfcManager.h"
#include "nativeNdef.h"
#include "NfcTag.h"
#include "Mutex.h"
#include "SyncEvent.h"
#include "IntervalTimer.h"
extern "C"
{
    #include "nfa_api.h"
    #include "phNxpLog.h"
    #include "ndef_utils.h"
    #include "phNxpExtns.h"
}

//define a few NXP error codes that NFC service expects;
//see external/libnfc-nxp/src/phLibNfcStatus.h;
//see external/libnfc-nxp/inc/phNfcStatus.h
#define NFCSTATUS_SUCCESS (0x0000)
#define NFCSTATUS_FAILED (0x00FF)

//default general trasceive timeout in millisecond
#define DEFAULT_GENERAL_TRANS_TIMEOUT  2000
#define DEFAULT_PRESENCE_CHECK_MDELAY 125

/*****************************************************************************
**
** public variables and functions
**
*****************************************************************************/
// flag for nfa callback indicating we are deactivating for RF interface switch
BOOLEAN              gIsTagDeactivating = FALSE;
// flag for nfa callback indicating we are selecting for RF interface switch
BOOLEAN              gIsSelectingRfInterface = FALSE;
BOOLEAN              fNeedToSwitchBack = FALSE;
nfcTagCallback_t     *gTagCallback = NULL;
tNFA_INTF_TYPE       sCurrentRfInterface = NFA_INTERFACE_ISO_DEP;

/*****************************************************************************
**
** private variables and functions
**
*****************************************************************************/
static INT32         sGeneralTransceiveTimeout = DEFAULT_GENERAL_TRANS_TIMEOUT;
static BOOLEAN       sConnectOk = FALSE;
static BOOLEAN       sConnectWaitingForComplete = FALSE;
static BOOLEAN       sGotDeactivate = FALSE;
static SyncEvent     sReconnectEvent;
static SyncEvent     sTransceiveEvent;
static SyncEvent     sPresenceCheckEvent;
static SyncEvent     sNfaVSCResponseEvent;
static SyncEvent     sNfaVSCNotificationEvent;
static SyncEvent     sReadEvent;
static BOOLEAN       sIsTagPresent = TRUE;
static BOOLEAN       sPresCheckRequired = TRUE;
static BOOLEAN       sIsTagInField;
static BOOLEAN       sVSCRsp;
static BOOLEAN       sReconnectFlag = FALSE;
static Mutex         sRfInterfaceMutex;
static UINT8         *sRxDataBuffer = NULL;
static UINT32        sRxDataBufferLen = 0;
static UINT32        sRxDataActualSize = -1;
static BOOLEAN       sWaitingForTransceive = FALSE;
static BOOLEAN       sTransceiveRfTimeout = FALSE;
static tNFA_STATUS   sMakeReadonlyStatus = NFA_STATUS_FAILED;
static BOOLEAN       sMakeReadonlyWaitingForComplete = FALSE;
static BOOLEAN       sWriteOk = FALSE;
static BOOLEAN       sWriteWaitingForComplete = FALSE;
static BOOLEAN       sIsReadingNdefMessage = FALSE;
static BOOLEAN       sFormatOk = FALSE;
static sem_t         sMakeReadonlySem;
static sem_t         sFormatSem;
static sem_t         sWriteSem;
static sem_t         sCheckNdefSem;
static UINT32        sCurrentConnectedHandle = NFA_HANDLE_INVALID;
static INT32         sCurrentConnectedTargetType = TARGET_TYPE_UNKNOWN;
static UINT32        sCheckNdefMaxSize = 0;
static BOOLEAN       sIsCheckingNDef = FALSE;
static BOOLEAN       sCheckNdefCardReadOnly = FALSE;
static BOOLEAN       sCheckNdefWaitingForComplete = FALSE;
static UINT32        sCheckNdefCurrentSize = 0;
static tNFA_STATUS   sCheckNdefStatus = 0; //whether tag already contains a NDEF message
static BOOLEAN       sCheckNdefCapable = FALSE; //whether tag has NDEF capability
static IntervalTimer sSwitchBackTimer; // timer used to tell us to switch back to ISO_DEP frame interface
static IntervalTimer sPresenceCheckTimer; // timer used for presence cmd notification timeout.
static IntervalTimer sReconnectNtfTimer ;
static tNFA_HANDLE   sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;

static BOOLEAN       sIsReconnecting = FALSE;
static INT32         doReconnectFlag = 0x00;

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
BOOLEAN              isMifare = FALSE;
static UINT8         key1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static UINT8         key2[] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
static UINT8         Presence_check_TypeB[] =  {0xB2};
#endif

static void nfaVSCCallback(UINT8 event, UINT16 param_len, UINT8 *p_param);
static void nfaVSCNtfCallback(UINT8 event, UINT16 param_len, UINT8 *p_param);
static void presenceCheckTimerProc (union sigval);
static void sReconnectTimerProc(union sigval);
static void *presenceCheckThread(void *arg);
static BOOLEAN doPresenceCheck ();
static BOOLEAN doDisconnect ();
static INT32 reSelect (tNFA_INTF_TYPE rfInterface, BOOLEAN fSwitchIfNeeded);
static BOOLEAN switchRfInterface(tNFA_INTF_TYPE rfInterface);
static inline void setReconnectState(BOOLEAN flag);
static INT32 nativeNfcTag_doReconnect ();

extern BOOLEAN       gActivated;
extern SyncEvent     gDeactivatedEvent;
extern Mutex         gSyncMutex;

void nativeNfcTag_abortWaits();
void nativeNfcTag_resetPresenceCheck();
void nativeNfcTag_releasePresenceCheck();

static void nfaVSCNtfCallback(UINT8 event, UINT16 param_len, UINT8 *p_param)
{
    (void)event;
    NXPLOG_API_D ("%s", __FUNCTION__);
    if(param_len == 4 && p_param[3] == 0x01)
    {
        sIsTagInField = TRUE;
    }
    else
    {
        sIsTagInField = FALSE;
    }

    NXPLOG_API_D ("%s is Tag in Field = %d", __FUNCTION__, sIsTagInField);
    usleep(100*1000);
    SyncEventGuard guard (sNfaVSCNotificationEvent);
    sNfaVSCNotificationEvent.notifyOne ();
}

static void nfaVSCCallback(UINT8 event, UINT16 param_len, UINT8 *p_param)
{
    (void)event;
    NXPLOG_API_D ("%s", __FUNCTION__);
    NXPLOG_API_D ("%s param_len = %d ", __FUNCTION__, param_len);
    NXPLOG_API_D ("%s p_param = %d ", __FUNCTION__, *p_param);

    if(param_len == 4 && p_param[3] == 0x00)
    {
        NXPLOG_API_D ("%s sVSCRsp = TRUE", __FUNCTION__);

        sVSCRsp = TRUE;
    }
    else
    {
        NXPLOG_API_D ("%s sVSCRsp = FALSE", __FUNCTION__);

        sVSCRsp = FALSE;
    }

    NXPLOG_API_D ("%s sVSCRsp = %d", __FUNCTION__, sVSCRsp);


    SyncEventGuard guard (sNfaVSCResponseEvent);
    sNfaVSCResponseEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        presenceCheckTimerProc
**
** Description:     Callback function for presence check timer.
**
** Returns:         None
**
*******************************************************************************/
static void presenceCheckTimerProc (union sigval)
{
    NXPLOG_API_D ("%s", __FUNCTION__);
    sIsTagInField = FALSE;
    sIsReconnecting = FALSE;
    SyncEventGuard guard (sNfaVSCNotificationEvent);
    sNfaVSCNotificationEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        sReconnectTimerProc
**
** Description:     Callback function for reconnect timer.
**
** Returns:         None
**
*******************************************************************************/
static void sReconnectTimerProc (union sigval)
{
    NXPLOG_API_D ("%s", __FUNCTION__);
    SyncEventGuard guard (sNfaVSCNotificationEvent);
    sNfaVSCNotificationEvent.notifyOne ();
}

static inline void setReconnectState(BOOLEAN flag)
{
    sReconnectFlag = flag;
    NXPLOG_API_D("setReconnectState = 0x%x",sReconnectFlag );
}


/*******************************************************************************
**
** Function:        reSelect
**
** Description:     Deactivates the tag and re-selects it with the specified
**                  rf interface.
**
** Returns:         status code, 0 on success, 1 on failure,
**                  146 (defined in service) on tag lost
**
*******************************************************************************/
static INT32 reSelect (tNFA_INTF_TYPE rfInterface, BOOLEAN fSwitchIfNeeded)
{
    UINT32 handle = sCurrentConnectedHandle;
    NXPLOG_API_D ("%s: enter; rf intf = %d, current intf = %d", __FUNCTION__, rfInterface, sCurrentRfInterface);

    sRfInterfaceMutex.lock ();

    if (fSwitchIfNeeded && (rfInterface == sCurrentRfInterface))
    {
        // already in the requested interface
        sRfInterfaceMutex.unlock ();
        return 0;   // success
    }

    NfcTag& natTag = NfcTag::getInstance ();

    tNFA_STATUS status;
    INT32 rVal = 1;

    do
    {
        //if tag has shutdown, abort this method
        if (NfcTag::getInstance ().isNdefDetectionTimedOut())
        {
            NXPLOG_API_D ("%s: ndef detection timeout; break", __FUNCTION__);
            rVal = NFA_STATUS_FAILED;
            break;
        }

        {
            SyncEventGuard g (sReconnectEvent);
            gIsTagDeactivating = TRUE;
            sGotDeactivate = FALSE;
            setReconnectState(FALSE);
            NFA_SetReconnectState(TRUE);
            if (NfcTag::getInstance ().isCashBeeActivated() == TRUE || NfcTag::getInstance ().isEzLinkTagActivated() == TRUE)
            {
                setReconnectState(TRUE);
                /* send deactivate to Idle command */
                NXPLOG_API_D ("%s: deactivate to Idle", __FUNCTION__);
                if (NFA_STATUS_OK != (status = NFA_StopRfDiscovery ())) //deactivate to sleep state
                {
                    NXPLOG_API_E ("%s: deactivate failed, status = %d", __FUNCTION__, status);
                    break;
                }
            }
            else
            {
                NXPLOG_API_D ("%s: deactivate to sleep", __FUNCTION__);
                if (NFA_STATUS_OK != (status = NFA_Deactivate (TRUE))) //deactivate to sleep state
                {
                    NXPLOG_API_E ("%s: deactivate failed, status = %d", __FUNCTION__, status);
                    break;
                }
            }
            if (sReconnectEvent.wait (1000) == FALSE) //if timeout occurred
            {
                NXPLOG_API_E ("%s: timeout waiting for deactivate", __FUNCTION__);
            }
        }

        if(NfcTag::getInstance().getActivationState() == NfcTag::Idle)
        {
            NXPLOG_API_D("%s:tag is in idle", __FUNCTION__);
            if((NfcTag::getInstance().mActivationParams_t.mTechLibNfcTypes == NFC_PROTOCOL_ISO_DEP))
            {
                if(NfcTag::getInstance().mActivationParams_t.mTechParams == NFC_DISCOVERY_TYPE_POLL_A)
                {
                    NfcTag::getInstance ().mCashbeeDetected = TRUE;
                }
                else if(NfcTag::getInstance().mActivationParams_t.mTechParams == NFC_DISCOVERY_TYPE_POLL_B)
                {
                    NfcTag::getInstance ().mEzLinkTypeTag = TRUE;
                }
            }
        }


        if (!(NfcTag::getInstance ().isCashBeeActivated() == TRUE || NfcTag::getInstance ().isEzLinkTagActivated() == TRUE ))
        {
            if (NfcTag::getInstance ().getActivationState () != NfcTag::Sleep)
            {
                NXPLOG_API_D ("%s: tag is not in sleep", __FUNCTION__);
                rVal = NFA_STATUS_FAILED;
                break;
            }
        }
        else
        {
            setReconnectState(FALSE);
        }
        gIsTagDeactivating = FALSE;

        {
            SyncEventGuard g2 (sReconnectEvent);
            gIsSelectingRfInterface = TRUE;
            sConnectWaitingForComplete = TRUE;
            if (NfcTag::getInstance ().isCashBeeActivated() == TRUE || NfcTag::getInstance ().isEzLinkTagActivated() == TRUE)
            {
                setReconnectState(TRUE);
                NXPLOG_API_D ("%s: Discover map cmd", __FUNCTION__);
                if (NFA_STATUS_OK != (status = NFA_StartRfDiscovery ())) //deactivate to sleep state
                {
                    NXPLOG_API_E ("%s: deactivate failed, status = %d", __FUNCTION__, status);
                    break;
                }
            }
            else
            {
                NXPLOG_API_D ("%s: select interface %u", __FUNCTION__, rfInterface);

                if (NFA_STATUS_OK != (status = NFA_Select (natTag.mTechHandles[handle], natTag.mTechLibNfcTypes[handle], rfInterface)))
                {
                    NXPLOG_API_E ("%s: NFA_Select failed, status = %d", __FUNCTION__, status);
                    break;
                }
            }
            sConnectOk = FALSE;
            if (sReconnectEvent.wait (1000) == FALSE) //if timeout occured
            {
                NXPLOG_API_E ("%s: timeout waiting for select", __FUNCTION__);
                status = NFA_Deactivate (FALSE);
                if (status != NFA_STATUS_OK)
                {
                    NXPLOG_API_E ("%s: deactivate failed; error=0x%X", __FUNCTION__, status);
                }
                break;
            }
        }

        NXPLOG_API_D("%s: select completed; sConnectOk=%d", __FUNCTION__, sConnectOk);
        if (NfcTag::getInstance ().getActivationState () != NfcTag::Active)
        {
            NXPLOG_API_D("%s: tag is not active", __FUNCTION__);
            rVal = NFA_STATUS_FAILED;
            break;
        }
        if(NfcTag::getInstance ().isEzLinkTagActivated() == TRUE)
        {
            NfcTag::getInstance ().mEzLinkTypeTag = FALSE;
        }
        if (sConnectOk)
        {
            rVal = 0;   // success
            sCurrentRfInterface = rfInterface;
        }
        else
        {
            rVal = 1;
        }
    } while (0);

    setReconnectState(FALSE);
    NFA_SetReconnectState(FALSE);
    sConnectWaitingForComplete = FALSE;
    gIsTagDeactivating = FALSE;
    gIsSelectingRfInterface = FALSE;
    sRfInterfaceMutex.unlock ();
    NXPLOG_API_D ("%s: exit; status=%d", __FUNCTION__, rVal);
    return rVal;
}

/*******************************************************************************
**
** Function:        switchRfInterface
**
** Description:     Switch controller's RF interface to frame, ISO-DEP, or NFC-DEP.
**                  rfInterface: Type of RF interface.
**
** Returns:         True if ok.
**
*******************************************************************************/
static BOOLEAN switchRfInterface (tNFA_INTF_TYPE rfInterface)
{
    UINT32 handle = sCurrentConnectedHandle;
    NXPLOG_API_D ("%s: rf intf = %d", __FUNCTION__, rfInterface);
    NfcTag& natTag = NfcTag::getInstance ();

    if (natTag.mTechLibNfcTypes[handle] != NFC_PROTOCOL_ISO_DEP)
    {
        NXPLOG_API_D ("%s: protocol: %d not ISO_DEP, do nothing", __FUNCTION__, natTag.mTechLibNfcTypes[handle]);
        return TRUE;
    }

    NXPLOG_API_D ("%s: new rf intf = %d, cur rf intf = %d", __FUNCTION__, rfInterface, sCurrentRfInterface);

    BOOLEAN rVal = TRUE;
    if (rfInterface != sCurrentRfInterface)
    {
        rVal = (0 == reSelect(rfInterface, TRUE));
        if (rVal)
        {
            sCurrentRfInterface = rfInterface;
        }
    }

    return rVal;
}

/*******************************************************************************
 **
 ** Function:       presenceCheckThread
 **
 ** Description:    thread to check if tag is still present
 **
 ** Returns:        None .
 **
 *******************************************************************************/
static void *presenceCheckThread(void *arg)
{
    (void)arg;
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    while(sIsTagPresent)
    {
        gSyncMutex.lock();
        if(sPresCheckRequired)
        {
            sIsTagPresent = doPresenceCheck();
        }
        else
        {
            NXPLOG_API_D("%s: Presence Check - Scheduled", __FUNCTION__);
            sPresCheckRequired = TRUE;
        }
        gSyncMutex.unlock();

        if ((NfcTag::getInstance ().getActivationState () != NfcTag::Active)
              || FALSE == sIsTagPresent)
        {
            NXPLOG_API_D ("%s: Tag Absent/Deactivated.... Exit Check ", __FUNCTION__);
            break;
        }
        else
        {
            SyncEventGuard g (gDeactivatedEvent);
            if(gDeactivatedEvent.wait(DEFAULT_PRESENCE_CHECK_MDELAY))
            {
                NXPLOG_API_D ("%s: Tag Deactivated Event Received.. Exit Presence Check ", __FUNCTION__);
                break;
            }
            else
            {
               NXPLOG_API_D ("%s: Presence Check Re-Scheduled", __FUNCTION__);
            }
        }
    }
    doDisconnect ();

    if(!NfcTag::getInstance().mNfcDisableinProgress)
    {
        if(gTagCallback && (NULL != gTagCallback->onTagDeparture))
        {
            gTagCallback->onTagDeparture();
        }
    }
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    pthread_exit(NULL);
    return NULL;
}

/*******************************************************************************
**
** Function:        doDisconnect
**
** Description:     Deactivate the RF field.
**
** Returns:         True if ok.
**
*******************************************************************************/
static BOOLEAN doDisconnect ()
{
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    tNFA_STATUS nfaStat = NFA_STATUS_OK;

    //reset Ndef states
    sCheckNdefMaxSize = 0;
    sCheckNdefCurrentSize = 0;
    sCheckNdefCardReadOnly = FALSE;
    sCheckNdefCapable = FALSE;

    if (NfcTag::getInstance ().getActivationState () != NfcTag::Active)
    {
        NXPLOG_API_D ("%s: tag already deactivated", __FUNCTION__);
        goto TheEnd;
    }

    nfaStat = NFA_Deactivate (FALSE);
    if (nfaStat != NFA_STATUS_OK)
    {
        NXPLOG_API_E ("%s: deactivate failed; error=0x%X", __FUNCTION__, nfaStat);
    }

TheEnd:
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    return (nfaStat == NFA_STATUS_OK) ? TRUE : FALSE;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doPresenceCheck
**
** Description:     Check if the tag is in the RF field.
**
** Returns:         True if tag is in RF field.
**
*******************************************************************************/
static BOOLEAN doPresenceCheck ()
{
    NXPLOG_API_D ("%s", __FUNCTION__);
    tNFA_STATUS status = NFA_STATUS_OK;
    BOOLEAN isPresent = FALSE;
    UINT8* uid;
    UINT32 uid_len;
    NfcTag::getInstance ().getTypeATagUID(&uid,&uid_len);
    UINT32 handle = sCurrentConnectedHandle;
    NXPLOG_API_D ("%s: sCurrentConnectedHandle= %02X", __FUNCTION__, sCurrentConnectedHandle);

    if(NfcTag::getInstance().mNfcDisableinProgress)
    {
        NXPLOG_API_D("%s, Nfc disable in progress",__FUNCTION__);
        return FALSE;
    }

    if (sIsCheckingNDef == TRUE)
    {
        NXPLOG_API_D("%s: Ndef is being checked", __FUNCTION__);
        return TRUE;
    }

    if (fNeedToSwitchBack)
    {
        sSwitchBackTimer.kill ();
    }

    // Special case for Kovio.  The deactivation would have already occurred
    // but was ignored so that normal tag opertions could complete.  Now we
    // want to process as if the deactivate just happened.
    if (NfcTag::getInstance ().mTechList [handle] == TARGET_TYPE_KOVIO_BARCODE)
    {
        NXPLOG_API_D ("%s: Kovio, force deactivate handling", __FUNCTION__);
        tNFA_DEACTIVATED deactivated = {NFA_DEACTIVATE_TYPE_IDLE};
        {
            SyncEventGuard g (gDeactivatedEvent);
            gActivated = FALSE; //guard this variable from multi-threaded access
            gDeactivatedEvent.notifyOne ();
        }

        NfcTag::getInstance().setDeactivationState (deactivated);
        nativeNfcTag_resetPresenceCheck();
        NfcTag::getInstance().connectionEventHandler (NFA_DEACTIVATED_EVT, NULL);
        nativeNfcTag_abortWaits();
        NfcTag::getInstance().abort ();

        return FALSE;
    }

    if (nativeNfcManager_isNfcActive() == FALSE)
    {
        return FALSE;
    }

    if (!sRfInterfaceMutex.tryLock())
    {
        NXPLOG_API_D ("%s: tag is being reSelected assume it is present", __FUNCTION__);
        return TRUE;
    }

    sRfInterfaceMutex.unlock();

    if (NfcTag::getInstance().isActivated () == FALSE)
    {
        NXPLOG_API_D ("%s: tag already deactivated", __FUNCTION__);
        return FALSE;
    }

    /*
     * This fix is made because NFA_RwPresenceCheck cmd is not woking for ISO-DEP in CEFH mode
     * Hence used the Properitary presence check cmd
     * */

    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_ISO_DEP)
    {
        if (sIsReconnecting == TRUE)
        {
            NXPLOG_API_D("%s: Reconnecting Tag", __FUNCTION__);
            return TRUE;
        }
        NXPLOG_API_D ("%s: presence check for TypeB / TypeA random uid", __FUNCTION__);
        sPresenceCheckTimer.set(500, presenceCheckTimerProc);

        tNFC_STATUS stat = NFA_RegVSCback (TRUE,nfaVSCNtfCallback); //Register CallBack for VS NTF
        if(NFA_STATUS_OK != stat)
        {
            goto TheEnd;
        }

        SyncEventGuard guard (sNfaVSCResponseEvent);
        stat = NFA_SendVsCommand (0x11,0x00,NULL,nfaVSCCallback);
        if(NFA_STATUS_OK == stat)
        {
            NXPLOG_API_D ("%s: presence check for TypeB - wait for NFA VS RSP to come", __FUNCTION__);
            sNfaVSCResponseEvent.wait(); //wait for NFA VS command to finish
            NXPLOG_API_D ("%s: presence check for TypeB - GOT NFA VS RSP", __FUNCTION__);
        }

        if(TRUE == sVSCRsp)
        {
            {
                SyncEventGuard guard (sNfaVSCNotificationEvent);
                NXPLOG_API_D ("%s: presence check for TypeB - wait for NFA VS NTF to come", __FUNCTION__);
                sNfaVSCNotificationEvent.wait(); //wait for NFA VS NTF to come
                NXPLOG_API_D ("%s: presence check for TypeB - GOT NFA VS NTF", __FUNCTION__);
                sPresenceCheckTimer.kill();
            }

            if(FALSE == sIsTagInField)
            {
                isPresent = FALSE;
            }
            else
            {
                isPresent =  TRUE;
            }
        }

        NFA_RegVSCback (FALSE,nfaVSCNtfCallback); //DeRegister CallBack for VS NTF
        NXPLOG_API_D ("%s: presence check for TypeB - return", __FUNCTION__);
        goto TheEnd;
    }

#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_T3BT)
    {
        UINT8 *pbuf = NULL;
        UINT8 bufLen = 0x00;
        BOOLEAN waitOk = FALSE;

        SyncEventGuard g (sTransceiveEvent);
        sTransceiveRfTimeout = FALSE;
        sWaitingForTransceive = TRUE;
        //sTransceiveDataLen = 0;
        bufLen = (UINT8) sizeof(Presence_check_TypeB);
        pbuf = Presence_check_TypeB;
        //memcpy(pbuf, Attrib_cmd_TypeB, bufLen);
        status = NFA_SendRawFrame (pbuf, bufLen,NFA_DM_DEFAULT_PRESENCE_CHECK_START_DELAY);
        if (status != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: fail send; error=%d", __FUNCTION__, status);
        }
        else
            waitOk = sTransceiveEvent.wait (sGeneralTransceiveTimeout);

        if (waitOk == FALSE || sTransceiveRfTimeout) //if timeout occurred
        {
            return FALSE;;
        }
        else
        {
            return TRUE;
        }
    }
#endif

    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
    {
        NXPLOG_API_D ("Calling EXTNS_MfcPresenceCheck");
        status = EXTNS_MfcPresenceCheck();
        if (status == NFCSTATUS_SUCCESS)
        {
            if (NFCSTATUS_SUCCESS == EXTNS_GetPresenceCheckStatus())
            {
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }

    {
        SyncEventGuard guard (sPresenceCheckEvent);
        status = NFA_RwPresenceCheck (NfcTag::getInstance().getPresenceCheckAlgorithm());
        if (status == NFA_STATUS_OK)
        {
            NXPLOG_API_D ("%s: NFA_RwPresenceCheck Wait.. ", __FUNCTION__);
            sPresenceCheckEvent.wait ();
            isPresent = sIsTagPresent ? TRUE : FALSE;
        }
    }

    TheEnd:

    if (isPresent == FALSE)
    {
        NXPLOG_API_D ("%s: tag absent", __FUNCTION__);
    }
    return isPresent;
}

/*******************************************************************************
**
** Function:        ndefHandlerCallback
**
** Description:     Receive NDEF-message related events from stack.
**                  event: Event code.
**                  p_data: Event data.
**
** Returns:         None
**
*******************************************************************************/
static void ndefHandlerCallback (tNFA_NDEF_EVT event, tNFA_NDEF_EVT_DATA *eventData)
{
    NXPLOG_API_D ("%s: event=%u, eventData=%p", __FUNCTION__, event, eventData);

    switch (event)
    {
        case NFA_NDEF_REGISTER_EVT:
        {
            tNFA_NDEF_REGISTER& ndef_reg = eventData->ndef_reg;
            NXPLOG_API_D ("%s: NFA_NDEF_REGISTER_EVT; status=0x%X; h=0x%X", __FUNCTION__, ndef_reg.status, ndef_reg.ndef_type_handle);
            sNdefTypeHandlerHandle = ndef_reg.ndef_type_handle;
        }
        break;

        case NFA_NDEF_DATA_EVT:
        {
            NXPLOG_API_D ("%s: NFA_NDEF_DATA_EVT; data_len = %lu", __FUNCTION__, eventData->ndef_data.len);
            sRxDataActualSize = eventData->ndef_data.len;
            if (sRxDataBufferLen >= sRxDataActualSize)
            {
                memcpy (sRxDataBuffer, eventData->ndef_data.p_data, eventData->ndef_data.len);
            }
            else
            {
                sRxDataActualSize = -1;
            }
        }
        break;

        default:
        {
            NXPLOG_API_E ("%s: Unknown event %u ????", __FUNCTION__, event);
            break;
        }
    }
}

/*******************************************************************************
**
** Function:        nativeNfcTag_registerNdefTypeHandler
**
** Description:     Register a callback to receive NDEF message from the tag
**                  from the NFA_NDEF_DATA_EVT.
**
** Returns:         None
**
*******************************************************************************/
//register a callback to receive NDEF message from the tag
//from the NFA_NDEF_DATA_EVT;
void nativeNfcTag_registerNdefTypeHandler ()
{
    NXPLOG_API_D ("%s", __FUNCTION__);
    sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
    NFA_RegisterNDefTypeHandler (TRUE, NFA_TNF_DEFAULT, (UINT8 *) "", 0, ndefHandlerCallback);
    EXTNS_MfcRegisterNDefTypeHandler(ndefHandlerCallback);
}


/*******************************************************************************
**
** Function:        nativeNfcTag_deregisterNdefTypeHandler
**
** Description:     No longer need to receive NDEF message from the tag.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_deregisterNdefTypeHandler ()
{
    NXPLOG_API_D ("%s", __FUNCTION__);
    NFA_DeregisterNDefTypeHandler (sNdefTypeHandlerHandle);
    sNdefTypeHandlerHandle = NFA_HANDLE_INVALID;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doWriteStatus
**
** Description:     Receive the completion status of write operation.  Called
**                  by NFA_WRITE_CPLT_EVT.
**                  isWriteOk: Status of operation.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doWriteStatus (BOOLEAN isWriteOk)
{
    if (sWriteWaitingForComplete != FALSE)
    {
        sWriteWaitingForComplete = FALSE;
        sWriteOk = isWriteOk;
        sem_post (&sWriteSem);
    }
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doReadCompleted
**
** Description:     Receive the completion status of read operation.  Called by
**                  NFA_READ_CPLT_EVT.
**                  status: Status of operation.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doReadCompleted (tNFA_STATUS status)
{
    NXPLOG_API_D ("%s: status=0x%X; is reading=%u", __FUNCTION__, status, sIsReadingNdefMessage);

    if (sIsReadingNdefMessage == FALSE)
        return; //not reading NDEF message right now, so just return

    if (status != NFA_STATUS_OK)
    {
        sRxDataActualSize = -1;
    }
    SyncEventGuard g (sReadEvent);
    sReadEvent.notifyOne ();
}


/*******************************************************************************
**
** Function:        nativeNfcTag_doTransceiveStatus
**
** Description:     Receive the completion status of transceive operation.
**                  status: operation status.
**                  buf: Contains tag's response.
**                  bufLen: Length of buffer.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doTransceiveStatus (tNFA_STATUS status, UINT8* buf, UINT32 bufLen)
{
    UINT32 handle = sCurrentConnectedHandle;

    sPresCheckRequired = FALSE;
    
    SyncEventGuard g (sTransceiveEvent);
    NXPLOG_API_D ("%s: data len=%d", __FUNCTION__, bufLen);
    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
    {
       if (EXTNS_GetCallBackFlag() == FALSE)
       {
           EXTNS_MfcCallBack(buf, (uint32_t)bufLen);
           return;
       }
    }

    if (!sWaitingForTransceive)
    {
        NXPLOG_API_E ("%s: drop data", __FUNCTION__);
        return;
    }
//    sRxDataStatus = status;
    if (status == NFA_STATUS_OK || status == NFA_STATUS_CONTINUE)
    {
        if (sRxDataActualSize + bufLen <= sRxDataBufferLen)
        {
            memcpy(sRxDataBuffer + sRxDataActualSize, buf, bufLen);
            sRxDataActualSize += bufLen;
        }
        else
        {
            //response size overflow
            sRxDataActualSize = 0;//TODO: define return value for overflow;
            sRxDataBufferLen = 0;
        }
    }
    if (status == NFA_STATUS_OK)
        sTransceiveEvent.notifyOne ();
}

void nativeNfcTag_notifyRfTimeout ()
{
    SyncEventGuard g (sTransceiveEvent);
    NXPLOG_API_D ("%s: waiting for transceive: %d", __FUNCTION__, sWaitingForTransceive);
    if (!sWaitingForTransceive)
        return;

    sTransceiveRfTimeout = TRUE;

    sTransceiveEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        nativeNfcTag_formatStatus
**
** Description:     Receive the completion status of format operation.  Called
**                  by NFA_FORMAT_CPLT_EVT.
**                  isOk: Status of operation.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_formatStatus (BOOLEAN isOk)
{
    sFormatOk = isOk;
    sem_post (&sFormatSem);
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doMakeReadonlyResult
**
** Description:     Receive the result of making a tag read-only. Called by the
**                  NFA_SET_TAG_RO_EVT.
**                  status: Status of the operation.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doMakeReadonlyResult (tNFA_STATUS status)
{
    if (sMakeReadonlyWaitingForComplete != FALSE)
    {
        sMakeReadonlyWaitingForComplete = FALSE;
        sMakeReadonlyStatus = status;

        sem_post (&sMakeReadonlySem);
    }
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doDeactivateStatus
**
** Description:     Receive the completion status of deactivate operation.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doDeactivateStatus (INT32 status)
{
    if(EXTNS_GetDeactivateFlag() == TRUE)
    {
        EXTNS_MfcDisconnect();
        EXTNS_SetDeactivateFlag(FALSE);
        return;
    }

    sGotDeactivate = (status == 0);

    SyncEventGuard g (sReconnectEvent);
    sReconnectEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doReconnect
**
** Description:     Re-connect to the tag in RF field.
**
** Returns:         Status code.
**
*******************************************************************************/
static INT32 nativeNfcTag_doReconnect ()
{
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    INT32 retCode = NFA_STATUS_OK;
    NfcTag& natTag = NfcTag::getInstance ();
    UINT32 handle = sCurrentConnectedHandle;

    UINT8* uid;
    UINT32 uid_len;
    NXPLOG_API_D ("%s: enter; handle=%x", __FUNCTION__, handle);
    natTag.getTypeATagUID(&uid,&uid_len);

    if(natTag.mNfcDisableinProgress)
    {
        NXPLOG_API_E ("%s: NFC disabling in progress", __FUNCTION__);
        retCode = NFA_STATUS_FAILED;
        goto TheEnd;
    }

    if (natTag.getActivationState() != NfcTag::Active)
    {
        NXPLOG_API_E ("%s: tag already deactivated", __FUNCTION__);
        retCode = NFA_STATUS_FAILED;
        goto TheEnd;
    }

    // special case for Kovio
    if (NfcTag::getInstance ().mTechList [handle] == TARGET_TYPE_KOVIO_BARCODE)
    {
        NXPLOG_API_D ("%s: fake out reconnect for Kovio", __FUNCTION__);
        goto TheEnd;
    }

    //special case for TypeB and TypeA random UID
    if ( (sCurrentRfInterface != NCI_INTERFACE_FRAME) &&
            ((natTag.mTechLibNfcTypes[handle] == NFA_PROTOCOL_ISO_DEP &&
            TRUE == natTag.isTypeBTag() ) ||
            ( NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_ISO_DEP &&
                    uid_len > 0 && uid[0] == 0x08))
         )
    {
        NXPLOG_API_D ("%s: reconnect for TypeB / TypeA random uid", __FUNCTION__);
        sReconnectNtfTimer.set(500, sReconnectTimerProc);

        tNFC_STATUS stat = NFA_RegVSCback (TRUE,nfaVSCNtfCallback); //Register CallBack for VS NTF
        if(NFA_STATUS_OK != stat)
        {
            retCode = 0x01;
            goto TheEnd;
        }

        SyncEventGuard guard (sNfaVSCResponseEvent);
        stat = NFA_SendVsCommand (0x11,0x00,NULL,nfaVSCCallback);
        if(NFA_STATUS_OK == stat)
        {
            sIsReconnecting = TRUE;
            NXPLOG_API_D ("%s: reconnect for TypeB - wait for NFA VS command to finish", __FUNCTION__);
            sNfaVSCResponseEvent.wait(); //wait for NFA VS command to finish
            NXPLOG_API_D ("%s: reconnect for TypeB - Got RSP", __FUNCTION__);
        }

        if(FALSE == sVSCRsp)
        {
            retCode = 0x01;
            sIsReconnecting = FALSE;
        }
        else
        {
            {
                NXPLOG_API_D ("%s: reconnect for TypeB - wait for NFA VS NTF to come", __FUNCTION__);
                SyncEventGuard guard (sNfaVSCNotificationEvent);
                sNfaVSCNotificationEvent.wait(); //wait for NFA VS NTF to come
                NXPLOG_API_D ("%s: reconnect for TypeB - GOT NFA VS NTF", __FUNCTION__);
                sReconnectNtfTimer.kill();
                sIsReconnecting = FALSE;
            }

            if(FALSE == sIsTagInField)
            {
                NXPLOG_API_D ("%s: NxpNci: TAG OUT OF FIELD", __FUNCTION__);
                retCode = NFA_STATUS_FAILED;

                SyncEventGuard g (gDeactivatedEvent);

                //Tag not present, deactivate the TAG.
                stat = NFA_Deactivate (FALSE);
                if (stat == NFA_STATUS_OK)
                {
                    gDeactivatedEvent.wait ();
                }
                else
                {
                    NXPLOG_API_E ("%s: deactivate failed; error=0x%X", __FUNCTION__, stat);
                }
            }

            else
            {
                retCode = 0x00;
            }
        }

        stat = NFA_RegVSCback (FALSE,nfaVSCNtfCallback); //DeRegister CallBack for VS NTF
        if(NFA_STATUS_OK != stat)
        {
            retCode = 0x01;
        }
        NXPLOG_API_D ("%s: reconnect for TypeB - return", __FUNCTION__);

        goto TheEnd;
    }
     // this is only supported for type 2 or 4 (ISO_DEP) tags
    if (natTag.mTechLibNfcTypes[handle] == NFA_PROTOCOL_ISO_DEP)
        retCode = reSelect(NFA_INTERFACE_ISO_DEP, FALSE);
    else if (natTag.mTechLibNfcTypes[handle] == NFA_PROTOCOL_T2T)
        retCode = reSelect(NFA_INTERFACE_FRAME, FALSE);
    else if (natTag.mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
        retCode = reSelect(NFA_INTERFACE_MIFARE, FALSE);

TheEnd:
    NXPLOG_API_D ("%s: exit 0x%X", __FUNCTION__, retCode);
    return retCode;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doConnectStatus
**
** Description:     Receive the completion status of connect operation.
**                  isConnectOk: Status of the operation.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doConnectStatus (BOOLEAN isConnectOk)
{
    if (EXTNS_GetConnectFlag() == TRUE)
    {
       // INT32 status = (isConnectOk == FALSE)?0xFF:0x00; /*commented to eliminate unused variable warning*/
        EXTNS_MfcActivated();
        EXTNS_SetConnectFlag (FALSE);
        return;
    }
    if (sConnectWaitingForComplete != FALSE)
    {
        sConnectWaitingForComplete = FALSE;
        sConnectOk = isConnectOk;
        SyncEventGuard g (sReconnectEvent);
        sReconnectEvent.notifyOne ();
    }
}

void nativeNfcTag_setReconnectState(BOOLEAN flag)
{
    sReconnectFlag = flag;
    NXPLOG_API_D("nativeNfcTag_setReconnectState = 0x%x",sReconnectFlag );
}

BOOLEAN nativeNfcTag_getReconnectState(void)
{
    NXPLOG_API_D("nativeNfcTag_getReconnectState = 0x%x",sReconnectFlag );
    return sReconnectFlag;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_abortWaits
**
** Description:     Unblock all thread synchronization objects.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_abortWaits ()
{
    NXPLOG_API_D ("%s", __FUNCTION__);
    {
        SyncEventGuard g (sReadEvent);
        sReadEvent.notifyOne ();
    }
    sem_post (&sWriteSem);
    sem_post (&sFormatSem);
    {
        SyncEventGuard g (sTransceiveEvent);
        sTransceiveEvent.notifyOne ();
    }
    {
        SyncEventGuard g (sReconnectEvent);
        sReconnectEvent.notifyOne ();
    }

    sem_post (&sCheckNdefSem);
    {
        SyncEventGuard guard (sPresenceCheckEvent);
        sPresenceCheckEvent.notifyOne ();
    }
    sem_post (&sMakeReadonlySem);
    sCurrentRfInterface = NFA_INTERFACE_ISO_DEP;
    sCurrentConnectedTargetType = TARGET_TYPE_UNKNOWN;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_resetPresenceCheck
**
** Description:     Reset variables related to presence-check.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_resetPresenceCheck ()
{
    sIsTagPresent = TRUE;
    NfcTag::getInstance ().mCashbeeDetected = FALSE;
    NfcTag::getInstance ().mEzLinkTypeTag = FALSE;
    //MfcResetPresenceCheckStatus();
}

/*******************************************************************************
**
** Function:        nativeNfcTag_releasePresenceCheck
**
** Description:     Reset variables related to presence-check.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_releasePresenceCheck ()
{
    SyncEventGuard guard (sPresenceCheckEvent);
    sIsTagPresent = FALSE;
    sPresenceCheckEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doPresenceCheckResult
**
** Description:     Receive the result of presence-check.
**                  status: Result of presence-check.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doPresenceCheckResult (tNFA_STATUS status)
{
    SyncEventGuard guard (sPresenceCheckEvent);
    sIsTagPresent = status == NFA_STATUS_OK;
    sPresenceCheckEvent.notifyOne ();
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doCheckNdefResult
**
** Description:     Receive the result of checking whether the tag contains a NDEF
**                  message.  Called by the NFA_NDEF_DETECT_EVT.
**                  status: Status of the operation.
**                  maxSize: Maximum size of NDEF message.
**                  currentSize: Current size of NDEF message.
**                  flags: Indicate various states.
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_doCheckNdefResult (tNFA_STATUS status, UINT32 maxSize, UINT32 currentSize, UINT8 flags)
{
    //this function's flags parameter is defined using the following macros
    //in nfc/include/rw_api.h;
    //#define RW_NDEF_FL_READ_ONLY  0x01    /* Tag is read only              */
    //#define RW_NDEF_FL_FORMATED   0x02    /* Tag formated for NDEF         */
    //#define RW_NDEF_FL_SUPPORTED  0x04    /* NDEF supported by the tag     */
    //#define RW_NDEF_FL_UNKNOWN    0x08    /* Unable to find if tag is ndef capable/formated/read only */
    //#define RW_NDEF_FL_FORMATABLE 0x10    /* Tag supports format operation */

    if (!sCheckNdefWaitingForComplete)
    {
        NXPLOG_API_E ("%s: not waiting", __FUNCTION__);
        return;
    }

    if (flags & RW_NDEF_FL_READ_ONLY)
    {
        NXPLOG_API_D ("%s: flag read-only", __FUNCTION__);
    }
    if (flags & RW_NDEF_FL_FORMATED)
    {
        NXPLOG_API_D ("%s: flag formatted for ndef", __FUNCTION__);
    }
    if (flags & RW_NDEF_FL_SUPPORTED)
    {
        NXPLOG_API_D ("%s: flag ndef supported", __FUNCTION__);
    }
    if (flags & RW_NDEF_FL_UNKNOWN)
    {
        NXPLOG_API_D ("%s: flag all unknown", __FUNCTION__);
    }
    if (flags & RW_NDEF_FL_FORMATABLE)
    {
        NXPLOG_API_D ("%s: flag formattable", __FUNCTION__);
    }

    sCheckNdefWaitingForComplete = FALSE;
    sCheckNdefStatus = status;
    if (sCheckNdefStatus != NFA_STATUS_OK && sCheckNdefStatus != NFA_STATUS_TIMEOUT)
        sCheckNdefStatus = NFA_STATUS_FAILED;
    sCheckNdefCapable = FALSE; //assume tag is NOT ndef capable
    if (sCheckNdefStatus == NFA_STATUS_OK)
    {
        //NDEF content is on the tag
        sCheckNdefMaxSize = maxSize;
        sCheckNdefCurrentSize = currentSize;
        sCheckNdefCardReadOnly = flags & RW_NDEF_FL_READ_ONLY;
        sCheckNdefCapable = TRUE;
    }
    else if (sCheckNdefStatus == NFA_STATUS_FAILED)
    {
        //no NDEF content on the tag
        sCheckNdefMaxSize = 0;
        sCheckNdefCurrentSize = 0;
        sCheckNdefCardReadOnly = flags & RW_NDEF_FL_READ_ONLY;
#if (defined(NDEF_WRITE_ON_NON_NDEF_TAG) && (NDEF_WRITE_ON_NON_NDEF_TAG == 1))
        /* Ignore Below to Avoid Formatting the card while Write NDEF */
        /* if stack understands the tag */
        if ((flags & RW_NDEF_FL_UNKNOWN) == 0)
        {
             /* if tag is NDEF capable */
            if (flags & RW_NDEF_FL_SUPPORTED)
            {
                sCheckNdefCapable = TRUE;
                sCheckNdefMaxSize = maxSize;
            }
        }
#endif /* (defined(NDEF_WRITE_ON_NON_NDEF_TAG) && (NDEF_WRITE_ON_NON_NDEF_TAG == 1)) */
    }
    else
    {
        if (sCheckNdefStatus == NFA_STATUS_TIMEOUT)
        {
            NXPLOG_API_D ("%s: Tag is lost, set state to deactivated", __FUNCTION__);
            doDisconnect ();
        }

        NXPLOG_API_E ("%s: unknown status=0x%X", __FUNCTION__, status);
        sCheckNdefMaxSize = 0;
        sCheckNdefCurrentSize = 0;
        sCheckNdefCardReadOnly = FALSE;
    }
    sem_post (&sCheckNdefSem);
}

/*******************************************************************************
**
** Function:        acquireRfInterfaceMutexLock
**
** Description:     acquire lock
**
** Returns:         None
**
*******************************************************************************/
void nativeNfcTag_acquireRfInterfaceMutexLock()
{
    NXPLOG_API_D ("%s: try to acquire lock", __FUNCTION__);
    sRfInterfaceMutex.lock();
    NXPLOG_API_D ("%s: sRfInterfaceMutex lock", __FUNCTION__);
}


/*******************************************************************************
**
** Function:       releaseRfInterfaceMutexLock
**
** Description:    release the lock
**
** Returns:        None
**
*******************************************************************************/
void nativeNfcTag_releaseRfInterfaceMutexLock()
{
    sRfInterfaceMutex.unlock();
    NXPLOG_API_D ("%s: sRfInterfaceMutex unlock", __FUNCTION__);
}


void nativeNfcTag_onTagArrival(nfc_tag_info_t *tag)
{
    pthread_t presenceCheck_thread;
    INT32 ret = -1;
    tNFA_STATUS nfaStat = NFA_STATUS_OK;

    sCurrentConnectedHandle = tag->handle;
    sCurrentConnectedTargetType = tag->technology;
    if(!NfcTag::getInstance().mNfcDisableinProgress)
    {
        if(gTagCallback && (NULL != gTagCallback->onTagArrival))
        {
            NXPLOG_API_D ("%s: notify tag is ready", __FUNCTION__);
            gTagCallback->onTagArrival(tag);
        }
    }

    /* start presence check thread */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&presenceCheck_thread, &attr, presenceCheckThread, NULL);
    if(ret != 0)
    {
        NXPLOG_API_E("Unable to create the thread");
        nfaStat = NFA_Deactivate (FALSE);
        if (nfaStat == NFA_STATUS_OK)
        {
            gDeactivatedEvent.wait ();
        }
        else
        {
            NXPLOG_API_E ("%s: deactivate failed; error=0x%X", __FUNCTION__, nfaStat);
        }
    }
    else
    {
        if(pthread_setname_np(presenceCheck_thread,"PRXMTY_CHK_TSK"))
        {
        	NXPLOG_API_E("pthread_setname_np in %s failed", __FUNCTION__);
        }
    }
}

/*****************************************************************************
**
** public functions
**
*****************************************************************************/

/*******************************************************************************
**
** Function:        nativeNfcTag_checkNdef
**
** Description:     Does the tag contain a NDEF message?
**                      tagHandle: handle of the tag
**                      maxNdefLength: for return, max NDef message length
**                      isWritable: for return, if write new NDef is allowed
**
** Returns:         TRUE if has NDEF message.
**
*******************************************************************************/
BOOLEAN nativeNfcTag_checkNdef(UINT32 tagHandle, ndef_info_t *info)
{
    tNFA_STATUS status = NFA_STATUS_FAILED;
    sIsCheckingNDef = TRUE;
    NXPLOG_API_D ("%s: enter; handle=%x", __FUNCTION__, tagHandle);

    if (tagHandle != sCurrentConnectedHandle)
    {
        NXPLOG_API_E ("%s: Invalid handle. ", __FUNCTION__);
        return FALSE;
    }
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if (NfcTag::getInstance ().mTechLibNfcTypes[tagHandle] == NFA_PROTOCOL_T3BT)
    {
        sIsCheckingNDef = FALSE;
        return FALSE;
    }
#endif

    // special case for Kovio
    if (NfcTag::getInstance ().mTechList [tagHandle] == TARGET_TYPE_KOVIO_BARCODE)
    {
        NXPLOG_API_D ("%s: Kovio tag, no NDEF", __FUNCTION__);
        sIsCheckingNDef = FALSE;
        return FALSE;
    }
    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        gSyncMutex.unlock();
        return FALSE;
    }

    if (info != NULL)
    {
        memset(info, 0, sizeof(ndef_info_t));
    }
    if (NfcTag::getInstance ().mTechLibNfcTypes[tagHandle] == NFA_PROTOCOL_MIFARE)
    {
        nativeNfcTag_doReconnect ();
    }

    doReconnectFlag = 0;

    /* Create the write semaphore */
    if (sem_init (&sCheckNdefSem, 0, 0) == -1)
    {
        NXPLOG_API_E ("%s: Check NDEF semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
        sIsCheckingNDef = FALSE;
        gSyncMutex.unlock();
        return FALSE;
    }

    if (NfcTag::getInstance ().getActivationState () != NfcTag::Active)
    {
        NXPLOG_API_E ("%s: tag already deactivated", __FUNCTION__);
        goto TheEnd;
    }

    NXPLOG_API_D ("%s: try NFA_RwDetectNDef", __FUNCTION__);
    sCheckNdefWaitingForComplete = TRUE;

    NXPLOG_API_D ("%s: NfcTag::getInstance ().mTechLibNfcTypes[%d]=%x", __FUNCTION__, tagHandle, NfcTag::getInstance ().mTechLibNfcTypes[tagHandle]);

    if (NfcTag::getInstance ().mTechLibNfcTypes[tagHandle] == NFA_PROTOCOL_MIFARE)
    {
        status = EXTNS_MfcCheckNDef ();
    }
    else
    {
        status = NFA_RwDetectNDef ();
    }

    if (status != NFA_STATUS_OK)
    {
        NXPLOG_API_E ("%s: NFA_RwDetectNDef failed, status = 0x%X", __FUNCTION__, status);
        goto TheEnd;
    }

    /* Wait for check NDEF completion status */
    if (sem_wait (&sCheckNdefSem))
    {
        NXPLOG_API_E ("%s: Failed to wait for check NDEF semaphore (errno=0x%08x)", __FUNCTION__, errno);
        goto TheEnd;
    }

    if (sCheckNdefStatus == NFA_STATUS_OK)
    {
        //stack found a NDEF message on the tag
        if (NfcTag::getInstance ().getProtocol () == NFA_PROTOCOL_T1T)
        {
            sCheckNdefMaxSize = NfcTag::getInstance ().getT1tMaxMessageSize ();
        }
        if (info != NULL)
        {
            info->is_ndef = TRUE;
            info->is_writable = sCheckNdefCardReadOnly ? FALSE : TRUE;
            info->current_ndef_length = sCheckNdefCurrentSize;
            info->max_ndef_length = sCheckNdefMaxSize;
        }
        status = NFA_STATUS_OK;
    }
    else if (sCheckNdefStatus == NFA_STATUS_FAILED)
    {
        //stack did not find a NDEF message on the tag;
        if (NfcTag::getInstance ().getProtocol () == NFA_PROTOCOL_T1T)
        {
            sCheckNdefMaxSize = NfcTag::getInstance ().getT1tMaxMessageSize ();
        }
        if (info != NULL)
        {
            info->is_ndef = FALSE;
            info->is_writable = sCheckNdefCardReadOnly ? FALSE : TRUE;
            info->current_ndef_length = 0;
            info->max_ndef_length = sCheckNdefMaxSize;
        }
        status = NFA_STATUS_FAILED;
    }
    else if ((sCheckNdefStatus == NFA_STATUS_TIMEOUT) && (NfcTag::getInstance ().getProtocol() == NFC_PROTOCOL_ISO_DEP))
    {
        //pn544InteropStopPolling ();
        status = sCheckNdefStatus;
    }
    else
    {
        NXPLOG_API_D ("%s: unknown status 0x%X", __FUNCTION__, sCheckNdefStatus);
        status = sCheckNdefStatus;
    }

TheEnd:
    /* Destroy semaphore */
    if (sem_destroy (&sCheckNdefSem))
    {
        NXPLOG_API_E ("%s: Failed to destroy check NDEF semaphore (errno=0x%08x)", __FUNCTION__, errno);
    }
    sCheckNdefWaitingForComplete = FALSE;
    sIsCheckingNDef = FALSE;
    gSyncMutex.unlock();
    NXPLOG_API_D ("%s: exit; status=0x%X", __FUNCTION__, status);
    return (status == NFA_STATUS_OK) ? TRUE : FALSE;
}

/*******************************************************************************
**
** Function:        readNdef
**
** Description:     Read the NDEF message on the tag.
**                  tagHandle: tag handle.
**
** Returns:         NDEF message.
**
*******************************************************************************/
INT32 nativeNfcTag_doReadNdef(UINT32 tagHandle, UINT8* ndefBuffer,  UINT32 ndefBufferLength, nfc_friendly_type_t *friendly_type)
{
    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    tNFA_STATUS status = NFA_STATUS_OK;
    UINT32 handle = sCurrentConnectedHandle;
    BOOLEAN isNdef = FALSE;
    UINT8 ndef_tnf;
    UINT8 *ndef_type;
    UINT8 ndef_typeLength;

    if (tagHandle != sCurrentConnectedHandle)
    {
        NXPLOG_API_E ("%s: Wrong tag handle!\n)", __FUNCTION__);
        return 0;
    }

    if (ndefBuffer == NULL || ndefBufferLength <= 0)
    {
        NXPLOG_API_E ("%s: invalide buffer!", __FUNCTION__);
        return -1;
    }
    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        goto End;
    }

    if (sRxDataBuffer != NULL)
    {
        NXPLOG_API_E ("%s: !!!! sRxDataBuffer must be NULL!", __FUNCTION__);
    }

    sRxDataBuffer = ndefBuffer;
    sRxDataBufferLen = ndefBufferLength;
    sRxDataActualSize = 0;

    if (sCheckNdefCurrentSize > 0)
    {
        {
            SyncEventGuard g (sReadEvent);
            sIsReadingNdefMessage = TRUE;
            if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
            {
                status = EXTNS_MfcReadNDef();
            }
            else
            {
                status = NFA_RwReadNDef ();
            }
            sReadEvent.wait (); //wait for NFA_READ_CPLT_EVT
        }
        sIsReadingNdefMessage = FALSE;

        if (sRxDataBufferLen > 0) //if stack actually read data from the tag
        {
            NXPLOG_API_D ("%s: read %u bytes", __FUNCTION__, sRxDataBufferLen);
            status = NFA_STATUS_OK;
            isNdef = TRUE;
        }
    }
    else
    {
        NXPLOG_API_D ("%s: no Ndef message", __FUNCTION__);
        status = NFA_STATUS_FAILED;
    }

    if (isNdef)
    {
        UINT8 *pRec;
        pRec = NDEF_MsgGetRecByIndex((UINT8*)ndefBuffer, 0);
        if (pRec == NULL )
        {
            NXPLOG_API_D ("%s: couldn't find Ndef record\n", __FUNCTION__);
            isNdef = FALSE;
            goto End;
        }
        ndef_type = NDEF_RecGetType(pRec, &ndef_tnf, &ndef_typeLength);
        *friendly_type = nativeNdef_getFriendlyType(ndef_tnf, ndef_type, ndef_typeLength);
    }

End:
    sRxDataBuffer = NULL;
    sRxDataBufferLen = 0;
    gSyncMutex.unlock();
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    return (isNdef) ? sRxDataActualSize : -1;
}

/*******************************************************************************
**
** Function:        writeNdef
**
** Description:     Write a NDEF message to the tag.
**                  buf: Contains a NDEF message.
**
** Returns:         0 if ok.
**
*******************************************************************************/
INT32 nativeNfcTag_doWriteNdef(UINT32 tagHandle, UINT8 *data,  UINT32 dataLength/*ndef message*/)
{
    tNFA_STATUS status = NFA_STATUS_OK;
    BOOLEAN result = FALSE;
    const INT32 maxBufferSize = 1024;
    UINT8 buffer[maxBufferSize] = { 0 };
    UINT32 curDataSize = 0;
    UINT32 handle = sCurrentConnectedHandle;

    NXPLOG_API_D ("%s: enter; len = %zu", __FUNCTION__, dataLength);
    if (tagHandle != sCurrentConnectedHandle)
    {
        NXPLOG_API_E ("%s: Wrong tag handle!\n)", __FUNCTION__);
        return NFA_STATUS_FAILED;
    }
    if (sCheckNdefCapable && sCheckNdefMaxSize < dataLength)
    {
        NXPLOG_API_E ("%s: NDEF message is too large!\n)", __FUNCTION__);
        return NFA_STATUS_FAILED;
    }
    if (NFA_STATUS_OK != NDEF_MsgValidate(data, dataLength,  FALSE))
    {
        NXPLOG_API_E ("%s: not NDEF message!\n)", __FUNCTION__);
        return NFA_STATUS_FAILED;
    }
    /* Create the write semaphore */
    if (sem_init (&sWriteSem, 0, 0) == -1)
    {
        NXPLOG_API_E ("%s: semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
        return NFA_STATUS_FAILED;
    }

    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        goto TheEnd;
    }
    sWriteWaitingForComplete = TRUE;
    if (sCheckNdefStatus == NFA_STATUS_FAILED)
    {
        //if tag does not contain a NDEF message
        //and tag is capable of storing NDEF message
        if (sCheckNdefCapable)
        {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            isMifare = FALSE;
#endif
            NXPLOG_API_D ("%s: try format", __FUNCTION__);
            sem_init (&sFormatSem, 0, 0);
            sFormatOk = FALSE;
            if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
            {
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
                isMifare = TRUE;
                status = EXTNS_MfcFormatTag(key1,sizeof(key1));
#endif
            }
            else
            {
                status = NFA_RwFormatTag ();
            }
            sem_wait (&sFormatSem);
            sem_destroy (&sFormatSem);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
            if(isMifare == TRUE && sFormatOk != TRUE)
            {
                sem_init (&sFormatSem, 0, 0);

                status = EXTNS_MfcFormatTag(key2,sizeof(key2));
                sem_wait (&sFormatSem);
                sem_destroy (&sFormatSem);
            }
#endif

            if (sFormatOk == FALSE) //if format operation failed
            {
                status = NFA_STATUS_FAILED;
                goto TheEnd;
            }
        }
        NXPLOG_API_D ("%s: try write", __FUNCTION__);
        status = NFA_RwWriteNDef (data, dataLength);
    }
    else if (dataLength == 0)
    {
        //if (NXP TagWriter wants to erase tag) then create and write an empty ndef message
        NDEF_MsgInit (buffer, maxBufferSize, &curDataSize);
        status = NDEF_MsgAddRec (buffer, maxBufferSize, &curDataSize, NDEF_TNF_EMPTY, NULL, 0, NULL, 0, NULL, 0);
        NXPLOG_API_D ("%s: create empty ndef msg; status=%u; size=%lu", __FUNCTION__, status, curDataSize);
        if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
        {
            status = EXTNS_MfcWriteNDef(buffer, (uint32_t)curDataSize);
        }
        else
        {
            status = NFA_RwWriteNDef (buffer, curDataSize);
        }
    }
    else
    {
        NXPLOG_API_D ("%s: NFA_RwWriteNDef", __FUNCTION__);
        if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
        {
            status = EXTNS_MfcWriteNDef(data, (uint32_t)dataLength);
        }
        else
        {
            status = NFA_RwWriteNDef (data, dataLength);
        }
    }

    if (status != NFA_STATUS_OK)
    {
        NXPLOG_API_E ("%s: write/format error=%d", __FUNCTION__, status);
        goto TheEnd;
    }

    /* Wait for write completion status */
    sWriteOk = FALSE;
    if (sem_wait (&sWriteSem))
    {
        NXPLOG_API_E ("%s: wait semaphore (errno=0x%08x)", __FUNCTION__, errno);
        status = NFA_STATUS_FAILED;
        goto TheEnd;
    }

    result = sWriteOk;

TheEnd:
    /* Destroy semaphore */
    if (sem_destroy (&sWriteSem))
    {
        NXPLOG_API_E ("%s: failed destroy semaphore (errno=0x%08x)", __FUNCTION__, errno);
    }
    sWriteWaitingForComplete = FALSE;
    gSyncMutex.unlock();
    NXPLOG_API_D ("%s: exit; result=%d", __FUNCTION__, result);

    return result ? 0 : -1;
}

/*******************************************************************************
**
** Function:        nativeNfcTag_doMakeReadonly
**
** Description:     Make the tag read-only.
**
** Returns:         0 if ok.
**
*******************************************************************************/
INT32 nativeNfcTag_doMakeReadonly (UINT32 tagHandle, UINT8 *key, UINT8 key_size)
{
    tNFA_STATUS result = NFA_STATUS_FAILED;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT32 handle = sCurrentConnectedHandle;

    NXPLOG_API_D ("%s: enter", __FUNCTION__);
    if (tagHandle != sCurrentConnectedHandle)
    {
        NXPLOG_API_E ("%s: Wrong tag handle!\n)", __FUNCTION__);
        return NFA_STATUS_FAILED;
    }

    /* Create the make_readonly semaphore */
    if (sem_init (&sMakeReadonlySem, 0, 0) == -1)
    {
        NXPLOG_API_E ("%s: Make readonly semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
        return NFA_STATUS_FAILED;
    }
    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        goto TheEnd;
    }

    sMakeReadonlyWaitingForComplete = TRUE;

    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
    {
        NXPLOG_API_E ("Calling EXTNS_MfcSetReadOnly");
        status = EXTNS_MfcSetReadOnly(key,key_size);
    }
    else
    {
        // Hard-lock the tag (cannot be reverted)
        status = NFA_RwSetTagReadOnly(TRUE);
        if (status == NFA_STATUS_REJECTED)
        {
            status = NFA_RwSetTagReadOnly (FALSE); //try soft lock
            if (status != NFA_STATUS_OK)
            {
                NXPLOG_API_E ("%s: fail soft lock, status=%d", __FUNCTION__, status);
                goto TheEnd;
            }
        }
    }
    if (status != NFA_STATUS_OK)
    {
        NXPLOG_API_E ("%s: fail hard lock, status=%d", __FUNCTION__, status);
        goto TheEnd;
    }

    /* Wait for check NDEF completion status */
    if (sem_wait (&sMakeReadonlySem))
    {
        NXPLOG_API_E ("%s: Failed to wait for make_readonly semaphore (errno=0x%08x)", __FUNCTION__, errno);
        goto TheEnd;
    }

    if (sMakeReadonlyStatus == NFA_STATUS_OK)
    {
        result = NFA_STATUS_OK;
    }

TheEnd:
    gSyncMutex.unlock();
    /* Destroy semaphore */
    if (sem_destroy (&sMakeReadonlySem))
    {
        NXPLOG_API_E ("%s: Failed to destroy read_only semaphore (errno=0x%08x)", __FUNCTION__, errno);
    }
    sMakeReadonlyWaitingForComplete = FALSE;
    return result;
}
/*******************************************************************************
**
** Function:        isFormatable
**
** Description:     Can tag be formatted to store NDEF message?
**                  tagHandle: Handle of tag.
**
** Returns:         True if formattable.
**
*******************************************************************************/
BOOLEAN nativeNfcTag_isFormatable(UINT32 tagHandle)
{
    BOOLEAN isFormattable = FALSE;

    NXPLOG_API_D ("%s: enter;", __FUNCTION__);

    switch (NfcTag::getInstance().getProtocol())
    {
    case NFA_PROTOCOL_T1T:
    case NFA_PROTOCOL_ISO15693:
    case NFA_PROTOCOL_MIFARE:
        isFormattable = TRUE;
        break;

    case NFA_PROTOCOL_T3T:
        isFormattable = NfcTag::getInstance().isFelicaLite() ? TRUE : FALSE;
        break;

    case NFA_PROTOCOL_T2T:
        isFormattable = ( NfcTag::getInstance().isMifareUltralight() |
                          NfcTag::getInstance().isInfineonMyDMove() |
                          NfcTag::getInstance().isKovioType2Tag() )
                        ? TRUE : FALSE;
        break;
    case NFA_PROTOCOL_ISO_DEP:
        /**
         * Determines whether this is a formatable IsoDep tag - currectly only NXP DESFire
         * is supported.
         */
        UINT8  get_version[] = {0x90, 0x60, 0x00, 0x00, 0x00};
        UINT8  resp1[9] = {0x0};
        UINT8  resp2[9] = {0x0};
        UINT8  resp3[16] = {0x0};
        INT32  respLength;
        if(NfcTag::getInstance().isMifareDESFire())
        {
            UINT8  addnl_info[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
            /* Identifies as DESfire, use get version cmd to be sure */
            respLength = nativeNfcTag_doTransceive(tagHandle, get_version, sizeof(get_version), resp1, sizeof(resp1), sGeneralTransceiveTimeout);
            // Check whether the response matches a typical DESfire
            // response.
            // libNFC even does more advanced checking than we do
            // here, and will only format DESfire's with a certain
            // major/minor sw version and NXP as a manufacturer.
            // We don't want to do such checking here, to avoid
            // having to change code in multiple places.
            // A successful (wrapped) DESFire getVersion command returns
            // 9 bytes, with byte 7 0x91 and byte 8 having status
            // code 0xAF (these values are fixed and well-known).
            if (respLength == sizeof(resp1) && resp1[respLength-2] == 0x91 && resp1[respLength-1] == 0xAF)
            {
                /* Get remaining software Version information */
                respLength = nativeNfcTag_doTransceive(tagHandle, addnl_info, sizeof(addnl_info), resp2, sizeof(resp2), sGeneralTransceiveTimeout);
                if (respLength == sizeof(resp2) && resp2[respLength-2] == 0x91 && resp2[respLength-1] == 0xAF)
                {
                    /* Get  the final remaining Version information */
                    respLength = nativeNfcTag_doTransceive(tagHandle, addnl_info, sizeof(addnl_info), resp3, sizeof(resp3), sGeneralTransceiveTimeout);
                    if (respLength == sizeof(resp3) && resp3[respLength-2] == 0x91 && resp3[respLength-1] == 0x00)
                    {
                        isFormattable = TRUE;
                    }
                }
            }

        }
        break;
    }
    NXPLOG_API_D("%s: is formattable=%u", __FUNCTION__, isFormattable);
    return isFormattable;
}

INT32 nativeNfcTag_doFormatTag(UINT32 tagHandle)
{
    NXPLOG_API_D ("%s: enter", __FUNCTION__);

    tNFA_STATUS status = NFA_STATUS_FAILED;
    UINT32 handle = sCurrentConnectedHandle;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    isMifare = FALSE;
#endif

    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        goto End;
    }

    if (tagHandle != sCurrentConnectedHandle)
    {
        NXPLOG_API_E ("%s: Wrong tag handle!\n)", __FUNCTION__);
        goto End;
    }

    // Do not try to format if tag is already deactivated.
    if (NfcTag::getInstance ().isActivated () == FALSE)
    {
        NXPLOG_API_D ("%s: tag already deactivated(no need to format)", __FUNCTION__);
        goto End;
    }

    sem_init (&sFormatSem, 0, 0);
    sFormatOk = FALSE;
    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
    {
        status = nativeNfcTag_doReconnect ();
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        isMifare = TRUE;
        NXPLOG_API_D("Format with First Key");
        status = EXTNS_MfcFormatTag(key1,sizeof(key1));
#endif
    }
    else
    {
        status = NFA_RwFormatTag ();
    }
    if (status == NFA_STATUS_OK)
    {
        NXPLOG_API_D ("%s: wait for completion", __FUNCTION__);
        sem_wait (&sFormatSem);
        status = sFormatOk ? NFA_STATUS_OK : NFA_STATUS_FAILED;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if(sFormatOk == TRUE && isMifare == TRUE)
        {
            NXPLOG_API_D ("Format with First Key Success");
        }
#endif
    }
    else
    {
        NXPLOG_API_E ("%s: error status=%u", __FUNCTION__, status);
    }
    sem_destroy (&sFormatSem);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(isMifare == TRUE && sFormatOk != TRUE)
    {
        NXPLOG_API_D ("Format with First Key Failed");

        status = nativeNfcTag_doReconnect ();
	if (status == NFA_STATUS_OK)
	{
	    sem_init (&sFormatSem, 0, 0);
            NXPLOG_API_D ("Format with Second Key");
            status = EXTNS_MfcFormatTag(key2,sizeof(key2));
            if (status == NFA_STATUS_OK)
            {
                NXPLOG_API_D ("%s:2nd try wait for completion", __FUNCTION__);
                sem_wait (&sFormatSem);
                status = sFormatOk ? NFA_STATUS_OK : NFA_STATUS_FAILED;
            }
            else
            {
                NXPLOG_API_E ("%s: error status=%u", __FUNCTION__, status);
            }
            sem_destroy (&sFormatSem);

            if(sFormatOk)
            {
                NXPLOG_API_D ("Format with Second Key Success");
            }
            else
            {
                NXPLOG_API_D ("Format with Second Key Failed");
            }
	}
    }
#endif

    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_ISO_DEP)
    {
        nativeNfcTag_doReconnect ();
    }
End:
    gSyncMutex.unlock();
    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    return sFormatOk ? NFA_STATUS_OK : NFA_STATUS_FAILED;
}

/* switch RF INT32erace, only for ISO-DEP */
INT32 nativeNfcTag_switchRF(UINT32 tagHandle, BOOLEAN isFrameRF)
{
    NXPLOG_API_D ("%s: enter, targetHandle = %d, isFrameRF = %d", __FUNCTION__, tagHandle, isFrameRF);
    NfcTag& natTag = NfcTag::getInstance ();
    UINT32 i = tagHandle;
    INT32 retCode = NFA_STATUS_FAILED;

    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        goto TheEnd;
    }
    if (tagHandle != sCurrentConnectedHandle)
    {
        NXPLOG_API_E ("%s: Handle not found", __FUNCTION__);
        goto TheEnd;
    }

    if (natTag.getActivationState() != NfcTag::Active)
    {
        NXPLOG_API_E ("%s: tag already deactivated", __FUNCTION__);
        goto TheEnd;
    }

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(natTag.mTechLibNfcTypes[i] == NFC_PROTOCOL_T3BT)
    {
        goto TheEnd;
    }
#endif
    if (natTag.mTechLibNfcTypes[i] != NFC_PROTOCOL_ISO_DEP)
    {
        NXPLOG_API_D ("%s() Nfc type = %d, do nothing for non ISO_DEP", __FUNCTION__, natTag.mTechLibNfcTypes[i]);
        retCode = NFA_STATUS_FAILED;
        goto TheEnd;
    }
    /* Switching is required for CTS protocol paramter test case.*/
    if (isFrameRF)
    {
        for (INT32 j = 0; j < natTag.mNumTechList; j++)
        {
            if (natTag.mTechList[j] == TARGET_TYPE_ISO14443_3A || natTag.mTechList[j] == TARGET_TYPE_ISO14443_3B)
            {
                NXPLOG_API_D ("%s: switching to tech: %d need to switch rf intf to frame", __FUNCTION__, natTag.mTechList[j]);
                retCode = switchRfInterface(NFA_INTERFACE_FRAME) ? NFA_STATUS_OK : NFA_STATUS_FAILED;
                break;
            }
        }
    }
    else
    {
        retCode = switchRfInterface(NFA_INTERFACE_ISO_DEP) ? NFA_STATUS_OK : NFA_STATUS_FAILED;
    }
    if (retCode == NFA_STATUS_OK)
    {
        sCurrentConnectedTargetType = natTag.mTechList[i];
    }
TheEnd:
    NXPLOG_API_D ("%s: exit 0x%X", __FUNCTION__, retCode);
    gSyncMutex.unlock();
    return retCode;
}

INT32 nativeNfcTag_doTransceive (UINT32 handle, UINT8* txBuffer, INT32 txBufferLen, UINT8* rxBuffer, INT32 rxBufferLen, UINT32 timeout)
{
    BOOLEAN waitOk = FALSE;
    BOOLEAN isNack = FALSE;
    tNFA_STATUS status = NFA_STATUS_FAILED;
    NXPLOG_API_D ("%s: enter", __FUNCTION__);

    if (handle != sCurrentConnectedHandle
            || rxBuffer == NULL || rxBufferLen <= 0)
    {
        return 0;
    }

    gSyncMutex.lock();
    if (!nativeNfcManager_isNfcActive())
    {
        NXPLOG_API_E ("%s: Nfc not initialized.", __FUNCTION__);
        gSyncMutex.unlock();
        return 0;
    }
    if (sRxDataBuffer != NULL)
    {
        NXPLOG_API_E ("%s: !!!! sRxDataBuffer must be NULL!", __FUNCTION__);
    }

    if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
    {
        if( doReconnectFlag == 0)
        {
            nativeNfcTag_doReconnect ();
            doReconnectFlag = 0x01;
        }
    }

    if (NfcTag::getInstance ().getActivationState () != NfcTag::Active)
    {
        NXPLOG_API_D ("%s: tag not active", __FUNCTION__);
        gSyncMutex.unlock();
        return 0;
    }

    NfcTag& natTag = NfcTag::getInstance ();

    sSwitchBackTimer.kill ();
    do
    {
        {
            SyncEventGuard g (sTransceiveEvent);
            sTransceiveRfTimeout = FALSE;
            sWaitingForTransceive = TRUE;
//            sRxDataStatus = NFA_STATUS_OK;
            sRxDataBuffer = rxBuffer;
            sRxDataBufferLen = rxBufferLen;
            sRxDataActualSize = 0;
            if (timeout < DEFAULT_GENERAL_TRANS_TIMEOUT)
            {
                sGeneralTransceiveTimeout = DEFAULT_GENERAL_TRANS_TIMEOUT;
            }
            else
            {
                sGeneralTransceiveTimeout = timeout;
            }
            if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
            {
                status = EXTNS_MfcTransceive((uint8_t *)txBuffer, (uint32_t)txBufferLen);
            }
            else
            {
                status = NFA_SendRawFrame ((UINT8 *)txBuffer, txBufferLen,
                        NFA_DM_DEFAULT_PRESENCE_CHECK_START_DELAY);
            }

            if (status != NFA_STATUS_OK)
            {
                NXPLOG_API_E ("%s: fail send; error=%d", __FUNCTION__, status);
                break;
            }
            if (timeout != 0)
            {
                waitOk = sTransceiveEvent.wait (sGeneralTransceiveTimeout);
            }
            else {
                waitOk = TRUE;
            }
        }

        if (waitOk == FALSE || sTransceiveRfTimeout) //if timeout occurred
        {
            NXPLOG_API_E ("%s: wait response timeout %d, %d", __FUNCTION__, waitOk, sTransceiveRfTimeout);
            sRxDataActualSize = 0;
            NXPLOG_API_D ("%s: Tag is lost, set state to deactivated", __FUNCTION__);
            doDisconnect ();
            break;
        }

        if (NfcTag::getInstance ().getActivationState () != NfcTag::Active)
        {
            NXPLOG_API_E ("%s: already deactivated", __FUNCTION__);
            sRxDataActualSize = 0;
            break;
        }

        NXPLOG_API_D ("%s: response %d bytes", __FUNCTION__, sRxDataActualSize);

        if ((natTag.getProtocol () == NFA_PROTOCOL_T2T) &&
            natTag.isT2tNackResponse (sRxDataBuffer, sRxDataActualSize))
        {
            isNack = TRUE;
        }

        if (sRxDataBufferLen > 0)
        {
            if (isNack)
            {
                //Some Mifare Ultralight C tags enter the HALT state after it
                //responds with a NACK.  Need to perform a "reconnect" operation
                //to wake it.
                NXPLOG_API_D ("%s: try reconnect", __FUNCTION__);
                nativeNfcTag_doReconnect ();
                NXPLOG_API_D ("%s: reconnect finish", __FUNCTION__);
            }
            else if (NfcTag::getInstance ().mTechLibNfcTypes[handle] == NFA_PROTOCOL_MIFARE)
            {
                if (EXTNS_CheckMfcResponse(&sRxDataBuffer, (uint32_t*)&sRxDataBufferLen) == NFCSTATUS_FAILED)
                {
                        nativeNfcTag_doReconnect ();
                }
                break;
            }
        }
    } while (0);

    sWaitingForTransceive = FALSE;

    NXPLOG_API_D ("%s: exit", __FUNCTION__);
    sRxDataBuffer = NULL;
    sRxDataBufferLen = 0;
    gSyncMutex.unlock();
    return sRxDataActualSize;
}

