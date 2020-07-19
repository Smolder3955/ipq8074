/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

#include <linux/nl80211.h>
#include <net/if.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#define CMD_TIMEOUT   10

#define RX_BUF_LEN_MAX 10000

/* uncomment to enable debug messages */
/* #define ATH10K_DEBUG 1 */

struct tcmd_rx
{
	/* data lenght, excluding the length header */
	uint32_t data_len;

	/* maximum length RX_BUF_LEN_MAX */
	uint8_t data[0];
};

static struct {
	unsigned int if_index;

	void (*rx_cb) (void *buf);
	bool initialized;
	unsigned char timeout;
	struct sigevent sev;
	timer_t timer;

	struct tcmd_rx *rx_buf;
	unsigned int rx_seq;

	/* netlink stuff */
	struct nl_sock *nl_sock;
	struct nl_cache *nl_cache;
	struct genl_family *nl80211;
} ctxt;

#define warn(...) fprintf(stderr, __VA_ARGS__)

#ifdef ATH10K_DEBUG

#define dbg(...) fprintf(stdout, __VA_ARGS__)

static inline void dbg_dump_nl(void *buf, size_t buf_len)
{
	uint8_t *b = buf;
	char *delimeter;
	int i;

	for (i = 0; i < buf_len; i++) {
		if ((i + 1) % 16 == 0)
			delimeter = "\n";
		else
			delimeter = " ";

		fprintf(stdout, "%02x%s", b[i], delimeter);
	}

	fprintf(stdout, "\n");
}

#else /* ATH10K_DEBUG */

#define dbg(...)

static inline void dbg_dump_nl(void *buf, size_t buf_len)
{
}

#endif /* ATH10K_DEBUG */

/********** ath10k testmode API **********/

/* master location net/wireless/ath/ath10k/testmode.c in kernel */

/* "API" level of the ath10k testmode interface. Bump it after every
 * incompatible interface change. */
#define ATH10K_TESTMODE_VERSION_MAJOR 1

/* Bump this after every _compatible_ interface change, for example
 * addition of a new command or an attribute. */
#define ATH10K_TESTMODE_VERSION_MINOR 0

#define ATH10K_TM_DATA_MAX_LEN		5000

enum ath10k_tm_attr {
	__ATH10K_TM_ATTR_INVALID	= 0,
	ATH10K_TM_ATTR_CMD		= 1,
	ATH10K_TM_ATTR_DATA		= 2,
	ATH10K_TM_ATTR_WMI_CMDID	= 3,
	ATH10K_TM_ATTR_VERSION_MAJOR	= 4,
	ATH10K_TM_ATTR_VERSION_MINOR	= 5,

	/* keep last */
	__ATH10K_TM_ATTR_AFTER_LAST,
	ATH10K_TM_ATTR_MAX		= __ATH10K_TM_ATTR_AFTER_LAST - 1,
};

/* All ath10k testmode interface commands specified in
 * ATH10K_TM_ATTR_CMD */
enum ath10k_tm_cmd {
	/* Returns the supported ath10k testmode interface version in
	 * ATH10K_TM_ATTR_VERSION. Always guaranteed to work. User space
	 * uses this to verify it's using the correct version of the
	 * testmode interface */
	ATH10K_TM_CMD_GET_VERSION = 0,

	/* Boots the UTF firmware, the netdev interface must be down at the
	 * time. */
	ATH10K_TM_CMD_UTF_START = 1,

	/* Shuts down the UTF firmware and puts the driver back into OFF
	 * state. */
	ATH10K_TM_CMD_UTF_STOP = 2,

	/* The command used to transmit a WMI command to the firmware and
	 * the event to receive WMI events from the firmware. Without
	 * struct wmi_cmd_hdr header, only the WMI payload. Command id is
	 * provided with ATH10K_TM_ATTR_WMI_CMDID and payload in
	 * ATH10K_TM_ATTR_DATA.*/
	ATH10K_TM_CMD_WMI = 3,
};

/********** WMI API **********/

