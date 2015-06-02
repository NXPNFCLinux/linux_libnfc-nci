/******************************************************************************
 *
 *  Copyright (C) 2010-2014 Broadcom Corporation
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
 *  Callout functions for memory allocation/deallocatoin
 *
 ******************************************************************************/
#ifndef NFA_MEM_CO_H
#define NFA_MEM_CO_H

#include "nfc_target.h"

/*****************************************************************************
**  Constants and data types
*****************************************************************************/


/*****************************************************************************
**  External Function Declarations
*****************************************************************************/
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
**
** Function         nfa_mem_co_alloc
**
** Description      allocate a buffer from platform's memory pool
**
** Returns:
**                  pointer to buffer if successful
**                  NULL otherwise
**
*******************************************************************************/
NFC_API extern void *nfa_mem_co_alloc (UINT32 num_bytes);


/*******************************************************************************
**
** Function         nfa_mem_co_free
**
** Description      free buffer previously allocated using nfa_mem_co_alloc
**
** Returns:
**                  Nothing
**
*******************************************************************************/
NFC_API extern void nfa_mem_co_free (void *p_buf);


#ifdef __cplusplus
}
#endif

#endif /* NFA_MEM_CO_H */
