/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <net/if.h>
#include <net/ethernet.h>
#include <asm/types.h>
#define _LINUX_IF_H /* Avoid redefinition of stuff */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

struct ucred {
    __u32   pid;
    __u32   uid;
    __u32   gid;
};

#include <ieee80211_external.h>
#include "wlanif_cfg80211.h"


#define TRACE_ENTRY() dbgf(soncfgDbgS.dbgModule, DBGINFO, "%s: Enter \n",__func__)
#define TRACE_EXIT() dbgf(soncfgDbgS.dbgModule, DBGINFO, "%s: Exit \n",__func__)
#define TRACE_EXIT_ERR() dbgf(soncfgDbgS.dbgModule, DBGERR, "%s: Exit with err %d\n",__func__,ret)

/* OL Radio xml parse using the shift and PARAM_BAND_INFO is fixed for cmd */
#define RADIO_PARAM_SHIFT 4096
#define RADIO_PARAM_BAND_INFO 399
#define OL_ATH_PARAM_BAND_INFO  (RADIO_PARAM_SHIFT + RADIO_PARAM_BAND_INFO)

static struct nla_policy
wlan_cfg80211_get_station_info_policy[QCA_WLAN_VENDOR_ATTR_PARAM_MAX + 1] = {

  [QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH] = {.type = NLA_U32 }
};

void cfg82011_station_info_cb(struct cfg80211_data *buffer)
{
    /*Parsing the nl msg updates the data and date_len from driver*/
    struct nlattr *attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_MAX + 1];
    nla_parse(attr_vendor, QCA_WLAN_VENDOR_ATTR_PARAM_MAX,
	      (struct nlattr *)buffer->data,
	      buffer->length, wlan_cfg80211_get_station_info_policy);
}

/* nl handler for IW based ioctl*/
static int wdev_info_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *nl_msg[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct wdev_info *info = arg;

    nla_parse(nl_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    if (nl_msg[NL80211_ATTR_WIPHY])
    {
        info->wiphy_idx = nla_get_u32(nl_msg[NL80211_ATTR_WIPHY]);
    } else {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "NL80211_ATTR_WIPHY not found\n");
        return -EINVAL;
    }

    if (nl_msg[NL80211_ATTR_IFTYPE])
    {
        info->nlmode = nla_get_u32(nl_msg[NL80211_ATTR_IFTYPE]);
    } else {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "NL80211_ATTR_IFTYPE not found\n");
        return -EINVAL;
    }

    if (nl_msg[NL80211_ATTR_MAC])
    {
        memcpy(info->macaddr, nla_data(nl_msg[NL80211_ATTR_MAC]), ETH_ALEN);
    } else {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "NL80211_ATTR_MAC not found\n");
        return -EINVAL;
    }

    if (nl_msg[NL80211_ATTR_SSID])
    {
        memcpy(info->essid, nla_data(nl_msg[NL80211_ATTR_SSID]), nla_len(nl_msg[NL80211_ATTR_SSID]));
        info->essid[nla_len(nl_msg[NL80211_ATTR_SSID])] = '\0';
    } else {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "NL80211_ATTR_SSID not found\n");
        return -EINVAL;
    }

    if(nl_msg[NL80211_ATTR_IFNAME])
    {
        memcpy(info->name, nla_data(nl_msg[NL80211_ATTR_IFNAME]), nla_len(nl_msg[NL80211_ATTR_IFNAME]));
        info->name[nla_len(nl_msg[NL80211_ATTR_IFNAME])] = '\0';
    } else {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "NL80211_ATTR_IFNAME not found\n");
        return -EINVAL;
    }

    if(nl_msg[NL80211_ATTR_WIPHY_FREQ])
    {
        info->freq = nla_get_u32(nl_msg[NL80211_ATTR_WIPHY_FREQ]);
        dbgf(soncfgDbgS.dbgModule, DBGINFO, "NL80211_ATTR_WIPHY_FREQ freq %d\n",info->freq);
    }

    return NL_SKIP;
}

