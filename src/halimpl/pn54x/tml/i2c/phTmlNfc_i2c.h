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
 * TML I2C port implementation for linux
 */

/* Basic type definitions */
#include <phNfcTypes.h>
#include <phTmlNfc.h>

/* Function declarations */
void phTmlNfc_i2c_close(void *pDevHandle);
NFCSTATUS phTmlNfc_i2c_open_and_configure(pphTmlNfc_Config_t pConfig, void ** pLinkHandle);
int phTmlNfc_i2c_read(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead);
int phTmlNfc_i2c_write(void *pDevHandle,uint8_t * pBuffer, int nNbBytesToWrite);
int phTmlNfc_i2c_reset(void *pDevHandle,long level);
bool_t getDownloadFlag(void);
phTmlNfc_i2cfragmentation_t fragmentation_enabled;
