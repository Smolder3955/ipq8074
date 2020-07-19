/*
 * Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 * Notifications and licenses are retained for attribution purposes only
 *
 * Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2017, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 * Copyright(c) 2015 Intel Deutschland GmbH
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <sys/types.h>
#include <net/if_arp.h>
#include <fcntl.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <errno.h>

#include "qca-vendor_copy.h"
#include "icm.h"


/* Some utility APIs */
void * zalloc(size_t size)
{
    void *n = malloc(size);
    if (n)
        memset(n, 0, size);
    return n;
}

int get_ifindex(char *ifname)
{
    if (ifname == NULL)
        return -1;

    return if_nametoindex(ifname);
}

/*
 * get_ie - Fetch a specified information element from IEs buffer
 * @ies: Information elements buffer
 * @len: Information elements buffer length
 * @eid: Information element identifier (WLAN_EID_*)
 * Returns: Pointer to the information element (id field) or %NULL if not found
 *
 * This function returns the first matching information element in the IEs
 * buffer or %NULL in case the element is not found.
 */
const u8 * get_ie(const u8 *ies, size_t len, u8 eid)
{
    const u8 *end;

    if (!ies)
        return NULL;

    end = ies + len;

    while (end - ies > 1) {
        if (2 + ies[1] > end - ies)
            break;

        if (ies[0] == eid)
            return ies;

        ies += 2 + ies[1];
    }

    return NULL;
}

static struct nl_handle * nl_create_handle(struct nl_cb *cb, const char *dbg)
{
    struct nl_handle *handle;

    handle = nl80211_handle_alloc(cb);
    if (handle == NULL) {
        icm_printf("nl80211: Failed to allocate netlink "
                "callbacks (%s)\n", dbg);
        return NULL;
    }

    if (genl_connect(handle)) {
        icm_printf("nl80211: Failed to connect to generic "
                "netlink (%s)\n", dbg);
        nl80211_handle_destroy(handle);
        return NULL;
    }

    return handle;
}


static void nl_destroy_handles(struct nl_handle **handle)
{
    if (*handle == NULL)
        return;
    nl80211_handle_destroy(*handle);
    *handle = NULL;
}


static int ack_handler(struct nl_msg *msg, void *arg)
{
    int *err = arg;
    *err = 0;
    return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    *ret = 0;
    return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
        void *arg)
{
    int *ret = arg;
    *ret = err->error;
    return NL_SKIP;
}


static int no_seq_check(struct nl_msg *msg, void *arg)
{
    return NL_OK;
}


static void nl80211_nlmsg_clear(struct nl_msg *msg)
{
    /*
     * Clear nlmsg data, e.g., to make sure key material is not left in
     * heap memory for unnecessarily long time.
     */
    if (msg) {
        struct nlmsghdr *hdr = nlmsg_hdr(msg);
        void *data = nlmsg_data(hdr);
        /*
         * This would use nlmsg_datalen() or the older nlmsg_len() if
         * only libnl were to maintain a stable API.. Neither will work
         * with all released versions, so just calculate the length
         * here.
         */
        int len = hdr->nlmsg_len - NLMSG_HDRLEN;

        memset(data, 0, len);
    }
}


static int send_and_recv(struct nl80211_global *global,
        struct nl_handle *nl_handle, struct nl_msg *msg,
        int (*valid_handler)(struct nl_msg *, void *),
        void *valid_data)
{
    struct nl_cb *cb;
    int err = -ENOMEM;

    if (!msg)
        return -ENOMEM;

    cb = nl_cb_clone(global->nl_cb);
    if (!cb)
        goto out;

    err = nl_send_auto_complete(nl_handle, msg);
    if (err < 0)
        goto out;

    err = 1;

    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

    if (valid_handler)
        nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
                valid_handler, valid_data);

    while (err > 0) {
        int res = nl_recvmsgs(nl_handle, cb);
        if (res < 0) {
            icm_printf("nl80211: %s->nl_recvmsgs failed: %d\n",
                    __func__, res);
        }
    }
out:
    nl_cb_put(cb);
    if (!valid_handler && valid_data == (void *) -1)
        nl80211_nlmsg_clear(msg);
    nlmsg_free(msg);
    return err;
}


int send_and_recv_msgs(struct nl80211_global *global,
        struct nl_msg *msg,
        int (*valid_handler)(struct nl_msg *, void *),
        void *valid_data)
{
    return send_and_recv(global, global->nl, msg,
            valid_handler, valid_data);
}


struct family_data {
    const char *group;
    int id;
};


static int family_handler(struct nl_msg *msg, void *arg)
{
    struct family_data *res = arg;
    struct nlattr *tb[CTRL_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *mcgrp;
    int i;

    nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb[CTRL_ATTR_MCAST_GROUPS])
        return NL_SKIP;

    nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], i) {
        struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];
        nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcgrp),
                nla_len(mcgrp), NULL);
        if (!tb2[CTRL_ATTR_MCAST_GRP_NAME] ||
                !tb2[CTRL_ATTR_MCAST_GRP_ID] ||
                strncmp(nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]),
                    res->group,
                    nla_len(tb2[CTRL_ATTR_MCAST_GRP_NAME])) != 0)
            continue;
        res->id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
        break;
    };

    return NL_SKIP;
}


static int nl_get_multicast_id(struct nl80211_global *global,
        const char *family, const char *group)
{
    struct nl_msg *msg;
    int ret;
    struct family_data res = { group, -ENOENT };

    msg = nlmsg_alloc();
    if (!msg)
        return -ENOMEM;
    if (!genlmsg_put(msg, 0, 0, genl_ctrl_resolve(global->nl, "nlctrl"),
                0, 0, CTRL_CMD_GETFAMILY, 0) ||
            nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family)) {
        nlmsg_free(msg);
        return -1;
    }

    ret = send_and_recv(global, global->nl, msg, family_handler, &res);
    if (ret == 0)
        ret = res.id;
    return ret;
}

void * nl80211_cmd(struct nl80211_global *global,
        struct nl_msg *msg, int flags, uint8_t cmd)
{
    return genlmsg_put(msg, 0, 0, global->nl80211_id,
            0, flags, cmd, 0);
}

static struct nl_msg *
nl80211_ifindex_msg(struct nl80211_global *global, int ifindex,
        int flags, uint8_t cmd)
{
    struct nl_msg *msg;

    msg = nlmsg_alloc();
    if (!msg) {
        icm_printf("nl80211: nl messge allocation failed\n");
        return NULL;
    }

    if (!nl80211_cmd(global, msg, flags, cmd) ||
            nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
        nlmsg_free(msg);
        return NULL;
    }

    return msg;
}

struct nl_msg * nl80211_drv_msg(struct nl80211_data *drv, int flags,
        uint8_t cmd)
{
    return nl80211_ifindex_msg(drv->global, drv->ifindex, flags, cmd);
}

static const char * nl80211_vendor_subcmd_to_string(enum qca_nl80211_vendor_subcmds subcmd)
{
#define C2S(x) case x: return #x;
    switch (subcmd) {
        C2S(QCA_NL80211_VENDOR_SUBCMD_UNSPEC)
            C2S(QCA_NL80211_VENDOR_SUBCMD_TEST)
            C2S(QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION)
            C2S(QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION)
            C2S(QCA_NL80211_VENDOR_SUBCMD_DCC_STATS_EVENT)
            C2S(QCA_NL80211_VENDOR_SUBCMD_LINK_PROPERTIES)
            C2S(QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST)
            C2S(QCA_NL80211_VENDOR_SUBCMD_SETBAND)
            C2S(QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN)
            C2S(QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE)
            C2S(QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS)
        default:
            return "NL80211_VENDOR_SUBCMD_UNKNOWN";
    }
#undef C2S
}

