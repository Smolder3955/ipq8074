/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <qcatools_lib.h>
#if BUILD_X86
#include <netlink/genl/genl.h>
#include <netlink/attr.h>
#endif
#if UMAC_SUPPORT_CFG80211
#include <netlink/genl/genl.h>
#include <netlink/attr.h>
#include <netlink/cache.h>
#endif

#include <ieee80211_external.h>

extern unsigned int if_nametoindex(const char *ifname);
int s = -1;

struct ieee80211_key{
	struct ieee80211req_key req_key;
	uint32_t suite;
	int seq_len;
	uint8_t seq[8];
};

/* cipher suite selectors */
#define WLAN_CIPHER_SUITE_WEP40         0x000FAC01
#define WLAN_CIPHER_SUITE_TKIP          0x000FAC02
#define WLAN_CIPHER_SUITE_RESERVED      0x000FAC03
#define WLAN_CIPHER_SUITE_CCMP          0x000FAC04
#define WLAN_CIPHER_SUITE_WEP104        0x000FAC05
#define WLAN_CIPHER_SUITE_AES_CMAC      0x000FAC06
#define WLAN_CIPHER_SUITE_GCMP          0x000FAC08
#define WLAN_CIPHER_SUITE_GCMP_256      0x000FAC09
#define WLAN_CIPHER_SUITE_CCMP_256      0x000FAC0A
#define WLAN_CIPHER_SUITE_BIP_GMAC_128  0x000FAC0B
#define WLAN_CIPHER_SUITE_BIP_GMAC_256  0x000FAC0C
#define WLAN_CIPHER_SUITE_BIP_CMAC_256  0x000FAC0D

static void checksocket()
{
	if (s < 0 ? (s = socket(PF_INET, SOCK_DGRAM, 0)) == -1 : 0)
		perror("socket(SOCK_DRAGM)");
}

struct athkeyhandler{
#if UMAC_SUPPORT_CFG80211
	wifi_cfg80211_context cfg80211_sock_ctx; /* cfg80211 socket context */
	config_mode_type cfg_flag;               /* cfg flag */
#endif /* UMAC_SUPPORT_CFG80211 */
};

#if UMAC_SUPPORT_CFG80211
#define IS_CFG80211_ENABLED(p)      (((p)->cfg_flag == CONFIG_CFG80211)?1:0)
#define GET_ADDR_OF_CFGSOCKINFO(p)  (&(p)->cfg80211_sock_ctx)

static int init_cfg80211_socket(struct athkeyhandler *athkey);
static void destroy_cfg80211_socket(struct athkeyhandler *athkey);

/*
 * Function     : init_cfg80211_socket
 * Description  : initialize cfg80211 socket
 * Input params : pointer to athkeyhandler
 * Return       : 0:success, -1:failure
 *
 */
static int init_cfg80211_socket(struct athkeyhandler *athkey)
{
	wifi_cfg80211_context *pcfg80211_sock_ctx =
		GET_ADDR_OF_CFGSOCKINFO(athkey);

	/* Reset the private sockets to 0 if the application does not pass
	 * custom private event and command sockets. In this case, the default
	 * port is used for netlink communication.
	 */
	pcfg80211_sock_ctx->pvt_cmd_sock_id = 0;
	pcfg80211_sock_ctx->pvt_event_sock_id = 0;

	if (wifi_init_nl80211(pcfg80211_sock_ctx) != 0) {
		return -1;
	}
	return 0;
}

/*
 * Function     : destroy_cfg80211_socket
 * Description  : destroy cfg80211 socket
 * Input params : pointer to athkeyhandler
 * Return       : void
 *
 */
static void destroy_cfg80211_socket(struct athkeyhandler *athkey)
{
	nl_socket_free(athkey->cfg80211_sock_ctx.cmd_sock);
}
#endif /* UMAC_SUPPORT_CFG80211 */

const char *progname;

