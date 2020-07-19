/*
 * @File: wlanifCommandParser.c
 *
 * @Abstract: Load balancing daemon communication with hostapd via socket
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * @@-COPYRIGHT-END-@@
 *
 */
#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "lb_common.h"
#include "wlanifSocket.h"
#include "wlanifCommandParser.h"
#include <string.h>
#include "wlanifBSteerEvents.h"

/**
  * @brief get channel width info based on driver inputs
  *
  * @param [in] secondary_channel  secondary channel
  * @param [in] ieee80211n  ieee80211n info
  * @param [in] ieee80211ac  ieee80211ac info
  * @param [in] vht_oper_chwidth  vht operation channel width info
  *
  * @return wlanif_chwidth_e enum value
  */
wlanif_chwidth_e getChannelWidth(int secondary_channel,int ieee80211n, int ieee80211ac, int vht_oper_chwidth)
{
    int ht, vht;
#define VHT_CHANWIDTH_USE_HT 0
#define VHT_CHANWIDTH_80MHZ 1
#define VHT_CHANWIDTH_160MHZ 2
#define VHT_CHANWIDTH_80P80MHZ 3
    if (ieee80211n)
        ht=1;
    if (ieee80211ac)
        vht = 1;
    if (!ht && !vht)
        return wlanif_chwidth_20;
    if (!secondary_channel)
        return wlanif_chwidth_20;
    if (!vht || vht_oper_chwidth == VHT_CHANWIDTH_USE_HT)
        return wlanif_chwidth_40;
    if (vht_oper_chwidth == VHT_CHANWIDTH_80MHZ)
        return wlanif_chwidth_80;
    if (vht_oper_chwidth == VHT_CHANWIDTH_160MHZ)
        return wlanif_chwidth_160;
    if (vht_oper_chwidth == VHT_CHANWIDTH_80P80MHZ)
        return wlanif_chwidth_80p80;
    return wlanif_chwidth_20;

}

/**
  * @brief get Phy mode from driver inputs
  *
  * @param [in] ieee80211n ieee80211n info
  * @param [in] ieee80211ac ieee80211n info
  * @param [in] freq frequency
  *
  * @return wlanif_phymode_e enum value
  *
  */
wlanif_phymode_e getPhyMode(int ieee80211n, int ieee80211ac, int freq)
{
    if (ieee80211ac)
        return wlanif_phymode_vht;
    else if (ieee80211n && freq>5000)
        return wlanif_phymode_ht;
    else if (ieee80211n)
        return wlanif_phymode_ht;
    else if (freq>5000)
        return  wlanif_phymode_ht;
    else
        return wlanif_phymode_basic;
}

/**
  * @brief get mac address removing ':'
  *
  * @param [in] macaddr macaddress with ':'
  * @param [out] client_addr array where updated macaddress is saved
  */
void wlanif_getMacaddr(char * macaddr,u_int8_t *client_addr)
{
    char *tmp_macaddr;
    char *substring_macaddr = NULL;
    char macaddr_array[512][512] = {{0},{0}};
    int m = 0;

    tmp_macaddr = macaddr;
    substring_macaddr = strtok_r(tmp_macaddr,":" ,&tmp_macaddr);
    for(m = 0; substring_macaddr != NULL; m++){
        strlcpy(macaddr_array[m], substring_macaddr, sizeof(macaddr_array[m]));
        client_addr[m] = (int)strtol(macaddr_array[m],NULL,16);
        substring_macaddr = strtok_r(NULL,":",&tmp_macaddr);
    }
}

/**
  * @brief get station chnnel width
  *
  * @param [in] ht ht enable/disable
  * @param [in] vht vht enable/disable
  * @param [in] vhtcaps vhtcaps info
  * @param [in] htcaps htcaps info
  *
  * @return width channel width
  *
  */
int wlanif_getStaChannelWidth(LBD_BOOL ht,LBD_BOOL vht,int vhtcaps, int htcaps)
{
#define VHT_CAP_SUPP_CHAN_WIDTH_MASK 0x000c
#define VHT_CAP_SUPP_CHAN_WIDTH_160MHZ 0x0004
#define VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ 0x0008
#define  HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET 0x0002
    wlanif_chwidth_e width;
    if (vht) {
        switch (vhtcaps & VHT_CAP_SUPP_CHAN_WIDTH_MASK) {
            case VHT_CAP_SUPP_CHAN_WIDTH_160MHZ:
                width = wlanif_chwidth_160;
                break;
            case VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ:
                width = wlanif_chwidth_80p80;
                break;
            default:
                width = wlanif_chwidth_80;
        }
    } else if (ht) {
        width = htcaps &
            HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET ? wlanif_chwidth_40 : wlanif_chwidth_20;
    } else {
        width = wlanif_chwidth_20;
    }
    return width;

}

/**
  * @brief get station Phy mode
  *
  * @param [in] freq operating frequency
  * @param [in] ht ht enable/disable
  * @param [in] vht vht enable/disable
  *
  * @return phy_mode phy mode
  *
  */
int wlanif_getStaPhyMode(int freq,LBD_BOOL ht,LBD_BOOL vht)
{
    wlanif_phymode_e phy_mode;
    if (freq>5000) {
        if(vht)
            phy_mode = wlanif_phymode_vht;
        else if (ht)
            phy_mode = wlanif_phymode_ht;
        else
            phy_mode = wlanif_phymode_ht;
    } else if (ht)
        phy_mode = wlanif_phymode_ht;
    else
        phy_mode = wlanif_phymode_basic;
    return phy_mode;
}

int mcsbitmasktomcs(int *mcsarray)
{
#define SUPP_RATES_MAX 32
#define SUPP_HT_RATES_MAX 77
    int rates[SUPP_RATES_MAX];
    int i;
    int j=0;

    for (i=0; i < SUPP_HT_RATES_MAX; i++) {
        if (mcsarray[i/8] & (1<<(i%8))) {
            rates[j++] = i;
        }
        if (j == SUPP_RATES_MAX) {
            break;
        }
    }
    if (j <= SUPP_RATES_MAX)
        return rates[j - 1];

    return 0;
}

int txrxvhtmaxmcs(int rx_vht_mcs_map,int tx_vht_mcs_map)
{
    int rx_max_mcs, tx_max_mcs, max_mcs;

    if (rx_vht_mcs_map && tx_vht_mcs_map) {
        rx_max_mcs = rx_vht_mcs_map & 0x03;
        tx_max_mcs = tx_vht_mcs_map & 0x03;
        max_mcs = rx_max_mcs < tx_max_mcs ? rx_max_mcs : tx_max_mcs;
        if (max_mcs < 0x03)
            return 7 + max_mcs;
    }

    return 0;
}
int maxnssconversion(int *mcs_set, int mcs_map,int ht_supported,int vht_supported )
{
    int i;
    int ht_rx_nss = 0;
    int vht_rx_nss = 1;
    int mcs;

    if (ht_supported && mcs_set != NULL) {
        if (mcs_set[0])
            ht_rx_nss++;
        if (mcs_set[1])
            ht_rx_nss++;
        if (mcs_set[2])
            ht_rx_nss++;
        if (mcs_set[3])
            ht_rx_nss++;
    }
    if (vht_supported) {
        for (i = 7; i >= 0; i--) {
            mcs = (mcs_map >> (2 * i)) & 0x03;
            if (mcs != 0x03) {
                vht_rx_nss = i + 1;
                break;
            }
        }
    }

    return (ht_rx_nss > vht_rx_nss ? ht_rx_nss : vht_rx_nss);
}


