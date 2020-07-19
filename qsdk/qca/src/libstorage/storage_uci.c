/*
 *  Copyright (c) 2012 Qualcomm Atheros Inc.
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
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "storage.h"
#include "uci/package.h"
#include "wlanif_cmn.h"

extern struct wlanif_config *wlanIfLS;

struct VPairs{
    char *name;
    char *value;
    struct VPairs *next;
};

struct storage_s{
    struct VPairs *head;
};

void *storage_getHandle()
{
    int ret;
    struct storage_s *strg = NULL;

    strg = malloc(sizeof (struct storage_s));
    if (!strg)
        return NULL;
    memset(strg, 0, sizeof (struct storage_s));
    if (wlanIfLS == NULL) {
        ret = wlanif_init();
        if (ret) {
            printf("libstorage: %s wlanif_init failed \n",__func__);
            return NULL;
        }
    }

    return (void *)strg;
}

static int _storage_destroy(void *handle)
{
    struct storage_s *strg = handle;
    struct VPairs *vps, *tvps;

    if (strg == NULL)
        return -1;

    vps = strg->head;
    while(vps)
    {
        tvps = vps;
        vps = vps->next;

        if (tvps->name)
            free (tvps->name);

        if (tvps->value)
            free (tvps->value);
    }

    strg->head = NULL;
    free (strg);
    wlanif_deinit();
    return 0;
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

static int  _storage_apply(void *handle)
{
    struct storage_s *strg = handle;
    struct VPairs *vps;
    int ret = 0;
    struct pkgmgr_s *pkgm;

    pkgm = pkgGetPkgmgr();
    if (!pkgm)
    {
        return -1;
    }

    ret = pkgInit(pkgm);
    if (ret)
        goto fail;

    vps = strg->head;
    while(vps)
    {
        ret = pkgSet(pkgm, vps->name, vps->value);
        if (ret)
        {
            goto fail;
        }
        vps = vps->next;
    }

    ret = pkgApply(pkgm);

fail:
    pkgDestroy(pkgm);

    return ret;
}


int  storage_apply(void *handle)
{
    int ret;

    ret = _storage_apply(handle);

    _storage_destroy(handle);

    return ret;

}

int storage_applyWithCallback(void *handle, storageCallback_f callback, void *cookie )
{
    int ret;

    if( !handle || !callback )
        return -1;

    ret = _storage_apply( handle );

    callback( handle, cookie, ret );

    return 0;
}

int storage_callbackDone( void *handle )
{
    if(!handle)
        return -1;

    _storage_destroy(handle);

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


/**
 * @brief Get sem set id if exists else create a new one.
 *
 * @return STRG_TRUE on successful get; otherwise STRG_FALSE
 */
strgBool_e storage_wifiLockInit() {
    key_t key;
    union semun{
        int val;
    } semarg;

    if (wifi_semid)
        return STRG_TRUE;

    key = ftok( "/proc/version", 76);
    if (key == -1)
        return STRG_FALSE;

    wifi_semid = semget(key, 0, 0);
    if (wifi_semid == -1)
    {
        wifi_semid = semget(key, 1, IPC_CREAT|IPC_EXCL|0666);
        if (wifi_semid == -1)
            return STRG_FALSE;

        semarg.val = 1;
        if (semctl(wifi_semid, 0, SETVAL, semarg) == -1)
        {
            semctl(wifi_semid, 0, IPC_RMID, semarg);
            return STRG_FALSE;
        }
    }

    return STRG_TRUE;
 }


/**
 * @brief Get lock
 *
 * @return STRG_TRUE on successful lock; otherwise STRG_FALSE
 */
strgBool_e storage_wifiLock() {
    struct sembuf sem;

    if (storage_wifiLockInit() == STRG_FALSE)
        return STRG_FALSE;

    sem.sem_num  = 0;
    sem.sem_op   = -1;
    sem.sem_flg  = SEM_UNDO;

    if (semop(wifi_semid, &sem, 1) == -1)
    {
        return STRG_FALSE;
    }

    return STRG_TRUE;
}


/**
 * @brief Release lock
 *
 * @return STRG_TRUE on successful unlock; otherwise STRG_FALSE
 */
strgBool_e storage_wifiUnlock() {

    struct sembuf sem;

    if (storage_wifiLockInit() == STRG_FALSE)
        return STRG_FALSE;

    sem.sem_num  = 0;
    sem.sem_op   = 1;
    sem.sem_flg  = SEM_UNDO;

    if (semop(wifi_semid, &sem, 1) == -1)
    {
        return STRG_FALSE;
    }

    return STRG_TRUE;
}


void storage_restartWireless(void)
{
    storage_wifiLock();
    system ("/sbin/wifi");
    storage_wifiUnlock();
}

int storage_AddDelVap(void *handle, const char *name, const char *value)
{
    int ret = 0;
    struct pkgmgr_s *pkgm;

    pkgm = pkgGetPkgmgr();

    if (!pkgm)
    {
        return -1;
    }

    ret = pkgInit(pkgm);

    if (ret < 0)
        goto fail;

    ret = pkgIfAction(pkgm, name, value);

fail:
    return ret;
}