static int
set80211priv(const char *dev, int op, void *data, int len, int show_err)
{
	struct iwreq iwr;
	checksocket();


	memset(&iwr, 0, sizeof(iwr));
	strlcpy(iwr.ifr_name, dev, IFNAMSIZ);
	if (len < IFNAMSIZ) {
		/*
		 * Argument data fits inline; put it there.
		 */
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(s, op, &iwr) < 0) {
		if (show_err) {
			static const char *opnames[] = {
				"ioctl[IEEE80211_IOCTL_SETPARAM]",
				"ioctl[IEEE80211_IOCTL_GETPARAM]",
				"ioctl[IEEE80211_IOCTL_SETKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_DELKEY]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETMLME]",
				NULL,
				"ioctl[IEEE80211_IOCTL_SETOPTIE]",
				"ioctl[IEEE80211_IOCTL_GETOPTIE]",
				"ioctl[IEEE80211_IOCTL_ADDMAC]",
				"ioctl[IEEE80211_IOCTL_DELMAC]",
				"ioctl[IEEE80211_IOCTL_GETCHANLIST]",
				"ioctl[IEEE80211_IOCTL_SETCHANLIST]",
				NULL,
			};
			if (IEEE80211_IOCTL_SETPARAM <= op &&
			    op <= IEEE80211_IOCTL_SETCHANLIST)
				perror(opnames[op - SIOCIWFIRSTPRIV]);
			else if (op == IEEE80211_IOCTL_GETKEY)
				perror("IEEE80211_IOCTL_GETKEY[fail]");
			else
				perror("ioctl[unknown???]");
		}
		return -errno;
	}
	return 0;
}

#if UMAC_SUPPORT_CFG80211
/**
 * ack_handler: ack handler
 * @msg: pointer to struct nl_msg
 * @arg: argument
 *
 * return NL state.
 */

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = (int *)arg;
	*err = 0;
	return NL_STOP;
}

/**
 * finish_handler: finish handler
 * @msg: pointer to struct nl_msg
 * @arg: argument
 *
 * return NL state.
 */

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = (int *)arg;
	*ret = 0;
	return NL_SKIP;
}


/**
 * error_handler: error handler
 * @msg: pointer to struct sockaddr
 * @err: pointer to nlmsgerr
 * @arg: argument
 *
 * return NL state.
 */
static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			void *arg)
{
	int *ret = (int *)arg;
	*ret = err->error;
	printf("Error received: %d \n", err->error);
	if (err->error > 0) {
		*ret = -(err->error);
	}
	return NL_SKIP;
}



static inline int min_int(int a, int b)
{
	if (a < b)
		return a;
	return b;
}

static int resp_handler(struct nl_msg *msg, void *data)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX,
		(struct nlattr *)genlmsg_attrdata(gnlh, 0),
		genlmsg_attrlen(gnlh, 0), NULL);

	/*
	* TODO: validate the key index and mac address!
	* Otherwise, there's a race condition as soon as
	* the kernel starts sending key notifications.
	*/

	if (tb[NL80211_ATTR_KEY_CIPHER])
		memcpy(data, nla_data(tb[NL80211_ATTR_KEY_CIPHER]),
			min_int(nla_len(tb[NL80211_ATTR_KEY_CIPHER]), 6));
	data = data + 4;
	*(int *)data = nla_len(tb[NL80211_ATTR_KEY_DATA]);
	data = data + 4;
	if (tb[NL80211_ATTR_KEY_DATA])
		memcpy(data, nla_data(tb[NL80211_ATTR_KEY_DATA]),
			min_int(nla_len(tb[NL80211_ATTR_KEY_DATA]), 100));
	return NL_SKIP;
}

/**
 * send_nlmsg: sends nl msg
 * @ctx: pointer to wifi_cfg80211_context
 * @nlmsg: pointer to nlmsg
 * @data: data
 *
 * return NL state.
 */
