/******************************************************************************
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
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
 *  Copyright (C) 2015 NXP Semiconductors
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

#include "OverrideLog.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>
#include "buildcfg.h"
#include "nfa_nv_co.h"
#include "nci_config.h"
#include "nfc_hal_target.h"
#include "nfc_hal_nv_co.h"
#include "nfa_nv_ci.h"
#include "CrcChecksum.h"
extern char nfc_nci_store[];
static const char* sNfaStorageBin = "/nfaStorage.bin";

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
extern void *nfa_mem_co_alloc(UINT32 num_bytes)
{
    return malloc(num_bytes);
}


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
extern void nfa_mem_co_free(void *pBuffer)
{
    free(pBuffer);
}


/*******************************************************************************
**
** Function         nfa_nv_co_read
**
** Description      This function is called by NFA to read in data from the
**                  previously opened file.
**
** Parameters       pBuffer   - buffer to read the data into.
**                  nbytes  - number of bytes to read into the buffer.
**
** Returns          void
**
**                  Note: Upon completion of the request, nfa_nv_ci_read() is
**                        called with the buffer of data, along with the number
**                        of bytes read into the buffer, and a status.  The
**                        call-in function should only be called when ALL requested
**                        bytes have been read, the end of file has been detected,
**                        or an error has occurred.
**
*******************************************************************************/
extern void nfa_nv_co_read(UINT8 *pBuffer, UINT16 nbytes, UINT8 block)
{
    char filename[256], filename2[256];

    memset (filename, 0, sizeof(filename));
    memset (filename2, 0, sizeof(filename2));
    strcpy(filename2, nfc_nci_store);
    strncat(filename2, sNfaStorageBin, sizeof(filename2)-strlen(filename2)-1);
    if (strlen(filename2) > 200)
    {
        //ALOGE ("%s: filename too long", __FUNCTION__);
        return;
    }
    sprintf (filename, "%s%u", filename2, block);

    //ALOGD ("%s: buffer len=%u; file=%s", __FUNCTION__, nbytes, filename);
    int fileStream = open (filename, O_RDONLY);
    if (fileStream >= 0)
    {
        unsigned short checksum = 0;
        size_t actualReadCrc = read (fileStream, &checksum, sizeof(checksum));
        size_t actualReadData = read (fileStream, pBuffer, nbytes);
        close (fileStream);
        if (actualReadData > 0)
        {
            //ALOGD ("%s: data size=%zu", __FUNCTION__, actualReadData);
            nfa_nv_ci_read (actualReadData, NFA_NV_CO_OK, block);
        }
        else
        {
            //ALOGE ("%s: fail to read", __FUNCTION__);
            nfa_nv_ci_read (0, NFA_NV_CO_FAIL, block);
        }
    }
    else
    {
        //ALOGD ("%s: fail to open", __FUNCTION__);
        nfa_nv_ci_read (0, NFA_NV_CO_FAIL, block);
    }
}

/*******************************************************************************
**
** Function         nfa_nv_co_write
**
** Description      This function is called by io to send file data to the
**                  phone.
**
** Parameters       pBuffer   - buffer to read the data from.
**                  nbytes  - number of bytes to write out to the file.
**
** Returns          void
**
**                  Note: Upon completion of the request, nfa_nv_ci_write() is
**                        called with the file descriptor and the status.  The
**                        call-in function should only be called when ALL requested
**                        bytes have been written, or an error has been detected,
**
*******************************************************************************/
extern void nfa_nv_co_write(const UINT8 *pBuffer, UINT16 nbytes, UINT8 block)
{
    char filename[256], filename2[256];

    memset (filename, 0, sizeof(filename));
    memset (filename2, 0, sizeof(filename2));
    strcpy(filename2, nfc_nci_store);
    strncat(filename2, sNfaStorageBin, sizeof(filename2)-strlen(filename2)-1);
    if (strlen(filename2) > 200)
    {
        //ALOGE ("%s: filename too long", __FUNCTION__);
        return;
    }
    sprintf (filename, "%s%u", filename2, block);
    //ALOGD ("%s: bytes=%u; file=%s", __FUNCTION__, nbytes, filename);

    int fileStream = 0;

    fileStream = open (filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fileStream >= 0)
    {
        unsigned short checksum = crcChecksumCompute (pBuffer, nbytes);
        size_t actualWrittenCrc = write (fileStream, &checksum, sizeof(checksum));
        size_t actualWrittenData = write (fileStream, pBuffer, nbytes);
        //ALOGD ("%s: %zu bytes written", __FUNCTION__, actualWrittenData);
        if ((actualWrittenData == nbytes) && (actualWrittenCrc == sizeof(checksum)))
        {
            nfa_nv_ci_write (NFA_NV_CO_OK);
        }
        else
        {
            //ALOGE ("%s: fail to write", __FUNCTION__);
            nfa_nv_ci_write (NFA_NV_CO_FAIL);
        }
        close (fileStream);
    }
    else
    {
        //ALOGE ("%s: fail to open, error = %d", __FUNCTION__, errno);
        nfa_nv_ci_write (NFA_NV_CO_FAIL);
    }
}

