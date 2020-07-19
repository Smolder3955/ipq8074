/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */

/* SON Libraries - Vendor specific module file  */
/*    This implements SON library APIs          */

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
#include <assert.h>
#include <linux/socket.h>

#include <net/if.h>
#define _LINUX_IF_H /* Avoid redefinition of stuff */
#include <linux/wireless.h>
#include <sys/un.h>
#include <evloop.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>

#include "sonlib_cmn.h"
#include "sonlib_ath10k_CommandParser.h"

//MAC address print marcos
#define ath10k_MACAddFmt(_sep) "%02X" _sep "%02X" _sep "%02X" _sep "%02X" _sep "%02X" _sep "%02X"
#define ath10k_MACAddData(_arg) __ath10kMidx(_arg, 0), __ath10kMidx(_arg, 1), __ath10kMidx(_arg, 2), \
                             __ath10kMidx(_arg, 3), __ath10kMidx(_arg, 4), __ath10kMidx(_arg, 5)
#define __ath10kMidx(_arg, _i) (((u_int8_t *)_arg)[_i])

#define ath10k_CopyMACAddr(src, dst) memcpy( dst, src, ETH_ALEN )
#define ARPHRD_ETHER_FAMILY    1

#ifdef QCA_PARTNER_PLATFORM
extern size_t strlcpy(char *dest_addr, const char *src_addr, size_t size);
#endif

int noOfInterfaces;

struct priv_ath10k priv_ath10k_ifname[32];
struct timerHandle_ath10k timerHandle;

/* Vendor mapping for APIs */
/* NOTE: Order of these APIs should match */
/* with somcmnOps struct defined in cmn.h */
static struct soncmnOps ath10k_ops = {
    .init = sonInit_ath10k,
    .deinit = sonDeinit_ath10k,
    .getName = getName_ath10k,
    .getBSSID = getBSSID_ath10k,
    .getSSID = getSSID_ath10k,
    .isAP = isAP_ath10k,
    .getFreq = getFreq_ath10k,
    .getIfFlags = getIfFlags_ath10k,
    .getAcsState = getAcsState_ath10k,
    .getCacState = getCacState_ath10k,
    .setBSteerAuthAllow = setBSteerAuthAllow_ath10k,
    .setBSteerProbeRespWH = setBSteerProbeRespWH_ath10k,
    .setBSteerProbeRespAllow2G = setBSteerProbeRespAllow2G_ath10k,
    .setBSteerOverload = setBSteerOverload_ath10k,
    .performLocalDisassoc = performLocalDisassoc_ath10k,
    .performKickMacDisassoc = performKickMacDisassoc_ath10k,
    .setBSteering = setBSteering_ath10k,
    .setBSteerEvents = setBSteerEvents_ath10k,
    .setBSteerInProgress = setBSteerInProgress_ath10k,
    .setBSteerParams = setBSteerParams_ath10k,
    .getBSteerRSSI = getBSteerRSSI_ath10k,
    .getDataRateInfo = getDataRateInfo_ath10k,
    .getStaInfo = getStaInfo_ath10k,
    .getStaStats = getStaStats_ath10k,
    .setAclPolicy = setAclPolicy_ath10k,
    .sendBcnRptReq = sendBcnRptReq_ath10k,
    .sendBTMReq = sendBTMReq_ath10k,
    .setMacFiltering = setMacFiltering_ath10k,
    .getChannelWidth = getChannelWidth_ath10k,
    .getOperatingMode = getOperatingMode_ath10k,
    .getChannelExtOffset = getChannelExtOffset_ath10k,
    .getCenterFreq160 = getCenterFreq160_ath10k,
    .getCenterFreqSeg1 = getCenterFreqSeg1_ath10k,
    .getCenterFreqSeg0 = getCenterFreqSeg0_ath10k,
    .enableBSteerEvents = enableBSteerEvents_ath10k,
    .dumpAtfTable = dumpAtfTable_ath10k,
};



/********************************************************************/
/************************ Southbound APIs ***************************/
/********************************************************************/

/**
 * @brief Function to get name of the device
 *
 * @param [in] ctx     opaque pointer to private vendor struct
 * @param [in] ifname  interface name
 *
 * @param [out] name   char pointer, filled by this function for
 *                     name of protocol
 *
 * @return Success: 0, Failure: -1
 *
 */
int getName_ath10k(const void *ctx, const char * ifname, char *name, size_t len)
{
    return 0;
}


/**
 * @brief Function to get BSSID address
 *
 * @param [in] ctx     opaque pointer to private vendor struct
 * @param [in] ifname  interface name
 *
 * @param [out] bssid  bssid pointer to be filled by this function
 *                     with BSSID address
 *
 * @return Success: 0, Failure: -1
 *
 */
int getBSSID_ath10k(const void *ctx, const char * ifname, struct ether_addr *bssid )
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
      goto err;
    char *macaddr = NULL;
    macaddr = (char *)malloc(20*sizeof(char));
    command_reply[recev] = '\0';
    tmp = command_reply;
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"%s command_reply:%s \n",__func__,command_reply);
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while (parameter_value) {
           if (strncmp (parameter_value,"bssid",5) == 0) {
                char *tmp_bssid;
                tmp_bssid=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"%s tmp_bssid:%s \n",__func__,tmp_bssid);
                strlcpy(macaddr,tmp_bssid,18);
                getMacaddr_ath10k(macaddr,(u_int8_t *)bssid);
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    free(macaddr);
    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return sd;
}

/**
 * @brief Function to get SSID info
 *
 * @param [in] ctx     opaque pointer to private vendor struct
 * @param [in] ifname  interface name
 *
 * @param [out] buf    ssid pointer to be filled by this function
 *                     with SSID name
 * @param [out] len    length pointer to be filled by this function
 *                     with length of SSID
 *
 * @return Success: 0, Failure: -1
 *
 */
int getSSID_ath10k(const void *ctx, const const char * ifname, void *ssidname, u_int32_t *length )
{
    u_int8_t i;
    struct priv_ath10k *privAth10k;

    i = soncmnGetIfIndex(ifname);
    privAth10k = &priv_ath10k_ifname[i];
    TRACE_ENTRY();
    getssidName_ath10k(privAth10k, (u_int8_t*)ssidname, length);
    TRACE_EXIT();
    return 0;
}

