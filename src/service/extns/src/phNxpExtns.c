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

#ifdef ESE_NFC_SYNCHRONIZATION
#include <linux/ese-nfc-sync.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <phNxpExtns_MifareStd.h>
#include <phNxpLog.h>
#include <phNxpConfig.h>

extern phNxpExtns_Context_t gphNxpExtns_MifareStd_Context;
extern phFriNfc_NdefMap_t    *NdefMap;
extern phNci_mfc_auth_cmd_t  gAuthCmdBuf;

static NFCSTATUS phNxpExtns_ProcessSysMessage(phLibNfc_Message_t * msg);
static NFCSTATUS phNxpExtns_SendMsg(phLibNfc_Message_t * sysmsg);

#ifdef ESE_NFC_SYNCHRONIZATION
/* timing calculation structure*/
typedef struct time_cal
{
    struct timeval  tv1;
    struct timeval tv2;
}TimeCal;
static int fd_ese_nfc_sync; /*file descriptor to hold sync driver handle*/
#endif

/*******************************************************************************
**
** Function         EXTNS_Init
**
** Description      This function Initializes Mifare Classic Extns. Allocates
**                  required memory and initializes the Mifare Classic Vars
**
** Returns          NFCSTATUS_SUCCESS if successfully initialized
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_Init(tNFA_DM_CBACK        *p_nfa_dm_cback,
                     tNFA_CONN_CBACK      *p_nfa_conn_cback)
{
    NFCSTATUS status = NFCSTATUS_FAILED;

    /* reset config cache */
    resetNxpConfig();

    /* Initialize Log level */
    phNxpLog_InitializeLogLevel();

    /* Validate parameters */
    if( (!p_nfa_dm_cback) || (!p_nfa_conn_cback) )
    {
        NXPLOG_EXTNS_E ("EXTNS_Init(): error null callback");
        goto clean_and_return;
    }

    gphNxpExtns_MifareStd_Context.p_dm_cback = p_nfa_dm_cback;
    gphNxpExtns_MifareStd_Context.p_conn_cback = p_nfa_conn_cback;

    if( NFCSTATUS_SUCCESS != phNxpExtns_MfcModuleInit() )
    {
       NXPLOG_EXTNS_E("ERROR: MFC Module Init Failed");
       goto clean_and_return;
    }
    gphNxpExtns_MifareStd_Context.Extns_status = EXTNS_STATUS_OPEN;

    status = NFCSTATUS_SUCCESS;
    return status;

clean_and_return:
    gphNxpExtns_MifareStd_Context.Extns_status = EXTNS_STATUS_CLOSE;
    return status;
}

/*******************************************************************************
**
** Function         EXTNS_Close
**
** Description      This function de-initializes Mifare Classic Extns.
**                  De-allocates memory
**
** Returns          None
**
*******************************************************************************/
void EXTNS_Close(void)
{
    gphNxpExtns_MifareStd_Context.Extns_status = EXTNS_STATUS_CLOSE;
    phNxpExtns_MfcModuleDeInit();
    return;
}

