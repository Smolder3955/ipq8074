/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <net/if.h>

#define FAMILY        "PAPINL"
#define MCAST_GRP     "PAPINL_MCGRP"

const char *prog_name;
int verbose;

/* attributes */
enum {
	PAPI_ATTR_UNSPEC,
	PAPI_ATTR_EVENT,
	PAPI_ATTR_TEST,
	__PAPI_ATTR_MAX
};
#define PAPI_ATTR_MAX (__PAPI_ATTR_MAX - 1)

/* commands */
enum {
	PAPI_CMD_UNSPEC,
	PAPI_CMD_EVENT,
	PAPI_CMD_TEST,
	__PAPI_CMD_MAX
};
#define PAPI_CMD_MAX (__PAPI_CMD_MAX - 1)

/*
 * report types
 */
typedef enum {
	REPORT_TYPE_UNSPEC,
	ASSOC_STA_REPORT,
	NON_ASSOC_STA_REPORT,
	__REPORT_TYPE_MAX
} REPORT_TYPE;
#define REPORT_TYPE_MAX (__REPORT_TYPE_MAX - 1)

/*
 * Associated station events
 */
typedef enum {
	STA_ASSOCIATION = 0,
	STA_DISASSOCIATION = 1,
	STA_NEIGHBOR_REPORT_REQUEST = 2,
} STA_EVENT;

/*
 * band types
 */
typedef enum {
	BAND_24_GHZ = 0,
	BAND_5_GHZ = 1,
} BANDS;


/*
 * associated stations report
 */
struct assoc_sta_report {
	uint8_t mac[6];
	uint32_t time_stamp;
	uint32_t sta_event;
};

/*
 * non-associated stations report
 */
struct non_assoc_sta_report {
	uint8_t mac[6];
	uint32_t time_stamp;
	uint8_t ssid[32+1];
	uint8_t ssid_len;
	uint8_t band;
	uint8_t chan_num;
	int8_t rssi;
};

struct papi_report {
	/* Type of the report: One of REPORT_TYPE.*/
	uint32_t type;
	/* The OS-specific index of the VAP on which the event occurred.*/
	uint32_t sys_index;
	/* Data for the event. Which memeber is valid is based on
		type field.*/
	union {
		struct assoc_sta_report assoc_report;
		struct non_assoc_sta_report non_assoc_report;
	} data;
};


struct app_context {
	struct nl_sock *sk;
	struct nl_cb *nl_cb;
	int family_id;
	int mcast_id;
	uint8_t recv_flags[REPORT_TYPE_MAX];
} ctx;

struct mcast_grp_info {
	const char *group_name;
	int id;
};

void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s options\n", prog_name);
	fprintf(stream,
		"-h --help     displays this usage information\n"
		"-a --assoc    receive associated STA events\n"
		"-n --nonassoc receive non-associated STA events\n"
		"-v --verbose  enable verbose logging\n");
	exit(exit_code);
}