/**
 * @brief Function to check whether the current device is AP
 *
 * @param [in] ctx     opaque pointer to private vendor struct
 * @param [in] ifname  interface name
 *
 * @param [out] result result pointer to be filled by this function
 *                     set to 1 if device is AP, else 0
 *
 * @return Success: 0, Failure: -1
 *
 */
int isAP_ath10k(const void *ctx, const char * ifname, u_int32_t *result)
{
    return 0;
}

/**
 * @brief Function to get frequency info
 *
 * @param [in] ctx     opaque pointer to private vendor struct
 * @param [in] ifname  interface name
 *
 * @param [out] freq   frequency pointer to be filled by this function
 *                     with frequency value in Hz
 *
 * @return Success: 0, Failure: -1
 *
 */
int getFreq_ath10k(const void *context, const char * ifname, int32_t * freq)
{

    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];
    TRACE_ENTRY();

    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while ( parameter_value){
            if (strncmp (parameter_value,"freq",4) == 0) {
                char *frequency;
                frequency = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                *freq = atoi(frequency);
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
//     Main son Logic code is dividing freq with 100000 so we are multipying freq
    *freq *= 100000;
   TRACE_EXIT();
   return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}


/**
 * @brief Function to get Interface Flags state
 *
 * @param [in] ctx     opaque pointer to private vendor struct
 * @param [in] ifname  interface name
 *
 * @param [out] ifr    pointer to ifreq struct, to be filled by
 *                     this function with interface flags
 *
 * @return Success: 0, Failure: -1
 *
 */
int getIfFlags_ath10k(const void *ctx, const const char *ifname , struct ifreq *ifr )
{
    int i,ret;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];
    TRACE_ENTRY();

    if ((ret = ioctl(privAth10k->sock, SIOCGIFFLAGS, ifr)) < 0) {
        goto err;
    }
    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/**
 * @brief Function to get ACS State
 *
 * @param [in] ctx       opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 *
 * @param [out] acsstate pointer to be filled by this function
 *                       ACS State (non-zero means ACS still in
 *                       progress, 0: ACS is done)
 *
 * @return Success: 0, Failure: -1
 *
 */
int getAcsState_ath10k(const void * ctx, const char *ifname, int *acs)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;

    command_reply[recev] = '\0';
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while (parameter_value) {
            if (strcmp (parameter_value,"state") == 0)
            {
                if (strcmp (strtok_r(tmp_substring_array,"=",&tmp_substring_array),"ACS") == 0) {
                    *acs=1;
                    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"acs = %d \n",*acs);
                }
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}

/**
 * @brief Function to get CAC State
 *
 * @param [in] ctx       opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 *
 * @param [out] cacstate pointer to be filled by this function
 *                       CAC State (non-zero means CAC still in
 *                       progress, 0: CAC is done)
 *
 * @return Success: 0, Failure: -1
 *
 */
int getCacState_ath10k(const void * ctx, const char *ifname, int * cacstate)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,
                                                          command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while (parameter_value) {
            if (strcmp (parameter_value,"cac_time_left_seconds") == 0)
            {
                if (strcmp (strtok_r(tmp_substring_array,"=",&tmp_substring_array),"N/A") != 0) {
                    *cacstate=1;
                    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"cacstate = %d \n",*cacstate);
                }
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}

/**
 * @brief Common function to send all DBGReq ioctls
 *          for various set and get subcommands
 *
 * @param [in] ctx    opaque pointer to private vendor struct
 * @param [in] ifname interface name
 * @param [in] data   data for ioctl
 * @param [in] len    length of the data
 *
 * @return Success: 0, Failure: -1
 *
 */
int sendDbgReqSetCmn(const void * ctx , const char *ifname, void *data , u_int32_t data_len)
{
    return 0;
}

/**
 * @brief Function to update whether authentication
 *          should be allowed for a given MAC or not
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] destAddr  stamac for which Auth will be allowed/disallowed
 * @param [in] authAllow flag, 1: allow Auth, 0: not allow Auth
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerAuthAllow_ath10k (const void *context, const char *ifname,
                               const u_int8_t *destAddr, u_int8_t authAllow)
{
    return 0;
}

/**
 * @brief Function to Enable/Disable probe responses
 *          withholding for a given MAC
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] destAddr stamac for which enable/disable probe resp withholding
 * @param [in] probeRespWHEnable flag, 1: Enable Withholding, 0: Disable withholding
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerProbeRespWH_ath10k (const void *context, const char *ifname,
                             const u_int8_t *destAddr, u_int8_t probeRespWHEnable)
{
    return 0;
}

/**
 * @brief Function to update whether probe responses
 *        should be allowed or not for a MAC in 2.4G band
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] destAddr stamac for which enable/disable 2G probe response
 * @param [in] allowProbeResp2G  flag, 1: Enable probe resp, 0: Disable probe resp
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerProbeRespAllow2G_ath10k (const void *context, const char *ifname,
                              const u_int8_t *destAddr, u_int8_t allowProbeResp2G)
{
   return 0;
}

/**
 * @brief Function to toggle a VAP's overloaded status based
 *          on channel utilization and thresholds
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] setOverload flag, 1: Set overload status, 0: Clear overload
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerOverload_ath10k (const void *context, const char *ifname, u_int8_t setOverload)
{
    return 0;
}

/**
 * @brief Function to cleanup all STA state (equivalent to
 *        disassociation, without sending the frame OTA)
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] destAddr sta addr which needs cleanup
 *
 * @return Success: 0, Failure: -1
 *
 */
int performLocalDisassoc_ath10k (const void *context, const char *ifname, const u_int8_t *staAddr)
{
    char command[50];
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    snprintf(command,50,"DEAUTHENTICATE %02x:%02x:%02x:%02x:%02x:%02x tx=0",staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5]);

    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;

    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}

/**
 * @brief Function to enable Band Steering on per radio
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] bSteerEnable flag, 1: Enable BSteering, 0: Disable BSteering
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteering_ath10k (const void *context, const char *ifname, u_int8_t bSteerEnable)
{
    return 0;
}

/**
 * @brief Function to enable or disable band steering on a VAP
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] bSteerEventsEnable flag, 1: Enable, 0: Disable
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerEvents_ath10k (const void *context, const char *ifname, u_int8_t bSteerEventsEnable)
{
    return 0;
}

/**
 * @brief Function to set/unset band steering in-progress flag for a STA
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] destAddr stamac for which steering-in-progress flag needs
 *                       to be set/unset
 * @param [in] bSteerInProgress flag, 1: Set, 0: Unset/Clear
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerInProgress_ath10k (const void *context, const char *ifname,
                             const u_int8_t *destAddr, u_int8_t bSteerInProgress)
{
    return 0;
}

/**
 * @brief Function to set static band steering config parameters
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] data      data to be passed to driver (bsteering params)
 * @param [in] data_len  length of the data (here sizeof(struct
 *                       ieee80211_bsteering_param_t)
 *
 * @return Success: 0, Failure: -1
 *
 */
