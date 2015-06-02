/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
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
 *  This file contains serial definitions from WIDCOMM's Universal Embedded
 *  Drivers API.
 *
 ******************************************************************************/
#ifndef UAMP_API_H
#define UAMP_API_H

/*****************************************************************************
**  Constant and Type Definitions
*****************************************************************************/

/* UAMP identifiers */
#define UAMP_ID_1   1
#define UAMP_ID_2   2
typedef UINT8 tUAMP_ID;

/* UAMP event ids (used by UAMP_CBACK) */
#define UAMP_EVT_RX_READY           0   /* Data from AMP controller is ready to be read */
#define UAMP_EVT_CTLR_REMOVED       1   /* Controller removed */
#define UAMP_EVT_CTLR_READY         2   /* Controller added/ready */
typedef UINT8 tUAMP_EVT;


/* UAMP Channels */
#define UAMP_CH_HCI_CMD            0   /* HCI Command channel */
#define UAMP_CH_HCI_EVT            1   /* HCI Event channel */
#define UAMP_CH_HCI_DATA           2   /* HCI ACL Data channel */
typedef UINT8 tUAMP_CH;

/* tUAMP_EVT_DATA: union for event-specific data, used by UAMP_CBACK */
typedef union {
    tUAMP_CH channel;       /* UAMP_EVT_RX_READY: channel for which rx occured */
} tUAMP_EVT_DATA;




/*****************************************************************************
**
** Function:    UAMP_CBACK
**
** Description: Callback for events. Register callback using UAMP_Init.
**
** Parameters   amp_id:         AMP device identifier that generated the event
**              amp_evt:        event id
**              p_amp_evt_data: pointer to event-specific data
**
*****************************************************************************/
typedef void (tUAMP_CBACK)(tUAMP_ID amp_id, tUAMP_EVT amp_evt, tUAMP_EVT_DATA *p_amp_evt_data);

/*****************************************************************************
**  external function declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
**
** Function:    UAMP_Init
**
** Description: Initialize UAMP driver
**
** Parameters   p_cback:    Callback function for UAMP event notification
**
*****************************************************************************/
BT_API BOOLEAN UAMP_Init(tUAMP_CBACK *p_cback);


/*****************************************************************************
**
** Function:    UAMP_Open
**
** Description: Open connection to local AMP device.
**
** Parameters   app_id: Application specific AMP identifer. This value
**                      will be included in AMP messages sent to the
**                      BTU task, to identify source of the message
**
*****************************************************************************/
BT_API BOOLEAN UAMP_Open(tUAMP_ID amp_id);

/*****************************************************************************
**
** Function:    UAMP_Close
**
** Description: Close connection to local AMP device.
**
** Parameters   app_id: Application specific AMP identifer.
**
*****************************************************************************/
BT_API void UAMP_Close(tUAMP_ID amp_id);


/*****************************************************************************
**
** Function:    UAMP_Write
**
** Description: Send buffer to AMP device.
**
**
** Parameters:  app_id:     AMP identifer.
**              p_buf:      pointer to buffer to write
**              num_bytes:  number of bytes to write
**              channel:    UAMP_CH_HCI_ACL, or UAMP_CH_HCI_CMD
**
** Returns:     number of bytes written
**
*****************************************************************************/
BT_API UINT16 UAMP_Write(tUAMP_ID amp_id, UINT8 *p_buf, UINT16 num_bytes, tUAMP_CH channel);


/*****************************************************************************
**
** Function:    UAMP_WriteBuf
**
** Description: Send GKI buffer to AMP device. Frees GKI buffer when done.
**
** Parameters   app_amp_id: AMP identifer (BTM_AMP_1, BTM_AMP_2, ...)
**              p_msg:      message to send.
**
*****************************************************************************/
BT_API UINT16 UAMP_WriteBuf(tUAMP_ID amp_id, BT_HDR *p_msg);


/*****************************************************************************
**
** Function:    UAMP_Read
**
** Description: Read incoming data from AMP. Call after receiving a
**              UAMP_EVT_RX_READY callback event.
**
** Parameters:  app_id:     AMP identifer.
**              p_buf:      pointer to buffer for holding incoming AMP data
**              buf_size:   size of p_buf
**              channel:    UAMP_CH_HCI_ACL, or UAMP_CH_HCI_EVT
**
** Returns:     number of bytes read
**
*****************************************************************************/
BT_API UINT16 UAMP_Read(tUAMP_ID amp_id, UINT8 *p_buf, UINT16 buf_size, tUAMP_CH channel);

#ifdef __cplusplus
}
#endif

#endif /* UAMP_API_H */
