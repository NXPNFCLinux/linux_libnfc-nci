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
 *  related internal function / definitions.
 *
 ******************************************************************************/

#ifndef RW_INT_H_
#define RW_INT_H_

#include "tags_defs.h"
#include "tags_int.h"
#include "rw_api.h"

/* Proprietary definitions for HR0 and HR1 */
#define RW_T1T_HR0_HI_NIB                       0xF0    /* HI NIB Tag                                               */
#define RW_T1T_IS_JEWEL64                       0x20    /* Jewel 64 Tag                                             */
#define RW_T1T_IS_JEWEL                         0x00    /* Jewel Tag                                                */
#define RW_T1T_IS_TOPAZ                         0x10    /* TOPAZ Tag                                                */
#define RW_T1T_IS_TOPAZ96                       0x11    /* TOPAZ96 Tag                                              */
#define RW_T1T_IS_TOPAZ512                      0x12    /* TOPAZ512 Tag                                             */
#define RW_T1T_HR1_MIN                          0x49    /* Supports dynamic commands on static tag if HR1 > 0x49    */

#define RW_T1T_MAX_MEM_TLVS                     0x05    /* Maximum supported Memory control TLVS in the tag         */
#define RW_T1T_MAX_LOCK_TLVS                    0x05    /* Maximum supported Lock control TLVS in the tag           */
#define RW_T1T_MAX_LOCK_BYTES                   0x1E    /* Maximum supported dynamic lock bytes                     */

/* State of the Tag as interpreted by RW */
#define RW_T1_TAG_ATTRB_UNKNOWN                 0x00    /* TAG State is unknown to RW                               */
#define RW_T1_TAG_ATTRB_INITIALIZED             0x01    /* TAG is in INITIALIZED state                              */
#define RW_T1_TAG_ATTRB_INITIALIZED_NDEF        0x02    /* TAG is in INITIALIZED state and has NDEF tlv with len=0  */
#define RW_T1_TAG_ATTRB_READ_ONLY               0x03    /* TAG is in READ ONLY state                                */
#define RW_T1_TAG_ATTRB_READ_WRITE              0x04    /* TAG is in READ WRITE state                               */

#define RW_T1T_LOCK_NOT_UPDATED                 0x00    /* Lock not yet set as part of SET TAG RO op                */
#define RW_T1T_LOCK_UPDATE_INITIATED            0x01    /* Sent command to set the Lock bytes                       */
#define RW_T1T_LOCK_UPDATED                     0x02    /* Lock bytes are set                                       */
typedef UINT8 tRW_T1T_LOCK_STATUS;

/* States */
#define RW_T1T_STATE_NOT_ACTIVATED              0x00    /* Tag not activated and or response not received for RID   */
#define RW_T1T_STATE_IDLE                       0x01    /* T1 Tag activated and ready to perform rw operation on Tag*/
#define RW_T1T_STATE_READ                       0x02    /* waiting rsp for read command sent to tag                 */
#define RW_T1T_STATE_WRITE                      0x03    /* waiting rsp for write command sent to tag                */
#define RW_T1T_STATE_TLV_DETECT                 0x04    /* performing TLV detection procedure                       */
#define RW_T1T_STATE_READ_NDEF                  0x05    /* performing read NDEF procedure                           */
#define RW_T1T_STATE_WRITE_NDEF                 0x06    /* performing update NDEF procedure                         */
#define RW_T1T_STATE_SET_TAG_RO                 0x07    /* Setting Tag as read only tag                             */
#define RW_T1T_STATE_CHECK_PRESENCE             0x08    /* Check if Tag is still present                            */
#define RW_T1T_STATE_FORMAT_TAG                 0x09    /* Format T1 Tag                                            */

/* Sub states */
#define RW_T1T_SUBSTATE_NONE                    0x00    /* Default substate                                         */

/* Sub states in RW_T1T_STATE_TLV_DETECT state */
#define RW_T1T_SUBSTATE_WAIT_TLV_DETECT         0x01    /* waiting for the detection of a tlv in a tag              */
#define RW_T1T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN 0x02    /* waiting for finding the len field is 1 or 3 bytes long   */
#define RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN0      0x03    /* waiting for extracting len field value                   */
#define RW_T1T_SUBSTATE_WAIT_READ_TLV_LEN1      0x04    /* waiting for extracting len field value                   */
#define RW_T1T_SUBSTATE_WAIT_READ_TLV_VALUE     0x05    /* waiting for extracting value field in the TLV            */
#define RW_T1T_SUBSTATE_WAIT_READ_LOCKS         0x06    /* waiting for reading dynamic locks in the TLV             */

/* Sub states in RW_T1T_STATE_WRITE_NDEF state */
#define RW_T1T_SUBSTATE_WAIT_READ_NDEF_BLOCK    0x07    /* waiting for response of reading a block that will be partially updated */
#define RW_T1T_SUBSTATE_WAIT_INVALIDATE_NDEF    0x08    /* waiting for response of invalidating NDEF Msg                          */
#define RW_T1T_SUBSTATE_WAIT_NDEF_WRITE         0x09    /* waiting for response of writing a part of NDEF Msg                     */
#define RW_T1T_SUBSTATE_WAIT_NDEF_UPDATED       0x0A    /* waiting for response of writing last part of NDEF Msg                  */
#define RW_T1T_SUBSTATE_WAIT_VALIDATE_NDEF      0x0B    /* waiting for response of validating NDEF Msg                            */

