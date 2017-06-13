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
 * DAL I2C port implementation for linux
 *
 * Project: Trusted NFC Linux
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>

#include <phNxpLog.h>
#include <phTmlNfc_lpcusbsio.h>
#include <phNfcStatus.h>
#include <string.h>
#include "phNxpNciHal_utils.h"

#include <lpcusbsio.h>
#include <lpcusbsio_i2c.h>

#define CRC_LEN                     2
#define NORMAL_MODE_HEADER_LEN      3
#define FW_DNLD_HEADER_LEN          2
#define FW_DNLD_LEN_OFFSET          1
#define NORMAL_MODE_LEN_OFFSET      2
#define FRAGMENTSIZE_MAX 48
static bool_t bFwDnldFlag = FALSE;

static LPC_HANDLE g_hI2CPort = NULL;

/*******************************************************************************
**
** Function         phTmlNfc_i2c_close
**
** Description      Closes PN54X device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phTmlNfc_i2c_close(void *pDevHandle)
{
    I2C_Close(0);
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_open_and_configure
**
** Description      Open and configure PN54X device
**
** Parameters       pConfig     - hardware information
**                  pLinkHandle - device handle
**
** Returns          NFC status:
**                  NFCSTATUS_SUCCESS            - open_and_configure operation success
**                  NFCSTATUS_INVALID_DEVICE     - device open operation failure
**
*******************************************************************************/
NFCSTATUS phTmlNfc_i2c_open_and_configure(pphTmlNfc_Config_t pConfig, void ** pLinkHandle)
{
	int res;
	I2C_PORTCONFIG_T cfgParam;

    /* Retrieve on which port is the device connected */
	res = I2C_GetNumPorts();

	if (res > 0) {
		NXPLOG_TML_D("Total I2C devices: %d", res);

		/* open device at index 0 */
		g_hI2CPort = I2C_Open(0);

		NXPLOG_TML_D("Handle: 0x%x", g_hI2CPort);

		cfgParam.ClockRate = I2C_CLOCK_FAST_MODE;
		cfgParam.Options = 0;

		/* Init the I2C port for standard speed communication */
		res = I2C_Init(g_hI2CPort, &cfgParam);

		if (res < 0) {
			NXPLOG_TML_D("Unable to init I2C port error:%d - %ls", res, I2C_Error(g_hI2CPort, res));
            return NFCSTATUS_INVALID_DEVICE;
		}

        /* Reset PN54X */

    }
	else {
		NXPLOG_TML_D("Error: No free ports. \r\n");
            return NFCSTATUS_INVALID_DEVICE;
	}

    *pLinkHandle = (void*) ((intptr_t)g_hI2CPort);

    return NFCSTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_read
**
** Description      Reads requested number of bytes from PN54X device into given buffer
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToRead   - number of bytes requested to be read
**
** Returns          numRead   - number of successfully read bytes
**                  -1        - read operation failure
**
*******************************************************************************/
int phTmlNfc_i2c_read(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToRead)
{
    int ret;

	    ret = I2C_DeviceRead(pDevHandle,
             			      0x28,
							  pBuffer,
							  nNbBytesToRead,
                              I2C_TRANSFER_OPTIONS_START_BIT | I2C_TRANSFER_OPTIONS_STOP_BIT |
                              I2C_TRANSFER_OPTIONS_NACK_LAST_BYTE);
        NXPLOG_TML_D("I2C_DeviceRead len = %d returned %xh", nNbBytesToRead, ret);

        if(ret<0) ret = -1;
    return ret;
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_write
**
** Description      Writes requested number of bytes from given buffer into PN54X device
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToWrite  - number of bytes requested to be written
**
** Returns          numWrote   - number of successfully written bytes
**                  -1         - write operation failure
**
*******************************************************************************/
int phTmlNfc_i2c_write(void *pDevHandle, uint8_t * pBuffer, int nNbBytesToWrite)
{
    int ret;
    int numWrote = 0;
    int i;
    int numBytes = nNbBytesToWrite;
    if (NULL == pDevHandle)
    {
        return -1;
    }

    while (numWrote < nNbBytesToWrite)
    {
        if(nNbBytesToWrite > FRAGMENTSIZE_MAX)
        {
            if(nNbBytesToWrite - numWrote > FRAGMENTSIZE_MAX)
            {
                numBytes = numWrote+ FRAGMENTSIZE_MAX;
            }
            else
            {
                numBytes = nNbBytesToWrite;
            }
        }
        usleep(500);
        ret = I2C_DeviceWrite(g_hI2CPort,
							  0x28,
							  pBuffer + numWrote,
							  numBytes - numWrote,
							  I2C_TRANSFER_OPTIONS_START_BIT | I2C_TRANSFER_OPTIONS_STOP_BIT);
        if (ret > 0)
        {
            numWrote += ret;
        }
        else if (ret == 0)
        {
            NXPLOG_TML_E("_i2c_write() EOF");
            return -1;
        }
        else
        {
            NXPLOG_TML_E("_i2c_write() errno : %x",errno);
            if (errno == EINTR || errno == EAGAIN)
            {
                continue;
            }
            return -1;
        }
    }

    return numWrote;
}

/*******************************************************************************
**
** Function         phTmlNfc_i2c_reset
**
** Description      Reset PN54X device, using VEN pin
**
** Parameters       pDevHandle     - valid device handle
**                  level          - reset level
**
** Returns           0   - reset operation success
**                  -1   - reset operation failure
**
*******************************************************************************/
int phTmlNfc_i2c_reset(void *pDevHandle, long level)
{
    int ret = 0;

    bFwDnldFlag = FALSE;

    switch (level)
    {
    case 0:
        if(GPIO_SetVENValue(pDevHandle, 0) != LPCUSBSIO_OK) return -1;
        break;

    case 1:
        if(GPIO_SetDWLValue(pDevHandle, 0) != LPCUSBSIO_OK) return -1;
        if(GPIO_SetVENValue(pDevHandle, 0) != LPCUSBSIO_OK) return -1;
        usleep(10000);
        if(GPIO_SetVENValue(pDevHandle, 1) != LPCUSBSIO_OK) return -1;
        break;

    case 2:
        if(GPIO_SetDWLValue(pDevHandle, 1) != LPCUSBSIO_OK) return -1;
        if(GPIO_SetVENValue(pDevHandle, 0) != LPCUSBSIO_OK) return -1;
        usleep(10000);
        if(GPIO_SetVENValue(pDevHandle, 1) != LPCUSBSIO_OK) return -1;
        bFwDnldFlag = TRUE;
        break;

    default: 
        ret = -1; 
        break;
    }

    return 0;
}

/*******************************************************************************
**
** Function         getDownloadFlag
**
** Description      Returns the current mode
**
** Parameters       none
**
** Returns           Current mode download/NCI
*******************************************************************************/
bool_t getDownloadFlag(void)
{
    return bFwDnldFlag;
}