#define WMI_10X_END_CMDID 0x9FFF
#define WMI_10X_PDEV_UTF_CMDID (WMI_10X_END_CMDID - 1)

#define WMI_10X_END_EVENTID 0x9FFF
#define WMI_10X_PDEV_UTF_EVENTID (WMI_10X_END_EVENTID - 1)

/* maximum length of event transmitted by the firmware */
#define MAX_UTF_EVENT_LENGTH 2048

/* maximum segment length */
#define MAX_WMI_UTF_LEN      252

typedef struct {
	/* total message length (which consists of multiple segments) */
	uint32_t len;

	/* message id, apparently not used by the firmware */
	uint32_t msgref;

	/* first 4 least significant bits: total number of segments
	 * second 4 bits: current segment number
	 * last 24 bits: unused */
	uint32_t segmentInfo;
	uint32_t pad;
	uint8_t data[0];
} SEG_HDR_INFO_STRUCT;

/********** generic callbacks **********/

static void timer_expire_nl(union sigval sig)
{
	//warn("Timer Expired..\n");
	dbg("Timer Expired..\n");
	ctxt.timeout = 1;
}

int IsDeviceOpened()
{
 return(1);
}

static int cb_error_nl(struct sockaddr_nl *nla, struct nlmsgerr *err,
		    void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int cb_finish(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}

static int cb_ack(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

/********** netlink helpers **********/

struct family_info
{
	const char *group;
	int mcast_grp_id;
};

static int cb_get_family(struct nl_msg *msg, void *arg)
{
	struct nlattr *m[CTRL_ATTR_MCAST_GRP_MAX + 1];
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct family_info *info;
	struct genlmsghdr *genl_hdr;
	struct nlattr *mc_grp;
	int remaining;

	info = arg;
	genl_hdr = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(genl_hdr, 0),
		  genlmsg_attrlen(genl_hdr, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mc_grp, tb[CTRL_ATTR_MCAST_GROUPS], remaining) {
		nla_parse(m, CTRL_ATTR_MCAST_GRP_MAX,
			  nla_data(mc_grp), nla_len(mc_grp), NULL);

		if (!m[CTRL_ATTR_MCAST_GRP_NAME])
			continue;

		if (!m[CTRL_ATTR_MCAST_GRP_ID])
			continue;

		if (strncmp(nla_data(m[CTRL_ATTR_MCAST_GRP_NAME]),
			    info->group,
			    nla_len(m[CTRL_ATTR_MCAST_GRP_NAME])) != 0)
			continue;

		info->mcast_grp_id = nla_get_u32(m[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	}

	return NL_SKIP;
}

static int get_multicast_id(const char *family, const char *group)
{
	int ret, err, ctrl_id;
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct family_info family_info = {
		.group = group,
		.mcast_grp_id = -EIO,
	};

	msg = nlmsg_alloc();
	if (!msg) {
		warn("failed to allocate netlink message\n");
		return -ENOMEM;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		ret = -ENOMEM;
		warn("failed to allocate netlink callback\n");
		goto out_free_msg;
	}

	ctrl_id = genl_ctrl_resolve(ctxt.nl_sock, "nlctrl");

	genlmsg_put(msg, 0, 0, ctrl_id, 0,
		    0, CTRL_CMD_GETFAMILY, 0);

	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = nl_send_auto_complete(ctxt.nl_sock, msg);
	if (ret < 0) {
		warn("failed to send netlink messages: %d\n", ret);
		goto out;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, cb_error_nl, &err);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, cb_get_family, &family_info);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_ack, &err);

	while (err > 0)
		nl_recvmsgs(ctxt.nl_sock, cb);

	if (err < 0) {
		warn("error from get family command: %d\n", err);
		ret = err;
		goto out;
	}

	ret = family_info.mcast_grp_id;

out:
	nl_cb_put(cb);

out_free_msg:
	nlmsg_free(msg);

	return ret;

nla_put_failure:
	warn("failed to put name to get family command\n");
	ret = -ENOMEM;
	goto out;
}

/********** ath10k netlink command wrappers **********/

#ifdef ATH10K_TEST_CMD_STOP

static int utf_stop(void)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct nlattr *nest;
	int ret, err;

	dbg("utf stop\n");

	msg = nlmsg_alloc();
	if (!msg) {
		warn("failed to allocate netlink message (utf stop)\n");
		return 0;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		warn("failed to allocate netlink callback (utf stop)\n");
		goto out_free_msg;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(ctxt.nl80211), 0,
		   0, NL80211_CMD_TESTMODE, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ctxt.if_index);

	nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!nest) {
		warn("failed to start the nest (utf stop)\n");
		goto out;
	}

	NLA_PUT_U32(msg, ATH10K_TM_ATTR_CMD, ATH10K_TM_CMD_UTF_STOP);

	nla_nest_end(msg, nest);

	ret = nl_send_auto_complete(ctxt.nl_sock, msg);
	if (ret < 0) {
		warn("failed to send utf stop command: %d\n", ret);
		goto out;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, cb_error_nl, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, cb_finish, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_ack, &err);

	while (err > 0)
		nl_recvmsgs(ctxt.nl_sock, cb);

	if (err == -ENETDOWN) {
		dbg("driver was not in utf mode\n");
		ret = 0;
		goto out;
	} else if (err < 0) {
		warn("error from utf stop command: %d\n", err);
		ret = err;
		goto out;
	}

	/* if no errors we assume that utf mode was stopped succesfully */
	ret = 0;

out:
	nl_cb_put(cb);

out_free_msg:
	nlmsg_free(msg);

	return ret;

nla_put_failure:
	warn("failed to put utf stop message\n");
	goto out;
}