/* Sub states in RW_T1T_STATE_SET_TAG_RO state */
#define RW_T1T_SUBSTATE_WAIT_SET_CC_RWA_RO      0x0C    /* waiting for response of setting CC-RWA to read only      */
#define RW_T1T_SUBSTATE_WAIT_SET_ST_LOCK_BITS   0x0D    /* waiting for response of setting all static lock bits     */
#define RW_T1T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS  0x0E    /* waiting for response of setting all dynamic lock bits    */

/* Sub states in RW_T1T_STATE_FORMAT_TAG state */
#define RW_T1T_SUBSTATE_WAIT_SET_CC             0x0F    /* waiting for response to format/set capability container  */
#define RW_T1T_SUBSTATE_WAIT_SET_NULL_NDEF      0x10    /* waiting for response to format/set NULL NDEF             */


typedef struct
{
    UINT16  offset;                                     /* Offset of the lock byte in the Tag                   */
    UINT8   num_bits;                                   /* Number of lock bits in the lock byte                 */
    UINT8   bytes_locked_per_bit;                       /* No. of tag bytes gets locked by a bit in this byte   */
} tRW_T1T_LOCK_INFO;

typedef struct
{
    UINT16  offset;                                     /* Reserved bytes offset taken from Memory control TLV  */
    UINT8   num_bytes;                                  /* Number of reserved bytes as per the TLV              */
}tRW_T1T_RES_INFO;

typedef struct
{
    UINT8               tlv_index;                      /* Index of Lock control tlv that points to this address*/
    UINT8               byte_index;                     /* Index of Lock byte pointed by the TLV                */
    UINT8               lock_byte;                      /* Value in the lock byte                               */
    tRW_T1T_LOCK_STATUS lock_status;                    /* Indicates if it is modifed to set tag as Read only   */
    BOOLEAN             b_lock_read;                    /* Is the lock byte is already read from tag            */
} tRW_T1T_LOCK;

typedef struct
{
    UINT8               addr;                           /* ADD/ADD8/ADDS field value                            */
    UINT8               op_code;                        /* Command sent                                         */
    UINT8               rsp_len;                        /* expected length of the response                      */
    UINT8               pend_retx_rsp;                  /* Number of pending rsps to retransmission on prev cmd */
} tRW_T1T_PREV_CMD_RSP_INFO;

#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))
#define T1T_BUFFER_SIZE             T1T_STATIC_SIZE     /* Buffer 0-E block, for easier tlv operation           */
#else
#define T1T_BUFFER_SIZE             T1T_UID_LEN         /* Buffer UID                                           */
#endif

/* RW Type 1 Tag control blocks */
typedef struct
{
    UINT8               hr[T1T_HR_LEN];                     /* Header ROM byte 0 - 0x1y,Header ROM byte 1 - 0x00    */
    UINT8               mem[T1T_SEGMENT_SIZE];              /* Tag contents of block 0 or from block 0-E            */
    tT1T_CMD_RSP_INFO   *p_cmd_rsp_info;                    /* Pointer to Command rsp info of last sent command     */
    UINT8               state;                              /* Current state of RW module                           */
    UINT8               tag_attribute;                      /* Present state of the Tag as interpreted by RW        */
    BT_HDR              *p_cur_cmd_buf;                     /* Buffer to hold cur sent command for retransmission   */
    UINT8               addr;                               /* ADD/ADD8/ADDS value                                  */
    tRW_T1T_PREV_CMD_RSP_INFO prev_cmd_rsp_info;            /* Information about previous sent command if retx      */
    TIMER_LIST_ENT      timer;                              /* timer to set timelimit for the response to command   */
    BOOLEAN             b_update;                           /* Tag header updated                                   */
    BOOLEAN             b_rseg;                             /* Segment 0 read from tag                              */
    BOOLEAN             b_hard_lock;                        /* Hard lock the tag as part of config tag to Read only */
#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))
    UINT8               segment;                            /* Current Tag segment                                  */
    UINT8               substate;                           /* Current substate of RW module                        */
    UINT16              work_offset;                        /* Working byte offset                                  */
    UINT8               ndef_first_block[T1T_BLOCK_SIZE];   /* Buffer for ndef first block                          */
    UINT8               ndef_final_block[T1T_BLOCK_SIZE];   /* Buffer for ndef last block                           */
    UINT8               *p_ndef_buffer;                     /* Buffer to store ndef message                         */
    UINT16              new_ndef_msg_len;                   /* Lenght of new updating NDEF Message                  */
    UINT8               block_read;                         /* Last read Block                                      */
    UINT8               write_byte;                         /* Index of last written byte                           */
    UINT8               tlv_detect;                         /* TLV type under detection                             */
    UINT16              ndef_msg_offset;                    /* The offset on Tag where first NDEF message is present*/
    UINT16              ndef_msg_len;                       /* Lenght of NDEF Message                               */
    UINT16              max_ndef_msg_len;                   /* Maximum size of NDEF that can be written on the tag  */
    UINT16              ndef_header_offset;                 /* The offset on Tag where first NDEF tlv is present    */
    UINT8               ndef_block_written;                 /* Last block where NDEF bytes are written              */
    UINT8               num_ndef_finalblock;                /* Block number where NDEF's last byte will be present  */
    UINT8               num_lock_tlvs;                      /* Number of lcok tlvs detected in the tag              */
    tRW_T1T_LOCK_INFO   lock_tlv[RW_T1T_MAX_LOCK_TLVS];     /* Information retrieved from lock control tlv          */
    UINT8               num_lockbytes;                      /* Number of dynamic lock bytes present in the tag      */
    tRW_T1T_LOCK        lockbyte[RW_T1T_MAX_LOCK_BYTES];    /* Dynamic Lock byte information                        */
    UINT8               num_mem_tlvs;                       /* Number of memory tlvs detected in the tag            */
    tRW_T1T_RES_INFO    mem_tlv[RW_T1T_MAX_MEM_TLVS];       /* Information retrieved from mem tlv                   */
    UINT8               attr_seg;                           /* Tag segment for which attributes are prepared        */
    UINT8               lock_attr_seg;                      /* Tag segment for which lock attributes are prepared   */
    UINT8               attr[T1T_BLOCKS_PER_SEGMENT];       /* byte information - Reserved/lock/otp or data         */
    UINT8               lock_attr[T1T_BLOCKS_PER_SEGMENT];  /* byte information - read only or read write           */
#endif
} tRW_T1T_CB;