static const char * nl80211_command_to_string(enum nl80211_commands cmd)
{
#define C2S(x) case x: return #x;
    switch (cmd) {
        C2S(NL80211_CMD_UNSPEC)
            C2S(NL80211_CMD_GET_WIPHY)
            C2S(NL80211_CMD_SET_WIPHY)
            C2S(NL80211_CMD_NEW_WIPHY)
            C2S(NL80211_CMD_DEL_WIPHY)
            C2S(NL80211_CMD_GET_INTERFACE)
            C2S(NL80211_CMD_SET_INTERFACE)
            C2S(NL80211_CMD_NEW_INTERFACE)
            C2S(NL80211_CMD_DEL_INTERFACE)
            C2S(NL80211_CMD_GET_KEY)
            C2S(NL80211_CMD_SET_KEY)
            C2S(NL80211_CMD_NEW_KEY)
            C2S(NL80211_CMD_DEL_KEY)
            C2S(NL80211_CMD_GET_BEACON)
            C2S(NL80211_CMD_SET_BEACON)
            C2S(NL80211_CMD_START_AP)
            C2S(NL80211_CMD_STOP_AP)
            C2S(NL80211_CMD_GET_STATION)
            C2S(NL80211_CMD_SET_STATION)
            C2S(NL80211_CMD_NEW_STATION)
            C2S(NL80211_CMD_DEL_STATION)
            C2S(NL80211_CMD_GET_MPATH)
            C2S(NL80211_CMD_SET_MPATH)
            C2S(NL80211_CMD_NEW_MPATH)
            C2S(NL80211_CMD_DEL_MPATH)
            C2S(NL80211_CMD_SET_BSS)
            C2S(NL80211_CMD_SET_REG)
            C2S(NL80211_CMD_REQ_SET_REG)
            C2S(NL80211_CMD_GET_MESH_CONFIG)
            C2S(NL80211_CMD_SET_MESH_CONFIG)
            C2S(NL80211_CMD_SET_MGMT_EXTRA_IE)
            C2S(NL80211_CMD_GET_REG)
            C2S(NL80211_CMD_GET_SCAN)
            C2S(NL80211_CMD_TRIGGER_SCAN)
            C2S(NL80211_CMD_NEW_SCAN_RESULTS)
            C2S(NL80211_CMD_SCAN_ABORTED)
            C2S(NL80211_CMD_REG_CHANGE)
            C2S(NL80211_CMD_AUTHENTICATE)
            C2S(NL80211_CMD_ASSOCIATE)
            C2S(NL80211_CMD_DEAUTHENTICATE)
            C2S(NL80211_CMD_DISASSOCIATE)
            C2S(NL80211_CMD_MICHAEL_MIC_FAILURE)
            C2S(NL80211_CMD_REG_BEACON_HINT)
            C2S(NL80211_CMD_JOIN_IBSS)
            C2S(NL80211_CMD_LEAVE_IBSS)
            C2S(NL80211_CMD_TESTMODE)
            C2S(NL80211_CMD_CONNECT)
            C2S(NL80211_CMD_ROAM)
            C2S(NL80211_CMD_DISCONNECT)
            C2S(NL80211_CMD_SET_WIPHY_NETNS)
            C2S(NL80211_CMD_GET_SURVEY)
            C2S(NL80211_CMD_NEW_SURVEY_RESULTS)
            C2S(NL80211_CMD_SET_PMKSA)
            C2S(NL80211_CMD_DEL_PMKSA)
            C2S(NL80211_CMD_FLUSH_PMKSA)
            C2S(NL80211_CMD_REMAIN_ON_CHANNEL)
            C2S(NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL)
            C2S(NL80211_CMD_SET_TX_BITRATE_MASK)
            C2S(NL80211_CMD_REGISTER_FRAME)
            C2S(NL80211_CMD_FRAME)
            C2S(NL80211_CMD_FRAME_TX_STATUS)
            C2S(NL80211_CMD_SET_POWER_SAVE)
            C2S(NL80211_CMD_GET_POWER_SAVE)
            C2S(NL80211_CMD_SET_CQM)
            C2S(NL80211_CMD_NOTIFY_CQM)
            C2S(NL80211_CMD_SET_CHANNEL)
            C2S(NL80211_CMD_SET_WDS_PEER)
            C2S(NL80211_CMD_FRAME_WAIT_CANCEL)
            C2S(NL80211_CMD_JOIN_MESH)
            C2S(NL80211_CMD_LEAVE_MESH)
            C2S(NL80211_CMD_UNPROT_DEAUTHENTICATE)
            C2S(NL80211_CMD_UNPROT_DISASSOCIATE)
            C2S(NL80211_CMD_NEW_PEER_CANDIDATE)
            C2S(NL80211_CMD_GET_WOWLAN)
            C2S(NL80211_CMD_SET_WOWLAN)
            C2S(NL80211_CMD_START_SCHED_SCAN)
            C2S(NL80211_CMD_STOP_SCHED_SCAN)
            C2S(NL80211_CMD_SCHED_SCAN_RESULTS)
            C2S(NL80211_CMD_SCHED_SCAN_STOPPED)
            C2S(NL80211_CMD_SET_REKEY_OFFLOAD)
            C2S(NL80211_CMD_PMKSA_CANDIDATE)
            C2S(NL80211_CMD_TDLS_OPER)
            C2S(NL80211_CMD_TDLS_MGMT)
            C2S(NL80211_CMD_UNEXPECTED_FRAME)
            C2S(NL80211_CMD_PROBE_CLIENT)
            C2S(NL80211_CMD_REGISTER_BEACONS)
            C2S(NL80211_CMD_UNEXPECTED_4ADDR_FRAME)
            C2S(NL80211_CMD_SET_NOACK_MAP)
            C2S(NL80211_CMD_CH_SWITCH_NOTIFY)
            C2S(NL80211_CMD_START_P2P_DEVICE)
            C2S(NL80211_CMD_STOP_P2P_DEVICE)
            C2S(NL80211_CMD_CONN_FAILED)
            C2S(NL80211_CMD_SET_MCAST_RATE)
            C2S(NL80211_CMD_SET_MAC_ACL)
            C2S(NL80211_CMD_RADAR_DETECT)
            C2S(NL80211_CMD_GET_PROTOCOL_FEATURES)
            C2S(NL80211_CMD_UPDATE_FT_IES)
            C2S(NL80211_CMD_FT_EVENT)
            C2S(NL80211_CMD_CRIT_PROTOCOL_START)
            C2S(NL80211_CMD_CRIT_PROTOCOL_STOP)
            C2S(NL80211_CMD_GET_COALESCE)
            C2S(NL80211_CMD_SET_COALESCE)
            C2S(NL80211_CMD_CHANNEL_SWITCH)
            C2S(NL80211_CMD_VENDOR)
            C2S(NL80211_CMD_SET_QOS_MAP)
            C2S(NL80211_CMD_ADD_TX_TS)
            C2S(NL80211_CMD_DEL_TX_TS)
        default:
            return "NL80211_CMD_UNKNOWN";
    }
#undef C2S
}

static void qca_nl80211_scan_done_event(ICM_INFO_T *picm, u8 *data, size_t len)
{
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SCAN_MAX + 1];
    enum scan_status status;
    struct  nl80211_data *drv = &picm->drv;
    struct nlattr *nl;
    int rem;
    int freqs[MAX_REPORT_FREQS];
    int num_freqs = 0;
    u64 cookie = 0;

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SCAN_MAX,
                (struct nlattr *) data, len, NULL) ||
            !tb[QCA_WLAN_VENDOR_ATTR_SCAN_STATUS] ||
            !tb[QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE])
        return;

    status = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_SCAN_STATUS]);
    if (status >= VENDOR_SCAN_STATUS_MAX)
        return; /* invalid status */
    cookie = nla_get_u64(tb[QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE]);
    if (cookie != drv->vendor_scan_cookie) {
        /* Event from an external scan, get scan results */
        drv->external_scan = 1;
        icm_printf("External scan. Ignore it!\n");
        return;
    } else {
        drv->external_scan = 0;
        if (status == VENDOR_SCAN_STATUS_NEW_RESULTS)
            drv->scan_state = SCAN_COMPLETED;
        else
            drv->scan_state = SCAN_ABORTED;

        drv->vendor_scan_cookie = 0;
    }

    drv->aborted = (status == VENDOR_SCAN_STATUS_ABORTED);

    if (tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES]) {
        char msg[300], *pos, *end;
        int res;

        pos = msg;
        end = pos + sizeof(msg);
        *pos = '\0';

        nla_for_each_nested(nl,
                tb[QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES],
                rem) {
            freqs[num_freqs] = nla_get_u32(nl);
            res = snprintf(pos, end - pos, " %d", freqs[num_freqs]);
            pos += res;
            num_freqs++;
            if (num_freqs == MAX_REPORT_FREQS - 1)
                break;
        }

        drv->freqs = freqs;
        drv->num_freqs = num_freqs;
        icm_printf("nl80211: Scan included frequencies:%s\n", msg);
    }
    // Send Event to ICM application here
    process_nl80211_events(drv->ctx, EVENT_SCAN_RESULTS, NULL);
}