/*******************************************************************************
**
** Function         EXTNS_MfcCallBack
**
** Description      Decodes Mifare Classic Tag Response
**                  This is called from NFA_SendRaw Callback
**
** Returns:
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcCallBack(uint8_t *buf, uint32_t buflen)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_RX_DATA;
    msg.pMsgData = buf;
    msg.Size = buflen;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }
    return status;
}

/*******************************************************************************
**
** Function         EXTNS_MfcCheckNDef
**
** Description      Performs NDEF detection for Mifare Classic Tag
**
**                  Upon successful completion of NDEF detection, a
**                  NFA_NDEF_DETECT_EVT will be sent, to notify the application
**                  of the NDEF attributes (NDEF total memory size, current
**                  size, etc.).
**
** Returns:
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcCheckNDef(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_CHECK_NDEF;
    msg.pMsgData = NULL;
    msg.Size = 0;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*******************************************************************************
**
** Function         EXTNS_MfcReadNDef
**
** Description      Reads NDEF message from Mifare Classic Tag.
**
**                  Upon receiving the NDEF message, the message will be sent to
**                  the handler registered with EXTNS_MfcRegisterNDefTypeHandler.
**
** Returns:
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcReadNDef(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_READ_NDEF;
    msg.pMsgData = NULL;
    msg.Size = 0;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}
/*******************************************************************************
**
** Function         EXTNS_MfcPresenceCheck
**
** Description      Do the check presence for Mifare Classic Tag.
**
**
** Returns:
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcPresenceCheck(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_PRESENCE_CHECK;
    msg.pMsgData = NULL;
    msg.Size = 0;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_D("Error Sending msg to Extension Thread");
    }

    return status;
}

/*******************************************************************************
**
** Function        EXTNS_MfcSetReadOnly
**
**
** Description:
**      Sets tag as read only.
**
**      When tag is set as read only, or if an error occurs, the app will be
**      notified with NFA_SET_TAG_RO_EVT.
**
** Returns:
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcSetReadOnly (uint8_t *key, uint8_t len)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_READ_ONLY;
    msg.pMsgData = key;
    msg.Size = len;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*******************************************************************************
**
** Function         EXTNS_MfcWriteNDef
**
** Description      Writes NDEF data to Mifare Classic Tag.
**
**                  When the entire message has been written, or if an error
**                  occurs, the app will be notified with NFA_WRITE_CPLT_EVT.
**
**                  p_data needs to be persistent until NFA_WRITE_CPLT_EVT
**
** Returns:
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcWriteNDef(uint8_t *p_data, uint32_t len)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_WRITE_NDEF;
    msg.pMsgData = p_data;
    msg.Size = len;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*****************************************************************************
**
** Function         EXTNS_MfcFormatTag
**
** Description      Formats Mifare Classic Tag.
**
**                  The NFA_RW_FORMAT_CPLT_EVT, status is used to
**                  indicate if tag is successfully formated or not
**
** Returns
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*****************************************************************************/
NFCSTATUS EXTNS_MfcFormatTag(uint8_t *key, uint8_t len)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_FORMAT_NDEF;
    msg.pMsgData = key;
    msg.Size = len;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*****************************************************************************
