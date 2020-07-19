/*
 * @File: sonlib_ath10k_CommandParser.c
 *
 * @Abstract: ATH10K Vendor specific module communicates between LBD and hostapd
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017, 2018 Qualcomm Technologies, Inc.
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
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <net/if.h>
#define _LINUX_IF_H
#include <linux/wireless.h>
#include "sonlib_ath10k_CommandParser.h"
#include "sonlib_cmn.h"
#include <string.h>

static const time_t NUM_NSEC_IN_SEC = 1000000000;

#define os_strncmp(s1, s2, n) strncmp((s1), (s2), (n))

int str_starts_ath10k(const char *str, const char *start)
{
    return os_strncmp(str, start, os_strlen(start)) == 0;
}


/**
  * @brief get channel width info based on driver inputs
  *
  * @param [in] secondary_channel  secondary channel
  * @param [in] ieee80211n  ieee80211n info
  * @param [in] ieee80211ac  ieee80211ac info
  * @param [in] vht_oper_chwidth  vht operation channel width info
  *
  * @return wlan_chwidth_e enum value
  */
wlan_chwidth_e getChannelWidth(int secondary_channel,int ieee80211n, int ieee80211ac, int vht_oper_chwidth)
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
        return chwidth_20;
    if (!secondary_channel)
        return chwidth_20;
    if (!vht || vht_oper_chwidth == VHT_CHANWIDTH_USE_HT)
        return chwidth_40;
    if (vht_oper_chwidth == VHT_CHANWIDTH_80MHZ)
        return chwidth_80;
    if (vht_oper_chwidth == VHT_CHANWIDTH_160MHZ)
        return chwidth_160;
    if (vht_oper_chwidth == VHT_CHANWIDTH_80P80MHZ)
        return chwidth_80p80;
    return chwidth_20;

}

/**
  * @brief get Phy mode from driver inputs
  *
  * @param [in] ieee80211n ieee80211n info
  * @param [in] ieee80211ac ieee80211n info
  * @param [in] freq frequency
  *
  * @return wlan_phymode enum value
  *
  */
wlan_phymode getPhyMode(int ieee80211n, int ieee80211ac, int freq)
{
    if (ieee80211ac)
        return phymode_vht;
    else if (ieee80211n && freq>5000)
        return phymode_ht;
    else if (ieee80211n)
        return phymode_ht;
    else if (freq>5000)
        return  phymode_ht;
    else
        return phymode_basic;
}

u_int8_t resolveChannum(u_int32_t freq, u_int8_t *channel) {

    if (!channel ) {
        return -1;
    }
    *channel = 0;

    freq /= 100000; // Convert to MHz
    if ((freq >= 2412) && (freq <= 2472)) {
        if (((freq - 2407) % 5) != 0) {
            /* error: freq not exact */
            return SONCMN_NOK;
        }
        *channel = (freq - 2407) / 5;
        return SONCMN_OK;
    }

    if (freq == 2484) {
        *channel = 14;
        return SONCMN_OK;
    }

#define IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c) ((_c) > 4940 && (_c) < 4990)
    if (freq >= 2512 && freq < 5000) {
        if (IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
             *channel = ((freq * 10) +
                         (((freq % 5) == 2) ? 5 : 0) - 49400)/5;
        } else if ( freq > 4900 ) {
             *channel = (freq - 4000) / 5;
        } else {
             *channel = 15 + ((freq - 2512) / 20);
        }
        return SONCMN_OK;
    }
    #define FREQ_5G_CH(_chan_num)   (5000 + (5 * _chan_num))

#define CASE_5G_FREQ(_chan_num)         \
    case FREQ_5G_CH(_chan_num):         \
        *channel = _chan_num;           \
        break;

    if ((freq >= FREQ_5G_CH(36)) && (freq <= FREQ_5G_CH(48))) {
        switch(freq) {
            CASE_5G_FREQ(36);
            CASE_5G_FREQ(40);
            CASE_5G_FREQ(44);
            CASE_5G_FREQ(48);
            default:
                /* No valid frequency in this range */
                return ATH10K_NOK;
        }
        return ATH10K_OK;
    }

    if ((freq >= FREQ_5G_CH(149)) && (freq <= FREQ_5G_CH(169))) {
        switch(freq) {
            CASE_5G_FREQ(149);
            CASE_5G_FREQ(153);
            CASE_5G_FREQ(157);
            CASE_5G_FREQ(161);
            CASE_5G_FREQ(165);
            CASE_5G_FREQ(169);
            default:
                /* No valid frequency in this range */
                return ATH10K_NOK;
        }
        return ATH10K_OK;
    }

    if ((freq >= FREQ_5G_CH(8)) && (freq <= FREQ_5G_CH(16))) {
        switch(freq) {
            CASE_5G_FREQ(8);
            CASE_5G_FREQ(12);
            CASE_5G_FREQ(16);
            default:
                /* No valid frequency in this range */
                return ATH10K_NOK;
        }
        return ATH10K_OK;
    }

    if ((freq >= FREQ_5G_CH(52)) && (freq <= FREQ_5G_CH(64))) {
        switch(freq) {
            CASE_5G_FREQ(52);
            CASE_5G_FREQ(56);
            CASE_5G_FREQ(60);
            CASE_5G_FREQ(64);
            default:
                /* No valid frequency in this range */
                return ATH10K_NOK;
        }
        return ATH10K_OK;
    }

    if ((freq >= FREQ_5G_CH(100)) && (freq <= FREQ_5G_CH(144))) {
        switch(freq) {
            CASE_5G_FREQ(100);
            CASE_5G_FREQ(104);
            CASE_5G_FREQ(108);
            CASE_5G_FREQ(112);
            CASE_5G_FREQ(116);
            CASE_5G_FREQ(120);
            CASE_5G_FREQ(124);
            CASE_5G_FREQ(128);
            CASE_5G_FREQ(132);
            CASE_5G_FREQ(136);
            CASE_5G_FREQ(140);
            CASE_5G_FREQ(144);
            default:
                /* No valid frequency in this range */
                return ATH10K_NOK;
        }
        return ATH10K_OK;
    }

    return ATH10K_NOK;

#undef IS_CHAN_IN_PUBLIC_SAFETY_BAND
#undef CASE_5G_FREQ
#undef FREQ_5G_CH
}