/**
  * @brief get station node association Event data
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddress mac address of the station
  * @param [in] freq operating frequency
  * @param [out] event event structure data
  */
void wlanif_getNodeAssocEventData(int sock,char *macaddr, int freq,ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *tmp_flags;
    char command[30] = "STA ";
    char *substring = NULL;
    char *flag_substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char flag_array[512][512] = {{0},{0}};
    char *parameter_value = NULL;
    int i, l = 0 ;
    LBD_BOOL ht = 0,vht = 0;
    int htcaps = 0,vhtcaps = 0,caps = 0;
    long long extcaps = 0;
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    int htmaxmcs = 0,vhtmaxmcs = 0;
    char  *vht_rx_mcs_map = NULL;
    char *vht_tx_mcs_map = NULL;
    int vhtrxmcs = 0,vhttxmcs = 0;
    int mcsarray[10];
    char htmcsarray[10][10];
    int maxnss;


#define IEEE80211_CAPINFO_RADIOMEAS 0x1000
#define HT_CAP_INFO_SMPS_MASK 0x000c
#define HT_CAP_INFO_SMPS_STATIC 0x0000
#define VHT_CAP_MU_BEAMFORMEE_CAPABLE 0x00100000

    if(strcmp(macaddr,"00:00:00:00:00:00") !=0) {
        strcat(command,macaddr);
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = wlanif_sendCommand(sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
            dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"command_send = %s\n", command_send);
        }
        command_reply[recev] = '\0';
        tmp = command_reply;
        substring = strtok_r(tmp,"\n" ,&tmp);
        while (substring) {
            strlcpy(substring_array, substring,sizeof(substring_array));
            tmp_substring_array = substring_array;
            if (strcmp(tmp_substring_array,macaddr) != 0){
                parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                while ( parameter_value){
                    if (strncmp (parameter_value,"flags",5) == 0) {
                        tmp_flags=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        flag_substring = strtok_r(tmp_flags, "[]",&tmp_flags);
                        for(i = 0; flag_substring != NULL; i++){
                            strlcpy(flag_array[i], flag_substring, sizeof(flag_array[i]));
                            flag_substring = strtok_r(NULL,"[]",&tmp_flags);
                        }
                        for(l = 0 ;l<i ;l++)
                        {
                            if (strcmp(flag_array[l],"VHT") == 0)
                                vht = 1;
                            if (strcmp(flag_array[l],"HT") == 0)
                                ht = 1;
                        }
                    }
                    else if (strncmp (parameter_value,"vht_caps_info",13) == 0) {
                        char *vht_cap_info;
                        vht_cap_info = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        vhtcaps = (int)strtol(vht_cap_info, NULL, 16);
                    }
                    else if (strncmp (parameter_value,"ht_caps_info",12) == 0) {
                        char *ht_cap_info;
                        ht_cap_info = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        htcaps = (int)strtol(ht_cap_info, NULL, 16);
                    }
                    else if (strncmp (parameter_value,"capability",10) == 0) {
                        char *capability;
                        capability = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        caps = (int)strtol(capability, NULL, 16);
                    }
                    else if (strncmp (parameter_value,"ext_capab",9) == 0) {
                        char *excapability;
                        char *exttemp = malloc(sizeof(char) *5);
                        excapability = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        excapability = excapability+4;
                        strlcpy(exttemp,excapability,3);
                        extcaps = strtoull(exttemp, NULL, 16);
                    }
                    else if (strncmp (parameter_value,"max_txpower",11) == 0) {
                        char *max_txpower;
                        max_txpower = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        event->data.bs_node_associated.datarate_info.max_txpower = atoi(max_txpower);
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
            }
            substring = strtok_r(NULL, "\n",&tmp);
        }
        htmaxmcs = mcsbitmasktomcs(mcsarray);

        vhtmaxmcs = txrxvhtmaxmcs(vhtrxmcs,vhttxmcs);
        maxnss = maxnssconversion(mcsarray, vhtrxmcs,ht, vht);
        event->data.bs_node_associated.datarate_info.num_streams = maxnss;

        event->type = ATH_EVENT_BSTEERING_NODE_ASSOCIATED;
        wlanif_getMacaddr(macaddr,client_addr);
        memcpy(event->data.bs_node_associated.client_addr,client_addr,IEEE80211_ADDR_LEN);
        event->data.bs_node_associated.isBTMSupported = !(!(extcaps & IEEE80211_EXTCAPIE_BSSTRANSITION));
        event->data.bs_node_associated.isRRMSupported = !(!(caps & IEEE80211_CAPINFO_RADIOMEAS));
        event->data.bs_node_associated.datarate_info.max_chwidth = wlanif_getStaChannelWidth(ht,vht,vhtcaps,htcaps);
        event->data.bs_node_associated.datarate_info.phymode = wlanif_getStaPhyMode(freq,ht,vht);

        if (vht == 0)
            event->data.bs_node_associated.datarate_info.max_MCS = htmaxmcs%8;
        else
            event->data.bs_node_associated.datarate_info.max_MCS = vhtmaxmcs;

        if(ht){
            event->data.bs_node_associated.datarate_info.is_static_smps = (LBD_BOOL)((htcaps & HT_CAP_INFO_SMPS_MASK) == HT_CAP_INFO_SMPS_STATIC);
        }
        if (vht) {
            event->data.bs_node_associated.datarate_info.is_mu_mimo_supported = (LBD_BOOL) vhtcaps & VHT_CAP_MU_BEAMFORMEE_CAPABLE;
        }
    }
}

/**
  * @brief update disassoc event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  */
void wlanif_getNodeDisassocEventData(char *macaddr, ath_netlink_bsteering_event_t *event)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    memcpy(event->data.bs_node_disassociated.client_addr,client_addr,IEEE80211_ADDR_LEN);
}

/**
  * @brief update probe request event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  * @param [in] rssi station rssi value
  */
void wlanif_getProbeReqInd(char *macaddr,ath_netlink_bsteering_event_t *event,int rssi)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    event->type = ATH_EVENT_BSTEERING_PROBE_REQ;
    memcpy(event->data.bs_probe.sender_addr,client_addr,IEEE80211_ADDR_LEN);
    event->data.bs_probe.rssi = rssi;
}

/**
  * @brief update opmode change event  data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure info
  * @param [in] value opmode value
  * @param [in] isbandwidth isbandwidth enable/disable
 */
