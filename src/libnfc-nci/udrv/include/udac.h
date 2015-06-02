/******************************************************************************
 *
 *  Copyright (C) 2001-2012 Broadcom Corporation
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
 *  Definitions for UDAC driver
 *
 ******************************************************************************/
#ifndef UDAC_H
#define UDAC_H


#define UDAC_GAIN_MAX     0x00FFF
typedef UINT16 tUDAC_GAIN;

/* API functions for DAC driver */


/*****************************************************************************
**
** Function         DAC_Init
**
** Description
**      Initialize the DAC subsystem
**
** Input parameters
**      Nothing
**
** Output parameters
**      Nothing
**
** Returns
**      Nothing
**
*****************************************************************************/
void UDAC_Init(void *p_cfg);


/*****************************************************************************
**
** Function         DAC_Read
**
** Description
**      Read current DAC gain
**
** Input parameters
**      Nothing
**
** Output parameters
**      Nothing
**
** Returns
**      Current gain setting
**
*****************************************************************************/
tUDAC_GAIN UDAC_Read(void);


/*****************************************************************************
**
** Function         DAC_Set
**
** Description
**      Set the DAC gain
**
** Input parameters
**      gain        Gain setting
**
** Output parameters
**      Nothing
**
** Returns
**      Nothing
**
*****************************************************************************/
void UDAC_Set(tUDAC_GAIN gain);

#endif /* #ifndef UDAC_H */