wlan_band resolveBandFromChannel(u_int8_t channum) {
	if (channum >=1 && channum <= 14) {
		return band_24g;
	} else if (channum >= 36 && channum <= 169) {
		return band_5g;
	}

	return band_invalid;
}

ATH10K_BOOL ath10kIsTimeBefore(const struct timespec *time1,
                        const struct timespec *time2) {
    assert(time1);
    assert(time2);

    return time1->tv_sec < time2->tv_sec ||
           (time1->tv_sec == time2->tv_sec &&
            time1->tv_nsec < time2->tv_nsec);
}

void lbTimeDiff(const struct timespec *time1,
                const struct timespec *time2,
                struct timespec *diff) {
    assert(time1);
    assert(time2);
    assert(diff);
    assert(!lbIsTimeAfter(time2, time1));

    time_t sec = time1->tv_sec;
    if (time1->tv_nsec >= time2->tv_nsec) {
        diff->tv_nsec = time1->tv_nsec - time2->tv_nsec;
    } else {
        sec--;
        diff->tv_nsec = NUM_NSEC_IN_SEC -
            (time2->tv_nsec - time1->tv_nsec);
    }

    diff->tv_sec = sec - time2->tv_sec;
}
/**
  * @brief callback function for msg received in hostapd socket
  *
  * @param [in] cookie priv_ath10k handle in which the socket is associated
  */
void bufferRead_ath10k(void *cookie)
{
    u_int32_t numBytes;
    char *msg = NULL;
    struct priv_ath10k *privAth10k =(struct priv_ath10k *) cookie;
    numBytes = bufrdNBytesGet(&privAth10k->state->readBuf);
    msg = bufrdBufGet(&privAth10k->state->readBuf);
    msg[numBytes] = '\0';
    do {
        if (!numBytes) {
            return;
        }
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG," Msg is %s and numBytes is %d for privAth10k %s \n",msg, numBytes, privAth10k->ifname);
    }while(0);

    bufrdConsume(&privAth10k->state->readBuf, numBytes);
    parseMsg_ath10k(msg,privAth10k);
}
/**
  * @brief get mac address removing ':'
  *
  * @param [in] macaddr macaddress with ':'
  * @param [out] client_addr array where updated macaddress is saved
  */
