/******************************************************************************
 *
 *  Copyright (C) 2012-2014 Broadcom Corporation
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


#ifndef NFC_TYPES_H
#define NFC_TYPES_H

/* Mask for NFC_HDR event field */
#define NFC_EVT_MASK                    0xFF00
#define NFC_SUB_EVT_MASK                0x00FF

/****************************************************************************
** NFC_HAL_TASK  definitions
*****************************************************************************/

/* NFC_HAL_TASK event messages */
#define NFC_HAL_EVT_TO_NFC_NCI             0x0100   /* NCI message for sending to NFCC          */
#define NFC_HAL_EVT_POST_CORE_RESET        0x0200   /* Request to start NCIT quick timer        */
#define NFC_HAL_EVT_TO_START_QUICK_TIMER   0x0300   /* Request to start chip-specific config    */
#define NFC_HAL_EVT_HCI                    0x0400   /* NCI message for hci persistency data     */
#define NFC_HAL_EVT_PRE_DISCOVER           0x0500   /* NCI message to issue prediscover config  */
#define NFC_HAL_EVT_CONTROL_GRANTED        0x0600   /* permission to send commands queued in HAL*/

/* NFC_HAL_TASK sub event messages */
#define NFC_HAL_HCI_RSP_NV_READ_EVT        (0x01 | NFC_HAL_EVT_HCI)
#define NFC_HAL_HCI_RSP_NV_WRITE_EVT       (0x02 | NFC_HAL_EVT_HCI)
#define NFC_HAL_HCI_VSC_TIMEOUT_EVT        (0x03 | NFC_HAL_EVT_HCI)


/* Event masks for NFC_TASK messages */
#define NFC_EVT_TO_NFC_NCI              0x4000      /* NCI message for sending to host stack    */
#define NFC_EVT_TO_NFC_ERR              0x4100      /* Error notification to NFC Task           */
#define NFC_EVT_TO_NFC_MSGS             0x4200      /* Messages between NFC and NCI task        */

/*****************************************************************************
** Macros to get and put bytes to and from a stream (Little Endian format).
*****************************************************************************/

#define UINT32_TO_STREAM(p, u32) {*(p)++ = (UINT8)(u32); *(p)++ = (UINT8)((u32) >> 8); *(p)++ = (UINT8)((u32) >> 16); *(p)++ = (UINT8)((u32) >> 24);}
#define UINT24_TO_STREAM(p, u24) {*(p)++ = (UINT8)(u24); *(p)++ = (UINT8)((u24) >> 8); *(p)++ = (UINT8)((u24) >> 16);}
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (UINT8)(u16); *(p)++ = (UINT8)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (UINT8)(u8);}
#define INT8_TO_STREAM(p, u8)    {*(p)++ = (INT8)(u8);}
#define ARRAY32_TO_STREAM(p, a)  {register int ijk; for (ijk = 0; ijk < 32;           ijk++) *(p)++ = (UINT8) a[31 - ijk];}
#define ARRAY16_TO_STREAM(p, a)  {register int ijk; for (ijk = 0; ijk < 16;           ijk++) *(p)++ = (UINT8) a[15 - ijk];}
#define ARRAY8_TO_STREAM(p, a)   {register int ijk; for (ijk = 0; ijk < 8;            ijk++) *(p)++ = (UINT8) a[7 - ijk];}
#define BDADDR_TO_STREAM(p, a)   {register int ijk; for (ijk = 0; ijk < BD_ADDR_LEN;  ijk++) *(p)++ = (UINT8) a[BD_ADDR_LEN - 1 - ijk];}
#define LAP_TO_STREAM(p, a)      {register int ijk; for (ijk = 0; ijk < LAP_LEN;      ijk++) *(p)++ = (UINT8) a[LAP_LEN - 1 - ijk];}
#define DEVCLASS_TO_STREAM(p, a) {register int ijk; for (ijk = 0; ijk < DEV_CLASS_LEN;ijk++) *(p)++ = (UINT8) a[DEV_CLASS_LEN - 1 - ijk];}
#define ARRAY_TO_STREAM(p, a, len) {register int ijk; for (ijk = 0; ijk < len;        ijk++) *(p)++ = (UINT8) a[ijk];}
#define REVERSE_ARRAY_TO_STREAM(p, a, len)  {register int ijk; for (ijk = 0; ijk < len; ijk++) *(p)++ = (UINT8) a[len - 1 - ijk];}

