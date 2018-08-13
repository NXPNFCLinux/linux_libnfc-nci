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
	mNumTags(0),
    mNumDiscTechList (0),
    mSelectedIndex (0),
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
    memset (&mDiscInfo, 0, sizeof(discoveryInfo_t));
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
    mNumTags = 0;
    mNumDiscTechList = 0;
    mSelectedIndex = 0;
    mActivationIndex = 0;
    mtT1tMaxMessageSize = 0;
    mReadCompletedStatus = NFA_STATUS_OK;
    mNfcDisableinProgress = false;
    resetTechnologies ();
    memset (&mDiscInfo, 0, sizeof(discoveryInfo_t));
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

    memcpy (&(mTechParams[mActivationIndex]), &(rfDetail.rf_tech_param), sizeof(rfDetail.rf_tech_param));
    if (mTechParams [mActivationIndex].mode != NFC_DISCOVERY_TYPE_POLL_KOVIO)
        return false;

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    bool rVal = false;
    if (mTechParams[mActivationIndex].param.pk.uid_len == mLastKovioUidLen)
    {
        if (memcmp(mLastKovioUid, &mTechParams [mActivationIndex].param.pk.uid, mTechParams[mActivationIndex].param.pk.uid_len) == 0)
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
        if ((mLastKovioUidLen = mTechParams[mActivationIndex].param.pk.uid_len) > NFC_KOVIO_MAX_LEN)
            mLastKovioUidLen = NFC_KOVIO_MAX_LEN;
        memcpy(mLastKovioUid, mTechParams[mActivationIndex].param.pk.uid, mLastKovioUidLen);
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

    mNumTechList = processNotification(rfDetail.protocol,rfDetail.rf_disc_id,
             rfDetail.rf_tech_param, TRUE, activationData );

    mActivationIndex = mNumTechList - 1;

    for (int i=0; i < mNumTechList; i++)
    {
        NXPLOG_API_W ("%s: index=%d; tech=%d; handle=%d; nfc type=%x",
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
    tNFA_ACTIVATED discActData;
    mNumDiscTechList = processNotification(discovery_ntf.protocol,
               discovery_ntf.rf_disc_id, discovery_ntf.rf_tech_param, FALSE,
                  /* The below parameter is ignored in the API */
                  discActData);

#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    if(discovery_ntf.more != NCI_DISCOVER_NTF_MORE)
#endif
    {
        for (int i=0; i < mNumDiscTechList; i++)
        {
            NXPLOG_API_W ("%s: index=%d; tech=%d; handle=%d; nfc type=%x",
                          "NfcTag::discoverTechnologies (discovery)",
                          i, mDiscInfo.mDiscList[i],
                          mDiscInfo.mDiscHandles[i],
                          mDiscInfo.mDiscNfcTypes[i]);
        }
    }

    NXPLOG_API_W("%s; mNumDiscTechList=%x",
                 "NfcTag::discoverTechnologies (discovery)", mNumDiscTechList);

}

/*******************************************************************************
**
** Function:        processNotification
**
** Description:     Process the Notification during the Discovery or Activation
**                  of the technologies that NFC service needs by interpreting
**                  the data structures from the stack.
**                  protocol: protocol data from discovery/activation events(s).
**                  rf_disc_id: rf_disc_id data from discovery/activation events(s).
**                  rf_tech_param: rf_tech_param data from discovery/activation events(s).
**                  activation: true if activation events(s) else discovery.
**
** Returns:         Number of Technologies Identified
**
*******************************************************************************/

int NfcTag::processNotification (UINT8 protocol, UINT8 rf_disc_id,
      tNFC_RF_TECH_PARAMS  rf_tech_param, BOOLEAN activation,
           tNFA_ACTIVATED& activationData)
{

    int techListIndex = 0;
    int maxTechList = 0;
    int *pmTechList = NULL;
    int *pmTechHandles = NULL;
    int *pmTechNfcTypes = NULL;
    tNFC_RF_TECH_PARAMS *pmTechParams = NULL;
    if(activation)
    {
        maxTechList = MAX_NUM_TECHNOLOGY;
        techListIndex = mNumTechList;
        pmTechList = mTechList;
        pmTechHandles = mTechHandles;
        pmTechNfcTypes = mTechLibNfcTypes;
        pmTechParams = mTechParams;
        NXPLOG_API_W ("%s (Activation): RF disc_id=%02x; protocol=%02x, mNumDiscList=%u",
                  "NfcTag::processNotification",
                      rf_disc_id, protocol,
                          techListIndex);
    }
    else
    {
        maxTechList = MAX_TAGS_DISCOVERED;
        techListIndex = mNumDiscTechList;
        pmTechList = (mDiscInfo.mDiscList);
        pmTechHandles = (mDiscInfo.mDiscHandles);
        pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);
        pmTechParams = (mDiscInfo.mDiscParams);
        NXPLOG_API_W ("%s (Discovery): RF disc_id=%02x; protocol=%02x, mNumDiscList=%u",
                  "NfcTag::processNotification",
                      rf_disc_id, protocol,
                          techListIndex);
    }

    if ((techListIndex >= maxTechList)
           || (pmTechList==NULL)||(pmTechList==NULL)||(pmTechList==NULL))
    {
        NXPLOG_API_W ("%s: Exceed max=%d or Invalid Memory",
                      "NfcTag::processNotification",
                       maxTechList);
    }
    else
    {
        *(pmTechHandles + techListIndex) = rf_disc_id;
        *(pmTechNfcTypes + techListIndex) = protocol;
        //save the stack's data structure for interpretation later
        memcpy ((pmTechParams + techListIndex), &(rf_tech_param), sizeof(rf_tech_param));

        switch (protocol)
        {
            case NFC_PROTOCOL_T1T:
            {
                *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
                break;
            }

            case NFC_PROTOCOL_T2T:
            {
                unsigned char sel_rsp = rf_tech_param.param.pa.sel_rsp;
                *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_3A;  //is TagTechnology.NFC_A by Java API
                //type-2 tags are identical to Mifare Ultralight, so Ultralight is also discovered
                if (techListIndex < (maxTechList - 1))
                {
                    switch(sel_rsp)
                    {
                        case 0x00:
                        {
                            // Mifare Ultralight
                            techListIndex++;
                            *(pmTechHandles + techListIndex) = rf_disc_id;
                            *(pmTechNfcTypes + techListIndex) = protocol;
                            *(pmTechList + techListIndex) = TARGET_TYPE_MIFARE_UL; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
                            //save the stack's data structure for interpretation later
                            memcpy ((pmTechParams + techListIndex), &(rf_tech_param), sizeof(rf_tech_param));
                            break;
                        }
                        //To support skylander tag.
                        case 0x01:
                        {
                            tNFC_RF_TECH_PARAMS *p_tech_params=NULL;
                            // Mifare Classic
                            techListIndex++;
                            *(pmTechHandles + techListIndex) = rf_disc_id;
                            *(pmTechNfcTypes + techListIndex) = NFC_PROTOCOL_MIFARE;
                            *(pmTechList + techListIndex) = TARGET_TYPE_MIFARE_CLASSIC; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
                            p_tech_params = (pmTechParams + techListIndex);
                            //save the stack's data structure for interpretation later
                            memcpy (p_tech_params, &(rf_tech_param), sizeof(rf_tech_param));
                            p_tech_params->param.pa.sel_rsp = 0x08;
                            if(activation)
                            {
                                EXTNS_MfcInit(activationData);
                            }
                            break;
                        }
                        case 0x08:
                        case 0x18:
                        {
                            // Mifare Classic
                            techListIndex++;
                            *(pmTechHandles + techListIndex) = rf_disc_id;
                            *(pmTechNfcTypes + techListIndex) = NFC_PROTOCOL_MIFARE;
                            *(pmTechList + techListIndex) = TARGET_TYPE_MIFARE_CLASSIC; //is TagTechnology.MIFARE_ULTRALIGHT by Java API
                            //save the stack's data structure for interpretation later
                            memcpy ((pmTechParams + techListIndex), &(rf_tech_param), sizeof(rf_tech_param));
                            if(activation)
                            {
                                EXTNS_MfcInit(activationData);
                            }
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }
                break;
            }

            case NFC_PROTOCOL_T3T:
            {
                *(pmTechList + techListIndex) = TARGET_TYPE_FELICA;
                if(activation)
                {
                    UINT8 xx = 0;
                    NXPLOG_API_W ("%s: Felica Tag Detected", __FUNCTION__);
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
            }

            case NFC_PROTOCOL_ISO_DEP: //type-4 tag uses technology ISO-DEP and technology A or B
            {
                *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_4; //is TagTechnology.ISO_DEP by Java API
                if ( (rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                        (rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) ||
                        (rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A) ||
                        (rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE) )
                {
                    if (techListIndex < (maxTechList - 1))
                    {
                        techListIndex++;
                        *(pmTechHandles + techListIndex) = rf_disc_id;
                        *(pmTechNfcTypes + techListIndex) = protocol;
                        *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_3A; //is TagTechnology.NFC_A by Java API
                        //save the stack's data structure for interpretation later
                        memcpy ((pmTechParams + techListIndex), &(rf_tech_param), sizeof(rf_tech_param));
                    }

                    if(activation)
                    {
                        unsigned char sel_rsp = rf_tech_param.param.pa.sel_rsp;
                        NXPLOG_API_W ("%s: ISO DEP - Type A Tag Detected", __FUNCTION__);
                        //is TagTechnology.NFC_A
                        NXPLOG_API_W ("%s (activation): NumTechList =%d sel_rsp=0x%x ", __FUNCTION__, mNumTechList, sel_rsp);
                    }
                }
                else if ( (rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B) ||
                        (rf_tech_param.mode == NFC_DISCOVERY_TYPE_POLL_B_PRIME) ||
                        (rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B) ||
                        (rf_tech_param.mode == NFC_DISCOVERY_TYPE_LISTEN_B_PRIME) )
                {
                    if (techListIndex < (maxTechList - 1))
                    {
                        techListIndex++;
                        *(pmTechHandles + techListIndex) = rf_disc_id;
                        *(pmTechNfcTypes + techListIndex) = protocol;
                        *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B by Java API
                        //save the stack's data structure for interpretation later
                        memcpy ((pmTechParams + techListIndex), &(rf_tech_param), sizeof(rf_tech_param));
                    }
                    if(activation)
                    {
                        NXPLOG_API_W ("%s: ISO DEP - Type B Tag Detected", __FUNCTION__);
                    }
                }
                break;
            }
            case NFC_PROTOCOL_15693: //is TagTechnology.NFC_V by Java API
            {
                *(pmTechList + techListIndex) = TARGET_TYPE_ISO15693;
                break;
            }
#if (NFC_NXP_NOT_OPEN_INCLUDED ==TRUE)
            case NFC_PROTOCOL_MIFARE:
            {
                /*
                 * Just Update the Technology as the parameters are already copied above
                 * */
                *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_3A;
                if (techListIndex < (maxTechList - 1))
                {
                    techListIndex++;
                    *(pmTechList + techListIndex) = TARGET_TYPE_MIFARE_CLASSIC;
                    *(pmTechHandles + techListIndex) = rf_disc_id;
                    *(pmTechNfcTypes + techListIndex) = protocol;
                    //save the stack's data structure for interpretation later
                    memcpy ((pmTechParams + techListIndex), &(rf_tech_param), sizeof(rf_tech_param));
                }
                if(activation)
                {
                    unsigned char sel_rsp = rf_tech_param.param.pa.sel_rsp;
                    NXPLOG_API_W ("Mifare Classic detected");
                    //is TagTechnology.NFC_A
                    NXPLOG_API_W ("%s (activation): NumTechList = %d sel_rsp=0x%x ", __FUNCTION__, techListIndex, sel_rsp);
                    EXTNS_MfcInit(activationData);
                }
                break;
            }
            case NFC_PROTOCOL_T3BT:
            {
                *(pmTechList + techListIndex) = TARGET_TYPE_ISO14443_3B; //is TagTechnology.NFC_B
                break;
            }
#endif

            default:
            {
               if (protocol == NFC_PROTOCOL_KOVIO)
               {
                   NXPLOG_API_E ("%s: Kovio",
                                   "NfcTag::processNotification");
                   *(pmTechList + techListIndex) = TARGET_TYPE_KOVIO_BARCODE;
               }
               else
               {
                   NXPLOG_API_E ("%s: unknown protocol ????",
                                   "NfcTag::processNotification");
                   *(pmTechList + techListIndex) = TARGET_TYPE_UNKNOWN;
               }
               break;
            }
        }

        techListIndex++;
        NXPLOG_API_W("%s; mNumTechList=%x",
                     "NfcTag::processNotification", techListIndex);

    }

    return techListIndex;
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

    NXPLOG_API_D ("%s: Selected: index=%d; tech=%d; handle=%d; nfc type=%x",
                  __FUNCTION__, mActivationIndex, mTechList[mActivationIndex],
                   mTechHandles[mActivationIndex], mTechLibNfcTypes[mActivationIndex]);
    //    tag.technology = mTechList [mNumTechList -1];
#if 1
    tag.technology = mTechList [mActivationIndex];
    tag.handle = mTechHandles[mActivationIndex];
    tag.protocol = mTechLibNfcTypes[mActivationIndex];
#else

    //tag.technology = activationData.activate_ntf.
    tag.handle = activationData.activate_ntf.rf_disc_id;
    tag.protocol = activationData.activate_ntf.protocol;
#endif
    setNfcTagUid(tag, activationData);

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
    switch (mTechParams [mActivationIndex].mode)
    {
    case NFC_DISCOVERY_TYPE_POLL_A:
    case NFC_DISCOVERY_TYPE_POLL_A_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_A:
    case NFC_DISCOVERY_TYPE_LISTEN_A_ACTIVE:
        NXPLOG_API_D ("%s: tech A", "NfcTag::setNfcTagUid");
        tag.uid_length = mTechParams [mActivationIndex].param.pa.nfcid1_len;
        memcpy(tag.uid, &mTechParams [mActivationIndex].param.pa.nfcid1, tag.uid_length);
        //a tag's NFCID1 can change dynamically at each activation;
        //only the first byte (0x08) is constant; a dynamic NFCID1's length
        //must be 4 bytes (see NFC Digitial Protocol,
        //section 4.7.2 SDD_RES Response, Requirements 20).
        mIsDynamicTagId = (mTechParams [mActivationIndex].param.pa.nfcid1_len == 4) &&
                (mTechParams [mActivationIndex].param.pa.nfcid1 [0] == 0x08);
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
            memcpy(tag.uid, &mTechParams [mActivationIndex].param.pb.nfcid0, NFC_NFCID0_MAX_LEN);
        }
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
        else
        {
            NXPLOG_API_D ("%s: chinaId card", "NfcTag::setNfcTagUid");
            NXPLOG_API_D ("%s: pipi_id[0]=%x", "NfcTag::setNfcTagUid",
                          mTechParams [mActivationIndex].param.pb.pupiid[0]);
            tag.uid_length = NFC_PUPIID_MAX_LEN;
            memcpy(tag.uid, &mTechParams [mActivationIndex].param.pb.pupiid, NFC_PUPIID_MAX_LEN);
        }
#endif
        break;

    case NFC_DISCOVERY_TYPE_POLL_F:
    case NFC_DISCOVERY_TYPE_POLL_F_ACTIVE:
    case NFC_DISCOVERY_TYPE_LISTEN_F:
    case NFC_DISCOVERY_TYPE_LISTEN_F_ACTIVE:
        tag.uid_length = NFC_NFCID2_LEN;
        memcpy(tag.uid, &mTechParams [mActivationIndex].param.pf.nfcid2, NFC_NFCID2_LEN);
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
        if( NFC_DISCOVERY_TYPE_POLL_KOVIO == mTechParams [mActivationIndex].mode)
        {
            NXPLOG_API_D ("%s: Kovio", "NfcTag::setNfcTagUid");
            tag.uid_length= mTechParams [mActivationIndex].param.pk.uid_len;
            memcpy(tag.uid, &mTechParams [mActivationIndex].param.pk.uid, tag.uid_length);
        }
        else
        {
            NXPLOG_API_E ("%s: tech unknown ????", "NfcTag::setNfcTagUid");
        }
        break;
    }

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
    int techDiscList = 0;
    int *pmTechNfcTypes = NULL;

    techDiscList = mNumDiscTechList;
    pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);

    for (int i = 0; i < techDiscList; i++)
    {
        if (pmTechNfcTypes[i] == NFA_PROTOCOL_NFC_DEP)
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
    mActivationParams_t.mTechParams = mTechParams[mActivationIndex].mode;
    mActivationParams_t.mTechLibNfcTypes = mTechLibNfcTypes [mActivationIndex];
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
    int techDiscList = 0;
    int *pmTechHandles = NULL;
    int *pmTechNfcTypes = NULL;
    tNFC_RF_TECH_PARAMS *pmDiscParams = NULL;
    techDiscList = mNumDiscTechList;
    pmTechHandles = (mDiscInfo.mDiscHandles);
    pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);
    pmDiscParams = (mDiscInfo.mDiscParams);

    for (int i = 0; i < techDiscList; i++)
    {
        //if remote device does not support P2P, just skip it
        if (pmTechNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
            continue;

        //if remote device supports tech F;
        //tech F is preferred because it is faster than tech A
        if ( (pmDiscParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F) ||
             (pmDiscParams[i].mode == NFC_DISCOVERY_TYPE_POLL_F_ACTIVE) )
        {
            rfDiscoveryId = pmTechHandles[i];
            break; //no need to search further
        }
        else if ( (pmDiscParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
                (pmDiscParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A_ACTIVE) )
        {
            //only choose tech A if tech F is unavailable
            if (rfDiscoveryId == 0)
            {
                rfDiscoveryId = pmTechHandles[i];
            }
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
    resetDiscInfo();
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
    mSelectedIndex = 0;
    mActivationIndex = 0;
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
** Function:        resetDiscInfo
**
** Description:     Clear all Information stored using the Discovery Notification .
**
** Returns:         None
**
*******************************************************************************/


void NfcTag::resetDiscInfo (void)
{
    mNumDiscNtf = 0;
    mNumTags = 0;
    mNumDiscTechList=0;
    memset (&mDiscInfo, 0, sizeof(discoveryInfo_t));
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

    int techDiscList = 0;
    int *pmTechHandles = NULL;
    int *pmTechNfcTypes = NULL;
#if 0
    {
        techDiscList = mNumTechList;
        pmTechHandles = mTechHandles;
        pmTechNfcTypes = mTechLibNfcTypes;
    }
#else
    {
        techDiscList = mNumDiscTechList;
        pmTechHandles = (mDiscInfo.mDiscHandles);
        pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);
    }
#endif


    for (int i = 0; i < techDiscList; i++)
    {
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X",
                      "NfcTag::selectFirstTag", i, pmTechHandles [i],
                            pmTechNfcTypes [i]);
        if (pmTechNfcTypes[i] != NFA_PROTOCOL_NFC_DEP)
        {
            foundIdx = i;
            break;
        }
    }

    if (foundIdx != -1)
    {
        if (pmTechNfcTypes [foundIdx] == NFA_PROTOCOL_ISO_DEP)
        {
            rf_intf = NFA_INTERFACE_ISO_DEP;
        }
#if (NFC_NXP_NOT_OPEN_INCLUDED ==TRUE)
        else if(pmTechNfcTypes [foundIdx] == NFA_PROTOCOL_MIFARE)
        {
            rf_intf = NFA_INTERFACE_MIFARE;
        }
#endif
        else
        {
            rf_intf = NFA_INTERFACE_FRAME;
        }

        tNFA_STATUS stat = NFA_Select (pmTechHandles [foundIdx], pmTechNfcTypes [foundIdx], rf_intf);
        if (stat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: fail select; error=0x%X",
                          "NfcTag::selectFirstTag", stat);
        }
        else
        {
            mSelectedIndex = foundIdx;
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
    int foundIdx = -1;

    int techDiscList = 0;
    int *pmTechHandles = NULL;
    int *pmTechNfcTypes = NULL;
#if 0
    {
        techDiscList = mNumTechList;
        pmTechHandles = mTechHandles;
        pmTechNfcTypes = mTechLibNfcTypes;
    }
#else
    {
        techDiscList = mNumDiscTechList;
        pmTechHandles = (mDiscInfo.mDiscHandles);
        pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);
    }
#endif
    NXPLOG_API_D("%s: enter, mNumDiscTechList=%x mSelectedIndex=%x", "NfcTag::checkNextValidProtocol",
                 mSelectedIndex, techDiscList);
    for (int i = 0; i < techDiscList; i++)
    {
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X",
                      "NfcTag::checkNextValidProtocol", i, pmTechHandles [i],
                       pmTechNfcTypes [i]);
        if ((pmTechHandles[mSelectedIndex] == pmTechHandles [i]) &&
            (pmTechNfcTypes[i] != NFA_PROTOCOL_NFC_DEP))
        {
            foundIdx = i;
            NXPLOG_API_E ("%s: Next Valid Protocol: index=%d handle=0x%X; protocol=0x%X",
                          __FUNCTION__, foundIdx, pmTechHandles [foundIdx],
                            pmTechNfcTypes [foundIdx]);
            break;
        }
    }
    return foundIdx;
}

 /*******************************************************************************
**
** Function:        selectNextTag
**
** Description:     When multiple tags are discovered, selects the Next one to activate.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectNextTag ()
{
    int foundIdx = -1;
    tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;
    tNFA_STATUS stat = NFA_STATUS_FAILED;
    int techDiscList = 0;
    int *pmTechHandles = NULL;
    int *pmTechNfcTypes = NULL;
#if 0
    {
        techDiscList = mNumTechList;
        pmTechHandles = mTechHandles;
        pmTechNfcTypes = mTechLibNfcTypes;
    }
#else
    {
        techDiscList = mNumDiscTechList;
        pmTechHandles = (mDiscInfo.mDiscHandles);
        pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);
    }
#endif

    NXPLOG_API_D("%s: enter, mNumDiscTechList=%x", "NfcTag::selectNextTag",
                       techDiscList);
    for (int i = 0,j = mSelectedIndex; i < techDiscList; i++, j++)
    {
        j = j % techDiscList;
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X",
                      "NfcTag::selectNextTag", i, pmTechHandles [j],
                            pmTechNfcTypes [j]);
        if ((pmTechHandles[mSelectedIndex] != pmTechHandles [j]) &&
            (pmTechNfcTypes[j] != NFA_PROTOCOL_NFC_DEP))
        {
            foundIdx = j;
            break;
        }
    }

    if (foundIdx != -1)
    {
        if (pmTechNfcTypes [foundIdx] == NFA_PROTOCOL_ISO_DEP)
        {
            rf_intf = NFA_INTERFACE_ISO_DEP;
        }
#if (NFC_NXP_NOT_OPEN_INCLUDED ==TRUE)
        else if(pmTechNfcTypes [foundIdx] == NFA_PROTOCOL_MIFARE)
        {
            rf_intf = NFA_INTERFACE_MIFARE;
        }
#endif
        else
        {
            rf_intf = NFA_INTERFACE_FRAME;
        }

        stat = NFA_Select (pmTechHandles [foundIdx], pmTechNfcTypes [foundIdx], rf_intf);
        if (stat == NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: Selecting  nfc target index=%d handle=0x%X; protocol=0x%X",
                          __FUNCTION__, foundIdx, pmTechHandles [foundIdx],
                           pmTechNfcTypes [foundIdx]);
            NXPLOG_API_E ("%s: stat=%x; wait for activated ntf",
                          __FUNCTION__, stat);
            mSelectedIndex = foundIdx;
            mNumTechList = 0;
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
** Function:        selectTag
**
** Description:     When multiple tags are discovered, selects the Tag with
** 					the tagHandle to activate.
**
** Returns:         None
**
*******************************************************************************/
void NfcTag::selectTag (int tagHandle)
{
    int foundIdx = -1;
    tNFA_INTF_TYPE rf_intf = NFA_INTERFACE_FRAME;
    tNFA_STATUS stat = NFA_STATUS_FAILED;

    int techDiscList = 0;
    int *pmTechHandles = NULL;
    int *pmTechNfcTypes = NULL;
#if 0
    {
        techDiscList = mNumTechList;
        pmTechHandles = mTechHandles;
        pmTechNfcTypes = mTechLibNfcTypes;
    }
#else
    {
        techDiscList = mNumDiscTechList;
        pmTechHandles = (mDiscInfo.mDiscHandles);
        pmTechNfcTypes = (mDiscInfo.mDiscNfcTypes);
    }
#endif
    NXPLOG_API_D("%s: enter, techListIndex=%x", __FUNCTION__, techDiscList);
    for (int i = 0; i < techDiscList; i++)
    {
        NXPLOG_API_D ("%s: nfa target idx=%d h=0x%X; protocol=0x%X", __FUNCTION__,
                        i, pmTechHandles [i], pmTechNfcTypes [i]);
        if ((tagHandle == pmTechHandles [i]))
        {
            foundIdx = i;
            break;
        }
    }

    if (foundIdx != -1)
    {
        if (pmTechNfcTypes [foundIdx] == NFA_PROTOCOL_ISO_DEP)
        {
            rf_intf = NFA_INTERFACE_ISO_DEP;
        }
#if (NFC_NXP_NOT_OPEN_INCLUDED ==TRUE)
        else if(pmTechNfcTypes [foundIdx] == NFA_PROTOCOL_MIFARE)
        {
            rf_intf = NFA_INTERFACE_MIFARE;
        }
#endif
        else
        {
            rf_intf = NFA_INTERFACE_FRAME;
        }

        stat = NFA_Select (pmTechHandles [foundIdx], pmTechNfcTypes [foundIdx], rf_intf);
        if (stat == NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: stat=%x; wait for activated ntf", __FUNCTION__, stat);
            mSelectedIndex = foundIdx;
            mNumTechList = 0;
        }
        else
        {
            NXPLOG_API_E ("%s: fail select; error=0x%X", __FUNCTION__, stat);
        }
    }
    else
    {
        NXPLOG_API_E ("%s: only found NFC-DEP technology.",__FUNCTION__);
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
        {
        	NXPLOG_API_W("%s:NFC Tag/Target Deactivated",__FUNCTION__);
            mIsActivated = false;
            mProtocol = NFC_PROTOCOL_UNKNOWN;
            resetTechnologies ();
        }
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
            /* By default the Type A TagUID is obtained from the first Tag
             * Which is not relevant anymore */
            *uid = mTechParams [i].param.pa.nfcid1;
            return;
        }
    }

    *len = 0;
    *uid = NULL;
}

void NfcTag::getTypeASelRsp(UINT8 *p_sel_rsp)
{
    unsigned int sel_rsp = 0;
    for (int i =0; i < mNumTechList; i++)
    {
        if ( (mTechParams[i].mode == NFC_DISCOVERY_TYPE_POLL_A) ||
             (mTechParams[i].mode == NFC_DISCOVERY_TYPE_LISTEN_A) )
        {
            sel_rsp = mTechParams[i].param.pa.sel_rsp;
            printf ("%s: sel_rsp=%u ", __FUNCTION__, sel_rsp);
            break;
        }
    }

    *p_sel_rsp = sel_rsp;
    return;
}