void getMacaddr_ath10k(char *macaddr,u_int8_t *client_addr)
{
    char *tmp_macaddr;
    char *substring_macaddr = NULL;
    char macaddr_array[512][512] = {{0},{0}};
    int m = 0;

    tmp_macaddr = macaddr;
    substring_macaddr = strtok_r(tmp_macaddr,":" ,&tmp_macaddr);
    for(m = 0; substring_macaddr != NULL; m++) {
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
int getStaChannelWidth_ath10k(ATH10K_BOOL ht,ATH10K_BOOL vht,int vhtcaps, int htcaps)
{
#define VHT_CAP_SUPP_CHAN_WIDTH_MASK 0x000c
#define VHT_CAP_SUPP_CHAN_WIDTH_160MHZ 0x0004
#define VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ 0x0008
#define  HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET 0x0002
    wlan_chwidth_e width;
    if (vht) {
        switch (vhtcaps & VHT_CAP_SUPP_CHAN_WIDTH_MASK) {
            case VHT_CAP_SUPP_CHAN_WIDTH_160MHZ:
                width = chwidth_160;
                break;
            case VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ:
                width = chwidth_80p80;
                break;
            default:
                width = chwidth_80;
        }
    } else if (ht) {
        width = htcaps &
            HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET ? chwidth_40 : chwidth_20;
    } else {
        width = chwidth_20;
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
int getStaPhyMode_ath10k(int freq,ATH10K_BOOL ht,ATH10K_BOOL vht)
{
    wlan_phymode phy_mode;
    if (freq>5000) {
        if(vht)
            phy_mode = phymode_vht;
        else if (ht)
            phy_mode = phymode_ht;
        else
            phy_mode = phymode_ht;
    } else if (ht)
        phy_mode = phymode_ht;
    else
        phy_mode = phymode_basic;
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
void getNodeAssocEventData_ath10k(int sock,char *macaddr, int freq,wlan_band band,ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k)
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
    ATH10K_BOOL ht = 0,vht = 0;
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

    TRACE_ENTRY();
    if(strcmp(macaddr,"00:00:00:00:00:00") !=0) {
        strcat(command,macaddr);
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = sendCommand_ath10k(sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
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
        getMacaddr_ath10k(macaddr,client_addr);
        memcpy(event->data.bs_node_associated.client_addr,client_addr,IEEE80211_ADDR_LEN);
        event->data.bs_node_associated.isBTMSupported = !(!(extcaps & IEEE80211_EXTCAPIE_BSSTRANSITION));
        event->data.bs_node_associated.isRRMSupported = !(!(caps & IEEE80211_CAPINFO_RADIOMEAS));
        event->data.bs_node_associated.datarate_info.max_chwidth = getStaChannelWidth_ath10k(ht,vht,vhtcaps,htcaps);
        event->data.bs_node_associated.datarate_info.phymode = getStaPhyMode_ath10k(freq,ht,vht);
        event->data.bs_node_associated.band_cap = (1 << band);

        if (vht == 0)
            event->data.bs_node_associated.datarate_info.max_MCS = htmaxmcs%8;
        else
            event->data.bs_node_associated.datarate_info.max_MCS = vhtmaxmcs;

        if(ht){
            event->data.bs_node_associated.datarate_info.is_static_smps = (ATH10K_BOOL)((htcaps & HT_CAP_INFO_SMPS_MASK) == HT_CAP_INFO_SMPS_STATIC);
        }
        if (vht) {
            event->data.bs_node_associated.datarate_info.is_mu_mimo_supported = (ATH10K_BOOL) vhtcaps & VHT_CAP_MU_BEAMFORMEE_CAPABLE;
        }
    }
    TRACE_EXIT();
}

/**
  * @brief update disassoc event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  */
void getNodeDisassocEventData_ath10k(char *macaddr, struct iw_event *ievent)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    memcpy(ievent->u.addr.sa_data,client_addr,IEEE80211_ADDR_LEN);
    TRACE_EXIT();
}

/**
  * @brief update probe request event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure
  * @param [in] rssi station rssi value
  */
void getProbeReqInd_ath10k(char *macaddr,ath_netlink_bsteering_event_t *event,int rssi)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    event->type = ATH_EVENT_BSTEERING_PROBE_REQ;
    memcpy(event->data.bs_probe.sender_addr,client_addr,IEEE80211_ADDR_LEN);
    event->data.bs_probe.rssi = rssi;
    TRACE_EXIT();
}

/**
  * @brief update opmode change event  data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure info
  * @param [in] value opmode value
  * @param [in] isbandwidth isbandwidth enable/disable
 */
void getOpModeChangeInd_ath10k(struct priv_ath10k *privAth10k,char *macaddr,ath_netlink_bsteering_event_t *event,int value , ATH10K_BOOL isbandwidth)
{
    TRACE_ENTRY();
    event->type = ATH_EVENT_BSTEERING_OPMODE_UPDATE;
    if (isbandwidth){
        getStanss_ath10k(privAth10k->sock,macaddr,event,privAth10k);
        event->data.opmode_update.datarate_info.max_chwidth = value;
    }
    else{
        event->data.opmode_update.datarate_info.num_streams = value;
        getStachwidth_ath10k(privAth10k->sock,macaddr,event,privAth10k);
    }
    TRACE_EXIT();
}

/**
  * @brief update smps event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event event structure data
  * @param [in] smpsmode smpsmode ebale/disable
  */
void getSmpsChangeInd_ath10k(char *macaddr,ath_netlink_bsteering_event_t *event,int smpsMode)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    memcpy(event->data.smps_update.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_SMPS_UPDATE;
    event->data.smps_update.is_static = smpsMode;
    TRACE_EXIT();

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
void getWnmEventInd_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event,int statusCode,int bssTermDelay,char *targetBssid)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    u_int8_t Bssid[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    getMacaddr_ath10k(targetBssid,Bssid);
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
    TRACE_EXIT();

}

/**
  * @brief update tx auth fail event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  */
void getTxAuthFailInd_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    memcpy(event->data.bs_auth.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_TX_AUTH_FAIL;
    TRACE_EXIT();
}

/**
  * @brief update rssi crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rssi sta rssi info
  * @param [in] isrssilow rssi low enable/disable
  */
void getRSSICrossingEvent_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event, int rssi, ATH10K_BOOL isrssilow)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    memcpy(event->data.bs_rssi_xing.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_CLIENT_RSSI_CROSSING;
    event->data.bs_rssi_xing.rssi = rssi;
    if (isrssilow)
        event->data.bs_rssi_xing.low_rssi_xing  = BSTEERING_XING_DOWN;
    else
        event->data.bs_rssi_xing.low_rssi_xing  = BSTEERING_XING_UP;
    TRACE_EXIT();
}

/**
  * @brief update rate crossing event data
  *
  * @param [in] macaddr station mac address
  * @param [out] event structure data
  * @param [in] rate sta rate info
  * @param [in] isratelow rate low enable/disable
  */
void getRateCrossingEvent_ath10k(char *macaddr, ath_netlink_bsteering_event_t *event, int rate, ATH10K_BOOL isratelow)
{
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};
    TRACE_ENTRY();
    getMacaddr_ath10k(macaddr,client_addr);
    memcpy(event->data.bs_tx_rate_xing.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->type = ATH_EVENT_BSTEERING_CLIENT_TX_RATE_CROSSING;
    event->data.bs_tx_rate_xing.tx_rate = rate;
    if (isratelow)
        event->data.bs_tx_rate_xing.xing = BSTEERING_XING_DOWN;
    else
        event->data.bs_tx_rate_xing.xing = BSTEERING_XING_UP;
    TRACE_EXIT();
}

/**
  * @brief get channel utilization event data
  *
  * @param [out] event event structure info
  * @param [in] chan_util channel utilization
  */
void getChanUtilInd_ath10k(ath_netlink_bsteering_event_t *event, int chan_util)
{
    TRACE_ENTRY();
    event->type = ATH_EVENT_BSTEERING_CHAN_UTIL;
    event->data.bs_chan_util.utilization = chan_util;
    TRACE_EXIT();
}

/**
  * @brief get frequency from VAP
  *
  * @param [in] privAth10k structure handle
  *
  * @return freq radio frequency
  *
  */
int getVapFreq_ath10k(struct priv_ath10k *privAth10k)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    int freq = 0;
    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGINFO,"command_send = %s\n", command_send);
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
    dbgf(soncmnDbgS.dbgModule, DBGERR,"%s Freq:%d \n",__func__, freq);
    TRACE_EXIT();
    return freq;
}