/* Mifare Ultalight/ Ultralight Family blank tag version block settings */
#define T2T_MIFARE_VERSION_BLOCK                        0x04    /* Block where version number of the tag is stored */
#define T2T_MIFARE_ULTRALIGHT_VER_NO                    0xFFFF  /* Blank Ultralight tag - Block 4 (byte 0, byte 1) */
#define T2T_MIFARE_ULTRALIGHT_FAMILY_VER_NO             0x0200  /* Blank Ultralight family tag - Block 4 (byte 0, byte 1) */

/* Infineon my-d move / my-d blank tag uid block settings */
#define T2T_INFINEON_VERSION_BLOCK                      0x00
#define T2T_INFINEON_MYD_MOVE_LEAN                      0x0570
#define T2T_INFINEON_MYD_MOVE                           0x0530

#define T2T_BRCM_VERSION_BLOCK                          0x00
#define T2T_BRCM_STATIC_MEM                             0x2E01
#define T2T_BRCM_DYNAMIC_MEM                            0x2E02

#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
/* CC2 value on MiFare ULC tag */
#define T2T_MIFARE_ULC_TMS                              0x12
/* Possible corrupt cc2 value range on MiFare ULC tags */
#define T2T_INVALID_CC_TMS_VAL0                         0x10
#define T2T_INVALID_CC_TMS_VAL1                         0x1F
#endif

#define T2T_NDEF_NOT_DETECTED                           0x00
#define T2T_NDEF_DETECTED                               0x01
#define T2T_NDEF_READ                                   0x02

#define T2T_MAX_NDEF_OFFSET                             128     /* Max offset of an NDEF message in a T2 tag */
#define T2T_MAX_RESERVED_BYTES_IN_TAG                   0x64
#define T2T_MAX_LOCK_BYTES_IN_TAG                       0x64

#define RW_T2T_MAX_MEM_TLVS                             0x05    /* Maximum supported Memory control TLVS in the tag         */
#define RW_T2T_MAX_LOCK_TLVS                            0x05    /* Maximum supported Lock control TLVS in the tag           */
#define RW_T2T_MAX_LOCK_BYTES                           0x1E    /* Maximum supported dynamic lock bytes                     */
#define RW_T2T_SEGMENT_BYTES                            128
#define RW_T2T_SEGMENT_SIZE                             16

#define RW_T2T_LOCK_NOT_UPDATED                         0x00    /* Lock not yet set as part of SET TAG RO op                */
#define RW_T2T_LOCK_UPDATE_INITIATED                    0x01    /* Sent command to set the Lock bytes                       */
#define RW_T2T_LOCK_UPDATED                             0x02    /* Lock bytes are set                                       */
typedef UINT8 tRW_T2T_LOCK_STATUS;


/* States */
#define RW_T2T_STATE_NOT_ACTIVATED                      0x00    /* Tag not activated                                        */
#define RW_T2T_STATE_IDLE                               0x01    /* T1 Tag activated and ready to perform rw operation on Tag*/
#define RW_T2T_STATE_READ                               0x02    /* waiting response for read command sent to tag            */
#define RW_T2T_STATE_WRITE                              0x03    /* waiting response for write command sent to tag           */
#define RW_T2T_STATE_SELECT_SECTOR                      0x04    /* Waiting response for sector select command               */
#define RW_T2T_STATE_DETECT_TLV                         0x05    /* Detecting Lock/Memory/NDEF/Proprietary TLV in the Tag    */
#define RW_T2T_STATE_READ_NDEF                          0x06    /* Performing NDEF Read procedure                           */
#define RW_T2T_STATE_WRITE_NDEF                         0x07    /* Performing NDEF Write procedure                          */
#define RW_T2T_STATE_SET_TAG_RO                         0x08    /* Setting Tag as Read only tag                             */
#define RW_T2T_STATE_CHECK_PRESENCE                     0x09    /* Check if Tag is still present                            */
#define RW_T2T_STATE_FORMAT_TAG                         0x0A    /* Format the tag                                           */
#define RW_T2T_STATE_HALT                               0x0B    /* Tag is in HALT State */

/* rw_t2t_read/rw_t2t_write takes care of sector change if the block to read/write is in a different sector
 * Next Substate should be assigned to control variable 'substate' before calling these function for State Machine to
 * move back to the particular substate after Sector change is completed and read/write command is sent on new sector       */

/* Sub states */
#define RW_T2T_SUBSTATE_NONE                            0x00