void wlanif_getOpModeChangeInd(struct wlanifBSteerControlVapInfo *vap,char *macaddr,ath_netlink_bsteering_event_t *event,int value , LBD_BOOL isbandwidth)
{
    event->type = ATH_EVENT_BSTEERING_OPMODE_UPDATE;
    if (isbandwidth){
        wlanif_getStanss(vap->sock,macaddr,event,vap);
        event->data.opmode_update.datarate_info.max_chwidth = value;
    }
    else{
        event->data.opmode_update.datarate_info.num_streams = value;
        wlanif_getStachwidth(vap->sock,macaddr,event,vap);
    }
}

/**
  * @brief update smps event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] smpsmode smpsmode ebale/disable
  */
void wlanif_getSmpsChangeInd(char *macaddr,ath_netlink_bsteering_event_t *event,int smpsMode)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    memcpy(event->data.smps_update.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_SMPS_UPDATE;
    event->data.smps_update.is_static = smpsMode;

}

/**
  * @brief update wnm event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] statusCode current status code
  * @param [in] bssTermDelay bss term delay received
  * @param [in] targetBssid target sta mac address
  */
void wlanif_getWnmEventInd(char *macaddr, ath_netlink_bsteering_event_t *event,int statusCode,int bssTermDelay,char *targetBssid)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    u_int8_t Bssid[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    wlanif_getMacaddr(targetBssid,Bssid);
    memcpy(event->data.wnm_event.macaddr,client_addr,IEEE80211_ADDR_LEN);
    event->data.wnm_event.wnm_type = BSTEERING_WNM_TYPE_BSTM_RESPONSE;
    event->type = ATH_EVENT_BSTEERING_WNM_EVENT;
    event->data.wnm_event.data.bstm_resp.status = statusCode;
    event->data.wnm_event.data.bstm_resp.reject_code = IEEE80211_BSTM_REJECT_REASON_INVALID ;
    if (event->data.wnm_event.data.bstm_resp.status != 0){
        event->data.wnm_event.data.bstm_resp.reject_code = statusCode;
    }
    event->data.wnm_event.data.bstm_resp.termination_delay = bssTermDelay;
    memcpy(event->data.wnm_event.data.bstm_resp.target_bssid,Bssid,IEEE80211_ADDR_LEN);

}

/**
  * @brief update tx auth fail event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  */
void wlanif_getTxAuthFailInd(char *macaddr, ath_netlink_bsteering_event_t *event)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    memcpy(event->data.bs_auth.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_TX_AUTH_FAIL;
}

/**
  * @brief update rssi crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rssi sta rssi info
  * @param [in] isrssilow rssi low enable/disable
  */
void wlanif_getRSSICrossingEvent(char *macaddr, ath_netlink_bsteering_event_t *event, int rssi, LBD_BOOL isrssilow)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    memcpy(event->data.bs_rssi_xing.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_CLIENT_RSSI_CROSSING;
    event->data.bs_rssi_xing.rssi = rssi;
    if (isrssilow)
        event->data.bs_rssi_xing.low_rssi_xing  = BSTEERING_XING_DOWN;
    else
        event->data.bs_rssi_xing.low_rssi_xing  = BSTEERING_XING_UP;
}

/**
  * @brief update rate crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rate sta rate info
  * @param [in] isratelow rate low enable/disable
  */
void wlanif_getRateCrossingEvent(char *macaddr, ath_netlink_bsteering_event_t *event, int rate, LBD_BOOL isratelow)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    wlanif_getMacaddr(macaddr,client_addr);
    memcpy(event->data.bs_tx_rate_xing.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_CLIENT_TX_RATE_CROSSING;
    event->data.bs_tx_rate_xing.tx_rate = rate;
    if (isratelow)
        event->data.bs_tx_rate_xing.xing = BSTEERING_XING_DOWN;
    else
        event->data.bs_tx_rate_xing.xing = BSTEERING_XING_UP;
}

/**
  * @brief get channel utilization event data
  *
  * @param [out] event event structure info
  * @param [in] chan_util channel utilization
  */
void wlanif_getChanUtilInd(ath_netlink_bsteering_event_t *event, int chan_util)
{
    event->type = ATH_EVENT_BSTEERING_CHAN_UTIL;
    event->data.bs_chan_util.utilization = chan_util;
}

/**
  * @brief get frequency from VAP
  *
  * @param [in] vap vap structure handle
  *
  * @return freq radio frequency
  *
  */
int wlanif_getVapFreq(struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    int freq = 0;
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"command_send = %s\n", command_send);
    }
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
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    return freq;
}

/**
  * @brief get channel utilization from VAP
  *
  * @param [in] vap vap structure handle
  *
  * @return chanUtil channel utilization
  *
  */
int wlanif_getChannelUtilization(struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    int chanutil = 0;
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"command_send = %s\n", command_send);
    }
    command_reply[recev] = '\0';
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while ( parameter_value){
            if (strncmp (parameter_value,"chan_util_avg",13) == 0) {
                char *util;
                util = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                chanutil = atoi(util);
                break;
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    return chanutil;
}

/**
  * @brief get ssidname from VAP
  *
  * @param [in] vap vap structure handle
  * @param [out] ssidname array to store ssid name
  * @param [out] length length of the ssid
  */
void wlanif_getssidName(struct wlanifBSteerControlVapInfo *vap,u_int8_t *ssidname,u_int32_t *length)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);

    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"COMMAND_SEND = %s\n", command_send);
    }
    command_reply[recev] = '\0';
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while ( parameter_value){
            if (strncmp (parameter_value,"ssid[0]",7) == 0) {
                char *ssid;
                ssid =strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                strlcpy((char *)ssidname,ssid,strlen(ssid)+1);
                *length  = strlen((char*)ssidname);
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    return ;
}

/**
  * @brief enable rrsi threshold
  *
  * @param [in] vap vap structure handle
  */
void wlanif_RssiXingThreshold(struct wlanifBSteerControlVapInfo *vap)
{
    char command[50];
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(vap->radio->channel);
     snprintf(command,50,"SIGNAL_MONITOR THRESHOLD=%d HYSTERESIS=0",(vap->state->bsteerControlHandle->bandInfo[band].configParams.low_rssi_crossing_threshold-95));
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"Commaand reply is %s \n",command_reply);
    return;

}

/**
  * @brief enable rate threshold
  *
  * @param [in] vap vap structure handle
  */
void wlanif_RateXingThreshold(struct wlanifBSteerControlVapInfo *vap)
{
    char command[50];
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    wlanif_band_e band = wlanif_resolveBandFromChannelNumber(vap->radio->channel);
    snprintf(command,90,"SIGNAL_TXRATE LOW=%d HIGH=%d",vap->state->bsteerControlHandle->bandInfo[band].configParams.low_tx_rate_crossing_threshold/100
            ,vap->state->bsteerControlHandle->bandInfo[band].configParams.high_tx_rate_crossing_threshold/100);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    command_reply[recev] = '\0';
    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"Commaand reply is %s \n",command_reply);
    return;

}

