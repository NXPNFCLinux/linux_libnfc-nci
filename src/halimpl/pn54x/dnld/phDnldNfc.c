/*
 * Copyright (C) 2010-2014 NXP Semiconductors
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


/*
 * Download Component
 * Download Interface routines implementation
 */

#include <phDnldNfc_Internal.h>
#include <phTmlNfc.h>
#include <phNxpLog.h>
#include <dlfcn.h>

#define NXP_NFC_FW_STORE_PATH_SIZE  0x100

static void *pFwLibHandle; /* Global firmware lib handle used in this file only */

static char nfcFwStore[NXP_NFC_FW_STORE_PATH_SIZE];
static char nfcPlatformFwStore[NXP_NFC_FW_STORE_PATH_SIZE];
static char nfcPkuFwStore[NXP_NFC_FW_STORE_PATH_SIZE];

uint16_t wMwVer = 0; /* Middleware version no */
uint16_t wFwVer = 0; /* Firmware version no */

/* (NFC_NXP_CHIP_TYPE == PN548C2) */
 uint8_t gRecFWDwnld; // flag set to true to indicate dummy FW download

static pphDnldNfc_DlContext_t gpphDnldContext = NULL; /* Download contex */
static pphDnldNfc_RspCb_t      UserCb; /* Upper layer call back function */
static void*                   UserCtxt; /* Pointer to upper layer context */
#undef EEPROM_Read_Mem_IMP

/* Function prototype declaration */
static void phDnldNfc_ReadComplete(void* pContext, NFCSTATUS status, void* pInfo);
static char *phDnldNfc_GetFwPath(bool_t ignore_cfg);

