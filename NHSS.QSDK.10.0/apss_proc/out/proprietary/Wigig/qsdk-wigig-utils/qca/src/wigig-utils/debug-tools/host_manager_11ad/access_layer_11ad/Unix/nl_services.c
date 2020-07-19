/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include "nl_services.h"
#include "nl80211_copy.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#ifndef SOL_NETLINK
    #define SOL_NETLINK    270
#endif

#define OUI_QCA 0x001374

enum qca_nl80211_vendor_subcmds_ {
    QCA_NL80211_VENDOR_SUBCMD_UNSPEC = 0,
};

enum qca_wlan_vendor_attr {
    QCA_WLAN_VENDOR_ATTR_TYPE = 7,
    QCA_WLAN_VENDOR_ATTR_BUF = 8,
    QCA_WLAN_VENDOR_ATTR_MAX,
};

/* This should be removed and DriverCommandType enum class should be used instead
   when netlink calls are compiled as C++ */
enum qca_cmd_type {
    DRIVER_CMD_FW_WMI = 0,
    DRIVER_CMD_GENERIC_COMMAND = 1,
    DRIVER_CMD_GET_DRIVER_STATISTICS = 2,
    DRIVER_CMD_REGISTER = 3
};

/* enumeration of generic commands supported by the Driver */
enum wil_nl_60g_generic_cmd {
    NL_60G_GEN_FORCE_WMI_SEND = 0,
    NL_60G_GEN_RADAR_ALLOC_BUFFER = 1,
    NL_60G_GEN_FW_RESET = 2,
};

/* structure with global state, passed to callback handlers */
typedef struct nl_state_t {
    /* callbacks handle for synchronous NL commands */
    struct nl_cb *cb;

    /* nl socket handle for synchronous NL commands */
    struct nl_sock *nl;

    /* nl socket handler for events */
    struct nl_sock *nl_event;

    /* family id for nl80211 events */
    int nl80211_id;

    /* interface index of wigig driver */
    int ifindex;

    /* event answer buffer to be filled */
    struct driver_event_report *driver_event_report_buf;

    /* sent command response */
    uint32_t command_response;

    /* true if driver has ability to publish WMI events and receive wmi CMD */
    bool has_wmi_pub;
} nl_state;

/**
 * nl callback handler for disabling sequence number checking
 */
static int no_seq_check(struct nl_msg *msg, void *arg)
{
    (void)msg;
    (void)arg;
    return NL_OK;
}

/**
 * nl callback handler called on error
 */
static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg)
{
    (void)nla;

    int *ret = (int *)arg;
    *ret = err->error;
    /* "error handler with error: %d\n", *ret */
    return NL_SKIP;
}

/**
 * nl callback handler called after all messages in
 * a multi-message reply are delivered. It is used
 * to detect end of synchronous command/reply
 */
static int finish_handler(struct nl_msg *msg, void *arg)
{
    (void)msg;

    int *ret = (int *)arg;
    *ret = 0;
    return NL_SKIP;
}

/**
 * nl callback handler called when ACK is received
 * for a command. It is also used to detect end of
 * synchronous command/reply
 */
static int ack_handler(struct nl_msg *msg, void *arg)
{
    (void)msg;

    int *err = (int *)arg;
    *err = 0;
    return NL_STOP;
}

/**
 * handler for resolving multicast group (family) id
 * used in nl_get_multicast_id below
 */
struct family_data {
    const char *group;
    int id;
};

