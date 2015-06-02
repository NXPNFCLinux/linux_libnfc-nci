/******************************************************************************
 *
 *  Copyright (C) 2009-2014 Broadcom Corporation
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

/******************************************************************************
 *
 *  This file contains the Near Field Communication (NFC) Reader/Writer mode
 *  related API function external definitions.
 *
 ******************************************************************************/

#ifndef RW_API_H
#define RW_API_H
#include "tags_defs.h"

#define RW_T1T_BLD_ADD(a, k, y)   a = ((k & 0xF) << 3) | (y&0x7);
#define RW_T1T_BLD_ADDS(a, s)     a = ((s & 0xF) << 4);

#define RW_T1T_FIRST_EVT    0x20
#define RW_T2T_FIRST_EVT    0x40
#define RW_T3T_FIRST_EVT    0x60
#define RW_T4T_FIRST_EVT    0x80
#define RW_I93_FIRST_EVT    0xA0
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define RW_T3BT_FIRST_EVT   0xB0
#endif

enum
{
    /* Note: the order of these events can not be changed */
    /* Type 1 tag events for tRW_CBACK */
    RW_T1T_RID_EVT = RW_T1T_FIRST_EVT,          /* Read ID command completd              */
    RW_T1T_RALL_CPLT_EVT,                       /* Read All command completed            */
    RW_T1T_READ_CPLT_EVT,                       /* Read byte completed                   */
    RW_T1T_WRITE_E_CPLT_EVT,                    /* Write byte after erase completed      */
    RW_T1T_WRITE_NE_CPLT_EVT,                   /* Write byte with no erase completed    */
    RW_T1T_RSEG_CPLT_EVT,                       /* Read segment completed                */
    RW_T1T_READ8_CPLT_EVT,                      /* Read block completed                  */
    RW_T1T_WRITE_E8_CPLT_EVT,                   /* Write block after erase completed     */
    RW_T1T_WRITE_NE8_CPLT_EVT,                  /* Write block with no erase completed   */
    RW_T1T_TLV_DETECT_EVT,                      /* Lock/Mem/Prop tlv detection complete  */
    RW_T1T_NDEF_DETECT_EVT,                     /* NDEF detection complete               */
    RW_T1T_NDEF_READ_EVT,                       /* NDEF read completed                   */
    RW_T1T_NDEF_WRITE_EVT,                      /* NDEF write complete                   */
    RW_T1T_SET_TAG_RO_EVT,                      /* Tag is set as read only               */
    RW_T1T_RAW_FRAME_EVT,                       /* Response of raw frame sent            */
    RW_T1T_PRESENCE_CHECK_EVT,                  /* Response to RW_T1tPresenceCheck       */
    RW_T1T_FORMAT_CPLT_EVT,                     /* Tag Formated                          */
    RW_T1T_INTF_ERROR_EVT,                      /* RF Interface error event              */
    RW_T1T_MAX_EVT,

    /* Type 2 tag events */
    RW_T2T_READ_CPLT_EVT = RW_T2T_FIRST_EVT,    /* Read completed                        */
    RW_T2T_WRITE_CPLT_EVT,                      /* Write completed                       */
    RW_T2T_SELECT_CPLT_EVT,                     /* Sector select completed               */
    RW_T2T_NDEF_DETECT_EVT,                     /* NDEF detection complete               */
    RW_T2T_TLV_DETECT_EVT,                      /* Lock/Mem/Prop tlv detection complete  */
    RW_T2T_NDEF_READ_EVT,                       /* NDEF read completed                   */
    RW_T2T_NDEF_WRITE_EVT,                      /* NDEF write complete                   */
    RW_T2T_SET_TAG_RO_EVT,                      /* Tag is set as read only               */
    RW_T2T_RAW_FRAME_EVT,                       /* Response of raw frame sent            */
    RW_T2T_PRESENCE_CHECK_EVT,                  /* Response to RW_T2tPresenceCheck       */
    RW_T2T_FORMAT_CPLT_EVT,                     /* Tag Formated                          */
    RW_T2T_INTF_ERROR_EVT,                      /* RF Interface error event              */
    RW_T2T_MAX_EVT,