/**
  * @brief get channel utilization from VAP
  *
  * @param [in] priv_ath10k structure handle
  *
  * @return chanUtil channel utilization
  *
  */
int getChannelUtilization_ath10k(struct priv_ath10k *privAth10k)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    int chanutil = 0;
    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"command_send = %s\n", command_send);
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
    TRACE_EXIT();
    return chanutil;
}

/**
  * @brief get ssidname from VAP
  *
  * @param [in] priv_ath10k structure handle
  * @param [out] ssidname array to store ssid name
  * @param [out] length length of the ssid
  */
void getssidName_ath10k(struct priv_ath10k *privAth10k,u_int8_t *ssidname,u_int32_t *length)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    char *tmp;
    char *substring = NULL;
    char substring_array[1028] = { 0 };
    char *tmp_substring_array;
    char *parameter_value = NULL;
    TRACE_ENTRY();
    memcpy(command_send,"STATUS",sizeof ("STATUS"));
    command_leng = strlen(command_send);

    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"COMMAND_SEND = %s\n", command_send);
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
    TRACE_EXIT();
}

/**
  * @brief enable rrsi threshold
  *
  * @param [in] priv_ath10k structure handle
  */
void rssiXingThreshold_ath10k(struct priv_ath10k *privAth10k)
{
    char command[50];
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    TRACE_ENTRY();
    snprintf(command,50,"SIGNAL_MONITOR THRESHOLD=%d HYSTERESIS=0",(LOW_RSSI_XING_THRESHOLD-95));
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"Commaand reply is %s \n",command_reply);
    TRACE_EXIT();
    return;

}

/**
  * @brief enable rate threshold
  *
  * @param [in] priv_ath10k structure handle
  */
void rateXingThreshold_ath10k(struct priv_ath10k *privAth10k)
{
    char command[50];
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;
    int freq;
    u_int8_t channel,ret;
    wlan_band band;

    TRACE_ENTRY();
    freq = getVapFreq_ath10k(privAth10k);
    /* resolveChannum is dividing freq with 100000 so we are multiplying */
    freq *= 100000;
    ret = resolveChannum(freq,&channel);
    if(!ret) {
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"wrong channel %d \n",ret);
    }
    band = resolveBandFromChannel(channel);
    if (band == band_24g) {
    snprintf(command,90,"SIGNAL_TXRATE LOW=%d HIGH=%d",LOW_TX_RATE_XING_THRESHOLD_24G/100
            ,HIGH_TX_RATE_XING_THRESHOLD_24G/100);
    }
    if (band == band_5g) {
    snprintf(command,90,"SIGNAL_TXRATE LOW=%d HIGH=%lu",LOW_TX_RATE_XING_THRESHOLD_5G/100
            ,HIGH_TX_RATE_XING_THRESHOLD_5G/100);
    }
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    command_reply[recev] = '\0';
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"Commaand reply is %s \n",command_reply);
    TRACE_EXIT();
    return;

}

/**
  * @brief enable channel utilization
  *
  * @param [in] priv_ath10k structure handle
  */
void chanutilEventEnable_ath10k(struct priv_ath10k *privAth10k)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    TRACE_ENTRY();
    memcpy(command_send,"ATTACH bss_load_events=1",sizeof ("ATTACH bss_load_events=1"));
    command_leng = strlen(command_send);

    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    command_reply[recev] = '\0';
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"Commaand reply is %s \n",command_reply);
    TRACE_EXIT();
    return;
}

/**
  * @brief enable probe events
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] enable events enable/disable
  */
void probeEventEnable_ath10k(struct priv_ath10k *privAth10k,int enable)
{
    static char command_send[120], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    if(enable)
        memcpy(command_send,"ATTACH probe_rx_events=1",sizeof ("ATTACH probe_rx_events=1"));
    else
        memcpy(command_send,"ATTACH probe_rx_events=0",sizeof ("ATTACH probe_rx_events=0"));
    command_leng = strlen(command_send);

    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"COMMAND_SEND in probe events is = %s enable %d \n", command_send,enable);
    }
    command_reply[recev] = '\0';
    return;
}

/**
  * @brief update BSS load update period
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] update_period period to be updated
  */
