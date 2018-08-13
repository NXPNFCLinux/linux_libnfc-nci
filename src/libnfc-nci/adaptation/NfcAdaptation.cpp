/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
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
#include <string.h>
#include <stdio.h>
#include "OverrideLog.h"
#include "NfcAdaptation.h"
#include "nci_config.h"

extern "C"
{
    #include "gki.h"
    #include "nfa_api.h"
    #include "nfc_int.h"
    #include "android_logmsg.h"
    #include "vendor_cfg.h"
    #include "phNxpNciHal.h"
    #include "phNxpNciHal_Adaptation.h"
    #include "phNxpLog.h"
    #include "phNxpConfig.h"
}

#define LOG_TAG "NfcAdaptation"

extern "C" void GKI_shutdown();
extern void resetConfig();
extern "C" void verify_stack_non_volatile_store ();
extern "C" void delete_stack_non_volatile_store (BOOLEAN forceDelete);



NfcAdaptation* NfcAdaptation::mpInstance = NULL;
ThreadMutex NfcAdaptation::sLock;
tHAL_NFC_CBACK* NfcAdaptation::mHalCallback = NULL;
tHAL_NFC_DATA_CBACK* NfcAdaptation::mHalDataCallback = NULL;
ThreadCondVar NfcAdaptation::mHalOpenCompletedEvent;
ThreadCondVar NfcAdaptation::mHalCloseCompletedEvent;
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
ThreadCondVar NfcAdaptation::mHalCoreResetCompletedEvent;
ThreadCondVar NfcAdaptation::mHalCoreInitCompletedEvent;
ThreadCondVar NfcAdaptation::mHalInitCompletedEvent;

#endif
UINT32 ScrProtocolTraceFlag = SCR_PROTO_TRACE_ALL; //0x017F00;
UINT8 appl_trace_level = 0xff;
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
UINT8 appl_dta_mode_flag = 0x00;
#endif
char nfc_nci_store[120];
char nci_hal_module[64];

static UINT8 nfa_dm_cfg[sizeof ( tNFA_DM_CFG ) ];
extern tNFA_DM_CFG *p_nfa_dm_cfg;
extern UINT8 nfa_ee_max_ee_cfg;
static UINT8 deviceHostWhiteList [NFA_HCI_MAX_HOST_IN_NETWORK];
static tNFA_HCI_CFG jni_nfa_hci_cfg;
extern tNFA_HCI_CFG *p_nfa_hci_cfg;

static uint8_t nfa_proprietary_cfg[sizeof(tNFA_PROPRIETARY_CFG)];
extern tNFA_PROPRIETARY_CFG* p_nfa_proprietary_cfg;