    /* Type 3 tag events for tRW_CBACK */
    RW_T3T_CHECK_CPLT_EVT = RW_T3T_FIRST_EVT,   /* Read completed                           */
    RW_T3T_UPDATE_CPLT_EVT,                     /* Write completed                          */
    RW_T3T_CHECK_EVT,                           /* Segment of data received from type 3 tag */
    RW_T3T_RAW_FRAME_EVT,                       /* SendRawFrame response                    */
    RW_T3T_NDEF_DETECT_EVT,                     /* NDEF detection complete                  */
    RW_T3T_PRESENCE_CHECK_EVT,                  /* Response to RW_T3tPresenceCheck          */
    RW_T3T_POLL_EVT,                            /* Response to RW_T3tPoll                   */
    RW_T3T_GET_SYSTEM_CODES_EVT,                /* Response to RW_T3tGetSystemCodes         */
    RW_T3T_FORMAT_CPLT_EVT,                     /* Tag Formated (Felica-Lite only)          */
    RW_T3T_SET_READ_ONLY_CPLT_EVT,              /* Tag is set as Read only                  */
    RW_T3T_INTF_ERROR_EVT,                      /* RF Interface error event                 */
    RW_T3T_MAX_EVT,

    /* Type 4 tag events for tRW_CBACK */
    RW_T4T_NDEF_DETECT_EVT = RW_T4T_FIRST_EVT,  /* Result of NDEF detection procedure       */
                                                /* Mandatory NDEF file is selected          */
    RW_T4T_NDEF_READ_EVT,                       /* Segment of data received from type 4 tag */
    RW_T4T_NDEF_READ_CPLT_EVT,                  /* Read operation completed                 */
    RW_T4T_NDEF_READ_FAIL_EVT,                  /* Read operation failed                    */
    RW_T4T_NDEF_UPDATE_CPLT_EVT,                /* Update operation completed               */
    RW_T4T_NDEF_UPDATE_FAIL_EVT,                /* Update operation failed                  */
    RW_T4T_SET_TO_RO_EVT,                       /* Tag is set as read only                  */
    RW_T4T_PRESENCE_CHECK_EVT,                  /* Response to RW_T4tPresenceCheck          */
    RW_T4T_RAW_FRAME_EVT,                       /* Response of raw frame sent               */
    RW_T4T_INTF_ERROR_EVT,                      /* RF Interface error event                 */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    RW_T4T_NDEF_FORMAT_CPLT_EVT,                /* Format operation completed               */
    RW_T4T_RAW_FRAME_RF_WTX_EVT,                /* Received RF WTX for raw frame sent       */
#endif
    RW_T4T_MAX_EVT,

    /* ISO 15693 tag events for tRW_CBACK */
    RW_I93_NDEF_DETECT_EVT = RW_I93_FIRST_EVT,  /* Result of NDEF detection procedure */
    RW_I93_NDEF_READ_EVT,                       /* Segment of data received from tag  */
    RW_I93_NDEF_READ_CPLT_EVT,                  /* Read operation completed           */
    RW_I93_NDEF_READ_FAIL_EVT,                  /* Read operation failed              */
    RW_I93_NDEF_UPDATE_CPLT_EVT,                /* Update operation completed         */
    RW_I93_NDEF_UPDATE_FAIL_EVT,                /* Update operation failed            */
    RW_I93_FORMAT_CPLT_EVT,                     /* Format procedure complete          */
    RW_I93_SET_TAG_RO_EVT,                      /* Set read-only procedure complete   */
    RW_I93_INVENTORY_EVT,                       /* Response of Inventory              */
    RW_I93_DATA_EVT,                            /* Response of Read, Get Multi Security */
    RW_I93_SYS_INFO_EVT,                        /* Response of System Information     */
    RW_I93_CMD_CMPL_EVT,                        /* Command complete                   */
    RW_I93_PRESENCE_CHECK_EVT,                  /* Response to RW_I93PresenceCheck    */
    RW_I93_RAW_FRAME_EVT,                       /* Response of raw frame sent         */
    RW_I93_INTF_ERROR_EVT,                      /* RF Interface error event           */
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    RW_I93_MAX_EVT,
    RW_T3BT_RAW_READ_CPLT_EVT,
    RW_T3BT_MAX_EVT
#else
    RW_I93_MAX_EVT
#endif
};
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
#define RW_I93_MAX_RSP_TIMEOUT    1000
#endif
#define RW_RAW_FRAME_EVT     0xFF

typedef UINT8 tRW_EVENT;

#define RW_NDEF_FL_READ_ONLY                    0x01    /* Tag is read only              */
#define RW_NDEF_FL_FORMATED                     0x02    /* Tag formated for NDEF         */
#define RW_NDEF_FL_SUPPORTED                    0x04    /* NDEF supported by the tag     */
#define RW_NDEF_FL_UNKNOWN                      0x08    /* Unable to find if tag is ndef capable/formated/read only */
#define RW_NDEF_FL_FORMATABLE                   0x10    /* Tag supports format operation */
#define RW_NDEF_FL_SOFT_LOCKABLE                0x20    /* Tag can be soft locked */
#define RW_NDEF_FL_HARD_LOCKABLE                0x40    /* Tag can be hard locked */
#define RW_NDEF_FL_OTP                          0x80    /* Tag is one time programmable */

