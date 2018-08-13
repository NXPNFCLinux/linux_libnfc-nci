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

#include <phNxpNciHal_Kovio.h>
#include <phNxpLog.h>


#define KOVIO_TIMEOUT 1000    /* Timeout value to wait for RF INTF Activated NTF.*/
#define KOVIO_ACT_NTF_TEMP_BUFF_LEN 32    /* length of temp buffer to manipulate
                                    the activated notification to match BCM format*/
#define MAX_WRITE_RETRY 5

/******************* Global variables *****************************************/
extern phNxpNciHal_Control_t nxpncihal_ctrl;
extern NFCSTATUS phNxpNciHal_send_ext_cmd(uint16_t cmd_len, uint8_t *p_cmd);

int kovio_detected = 0x00;
int send_to_upper_kovio = 0x01;
int disable_kovio=0x00;
static uint8_t rf_deactivate_cmd[]   = { 0x21, 0x06, 0x01, 0x03 }; /* discovery */
static uint8_t rf_deactivated_ntf[]  = { 0x61, 0x06, 0x02, 0x03, 0x01 };
static uint8_t reset_ntf[] = {0x60, 0x00, 0x06, 0xA0, 0x00, 0xC7, 0xD4, 0x00, 0x00};

static uint8_t kovio_protocol_type=0;
static uint8_t kovio_discovery_type=0;
static uint32_t kovio_timer;

/************** Kovio functions ***************************************/

static NFCSTATUS phNxpNciHal_rf_deactivate(void);

/*******************************************************************************
**
** Function         hal_write_cb
**
** Description      Callback function for hal write.
**
** Returns          None
**
*******************************************************************************/
static void hal_write_cb(void *pContext, phTmlNfc_TransactInfo_t *pInfo)
{
    UNUSED(pContext);
    UNUSED(pInfo);
    return;
}

/*******************************************************************************
**
** Function         kovio_timer_handler
**
** Description      Callback function for kovio timer.
**
** Returns          None
**
*******************************************************************************/
static void kovio_timer_handler(uint32_t timerId, void *pContext)
{
    UNUSED(timerId);
    UNUSED(pContext);
    NXPLOG_NCIHAL_D(">> kovio_timer_handler. Did not receive RF_INTF_ACTIVATED_NTF, Kovio TAG must be removed.");

    phOsalNfc_Timer_Delete(kovio_timer);

    kovio_detected = 0x00;
    send_to_upper_kovio=0x01;
    disable_kovio=0x00;
    /*
     * send kovio deactivated ntf to upper layer.
    */
    NXPLOG_NCIHAL_D(">> send kovio deactivated ntf to upper layer.");
    if (nxpncihal_ctrl.p_nfc_stack_data_cback != NULL)
    {
        (*nxpncihal_ctrl.p_nfc_stack_data_cback)(
                sizeof(rf_deactivated_ntf), rf_deactivated_ntf);
    }
    return;
}

/*******************************************************************************
**
** Function         phNxpNciHal_rf_deactivate
**
** Description      Sends rf deactivate cmd to NFCC
**
** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED
**
*******************************************************************************/
static NFCSTATUS phNxpNciHal_rf_deactivate()
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    int cb_data;
    int retryCnt = 0;

    do
    {
        retryCnt++;
        status = phTmlNfc_Write(rf_deactivate_cmd,
                sizeof(rf_deactivate_cmd),
                (pphTmlNfc_TransactCompletionCb_t) &hal_write_cb, &cb_data);
    } while(status != NFCSTATUS_PENDING && retryCnt <= MAX_WRITE_RETRY);

    if(status != NFCSTATUS_PENDING)
    {
        //phNxpNciHal_emergency_recovery();
        if (nxpncihal_ctrl.p_nfc_stack_data_cback!= NULL &&
            nxpncihal_ctrl.hal_open_status == TRUE)
        {
            NXPLOG_NCIHAL_D("Send the Core Reset NTF to upper layer, which will trigger the recovery\n");
            //Send the Core Reset NTF to upper layer, which will trigger the recovery.
            nxpncihal_ctrl.rx_data_len = sizeof(reset_ntf);
            memcpy(nxpncihal_ctrl.p_rx_data, reset_ntf, sizeof(reset_ntf));
            (*nxpncihal_ctrl.p_nfc_stack_data_cback)(nxpncihal_ctrl.rx_data_len, nxpncihal_ctrl.p_rx_data);
        }
    }

    return status;
}

