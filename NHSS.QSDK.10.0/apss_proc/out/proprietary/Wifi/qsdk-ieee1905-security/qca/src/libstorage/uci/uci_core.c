/*
 * @File: uci_core.c
 *
 * @Abstract: UCI core interface
 *
 * @Notes:
 *
 * Copyright (c) 2013 Qualcomm Atheros Inc.
 * All rights reserved.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include "uci_core.h"

int uciSet(struct uci_context *ctx, char *package, char *option, char *value)
{
    struct uci_ptr ptr;
    int ret = UCI_OK;
    char optv[UCI_MAX_STR_LEN*2];

    snprintf(optv, sizeof(optv), "%s.%s=%s", package, option, value);

    if (uci_lookup_ptr(ctx, &ptr, optv, true) != UCI_OK) {
        return -1;
    }

    ret = uci_set(ctx, &ptr);
    if (ret != UCI_OK)
        return -1;

    ret = uci_save(ctx, ptr.p);
    if (ret != UCI_OK)
        return -1;

    return 0;
}

int uciGet(struct uci_context *ctx, char *package, char *option, char *value)
{
    struct uci_ptr ptr;
    struct uci_element *e;
    char optv[UCI_MAX_STR_LEN];

    snprintf(optv, sizeof(optv), "%s.%s", package, option);

    if (uci_lookup_ptr(ctx, &ptr, optv, true) != UCI_OK) {
        return -1;
    }

    if (!(ptr.flags & UCI_LOOKUP_COMPLETE)) {
        return -1;
    }

    e = ptr.last;
    if (e->type != UCI_TYPE_OPTION ||
        ptr.o->type != UCI_TYPE_STRING)
        return -1;


    strlcpy(value, ptr.o->v.string, UCI_MAX_STR_LEN);

//   printf("get string %s\n", ptr.o->v.string);
    return 0;

}


int uciCommit(struct uci_context *ctx, char *package)
{
    struct uci_ptr ptr;

    if (uci_lookup_ptr(ctx, &ptr, package, true) != UCI_OK) {
        return -1;
    }

    if (uci_commit(ctx, &ptr.p, false) != UCI_OK)
        return -1;

    if (ptr.p)
        uci_unload(ctx, ptr.p);
    return 0;
}

struct uci_context *uciInit()
{
    struct uci_context *ctx = NULL;

    ctx = uci_alloc_context();
    if (!ctx) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }
//    uci_load_plugins(ctx, NULL);
    return ctx;
}

int uciDestroy(struct uci_context *ctx)
{
    uci_free_context(ctx);
    return 0;
}

int uciDelete(struct uci_context *ctx, const char *package, const char *option)
{
    struct uci_ptr ptr;
    int ret = UCI_OK;
    char optv[UCI_MAX_STR_LEN * 2];

    snprintf(optv, sizeof(optv), "%s.%s", package, option);
    if (uci_lookup_ptr(ctx, &ptr, optv, true) != UCI_OK) {
        return -1;
    }

    ret = uci_delete(ctx, &ptr);

    if (ret != UCI_OK)
        return -1;

    return 0;
}

int uciAddSection(struct uci_context *ctx, const char *package, const char *option,
        const char *sectionName)
{
    struct uci_section *s = NULL;
    struct uci_ptr ptr;
    char optv[UCI_MAX_STR_LEN * 2];
    struct uci_element* element_section = NULL;
#define SMALL_BUFF 512
    char vaps = 0;

    snprintf(optv, sizeof(optv), "%s.%s", package, option);

    if (uci_lookup_ptr(ctx, &ptr, optv, true) != UCI_OK) {
        return -1;
    }

    uci_add_section(ctx, ptr.p, sectionName , &s);
    uci_save(ctx, ptr.p);

    uci_foreach_element(&ptr.p->sections, element_section) {
        struct uci_section* section = NULL;
        section = uci_to_section(element_section);

        if (!strcmp(section->type,"wifi-iface")) {
            vaps++;
        }
    }
#undef SMALL_BUFF
    return vaps;
}