/*allocate and send nlmsg to handle IW based ioctl*/
int send_nlmsg_wdev_info ( const char *ifname, wifi_cfg80211_context *cfgCtx, struct wdev_info *dev_info)
{
    struct nl_msg *nlmsg;
    struct nl_cb *cb;
    int ret, err;

    nlmsg = nlmsg_alloc();
    if (!nlmsg) {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "ERROR: Failed to allocate netlink message for msg.\n");
        return -ENOMEM;
    }

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "ERROR: Failed to allocate netlink callbacks.\n");
        nlmsg_free(nlmsg);
        return -ENOMEM;
    }

    /* Prepare nlmsg get the Interface attributes */
    genlmsg_put(nlmsg, 0, 0, cfgCtx->nl80211_family_id , 0, 0, NL80211_CMD_GET_INTERFACE, 0);
    nla_put_u32(nlmsg, NL80211_ATTR_IFINDEX, if_nametoindex(ifname));

    err = 1;

    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,wdev_info_handler , dev_info);

    /* send message */
    ret = nl_send_auto_complete(cfgCtx->cmd_sock, nlmsg);
    if (ret < 0) {
        goto out;
    }

    /*   wait for reply */
    while (err > 0) {  /* error will be set by callbacks */
        ret = nl_recvmsgs(cfgCtx->cmd_sock, cb);
        if (ret) {
            dbgf(soncfgDbgS.dbgModule, DBGERR, "nl80211: %s->nl_recvmsgs failed: %d\n", __func__, ret);
        }
    }

out:
    if (cb) {
        nl_cb_put(cb);
    }
    if (nlmsg) {
        nlmsg_free(nlmsg);
    }
    return err;
}

//cfg80211 command to get param from driver
int send_command_get_cfg80211( wifi_cfg80211_context *cfgCtx, const char *ifname, int op, int *data)
{
    int ret;
    struct cfg80211_data buffer;
    buffer.data = data;
    buffer.length = sizeof(int);
    buffer.parse_data = 0;
    buffer.callback = NULL;
    buffer.parse_data = 0;
    if((ret=wifi_cfg80211_send_getparam_command(cfgCtx, QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS, op, ifname, (char *)&buffer, sizeof(int))) < 0)
    {
        return -EIO;
    }
    return 0;
}

/*cfg80211 command to set param in driver*/
int send_command_set_cfg80211(wifi_cfg80211_context *cfgCtx, const char *ifname, int op, int *data, int data_len)
{
    int ret;
    struct cfg80211_data buffer;
    buffer.data = data;
    buffer.length = sizeof(int);
    buffer.parse_data = 0;
    buffer.callback = NULL;
    buffer.parse_data = 0;
    if((ret=wifi_cfg80211_send_setparam_command(cfgCtx, QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS, op, ifname, (char *)&buffer, sizeof(int))) < 0)
    {
        return -EIO;
    }
    return 0;
}

#if QCA_AIRTIME_FAIRNESS
static void get_atf_table(struct cfg80211_data *buffer)
{
    static uint32_t length = 0;

    length += buffer->length;

    if (length >= sizeof(struct atftable)) {
        length = 0;
    }
}
#endif

/* Cfg80211 command to send genric commands to driver*/
int send_generic_command_cfg80211(wifi_cfg80211_context *cfgCtx, const char *ifname, int cmd, char *data, int data_len)
{
    int res;
    struct cfg80211_data buffer;
    buffer.data = (void *)data;
    buffer.length = data_len;
    buffer.callback = NULL;
#if QCA_AIRTIME_FAIRNESS
    if (cmd == QCA_NL80211_VENDOR_SUBCMD_ATF)
    {
        buffer.callback = &get_atf_table;
        buffer.parse_data = 0;
    }
#endif
    res = wifi_cfg80211_send_generic_command(cfgCtx, QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION, cmd, ifname, (char *)&buffer, data_len);
    if (res < 0) {
        dbgf(soncfgDbgS.dbgModule, DBGERR, " %s : send NL command failed \n",__func__);
        return res;
    }

    return 0;
}