typedef UINT8 tRW_NDEF_FLAG;

/* options for RW_T4tPresenceCheck  */
#define RW_T4T_CHK_READ_BINARY_CH0      0
#define RW_T4T_CHK_READ_BINARY_CH1      1
#define RW_T4T_CHK_READ_BINARY_CH2      2
#define RW_T4T_CHK_READ_BINARY_CH3      3
#define RW_T4T_CHK_EMPTY_I_BLOCK        4

typedef struct
{
    tNFC_STATUS     status;
    UINT8           hr[T1T_HR_LEN];
    UINT8           uid[T1T_CMD_UID_LEN];
} tRW_T1T_RID_EVT;

typedef struct
{
    tNFC_STATUS     status;
    UINT16          msg_len;            /* Length of the NDEF message */
} tRW_T2T_DETECT;

typedef struct
{
    tNFC_STATUS     status;             /* Status of the POLL request */
    UINT8           rc;                 /* RC (request code) used in the POLL request */
    UINT8           response_num;       /* Number of SENSF_RES responses */
    UINT8           response_bufsize;   /* Size of SENSF_RES responses */
    UINT8           *response_buf;      /* Buffer of responses (length + SENSF_RES) see $8.1.2.2 of NCI specs */
} tRW_T3T_POLL;

typedef struct
{
    tNFC_STATUS     status;             /* Status of the Get System Codes request */
    UINT8           num_system_codes;   /* Number of system codes */
    UINT16          *p_system_codes;    /* Table of system codes */
} tRW_T3T_SYSTEM_CODES;

typedef struct
{
    tNFC_STATUS     status;             /* status of NDEF detection */
    tNFC_PROTOCOL   protocol;           /* protocol used to detect NDEF */
    UINT32          max_size;           /* max number of bytes available for NDEF data */
    UINT32          cur_size;           /* current size of stored NDEF data (in bytes) */
    tRW_NDEF_FLAG   flags;              /* Flags to indicate NDEF capability,formated,formatable and read only */
} tRW_DETECT_NDEF_DATA;

typedef struct
{
    tNFC_STATUS     status;             /* status of NDEF detection */
    tNFC_PROTOCOL   protocol;           /* protocol used to detect TLV */
    UINT8           num_bytes;          /* number of reserved/lock bytes based on the type of tlv */
} tRW_DETECT_TLV_DATA;

typedef struct
{
    tNFC_STATUS     status;
    BT_HDR         *p_data;
} tRW_READ_DATA;

typedef struct
{
    tNFC_STATUS     status;
    UINT8           sw1;
    UINT8           sw2;
} tRW_T4T_SW;

typedef struct                              /* RW_I93_INVENTORY_EVT        */
{
    tNFC_STATUS     status;                 /* status of Inventory command */
    UINT8           dsfid;                  /* DSFID                       */
    UINT8           uid[I93_UID_BYTE_LEN];  /* UID[0]:MSB, ... UID[7]:LSB  */
} tRW_I93_INVENTORY;

typedef struct                              /* RW_I93_DATA_EVT               */
{
    tNFC_STATUS     status;                 /* status of Read/Get security status command */
    UINT8           command;                /* sent command                  */
    BT_HDR         *p_data;                 /* block data of security status */
} tRW_I93_DATA;

typedef struct                              /* RW_I93_SYS_INFO_EVT             */
{
    tNFC_STATUS     status;                 /* status of Get Sys Info command  */
    UINT8           info_flags;             /* information flags               */
    UINT8           uid[I93_UID_BYTE_LEN];  /* UID[0]:MSB, ... UID[7]:LSB      */
    UINT8           dsfid;                  /* DSFID if I93_INFO_FLAG_DSFID    */
    UINT8           afi;                    /* AFI if I93_INFO_FLAG_AFI        */
    UINT16          num_block;              /* number of blocks if I93_INFO_FLAG_MEM_SIZE   */
    UINT8           block_size;             /* block size in byte if I93_INFO_FLAG_MEM_SIZE */
    UINT8           IC_reference;           /* IC Reference if I93_INFO_FLAG_IC_REF         */
} tRW_I93_SYS_INFO;

typedef struct                              /* RW_I93_CMD_CMPL_EVT             */
{
    tNFC_STATUS     status;                 /* status of sent command          */
    UINT8           command;                /* sent command                    */
    UINT8           error_code;             /* error code; I93_ERROR_CODE_XXX  */
} tRW_I93_CMD_CMPL;

typedef struct
{
    tNFC_STATUS     status;
    BT_HDR         *p_data;
} tRW_RAW_FRAME;

