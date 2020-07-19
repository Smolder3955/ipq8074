/*
 * @File: profile.c
 *
 * @Abstract: Load balancing daemon configuration/profile support
 *
 * @Notes:
 *
 * Copyright (c) 2011,2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>

#include <dbg.h>
#include <split.h>

#include "module.h"
#include "config.h"
#include "profile.h"
#include "lbd_assert.h"

#define profileMissing ""
#define MAX_CLIENT_CLASS_GROUP 2
#define MAX_CLIENT_CLASS_GROUP_SIZE 16

/*--- profileState -- global data for profile
 */
struct profileState {
    int IsInit;
    const char *File;
    struct dbgModule *DebugModule;
} profileS;

struct profileOpt {
    const char *Section;
    const char *Missing;
} profileOpts[] = {
    {"MAIN",        profileMissing},
#include "lb_profileSections.h"
#ifdef LBD_MODULE_SONEVENT
    {"SonEventService", profileMissing},
#endif
};

/*--- profileDebug -- print debug messages (see dbgf documentation)
 */
#define profileDebug(...) dbgf(profileS.DebugModule, __VA_ARGS__)

const char *profileElementDefault(const char *Element,
        struct profileElement *DefaultTable)
{
    int Index = 0;

    if (!Element || !DefaultTable) return NULL;

    while (DefaultTable[Index].Element) {
        if (!strcmp(DefaultTable[Index].Element, Element))
            return DefaultTable[Index].Default;
        Index++;
    }

    return NULL;
}

/* DefaultTable is the default value which the module owns.
 * It can be set to NULL when there is no default table.
 */
const char *profileGetOpts(u_int32_t ModuleID,
        const char *Element,
        struct profileElement *DefaultTable)
{
    const char *Result = NULL;

    if (ModuleID >= mdModuleID_MaxNum || !Element){
        profileDebug(DBGERR, "%s: Invalid parameters: ModuleID %d, Element %p",__func__,ModuleID, Element);
        goto out;
    }
    Result = configstring(profileS.File,
            profileOpts[ModuleID].Section,
            Element,
            profileOpts[ModuleID].Missing);

    if (!Result || !strlen(Result)) {
        if (Result) { free((char *) Result); }
        Result = profileElementDefault(Element, DefaultTable);

        // Allocations from the defaults table need to be strdup'ed so that
        // all return values from this function need to be free'ed. Otherwise,
        // the caller would have no way to know whether free() should be
        // called or not.
        if (Result) { Result = strdup(Result); }
    }

out:
    return Result ? Result : strdup(profileMissing);
}

int profileGetOptsInt(u_int32_t ModuleID,
        const char *Element,
        struct profileElement *DefaultTable)
{
    int Result = -1;
    const char *ResultStr = profileGetOpts(ModuleID, Element, DefaultTable);
    if (ResultStr) {
        Result = atoi(ResultStr);
        free((char *) ResultStr);  // must cast away const-ness for free
    }

    return Result;
}

float profileGetOptsFloat(u_int32_t ModuleID,
        const char *Element,
        struct profileElement *DefaultTable)
{
    float Result = -1;
    const char *ResultStr = profileGetOpts(ModuleID, Element, DefaultTable);
    if (ResultStr) {
        Result = atof(ResultStr);
        free((char *) ResultStr);  // must cast away const-ness for free
    }

    return Result;
}

int
profileGetOptsIntArray(u_int32_t ModuleID,
                       const char *Element,
                       struct profileElement *DefaultTable,
                       int *Key)
{
    int Result = -1;
    const char *ResultStr = NULL;
    u_int32_t Index = 0;
    int NumKeys = 0;
    char Buff[MAX_CLIENT_CLASS_GROUP][MAX_CLIENT_CLASS_GROUP_SIZE];

    ResultStr = profileGetOpts(ModuleID, Element, DefaultTable);

    if (ResultStr != NULL && strlen(ResultStr) > 0) {

        NumKeys = splitByToken(ResultStr,
                               sizeof(Buff) / sizeof(Buff[0]),
                               sizeof(Buff[0]),
                               (char *)Buff, ' ');

        if (NumKeys == -1) {
            return Result;
        } else if (NumKeys == 0) {
            for (Index = 0; Index < MAX_CLIENT_CLASS_GROUP; Index++) {
                Key[Index] = atol(ResultStr);
            }
        } else {
            for (Index = 0; Index < NumKeys; Index++) {
                Key[Index] = atol(Buff[Index]);
            }
            if (NumKeys < MAX_CLIENT_CLASS_GROUP) {
                for (Index = NumKeys; Index < MAX_CLIENT_CLASS_GROUP; Index++) {
                    Key[Index] = atol(Buff[0]);
                }
            }
        }
    }

    Result = 0;
    free((char *) ResultStr);
    return Result;
}

/*====================================================================*
 *          Init
 *--------------------------------------------------------------------*/
void profileInit(const char *File)
{
    if (profileS.IsInit)
        return;

    memset(&profileS, 0, sizeof profileS);
    profileS.IsInit = 1;
    profileS.File = File;

    profileS.DebugModule = dbgModuleFind("profile");
    profileDebug(DBGDEBUG, "%s: File %s", __func__, File);
}