/* Sub states in RW_T2T_STATE_SELECT_SECTOR state */
#define RW_T2T_SUBSTATE_WAIT_SELECT_SECTOR_SUPPORT      0x01    /* waiting for response of sector select CMD 1              */
#define RW_T2T_SUBSTATE_WAIT_SELECT_SECTOR              0x02    /* waiting for response of sector select CMD 2              */

/* Sub states in RW_T1T_STATE_DETECT_XXX state */
#define RW_T2T_SUBSTATE_WAIT_READ_CC                    0x03    /* waiting for the detection of a tlv in a tag              */
#define RW_T2T_SUBSTATE_WAIT_TLV_DETECT                 0x04    /* waiting for the detection of a tlv in a tag              */
#define RW_T2T_SUBSTATE_WAIT_FIND_LEN_FIELD_LEN         0x05    /* waiting for finding the len field is 1 or 3 bytes long   */
#define RW_T2T_SUBSTATE_WAIT_READ_TLV_LEN0              0x06    /* waiting for extracting len field value                   */
#define RW_T2T_SUBSTATE_WAIT_READ_TLV_LEN1              0x07    /* waiting for extracting len field value                   */
#define RW_T2T_SUBSTATE_WAIT_READ_TLV_VALUE             0x08    /* waiting for extracting value field in the TLV            */
#define RW_T2T_SUBSTATE_WAIT_READ_LOCKS                 0x09    /* waiting for reading dynamic locks in the TLV             */

/* Sub states in RW_T2T_STATE_WRITE_NDEF state */
#define RW_T2T_SUBSTATE_WAIT_READ_NDEF_FIRST_BLOCK      0x0A    /* waiting for rsp to reading the block where NDEF starts   */
#define RW_T2T_SUBSTATE_WAIT_READ_NDEF_LAST_BLOCK       0x0B    /* waiting for rsp to reading block where new NDEF Msg ends */
#define RW_T2T_SUBSTATE_WAIT_READ_TERM_TLV_BLOCK        0x0C    /* waiting for rsp to reading block where Trm tlv gets added*/
#define RW_T2T_SUBSTATE_WAIT_READ_NDEF_NEXT_BLOCK       0x0D    /* waiting for rsp to reading block where nxt NDEF write    */
#define RW_T2T_SUBSTATE_WAIT_WRITE_NDEF_NEXT_BLOCK      0x0E    /* waiting for rsp to writting NDEF block                   */
#define RW_T2T_SUBSTATE_WAIT_WRITE_NDEF_LAST_BLOCK      0x0F    /* waiting for rsp to last NDEF block write cmd             */
#define RW_T2T_SUBSTATE_WAIT_READ_NDEF_LEN_BLOCK        0x10    /* waiting for rsp to reading NDEF len field block          */
#define RW_T2T_SUBSTATE_WAIT_WRITE_NDEF_LEN_BLOCK       0x11    /* waiting for rsp of updating first NDEF len field block   */
#define RW_T2T_SUBSTATE_WAIT_WRITE_NDEF_LEN_NEXT_BLOCK  0x12    /* waiting for rsp of updating next NDEF len field block    */
#define RW_T2T_SUBSTATE_WAIT_WRITE_TERM_TLV_CMPLT       0x13    /* waiting for rsp to writing to Terminator tlv             */

/* Sub states in RW_T2T_STATE_FORMAT_TAG state */
#define RW_T2T_SUBSTATE_WAIT_READ_VERSION_INFO          0x14
#define RW_T2T_SUBSTATE_WAIT_SET_CC                     0x15    /* waiting for response to format/set capability container  */
#define RW_T2T_SUBSTATE_WAIT_SET_LOCK_TLV               0x16
#define RW_T2T_SUBSTATE_WAIT_SET_NULL_NDEF              0x17    /* waiting for response to format/set NULL NDEF             */

/* Sub states in RW_T2T_STATE_SET_TAG_RO state */
#define RW_T2T_SUBSTATE_WAIT_SET_CC_RO                  0x19    /* waiting for response to set CC3 to RO                    */
#define RW_T2T_SUBSTATE_WAIT_READ_DYN_LOCK_BYTE_BLOCK   0x1A    /* waiting for response to read dynamic lock bytes block    */
#define RW_T2T_SUBSTATE_WAIT_SET_DYN_LOCK_BITS          0x1B    /* waiting for response to set dynamic lock bits            */
#define RW_T2T_SUBSTATE_WAIT_SET_ST_LOCK_BITS           0x1C    /* waiting for response to set static lock bits             */

typedef struct
{
    UINT16              offset;                             /* Offset of the lock byte in the Tag                       */
    UINT8               num_bits;                           /* Number of lock bits in the lock byte                     */
    UINT8               bytes_locked_per_bit;               /* No. of tag bytes gets locked by a bit in this byte       */
} tRW_T2T_LOCK_INFO;

typedef struct
{
    UINT16  offset;                                         /* Reserved bytes offset taken from Memory control TLV      */
    UINT8   num_bytes;                                      /* Number of reserved bytes as per the TLV                  */
}tRW_T2T_RES_INFO;

typedef struct
{
    UINT8               tlv_index;                          /* Index of Lock control tlv that points to this address    */
    UINT8               byte_index;                         /* Index of Lock byte pointed by the TLV                    */
    UINT8               lock_byte;                          /* Value in the lock byte                                   */
    tRW_T2T_LOCK_STATUS lock_status;                        /* Indicates if it is modifed to set tag as Read only       */
    BOOLEAN             b_lock_read;                        /* Is the lock byte is already read from tag                */
} tRW_T2T_LOCK;