static int qca_nl80211_start_acs_event(ICM_INFO_T *picm, u8 *data, size_t len)
{
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_MAX + 1];
    ICM_CAPABILITY_INFO_T *acs_data;
    struct nlattr *nl, *nl_pcl[3], *cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX + 1];
    int rem, i = 0;
    u32 *freq_list = NULL;
    u16 nl80211_rropavail_info = 0;

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_MAX,
                (struct nlattr *) data, len, NULL))
        return NL_SKIP;

    acs_data = zalloc(sizeof(*acs_data));
    if (acs_data == NULL) {
        icm_printf("nl80211: Failed to allocate memory\n");
        return NL_SKIP;
    }

    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_REASON])
        acs_data->config_reason = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_REASON]);

    acs_data->spectral_capable = !!tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_IS_SPECTRAL_SUPPORTED];
    acs_data->offload_enabled = !!tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_IS_OFFLOAD_ENABLED];
    acs_data->add_chan_stat_supported = !!tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_ADD_CHAN_STATS_SUPPORT];
    acs_data->vap_status = !!tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_AP_UP];

    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_SAP_MODE])
        acs_data->vap_mode = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_SAP_MODE]);

    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_WIDTH])
        acs_data->chan_width = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_WIDTH]);

    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_BAND])
        acs_data->scan_band = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_BAND]);

    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PHY_MODE])
        acs_data->phy_mode = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PHY_MODE]);

    nl = tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_FREQ_LIST];
    if (nl != NULL) {
	acs_data->chan_list_len = nla_len(nl)/sizeof(uint32_t);
	freq_list = nla_data(nl);
	acs_data->chan_list = zalloc(acs_data->chan_list_len);
        if (acs_data->chan_list == NULL) {
            icm_printf("nl80211: Failed to allocate memory\n");
	    if (acs_data)
		free(acs_data);
            return NL_SKIP;
        }

	for (i = 0; i < acs_data->chan_list_len; i++)
            acs_data->chan_list[i] = icm_convert_mhz2channel(freq_list[i]);
    }

    nl = tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL];
    if (nl != NULL) {
        i = 0;
        nla_for_each_nested(nl, tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL], rem)
            i++;
    }
    nl = tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL];
    if (nl != NULL) {
        acs_data->pcl.len = i;
        acs_data->pcl.list = zalloc(acs_data->pcl.len);
        if (acs_data->pcl.list == NULL) {
            icm_printf("nl80211: Failed to allocate memory\n");
	    if (acs_data->chan_list)
	        free(acs_data->chan_list);
	    if (acs_data)
	        free(acs_data);
            return NL_SKIP;
        }

        acs_data->pcl.weight = zalloc(acs_data->pcl.len);
        if (acs_data->pcl.weight == NULL) {
            icm_printf("nl80211: Failed to allocate memory\n");
	    if (acs_data->chan_list)
	        free(acs_data->chan_list);
	    if (acs_data->pcl.list)
	        free(acs_data->pcl.list);
	    if (acs_data)
	        free(acs_data);
            return NL_SKIP;
        }

        i = 0;
        nla_for_each_nested(nl, tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_PCL], rem) {
            if (nla_parse(nl_pcl, 2, nla_data(nl), nla_len(nl), NULL)) {
                icm_printf("nl80211: failed to parse PCL info\n");
	    if (acs_data->chan_list)
	        free(acs_data->chan_list);
	    if (acs_data->pcl.list)
	        free(acs_data->pcl.list);
	    if (acs_data->pcl.weight)
	        free(acs_data->pcl.weight);
	    if (acs_data)
	        free(acs_data);
                return NL_SKIP;
            }
            /* <channel (u8), weight (u8)> */
            if (nl_pcl[QCA_WLAN_VENDOR_ATTR_PCL_CHANNEL])
                acs_data->pcl.list[i] = nla_get_u8(nl_pcl[QCA_WLAN_VENDOR_ATTR_PCL_CHANNEL]);
            if (nl_pcl[QCA_WLAN_VENDOR_ATTR_PCL_WEIGHT])
                acs_data->pcl.weight[i] = nla_get_u8(nl_pcl[QCA_WLAN_VENDOR_ATTR_PCL_WEIGHT]);
            i++;
        }
    }

    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_INFO]) {
        icm_printf("nl80211: Channel information missing\n");
	if (acs_data->chan_list)
	    free(acs_data->chan_list);
	if (acs_data->pcl.list)
	    free(acs_data->pcl.list);
	if (acs_data->pcl.weight)
	    free(acs_data->pcl.weight);
	if (acs_data)
	    free(acs_data);
        return NL_SKIP;
    }

    i = 0;
    nla_for_each_nested(nl, tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_CHAN_INFO], rem) {
        if (nla_parse(cinfo, QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX,
                    nla_data(nl), nla_len(nl), NULL)) {
            icm_printf("nl80211: failed to parse channel info\n");
	    if (acs_data->chan_list)
		free(acs_data->chan_list);
	    if (acs_data->pcl.list)
		free(acs_data->pcl.list);
	    if (acs_data->pcl.weight)
		free(acs_data->pcl.weight);
	    if (acs_data)
		free(acs_data);
            return NL_SKIP;
        }

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ]) {
            acs_data->chan_info[i].ic_freq = nla_get_u16(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FREQ]);
            acs_data->chan_info[i].ic_ieee = icm_convert_mhz2channel(acs_data->chan_info[i].ic_freq);
        }

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS])
            acs_data->chan_info[i].ic_flags = nla_get_u32(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAG_EXT])
            acs_data->chan_info[i].ic_flagext = nla_get_u32(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAG_EXT]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_REG_POWER])
            acs_data->chan_info[i].ic_maxregpower = (s8) nla_get_u32(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_REG_POWER]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_POWER])
            acs_data->chan_info[i].ic_maxpower = (s8) nla_get_u32(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MAX_POWER]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MIN_POWER])
            acs_data->chan_info[i].ic_minpower = (s8) nla_get_u32(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_MIN_POWER]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_REG_CLASS_ID])
            acs_data->chan_info[i].ic_regClassId = nla_get_u8(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_REG_CLASS_ID]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_ANTENNA_GAIN])
            acs_data->chan_info[i].ic_antennamax = nla_get_u8(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_ANTENNA_GAIN]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_0])
            acs_data->chan_info[i].ic_vhtop_ch_freq_seg1 = nla_get_u8(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_0]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_1])
            acs_data->chan_info[i].ic_vhtop_ch_freq_seg2 = nla_get_u8(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_VHT_SEG_1]);

        if (cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS_2])
            acs_data->chan_info[i].ic_flags |= (((u_int64_t)(nla_get_u32(cinfo[QCA_WLAN_VENDOR_EXTERNAL_ACS_EVENT_CHAN_INFO_ATTR_FLAGS_2]))) << 32);

        i++;
    }

    acs_data->pcl.policy = -1;
    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_POLICY])
        acs_data->pcl.policy = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_POLICY]);

    if (tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_RROPAVAIL_INFO]) {
        nl80211_rropavail_info =
            nla_get_u16(\
                    tb[QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_EVENT_RROPAVAIL_INFO]);

        switch (nl80211_rropavail_info) {
            case QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_UNAVAILABLE:
                acs_data->rropavail_info = ICM_RROPAVAIL_INFO_UNAVAILABLE;
                break;
            case QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_EXTERNAL_ACS_START:
                acs_data->rropavail_info =
                    ICM_RROPAVAIL_INFO_EXTERNAL_ACS_START;
                break;
            case QCA_WLAN_VENDOR_ATTR_RROPAVAIL_INFO_VSCAN_END:
                acs_data->rropavail_info = ICM_RROPAVAIL_INFO_VSCAN_END;
                break;
            default:
                icm_printf("Warning: Unknown value %hu for RROP availability "
                           "information. Treating RROP as unavailable.\n",
                           nl80211_rropavail_info);
                acs_data->rropavail_info = ICM_RROPAVAIL_INFO_UNAVAILABLE;
        }
    }

    // Send Event to ICM application here
    process_nl80211_events(picm, EVENT_START_ACS, acs_data);

    if (acs_data->chan_list)
        free(acs_data->chan_list);
    if (acs_data->pcl.list)
        free(acs_data->pcl.list);
    if (acs_data->pcl.weight)
        free(acs_data->pcl.weight);
    if (acs_data)
        free(acs_data);

    return 0;
}

static void nl80211_vendor_event_qca(ICM_INFO_T *picm, u32 subcmd,
        u8 *data, size_t len)
{
    switch (subcmd) {
        case QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS:
            qca_nl80211_start_acs_event(picm, data, len);
            break;
        case QCA_NL80211_VENDOR_SUBCMD_SCAN_DONE:
            qca_nl80211_scan_done_event(picm, data, len);
            break;
        default:
            icm_printf("Unknown vendor subcmd\n");
            break;
    }
}

static void nl80211_vendor_event(ICM_INFO_T *picm, struct nlattr **tb)
{
    u32 vendor_id, subcmd, wiphy = 0;
    u8 *data = NULL;
    size_t len = 0;

    if (!tb[NL80211_ATTR_VENDOR_ID] ||
            !tb[NL80211_ATTR_VENDOR_SUBCMD])
        return;

    vendor_id = nla_get_u32(tb[NL80211_ATTR_VENDOR_ID]);
    subcmd = nla_get_u32(tb[NL80211_ATTR_VENDOR_SUBCMD]);

    if (tb[NL80211_ATTR_WIPHY])
        wiphy = nla_get_u32(tb[NL80211_ATTR_WIPHY]);

    icm_printf("nl80211: Vendor event: wiphy=%u vendor_id=0x%x subcmd=%u (%s)\n",
            wiphy, vendor_id, subcmd, nl80211_vendor_subcmd_to_string(subcmd));

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
        len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);
    }

    switch (vendor_id) {
        case OUI_QCA:
            nl80211_vendor_event_qca(picm, subcmd, data, len);
            break;
        default:
            icm_printf("nl80211: Ignore unsupported vendor event\n");
            break;
    }

}

static void do_process_drv_event(ICM_INFO_T *picm, int cmd,
        struct nlattr **tb)
{
    //Process all events in this function. Check supplicant implemenation.
    icm_printf("nl80211: Drv Event %d (%s) received for %s\n",
            cmd, nl80211_command_to_string(cmd), picm->dev_ifname);

    switch(cmd) {
        case NL80211_CMD_VENDOR:
            nl80211_vendor_event(picm, tb);
            break;
        default:
            icm_printf("nl80211: Ignored unknown event (cmd=%d)\n", cmd);
            break;
    }
}

