/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

/*
 * Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2015, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#if IFACE_SUPPORT_CFG80211

#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include "common.h"
#include "eloop.h"
#include <errno.h>
#include "drv_nl80211.h"
#include "nl80211_copy.h"
#include "qca-vendor.h"

#if 1
#define ifacemgr_printf(fmt, args...) do { \
    printf("iface_mgr: %s: %d: " fmt "\n", __func__, __LINE__, ## args); \
} while (0)
#else
#define ifacemgr_printf(args...) do { } while (0)
#endif

#define QCA_NL80211_VENDOR_SUBCMD_FWD_MGMT_FRAME 262

static
int process_cfg_event(struct nl_msg *msg, void *arg)
{
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *nla[NL80211_ATTR_MAX + 1];
    struct nlattr *tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
    struct nlattr *tb;
    struct nlk80211_global *g_nl;
    int ifidx = -1;
    u8 *data = NULL;
    size_t len = 0;
    u32 vendor_id,cmd,subcmd = 0;
    void *frame = NULL;
    uint32_t frame_len = 0;

    g_nl = (struct nlk80211_global *)arg;
    nla_parse(nla, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

     if (gnlh->cmd != NL80211_CMD_VENDOR) {
         return -1;
     }

     vendor_id = nla_get_u32(nla[NL80211_ATTR_VENDOR_ID]);
     if (vendor_id != OUI_QCA) {
         return -1;
     }
     cmd = nla_get_u32(nla[NL80211_ATTR_VENDOR_SUBCMD]);
     if (cmd != QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_CONFIGURATION) {
         return -1;
     }
     if (nla[NL80211_ATTR_IFINDEX]) {
         ifidx = nla_get_u32(nla[NL80211_ATTR_IFINDEX]);
     }
     if (ifidx == -1) {
         return -1;
     }
     if (nla[NL80211_ATTR_VENDOR_DATA]) {
         data = nla_data(nla[NL80211_ATTR_VENDOR_DATA]);
         len = nla_len(nla[NL80211_ATTR_VENDOR_DATA]);
     }
     if (nla_parse(tb_array, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
                 (struct nlattr *) data, len, NULL)) {
         return -1;
     }

     subcmd = nla_get_u32(tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_COMMAND]);

     if (subcmd != QCA_NL80211_VENDOR_SUBCMD_FWD_MGMT_FRAME) {
         return -1;
     }

     tb = tb_array[QCA_WLAN_VENDOR_ATTR_CONFIG_GENERIC_DATA];
     if (tb) {
         frame = nla_data(tb);
         frame_len = nla_len(tb);
         g_nl->do_process_driver_event(arg, NULL, frame, frame_len, ifidx);
     }
     return 0;
}

static struct nlk_handle * nlk_create_handle(struct nl_cb *cb, const char *dbg)
{
        struct nlk_handle *handle;
        handle = nlk80211_handle_alloc(cb);
        if (handle == NULL) {
                ifacemgr_printf("Failed to allocate (%s)", dbg);
                return NULL;
        }
        if (genl_connect(handle)) {
                ifacemgr_printf("Failed to connect (%s)", dbg);
                nlk80211_handle_destroy(handle);
                return NULL;
        }
        return handle;
}

static int ack_handle(struct nl_msg *msg, void *arg)
{
        int *err = arg;
        *err = 0;
        return NL_STOP;
}

static int finish_handle(struct nl_msg *msg, void *arg)
{
        int *ret = arg;
        *ret = 0;
        return NL_SKIP;
}

static int error_handle(struct sockaddr_nl *nla, struct nlmsgerr *err,
                         void *arg)
{
        int *ret = arg;
        *ret = err->error;
        ifacemgr_printf("err:%d", err);
        return NL_SKIP;
}

static int no_seq_chk(struct nl_msg *msg, void *arg)
{
        return NL_OK;
}

static void nlk80211_nlmsg_clear(struct nl_msg *msg)
{
        if (msg) {
                struct nlmsghdr *hdr = nlmsg_hdr(msg);
                void *data = nlmsg_data(hdr);
                int len = hdr->nlmsg_len - NLMSG_HDRLEN;

                os_memset(data, 0, len);
        }
}

static int send_and_receive(struct nlk80211_global *g_nl,
                         struct nlk_handle *nlk_handle, struct nl_msg *msg,
                         int (*valid_handle)(struct nl_msg *, void *),
                         void *valid_data)
{
        struct nl_cb *cb;
        int err = -ENOMEM;

        if (!msg)
                return -ENOMEM;

        cb = nl_cb_clone(g_nl->nlk_cb);
        if (!cb)
                goto out;

        err = nl_send_auto_complete(nlk_handle, msg);
        if (err < 0)
                goto out;

        err = 1;

        nl_cb_err(cb, NL_CB_CUSTOM, error_handle, &err);
        nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handle, &err);
        nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handle, &err);

        if (valid_handle)
                nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
                          valid_handle, valid_data);

        while (err > 0) {
                int res = nl_recvmsgs(nlk_handle, cb);
                if (res < 0) {
                        ifacemgr_printf("nl_recvmsgs failed: %d", res);
                }
        }
 out:
        nl_cb_put(cb);
        if (!valid_handle && valid_data == (void *) -1)
                nlk80211_nlmsg_clear(msg);
        nlmsg_free(msg);
        return err;
}

static int family_handle(struct nl_msg *msg, void *arg)
{
        struct family_cnt *res = arg;
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
                    os_strncmp(nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]),
                               res->group,
                               nla_len(tb2[CTRL_ATTR_MCAST_GRP_NAME])) != 0)
                        continue;
                res->id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
                break;
        };

        return NL_SKIP;
}

static int nlk_get_multicast_id(struct nlk80211_global *g_nl,
                               const char *family, const char *group)
{
        struct nl_msg *msg;
        int ret;
        struct family_cnt res = { group, -ENOENT };

        msg = nlmsg_alloc();
        if (!msg)
                return -ENOMEM;
        if (!genlmsg_put(msg, 0, 0, genl_ctrl_resolve(g_nl->nlk, "nlctrl"),
                         0, 0, CTRL_CMD_GETFAMILY, 0) ||
            nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family)) {
                nlmsg_free(msg);
                return -1;
        }

        ret = send_and_receive(g_nl, g_nl->nlk, msg, family_handle, &res);
        if (ret == 0)
                ret = res.id;
        return ret;
}

static void nlk80211_event_receive(int sock, void *eloop_ctx,
                                             void *handle)
{
        struct nl_cb *cb = eloop_ctx;
        int res;
        res = nl_recvmsgs(handle, cb);
        if (res < 0) {
                ifacemgr_printf("nl_recvmsgs failed: %d", res);
        }
}

static void nlk80211_register_eloop_read(struct nlk_handle **handle,
                                        eloop_sock_handler handler,
                                        void *eloop_data)
{
        if (nl_socket_set_buffer_size(*handle, 262144, 0) < 0) {
                ifacemgr_printf("Not able to set nl_socket RX buf size: %s",
                           strerror(errno));
        }
        nl_socket_set_nonblocking(*handle);
        eloop_register_read_sock(nl_socket_get_fd(*handle), handler,
                                 eloop_data, *handle);
        *handle = (void *) (((intptr_t) *handle) ^ ELOOP_SOCKET_INVALID);
}

static void nlk_destroy_handles(struct nlk_handle **handle)
{
        if (*handle == NULL)
                return;
        nlk80211_handle_destroy(*handle);
        *handle = NULL;
}

int nlk80211_create_socket(struct nlk80211_global *g_nl)
{
    int ret = 0;
    g_nl->nlk_cb = nl_cb_alloc(NL_CB_DEFAULT);

    if (g_nl->nlk_cb == NULL) {
        ifacemgr_printf("Failed to allocate netlink callback");
        goto err;
    }
    printf("Creating nl80211_socket to receive NL event\n");
    g_nl->nlk = nlk_create_handle(g_nl->nlk_cb, "nl_handle");
    if (g_nl->nlk == NULL) {
        ifacemgr_printf("Failed to allocate handle");
        goto err1;
    }
    g_nl->nlk80211_id = genl_ctrl_resolve(g_nl->nlk, "nl80211");
    if (g_nl->nlk80211_id < 0) {
        ifacemgr_printf("nl80211 generic nlink not found");
        goto err2;
    }

    g_nl->nlk_event = nlk_create_handle(g_nl->nlk_cb, "nl_event");
    if (g_nl->nlk_event == NULL) {
        goto err2;
    }
    ret = nlk_get_multicast_id(g_nl, "nl80211", "vendor");
    if (ret >= 0)
        ret = nl_socket_add_membership(g_nl->nlk_event, ret);
    if (ret < 0) {
        ifacemgr_printf("Not able to add multicast membership %d (%s)",
                ret, strerror(-ret));
        goto err3;
    }

    nl_cb_set(g_nl->nlk_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
            no_seq_chk, NULL);
    nl_cb_set(g_nl->nlk_cb, NL_CB_VALID, NL_CB_CUSTOM,
            process_cfg_event, g_nl);

    nlk80211_register_eloop_read(&g_nl->nlk_event,
            nlk80211_event_receive,
            g_nl->nlk_cb);

    return 0;
err3:
    nlk_destroy_handles(&g_nl->nlk_event);
err2:
    nlk_destroy_handles(&g_nl->nlk);
err1:
    nl_cb_put(g_nl->nlk_cb);
    g_nl->nlk_cb = NULL;
err:
    return -1;
}

void nlk80211_close_socket(struct nlk80211_global *g_nl)
{
    nlk_destroy_handles(&g_nl->nlk_event);
    nlk_destroy_handles(&g_nl->nlk);
    nl_cb_put(g_nl->nlk_cb);
    g_nl->nlk_cb = NULL;
    return;
}
#endif //IFACE_SUPPORT_CFG80211
