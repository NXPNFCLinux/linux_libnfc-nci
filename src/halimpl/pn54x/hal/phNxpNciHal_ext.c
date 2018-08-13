/*
 * Copyright (C) 2012-2014 NXP Semiconductors
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
#include <phNxpNciHal_ext.h>
#include <phNxpNciHal.h>
#include <phTmlNfc.h>
#include <phDal4Nfc_messageQueueLib.h>
#include <phNxpNciHal_NfcDepSWPrio.h>
#include <phNxpNciHal_Kovio.h>
#include <phNxpLog.h>
#include <phNxpConfig.h>

#define HAL_EXTNS_WRITE_RSP_TIMEOUT   (1000)                /* Timeout value to wait for response from PN548AD */

#undef P2P_PRIO_LOGIC_HAL_IMP

/******************* Global variables *****************************************/
extern phNxpNciHal_Control_t nxpncihal_ctrl;
extern phNxpNciProfile_Control_t nxpprofile_ctrl;

extern int kovio_detected;
extern int disable_kovio;
extern int send_to_upper_kovio;
extern uint32_t cleanup_timer;
static uint8_t icode_detected = 0x00;
uint8_t icode_send_eof = 0x00;
static uint8_t ee_disc_done = 0x00;
uint8_t EnableP2P_PrioLogic = FALSE;
static uint32_t RfDiscID = 1;
static uint32_t RfProtocolType = 4;
/* NFCEE Set mode */
static uint8_t setEEModeDone = 0x00;
static uint8_t cmd_nfcee_setmode_enable[] = { 0x22, 0x01, 0x02, 0x01, 0x01 };

/* External global variable to get FW version from NCI response*/
extern uint32_t wFwVerRsp;
/* External global variable to get FW version from FW file*/
extern uint16_t wFwVer;

/* local buffer to store CORE_INIT response */
static uint32_t bCoreInitRsp[40];
static uint32_t iCoreInitRspLen;

extern uint32_t timeoutTimerId;

extern NFCSTATUS read_retry();
/************** HAL extension functions ***************************************/
static void hal_extns_write_rsp_timeout_cb(uint32_t TimerId, void *pContext);

/*Proprietary cmd sent to HAL to send reader mode flag
 * Last byte of 4 byte proprietary cmd data contains ReaderMode flag
 * If this flag is enabled, NFC-DEP protocol is modified to T3T protocol
 * if FrameRF interface is selected. This needs to be done as the FW
 * always sends Ntf for FrameRF with NFC-DEP even though FrameRF with T3T is
 * previously selected with DISCOVER_SELECT_CMD
 */
#define PROPRIETARY_CMD_FELICA_READER_MODE 0xFE
static uint8_t gFelicaReaderMode;


/*******************************************************************************
**
** Function         phNxpNciHal_ext_init
**
** Description      initialize extension function
**
*******************************************************************************/
void phNxpNciHal_ext_init (void)
{
    icode_detected = 0x00;
    icode_send_eof = 0x00;
    setEEModeDone = 0x00;
    kovio_detected = 0x00;
    disable_kovio = 0x00;
    send_to_upper_kovio = 0x01;
    EnableP2P_PrioLogic = FALSE;
}