static int icm_is_intf_radiochild(ICM_INFO_T *picm, int ifidx) {
    char vap[IF_NAMESIZE];
    u_int8_t vapaddr[ETH_ALEN];
    ICM_DEV_INFO_T* pdev = get_pdev();

    if_indextoname(ifidx, vap);
    icm_get_iface_addr(pdev, vap, vapaddr);

    return icm_is_vap_radiochild(vapaddr, picm->radio_addr);
}

#ifndef ICM_RTR_DRIVER
static void validate_icm_trigger_event(ICM_DEV_INFO_T* pdev, int cmd,
        struct nlattr **tb, int ifidx)
{
    int i;
    u32 subcmd;
    char dev_ifname[IFNAMSIZ];
    ICM_INFO_T *picm = NULL;

    if (cmd != NL80211_CMD_VENDOR ||
            !tb[NL80211_ATTR_VENDOR_ID] ||
            !tb[NL80211_ATTR_VENDOR_SUBCMD])
        return;

    subcmd = nla_get_u32(tb[NL80211_ATTR_VENDOR_SUBCMD]);
    if (subcmd != QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS || ifidx == -1)
        return;

    /* For now it is updating all radios, we need to sort this out when
     * MBL supports for multiple concurrent AP interface is enabled.
     * This would work fine if only one EXTERNAL_ACS event is requested
     * at a time. */
    for (i = 0; i < pdev->conf.num_radios; i++) {
        picm = &pdev->icm[i];
        if (if_indextoname(ifidx, dev_ifname) == NULL) {
            icm_printf("nl80211: Error converting ifindex %d to interface "
                       "name. error: %s)", ifidx, strerror(errno));
            return;
        } else if (strcmp(picm->dev_ifname, dev_ifname) != 0) {
            icm_printf("Interface changed from %s (ifindex=%d) to %s (ifindex=%d)."
                       " Re-register nl event handle.", picm->dev_ifname,
                       picm->drv.ifindex, dev_ifname, ifidx);
            os_strlcpy(picm->dev_ifname, dev_ifname, IFNAMSIZ);
            picm->drv.ifindex = ifidx;

            if (nl80211_handle_ifindx_change(picm, pdev->nl80211) < 0)
                icm_printf("Failed to register nl event handle");

        }
    }
}
#endif /* ICM_RTR_DRIVER */

static int process_global_event(struct nl_msg *msg, void *arg)
{
    struct nl80211_global *global = arg;
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    int ifidx = -1;
    u64 wdev_id = 0;
    int wdev_id_set = 0;
    int i;
    ICM_DEV_INFO_T* pdev = global->ctx;
    ICM_INFO_T *picm = NULL;
#ifdef ICM_RTR_DRIVER
    char dev_ifname[IFNAMSIZ];
#endif /* ICM_RTR_DRIVER */

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_IFINDEX])
        ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);
    else if (tb[NL80211_ATTR_WDEV]) {
        wdev_id = nla_get_u64(tb[NL80211_ATTR_WDEV]);
        wdev_id_set = 1;
    }

#ifndef ICM_RTR_DRIVER
    /* Check if this event is to start ACS. Start EXTERNAL_ACS event
     * could be triggered for any interface. ICM should operate on same
     * interface. */
    validate_icm_trigger_event(pdev, gnlh->cmd, tb, ifidx);
#endif /* ICM_RTR_DRIVER */

    /* foreach pdev->icm check if dev_ifname matches with received event
     * ifindex. However for RTR platforms we check if the ifindex falls under
     * the radio registered to the pdev->icm instance, and update the desired
     * dev_ifname into pdev->icm.
     */
    for (i = 0; i < pdev->conf.num_radios; i++) {
        picm = &pdev->icm[i];
        if ((ifidx  == -1 && !wdev_id_set) ||
#ifndef ICM_RTR_DRIVER
                ifidx == picm->drv.ifindex ||
#else
                icm_is_intf_radiochild(picm, ifidx) ||
#endif /* ICM_RTR_DRIVER */
                wdev_id_set) {
#ifdef ICM_RTR_DRIVER
            if (ifidx != -1) {
                if (if_indextoname(ifidx, dev_ifname) == NULL) {
                    icm_printf("nl80211: Error converting ifindex to interface "
                               "name. Ignoring event. (cmd=%d ifindex %d wdev "
                               "0x%llx error: %s).\n",
                        gnlh->cmd, ifidx, (long long unsigned int) wdev_id,
                        strerror(errno));
                    return NL_SKIP;
                }

                os_strlcpy(picm->dev_ifname, dev_ifname, IFNAMSIZ);
            } else {
                /* Currently ICM call flows on RTR require a valid ifidx. We
                 * trap invalid ifidx and ignore event.
                 * XXX: Add support for ifidx = -1 here if required in the
                 * future.
                 */
                icm_printf("nl80211: ifindex = -1 not supported. Ignoring "
                           "event. (cmd=%d wdev 0x%llx).\n",
                        gnlh->cmd, (long long unsigned int) wdev_id);
                return NL_SKIP;
            }

            picm->drv.ifindex = ifidx;
#endif /* ICM_RTR_DRIVER */

            do_process_drv_event(picm, gnlh->cmd, tb);
            return NL_SKIP;
        }
    }
    icm_printf("nl80211: Ignored event (cmd=%d) for foreign interface (ifindex %d wdev 0x%llx)\n",
            gnlh->cmd, ifidx, (long long unsigned int) wdev_id);
    /* end of processising events */
    return NL_SKIP;
}

int driver_nl80211_event_receive(void *ctx, void *handle)
{
    struct nl_cb *cb = ctx;
    int res;

    icm_printf("nl80211: Event message available\n");
    /* nl_recvmsgs (struct nl_sock *sk, struct nl_cb *cb) */
    res = nl_recvmsgs(handle, cb);
    if (res < 0) {
        icm_printf("nl80211: %s->nl_recvmsgs failed: %d\n",
                __func__, res);
    }

    return res;
}


static int nl80211_check_global(struct nl80211_global *global)
{
    struct nl_handle *handle;
    const char *groups[] = { "scan", "mlme", "regulatory", "vendor", NULL };
    int ret = 0;
    unsigned int i;

    icm_printf("nl80211: Re-register nl_event handle.");
    /*
    * Try to re-add memberships to handle case of cfg80211 getting reloaded
    * and all registration having been cleared.
    */
    nl_destroy_handles(&global->nl_event);
    global->nl_event = nl_create_handle(global->nl_cb, "event");
    if (global->nl_event == NULL) {
        nl_destroy_handles(&global->nl_event);
        icm_printf("nl80211: failed to create nl_event handle");
        return -1;
    }

    handle = global->nl_event;
    for (i = 0; groups[i]; i++) {
        ret = nl_get_multicast_id(global, "nl80211", groups[i]);
        if (ret >= 0)
            ret = nl_socket_add_membership(handle, ret);
        if (ret < 0) {
            icm_printf("nl80211: Could not re-add multicast membership for %s events: %d (%s)",
                       groups[i], ret, strerror(-ret));
            return ret;
        }
    }

    return ret;
}

int nl80211_handle_ifindx_change(void *ctx, struct nl80211_global *global) {
    ICM_INFO_T *picm = ctx;

    icm_printf("nl80211: Update nl event handle.");
    if (nl80211_check_global(global) < 0) {
        icm_printf("nl80211: Failed to re-register nl event handle.");
        return -1;
    }

    if (nl80211_get_macaddr(picm, picm->dev_ifname, picm->radio_addr)) {
        icm_printf("nl80211: Failed to get mac address.");
        return -1;
    }
    icm_printf("nl80211: New MAC addr: " MACSTR, MAC2STR(picm->radio_addr));

    return 0;
}

static int driver_nl80211_init_nl_global(struct nl80211_global *global)
{
    int ret;

    global->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (global->nl_cb == NULL) {
        icm_printf("nl80211: Failed to allocate netlink "
                "callbacks\n");
        return -1;
    }

    global->nl = nl_create_handle(global->nl_cb, "nl");
    if (global->nl == NULL)
        goto err;

    global->nl80211_id = genl_ctrl_resolve(global->nl, "nl80211");
    if (global->nl80211_id < 0) {
        icm_printf("nl80211: 'nl80211' generic netlink not "
                "found\n");
        goto err;
    }

    global->nl_event = nl_create_handle(global->nl_cb, "event");
    if (global->nl_event == NULL)
        goto err;

    ret = nl_get_multicast_id(global, "nl80211", "scan");
    if (ret >= 0)
        ret = nl_socket_add_membership(global->nl_event, ret);
    if (ret < 0) {
        icm_printf("nl80211: Could not add multicast "
                "membership for scan events: %d (%s)\n",
                ret, strerror(-ret));
        goto err;
    }

    ret = nl_get_multicast_id(global, "nl80211", "mlme");
    if (ret >= 0)
        ret = nl_socket_add_membership(global->nl_event, ret);
    if (ret < 0) {
        icm_printf("nl80211: Could not add multicast "
                "membership for mlme events: %d (%s)\n",
                ret, strerror(-ret));
        goto err;
    }

    ret = nl_get_multicast_id(global, "nl80211", "regulatory");
    if (ret >= 0)
        ret = nl_socket_add_membership(global->nl_event, ret);
    if (ret < 0) {
        icm_printf("nl80211: Could not add multicast "
                "membership for regulatory events: %d (%s)\n",
                ret, strerror(-ret));
        /* Continue without regulatory events */
    }

    ret = nl_get_multicast_id(global, "nl80211", "vendor");
    if (ret >= 0)
        ret = nl_socket_add_membership(global->nl_event, ret);
    if (ret < 0) {
        icm_printf("nl80211: Could not add multicast "
                "membership for vendor events: %d (%s)\n",
                ret, strerror(-ret));
        /* Continue without vendor events */
    }

    nl_cb_set(global->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
            no_seq_check, NULL);
    nl_cb_set(global->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
            process_global_event, global);
    return 0;
err:
    nl_destroy_handles(&global->nl_event);
    nl_destroy_handles(&global->nl);
    nl_cb_put(global->nl_cb);
    global->nl_cb = NULL;
    return -1;
}