int setBSteerParams_ath10k (const void *context, const char *ifname, void *data, u_int32_t data_len)
{
    return 0;
}

/**
 * @brief Function to indicate driver that SON wants RSSI info for a STA
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] destAddr  sta mac for which RSSI is requested
 * @param [in] numSamples number of samples to collect for RSSI reporting
 *
 * @return Success: 0
 *
 */
int getBSteerRSSI_ath10k(const void * context,
                               const char * ifname,
                               const u_int8_t *staAddr, u_int8_t numSamples)
{
    char command[50] = "POLL_STA ";
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev,sd;
    static const struct ether_addr tempAddr = {{0,0,0,0,0,0}};
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    if (!(lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, staAddr))) {
    snprintf(command,50,"POLL_STA %02x:%02x:%02x:%02x:%02x:%02x",staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5]);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    }
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}


/**
 * @brief Function to send Beacon report request (802.11k)
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] destAddr  destination mac for Beacon request
 * @param [in] bcnreq     data for Beacon request
 *
 * @return Success: 0, Failure: -1
 *
 */
int sendBcnRptReq_ath10k (const void * context, const char * ifname,
        const u_int8_t *staAddr, struct soncmn_ieee80211_rrm_beaconreq_info_t *bcnrpt)
{
    char command[1028] = "REQ_BEACON ";
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char ssid[IEEE80211_NWID_LEN + 1];
    u_int32_t ssidLength;
    u_int8_t subElementId0;
    u_int8_t subElementId1, subElement1Len;
    u_int8_t subElementId2, subElement2Len;
    u_int8_t reqmode = 0x00;
    u_int8_t j;
    j = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[j];

    TRACE_ENTRY();
    // Fill Sub-Element ID - 00
    subElementId0 = 0;
    getssidName_ath10k(privAth10k, (void *)&ssid, &ssidLength);
//    subElement0Len = ssidLength;
    char ssidname[128];
    char *name = ssidname;
    int i,pcount=0;
    for(i =0 ;i<ssidLength;i++)
    {
        pcount+=sprintf((name+pcount),"%02x",((unsigned char *) ssid)[i]);
    }
    // Fill Sub-Element ID - 01
    subElementId1 = 1;
    subElement1Len = sizeof(bcnrpt->rep_cond) + sizeof(bcnrpt->rep_thresh);

    // Fill Sub-Element ID - 02
    subElementId2 = 2;
    subElement2Len = sizeof(bcnrpt->rep_detail);
    bcnrpt->duration = le16toh( bcnrpt->duration);

    snprintf(command, 1028, "REQ_BEACON %02x:%02x:%02x:%02x:%02x:%02x req_mode=%02d %2x%02x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x%02x%s%02d%02d%02d%02d%02d%02d%02d",
             staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5],
             reqmode, bcnrpt->regclass, bcnrpt->channum, bcnrpt->random_ivl, bcnrpt->duration, bcnrpt->mode,
             bcnrpt->bssid[0], bcnrpt->bssid[1], bcnrpt->bssid[2], bcnrpt->bssid[3], bcnrpt->bssid[4], bcnrpt->bssid[5],
             subElementId0, ssidLength, ssidname,
             subElementId1, subElement1Len, bcnrpt->rep_cond, bcnrpt->rep_thresh,
             subElementId2, subElement2Len, bcnrpt->rep_detail);

    memcpy(command_send, command, sizeof (command));
    command_leng = strlen(command);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}

/**
 * @brief Function to send BTM request (802.11v)
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] destAddr  destination mac for BTM request
 * @param [in] btmreq     data for BTM request
 *
 * @return Success: 0, Failure: -1
 *
 */
int sendBTMReq_ath10k (const void * context, const char * ifname,
        const u_int8_t *staAddr, struct soncmn_ieee80211_bstm_reqinfo_target *reqinfo)
{
    char command[1200] = "BSS_TM_REQ ";
    char command_var[200] = " ";
    char wbc_subie[] = "0603000000";
    char pref_subie[] = "0301";
    static char command_send[1200], command_reply[10];
    int command_leng,reply_leng,recev,sd;
    u_int8_t i,j;

    j = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k =  &priv_ath10k_ifname[j];

    TRACE_ENTRY();
    snprintf(command, 200 ,"BSS_TM_REQ %02x:%02x:%02x:%02x:%02x:%02x "
            "disassoc_timer=%d disassoc_imminent=%d "
            "pref=1 abridged=1 valid_int=255 bss_term=0 ",
            staAddr[0],staAddr[1],staAddr[2],
            staAddr[3],staAddr[4],staAddr[5],
            reqinfo->disassoc_timer, reqinfo->disassoc);

    for( i=0; i < reqinfo->num_candidates; i++ ) {
        snprintf(command_var, 200, "neighbor=%02x:%02x:%02x:%02x:%02x:%02x,23,%d,%d,%d,%s%s%02x ",
                reqinfo->candidates[i].bssid[0], reqinfo->candidates[i].bssid[1], reqinfo->candidates[i].bssid[2],
                reqinfo->candidates[i].bssid[3], reqinfo->candidates[i].bssid[4], reqinfo->candidates[i].bssid[5],
                reqinfo->candidates[i].op_class, reqinfo->candidates[i].channel_number,
                reqinfo->candidates[i].phy_type, wbc_subie, pref_subie, reqinfo->candidates[i].preference);
        strncat(command, command_var, sizeof(command_var));
    }

    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd > 0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"command_reply is %s\n", command_reply);

    TRACE_EXIT();
    if( strncmp(command_reply, "OK", 2) == 0 )
        return SONCMN_OK;
    else
        return SONCMN_NOK;
err:
   TRACE_EXIT_ERR();
   return sd;
}