**
** Function         EXTNS_MfcDisconnect
**
** Description      Disconnects Mifare Classic Tag.
**
** Returns
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*****************************************************************************/
NFCSTATUS EXTNS_MfcDisconnect(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_DISCONNECT;
    msg.pMsgData = NULL;
    msg.Size = 0;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*****************************************************************************
**
** Function         EXTNS_MfcActivated
**
** Description      Activates Mifare Classic Tag.
**
** Returns
**                  NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*****************************************************************************/
NFCSTATUS EXTNS_MfcActivated(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_ACTIVATED;
    msg.pMsgData = NULL;
    msg.Size = 0;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*******************************************************************************
**
** Function         EXTNS_MfcTransceive
**
** Description      Sends raw frame to Mifare Classic Tag.
**
** Returns          NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcTransceive(uint8_t *p_data, uint32_t len)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phLibNfc_Message_t msg;

    msg.eMsgType = PH_NXPEXTNS_MIFARE_TRANSCEIVE;
    msg.pMsgData = p_data;
    msg.Size = len;

    status = phNxpExtns_SendMsg(&msg);
    if( NFCSTATUS_SUCCESS != status )
    {
        NXPLOG_EXTNS_E("Error Sending msg to Extension Thread");
    }

    return status;
}

/*******************************************************************************
**
** Function         EXTNS_MfcInit
**
** Description      This function is used to Init Mifare Classic Extns.
**                  This function should be called when the tag detected is
**                  Mifare Classic.
**
** Returns          NFCSTATUS_SUCCESS
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcInit(tNFA_ACTIVATED activationData)
{
    tNFC_ACTIVATE_DEVT rfDetail = activationData.activate_ntf;

    NdefMap->psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.Sak     = rfDetail.rf_tech_param.param.pa.sel_rsp;
    NdefMap->psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.AtqA[0] = rfDetail.rf_tech_param.param.pa.sens_res[0];
    NdefMap->psRemoteDevInfo->RemoteDevInfo.Iso14443A_Info.AtqA[1] = rfDetail.rf_tech_param.param.pa.sens_res[1];

    return NFCSTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phNxpExtns_ProcessSysMessage
**
** Description      Internal function to route the request from JNI and Callback
**                  from NFA_SendRawFrame to right function
**
** Returns          NFCSTATUS_SUCCESS if valid request
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
static NFCSTATUS phNxpExtns_ProcessSysMessage (phLibNfc_Message_t *msg)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    if(gphNxpExtns_MifareStd_Context.Extns_status == EXTNS_STATUS_CLOSE)
    {
        return NFCSTATUS_FAILED;
    }

    switch( msg->eMsgType )
    {
        case PH_NXPEXTNS_RX_DATA:
            status = Mfc_RecvPacket(msg->pMsgData, msg->Size);
            break;

        case PH_NXPEXTNS_MIFARE_CHECK_NDEF:
            pthread_mutex_init(&gAuthCmdBuf.syncmutex, NULL);
            pthread_mutex_lock(&gAuthCmdBuf.syncmutex);
            status = Mfc_CheckNdef();
            pthread_mutex_unlock(&gAuthCmdBuf.syncmutex);
            pthread_mutex_destroy(&gAuthCmdBuf.syncmutex);
            break;

        case PH_NXPEXTNS_MIFARE_READ_NDEF:
            status = Mfc_ReadNdef();
            break;

        case PH_NXPEXTNS_MIFARE_WRITE_NDEF:
            status = Mfc_WriteNdef(msg->pMsgData, msg->Size);
            break;

        case PH_NXPEXTNS_MIFARE_FORMAT_NDEF:
            status = Mfc_FormatNdef(msg->pMsgData,msg->Size);
            break;

        case PH_NXPEXTNS_DISCONNECT:
            Mfc_DeactivateCbackSelect();
            break;

        case PH_NXPEXTNS_ACTIVATED:
            Mfc_ActivateCback();
            break;

        case PH_NXPEXTNS_MIFARE_TRANSCEIVE:
            status = Mfc_Transceive(msg->pMsgData, msg->Size);
            break;

        case PH_NXPEXTNS_MIFARE_READ_ONLY:
            status = Mfc_SetReadOnly (msg->pMsgData, msg->Size);
            break;
        case PH_NXPEXTNS_MIFARE_PRESENCE_CHECK:
            pthread_mutex_init(&gAuthCmdBuf.syncmutex, NULL);
            pthread_mutex_lock(&gAuthCmdBuf.syncmutex);
            status = Mfc_PresenceCheck();
            pthread_mutex_unlock(&gAuthCmdBuf.syncmutex);
            pthread_mutex_destroy(&gAuthCmdBuf.syncmutex);
            break;
        default:
            status = NFCSTATUS_FAILED;
            NXPLOG_EXTNS_E("Illegal Command for Extension");
            break;
        }

    return status;
}

/*******************************************************************************
**
** Function         phNxpExtns_SendMsg
**
** Description      unlocks phNxpExtns_ProcessSysMessage with a valid message
**
** Returns          NFCSTATUS_SUCCESS if successfully initiated
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
static NFCSTATUS phNxpExtns_SendMsg(phLibNfc_Message_t *sysmsg)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    status = phNxpExtns_ProcessSysMessage(sysmsg);

    return status;
}

/*******************************************************************************
**
** Function         EXTNS_MfcRegisterNDefTypeHandler
**
** Description      This function allows the applications to register for
**                  specific types of NDEF records.
**
**                  For records types which were not registered, the record will
**                  be sent to the default handler.
**
** Returns          NFCSTATUS_SUCCESS
**
*******************************************************************************/
NFCSTATUS EXTNS_MfcRegisterNDefTypeHandler(tNFA_NDEF_CBACK *ndefHandlerCallback)
{

    NFCSTATUS status = NFCSTATUS_FAILED;
    if(NULL != ndefHandlerCallback)
    {
        gphNxpExtns_MifareStd_Context.p_ndef_cback = ndefHandlerCallback;
        status = NFCSTATUS_SUCCESS;
    }

    return status;
}

/*******************************************************************************
**                     Synchronizing Functions                                **
**            Synchronizes Callback in JNI and MFC Extns                      **
*******************************************************************************/

bool_t EXTNS_GetConnectFlag(void)
{
   return (gphNxpExtns_MifareStd_Context.ExtnsConnect);

}
void EXTNS_SetConnectFlag(bool_t flagval)
{
    gphNxpExtns_MifareStd_Context.ExtnsConnect = flagval;

}
bool_t EXTNS_GetDeactivateFlag(void)
{
   return (gphNxpExtns_MifareStd_Context.ExtnsDeactivate);

}
void EXTNS_SetDeactivateFlag(bool_t flagval)
{
    gphNxpExtns_MifareStd_Context.ExtnsDeactivate = flagval;

}
bool_t EXTNS_GetCallBackFlag(void)
{
   return (gphNxpExtns_MifareStd_Context.ExtnsCallBack);

}
void EXTNS_SetCallBackFlag(bool_t flagval)
{
    gphNxpExtns_MifareStd_Context.ExtnsCallBack = flagval;

}
NFCSTATUS EXTNS_GetPresenceCheckStatus(void)
{
    struct timespec ts;
    gAuthCmdBuf.status = NFCSTATUS_FAILED;
    if (sem_init (&gAuthCmdBuf.semPresenceCheck, 0, 0) == -1)
    {
        NXPLOG_API_E ("%s: semaphore creation failed (errno=0x%08x)", __FUNCTION__, errno);
        return NFCSTATUS_FAILED;
    }

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 0;
    ts.tv_nsec += 100*1000*1000; //100 milisec
    if(ts.tv_nsec >= 1000 * 1000 * 1000)
    {
        ts.tv_sec += 1;
        ts.tv_nsec = ts.tv_nsec - (1000 * 1000 * 1000);
    }

    if (sem_timedwait (&gAuthCmdBuf.semPresenceCheck, &ts))
    {
        NXPLOG_API_E ("%s: failed to wait (errno=0x%08x)", __FUNCTION__, errno);
        return NFCSTATUS_FAILED;
    }
    if (sem_destroy (&gAuthCmdBuf.semPresenceCheck))
    {
        NXPLOG_API_E ("Failed to destroy check Presence semaphore (errno=0x%08x)", errno);
    }
    return gAuthCmdBuf.status;
}

void MfcPresenceCheckResult(NFCSTATUS status)
{
    gAuthCmdBuf.status = status;
    EXTNS_SetCallBackFlag(TRUE);
    sem_post(&gAuthCmdBuf.semPresenceCheck);
}
void MfcResetPresenceCheckStatus(void)
{
    gAuthCmdBuf.auth_sent = FALSE;
}
/*******************************************************************************
**
** Function         EXTNS_CheckMfcResponse
**
** Description      This function is called from JNI Transceive for Mifare
**                  Classic Tag status interpretation and to send the required
**                  status to application
**
** Returns          NFCSTATUS_SUCCESS
**                  NFCSTATUS_FAILED
**
*******************************************************************************/
NFCSTATUS EXTNS_CheckMfcResponse(uint8_t** sTransceiveData, uint32_t *sTransceiveDataLen)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    if (*sTransceiveDataLen == 3)
    {
        if((*sTransceiveData)[0] == 0x10 && (*sTransceiveData)[1] != 0x0A)
        {
            NXPLOG_EXTNS_E(" Mifare Error in payload response");
            *sTransceiveDataLen = 0x1;
            *sTransceiveData += 1;
            return NFCSTATUS_FAILED;
        }
    }
    if((*sTransceiveData)[0] == 0x40)
    {
        *sTransceiveData += 1;
        *sTransceiveDataLen = 0x01;
        if((*sTransceiveData)[0] == 0x03)
        {
            *sTransceiveDataLen = 0x00;
            status = NFCSTATUS_FAILED;
        }
    }
    else if((*sTransceiveData)[0] == 0x10)
    {
        *sTransceiveData += 1;
        *sTransceiveDataLen = 0x10;
    }

    return status;
}