static void nl80211_global_deinit(void *priv)
{
    struct nl80211_global *global = priv;
    if (global == NULL)
        return;
    if (!dl_list_empty(&global->interfaces)) {
        icm_printf("nl80211: %u interface(s) remain at "
                "nl80211_global_deinit\n",
                dl_list_len(&global->interfaces));
    }

    nl_destroy_handles(&global->nl);
    nl_cb_put(global->nl_cb);

    free(global);
}

static void * nl80211_global_init(void *ctx)
{
    struct nl80211_global *global;

    global = zalloc(sizeof(*global));
    if (global == NULL)
        return NULL;
    global->ctx = ctx;
    dl_list_init(&global->interfaces);
    global->if_add_ifindex = -1;

    if (driver_nl80211_init_nl_global(global) < 0)
        goto err;

    return global;

err:
    nl80211_global_deinit(global);
    return NULL;
}



int init_driver_netlink(void *ctx)
{
    ICM_DEV_INFO_T *pdev = (ICM_DEV_INFO_T *)ctx;

    pdev->nl80211 = nl80211_global_init(ctx);
    if (pdev->nl80211 == NULL)
        return -1;

    return 0;
}

int deinit_driver_netlink(void *ctx)
{
    ICM_DEV_INFO_T *pdev = (ICM_DEV_INFO_T *)ctx;

    if (pdev)
        nl80211_global_deinit(pdev->nl80211);

    return 0;
}

int driver_nl80211_abort_scan(void *data)
{
    int ret = 0;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;

    icm_printf("nl80211: Abort scan\n");
    msg = nl80211_drv_msg(drv, 0, NL80211_CMD_ABORT_SCAN);
    ret = send_and_recv_msgs(drv->global, msg, NULL, NULL);
    if (ret) {
        icm_printf("nl80211: Abort scan failed: ret=%d (%s)\n", ret, strerror(-ret));
    }

    return ret;
}


static int scan_cookie_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    u64 *cookie = arg;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_SCAN_MAX + 1];

        nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_SCAN_MAX,
                nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE])
            *cookie = nla_get_u64(
                    tb_vendor[QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE]);
    }

    return NL_SKIP;
}


int driver_nl80211_vendor_scan(void *data, struct nl80211_scan_config *params)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    struct nlattr *attr;
    size_t i;
    u64 cookie = 0;

    icm_printf("nl80211: Start vendor scan request\n");

    if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    if (params->scan_ies) {
        icm_printf("nl80211: Scan extra IEs\n");
        if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_SCAN_IE,
                    params->scan_ies_len, params->scan_ies))
            goto fail;
    }

    if (params->freqs) {
        struct nlattr *freqs;

        freqs = nla_nest_start(msg,
                QCA_WLAN_VENDOR_ATTR_SCAN_FREQUENCIES);
        if (freqs == NULL)
            goto fail;
        for (i = 0; params->freqs[i]; i++) {
            icm_printf("nl80211: Scan frequency %u MHz\n", params->freqs[i]);
            if (nla_put_u32(msg, i + 1, params->freqs[i]))
                goto fail;
        }
        nla_nest_end(msg, freqs);

        free(params->freqs);
    }

    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, scan_cookie_handler, &cookie);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: Vendor scan trigger failed: ret=%d (%s)\n", ret, strerror(-ret));
        goto fail;
    }
    drv->vendor_scan_cookie = cookie;
    drv->scan_state = SCAN_REQUESTED;

    icm_printf("nl80211: Vendor scan requested (ret=%d) - scan timeout 30 seconds, scan cookie:0x%llx\n",
            ret, (long long unsigned int) cookie);

fail:
    nlmsg_free(msg);
    return ret;

}


static int get_noise_for_scan_results(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *sinfo[NL80211_SURVEY_INFO_MAX + 1];
    static struct nla_policy survey_policy[NL80211_SURVEY_INFO_MAX + 1] = {
        [NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
        [NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8 },
    };
    struct nl80211_noise_info *info = arg;

    if (info->count >= MAX_NL80211_NOISE_FREQS)
        return NL_STOP;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb[NL80211_ATTR_SURVEY_INFO]) {
        icm_printf("nl80211: Survey data missing\n");
        return NL_SKIP;
    }

    if (nla_parse_nested(sinfo, NL80211_SURVEY_INFO_MAX,
                tb[NL80211_ATTR_SURVEY_INFO],
                survey_policy)) {
        icm_printf("nl80211: Failed to parse nested "
                "attributes\n");
        return NL_SKIP;
    }

    if (!sinfo[NL80211_SURVEY_INFO_NOISE])
        return NL_SKIP;

    if (!sinfo[NL80211_SURVEY_INFO_FREQUENCY])
        return NL_SKIP;

    info->freq[info->count] =
        nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]);
    info->noise[info->count] =
        (s8) nla_get_u8(sinfo[NL80211_SURVEY_INFO_NOISE]);
    if (sinfo[NL80211_SURVEY_INFO_TIME])
        info->time[info->count] = nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME]);
    if (sinfo[NL80211_SURVEY_INFO_TIME_BUSY])
        info->time_busy[info->count] = nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_BUSY]);
    if (sinfo[NL80211_SURVEY_INFO_TIME_TX])
        info->time_tx[info->count] = nla_get_u64(sinfo[NL80211_SURVEY_INFO_TIME_TX]);

    info->count++;

    return NL_SKIP;
}


static int nl80211_get_noise_for_scan_results(struct nl80211_data *drv,
        struct nl80211_noise_info *info)
{
    struct nl_msg *msg;

    memset(info, 0, sizeof(*info));
    msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);
    return send_and_recv_msgs(drv->global, msg, get_noise_for_scan_results, info);
}