/*******************************************************************************
**
** Function         phDnldNfc_Reset
**
** Description      Performs a soft reset of the download module
**
** Parameters       pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - reset request to NFCC is successful
**                  NFCSTATUS_FAILED      - reset request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_Reset(pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pNotify) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTNone;
            (gpphDnldContext->tCmdId) = PH_DL_CMD_RESET;
            (gpphDnldContext->tRspBuffInfo.pBuff) = NULL;
            (gpphDnldContext->tRspBuffInfo.wLen) = 0;
            (gpphDnldContext->tUserData.pBuff) = NULL;
            (gpphDnldContext->tUserData.wLen) = 0;
            (gpphDnldContext->UserCb) = pNotify;
            (gpphDnldContext->UserCtxt) = pContext;

            wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventReset);

            if(NFCSTATUS_PENDING == wStatus)
            {
                NXPLOG_FWDNLD_D("Reset Request submitted successfully");
            }
            else
            {
                NXPLOG_FWDNLD_E("Reset Request Failed!!");
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_GetVersion
**
** Description      Retrieves Hardware version, ROM Code version, Protected Data version,
**                  Trim data version, User data version, and Firmware version information
**
** Parameters       pVersionInfo - response buffer which gets updated with complete version info from NFCC
**                  pNotify      - notify caller after getting response
**                  pContext     - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - GetVersion request to NFCC is successful
**                  NFCSTATUS_FAILED      - GetVersion request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_GetVersion(pphDnldNfc_Buff_t pVersionInfo, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pVersionInfo) || (NULL == pNotify) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if((NULL != pVersionInfo->pBuff) && (0 != pVersionInfo->wLen))
            {
                (gpphDnldContext->tRspBuffInfo.pBuff) = pVersionInfo->pBuff;
                (gpphDnldContext->tRspBuffInfo.wLen) = pVersionInfo->wLen;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTNone;
                (gpphDnldContext->tCmdId) = PH_DL_CMD_GETVERSION;
                (gpphDnldContext->tUserData.pBuff) = NULL;
                (gpphDnldContext->tUserData.wLen) = 0;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventGetVer);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("GetVersion Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("GetVersion Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Buff Parameters!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_GetSessionState
**
** Description      Retrieves the current session state of NFCC
**
** Parameters       pSession - response buffer which gets updated with complete version info from NFCC
**                  pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - GetSessionState request to NFCC is successful
**                  NFCSTATUS_FAILED      - GetSessionState request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_GetSessionState(pphDnldNfc_Buff_t pSession, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pSession) || (NULL == pNotify) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if((NULL != pSession->pBuff) && (0 != pSession->wLen))
            {
                (gpphDnldContext->tRspBuffInfo.pBuff) = pSession->pBuff;
                (gpphDnldContext->tRspBuffInfo.wLen) = pSession->wLen;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTNone;
                (gpphDnldContext->tCmdId) = PH_DL_CMD_GETSESSIONSTATE;
                (gpphDnldContext->tUserData.pBuff) = NULL;
                (gpphDnldContext->tUserData.wLen) = 0;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventGetSesnSt);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("GetSessionState Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("GetSessionState Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Buff Parameters!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_CheckIntegrity
**
** Description      Inspects the integrity of EEPROM and FLASH contents of the NFCC,
**                  provides CRC for each section
**                  NOTE: The user data section CRC is valid only after fresh download
**
** Parameters       bChipVer - current ChipVersion for including additional parameters in request payload
**                  pCRCData - response buffer which gets updated with respective section CRC status
**                             and CRC bytes from NFCC
**                  pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - CheckIntegrity request is successful
**                  NFCSTATUS_FAILED      - CheckIntegrity request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_CheckIntegrity(uint8_t bChipVer, pphDnldNfc_Buff_t pCRCData, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pNotify) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if((PHDNLDNFC_HWVER_MRA2_1 == bChipVer) || (PHDNLDNFC_HWVER_MRA2_2 == bChipVer) ||
               (PHDNLDNFC_HWVER_PN548AD_MRA1_0 == bChipVer))
            {
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_ChkIntg;
            }
            else
            {
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTNone;
            }

            if((NULL != pCRCData->pBuff) && (0 != pCRCData->wLen))
            {
                (gpphDnldContext->tRspBuffInfo.pBuff) = pCRCData->pBuff;
                (gpphDnldContext->tRspBuffInfo.wLen) = pCRCData->wLen;
                (gpphDnldContext->tCmdId) = PH_DL_CMD_CHECKINTEGRITY;
                (gpphDnldContext->tUserData.pBuff) = NULL;
                (gpphDnldContext->tUserData.wLen) = 0;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventIntegChk);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("CheckIntegrity Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("CheckIntegrity Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Buff Parameters!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}
/*******************************************************************************
**
** Function         phDnldNfc_ReadLog
**
** Description      Retrieves log data from EEPROM
**
** Parameters       pData    - response buffer which gets updated with data from EEPROM
**                  pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - Read request to NFCC is successful
**                  NFCSTATUS_FAILED      - Read request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_ReadLog(pphDnldNfc_Buff_t pData, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pNotify) || (NULL == pData) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if((NULL != pData->pBuff) && (0 != pData->wLen))
            {
                (gpphDnldContext->tCmdId) = PH_DL_CMD_READ;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTRead;
                (gpphDnldContext->FrameInp.dwAddr) = PHDNLDNFC_EEPROM_LOG_START_ADDR;
                (gpphDnldContext->tRspBuffInfo.pBuff) = pData->pBuff;
                (gpphDnldContext->tRspBuffInfo.wLen) = pData->wLen;
                (gpphDnldContext->tUserData.pBuff) = NULL;
                (gpphDnldContext->tUserData.wLen) = 0;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                memset(&(gpphDnldContext->tRWInfo),0,sizeof(gpphDnldContext->tRWInfo));

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventRead);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("Read Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("Read Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Buff Parameters!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_Write
**
** Description      Writes requested  data of length len to desired EEPROM/FLASH address
**
** Parameters       bRecoverSeq - flag to indicate whether recover sequence data needs to be written or not
**                  pData       - data buffer to write into EEPROM/FLASH by user
**                  pNotify     - notify caller after getting response
**                  pContext    - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - Write request to NFCC is successful
**                  NFCSTATUS_FAILED      - Write request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_Write(bool_t  bRecoverSeq, pphDnldNfc_Buff_t pData, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;
    uint8_t *pImgPtr = NULL;
    uint16_t wLen = 0;
    phDnldNfc_Buff_t tImgBuff;

    if((NULL == pNotify) || (NULL == pContext))
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if(NULL != pData)
            {
                pImgPtr = pData->pBuff;
                wLen = pData->wLen;
            }
            else
            {
                if(FALSE == bRecoverSeq)
                {

                    pImgPtr = (uint8_t *)gpphDnldContext->nxp_nfc_fw;
                    wLen = gpphDnldContext->nxp_nfc_fw_len;

                }
                else
                {
                    if(PH_DL_STATUS_PLL_ERROR == (gpphDnldContext->tLastStatus))
                    {
                        wStatus = phDnldNfc_LoadRecInfo();
                    }
                    else if(PH_DL_STATUS_SIGNATURE_ERROR == (gpphDnldContext->tLastStatus))
                    {
                        wStatus = phDnldNfc_LoadPKInfo();
                    }
                    else
                    {
                    }

                    if(NFCSTATUS_SUCCESS == wStatus)
                    {
                        pImgPtr = (uint8_t *)gpphDnldContext->nxp_nfc_fwp;
                        wLen = gpphDnldContext->nxp_nfc_fwp_len;
                    }
                    else
                    {
                        NXPLOG_FWDNLD_E("Platform Recovery Image extraction Failed!!");
                        pImgPtr = NULL;
                        wLen = 0;
                    }
                }
            }

            if((NULL != pImgPtr) && (0 != wLen))
            {
                tImgBuff.pBuff = pImgPtr;
                tImgBuff.wLen = wLen;

                (gpphDnldContext->tCmdId) = PH_DL_CMD_WRITE;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTWrite;
                (gpphDnldContext->tRspBuffInfo.pBuff) = NULL;
                (gpphDnldContext->tRspBuffInfo.wLen) = 0;
                (gpphDnldContext->tUserData.pBuff) = pImgPtr;
                (gpphDnldContext->tUserData.wLen) = wLen;
                (gpphDnldContext->bResendLastFrame) = FALSE;

                memset(&(gpphDnldContext->tRWInfo),0,sizeof(gpphDnldContext->tRWInfo));
                (gpphDnldContext->tRWInfo.bFirstWrReq) = TRUE;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventWrite);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("Write Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("Write Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Download Image Primitives extraction failed!!");
                wStatus = NFCSTATUS_FAILED;
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_Log
**
** Description      Provides a full page free write to EEPROM
**
** Parameters       pData       - data buffer to write into EEPROM/FLASH by user
**                  pNotify     - notify caller after getting response
**                  pContext    - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - Write request to NFCC is successful
**                  NFCSTATUS_FAILED      - Write request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_Log(pphDnldNfc_Buff_t pData, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pNotify) || (NULL == pData) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if((NULL != (pData->pBuff)) && ((0 != (pData->wLen) && (PHDNLDNFC_MAX_LOG_SIZE >= (pData->wLen)))))
            {
                (gpphDnldContext->tCmdId) = PH_DL_CMD_LOG;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTLog;
                (gpphDnldContext->tRspBuffInfo.pBuff) = NULL;
                (gpphDnldContext->tRspBuffInfo.wLen) = 0;
                (gpphDnldContext->tUserData.pBuff) = (pData->pBuff);
                (gpphDnldContext->tUserData.wLen) = (pData->wLen);

                memset(&(gpphDnldContext->tRWInfo),0,sizeof(gpphDnldContext->tRWInfo));
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventLog);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("Log Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("Log Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Input Parameters for Log!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_Force
**
** Description      Used as an emergency recovery procedure for NFCC due to corrupt
**                  settings of system platform specific parameters by the host
**
** Parameters       pInputs  - input buffer which contains  clk src & clk freq settings for desired platform
**                  pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - Emergency Recovery request is successful
**                  NFCSTATUS_FAILED      - Emergency Recovery failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_Force(pphDnldNfc_Buff_t pInputs, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint8_t bClkSrc = 0x00,bClkFreq = 0x00;
    uint8_t bPldVal[3] = {0x11,0x00,0x00};  /* default values to be used if input not provided */

    if((NULL == pNotify) || (NULL == pContext))
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            (gpphDnldContext->tCmdId) = PH_DL_CMD_FORCE;
            (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTForce;
            (gpphDnldContext->tRspBuffInfo.pBuff) = NULL;
            (gpphDnldContext->tRspBuffInfo.wLen) = 0;

            if((0 != (pInputs->wLen)) || (NULL != (pInputs->pBuff)))
            {
               if(CLK_SRC_XTAL == (pInputs->pBuff[0]))
               {
                   bClkSrc = phDnldNfc_ClkSrcXtal;
               }
               else if(CLK_SRC_PLL == (pInputs->pBuff[0]))
               {
                   bClkSrc = phDnldNfc_ClkSrcPLL;
                   if(CLK_FREQ_13MHZ == (pInputs->pBuff[1]))
                   {
                       bClkFreq = phDnldNfc_ClkFreq_13Mhz;
                   }
                   else if(CLK_FREQ_19_2MHZ == (pInputs->pBuff[1]))
                   {
                       bClkFreq = phDnldNfc_ClkFreq_19_2Mhz;
                   }
                   else if(CLK_FREQ_24MHZ == (pInputs->pBuff[1]))
                   {
                       bClkFreq = phDnldNfc_ClkFreq_24Mhz;
                   }
                   else if(CLK_FREQ_26MHZ == (pInputs->pBuff[1]))
                   {
                       bClkFreq = phDnldNfc_ClkFreq_26Mhz;
                   }
                   else if(CLK_FREQ_38_4MHZ == (pInputs->pBuff[1]))
                   {
                       bClkFreq = phDnldNfc_ClkFreq_38_4Mhz;
                   }
                   else if(CLK_FREQ_52MHZ == (pInputs->pBuff[1]))
                   {
                       bClkFreq = phDnldNfc_ClkFreq_52Mhz;
                   }
                   else
                   {
                       NXPLOG_FWDNLD_E("Invalid Clk Frequency !! Using default value of 19.2Mhz..");
                       bClkFreq = phDnldNfc_ClkFreq_19_2Mhz;
                   }

               }
               else if(CLK_SRC_PADDIRECT == (pInputs->pBuff[0]))
               {
                   bClkSrc = phDnldNfc_ClkSrcPad;
               }
               else
               {
                   NXPLOG_FWDNLD_E("Invalid Clk src !! Using default value of PLL..");
                   bClkSrc = phDnldNfc_ClkSrcPLL;
               }

               bPldVal[0] = 0U;
               bPldVal[0] = ((bClkSrc << 3U) | bClkFreq);
            }
            else
            {
                NXPLOG_FWDNLD_E("Clk src inputs not provided!! Using default values..");
            }

            (gpphDnldContext->tUserData.pBuff) = bPldVal;
            (gpphDnldContext->tUserData.wLen) = sizeof(bPldVal);

            memset(&(gpphDnldContext->tRWInfo),0,sizeof(gpphDnldContext->tRWInfo));
            (gpphDnldContext->UserCb) = pNotify;
            (gpphDnldContext->UserCtxt) = pContext;

            wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventForce);

            if(NFCSTATUS_PENDING == wStatus)
            {
                NXPLOG_FWDNLD_D("Force Command Request submitted successfully");
            }
            else
            {
                NXPLOG_FWDNLD_E("Force Command Request Failed!!");
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_SetHwDevHandle
**
** Description      Stores the HwDev handle to download context. The handle is required for subsequent operations
**
** Parameters       None
**
** Returns          None                -
**
*******************************************************************************/
void phDnldNfc_SetHwDevHandle(void)
{
    pphDnldNfc_DlContext_t  psDnldContext = NULL;

    if(NULL == gpphDnldContext)
    {
        NXPLOG_FWDNLD_D("Allocating Mem for Dnld Context..");
        /* Create the memory for Download Mgmt Context */
        psDnldContext = (pphDnldNfc_DlContext_t)
                        malloc(sizeof(phDnldNfc_DlContext_t));

        if(psDnldContext != NULL)
        {
            (void ) memset((void *)psDnldContext,0,
                                            sizeof(phDnldNfc_DlContext_t));
            gpphDnldContext = psDnldContext;
        }
        else
        {
            NXPLOG_FWDNLD_E("Error Allocating Mem for Dnld Context..");
        }
    }
    else
    {
        (void ) memset((void *)gpphDnldContext,0,
                                            sizeof(phDnldNfc_DlContext_t));
    }
    return;
}

/*******************************************************************************
**
** Function         phDnldNfc_ReSetHwDevHandle
**
** Description      Frees the HwDev handle to download context.
**
** Parameters       None
**
** Returns          None                -
**
*******************************************************************************/
void phDnldNfc_ReSetHwDevHandle(void)
{
    if (gpphDnldContext != NULL)
    {
        NXPLOG_FWDNLD_E("Freeing Mem for Dnld Context..");
        free(gpphDnldContext);
        gpphDnldContext = NULL;
    }
}


/*******************************************************************************
**
** Function         phDnldNfc_RawReq
**
** Description      Sends raw frame request to NFCC.
**                  It is currently used for sending an NCI RESET cmd after doing a production key update
**
** Parameters       pFrameData - input buffer, contains raw frame packet to be sent to NFCC
**                  pRspData   - response buffer received from NFCC
**                  pNotify    - notify caller after getting response
**                  pContext   - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - GetSessionState request to NFCC is successful
**                  NFCSTATUS_FAILED      - GetSessionState request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_RawReq(pphDnldNfc_Buff_t pFrameData, pphDnldNfc_Buff_t pRspData, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pFrameData) || (NULL == pNotify) || (NULL == pRspData) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Raw Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if(((NULL != pFrameData->pBuff) && (0 != pFrameData->wLen)) &&
                ((NULL != pRspData->pBuff) && (0 != pRspData->wLen))
                )
            {
                (gpphDnldContext->tRspBuffInfo.pBuff) = pRspData->pBuff;
                (gpphDnldContext->tRspBuffInfo.wLen) = pRspData->wLen;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTRaw;
                (gpphDnldContext->tCmdId) = PH_DL_CMD_NONE;
                (gpphDnldContext->tUserData.pBuff) = pFrameData->pBuff;
                (gpphDnldContext->tUserData.wLen) = pFrameData->wLen;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventRaw);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("RawFrame Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("RawFrame Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Buff Parameters!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_InitImgInfo
**
** Description      Extracts image information and stores it in respective variables,
**                  to be used internally for write operation
**
** Parameters       None
**
** Returns          NFC status
**
*******************************************************************************/
NFCSTATUS phDnldNfc_InitImgInfo(void)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint8_t *pImageInfo =  NULL;
    uint16_t ImageInfoLen = 0;
    char* pathName = NULL;

    /* if memory is not allocated then allocate memory for donwload context structure */
    phDnldNfc_SetHwDevHandle();

    pathName = phDnldNfc_GetFwPath(FALSE);

    /* load the library and get the image info pointer */
    if ((gRecFWDwnld == TRUE) && (phNxpNciHal_getChipType() == pn548C2))
    {
        wStatus = phDnldNfc_LoadRecoveryFW (pathName, &pImageInfo, &ImageInfoLen);
    }
    else
    {
        wStatus = phDnldNfc_LoadFW (pathName, &pImageInfo, &ImageInfoLen);
    }

    NXPLOG_FWDNLD_D("FW Image Length - ImageInfoLen %d",ImageInfoLen);
    NXPLOG_FWDNLD_D("FW Image Info Pointer - pImageInfo %x",(uintptr_t)pImageInfo);

    if((pImageInfo == NULL) || (ImageInfoLen == 0))
    {
        NXPLOG_FWDNLD_E("Image extraction Failed - invalid imginfo or imginfolen!!");
        wStatus = NFCSTATUS_FAILED;
    }

    if (wStatus != NFCSTATUS_SUCCESS)
    {
        NXPLOG_FWDNLD_E("Error loading libpn54x_fw !!\n");
    }

   /* get the MW version */
   if(NFCSTATUS_SUCCESS == wStatus)
   {
       //NXPLOG_FWDNLD_D("MW Major Version Num - %x",NXP_MW_VERSION_MAJ);
       //NXPLOG_FWDNLD_D("MW Minor Version Num - %x",NXP_MW_VERSION_MIN);
       wMwVer = (((uint16_t)(NXP_MW_VERSION_MAJ) << 8U) | (NXP_MW_VERSION_MIN));
   }

   if(NFCSTATUS_SUCCESS == wStatus)
   {
       gpphDnldContext->nxp_nfc_fw = (uint8_t*)pImageInfo;
       gpphDnldContext->nxp_nfc_fw_len = ImageInfoLen;
       if((NULL != gpphDnldContext->nxp_nfc_fw) && (0 != gpphDnldContext->nxp_nfc_fw_len))
       {
           NXPLOG_FWDNLD_D("FW Major Version Num - %x",gpphDnldContext->nxp_nfc_fw[5]);
           NXPLOG_FWDNLD_D("FW Minor Version Num - %x",gpphDnldContext->nxp_nfc_fw[4]);
           NXPLOG_FWDNLD_D("FW Image Length - %d",ImageInfoLen);
           NXPLOG_FWDNLD_D("FW Image Info Pointer - %x",(uintptr_t)pImageInfo);

           /* get the FW version */
           wFwVer = (((uint16_t)(gpphDnldContext->nxp_nfc_fw[5]) << 8U) | (gpphDnldContext->nxp_nfc_fw[4]));
           wStatus = NFCSTATUS_SUCCESS;
       }
       else
       {
           NXPLOG_FWDNLD_E("Image details extraction Failed!!");
           wStatus = NFCSTATUS_FAILED;
       }
   }

   return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_LoadRecInfo
**
** Description      Extracts recovery sequence image information and stores it
**                  in respective variables, to be used internally for write operation
**
** Parameters       None
**
** Returns          NFC status
**
*******************************************************************************/
NFCSTATUS phDnldNfc_LoadRecInfo(void)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint8_t *pImageInfo = NULL;
    uint16_t ImageInfoLen = 0;
    char* pathName = NULL;

    /* if memory is not allocated then allocate memory for donwload context structure */
    phDnldNfc_SetHwDevHandle();

    pathName = phDnldNfc_GetFwPath(FALSE);

    /* load the library and get the image info pointer */
    if ((gRecFWDwnld == TRUE) && (phNxpNciHal_getChipType() == pn548C2))
    {
        wStatus = phDnldNfc_LoadRecoveryFW (nfcPlatformFwStore, &pImageInfo, &ImageInfoLen);
    }
    else
    {
        wStatus = phDnldNfc_LoadFW (nfcPlatformFwStore, &pImageInfo, &ImageInfoLen);
    }

    if((pImageInfo == NULL) || (ImageInfoLen == 0))
    {
        NXPLOG_FWDNLD_E("Image extraction Failed - invalid imginfo or imginfolen!!");
        wStatus = NFCSTATUS_FAILED;
    }

    /* load the PLL recovery image library */
    if (wStatus != NFCSTATUS_SUCCESS)
    {
        NXPLOG_FWDNLD_E("Error loading libpn54x_fw_platform !!\n");
    }

   if(NFCSTATUS_SUCCESS == wStatus)
   {
       /* fetch the PLL recovery image pointer and the image length */
       gpphDnldContext->nxp_nfc_fwp = (uint8_t*)pImageInfo;
       gpphDnldContext->nxp_nfc_fwp_len = ImageInfoLen;
       if((NULL != gpphDnldContext->nxp_nfc_fwp) && (0 != gpphDnldContext->nxp_nfc_fwp_len))
       {
           NXPLOG_FWDNLD_D("Recovery Image Length - %d",ImageInfoLen);
           NXPLOG_FWDNLD_D("Recovery Image Info Pointer - %x",(uintptr_t)pImageInfo);
           wStatus = NFCSTATUS_SUCCESS;
       }
       else
       {
           NXPLOG_FWDNLD_E("Recovery Image details extraction Failed!!");
           wStatus = NFCSTATUS_FAILED;
       }
   }

   return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_LoadPKInfo
**
** Description      Extracts production sequence image information and stores it
**                  in respective variables, to be used internally for write operation
**
** Parameters       None
**
** Returns          NFC status
**
*******************************************************************************/
NFCSTATUS phDnldNfc_LoadPKInfo(void)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint8_t *pImageInfo = NULL;
    uint16_t ImageInfoLen = 0;
    char* pathName = NULL;

    /* if memory is not allocated then allocate memory for donwload context structure */
    phDnldNfc_SetHwDevHandle();

    pathName = phDnldNfc_GetFwPath(FALSE);

    /* load the PKU image library */
    if ((gRecFWDwnld == TRUE) && (phNxpNciHal_getChipType() == pn548C2))
    {
        wStatus = phDnldNfc_LoadRecoveryFW (nfcPkuFwStore, &pImageInfo, &ImageInfoLen);
    }
    else
    {
        wStatus = phDnldNfc_LoadFW (nfcPkuFwStore, &pImageInfo, &ImageInfoLen);
    }

    if((pImageInfo == NULL) || (ImageInfoLen == 0))
    {
        NXPLOG_FWDNLD_E("Image extraction Failed - invalid imginfo or imginfolen!!");
        wStatus = NFCSTATUS_FAILED;
    }

    if (wStatus != NFCSTATUS_SUCCESS)
    {
        NXPLOG_FWDNLD_E("Error loading libpn54x_fw_pku !!\n");
    }

   if(NFCSTATUS_SUCCESS == wStatus)
   {
       /* fetch the PKU image pointer and the image length */
       gpphDnldContext->nxp_nfc_fwp = (uint8_t*)pImageInfo;
       gpphDnldContext->nxp_nfc_fwp_len = ImageInfoLen;

       if((NULL != gpphDnldContext->nxp_nfc_fwp) && (0 != gpphDnldContext->nxp_nfc_fwp_len))
       {
           NXPLOG_FWDNLD_D("PKU Image Length - %d",ImageInfoLen);
           NXPLOG_FWDNLD_D("PKU Image Info Pointer - %x",(uintptr_t)pImageInfo);
           wStatus = NFCSTATUS_SUCCESS;
       }
       else
       {
           NXPLOG_FWDNLD_E("PKU Image details extraction Failed!!");
           wStatus = NFCSTATUS_FAILED;
       }
   }

   return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_CloseFwLibHandle
**
** Description      Closes previously opened fw library handle as part of
**                  dynamic loader processing
**
** Parameters       None
**
** Returns          None
**
*******************************************************************************/
void phDnldNfc_CloseFwLibHandle(void)
{
    NFCSTATUS wStatus;

    wStatus = phDnldNfc_UnloadFW();
    if(wStatus != NFCSTATUS_SUCCESS)
    {
        NXPLOG_FWDNLD_E("free library FAILED !!\n");
    }
    else
    {
        NXPLOG_FWDNLD_E("free library SUCCESS !!\n");
    }
    return;
}

/*******************************************************************************
**
** Function         phDnldNfc_GetFwPath
**
** Description      Gets the Firmware Binary path from the configuration file
**                  or load the default file name
**
** Parameters       ignore_cfg
**
** Returns          returns the firmware binary path name
**
*******************************************************************************/
static char *phDnldNfc_GetFwPath(bool_t ignore_cfg)
{
    char fwFileName[NXP_NFC_FW_STORE_PATH_SIZE];
    char fwFileNameCfg[NXP_NFC_FW_STORE_PATH_SIZE];
    char fwFilePath[NXP_NFC_FW_STORE_PATH_SIZE];
    bool_t fw_name_default = FALSE;

    memset (&fwFilePath, 0, sizeof(fwFilePath));
    memset (&nfcPlatformFwStore, 0, sizeof(fwFilePath));
    memset (&nfcPkuFwStore, 0, sizeof(fwFilePath));

    /*Read Firmware Path name from config file*/
    if ((GetNxpStrValue( NAME_NXP_FW_PATH,
           (char*)fwFilePath, NXP_NFC_FW_STORE_PATH_SIZE) != TRUE)
        || (TRUE == ignore_cfg)
        )
    {
        strcpy(fwFilePath, FW_BIN_ROOT_DIR);
        NXPLOG_FWDNLD_E("Ignoring Config :NFC FW Binary Path:%s\n",fwFilePath);
    }

    /* check for path name */
    if(phNxpNciHal_getChipType() == pn548C2)
    {
        strcpy(fwFileName, "libpn548ad");
    }
    else if(phNxpNciHal_getChipType() == pn551)
    {
        strcpy(fwFileName, "libpn551");
    }
    else if(phNxpNciHal_getChipType() == pn553)
    {
        strcpy(fwFileName, "libpn553");
    }
    else if(phNxpNciHal_getChipType() == pn557)
    {
        strcpy(fwFileName, "libpn557");
    }
    else
    {
        strcpy(fwFileName, "libpn547");
    }

    /*Read Firmware file name from config file*/
    if ((GetNxpStrValue(NAME_NXP_FW_NAME,
              (char*)fwFileNameCfg, sizeof(fwFileNameCfg)) != TRUE)
        || (TRUE == ignore_cfg)
        )
    {
        fw_name_default = TRUE;
    }

    if (TRUE == fw_name_default)
    {
        sprintf(nfcFwStore,"%s/%s%s%s",
                        fwFilePath, fwFileName, "_fw", FW_BIN_EXTENSION);
        NXPLOG_FWDNLD_E("Ignoring Config:NFC FW Binary :%s\n", fwFileName);
    }
    else
    {
        sprintf(nfcFwStore,"%s/%s", fwFilePath, fwFileNameCfg);
    }
    sprintf(nfcPlatformFwStore,"%s/%s%s%s",
                    fwFilePath, fwFileName, "_fw_platform", FW_BIN_EXTENSION);
    sprintf(nfcPkuFwStore,"%s/%s%s%s",
                    fwFilePath, fwFileName, "_fw_pku", FW_BIN_EXTENSION);


    NXPLOG_FWDNLD_E("NFC FW Binary :%s\n",nfcFwStore);
#if 0
    NXPLOG_FWDNLD_E("NFC FW Platform Binary:%s\n",nfcFwStore);
    NXPLOG_FWDNLD_E("NFC FW PKU Binary:%s\n",nfcFwStore);
#endif

    return (char *)nfcFwStore;
}


/*******************************************************************************
**
** Function         phDnldNfc_LoadFW
**
** Description      Load the firmware version form firmware lib
**
** Parameters       pathName    - Firmware image path
**                  pImgInfo    - Firmware image handle
**                  pImgInfoLen - Firmware image length
**
** Returns          NFC status
**
*******************************************************************************/
NFCSTATUS phDnldNfc_LoadFW(const char* pathName, uint8_t **pImgInfo, uint16_t* pImgInfoLen)
{
    void* pImageInfo = NULL;
    void* pImageInfoLen = NULL;

    /* Check the Path Name for Null */
    if(NULL == pathName)
    {
        pathName = phDnldNfc_GetFwPath(TRUE);
    }

    /* check if the handle is not NULL then free the library */
    if(pFwLibHandle != NULL)
    {
        phDnldNfc_UnloadFW();
        pFwLibHandle = NULL;
    }
    /* load the DLL file */
    pFwLibHandle = dlopen(pathName, RTLD_LAZY);
    NXPLOG_FWDNLD_D ("@@@%s", pathName);

    /* if library load failed then handle will be NULL */
    if(pFwLibHandle == NULL)
    {
        NXPLOG_FWDNLD_E("NULL handler : unable to load the binary file, verify binary path");
        return NFCSTATUS_FAILED;
    }

    dlerror();    /* Clear any existing error */

    /* load the address of download image pointer and image size */
   pImageInfo     =  (void*)dlsym(pFwLibHandle,"gphDnldNfc_DlSeq");

   if(dlerror()|| (NULL == pImageInfo))
   {
       NXPLOG_FWDNLD_E("Problem loading symbol : gphDnldNfc_DlSeq");
       return NFCSTATUS_FAILED;
   }
   (*pImgInfo) = (*(uint8_t**)pImageInfo);

   pImageInfoLen  = (void*)dlsym(pFwLibHandle,"gphDnldNfc_DlSeqSz");
   if(dlerror() || (NULL == pImageInfoLen))
   {
       NXPLOG_FWDNLD_E("Problem loading symbol : gphDnldNfc_DlSeqSz");
       return NFCSTATUS_FAILED;
   }

   (*pImgInfoLen) = (uint16_t)(*((uint16_t*)pImageInfoLen));

    return NFCSTATUS_SUCCESS;
}


/*******************************************************************************
**
** Function         phDnldNfc_UnloadFW
**
** Description      Deinit the firmware handle
**
** Parameters       None
**
** Returns          NFC status
**
*******************************************************************************/
NFCSTATUS phDnldNfc_UnloadFW(void)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    int32_t   status;

    /* check if the handle is not NULL then free the library */
    if(pFwLibHandle != NULL)
    {
        status = dlclose(pFwLibHandle);
        pFwLibHandle = NULL;

        dlerror();    /* Clear any existing error */
        if(status != 0)
        {
            wStatus = NFCSTATUS_FAILED;
            NXPLOG_FWDNLD_E("Free library file failed");
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_LoadRecoveryFW
**
** Description      Load the dummy firmware version form firmware lib for recovery
**                  This will change the FW version of the NFCC firmware
**                  and enable flashing of firmware of same version.
**
** Parameters       pathName    - Firmware image path
**                  pImgInfo    - Firmware image handle
**                  pImgInfoLen - Firmware image length
**
** Returns          NFCSTATUS
**
*******************************************************************************/
NFCSTATUS phDnldNfc_LoadRecoveryFW (const char* pathName, uint8_t **pImgInfo, uint16_t* pImgInfoLen)
{
    void* pImageInfo = NULL;
    void* pImageInfoLen = NULL;

    /* Check the Path Name for Null */
    if(NULL == pathName)
    {
        pathName = phDnldNfc_GetFwPath(TRUE);
    }

    /* check if the handle is not NULL then free the library */
    if (pFwLibHandle != NULL)
    {
        phDnldNfc_UnloadFW ();
        pFwLibHandle = NULL;
    }
    /* load the DLL file */
    pFwLibHandle = dlopen (pathName, RTLD_LAZY);
    NXPLOG_FWDNLD_D ("phDnldNfc_LoadRecoveryFW %s ", pathName);

    /* if library load failed then handle will be NULL */
    if (pFwLibHandle == NULL)
    {
        NXPLOG_FWDNLD_E("NULL handler : unable to load the binary file, verify path");
        return NFCSTATUS_FAILED;
    }

    dlerror ();    /* Clear any existing error */

    /* load the address of download image pointer and image size */
    pImageInfo =  (void*)dlsym (pFwLibHandle, "gphDnldNfc_DummyDlSeq");

    if (dlerror() || (NULL == pImageInfo))
    {
        NXPLOG_FWDNLD_E ("Problem loading symbol : gphDnldNfc_DummyDlSeq");
        return NFCSTATUS_FAILED;
    }

    (*pImgInfo) = (*(uint8_t**)pImageInfo);
    pImageInfoLen  = (void*)dlsym (pFwLibHandle, "gphDnldNfc_DlSeqDummyFwSz");
    if (dlerror() ||(NULL == pImageInfoLen))
    {
       NXPLOG_FWDNLD_E ("Problem loading symbol : gphDnldNfc_DlSeqDummyFwSz");
       return NFCSTATUS_FAILED;
    }

    (*pImgInfoLen) = (uint16_t)(*((uint16_t*)pImageInfoLen));

    return NFCSTATUS_SUCCESS;
}

#ifdef EEPROM_Read_Mem_IMP
/*******************************************************************************
**
** Function         phDnldNfc_ReadMem
**
** Description      Dumps the contents of EEPROM. The handle is required for subsequent operations
**
** Parameters       pHwRef   - pointer to the hardware device
**                  pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - request to NFCC is successful
**                  NFCSTATUS_FAILED      - request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_ReadMem(void *pHwRef, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    uint32_t wAddr = 0x2011C0; /* eeprom platform specific area start address */
    uint32_t wRdAddr = 0;
    uint8_t *pAddr;
    static uint8_t bRdData[3519];  /* buffer to hold the read data */
    static phDnldNfc_Buff_t Data;

    if((NULL == pNotify) || (NULL == pContext))
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        /* Call Tml Ioctl to enable download mode */
        wStatus = phTmlNfc_IoCtl(phTmlNfc_e_EnableDownloadMode);

        if(NFCSTATUS_SUCCESS == wStatus)
        {
           /* Set the obtained device handle to download module */
           phDnldNfc_SetHwDevHandle();
        }
        else
        {
            wStatus = NFCSTATUS_FAILED;
        }

        if(NFCSTATUS_SUCCESS == wStatus)
        {
            pAddr = (uint8_t *)&wAddr;

            wRdAddr = (pAddr[3]);
            wRdAddr <<= 8;
            wRdAddr |= (pAddr[2]);
            wRdAddr <<= 8;
            wRdAddr |= (pAddr[1]);
            wRdAddr <<= 8;
            wRdAddr |= (pAddr[0]);

            Data.pBuff = bRdData;
            Data.wLen = sizeof(bRdData);
            UserCb = pNotify;
            UserCtxt = pContext;

            wStatus = phDnldNfc_Read(&Data, wRdAddr,(pphDnldNfc_RspCb_t)phDnldNfc_ReadComplete,gpphDnldContext);
        }
        else
        {
            Data.pBuff = NULL;
            Data.wLen = 0;
            wStatus = NFCSTATUS_FAILED;
        }

        if(NFCSTATUS_PENDING == wStatus)
        {
            NXPLOG_FWDNLD_D("Read Request submitted successfully..");
        }
        else
        {
            NXPLOG_FWDNLD_E("Read Request submission failed!!");
        }
    }

    return wStatus;
}

/*******************************************************************************
**
** Function         phDnldNfc_ReadComplete
**
** Description      Read complete
**
** Parameters       pContext - caller layer context
**                  status   - status of the transaction
**                  pInfo    - transaction info
**
** Returns          None
**
*******************************************************************************/
static void phDnldNfc_ReadComplete(void* pContext,NFCSTATUS status,void* pInfo)
{
    NFCSTATUS wStatus = NFCSTATUS_SUCCESS;
    UNUSED(pContext);

    /* Call Tml Ioctl to enable/restore normal mode */
    wStatus = phTmlNfc_IoCtl(phTmlNfc_e_EnableNormalMode);

    if(NFCSTATUS_SUCCESS == wStatus)
    {
        NXPLOG_FWDNLD_D("Read Done!!");
    }

    UserCb(&UserCtxt,status,pInfo);

    return;
}

/*******************************************************************************
**
** Function         phDnldNfc_Read
**
** Description      Retrieves requested data of specified length from desired EEPROM address
**
** Parameters       pData    - response buffer which gets updated with data from EEPROM
**                  dwRdAddr - EEPROM address for data read
**                  pNotify  - notify caller after getting response
**                  pContext - caller context
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS     - Read request to NFCC is successful
**                  NFCSTATUS_FAILED      - Read request failed due to internal error
**                  NFCSTATUS_NOT_ALLOWED - command not allowed
**                  Other command specific errors                -
**
*******************************************************************************/
NFCSTATUS phDnldNfc_Read(pphDnldNfc_Buff_t pData, uint32_t dwRdAddr, pphDnldNfc_RspCb_t pNotify, void *pContext)
{
    NFCSTATUS   wStatus = NFCSTATUS_SUCCESS;

    if((NULL == pNotify) || (NULL == pData) ||
       (NULL == pContext)
       )
    {
        NXPLOG_FWDNLD_E("Invalid Input Parameters!!");
        wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
    }
    else
    {
        if(phDnldNfc_TransitionIdle != gpphDnldContext->tDnldInProgress)
        {
            NXPLOG_FWDNLD_E("Dnld Cmd Request in Progress..Cannot Continue!!");
            wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_BUSY);
        }
        else
        {
            if((NULL != pData->pBuff) && (0 != pData->wLen))
            {
                (gpphDnldContext->tCmdId) = PH_DL_CMD_READ;
                (gpphDnldContext->FrameInp.Type) = phDnldNfc_FTRead;
                (gpphDnldContext->FrameInp.dwAddr) = dwRdAddr;
                (gpphDnldContext->tRspBuffInfo.pBuff) = pData->pBuff;
                (gpphDnldContext->tRspBuffInfo.wLen) = pData->wLen;
                (gpphDnldContext->tUserData.pBuff) = NULL;
                (gpphDnldContext->tUserData.wLen) = 0;
                (gpphDnldContext->UserCb) = pNotify;
                (gpphDnldContext->UserCtxt) = pContext;

                memset(&(gpphDnldContext->tRWInfo),0,sizeof(gpphDnldContext->tRWInfo));

                wStatus = phDnldNfc_CmdHandler(gpphDnldContext,phDnldNfc_EventRead);

                if(NFCSTATUS_PENDING == wStatus)
                {
                    NXPLOG_FWDNLD_D("Read Request submitted successfully");
                }
                else
                {
                    NXPLOG_FWDNLD_E("Read Request Failed!!");
                }
            }
            else
            {
                NXPLOG_FWDNLD_E("Invalid Buff Parameters!!");
                wStatus = PHNFCSTVAL(CID_NFC_DNLD, NFCSTATUS_INVALID_PARAMETER);
            }
        }
    }

    return wStatus;
}
#endif