static int test_utf_stop(void)
{
	int ret;

	ret = utf_stop();
	if (ret) {
		warn("failed to stop utf mode: %d\n", ret);
		return -2;
	}

	warn("utf stop was succesful\n");
	return -3;
}

#endif /* ATH10K_TEST_CMD_STOP */

static int utf_start(void)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct nlattr *nest;
	int ret, err;

	dbg("utf start\n");
	warn("-------------entered into utf_start function------\n");//rm

	msg = nlmsg_alloc();
	if (!msg) {
		warn("failed to allocate netlink message (utf start)\n");
		return 0;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		warn("failed to allocate netlink callback (utf start)\n");
		goto out_free_msg;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(ctxt.nl80211), 0,
		   0, NL80211_CMD_TESTMODE, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ctxt.if_index);

	nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!nest) {
		warn("failed to start the nest (utf start)\n");
		goto out;
	}

	warn("------utf start command is send-----\n");//rm
	NLA_PUT_U32(msg, ATH10K_TM_ATTR_CMD, ATH10K_TM_CMD_UTF_START);

	nla_nest_end(msg, nest);

	ret = nl_send_auto_complete(ctxt.nl_sock, msg);
	warn("------ response from nl_send_auto complete %d---",ret); //rm
	if (ret < 0) {
		warn("failed to send utf start command: %d\n", ret);
		goto out;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, cb_error_nl, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, cb_finish, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_ack, &err);

	while (err > 0)
		nl_recvmsgs(ctxt.nl_sock, cb);

	if (err == -EALREADY) {
		dbg("driver was already in utf mode\n");
		ret = 0;
		goto out;
	} else if (err < 0) {
		warn("error from utf start command: %d\n", err);
		ret = err;
		goto out;
	}

	/* if no errors we assume that utf mode was started succesfully */
        warn("--------utf mode was started successfullly -----\n");//rm
	ret = 0;

out:
	nl_cb_put(cb);

out_free_msg:
	nlmsg_free(msg);

	return ret;

nla_put_failure:
	warn("failed to put utf start message\n");
	goto out;
}

struct testmode_version
{
	uint32_t major;
	uint32_t minor;
};