/**
 * @brief Function to get Data rate Info for a Station or VAP
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] destAddr  sta mac for which data rate info is requested
 * @param [in] datarateInfo pointer to data, to be filled by this function
 *
 * @return Success: 0, Failure: -1

 *
 */
int getDataRateInfo_ath10k (const void *context, const char *ifname,
       const u_int8_t *destAddr, struct soncmn_ieee80211_bsteering_datarate_info_t *datarateInfo)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    int secondary_channel = 0, ieee80211n = 0,ieee80211ac = 0,vht_oper_width = 0,freq = 0;
    char  *vht_rx_mcs_map = NULL;
    char *vht_tx_mcs_map = NULL;
    int htmaxmcs = 0,vhtmaxmcs = 0;
    int vhtrxmcs = 0,vhttxmcs = 0;
    int mcsarray[10];
    char htmcsarray[10][10];
    int maxnss;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while ( parameter_value){
            if (strncmp (parameter_value,"freq",4) == 0) {
                char *frequency;
                frequency = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                freq = atoi(frequency);
            }
            else if (strncmp (parameter_value,"secondary_channel",17) == 0) {
                char *sec_channel;
                sec_channel = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                secondary_channel = atoi(sec_channel);
            }
            else if (strncmp (parameter_value,"ieee80211n",10) == 0) {
                char *ieee80211_n;
                ieee80211_n = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                ieee80211n = atoi(ieee80211_n);
            }
            else if (strncmp (parameter_value,"ieee80211ac",11) == 0) {
                char *ieee80211_ac;
                ieee80211_ac = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                ieee80211ac = atoi(ieee80211_ac);
            }
            else if (strncmp (parameter_value,"vht_oper_chwidth",16) == 0) {
                char *vhtoperchwidth;
                vhtoperchwidth = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                vht_oper_width = atoi(vhtoperchwidth);
            }
            else if (strncmp (parameter_value,"max_txpower",11) == 0) {
                char *max_txpower;
                max_txpower = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                datarateInfo->max_txpower = atoi(max_txpower);
            }
            else if (strncmp (parameter_value,"ht_mcs_bitmask",14) == 0) {
                char *htmcsbitmask;
                int i;
                htmcsbitmask = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                for (i = 0;*htmcsbitmask != '\0';i++)
                {
                    strlcpy(htmcsarray[i],htmcsbitmask,3);
                    mcsarray[i] = (int)strtol(htmcsarray[i], NULL, 16);
                    htmcsbitmask = htmcsbitmask+2;
                }
            }
            else if (strncmp (parameter_value,"rx_vht_mcs_map",14) == 0) {
                vht_rx_mcs_map = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                vhtrxmcs = (int)strtol(vht_rx_mcs_map, NULL, 16);
            }
            else if (strncmp (parameter_value,"tx_vht_mcs_map",14) == 0) {
                vht_tx_mcs_map = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                vhttxmcs = (int)strtol(vht_tx_mcs_map, NULL, 16);
            }

            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    htmaxmcs = mcsbitmasktomcs(mcsarray);
    vhtmaxmcs = txrxvhtmaxmcs(vhtrxmcs,vhttxmcs);
    maxnss = maxnssconversion(mcsarray, vhtrxmcs,ieee80211n, ieee80211ac);

    datarateInfo->num_streams = maxnss;
    datarateInfo->max_chwidth = getChannelWidth(secondary_channel, ieee80211n, ieee80211ac, vht_oper_width);
    datarateInfo->phymode = getPhyMode(ieee80211n,ieee80211ac,freq);
    if((ieee80211ac == 0) && (freq < 5000)){
        datarateInfo->max_MCS = htmaxmcs%8;
    }else{
        datarateInfo->max_MCS = vhtmaxmcs;
    }
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   return sd;
}

/**
 * @brief Function to get associated Station information
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] getStaInfo pointer to StaInfo data, to be filled by this function
 *
 * @return Success: 0, Failure: -1
 *
 */
int getStaInfo_ath10k (const void * context, const char * ifname, struct soncmn_req_stainfo_t *getStaInfo)
{

    SONCMN_STATUS result = SONCMN_OK;
    int loop_iter = 0, i = 0;
    struct ether_addr addr = {{0,0,0,0,0,0}};
    u_int8_t channel;
    u_int32_t  freq;
    wlan_band band;
    u_int8_t j;
    j = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[j];

    TRACE_ENTRY();
    freq = getVapFreq_ath10k(privAth10k);
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"%s Freq:%d \n",__func__, freq);
//     Main son Logic code is dividing freq with 100000 so we are multiplying
    freq *= 100000;
    result = resolveChannum(freq,&channel);
    if(result!= SONCMN_OK) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s Freq not exact \n",__func__);
    }
    band = resolveBandFromChannel(channel);
    do {

        ATH10K_BOOL isRRMSupported = 0;
        ATH10K_BOOL isBTMSupported = 0;

        struct soncmn_ieee80211_bsteering_datarate_info_t phyCapInfo;
        getStaInfo->stainfo[i].phy_cap_valid = 0;
        getStaInfo->stainfo[i].isbtm = 0;
        getStaInfo->stainfo[i].isrrm = 0;
        getDataRateSta_ath10k(privAth10k,&addr,&phyCapInfo, &isRRMSupported,&isBTMSupported,&loop_iter);
        if (loop_iter != 0)
        {
            break;
        }

        ath10k_CopyMACAddr(addr.ether_addr_octet, getStaInfo->stainfo[i].stamac);
        getStaInfo->numSta++;
        getStaInfo->stainfo[i].isbtm = isBTMSupported;
        getStaInfo->stainfo[i].isrrm = isRRMSupported;
        getStaInfo->stainfo[i].phy_cap_valid = 1;
        getStaInfo->stainfo[i].datarateInfo.max_chwidth = phyCapInfo.max_chwidth;
        getStaInfo->stainfo[i].datarateInfo.phymode = phyCapInfo.phymode;
        getStaInfo->stainfo[i].datarateInfo.num_streams = phyCapInfo.num_streams;
        getStaInfo->stainfo[i].datarateInfo.max_MCS = phyCapInfo.max_MCS;
        getStaInfo->stainfo[i].datarateInfo.max_txpower = phyCapInfo.max_txpower;
        getStaInfo->stainfo[i].datarateInfo.is_mu_mimo_supported = phyCapInfo.is_mu_mimo_supported;
        getStaInfo->stainfo[i].datarateInfo.is_static_smps = phyCapInfo.is_static_smps;
        getStaInfo->stainfo[i].operating_band = ( 1 << band);
//             Get next STA record
        i++;
      }while(loop_iter != 1);
    TRACE_EXIT();
     return 0;
}