/*******************************************************************************
**
** Function:    NfcAdaptation::NfcAdaptation()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::NfcAdaptation()
{
    memset (&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
}

/*******************************************************************************
**
** Function:    NfcAdaptation::~NfcAdaptation()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
NfcAdaptation::~NfcAdaptation()
{
    mpInstance = NULL;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::GetInstance()
**
** Description: access class singleton
**
** Returns:     pointer to the singleton object
**
*******************************************************************************/
NfcAdaptation& NfcAdaptation::GetInstance()
{
    AutoThreadMutex  a(sLock);

    if (!mpInstance)
        mpInstance = new NfcAdaptation;
    return *mpInstance;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Initialize()
**
** Description: class initializer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Initialize ()
{
    const char* func = "NfcAdaptation::Initialize";
    NXPLOG_API_D("%s: enter", func);
    unsigned long num;

    if ( GetNumValue ( NAME_USE_RAW_NCI_TRACE, &num, sizeof ( num ) ) )
    {
        if (num == 1)
        {
            // display protocol traces in raw format
            ProtoDispAdapterUseRawOutput (TRUE);
            NXPLOG_API_D("%s: logging protocol in raw format", func);
        }
    }

    if ( !GetStrValue ( NAME_NFA_STORAGE, nfc_nci_store, sizeof ( nfc_nci_store ) ) )
    {
        strcpy(nfc_nci_store, "/usr/local/lib/");
    }

    if ( GetNumValue ( NAME_PROTOCOL_TRACE_LEVEL, &num, sizeof ( num ) ) )
        ScrProtocolTraceFlag = num;

    if ( GetStrValue ( NAME_NFA_DM_CFG, (char*)nfa_dm_cfg, sizeof ( nfa_dm_cfg ) ) )
        p_nfa_dm_cfg = ( tNFA_DM_CFG * ) &nfa_dm_cfg[0];

    //configure device host whitelist of HCI host ID's; see specification ETSI TS 102 622 V11.1.10
    //(2012-10), section 6.1.3.1
    num = GetStrValue ( NAME_DEVICE_HOST_WHITE_LIST, (char*) deviceHostWhiteList, sizeof ( deviceHostWhiteList ) );
    if (num)
    {
        memmove (&jni_nfa_hci_cfg, p_nfa_hci_cfg, sizeof(jni_nfa_hci_cfg));
        jni_nfa_hci_cfg.num_whitelist_host = (UINT8) num; //number of HCI host ID's in the whitelist
        jni_nfa_hci_cfg.p_whitelist = deviceHostWhiteList; //array of HCI host ID's
        p_nfa_hci_cfg = &jni_nfa_hci_cfg;
    }

    initializeGlobalAppLogLevel ();

    verify_stack_non_volatile_store ();
    if ( GetNumValue ( NAME_PRESERVE_STORAGE, (char*)&num, sizeof ( num ) ) &&
            (num == 1) )
    {
        NXPLOG_API_D ("%s: preserve stack NV store", __FUNCTION__);
    }
    else
    {
        delete_stack_non_volatile_store (FALSE);
    }

    GKI_init ();
    GKI_enable ();
    GKI_create_task ((TASKPTR)NFCA_TASK, BTU_TASK, (INT8*)"NFCA_TASK", 0, 0, (pthread_cond_t*)NULL, NULL);
    {
        mCondVar.lock();
        GKI_create_task ((TASKPTR)Thread, MMI_TASK, (INT8*)"NFCA_THREAD", 0, 0, (pthread_cond_t*)NULL, NULL);
        mCondVar.wait();
    }

    memset (&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));
    InitializeHalDeviceContext ();
    NXPLOG_API_D ("%s: exit", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Configure()
**
** Description: class configuration
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Configure ()
{
    unsigned long num;

    if(phNxpNciHal_getChipType() == pn547C2)
    {
        isNxpConfigValid(NXP_CONFIG_TYPE_PN547);
    }
    else
    {
        isNxpConfigValid(NXP_CONFIG_TYPE_PN548);
    }
    memset (&nfa_proprietary_cfg, 0, sizeof(nfa_proprietary_cfg));
    if (GetNxpStrValue(NAME_NXP_NFC_PROPRIETARY_CFG, (char*)nfa_proprietary_cfg,
                    sizeof(tNFA_PROPRIETARY_CFG))) {
      p_nfa_proprietary_cfg =
          (tNFA_PROPRIETARY_CFG*)(void*)(&nfa_proprietary_cfg[0]);
    }

    if ( GetNxpNumValue ( NAME_NXP_NFC_MAX_EE_SUPPORTED, &num, sizeof ( num ) ) )
    {
        nfa_ee_max_ee_cfg = num;
        NXPLOG_API_D("%s: Overriding NFA_EE_MAX_EE_SUPPORTED to use %d", __FUNCTION__, nfa_ee_max_ee_cfg);
    }
}
/*******************************************************************************
**
** Function:    NfcAdaptation::Finalize()
**
** Description: class finalizer
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::Finalize()
{
    const char* func = "NfcAdaptation::Finalize";
    AutoThreadMutex  a(sLock);

    NXPLOG_API_D ("%s: enter", func);
    GKI_shutdown ();

    resetConfig();

    mHalCallback = NULL;
    memset (&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));

    NXPLOG_API_D ("%s: exit", func);
    delete this;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::signal()
**
** Description: signal the CondVar to release the thread that is waiting
**
** Returns:     none
**
*******************************************************************************/
void NfcAdaptation::signal ()
{
    mCondVar.signal();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::NFCA_TASK()
**
** Description: NFCA_TASK runs the GKI main task
**
** Returns:     none
**
*******************************************************************************/
UINT32 NfcAdaptation::NFCA_TASK (UINT32 arg)
{
    const char* func = "NfcAdaptation::NFCA_TASK";
    NXPLOG_API_D ("%s: enter", func);
    GKI_run (0);
    NXPLOG_API_D ("%s: exit", func);
    return 0;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::Thread()
**
** Description: Creates work threads
**
** Returns:     none
**
*******************************************************************************/
UINT32 NfcAdaptation::Thread (UINT32 arg)
{
    const char* func = "NfcAdaptation::Thread";
    NXPLOG_API_D ("%s: enter", func);

    {
        ThreadCondVar    CondVar;
        CondVar.lock();
        GKI_create_task ((TASKPTR)nfc_task, NFC_TASK, (INT8*)"NFC_TASK", 0, 0, (pthread_cond_t*)CondVar, (pthread_mutex_t*)CondVar);
        CondVar.wait();
    }

    NfcAdaptation::GetInstance().signal();

    GKI_exit_task (GKI_get_taskid ());
    NXPLOG_API_D ("%s: exit", func);
    return 0;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::GetHalEntryFuncs()
**
** Description: Get the set of HAL entry points.
**
** Returns:     Functions pointers for HAL entry points.
**
*******************************************************************************/
tHAL_NFC_ENTRY* NfcAdaptation::GetHalEntryFuncs ()
{
    return &mHalEntryFuncs;
}

/*******************************************************************************
**
** Function:    NfcAdaptation::InitializeHalDeviceContext
**
** Description: Ask the generic Android HAL to find the Broadcom-specific HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::InitializeHalDeviceContext ()
{
    const char* func = "NfcAdaptation::InitializeHalDeviceContext";
    NXPLOG_API_D ("%s: enter", func);
    mHalCallback =  NULL;
    mHalDataCallback = NULL;

    memset (&mHalEntryFuncs, 0, sizeof(mHalEntryFuncs));

    mHalEntryFuncs.initialize = HalInitialize;
    mHalEntryFuncs.terminate = HalTerminate;
    mHalEntryFuncs.open = HalOpen;
    mHalEntryFuncs.close = HalClose;
    mHalEntryFuncs.core_initialized = HalCoreInitialized;
    mHalEntryFuncs.write = HalWrite;
    mHalEntryFuncs.prediscover = HalPrediscover;
    mHalEntryFuncs.control_granted = HalControlGranted;
    mHalEntryFuncs.power_cycle = HalPowerCycle;
    mHalEntryFuncs.get_max_ee = HalGetMaxNfcee;
    NXPLOG_API_D ("%s: exit", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalInitialize
**
** Description: Not implemented because this function is only needed
**              within the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalInitialize ()
{
    const char* func = "NfcAdaptation::HalInitialize";
    NXPLOG_API_D ("%s", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalTerminate
**
** Description: Not implemented because this function is only needed
**              within the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalTerminate ()
{
    const char* func = "NfcAdaptation::HalTerminate";
    NXPLOG_API_D ("%s", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalOpen
**
** Description: Turn on controller, download firmware.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalOpen (tHAL_NFC_CBACK *p_hal_cback, tHAL_NFC_DATA_CBACK* p_data_cback)
{
    const char* func = "NfcAdaptation::HalOpen";
    NXPLOG_API_D ("%s", func);
    mHalCallback = p_hal_cback;
    mHalDataCallback = p_data_cback;
    phNxpNciHal_open (HalDeviceContextCallback, HalDeviceContextDataCallback);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalClose
**
** Description: Turn off controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalClose ()
{
    const char* func = "NfcAdaptation::HalClose";
    NXPLOG_API_D ("%s", func);
    phNxpNciHal_close ();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDeviceContextCallback
**
** Description: Translate generic Android HAL's callback into Broadcom-specific
**              callback function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDeviceContextCallback (nfc_event_t event, nfc_status_t event_status)
{
    const char* func = "NfcAdaptation::HalDeviceContextCallback";
    NXPLOG_API_D ("%s: event=%u", func, event);
    if (mHalCallback)
        mHalCallback (event, (tHAL_NFC_STATUS) event_status);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDeviceContextDataCallback
**
** Description: Translate generic Android HAL's callback into Broadcom-specific
**              callback function.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDeviceContextDataCallback (uint16_t data_len, uint8_t* p_data)
{
    const char* func = "NfcAdaptation::HalDeviceContextDataCallback";
    NXPLOG_API_D ("%s: len=%u", func, data_len);
#if (NFC_SERVICE_DATA_DEBUG == 0x01)
    phNxpLog_LogBuffer (gLog_level.global_log_level, "\tRecvd", p_data , data_len);
#endif
    if (mHalDataCallback)
        mHalDataCallback (data_len, p_data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalWrite
**
** Description: Write NCI message to the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalWrite (UINT16 data_len, UINT8* p_data)
{
    const char* func = "NfcAdaptation::HalWrite";
    NXPLOG_API_D ("%s", func);
#if (NFC_SERVICE_DATA_DEBUG == 0x01)
    phNxpLog_LogBuffer (gLog_level.global_log_level, "\tSend", p_data , data_len);
#endif

    phNxpNciHal_write (data_len, p_data);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalCoreInitialized
**
** Description: Adjust the configurable parameters in the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalCoreInitialized (UINT8* p_core_init_rsp_params)
{
    const char* func = "NfcAdaptation::HalCoreInitialized";
    NXPLOG_API_D ("%s", func);
    phNxpNciHal_core_initialized (p_core_init_rsp_params);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPrediscover
**
** Description:     Perform any vendor-specific pre-discovery actions (if needed)
**                  If any actions were performed TRUE will be returned, and
**                  HAL_PRE_DISCOVER_CPLT_EVT will notify when actions are
**                  completed.
**
** Returns:          TRUE if vendor-specific pre-discovery actions initialized
**                  FALSE if no vendor-specific pre-discovery actions are needed.
**
*******************************************************************************/
BOOLEAN NfcAdaptation::HalPrediscover ()
{
    const char* func = "NfcAdaptation::HalPrediscover";
    NXPLOG_API_D ("%s", func);
    BOOLEAN retval = FALSE;

    return phNxpNciHal_pre_discover ();
}

/*******************************************************************************
**
** Function:        HAL_NfcControlGranted
**
** Description:     Grant control to HAL control for sending NCI commands.
**                  Call in response to HAL_REQUEST_CONTROL_EVT.
**                  Must only be called when there are no NCI commands pending.
**                  HAL_RELEASE_CONTROL_EVT will notify when HAL no longer
**                  needs control of NCI.
**
** Returns:         void
**
*******************************************************************************/
void NfcAdaptation::HalControlGranted ()
{
    const char* func = "NfcAdaptation::HalControlGranted";
    NXPLOG_API_D ("%s", func);
    phNxpNciHal_control_granted ();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalPowerCycle
**
** Description: Turn off and turn on the controller.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalPowerCycle ()
{
    const char* func = "NfcAdaptation::HalPowerCycle";
    NXPLOG_API_D ("%s", func);
    phNxpNciHal_power_cycle ();
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalGetMaxNfcee
**
** Description: Turn off and turn on the controller.
**
** Returns:     None.
**
*******************************************************************************/
UINT8 NfcAdaptation::HalGetMaxNfcee()
{
    const char* func = "NfcAdaptation::HalGetMaxNfcee";
    UINT8 maxNfcee = 0;
    NXPLOG_API_D ("%s", func);
    return nfa_ee_max_ee_cfg;
}


/*******************************************************************************
**
** Function:    NfcAdaptation::DownloadFirmware
**
** Description: Download firmware patch files.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::DownloadFirmware ()
{
    const char* func = "NfcAdaptation::DownloadFirmware";
    NXPLOG_API_D ("%s: enter", func);
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    static UINT8 cmd_reset_nci[] = {0x20,0x00,0x01,0x01};
    static UINT8 cmd_init_nci[]  = {0x20,0x01,0x00};
    static UINT8 cmd_reset_nci_size = sizeof(cmd_reset_nci) / sizeof(UINT8);
    static UINT8 cmd_init_nci_size  = sizeof(cmd_init_nci)  / sizeof(UINT8);
    UINT8 p_core_init_rsp_params;
#endif
    HalInitialize ();

    mHalOpenCompletedEvent.lock ();
    NXPLOG_API_D ("%s: try open HAL", func);
    HalOpen (HalDownloadFirmwareCallback, HalDownloadFirmwareDataCallback);
    mHalOpenCompletedEvent.wait ();
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    /* Send a CORE_RESET and CORE_INIT to the NFCC. This is required because when calling
     * HalCoreInitialized, the HAL is going to parse the conf file and send NCI commands
     * to the NFCC. Hence CORE-RESET and CORE-INIT have to be sent prior to this.
     */
    mHalCoreResetCompletedEvent.lock();
    NXPLOG_API_D("%s: send CORE_RESET", func);
    HalWrite(cmd_reset_nci_size , cmd_reset_nci);
    mHalCoreResetCompletedEvent.wait();
    mHalCoreInitCompletedEvent.lock();
    NXPLOG_API_D("%s: send CORE_INIT", func);
    HalWrite(cmd_init_nci_size , cmd_init_nci);
    mHalCoreInitCompletedEvent.wait();
    mHalInitCompletedEvent.lock ();
    NXPLOG_API_D ("%s: try init HAL", func);
    HalCoreInitialized (&p_core_init_rsp_params);
    mHalInitCompletedEvent.wait ();
#endif

    mHalCloseCompletedEvent.lock ();
    NXPLOG_API_D ("%s: try close HAL", func);
    HalClose ();
    mHalCloseCompletedEvent.wait ();

    HalTerminate ();
    NXPLOG_API_D ("%s: exit", func);
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDownloadFirmwareCallback
**
** Description: Receive events from the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDownloadFirmwareCallback (nfc_event_t event, nfc_status_t event_status)
{
    const char* func = "NfcAdaptation::HalDownloadFirmwareCallback";
    NXPLOG_API_D ("%s: event=0x%X", func, event);
    switch (event)
    {
    case HAL_NFC_OPEN_CPLT_EVT:
        {
            NXPLOG_API_D ("%s: HAL_NFC_OPEN_CPLT_EVT", func);
            mHalOpenCompletedEvent.signal ();
            break;
        }
    case HAL_NFC_POST_INIT_CPLT_EVT:
        {
            NXPLOG_API_D ("%s: HAL_NFC_POST_INIT_CPLT_EVT", func);
            mHalInitCompletedEvent.signal ();
            break;
        }
    case HAL_NFC_CLOSE_CPLT_EVT:
        {
            NXPLOG_API_D ("%s: HAL_NFC_CLOSE_CPLT_EVT", func);
            mHalCloseCompletedEvent.signal ();
            break;
        }
    }
}

/*******************************************************************************
**
** Function:    NfcAdaptation::HalDownloadFirmwareDataCallback
**
** Description: Receive data events from the HAL.
**
** Returns:     None.
**
*******************************************************************************/
void NfcAdaptation::HalDownloadFirmwareDataCallback (uint16_t data_len, uint8_t* p_data)
{
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if (data_len > 3)
    {
        if (p_data[0] == 0x40 && p_data[1] == 0x00)
        {
            mHalCoreResetCompletedEvent.signal();
        }
        else if (p_data[0] == 0x40 && p_data[1] == 0x01)
        {
            mHalCoreInitCompletedEvent.signal();
        }
    }
#endif
}


/*******************************************************************************
**
** Function:    ThreadMutex::ThreadMutex()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::ThreadMutex()
{
    pthread_mutexattr_t mutexAttr;

    pthread_mutexattr_init(&mutexAttr);
    pthread_mutex_init(&mMutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);
}

/*******************************************************************************
**
** Function:    ThreadMutex::~ThreadMutex()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadMutex::~ThreadMutex()
{
    pthread_mutex_destroy(&mMutex);
}

/*******************************************************************************
**
** Function:    ThreadMutex::lock()
**
** Description: lock kthe mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::lock()
{
    pthread_mutex_lock(&mMutex);
}

/*******************************************************************************
**
** Function:    ThreadMutex::unblock()
**
** Description: unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
void ThreadMutex::unlock()
{
    pthread_mutex_unlock(&mMutex);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::ThreadCondVar()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::ThreadCondVar()
{
    pthread_condattr_t CondAttr;

    pthread_condattr_init(&CondAttr);
    pthread_cond_init(&mCondVar, &CondAttr);

    pthread_condattr_destroy(&CondAttr);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::~ThreadCondVar()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
ThreadCondVar::~ThreadCondVar()
{
    pthread_cond_destroy(&mCondVar);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::wait()
**
** Description: wait on the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void ThreadCondVar::wait()
{
    pthread_cond_wait(&mCondVar, *this);
    pthread_mutex_unlock(*this);
}

/*******************************************************************************
**
** Function:    ThreadCondVar::signal()
**
** Description: signal the mCondVar
**
** Returns:     none
**
*******************************************************************************/
void ThreadCondVar::signal()
{
    AutoThreadMutex  a(*this);
    pthread_cond_signal(&mCondVar);
}

/*******************************************************************************
**
** Function:    AutoThreadMutex::AutoThreadMutex()
**
** Description: class constructor, automatically lock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::AutoThreadMutex(ThreadMutex &m)
    : mm(m)
{
    mm.lock();
}

/*******************************************************************************
**
** Function:    AutoThreadMutex::~AutoThreadMutex()
**
** Description: class destructor, automatically unlock the mutex
**
** Returns:     none
**
*******************************************************************************/
AutoThreadMutex::~AutoThreadMutex()
{
    mm.unlock();
}