/*******************************************************************************
**
** Function         EXTNS_EseNfcSyncInit
**
** Description      This function initializes Nfc Ese synchronization driver.
**
** Returns          NFCSTATUS_SUCCESS if successfully initialized
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_EseNfcSyncInit(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
#ifdef ESE_NFC_SYNCHRONIZATION
    /*initialize the file descriptor*/
    fd_ese_nfc_sync = -1;

    fd_ese_nfc_sync = open("/dev/ese_nfc_sync", O_RDWR);
    if (fd_ese_nfc_sync < 0)
    {
        NXPLOG_API_D("Error in opening file Error No = %d \n",errno);
        status = NFCSTATUS_FAILED;
    }
    else
    {
        NXPLOG_API_D("/dev/ese_nfc_sync driver opened for read and write the device ");
    }
#endif
    return status;
}

/*******************************************************************************
**
** Function         EXTNS_EseNfcSyncDeInit
**
** Description      This function deinitializes Nfc Ese synchronization driver.
**
** Returns          NFCSTATUS_SUCCESS if successfully initialized
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_EseNfcSyncDeInit(void)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
#ifdef ESE_NFC_SYNCHRONIZATION
    if (fd_ese_nfc_sync > 0)
    {
        /* In error case need to free the required resource*/
        EXTNS_NfcReleaseResourceIoctl(5000);
        if(close(fd_ese_nfc_sync) ==  0)
        {
            fd_ese_nfc_sync = -1;
        }
        else
        {
            status = NFCSTATUS_FAILED;
        }
    }
    else
    {
        NXPLOG_API_E("Ese NFC driver handle is not available\n");
    }