/* Function to get name of the dev */
int getName_cfg80211(void *ctx, const char * ifname, char *name )
{

    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    struct wdev_info devinfo = {0};

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_nlmsg_wdev_info(ifname, &(cfgPriv->cfg80211_ctx_qca), &devinfo)) < 0) {
        goto err;
    }

    strlcpy( name, devinfo.name, IFNAMSIZ );

    TRACE_EXIT();

    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to check whether the current device is AP */
int isAP_cfg80211(void *ctx, const char * ifname, uint32_t *result)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    struct wdev_info devinfo = {0};

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_nlmsg_wdev_info(ifname, &(cfgPriv->cfg80211_ctx_qca), &devinfo)) < 0) {
        goto err;
    }

    *result = ( devinfo.nlmode == NL80211_IFTYPE_AP ? 1 : 0 );

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

static struct nla_policy
wlan_cfg80211_get_wireless_mode_policy[QCA_WLAN_VENDOR_ATTR_PARAM_MAX + 1] = {

  [QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH] = {.type = NLA_U32 },
  [QCA_WLAN_VENDOR_ATTR_PARAM_DATA] = {.type = NLA_STRING },
};

void cfg82011_wificonfiguration_cb(struct cfg80211_data *buffer)
{
    struct nlattr *attr_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_MAX + 1];

    nla_parse(attr_vendor, QCA_WLAN_VENDOR_ATTR_PARAM_MAX,
            (struct nlattr *)buffer->data,
            buffer->length, wlan_cfg80211_get_wireless_mode_policy);
}

/* Function to get BSSID address */
int getBSSID_cfg80211(void *ctx, const char * ifname, struct ether_addr *BSSID )
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    struct cfg80211_data buffer;
    buffer.data = BSSID;
    buffer.length = IEEE80211_ADDR_LEN;
    buffer.parse_data = 0;
    buffer.callback = &cfg82011_wificonfiguration_cb;
    if((ret=wifi_cfg80211_send_getparam_command(&(cfgPriv->cfg80211_ctx_qca), QCA_NL80211_VENDORSUBCMD_BSSID, 0, ifname, (char *)&buffer, IEEE80211_ADDR_LEN)) < 0)
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Funtion to get ESSID info */
int getESSID_cfg80211(void *ctx, const const char * ifname, void *buf, uint32_t *len )
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    struct cfg80211_data buffer;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    memset(buf, 0, (IEEE80211_NWID_LEN + 1));
    buffer.data = (void *)buf;
    buffer.length = IEEE80211_NWID_LEN;
    buffer.parse_data = 0; /* Enable callback */
    buffer.callback = &cfg82011_wificonfiguration_cb;

    if((ret=wifi_cfg80211_send_getparam_command(&(cfgPriv->cfg80211_ctx_qca),
                    QCA_NL80211_VENDORSUBCMD_GET_SSID, 0,
                    ifname, (char *)&buffer, IEEE80211_NWID_LEN)) < 0)
    {
        goto err;
    }
    *len = strlen((char *)buf);

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get frequency info*/
/* Devinfo command fails if the interface is not up, so using the vendor command to get frequency */
int getFreq_cfg80211(void *ctx, const char * ifname, int32_t * freq)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_GET_FREQUENCY, freq)) < 0) {
        goto err;
    }

    *freq = (*freq * 100000);

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get wireless extension */
int getRange_cfg80211(void *ctx, const char *ifname, int *we_version)
{
    TRACE_ENTRY();

    /*
     * Since this cfg path does not use wireless extension feature,
     * assigning a constant that always passes.
     */
    *we_version = WIRELESS_EXT;

    //TRACE_EXIT();
    return 0;
}

/* Function to get channel width*/
int getChannelWidth_cfg80211(void *ctx, const char * ifname, int * chwidth)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_CHWIDTH, chwidth)) < 0)
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get channel extoffset */
int getChannelExtOffset_cfg80211(void *ctx, const char * ifname, int * choffset)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_CHEXTOFFSET, choffset)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;

}