/*******************************************************************************
**
** Function         phNxpNciHal_process_ext_rsp
**
** Description      Process extension function response
**
** Returns          NFCSTATUS_SUCCESS if success
**
*******************************************************************************/
NFCSTATUS phNxpNciHal_process_ext_rsp (uint8_t *p_ntf, uint16_t *p_len)
{

    NFCSTATUS status = NFCSTATUS_SUCCESS;
    uint16_t rf_technology_length_param = 0;

    if (p_ntf[0] == 0x61 &&
        p_ntf[1] == 0x05 &&
        p_ntf[4] == 0x03 &&
        p_ntf[5] == 0x05 &&
        nxpprofile_ctrl.profile_type == EMV_CO_PROFILE)
    {
        p_ntf[4] = 0xFF;
        p_ntf[5] = 0xFF;
        p_ntf[6] = 0xFF;
        NXPLOG_NCIHAL_D("Nfc-Dep Detect in EmvCo profile - Restart polling");
    }

    if (p_ntf[0] == 0x61 &&
        p_ntf[1] == 0x05 &&
        p_ntf[4] == 0x01 &&
        p_ntf[5] == 0x05 &&
        p_ntf[6] == 0x02 &&
        gFelicaReaderMode)
    {
     /*If FelicaReaderMode is enabled,Change Protocol to T3T from NFC-DEP
          * when FrameRF interface is selected*/
        p_ntf[5] = 0x03;
        NXPLOG_NCIHAL_D("FelicaReaderMode:Activity 1.1");
    }

#ifdef P2P_PRIO_LOGIC_HAL_IMP
    if(p_ntf[0] == 0x61 &&
       p_ntf[1] == 0x05 &&
       p_ntf[4] == 0x02 &&
       p_ntf[5] == 0x04 &&
       nxpprofile_ctrl.profile_type == NFC_FORUM_PROFILE)
    {
        EnableP2P_PrioLogic = TRUE;
    }

    NXPLOG_NCIHAL_D("Is EnableP2P_PrioLogic: 0x0%X",  EnableP2P_PrioLogic);
    if(phNxpDta_IsEnable() == FALSE)
    {
        if ((icode_detected != 1)&&(kovio_detected != 1) && (EnableP2P_PrioLogic == TRUE))
        {
            if (phNxpNciHal_NfcDep_comapre_ntf(p_ntf, *p_len) == NFCSTATUS_FAILED)
            {
                status = phNxpNciHal_NfcDep_rsp_ext(p_ntf,p_len);
                if(status != NFCSTATUS_INVALID_PARAMETER)
                {
                    return status;
                }
            }
        }
    }
#endif

    status = NFCSTATUS_SUCCESS;
    status = phNxpNciHal_kovio_rsp_ext(p_ntf,p_len);

    if (p_ntf[0] == 0x61 &&
            p_ntf[1] == 0x05)
    {
        switch (p_ntf[4])
        {
        case 0x00:
            NXPLOG_NCIHAL_D("NxpNci: RF Interface = NFCEE Direct RF");
            break;
        case 0x01:
            NXPLOG_NCIHAL_D("NxpNci: RF Interface = Frame RF");
            break;
        case 0x02:
            NXPLOG_NCIHAL_D("NxpNci: RF Interface = ISO-DEP");
            break;
        case 0x03:
            NXPLOG_NCIHAL_D("NxpNci: RF Interface = NFC-DEP");
            break;
        case 0x80:
            NXPLOG_NCIHAL_D("NxpNci: RF Interface = MIFARE");
            break;
        default:
            NXPLOG_NCIHAL_D("NxpNci: RF Interface = Unknown");
            break;
        }

        switch (p_ntf[5])
        {
        case 0x01:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = T1T");
            phNxpDta_T1TEnable();
            break;
        case 0x02:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = T2T");
            break;
        case 0x03:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = T3T");
            break;
        case 0x04:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = ISO-DEP");
            break;
        case 0x05:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = NFC-DEP");
            break;
        case 0x06:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = 15693");
            break;
        case 0x80:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = MIFARE");
            break;
        case 0x81:
            if (phNxpNciHal_getChipType() != pn547C2)
            {
                NXPLOG_NCIHAL_D("NxpNci: Mode = Kovio");
            }
            else
            {
                NXPLOG_NCIHAL_D("NxpNci: Protocol = Unknown");
            }
            break;
        case 0x8A:
            if (phNxpNciHal_getChipType()  == pn547C2)
            {
                NXPLOG_NCIHAL_D("NxpNci: Mode = Kovio");
            }
            else
            {
                NXPLOG_NCIHAL_D("NxpNci: Protocol = Unknown");
            }
            break;
        default:
            NXPLOG_NCIHAL_D("NxpNci: Protocol = Unknown");
            break;
        }

        switch (p_ntf[6])
        {
        case 0x00:
            NXPLOG_NCIHAL_D("NxpNci: Mode = A Passive Poll");
            break;
        case 0x01:
            NXPLOG_NCIHAL_D("NxpNci: Mode = B Passive Poll");
            break;
        case 0x02:
            NXPLOG_NCIHAL_D("NxpNci: Mode = F Passive Poll");
            break;
        case 0x03:
            NXPLOG_NCIHAL_D("NxpNci: Mode = A Active Poll");
            break;
        case 0x05:
            NXPLOG_NCIHAL_D("NxpNci: Mode = F Active Poll");
            break;
        case 0x06:
            NXPLOG_NCIHAL_D("NxpNci: Mode = 15693 Passive Poll");
            break;
        case 0x70:
            if (phNxpNciHal_getChipType() != pn547C2)
            {
                NXPLOG_NCIHAL_D("NxpNci: Mode = Kovio");
            }
            else
            {
                NXPLOG_NCIHAL_D("NxpNci: Protocol = Unknown");
            }
            break;
        case 0x77:
            if (phNxpNciHal_getChipType() == pn547C2)
            {
                NXPLOG_NCIHAL_D("NxpNci: Mode = Kovio");
            }
            else
            {
                NXPLOG_NCIHAL_D("NxpNci: Protocol = Unknown");
            }
            break;
        case 0x80:
            NXPLOG_NCIHAL_D("NxpNci: Mode = A Passive Listen");
            break;
        case 0x81:
            NXPLOG_NCIHAL_D("NxpNci: Mode = B Passive Listen");
            break;
        case 0x82:
            NXPLOG_NCIHAL_D("NxpNci: Mode = F Passive Listen");
            break;
        case 0x83:
            NXPLOG_NCIHAL_D("NxpNci: Mode = A Active Listen");
            break;
        case 0x85:
            NXPLOG_NCIHAL_D("NxpNci: Mode = F Active Listen");
            break;
        case 0x86:
            NXPLOG_NCIHAL_D("NxpNci: Mode = 15693 Passive Listen");
            break;
        default:
            NXPLOG_NCIHAL_D("NxpNci: Mode = Unknown");
            break;
        }
    }

    if (p_ntf[0] == 0x61 &&
            p_ntf[1] == 0x05 &&
            p_ntf[2] == 0x15 &&
            p_ntf[4] == 0x01 &&
            p_ntf[5] == 0x06 &&
            p_ntf[6] == 0x06)
    {
        NXPLOG_NCIHAL_D ("> Going through workaround - notification of ISO 15693");
        icode_detected = 0x01;
        p_ntf[21] = 0x01;
        p_ntf[22] = 0x01;
    }
    else if (icode_detected == 1 &&
            icode_send_eof == 2)
    {
        icode_send_eof = 3;
        status = NFCSTATUS_FAILED;
        return status;
    }
    else if (p_ntf[0] == 0x00 &&
                p_ntf[1] == 0x00 &&
                icode_detected == 1)
    {
        if (icode_send_eof == 3)
        {
            icode_send_eof = 0;
        }
        if (p_ntf[p_ntf[2]+ 2] == 0x00)
        {
            NXPLOG_NCIHAL_D ("> Going through workaround - data of ISO 15693");
            p_ntf[2]--;
            (*p_len)--;
        }
    }
    else if (p_ntf[2] == 0x02 &&
            p_ntf[1] == 0x00 && icode_detected == 1)
    {
        NXPLOG_NCIHAL_D ("> ICODE EOF response do not send to upper layer");
    }
    else if(p_ntf[0] == 0x61 &&
            p_ntf[1] == 0x06 && icode_detected == 1)
    {
        NXPLOG_NCIHAL_D ("> Polling Loop Re-Started");
        icode_detected = 0;
        icode_send_eof = 0;
    }
#if 0 /* artf551669/artf551636 */
    else if(*p_len == 4 &&
                p_ntf[0] == 0x40 &&
                p_ntf[1] == 0x02 &&
                p_ntf[2] == 0x01 &&
                p_ntf[3] == 0x06 )
    {
        NXPLOG_NCIHAL_D ("> Deinit workaround for LLCP set_config 0x%x 0x%x 0x%x", p_ntf[21], p_ntf[22], p_ntf[23]);
        p_ntf[0] = 0x40;
        p_ntf[1] = 0x02;
        p_ntf[2] = 0x02;
        p_ntf[3] = 0x00;
        p_ntf[4] = 0x00;
        *p_len = 5;
    }
#endif
    else if ((p_ntf[0] == 0x40) && (p_ntf[1] == 0x01))
    {
        int len = p_ntf[2] + 2; /*include 2 byte header*/
        wFwVerRsp= (((uint32_t)p_ntf[len - 2])<< 16U)|(((uint32_t)p_ntf[len - 1])<< 8U)|p_ntf[len];
        if(wFwVerRsp == 0)
            status = NFCSTATUS_FAILED;
        iCoreInitRspLen = *p_len;
        memcpy(bCoreInitRsp, p_ntf, *p_len);
        NXPLOG_NCIHAL_D ("NxpNci> FW Version: %x.%x.%x", p_ntf[len-2], p_ntf[len-1], p_ntf[len]);
        if(phNxpNciHal_deriveChipType(bCoreInitRsp, iCoreInitRspLen) == pnInvalid)
        {
            status = NFCSTATUS_FAILED;
            NXPLOG_NCIHAL_E ("NxpNci> Invalid NFC Chip");
        }
    }
    //4200 02 00 01
    else if(p_ntf[0] == 0x42 && p_ntf[1] == 0x00 && ee_disc_done == 0x01)
    {
        NXPLOG_NCIHAL_D("Going through workaround - NFCEE_DISCOVER_RSP");
        if(p_ntf[4] == 0x01)
        {
            p_ntf[4] = 0x00;

            ee_disc_done = 0x00;
        }
        NXPLOG_NCIHAL_D("Going through workaround - NFCEE_DISCOVER_RSP - END");

    }
    else if(p_ntf[0] == 0x61 && p_ntf[1] == 0x03 /*&& cleanup_timer!=0*/)
    {
        if(cleanup_timer!=0)
        {
            /* if RF Notification Type of RF_DISCOVER_NTF is Last Notification */
            if(0== (*(p_ntf + 2 + (*(p_ntf+2)))))
            {
                phNxpNciHal_select_RF_Discovery(RfDiscID,RfProtocolType);
                status = NFCSTATUS_FAILED;
                return status;
            }
            else
            {
                RfDiscID=p_ntf[3];
                RfProtocolType=p_ntf[4];
            }
            status = NFCSTATUS_FAILED;
            return status;

        }

    }
    else if(p_ntf[0] == 0x41 && p_ntf[1] == 0x04 && cleanup_timer!=0)
    {
        status = NFCSTATUS_FAILED;
        return status;
    }
    else if(p_ntf[0] == 0x60 && p_ntf[1] == 0x00)
    {
        NXPLOG_NCIHAL_E("CORE_RESET_NTF received!");
        phNxpNciHal_emergency_recovery();
    }
    else if(p_ntf[0] == 0x61 && p_ntf[1] == 0x05
            && p_ntf[4] == 0x02 && p_ntf[5] == 0x80
            && p_ntf[6] == 0x00
            && (phNxpNciHal_getChipType() == pn547C2))
    {
        NXPLOG_NCIHAL_D("Going through workaround - iso-dep  interface  mifare protocol with sak value not equal to 0x20");
        rf_technology_length_param = p_ntf[9];
        if((p_ntf[ 9 + rf_technology_length_param] & 0x20) != 0x20)
        {
            p_ntf[4] = 0x80;
        }
    }
    /*
    else if(p_ntf[0] == 0x61 && p_ntf[1] == 0x05 && p_ntf[4] == 0x01 && p_ntf[5] == 0x00 && p_ntf[6] == 0x01)
    {
        NXPLOG_NCIHAL_D("Picopass type 3-B with undefined protocol is not supported, disabling");
        p_ntf[4] = 0xFF;
        p_ntf[5] = 0xFF;
        p_ntf[6] = 0xFF;
    }*/

    return status;
}