/**
  * @brief enable channel utilization
  *
  * @param [in] vap vap structure handle
  */
void wlanif_chanutilEventEnable(struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    memcpy(command_send,"ATTACH bss_load_events=1",sizeof ("ATTACH bss_load_events=1"));
    command_leng = strlen(command_send);

    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    command_reply[recev] = '\0';
    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"Commaand reply is %s \n",command_reply);
    return;
}

/**
  * @brief enable probe events
  *
  * @param [in] vap vap structure handle
  * @param [in] enable events enable/disable
  */
void wlanif_probeEventEnable(struct wlanifBSteerControlVapInfo *vap,int enable)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    if(enable)
        memcpy(command_send,"ATTACH probe_rx_events=1",sizeof ("ATTACH probe_rx_events=1"));
    else
        memcpy(command_send,"ATTACH probe_rx_events=0",sizeof ("ATTACH probe_rx_events=0"));
    command_leng = strlen(command_send);

    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"COMMAND_SEND in probe events is = %s enable %d \n", command_send,enable);
    }
    command_reply[recev] = '\0';
    return;
}

/**
  * @brief update BSS load update period
  *
  * @param [in] vap vap structure handle
  * @param [in] update_period period to be updated
  */
void wlanif_setBssUpdatePeriod(struct wlanifBSteerControlVapInfo *vap,int update_period)
{
    char command[50];
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    snprintf(command,50,"SET bss_load_update_period %d",update_period);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"%s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    return;

}

/**
  * @brief set average utilization period
  *
  * @param [in] vap vap structure handle
  * @param [in] avg_period average period to be updated
  */
void wlanif_setChanUtilAvgPeriod(struct wlanifBSteerControlVapInfo *vap,int avg_period)
{
    char command[50];
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    snprintf(command,50,"SET chan_util_avg_period %d",avg_period);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGERR,"%s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    return;

}

/**
  * @brief get frequency of the radio when vap sturcture is yet to be initialized
  *
  * @param [in] ifname interface name
  * @param [in] radio_ifname radio interface name
  */
int wlanif_getFreq(const char* ifname,const char* radio_ifname)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    char sun_path[60] = "/var/run/ctrl_";
    int freq = 0,sk;

    sk=wlanif_create_sock(radio_ifname,ifname);
    if (sk<0) {
        dbgf(NULL, DBGERR,"%s Socket creation failed for Freq \n",__func__);
        return 0;
    }
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(sk,command_send,command_leng,command_reply,reply_leng,&recev,NULL);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(NULL, DBGDEBUG,"command_send = %s\n", command_send);
    }
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
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    strcat(sun_path,ifname);
    if (shutdown(sk,SHUT_RDWR) < 0) {
        dbgf(NULL, DBGERR,"%s shutdown failed \n",__func__);
    }
    unlink(sun_path);
    close(sk);
    return freq;

}

/**
  * @brief get acs,cac state
  *
  * @param [in] sock hostapd socket
  * @param [out] acs input to store acs value
  * @param [out] cac input to store cac value
  * @param [in] bssid bssid info
  */
int wlanif_getAcsCacState (int sock, int *acs, int *cac, u_int8_t *bssid,struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    char *macaddr = NULL;
    macaddr = (char *)malloc(20*sizeof(char));

    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
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
                    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"acs = %d \n",*acs);
                }
            }
            else if (strcmp (parameter_value,"cac_time_left_seconds") == 0)
            {
                if (strcmp (strtok_r(tmp_substring_array,"=",&tmp_substring_array),"N/A") != 0) {
                    *cac=1;
                    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"cac = %d \n",*cac);
                }
            }
            else if (strncmp (parameter_value,"bssid",5) == 0) {
                char *tmp_bssid;
                tmp_bssid=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                strlcpy(macaddr,tmp_bssid,sizeof(tmp_bssid));
                wlanif_getMacaddr(macaddr,bssid);
            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    free(macaddr);
    return 0;
}

/**
  * @brief get station data rate info
  *
  * @param [in] vap vap structure handle
  * @param [in] addr mac address info
  * @param [out] phyCapInfo phy info
  * @param [out] isStaticSMPS static smps enabled/disabled
  * @param [out] isMUMIMOSupported MUMIMO enabled/disabled
  * @param [out] isRRMSupported RRM supported/unsupported
  * @param [out] isBTMSupported BTM supported/unsupported
  * @param [in] loop_iter iteration loop
  */