/* RW Type 2 Tag control block */
typedef struct
{
    UINT8               state;                              /* Reader/writer state                                          */
    UINT8               substate;                           /* Reader/write substate in NDEF write state                    */
    UINT8               prev_substate;                      /* Substate of the tag before moving to different sector        */
    UINT8               sector;                             /* Sector number that is selected                               */
    UINT8               select_sector;                      /* Sector number that is expected to get selected               */
    UINT8               tag_hdr[T2T_READ_DATA_LEN];         /* T2T Header blocks                                            */
    UINT8               tag_data[T2T_READ_DATA_LEN];        /* T2T Block 4 - 7 data                                         */
    UINT8               ndef_status;                        /* The current status of NDEF Write operation                   */
    UINT16              block_read;                         /* Read block                                                   */
    UINT16              block_written;                      /* Written block                                                */
    tT2T_CMD_RSP_INFO   *p_cmd_rsp_info;                    /* Pointer to Command rsp info of last sent command             */
    BT_HDR              *p_cur_cmd_buf;                     /* Copy of current command, for retx/send after sector change   */
    BT_HDR              *p_sec_cmd_buf;                     /* Copy of command, to send after sector change                 */
    TIMER_LIST_ENT      t2_timer;                           /* timeout for each API call                                    */
    BOOLEAN             b_read_hdr;                         /* Tag header read from tag                                     */
    BOOLEAN             b_read_data;                        /* Tag data block read from tag                                 */
    BOOLEAN             b_hard_lock;                        /* Hard lock the tag as part of config tag to Read only         */
    BOOLEAN             check_tag_halt;                     /* Resent command after NACK rsp to find tag is in HALT State   */
#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))
    BOOLEAN             skip_dyn_locks;                     /* Skip reading dynamic lock bytes from the tag                 */
    UINT8               found_tlv;                          /* The Tlv found while searching a particular TLV               */
    UINT8               tlv_detect;                         /* TLV type under detection                                     */
    UINT8               num_lock_tlvs;                      /* Number of lcok tlvs detected in the tag                      */
    UINT8               attr_seg;                           /* Tag segment for which attributes are prepared                */
    UINT8               lock_attr_seg;                      /* Tag segment for which lock attributes are prepared           */
    UINT8               segment;                            /* Current operating segment                                    */
    UINT8               ndef_final_block[T2T_BLOCK_SIZE];   /* Buffer for ndef last block                                   */
    UINT8               num_mem_tlvs;                       /* Number of memory tlvs detected in the tag                    */
    UINT8               num_lockbytes;                      /* Number of dynamic lock bytes present in the tag              */
    UINT8               attr[RW_T2T_SEGMENT_SIZE];          /* byte information - Reserved/lock/otp or data                 */
    UINT8               lock_attr[RW_T2T_SEGMENT_SIZE];     /* byte information - read only or read write                   */
    UINT8               tlv_value[3];                       /* Read value field of TLV                                      */
    UINT8               ndef_first_block[T2T_BLOCK_LEN];    /* NDEF TLV Header block                                        */
    UINT8               ndef_read_block[T2T_BLOCK_LEN];     /* Buffer to hold read before write block                       */
    UINT8               ndef_last_block[T2T_BLOCK_LEN];     /* Terminator TLV block after NDEF Write operation              */
    UINT8               terminator_tlv_block[T2T_BLOCK_LEN];/* Terminator TLV Block                                         */
    UINT16              ndef_last_block_num;                /* Block where last byte of updating ndef message will exist    */
    UINT16              ndef_read_block_num;                /* Block read during NDEF Write to avoid overwritting res bytes */
    UINT16              bytes_count;                        /* No. of bytes remaining to collect during tlv detect          */
    UINT16              terminator_byte_index;              /* The offset of the tag where terminator tlv may be added      */
    UINT16              work_offset;                        /* Working byte offset                                          */
    UINT16              ndef_header_offset;
    UINT16              ndef_msg_offset;                    /* Offset on Tag where first NDEF message is present            */
    UINT16              ndef_msg_len;                       /* Lenght of NDEF Message                                       */
    UINT16              max_ndef_msg_len;                   /* Maximum size of NDEF that can be written on the tag          */
    UINT16              new_ndef_msg_len;                   /* Lenght of new updating NDEF Message                          */
    UINT16              ndef_write_block;
    UINT16              prop_msg_len;                       /* Proprietary tlv length                                       */
    UINT8               *p_new_ndef_buffer;                 /* Pointer to updating NDEF Message                             */
    UINT8               *p_ndef_buffer;                     /* Pointer to NDEF Message                                      */
    tRW_T2T_LOCK_INFO   lock_tlv[RW_T2T_MAX_LOCK_TLVS];     /* Information retrieved from lock control tlv                  */
    tRW_T2T_LOCK        lockbyte[RW_T2T_MAX_LOCK_BYTES];    /* Dynamic Lock byte information                                */
    tRW_T2T_RES_INFO    mem_tlv[RW_T2T_MAX_MEM_TLVS];       /* Information retrieved from mem tlv                           */
#endif
} tRW_T2T_CB;

/* Type 3 Tag control block */
typedef UINT8 tRW_T3T_RW_STATE;