/*******************************************************************************
**
** Function         phNxpNciHal_kovio_rsp_ext
**
** Description      Implements kovio presence check. In BCM controller this is
**                  managed by NFCC. But since PN54X does not handle this, the
**                  presence check is mimiced here.
**                  For the very first time Kovio is detected, NTF has to be
**                  passed on to upper layer. for every NTF, DH send a deactivated
**                  command to NFCC and NFCC follows this up with another activated
**                  notification. When the tag is removed, activated notification
**                  stops coming and this is indicated to upper layer with a HAL
**                  generated deactivated notification.
** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED
**
*******************************************************************************/
NFCSTATUS phNxpNciHal_kovio_rsp_ext(uint8_t *p_ntf, uint16_t *p_len)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    uint8_t tBuff[KOVIO_ACT_NTF_TEMP_BUFF_LEN];

    send_to_upper_kovio = 1;
    if((p_ntf[0]==0x61)&&(p_ntf[1]==0x05))
    {
        if((p_ntf[5] == kovio_protocol_type) && (p_ntf[6] == kovio_discovery_type) )
        {
            if (kovio_detected == 0)
            {
                if((*p_len-9)<KOVIO_ACT_NTF_TEMP_BUFF_LEN)
                {
                    p_ntf[2]+=1;
                    memcpy(tBuff, &p_ntf[9], *p_len-9);
                    p_ntf[9]=p_ntf[9]+1;
                    memcpy(&p_ntf[10], tBuff, *p_len-9);
                    *p_len+=1;
                }else
                {
                    NXPLOG_NCIHAL_D("Kovio Act ntf payload exceeded temp buffer size");
                }
                kovio_detected = 1;
                kovio_timer = phOsalNfc_Timer_Create();
                NXPLOG_NCIHAL_D("custom kovio timer Created - %d", kovio_timer);
            }
            else
            {
                send_to_upper_kovio = 0;
            }
            NXPLOG_NCIHAL_D("Send RF deactivate command to NFCC");
            status = phNxpNciHal_rf_deactivate();
            status = phOsalNfc_Timer_Start(kovio_timer,
                    KOVIO_TIMEOUT,
                    &kovio_timer_handler,
                    NULL);
            if (NFCSTATUS_SUCCESS == status)
            {
                NXPLOG_NCIHAL_D("kovio timer started");
            }
            else
            {
                NXPLOG_NCIHAL_E("kovio timer not started!!!");
                status  = NFCSTATUS_FAILED;
            }
        }
        else
        {
            if (kovio_detected == 1)
            {
                phNxpNciHal_clean_Kovio_Ext();
                NXPLOG_NCIHAL_D ("Disabling Kovio detection logic as another tag type detected");
            }
        }
    }
    else if((p_ntf[0]==0x41)&&(p_ntf[1]==0x06)&&(p_ntf[2]==0x01)&&(p_ntf[3]==0x00))
    {
        if(kovio_detected == 1)
            send_to_upper_kovio = 0;
        if((kovio_detected == 1)&&(disable_kovio==0x01))
        {
            NXPLOG_NCIHAL_D ("Disabling Kovio detection logic");
            phNxpNciHal_clean_Kovio_Ext();
            disable_kovio=0x00;
        }
    }
    else if((p_ntf[0]==0x61)&&(p_ntf[1]==0x06)&&(p_ntf[2]==0x02)&&(p_ntf[3]==0x03)&&(p_ntf[4]==0x00))
    {
        if(kovio_detected == 1)
            send_to_upper_kovio = 0;
    }
    else if((p_ntf[0]==0x61)&&(p_ntf[1]==0x03))
    {
        if(kovio_detected == 1)
            send_to_upper_kovio = 0;
    }
    return status;
}


/*******************************************************************************
**
** Function         phNxpNciHal_clean_Kovio_Ext
**
** Description      Clean up Kovio extension state machine.
** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED
**
*******************************************************************************/
void phNxpNciHal_clean_Kovio_Ext()
{
    NXPLOG_NCIHAL_D(">> Cleaning up Kovio State machine and timer.");
    phOsalNfc_Timer_Delete(kovio_timer);
    kovio_detected = 0x00;
    send_to_upper_kovio=0x01;
    disable_kovio=0x00;
    /*
     * send kovio deactivated ntf to upper layer.
    */
    NXPLOG_NCIHAL_D(">> send kovio deactivated ntf to upper layer.");
    if (nxpncihal_ctrl.p_nfc_stack_data_cback != NULL)
    {
        (*nxpncihal_ctrl.p_nfc_stack_data_cback)(
                sizeof(rf_deactivated_ntf), rf_deactivated_ntf);
    }
    return;
}