static int family_handler(struct nl_msg *msg, void *arg)
{
    struct family_data *res = (struct family_data *)arg;
    struct nlattr *tb[CTRL_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *mcgrp;
    int tmp;

    nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh, 0), NULL);
    if (!tb[CTRL_ATTR_MCAST_GROUPS])
        return NL_SKIP;

    nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], tmp) {
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

/**
 * handler for NL80211_CMD_GET_WIPHY results
 */
static int wiphy_info_handler(struct nl_msg *msg, void* arg)
{
    nl_state *state = (nl_state *)arg;
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *attr;
    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nl80211_vendor_cmd_info *cmd;
    int tmp;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh,0) , NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        nla_for_each_nested(attr, tb[NL80211_ATTR_VENDOR_DATA], tmp) {
            if (nla_len(attr) != sizeof(*cmd)) {
                /* "unexpected vendor cmd info\n" */
                continue;
            }
            cmd = nla_data(attr);
            if (cmd->vendor_id == OUI_QCA &&
                cmd->subcmd ==
                QCA_NL80211_VENDOR_SUBCMD_UNSPEC) {
                state->has_wmi_pub = true;
                break;
            }
        }
    }
    return NL_SKIP;
}

/**
* handler for getting command result value
* Note: The only supported result is 32 bits field
*/
static int command_info_handler(struct nl_msg *msg, void* arg)
{
    nl_state *state = (nl_state *)arg;
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *attr;
    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    uint32_t* response;
    int tmp;

    state->command_response = 0x0; // initialize with failure value

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
        genlmsg_attrlen(gnlh, 0), NULL);

    if (tb[NL80211_ATTR_VENDOR_DATA]) {
        nla_for_each_nested(attr, tb[NL80211_ATTR_VENDOR_DATA], tmp) {
            if (nla_len(attr) != sizeof(uint32_t)) {
                /* "unexpected response\n" */
                continue;
            }
            // otherwise we get the response
            response = (uint32_t*)(nla_data(attr));
            state->command_response = *response;
            break;
        }
    }

    return NL_SKIP;
}

/**
 * send NL command and receive reply synchronously
 */
static int nl_cmd_send_and_recv(struct nl_sock *nl,
                struct nl_cb *cb,
                struct nl_msg *msg,
                int (*valid_handler)(struct nl_msg *, void *),
                void *valid_data)
{
    struct nl_cb *cb_clone = nl_cb_clone(cb);
    int err;

    if (!cb_clone) {
        /* "failed to allocate cb_clone\n" */
        return -ENOMEM;
    }

    err = nl_send_auto_complete(nl, msg);
    if (err < 0) {
        /* "failed to send message, err %d\n", err */
        nl_cb_put(cb_clone);
        return err;
    }

    err = 1;
    nl_cb_err(cb_clone, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb_clone, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb_clone, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
    if (valid_handler) {
        nl_cb_set(cb_clone, NL_CB_VALID, NL_CB_CUSTOM, valid_handler, valid_data);
    }

    while (err > 0) {
        int res = nl_recvmsgs(nl, cb_clone);
        if (res < 0) {
            /* "nl_recvmsgs failed: %d\n", res */
            /* do not exit the loop since similar code in supplicant does not */
        }
    }

    nl_cb_put(cb_clone);
    return err;
}

/**
 * get a multicast group id for registering
 * (such as for vendor events)
 */
static int nl_get_multicast_id(nl_state *state,
                   const char *family, const char *group)
{
    struct nl_msg *msg;
    int ret;
    struct family_data res = { group, -ENOENT };

    msg = nlmsg_alloc();
    if (!msg)
        return -ENOMEM;
    if (!genlmsg_put(msg, 0, 0, genl_ctrl_resolve(state->nl, "nlctrl"),
             0, 0, CTRL_CMD_GETFAMILY, 0) ||
        nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family)) {
        nlmsg_free(msg);
        return -ENOBUFS;
    }

    ret = nl_cmd_send_and_recv(state->nl, state->cb, msg, family_handler, &res);
    if (ret == 0)
        ret = res.id;

    return ret;
}

/**
 * add a multicast group to an NL socket so it
 * can receive events for that group
 */
int nl_socket_add_membership(struct nl_sock *sk, int group_id)
{
    int err;
    int s_fd = nl_socket_get_fd(sk);

    if (s_fd == -1 || group_id < 0)
        return -EINVAL;

    err = setsockopt(s_fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
             &group_id, sizeof(group_id));

    if (err < 0)
        return err;

    return 0;
}

/**
 * handle for vendor events
 */
