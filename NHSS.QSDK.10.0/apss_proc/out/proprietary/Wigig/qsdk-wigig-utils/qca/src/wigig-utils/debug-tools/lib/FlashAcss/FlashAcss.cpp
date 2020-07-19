/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "flashacss.h"
#include "flash_gateway.h"
#include "SparrowGateway.h"
#include "MarlonGateway.h"

FLSHACSS_API int wfa_create_flash_access_handler(DType devType, void* pDeviceAccess, void** pFlashAccess)
{
    flash_gateway* p_flash_gateway = NULL;
    if (devType == MST_TALYN)
    {
        p_flash_gateway = new SparrowGateway(pDeviceAccess);
    }
    if (devType == MST_SPARROW)
    {
        p_flash_gateway = new SparrowGateway(pDeviceAccess);
    }
    else if (devType == MST_MARLON)
    {
        p_flash_gateway = new MarlonGateway(pDeviceAccess);
    }
    else
    {
        return WLCT_OS_ERROR_NOT_SUPPORTED;
    }
    *pFlashAccess = (void*)p_flash_gateway;
    return 0;
}

FLSHACSS_API int wfa_close_flash_access_handler(void* pFlashAccess)
{
    int res = 0;
    flash_gateway* p_flash_gateway = (flash_gateway*)pFlashAccess;
    if (p_flash_gateway != NULL)
    {
        __TRY
        {
            delete p_flash_gateway;
        }
        __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
        {
            res = -1;
        }
    }
    return res;
}

FLSHACSS_API int wfa_run_command(void* pFlashAccss, UCHAR flash_cmd, FLASH_PHASE phase, bool bWrite, unsigned long addr, unsigned long ulSize)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->runCommand(flash_cmd, phase, bWrite, addr, ulSize);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}

FLSHACSS_API int wfa_set_data(void* pFlashAccss, const BYTE *buffer, unsigned long size)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->setData(buffer, size);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}

FLSHACSS_API int wfa_get_data(void* pFlashAccss, BYTE *buffer, unsigned long size)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->getData(buffer, size);
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}

FLSHACSS_API int wfa_wait_done(void* pFlashAccss)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->wait_done();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}

FLSHACSS_API int wfa_boot_done(void* pFlashAccss)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->boot_done();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}

FLSHACSS_API bool wfa_get_lock (void* pFlashAccss)
{
    bool ret = false;

    if (pFlashAccss == NULL)
        return ret;

    __TRY
    {
        ret = ((flash_gateway*)pFlashAccss)->get_lock();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return ret;
}

FLSHACSS_API int wfa_force_lock (void* pFlashAccss)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->force_lock();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}

FLSHACSS_API int wfa_release_lock(void* pFlashAccss)
{
    int rc = 0;
    if (pFlashAccss == NULL)
        return -1;

    __TRY
    {
        rc = ((flash_gateway*)pFlashAccss)->release_lock();
    }
    __EXCEPT(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode())
    {
    }
    return rc;
}
