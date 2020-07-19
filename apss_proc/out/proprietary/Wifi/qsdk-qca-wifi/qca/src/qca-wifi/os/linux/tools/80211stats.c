/*
 * Copyright (c) 2013,2018 Qualcomm Innovation Center, Inc.
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
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * 80211stats [-i interface]
 * (default interface is ath0).
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <netinet/ether.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>

/*
 * Linux uses __BIG_ENDIAN and __LITTLE_ENDIAN while BSD uses _foo
 * and an explicit _BYTE_ORDER.  Sorry, BSD got there first--define
 * things in the BSD way...
 */
#ifndef	_LITTLE_ENDIAN
#define	_LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#endif
#ifndef	_BIG_ENDIAN
#define	_BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#endif
#ifndef ATH_SUPPORT_LINUX_STA
#include <asm/byteorder.h>
#endif
#if defined(__LITTLE_ENDIAN)
#define	_BYTE_ORDER	_LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define	_BYTE_ORDER	_BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif

#include "os/linux/include/ieee80211_external.h"

#ifndef SIOCG80211STATS
#define	SIOCG80211STATS	(SIOCDEVPRIVATE+2)
#endif

static void
printstats(FILE *fd, const struct ieee80211_stats *stats)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
#define	STAT(x,fmt) \
	if (stats->is_##x) fprintf(fd, "%llu " fmt "\n", stats->is_##x)
	STAT(rx_badversion,	"rx frame with bad version");
	STAT(rx_tooshort,	"rx frame too short");
	STAT(rx_wrongbss,	"rx from wrong bssid");
	STAT(rx_wrongdir,	"rx w/ wrong direction");
	STAT(rx_mcastecho,	"rx discard 'cuz mcast echo");
	STAT(rx_notassoc,	"rx discard 'cuz sta !assoc");
	STAT(rx_noprivacy,	"rx w/ wep but privacy off");
	STAT(rx_decap,		"rx decapsulation failed");
	STAT(rx_mgtdiscard,	"rx discard mgt frames");
	STAT(rx_ctl,		"rx discard ctrl frames");
	STAT(rx_beacon,		"rx beacon frames");
	STAT(rx_rstoobig,	"rx rate set truncated");
	STAT(rx_elem_missing,	"rx required element missing");
	STAT(rx_elem_toobig,	"rx element too big");
	STAT(rx_elem_toosmall,	"rx element too small");
	STAT(rx_elem_unknown,	"rx element unknown");
	STAT(rx_badchan,	"rx frame w/ invalid chan");
	STAT(rx_chanmismatch,	"rx frame chan mismatch");
	STAT(rx_nodealloc,	"nodes allocated (rx)");
	STAT(rx_ssidmismatch,	"rx frame ssid mismatch");
	STAT(rx_auth_unsupported,"rx w/ unsupported auth alg");
	STAT(rx_auth_fail,	"rx sta auth failure");
	STAT(rx_auth_countermeasures,
		"rx sta auth failure 'cuz of TKIP countermeasures");
	STAT(rx_assoc_bss,	"rx assoc from wrong bssid");
	STAT(rx_assoc_notauth,	"rx assoc w/o auth");
	STAT(rx_assoc_capmismatch,"rx assoc w/ cap mismatch");
	STAT(rx_assoc_norate,	"rx assoc w/ no rate match");
	STAT(rx_assoc_badwpaie,	"rx assoc w/ bad WPA IE");
	STAT(rx_deauth,		"rx deauthentication");
	STAT(rx_disassoc,	"rx disassociation");
	STAT(rx_action,		"rx action mgt");
	STAT(rx_badsubtype,	"rx frame w/ unknown subtype");
	STAT(rx_nobuf,		"rx failed for lack of sk_buffer");
	STAT(rx_ahdemo_mgt,
		"rx discard mgmt frame received in ahdoc demo mode");
	STAT(rx_bad_auth,	"rx bad authentication request");
	STAT(rx_unauth,		"rx discard 'cuz port unauthorized");
	STAT(rx_badcipher,	"rx failed 'cuz bad cipher/key type");
	STAT(rx_nocipherctx,	"rx failed 'cuz key/cipher ctx not setup");
	STAT(rx_acl,		"rx discard 'cuz acl policy");
	STAT(rx_ffcnt,		"rx fast frames");
	STAT(rx_badathtnl,   	"rx fast frame failed 'cuz bad tunnel header");
	STAT(tx_nobuf,		"tx failed for lack of sk_buffer");
	STAT(tx_nonode,		"tx failed for no node");
	STAT(tx_unknownmgt,	"tx of unknown mgt frame");
	STAT(tx_badcipher,	"tx failed 'cuz bad ciper/key type");
	STAT(tx_nodefkey,	"tx failed 'cuz no defkey");
	STAT(tx_noheadroom,	"tx failed 'cuz no space for crypto hdrs");
	STAT(tx_ffokcnt,	"tx atheros fast frames successful");
	STAT(tx_fferrcnt,	"tx atheros fast frames failed");
	STAT(scan_active,	"active scans started");
	STAT(scan_passive,	"passive scans started");
	STAT(node_timeout,	"nodes timed out inactivity");
	STAT(crypto_nomem,	"cipher context malloc failed");
	STAT(crypto_tkip,	"tkip crypto done in s/w");
	STAT(crypto_tkipenmic,	"tkip tx MIC done in s/w");
	STAT(crypto_tkipdemic,	"tkip rx MIC done in s/w");
	STAT(crypto_tkipcm,	"tkip dropped frames 'cuz of countermeasures");
	STAT(crypto_ccmp,	"ccmp crypto done in s/w");
	STAT(crypto_wep,	"wep crypto done in s/w");
	STAT(crypto_setkey_cipher,"setkey failed 'cuz cipher rejected data");
	STAT(crypto_setkey_nokey,"setkey failed 'cuz no key index");
	STAT(crypto_delkey,	"driver key delete failed");
	STAT(crypto_badcipher,	"setkey failed 'cuz unknown cipher");
	STAT(crypto_nocipher,	"setkey failed 'cuz cipher module unavailable");
	STAT(crypto_attachfail,	"setkey failed 'cuz cipher attach failed");
	STAT(crypto_swfallback,	"crypto fell back to s/w implementation");
	STAT(crypto_keyfail,	"setkey faied 'cuz driver key alloc failed");
    STAT(crypto_enmicfail, "en-MIC failed ");
    STAT(ibss_capmismatch, " merge failed-cap mismatch");
    STAT(ibss_norate," merge failed-rate mismatch");
    STAT(ps_unassoc,"ps-poll for unassoc. sta ");
    STAT(ps_badaid, " ps-poll w/ incorrect aid ");
    STAT(ps_qempty," ps-poll w/ nothing to send ");
#undef STAT
#undef N
}