static int nl_event_handler(struct nl_msg *msg, void *arg)
{
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = (struct genlmsghdr *)
        nlmsg_data(nlmsg_hdr(msg));
    uint32_t cmd;
    struct driver_event_report *evt;

    if (!arg) {
        return NL_SKIP; /* should not happen */
    }

    evt = ((nl_state*)arg)->driver_event_report_buf;

    nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
          genlmsg_attrlen(gnlh, 0) , NULL);

    if (!tb[NL80211_ATTR_VENDOR_ID] ||
        !tb[NL80211_ATTR_VENDOR_SUBCMD] ||
        !tb[NL80211_ATTR_VENDOR_DATA]) {
        return NL_SKIP;
    }

    if (nla_get_u32(tb[NL80211_ATTR_VENDOR_ID]) != OUI_QCA) {
        return NL_SKIP;
    }

    if (nla_parse_nested(tb2, QCA_WLAN_VENDOR_ATTR_MAX,
                tb[NL80211_ATTR_VENDOR_DATA], NULL)) {
        /* "failed to parse vendor command\n" */
        return NL_SKIP;
    }

    cmd = nla_get_u32(tb[NL80211_ATTR_VENDOR_SUBCMD]);
    switch (cmd) {
    case QCA_NL80211_VENDOR_SUBCMD_UNSPEC:
        if (tb2[QCA_WLAN_VENDOR_ATTR_BUF]) {
            uint32_t len = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_BUF]);
            if (len > sizeof(struct driver_event_report)) {
                /* "event respond length is bigger than allocated %d [bytes]\n", sizeof(struct driver_event_report) */
                return NL_SKIP;
            }

            /* evt validity already tested */
            memcpy(evt, nla_data(tb2[QCA_WLAN_VENDOR_ATTR_BUF]), len);
        }
        break;
    default:
        /* "\nunknown event %d\n", cmd */
        break;
    }

    return NL_SKIP;
}

/**
 * destroy the structures for NL communication
 */
static void destroy_nl_globals(nl_state *state)
{
    if (state->nl) {
        nl_socket_free(state->nl);
        state->nl = NULL;
    }
    if (state->nl_event) {
        nl_socket_free(state->nl_event);
        state->nl_event = NULL;
    }
    if (state->cb) {
        nl_cb_put(state->cb);
        state->cb = NULL;
    }
    state->nl80211_id = 0;
}

/**
 * initialize structures for NL communication
 * in case of failure it is the caller responsibility to call destroy_nl_globals
 */
static bool init_nl_globals(nl_state *state)
{
    int group_id = -ENOMEM;

    /* specify NL_CB_DEBUG instead of NL_CB_DEFAULT to get detailed traces of NL messages */
    state->cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (state->cb == NULL) {
        /* "failed to allocate nl callback\n" */
        return false;
    }

    if (nl_cb_set(state->cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL) < 0) {
        return false;
    }

    state->nl = nl_socket_alloc_cb(state->cb);
    if (state->nl == NULL) {
        /* "failed to allocate nl handle\n" */
        return false;
    }

    if (genl_connect(state->nl) < 0) {
        /* "failed to connect to nl handle\n" */
        return false;
    }

    state->nl80211_id = genl_ctrl_resolve(state->nl, "nl80211");
    if (state->nl80211_id < 0) {
        /* "failed to get nl80211 family, error %d\n", state->nl80211_id */
        return false;
    }

    state->nl_event = nl_socket_alloc_cb(state->cb);
    if (state->nl_event == NULL) {
        /* "failed to allocate nl handle for events\n" */
        return false;
    }

    if (genl_connect(state->nl_event) < 0) {
        /* "failed to connect to nl_event handle\n" */
        return false;
    }

    /* register for receiving vendor events */
    group_id = nl_get_multicast_id(state, "nl80211", "vendor");
    if (group_id < 0) {
        /* "could not get vendor multicast id, err %d\n", rc */
        return false;
    }

    if (nl_socket_add_membership(state->nl_event, group_id) < 0) {
        /* "could not register for vendor events, err %d\n", rc */
        return false;
    }

    if (nl_socket_set_nonblocking(state->nl_event) < 0) {
        return false;
    }

    // provide the state to be passed as last argument to the callback
    // it will contain the buffer address
    if (nl_cb_set(state->cb, NL_CB_VALID, NL_CB_CUSTOM, nl_event_handler, state) < 0) {
        return false;
    }

    return true;
}

