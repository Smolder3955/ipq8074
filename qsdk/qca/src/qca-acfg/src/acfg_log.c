/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include <stdint.h>

#include <acfg_types.h>
#include <qdf_types.h>
#include <acfg_api_pvt.h>

#define LOG_NAME_LEN    100

/*
 * Buffer to hold name of log file.
 */
static uint8_t logfile[LOG_NAME_LEN] ;

static uint8_t *logprefix = (uint8_t *)"/tmp/acfglog." ;

static FILE *logfd = NULL;

static FILE* get_log_fd(void)
{
    FILE *fd = NULL;
    pid_t pid ;
    char *str ;
    int ret ;

    pid = getpgrp();
    ret = asprintf(&str,"%s%d",(char *)logprefix,pid);
    if(ret != -1)
    {
        uint32_t len ;

        logfile[0] = '\0' ;
        len = acfg_os_strcpy(logfile,(uint8_t *)str,LOG_NAME_LEN);
        free(str);
        if(len)
            fd = fopen((const char *)logfile,"w+");
    }
    else
        fd = NULL ;

    logfd = fd ;
    return fd;
}


/**
 * @brief Log acfg lib messages to a file.
 *        The file is named as '<logprefix><group id>'
 *        logprefix is predefined in this file.
 *        Group id is the pid of the first thread in this group.
 *
 * @param msg
 *
 * @return
 */
uint32_t
acfg_log(uint8_t *msg)
{
    FILE *fd ;

    if(logfd == NULL)
    {
        fd = get_log_fd();
        if(fd == NULL)
            return QDF_STATUS_E_FAILURE ;
    }
    else
        fd = logfd ;

    //fprintf(stderr, "Opening log file %s OK \n", (char *)logfile);
    fprintf(fd,"%s\n",(char *)msg);
    fflush(fd);
    return QDF_STATUS_SUCCESS ;
}

#include <string.h>
#include <stdarg.h>

#define ACFG_ERRSTR_LEN 1024

static char errstr[ACFG_ERRSTR_LEN];
static unsigned int errstr_bytes = 0;

char *acfg_get_errstr(void)
{
    return errstr;
}

void acfg_reset_errstr(void)
{
    memset(errstr, '\0', sizeof(errstr));
    errstr_bytes = 0;
}

void _acfg_log_errstr(const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = vsnprintf(&errstr[errstr_bytes], (sizeof(errstr) - errstr_bytes), fmt, ap);
    va_end(ap);

    if (ret < 0) {
        /* ignore output error */
    } else if ((unsigned)ret >= (sizeof(errstr) - errstr_bytes)) {
        /* output was truncated */
        errstr_bytes = sizeof(errstr);
        errstr[errstr_bytes - 1] = '\0';
    } else {
        /* success */
        errstr_bytes += ret;
    }
}

void _acfg_print(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