/**
 * @brief Function to get Station statistics
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] destAddr  sta mac for which stats is requested
 * @param [in] stats      pointer to stats, to be filled by this function
 *
 * @return Success: 0, Failure: -1
 *
 */
int getStaStats_ath10k(const void * ctx , const char *ifname,
       const u_int8_t *staAddr, struct soncmn_staStatsSnapshot_t *stats)
{
    char command[50] = "STA ";
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    char *macaddr = NULL;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];
    macaddr = (char *)malloc(20*sizeof(char));

    TRACE_ENTRY();
    snprintf(command,50,"STA %02x:%02x:%02x:%02x:%02x:%02x",staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5]);
    snprintf(macaddr,20,"%02x:%02x:%02x:%02x:%02x:%02x",staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5]);
    if(strcmp(macaddr,"00:00:00:00:00:00")!= 0){
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
            dbgf(soncmnDbgS.dbgModule, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
        }
        else
            goto err;
        command_reply[recev] = '\0';
        tmp = command_reply;
        substring = strtok_r(tmp,"\n" ,&tmp);
        while (substring) {
            strlcpy(substring_array, substring,sizeof(substring_array));
            tmp_substring_array = substring_array;
            if (strcmp(tmp_substring_array,macaddr) != 0){
                parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                while ( parameter_value){
                    if (strncmp (parameter_value,"rx_bytes",8) == 0) {
                        char *rx_bytes;
                        rx_bytes = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        stats->rxBytes = atoi(rx_bytes);
                    }
                    else if (strncmp (parameter_value,"tx_bytes",8) == 0) {
                        char *tx_bytes;
                        tx_bytes = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        stats->txBytes = atoi(tx_bytes);
                    }
                    else if (strncmp (parameter_value,"rx_rate_info",12) == 0) {
                        char *last_rx_rate;
                        last_rx_rate = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                        stats->lastRxRate = (atoi(last_rx_rate))*100;
                    }
                    else if (strncmp (parameter_value,"tx_rate_info",12) == 0) {
                        char *last_tx_rate;
                        last_tx_rate = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                        stats->lastTxRate = (atoi(last_tx_rate))*100;
                    }
                    parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
                }
            }
            substring = strtok_r(NULL, "\n",&tmp);
        }
        free(macaddr);
    }
    TRACE_EXIT();
    return 0;
err:
   TRACE_EXIT_ERR();
   free(macaddr);
   return sd;
}


/**
 * @brief Function to set ACL Policy
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] aclCmd    Access Control List policy command
 *
 * @return Success: 0, Failure: -1
 *
 */
int setAclPolicy_ath10k(const void * context, const char *ifname, int aclCmd)
{
    return 0;
}

/**
 * @brief Function to add or delete MAC from ACL Policy filtering list
 *
 * @param [in] context    opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 * @param [in] staAddr    sta mac which needs to be added or deleted
 *                        from mac filtering list
 * @param [in] isAdd      flag, 1: Add this mac, 0: Delete this mac
 *
 * @return Success: 0, Failure: -1
 *
 */
int setMacFiltering_ath10k (const void *context, const char* ifname,
                           const u_int8_t* staAddr, u_int8_t isAdd)
{
  char command[50];
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    if (isAdd){
        snprintf(command,50,"BLACKLIST_ADD %02x:%02x:%02x:%02x:%02x:%02x",staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5]);
    }
    else {
        snprintf(command,50,"BLACKLIST_DEL %02x:%02x:%02x:%02x:%02x:%02x",staAddr[0],staAddr[1],staAddr[2],staAddr[3],staAddr[4],staAddr[5]);
    }

    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;

    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,
                                                        command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
        goto err;
    command_reply[recev] = '\0';
    TRACE_EXIT();
    return SONCMN_OK;
err:
    TRACE_EXIT_ERR();
    return sd;
}

/**
 * @brief Function to disaasoc a connected client by sending disassoc MLME frame
 *
 * @param [in] context   opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] staAddr   sta mac for which disaaosc frame needs to be sent
 *
 * @return Success: 0, Failure: -1
 *
 */
int performKickMacDisassoc_ath10k (const void *context,
                    const char* ifname, const u_int8_t* staAddr)
{
    char command[50];
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);
    struct priv_ath10k *privAth10k;
    privAth10k = &priv_ath10k_ifname[i];

    TRACE_ENTRY();
    snprintf(command,50,"DEAUTHENTICATE %02x:%02x:%02x:%02x:%02x:%02x",(unsigned int)staAddr[0],(unsigned int)staAddr[1],(unsigned int)staAddr[2],(unsigned int)staAddr[3],(unsigned int)staAddr[4],(unsigned int)staAddr[5]);

    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;

    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,
                                                        command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    else
         goto err;
    command_reply[recev] = '\0';
    TRACE_EXIT();
    return SONCMN_OK;
err:
    TRACE_EXIT_ERR();
    return sd;
}


/**
 * @brief Function to get channel width
 *
 * @param [in] ctxt      opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] chwidth   channel width pointer, to be filled by this function
 *
 * @return Success: 0, Failure: -1
 *
 */
int getChannelWidth_ath10k (const void *ctx, const char * ifname, int * chwidth)
{
    return 0;
}

/**
 * @brief Function to get operating mode
 *
 * @param [in] ctx       opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] opMode    operating Mode pointer, to be filled by this function
 *
 * @return Success: 0, Failure: -1
 *
 */
int getOperatingMode_ath10k (const void *ctx, const char * ifname, int *opMode)
{
    return 0;
}

/**
 * @brief Function to get channel offset
 *
 * @param [in] ctx       opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 *
 * @param [out] choffset pointer to be filled by this function
 *
 * @return Success: 0, Failure: -1
 *
 */
int getChannelExtOffset_ath10k (const void *ctx, const char *ifname, int *choffset)
{
    return 0;
}

/**
 * @brief Function to get center frequency for 160MHz bandwidth channel
 *
 * @param [in] ctx       opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] channum   channel number for which center freq is needed
 *
 * @param [out] cfreq    Center freq pointer, to be filled by this function
 *
 * @return Success: 0, Failure: -1
 *
 */