/**
 * allocate an nl_msg for sending a command
 */

static struct nl_msg *allocate_nl_cmd_msg(int family, int ifindex,
                      int flags, uint8_t cmd)
{
    struct nl_msg *msg = nlmsg_alloc();

    if (!msg) {
        /* "failed to allocate nl msg\n" */
        return NULL;
    }

    if (!genlmsg_put(msg,
              0, // pid (automatic)
              0, // sequence number (automatic)
              family, // family
              0, // user specific header length
              flags, // flags
              cmd, // command
              0) // protocol version
        ) {
        /* "failed to init msg\n" */
        nlmsg_free(msg);
        return NULL;
    }

    if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, (uint32_t)ifindex) < 0) {
        /* "failed to set interface index\n" */
        nlmsg_free(msg);
        return NULL;
    }

    return msg;
}

/**
 * send NL command and receive reply synchronously, for
 * non-blocking sockets
 */
static int nl_cmd_send_and_recv_nonblock(
                struct nl_sock *nl,
                struct nl_cb *cb,
                struct nl_msg *msg)
{
    static const int polling_timeout_msec = 500; /* timeout is in msec. */
    struct nl_cb *cb_clone = nl_cb_clone(cb);
    struct pollfd fds;
    int err;

    if (!cb_clone) {
        /* "failed to allocate cb_clone\n" */
        return -ENOMEM;
    }

    err = nl_send_auto_complete(nl, msg);
    if (err < 0) {
        /* "failed to send message, err %d\n", err */
        nl_cb_put(cb_clone);
        return err;
    }

    err = 1;
    nl_cb_err(cb_clone, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb_clone, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb_clone, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

    memset(&fds, 0, sizeof(fds));
    fds.fd = nl_socket_get_fd(nl);
    fds.events = POLLIN;
    while (err > 0) {
        int res = poll(&fds, 1, polling_timeout_msec);

        if (res == 0) {
            /* timeout */
            err = -1;
            break;
        }
        else if (res < 0) {
            /* "poll error %d\n", res */
            err = -2;
            break;
        }

        if (fds.revents & POLLIN) {
            res = nl_recvmsgs(nl, cb_clone);
            if (res < 0) {
                /* "nl_recvmsgs failed: %d\n", res */
                err = -3;
                break;
            }
        }
    }

    nl_cb_put(cb_clone);
    return err;
}

/**
* get publish_event capability for driver using the
* NL80211_CMD_GET_WIPHY command
*/
static nl_status nl_get_publish_event_capability(nl_state *state)
{
    nl_status res = NL_DRIVER_EVENTS_NOT_SUPPORTED;

    struct nl_msg *msg = allocate_nl_cmd_msg(state->nl80211_id,
        state->ifindex,
        NLM_F_DUMP,
        NL80211_CMD_GET_WIPHY);
    if (msg == NULL) {
        /* "failed to allocate msg for GET_WIPHY\n" */
        return NL_OUT_OF_MEMORY;
    }

    if (nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP) < 0) {
        /* "failed to set params for GET_WIPHY\n" */
        res = NL_FAILED_SETTING_FLAG;
        goto out;
    }

    if (nl_cmd_send_and_recv(state->nl, state->cb, msg, wiphy_info_handler, state) < 0) {
        /* "failed to send GET_WIPHY command, err %d\n", rc */
        res = NL_FAILED_SENDING_NL_COMMAND_BLOCK;
        goto out;
    }

    res = state->has_wmi_pub ? NL_SUCCESS : NL_DRIVER_EVENTS_NOT_SUPPORTED;

out:
    nlmsg_free(msg);
    return res;
}