#define STREAM_TO_UINT8(u8, p)   {u8 = (UINT8)(*(p)); (p) += 1;}
#define STREAM_TO_UINT16(u16, p) {u16 = ((UINT16)(*(p)) + (((UINT16)(*((p) + 1))) << 8)); (p) += 2;}
#define STREAM_TO_UINT24(u32, p) {u32 = (((UINT32)(*(p))) + ((((UINT32)(*((p) + 1)))) << 8) + ((((UINT32)(*((p) + 2)))) << 16) ); (p) += 3;}
#define STREAM_TO_UINT32(u32, p) {u32 = (((UINT32)(*(p))) + ((((UINT32)(*((p) + 1)))) << 8) + ((((UINT32)(*((p) + 2)))) << 16) + ((((UINT32)(*((p) + 3)))) << 24)); (p) += 4;}
#define STREAM_TO_BDADDR(a, p)   {register int ijk; register UINT8 *pbda = (UINT8 *)a + BD_ADDR_LEN - 1; for (ijk = 0; ijk < BD_ADDR_LEN; ijk++) *pbda-- = *p++;}
#define STREAM_TO_ARRAY32(a, p)  {register int ijk; register UINT8 *_pa = (UINT8 *)a + 31; for (ijk = 0; ijk < 32; ijk++) *_pa-- = *p++;}
#define STREAM_TO_ARRAY16(a, p)  {register int ijk; register UINT8 *_pa = (UINT8 *)a + 15; for (ijk = 0; ijk < 16; ijk++) *_pa-- = *p++;}
#define STREAM_TO_ARRAY8(a, p)   {register int ijk; register UINT8 *_pa = (UINT8 *)a + 7; for (ijk = 0; ijk < 8; ijk++) *_pa-- = *p++;}
#define STREAM_TO_DEVCLASS(a, p) {register int ijk; register UINT8 *_pa = (UINT8 *)a + DEV_CLASS_LEN - 1; for (ijk = 0; ijk < DEV_CLASS_LEN; ijk++) *_pa-- = *p++;}
#define STREAM_TO_LAP(a, p)      {register int ijk; register UINT8 *plap = (UINT8 *)a + LAP_LEN - 1; for (ijk = 0; ijk < LAP_LEN; ijk++) *plap-- = *p++;}
#define STREAM_TO_ARRAY(a, p, len) {register int ijk; for (ijk = 0; ijk < len; ijk++) ((UINT8 *) a)[ijk] = *p++;}
#define REVERSE_STREAM_TO_ARRAY(a, p, len) {register int ijk; register UINT8 *_pa = (UINT8 *)a + len - 1; for (ijk = 0; ijk < len; ijk++) *_pa-- = *p++;}

/*****************************************************************************
** Macros to get and put bytes to and from a field (Little Endian format).
** These are the same as to stream, except the pointer is not incremented.
*****************************************************************************/

#define UINT32_TO_FIELD(p, u32) {*(UINT8 *)(p) = (UINT8)(u32); *((UINT8 *)(p)+1) = (UINT8)((u32) >> 8); *((UINT8 *)(p)+2) = (UINT8)((u32) >> 16); *((UINT8 *)(p)+3) = (UINT8)((u32) >> 24);}
#define UINT24_TO_FIELD(p, u24) {*(UINT8 *)(p) = (UINT8)(u24); *((UINT8 *)(p)+1) = (UINT8)((u24) >> 8); *((UINT8 *)(p)+2) = (UINT8)((u24) >> 16);}
#define UINT16_TO_FIELD(p, u16) {*(UINT8 *)(p) = (UINT8)(u16); *((UINT8 *)(p)+1) = (UINT8)((u16) >> 8);}
#define UINT8_TO_FIELD(p, u8)   {*(UINT8 *)(p) = (UINT8)(u8);}


/*****************************************************************************
** Macros to get and put bytes to and from a stream (Big Endian format)
*****************************************************************************/