int getCenterFreq160_ath10k(const void *ctx,
             const char *ifname, u_int8_t channum, u_int8_t *cfreq)
{
    return 0;
}

/**
 * @brief Function to get center frequency segment 1 for VHT 80_80 operation
 *
 * @param [in] ctx        opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 *
 * @param [out] cFreqSeg1 pointer to be filled by this function with center freq
 *
 * @return Success: 0, Failure: -1
 *
 */
int getCenterFreqSeg1_ath10k (const void *ctx, const char *ifname, int32_t *cFreqSeg1)
{
    return 0;
}

/**
 * @brief Function to get center frequency segment 0 for VHT operation
 *
 * @param [in] ctx        opaque pointer to private vendor struct
 * @param [in] ifname     interface name
 *
 * @param [out] cFreqSeg0 pointer to be filled by this function with center freq
 *
 * @return Success: 0, Failure: -1
 *
 */
int getCenterFreqSeg0_ath10k(const void *ctx, const char *ifname, u_int8_t channum, u_int8_t *cFreqSeg0)
{
    return 0;
}

/**
 * @brief Function to dump ATF Table for a single interface.
 *
 * @param [in] ctx       opaque pointer to private vendor struct
 * @param [in] ifname    interface name
 * @param [in] atfInfo   data pointer for ATF Info
 * @param [in] len       length of ATF data
 *
 * @return Success: 0, Failure: -1
 *
 */
int dumpAtfTable_ath10k(const void *ctx, const char *ifname, void *atfInfo, int32_t len)
{
    return 0;
}

void bSteerControlCmnChanUtilTimeoutHandler_ath10k(void *cookie)
{
    int chanUtil=0;
    struct priv_ath10k *privAth10k=( struct priv_ath10k *) cookie;

    TRACE_ENTRY();
    ath_netlink_bsteering_event_t *event = calloc(1, sizeof(ath_netlink_bsteering_event_t));
    chanUtil=getChannelUtilization_ath10k(privAth10k);
    wlanifBSteerEventsHandle_t lbdEvHandle = ((wlanifBSteerEventsHandle_t)(soncmnGetLbdEventHandler()));
    chanUtil=(chanUtil*100)/255;
    event->sys_index = if_nametoindex(privAth10k->ifname);
    getChanUtilInd_ath10k(event, chanUtil);
    wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    free(event);
    evloopTimeoutRegister(&privAth10k->chanUtil, privAth10k->ChanUtilAvg, 0);
    TRACE_EXIT();
}

void inactStaStatsTimeoutHandler_ath10k(void *cookie) {

    static const struct ether_addr tempAddr = {{0,0,0,0,0,0}};
    size_t i;
    TRACE_ENTRY();
    for (i = 0; i < noOfInterfaces; i++) {
                struct inactStaStats *temp=NULL;
                struct priv_ath10k *privAth10k=&priv_ath10k_ifname[i];
                getInactStaStats_ath10k(privAth10k);
                temp=privAth10k->head;
                wlanifBSteerEventsHandle_t lbdEvHandle = ((wlanifBSteerEventsHandle_t)(soncmnGetLbdEventHandler()));
                while(temp)
                {
                        if(!(lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, temp->macaddr)))
                  {
                        ath_netlink_bsteering_event_t *event=calloc(1, sizeof(ath_netlink_bsteering_event_t));
                        event->sys_index = if_nametoindex(privAth10k->ifname);
                        event->type = ATH_EVENT_BSTEERING_STA_STATS;
                        event->data.bs_sta_stats.peer_count=1;
                        event->data.bs_sta_stats.peer_stats[0].rssi=temp->rssi;
                        event->data.bs_sta_stats.peer_stats[0].tx_byte_count=temp->tx_bytes;
                        event->data.bs_sta_stats.peer_stats[0].rx_byte_count=temp->rx_bytes;
                        event->data.bs_sta_stats.peer_stats[0].tx_packet_count=temp->tx_packets;
                        event->data.bs_sta_stats.peer_stats[0].rx_packet_count=temp->rx_packets;
                        event->data.bs_sta_stats.peer_stats[0].tx_rate=temp->tx_rate;
                        lbCopyMACAddr(temp->macaddr,event->data.bs_sta_stats.peer_stats[0].client_addr);
                        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
                        free(event);
                    if((temp->flag & 2) && (temp->activity == 0))
                    {
                        ath_netlink_bsteering_event_t *ievent=calloc(1, sizeof(ath_netlink_bsteering_event_t));
                        ievent->sys_index = if_nametoindex(privAth10k->ifname);
                        ievent->type = ATH_EVENT_BSTEERING_CLIENT_ACTIVITY_CHANGE;
                        lbCopyMACAddr(temp->macaddr,ievent->data.bs_activity_change.client_addr);
                        ievent->data.bs_activity_change.activity=1;
                        temp->activity=1;
                        wlanifBSteerEventsMsgRx(lbdEvHandle,ievent);
                        temp->flag=temp->flag^2;
                        free(ievent);
                    }
                    if ((temp->flag & 4) && (temp->activity == 1)) {
                        ath_netlink_bsteering_event_t *ievent=calloc(1, sizeof(ath_netlink_bsteering_event_t));
                        ievent->sys_index = if_nametoindex(privAth10k->ifname);
                        ievent->type = ATH_EVENT_BSTEERING_CLIENT_ACTIVITY_CHANGE;
                        lbCopyMACAddr(temp->macaddr,ievent->data.bs_activity_change.client_addr);
                        ievent->data.bs_activity_change.activity=0;
                        temp->activity=0;
                        wlanifBSteerEventsMsgRx(lbdEvHandle,ievent);
                        temp->flag=temp->flag^4;
                        free(ievent);
                    }
                        }
                    temp=temp->next;
                }
    }
    evloopTimeoutRegister(&timerHandle.inactStaStatsTimeout, INACT_CHECK_PERIOD, 0);
    TRACE_EXIT();
}