static nl_status nl_convert_errno_to_status(int err, nl_status default_status)
{
    switch (err) {
        case (-EINVAL): return NL_FAILED_NOT_SUPPORTED_BY_DRIVER;
        case (-EOPNOTSUPP): return NL_FAILED_NOT_SUPPORTED_BY_FW;
        default: return default_status;
    }
}

/*
 * Send command to the Driver
 * Notes:
 * Id represents driver command type (qca_cmd_type enumeration) which is a contract between the Driver and the command initiator.
 * Response is updated only for DRIVER_CMD_GENERIC_COMMAND.
 */
nl_status nl_send_driver_command(nl_state *state, uint32_t id, uint32_t bufLen, const void* pBuffer, uint32_t* pResponse)
{
    nl_status res;
    struct nlattr *vendor_data;
    int send_res;

    struct nl_msg *msg = allocate_nl_cmd_msg(state->nl80211_id,
                        state->ifindex,
                        0,
                        NL80211_CMD_VENDOR);
    if (msg == NULL) {
        /* "failed to allocate msg for GET_WIPHY\n" */
        res = NL_OUT_OF_MEMORY;
        goto out;
    }

    if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) < 0 ||
        nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, QCA_NL80211_VENDOR_SUBCMD_UNSPEC) < 0) {
        /* "unable to set parameters for QCA_NL80211_VENDOR_SUBCMD_UNSPEC\n" */
        res = NL_FAILED_SETTING_FLAG;
        goto out;
    }

    vendor_data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
    if (!vendor_data) {
        /* "fail nla_nest_start for vendor_data\n" */
        res = NL_FAILED_SETTING_FLAG;
        goto out;
    }

    if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_TYPE, id)) {
        /* "unable to set wmi_send QCA_WLAN_VENDOR_ATTR_TYPE\n" */
        res = NL_FAILED_SETTING_COMMAND_ID;
        goto out;
    }

    if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_BUF, (int)bufLen, pBuffer) < 0) {
        /* "unable to set wmi_send\n" */
        res = NL_FAILED_SETTING_COMMAND_PAYLOAD;
        goto out;
    }

    nla_nest_end(msg, vendor_data); /* always returns zero */

    if (pResponse && id == DRIVER_CMD_GENERIC_COMMAND)
    {   // response required, blocking send-receive
        send_res = nl_cmd_send_and_recv(state->nl, state->cb, msg, command_info_handler, state);
        if (send_res < 0) {
            res = nl_convert_errno_to_status(send_res, NL_FAILED_SENDING_NL_COMMAND_BLOCK);
            goto out;
        }

        *pResponse = state->command_response;
    }
    else
    {   // no response expected, non blocking send-receive
        send_res = nl_cmd_send_and_recv_nonblock(state->nl, state->cb, msg);
        if (send_res < 0) {
            res = nl_convert_errno_to_status(send_res, NL_FAILED_SENDING_NL_COMMAND_NON_BLOCK);
            goto out;
        }
    }

    res = NL_SUCCESS;

out:
    nlmsg_free(msg);
    return res;
}

nl_status nl_get_driver_event(nl_state *state, int cancelationFd, struct driver_event_report* pMessageBuf)
{
    /* 'cancelationFd' is a file descriptor for one of the sockets from the cancellation sockets pair */
    /* sockets pair serves as a pipe - a value written to one of its sockets, is also written to the second one */
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = nl_socket_get_fd(state->nl_event);
    fds[0].events |= POLLIN;
    fds[1].fd = cancelationFd;
    fds[1].events |= POLLIN;

    int res = poll(fds, 2, -1); // infinite timeout
    if (res < 0) {
        /* "poll err %d waiting for events\n", res */
        return NL_FAILED_WAITING_FOR_EVENTS;
    }

    if (fds[0].revents & POLLIN) {
        state->driver_event_report_buf = pMessageBuf; /* store report pointer to be used inside the callback*/
        res = nl_recvmsgs(state->nl_event, state->cb);
        if (res < 0) {
            /* "nl_recvmsgs failed: %d\n", res */
            return NL_FAILED_RECEIVE_MESSAGE;
        }
    }
    else
    {
        return NL_NO_EVENTS_RETURNED;
    }

    return NL_SUCCESS;
}