void setBssUpdatePeriod_ath10k(struct priv_ath10k *privAth10k,int update_period)
{
    char command[50];
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    TRACE_ENTRY();
    snprintf(command,50,"SET bss_load_update_period %d",update_period);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"%s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    TRACE_EXIT();
    return;

}

/**
  * @brief set average utilization period
  *
  * @param [in] priv_ath10k structure handle
  * @param [in] avg_period average period to be updated
  */
void setChanUtilAvgPeriod_ath10k(struct priv_ath10k *privAth10k,int avg_period)
{
    char command[50];
    static char command_send[1028], command_reply[1028];
    int command_leng,reply_leng,recev, sd;

    TRACE_ENTRY();
    snprintf(command,50,"SET chan_util_avg_period %d",avg_period);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s command_send is %s\n",__func__, command_send);
    }
    command_reply[recev] = '\0';
    TRACE_EXIT();
    return;

}


void getDataRateSta_ath10k(struct priv_ath10k *privAth10k,struct ether_addr *addr, struct soncmn_ieee80211_bsteering_datarate_info_t *phyCapInfo, ATH10K_BOOL *isRRMSupported,ATH10K_BOOL *isBTMSupported,int *loop_iter)
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
    ATH10K_BOOL ht = 0,vht = 0;
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
    TRACE_ENTRY();
    if (!(lbAreEqualMACAddrs(&tempAddr.ether_addr_octet, addr->ether_addr_octet)))
  {
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
    sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);


    if(sd >0)
    {
        command_send[command_leng] = '\0';
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG," In %s command_send is %s\n",__func__ ,command_send);
    }
    command_reply[recev] = '\0';
    dbgf(soncmnDbgS.dbgModule, DBGDEBUG," %s recev is %d Command reply is %s \n",__func__,recev,command_reply);
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
                    phyCapInfo->max_txpower = atoi(max_txpower);
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

    phyCapInfo->num_streams = maxnss;
    getMacaddr_ath10k(copied_macaddr,client_addr);
    lbCopyMACAddr(client_addr,addr->ether_addr_octet);
    freq = getVapFreq_ath10k(privAth10k);
    phyCapInfo->max_chwidth = getStaChannelWidth_ath10k(ht,vht,vhtcaps,htcaps);
    phyCapInfo->phymode = getStaPhyMode_ath10k(freq,ht,vht);
    if (vht == 0)
        phyCapInfo->max_MCS = htmaxmcs%8;
    else
        phyCapInfo->max_MCS = vhtmaxmcs;

    if (vht) {
        phyCapInfo->is_mu_mimo_supported = (ATH10K_BOOL) vhtcaps & VHT_CAP_MU_BEAMFORMEE_CAPABLE;
    }
    if (ht) {
        phyCapInfo->is_static_smps = (ATH10K_BOOL)((htcaps & HT_CAP_INFO_SMPS_MASK) == HT_CAP_INFO_SMPS_STATIC);
    }
    *isBTMSupported =  !(!(extcaps & IEEE80211_EXTCAPIE_BSSTRANSITION));
    *isRRMSupported = !(!(caps & IEEE80211_CAPINFO_RADIOMEAS)) ;
    free(macaddr);
    free(copied_macaddr);
    TRACE_EXIT();
}


/**
 * @brief Get the channel width for the Station
 *
 * @param [in] sock Hostapd Socket
 * @param [in] macaddr station mac address
 * @param [in] event event structure info
 * @param [in] priv_ath10k Structure
 */

void getStachwidth_ath10k(int sock, char *macaddr, ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k)
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
    ATH10K_BOOL ht = 0,vht = 0;
    int htcaps = 0,vhtcaps = 0;
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};

    TRACE_ENTRY();
    if(strcmp(macaddr,"00:00:00:00:00:00") !=0){
    strcat(command,macaddr);
    memcpy(command_send,command,sizeof (command));
    command_leng = strlen(command_send);
    reply_leng = sizeof(command_reply) - 1;
    sd = sendCommand_ath10k(sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
    if(sd >0)
    {
        command_send[command_leng] = '\0';
    }
    command_reply[recev] = '\0';
    tmp = command_reply;
    dbgf(NULL, DBGDEBUG," %s recev is %d Command reply is %s \n",__func__,recev,command_reply);
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

    getMacaddr_ath10k(macaddr,client_addr);
    memcpy(event->data.opmode_update.client_addr,client_addr,IEEE80211_ADDR_LEN);
    event->data.opmode_update.datarate_info.max_chwidth = getStaChannelWidth_ath10k(ht,vht,vhtcaps,htcaps);
    }
    TRACE_EXIT();
}

/**
 * @brief Get the nss for the given station
 *
 * @param [in] sock Hostapd Socket
 * @param [in] macaddr station mac address
 * @param [in] event event structure info
 * @param [in] priv_ath10k Structure
 */
void getStanss_ath10k(int sock, char *macaddr, ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k)
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
    ATH10K_BOOL ht = 0,vht = 0;
    int htcaps = 0,vhtcaps = 0;
    u_int8_t client_addr[IEEE80211_ADDR_LEN] = {0};

    TRACE_ENTRY();
    if(strcmp(macaddr,"00:00:00:00:00:00") !=0){
        strcat(command,macaddr);
        memcpy(command_send,command,sizeof (command));
        command_leng = strlen(command_send);
        reply_leng = sizeof(command_reply) - 1;
        sd = sendCommand_ath10k(sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
        }
        command_reply[recev] = '\0';
        tmp = command_reply;
        dbgf(NULL, DBGDEBUG," %s recev is %d Command reply is %s \n",__func__,recev,command_reply);
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
        getMacaddr_ath10k(macaddr,client_addr);
        event->data.opmode_update.datarate_info.max_chwidth = getStaChannelWidth_ath10k(ht,vht,vhtcaps,htcaps);
    }
    TRACE_EXIT();
}