typedef union
{
    tNFC_STATUS             status;
    tRW_T3T_POLL            t3t_poll;   /* Response to t3t poll command          */
    tRW_T3T_SYSTEM_CODES    t3t_sc;     /* Received system codes from t3 tag     */
    tRW_DETECT_TLV_DATA     tlv;        /* The information of detected TLV data  */
    tRW_DETECT_NDEF_DATA    ndef;       /* The information of detected NDEF data */
    tRW_READ_DATA           data;       /* The received data from a tag          */
    tRW_RAW_FRAME           raw_frame;  /* Response of raw frame sent            */
    tRW_T4T_SW              t4t_sw;     /* Received status words from a tag      */
    tRW_I93_INVENTORY       i93_inventory;  /* ISO 15693 Inventory response      */
    tRW_I93_DATA            i93_data;       /* ISO 15693 Data response           */
    tRW_I93_SYS_INFO        i93_sys_info;   /* ISO 15693 System Information      */
    tRW_I93_CMD_CMPL        i93_cmd_cmpl;   /* ISO 15693 Command complete        */
} tRW_DATA;


typedef void (tRW_CBACK) (tRW_EVENT event, tRW_DATA *p_data);

/*******************************************************************************
**
** Function         RW_T1tRid
**
** Description      This function send a RID command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tRid (void);

/*******************************************************************************
**
** Function         RW_T1tReadAll
**
** Description      This function send a RALL command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tReadAll (void);

/*******************************************************************************
**
** Function         RW_T1tRead
**
** Description      This function send a READ command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tRead (UINT8 block, UINT8 byte);

/*******************************************************************************
**
** Function         RW_T1tWriteErase
**
** Description      This function send a WRITE-E command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tWriteErase (UINT8 block, UINT8 byte, UINT8 new_byte);

/*******************************************************************************
**
** Function         RW_T1tWriteNoErase
**
** Description      This function send a WRITE-NE command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tWriteNoErase (UINT8 block, UINT8 byte, UINT8 new_byte);

/*******************************************************************************
**
** Function         RW_T1tReadSeg
**
** Description      This function send a RSEG command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tReadSeg (UINT8 segment);

/*******************************************************************************
**
** Function         RW_T1tRead8
**
** Description      This function send a READ8 command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tRead8 (UINT8 block);

/*******************************************************************************
**
** Function         RW_T1tWriteErase8
**
** Description      This function send a WRITE-E8 command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tWriteErase8 (UINT8 block, UINT8 *p_new_dat);

/*******************************************************************************
**
** Function         RW_T1tWriteNoErase8
**
** Description      This function send a WRITE-NE8 command for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tWriteNoErase8 (UINT8 block, UINT8 *p_new_dat);

/*******************************************************************************
**
** Function         RW_T1tLocateTlv
**
** Description      This function is called to find the start of the given TLV
**
** Parameters:      void
**
** Returns          NCI_STATUS_OK, if detection was started. Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tLocateTlv (UINT8 tlv_type);

/*******************************************************************************
**
** Function         RW_T1tDetectNDef
**
** Description      This function can be called to detect if there is an NDEF
**                  message on the tag.
**
** Parameters:      void
**
** Returns          NCI_STATUS_OK, if detection was started. Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tDetectNDef (void);

/*******************************************************************************
**
** Function         RW_T1tReadNDef
**
** Description      This function can be called to read the NDEF message on the tag.
**
** Parameters:      p_buffer:   The buffer into which to read the NDEF message
**                  buf_len:    The length of the buffer
**
** Returns          NCI_STATUS_OK, if read was started. Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tReadNDef (UINT8 *p_buffer, UINT16 buf_len);

/*******************************************************************************
**
** Function         RW_T1tWriteNDef
**
** Description      This function can be called to write an NDEF message to the tag.
**
** Parameters:      msg_len:    The length of the buffer
**                  p_msg:      The NDEF message to write
**
** Returns          NCI_STATUS_OK, if write was started. Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tWriteNDef (UINT16 msg_len, UINT8 *p_msg);

/*******************************************************************************
**
** Function         RW_T1tSetTagReadOnly
**
** Description      This function can be called to set the tag in to read only
**                  state
**
** Parameters:      b_hard_lock: To hard lock or just soft lock the tag
**
** Returns          NCI_STATUS_OK, if set readonly operation started.
**                                 Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tSetTagReadOnly (BOOLEAN b_hard_lock);

/*****************************************************************************
**
** Function         RW_T1tPresenceCheck
**
** Description
**      Check if the tag is still in the field.
**
**      The RW_T1T_PRESENCE_CHECK_EVT w/ status is used to indicate presence
**      or non-presence.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T1tPresenceCheck (void);

/*****************************************************************************
**
** Function         RW_T1tFormatNDef
**
** Description
**      Format Tag content
**
** Returns
**      NFC_STATUS_OK, Command sent to format Tag
**      NFC_STATUS_REJECTED: Invalid HR0 and cannot format the tag
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
tNFC_STATUS RW_T1tFormatNDef (void);

/*******************************************************************************
**
** Function         RW_T2tLocateTlv
**
** Description      This function is called to find the start of the given TLV
**
** Returns          Pointer to the TLV, if successful. Otherwise, NULL.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tLocateTlv(UINT8 tlv_type);

/*******************************************************************************
**
** Function         RW_T2tRead
**
** Description      This function issues the Type 2 Tag READ command. When the
**                  operation is complete the callback function will be called
**                  with a RW_T2T_READ_EVT.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tRead (UINT16 block);

/*******************************************************************************
**
** Function         RW_T2tWrite
**
** Description      This function issues the Type 2 Tag WRITE command. When the
**                  operation is complete the callback function will be called
**                  with a RW_T2T_WRITE_EVT.
**
**                  p_write_data points to the array of 4 bytes to be written
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tWrite (UINT16 block, UINT8 *p_write_data);

/*******************************************************************************
**
** Function         RW_T2tSectorSelect
**
** Description      This function issues the Type 2 Tag SECTOR-SELECT command
**                  packet 1. If a NACK is received as the response, the callback
**                  function will be called with a RW_T2T_SECTOR_SELECT_EVT. If
**                  an ACK is received as the response, the command packet 2 with
**                  the given sector number is sent to the peer device. When the
**                  response for packet 2 is received, the callback function will
**                  be called with a RW_T2T_SECTOR_SELECT_EVT.
**
**                  A sector is 256 contiguous blocks (1024 bytes).
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tSectorSelect (UINT8 sector);

/*******************************************************************************
**
** Function         RW_T2tDetectNDef
**
** Description      This function will find NDEF message if any in the Tag
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tDetectNDef (BOOLEAN skip_dyn_locks);

/*******************************************************************************
**
** Function         RW_T2tReadNDef
**
** Description      This function can be called to read the NDEF message on the tag.
**
** Parameters:      p_buffer:   The buffer into which to read the NDEF message
**                  buf_len:    The length of the buffer
**
** Returns          NCI_STATUS_OK, if read was started. Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tReadNDef (UINT8 *p_buffer, UINT16 buf_len);

/*******************************************************************************
**
** Function         RW_T2tWriteNDef
**
** Description      This function can be called to write an NDEF message to the tag.
**
** Parameters:      msg_len:    The length of the buffer
**                  p_msg:      The NDEF message to write
**
** Returns          NCI_STATUS_OK, if write was started. Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tWriteNDef (UINT16 msg_len, UINT8 *p_msg );

/*******************************************************************************
**
** Function         RW_T2tSetTagReadOnly
**
** Description      This function can be called to set the tag in to read only
**                  state
**
** Parameters:      b_hard_lock:   To indicate hard lock the tag or not
**
** Returns          NCI_STATUS_OK, if set readonly operation started.
**                                 Otherwise, error status.
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tSetTagReadOnly (BOOLEAN b_hard_lock);

/*****************************************************************************
**
** Function         RW_T2tPresenceCheck
**
** Description
**      Check if the tag is still in the field.
**
**      The RW_T2T_PRESENCE_CHECK_EVT w/ status is used to indicate presence
**      or non-presence.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T2tPresenceCheck (void);

/*****************************************************************************
**
** Function         RW_T2tFormatNDef
**
** Description
**      Format Tag content
**
** Returns
**      NFC_STATUS_OK, Command sent to format Tag
**      NFC_STATUS_FAILED: otherwise
**
*****************************************************************************/
tNFC_STATUS RW_T2tFormatNDef (void);