int send_key_nlmsg(wifi_cfg80211_context *ctx, struct nl_msg *nlmsg,
		void *data)
{
	int err = 0, res = 0;
	struct nl_cb *cb = nl_cb_alloc(NL_CB_DEFAULT);

	if (!cb) {
		err = -1;
		goto out;
	}

	/* send message */
	err = nl_send_auto_complete(ctx->cmd_sock, nlmsg);

	if (err < 0) {
		goto out;
	}
	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, resp_handler, data);

	/*   wait for reply */
	while (err > 0) {  /* error will be set by callbacks */
		res = nl_recvmsgs(ctx->cmd_sock, cb);
		if (res) {
			fprintf(stderr, "nl80211: %s->nl_recvmsgs failed:%d\n",
				 __func__, res);
		}
	}

out:
	if (cb) {
		nl_cb_put(cb);
		free(nlmsg);
	}
	return err;
}

static struct nl_msg *cfg80211_key_prepare(wifi_cfg80211_context *ctx,
						int cmdid,
						const char *ifname)
{
	int res;
	struct nl_msg *nlmsg = nlmsg_alloc();
	if (nlmsg == NULL) {
		fprintf(stderr, "Out of memory\n");
		return NULL;
	}

	genlmsg_put(nlmsg, 0, 0, ctx->nl80211_family_id,
		0, 0, cmdid, 0);

	res = nla_put_u32(nlmsg, NL80211_ATTR_IFINDEX,
				if_nametoindex(ifname));
	if (res < 0) {
		fprintf(stderr, "Failed to put vendor sub command\n");
		nlmsg_free(nlmsg);
		return NULL;
	}
	return nlmsg;
}

static int cfg80211_key_send(wifi_cfg80211_context *ctx, int cmdid,
				const char *ifname, char *buffer,
				void* req_key)
{
	struct nl_msg *nlmsg = NULL;
	int res;
	nlmsg = cfg80211_key_prepare(ctx, cmdid, ifname);
	struct ieee80211_key *key = req_key;

	if (!nlmsg)
		return -EINVAL;

	if(!nlmsg)
		return -EINVAL;

	switch(cmdid) {
	case NL80211_CMD_GET_KEY:
		if(!nlmsg || (key->req_key.ik_macaddr &&
			nla_put(nlmsg, NL80211_ATTR_MAC, 6,
			key->req_key.ik_macaddr)) || nla_put_u8(nlmsg,
			NL80211_ATTR_KEY_IDX, key->req_key.ik_keyix)) {
			nlmsg_free(nlmsg);
			return -ENOBUFS;
		}
		break;
	case NL80211_CMD_NEW_KEY:
		if (!nlmsg || ((key->req_key.ik_keylen >= 1) &&
			nla_put(nlmsg, NL80211_ATTR_KEY_DATA,
			key->req_key.ik_keylen,
			(const u_int8_t *)key->req_key.ik_keydata)) ||
			nla_put_u32(nlmsg, NL80211_ATTR_KEY_CIPHER,
			key->suite) || (key->seq && (key->seq_len >= 1)
			&& nla_put(nlmsg, NL80211_ATTR_KEY_SEQ,
			key->seq_len, (const u_int8_t *)key->seq))
			|| (key->req_key.ik_macaddr
			&& !IEEE80211_IS_BROADCAST(key->req_key.ik_macaddr)
			&& nla_put(nlmsg, NL80211_ATTR_MAC, 6,
			key->req_key.ik_macaddr)) || nla_put_u8(nlmsg,
			NL80211_ATTR_KEY_IDX, key->req_key.ik_keyix)) {
			nlmsg_free(nlmsg);
			return -ENOBUFS;
		}
		break;
	default:
		printf("Invalid nl cmd id\n");
		break;
	}

	res = send_key_nlmsg(ctx, nlmsg, buffer);
	if (res < 0) {
		return res;
	}
	return res;
}
#endif

