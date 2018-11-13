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
/*
 *  Manage the listen-mode routing table.
 */
#pragma once
#include "SyncEvent.h"

extern "C"
{
    #include "data_types.h"
    #include "linux_nfc_api.h"
    #include "nfa_api.h"
    #include "nfa_ee_api.h"
}

class RoutingManager
{
public:
    static RoutingManager& getInstance ();
    bool initialize();
    void finalize();
    void enableRoutingToHost(bool skipCheckNDEF);
    void disableRoutingToHost();
    void registerHostCallback(nfcHostCardEmulationCallback_t *callback);
    void deregisterHostCallback();
    int registerT3tIdentifier(UINT8* t3tId, UINT8 t3tIdLen);
    void deregisterT3tIdentifier();

private:
    RoutingManager();
    ~RoutingManager();

    void handleData (const UINT8* data, UINT32 dataLen, tNFA_STATUS status);
    bool commitRouting();
    void notifyHceActivated(UINT8 mode);
    void notifyHceDeactivated();

    static void nfaEeCallback (tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* eventData);
    static void stackCallback (UINT8 event, tNFA_CONN_EVT_DATA* eventData);
    static void nfcFCeCallback (UINT8 event, tNFA_CONN_EVT_DATA* eventData);
    static const int ROUTE_HOST = 0;

    UINT8* mRxDataBuffer;
    UINT32 mRxDataBufferLen;
    SyncEvent mEeRegisterEvent;
    SyncEvent mRoutingEvent;
    SyncEvent mEeSetModeEvent;
    int mActiveSe;
    tNFA_TECHNOLOGY_MASK mSeTechMask;
    int mDefaultEe; //since this variable is used in both cases moved out of compiler switch
    int mHostListnEnable;
    int mFwdFuntnEnable;
    bool mSkipCheckNDEF;
    nfcHostCardEmulationCallback_t *mCallback;
    int mNfcFOnDhHandle;    
};

