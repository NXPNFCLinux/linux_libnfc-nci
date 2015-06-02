/******************************************************************************
 *
 *  Copyright (C) 2002-2012 Broadcom Corporation
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
 *  This file contains NV definitions from WIDCOMM's Universal Embedded
 *  Drivers API.
 *
 ******************************************************************************/

#ifndef UNV_H
#define UNV_H

#include "data_types.h"

/*******************************************************************************
** NV APIs
*******************************************************************************/

/**** Storage preferences ****/
#define UNV_BLOCK         1
#define UNV_BYTE          2
#define UNV_NOPREF        3

typedef UINT8 tUNV_STORAGE_PREF;

/**** Status ****/
#define UNV_REINIT      (-1)
#define UNV_WRITELOCKED (-2)
#define UNV_ERROR       (-3)

typedef INT16 tUNV_STATUS;

/* Prototype for function to restore defaults to a block */
typedef void  (tUNV_DEFAULT_FUNC)(void);

/*******************************************************************************
** Function Prototypes
*******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UDRV_API
#define UDRV_API
#endif

UDRV_API extern void        UNV_Init(void *);
UDRV_API extern BOOLEAN     UNV_MapBlock(UINT16, tUNV_STORAGE_PREF, UINT16,
                                         UINT16, UINT16 *, void *);
UDRV_API extern BOOLEAN     UNV_ReadMap(UINT16, tUNV_STORAGE_PREF *, UINT16 *,
                                        UINT16 *, UINT16 *);
UDRV_API extern BOOLEAN     UNV_EraseBlock(UINT16);
UDRV_API extern void        UNV_Default(UINT16);
UDRV_API extern tUNV_STATUS UNV_Read(UINT16, UINT16, UINT16, UINT16, void *);
UDRV_API extern tUNV_STATUS UNV_Write(UINT16, UINT16, UINT16, UINT16, void *);
UDRV_API extern tUNV_STATUS UNV_ReadBlock(UINT16, UINT16, void *);
UDRV_API extern tUNV_STATUS UNV_WriteBlock(UINT16, void *);
UDRV_API extern UINT32      UNV_BytesRemaining(void);
UDRV_API extern void        UNV_Consolidate(void);
UDRV_API extern tUNV_STATUS UNV_ReadPtr(UINT16, UINT16, UINT8 **);
UDRV_API extern tUNV_STATUS UNV_FreePtr(UINT16, UINT16);

#ifdef __cplusplus
}
#endif

#endif /* UNV_H */
