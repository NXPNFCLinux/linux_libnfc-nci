/*
 * Copyright (C) 2012 The Android Open Source Project
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
/*
 *  Tag-reading, tag-writing operations.
 */
#include "OverrideLog.h"
#include "NfcTag.h"
#include "nci_config.h"
/******************************************************************************
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

#include <string.h>
#include "NfcDefs.h"

extern "C"
{
    #include "rw_int.h"
    #include "phNxpExtns.h"
    #include "phNxpLog.h"
    #include "nativeNfcTag.h"
}
extern void nativeNfcTag_onTagArrival(nfc_tag_info_t *tag);

#define DEFAULT_PRESENCE_CHECK_DELAY    125
int selectedId = 0;

/*******************************************************************************
**
** Function:        NfcTag
**
** Description:     Initialize member variables.
**
** Returns:         None
**
*******************************************************************************/
NfcTag::NfcTag ()
:   mNumTechList (0),
    mNumDiscNtf (0),
    mNumDiscTechList (0),
    mTechListIndex (0),
    mCashbeeDetected(false),
    mEzLinkTypeTag(false),
    //mTechnologyTimeoutsTable (MAX_NUM_TECHNOLOGY),
    mIsActivated (false),
    mActivationState (Idle),
    mProtocol(NFC_PROTOCOL_UNKNOWN),
    mtT1tMaxMessageSize (0),
    mReadCompletedStatus (NFA_STATUS_OK),
    mLastKovioUidLen (0),
    mNdefDetectionTimedOut (false),
    mIsDynamicTagId (false),
    mPresenceCheckAlgorithm (NFA_RW_PRES_CHK_DEFAULT),
    mIsFelicaLite(false)
{
    memset (mTechList, 0, sizeof(mTechList));
    memset (mTechHandles, 0, sizeof(mTechHandles));
    memset (mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
    memset (mTechParams, 0, sizeof(mTechParams));
    memset(mLastKovioUid, 0, NFC_KOVIO_MAX_LEN);
    memset(&mActivationParams_t, 0, sizeof(activationParams_t));
}


/*******************************************************************************
**
** Function:        getInstance
**
** Description:     Get a reference to the singleton NfcTag object.
**
** Returns:         Reference to NfcTag object.
**
*******************************************************************************/
NfcTag& NfcTag::getInstance ()
{
    static NfcTag tag;
    return tag;
}


/*******************************************************************************
**
** Function:        initialize
**
** Description:     Reset member variables.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::initialize ()
{
    long num = 0;

    mIsActivated = false;
    mActivationState = Idle;
    mProtocol = NFC_PROTOCOL_UNKNOWN;
    mNumTechList = 0;
    mNumDiscNtf = 0;
    mNumDiscTechList = 0;
    mTechListIndex = 0;
    mtT1tMaxMessageSize = 0;
    mReadCompletedStatus = NFA_STATUS_OK;
    mNfcDisableinProgress = false;
    resetTechnologies ();
    if (GetNumValue(NAME_PRESENCE_CHECK_ALGORITHM, &num, sizeof(num)))
        mPresenceCheckAlgorithm = num;
}


/*******************************************************************************
**
** Function:        abort
**
** Description:     Unblock all operations.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::abort ()
{
    SyncEventGuard g (mReadCompleteEvent);
    mReadCompleteEvent.notifyOne ();
}


/*******************************************************************************
**
** Function:        getActivationState
**
** Description:     What is the current state: Idle, Sleep, or Activated.
**
** Returns:         Idle, Sleep, or Activated.
**
*******************************************************************************/
NfcTag::ActivationState NfcTag::getActivationState ()
{
    return mActivationState;
}


/*******************************************************************************
**
** Function:        setDeactivationState
**
** Description:     Set the current state: Idle or Sleep.
**                  deactivated: state of deactivation.
**
** Returns:         None.
**
*******************************************************************************/
void NfcTag::setDeactivationState (tNFA_DEACTIVATED& deactivated)
{
    mActivationState = Idle;
    mNdefDetectionTimedOut = false;
    if (deactivated.type == NFA_DEACTIVATE_TYPE_SLEEP)
        mActivationState = Sleep;
    NXPLOG_API_D ("%s: state=%u", "NfcTag::setDeactivationState",
                  mActivationState);
}


/*******************************************************************************
**
** Function:        setActivationState
**
** Description:     Set the current state to Active.
**
** Returns:         None.
**
*******************************************************************************/
void NfcTag::setActivationState ()
{
    mNdefDetectionTimedOut = false;
    mActivationState = Active;
    NXPLOG_API_D ("%s: state=%u", "NfcTag::setActivationState",
                  mActivationState);
}

/*******************************************************************************
**
** Function:        isActivated
**
** Description:     Is tag activated?
**
** Returns:         True if tag is activated.
**
*******************************************************************************/
bool NfcTag::isActivated ()
{
    return mIsActivated;
}


/*******************************************************************************
**
** Function:        getProtocol
**
** Description:     Get the protocol of the current tag.
**
** Returns:         Protocol number.
**
*******************************************************************************/
tNFC_PROTOCOL NfcTag::getProtocol()
{
    return mProtocol;
}

/*******************************************************************************
**
** Function         TimeDiff
**
** Description      Computes time difference in milliseconds.
**
** Returns          Time difference in milliseconds
**
*******************************************************************************/
UINT32 TimeDiff(timespec start, timespec end)
{
    timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0)
    {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }

    return (temp.tv_sec * 1000) + (temp.tv_nsec / 1000000);
}