void wlanif_getDataRateSta(struct wlanifBSteerControlVapInfo *vap,struct ether_addr *addr, wlanif_phyCapInfo_t *phyCapInfo, LBD_BOOL *isStaticSMPS,LBD_BOOL *isMUMIMOSupported,LBD_BOOL *isRRMSupported,LBD_BOOL *isBTMSupported,int *loop_iter)
{
    char command[50];
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *tmp_flags;
    char *substring = NULL;
    char *flag_substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *macaddr = NULL;
    char *copied_macaddr = NULL;
    char flag_array[512][512] = {{0},{0}};
    char *parameter_value = NULL;
    int i, l = 0,m = 0 ;
    LBD_BOOL ht = 0,vht = 0;
    int htcaps = 0,vhtcaps = 0,freq;
    macaddr = (char *)malloc(20*sizeof(char));
    copied_macaddr = (char *)malloc(20*sizeof(char));
    static const struct ether_addr tempAddr = {{0,0,0,0,0,0}};
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    int caps = 0;
    long long extcaps = 0;
    int htmaxmcs = 0,vhtmaxmcs = 0;
    char  *vht_rx_mcs_map = NULL;
    char *vht_tx_mcs_map = NULL;
    int vhtrxmcs = 0,vhttxmcs = 0;
    int mcsarray[10];
    char htmcsarray[10][10];
    int maxnss;


#define HT_CAP_INFO_SMPS_STATIC 0x0000
#define VHT_CAP_MU_BEAMFORMEE_CAPABLE 0x00100000
#define HT_CAP_INFO_SMPS_MASK 0x000c
#define IEEE80211_CAPINFO_RADIOMEAS 0x1000

    if (!(lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, addr->ether_addr_octet))) {
        snprintf(command,50,"STA-NEXT %02x:%02x:%02x:%02x:%02x:%02x",addr->ether_addr_octet[0],addr->ether_addr_octet[1],addr->ether_addr_octet[2],addr->ether_addr_octet[3],addr->ether_addr_octet[4],addr->ether_addr_octet[5]);
        snprintf(macaddr,50,"%02x:%02x:%02x:%02x:%02x:%02x",addr->ether_addr_octet[0],addr->ether_addr_octet[1],addr->ether_addr_octet[2],addr->ether_addr_octet[3],addr->ether_addr_octet[4],addr->ether_addr_octet[5]);
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
    }
    else {
        memcpy(command_send,"STA-FIRST",sizeof ("STA-FIRST"));
        command_leng = strlen(command_send);
    }
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);


    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," In %s command_send is %s\n",__func__ ,command_send);
    }
    command_reply[recev] = '\0';
    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s recev is %d Command reply is %s \n",__func__,recev,command_reply);
    if((strcmp(command_reply,"") == 0) || (strncmp(command_reply,"FAIL",4)==0))
    {
        *loop_iter = 1;
        return;
    }
    tmp = command_reply;
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;

        parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
        while ( parameter_value){
            if (m == 0)
            {
                strlcpy(copied_macaddr,parameter_value,18);
                m++;
            }
            else {
                if (strncmp (parameter_value,"flags",5) == 0) {
                    tmp_flags=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    flag_substring = strtok_r(tmp_flags, "[]",&tmp_flags);
                    for(i = 0; flag_substring != NULL; i++){
                        strlcpy(flag_array[i], flag_substring, sizeof(flag_array[i]));
                        flag_substring = strtok_r(NULL,"[]",&tmp_flags);
                    }
                    for(l = 0 ;l<i ;l++)
                    {
                        if (strcmp(flag_array[l],"VHT") == 0)
                            vht = 1;
                        if (strcmp(flag_array[l],"HT") == 0)
                            ht = 1;
                    }

                }
                else if (strncmp (parameter_value,"vht_caps_info",13) == 0) {
                    char *vht_cap_info;
                    vht_cap_info = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    vhtcaps = (int)strtol(vht_cap_info, NULL, 16);
                }
                else if (strncmp (parameter_value,"ht_caps_info",12) == 0) {
                    char *ht_cap_info;
                    ht_cap_info = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    htcaps = (int)strtol(ht_cap_info, NULL, 16);
                }
                else if (strncmp (parameter_value,"capability",10) == 0) {
                    char *capability;
                    capability = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    caps = (int)strtol(capability, NULL, 16);
                }
                else if (strncmp (parameter_value,"ext_capab",9) == 0) {
                    char *excapability;
                    char *exttemp = malloc(sizeof(char) *5);
                    excapability = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    excapability = excapability+4;
                    strlcpy(exttemp,excapability,3);
                    extcaps = strtoull(exttemp, NULL, 16);
                }
                else if (strncmp (parameter_value,"max_txpower",11) == 0) {
                    char *max_txpower;
                    max_txpower = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    phyCapInfo->maxTxPower = atoi(max_txpower);
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

            }
            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }
    htmaxmcs = mcsbitmasktomcs(mcsarray);
    vhtmaxmcs = txrxvhtmaxmcs(vhtrxmcs,vhttxmcs);
    maxnss = maxnssconversion(mcsarray, vhtrxmcs,ht, vht);

    phyCapInfo->numStreams = maxnss;
    wlanif_getMacaddr(copied_macaddr,client_addr);
    lbCopyMACAddr(client_addr,addr->ether_addr_octet);
    freq = wlanif_getVapFreq(vap);
    phyCapInfo->valid = LBD_TRUE;
    phyCapInfo->maxChWidth = wlanif_getStaChannelWidth(ht,vht,vhtcaps,htcaps);
    phyCapInfo->phyMode = wlanif_getStaPhyMode(freq,ht,vht);
    if (vht == 0)
        phyCapInfo->maxMCS = htmaxmcs%8;
    else
        phyCapInfo->maxMCS = vhtmaxmcs;

    if (vht) {
        *isMUMIMOSupported = (LBD_BOOL) vhtcaps & VHT_CAP_MU_BEAMFORMEE_CAPABLE;
    }
    if (ht) {
        *isStaticSMPS = (LBD_BOOL)((htcaps & HT_CAP_INFO_SMPS_MASK) == HT_CAP_INFO_SMPS_STATIC);
    }
    *isBTMSupported =  !(!(extcaps & IEEE80211_EXTCAPIE_BSSTRANSITION));
    *isRRMSupported = !(!(caps & IEEE80211_CAPINFO_RADIOMEAS)) ;
    free(macaddr);
    free(copied_macaddr);
}

/**
  * @brief get data rate from vap
  *
  * @param [in] vap vap structure handle
  */
void wlanif_getDataRate (struct wlanifBSteerControlVapInfo *vap)
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

    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
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
                vap->phyCapInfo.maxTxPower = atoi(max_txpower);
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

    vap->phyCapInfo.numStreams = maxnss;
    vap->phyCapInfo.valid = LBD_TRUE;
    vap->phyCapInfo.maxChWidth = getChannelWidth(secondary_channel, ieee80211n, ieee80211ac, vht_oper_width);
    vap->phyCapInfo.phyMode = getPhyMode(ieee80211n,ieee80211ac,freq);
    if((ieee80211ac == 0) && (freq < 5000)){
        vap->phyCapInfo.maxMCS = htmaxmcs%8;
    }else{
        vap->phyCapInfo.maxMCS = vhtmaxmcs;
    }
}

/**
  * @brief get station status
  *
  * @param [in] sock hostapd socket
  * @param [in] staAddr station mac address
  * @param [out] stats sta stats structure handle
  */
void wlanif_getStaStats(int sock, const struct ether_addr *staAddr, struct ieee80211req_sta_stats *stats,struct wlanifBSteerControlVapInfo *vap)
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
    macaddr = (char *)malloc(20*sizeof(char));

    snprintf(command,50,"STA %02x:%02x:%02x:%02x:%02x:%02x",staAddr->ether_addr_octet[0],staAddr->ether_addr_octet[1],staAddr->ether_addr_octet[2],staAddr->ether_addr_octet[3],staAddr->ether_addr_octet[4],staAddr->ether_addr_octet[5]);
    snprintf(macaddr,20,"%02x:%02x:%02x:%02x:%02x:%02x",staAddr->ether_addr_octet[0],staAddr->ether_addr_octet[1],staAddr->ether_addr_octet[2],staAddr->ether_addr_octet[3],staAddr->ether_addr_octet[4],staAddr->ether_addr_octet[5]);
    if(strcmp(macaddr,"00:00:00:00:00:00")!= 0){
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = wlanif_sendCommand(sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
            dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
        }
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
                        stats->is_stats.ns_rx_bytes = atoi(rx_bytes);
                    }
                    else if (strncmp (parameter_value,"tx_bytes",8) == 0) {
                        char *tx_bytes;
                        tx_bytes = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        stats->is_stats.ns_tx_bytes = atoi(tx_bytes);
                    }
                    else if (strncmp (parameter_value,"rx_rate_info",12) == 0) {
                        char *last_rx_rate;
                        last_rx_rate = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                        stats->is_stats.ns_last_rx_rate = (atoi(last_rx_rate))*100;
                    }
                    else if (strncmp (parameter_value,"tx_rate_info",12) == 0) {
                        char *last_tx_rate;
                        last_tx_rate = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                        stats->is_stats.ns_last_tx_rate = (atoi(last_tx_rate))*100;
                    }
                    parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
                }
            }
            substring = strtok_r(NULL, "\n",&tmp);
        }
        free(macaddr);
    }
}