/**
* athkey_send_command; function to send the cfg command or ioctl command.
* @athkey     : pointer to athkeyhandler
* @ifname    : interface name
* @buf       : buffer
* return     : 0 for sucess, -1 for failure
*/
static int athkey_send_command(struct athkeyhandler *athkey,
				const char *ifname, int op,
				void *buf)
{
#if UMAC_SUPPORT_CFG80211
	wifi_cfg80211_context *pcfg80211_sock_ctx;
	pcfg80211_sock_ctx = GET_ADDR_OF_CFGSOCKINFO(athkey);

	if (IS_CFG80211_ENABLED(athkey)) {
		struct cfg80211_data buffer;
		int nl_cmd = NL80211_CMD_GET_KEY;
		int msg;
		struct ieee80211_key *key;
		buffer.data = buf;
		buffer.length = sizeof(struct ieee80211_key);
		buffer.callback = NULL;
		buffer.parse_data = 0;
		key = buf;
                if (op == IEEE80211_IOCTL_SETKEY)
			nl_cmd = NL80211_CMD_NEW_KEY;
		msg = cfg80211_key_send(pcfg80211_sock_ctx,
			nl_cmd, ifname, (char *)&buffer, buf);
		if (msg < 0) {
			fprintf(stderr, "Couldn't send NL command\n");
			return -1;
		}
		if (op == IEEE80211_IOCTL_GETKEY) {
		memcpy(&key->req_key.ik_type, &buffer.data, 1);
		memcpy(&key->req_key.ik_keylen, &buffer.data+1, 1);
		memcpy(&key->req_key.ik_keydata, &buffer.data+2,
			key->req_key.ik_keylen);
		}
	} else
#endif /* UMAC_SUPPORT_CFG80211 */
	{
		return set80211priv(ifname, op, buf,
					sizeof(struct ieee80211req_key), 1);
	}
	return 0;
}

static int digittoint(int c)
{
	return isdigit(c) ? c - '0' : isupper(c) ? c - 'A' + 10 : c - 'a' + 10;
}

static int getdata(const char *arg, u_int8_t *data, size_t maxlen,
			int is_key)
{
	const char *cp = arg;
	int len;

	if (cp[0] == '0' && (cp[1] == 'x' || cp[1] == 'X'))
		cp += 2;
	len = 0;

	if (is_key) {
		int keyb0 = 0;
		if (!isxdigit(cp[0])) {
			fprintf(stderr, "%s: invalid key value %c (not hex)\n",
				progname, cp[0]);
			exit(-1);
		}
		keyb0 = digittoint(cp[0]);
		if (keyb0 == 0) {
			printf("Invalid key value, first bit must be non-zero\n");
			return -1;
		}
	}
	while (*cp) {
		int b0, b1;
		if (cp[0] == ':' || cp[0] == '-' || cp[0] == '.') {
			cp++;
			continue;
		}
		if (!isxdigit(cp[0])) {
			fprintf(stderr, "%s: invalid data value %c (not hex)\n",
				progname, cp[0]);
			exit(-1);
		}
		b0 = digittoint(cp[0]);
		if (cp[1] != '\0') {
			if (!isxdigit(cp[1])) {
				fprintf(stderr, "%s: invalid data value %c "
					"(not hex)\n", progname, cp[1]);
				exit(-1);
			}
			b1 = digittoint(cp[1]);
			cp += 2;
		} else {			/* fake up 0<n> */
			b1 = b0, b0 = 0;
			cp += 1;
		}
		if (len > maxlen) {
			fprintf(stderr,
				"%s: too much data in %s, max %zu bytes\n",
				progname, arg, maxlen);
		}
		data[len++] = (b0<<4) | b1;
	}
	return len;
}