#endif
    return status;
}
#ifdef ESE_NFC_SYNCHRONIZATION
static void start_time(TimeCal *time_val)
{
    gettimeofday(&time_val->tv1, NULL);
}
static double stop_time(TimeCal *time_val)
{
    double total_time = 0;
    gettimeofday(&time_val->tv2, NULL);
    /* Display Summary */
    NXPLOG_API_D("==========================================================================\n");
    NXPLOG_API_D("SUMMARY\n\n");
    total_time = (double) (time_val->tv2.tv_usec - time_val->tv1.tv_usec)/1000000 +
            (double) (time_val->tv2.tv_sec - time_val->tv1.tv_sec);
    NXPLOG_API_D("Total execution time = %f seconds\n",total_time);

    NXPLOG_API_D("================================================================="
            "===============\n");
    return total_time;
}
#endif
/*******************************************************************************
**
** Function         EXTNS_NfcReqResourceIoctl
**
** Description      This function acquire resource from synchronization driver.
**                  It requires the time limit in microseconds using that it retry for the resource
**                  if it is used by other(SPI) service.
**
** Returns          NFCSTATUS_SUCCESS if successfully initialized
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_NfcReqResourceIoctl(double time_limit)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
#ifdef ESE_NFC_SYNCHRONIZATION
    sync_status_e sync_status = SYNC_IO_FAILED;
    double elapsed_time = 0;
    double total_time = 0;
    TimeCal time;

    if (fd_ese_nfc_sync > 0)
    {
        do{
            start_time(&time);
            if (ioctl(fd_ese_nfc_sync, SYNC_NFCIOCREQUEST, &sync_status) < 0)
            {
                if (sync_status == SYNC_IO_BUSY)
                {
                    NXPLOG_API_E("%s :: SYNC_NFCIOCREQUEST Failed SYNC_IO_BUSY\n", __FUNCTION__);
                    status = NFCSTATUS_FAILED;
                }
                else
                {
                    NXPLOG_API_E("%s :: SYNC_NFCIOCREQUEST Failed\n", __FUNCTION__);
                    status = NFCSTATUS_FAILED;
                    break;
                }
            }
            else
            {
                NXPLOG_API_E("%s :: SYNC_NFCIOCREQUEST sync_status %d\n", __FUNCTION__, sync_status);
                if (sync_status == SYNC_IO_SUCCESS)
                {
                    NXPLOG_API_D("%s : SUCCESS\n", __FUNCTION__);
                    status = NFCSTATUS_SUCCESS;
                    break;
                }
                else
                {
                    NXPLOG_API_D("%s : SYNC_SPI_USED\n", __FUNCTION__);
                    NXPLOG_API_D("Retrying to get the resource\n");
                    status = NFCSTATUS_FAILED;
                    /* blocking timer should be used here */
                    usleep(5*1000);
                }
            }
            elapsed_time = stop_time(&time);
            total_time += elapsed_time;
        }while(total_time <= time_limit);
        if (total_time < time_limit)
        {
            elapsed_time = stop_time(&time);
        }
    }
    else
    {
        NXPLOG_API_E("Ese NFC driver handle is not available\n");
    }