typedef struct
{
    tNFC_STATUS         status;
    UINT8               version;        /* Ver: peer version */
    UINT8               nbr;            /* NBr: number of blocks that can be read using one Check command */
    UINT8               nbw;            /* Nbw: number of blocks that can be written using one Update command */
    UINT16              nmaxb;          /* Nmaxb: maximum number of blocks available for NDEF data */
    UINT8               writef;         /* WriteFlag: 00h if writing data finished; 0Fh if writing data in progress */
    UINT8               rwflag;         /* RWFlag: 00h NDEF is read-only; 01h if read/write available */
    UINT32              ln;             /* Ln: actual size of stored NDEF data (in bytes) */
} tRW_T3T_DETECT;

/* RW_T3T control block flags */
#define RW_T3T_FL_IS_FINAL_NDEF_SEGMENT         0x01    /* The final command for completing the NDEF read/write */
#define RW_T3T_FL_W4_PRESENCE_CHECK_POLL_RSP    0x02    /* Waiting for POLL response for presence check */
#define RW_T3T_FL_W4_GET_SC_POLL_RSP            0x04    /* Waiting for POLL response for RW_T3tGetSystemCodes */
#define RW_T3T_FL_W4_NDEF_DETECT_POLL_RSP       0x08    /* Waiting for POLL response for RW_T3tDetectNDef */
#define RW_T3T_FL_W4_FMT_FELICA_LITE_POLL_RSP   0x10    /* Waiting for POLL response for RW_T3tFormat */
#define RW_T3T_FL_W4_SRO_FELICA_LITE_POLL_RSP   0x20    /* Waiting for POLL response for RW_T3tSetReadOnly */

typedef struct
{
    UINT32              cur_tout;               /* Current command timeout */
    /* check timeout is check_tout_a + n * check_tout_b; X is T/t3t * 4^E */
    UINT32              check_tout_a;           /* Check command timeout (A+1)*X */
    UINT32              check_tout_b;           /* Check command timeout (B+1)*X */
    /* update timeout is update_tout_a + n * update_tout_b; X is T/t3t * 4^E */
    UINT32              update_tout_a;          /* Update command timeout (A+1)*X */
    UINT32              update_tout_b;          /* Update command timeout (B+1)*X */
    tRW_T3T_RW_STATE    rw_state;               /* Reader/writer state */
    UINT8               rw_substate;
    UINT8               cur_cmd;                /* Current command being executed */
    BT_HDR              *p_cur_cmd_buf;         /* Copy of current command, for retransmission */
    TIMER_LIST_ENT      timer;                  /* timeout for waiting for response */
    TIMER_LIST_ENT      poll_timer;             /* timeout for waiting for response */

    tRW_T3T_DETECT      ndef_attrib;            /* T3T NDEF attribute information */

    UINT32              ndef_msg_len;           /* Length of ndef message to send */
    UINT32              ndef_msg_bytes_sent;    /* Length of ndef message sent so far */
    UINT8               *ndef_msg;              /* Buffer for outgoing NDEF message */
    UINT32              ndef_rx_readlen;        /* Number of bytes read in current CHECK command */
    UINT32              ndef_rx_offset;         /* Length of ndef message read so far */

    UINT8               num_system_codes;       /* System codes detected */
    UINT16              system_codes[T3T_MAX_SYSTEM_CODES];

    UINT8               peer_nfcid2[NCI_NFCID2_LEN];
    UINT8               cur_poll_rc;            /* RC used in current POLL command */

    UINT8               flags;                  /* Flags see RW_T3T_FL_* */
} tRW_T3T_CB;


/*
**  Type 4 Tag
*/

/* Max data size using a single ReadBinary. 2 bytes are for status bytes */
#define RW_T4T_MAX_DATA_PER_READ           (NFC_RW_POOL_BUF_SIZE - BT_HDR_SIZE - NCI_DATA_HDR_SIZE - T4T_RSP_STATUS_WORDS_SIZE)

/* Max data size using a single UpdateBinary. 6 bytes are for CLA, INS, P1, P2, Lc */
#define RW_T4T_MAX_DATA_PER_WRITE          (NFC_RW_POOL_BUF_SIZE - BT_HDR_SIZE - NCI_MSG_OFFSET_SIZE - NCI_DATA_HDR_SIZE - T4T_CMD_MAX_HDR_SIZE)



/* Mandatory NDEF file control */
typedef struct
{
    UINT16              file_id;        /* File Identifier          */
    UINT16              max_file_size;  /* Max NDEF file size       */
    UINT8               read_access;    /* read access condition    */
    UINT8               write_access;   /* write access condition   */
} tRW_T4T_NDEF_FC;

/* Capability Container */
typedef struct
{
    UINT16              cclen;      /* the size of this capability container        */
    UINT8               version;    /* the mapping specification version            */
    UINT16              max_le;     /* the max data size by a single ReadBinary     */
    UINT16              max_lc;     /* the max data size by a single UpdateBinary   */
    tRW_T4T_NDEF_FC     ndef_fc;    /* Mandatory NDEF file control                  */
} tRW_T4T_CC;

typedef UINT8 tRW_T4T_RW_STATE;
typedef UINT8 tRW_T4T_RW_SUBSTATE;