/******************************************************************************
 * Function         phNxpNciHal_process_ext_cmd_rsp
 *
 * Description      This function process the extension command response. It
 *                  also checks the received response to expected response.
 *
 * Returns          returns NFCSTATUS_SUCCESS if response is as expected else
 *                  returns failure.
 *
 ******************************************************************************/
static NFCSTATUS phNxpNciHal_process_ext_cmd_rsp(uint16_t cmd_len, uint8_t *p_cmd)
{
    NFCSTATUS status = NFCSTATUS_FAILED;
    uint16_t data_written = 0;

    /* Create the local semaphore */
    if (phNxpNciHal_init_cb_data(&nxpncihal_ctrl.ext_cb_data, NULL)
            != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("Create ext_cb_data failed");
        return NFCSTATUS_FAILED;
    }

    nxpncihal_ctrl.ext_cb_data.status = NFCSTATUS_SUCCESS;

    /* Send ext command */
    data_written = phNxpNciHal_write_unlocked(cmd_len, p_cmd);
    if (data_written != cmd_len)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_write failed for hal ext");
        goto clean_and_return;
    }

    /* Start timer */
    status = phOsalNfc_Timer_Start(timeoutTimerId,
            HAL_EXTNS_WRITE_RSP_TIMEOUT,
            &hal_extns_write_rsp_timeout_cb,
            NULL);
    if (NFCSTATUS_SUCCESS == status)
    {
        NXPLOG_NCIHAL_D("Response timer started");
    }
    else
    {
        NXPLOG_NCIHAL_E("Response timer not started!!!");
        status  = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    /* Wait for rsp */
    NXPLOG_NCIHAL_D("Waiting after ext cmd sent");
    if (SEM_WAIT(nxpncihal_ctrl.ext_cb_data))
    {
        NXPLOG_NCIHAL_E("p_hal_ext->ext_cb_data.sem semaphore error");
        goto clean_and_return;
    }

    /* Stop Timer */
    status = phOsalNfc_Timer_Stop(timeoutTimerId);

    if (NFCSTATUS_SUCCESS == status)
    {
        NXPLOG_NCIHAL_D("Response timer stopped");
    }
    else
    {
        NXPLOG_NCIHAL_E("Response timer stop ERROR!!!");
        status  = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    if(nxpncihal_ctrl.ext_cb_data.status != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_E("Callback Status is failed!! Timer Expired!! Couldn't read it! 0x%x", nxpncihal_ctrl.ext_cb_data.status);
        status  = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    NXPLOG_NCIHAL_D("Checking response");
    status = NFCSTATUS_SUCCESS;

clean_and_return:
    phNxpNciHal_cleanup_cb_data(&nxpncihal_ctrl.ext_cb_data);

    return status;
}

/******************************************************************************
 * Function         phNxpNciHal_write_ext
 *
 * Description      This function inform the status of phNxpNciHal_open
 *                  function to libnfc-nci.
 *
 * Returns          It return NFCSTATUS_SUCCESS then continue with send else
 *                  sends NFCSTATUS_FAILED direct response is prepared and
 *                  do not send anything to NFCC.
 *
 ******************************************************************************/

NFCSTATUS phNxpNciHal_write_ext(uint16_t *cmd_len, uint8_t *p_cmd_data,
        uint16_t *rsp_len, uint8_t *p_rsp_data)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phNxpNciHal_NfcDep_cmd_ext(p_cmd_data, cmd_len);

    if(phNxpDta_IsEnable() == TRUE)
    {
        status = phNxpNHal_DtaUpdate(cmd_len, p_cmd_data,rsp_len, p_rsp_data);
    }

    if (p_cmd_data[0] == PROPRIETARY_CMD_FELICA_READER_MODE &&
        p_cmd_data[1] == PROPRIETARY_CMD_FELICA_READER_MODE &&
        p_cmd_data[2] == PROPRIETARY_CMD_FELICA_READER_MODE)
    {
        NXPLOG_NCIHAL_D ("Received proprietary command to set Felica Reader mode:%d",p_cmd_data[3]);
        gFelicaReaderMode = p_cmd_data[3];
        /* frame the dummy response */
        *rsp_len = 4;
        p_rsp_data[0] = 0x00;
        p_rsp_data[1] = 0x00;
        p_rsp_data[2] = 0x00;
        p_rsp_data[3] = 0x00;
        status = NFCSTATUS_FAILED;
    }
    else if (p_cmd_data[0] == 0x20 &&
            p_cmd_data[1] == 0x02 &&
            p_cmd_data[2] == 0x05 &&
            p_cmd_data[3] == 0x01 &&
            p_cmd_data[4] == 0xA0 &&
            p_cmd_data[5] == 0x44 &&
            p_cmd_data[6] == 0x01 &&
            p_cmd_data[7] == 0x01)
    {
        nxpprofile_ctrl.profile_type = EMV_CO_PROFILE;
        NXPLOG_NCIHAL_D ("EMV_CO_PROFILE mode - Enabled");
        status = NFCSTATUS_SUCCESS;
    }
    else if (p_cmd_data[0] == 0x20 &&
            p_cmd_data[1] == 0x02 &&
            p_cmd_data[2] == 0x05 &&
            p_cmd_data[3] == 0x01 &&
            p_cmd_data[4] == 0xA0 &&
            p_cmd_data[5] == 0x44 &&
            p_cmd_data[6] == 0x01 &&
            p_cmd_data[7] == 0x00)
    {
        NXPLOG_NCIHAL_D ("NFC_FORUM_PROFILE mode - Enabled");
        nxpprofile_ctrl.profile_type = NFC_FORUM_PROFILE;
        status = NFCSTATUS_SUCCESS;
    }

    if (nxpprofile_ctrl.profile_type == EMV_CO_PROFILE)
    {
        if (p_cmd_data[0] == 0x21 &&
                p_cmd_data[1] == 0x06 &&
                p_cmd_data[2] == 0x01 &&
                p_cmd_data[3] == 0x03)
        {
#if 0
            //Needs clarification whether to keep it or not
            NXPLOG_NCIHAL_D ("EmvCo Poll mode - RF Deactivate discard");
            phNxpNciHal_print_packet("SEND", p_cmd_data, *cmd_len);
            *rsp_len = 4;
            p_rsp_data[0] = 0x41;
            p_rsp_data[1] = 0x06;
            p_rsp_data[2] = 0x01;
            p_rsp_data[3] = 0x00;
            phNxpNciHal_print_packet("RECV", p_rsp_data, 4);
            status = NFCSTATUS_FAILED;
#endif
        }
        else if(p_cmd_data[0] == 0x21 &&
                p_cmd_data[1] == 0x03 )
        {
            NXPLOG_NCIHAL_D ("EmvCo Poll mode - Discover map only for A and B");
            p_cmd_data[2] = 0x05;
            p_cmd_data[3] = 0x02;
            p_cmd_data[4] = 0x00;
            p_cmd_data[5] = 0x01;
            p_cmd_data[6] = 0x01;
            p_cmd_data[7] = 0x01;
            *cmd_len = 8;
        }
    }
    if ( p_cmd_data[0] == 0x21 &&
        p_cmd_data[1] == 0x00
        && (phNxpNciHal_getChipType() == pn547C2))
    {
        unsigned long retval = 0;
        int isfound =  GetNxpNumValue(NAME_MIFARE_READER_ENABLE, &retval, sizeof(unsigned long));

        if(retval == 0x01)
        {
            NXPLOG_NCIHAL_D ("Going through extns - Adding Mifare in RF Discovery");
            p_cmd_data[2] += 3;
            p_cmd_data[3] += 1;
            p_cmd_data[*cmd_len] = 0x80;
            p_cmd_data[*cmd_len + 1] = 0x01;
            p_cmd_data[*cmd_len + 2] = 0x80;
            *cmd_len += 3;
            status = NFCSTATUS_SUCCESS;
            NXPLOG_NCIHAL_D ("Going through extns - Adding Mifare in RF Discovery - END");
        }
    }
    else
    if (p_cmd_data[3] == 0x81 &&
            p_cmd_data[4] == 0x01 &&
            p_cmd_data[5] == 0x03)
    {
        NXPLOG_NCIHAL_D("> Going through workaround - set host list");

        if( (phNxpNciHal_getChipType() != pn547C2))
        {
            *cmd_len = 8;

            p_cmd_data[2] = 0x05;
            p_cmd_data[6] = 0x02;
            p_cmd_data[7] = 0xC0;
        }
        else
        {
            *cmd_len = 7;

            p_cmd_data[2] = 0x04;
            p_cmd_data[6] = 0xC0;
        }

        NXPLOG_NCIHAL_D("> Going through workaround - set host list - END");
        status = NFCSTATUS_SUCCESS;
    }
    else if(icode_detected)
    {
        if ((p_cmd_data[3] & 0x40) == 0x40 &&
            (p_cmd_data[4] == 0x21 ||
             p_cmd_data[4] == 0x22 ||
             p_cmd_data[4] == 0x24 ||
             p_cmd_data[4] == 0x27 ||
             p_cmd_data[4] == 0x28 ||
             p_cmd_data[4] == 0x29 ||
             p_cmd_data[4] == 0x2a))
        {
            NXPLOG_NCIHAL_D ("> Send EOF set");
            icode_send_eof = 1;
        }

        if(p_cmd_data[3]  == 0x20 || p_cmd_data[3]  == 0x24 ||
           p_cmd_data[3]  == 0x60)
        {
            NXPLOG_NCIHAL_D ("> NFC ISO_15693 Proprietary CMD ");
            p_cmd_data[3] += 0x02;
        }
    }
    else if(p_cmd_data[0] == 0x21 &&
            p_cmd_data[1] == 0x03 )
    {
        NXPLOG_NCIHAL_D ("> Polling Loop Started");
        icode_detected = 0;
        icode_send_eof = 0;
    }
    //22000100
    else if (p_cmd_data[0] == 0x22 &&
            p_cmd_data[1] == 0x00 &&
            p_cmd_data[2] == 0x01 &&
            p_cmd_data[3] == 0x00
            )
    {
        //ee_disc_done = 0x01;//Reader Over SWP event getting
        *rsp_len = 0x05;
        p_rsp_data[0] = 0x42;
        p_rsp_data[1] = 0x00;
        p_rsp_data[2] = 0x02;
        p_rsp_data[3] = 0x00;
        p_rsp_data[4] = 0x00;
        phNxpNciHal_print_packet("RECV", p_rsp_data,5);
        status = NFCSTATUS_FAILED;
    }
    //2002 0904 3000 3100 3200 5000
    else if ( (p_cmd_data[0] == 0x20 && p_cmd_data[1] == 0x02 ) &&
            ( (p_cmd_data[2] == 0x09 && p_cmd_data[3] == 0x04) /*||
              (p_cmd_data[2] == 0x0D && p_cmd_data[3] == 0x04)*/
            )
    )
    {
        *cmd_len += 0x01;
        p_cmd_data[2] += 0x01;
        p_cmd_data[9] = 0x01;
        p_cmd_data[10] = 0x40;
        p_cmd_data[11] = 0x50;
        p_cmd_data[12] = 0x00;

        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config ");
//        phNxpNciHal_print_packet("SEND", p_cmd_data, *cmd_len);
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config - End ");
    }
//    20020703300031003200
//    2002 0301 3200
    else if ( (p_cmd_data[0] == 0x20 && p_cmd_data[1] == 0x02 ) &&
            (
              (p_cmd_data[2] == 0x07 && p_cmd_data[3] == 0x03) ||
              (p_cmd_data[2] == 0x03 && p_cmd_data[3] == 0x01 && p_cmd_data[4] == 0x32)
            )
    )
    {
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config ");
        phNxpNciHal_print_packet("SEND", p_cmd_data, *cmd_len);
        *rsp_len = 5;
        p_rsp_data[0] = 0x40;
        p_rsp_data[1] = 0x02;
        p_rsp_data[2] = 0x02;
        p_rsp_data[3] = 0x00;
        p_rsp_data[4] = 0x00;

        phNxpNciHal_print_packet("RECV", p_rsp_data, 5);
        status = NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config - End ");
    }

    //2002 0D04 300104 310100 320100 500100
    //2002 0401 320100
    else if ( (p_cmd_data[0] == 0x20 && p_cmd_data[1] == 0x02 ) &&
            (
              /*(p_cmd_data[2] == 0x0D && p_cmd_data[3] == 0x04)*/
                (p_cmd_data[2] == 0x04 && p_cmd_data[3] == 0x01 && p_cmd_data[4] == 0x32 && p_cmd_data[5] == 0x00)
            )
    )
    {
//        p_cmd_data[12] = 0x40;

        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config ");
        phNxpNciHal_print_packet("SEND", p_cmd_data, *cmd_len);
        p_cmd_data[6] = 0x60;

        phNxpNciHal_print_packet("RECV", p_rsp_data, 5);
//        status = NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config - End ");
    }

#if 0
    else if ( (p_cmd_data[0] == 0x20 && p_cmd_data[1] == 0x02 ) &&
                 ((p_cmd_data[2] == 0x09 && p_cmd_data[3] == 0x04) ||
                     (p_cmd_data[2] == 0x0B && p_cmd_data[3] == 0x05) ||
                     (p_cmd_data[2] == 0x07 && p_cmd_data[3] == 0x02) ||
                     (p_cmd_data[2] == 0x0A && p_cmd_data[3] == 0x03) ||
                     (p_cmd_data[2] == 0x0A && p_cmd_data[3] == 0x04) ||
                     (p_cmd_data[2] == 0x05 && p_cmd_data[3] == 0x02))
             )
    {
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config ");
        phNxpNciHal_print_packet("SEND", p_cmd_data, *cmd_len);
        *rsp_len = 5;
        p_rsp_data[0] = 0x40;
        p_rsp_data[1] = 0x02;
        p_rsp_data[2] = 0x02;
        p_rsp_data[3] = 0x00;
        p_rsp_data[4] = 0x00;

        phNxpNciHal_print_packet("RECV", p_rsp_data, 5);
        status = NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config - End ");
    }

    else if((p_cmd_data[0] == 0x20 && p_cmd_data[1] == 0x02) &&
           ((p_cmd_data[3] == 0x00) ||
           ((*cmd_len >= 0x06) && (p_cmd_data[5] == 0x00)))) /*If the length of the first param id is zero don't allow*/
    {
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config ");
        phNxpNciHal_print_packet("SEND", p_cmd_data, *cmd_len);
        *rsp_len = 5;
        p_rsp_data[0] = 0x40;
        p_rsp_data[1] = 0x02;
        p_rsp_data[2] = 0x02;
        p_rsp_data[3] = 0x00;
        p_rsp_data[4] = 0x00;

        phNxpNciHal_print_packet("RECV", p_rsp_data, 5);
        status = NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_D ("> Going through workaround - Dirty Set Config - End ");
    }
#endif
    else if ((wFwVerRsp & 0x0000FFFF) == wFwVer)
    {
        /* skip CORE_RESET and CORE_INIT from Brcm */
        if (p_cmd_data[0] == 0x20 &&
            p_cmd_data[1] == 0x00 &&
            p_cmd_data[2] == 0x01 &&
            p_cmd_data[3] == 0x01
            )
        {
//            *rsp_len = 6;
//
//            NXPLOG_NCIHAL_D("> Going - core reset optimization");
//
//            p_rsp_data[0] = 0x40;
//            p_rsp_data[1] = 0x00;
//            p_rsp_data[2] = 0x03;
//            p_rsp_data[3] = 0x00;
//            p_rsp_data[4] = 0x10;
//            p_rsp_data[5] = 0x01;
//
//            status = NFCSTATUS_FAILED;
//            NXPLOG_NCIHAL_D("> Going - core reset optimization - END");
        }
        /* CORE_INIT */
        else if (
            p_cmd_data[0] == 0x20 &&
            p_cmd_data[1] == 0x01 &&
            p_cmd_data[2] == 0x00
            )
        {
//            NXPLOG_NCIHAL_D("> Going - core init optimization");
//            *rsp_len = iCoreInitRspLen;
//            memcpy(p_rsp_data, bCoreInitRsp, iCoreInitRspLen);
//            status = NFCSTATUS_FAILED;
//            NXPLOG_NCIHAL_D("> Going - core init optimization - END");
        }
    }
    return status;
}

/******************************************************************************
 * Function         phNxpNciHal_send_ext_cmd
 *
 * Description      This function send the extension command to NFCC. No
 *                  response is checked by this function but it waits for
 *                  the response to come.
 *
 * Returns          Returns NFCSTATUS_SUCCESS if sending cmd is successful and
 *                  response is received.
 *
 ******************************************************************************/
NFCSTATUS phNxpNciHal_send_ext_cmd(uint16_t cmd_len, uint8_t *p_cmd)
{
    NFCSTATUS status = NFCSTATUS_FAILED;

    HAL_ENABLE_EXT();
    nxpncihal_ctrl.cmd_len = cmd_len;
    memcpy(nxpncihal_ctrl.p_cmd_data, p_cmd, cmd_len);
    status = phNxpNciHal_process_ext_cmd_rsp(nxpncihal_ctrl.cmd_len, nxpncihal_ctrl.p_cmd_data);
    HAL_DISABLE_EXT();

    return status;
}

/******************************************************************************
 * Function         hal_extns_write_rsp_timeout_cb
 *
 * Description      Timer call back function
 *
 * Returns          None
 *
 ******************************************************************************/
static void hal_extns_write_rsp_timeout_cb(uint32_t timerId, void *pContext)
{
    UNUSED(timerId);
    UNUSED(pContext);
    NXPLOG_NCIHAL_E("hal_extns_write_rsp_timeout_cb - write timeout!!!");
    nxpncihal_ctrl.ext_cb_data.status = NFCSTATUS_FAILED;
    usleep(1);
    SEM_POST(&(nxpncihal_ctrl.ext_cb_data));

    return;
}
