/******************************************************************************
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
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
#pragma once
#include <pthread.h>
#ifndef UINT32
typedef unsigned long   UINT32;
#endif
#include "nfc_target.h"
#include "nfc_hal_api.h"

class ThreadMutex
{
public:
    ThreadMutex();
    virtual ~ThreadMutex();
    void lock();
    void unlock();
    operator pthread_mutex_t* () {return &mMutex;}
private:
    pthread_mutex_t mMutex;
};

class ThreadCondVar : public ThreadMutex
{
public:
    ThreadCondVar();
    virtual ~ThreadCondVar();
    void signal();
    void wait();
    operator pthread_cond_t* () {return &mCondVar;}
    operator pthread_mutex_t* () {return ThreadMutex::operator pthread_mutex_t*();}
private:
    pthread_cond_t  mCondVar;
};

class AutoThreadMutex
{
public:
    AutoThreadMutex(ThreadMutex &m);
    virtual ~AutoThreadMutex();
    operator ThreadMutex& ()          {return mm;}
    operator pthread_mutex_t* () {return (pthread_mutex_t*)mm;}
private:
    ThreadMutex  &mm;
};

class NfcAdaptation
{
public:
    virtual ~NfcAdaptation();
    void    Initialize();
    void    Configure();
    void    Finalize();
    static  NfcAdaptation& GetInstance();
    tHAL_NFC_ENTRY* GetHalEntryFuncs ();
    void    DownloadFirmware ();
private:
    NfcAdaptation();
    void    signal();
    static  NfcAdaptation* mpInstance;
    static  ThreadMutex sLock;
    ThreadCondVar    mCondVar;
    tHAL_NFC_ENTRY   mHalEntryFuncs; // function pointers for HAL entry points
    static tHAL_NFC_CBACK* mHalCallback;
    static tHAL_NFC_DATA_CBACK* mHalDataCallback;
    static ThreadCondVar mHalOpenCompletedEvent;
    static ThreadCondVar mHalCloseCompletedEvent;
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    pthread_t mThreadId;
    static ThreadCondVar mHalCoreResetCompletedEvent;
    static ThreadCondVar mHalCoreInitCompletedEvent;
    static ThreadCondVar mHalInitCompletedEvent;
#endif
    static UINT32 NFCA_TASK (UINT32 arg);
    static UINT32 Thread (UINT32 arg);
    void InitializeHalDeviceContext ();
    static void HalDeviceContextCallback (nfc_event_t event, nfc_status_t event_status);
    static void HalDeviceContextDataCallback (UINT16 data_len, UINT8* p_data);

    static void HalInitialize ();
    static void HalTerminate ();
    static void HalOpen (tHAL_NFC_CBACK* p_hal_cback, tHAL_NFC_DATA_CBACK* p_data_cback);
    static void HalClose ();
    static void HalCoreInitialized (UINT8* p_core_init_rsp_params);
    static void HalWrite (UINT16 data_len, UINT8* p_data);
    static BOOLEAN HalPrediscover ();
    static void HalControlGranted ();
    static void HalPowerCycle ();
    static UINT8 HalGetMaxNfcee ();
    static void HalDownloadFirmwareCallback (nfc_event_t event, nfc_status_t event_status);
    static void HalDownloadFirmwareDataCallback (UINT16 data_len, UINT8* p_data);
};