static int cb_get_version(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *ta[ATH10K_TM_ATTR_MAX + 1];
	struct genlmsghdr *genl_hdr;
	struct testmode_version *version;

	version = arg;
	genl_hdr = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(genl_hdr, 0),
		  genlmsg_attrlen(genl_hdr, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		warn("no nl80211 testdata\n");
		return NL_SKIP;
	}

	if (!tb[NL80211_ATTR_WIPHY]) {
		warn("no wiphy\n");
		return NL_SKIP;
	}

	nla_parse(ta, ATH10K_TM_ATTR_MAX,
		  nla_data(tb[NL80211_ATTR_TESTDATA]),
		  nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	if (!ta[ATH10K_TM_ATTR_VERSION_MAJOR]) {
		warn("no major version attribute\n");
		return NL_SKIP;
	}

	version->major = nla_get_u32(ta[ATH10K_TM_ATTR_VERSION_MAJOR]);

	if (!ta[ATH10K_TM_ATTR_VERSION_MINOR]) {
		warn("no minor version attribute\n");
		return NL_SKIP;
	}

	version->minor = nla_get_u32(ta[ATH10K_TM_ATTR_VERSION_MAJOR]);

	dbg("cb version major %d minor %d\n",
	    version->major, version->minor);

	return NL_SKIP;
}

static int get_version(struct testmode_version *version)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct nlattr *nest;
	int ret, err;

	dbg("get version\n");

	msg = nlmsg_alloc();
	if (!msg) {
		warn("failed to allocate netlink message (get version)\n");
		return 0;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		warn("failed to allocate netlink callback (get version)\n");
		goto out_free_msg;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(ctxt.nl80211), 0,
		   0, NL80211_CMD_TESTMODE, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ctxt.if_index);

	nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!nest) {
		warn("failed to start nest (get version)\n");
		goto out;
	}

	NLA_PUT_U32(msg, ATH10K_TM_ATTR_CMD, ATH10K_TM_CMD_GET_VERSION);

	nla_nest_end(msg, nest);

	ret = nl_send_auto_complete(ctxt.nl_sock, msg);
	if (ret < 0) {
		warn("failed to send get version command: %d\n", ret);
		goto out;
	}

	err = 1;

	/* FIXME: what callbacks do we really require? */
	nl_cb_err(cb, NL_CB_CUSTOM, cb_error_nl, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, cb_finish, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_ack, &err);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, cb_get_version, version);

	while (err > 0)
		nl_recvmsgs(ctxt.nl_sock, cb);

	if (err < 0) {
		warn("error from nl80211 command: %d\n", err);
		goto out;
	}

	/* FIXME: check that utf mode was enabled in ath10k */

out:
	nl_cb_put(cb);

out_free_msg:
	nlmsg_free(msg);

	return 0;

nla_put_failure:
	warn("failed to create get version message\n");
	goto out;
}

static int send_wmi(uint32_t cmd_id, void *buf, int len, int more_seg)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	struct nlattr *nest;
	int ret, err;

	dbg("send wmi buf %p len %d\n", buf, len);

	msg = nlmsg_alloc();
	if (!msg) {
		warn("failed to allocate netlink message (send wmi)\n");
		return -ENOMEM;
	}

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		warn("failed to allocate netlink callback (send wmi)\n");
		ret = -ENOMEM;
		goto out_free_msg;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(ctxt.nl80211), 0,
		   0, NL80211_CMD_TESTMODE, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ctxt.if_index);

	nest = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!nest) {
		warn("failed to start nest (send wmi)\n");
		ret = -ENOMEM;
		goto out;
	}

	NLA_PUT_U32(msg, ATH10K_TM_ATTR_CMD, ATH10K_TM_CMD_WMI);
	NLA_PUT_U32(msg, ATH10K_TM_ATTR_WMI_CMDID, cmd_id);
	NLA_PUT(msg, ATH10K_TM_ATTR_DATA, len, buf);

	nla_nest_end(msg, nest);

	ret = nl_send_auto_complete(ctxt.nl_sock, msg);
	if (ret < 0) {
		warn("failed to send netlink message (send wmi): %d\n", ret);
		goto out;
	}
	if (!more_seg) {
		ret = 0;
		goto out;
	}
	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, cb_error_nl, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, cb_finish, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_ack, &err);

	while (err > 0)
		nl_recvmsgs(ctxt.nl_sock, cb);

	if (err < 0) {
		warn("receiving netlink messages failed (send wmi): %d\n", err);
		goto out;
	}

	ret = 0;