static int recv_events(struct nl_msg *msg, void *arg)
{
	char ifname[IF_NAMESIZE + 1] = {0};
	struct nlattr *attrs[PAPI_ATTR_MAX+1];
	struct papi_report *report;
	struct assoc_sta_report *a_report;
	struct non_assoc_sta_report *na_report;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(attrs, PAPI_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		genlmsg_attrlen(gnlh, 0), NULL);

	if (!attrs[PAPI_ATTR_EVENT]) {
		fprintf(stderr, "Error! No event attribute\n");
		return NL_SKIP;
	}

	report = (struct papi_report *)nla_data(attrs[PAPI_ATTR_EVENT]);
	if (!report) {
		fprintf(stderr, "Error! empty payload\n");
		return NL_SKIP;
	}

	if (ctx.recv_flags[report->type -1]) {
		fprintf(stdout, "\nReport Type = %u\n",report->type);
		if_indextoname(report->sys_index, ifname);
		ifname[IF_NAMESIZE] = '\0';
		fprintf(stdout, "Interface = %s\n",ifname);

		switch (report->type) {
			case ASSOC_STA_REPORT:
				a_report = (struct assoc_sta_report*)&report->data.assoc_report;
				fprintf(stdout, "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", a_report->mac[0], a_report->mac[1],
					a_report->mac[2],a_report->mac[3],a_report->mac[4],a_report->mac[5]);
				fprintf(stdout, "Timestamp: %u\n",a_report->time_stamp);
				fprintf(stdout, "Station Event: %u\n",a_report->sta_event);
				break;

			case NON_ASSOC_STA_REPORT:
				na_report = (struct non_assoc_sta_report*)&report->data.non_assoc_report;
				fprintf(stdout, "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", na_report->mac[0], na_report->mac[1],
					na_report->mac[2], na_report->mac[3], na_report->mac[4], na_report->mac[5]);
				fprintf(stdout, "Timestamp: %u\n",na_report->time_stamp);
				fprintf(stdout, "SSID: %s\n",na_report->ssid_len?(char *)na_report->ssid:"<hidden ssid>");
				fprintf(stdout, "SSID len: %d\n",na_report->ssid_len);
				fprintf(stdout, "Band: %s\n",na_report->band?"5GHz":"2.4GHz");
				fprintf(stdout, "Channel: %u\n",na_report->chan_num);
				fprintf(stdout, "Rssi: %d\n",na_report->rssi);
				break;

			default:
				fprintf(stdout, "Invalid report\n");
		}
		printf("=====================================");
	}

	return NL_SKIP;
}

static int family_cb(struct nl_msg *msg, void *arg)
{
	int i;
	struct mcast_grp_info *result = (struct mcast_grp_info *)arg;
	struct nlattr *mcast_grp;
	struct nlattr *attrs[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *genl_hdr = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(attrs, CTRL_ATTR_MAX, genlmsg_attrdata(genl_hdr, 0),
		genlmsg_attrlen(genl_hdr, 0), NULL);
	if (!attrs[CTRL_ATTR_MCAST_GROUPS]) {
		fprintf(stderr, "No mcast groups\n");
		return NL_SKIP;
	}

	nla_for_each_nested(mcast_grp, attrs[CTRL_ATTR_MCAST_GROUPS], i) {
		struct nlattr *attrs2[CTRL_ATTR_MCAST_GRP_MAX + 1];
		nla_parse(attrs2, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcast_grp),
			nla_len(mcast_grp), NULL);
		if (!attrs2[CTRL_ATTR_MCAST_GRP_NAME] ||
			!attrs2[CTRL_ATTR_MCAST_GRP_ID] ||
			strncmp(nla_data(attrs2[CTRL_ATTR_MCAST_GRP_NAME]),
				result->group_name,
				nla_len(attrs2[CTRL_ATTR_MCAST_GRP_NAME])) != 0) {
			continue;
                }

		result->id = nla_get_u32(attrs2[CTRL_ATTR_MCAST_GRP_ID]);
		if (verbose) {
			fprintf(stdout, "Multicast group = %s, id = %d\n",
				(char *)result->group_name,result->id);
		}
		break;
	};

	return NL_SKIP;
}

static int send_and_recv_msg(struct nl_msg *msg,
	int (*valid_cb)(struct nl_msg *, void *),
	void *data)
{
	struct nl_cb *cb;
	int ret = -1;

	if (!msg)
		return -ENOMEM;

	cb = nl_cb_clone(ctx.nl_cb);
	if (!cb)
		goto out;

	ret = nl_send_auto(ctx.sk, msg);
	if (ret < 0)
		goto out;

	if (valid_cb) {
		ret = nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
				 valid_cb, data);
		if (ret < 0)
			goto out;
        }

	ret = nl_recvmsgs(ctx.sk, cb);
	if (ret < 0)
		fprintf(stderr, "Error! Failed to receive nl msgs\n");
out:
	nl_cb_put(cb);
	nlmsg_free(msg);
	return ret;
}