int bss_info_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
        [NL80211_BSS_BSSID] = { .type = NLA_UNSPEC },
        [NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
        [NL80211_BSS_TSF] = { .type = NLA_U64 },
        [NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
        [NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
        [NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
        [NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
        [NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
        [NL80211_BSS_STATUS] = { .type = NLA_U32 },
        [NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
        [NL80211_BSS_BEACON_IES] = { .type = NLA_UNSPEC },
    };
    struct nl80211_bss_info_arg *_arg = arg;
    struct scan_results *res = _arg->res;
    struct scan_res **tmp;
    struct scan_res *r;
    const u8 *ie, *beacon_ie;
    size_t ie_len, beacon_ie_len;
    u8 *pos;
    size_t i;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb[NL80211_ATTR_BSS])
        return NL_SKIP;
    if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
                bss_policy))
        return NL_SKIP;
    if (bss[NL80211_BSS_STATUS]) {
        enum nl80211_bss_status status;
        status = nla_get_u32(bss[NL80211_BSS_STATUS]);
        if (status == NL80211_BSS_STATUS_ASSOCIATED &&
                bss[NL80211_BSS_FREQUENCY]) {
            _arg->assoc_freq =
                nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
            icm_printf("nl80211: Associated on %u MHz\n",
                    _arg->assoc_freq);
        }
        if (status == NL80211_BSS_STATUS_IBSS_JOINED &&
                bss[NL80211_BSS_FREQUENCY]) {
            _arg->ibss_freq =
                nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
            icm_printf("nl80211: IBSS-joined on %u MHz\n",
                    _arg->ibss_freq);
        }
        if (status == NL80211_BSS_STATUS_ASSOCIATED &&
                bss[NL80211_BSS_BSSID]) {
            memcpy(_arg->assoc_bssid,
                    nla_data(bss[NL80211_BSS_BSSID]), ETH_ALEN);
            icm_printf("nl80211: Associated with "
                    MACSTR "\n", MAC2STR(_arg->assoc_bssid));
        }
    }
    if (!res)
        return NL_SKIP;
    if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
        ie = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
        ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    } else {
        ie = NULL;
        ie_len = 0;
    }
    if (bss[NL80211_BSS_BEACON_IES]) {
        beacon_ie = nla_data(bss[NL80211_BSS_BEACON_IES]);
        beacon_ie_len = nla_len(bss[NL80211_BSS_BEACON_IES]);
    } else {
        beacon_ie = NULL;
        beacon_ie_len = 0;
    }

    r = zalloc(sizeof(*r) + ie_len + beacon_ie_len);
    if (r == NULL)
        return NL_SKIP;
    if (bss[NL80211_BSS_BSSID])
        memcpy(r->bssid, nla_data(bss[NL80211_BSS_BSSID]),
                ETH_ALEN);
    if (bss[NL80211_BSS_FREQUENCY])
        r->freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
    if (bss[NL80211_BSS_BEACON_INTERVAL])
        r->beacon_int = nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]);
    if (bss[NL80211_BSS_CAPABILITY])
        r->caps = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
    r->flags |= WPA_SCAN_NOISE_INVALID;
    if (bss[NL80211_BSS_SIGNAL_MBM]) {
        r->level = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
        r->level /= 100; /* mBm to dBm */
        r->flags |= WPA_SCAN_LEVEL_DBM | WPA_SCAN_QUAL_INVALID;
    } else if (bss[NL80211_BSS_SIGNAL_UNSPEC]) {
        r->level = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
        r->flags |= WPA_SCAN_QUAL_INVALID;
    } else
        r->flags |= WPA_SCAN_LEVEL_INVALID | WPA_SCAN_QUAL_INVALID;
    if (bss[NL80211_BSS_TSF])
        r->tsf = nla_get_u64(bss[NL80211_BSS_TSF]);
    if (bss[NL80211_BSS_BEACON_TSF]) {
        u64 tsf = nla_get_u64(bss[NL80211_BSS_BEACON_TSF]);
        if (tsf > r->tsf)
            r->tsf = tsf;
    }
    if (bss[NL80211_BSS_SEEN_MS_AGO])
        r->age = nla_get_u32(bss[NL80211_BSS_SEEN_MS_AGO]);
    r->ie_len = ie_len;
    pos = (u8 *) (r + 1);
    if (ie) {
        memcpy(pos, ie, ie_len);
        pos += ie_len;
    }
    r->beacon_ie_len = beacon_ie_len;
    if (beacon_ie)
        memcpy(pos, beacon_ie, beacon_ie_len);

    if (bss[NL80211_BSS_STATUS]) {
        enum nl80211_bss_status status;
        status = nla_get_u32(bss[NL80211_BSS_STATUS]);
        switch (status) {
            case NL80211_BSS_STATUS_ASSOCIATED:
                r->flags |= WPA_SCAN_ASSOCIATED;
                break;
            default:
                break;
        }
    }

    /*
     * cfg80211 maintains separate BSS table entries for APs if the same
     * BSSID,SSID pair is seen on multiple channels. wpa_supplicant does
     * not use frequency as a separate key in the BSS table, so filter out
     * duplicated entries. Prefer associated BSS entry in such a case in
     * order to get the correct frequency into the BSS table. Similarly,
     * prefer newer entries over older.
     */
    for (i = 0; i < res->num; i++) {
        const u8 *s1, *s2;
        if (memcmp(res->res[i]->bssid, r->bssid, ETH_ALEN) != 0)
            continue;

        s1 = get_ie((u8 *) (res->res[i] + 1),
                res->res[i]->ie_len, WLAN_EID_SSID);
        s2 = get_ie((u8 *) (r + 1), r->ie_len, WLAN_EID_SSID);
        if (s1 == NULL || s2 == NULL || s1[1] != s2[1] ||
                memcmp(s1, s2, 2 + s1[1]) != 0)
            continue;

        /* Same BSSID,SSID was already included in scan results */
        icm_printf("nl80211: Remove duplicated scan result "
                "for " MACSTR "\n", MAC2STR(r->bssid));

        if (((r->flags & WPA_SCAN_ASSOCIATED) &&
                    !(res->res[i]->flags & WPA_SCAN_ASSOCIATED)) ||
                r->age < res->res[i]->age) {
            free(res->res[i]);
            res->res[i] = r;
        } else
            free(r);
        return NL_SKIP;
    }

    tmp = realloc(res->res, (res->num + 1) * sizeof(struct scan_res *));
    if (tmp == NULL) {
        free(r);
        return NL_SKIP;
    }
    tmp[res->num++] = r;
    res->res = tmp;

    return NL_SKIP;
}

void scan_results_free(struct scan_results *res)
{
    size_t i;

    if (res == NULL)
        return;

    for (i = 0; i < res->num; i++)
        free(res->res[i]);
    free(res->res);
    free(res);
}


struct scan_results * driver_nl80211_get_scan_results(void *ctx)
{
    ICM_INFO_T *picm = ctx;
    struct nl80211_data *drv = &picm->drv;
    struct scan_results *res;
    struct nl_msg *msg;
    int ret;
    struct nl80211_bss_info_arg arg;

    res = zalloc(sizeof(*res));
    if (res == NULL)
        return NULL;
    if (!(msg = nl80211_drv_msg(drv, NLM_F_DUMP,
                    NL80211_CMD_GET_SCAN))) {
        scan_results_free(res);
        return NULL;
    }

    arg.drv = drv;
    arg.res = res;
    ret = send_and_recv_msgs(drv->global, msg, bss_info_handler, &arg);
    if (ret == 0) {
        icm_printf("nl80211: Received scan results (%lu "
                "BSSes)\n", (unsigned long) res->num);
        if (nl80211_get_noise_for_scan_results(drv, &res->info) == 0) {
            icm_printf("nl80211: Received survey results (%lu "
                    "count)\n", (unsigned long) res->info.count);
        }
        return res;
    }
    icm_printf("nl80211: Scan result fetch failed: ret=%d "
            "(%s)\n", ret, strerror(-ret));
    scan_results_free(res);
    return NULL;

}

int driver_nl80211_set_channel(void *ctx, struct nl80211_channel_config *chan_config)
{
    ICM_INFO_T *picm = ctx;
    struct nl80211_data *drv = &picm->drv;
    struct nl_msg *msg = NULL;
    struct nlattr *attr, *channels, *channel;
    int i, ret = -1;

    icm_printf("nl80211: Send Set channel request\n");

    if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_EXTERNAL_ACS))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_REASON, chan_config->reselect_reason))
        goto fail;

    channels = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_LIST);
    if (channels == NULL)
        goto fail;

    for (i = 0; i < chan_config->num_channel; i++) {
        channel = nla_nest_start(msg, i);
        if (channel == NULL)
            goto fail;

        if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_BAND, chan_config->channel_list[i].band) ||
                nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_PRIMARY, chan_config->channel_list[i].pri_ch) ||
                nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_SECONDARY, chan_config->channel_list[i].ht_sec_ch) ||
                nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_WIDTH, chan_config->channel_list[i].channel_width) ||
                nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG0, chan_config->channel_list[i].vht_seg0_center_ch) ||
                nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_EXTERNAL_ACS_CHANNEL_CENTER_SEG1, chan_config->channel_list[i].vht_seg1_center_ch))
            goto fail;

        nla_nest_end(msg, channel);
    }

    if (chan_config->channel_list)
        free(chan_config->channel_list);

    nla_nest_end(msg, channels);
    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, NULL, NULL);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: set channel failed: ret=%d (%s)\n", ret, strerror(-ret));
    }
fail:
    nlmsg_free(msg);
    return ret;
}


static int nl80211_get_country(struct nl_msg *msg, void *arg)
{
    char *alpha2 = arg;
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb_msg[NL80211_ATTR_REG_ALPHA2]) {
        icm_printf("nl80211: No country information available\n");
        return NL_SKIP;
    }
    strlcpy(alpha2, nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]), 3);
    return NL_SKIP;
}


int driver_nl80211_get_country(void *ctx, char *alpha2)
{
    int ret = 0;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = ctx;
    struct nl80211_data *drv = &picm->drv;

    icm_printf("nl80211: get country code\n");
    msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_REG);
    alpha2[0] = '\0';
    ret = send_and_recv_msgs(drv->global, msg, nl80211_get_country, alpha2);
    if (ret) {
        icm_printf("nl80211: get country code failed: ret=%d (%s)\n", ret, strerror(-ret));
    }

    return ret;
}


static int nl80211_get_reg(struct nl_msg *msg, void *arg)
{
    enum nl80211_dfs_regions *dfs_domain = arg;
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb_msg[NL80211_ATTR_DFS_REGION]) {
        icm_printf("nl80211: No DFS information available\n");
        return NL_SKIP;
    }

    *dfs_domain = nla_get_u8(tb_msg[NL80211_ATTR_DFS_REGION]);
    return NL_SKIP;
}


int driver_nl80211_get_reg_domain(void *ctx, enum nl80211_dfs_regions *dfs_domain)
{
    int ret = 0;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = ctx;
    struct nl80211_data *drv = &picm->drv;

    icm_printf("nl80211: get DFS domain code\n");
    msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_REG);
    ret = send_and_recv_msgs(drv->global, msg, nl80211_get_reg, dfs_domain);
    if (ret) {
        icm_printf("nl80211: get dfs domain code failed: ret=%d (%s)\n", ret, strerror(-ret));
    }

    return ret;
}