static void
cipher_type(int type)
{
	switch(type) {
	case IEEE80211_CIPHER_WEP:
		printf("WEP");
		break;
	case IEEE80211_CIPHER_TKIP:
		printf("TKIP");
		break;
	case IEEE80211_CIPHER_AES_OCB:
		printf("AES-OCB");
		break;
	case IEEE80211_CIPHER_CKIP:
		printf("CKIP");
		break;
	case IEEE80211_CIPHER_AES_CCM:
		printf("CCMP");
		break;
	case IEEE80211_CIPHER_AES_CCM_256:
		printf("CCMP-256");
		break;
	case IEEE80211_CIPHER_AES_GCM:
		printf("GCMP");
		break;
	case IEEE80211_CIPHER_AES_GCM_256:
		printf("GCMP-256");
		break;
	case IEEE80211_CIPHER_AES_CMAC:
		printf("AES-CMAC");
		break;
	case IEEE80211_CIPHER_AES_GMAC_256:
		printf("GMAC-256");
		break;
	case IEEE80211_CIPHER_AES_CMAC_256:
		printf("CMAC-256");
		break;
	case IEEE80211_CIPHER_WAPI:
		printf("WAPI");
		break;
	case IEEE80211_CIPHER_NONE:
		printf("none");
		break;
	default :
		printf("undefined");
		break;
	}
}

static int
getcipher(const char *name)
{
//#define	streq(a,b)	(strcasecmp(a,b) == 0)

	if (streq(name, "wep"))
		return IEEE80211_CIPHER_WEP;
	if (streq(name, "tkip"))
		return IEEE80211_CIPHER_TKIP;
	if (streq(name, "aes-ocb") || streq(name, "ocb"))
		return IEEE80211_CIPHER_AES_OCB;
	if (streq(name, "aes-ccm") || streq(name, "ccm") ||
	    streq(name, "aes"))
		return IEEE80211_CIPHER_AES_CCM;
	if (streq(name, "ccm256"))
		return IEEE80211_CIPHER_AES_CCM_256;
	if (streq(name, "ckip"))
		return IEEE80211_CIPHER_CKIP;
	if (streq(name, "gcm"))
		return IEEE80211_CIPHER_AES_GCM;
	if (streq(name, "gcm256"))
		return IEEE80211_CIPHER_AES_GCM_256;
	if (streq(name, "cmac"))
		return IEEE80211_CIPHER_AES_CMAC;
	if (streq(name, "cmac256"))
		return IEEE80211_CIPHER_AES_CMAC_256;
	if (streq(name, "gmac256"))
		return IEEE80211_CIPHER_AES_GMAC_256;
	if (streq(name, "wapi"))
		return IEEE80211_CIPHER_WAPI;
	if (streq(name, "none") || streq(name, "clr"))
		return IEEE80211_CIPHER_NONE;

	fprintf(stderr, "%s: unknown cipher %s\n", progname, name);
	exit(-1);
#undef streq
}

static uint32_t cipher_to_suite(uint8_t cipher, uint8_t keylen) {
	switch(cipher) {
	case IEEE80211_CIPHER_WEP:
		if (keylen == 5)
			return WLAN_CIPHER_SUITE_WEP40;
		return WLAN_CIPHER_SUITE_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		return WLAN_CIPHER_SUITE_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		return WLAN_CIPHER_SUITE_CCMP;
		break;
	case IEEE80211_CIPHER_AES_CCM_256:
		return WLAN_CIPHER_SUITE_CCMP_256;
		break;
	case IEEE80211_CIPHER_AES_GCM:
		return WLAN_CIPHER_SUITE_GCMP;
		break;
	case IEEE80211_CIPHER_AES_GCM_256:
		return WLAN_CIPHER_SUITE_GCMP_256;
		break;
	case IEEE80211_CIPHER_AES_CMAC:
		return WLAN_CIPHER_SUITE_AES_CMAC;
		break;
	case IEEE80211_CIPHER_AES_CMAC_256:
		return WLAN_CIPHER_SUITE_BIP_CMAC_256;
		break;
	case IEEE80211_CIPHER_AES_GMAC_256:
		return WLAN_CIPHER_SUITE_BIP_GMAC_256;
		break;
	case IEEE80211_CIPHER_NONE:
		printf("no valid suite");
		break;
	default :
		printf("undefined suite");
		break;
	}
	return 0;
}