/* Initialize the netlink interface */
nl_status nl_initialize(const char* interfaceName, nl_state** ppState)
{
    int ifindex = 0;
    nl_state* pState = NULL;

    if (!ppState) {
        return NL_INVALID_ARGUMENT;
    }

    ifindex = if_nametoindex(interfaceName);
    if (ifindex == 0) {
        return NL_UNKNOWN_INTERFACE;
    }

    pState = (nl_state*)malloc(sizeof(nl_state));
    if (!pState) {
        return NL_OUT_OF_MEMORY;
    }

    memset(pState, 0, sizeof(*pState));
    pState->ifindex = ifindex;

    /* initialize structures for NL communication */
    if (!init_nl_globals(pState)) {
        nl_release(pState); /*it is the caller responsibility */
        return NL_INITIALIZATION_FAILURE;
    }

    nl_status res = nl_get_publish_event_capability(pState);
    if (res != NL_SUCCESS) {
        nl_release(pState);
        return res;
    }

    *ppState = pState;
    return NL_SUCCESS;
}

void nl_release(nl_state* pState)
{
    if (!pState) {
        return;
    }

    destroy_nl_globals(pState);
    free(pState);
}

/* Enable/Disable commands and events transport on the Driver side */
nl_status nl_enable_driver_commands_transport(nl_state_ptr pState, bool enable)
{
    // send buffer of 4 bytes with 1 to enable and zero to disable
    uint32_t buf = enable ? (uint32_t)1U : (uint32_t)0U;

    return nl_send_driver_command(pState, (uint32_t)DRIVER_CMD_REGISTER, sizeof(buf), &buf, NULL /*no response*/);
}

/* FW Reset through generic driver command */
nl_status nl_fw_reset(nl_state_ptr pState)
{
    uint32_t buf = (uint32_t)NL_60G_GEN_FW_RESET; // payload contains only the generic command id
    return nl_send_driver_command(pState, (uint32_t)DRIVER_CMD_GENERIC_COMMAND, sizeof(buf), &buf, NULL /*no response*/);
}

/* Convert status enumeration value to a string */
const char* nl_convert_status_to_string(nl_status status)
{
    switch (status) {
        case (NL_SUCCESS):                              return "Success";
        case (NL_OUT_OF_MEMORY):                        return "Out of Memory";
        case (NL_INVALID_ARGUMENT):                     return "Invalid Argument";
        case (NL_UNKNOWN_INTERFACE):                    return "Unknown Interface name";
        case (NL_INITIALIZATION_FAILURE):               return "Initialization Failure";
        case (NL_DRIVER_EVENTS_NOT_SUPPORTED):          return "New driver version required...";
        case (NL_FAILED_SETTING_FLAG):                  return "Failed setting netlink flag";
        case (NL_FAILED_WAITING_FOR_EVENTS):            return "Failed waiting for driver events";
        case (NL_FAILED_RECEIVE_MESSAGE):               return "Failed to receive message";
        case (NL_NO_EVENTS_RETURNED):                   return "No event received";
        case (NL_FAILED_SETTING_COMMAND_ID):            return "Failed to send driver command (unable to set command Id)";
        case (NL_FAILED_SETTING_COMMAND_PAYLOAD):       return "Failed to send driver command (unable to set command payload)";
        case (NL_FAILED_SENDING_NL_COMMAND_NON_BLOCK):  return "Failed sending (non blocking) netlink command";
        case (NL_FAILED_SENDING_NL_COMMAND_BLOCK):      return "Failed sending (blocking) netlink command";
        case (NL_FAILED_NOT_SUPPORTED_BY_DRIVER):       return "Command not supported, new driver version required...";
        case (NL_FAILED_NOT_SUPPORTED_BY_FW):           return "Command not supported for FW on the DUT";
        default:                                        return "Unknown Status";
    }
}