/*Function to get ACS info*/
int getAcsState_cfg80211(void *ctx, const char * ifname, int * acsstate)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_GET_ACS, acsstate)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get CAC info*/
int getCacState_cfg80211(void *ctx, const char * ifname, int * cacstate)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_GET_CAC, cacstate)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get ParentIndex info*/
int getParentIfindex_cfg80211(void *ctx, const char * ifname, int * parentIndex)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_PARENT_IFINDEX, parentIndex)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Function to get smart monitor info*/
int getSmartMonitor_cfg80211(void *ctx, const char * ifname, int * smartmonitor)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_RX_FILTER_SMART_MONITOR, smartmonitor)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Function to get channel bandwidth*/
int getChannelBandwidth_cfg80211(void *ctx, const char * ifname, int * bandwidth)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_BANDWIDTH, bandwidth)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;

}

/* Function to get Atf related generic info*/
int getGenericInfoAtf_cfg80211(void *ctx, const char * ifname, int cmd ,void * chanInfo, int chanInfoSize)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

#if QCA_AIRTIME_FAIRNESS
    if(cmd == IEEE80211_IOCTL_ATF_SHOWATFTBL)
    {
        struct atf_data atfdata;
        memset(&atfdata, 0, sizeof(atfdata));
        atfdata.id_type = IEEE80211_IOCTL_ATF_SHOWATFTBL;
        atfdata.buf = chanInfo;
        atfdata.len = chanInfoSize;
        ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_ATF, (void *)&atfdata, sizeof(atfdata));
    }
    else
#endif
    {
        ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_ATF, chanInfo, chanInfoSize);
    }

    if (ret < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get Ald related generic info*/
int getGenericInfoAld_cfg80211(void *ctx, const char * ifname,void * chanInfo, int chanInfoSize)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_ALD_PARAMS, chanInfo, chanInfoSize)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to hmwds related generic info*/
int getGenericInfoHmwds_cfg80211(void *ctx, const char * ifname,void * chanInfo, int chanInfoSize)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_HMWDS_PARAMS, chanInfo, chanInfoSize)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to Nac related generic info*/
int getGenericNac_cfg80211(void *ctx, const char * ifname,void * config, int configSize)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_NAC, config, configSize)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get centre frequency*/
int getCfreq2_cfg80211(void *ctx, const char * ifname, int32_t * cfreq2)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, IEEE80211_PARAM_SECOND_CENTER_FREQ,cfreq2)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to setparam in the driver*/
int setParam_cfg80211(void *ctx, const char *ifname, int cmd, void *data, uint32_t len)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if((ret = send_command_set_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, cmd, data, len)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Functin to set maccmd, special case in argument handling*/
int setParamMaccmd_cfg80211(void *ctx, const char *ifname, void *data, uint32_t len)
{
    int ret,temp[2];
    struct wlanif_cfg80211_priv * cfgPriv;
    memcpy(temp, data, len);

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if((ret = send_command_set_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, temp[0], &temp[1], sizeof(temp[1]))) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;

}

/* Function to get channel info*/
int getChannelInfo_cfg80211(void *ctx, const char * ifname, void * chanInfo, int chanInfoSize)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_LIST_CHAN, chanInfo, chanInfoSize)) < 0) {
        goto err;
    }


    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;

}

/* Function to get channel info160*/
int getChannelInfo160_cfg80211(void *ctx, const char * ifname, void * chanInfo, int chanInfoSize)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDOR_SUBCMD_LIST_CHAN160, chanInfo, chanInfoSize)) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;

}

