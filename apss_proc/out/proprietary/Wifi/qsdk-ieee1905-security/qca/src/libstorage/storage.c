/*
 * Copyright (c) 2012, 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2012 Qualcomm Atheros Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "storage.h"

#define MIB_PATH_RADIO  "InternetGatewayDevice.X_ATH-COM_RadioConfiguration."
#define MIB_PATH_WLAN   "InternetGatewayDevice.LANDevice.1.WLANConfiguration."
#define MIB_PATH_PLC    "InternetGatewayDevice.LANDevice.1.PowerLineConfigManagement."
#define MIB_PATH_WSPLC  "InternetGatewayDevice.X_ATH-COM_WSPLC."
#define MIB_PATH_HYFI   "InternetGatewayDevice.X_ATH-COM_HY."

typedef void (*mibDoneCB_f)(struct mibifTransaction *XX, void *Cookie, enum mibErr Fail);

void *storage_getHandle()
{
    struct mibifTransaction *X = NULL;
    X = mibifTransactionCreate();
    return (void *)X;
}


int  storage_setParam(void *handle, const char *name, const char *value)
{
    struct mibifTransaction *X = (struct mibifTransaction *)handle;
    char path[256];
    u_int32_t len;

    if(NULL == X)
    {
        return -1;
    }

    if (len = strlen(CONFIG_RADIO),
        strncmp(name, CONFIG_RADIO, len) == 0)
    {
        snprintf(path, 256, MIB_PATH_RADIO"%s", name + len);
    }
    else if (len = strlen(CONFIG_WLAN),
        strncmp(name, CONFIG_WLAN, len) == 0)
    {
        snprintf(path, 256, MIB_PATH_WLAN"%s", name + len);
    }
    else if (len = strlen(CONFIG_PLC),
        strncmp(name, CONFIG_PLC, len) == 0)
    {
        snprintf(path, 256, MIB_PATH_PLC"%s", name + len);
    }
    else if (len = strlen(CONFIG_WSPLC),
        strncmp(name, CONFIG_WSPLC, len) == 0)
    {
        snprintf(path, 256, MIB_PATH_WSPLC"%s", name + len);
    }
    else if (len = strlen(CONFIG_HYFI),
        strncmp(name, CONFIG_HYFI, len) == 0)
    {
        snprintf(path, 256, MIB_PATH_HYFI"%s", name + len);
    }
    else
    {
        return -1;
    }

    mibifSetOperation(X, NULL, path, value);
    return 0;
}


int  storage_apply(void *handle)
{
    struct mibifTransaction *X = (struct mibifTransaction *)handle;
    int ret = 0;
    ret = mibifTransactionProcess(X);
    mibifTransactionDestroy(X);  
    return ret;
}


int storage_applyWithCallback(void *handle, storageCallback_f callback, void *cookie )
{
    struct mibifTransaction *X = (struct mibifTransaction *)handle;

    if(!X || !callback)
        return -1;

    mibifTransactionGo( X, (mibDoneCB_f)callback, cookie );
    return 0;
}

int storage_callbackDone( void *handle )
{
    struct mibifTransaction *X = (struct mibifTransaction *)handle;

    if(!X)
        return -1;

    mibifTransactionDestroy(X);
    return 0;
}

int  storage_addVAP()
{
    struct mibifTransaction * X = NULL;
    struct mibifOperation * O = NULL;

    int objIdx = -1;
    char ** ret = NULL;
    char * tmpval = NULL;

    X = mibifTransactionCreate();
    if(NULL == X)
    {
        return -1;
    }

    O = mibifAddObjectOperation(X, NULL, MIB_PATH_WLAN);
    if(NULL == O)
    {
        mibifTransactionDestroy(X);
        return -1;
    }
    
    if(mibifTransactionProcess(X) != 0)
    {
        mibifTransactionDestroy(X);        
        return -1;
    }

    ret = mibifTransactionNameResultsGet(X);

    if(ret != NULL)
    {
        char buf[128];
        strlcpy(buf,ret[0],sizeof(buf));
        buf[strlen(buf)-1] = '\0';
        tmpval = strrchr(buf,'.');
        if(tmpval)
        {
            tmpval++;
            objIdx = atoi(tmpval);
        }
    }

    mibifTransactionDestroy(X);
    return objIdx;         
}


int  storage_delVAP(int index)
{

    struct mibifTransaction *X = NULL;
    int ret = 0;
    char path[256];

    X = mibifTransactionCreate();
    if(NULL == X)
    {
       return -1;
    }

    snprintf(path, 256, MIB_PATH_WLAN"%d", index);
    mibifDeleteObjectOperation(X, NULL, path);
  
    ret = mibifTransactionProcess(X);
    mibifTransactionDestroy(X);  

    return ret;

}

void storage_restartWireless(void)
{
   system("/sbin/wifi")
}