void txPowerTimeoutHandler_ath10k(void *cookie) {
    size_t i;
    int txpower;
    TRACE_ENTRY();
    for (i = 0; i < noOfInterfaces; ++i) {
                txpower=netlink_ath10k(priv_ath10k_ifname[i].ifname);
                txpower=txpower/100;
                wlanifBSteerEventsHandle_t lbdEvHandler = (wlanifBSteerEventsHandle_t)soncmnGetLbdEventHandler();
                if(priv_ath10k_ifname[i].txpower==0)
                    priv_ath10k_ifname[i].txpower=txpower;
                else if(priv_ath10k_ifname[i].txpower!=txpower)
                {
                    ath_netlink_bsteering_event_t *event=calloc(1, sizeof(ath_netlink_bsteering_event_t));
                    event->sys_index = if_nametoindex(priv_ath10k_ifname[i].ifname);
                    event->type = ATH_EVENT_BSTEERING_TX_POWER_CHANGE;
                    event->data.bs_tx_power_change.tx_power=txpower;
                    wlanifBSteerEventsMsgRx(lbdEvHandler,event);
                    priv_ath10k_ifname[i].txpower=txpower;
                    free(event);
                }
    }
    evloopTimeoutRegister(&timerHandle.txPowerTimeout, TX_POWER_CHANGE_PERIOD, 0);
    TRACE_EXIT();
}

/*============================================================
================= Init Functions =============================
============================================================== */

/**
 * @brief  Vendor's Init function, creates sockets for
 *          driver communication, event handlers etc.
 *
 * @param [in] drvhandle_ath10k   driver Handle
 *
 * @return Success: 0, Failure: -1
 *
 */
int sonInit_ath10k(struct soncmnDrvHandle *drvhandle_ath10k, char* ifname)
{
    u_int8_t i;
    i = soncmnGetIfIndex(ifname);

    static char command[120], reply[1028], event[1028];
    int sk, command_len, reply_len, rcv, snd, bufferSize;
    char dest_path[60] = "/var/run/hostapd";
    char sun_path[60] = "/var/run/ctrl_1_";
    const int trueFlag = 1;

    struct sockaddr_un local, dest;
    TRACE_ENTRY();
    priv_ath10k_ifname[i].ifname = (char *)calloc(1,sizeof(char));
    strlcpy(priv_ath10k_ifname[i].ifname, ifname,strlen(ifname)+1);
    priv_ath10k_ifname[i].head = (struct inactStaStats *)calloc(1,sizeof(struct inactStaStats));
    priv_ath10k_ifname[i].state = calloc(1, sizeof(struct bSteerEventsPriv_t_ath10k));
    dbgf(soncmnDbgS.dbgModule,DBGINFO, "For %d th time the address of priv_ath10k_ifname[%d] is %p ifname:%s  priv_ath10k_ifname[i].ifname:%s \n",
                                         i,i,&priv_ath10k_ifname[i],ifname,priv_ath10k_ifname[i].ifname);

    sk = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sk < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket creation %d\n",__func__, errno);
        goto out;
    }

    local.sun_family = AF_UNIX;
    strcat(sun_path,ifname);
    strcpy(local.sun_path, sun_path);
    if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &trueFlag, sizeof(int)) < 0)
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s Failure \n",__func__);
    if (setsockopt(sk, SOL_SOCKET, SO_REUSEPORT, &trueFlag, sizeof(int)) < 0)
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s Failure \n",__func__);
    snd = bind(sk, (struct sockaddr *)&local, sizeof(local));
    if (snd < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket bind %d\n",__func__, errno);
        goto close_and_out;
    }

    strcat(dest_path,"/");
    strcat(dest_path,ifname);
    strcpy(dest.sun_path, dest_path);
    dest.sun_family = AF_UNIX;
    rcv = connect(sk, (struct sockaddr *)&dest, sizeof(dest));
    if (rcv < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket connect %d\n",__func__, errno);
        goto close_and_out;
    }

    memcpy(command, "ATTACH", sizeof("ATTACH"));
    command_len = strlen(command);
    snd = send(sk, command, command_len, 0);
    if (snd < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket send %d\n",__func__, errno);
        goto close_and_out;
    }
    command[command_len] = '\0';

    reply_len = sizeof(reply) - 1;

    rcv = recv(sk, reply, reply_len, 0);
    if (rcv < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket receive %d\n",__func__, errno);
        goto close_and_out;
    }
    reply[rcv] = '\0';
    priv_ath10k_ifname[i].sock = sk;
    noOfInterfaces++;
    bufferSize = sizeof(event);
    bufrdCreate(&priv_ath10k_ifname[i].state->readBuf, "HostapdEvents-rd",
            sk, bufferSize,
            bufferRead_ath10k,  &priv_ath10k_ifname[i]);
    rssiXingThreshold_ath10k(&priv_ath10k_ifname[i]);
    rateXingThreshold_ath10k(&priv_ath10k_ifname[i]);
    setBssUpdatePeriod_ath10k(&priv_ath10k_ifname[i], (utilization_sample_period*10));
    setChanUtilAvgPeriod_ath10k(&priv_ath10k_ifname[i], (utilization_sample_period * utilization_average_num_samples*10));
    priv_ath10k_ifname[i].ChanUtilAvg=utilization_sample_period * utilization_average_num_samples;
    evloopTimeoutCreate(&priv_ath10k_ifname[i].chanUtil, "ChanUtil", bSteerControlCmnChanUtilTimeoutHandler_ath10k, &priv_ath10k_ifname[i]);
    evloopTimeoutRegister(&priv_ath10k_ifname[i].chanUtil,(utilization_sample_period * utilization_average_num_samples) , 0);
    chanutilEventEnable_ath10k(&priv_ath10k_ifname[i]);
    probeEventEnable_ath10k(&priv_ath10k_ifname[i],1);
    static int j = 0;
    if (j == 0)
    {
        evloopTimeoutCreate(&timerHandle.inactStaStatsTimeout, "InactStaStatsTimeout",
                inactStaStatsTimeoutHandler_ath10k, &timerHandle);
        evloopTimeoutRegister(&timerHandle.inactStaStatsTimeout, INACT_CHECK_PERIOD, 0);
        evloopTimeoutCreate(&timerHandle.txPowerTimeout, "TxPowerTimeout",
                txPowerTimeoutHandler_ath10k, &timerHandle);
        evloopTimeoutRegister(&timerHandle.txPowerTimeout, TX_POWER_CHANGE_PERIOD, 0);
        j++;
    }
    dbgf(soncmnDbgS.dbgModule, DBGINFO, "%s: Successful init from Vendor ATH10k",__func__);
    TRACE_EXIT();
    return 0;
close_and_out:
    close(sk);