/*******************************************************************************
**
** Function:        IsSameKovio
**
** Description:     Checks if tag activate is the same (UID) Kovio tag previously
**                  activated.  This is needed due to a problem with some Kovio
**                  tags re-activating multiple times.
**                  activationData: data from activation.
**
** Returns:         true if the activation is from the same tag previously
**                  activated, false otherwise
**
*******************************************************************************/
bool NfcTag::IsSameKovio(tNFA_ACTIVATED& activationData)
{
    NXPLOG_API_D ("%s: enter", "NfcTag::IsSameKovio");
    tNFC_ACTIVATE_DEVT& rfDetail = activationData.activate_ntf;

    if (rfDetail.protocol != NFC_PROTOCOL_KOVIO)
        return false;

    memcpy (&(mTechParams[0]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
    if (mTechParams [0].mode != NFC_DISCOVERY_TYPE_POLL_KOVIO)
        return false;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    bool rVal = false;
    if (mTechParams[0].param.pk.uid_len == mLastKovioUidLen)
    {
        if (memcmp(mLastKovioUid, &mTechParams [0].param.pk.uid, mTechParams[0].param.pk.uid_len) == 0)
        {
            //same tag
            if (TimeDiff(mLastKovioTime, now) < 500)
            {
                // same tag within 500 ms, ignore activation
                rVal = true;
            }
        }
    }

    // save Kovio tag info
    if (!rVal)
    {
        if ((mLastKovioUidLen = mTechParams[0].param.pk.uid_len) > NFC_KOVIO_MAX_LEN)
            mLastKovioUidLen = NFC_KOVIO_MAX_LEN;
        memcpy(mLastKovioUid, mTechParams[0].param.pk.uid, mLastKovioUidLen);
    }
    mLastKovioTime = now;
    NXPLOG_API_D ("%s: exit, is same Kovio=%d", "NfcTag::IsSameKovio", rVal);
    return rVal;
}

/*******************************************************************************
**
** Function:        discoverTechnologies
**
** Description:     Discover the technologies that NFC service needs by interpreting
**                  the data structures from the stack.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::discoverTechnologies (tNFA_ACTIVATED& activationData)
{
    NXPLOG_API_D ("%s: enter", "NfcTag::discoverTechnologies (activation)");
    tNFC_ACTIVATE_DEVT& rfDetail = activationData.activate_ntf;

    mNumTechList = mTechListIndex;
    NXPLOG_API_D ("mNumTechList =%d, mTechListIndex=%d", mNumTechList, mTechListIndex);
    mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
    mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;

    //save the stack's data structure for interpretation later
    memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

    switch (rfDetail.protocol)
    {
    case NFC_PROTOCOL_T1T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
        break;

    case NFC_PROTOCOL_T2T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A;  //is TagTechnology.NFC_A by Java API
        // could be MifFare UL or Classic or Kovio
        {
            // need to look at first byte of uid to find Manufacture Byte
            tNFC_RF_TECH_PARAMS tech_params;
            memcpy (&tech_params, &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

            if ((tech_params.param.pa.nfcid1[0] == 0x04 && rfDetail.rf_tech_param.param.pa.sel_rsp == 0) ||
                rfDetail.rf_tech_param.param.pa.sel_rsp == 0x18 ||
                rfDetail.rf_tech_param.param.pa.sel_rsp == 0x08 ||
                rfDetail.rf_tech_param.param.pa.sel_rsp == 0x01)
            {
                if (rfDetail.rf_tech_param.param.pa.sel_rsp == 0)
                {
                    mNumTechList++;
                    mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
                    mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
                    //save the stack's data structure for interpretation later
                    memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
                    mTechList [mNumTechList] = TARGET_TYPE_MIFARE_UL; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
                }                //To support skylander tag.
                else if (rfDetail.rf_tech_param.param.pa.sel_rsp == 0x01)
                {
                    mTechLibNfcTypes [mNumTechList] = NFC_PROTOCOL_MIFARE;
                    rfDetail.rf_tech_param.param.pa.sel_rsp = 0x08;
                    memcpy (&tech_params, &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
                    mNumTechList++;
                    mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
                    mTechLibNfcTypes [mNumTechList] = NFC_PROTOCOL_MIFARE;
                    //save the stack's data structure for interpretation later
                    memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
                    mTechList [mNumTechList] = TARGET_TYPE_MIFARE_CLASSIC; //is TagTechnology.MIFARE_CLASSIC by Java API
                    EXTNS_MfcInit(activationData);
                }
            }
        }
        break;

    case NFC_PROTOCOL_T3T:
        {
            UINT8 xx = 0;

            mTechList [mNumTechList] = TARGET_TYPE_FELICA;

            //see if it is Felica Lite.
            while (xx < activationData.params.t3t.num_system_codes)
            {
                if (activationData.params.t3t.p_system_codes[xx++] == T3T_SYSTEM_CODE_FELICA_LITE)
                {
                    mIsFelicaLite = true;
                    break;
                }
            }
        }
        break;

    case NFC_PROTOCOL_ISO_DEP: //type-4 tag uses technology ISO-DEP and technology A or B
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_4; //is TagTechnology.ISO_DEP by Java API
        if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
        {
            mNumTechList++;
            mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
        }
        else if ( (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                (rfDetail.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) )
        {
            mNumTechList++;
            mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
        }
        break;

    case NFC_PROTOCOL_15693: //is TagTechnology.NFC_V by Java API
        mTechList [mNumTechList] = TARGET_TYPE_ISO15693;
        break;

    case NFC_PROTOCOL_KOVIO:
        NXPLOG_API_D ("%s: Kovio", "NfcTag::discoverTechnologies (activation)");
        mTechList [mNumTechList] = TARGET_TYPE_KOVIO_BARCODE;
        break;

#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    case NFC_PROTOCOL_MIFARE:
        NXPLOG_API_E ("Mifare Classic detected");
        EXTNS_MfcInit(activationData);
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A;  //is TagTechnology.NFC_A by Java API
        // could be MifFare UL or Classic or Kovio
        {
            // need to look at first byte of uid to find manuf.
            tNFC_RF_TECH_PARAMS tech_params;
            memcpy (&tech_params, &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));

                {
                    mNumTechList++;
                    mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
                    mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
                    //save the stack's data structure for interpretation later
                    memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
                    mTechList [mNumTechList] = TARGET_TYPE_MIFARE_CLASSIC; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
                }
        }
        break;

    case NFC_PROTOCOL_T3BT:
        {
            mTechHandles [mNumTechList] = rfDetail.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = rfDetail.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B by Java API
            //save the stack's data structure for interpretation later
            memcpy (&(mTechParams[mNumTechList]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
        }
        break;
#endif

    default:
        NXPLOG_API_E ("%s: unknown protocol ????",
                      "NfcTag::discoverTechnologies (activation)");
        mTechList [mNumTechList] = TARGET_TYPE_UNKNOWN;
        break;
    }

    mNumTechList++;
    for (int i=0; i < mNumTechList; i++)
    {
        NXPLOG_API_D ("%s: index=%d; tech=%d; handle=%d; nfc type=%d",
                      "NfcTag::discoverTechnologies (activation)",
                i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
    }
    NXPLOG_API_D ("%s: exit", "NfcTag::discoverTechnologies (activation)");
}


/*******************************************************************************
**
** Function:        discoverTechnologies
**
** Description:     Discover the technologies that NFC service needs by interpreting
**                  the data structures from the stack.
**                  discoveryData: data from discovery events(s).
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::discoverTechnologies (tNFA_DISC_RESULT& discoveryData)
{
    tNFC_RESULT_DEVT& discovery_ntf = discoveryData.discovery_ntf;

    NXPLOG_API_D ("%s: enter: rf disc. id=%u; protocol=%u, mNumTechList=%u",
                  "NfcTag::discoverTechnologies (discovery)",
                  discovery_ntf.rf_disc_id, discovery_ntf.protocol,
                  mNumTechList);
    if (mNumTechList >= MAX_NUM_TECHNOLOGY)
    {
        NXPLOG_API_E ("%s: exceed max=%d",
                      "NfcTag::discoverTechnologies (discovery)",
                      MAX_NUM_TECHNOLOGY);
        goto TheEnd;
    }
    mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
    mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;

    //save the stack's data structure for interpretation later
    memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));

    switch (discovery_ntf.protocol)
    {
    case NFC_PROTOCOL_T1T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
        break;

    case NFC_PROTOCOL_T2T:
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A;  //is TagTechnology.NFC_A by Java API
        //type-2 tags are identical to Mifare Ultralight, so Ultralight is also discovered
        if ((discovery_ntf.rf_tech_param.param.pa.sel_rsp == 0) &&
                (mNumTechList < (MAX_NUM_TECHNOLOGY-1)))
        {
            // Mifare Ultralight
            mNumTechList++;
            mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
            mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
            mTechList [mNumTechList] = TARGET_TYPE_MIFARE_UL; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
        }

        //save the stack's data structure for interpretation later
        memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
        break;

    case NFC_PROTOCOL_T3T:
        mTechList [mNumTechList] = TARGET_TYPE_FELICA;
        break;

    case NFC_PROTOCOL_ISO_DEP: //type-4 tag uses technology ISO-DEP and technology A or B
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_4; //is TagTechnology.ISO_DEP by Java API
        if ( (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
        {
            if (mNumTechList < (MAX_NUM_TECHNOLOGY-1))
            {
                mNumTechList++;
                mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
                mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
                mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
                //save the stack's data structure for interpretation later
                memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
            }
        }
        else if ( (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                (discovery_ntf.rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) )
        {
            if (mNumTechList < (MAX_NUM_TECHNOLOGY-1))
            {
                mNumTechList++;
                mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
                mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
                mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B by Java API
                //save the stack's data structure for interpretation later
                memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
            }
        }
        break;

    case NFC_PROTOCOL_15693: //is TagTechnology.NFC_V by Java API
        mTechList [mNumTechList] = TARGET_TYPE_ISO15693;
        break;

#if (NFC_NXP_NOT_OPEN_INCLUDED ==TRUE)
    case NFC_PROTOCOL_MIFARE:
        mTechHandles [mNumTechList] = discovery_ntf.rf_disc_id;
        mTechLibNfcTypes [mNumTechList] = discovery_ntf.protocol;
        mTechList [mNumTechList] = TARGET_TYPE_MIFARE_CLASSIC;
        //save the stack's data structure for interpretation later
        memcpy (&(mTechParams[mNumTechList]), &(discovery_ntf.rf_tech_param), sizeof(discovery_ntf.rf_tech_param));
            mNumTechList++;
        mTechList [mNumTechList] = TARGET_TYPE_ISO14443_3A;
        break;
#endif

    default:
        NXPLOG_API_E ("%s: unknown protocol ????",
                      "NfcTag::discoverTechnologies (discovery)");
        mTechList [mNumTechList] = TARGET_TYPE_UNKNOWN;
        break;
    }

    mNumTechList++;

#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(discovery_ntf.more != NCI_DISCOVER_NTF_MORE)
#endif
    {
        for (int i=0; i < mNumTechList; i++)
        {
            NXPLOG_API_D ("%s: index=%d; tech=%d; handle=%d; nfc type=%d",
                          "NfcTag::discoverTechnologies (discovery)",
                    i, mTechList[i], mTechHandles[i], mTechLibNfcTypes[i]);
        }
    }
    mNumDiscTechList = mNumTechList;
    NXPLOG_API_D("%s; mNumDiscTechList=%x",
                 "NfcTag::discoverTechnologies (discovery)", mNumDiscTechList);

TheEnd:
    NXPLOG_API_D ("%s: exit", "NfcTag::discoverTechnologies (discovery)");
}


/*******************************************************************************
**
** Function:        createNativeNfcTag
**
** Description:     Create a brand new Java NativeNfcTag object;
**                  fill the objects's member variables with data;
**                  notify NFC service;
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::createNativeNfcTag (tNFA_ACTIVATED& activationData)
{
    NXPLOG_API_D ("%s: enter", "NfcTag::createNativeNfcTag");

    nfc_tag_info_t tag;
    memset(&tag, 0, sizeof(nfc_tag_info_t));

    tag.technology = mTechList [mNumTechList -1];
    tag.handle = mTechHandles[0];
    tag.protocol = mTechLibNfcTypes[0];
    setNfcTagUid(tag, activationData);

    //notify app about this new tag
    //mNumDiscNtf = 0;
    storeActivationParams();
    nativeNfcTag_onTagArrival(&tag);
    NXPLOG_API_D ("%s: exit", "NfcTag::createNativeNfcTag");
}

/*******************************************************************************
**
** Function:        setNfcTagUid
**
** Description:     Fill nfcTag's members: mUid.
**                  tag: nfcTag object.
**                  activationData: data from activation.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::setNfcTagUid (nfc_tag_info_t& tag, tNFA_ACTIVATED& activationData)
{
    switch (mTechParams [0].mode)
    {
    case NFC_DISCOVERY_TYPE_POLL_KOVIO:
        NXPLOG_API_D ("%s: Kovio", "NfcTag::setNfcTagUid");
        tag.uid_length= mTechParams [0].param.pk.uid_len;
        memcpy(tag.uid, &mTechParams [0].param.pk.uid, tag.uid_length);
        break;

    case NFC_DISCOVERY_TYPE_POLL_A:
    case NFC_DISCOVERY_TYPE_POLL_A_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_A:
    case NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE:
        NXPLOG_API_D ("%s: tech A", "NfcTag::setNfcTagUid");
        tag.uid_length = mTechParams [0].param.pa.nfcid1_len;
        memcpy(tag.uid, &mTechParams [0].param.pa.nfcid1, tag.uid_length);
        //a tag's NFCID1 can change dynamically at each activation;
        //only the first byte (0x08) is constant; a dynamic NFCID1's length
        //must be 4 bytes (see NFC Digitial Protocol,
        //section 4.7.2 SDD_RES Response, Requirements 20).
        mIsDynamicTagId = (mTechParams [0].param.pa.nfcid1_len == 4) &&
                (mTechParams [0].param.pa.nfcid1 [0] == 0x08);
        break;

    case NFC_DISCOVERY_TYPE_POLL_B:
    case NFC_DISCOVERY_TYPE_POLL_B_PRIME:
    case NFC_DISCOVERY_TYPE_LISTEN_B:
    case NFC_DISCOVERY_TYPE_LISTEN_B_PRIME:
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        if(activationData.activate_ntf.protocol != NFA_PROTOCOL_T3BT)
#endif
        {
            NXPLOG_API_D ("%s: tech B", "NfcTag::setNfcTagUid");
            tag.uid_length = NFC_NFCID0_MAX_LEN;
            memcpy(tag.uid, &mTechParams [0].param.pb.nfcid0, NFC_NFCID0_MAX_LEN);
        }
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        else
        {
            NXPLOG_API_D ("%s: chinaId card", "NfcTag::setNfcTagUid");
            NXPLOG_API_D ("%s: pipi_id[0]=%x", "NfcTag::setNfcTagUid",
                          mTechParams [0].param.pb.pupiid[0]);
            tag.uid_length = NFC_PUPIID_MAX_LEN;
            memcpy(tag.uid, &mTechParams [0].param.pb.pupiid, NFC_PUPIID_MAX_LEN);
        }
#endif
        break;

    case NFC_DISCOVERY_TYPE_POLL_F:
    case NFC_DISCOVERY_TYPE_POLL_F_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_F:
    case NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE:
        tag.uid_length = NFC_NFCID2_LEN;
        memcpy(tag.uid, &mTechParams [0].param.pf.nfcid2, NFC_NFCID2_LEN);
        NXPLOG_API_D ("%s: tech F", "NfcTag::setNfcTagUid");
        break;

    case NFC_DISCOVERY_TYPE_POLL_ISO15693:
    case NFC_DISCOVERY_TYPE_LISTEN_ISO15693:
        {
            NXPLOG_API_D ("%s: tech iso 15693", "NfcTag::setNfcTagUid");
            unsigned char data [I93_UID_BYTE_LEN];  //8 bytes
            for (int i=0; i<I93_UID_BYTE_LEN; ++i) //reverse the ID
                data[i] = activationData.params.i93.uid [I93_UID_BYTE_LEN - i - 1];
            tag.uid_length = I93_UID_BYTE_LEN;
            memcpy(tag.uid, data, I93_UID_BYTE_LEN);
       }
        break;

    default:
        NXPLOG_API_E ("%s: tech unknown ????", "NfcTag::setNfcTagUid");
        break;
    }
    mTechListIndex = mNumTechList;
    if(!mNumDiscNtf)
        mTechListIndex = 0;
    NXPLOG_API_D("%s;mTechListIndex=%x", "NfcTag::setNfcTagUid",
                 mTechListIndex);
}

/*******************************************************************************
**
** Function:        isP2pDiscovered
**
** Description:     Does the peer support P2P?
**
** Returns:         True if the peer supports P2P.
**
*******************************************************************************/
bool NfcTag::isP2pDiscovered ()
{
    bool retval = false;

    for (int i = 0; i < mNumTechList; i++)
    {
        if (mTechLibNfcTypes[i] == NFA_PROTOCOL_NFC_DEP)
        {
            //if remote device supports P2P
            NXPLOG_API_D ("%s: discovered P2P", "NfcTag::isP2pDiscovered");
            retval = true;
            break;
        }
    }
    NXPLOG_API_D ("%s: return=%u", "NfcTag::isP2pDiscovered", retval);
    return retval;
}

/*******************************************************************************
**
** Function:        storeActivationParams
**
** Description:     stores tag activation parameters for backup
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::storeActivationParams()
{
    mActivationParams_t.mTechParams = mTechParams[0].mode;
    mActivationParams_t.mTechLibNfcTypes = mTechLibNfcTypes [0];
}
/*******************************************************************************
**
** Function:        selectP2p
**
** Description:     Select the preferred P2P technology if there is a choice.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectP2p()
{
    UINT8 rfDiscoveryId = 0;

    for (int i = 0; i < mNumTechList; i++)
    {
        //if remote device does not support P2P, just skip it
        if (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
            continue;

        //if remote device supports tech F;
        //tech F is preferred because it is faster than tech A
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE) )
        {
            rfDiscoveryId = mTechHandles[i];
            break; //no need to search further
        }
        else if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) )
        {
            //only choose tech A if tech F is unavailable
            if (rfDiscoveryId == 0)
                rfDiscoveryId = mTechHandles[i];
        }
    }

    if (rfDiscoveryId > 0)
    {
        NXPLOG_API_D ("%s: select P2P; target rf discov id=0x%X",
                      "NfcTag::selectP2p", rfDiscoveryId);
        tNFA_STATUS stat = NFA_Select (rfDiscoveryId, NFA_PROTOCOL_NFC_DEP, NFA_INTERFACE_NFC_DEP);
        if (stat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: fail select P2P; error=0x%X",
                          "NfcTag::selectP2p", stat);
        }
    }
    else
    {
        NXPLOG_API_E ("%s: cannot find P2P", "NfcTag::selectP2p");
    }
    resetTechnologies ();
}


/*******************************************************************************
**
** Function:        resetTechnologies
**
** Description:     Clear all data related to the technology, protocol of the tag.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::resetTechnologies ()
{
    NXPLOG_API_D ("%s", "NfcTag::resetTechnologies");
    mNumTechList = 0;
    mTechListIndex = 0;
    memset (mTechList, 0, sizeof(mTechList));
    memset (mTechHandles, 0, sizeof(mTechHandles));
    memset (mTechLibNfcTypes, 0, sizeof(mTechLibNfcTypes));
    memset (mTechParams, 0, sizeof(mTechParams));
    mIsDynamicTagId = false;
    mIsFelicaLite = false;
    //resetAllTransceiveTimeouts ();
}


/*******************************************************************************
**
** Function:        selectFirstTag
**
** Description:     When multiple tags are discovered, just select the first one to activate.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectFirstTag ()
{
    int foundIdx = -1;
    tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;

    for (int i = 0; i < mNumTechList; i++)
    {
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X",
                      "NfcTag::selectFirstTag", i, mTechHandles [i],
                      mTechLibNfcTypes [i]);
        if (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
        {
            foundIdx = i;
            selectedId = i;
            break;
        }
    }

    if (foundIdx != -1)
    {
        if (mTechLibNfcTypes [foundIdx] == NFA_PROTOCOL_ISO_DEP)
        {
            rf_intf = NFA_INTERFACE_ISO_DEP;
        }
#if (NFC_NXP_NOT_OPEN_INCLUDED ==TRUE)
        else if(mTechLibNfcTypes [foundIdx] == NFA_PROTOCOL_MIFARE)
        {
            rf_intf = NFA_INTERFACE_MIFARE;
        }
#endif
        else
            rf_intf = NFA_INTERFACE_FRAME;

        tNFA_STATUS stat = NFA_Select (mTechHandles [foundIdx], mTechLibNfcTypes [foundIdx], rf_intf);
        if (stat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: fail select; error=0x%X",
                          "NfcTag::selectFirstTag", stat);
        }
    }
    else
    {
        NXPLOG_API_E ("%s: only found NFC-DEP technology.",
                      "NfcTag::selectFirstTag");
    }
}

/*******************************************************************************
**
** Function:        checkNextValidProtocol
**
** Description:     When multiple tags are discovered, check next valid protocol
**
** Returns:         id
**
*******************************************************************************/
int NfcTag::checkNextValidProtocol(void)
{
    NXPLOG_API_D("%s: enter, mNumDiscTechList=%x",
                 "NfcTag::checkNextValidProtocol", mNumDiscTechList);
    int foundIdx = -1;
    NXPLOG_API_D("%s: enter,selectedId=%x", "NfcTag::checkNextValidProtocol",
                 selectedId);
    for (int i = 0; i < mNumDiscTechList; i++)
    {
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X",
                      "NfcTag::checkNextValidProtocol", i, mTechHandles [i],
                      mTechLibNfcTypes [i]);
        if ((mTechHandles[selectedId] != mTechHandles [i]) &&
            (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP))
        {
            foundIdx = i;
            break;
        }
    }
    return foundIdx;
}

 /*******************************************************************************
**
** Function:        selectNextTag
**
** Description:     When multiple tags are discovered, selects the Nex one to activate.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectNextTag ()
{
    int foundIdx = -1;
    tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;
    tNFA_STATUS stat = NFA_STATUS_FAILED;

    NXPLOG_API_D("%s: enter, mNumDiscTechList=%x", "NfcTag::selectNextTag",
                 mNumDiscTechList);
    for (int i = 0; i < mNumDiscTechList; i++)
    {
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X",
                      "NfcTag::selectNextTag", i, mTechHandles [i],
                      mTechLibNfcTypes [i]);
        if ((mTechHandles[selectedId] != mTechHandles [i]) &&
            (mTechLibNfcTypes[i] != NFA_PROTOCOL_NFC_DEP))
        {
            selectedId = i;
            foundIdx = i;
            break;
        }
    }

    if (foundIdx != -1)
    {
        if (mTechLibNfcTypes [foundIdx] == NFA_PROTOCOL_ISO_DEP)
        {
            rf_intf = NFA_INTERFACE_ISO_DEP;
        }
        else if(mTechLibNfcTypes [foundIdx] == NFA_PROTOCOL_MIFARE)
        {
            rf_intf = NFA_INTERFACE_MIFARE;
        }
        else
            rf_intf = NFA_INTERFACE_FRAME;

        stat = NFA_Select (mTechHandles [foundIdx], mTechLibNfcTypes [foundIdx], rf_intf);
        if (stat == NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: stat=%x; wait for activated ntf",
                          "NfcTag::selectNextTag", stat);
        }
        else
        {
            NXPLOG_API_E ("%s: fail select; error=0x%X",
                          "NfcTag::selectNextTag", stat);
        }
    }
    else
    {
        NXPLOG_API_E ("%s: only found NFC-DEP technology.",
                      "NfcTag::selectNextTag");
    }
}


/*******************************************************************************
**
** Function:        getT1tMaxMessageSize
**
** Description:     Get the maximum size (octet) that a T1T can store.
**
** Returns:         Maximum size in octets.
**
*******************************************************************************/
int NfcTag::getT1tMaxMessageSize ()
{
    if (mProtocol != NFC_PROTOCOL_T1T)
    {
        NXPLOG_API_E ("%s: wrong protocol %u", "NfcTag::getT1tMaxMessageSize",
                      mProtocol);
        return 0;
    }
    return mtT1tMaxMessageSize;
}


/*******************************************************************************
**
** Function:        calculateT1tMaxMessageSize
**
** Description:     Calculate type-1 tag's max message size based on header ROM bytes.
**                  activate: reference to activation data.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::calculateT1tMaxMessageSize (tNFA_ACTIVATED& activate)
{
    //make sure the tag is type-1
    if (activate.activate_ntf.protocol != NFC_PROTOCOL_T1T)
    {
        mtT1tMaxMessageSize = 0;
        return;
    }

    //examine the first byte of header ROM bytes
    switch (activate.params.t1t.hr[0])
    {
    case RW_T1T_IS_TOPAZ96:
        mtT1tMaxMessageSize = 90;
        break;
    case RW_T1T_IS_TOPAZ512:
        mtT1tMaxMessageSize = 462;
        break;
    default:
        NXPLOG_API_E ("%s: unknown T1T HR0=%u",
                      "NfcTag::calculateT1tMaxMessageSize",
                      activate.params.t1t.hr[0]);
        mtT1tMaxMessageSize = 0;
        break;
    }
}


/*******************************************************************************
**
** Function:        isMifareUltralight
**
** Description:     Whether the currently activated tag is Mifare Ultralight.
**
** Returns:         True if tag is Mifare Ultralight.
**
*******************************************************************************/
bool NfcTag::isMifareUltralight ()
{
    bool retval = false;

    for (int i =0; i < mNumTechList; i++)
    {
        if (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            //see NFC Digital Protocol, section 4.6.3 (SENS_RES); section 4.8.2 (SEL_RES).
            //see "MF0ICU1 Functional specification MIFARE Ultralight", Rev. 3.4 - 4 February 2008,
            //section 6.7.
            if ( (mTechParams[i].param.pa.sens_res[0] == 0x44) &&
                 (mTechParams[i].param.pa.sens_res[1] == 0) &&
                 ( (mTechParams[i].param.pa.sel_rsp == 0) || (mTechParams[i].param.pa.sel_rsp == 0x04) ) &&
                 (mTechParams[i].param.pa.nfcid1[0] == 0x04) )
            {
                retval = true;
            }
            break;
        }
    }
    NXPLOG_API_D ("%s: return=%u", "NfcTag::isMifareUltralight", retval);
    return retval;
}


/*******************************************************************************
**
** Function:        isMifareDESFire
**
** Description:     Whether the currently activated tag is Mifare DESFire.
**
** Returns:         True if tag is Mifare DESFire.
**
*******************************************************************************/
bool NfcTag::isMifareDESFire ()
{
    bool retval = false;

    for (int i =0; i < mNumTechList; i++)
    {
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
        {
            /* DESfire has one sak byte and 2 ATQA bytes */
            if ( (mTechParams[i].param.pa.sens_res[0] == 0x44) &&
                 (mTechParams[i].param.pa.sens_res[1] == 3) &&
                 (mTechParams[i].param.pa.sel_rsp == 0x20))
            {
                retval = true;
            }
            break;
        }
    }
    NXPLOG_API_D ("%s: return=%u", "NfcTag::isMifareDESFire", retval);
    return retval;
}


/*******************************************************************************
**
** Function:        isFelicaLite
**
** Description:     Whether the currently activated tag is Felica Lite.
**
** Returns:         True if tag is Felica Lite.
**
*******************************************************************************/

bool NfcTag::isFelicaLite ()
{
    return mIsFelicaLite;
}


/*******************************************************************************
**
** Function:        isT2tNackResponse
**
** Description:     Whether the response is a T2T NACK response.
**                  See NFC Digital Protocol Technical Specification (2010-11-17).
**                  Chapter 9 (Type 2 Tag Platform), section 9.6 (READ).
**                  response: buffer contains T2T response.
**                  responseLen: length of the response.
**
** Returns:         True if the response is NACK
**
*******************************************************************************/
bool NfcTag::isT2tNackResponse (const UINT8* response, UINT32 responseLen)
{
    bool isNack = false;

    if (responseLen == 1)
    {
        if (response[0] == 0xA)
            isNack = false; //an ACK response, so definitely not a NACK
        else
            isNack = true; //assume every value is a NACK
    }
    NXPLOG_API_D ("%s: return %u", "NfcTag::isT2tNackResponse", isNack);
    return isNack;
}


/*******************************************************************************
**
** Function:        isNdefDetectionTimedOut
**
** Description:     Whether NDEF-detection algorithm timed out.
**
** Returns:         True if NDEF-detection algorithm timed out.
**
*******************************************************************************/
bool NfcTag::isNdefDetectionTimedOut ()
{
    return mNdefDetectionTimedOut;
}


/*******************************************************************************
**
** Function:        connectionEventHandler
**
** Description:     Handle connection-related events.
**                  event: event code.
**                  data: pointer to event data.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::connectionEventHandler (UINT8 event, tNFA_CONN_EVT_DATA* data)
{
    switch (event)
    {
    case NFA_DISC_RESULT_EVT:
        {
            tNFA_DISC_RESULT& disc_result = data->disc_result;
            if (disc_result.status == NFA_STATUS_OK)
            {
                discoverTechnologies (disc_result);
            }
        }
        break;

    case NFA_ACTIVATED_EVT:
        // Only do tag detection if we are polling and it is not 'EE Direct RF' activation
        // (which may happen when we are activated as a tag).
        if (data->activated.activate_ntf.rf_tech_param.mode < NCI_DISCOVERY_TYPE_LISTEN_A
            && data->activated.activate_ntf.intf_param.type != NFC_INTERFACE_EE_DIRECT_RF)
        {
            tNFA_ACTIVATED& activated = data->activated;
            if (IsSameKovio(activated))
                break;
            mIsActivated = true;
            mProtocol = activated.activate_ntf.protocol;
            calculateT1tMaxMessageSize (activated);
            discoverTechnologies (activated);
            createNativeNfcTag (activated);
        }
        break;

    case NFA_DEACTIVATED_EVT:
        mIsActivated = false;
        mProtocol = NFC_PROTOCOL_UNKNOWN;
        resetTechnologies ();
        break;

    case NFA_READ_CPLT_EVT:
        {
            SyncEventGuard g (mReadCompleteEvent);
            mReadCompletedStatus = data->status;
            mReadCompleteEvent.notifyOne ();
        }
        break;

    case NFA_NDEF_DETECT_EVT:
        {
            tNFA_NDEF_DETECT& ndef_detect = data->ndef_detect;
            mNdefDetectionTimedOut = ndef_detect.status == NFA_STATUS_TIMEOUT;
            if (mNdefDetectionTimedOut)
            {
                NXPLOG_API_E ("%s: NDEF detection timed out",
                              "NfcTag::connectionEventHandler");
            }
        }
        break;

    case NFA_ACTIVATED_UPDATE_EVT:
        {
            tNFA_ACTIVATED& activated = data->activated;
            mIsActivated = true;
            mProtocol = activated.activate_ntf.protocol;
            discoverTechnologies (activated);
        }
        break;
    }
}


/*******************************************************************************
**
** Function         setActive
**
** Description      Sets the active state for the object
**
** Returns          None.
**
*******************************************************************************/
void NfcTag::setActive(bool active)
{
    mIsActivated = active;
}


/*******************************************************************************
**
** Function:        isDynamicTagId
**
** Description:     Whether a tag has a dynamic tag ID.
**
** Returns:         True if ID is dynamic.
**
*******************************************************************************/
bool NfcTag::isDynamicTagId ()
{
    return mIsDynamicTagId &&
            (mTechList [0] == TARGET_TYPE_ISO14443_4) &&  //type-4 tag
            (mTechList [1] == TARGET_TYPE_ISO14443_3A);  //tech A
}

#if 0
/*******************************************************************************
**
** Function:        resetAllTransceiveTimeouts
**
** Description:     Reset all timeouts for all technologies to default values.
**
** Returns:         none
**
*******************************************************************************/
void NfcTag::resetAllTransceiveTimeouts ()
{
    mTechnologyTimeoutsTable [TARGET_TYPE_ISO14443_3A] = 618; //NfcA
    mTechnologyTimeoutsTable [TARGET_TYPE_ISO14443_3B] = 1000; //NfcB
    mTechnologyTimeoutsTable [TARGET_TYPE_ISO14443_4] = 309; //ISO-DEP
    mTechnologyTimeoutsTable [TARGET_TYPE_FELICA] = 255; //Felica
    mTechnologyTimeoutsTable [TARGET_TYPE_ISO15693] = 1000;//NfcV
    mTechnologyTimeoutsTable [TARGET_TYPE_NDEF] = 1000;
    mTechnologyTimeoutsTable [TARGET_TYPE_NDEF_FORMATABLE] = 1000;
    mTechnologyTimeoutsTable [TARGET_TYPE_MIFARE_CLASSIC] = 618; //MifareClassic
    mTechnologyTimeoutsTable [TARGET_TYPE_MIFARE_UL] = 618; //MifareUltralight
    mTechnologyTimeoutsTable [TARGET_TYPE_KOVIO_BARCODE] = 1000; //NfcBarcode
}

/*******************************************************************************
**
** Function:        getTransceiveTimeout
**
** Description:     Get the timeout value for one technology.
**                  techId: one of the values in TARGET_TYPE_* defined in NfcJniUtil.h
**
** Returns:         Timeout value in millisecond.
**
*******************************************************************************/
int NfcTag::getTransceiveTimeout (int techId)
{
    int retval = 1000;
    if ((techId > 0) && (techId < (int) mTechnologyTimeoutsTable.size()))
        retval = mTechnologyTimeoutsTable [techId];
    else
        NXPLOG_API_E ("%s: invalid tech=%d", "NfcTag::getTransceiveTimeout",
                      techId);
    return retval;
}

/*******************************************************************************
**
** Function:        setTransceiveTimeout
**
** Description:     Set the timeout value for one technology.
**                  techId: one of the values in TARGET_TYPE_* defined in NfcJniUtil.h
**                  timeout: timeout value in millisecond.
**
** Returns:         Timeout value.
**
*******************************************************************************/
void NfcTag::setTransceiveTimeout (int techId, int timeout)
{
    if ((techId >= 0) && (techId < (int) mTechnologyTimeoutsTable.size()))
        mTechnologyTimeoutsTable [techId] = timeout;
    else
        NXPLOG_API_E ("%s: invalid tech=%d", "NfcTag::setTransceiveTimeout",
                      techId);
}
#endif
/*******************************************************************************
**
** Function:        isEzLinkTagActivated
**
** Description:     checks if EzLinkTag tag is detected
**
** Returns:         True if tag is activated.
**
*******************************************************************************/
bool NfcTag::isEzLinkTagActivated ()
{
    return mEzLinkTypeTag;
}

/*******************************************************************************
**
** Function:        isCashBeeActivated
**
** Description:     checks if cashbee tag is detected
**
** Returns:         True if tag is activated.
**
*******************************************************************************/
bool NfcTag::isCashBeeActivated ()
{
    return mCashbeeDetected;
}

/*******************************************************************************
**
** Function:        isTypeBTag
**
** Description:     Whether the currently activated tag is Type B.
**
** Returns:         True if tag is Type B.
**
*******************************************************************************/
bool NfcTag::isTypeBTag ()
{
    bool retval = false;

    for (int i =0; i < mNumTechList; i++)
    {
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_B) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_B) )
        {
            retval = true;
            break;
        }
    }
    NXPLOG_API_D ("%s: return=%u", "NfcTag::isTypeBTag", retval);
    return retval;
}