/**
 * @brief Get the channel width for the Station
 *
 * @param [in] sock Hostapd Socket
 * @param [in] macaddr station mac address
 * @param [in] event event structure info
 * @param [in] vap Vap Structure
 */

void wlanif_getStachwidth(int sock, char *macaddr, ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *tmp_flags;
    char command[30] = "STA ";
    char *substring = NULL;
    char *flag_substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char flag_array[512][512] = {{0},{0}};
    char *parameter_value = NULL;
    int i, l = 0 ;
    LBD_BOOL ht = 0,vht = 0;
    int htcaps = 0,vhtcaps = 0;
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};

    if(strcmp(macaddr,"00:00:00:00:00:00") !=0){
    strcat(command,macaddr);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    command_reply[recev] = '\0';
    tmp = command_reply;
    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s recev is %d Command reply is %s \n",__func__,recev,command_reply);
    substring = strtok_r(tmp,"\n" ,&tmp);
    while (substring) {
        strlcpy(substring_array, substring,sizeof(substring_array));
        tmp_substring_array = substring_array;
        if (strcmp(tmp_substring_array,macaddr) != 0){
            parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
            while ( parameter_value){
                if (strncmp (parameter_value,"flags",5) == 0) {
                    tmp_flags=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    flag_substring = strtok_r(tmp_flags, "[]",&tmp_flags);
                    for(i = 0; flag_substring != NULL; i++){
                        strlcpy(flag_array[i], flag_substring, sizeof(flag_array[i]));
                        flag_substring = strtok_r(NULL,"[]",&tmp_flags);
                    }
                    for(l = 0 ;l<i ;l++)
                    {
                        if (strcmp(flag_array[l],"VHT") == 0)
                            vht = 1;
                        if (strcmp(flag_array[l],"HT") == 0)
                            ht = 1;
                    }
                }
                else if (strncmp (parameter_value,"vht_caps_info",13) == 0) {
                    char *vht_cap_info;
                    vht_cap_info = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    vhtcaps = (int)strtol(vht_cap_info, NULL, 16);
                }
                else if (strncmp (parameter_value,"ht_caps_info",12) == 0) {
                    char *ht_cap_info;
                    ht_cap_info = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    htcaps = (int)strtol(ht_cap_info, NULL, 16);
                }

                parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
            }
        }
        substring = strtok_r(NULL, "\n",&tmp);
    }

    wlanif_getMacaddr(macaddr,client_addr);
    memcpy(event->data.opmode_update.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->data.opmode_update.datarate_info.max_chwidth = wlanif_getStaChannelWidth(ht,vht,vhtcaps,htcaps);
    }
}

/**
 * @brief Get the nss for the given station
 *
 * @param [in] sock Hostapd Socket
 * @param [in] macaddr station mac address
 * @param [in] event event structure info
 * @param [in] vap Vap Structure
 */
void wlanif_getStanss(int sock, char *macaddr, ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *tmp_flags;
    char command[30] = "STA ";
    char *substring = NULL;
    char *flag_substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char flag_array[512][512] = {{0},{0}};
    char *parameter_value = NULL;
    int i, l = 0 ;
    LBD_BOOL ht = 0,vht = 0;
    int htcaps = 0,vhtcaps = 0;
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};

    if(strcmp(macaddr,"00:00:00:00:00:00") !=0){
        strcat(command,macaddr);
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = wlanif_sendCommand(sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
        }
        command_reply[recev] = '\0';
        tmp = command_reply;
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s recev is %d Command reply is %s \n",__func__,recev,command_reply);
        substring = strtok_r(tmp,"\n" ,&tmp);
        while (substring) {
            strlcpy(substring_array, substring,sizeof(substring_array));
            tmp_substring_array = substring_array;
            if (strcmp(tmp_substring_array,macaddr) != 0){
                parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                while ( parameter_value){
                    if (strncmp (parameter_value,"flags",5) == 0) {
                        tmp_flags=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        flag_substring = strtok_r(tmp_flags, "[]",&tmp_flags);
                        for(i = 0; flag_substring != NULL; i++){
                            strlcpy(flag_array[i], flag_substring, sizeof(flag_array[i]));
                            flag_substring = strtok_r(NULL,"[]",&tmp_flags);
                        }
                        for(l = 0 ;l<i ;l++)
                        {
                            if (strcmp(flag_array[l],"VHT") == 0)
                                vht = 1;
                            if (strcmp(flag_array[l],"HT") == 0)
                                ht = 1;
                        }
                    }
                    else if (strncmp (parameter_value,"max_nss",7) == 0) {
                        char *max_nss;
                        max_nss = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                        event->data.opmode_update.datarate_info.num_streams = atoi(max_nss);
                    }
                    parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
                }
            }
            substring = strtok_r(NULL, "\n",&tmp);
        }
        wlanif_getMacaddr(macaddr,client_addr);
        event->data.opmode_update.datarate_info.max_chwidth = wlanif_getStaChannelWidth(ht,vht,vhtcaps,htcaps);
    }
}

/**
 * @brief get station inactivity status for all sta connected to the interface
 *
 * @param [in] vap vap structure handle
 */
