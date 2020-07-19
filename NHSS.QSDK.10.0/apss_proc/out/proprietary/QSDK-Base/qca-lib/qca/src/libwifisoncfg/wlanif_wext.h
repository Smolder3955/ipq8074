/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <asm/types.h>
#include <linux/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <assert.h>

#include <sys/types.h>
#include <net/if.h>
#include <net/ethernet.h>
#define _LINUX_IF_H /* Avoid redefinition of stuff */
#include <linux/wireless.h>
#include <ieee80211_external.h>
#include <linux/netlink.h>
#include <dbg.h>

#include "wlanif_cmn.h"

struct wlanif_wext_priv {
    int32_t Sock;
};

/* For debugging support */
struct sonwextDbg_t {
    struct dbgModule *dbgModule;
} sonwextDbgS;

int getName_wext(void *,const char *ifname, char *name );
int isAP_wext(void *, const char * ifname, uint32_t *result);
int getBSSID_wext(void *, const char *ifname, struct ether_addr *BSSID );
int getESSID_wext(void *ctx, const char * ifname, void *buf, uint32_t *len );
int getFreq_wext(void *, const char * ifname, int32_t * freq);
int getChannelWidth_wext(void *, const char *ifname, int * chwidth);
int getChannelExtOffset_wext(void *, const char *ifname, int * choffset);
int getChannelBandwidth_wext(void *, const char *ifname, int * bandwidth);
int getAcsState_wext(void *, const char *ifname, int * acsstate);
int getCacState_wext(void *, const char *ifname, int * cacstate);
int getParamIfindex_wext(void *, const char *ifname, int * cacstate);
int getSmartMonitor_wext(void *, const char *ifname, int * smartmonitor);
int getGenericInfoCmd_wext(void *, const char *ifname, int cmd, void * chanInfo, int chanInfoSize);
int getGenericInfo_wext(void *, const char *ifname, void * chanInfo, int chanInfoSize);
int getCfreq2_wext(void *, const char * ifname, int32_t * cfreq2);
int getChannelInfo_wext(void *, const char *ifname, void * chanInfo, int chanInfoSize);
int getChannelInfo160_wext(void *, const char *ifname, void * chanInfo, int chanInfoSize);
int getStationInfo_wext(void *, const char *ifname, void *buf, int * len);
int getDbgreq_wext(void * , const char *ifname, void *data , uint32_t data_len);
int getExtended_wext(void * , const char *ifname, void *data , uint32_t data_len);
int addDelKickMAC_wext(void *, const char *ifname, int operation, void *data, uint32_t len);
int setFilter_wext(void *, const char *ifname, void *data, uint32_t len);
int getWirelessMode_wext(void * ctx , const char *ifname, void *data, uint32_t len);
int sendMgmt_wext(void *, const char *ifname, void *data, uint32_t len);
int setParamMaccmd_wext(void *, const char *ifname, void *data, uint32_t len);
int setParam_wext(void *, const char *ifname,int cmd, void *data, uint32_t len);
int getStaStats_wext(void *, const char *ifname, void *data, uint32_t len);
int getRange_wext(void *ctx, const char *ifname, int *we_version);
int getStaCount_wext(void *, const char *ifname, int32_t *result);
int setIntfMode_wext(void *, const char *ifname, const char *mode, u_int8_t len);
int setParamVapInd_wext(void * ctx, const char *ifname, void *param, uint32_t len);
int setFreq_wext(void *ctx, const char * ifname, int freq);