#else
    (void)time_limit;
#endif
    return status;
}

/*******************************************************************************
**
** Function         EXTNS_NfcReleaseResourceIoctl
**
** Description      This function releases acquire resource.
**                  It requires the time limit in microseconds using that it retry for the resource
**                  if it is used by other(SPI) service.
** Returns          NFCSTATUS_SUCCESS if successfully initialized
**                  NFCSTATUS_FAILED otherwise
**
*******************************************************************************/
NFCSTATUS EXTNS_NfcReleaseResourceIoctl(double time_limit)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
#ifdef ESE_NFC_SYNCHRONIZATION
    double elapsed_time = 0;
    double total_time = 0;
    TimeCal time;
    sync_status_e sync_status = SYNC_IO_FAILED;

    if (fd_ese_nfc_sync > 0)
    {
        do{
            start_time(&time);
            if (ioctl(fd_ese_nfc_sync, SYNC_NFCIOCRELEASE, &sync_status) < 0)
            {
                NXPLOG_API_E("%s :: SYNC_NFCIOCRELEASE Failed sync_status %d\n", __FUNCTION__, sync_status);
                if (sync_status == SYNC_IO_BUSY)
                {
                    NXPLOG_API_E("%s :: SYNC_NFCIOCRELEASE Failed SYNC_IO_BUSY\n", __FUNCTION__);
                    status = NFCSTATUS_FAILED;
                }
                else
                {
                    NXPLOG_API_E("%s :: SYNC_NFCIOCRELEASE Failed\n", __FUNCTION__);
                    status = NFCSTATUS_FAILED;
                    break;
                }
            }
            else
            {
                NXPLOG_API_E("%s :: SYNC_NFCIOCRELEASE sync_status %d\n", __FUNCTION__, sync_status);
                if (sync_status == SYNC_IO_SUCCESS)
                {
                    NXPLOG_API_D("%s : SUCCESS\n", __FUNCTION__);
                }
                else
                {
                    NXPLOG_API_E("%s :: SYNC_NFCIOCRELEASE Failed \n", __FUNCTION__);
                    status = NFCSTATUS_FAILED;
                }
                break;
             }
            elapsed_time = stop_time(&time);
            total_time += elapsed_time;
        }while(total_time <= time_limit);

        if (total_time < time_limit)
        {
            elapsed_time = stop_time(&time);
        }
    }
    else
    {
        NXPLOG_API_E("Ese NFC Syncdriver handle is not available\n");
    }
#else
    (void)time_limit;
#endif
    return status;
}