static int rropinfo_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *nl = NULL;
    struct nlattr *nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_MAX + 1];
    int rem = 0, i = 0;
    u32 num_rtplinst = 0;
    struct nl80211_rropinfo *rropinfo = (struct nl80211_rropinfo *)arg;

    /* We expect a non-NULL rropinfo. However, we also expect a NULL
     * rropinfo->rtpl since we will be allocating this.
     */
    ICM_ASSERT(rropinfo != NULL);
    ICM_ASSERT(rropinfo->rtpl == NULL);

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_MAX \
                                    + 1];

        nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_RROP_INFO_MAX,
                nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        nl = tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL];
        if (nl != NULL) {
            num_rtplinst = 0;
            nla_for_each_nested(nl,
                    tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL],
                    rem) {
                num_rtplinst++;
            }

            if (!num_rtplinst)
                goto fail;

            rropinfo->num_rtplinst = num_rtplinst;

            rropinfo->rtpl =
                zalloc((rropinfo->num_rtplinst) *
                       sizeof(struct nl80211_rtplinst));

            if (rropinfo->rtpl == NULL) {
                icm_printf("nl80211: Failed to allocate memory for RTPL\n");
                goto fail;
            }

            i = 0;
            nla_for_each_nested(nl,
                    tb_vendor[QCA_WLAN_VENDOR_ATTR_RROP_INFO_RTPL],
                    rem) {
                if (nla_parse(nl_rtplinst,
                            QCA_WLAN_VENDOR_ATTR_RTPLINST_MAX,
                            nla_data(nl), nla_len(nl), NULL)) {
                    icm_printf("nl80211: failed to parse RTPL\n");
                    goto fail;
                }

                if (nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY])
                    rropinfo->rtpl[i].primary =
                        nla_get_u8(nl_rtplinst[\
                                QCA_WLAN_VENDOR_ATTR_RTPLINST_PRIMARY]);

                if (nl_rtplinst[\
                        QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT])
                    rropinfo->rtpl[i].txpower_throughput =
                        (s32)(nla_get_u32(nl_rtplinst[\
                           QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_THROUGHPUT]));

                if (nl_rtplinst[QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE])
                    rropinfo->rtpl[i].txpower_range =
                        (s32)(nla_get_u32(nl_rtplinst[\
                           QCA_WLAN_VENDOR_ATTR_RTPLINST_TXPOWER_RANGE]));

                i++;
            }
        }
    }

    return NL_SKIP;

fail:
    if (rropinfo->rtpl) {
        free(rropinfo->rtpl);
        rropinfo->rtpl = NULL;
    }
    rropinfo->num_rtplinst = 0;

    return NL_SKIP;

}

int driver_nl80211_vendor_get_chan_rropinfo(void *ctx,
        struct nl80211_rropinfo *rropinfo)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = ctx;
    struct nl80211_data *drv = &picm->drv;

    icm_printf("nl80211: Start vendor get RROP info request\n");

    if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_GET_RROP_INFO))
        goto fail;

    ret = send_and_recv_msgs(drv->global, msg, rropinfo_handler, rropinfo);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: Vendor get RROP info request failed: "
                   "ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}

struct wiphy_idx_data {
    /* Index of wiphy to operate on */
    int wiphy_idx;
    /* One of interface mode from nl80211_iftype */
    enum nl80211_iftype nlmode;
    /* MAC address of the interface */
    u8 macaddr[ETH_ALEN];
};

static int netdev_info_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct wiphy_idx_data *info = arg;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
              genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_WIPHY])
        info->wiphy_idx = nla_get_u32(tb[NL80211_ATTR_WIPHY]);

    if (tb[NL80211_ATTR_IFTYPE])
        info->nlmode = nla_get_u32(tb[NL80211_ATTR_IFTYPE]);

    if (tb[NL80211_ATTR_MAC])
        memcpy(info->macaddr, nla_data(tb[NL80211_ATTR_MAC]), ETH_ALEN);

    return NL_SKIP;
}

static enum nl80211_iftype nl80211_get_ifmode(ICM_INFO_T *picm, int ifidx, u8* addr)
{
    struct nl_msg *msg;
    struct nl80211_data *drv = &picm->drv;
    struct wiphy_idx_data data = {
            .nlmode = NL80211_IFTYPE_UNSPECIFIED,
    };

    memset(data.macaddr, 0, ETH_ALEN);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_GET_INTERFACE)))
        return NL80211_IFTYPE_UNSPECIFIED;

    if (send_and_recv_msgs(drv->global, msg, netdev_info_handler, &data) == 0) {
        if (addr != NULL)
            memcpy(addr, data.macaddr, ETH_ALEN);
        return data.nlmode;

    }
    return NL80211_IFTYPE_UNSPECIFIED;
}

int nl80211_get_macaddr(void *ctx, const char *ifname, u8* addr)
{
    ICM_INFO_T *picm = ctx;
    int ifidx = if_nametoindex(ifname);

    enum nl80211_iftype mode = nl80211_get_ifmode(picm, ifidx, addr);
    if (addr == NULL || is_zero_ether_addr(addr)) {
        icm_printf("Failed to get macaddr for %s (%d) mode=%d", ifname,
                   ifidx, mode);
        return -1;
    }

    return 0;
}


#ifdef ICM_RTR_DRIVER

int driver_set_wifi_priv_int_param(void *data,
                                   const char *ifname,
                                   int param,
                                   int32_t val)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    struct nlattr *attr;
    int ifidx;

    ifidx = if_nametoindex(ifname);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    if (nla_put_u32(msg,
                    QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
                    QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS))
        goto fail;

    if (nla_put_u32(msg,
                    QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE,
                    param))
        goto fail;

    if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA,
                sizeof(val), &val))
        goto fail;

    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, NULL, NULL);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: set radio params failed ret=%d (%s)\n", ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;
}

static int wifi_priv_int_param_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    int32_t *val = arg;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[NL80211_ATTR_MAX_INTERNAL + 1];

        nla_parse(tb_vendor, NL80211_ATTR_MAX_INTERNAL,
                nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]) {
            void *temp;
            /* memcpy tb_vendor to data */
            temp = nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]);
            *val = *((int32_t *)temp);
        }
        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE])
            *val = nla_get_u64(
                    tb_vendor[QCA_WLAN_VENDOR_ATTR_SCAN_COOKIE]);
    }

    return NL_SKIP;
}

int driver_get_wifi_priv_int_param(void *data,
                                   const char *ifname,
                                   int param,
                                   int32_t *val)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    struct nlattr *attr;
    int ifidx;

    ifidx = if_nametoindex(ifname);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    if (nla_put_u32(msg,
                    QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
                    QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS))
        goto fail;

    if (nla_put_u32(msg,
                    QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_VALUE,
                    param))
        goto fail;

    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, wifi_priv_int_param_handler, val);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: get radio params failed ret=%d (%s)\n", ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;
}

static int wifi_pri20_blockchanlist_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = NULL;
    struct nl80211_user_chanlist *val = NULL;
    uint8_t *temp_msg_ptr = NULL;
    uint32_t msg_len = 0;
    uint8_t num_channels_to_copy = 0;
    uint8_t i = 0;

    ICM_ASSERT(msg != NULL);
    ICM_ASSERT(arg != NULL);

    val = arg;
    val->num_channels = 0;

    gnlh = nlmsg_data(nlmsg_hdr(msg));

    if (nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL)) {
        icm_printf("nl80211: nla_parse() failed\n");
        return NL_SKIP;
    }

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[NL80211_ATTR_MAX_INTERNAL + 1];

        if (nla_parse(tb_vendor, NL80211_ATTR_MAX_INTERNAL,
                nla_data(nl_vendor), nla_len(nl_vendor), NULL)) {
            icm_printf("nl80211: nla_parse() failed\n");
            return NL_SKIP;
        }

        /*
         * The vendor specific data expected here is an array of zero or more
         * uint8_t IEEE channel number values.
         */
        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH]) {
            msg_len =
                nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_LENGTH]);
        }

        if (msg_len > 0) {
            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]) {
                temp_msg_ptr = (uint8_t *)
                    nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_PARAM_DATA]);
                if (temp_msg_ptr == NULL) {
                    icm_printf("nl80211: Error while getting param data. "
                               "Investigate.\n");
                    return NL_SKIP;
                }

                num_channels_to_copy = MIN_INT(msg_len, ARRAY_LEN(val->chan));
                for (i = 0; i < num_channels_to_copy; i++) {
                    val->chan[i] = temp_msg_ptr[i];
                }
                val->num_channels = num_channels_to_copy;
            }
        }
    }

    return NL_SKIP;
}

/*
 * driver_get_wifi_pri20_blockchanlist - Get radio level primary 20 MHz channel
 * block list
 * @ctx: nl80211 context
 * @ifname: Name of radio interface
 * @val: pointer to nl80211_user_chanlist structure to be populated
 * Return: 0 on success, -1 on error
 */
int driver_get_wifi_pri20_blockchanlist(void *ctx,
                                   const char *ifname,
                                   struct nl80211_user_chanlist *val)
{
    int ret = -1;
    int status = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = NULL;
    struct nl80211_data *drv = NULL;
    struct nlattr *attr = NULL;
    int ifidx = 0;

    ICM_ASSERT(ctx != NULL);
    ICM_ASSERT(ifname != NULL);
    ICM_ASSERT(val != NULL);

    picm = (ICM_INFO_T *)ctx;
    drv = &picm->drv;