out:
	nl_cb_put(cb);

out_free_msg:
	nlmsg_free(msg);

	return ret;

nla_put_failure:
	warn("failed to put data tonetlink message (send wmi)\n");
	goto out;
}

static int parse_segment(void *seg, size_t seg_len)
{
	SEG_HDR_INFO_STRUCT *seg_hdr;
	unsigned int seg_num, seg_num_total;
	size_t data_len;

	/* ignore msgref, it seems to be unused */

	seg_hdr = seg;
	data_len = seg_len - sizeof(*seg_hdr);
	seg_num = seg_hdr->segmentInfo & 0x0f;
	seg_num_total = (seg_hdr->segmentInfo & 0xf0) >> 4;

	warn("received segment %d/%d seg_len %zd data_len %zd total_len %d\n",
	    seg_num, seg_num_total, seg_len, data_len, seg_hdr->len); //make dbg

	if (seg_num == 0) {
		/* this segment starts a new message */
		ctxt.rx_seq = 0;
		ctxt.rx_buf->data_len = 0;
	}

	if (seg_num != ctxt.rx_seq) {
		warn("received wrong sequence number, expected %d and got %d\n",
		     ctxt.rx_seq, seg_num);
		return -EINVAL;
	}

	if (ctxt.rx_buf->data_len + data_len > RX_BUF_LEN_MAX) {
		warn("rx_buf overflow with %zd bytes\n",
		     ctxt.rx_buf->data_len + data_len);
		return -E2BIG;
	}

	memcpy(ctxt.rx_buf->data + ctxt.rx_buf->data_len, seg_hdr->data, data_len);
	ctxt.rx_buf->data_len += data_len;
	ctxt.rx_seq++;

	if (ctxt.rx_seq != seg_num_total) {
		/* wait to receive all segments */
		dbg("wait more segments (%d/%d)\n", ctxt.rx_seq, seg_num_total);
		return EAGAIN;
	}

	if (ctxt.rx_buf->data_len != seg_hdr->len) {
		warn("all segments received but size doesn't match, expected %d got %d\n",
		     seg_hdr->len, ctxt.rx_buf->data_len);
		return -EMSGSIZE;
	}

	if (ctxt.rx_cb == NULL) {
		warn("rx callback is NULL\n");
		return -EINVAL;
	}

	dbg("call rx_cb rx_buf->data_len %d\n", ctxt.rx_buf->data_len);
	dbg_dump_nl(ctxt.rx_buf, sizeof(struct tcmd_rx) + ctxt.rx_buf->data_len);

	ctxt.rx_cb(ctxt.rx_buf);

	return 0;
}

static int cb_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

