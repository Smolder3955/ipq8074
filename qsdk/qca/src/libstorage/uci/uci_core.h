/*
 * @File: uci_core.h
 *
 * @Abstract: UCI core interface
 *
 * @Notes:
 *
 * Copyright (c) 2013 Qualcomm Atheros Inc.
 * All rights reserved.
 *
 */

#include <uci.h>


#define UCI_MAX_STR_LEN 256

int uciSet(struct uci_context *ctx, char *package, char *option, char *value);
int uciGet(struct uci_context *ctx, char *package, char *option, char *value);
int uciCommit(struct uci_context *ctx, char *package);
struct uci_context *uciInit();
int uciDestroy(struct uci_context *ctx);
int uciDelete(struct uci_context *ctx, const char *package, const char *option);
int uciAddSection(struct uci_context *ctx, const char *package
        , const char *option, const char *sectionName);