#define UINT32_TO_BE_STREAM(p, u32) {*(p)++ = (UINT8)((u32) >> 24);  *(p)++ = (UINT8)((u32) >> 16); *(p)++ = (UINT8)((u32) >> 8); *(p)++ = (UINT8)(u32); }
#define UINT24_TO_BE_STREAM(p, u24) {*(p)++ = (UINT8)((u24) >> 16); *(p)++ = (UINT8)((u24) >> 8); *(p)++ = (UINT8)(u24);}
#define UINT16_TO_BE_STREAM(p, u16) {*(p)++ = (UINT8)((u16) >> 8); *(p)++ = (UINT8)(u16);}
#define UINT8_TO_BE_STREAM(p, u8)   {*(p)++ = (UINT8)(u8);}
#define ARRAY_TO_BE_STREAM(p, a, len) {register int ijk; for (ijk = 0; ijk < len; ijk++) *(p)++ = (UINT8) a[ijk];}

#define BE_STREAM_TO_UINT8(u8, p)   {u8 = (UINT8)(*(p)); (p) += 1;}
#define BE_STREAM_TO_UINT16(u16, p) {u16 = (UINT16)(((UINT16)(*(p)) << 8) + (UINT16)(*((p) + 1))); (p) += 2;}
#define BE_STREAM_TO_UINT24(u32, p) {u32 = (((UINT32)(*((p) + 2))) + ((UINT32)(*((p) + 1)) << 8) + ((UINT32)(*(p)) << 16)); (p) += 3;}
#define BE_STREAM_TO_UINT32(u32, p) {u32 = ((UINT32)(*((p) + 3)) + ((UINT32)(*((p) + 2)) << 8) + ((UINT32)(*((p) + 1)) << 16) + ((UINT32)(*(p)) << 24)); (p) += 4;}
#define BE_STREAM_TO_ARRAY(p, a, len) {register int ijk; for (ijk = 0; ijk < len; ijk++) ((UINT8 *) a)[ijk] = *p++;}


/*****************************************************************************
** Macros to get and put bytes to and from a field (Big Endian format).
** These are the same as to stream, except the pointer is not incremented.
*****************************************************************************/

#define UINT32_TO_BE_FIELD(p, u32) {*(UINT8 *)(p) = (UINT8)((u32) >> 24);  *((UINT8 *)(p)+1) = (UINT8)((u32) >> 16); *((UINT8 *)(p)+2) = (UINT8)((u32) >> 8); *((UINT8 *)(p)+3) = (UINT8)(u32); }
#define UINT24_TO_BE_FIELD(p, u24) {*(UINT8 *)(p) = (UINT8)((u24) >> 16); *((UINT8 *)(p)+1) = (UINT8)((u24) >> 8); *((UINT8 *)(p)+2) = (UINT8)(u24);}
#define UINT16_TO_BE_FIELD(p, u16) {*(UINT8 *)(p) = (UINT8)((u16) >> 8); *((UINT8 *)(p)+1) = (UINT8)(u16);}
#define UINT8_TO_BE_FIELD(p, u8)   {*(UINT8 *)(p) = (UINT8)(u8);}

/*****************************************************************************
** Define trace levels
*****************************************************************************/

#define BT_TRACE_LEVEL_NONE    0          /* No trace messages to be generated    */
#define BT_TRACE_LEVEL_ERROR   1          /* Error condition trace messages       */
#define BT_TRACE_LEVEL_WARNING 2          /* Warning condition trace messages     */
#define BT_TRACE_LEVEL_API     3          /* API traces                           */
#define BT_TRACE_LEVEL_EVENT   4          /* Debug messages for events            */
#define BT_TRACE_LEVEL_DEBUG   5          /* Full debug messages                  */


#define TRACE_CTRL_GENERAL          0x00000000
#define TRACE_LAYER_NCI             0x00280000
#define TRACE_LAYER_HAL             0x00310000
#define TRACE_LAYER_GKI             0x001a0000
#define TRACE_ORG_STACK             0x00000000
#define TRACE_ORG_GKI               0x00000400

#define TRACE_TYPE_ERROR            0x00000000
#define TRACE_TYPE_WARNING          0x00000001
#define TRACE_TYPE_API              0x00000002
#define TRACE_TYPE_EVENT            0x00000003
#define TRACE_TYPE_DEBUG            0x00000004

#define TRACE_TYPE_GENERIC          0x00000008

#endif /* NFC_TYPES_H */