static void
macaddr_printf(u_int8_t *mac)
{
	printf("%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void
usage(void)
{
	fprintf(stderr, "usage to set: %s -i athX -s keyix cipher keyval [mac]\n",
		progname);
	fprintf(stderr, "usage to get: %s -i athX -x keyix [mac]\n",
                progname);
#if UMAC_SUPPORT_CFG80211
	fprintf(stderr, "usage to get for cfg: %s -cfg80211 -i athX -x keyix [mac]\n",
                progname);
	fprintf(stderr, "usage to set for cfg: %s -cfg80211 -i athX -s -e -u -r <rxsc> <keyidx> <cipher> <keyval> <mac>\n",
                progname);
#endif
	exit(-1);
}

int
main(int argc, char *argv[])
{
	const char *ifname = NULL;
	struct ieee80211req_del_key delkey;
	struct ieee80211_key setkey;
	struct ieee80211req_key getkey;
	int c, keyix;
	int help = 0;
	int get_key = 0;
	int cipher = 0;
	int op = IEEE80211_IOCTL_SETKEY;
	int err = 0;
	int swencrypt = 0;
	int swdecrypt = 0;
	int seq_len = 0;
	uint64_t keytsc = 0;
	uint64_t keyrsc = 0;
	struct athkeyhandler athkey;
	struct ieee80211_key *athkey_key = NULL;

	progname = argv[0];

	memset(&athkey, 0, sizeof(athkey));
#if UMAC_SUPPORT_CFG80211
	/* figure out whether cfg80211 is enabled */
	if (argc > 1 && (strcmp(argv[1], "-cfg80211") == 0)) {
		athkey.cfg_flag = CONFIG_CFG80211;
		argc -= 1;
		argv += 1;
	}
	if (IS_CFG80211_ENABLED(&athkey)) {
	/* init cfg80211 socket */
		if (init_cfg80211_socket(&athkey) < 0) {
			fprintf(stderr, "unable to create cfg80211 socket");
			exit(EXIT_FAILURE);
		}
	}
#endif /* UMAC_SUPPORT_CFG80211 */

	while ((c = getopt(argc, argv, "dgsxheut:r:i:")) != -1)
		switch (c) {
		case 'd':
			op = IEEE80211_IOCTL_DELKEY;
			break;
		case 'g':
			get_key = 1;
			op = IEEE80211_IOCTL_GETKEY;
			break;
		case 's':
			op = IEEE80211_IOCTL_SETKEY;
			break;
		case 'i':
			ifname = optarg;
			break;
		case 'h':
			help = 1;
			break;
		case 'x':
			op = IEEE80211_IOCTL_GETKEY;
			cipher = 1;
			swencrypt = 1;
			break;
		case 'u':
			swdecrypt = 1;
			break;
		case 't':
			keytsc = strtoull(optarg, (char **)NULL, 0);
			break;
		case 'r':
			keyrsc = strtoull(optarg, (char **)NULL, 0);
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	argc -= optind;
	argv += optind;
	if (argc < 1 || ifname == NULL)
		usage();

	if(help == 1)
		usage();
	/* 0 keyix for default keyix */
	keyix = atoi(argv[0]);
	if (!(0 <= keyix && keyix <= 4) )
		errx(-1, "%s: invalid key index %s, must be [0..4]",
			progname, argv[0]);
	switch (op) {
	case IEEE80211_IOCTL_DELKEY:
		memset(&delkey, 0, sizeof(delkey));
		athkey_key = (struct ieee80211_key *)&delkey;
		delkey.idk_keyix = keyix-1;
		return athkey_send_command(&athkey, ifname, op,
						(void *)athkey_key);
	case IEEE80211_IOCTL_SETKEY:
		if (argc != 3 && argc != 4)
			usage();
		memset(&setkey, 0, sizeof(setkey));
		setkey.req_key.ik_flags = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
		               if (swencrypt)
		setkey.req_key.ik_flags |= IEEE80211_KEY_SWENCRYPT;
		if (swdecrypt)
			setkey.req_key.ik_flags |= IEEE80211_KEY_SWDECRYPT;
			setkey.req_key.ik_keyix = keyix-1;
		setkey.req_key.ik_type = getcipher(argv[1]);
		setkey.req_key.ik_keylen = getdata(argv[2],
			setkey.req_key.ik_keydata,
			sizeof(setkey.req_key.ik_keydata), 1);
		if (argc == 4)
			(void) getdata(argv[3], setkey.req_key.ik_macaddr,
				IEEE80211_ADDR_LEN, 0);
		setkey.suite = cipher_to_suite(setkey.req_key.ik_type,
					setkey.req_key.ik_keylen);
		if (setkey.req_key.ik_type != IEEE80211_CIPHER_WEP &&
			setkey.req_key.ik_type != IEEE80211_CIPHER_WAPI)
			seq_len = 6;
		else if (setkey.req_key.ik_type == IEEE80211_CIPHER_WAPI)
			seq_len = 8;
		setkey.seq_len = seq_len;

		setkey.req_key.ik_keyrsc = keyrsc;
		setkey.req_key.ik_keytsc = keytsc;
		{
			int i = 0;
			if (seq_len == 6) {
				setkey.seq[6] = 0;
				setkey.seq[7] = 0;
			}
			/* set rxiv value (seq) */
			for(i = 0; i < seq_len; i++) {
				setkey.seq[i] = (keyrsc >>
					(seq_len-1-i)*8) & 0xFF;
			}
		}
		return athkey_send_command(&athkey, ifname, op,
						&setkey);

	case IEEE80211_IOCTL_GETKEY:
		memset(&getkey, 0, sizeof(getkey));
		athkey_key = (struct ieee80211_key *)&getkey;
		if (argv[1] == NULL)
			memset(getkey.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		else
			(void) getdata(argv[1], getkey.ik_macaddr,
                                IEEE80211_ADDR_LEN, 0);
		getkey.ik_keyix = keyix-1;
		if ((err = athkey_send_command(&athkey, ifname, op,
				((void *)athkey_key))))
		{
			if (err != -ENOENT)
				printf("%s: Failed to get encryption data\n",
					__func__);
			return -1;
		}
		if (getkey.ik_type == IEEE80211_CIPHER_NONE) {
			return 0;
		}
		if(get_key == 1)
		{
			printf("%-17.17s %3s %4s %4s\n"
				, "ADDR"
				, "KEYIDX"
				, "CIPHER"
				, "KEY"
				);
		}
		else if(cipher == 1)
		{
			printf("%-17.17s %3s %4s\n"
			, "ADDR"
			, "KEYIDX"
			, "CIPHER"
			);
		}

		if(get_key == 1)
		{
			int i = 0;
			macaddr_printf(getkey.ik_macaddr);
			printf("  %4d   ", keyix);
			cipher_type(getkey.ik_type);
			printf("    %02X%02X", *(getkey.ik_keydata),
				*(getkey.ik_keydata+1));
			for(i=2;i<getkey.ik_keylen && i < 48;i++)
			{
				if( i%2 == 0)
					printf("-");
				printf("%02X", *(getkey.ik_keydata+i));
			}
			printf("\n");
		}
		else if(cipher == 1)
		{
			macaddr_printf(getkey.ik_macaddr);
			printf("  %4d   ", keyix);
			cipher_type(getkey.ik_type);
			printf("\n");
		}
#if UMAC_SUPPORT_CFG80211
		if (IS_CFG80211_ENABLED(&athkey)) {
			destroy_cfg80211_socket(&athkey);
		} else
#endif /* UMAC_SUPPORT_CFG80211 */
		{
			close(s);
		}
		return 0;
	}
	return -1;
}
