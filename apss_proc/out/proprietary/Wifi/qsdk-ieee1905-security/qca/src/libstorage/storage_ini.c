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

#define PATH_INI "/var/storage.ini"

struct VPairs{
    char *name;
    char *value;
    struct VPairs *next;
};

struct storage_s{
    struct VPairs *head;
    /*Other Storage Context*/
};

static struct storage_s *g_storage = NULL;

void *storage_getHandle()
{
    if (g_storage)
        return g_storage;

    g_storage = malloc(sizeof (struct storage_s));
    if (g_storage)
        memset(g_storage, 0, sizeof (struct storage_s));
    return (void *)g_storage;
}


int  storage_setParam(void *handle, const char *name, const char *value)
{
    struct VPairs *vps, *tail;
    struct storage_s *strg = handle;
    char *pstr;

    if (strg == NULL)
        return -1;

    tail = vps = strg->head;
    while (vps)
    {
        if (strcmp(vps->name, name) == 0)
           break;

        tail = vps;
        vps = vps->next;

    }

    if (vps)
    {
        pstr = strdup(value);
        if (!pstr)
            return -1;

        free(vps->value);
        vps->value = pstr;
    }
    else
    {
        vps = malloc(sizeof (struct VPairs));
        if (!vps)
            return -1;
        memset(vps, 0, sizeof (struct VPairs));
        
        vps->name = strdup(name);
        if (!vps->name)
        {
            free (vps);
            return -1;
        }
        
        vps->value = strdup(value);
        if (!vps->value)
        {
            free (vps->name);
            free (vps);
            return -1;
        }
     
        if (tail)
            tail->next = vps;
        else
            strg->head = vps;
        
    }
    
    return 0;
}


int  storage_apply(void *handle)
{
    struct storage_s *strg = handle;
    struct VPairs *vps;
    FILE *f;
    int ret = 0;

    if (strg == NULL)
        return -1;

    f = fopen(PATH_INI,"w");

    if (f == NULL)
    {
        ret = -1;
        goto out;
    }
    fprintf(f, "#Configuration begin\n");

    vps = strg->head;
    while(vps)
    {
        fprintf(f, "%s=%s\n", vps->name, vps->value);
        vps = vps->next;
    }

    fprintf(f, "#Configuration end\n");
    fclose(f);

    /* Call script/application to apply changes*/

out:
    return ret;
}

int storage_applyWithCallback(void *handle, storageCallback_f callback, void *cookie )
{
    int ret;

    if( !handle || !callback )
        return -1;

    ret = storage_apply( handle );

    callback( handle, cookie, ret );

    return 0;
}

int storage_callbackDone( void *handle )
{
    if(!handle)
        return -1;

    return 0;
}


/* Add a VAP with default configuration, this function is only called by HyFi 1.0
 * AP cloning when Server and Client have different number of VAPs.
 */
int  storage_addVAP()
{
    /* Call script/application to create a VAP. Return a valid index which could 
    be used to setParam */
    return 0;         
}


/* Delete a VAP , this function is only called by HyFi 1.0 AP cloning when Server 
 * and Client have different number of VAPs.
 */
int  storage_delVAP(int index)
{
    /* Call script/application to delete a VAP*/
    return 0;
}

/* storage_AddDelVap is added for uci.. so added the dummy function to avoid linker
 * error for ini format.
 */
int storage_AddDelVap(void *handle, const char *name, const char *value)
{
    return 0;
}

void storage_restartWireless(void) 
{
}
