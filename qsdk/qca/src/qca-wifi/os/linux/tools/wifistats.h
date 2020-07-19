/*
* Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/
#ifndef _WIFISTATS_H_
#define _WIFISTATS_H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <qcatools_lib.h>

struct wifistats_module {
    struct wifistats_module *next;
    char *name;
    void (*help) (int argc, char *argv[]);
    int (*input_validate) (int argc, char *argv[]);
    void * (*input_buff_alloc) (int *size);
    int (*input_parse) (void *, int argc, char *argv[]);
    void (*input_buff_free) (void *);
    int (*input_cookie_generate) (void);
    int (*output_handler) (void *, int len);
    int (*output_cookie_get) (void *, int len);
    int timeout; /* timeout value in milli seconds */
};

int wifistats_module_register (struct wifistats_module *module, int size);
int wifistats_module_unregister (struct wifistats_module *module, int size);
int  extract_mac_addr(uint8_t *addr, const char *text);

enum LISTEN_STATUS {
    LISTEN_CONTINUE = 0,
    LISTEN_DONE,
};

#define IEEE80211_ADDR_LEN 6

#define A_ASSERT(expr)  \
    if (!(expr)) {   \
        errx(1, "Debug Assert Caught, File %s, Line: %d, Test:%s \n",__FILE__, __LINE__,#expr); \
    }
#endif /*_WIFISTATS_H_*/