/* Function to get station info*/
int getStationInfo_cfg80211(void * ctx , const char *ifname, void *data , int * data_len)
{

#define LIST_STA_MAX_CFG80211_LENGTH (3*1024)
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    struct cfg80211_data buffer;
    int subCmd = QCA_NL80211_VENDOR_SUBCMD_LIST_STA;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    buffer.data = (void *)data;
    buffer.length = *data_len;
    buffer.parse_data = 0; /* Enable callback */
    buffer.callback = &cfg82011_station_info_cb;

    ret = wifi_cfg80211_send_generic_command(&(cfgPriv->cfg80211_ctx_qca), QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION, subCmd, ifname, (char *)&buffer, *data_len);
    if (ret < 0) {
        dbgf(soncfgDbgS.dbgModule, DBGERR, " %s : send NL command failed \n",__func__);
        goto err;
    }

    *data_len = buffer.length;

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get dbreq info*/
int getDbgreq_cfg80211(void * ctx , const char *ifname, void *data , uint32_t data_len)
{

    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    struct ieee80211req_athdbg * req;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    req = (struct ieee80211req_athdbg *) data;
    assert(req != NULL);

    if (req->cmd == IEEE80211_DBGREQ_BSTEERING_GET_DATARATE_INFO) {
        req->needs_reply = DBGREQ_REPLY_IS_REQUIRED;
    }

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, QCA_NL80211_VENDOR_SUBCMD_DBGREQ, data, data_len) < 0))
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Funtion to get extended subcommands */
int getExtended_cfg80211(void * ctx , const char *ifname, void *data , uint32_t data_len)
{

    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, QCA_NL80211_VENDOR_SUBCMD_EXTENDEDSTATS, data, data_len) < 0))
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Funtion to get station stats*/
int getStaStats_cfg80211(void * ctx , const char *ifname, void *data , uint32_t data_len)
{

    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if ((ret = send_generic_command_cfg80211(&(cfgPriv->cfg80211_ctx_qca),ifname, QCA_NL80211_VENDOR_SUBCMD_STA_STATS, data, data_len)) < 0)
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Funtion to handle Add/Del/Kick Mac commands*/
int addDelKickMAC_cfg80211(void * ctx , const char *ifname, int operation, void *data, uint32_t len)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    int cfg_id=-1;
    char *ptr = (char *)data;

    TRACE_ENTRY();

    /* TODO: Check for proper handling */
    /* set len to 6 bytes to be coherent with the driver changes */
    len = ETH_ALEN;
    ptr = ptr + 2; /* Move 2 bytes (sa_family) to get the mac starting address */
    data = (void *)ptr;

    switch (operation)
    {
        case IO_OPERATION_ADDMAC:
            cfg_id = QCA_NL80211_VENDORSUBCMD_ADDMAC;
            break;
        case IO_OPERATION_DELMAC:
            cfg_id = QCA_NL80211_VENDORSUBCMD_DELMAC;
            break;
        case IO_OPERATION_KICKMAC:
            cfg_id = QCA_NL80211_VENDORSUBCMD_KICKMAC;
            break;
        default:
            /*Unsupported operation*/
            return -1;
    }

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if(( ret = send_generic_command_cfg80211 (&(cfgPriv->cfg80211_ctx_qca), ifname, cfg_id, data, len)) < 0 )
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Function to set filter command */
int setFilter_cfg80211(void * ctx , const char *ifname, void *data, uint32_t len)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);
    if(( ret = send_generic_command_cfg80211 (&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDORSUBCMD_SETFILTER, data, len)) < 0 )
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/*Function to get Wireless mode from driver*/
int getWirelessMode_cfg80211(void * ctx , const char *ifname, void *data, uint32_t len)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    struct cfg80211_data buffer;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    buffer.data = (void *)data;
    buffer.length = len;
    buffer.parse_data = 0; /* Enable callback */
    buffer.callback = &cfg82011_wificonfiguration_cb;

    ret = wifi_cfg80211_send_generic_command(&(cfgPriv->cfg80211_ctx_qca), QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION, QCA_NL80211_VENDORSUBCMD_WIRELESS_MODE, ifname, (char *)&buffer, len);

    if (ret < 0) {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "%s :  send NL command failed - error %d\n",__func__,ret);
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to send mgmt packet*/
int sendMgmt_cfg80211(void * ctx , const char *ifname, void *data, uint32_t len)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);
    if(( ret = send_generic_command_cfg80211 (&(cfgPriv->cfg80211_ctx_qca), ifname, QCA_NL80211_VENDORSUBCMD_SEND_MGMT, data, len)) < 0 )
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;
err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get station count */
int getStaCount_cfg80211(void *ctx, const char * ifname, int32_t * result)
{
    TRACE_ENTRY();

    TRACE_EXIT();
    return 0;
}