int cmd_set_timer_nl()
{
	warn("\n\n------------------cmd_set_timer---added in patch\n\n");
	struct itimerspec exp_time;
	int err;
	dbg("cmd set timer\n");
	
	bzero(&exp_time, sizeof(exp_time));
	exp_time.it_value.tv_sec = CMD_TIMEOUT;
	err = timer_settime(ctxt.timer, 0, &exp_time, NULL);
	ctxt.timeout = 0;
	
	if (err < 0)
		return errno;
	
	return 0;
}
int cmd_stop_timer_nl()
{
	warn("\n\n------------------cmd_stop_timer---added in patch\n\n");
	struct itimerspec exp_time;
	int err;
	dbg("cmd stop timer\n");
	err = timer_gettime(ctxt.timer, &exp_time);
	if (err < 0)
		return errno;
	if (!exp_time.it_value.tv_sec && !exp_time.it_value.tv_nsec)
		return -ETIMEDOUT;
	bzero(&exp_time, sizeof(exp_time));
	err = timer_settime(ctxt.timer, 0, &exp_time, NULL);
	if (err < 0)
		return errno;
	return 0;
}
static int cb_event(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *genl_hdr;
	struct nlattr *tb[NL80211_ATTR_MAX + 1], *ta[ATH10K_TM_ATTR_MAX + 1];
	uint32_t cmd_id, wmi_id;
	//int *err, buf_len;
	int buf_len;
	void *buf;
         warn( "entered into cb_event funtion------\n" );//rm
	dbg("%s()\n", __func__);

	//err = arg;
	genl_hdr = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(genl_hdr, 0),
		  genlmsg_attrlen(genl_hdr, 0), NULL);

	if (!tb[NL80211_ATTR_WIPHY]) {
		warn("no wiphy found\n");
		goto out;
	}

	/* FIXME: check that this is correct wiphy. how to map that with
	 * ctxt.if_index? */

	dbg("phy %d\n", nla_get_u32(tb[NL80211_ATTR_WIPHY]));

	if (genl_hdr->cmd != NL80211_CMD_TESTMODE) {
		warn("wrong event: %d\n", genl_hdr->cmd);
		goto out;
	}

	if (!tb[NL80211_ATTR_TESTDATA]) {
		warn("no nl80211 testdata attribute\n");
		return NL_SKIP;
	}

	nla_parse(ta, ATH10K_TM_ATTR_MAX,
		  nla_data(tb[NL80211_ATTR_TESTDATA]),
		  nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	if (!ta[ATH10K_TM_ATTR_CMD]) {
		warn("ath10k cmd attribute missing\n");
		goto out;
	}

	cmd_id = nla_get_u32(ta[ATH10K_TM_ATTR_CMD]);

	if (cmd_id != ATH10K_TM_CMD_WMI) {
		warn("not wmi event\n");
		goto out;
	}

	wmi_id = nla_get_u32(ta[ATH10K_TM_ATTR_WMI_CMDID]);
	dbg("wmi event id %d\n", wmi_id);
	cmd_stop_timer_nl();
	if (!ta[ATH10K_TM_ATTR_DATA]) {
		warn("no data\n");
		goto out;
	}

	buf = nla_data(ta[ATH10K_TM_ATTR_DATA]);
	buf_len = nla_len(ta[ATH10K_TM_ATTR_DATA]);

	dbg_dump_nl(buf, buf_len);

	if (parse_segment(buf, buf_len) > 0) {
		warn("\n\n calling set timer--patch\n\n");
		cmd_set_timer_nl();
		goto out;
	}

	//*err = 0;
	ctxt.timeout = 1;

out:
	return NL_SKIP;
}

static int wait_events(void)
{
	struct nl_cb *cb;
	//int err;
        warn("-------------Entered into wait events funnction--------- \n");//rm
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		warn("failed to allocate netlink callback (wait events)\n");
		return -ENOMEM;
	}

	/* multicast messages should not do sequence checking */
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, cb_seq_check, NULL);
//
//	err = 1;
  //      warn("calling nl_cb_set in waitevents ---------\n");
//	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, cb_event, &err);
//
//	while (err > 0) {
//		dbg("nl_recvmsgs\n");
//		nl_recvmsgs(ctxt.nl_sock, cb);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, cb_event, NULL);
	if (cmd_set_timer_nl() < 0) {
		nl_cb_put(cb);
		warn("failed to start timer for events\n");
		return -EINVAL;
	}
	while (!ctxt.timeout)
		nl_recvmsgs(ctxt.nl_sock, cb);
	nl_cb_put(cb);

	return 0;
}

/********** athtestcmd API handlers **********/