/*******************************************************************************
**
** Function         delete_stack_non_volatile_store
**
** Description      Delete all the content of the stack's storage location.
**
** Parameters       forceDelete: unconditionally delete the storage.
**
** Returns          none
**
*******************************************************************************/
void delete_stack_non_volatile_store (BOOLEAN forceDelete)
{
    static BOOLEAN firstTime = TRUE;
    char filename[256], filename2[256];

    if ((firstTime == FALSE) && (forceDelete == FALSE))
        return;
    firstTime = FALSE;

    //ALOGD ("%s", __FUNCTION__);

    memset (filename, 0, sizeof(filename));
    memset (filename2, 0, sizeof(filename2));
    strcpy(filename2, nfc_nci_store);
    strncat(filename2, sNfaStorageBin, sizeof(filename2)-strlen(filename2)-1);
    if (strlen(filename2) > 200)
    {
        //ALOGE ("%s: filename too long", __FUNCTION__);
        return;
    }
    sprintf (filename, "%s%u", filename2, DH_NV_BLOCK);
    remove (filename);
    sprintf (filename, "%s%u", filename2, HC_F3_NV_BLOCK);
    remove (filename);
    sprintf (filename, "%s%u", filename2, HC_F4_NV_BLOCK);
    remove (filename);
    sprintf (filename, "%s%u", filename2, HC_F2_NV_BLOCK);
    remove (filename);
    sprintf (filename, "%s%u", filename2, HC_F5_NV_BLOCK);
    remove (filename);
}

/*******************************************************************************
**
** Function         verify_stack_non_volatile_store
**
** Description      Verify the content of all non-volatile store.
**
** Parameters       none
**
** Returns          none
**
*******************************************************************************/
void verify_stack_non_volatile_store ()
{
    //ALOGD ("%s", __FUNCTION__);
    char filename[256], filename2[256];
    BOOLEAN isValid = FALSE;

    memset (filename, 0, sizeof(filename));
    memset (filename2, 0, sizeof(filename2));
    strcpy(filename2, nfc_nci_store);
    strncat(filename2, sNfaStorageBin, sizeof(filename2)-strlen(filename2)-1);
    if (strlen(filename2) > 200)
    {
        //ALOGE ("%s: filename too long", __FUNCTION__);
        return;
    }

    sprintf (filename, "%s%u", filename2, DH_NV_BLOCK);
    if (crcChecksumVerifyIntegrity (filename))
    {
        sprintf (filename, "%s%u", filename2, HC_F3_NV_BLOCK);
        if (crcChecksumVerifyIntegrity (filename))
        {
            sprintf (filename, "%s%u", filename2, HC_F4_NV_BLOCK);
            if (crcChecksumVerifyIntegrity (filename))
            {
                sprintf (filename, "%s%u", filename2, HC_F2_NV_BLOCK);
                if (crcChecksumVerifyIntegrity (filename))
                {
                    sprintf (filename, "%s%u", filename2, HC_F5_NV_BLOCK);
                    if (crcChecksumVerifyIntegrity (filename))
                        isValid = TRUE;
                }
            }
        }
    }

    if (isValid == FALSE)
        delete_stack_non_volatile_store (TRUE);
}