/* Function to set Interface mode */
int setIntfMode_cfg80211(void *ctx, const char * ifname, const char * mode, u_int8_t len)
{
    TRACE_ENTRY();

    TRACE_EXIT();
    return 0;
}

/* Function to get Band Info for the given radio interface*/
int getBandInfo_cfg80211(void *ctx, const char * ifname, uint8_t * band_info)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    int bandType;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if (( ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, OL_ATH_PARAM_BAND_INFO, &bandType)) < 0)
    {
        goto err;
    }
    *band_info = (uint8_t)bandType;

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to get MixedBh_uplink rate */
int getUplinkRate_cfg80211(void *ctx, const char * ifname, uint16_t * ul_rate)
{
    int ret;
    struct wlanif_cfg80211_priv * cfgPriv;
    int upLinkRate;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if (( ret = send_command_get_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, IEEE80211_PARAM_WHC_MIXEDBH_ULRATE, &upLinkRate)) < 0)
    {
        goto err;
    }

    *ul_rate = (uint16_t)upLinkRate;

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to set MixedBh_uplink rate */
int setUplinkRate_cfg80211(void * ctx, const char *ifname, uint16_t ul_rate)
{
    int ret;
    int upLinkRate;

    upLinkRate = (int)ul_rate;

    TRACE_ENTRY();

    if (( ret = setParam_cfg80211(ctx, ifname, IEEE80211_PARAM_WHC_MIXEDBH_ULRATE, &upLinkRate, sizeof(upLinkRate))) < 0)
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Function to set the son mode and BackHaul_type */
int setSonBhType_cfg80211(void * ctx, const char *ifname, uint8_t bh_type)
{
    int ret;
    int bhType;

    bhType = (int)bh_type;

    TRACE_ENTRY();

    if (( ret = setParam_cfg80211(ctx, ifname, IEEE80211_PARAM_WHC_BACKHAUL_TYPE, &bhType, sizeof(bhType))) < 0)
    {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

int setParamVapInd_cfg80211(void *ctx, const char *ifname, void *data, uint32_t len)
{
    int ret,temp[2];
    struct wlanif_cfg80211_priv * cfgPriv;
    memcpy(temp, data, len);

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    if((ret = send_command_set_cfg80211(&(cfgPriv->cfg80211_ctx_qca), ifname, temp[0], &temp[1], sizeof(temp[1]))) < 0) {
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

int setFreq_cfg80211(void *ctx, const char * ifname, int freq)
{
    int ret;
    struct cfg80211_data buffer;
    struct wlanif_cfg80211_priv * cfgPriv;

    TRACE_ENTRY();

    cfgPriv = (struct wlanif_cfg80211_priv *) ctx;
    assert(cfgPriv != NULL);

    buffer.data = &freq;
    buffer.length = sizeof(int);
    buffer.parse_data = 0;
    buffer.callback = NULL;
    buffer.parse_data = 0;
    if((ret=wifi_cfg80211_send_setparam_command(&(cfgPriv->cfg80211_ctx_qca), QCA_NL80211_VENDORSUBCMD_CHANNEL_CONFIG, freq, ifname, (char *)&buffer, sizeof(int))) < 0)
    {
        return -EIO;
        goto err;
    }

    TRACE_EXIT();
    return 0;

err:
    TRACE_EXIT_ERR();
    return ret;
}

/* Init Fucnction to handle private ioctls*/
int wlanif_cfg80211_init(struct wlanif_config *cfg80211_conf)
{
    struct wlanif_cfg80211_priv * cfgPriv;

    int ret;

    /* Initialize debug module used for logging */
    soncfgDbgS.dbgModule = dbgModuleFind("libsoncfg");
    soncfgDbgS.dbgModule->Level=DBGERR;

    assert(cfg80211_conf != NULL);

    cfg80211_conf->IsCfg80211 = 1;
    cfg80211_conf->ctx = malloc(sizeof(struct wlanif_cfg80211_priv));

    if (cfg80211_conf->ctx == NULL)
    {
        printf("%s: Failed\n",__func__);
        return -ENOMEM;
    }

    cfgPriv = (struct wlanif_cfg80211_priv *) cfg80211_conf->ctx;

    assert(cfgPriv != NULL);

    /* Fill the private socket id for command and events */
    cfgPriv->cfg80211_ctx_qca.pvt_cmd_sock_id = cfg80211_conf->pvt_cmd_sock_id;
    cfgPriv->cfg80211_ctx_qca.pvt_event_sock_id = cfg80211_conf->pvt_event_sock_id;

    /*Initializing event related members to zero for not event supporting module*/
    cfgPriv->cfg80211_ctx_qca.event_thread_running = 0;
    cfgPriv->cfg80211_ctx_qca.event_sock = NULL;

    ret = wifi_init_nl80211(&(cfgPriv->cfg80211_ctx_qca));
    if (ret) {
        dbgf(soncfgDbgS.dbgModule, DBGERR, "%s : unable to create NL socket\n",__func__) ;
        return -EIO;
    }

    return 0;
}

/* Destroy the intialized context for cfg80211*/
void wlanif_cfg80211_deinit(struct wlanif_config *cfg80211_conf)
{
    struct wlanif_cfg80211_priv * cfgPriv;

    assert(cfg80211_conf != NULL);

    cfgPriv = (struct wlanif_cfg80211_priv *) cfg80211_conf->ctx;

    wifi_destroy_nl80211(&(cfgPriv->cfg80211_ctx_qca));

    free(cfg80211_conf->ctx);
}

/*ops table listing the supported commands*/
static struct wlanif_config_ops wlanif_cfg80211_ops = {
    .init = wlanif_cfg80211_init,
    .deinit = wlanif_cfg80211_deinit,
    .getName = getName_cfg80211,
    .isAP = isAP_cfg80211,
    .getBSSID = getBSSID_cfg80211,
    .getESSID = getESSID_cfg80211,
    .getFreq = getFreq_cfg80211,
    .getChannelWidth = getChannelWidth_cfg80211,
    .getChannelExtOffset = getChannelExtOffset_cfg80211,
    .getChannelBandwidth = getChannelBandwidth_cfg80211,
    .getAcsState = getAcsState_cfg80211,
    .getCacState = getCacState_cfg80211,
    .getParentIfindex = getParentIfindex_cfg80211,
    .getSmartMonitor = getSmartMonitor_cfg80211,
    .getGenericInfoAtf = getGenericInfoAtf_cfg80211,
    .getGenericInfoAld = getGenericInfoAld_cfg80211,
    .getGenericHmwds = getGenericInfoHmwds_cfg80211,
    .getGenericNac = getGenericNac_cfg80211,
    .getCfreq2 = getCfreq2_cfg80211,
    .getChannelInfo = getChannelInfo_cfg80211,
    .getChannelInfo160 = getChannelInfo160_cfg80211,
    .getStationInfo = getStationInfo_cfg80211,
    .getWirelessMode = getWirelessMode_cfg80211,
    .getDbgreq = getDbgreq_cfg80211,
    .getExtended = getExtended_cfg80211,
    .addDelKickMAC = addDelKickMAC_cfg80211,
    .setFilter = setFilter_cfg80211,
    .sendMgmt = sendMgmt_cfg80211,
    .setParamMaccmd = setParamMaccmd_cfg80211,
    .setParam = setParam_cfg80211,
    .getStaStats = getStaStats_cfg80211,
    .getRange = getRange_cfg80211,
    .getStaCount = getStaCount_cfg80211,
    .setIntfMode = setIntfMode_cfg80211,
    .getBandInfo = getBandInfo_cfg80211,
    .getUplinkRate = getUplinkRate_cfg80211,
    .setUplinkRate = setUplinkRate_cfg80211,
    .setSonBhType = setSonBhType_cfg80211,
    .setParamVapInd = setParamVapInd_cfg80211,
    .setFreq = setFreq_cfg80211,
};

struct wlanif_config_ops * wlanif_cfg80211_get_ops()
{
    return &wlanif_cfg80211_ops;
}