void wlanif_getInactStaStats(struct wlanifBSteerControlVapInfo *vap)
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
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    struct ether_addr addr;
    int flag=1,m=0;
    macaddr = (char *)malloc(20*sizeof(char));
    do
    {
        if(flag)
        {
            snprintf(command,50,"STA-FIRST");
            flag=0;
        }
        else
        {
            snprintf(command,50,"STA-NEXT %02x:%02x:%02x:%02x:%02x:%02x",addr.ether_addr_octet[0],addr.ether_addr_octet[1],addr.ether_addr_octet[2],addr.ether_addr_octet[3],addr.ether_addr_octet[4],addr.ether_addr_octet[5]);
        }
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
            dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
        }
        else {
            break;
        }
        command_reply[recev] = '\0';
        m=0;
        if ((recev > 0) && (strncmp(command_reply,"FAIL",4)!= 0) && (strcmp(command_reply,"") != 0))
        {
            struct wlanifInactStaStats temp;
            tmp = command_reply;
            substring = strtok_r(tmp,"\n" ,&tmp);
            while (substring) {
                strlcpy(substring_array, substring,sizeof(substring_array));
                tmp_substring_array = substring_array;
                parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                if (m == 0)
                {
                    strlcpy(macaddr,parameter_value,18);
                    wlanif_getMacaddr(macaddr,client_addr);
                    lbCopyMACAddr(client_addr,addr.ether_addr_octet);
                    lbCopyMACAddr(client_addr,temp.macaddr);
                    m++;
                }
                else
                {
                    if (strcmp(tmp_substring_array,macaddr) != 0){
                        while ( parameter_value){
                            if (strncmp (parameter_value,"rx_bytes",8) == 0) {
                                char *rx_bytes;
                                rx_bytes = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                                temp.rx_bytes = atoi(rx_bytes);
                            }
                            else if (strncmp (parameter_value,"tx_bytes",8) == 0) {
                                char *tx_bytes;
                                tx_bytes = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                                temp.tx_bytes = atoi(tx_bytes);
                            }
                            else if (strncmp (parameter_value,"tx_rate_info",12) == 0) {
                                char *last_tx_rate;
                                last_tx_rate = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                                temp.tx_rate = (atoi(last_tx_rate))*100;
                            }
                            else if (strncmp (parameter_value,"tx_packets",10) == 0) {
                                char *tx_packet;
                                tx_packet = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                                temp.tx_packets = atoi(tx_packet);
                            }
                            else if (strncmp (parameter_value,"rx_packets",10) == 0) {
                                char *rx_packet;
                                rx_packet = strtok_r(tmp_substring_array,"=' '",&tmp_substring_array);
                                temp.rx_packets = atoi(rx_packet);
                            }
                            else if (strncmp (parameter_value,"signal",6) == 0)
                            {
                                char *signal;
                                signal=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                                temp.rssi=(atoi(signal)+95);
                            }
                            parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
                        }
                    }
                }
                substring = strtok_r(NULL, "\n",&tmp);
            }
            struct wlanifInactStaStats *traverse = vap->head;
            if (traverse->valid == 0)
            {
                lbCopyMACAddr(temp.macaddr,traverse->macaddr);
                traverse->tx_bytes = temp.tx_bytes;
                traverse->rx_bytes = temp.rx_bytes;
                traverse->tx_rate = temp.tx_rate;
                traverse->tx_packets = temp.tx_packets;
                traverse->rx_packets = temp.rx_packets;
                traverse->rssi = temp.rssi;
                traverse->valid=1;
                traverse->activity=1;
            }
            else
            {
                int copy_flag=0;
                while(traverse)
                {
                    if(lbAreEqualMACAddrs(traverse->macaddr,temp.macaddr)) {
                        traverse->tx_bytes = temp.tx_bytes;
                        traverse->rx_bytes = temp.rx_bytes;
                        traverse->tx_rate = temp.tx_rate;
                        traverse->rssi = temp.rssi;
                        if((traverse->tx_packets == temp.tx_packets) || (traverse->rx_packets == temp.rx_packets))
                            traverse->flag=flag|4;
                        if((traverse->tx_packets != temp.tx_packets) || (traverse->rx_packets != temp.rx_packets)) {
                            traverse->flag=flag|2;
                            traverse->tx_packets = temp.tx_packets;
                            traverse->rx_packets = temp.rx_packets;
                        }
                        copy_flag=1;
                        break;
                    }
                    else {
                        traverse=traverse->next;
                    }
                }
                if(copy_flag==0)
                {
                    traverse=vap->head;
                    while(traverse->next!=NULL)
                        traverse=traverse->next;
                    traverse->next=calloc(1, sizeof(struct wlanifInactStaStats));
                    traverse=traverse->next;
                    lbCopyMACAddr(temp.macaddr,traverse->macaddr);
                    traverse->tx_bytes = temp.tx_bytes;
                    traverse->rx_bytes = temp.rx_bytes;
                    traverse->tx_rate = temp.tx_rate;
                    traverse->tx_packets = temp.tx_packets;
                    traverse->rx_packets = temp.rx_packets;
                    traverse->rssi = temp.rssi;
                    traverse->valid=1;
                    traverse->activity=1;
                    traverse->next=NULL;
                }
            }
        }
        else
            break;
    }while(1);
    free(macaddr);
}

/**
  * @brief refresh station rssi info
  *
  * @param [in] vap vap structure info
  * @param [in] addr station mac address
  * @param [in] numSamples number of samples
  */
void wlanif_RequestRssi(struct wlanifBSteerControlVapInfo *vap,const struct ether_addr *addr, u_int8_t numSamples)
{
    char command[50] = "POLL_STA ";
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    static const struct ether_addr tempAddr = {{0,0,0,0,0,0}};

    if(!(lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, addr->ether_addr_octet))) {
    snprintf(command,50,"POLL_STA %02x:%02x:%02x:%02x:%02x:%02x",addr->ether_addr_octet[0],addr->ether_addr_octet[1],addr->ether_addr_octet[2],addr->ether_addr_octet[3],addr->ether_addr_octet[4],addr->ether_addr_octet[5]);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"%s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    }
}

/**
  * @brief get station rssi info
  *
  * @param [in] vap vap structure handle
  * @param [in] addr station mac address
  * @param [out] rssi return rssi value
  *
  * @return LBD_OK on successful get; otherwise LBD_NOK
  *
  */
int wlanif_getRssi(struct wlanifBSteerControlVapInfo *vap, const struct ether_addr *addr, int *rssi)
{
    char command[50] = "STA ";
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp, *tmp_substring_array, *parameter_value = NULL, *substring = NULL;
    char substring_array[1028] = { 0 };
    static const struct ether_addr tempAddr = {{0,0,0,0,0,0}};

    if (!(lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, addr->ether_addr_octet))) {

        snprintf(command,50,"STA %02x:%02x:%02x:%02x:%02x:%02x",addr->ether_addr_octet[0],addr->ether_addr_octet[1],addr->ether_addr_octet[2],addr->ether_addr_octet[3],addr->ether_addr_octet[4],addr->ether_addr_octet[5]);
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
            dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"%s command_send is %s\n",__func__,command_send);
        }
        command_reply[recev] = '\0';
        tmp = command_reply;
        substring = strtok_r(tmp,"\n" ,&tmp);
        while (substring) {
            strlcpy(substring_array, substring,sizeof(substring_array));
            tmp_substring_array = substring_array;
            parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
            while (parameter_value) {
                if (strcmp (parameter_value,"signal") == 0)
                {
                    parameter_value=strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                    *rssi=(atoi(parameter_value)+95);
                    return LBD_OK;
                }
                parameter_value = strtok_r(NULL,",\n",&tmp_substring_array);
            }
            substring = strtok_r(NULL, "\n",&tmp);
        }
    }
        return LBD_NOK;
}

/**
  * @brief send BTM request
  *
  * @param [in] vap vap structure handle
  * @param [in] addr station mac address
  * @param [in] reqinfo btm requirement info to be sent
  *
  * @return LBD_OK on successful get; otherwise LBD_NOK
  *
  */
int wlanif_SendBTMRequest(struct wlanifBSteerControlVapInfo *vap,const struct ether_addr *addr, struct ieee80211_bstm_reqinfo_target *reqinfo)
{
    char command[1200] = "BSS_TM_REQ ";
    char command_var[200] = " ";
    char wbc_subie[] = "0603000000";
    char pref_subie[] = "0301";
    static char command_send[1200], command_reply[10];
    int command_leng,reply_leng,recev,sd,i;

    snprintf(command, 200 ,"BSS_TM_REQ %02x:%02x:%02x:%02x:%02x:%02x "
            "disassoc_timer=%d disassoc_imminent=%d "
            "pref=1 abridged=1 valid_int=255 bss_term=0 ",
            addr->ether_addr_octet[0],addr->ether_addr_octet[1],addr->ether_addr_octet[2],
            addr->ether_addr_octet[3],addr->ether_addr_octet[4],addr->ether_addr_octet[5],
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
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd > 0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"command_reply is %s\n", command_reply);

    if( strncmp(command_reply, "OK", 2) == 0 )
        return LBD_OK;
    else
        return LBD_NOK;
}