static int nl_get_mcast_id(const char *family_name, const char *group_name)
{
	int res;
	struct nl_msg *msg;
	struct mcast_grp_info result = { group_name, -ENOENT };

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "%s: nl msg alloc failed\n",__func__);
	 	return -ENOMEM;
	}

	if (!genlmsg_put(msg, 0, 0, genl_ctrl_resolve(ctx.sk, "nlctrl"),
		0, 0, CTRL_CMD_GETFAMILY, 0) ||
		nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family_name)) {
			nlmsg_free(msg);
			fprintf(stderr, "%s: Genl msg put failed\n",__func__);
			return -1;
	}

	res = send_and_recv_msg(msg, family_cb, &result);
	if (0 == res)
		res = result.id;

	return res;
}

int main(int argc, char *argv[]) {
        int ret;
	int next_option;
	const char *const short_options = "hanv";
	const struct option long_options[] = {
		{ "help", 0, NULL, 'h' },
		{ "assoc", 0, NULL, 'a' },
		{ "nonassoc", 0, NULL, 'n' },
		{ "verbose", 0, NULL, 'v' },
		{   NULL, 0, NULL,  0  }
	};


	prog_name = argv[0];
	if(1 == argc)
		print_usage(stdout, 0);

	do {
		next_option = getopt_long(argc, argv, short_options,
				long_options, NULL);
		switch(next_option) {
			case 'h': /* -h or --help */
				print_usage(stdout, 0);
			case 'a': /* -a or --assoc */
				ctx.recv_flags[ASSOC_STA_REPORT - 1] = 1;
				break;
			case 'n': /* -n or --nonassoc */
				ctx.recv_flags[NON_ASSOC_STA_REPORT - 1] = 1;
				break;
			case 'v': /* -v or --verbose */
				verbose = 1;
				break;
			case '?': /* invalid option by user */
				print_usage(stderr, 1);
			case -1: /* done with options */
				break;
			default: /* unexpected */
				abort();
		}
	} while(next_option != -1);

        ctx.nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
        if (NULL == ctx.nl_cb) {
		fprintf(stderr, "Error! Failed to allocate nl callbacks\n");
		goto error;
	}

	ctx.sk = nl_socket_alloc_cb(ctx.nl_cb);
	if (NULL == ctx.sk) {
		fprintf(stderr, "Error! Failed to allocate socket\n");
		goto error;
	}

	nl_socket_set_local_port(ctx.sk, 0);
	nl_socket_disable_seq_check(ctx.sk);
	nl_socket_disable_auto_ack(ctx.sk);
	if (genl_connect(ctx.sk) != 0) {
		fprintf(stderr, "Error! Failed to connect to Generic Netlink socket\n");
		goto error;
	}

	ctx.family_id = genl_ctrl_resolve(ctx.sk, FAMILY);
	if(ctx.family_id < 0) {
		fprintf(stderr, "Error! Cannot resolve family\n");
		goto error;
	} else {
		if (verbose)
			fprintf(stderr, "Family id %d\n", ctx.family_id);
	}

	fprintf(stdout, "Receiving notifications...\n");
	ctx.mcast_id = nl_get_mcast_id(FAMILY, MCAST_GRP);
	if (ctx.mcast_id >= 0) {
		ret = nl_socket_add_membership(ctx.sk, ctx.mcast_id);
		if (ret < 0) {
			fprintf(stderr, "Error! failed add mcast membership\n");
			goto error;
		}
	}
	if (ctx.mcast_id < 0) {
		fprintf(stderr, "Error! Mcast id not found\n");
		goto error;
	}
	nl_socket_modify_cb(ctx.sk, NL_CB_VALID, NL_CB_CUSTOM, recv_events, NULL);
	while (1) {
		nl_recvmsgs_default(ctx.sk);
	}
error:
	if (ctx.sk) {
		nl_socket_free(ctx.sk);
		ctx.sk = NULL;
	}
	if (ctx.nl_cb) {
		nl_cb_put(ctx.nl_cb);
		ctx.nl_cb = NULL;
	}

	return 0;

}