/* Type 4 Tag Control Block */
typedef struct
{
    tRW_T4T_RW_STATE    state;              /* main state                       */
    tRW_T4T_RW_SUBSTATE sub_state;          /* sub state                        */
    UINT8               version;            /* currently effective version      */
    TIMER_LIST_ENT      timer;              /* timeout for each API call        */

    UINT16              ndef_length;        /* length of NDEF data              */
    UINT8              *p_update_data;      /* pointer of data to update        */
    UINT16              rw_length;          /* remaining bytes to read/write    */
    UINT16              rw_offset;          /* remaining offset to read/write   */
    BT_HDR             *p_data_to_free;     /* GKI buffet to delete after done  */

    tRW_T4T_CC          cc_file;            /* Capability Container File        */

#define RW_T4T_NDEF_STATUS_NDEF_DETECTED    0x01    /* NDEF has been detected   */
#define RW_T4T_NDEF_STATUS_NDEF_READ_ONLY   0x02    /* NDEF file is read-only   */

    UINT8               ndef_status;        /* bitmap for NDEF status           */
    UINT8               channel;            /* channel id: used for read-binary */

    UINT16              max_read_size;      /* max reading size per a command   */
    UINT16              max_update_size;    /* max updating size per a command  */
#if (NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
    UINT16              card_size;
    UINT8               card_type;
#endif
} tRW_T4T_CB;

/* RW retransmission statistics */
#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
typedef struct
{
    UINT32              start_tick;         /* System tick count at activation */
    UINT32              bytes_sent;         /* Total bytes sent since activation */
    UINT32              bytes_received;     /* Total bytes received since activation */
    UINT32              num_ops;            /* Number of operations since activation */
    UINT32              num_retries;        /* Number of retranmissions since activation */
    UINT32              num_crc;            /* Number of crc failures */
    UINT32              num_trans_err;      /* Number of transmission error notifications */
    UINT32              num_fail;           /* Number of aborts (failures after retries) */
} tRW_STATS;
#endif  /* RW_STATS_INCLUDED */

/* ISO 15693 RW Control Block */
typedef UINT8 tRW_I93_RW_STATE;
typedef UINT8 tRW_I93_RW_SUBSTATE;

#define RW_I93_FLAG_READ_ONLY           0x01    /* tag is read-only                        */
#define RW_I93_FLAG_READ_MULTI_BLOCK    0x02    /* tag supports read multi block           */
#define RW_I93_FLAG_RESET_DSFID         0x04    /* need to reset DSFID for formatting      */
#define RW_I93_FLAG_RESET_AFI           0x08    /* need to reset AFI for formatting        */
#define RW_I93_FLAG_16BIT_NUM_BLOCK     0x10    /* use 2 bytes for number of blocks        */

#define RW_I93_TLV_DETECT_STATE_TYPE      0x01  /* searching for type                      */
#define RW_I93_TLV_DETECT_STATE_LENGTH_1  0x02  /* searching for the first byte of length  */
#define RW_I93_TLV_DETECT_STATE_LENGTH_2  0x03  /* searching for the second byte of length */
#define RW_I93_TLV_DETECT_STATE_LENGTH_3  0x04  /* searching for the third byte of length  */
#define RW_I93_TLV_DETECT_STATE_VALUE     0x05  /* reading value field                     */

enum
{
    RW_I93_ICODE_SLI,                   /* ICODE SLI, SLIX                  */
    RW_I93_ICODE_SLI_S,                 /* ICODE SLI-S, SLIX-S              */
    RW_I93_ICODE_SLI_L,                 /* ICODE SLI-L, SLIX-L              */
    RW_I93_TAG_IT_HF_I_PLUS_INLAY,      /* Tag-it HF-I Plus Inlay           */
    RW_I93_TAG_IT_HF_I_PLUS_CHIP,       /* Tag-it HF-I Plus Chip            */
    RW_I93_TAG_IT_HF_I_STD_CHIP_INLAY,  /* Tag-it HF-I Standard Chip/Inlyas */
    RW_I93_TAG_IT_HF_I_PRO_CHIP_INLAY,  /* Tag-it HF-I Pro Chip/Inlyas      */
    RW_I93_STM_LRI1K,                   /* STM LRI1K                        */
    RW_I93_STM_LRI2K,                   /* STM LRI2K                        */
    RW_I93_STM_LRIS2K,                  /* STM LRIS2K                       */
    RW_I93_STM_LRIS64K,                 /* STM LRIS64K                      */
    RW_I93_STM_M24LR64_R,               /* STM M24LR64-R                    */
    RW_I93_STM_M24LR04E_R,              /* STM M24LR04E-R                   */
    RW_I93_STM_M24LR16E_R,              /* STM M24LR16E-R                   */
    RW_I93_STM_M24LR64E_R,              /* STM M24LR64E-R                   */
    RW_I93_UNKNOWN_PRODUCT              /* Unknwon product version          */
};