/*****************************************************************************
**
** Function         RW_T3tDetectNDef
**
** Description
**      This function is used to perform NDEF detection on a Type 3 tag, and
**      retrieve the tag's NDEF attribute information (block 0).
**
**      Before using this API, the application must call RW_SelectTagType to
**      indicate that a Type 3 tag has been activated, and to provide the
**      tag's Manufacture ID (IDm) .
**
** Returns
**      NFC_STATUS_OK: ndef detection procedure started
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tDetectNDef (void);

/*****************************************************************************
**
** Function         RW_T3tFormatNDef
**
** Description
**      Format a type-3 tag for NDEF.
**
**      Only Felica-Lite tags are supported by this API. The
**      RW_T3T_FORMAT_CPLT_EVT is used to notify the status of the operation.
**
** Returns
**      NFC_STATUS_OK: ndef detection procedure started
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tFormatNDef (void);

/*****************************************************************************
**
** Function         RW_T3tSetReadOnly
**
** Description
**      Set a type-3 tag to Read Only
**
**      Only Felica-Lite tags are supported by this API.
**      RW_T3tDetectNDef() must be called before using this
**      The RW_T3T_SET_READ_ONLY_CPLT_EVT event will be returned.
**
** Returns
**      NFC_STATUS_OK if success
**      NFC_STATUS_FAILED if T3T is busy or other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tSetReadOnly (BOOLEAN b_hard_lock);

/*****************************************************************************
**
** Function         RW_T3tCheckNDef
**
** Description
**      Retrieve NDEF contents from a Type3 tag.
**
**      The RW_T3T_CHECK_EVT event is used to notify the application for each
**      segment of NDEF data received. The RW_T3T_CHECK_CPLT_EVT event is used to
**      notify the application all segments have been received.
**
**      Before using this API, the RW_T3tDetectNDef function must be called to
**      verify that the tag contains NDEF data, and to retrieve the NDEF
**      attributes.
**
**      Internally, this command will be separated into multiple Tag 3 Check
**      commands (if necessary) - depending on the tag's Nbr (max number of
**      blocks per read) attribute.
**
** Returns
**      NFC_STATUS_OK: check command started
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tCheckNDef (void);

/*****************************************************************************
**
** Function         RW_T3tUpdateNDef
**
** Description
**      Write NDEF contents to a Type3 tag.
**
**      The RW_T3T_UPDATE_CPLT_EVT callback event will be used to notify the
**      application of the response.
**
**      Before using this API, the RW_T3tDetectNDef function must be called to
**      verify that the tag contains NDEF data, and to retrieve the NDEF
**      attributes.
**
**      Internally, this command will be separated into multiple Tag 3 Update
**      commands (if necessary) - depending on the tag's Nbw (max number of
**      blocks per write) attribute.
**
** Returns
**      NFC_STATUS_OK: check command started
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_REFUSED: tag is read-only
**      NFC_STATUS_BUFFER_FULL: len exceeds tag's maximum size
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tUpdateNDef (UINT32 len, UINT8 *p_data);

/*****************************************************************************
**
** Function         RW_T3tCheck
**
** Description
**      Read (non-NDEF) contents from a Type3 tag.
**
**      The RW_READ_EVT event is used to notify the application for each
**      segment of NDEF data received. The RW_READ_CPLT_EVT event is used to
**      notify the application all segments have been received.
**
**      Before using this API, the application must call RW_SelectTagType to
**      indicate that a Type 3 tag has been activated, and to provide the
**      tag's Manufacture ID (IDm) .
**
** Returns
**      NFC_STATUS_OK: check command started
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tCheck (UINT8 num_blocks, tT3T_BLOCK_DESC *t3t_blocks);

/*****************************************************************************
**
** Function         RW_T3tUpdate
**
** Description
**      Write (non-NDEF) contents to a Type3 tag.
**
**      The RW_WRITE_CPLT_EVT event is used to notify the application all
**      segments have been received.
**
**      Before using this API, the application must call RW_SelectTagType to
**      indicate that a Type 3 tag has been activated, and to provide the tag's
**      Manufacture ID (IDm) .
**
** Returns
**      NFC_STATUS_OK: check command started
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tUpdate (UINT8 num_blocks, tT3T_BLOCK_DESC *t3t_blocks, UINT8 *p_data);

/*****************************************************************************
**
** Function         RW_T3tSendRawFrame
**
** Description
**      This function is called to send a raw data frame to the peer device.
**      When type 3 tag receives response from peer, the callback function
**      will be called with a RW_T3T_RAW_FRAME_EVT [Table 6].
**
**      Before using this API, the application must call RW_SelectTagType to
**      indicate that a Type 3 tag has been activated.
**
**      The raw frame should be a properly formatted Type 3 tag message.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tSendRawFrame (UINT16 len, UINT8 *p_data);

/*****************************************************************************
**
** Function         RW_T3tPoll
**
** Description
**      Send POLL command
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tPoll (UINT16 system_code, tT3T_POLL_RC rc, UINT8 tsn);

/*****************************************************************************
**
** Function         RW_T3tPresenceCheck
**
** Description
**      Check if the tag is still in the field.
**
**      The RW_T3T_PRESENCE_CHECK_EVT w/ status is used to indicate presence
**      or non-presence.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tPresenceCheck (void);

/*****************************************************************************
**
** Function         RW_T3tGetSystemCodes
**
** Description
**      Get systems codes supported by the activated tag:
**              Poll for wildcard (FFFF):
**                  - If felica-lite code then poll for ndef (12fc)
**                  - Otherwise send RequestSystmCode command to get
**                    system codes.
**
**      Before using this API, the application must call RW_SelectTagType to
**      indicate that a Type 3 tag has been activated.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T3tGetSystemCodes (void);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/*****************************************************************************
**
** Function         RW_T4tFormatNDef
**
** Description
**      Format a type-4 tag for NDEF.
**
**      Only Desifire tags are supported by this API. The
**      RW_T4T_FORMAT_CPLT_EVT is used to notify the status of the operation.
**
** Returns
**      NFC_STATUS_OK: if success
**      NFC_STATUS_FAILED: other error
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T4tFormatNDef (void);
#endif

/*******************************************************************************
**
** Function         RW_T4tDetectNDef
**
** Description      This function performs NDEF detection procedure
**
**                  RW_T4T_NDEF_DETECT_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T4tDetectNDef (void);

/*******************************************************************************
**
** Function         RW_T4tReadNDef
**
** Description      This function performs NDEF read procedure
**                  Note: RW_T4tDetectNDef() must be called before using this
**
**                  The following event will be returned
**                      RW_T4T_NDEF_READ_EVT for each segmented NDEF message
**                      RW_T4T_NDEF_READ_CPLT_EVT for the last segment or complete NDEF
**                      RW_T4T_NDEF_READ_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T4tReadNDef (void);

/*******************************************************************************
**
** Function         RW_T4tUpdateNDef
**
** Description      This function performs NDEF update procedure
**                  Note: RW_T4tDetectNDef() must be called before using this
**                        Updating data must not be removed until returning event
**
**                  The following event will be returned
**                      RW_T4T_NDEF_UPDATE_CPLT_EVT for complete
**                      RW_T4T_NDEF_UPDATE_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_T4tUpdateNDef (UINT16 length, UINT8 *p_data);

/*****************************************************************************
**
** Function         RW_T4tPresenceCheck
**
** Description
**      Check if the tag is still in the field.
**
**      The RW_T4T_PRESENCE_CHECK_EVT w/ status is used to indicate presence
**      or non-presence.
**      option is RW_T4T_CHK_EMPTY_I_BLOCK, use empty I block for presence check.
**
** Returns
**      NFC_STATUS_OK, if raw data frame sent
**      NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**      NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T4tPresenceCheck (UINT8 option);

/*****************************************************************************
**
** Function         RW_T4tSetNDefReadOnly
**
** Description      This function performs NDEF read-only procedure
**                  Note: RW_T4tDetectNDef() must be called before using this
**
**                  The RW_T4T_SET_TO_RO_EVT event will be returned.
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_T4tSetNDefReadOnly (void);

/*******************************************************************************
**
** Function         RW_I93Inventory
**
** Description      This function send Inventory command with/without AFI
**                  If UID is provided then set UID[0]:MSB, ... UID[7]:LSB
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_FAILED if T4T is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93Inventory (BOOLEAN including_afi, UINT8 afi, UINT8 *p_uid);

/*******************************************************************************
**
** Function         RW_I93StayQuiet
**
** Description      This function send Inventory command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93StayQuiet (void);

/*******************************************************************************
**
** Function         RW_I93ReadSingleBlock
**
** Description      This function send Read Single Block command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93ReadSingleBlock (UINT16 block_number);

/*******************************************************************************
**
** Function         RW_I93WriteSingleBlock
**
** Description      This function send Write Single Block command
**                  Application must get block size first by calling RW_I93GetSysInfo().
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93WriteSingleBlock (UINT16 block_number,
                                                   UINT8  *p_data);

/*******************************************************************************
**
** Function         RW_I93LockBlock
**
** Description      This function send Lock Block command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93LockBlock (UINT8 block_number);

/*******************************************************************************
**
** Function         RW_I93ReadMultipleBlocks
**
** Description      This function send Read Multiple Blocks command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93ReadMultipleBlocks (UINT16 first_block_number,
                                                     UINT16 number_blocks);

/*******************************************************************************
**
** Function         RW_I93WriteMultipleBlocks
**
** Description      This function send Write Multiple Blocks command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93WriteMultipleBlocks (UINT8  first_block_number,
                                                      UINT16 number_blocks,
                                                      UINT8 *p_data);

/*******************************************************************************
**
** Function         RW_I93Select
**
** Description      This function send Select command
**
**                  UID[0]: 0xE0, MSB
**                  UID[1]: IC Mfg Code
**                  ...
**                  UID[7]: LSB
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93Select (UINT8 *p_uid);

/*******************************************************************************
**
** Function         RW_I93ResetToReady
**
** Description      This function send Reset To Ready command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93ResetToReady (void);

/*******************************************************************************
**
** Function         RW_I93WriteAFI
**
** Description      This function send Write AFI command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93WriteAFI (UINT8 afi);

/*******************************************************************************
**
** Function         RW_I93LockAFI
**
** Description      This function send Lock AFI command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93LockAFI (void);

/*******************************************************************************
**
** Function         RW_I93WriteDSFID
**
** Description      This function send Write DSFID command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93WriteDSFID (UINT8 dsfid);

/*******************************************************************************
**
** Function         RW_I93LockDSFID
**
** Description      This function send Lock DSFID command
**
**                  RW_I93_CMD_CMPL_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93LockDSFID (void);

/*******************************************************************************
**
** Function         RW_I93GetSysInfo
**
** Description      This function send Get System Information command
**                  If UID is provided then set UID[0]:MSB, ... UID[7]:LSB
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93GetSysInfo (UINT8 *p_uid);

/*******************************************************************************
**
** Function         RW_I93GetMultiBlockSecurityStatus
**
** Description      This function send Get Multiple Block Security Status command
**
**                  RW_I93_RESPONSE_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_NO_BUFFERS if out of buffer
**                  NFC_STATUS_BUSY if busy
**                  NFC_STATUS_FAILED if other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93GetMultiBlockSecurityStatus (UINT16 first_block_number,
                                                              UINT16 number_blocks);

/*******************************************************************************
**
** Function         RW_I93DetectNDef
**
** Description      This function performs NDEF detection procedure
**
**                  RW_I93_NDEF_DETECT_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93DetectNDef (void);

/*******************************************************************************
**
** Function         RW_I93ReadNDef
**
** Description      This function performs NDEF read procedure
**                  Note: RW_I93DetectNDef() must be called before using this
**
**                  The following event will be returned
**                      RW_I93_NDEF_READ_EVT for each segmented NDEF message
**                      RW_I93_NDEF_READ_CPLT_EVT for the last segment or complete NDEF
**                      RW_I93_NDEF_READ_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if I93 is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93ReadNDef (void);

/*******************************************************************************
**
** Function         RW_I93UpdateNDef
**
** Description      This function performs NDEF update procedure
**                  Note: RW_I93DetectNDef() must be called before using this
**                        Updating data must not be removed until returning event
**
**                  The following event will be returned
**                      RW_I93_NDEF_UPDATE_CPLT_EVT for complete
**                      RW_I93_NDEF_UPDATE_FAIL_EVT for failure
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if I93 is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93UpdateNDef (UINT16 length, UINT8 *p_data);

/*******************************************************************************
**
** Function         RW_I93FormatNDef
**
** Description      This function performs formatting procedure
**
**                  RW_I93_FORMAT_CPLT_EVT will be returned
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93FormatNDef (void);

/*******************************************************************************
**
** Function         RW_I93SetTagReadOnly
**
** Description      This function performs NDEF read-only procedure
**                  Note: RW_I93DetectNDef() must be called before using this
**                        Updating data must not be removed until returning event
**
**                  The RW_I93_SET_TAG_RO_EVT event will be returned.
**
** Returns          NFC_STATUS_OK if success
**                  NFC_STATUS_FAILED if I93 is busy or other error
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_I93SetTagReadOnly (void);

/*****************************************************************************
**
** Function         RW_I93PresenceCheck
**
** Description      Check if the tag is still in the field.
**
**                  The RW_I93_PRESENCE_CHECK_EVT w/ status is used to indicate
**                  presence or non-presence.
**
** Returns          NFC_STATUS_OK, if raw data frame sent
**                  NFC_STATUS_NO_BUFFERS: unable to allocate a buffer for this operation
**                  NFC_STATUS_FAILED: other error
**
*****************************************************************************/
NFC_API extern tNFC_STATUS RW_I93PresenceCheck (void);

/*******************************************************************************
**
** Function         RW_SendRawFrame
**
** Description      This function sends a raw frame to the peer device.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_SendRawFrame (UINT8 *p_raw_data, UINT16 data_len);

/*******************************************************************************
**
** Function         RW_SetActivatedTagType
**
** Description      This function sets tag type for Reader/Writer mode.
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
NFC_API extern tNFC_STATUS RW_SetActivatedTagType (tNFC_ACTIVATE_DEVT *p_activate_params, tRW_CBACK *p_cback);

/*******************************************************************************
**
** Function         RW_SetTraceLevel
**
** Description      This function sets the trace level for Reader/Writer mode.
**                  If called with a value of 0xFF,
**                  it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
NFC_API extern UINT8 RW_SetTraceLevel (UINT8 new_level);

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
NFC_API extern tNFC_STATUS RW_T3BtGetPupiID();
#endif

#endif /* RW_API_H */