    ifidx = if_nametoindex(ifname);
    if (!ifidx) {
        icm_printf("nl80211: if_nametoindex() failed\n");
        goto fail;
    }

    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0,
                    NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION)) {
        icm_printf("nl80211: nl80211_ifindex_msg() failed\n");
        goto fail;
    }

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL) {
        icm_printf("nl80211: nla_nest_start() failed\n");
        goto fail;
    }

    if (nla_put_u32(msg,
                    QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND,
                    QCA_NL80211_VENDOR_SUBCMD_GET_PRI20_BLOCKCHANLIST)) {
        icm_printf("nl80211: nla_put_u32() failed\n");
        goto fail;
    }

    nla_nest_end(msg, attr);

    status = send_and_recv_msgs(drv->global, msg,
                    wifi_pri20_blockchanlist_handler,
                    val);
    msg = NULL;

    if (status) {
        icm_printf("nl80211: get command for radio pri20 blocked channel list "
                   "failed status=%d (%s)\n", status, strerror(-status));
        goto fail;
    } else {
        ret = 0;
    }

fail:
    nlmsg_free(msg);
    return ret;
}
#endif /* RTR_DRIVER */

#ifdef WLAN_SPECTRAL_ENABLE
static int spectral_scan_cookie_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    u64 *cookie = arg;

    ICM_ASSERT(cookie);
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX + 1];

        nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
                nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE])
            *cookie = nla_get_u64(
                    tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE]);
    }

    return NL_SKIP;
}

int driver_nl80211_start_spectral_scan(void *data, const char *radio_name)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    struct nlattr *attr;
    u64 cookie;
    int ifidx;

    icm_printf("nl80211: Start Spectral scan request\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    nla_put_u32(
        msg,
        QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE,
        QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_SCAN);
    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, spectral_scan_cookie_handler, &cookie);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: start Spectral scan failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}

int driver_nl80211_stop_spectral_scan(void *data, const char *radio_name)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    int ifidx;

    icm_printf("nl80211: Stopping Spectral scan\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                        QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP))
        goto fail;

    ret = send_and_recv_msgs(drv->global, msg, 0, 0);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: stop Spectral scan failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}

int driver_nl80211_set_spectral_params(void *data, const char *radio_name,
                                       SPECTRAL_PARAMS_T *sp)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    struct nlattr *attr;
    int ifidx;
    u64 cookie;

    icm_printf("nl80211: Setting Spectral params\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    if (nla_put_u32(
        msg,
        QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE,
        QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT,
         sp->ss_count))
        goto fail;

    if (nla_put_u32(msg,
      QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD,
      sp->ss_period))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY,
         sp->ss_spectral_pri))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE,
         sp->ss_fft_size))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA,
         sp->ss_gc_ena))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA,
         sp->ss_restart_ena))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF,
         sp->ss_noise_floor_ref))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY,
         sp->ss_init_delay))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR,
         sp->ss_nb_tone_thr))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR,
         sp->ss_str_bin_thr))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE,
         sp->ss_wb_rpt_mode))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE,
         sp->ss_rssi_rpt_mode))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR,
         sp->ss_rssi_thr))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT,
         sp->ss_pwr_format))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE,
         sp->ss_rpt_mode))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE,
         sp->ss_bin_scale))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ,
         sp->ss_dBm_adj))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK,
         sp->ss_chn_mask))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD,
         sp->ss_fft_period))
        goto fail;

    if (nla_put_u32(msg,
         QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT,
         sp->ss_short_report))
        goto fail;

    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, spectral_scan_cookie_handler, &cookie);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: set Spectral params failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}

int driver_nl80211_set_spectral_debug(void *data, const char *radio_name,
                                      int dbglevel)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    struct nlattr *attr;
    int ifidx;

    icm_printf("nl80211: Setting Spectral debug level\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START))
        goto fail;

    attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (attr == NULL)
        goto fail;

    nla_put_u32(
            msg,
            QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE,
            QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_REQUEST_TYPE_CONFIG);

    nla_put_u32(msg,QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DEBUG_LEVEL,
                dbglevel);

    nla_nest_end(msg, attr);

    ret = send_and_recv_msgs(drv->global, msg, 0, 0);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: set Spectral debug level failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}

static int spectral_get_config_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    ICM_ASSERT(arg);
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX + 1];

        nla_parse(tb_vendor,QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
        nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        struct spectral_params *sp = (struct spectral_params *)arg;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT])
            sp->ss_count = nla_get_u32(tb_vendor
                [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD])
            sp->ss_period = nla_get_u32(tb_vendor
            [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY])
            sp->ss_spectral_pri = nla_get_u32(tb_vendor
                [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE])
            sp->ss_fft_size = nla_get_u32(tb_vendor
                [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA])
            sp->ss_gc_ena = nla_get_u32(tb_vendor
                [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA])
            sp->ss_restart_ena = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF])
            sp->ss_noise_floor_ref = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY])
            sp->ss_init_delay = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR])
            sp->ss_nb_tone_thr = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR])
            sp->ss_str_bin_thr = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE])
            sp->ss_wb_rpt_mode = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE])
            sp->ss_rssi_rpt_mode = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR])
            sp->ss_rssi_thr = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT])
            sp->ss_pwr_format = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE])
            sp->ss_rpt_mode = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE])
            sp->ss_bin_scale = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ])
            sp->ss_dBm_adj = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK])
            sp->ss_chn_mask = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD])
            sp->ss_fft_period = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_PERIOD]);

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT])
            sp->ss_short_report = nla_get_u32(tb_vendor
               [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SHORT_REPORT]);
    } else
        perror("vendor data attribute not present in nl_msg");
    return NL_SKIP;
}

int driver_nl80211_get_spectral_params(void *data, const char *radio_name,
                                      SPECTRAL_PARAMS_T *sp)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    int ifidx;

    icm_printf("nl80211: Getting Spectral params\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                        QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG))
        goto fail;

    ret = send_and_recv_msgs(drv->global, msg, spectral_get_config_handler,
                             sp);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: get Spectral params failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}

static int spectral_get_capabilities_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = NULL;

    ICM_ASSERT(msg != NULL);
    ICM_ASSERT(arg != NULL);

    gnlh = nlmsg_data(nlmsg_hdr(msg));

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
        struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_MAX + \
            1];
        struct spectral_caps *scaps = NULL;

        nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_MAX,
            nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        scaps = (struct spectral_caps *)arg;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_PHYDIAG])
            scaps->phydiag_cap = 1;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_RADAR])
            scaps->radar_cap = 1;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_SPECTRAL])
            scaps->spectral_cap = 1;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_ADVANCED_SPECTRAL])
            scaps->advncd_spectral_cap = 1;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN])
            scaps->hw_gen = nla_get_u32(tb_vendor
                [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CAP_HW_GEN]);
    } else
        perror("vendor data attribute not present in nl_msg");

    return NL_SKIP;
}

int driver_nl80211_get_spectral_capabilities(void *data, const char *radio_name,
        struct spectral_caps *scaps)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = NULL;
    struct nl80211_data *drv = NULL;
    int ifidx;

    ICM_ASSERT(data != NULL);
    ICM_ASSERT(radio_name != NULL);
    ICM_ASSERT(scaps != NULL);

    picm = (ICM_INFO_T *)data;
    drv = &picm->drv;

    icm_printf("nl80211: Getting Spectral capabilities\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0,
                    NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                        QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO))
        goto fail;

    ret = send_and_recv_msgs(drv->global, msg,
            spectral_get_capabilities_handler, scaps);
    msg = NULL;

    if (ret) {
        icm_printf("nl80211: get Spectral capabilities failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;
}

static int spectral_get_status_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

    ICM_ASSERT(arg);
    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
            struct nlattr *nl_vendor = tb[NL80211_ATTR_VENDOR_DATA];
            struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MAX + 1];

            nla_parse(tb_vendor,QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_MAX,
                    nla_data(nl_vendor), nla_len(nl_vendor), NULL);

        struct nl80211_spectral_scan_state *status =
                (struct nl80211_spectral_scan_state*)arg;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ACTIVE])
            status->is_active = 1;

        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_STATUS_IS_ENABLED])
            status->is_enabled = 1;
    } else
        perror("vendor data attribute not present in nl_msg");
    return NL_SKIP;
}

int driver_nl80211_spectral_get_status(void *data, const char *radio_name,
                                       struct nl80211_spectral_scan_state *status)
{
    int ret = -1;
    struct nl_msg *msg = NULL;
    ICM_INFO_T *picm = data;
    struct nl80211_data *drv = &picm->drv;
    int ifidx;

    icm_printf("nl80211: Getting Spectral scan status\n");

    ifidx = if_nametoindex(radio_name);
    if (!(msg = nl80211_ifindex_msg(drv->global, ifidx, 0, NL80211_CMD_VENDOR)) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
                        QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS))
        goto fail;

    ret = send_and_recv_msgs(drv->global, msg, spectral_get_status_handler,
                             status);
    msg = NULL;
    if (ret) {
        icm_printf("nl80211: get Spectral status failed ret=%d (%s)\n",
                   ret, strerror(-ret));
        goto fail;
    }

fail:
    nlmsg_free(msg);
    return ret;

}
#endif /* WLAN_SPECTRAL_ENABLE */