void cmd_send2(void *buf, int buf_len, unsigned char *respdata, unsigned int *responseNeeded,unsigned int devid)
{
	warn("--------------Entered into the cmd_send2--------- \n");
	int ret, seg_len, seg_num, segments, chunk_len, pos_len, error=0;
	SEG_HDR_INFO_STRUCT *seg_hdr;
	uint8_t seg[sizeof(SEG_HDR_INFO_STRUCT) + MAX_WMI_UTF_LEN], *pos;

	/* athtestcmd.c adds 8 bytes header for the ioctl command, strip
	 * that. But it doesn't modify the length so we keep that
	 * unmodified. */
	buf = (uint8_t *) buf + 8;

	dbg("cmd send buf %p buf_len %d response %d responsedata %d devid %d\n",
	    buf, buf_len, responseNeeded,respdata,devid);

	dbg_dump_nl(buf, buf_len);

	if (!ctxt.initialized) {
		warn("cmd send called but not initialized\n");
		return;
	}

	/* create segments */
	segments = buf_len / MAX_WMI_UTF_LEN;

	if (buf_len - segments * MAX_WMI_UTF_LEN)
		segments++;

	pos = buf;
	pos_len = buf_len;
	seg_num = 0;

	while (pos_len) {
		if (pos_len > MAX_WMI_UTF_LEN)
			chunk_len =  MAX_WMI_UTF_LEN;
		else
			chunk_len = pos_len;

		memset(seg, 0, sizeof(seg));
		warn (" inside cmd_send_nl while(pos_len)\n");//rm

		seg_hdr = (SEG_HDR_INFO_STRUCT *) seg;
		seg_len = sizeof(*seg_hdr) + chunk_len;
		seg_hdr->len = buf_len;
		seg_hdr->segmentInfo |= (segments << 4) & 0xf0;
		seg_hdr->segmentInfo |= seg_num & 0x0f;

		/* msgref is just ignored */
		seg_hdr->msgref = 0;

		memcpy(seg_hdr->data, pos, chunk_len);
		pos += chunk_len;
		pos_len -= chunk_len;
		seg_num++;

		warn("send segment %d/%d seg_len %d\n",
		    seg_num, segments, seg_len);//make dbg

		//ret = send_wmi(WMI_10X_PDEV_UTF_CMDID, seg_hdr, seg_len);
		ret = send_wmi(WMI_10X_PDEV_UTF_CMDID, seg_hdr, seg_len, pos_len);
		if (ret) {
			warn("failed to send segment %d: %d\n", seg_num, ret);
			return;
		}

		//pos += chunk_len;
		//pos_len -= chunk_len;
		//seg_num++;
	}

	if (!responseNeeded)
		return;
        warn("--------moving to wait events---------------\n");
	wait_events();
	warn ("reponse needed value before assigning %d\n",*responseNeeded);//rm
	*responseNeeded = ctxt.rx_buf->data_len;
        warn ("response needed value after assigning %d\n",*responseNeeded);
	memcpy(respdata,&ctxt.rx_buf->data_len,sizeof(int));

	printf("\nsize copied to response buf\n");
	DispHexString(respdata,4);
	memcpy(respdata+sizeof(int),ctxt.rx_buf->data,ctxt.rx_buf->data_len+4);
	*responseNeeded+=4;
        printf("Response data also copied to response buf without error\n\n");
	DispHexString(respdata,*responseNeeded);	
	warn("response data length---%d\n",ctxt.rx_buf->data_len);
	// check if caldata needs to be logged and write into Flash
	if(*responseNeeded > 107){
		if(devid == 0x40) {
			if((respdata[52]==0xE9) && (respdata[92]==7) &&(respdata[107]<0x30) ){
				BoardDataCapture(respdata,devid);
			}
			}else if (devid == 0x50) {
				if((respdata[52]==0xC8) && (respdata[92]==7)) {
					BoardDataCapture(respdata,devid);
				}
			}
		}

}

/*int cmd_set_timer_nl()
{
	struct itimerspec exp_time;
	int err;

	dbg("cmd set timer\n");

	bzero(&exp_time, sizeof(exp_time));
	exp_time.it_value.tv_sec = CMD_TIMEOUT;
	err = timer_settime(ctxt.timer, 0, &exp_time, NULL);
	ctxt.timeout = 0;

	if (err < 0)
		return errno;

	return 0;
}

int cmd_stop_timer_nl()
{
	struct itimerspec exp_time;
	int err;

	dbg("cmd stop timer\n");

	bzero(&exp_time, sizeof(exp_time));
	err = timer_settime(ctxt.timer, 0, &exp_time, NULL);

	if (err < 0)
		return errno;

	return 0;
}*/