/**
  * @brief send RRM beacon request
  *
  * @param [in] vap vap structure handle
  * @param [in] addr station mac address
  * @param [in] reqmode reqmode in which the report has to be sent
  * @param [in] bcnrpt beconreport info to be sent
  *
  * @return LBD_OK on successful get; otherwise LBD_NOK
  *
  */
int wlanif_SendRRMBcnrptRequest(struct wlanifBSteerControlVapInfo *vap, const struct ether_addr *addr, u_int8_t reqmode, ieee80211_rrm_beaconreq_info_t bcnrpt)
{
    char command[1028] = "REQ_BEACON ";
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char ssid[IEEE80211_NWID_LEN + 1];
    u_int32_t ssidLength;
    u_int8_t subElementId0, subElement0Len;
    u_int8_t subElementId1, subElement1Len;
    u_int8_t subElementId2, subElement2Len;

    // Fill Sub-Element ID - 00
    subElementId0 = 0;
    wlanif_getssidName(vap, (void *)&ssid, &ssidLength);
    subElement0Len = ssidLength;
    char ssidname[128];
    char *name = ssidname;
    int i,pcount=0;
    for(i =0 ;i<ssidLength;i++)
    {
        pcount+=sprintf((name+pcount),"%02x",((unsigned char *) ssid)[i]);
    }
    // Fill Sub-Element ID - 01
    subElementId1 = 1;
    subElement1Len = sizeof(bcnrpt.rep_cond) + sizeof(bcnrpt.rep_thresh);

    // Fill Sub-Element ID - 02
    subElementId2 = 2;
    subElement2Len = sizeof(bcnrpt.rep_detail);
    bcnrpt.duration = le16toh( bcnrpt.duration);

    snprintf(command, 1028, "REQ_BEACON %02x:%02x:%02x:%02x:%02x:%02x req_mode=%02d %2x%02x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x%02x%s%02d%02d%02d%02d%02d%02d%02d",
             addr->ether_addr_octet[0],addr->ether_addr_octet[1],addr->ether_addr_octet[2],addr->ether_addr_octet[3],addr->ether_addr_octet[4],addr->ether_addr_octet[5],
             reqmode, bcnrpt.regclass, bcnrpt.channum, bcnrpt.random_ivl, bcnrpt.duration, bcnrpt.mode,
             bcnrpt.bssid[0], bcnrpt.bssid[1], bcnrpt.bssid[2], bcnrpt.bssid[3], bcnrpt.bssid[4], bcnrpt.bssid[5],
             subElementId0, ssidLength, ssidname,
             subElementId1, subElement1Len, bcnrpt.rep_cond, bcnrpt.rep_thresh,
             subElementId2, subElement2Len, bcnrpt.rep_detail);

    memcpy(command_send, command, sizeof (command));
    command_leng = strlen(command);
    reply_leng = sizeof(command_reply) - 1;
    sd = wlanif_sendCommand(vap->sock,command_send,command_leng,command_reply,reply_leng,&recev,vap);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG, "%s command_send is %s \n",__func__,command_send);
    }
    command_reply[recev] = '\0';
    return LBD_OK;
}

/**
  * @brief breakdown the beacon report received to lbd readable form
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddr station macaddr
  * @param [in] msg msg received from hostapd
  * @param [out] event event structure date
  */
void wlanif_getBSteerRRMRptEventData(int sock, char *macaddr, const char *msg, ath_netlink_bsteering_event_t *event,struct wlanifBSteerControlVapInfo *vap)
{
    event->type = ATH_EVENT_BSTEERING_RRM_REPORT;
    char *s = NULL;
    if (msg) {
        wlanif_getMacaddr(macaddr, event->data.rrm_report.macaddr);
        s = (char *)msg;
        char *substring_msg = NULL;
        char msg_array[512][512] = {{0},{0}};
        int m = 0;
        char *tmp_msg;
        tmp_msg = s;
        substring_msg = strtok_r(tmp_msg," " ,&tmp_msg);
        for(m = 0; substring_msg != NULL; m++){
            strlcpy(msg_array[m], substring_msg, sizeof(msg_array[m]));
            substring_msg = strtok_r(NULL," ",&tmp_msg);
        }
        event->data.rrm_report.rrm_type = BSTEERING_RRM_TYPE_BCNRPT;
        event->data.rrm_report.dialog_token = atoi(msg_array[0]);
        event->data.rrm_report.measrpt_mode = IEEE80211_RRM_MEASRPT_MODE_SUCCESS;
        char *long_msg = msg_array[2];
        char *channel;
        int channel_num;
        channel = (char *)malloc(5*sizeof(char));
        long_msg = long_msg+2;
        strlcpy(channel,long_msg,3);
        channel_num = (int)strtol(channel, NULL, 16);
        long_msg= long_msg+24;
        char *rcpi;
        int rcpi_value;
        rcpi = (char *)malloc(5*sizeof(char));
        strlcpy(rcpi,long_msg,3);
        rcpi_value = (int)strtol(rcpi, NULL, 16);

        long_msg = long_msg+2;
        char *rsni;
        int rsni_value;
        rsni = (char *)malloc(5*sizeof(char));
        strlcpy(rsni,long_msg,3);
        rsni_value = (int)strtol(rsni, NULL, 16);
        event->data.rrm_report.data.bcnrpt[0].chnum = channel_num;
        event->data.rrm_report.data.bcnrpt[0].rcpi = rcpi_value;
        event->data.rrm_report.data.bcnrpt[0].rsni = rsni_value;

        u_int8_t bssid[6];
        char *bssid_value;
        int j = 0;
        bssid_value = (char *)malloc(5*sizeof(char));
        for(j=0;j<6;j++)
        {
            long_msg = long_msg+2;
            strlcpy(bssid_value,long_msg,3);
            bssid[j] = (int)strtol(bssid_value,NULL,16);
        }
        memcpy(event->data.rrm_report.data.bcnrpt[0].bssid,bssid,6);
        dbgf(vap->state->bsteerControlHandle->dbgModule, DBGDEBUG,"BSSID is %02x:%02x:%02x:%02x:%02x:%02x \n",event->data.rrm_report.data.bcnrpt[0].bssid[0],event->data.rrm_report.data.bcnrpt[0].bssid[1],event->data.rrm_report.data.bcnrpt[0].bssid[2],event->data.rrm_report.data.bcnrpt[0].bssid[3],event->data.rrm_report.data.bcnrpt[0].bssid[4],event->data.rrm_report.data.bcnrpt[0].bssid[5]);
        /* Dialog Token */
        event->data.rrm_report.data.bcnrpt[0].more = 0;
    }
}