out:
    unlink(sun_path);
    TRACE_EXIT_ERR();
    return 0;
}


/**
 * @brief  Vendor's De-init function to free up allocated memory
 *
 * @param [in] drvhandle_ath10k   driver Handle
 *
 * @return Success: 0, Failure: -1
 *
 */
void sonDeinit_ath10k(struct soncmnDrvHandle *drvhandle_ath10k)
{
    char sun_path[60] = "/var/run/ctrl_1_";
    u_int8_t i;

    TRACE_ENTRY();
    for (i = 0; i < noOfInterfaces; i++) {
        strcat(sun_path,priv_ath10k_ifname[i].ifname);
        unlink(sun_path);

        bufrdDestroy(&priv_ath10k_ifname[i].state->readBuf);
        // Cancel the timers started in Init in case if running.
        evloopTimeoutUnregister(&priv_ath10k_ifname[i].chanUtil);
        //Free calloc done in Init

        free(priv_ath10k_ifname[i].head);
        free(priv_ath10k_ifname[i].state);
        free(priv_ath10k_ifname[i].ifname);

        //close(sock);
        close(priv_ath10k_ifname[i].sock);
    }
    // Cancel the timers that are started in Init in case if running.
    evloopTimeoutUnregister(&timerHandle.inactStaStatsTimeout);
    evloopTimeoutUnregister(&timerHandle.txPowerTimeout);
    TRACE_EXIT();
}


/**
 * @brief  Function to map vendor APIs to soncmnOPs
 *
 */
struct soncmnOps * sonGetOps_ath10k()
{

    return &ath10k_ops;
}

/**
 * @brief  Function to enable Band Steering in driver via Netlink message
 *
 * @param [in] sysIndex   sys index for interface
 *
 * @return Success: 0, Failure: -1
 *
 */
int enableBSteerEvents_ath10k(u_int32_t sysIndex)
{
    return 0;
}

 /**
  * @brief send command to hostapd through socket
  *
  * @param [in] sock hostapd socket
  * @param [in] command command to be sent
  * @param [in] command_len command length
  * @param [out] command_reply reply received for the command sent
  * @param [in] reply_len length of the reply received
  * @param [out] rcv number of character received
  *
  * @return 1 on successful; otherwise -1
  *
  */
int sendCommand_ath10k( int sock, char *command,int command_len,char *command_reply,int reply_len, int *rcv,struct priv_ath10k *privAth10k)
{
    int snd;
    char msg[1028];
    struct timespec sendts = {0};
    struct timespec receivets = {0};
    struct timespec diff;
    lbGetTimestamp(&sendts);

    snd = send(sock, command, command_len, 0);
    if (snd < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket send %d\n",__func__, errno);
        return -1;
    }
receive:
    *rcv = recv(sock, command_reply, reply_len, 0);
    lbGetTimestamp(&receivets);
    lbTimeDiff(&receivets, &sendts, &diff);

    if (*rcv < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s socket receive %d\n",__func__, errno);
        return -1;
    }
    if (str_starts_ath10k(command_reply, START)) {
        if (privAth10k != NULL)
        {
            if (lbIsTimeAfter(&diff, &MAX_WAIT_TIME)) {
                dbgf(soncmnDbgS.dbgModule,DBGINFO, "Time diff is %lld \n",(long long)diff.tv_sec);
                return -1;
            }

            strlcpy(msg,command_reply,*rcv+1);
            dbgf(soncmnDbgS.dbgModule,DBGDEBUG,"Cmd_send is %s and Event rcved in cmd reply  %s and rcv_leng is %d \n",command,msg,*rcv);
            parseMsg_ath10k(msg,privAth10k);
            goto receive;
        }
    }

    return 1;
}



static int callback(struct nl_msg *msg, void *arg) {
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

    TRACE_ENTRY();

    // Looks like this parses `msg` into the `tb_msg` array with pointers.
    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    // Print everything.
    if (tb_msg[NL80211_ATTR_IFNAME]) {
        dbgf(soncmnDbgS.dbgModule, DBGINFO,"Interface %s\n", nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
    } else {
        dbgf(soncmnDbgS.dbgModule, DBGERR,"Unnamed/non-netdev interface\n");
    }
    if (tb_msg[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]) {
        uint32_t txp = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]);

        dbgf(soncmnDbgS.dbgModule, DBGINFO,"\ttxpower %d.%.2d dBm\n",
                 txp / 100, txp % 100);
        *(int *)arg=txp;
    }
    TRACE_EXIT();
    return NL_SKIP;
}

int netlink_ath10k(const char *ifname)
{
    struct nl_msg *msg;
    int ret,txpower;

    TRACE_ENTRY();
    // Open socket to kernel.
    struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
    if (!socket) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,":%s Error! Failed to allocate socket  \n",__func__);
        goto error;
    }
    if (genl_connect(socket) < 0) { // Create file descriptor and bind socket.
        dbgf(soncmnDbgS.dbgModule, DBGERR,":%s Error! Failed to connect to Generic Netlink socket \n",__func__);
        goto error;
    }

    int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.
    if (driver_id < 0) {
        dbgf(soncmnDbgS.dbgModule, DBGERR,":%s Error! genl resolve failed \n",__func__);
        goto error;
    }
    // First we'll get info for wlan0.
    dbgf(soncmnDbgS.dbgModule,DBGINFO,":%s>>> Getting info for %s:\n",__func__,ifname);
    nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, callback, &txpower);
    msg = nlmsg_alloc();  // Allocate a message.
    int if_index = if_nametoindex(ifname);
    genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_GET_INTERFACE, 0);  // Setup the message.
    NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attributes.
    ret = nl_send_auto_complete(socket, msg);  // Send the message.
    dbgf(soncmnDbgS.dbgModule, DBGINFO,"nl_send_auto_complete returned %d\n", ret);
    nl_recvmsgs_default(socket);  // Retrieve the kernel's answer.
    nl_wait_for_ack(socket);
    TRACE_EXIT();
    return txpower;

error:
    dbgf(soncmnDbgS.dbgModule, DBGINFO,":%s >>> Program exit. txp %d\n",__func__,txpower);
    nl_close(socket);
    nl_socket_free(socket);
    TRACE_EXIT_ERR();
    return -1;

    // Goto statement required by NLA_PUT_U32().
nla_put_failure:
    nlmsg_free(msg);
    TRACE_EXIT_ERR();
    return 1;
}