typedef struct
{
    tRW_I93_RW_STATE    state;                  /* main state                       */
    tRW_I93_RW_SUBSTATE sub_state;              /* sub state                        */
    TIMER_LIST_ENT      timer;                  /* timeout for each sent command    */
    UINT8               sent_cmd;               /* last sent command                */
    UINT8               retry_count;            /* number of retry                  */
    BT_HDR             *p_retry_cmd;            /* buffer to store cmd sent last    */

    UINT8               info_flags;             /* information flags                */
    UINT8               uid[I93_UID_BYTE_LEN];  /* UID of currently activated       */
    UINT8               dsfid;                  /* DSFID if I93_INFO_FLAG_DSFID     */
    UINT8               afi;                    /* AFI if I93_INFO_FLAG_AFI         */
    UINT8               block_size;             /* block size of tag, in bytes      */
    UINT16              num_block;              /* number of blocks in tag          */
    UINT8               ic_reference;           /* IC Reference of tag              */
    UINT8               product_version;        /* tag product version              */

    UINT8               intl_flags;             /* flags for internal information   */

    UINT8               tlv_detect_state;       /* TLV detecting state              */
    UINT8               tlv_type;               /* currently detected type          */
    UINT16              tlv_length;             /* currently detected length        */

    UINT16              ndef_tlv_start_offset;  /* offset of first byte of NDEF TLV */
    UINT16              ndef_tlv_last_offset;   /* offset of last byte of NDEF TLV  */
    UINT16              max_ndef_length;        /* max NDEF length the tag contains */
    UINT16              ndef_length;            /* length of NDEF data              */

    UINT8              *p_update_data;          /* pointer of data to update        */
    UINT16              rw_length;              /* bytes to read/write              */
    UINT16              rw_offset;              /* offset to read/write             */
} tRW_I93_CB;

/* RW memory control blocks */
typedef union
{
    tRW_T1T_CB          t1t;
    tRW_T2T_CB          t2t;
    tRW_T3T_CB          t3t;
    tRW_T4T_CB          t4t;
    tRW_I93_CB          i93;
} tRW_TCB;

/* RW control blocks */
typedef struct
{
    tRW_TCB             tcb;
    tRW_CBACK           *p_cback;
    UINT32              cur_retry;          /* Retry count for the current operation */
#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
    tRW_STATS           stats;
#endif  /* RW_STATS_INCLUDED */
    UINT8               trace_level;
} tRW_CB;


/*****************************************************************************
**  EXTERNAL FUNCTION DECLARATIONS
*****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/* Global NFC data */
#if NFC_DYNAMIC_MEMORY == FALSE
NFC_API extern tRW_CB  rw_cb;
#else
NFC_API extern tRW_CB *rw_cb_ptr;
#define rw_cb (*rw_cb_ptr)
#endif

/* from .c */

#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))
extern tRW_EVENT rw_t1t_handle_rsp (const tT1T_CMD_RSP_INFO * p_info, BOOLEAN *p_notify, UINT8 *p_data, tNFC_STATUS *p_status);
extern tRW_EVENT rw_t1t_info_to_event (const tT1T_CMD_RSP_INFO * p_info);
#else
#define rw_t1t_handle_rsp(p, a, b, c)       t1t_info_to_evt (p)
#define rw_t1t_info_to_event(p)             t1t_info_to_evt (p)
#endif

extern void rw_init (void);
extern tNFC_STATUS rw_t1t_select (UINT8 hr[T1T_HR_LEN], UINT8 uid[T1T_CMD_UID_LEN]);
extern tNFC_STATUS rw_t1t_send_dyn_cmd (UINT8 opcode, UINT8 add, UINT8 *p_dat);
extern tNFC_STATUS rw_t1t_send_static_cmd (UINT8 opcode, UINT8 add, UINT8 dat);
extern void rw_t1t_process_timeout (TIMER_LIST_ENT *p_tle);
extern void rw_t1t_handle_op_complete (void);

#if (defined (RW_NDEF_INCLUDED) && (RW_NDEF_INCLUDED == TRUE))
extern tRW_EVENT rw_t2t_info_to_event (const tT2T_CMD_RSP_INFO *p_info);
extern void rw_t2t_handle_rsp (UINT8 *p_data);
#else
#define rw_t2t_info_to_event(p)             t2t_info_to_evt (p)
#define rw_t2t_handle_rsp(p)
#endif

extern tNFC_STATUS rw_t2t_sector_change (UINT8 sector);
extern tNFC_STATUS rw_t2t_read (UINT16 block);
extern tNFC_STATUS rw_t2t_write (UINT16 block, UINT8 *p_write_data);
extern void rw_t2t_process_timeout (TIMER_LIST_ENT *p_tle);
extern tNFC_STATUS rw_t2t_select (void);
void rw_t2t_handle_op_complete (void);

extern void rw_t3t_process_timeout (TIMER_LIST_ENT *p_tle);
extern tNFC_STATUS rw_t3t_select (UINT8 peer_nfcid2[NCI_RF_F_UID_LEN], UINT8 mrti_check, UINT8 mrti_update);
void rw_t3t_handle_nci_poll_ntf (UINT8 nci_status, UINT8 num_responses, UINT8 sensf_res_buf_size, UINT8 *p_sensf_res_buf);

extern tNFC_STATUS rw_t4t_select (void);
extern void rw_t4t_process_timeout (TIMER_LIST_ENT *p_tle);

extern tNFC_STATUS rw_i93_select (UINT8 *p_uid);
extern void rw_i93_process_timeout (TIMER_LIST_ENT *p_tle);

#if (defined (RW_STATS_INCLUDED) && (RW_STATS_INCLUDED == TRUE))
/* Internal fcns for statistics (from rw_main.c) */
void rw_main_reset_stats (void);
void rw_main_update_tx_stats (UINT32 bytes_tx, BOOLEAN is_retry);
void rw_main_update_rx_stats (UINT32 bytes_rx);
void rw_main_update_crc_error_stats (void);
void rw_main_update_trans_error_stats (void);
void rw_main_update_fail_stats (void);
void rw_main_log_stats (void);
#endif  /* RW_STATS_INCLUDED */

#ifdef __cplusplus
}
#endif

#endif /* RW_INT_H_ */