/*******************************************************************************
**
** Function:        getPresenceCheckAlgorithm
**
** Description:     Get presence-check algorithm from .conf file.
**
** Returns:         Presence-check algorithm.
**
*******************************************************************************/
tNFA_RW_PRES_CHK_OPTION NfcTag::getPresenceCheckAlgorithm ()
{
    return mPresenceCheckAlgorithm;
}


/*******************************************************************************
**
** Function:        isInfineonMyDMove
**
** Description:     Whether the currently activated tag is Infineon My-D Move.
**
** Returns:         True if tag is Infineon My-D Move.
**
*******************************************************************************/
bool NfcTag::isInfineonMyDMove ()
{
    bool retval = false;

    for (int i =0; i < mNumTechList; i++)
    {
        if (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            //see Infineon my-d move, my-d move NFC, SLE 66R01P, SLE 66R01PN,
            //Short Product Information, 2011-11-24, section 3.5
            if (mTechParams[i].param.pa.nfcid1[0] == 0x05)
            {
                UINT8 highNibble = mTechParams[i].param.pa.nfcid1[1] & 0xF0;
                if (highNibble == 0x30)
                    retval = true;
            }
            break;
        }
    }
    NXPLOG_API_D ("%s: return=%u", "NfcTag::isInfineonMyDMove", retval);
    return retval;
}


/*******************************************************************************
**
** Function:        isKovioType2Tag
**
** Description:     Whether the currently activated tag is Kovio Type-2 tag.
**
** Returns:         True if tag is Kovio Type-2 tag.
**
*******************************************************************************/
bool NfcTag::isKovioType2Tag ()
{
    bool retval = false;

    for (int i =0; i < mNumTechList; i++)
    {
        if (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A)
        {
            //Kovio 2Kb RFID Tag, Functional Specification,
            //March 2, 2012, v2.0, section 8.3.
            if (mTechParams[i].param.pa.nfcid1[0] == 0x37)
                retval = true;
            break;
        }
    }
    NXPLOG_API_D ("%s: return=%u", "NfcTag::isKovioType2Tag", retval);
    return retval;
}


void NfcTag::getTypeATagUID(UINT8 **uid, UINT32 *len)
{
    for (int i =0; i < mNumTechList; i++)
    {
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) )
        {
            *len = mTechParams [i].param.pa.nfcid1_len;
            *uid = mTechParams [0].param.pa.nfcid1;
            return;
        }
    }

    *len = 0;
    *uid = NULL;
}
