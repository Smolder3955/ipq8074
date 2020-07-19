/* @File: wsplcd.c
 * @Notes:  IEEE1905 AP Auto-Configuration Daemon
 *          AP Enrollee gets wifi configuration from AP Registrar via
 *          authenticated IEEE1905 Interfaces
 *
 * Copyright (c) 2012, 2015-2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2012, 2015-2016 Qualcomm Atheros, Inc.
 *
 * Qualcomm Atheros Confidential and Proprietary.
 * All rights reserved.
 *
 */

/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

#include "wsplcd.h"
#include "eloop.h"
#include "ucpk_hyfi20.h"
#include "apac_priv.h"
#include "apac_hyfi20_mib.h"
#include <sys/time.h>
#include "apac_map.h"

char g_cfg_file[APAC_CONF_FILE_NAME_MAX_LEN];
char g_log_file_path[APAC_CONF_FILE_NAME_MAX_LEN];

char g_map_cfg_file[APAC_CONF_FILE_NAME_MAX_LEN];
apacMapEProfileMatcherType_e g_map_cfg_file_format;

int debug_level = MSG_INFO;
apacLogFileMode_e logFileMode = APAC_LOG_FILE_INVALID;

FILE *pLogFile = NULL;

extern int wlanIfConfigInit(u32);
void wlanIfConfigExit(void);

int dprintf(int level, const char *fmt, ...)
{
	va_list ap;
	struct timeval curTime;

	va_start(ap, fmt);
	if (level >= debug_level) {
		if (pLogFile) {
			gettimeofday(&curTime, NULL);
			fprintf(pLogFile, "[%lu.%lu] ", curTime.tv_sec, curTime.tv_usec);
			vfprintf(pLogFile, fmt, ap);
			fflush(pLogFile);
		} else {
			vprintf(fmt, ap);
		}
	}
	va_end(ap);
	return 0;
}

void shutdown_fatal(void)
{
    if (pLogFile) {
        fclose(pLogFile);
    }
    exit(1);
}

void apacHyfi20CheckArgList(int *argc, char **argv) {
    char tempArgvList[*argc][APAC_CONF_FILE_NAME_MAX_LEN];
    u_int8_t i, j = 0;

    for (i = 0; i < *argc; i++) {
        if (strlen(argv[i]) > 0) {
            memcpy(&tempArgvList[j++], &argv[i], strlen(argv[i]));
        }
    }

    for (i = 0; i < j; i++) {
        memcpy(&argv[i], &tempArgvList[i], strlen(tempArgvList[i]));
    }

    *argc = i;
}

int main(int argc, char **argv)
{
    apacInfo_t apacInfo;
    memset(&apacInfo, 0, sizeof(apacInfo_t));

    apacHyfi20CheckArgList(&argc, argv);
    apacHyfi20CmdLogFileModeName(argc, argv);

    if (logFileMode == APAC_LOG_FILE_APPEND) {
        pLogFile = fopen(g_log_file_path, "a");
    } else if (logFileMode == APAC_LOG_FILE_TRUNCATE) {
        pLogFile = fopen(g_log_file_path, "w");
    }

    /* enable command line configuration or read config file */
    optind = 0;
    apacHyfi20CmdConfig(&apacInfo.hyfi20, argc, argv);

    if (wlanIfConfigInit(apacInfo.hyfi20.isCfg80211))
    {
        dprintf(MSG_INFO, "wlanIfConfigInit Failed\n");
        return -1;
    }

    /* set up default configuration */
    wsplcd_hyfi10_init(&apacInfo.hyfi10);

    apacHyfi20ConfigInit(&apacInfo.hyfi20);

    /* Start wsplcd daemon */
    dprintf(MSG_INFO, "wsplcd daemon starting. cfg80211 config %d \n",apacInfo.hyfi20.isCfg80211);

    eloop_init(&apacInfo);

    if (apacHyfi20Init(&apacInfo.hyfi20) <0)
    {
        dprintf(MSG_INFO, "%s, Failed to initialize\n", __func__);
        return -1;
    }

    if (!apacHyfiMapInit(HYFI20ToMAP(&apacInfo.hyfi20))) {
        dprintf(MSG_INFO, "%s: Failed to initialize MAP logic\n", __func__);
        return -1;
    }

    apacHyfi20ConfigDump(&apacInfo.hyfi20);

    apacHyfi20AtfConfigDump(&apacInfo.hyfi20);

    apacHyfiMapConfigDump(HYFI20ToMAP(&apacInfo.hyfi20));
    /* Restore QCA VAPIndependent flag*/
    if (apacInfo.hyfi20.config.manage_vap_ind)
    {
        apac_mib_set_vapind(&apacInfo.hyfi20, apacInfo.hyfi20.config.manage_vap_ind);
    }

    /* UCPK Init*/
    if (strlen(apacInfo.hyfi20.config.ucpk) > 0){
        char wpapsk[62+1];
        char plcnmk[32+1];
        if (ucpkHyfi20Init(apacInfo.hyfi20.config.ucpk,
            apacInfo.hyfi20.config.salt,
            apacInfo.hyfi20.config.wpa_passphrase_type,
            wpapsk,
            plcnmk) < 0)
        {
            dprintf(MSG_INFO, "%s :Invalid 1905.1 UCPK\n", __func__);
        }
        else
        {
            apac_mib_set_ucpk(&apacInfo.hyfi20, wpapsk, plcnmk);
        }
    }

    apacHyfi20Startup(&apacInfo.hyfi20);

    /* check compatiblility with Hyfi-1.0 */
    if (apacInfo.hyfi20.config.hyfi10_compatible)
    {
        wsplcd_hyfi10_startup(&apacInfo.hyfi10);
    }

    if (apacInfo.mapData.enable) {
        apacGetPIFMapCap(&apacInfo.hyfi20);
    }

    eloop_run();
    eloop_destroy();

    if (apacInfo.mapData.enable) {
        apacHyfiMapDInit(&apacInfo.mapData);
    }

    if (apacInfo.hyfi20.config.hyfi10_compatible)
    {
        wsplcd_hyfi10_stop(&apacInfo.hyfi10);
    }

    wlanIfConfigExit();

    apacHyfi20DeinitSock(&apacInfo.hyfi20);

    if (pLogFile) {
        fclose(pLogFile);
    }

    /* Probably won't get here... */
    printf("Leaving wsplcd executive program\n");

    return 0;
}