/**
 * @brief get station inactivity status for all sta connected to the interface
 *
 * @param [in] priv_ath10k structure handle
 */
void getInactStaStats_ath10k(struct priv_ath10k *privAth10k)
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
    TRACE_ENTRY();
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
        sd = sendCommand_ath10k(privAth10k->sock,command_send,command_leng,command_reply,reply_leng,&recev,privAth10k);
        if(sd >0)
        {
            command_send[command_leng] = '\0';
            dbgf(NULL, DBGDEBUG," %s command_send is %s\n",__func__, command_send);
        }
        else {
            break;
        }
        command_reply[recev] = '\0';
        m=0;
        if ((recev > 0) && (strncmp(command_reply,"FAIL",4)!= 0) && (strcmp(command_reply,"") != 0))
        {
            struct inactStaStats temp;
            tmp = command_reply;
            substring = strtok_r(tmp,"\n" ,&tmp);
            while (substring) {
                strlcpy(substring_array, substring,sizeof(substring_array));
                tmp_substring_array = substring_array;
                parameter_value = strtok_r(tmp_substring_array,"=",&tmp_substring_array);
                if (m == 0)
                {
                    strlcpy(macaddr,parameter_value,18);
                    getMacaddr_ath10k(macaddr,client_addr);
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
            struct inactStaStats *traverse = privAth10k->head;
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
                    traverse=privAth10k->head;
                    while(traverse->next!=NULL)
                        traverse=traverse->next;
                    traverse->next=calloc(1, sizeof(struct inactStaStats));
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
    TRACE_EXIT();
}

/**
  * @brief breakdown the beacon report received to readable form
  *
  * @param [in] sock hostapd socket
  * @param [in] macaddr station macaddr
  * @param [in] msg msg received from hostapd
  * @param [out] event event structure date
  */
void getBSteerRRMRptEventData_ath10k(int sock, char *macaddr, const char *msg, ath_netlink_bsteering_event_t *event,struct priv_ath10k *privAth10k)
{
    event->type = ATH_EVENT_BSTEERING_RRM_REPORT;
    char *s = NULL;
    TRACE_ENTRY();
    if (msg) {
        getMacaddr_ath10k(macaddr, event->data.rrm_report.macaddr);
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
        free(channel);
        long_msg= long_msg+24;
        char *rcpi;
        int rcpi_value;
        rcpi = (char *)malloc(5*sizeof(char));
        strlcpy(rcpi,long_msg,3);
        rcpi_value = (int)strtol(rcpi, NULL, 16);
        free(rcpi);

        long_msg = long_msg+2;
        char *rsni;
        int rsni_value;
        rsni = (char *)malloc(5*sizeof(char));
        strlcpy(rsni,long_msg,3);
        rsni_value = (int)strtol(rsni, NULL, 16);
        event->data.rrm_report.data.bcnrpt[0].chnum = channel_num;
        event->data.rrm_report.data.bcnrpt[0].rcpi = rcpi_value;
        event->data.rrm_report.data.bcnrpt[0].rsni = rsni_value;
        free(rsni);

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
        dbgf(NULL, DBGDEBUG,"BSSID is %02x:%02x:%02x:%02x:%02x:%02x \n",event->data.rrm_report.data.bcnrpt[0].bssid[0],event->data.rrm_report.data.bcnrpt[0].bssid[1],event->data.rrm_report.data.bcnrpt[0].bssid[2],event->data.rrm_report.data.bcnrpt[0].bssid[3],event->data.rrm_report.data.bcnrpt[0].bssid[4],event->data.rrm_report.data.bcnrpt[0].bssid[5]);
        /* Dialog Token */
        event->data.rrm_report.data.bcnrpt[0].more = 0;
        free(bssid_value);
    }
    TRACE_EXIT();
}


/**
  * @brief parse message received from hostapd
  *
  * @param [in] msg message received from hostapd
  * @param [in] priv_ath10k structure handle
  *
  * @return parsed end string
  *
  */
void parseMsg_ath10k(const char *msg, struct priv_ath10k *privAth10k)
{
    const char *start , *s = NULL;
    const char *signal = NULL;
    const char *rate = NULL;
    const char *smpsMode = NULL;
    char *statusCode = NULL;
    char *bssTermDelay = NULL;
    const char *targetBssid = NULL;
    const char *bandwidth = NULL;
    const char *nss = NULL;
    char *macaddr = NULL;
    char *bssid = NULL;
    char *channelChanged = NULL;
    int rssi, value = 0;
    ATH10K_BOOL isBandwidth = ATH10K_FALSE;
    macaddr = (char *)malloc(20*sizeof(char));
    bssid = (char *)calloc(20,sizeof(char));
    ath_netlink_bsteering_event_t *event = calloc(1, sizeof(ath_netlink_bsteering_event_t));
    struct iw_event *ievent = calloc(1,sizeof(struct iw_event));

    start = strchr(msg, '>');
    if (start == NULL){
        start = strchr(msg,':');
        if (start == NULL){
            start = msg;
            //return NULL;
        }
    }

    start++;
    event->sys_index = if_nametoindex(privAth10k->ifname);
    wlanifBSteerEventsHandle_t lbdEvHandle = (wlanifBSteerEventsHandle_t)soncmnGetLbdEventHandler();
    if ( lbdEvHandle == NULL ) {
       goto err;
    }
    wlanifLinkEventsHandle_t lbdLinkHandle = (wlanifLinkEventsHandle_t)soncmnGetLbdLinkHandler();
    if ( lbdLinkHandle == NULL ) {
       goto err;
    }
       if (str_starts_ath10k(start, AP_STA_CONNECTED)) {
        int freq;
        s = strchr(start, ' ');
        if (s == NULL)
            goto err;
        s++;
        dbgf(NULL, DBGDEBUG,"%s Sta connected is %s \n",__func__,s);
        strlcpy(macaddr,s,18);
        freq = getVapFreq_ath10k(privAth10k);

        u_int8_t channel;
        wlan_band band;
        SONCMN_STATUS result = SONCMN_OK;
        /* resolveChannum is dividing freq with 100000 so we are multiplying */
        freq *= 100000;
        result = resolveChannum(freq,&channel);
        if(result!= SONCMN_OK) {
            dbgf(soncmnDbgS.dbgModule, DBGERR,"%s Freq not exact \n",__func__);
        }
        band = resolveBandFromChannel(channel);
        dbgf(NULL, DBGDEBUG,"%s Sta connected is freq:%d  band:%d \n",__func__,freq, band);
        getNodeAssocEventData_ath10k(privAth10k->sock,macaddr,freq,band, event,privAth10k);
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    else if (str_starts_ath10k(start, AP_STA_DISCONNECTED)) {
        s = strchr(start, ' ');
        if (s == NULL)
            goto err;
        s++;
        dbgf(soncmnDbgS.dbgModule, DBGERR,"%s Sta disconnected is %s \n",__func__,s);
        strlcpy(macaddr,s,18);
        getNodeDisassocEventData_ath10k(macaddr,ievent);
        wlanifLinkEventsCmnGenerateDisassocEvent(lbdLinkHandle, (void *)ievent,event->sys_index);

    }
    else if (str_starts_ath10k(start, AP_CSA_FINISHED)) {
        s = strchr(start , ' ');
        if (s == NULL)
            goto err;
        s++;
        if (str_starts_ath10k(s,"freq")){
            s = strchr(s, '=');
            s++;

            strlcpy(channelChanged,s,4);
        }
        ievent->u.freq.m = atoi(channelChanged);

        wlan_band band;
        band = resolveBandFromChannel(ievent->u.freq.m);
        wlanifLinkEventsCmnProcessChannelChange(
                lbdLinkHandle, (void *)ievent, band,event->sys_index);
        wlanifLinkEventsProcessChannelChange(event->sys_index, ievent->u.freq.m);
    }
    else if (str_starts_ath10k(start, PROBE_REQ)) {
        s = strchr(start , ' ');
        if (s == NULL)
            goto err;
        s++;
        if(str_starts_ath10k(s,"sa")){
            s = strchr(s, '=');
            s++;
            strlcpy(macaddr,s,18);
        }
        s=s+18;
        while(*s != '\n') {
            if(str_starts_ath10k(s,"signal")){
                signal = strchr(s, '=');
                if (signal == NULL)
                    goto err;
                signal++;
                break;
            }
            s=strchr(s, ' ');
        }
        //Conversion from rssi to SNR
        rssi = atoi(signal)+95;
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"%s:Probe request for sta %s rssi %d \n",__func__, macaddr, rssi);
        getProbeReqInd_ath10k(macaddr, event,rssi);
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    else if ((str_starts_ath10k(start,STA_WIDTH_MODIFIED)) || (str_starts_ath10k(start,STA_RX_NSS_MODIFIED))) {
        s = strchr (start, ' ');
        if (s == NULL)
            goto err;
        s++;

        strlcpy(macaddr,s,18);
        s = s+18;
        while(*s != '\n') {
            if(str_starts_ath10k(s,"width")){
                bandwidth = strchr(s, '=');
                if (bandwidth == NULL)
                    goto err;
                bandwidth++;
                value = atoi(bandwidth);
                isBandwidth = ATH10K_TRUE;
                break;
            }
            else if (str_starts_ath10k(s,"rx_nss")){
                nss = strchr(s, '=');
                if (nss == NULL)
                    goto err;
                nss++;
                value = atoi(nss);
                break;
            }

            s=strchr(s, ' ');
        }

        getOpModeChangeInd_ath10k(privAth10k,macaddr, event,value,isBandwidth);
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    else if (str_starts_ath10k(start,STA_SMPS_MODE_MODIFIED)) {
        s = strchr (start, ' ');
        if (s == NULL)
            goto err;
        s++;
        strlcpy(macaddr,s,18);
        s = s+18;
        while(*s != '\n') {
            if(str_starts_ath10k(s,"smps mode")){
                smpsMode = strchr(s, '=');
                if (smpsMode == NULL)
                    goto err;
                smpsMode++;
                break;
            }
            s=strchr(s, ' ');
        }
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"%s:SMPS mode change  is %d \n", __func__, atoi(smpsMode));
        getSmpsChangeInd_ath10k(macaddr, event,atoi(smpsMode));
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    if (str_starts_ath10k(start,BSS_TM_RESP)) {
        s = strchr (start, ' ');
        if (s == NULL)
            goto err;
        s++;
        strlcpy(macaddr,s,18);
        s = s+18;
      while(s != NULL) {
            if(str_starts_ath10k(s,"status_code")){
                statusCode = (char *)malloc(2*sizeof(char));
                s = strchr(s, '=');
                if (s == NULL)
                    goto err;
                s++;
                strlcpy(statusCode,s,2);
            }
            else if (str_starts_ath10k(s," bss_termination_delay")){
                bssTermDelay = (char *)malloc(2*sizeof(char));
                s = strchr(s, '=');
                if (s == NULL)
                    goto err;
                s++;
                strlcpy(bssTermDelay,s,2);
            }
            else if (str_starts_ath10k(s," target_bssid")){
                targetBssid = strchr(s, '=');
                if (targetBssid == NULL)
                    goto err;
                targetBssid++;
                strlcpy(bssid,targetBssid,18);
                s = s+19;
            }
            s=strchr(s, ' ');
        }
        getWnmEventInd_ath10k(macaddr, event,atoi(statusCode),atoi(bssTermDelay),bssid);
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }

    else if(str_starts_ath10k(start,TX_AUTH_FAIL)){
        s = strchr(start, ' ');
        if (s == NULL)
            goto err;
        s++;
        dbgf(soncmnDbgS.dbgModule, DBGDEBUG,"Sta with Auth failure is %s \n", s);
        strlcpy(macaddr,s,18);
        getTxAuthFailInd_ath10k(macaddr,event);
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    if(str_starts_ath10k(start,RSSI_CROSSING)) {
        ATH10K_BOOL isrssilow;
        int rssi = 0;
        s = strchr(start, ' ');
        if (s == NULL)
            goto err;
        s++;
        if(str_starts_ath10k(s,RSSI_CROSSING_LOW)) {
            s = strchr(s, ':');
            s--;
            s--;
            strlcpy(macaddr,s,18);
            isrssilow = ATH10K_TRUE;
            s=s+18;
            while(*s != '\n') {
                if(str_starts_ath10k(s,"RSSI")){
                 signal = strchr(s, ':');
                    if (signal == NULL)
                        goto err;
                    signal++;
                    break;
                }
                s=strchr(s, ' ');
            }
            //Convertion from rssi to SNR
            rssi = atoi(signal)+95;

            getRSSICrossingEvent_ath10k(macaddr,event,rssi,isrssilow);
        }
        else if (str_starts_ath10k(s,RSSI_CROSSING_HIGH)) {
            s = strchr(s,':');
            s--;
            s--;
            strlcpy(macaddr,s,18);
            isrssilow = ATH10K_FALSE;
            s=s+18;
            while(*s != '\n') {
                if(str_starts_ath10k(s,"RSSI")){
                    signal = strchr(s, ':');
                    if (signal == NULL)
                        goto err;
                    signal++;
                    break;
                }
                s=strchr(s, ' ');
            }
            //Convertion from rssi to SNR
            rssi = atoi(signal)+95;
            getRSSICrossingEvent_ath10k(macaddr,event,rssi,isrssilow);
        }
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    if(str_starts_ath10k(start,RATE_CROSSING)) {
        ATH10K_BOOL isratelow;
        int rateValue = 0;
        s = strchr(start, ' ');
        if (s == NULL)
            goto err;
        s++;
        if(str_starts_ath10k(s,RATE_CROSSING_LOW)) {
            s = strchr(s,':');
            s--;
            s--;
            strlcpy(macaddr,s,18);
            isratelow = ATH10K_TRUE;
            s=s+18;
            while(*s != '\n') {
                if(str_starts_ath10k(s,"txrate ")){
                    rate = strchr(s, ':');
                    if (rate == NULL)
                        goto err;
                    rate++;
                    break;
                }
                s=strchr(s, ' ');
            }
                        rateValue = atoi(rate)*100;
            getRateCrossingEvent_ath10k(macaddr,event,rateValue,isratelow);
        }
        else if (str_starts_ath10k(s,RATE_CROSSING_HIGH)) {
            s = strchr(s,':');
            s--;
            s--;
            strlcpy(macaddr,s,18);
            isratelow = ATH10K_FALSE;
            s=s+18;
            while(*s != '\n') {
                if(str_starts_ath10k(s,"txrate ")){
                    rate = strchr(s, ':');
                    if (rate == NULL)
                        goto err;
                    rate++;
                    break;
                }
                s=strchr(s, ' ');
            }
            rateValue = atoi(rate);
            getRateCrossingEvent_ath10k(macaddr,event,rateValue*100,isratelow);
        }
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }
    if (str_starts_ath10k(start, BCN_RESP_RX)) {
        s = strchr(start, ' ');
        if (s == NULL)
            goto err;
        s++;
        strlcpy(macaddr,s,18);
        s = s+18;
        getBSteerRRMRptEventData_ath10k(privAth10k->sock,macaddr,s,event,privAth10k);
        wlanifBSteerEventsMsgRx(lbdEvHandle,event);
    }

err:
     free(macaddr);
     free(bssid);
     free(event);
     free(ievent);
     return ;
}