static void
print_mac_stats(FILE *fd, const struct ieee80211_mac_stats *stats, int unicast)
{
#define	STAT(x,fmt) \
	if (stats->ims_##x) fprintf(fd, "%lu " fmt "\n", (long unsigned int) stats->ims_##x)

    fprintf(fd, "%s\n", unicast ? "\t**** 80211 unicast stats **** " : "\t **** 80211 multicast stats ****");
    STAT(tx_packets, "frames successfully transmitted ");
    STAT(rx_packets, "frames successfully received ");

    /* Decryption errors */
    STAT(rx_unencrypted, "rx w/o wep and privacy on ");
    STAT(rx_badkeyid,    "rx w/ incorrect keyid ");
    STAT(rx_decryptok,   "rx decrypt okay ");
    STAT(rx_decryptcrc,  "rx decrypt failed on crc ");
    STAT(rx_wepfail,     "rx wep processing failed ");
    STAT(rx_tkipreplay,  "rx seq# violation (TKIP) ");
    STAT(rx_tkipformat,  "rx format bad (TKIP) ");
    STAT(rx_tkipmic,     "rx MIC check failed (TKIP) ");
    STAT(rx_tkipicv,     "rx ICV check failed (TKIP) ");
    STAT(rx_ccmpreplay,  "rx seq# violation (CCMP) ");
    STAT(rx_ccmpformat,  "rx format bad (CCMP) ");
    STAT(rx_ccmpmic,     "rx MIC check failed (CCMP) ");

    /* Other Tx/Rx errors */
    STAT(tx_discard,     "tx dropped by NIC ");
    STAT(rx_discard,     "rx dropped by NIC ");
    STAT(rx_countermeasure, "rx TKIP countermeasure activation count ");
#undef STAT
}

struct ifreq ifr;
int	s;

static void
print_sta_stats(FILE *fd, const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
#define	STAT(x,fmt) \
	if (ns->ns_##x) { fprintf(fd, "%s" #x " " fmt, sep, ns->ns_##x); sep = " "; }
#define	STAT64(x,fmt) \
	if (ns->ns_##x) { fprintf(fd, "%s" #x " " fmt, sep, (long long unsigned int) ns->ns_##x); sep = " "; }
	struct iwreq iwr;
	struct ieee80211req_sta_stats stats;
	const struct ieee80211_nodestats *ns = &stats.is_stats;
	const char *sep;

	(void) memset(&iwr, 0, sizeof(iwr));

	if (strlcpy(iwr.ifr_name, ifr.ifr_name, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
		errx(1, "Interface name too long\n");
		return;
	}

	iwr.u.data.pointer = (void *) &stats;
	iwr.u.data.length = sizeof(stats);
	memcpy(stats.is_u.macaddr, macaddr, IEEE80211_ADDR_LEN);
	if (ioctl(s, IEEE80211_IOCTL_STA_STATS, &iwr) < 0)
		err(1, "unable to get station stats for %s",
			ether_ntoa((const struct ether_addr*) macaddr));

	fprintf(fd, "%s:\n", ether_ntoa((const struct ether_addr*) macaddr));

	sep = "\t";
	STAT(rx_data, "%u");
	STAT(rx_mgmt, "%u");
	STAT(rx_ctrl, "%u");
	STAT64(rx_beacons, "%llu");
	STAT(rx_proberesp, "%u");
	STAT(rx_ucast, "%u");
	STAT(rx_mcast, "%u");
	STAT(rx_bytes, "%llu");
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	STAT(rx_ucast_bytes, "%llu");
	STAT(rx_mcast_bytes, "%llu");
#endif
	STAT(rx_dup, "%u");
	STAT(rx_noprivacy, "%u");
	STAT(rx_wepfail, "%u");
	STAT(rx_demicfail, "%u");
	STAT(rx_decap, "%u");
	STAT(rx_defrag, "%u");
	STAT(rx_disassoc, "%u");
	STAT(rx_deauth, "%u");
	STAT(rx_action, "%u");
	STAT(rx_decryptcrc, "%u");
	STAT(rx_unauth, "%u");
	STAT(rx_unencrypted, "%u");
	fprintf(fd, "\n");

	sep = "\t";
	STAT(tx_data, "%u");
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	STAT(tx_data_success, "%u");
#endif
	STAT(tx_mgmt, "%u");
	STAT(tx_probereq, "%u");
	STAT(tx_ucast, "%u");
	STAT(tx_mcast, "%u");
	STAT(tx_bytes, "%llu");
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	STAT(tx_bytes_success, "%llu");
	STAT(tx_ucast_bytes, "%llu");
	STAT(tx_mcast_bytes, "%llu");
#endif
	STAT(last_per, "%llu");
	STAT(tx_novlantag, "%u");
	STAT(tx_vlanmismatch, "%u");
	fprintf(fd, "\n");

	sep = "\t";
	STAT(tx_assoc, "%u");
	STAT(tx_assoc_fail, "%u");
	STAT(tx_auth, "%u");
	STAT(tx_auth_fail, "%u");
	STAT(tx_deauth, "%u");
	STAT(tx_deauth_code, "%u");
	STAT(tx_disassoc, "%u");
	STAT(tx_disassoc_code, "%u");
	STAT(psq_drops, "%u");
	fprintf(fd, "\n");

	sep = "\t";
	STAT(tx_uapsd, "%u");
	STAT(uapsd_triggers, "%u");
	fprintf(fd, "\n");
	sep = "\t";
	STAT(uapsd_duptriggers, "%u");
	STAT(uapsd_ignoretriggers, "%u");
	fprintf(fd, "\n");
	sep = "\t";
	STAT(uapsd_active, "%u");
	STAT(uapsd_triggerenabled, "%u");
	fprintf(fd, "\n");
	sep = "\t";
	STAT(tx_eosplost, "%u");
	fprintf(fd, "\n");
#undef STAT
#undef STAT64
}

#define LIST_STATION_IOC_ALLOC_SIZE (24*1024)

int
main(int argc, char *argv[])
{
	struct iwreq iwr;
	int c, len;
	struct ieee80211req_sta_info *si;
	u_int8_t *cp;
	int allnodes = 0;
	u_int32_t buflen = 0;
	u_int8_t *buf = NULL;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");
	strlcpy(ifr.ifr_name, "ath0", sizeof (ifr.ifr_name));
	while ((c = getopt(argc, argv, "ai:")) != -1)
		switch (c) {
		case 'a':
			allnodes++;
			break;
		case 'i':
			if (strlcpy(ifr.ifr_name, optarg, sizeof (ifr.ifr_name)) >= sizeof (ifr.ifr_name)) {
				errx(1, "Interface name too long\n");
			}
			break;
		default:
			errx(1, "usage: 80211stats [-a] [-i device] [mac...]\n");
			/*NOTREACHED*/
		}

	if (argc == optind && !allnodes) {
		struct ieee80211_stats *stats;

		/* no args, just show global stats */
        /* fetch both ieee80211_stats, and mac_stats, including multicast and unicast stats */
        stats = malloc(sizeof(struct ieee80211_stats)+ 2* sizeof(struct ieee80211_mac_stats));
        if (!stats) {
            fprintf (stderr, "Unable to allocate memory for stats\n");
            return -1;
        }
		ifr.ifr_data = (caddr_t) stats;
		if (ioctl(s, SIOCG80211STATS, &ifr) < 0)
			err(1, "%s", ifr.ifr_name);
		printstats(stdout, stats);
        /* MAC stats uses, u_int64_t, there is a 8 byte hole in between stats and mac stats, 
         * account for that.
         */
        print_mac_stats(stdout, (struct ieee80211_mac_stats*)(((void*)stats)+sizeof(*stats)+8), 1); 
        /* multicast stats */
        print_mac_stats(stdout, &((struct ieee80211_mac_stats *)(((void*)stats)+sizeof(*stats)+8))[1], 0);
                    
        free(stats);
		return 0;
	}
	if (allnodes) {
                buflen = LIST_STATION_IOC_ALLOC_SIZE;
		buf = (u_int8_t *)malloc(buflen);
		if (NULL == buf) {
			printf("Unable to allocate memory for node information\n");
			return -1;
		}

		/*
		 * Retrieve station/neighbor table and print stats for each.
		 */
		(void) memset(&iwr, 0, sizeof(iwr));

		if (strlcpy(iwr.ifr_name, ifr.ifr_name, sizeof(iwr.ifr_name)) >= sizeof(iwr.ifr_name)) {
			errx(1, "Interface name too long\n");
		}

                iwr.u.data.pointer = buf;
                iwr.u.data.length = buflen;
		if (ioctl(s, IEEE80211_IOCTL_STA_INFO, &iwr) < 0)
			err(1, "unable to get station information");
		len = iwr.u.data.length;
		if (len >= sizeof(struct ieee80211req_sta_info)) {
			cp = buf;
			do {
				si = (struct ieee80211req_sta_info *) cp;
				print_sta_stats(stdout, si->isi_macaddr);
				cp += si->isi_len, len -= si->isi_len;
			} while (len >= sizeof(struct ieee80211req_sta_info));
		}
		if (buf != NULL) {
			free(buf);

		}
	} else {
		/*
		 * Print stats for specified stations.
		 */
		for (c = optind; c < argc; c++) {
			const struct ether_addr *ea = ether_aton(argv[c]);
			if (ea != NULL)
				print_sta_stats(stdout, ea->ether_addr_octet);
		}
	}
        return 0;       	
}