int cmd_init(char *ifname, void (*rx_cb) (void *buf))
{
	struct testmode_version version;
	int ret, mc_id;

	dbg("cmd init ifname %s rx_cb %p\n", ifname, rx_cb);

	if (ctxt.initialized)
		return -1;

	ctxt.rx_buf = malloc(sizeof(struct tcmd_rx) + RX_BUF_LEN_MAX);
	if (!ctxt.rx_buf) {
		warn("failed to allocate rx_buf");
		return -2;
	}

	ctxt.nl_sock = nl_socket_alloc();
	if (!ctxt.nl_sock) {
		warn("failed to allocate netlink socket\n");
		return -2;
	}

//	nl_socket_set_buffer_size(ctxt.nl_sock, 8192, 8192);
	nl_socket_set_buffer_size(ctxt.nl_sock, 212992, 212992);
	warn("\n\n...................cmd_init----added patch\n\n");

	ret = genl_connect(ctxt.nl_sock);
	if (ret) {
		warn("failed to connect generic netlink: %d\n", ret);
		return -2;
	}

	ret = genl_ctrl_alloc_cache(ctxt.nl_sock, &ctxt.nl_cache);
	if (ret) {
		warn("failed to connect generic netlink: %d\n", ret);
		return -2;
	}

	ctxt.nl80211 = genl_ctrl_search_by_name(ctxt.nl_cache, "nl80211");
	if (!ctxt.nl80211) {
		warn("was not able to find nl80211\n");
		return -2;
	}

	/* hopefully this helps with big packets */
	//nl_socket_enable_msg_peek(ctxt.nl_sock);
	nl_socket_disable_msg_peek(ctxt.nl_sock);

	ctxt.if_index = if_nametoindex(ifname);
	if (!ctxt.if_index) {
		warn("interface '%s' was not found\n", ifname);
		return -2;
	}

	mc_id = get_multicast_id("nl80211", "testmode");
	if (mc_id < 0) {
		warn("failed to get testmode multicast id: %d\n", mc_id);
		return -2;
	}

	dbg("testmode multicast id %d\n", mc_id);

	ret = nl_socket_add_membership(ctxt.nl_sock, mc_id);
	if (ret) {
		warn("failed to add testmode multicast membership: %d\n", ret);
		return -2;
	}

	ret = get_version(&version);
	if (ret) {
		warn("failed to get ath10k testmode version");
		return -2;
	}

	if (version.major != ATH10K_TESTMODE_VERSION_MAJOR ||
	    version.minor < ATH10K_TESTMODE_VERSION_MINOR) {
		warn("ath10k testmode version %d.%d not supported, expected %d.%d\n",
		     version.major, version.minor,
		     ATH10K_TESTMODE_VERSION_MAJOR,
		     ATH10K_TESTMODE_VERSION_MINOR);
		return -2;
	}

	ret = utf_start();
	warn("\n----------return message from utf %d--------------\n",ret);
	if (ret) {
		warn("failed to start utf mode: %d\n", ret);
		return -2;
	}

#ifdef ATH10K_TEST_CMD_STOP
	return test_utf_stop();
#endif

	ctxt.rx_cb = rx_cb;

        if (ctxt.rx_cb == NULL) {
                warn("rx callback is NULL\n");
        }



	ctxt.sev.sigev_notify = SIGEV_THREAD;
	ctxt.sev.sigev_notify_function = timer_expire_nl;
	timer_create(CLOCK_REALTIME, &ctxt.sev, &ctxt.timer);

	nl_socket_set_nonblocking(ctxt.nl_sock);
	ctxt.initialized = true;

	return ret;
}

/* nobody apparently calls this and looks useless */
int cmd_end_nl()
{
	dbg("cmd end\n");

	free(ctxt.rx_buf);

	ctxt.initialized = false;

	return 0;
}

