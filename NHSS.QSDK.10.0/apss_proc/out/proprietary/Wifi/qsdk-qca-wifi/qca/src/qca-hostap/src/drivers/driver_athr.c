/*
 * Driver interaction with Atheros Linux driver
 * Copyright (c) 2003-2010, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2010, Atheros Communications
 * Copyright (c) 2011, Qualcomm Atheros
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if_arp.h>

#include "linux_wext.h"
#include "common.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_common.h"
#include "l2_packet/l2_packet.h"
#include "p2p/p2p.h"
#include "priv_netlink.h"
#include "netlink.h"
#include "linux_ioctl.h"
#include "driver.h"

#define _BYTE_ORDER _BIG_ENDIAN
#undef WPA_OUI_TYPE

#include "ieee80211_external.h"

enum tdls_frame_type {
	IEEE80211_TDLS_SETUP_REQ,
	IEEE80211_TDLS_SETUP_RESP,
	IEEE80211_TDLS_SETUP_CONFIRM,
	IEEE80211_TDLS_TEARDOWN,
	IEEE80211_TDLS_ENABLE_LINK = 251,
	IEEE80211_TDLS_ERROR = 252,
	IEEE80211_TDLS_TEARDOWN_DELETE_NODE = 253,
	IEEE80211_TDLS_LEARNED_ARP = 254,
	IEEE80211_TDLS_MSG_MAX = 255
};

struct driver_atheros_data {
	void *ctx;
	struct netlink_data *netlink;
	int ioctl_sock;
	int mlme_sock;
	char ifname[IFNAMSIZ + 1];
	char shared_ifname[IFNAMSIZ];

	int ifindex;
	int ifindex2;
	int if_removed;
	u8 *assoc_req_ies;
	size_t assoc_req_ies_len;
	u8 *assoc_resp_ies;
	size_t assoc_resp_ies_len;
	struct wpa_driver_capa capa;
	int ignore_scan_done;
	int has_capability;
	int we_version_compiled;

	struct l2_packet_data *l2;

	int operstate;

	char mlmedev[IFNAMSIZ + 1];

	int report_probe_req;
	int last_assoc_mode;
	int assoc_event_sent;
	unsigned int pending_set_chan_freq;
	unsigned int pending_set_chan_dur;
	/* Last IOC_P2P_SET_CHANNEL req_ie */
	unsigned int req_id;
	u8  own_addr[ETH_ALEN];

	int drv_in_scan;
	int drv_in_remain_on_chan;
	enum { ATHR_FULL_SCAN, ATHR_PARTIAL_SCAN } prev_scan_type;

	int start_hostap;

	int country_code;

	int best_24_freq;
	int best_5_freq;
	int best_overall_freq;

	int opmode;
	int disabled;
};

#if 0
static int driver_atheros_flush_pmkid(void *priv);
#endif

static int driver_atheros_set_mlme(struct driver_atheros_data *drv, int op,
				   const u8 *bssid, const u8 *ssid);
static void driver_atheros_scan_timeout(void *eloop_ctx, void *timeout_ctx);

static int driver_atheros_deinit_ap(void *priv);
static int driver_atheros_start_ap(struct driver_atheros_data *drv,
				   struct wpa_driver_associate_params *params);
static int driver_atheros_get_range(void *priv);
static int driver_atheros_finish_drv_init(struct driver_atheros_data *drv);
static void athr_clear_bssid(struct driver_atheros_data *drv);
static int
set80211big(struct driver_atheros_data *drv, int op,const void *data, int len);
static int set80211param(struct driver_atheros_data *drv, int op, int arg,
			 int show_err);
static int set80211param_ifname(struct driver_atheros_data *drv,
				const char *ifname,
				int op, int arg, int show_err);
static int set80211priv(struct driver_atheros_data *drv, int op, void *data,
			int len, int show_err);
static int driver_atheros_set_cipher(struct driver_atheros_data *drv, int type,
				     unsigned int suite);
static int get80211param(struct driver_atheros_data *drv, char *ifname, int op,
			 int *data);

static int driver_atheros_set_wpa_ie(struct driver_atheros_data *drv,
				     const u8 *wpa_ie, size_t wpa_ie_len);

#ifdef CONFIG_TDLS
static int athr_tdls_enable(struct driver_atheros_data *drv, int enabled);
#endif /* CONFIG_TDLS */

static const char * athr_get_ioctl_name(int op)
{
	switch (op) {
	case IEEE80211_IOCTL_SETPARAM:
		return "SETPARAM";
	case IEEE80211_IOCTL_GETPARAM:
		return "GETPARAM";
	case IEEE80211_IOCTL_SETKEY:
		return "SETKEY";
	case IEEE80211_IOCTL_SETWMMPARAMS:
		return "SETWMMPARAMS";
	case IEEE80211_IOCTL_DELKEY:
		return "DELKEY";
	case IEEE80211_IOCTL_GETWMMPARAMS:
		return "GETWMMPARAMS";
	case IEEE80211_IOCTL_SETMLME:
		return "SETMLME";
	case IEEE80211_IOCTL_GETCHANINFO:
		return "GETCHANINFO";
	case IEEE80211_IOCTL_SETOPTIE:
		return "SETOPTIE";
	case IEEE80211_IOCTL_GETOPTIE:
		return "GETOPTIE";
	case IEEE80211_IOCTL_ADDMAC:
		return "ADDMAC";
	case IEEE80211_IOCTL_DELMAC:
		return "DELMAC";
	case IEEE80211_IOCTL_GETCHANLIST:
		return "GETCHANLIST";
	case IEEE80211_IOCTL_SETCHANLIST:
		return "SETCHANLIST";
	case IEEE80211_IOCTL_KICKMAC:
		return "KICKMAC";
	case IEEE80211_IOCTL_CHANSWITCH:
		return "CHANSWITCH";
	case IEEE80211_IOCTL_GETMODE:
		return "GETMODE";
	case IEEE80211_IOCTL_SETMODE:
		return "SETMODE";
	case IEEE80211_IOCTL_GET_APPIEBUF:
		return "GET_APPIEBUF";
	case IEEE80211_IOCTL_SET_APPIEBUF:
		return "SET_APPIEBUF";
	case IEEE80211_IOCTL_SET_ACPARAMS:
		return "SET_ACPARAMS";
	case IEEE80211_IOCTL_FILTERFRAME:
		return "FILTERFRAME";
	case IEEE80211_IOCTL_SET_RTPARAMS:
		return "SET_RTPARAMS";
	case IEEE80211_IOCTL_SET_MEDENYENTRY:
		return "SET_MEDENYENTRY";
	case IEEE80211_IOCTL_GET_MACADDR:
		return "GET_MACADDR";
	case IEEE80211_IOCTL_SET_HBRPARAMS:
		return "SET_HBRPARAMS";
	case IEEE80211_IOCTL_SET_RXTIMEOUT:
		return "SET_RXTIMEOUT";
	case IEEE80211_IOCTL_P2P_BIG_PARAM:
		return "P2P_BIG_PARAM";
	case IEEE80211_IOC_P2P_GO_NOA:
		return "P2P_GO_NOA";
	default:
		return "??";
	}
}


static const char * athr_get_param_name(int op)
{
	switch (op) {
	case IEEE80211_PARAM_PRIVACY:
		return "PRIVACY";
	case IEEE80211_PARAM_COUNTERMEASURES:
		return "COUNTERMEASURES";
	case IEEE80211_PARAM_DROPUNENCRYPTED:
		return "DROPUNENCRYPTED";
	case IEEE80211_PARAM_AMPDU:
		return "AMPDU";
	case IEEE80211_PARAM_SLEEP:
		return "SLEEP";
	case IEEE80211_PARAM_PSPOLL:
		return "PSPOLL";
	case IEEE80211_PARAM_NETWORK_SLEEP:
		return "NETWORK_SLEEP";
	case IEEE80211_PARAM_UAPSDINFO:
		return "UAPSDINFO";
	case IEEE80211_PARAM_COUNTRYCODE:
		return "COUNTRYCODE";
	case IEEE80211_IOC_P2P_GO_OPPPS:
		return "P2P_GO_OPPPS";
	case IEEE80211_IOC_P2P_GO_CTWINDOW:
		return "P2P_GO_CTWINDOW";
	case IEEE80211_IOC_SCAN_REQ:
		return "SCAN_REQ";
	case IEEE80211_IOC_SCAN_RESULTS:
		return "SCAN_RESULTS";
	case IEEE80211_IOC_SSID:
                return "SSID";
	case IEEE80211_IOC_MLME:
                return "MLME";
	case IEEE80211_IOC_CHANNEL:
		return "CHANNEL";
	case IEEE80211_IOC_WPA:
		return "WPA";
	case IEEE80211_IOC_AUTHMODE:
		return "AUTHMODE";
	case IEEE80211_IOC_KEYMGTALGS:
		return "KEYMGTALGS";
	case IEEE80211_IOC_WPS_MODE:
		return "WPS_MODE";
	case IEEE80211_IOC_UCASTCIPHERS:
		return "UCASTCIPHERS";
	case IEEE80211_IOC_UCASTCIPHER:
		return "UCASTCIPHER";
	case IEEE80211_IOC_MCASTCIPHER:
		return "MCASTCIPHER";
	case IEEE80211_IOC_START_HOSTAP:
		return "START_HOSTAP";
	case IEEE80211_IOC_DROPUNENCRYPTED:
		return "DROPUNENCRYPTED";
	case IEEE80211_IOC_PRIVACY:
		return "PRIVACY";
	case IEEE80211_IOC_OPTIE:
		return "OPTIE";
	case IEEE80211_IOC_BSSID:
		return "BSSID";
	case IEEE80211_IOC_P2P_SET_CHANNEL:
		return "P2P_SET_CHANNEL";
	case IEEE80211_IOC_P2P_CANCEL_CHANNEL:
		return "P2P_CANCEL_CHANNEL";
	case IEEE80211_IOC_P2P_SEND_ACTION:
		return "P2P_SEND_ACTION";
	case IEEE80211_IOC_P2P_OPMODE:
		return "P2P_OPMODE";
	case IEEE80211_IOC_SCAN_FLUSH:
		return "SCAN_FLUSH";
	case IEEE80211_IOC_CONNECTION_STATE:
		return "CONNECTION_STATE";
#ifdef IEEE80211_IOC_P2P_FIND_BEST_CHANNEL
	case IEEE80211_IOC_P2P_FIND_BEST_CHANNEL:
		return "P2P_FIND_BEST_CHANNEL";
#endif /* IEEE80211_IOC_P2P_FIND_BEST_CHANNEL */
	default:
		return "??";
	}
}

int driver_atheros_alg_to_cipher_suite(int alg, int key_len)
{
        switch (alg) {
#ifdef ATH_GCM_SUPPORT
        case WPA_ALG_CCMP_256:
                return WPA_CIPHER_CCMP_256;
        case WPA_ALG_GCMP_256:
                return WPA_CIPHER_GCMP_256;
        case WPA_ALG_GCMP:
                return WPA_CIPHER_GCMP;
#endif /*ATH_GCM_SUPPORT*/
        case WPA_ALG_CCMP:
                return WPA_CIPHER_CCMP;
        case WPA_ALG_TKIP:
                return WPA_CIPHER_TKIP;
        case WPA_ALG_WEP:
		if (key_len == 5)
			return WPA_CIPHER_WEP40;
                return WPA_CIPHER_WEP104;
        case WPA_ALG_IGTK:
                return WPA_CIPHER_AES_128_CMAC;
#ifdef ATH_GCM_SUPPORT
        case WPA_ALG_BIP_CMAC_256:
                return WPA_CIPHER_BIP_CMAC_256;
        case WPA_ALG_BIP_GMAC_128:
                return WPA_CIPHER_BIP_GMAC_128;
        case WPA_ALG_BIP_GMAC_256:
                return WPA_CIPHER_BIP_GMAC_256;
#endif /* ATH_GCM_SUPPORT */
        }
        return WPA_CIPHER_NONE;
}
int driver_atheros_set_auth_param(struct driver_atheros_data *drv, int idx,
				  u32 value)
{
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.param.flags = idx & IW_AUTH_INDEX;
	iwr.u.param.value = value;

	if (ioctl(drv->ioctl_sock, SIOCSIWAUTH, &iwr) < 0) {
		if (errno != EOPNOTSUPP) {
			wpa_printf(MSG_DEBUG, "WEXT: SIOCSIWAUTH(param %d "
				   "value 0x%x) failed: %s)",
				   idx, value, strerror(errno));
		}
		ret = errno == EOPNOTSUPP ? -2 : -1;
	}

	return ret;
}


static int athr_no_stop_disassoc(struct driver_atheros_data *drv, int val)
{
#ifdef CONFIG_ATHR_RADIO_DISABLE
	return set80211param(drv, IEEE80211_PARAM_NO_STOP_DISASSOC, val, 1);
#else /* CONFIG_ATHR_RADIO_DISABLE */
	return -1;
#endif /* CONFIG_ATHR_RADIO_DISABLE */
}


/**
 * driver_atheros_get_bssid - Get BSSID, SIOCGIWAP
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @bssid: Buffer for BSSID
 * Returns: 0 on success, -1 on failure
 */
int driver_atheros_get_bssid(void *priv, u8 *bssid)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIWAP, &iwr) < 0) {
		perror("ioctl[SIOCGIWAP]");
		ret = -1;
	}
	os_memcpy(bssid, iwr.u.ap_addr.sa_data, ETH_ALEN);

	return ret;
}


/**
 * driver_atheros_set_bssid - Set BSSID, SIOCSIWAP
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @bssid: BSSID
 * Returns: 0 on success, -1 on failure
 */
int driver_atheros_set_bssid(void *priv, const u8 *bssid)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.ap_addr.sa_family = ARPHRD_ETHER;
	if (bssid)
		os_memcpy(iwr.u.ap_addr.sa_data, bssid, ETH_ALEN);
	else
		os_memset(iwr.u.ap_addr.sa_data, 0, ETH_ALEN);

	if (ioctl(drv->ioctl_sock, SIOCSIWAP, &iwr) < 0) {
		perror("ioctl[SIOCSIWAP]");
		ret = -1;
	}

	return ret;
}


/**
 * driver_atheros_get_ssid - Get SSID, SIOCGIWESSID
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @ssid: Buffer for the SSID; must be at least 32 bytes long
 * Returns: SSID length on success, -1 on failure
 */
int driver_atheros_get_ssid(void *priv, u8 *ssid)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) ssid;
	iwr.u.essid.length = 32;

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else {
		ret = iwr.u.essid.length;
		if (ret > 32)
			ret = 32;
		/* Some drivers include nul termination in the SSID, so let's
		 * remove it here before further processing. WE-21 changes this
		 * to explicitly require the length _not_ to include nul
		 * termination. */
		if (ret > 0 && ssid[ret - 1] == '\0' &&
		    drv->we_version_compiled < 21)
			ret--;
	}

	return ret;
}


/**
 * driver_atheros_set_ssid - Set SSID, SIOCSIWESSID
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @ssid: SSID
 * @ssid_len: Length of SSID (0..32)
 * Returns: 0 on success, -1 on failure
 */
int driver_atheros_set_ssid(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;
	char buf[33];

	if (ssid_len > 32)
		return -1;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* flags: 1 = ESSID is active, 0 = not (promiscuous) */
	iwr.u.essid.flags = (ssid_len != 0);
	os_memset(buf, 0, sizeof(buf));
	os_memcpy(buf, ssid, ssid_len);
	iwr.u.essid.pointer = (caddr_t) buf;
	if (drv->we_version_compiled < 21) {
		/* For historic reasons, set SSID length to include one extra
		 * character, C string nul termination, even though SSID is
		 * really an octet string that should not be presented as a C
		 * string. Some Linux drivers decrement the length by one and
		 * can thus end up missing the last octet of the SSID if the
		 * length is not incremented here. WE-21 changes this to
		 * explicitly require the length _not_ to include nul
		 * termination. */
		if (ssid_len)
			ssid_len++;
	}
	iwr.u.essid.length = ssid_len;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		ret = -1;
	}

	return ret;
}


/**
 * driver_atheros_set_freq - Set frequency/channel, SIOCSIWFREQ
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @freq: Frequency in MHz
 * Returns: 0 on success, -1 on failure
 */
int driver_atheros_set_freq(void *priv, int freq)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.freq.m = freq * 100000;
	iwr.u.freq.e = 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWFREQ, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "athr: ioctl[SIOCSIWFREQ] failed: %s",
			   strerror(errno));
		ret = -1;
	}

	return ret;
}


static int athr_get_freq(struct driver_atheros_data *drv, const char *ifname)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIWFREQ, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "athr: ioctl[SIOCGIWFREQ] failed: %s",
			   strerror(errno));
		return 0;
	}

	if (iwr.u.freq.e != 1)
		return 0;
	return iwr.u.freq.m / 100000;
}


static int athr_get_countrycode(struct driver_atheros_data *drv)
{
        union wpa_event_data data;
        int cc;

        os_memset(&data, 0, sizeof(data));
	if (get80211param(drv, drv->ifname, IEEE80211_PARAM_COUNTRYCODE, &cc)
	    < 0) {
		wpa_printf(MSG_DEBUG, "athr: Failed to get country code: %s",
			   strerror(errno));
		return -1;
	}

	if (cc != drv->country_code) {
		wpa_printf(MSG_DEBUG, "athr: Country code changed %d -> %d",
			   drv->country_code, cc);
		drv->country_code = cc;
		wpa_supplicant_event(drv->ctx, EVENT_CHANNEL_LIST_CHANGED,
				     &data);
		return 1;
	}

	return 0;
}


static void
driver_atheros_event_wireless_custom(struct driver_atheros_data *drv,
				     char *custom)
{
	union wpa_event_data data;
	void *ctx = drv->ctx;

	wpa_printf(MSG_MSGDUMP, "WEXT: Custom wireless event: '%s'",
		   custom);

	os_memset(&data, 0, sizeof(data));
	/* Host AP driver */
	if (os_strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		data.michael_mic_failure.unicast =
			os_strstr(custom, " unicast ") != NULL;
		/* TODO: parse parameters(?) */
		wpa_supplicant_event(ctx, EVENT_MICHAEL_MIC_FAILURE, &data);
	} else if (os_strncmp(custom, "ASSOCINFO(ReqIEs=", 17) == 0) {
		char *spos;
		int bytes;
		u8 *req_ies = NULL, *resp_ies = NULL;

		spos = custom + 17;

		bytes = strspn(spos, "0123456789abcdefABCDEF");
		if (!bytes || (bytes & 1))
			return;
		bytes /= 2;

		req_ies = os_malloc(bytes);
		if (req_ies == NULL ||
		    hexstr2bin(spos, req_ies, bytes) < 0)
			goto done;
		data.assoc_info.req_ies = req_ies;
		data.assoc_info.req_ies_len = bytes;

		spos += bytes * 2;

		data.assoc_info.resp_ies = NULL;
		data.assoc_info.resp_ies_len = 0;

		if (os_strncmp(spos, " RespIEs=", 9) == 0) {
			spos += 9;

			bytes = strspn(spos, "0123456789abcdefABCDEF");
			if (!bytes || (bytes & 1))
				goto done;
			bytes /= 2;

			resp_ies = os_malloc(bytes);
			if (resp_ies == NULL ||
			    hexstr2bin(spos, resp_ies, bytes) < 0)
				goto done;
			data.assoc_info.resp_ies = resp_ies;
			data.assoc_info.resp_ies_len = bytes;
		}

		data.assoc_info.freq = athr_get_freq(drv, drv->ifname);

		wpa_supplicant_event(ctx, EVENT_ASSOCINFO, &data);

	done:
		os_free(resp_ies);
		os_free(req_ies);
#ifdef CONFIG_PEERKEY
	} else if (os_strncmp(custom, "STKSTART.request=", 17) == 0) {
                if (hwaddr_aton(custom + 17, data.stkstart.peer)) {
                        wpa_printf(MSG_DEBUG, "WEXT: unrecognized "
                                   "STKSTART.request '%s'", custom + 17);
                        return;
                }
		wpa_supplicant_event(ctx, EVENT_STKSTART, &data);
#endif /* CONFIG_PEERKEY */
	} else if (os_strncmp(custom, "PUSH-BUTTON.indication", 22) == 0) {
		char *durp;
		int duration;

		durp = os_strstr(custom, "dur=");
		if (durp) {
			durp += 4;
			duration = atoi(durp);
			wpa_msg(ctx, MSG_INFO, "WPS-PUSH-BUTTON dur=%d",
				duration);
		}
		wpa_printf(MSG_DEBUG, "athr: WPS push button pressed for "
			   "client VAP");
		wpa_supplicant_event(ctx, EVENT_WPS_BUTTON_PUSHED, &data);
#ifdef CONFIG_IEEE80211V
	} else if (os_strncmp(custom, "MLME-SLEEPMODE.request(action=", 30) ==
		   0) {
		char *pos;
		pos = os_strstr(custom, "action=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				   "MLME-SLEEPMODE.request without action "
				   "type ignored");
			return;
		}
		data.wnm.sleep_action = atoi(pos + 7);
		pos = os_strstr(custom, "intval=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				   "MLME-SLEEPMODE.request without intval "
				   "ignored");
			return;
		}
		data.wnm.sleep_intval = atoi(pos + 7);
		data.wnm.oper = WNM_OPER_SLEEP;

		wpa_supplicant_event(ctx, EVENT_WNM, &data);
#endif /* CONFIG_IEEE80211V */
	}
}


static int driver_atheros_event_wireless_michaelmicfailure(
	void *ctx, const char *ev, size_t len)
{
	const struct iw_michaelmicfailure *mic;
	union wpa_event_data data;

	if (len < sizeof(*mic))
		return -1;

	mic = (const struct iw_michaelmicfailure *) ev;

	wpa_printf(MSG_DEBUG, "Michael MIC failure wireless event: "
		   "flags=0x%x src_addr=" MACSTR, mic->flags,
		   MAC2STR(mic->src_addr.sa_data));

	os_memset(&data, 0, sizeof(data));
	data.michael_mic_failure.unicast = !(mic->flags & IW_MICFAILURE_GROUP);
	wpa_supplicant_event(ctx, EVENT_MICHAEL_MIC_FAILURE, &data);

	return 0;
}


static int driver_atheros_event_wireless_pmkidcand(
	struct driver_atheros_data *drv, const char *ev, size_t len)
{
	const struct iw_pmkid_cand *cand;
	union wpa_event_data data;
	const u8 *addr;

	if (len < sizeof(*cand))
		return -1;

	cand = (const struct iw_pmkid_cand *) ev;
	addr = (const u8 *) cand->bssid.sa_data;

	wpa_printf(MSG_DEBUG, "PMKID candidate wireless event: "
		   "flags=0x%x index=%d bssid=" MACSTR, cand->flags,
		   cand->index, MAC2STR(addr));

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.pmkid_candidate.bssid, addr, ETH_ALEN);
	data.pmkid_candidate.index = cand->index;
	data.pmkid_candidate.preauth = cand->flags & IW_PMKID_CAND_PREAUTH;
	wpa_supplicant_event(drv->ctx, EVENT_PMKID_CANDIDATE, &data);

	return 0;
}


static int driver_atheros_event_wireless_assocreqie(
	struct driver_atheros_data *drv, const char *ev, int len)
{
	if (len < 0)
		return -1;

	wpa_hexdump(MSG_DEBUG, "AssocReq IE wireless event", (const u8 *) ev,
		    len);
#ifdef CONFIG_TDLS
	if (len >= 17 && os_strncmp(ev, "STKSTART.request=", 17) == 0) {
		int hdrlen = 17;
		const char *spos;
		u16 mtype, mlen;
		union wpa_event_data data;

		wpa_printf(MSG_DEBUG, "athr: TDLS event in IWEVASSOCREQIE");
		wpa_hexdump_ascii(MSG_DEBUG, "athr: TDLS event",
				  (const u8 *) ev, len);
		if (len >= 25 &&
		    os_strncmp(ev, "STKSTART.request=TearDown", 25) == 0)
			hdrlen = 25;

		spos = ev + hdrlen;
		os_memset(&data, 0, sizeof(data));
		os_memcpy(data.tdls.peer, spos, ETH_ALEN);
		spos += ETH_ALEN;

		os_memcpy(&mtype, spos, sizeof(u16));
		spos += sizeof(u16);

		switch (mtype) {
		case IEEE80211_TDLS_TEARDOWN:
			wpa_printf(MSG_DEBUG, "athr: TDLSSTART event "
				   "requesting FTIE for Teardown");
			data.tdls.oper = TDLS_REQUEST_TEARDOWN;
			break;
		case IEEE80211_TDLS_SETUP_REQ:
			wpa_printf(MSG_DEBUG, "athr: TDLSSTART event "
				   "requesting TDLS Setup");
			data.tdls.oper = TDLS_REQUEST_SETUP;
			break;
		default:
			wpa_printf(MSG_DEBUG, "athr: Unknown TDLSSTART event "
				   "type %u", mtype);
			return -1;
		}

		os_memcpy(&mlen, spos, sizeof(u16));
		spos += sizeof(u16);
		if (mlen > 1000) {
			wpa_printf(MSG_DEBUG, "athr: Unexpectedly long "
				   "TDLSSTART event length %u - drop it",
				   mlen);
			return -1;
		}

		if (mtype == IEEE80211_TDLS_TEARDOWN && mlen >= 2)
			os_memcpy(&data.tdls.reason_code, spos, 2);

		wpa_supplicant_event(drv->ctx, EVENT_TDLS, &data);
		return 0;
	}
#endif /* CONFIG_TDLS */
	os_free(drv->assoc_req_ies);
	drv->assoc_req_ies = os_malloc(len);
	if (drv->assoc_req_ies == NULL) {
		drv->assoc_req_ies_len = 0;
		return -1;
	}
	os_memcpy(drv->assoc_req_ies, ev, len);
	drv->assoc_req_ies_len = len;
	athr_get_countrycode(drv);

	return 0;
}


static int driver_atheros_event_wireless_assocrespie(
	struct driver_atheros_data *drv, const char *ev, int len)
{
	if (len < 0)
		return -1;

	wpa_hexdump(MSG_DEBUG, "AssocResp IE wireless event", (const u8 *) ev,
		    len);
	os_free(drv->assoc_resp_ies);
	drv->assoc_resp_ies = os_malloc(len);
	if (drv->assoc_resp_ies == NULL) {
		drv->assoc_resp_ies_len = 0;
		return -1;
	}
	os_memcpy(drv->assoc_resp_ies, ev, len);
	drv->assoc_resp_ies_len = len;

	return 0;
}


static void driver_atheros_event_assoc_ies(struct driver_atheros_data *drv)
{
	union wpa_event_data data;
	int freq;

	freq = athr_get_freq(drv, drv->ifname);

	if (drv->assoc_req_ies == NULL && drv->assoc_resp_ies == NULL &&
	    freq == 0)
		return;

	os_memset(&data, 0, sizeof(data));
	if (drv->assoc_req_ies) {
		data.assoc_info.req_ies = drv->assoc_req_ies;
		data.assoc_info.req_ies_len = drv->assoc_req_ies_len;
	}
	if (drv->assoc_resp_ies) {
		data.assoc_info.resp_ies = drv->assoc_resp_ies;
		data.assoc_info.resp_ies_len = drv->assoc_resp_ies_len;
	}
	data.assoc_info.freq = freq;

	wpa_supplicant_event(drv->ctx, EVENT_ASSOCINFO, &data);

	os_free(drv->assoc_req_ies);
	drv->assoc_req_ies = NULL;
	os_free(drv->assoc_resp_ies);
	drv->assoc_resp_ies = NULL;
}


static void send_action_cb_event(
	struct driver_atheros_data *drv,
	char *data, size_t data_len)
{
	union wpa_event_data event;
	struct ieee80211_send_action_cb *sa;
	const struct ieee80211_hdr *hdr;
	u16 fc;

	if (data_len < sizeof(*sa) + 24) {
		wpa_printf(MSG_DEBUG, "athr: Too short event message "
			   "(data_len=%d sizeof(*sa)=%d)",
			   (int) data_len, (int) sizeof(*sa));
		wpa_hexdump(MSG_DEBUG, "athr: Short event message",
			    (u8 *) data, data_len);
		return;
	}

	sa = (struct ieee80211_send_action_cb *) data;

	hdr = (const struct ieee80211_hdr *) (sa + 1);
	fc = le_to_host16(hdr->frame_control);

	os_memset(&event, 0, sizeof(event));
	event.tx_status.type = WLAN_FC_GET_TYPE(fc);
	event.tx_status.stype = WLAN_FC_GET_STYPE(fc);
	event.tx_status.dst = sa->dst_addr;
	event.tx_status.data = (u8 *) hdr;
	event.tx_status.data_len = data_len - sizeof(*sa);
	event.tx_status.ack = sa->ack;
	wpa_supplicant_event(drv->ctx, EVENT_TX_STATUS, &event);

}


/*
* Handle size of data problem. WEXT only allows data of 256 bytes for custom
* events, and p2p data can be much bigger. So the athr driver sends a small
* event telling me to collect the big data with an ioctl.
* On the first event, send all pending events to supplicant.
*/
static void fetch_pending_big_events(struct driver_atheros_data *drv)
{
	union wpa_event_data event;
	const struct ieee80211_mgmt *mgmt;
	u8 tbuf[IW_PRIV_SIZE_MASK]; /* max size is 2047 bytes */
	u16 fc, stype;
	struct iwreq iwr;
	size_t data_len;
	u32 freq, frame_type;

	while (1) {
		os_memset(&iwr, 0, sizeof(iwr));
		os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

		iwr.u.data.pointer = (void *) tbuf;
		iwr.u.data.length = sizeof(tbuf);
		iwr.u.data.flags = IEEE80211_IOC_P2P_FETCH_FRAME;

		if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr)
		    < 0) {
			if (errno == ENOSPC) {
				wpa_printf(MSG_DEBUG, "%s:%d exit",
					   __func__, __LINE__);
				return;
			}
			wpa_printf(MSG_DEBUG, "athr: %s: P2P_BIG_PARAM["
				   "P2P_FETCH_FRAME] failed: %s",
				   __func__, strerror(errno));
			return;
		}
		data_len = iwr.u.data.length;
		wpa_hexdump(MSG_DEBUG, "athr: P2P_FETCH_FRAME data",
			    (u8 *) tbuf, data_len);
		if (data_len < sizeof(freq) + sizeof(frame_type) + 24) {
			wpa_printf(MSG_DEBUG, "athr: frame too short");
			continue;
		}
		os_memcpy(&freq, tbuf, sizeof(freq));
		os_memcpy(&frame_type, &tbuf[sizeof(freq)],
			  sizeof(frame_type));
		mgmt = (void *) &tbuf[sizeof(freq) + sizeof(frame_type)];
		data_len -= sizeof(freq) + sizeof(frame_type);

		if (frame_type == IEEE80211_EV_RX_MGMT) {
			fc = le_to_host16(mgmt->frame_control);
			stype = WLAN_FC_GET_STYPE(fc);

			wpa_printf(MSG_DEBUG, "athr: EV_RX_MGMT stype=%u "
				"freq=%u len=%u", stype, freq, (int) data_len);

			if (stype == WLAN_FC_STYPE_ACTION) {
				os_memset(&event, 0, sizeof(event));
				event.rx_mgmt.freq = freq;
				event.rx_mgmt.frame = (u8 *) mgmt;
				event.rx_mgmt.frame_len = data_len;
				wpa_supplicant_event(drv->ctx, EVENT_RX_MGMT,
						     &event);
				continue;
			}

			if (stype != WLAN_FC_STYPE_PROBE_REQ)
				continue;
			if (drv->opmode == IEEE80211_M_HOSTAP ||
			    drv->opmode == IEEE80211_M_P2P_GO) {
				os_memset(&event, 0, sizeof(event));
				event.rx_probe_req.sa = mgmt->sa;
				event.rx_probe_req.ie =
					mgmt->u.probe_req.variable;
				event.rx_probe_req.ie_len =
					data_len - (IEEE80211_HDRLEN +
						    sizeof(mgmt->u.probe_req));
				wpa_supplicant_event(drv->ctx,
						     EVENT_RX_PROBE_REQ,
						     &event);
			}

			if (!drv->report_probe_req)
				continue;
			os_memset(&event, 0, sizeof(event));
			event.rx_mgmt.frame = (void *) mgmt;
			event.rx_mgmt.frame_len = data_len;
			wpa_supplicant_event(drv->ctx, EVENT_RX_MGMT, &event);
		} else if (frame_type == IEEE80211_EV_P2P_SEND_ACTION_CB) {
			wpa_printf(MSG_DEBUG, "%s: ACTION_CB frame_type=%u "
				   "len=%zu", __func__, frame_type, data_len);
			send_action_cb_event(drv, (void *) mgmt, data_len);
		} else {
			wpa_printf(MSG_DEBUG, "athr: %s unknown type %d",
				   __func__, frame_type);
			continue;
		}
	}
}


static void
driver_atheros_event_wireless_p2p_custom(struct driver_atheros_data *drv,
					 int opcode, char *buf, int len)
{
	union wpa_event_data event;

	switch (opcode) {
	case IEEE80211_EV_SCAN_DONE:
		wpa_printf(MSG_DEBUG, "athr: EV_SCAN_DONE event recvd");
		eloop_cancel_timeout(driver_atheros_scan_timeout, drv,
				     drv->ctx);
		athr_get_countrycode(drv);
		if (!drv->drv_in_scan)
			wpa_printf(MSG_DEBUG, "athr: Driver was not in scan");
		drv->drv_in_scan = 0;

		if (drv->ignore_scan_done) {
			wpa_printf(MSG_DEBUG, "athrosx: EV_SCAN_DONE "
				   "(ignore)");
			break;
		}
		wpa_supplicant_event(drv->ctx, EVENT_SCAN_RESULTS, NULL);
		break;
	case IEEE80211_EV_CHAN_START: {
		u32 ev_freq, ev_dur, req_id = 0;
		if (len < 8) {
			wpa_printf(MSG_DEBUG, "athrosx: Too short "
				   "EV_CHAN_START event");
			break;
		}
		os_memcpy(&ev_freq, buf, 4);
		os_memcpy(&ev_dur, buf + 4, 4);
		if (len >= 12)
			os_memcpy(&req_id, buf + 8, 4);
		wpa_printf(MSG_DEBUG, "athr: EV_CHAN_START (freq=%u dur=%u "
			   "req_id=%u)",
			   ev_freq, ev_dur, req_id);
		if (drv->drv_in_remain_on_chan)
			wpa_printf(MSG_DEBUG, "athr: Driver was already in "
				   "remain-on-chan");
		drv->drv_in_remain_on_chan = 1;
		if (req_id && req_id != drv->req_id) {
			wpa_printf(MSG_DEBUG, "athr: Ignore EV_CHAN_START -"
				   " mismatch in req_id (expected %u)",
				   drv->req_id);
			break;
		}
		if (ev_freq != drv->pending_set_chan_freq) {
			wpa_printf(MSG_DEBUG, "athr: Ignore EV_CHAN_START -"
				   " pending_set_chan_freq=%u",
				   drv->pending_set_chan_freq);
			break;
		}
		if (drv->pending_set_chan_freq) {
			os_memset(&event, 0, sizeof(event));
			event.remain_on_channel.freq =
				drv->pending_set_chan_freq;
			event.remain_on_channel.duration =
				drv->pending_set_chan_dur;
			wpa_supplicant_event(drv->ctx, EVENT_REMAIN_ON_CHANNEL,
					     &event);
			drv->pending_set_chan_freq = 0;
		}
		break;
	}
	case IEEE80211_EV_CHAN_END: {
		u32 ev_freq = 0, ev_reason = 0, ev_dur = 0, req_id = 0;
		if (len >= 16) {
			os_memcpy(&ev_freq, buf, 4);
			os_memcpy(&ev_reason, buf + 4, 4);
			os_memcpy(&ev_dur, buf + 8, 4);
			os_memcpy(&req_id, buf + 12, 4);
		}
		wpa_printf(MSG_DEBUG, "athr: EV_CHAN_END (freq=%u reason=%u "
			   "dur=%u req_id=%u)",
			   ev_freq, ev_reason, ev_dur, req_id);
		if (!drv->drv_in_remain_on_chan) {
			wpa_printf(MSG_DEBUG, "athr: Driver was not in "
				   "remain-on-channel");
			break;
		}
		drv->drv_in_remain_on_chan = 0;
		if (req_id && req_id != drv->req_id) {
			wpa_printf(MSG_DEBUG, "athr: Ignore EV_CHAN_END -"
				   " mismatch in req_id (expected %u)",
				   drv->req_id);
			break;
		}
		os_memset(&event, 0, sizeof(event));
		wpa_supplicant_event(drv->ctx, EVENT_CANCEL_REMAIN_ON_CHANNEL,
				     &event);
		break;
	}
	case IEEE80211_EV_RX_MGMT:
		wpa_printf(MSG_DEBUG, "WEXT: EV_RX_MGMT");
		fetch_pending_big_events(drv);
		break;
	case IEEE80211_EV_P2P_SEND_ACTION_CB:
		wpa_printf(MSG_DEBUG, "WEXT: EV_P2P_SEND_ACTION_CB");
		fetch_pending_big_events(drv);
		break;
	default:
		break;
	}
}


static void
get_sta_assoc_req_ie(struct driver_atheros_data *drv, u8 *data,
		     size_t data_len)
{
	u8 *addr;
	struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	u8 *ie;

	if (data_len < ETH_ALEN)
		return;

	addr = data;
	wpa_printf(MSG_DEBUG, "athrosx: Connection event for " MACSTR,
		   MAC2STR(addr));
	wpa_hexdump(MSG_MSGDUMP, "get_sta_assoc_req_ie data", data, data_len);
	data += ETH_ALEN;
	data_len -= ETH_ALEN;
	if (data_len < 24)
		return;

	mgmt = (struct ieee80211_mgmt *) data;
	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);
	if (stype == WLAN_FC_STYPE_ASSOC_REQ) {
		wpa_printf(MSG_DEBUG, "athrosx: Association Request");
		ie = mgmt->u.assoc_req.variable;
	} else if (stype == WLAN_FC_STYPE_REASSOC_REQ) {
		wpa_printf(MSG_DEBUG, "athrosx: Reassociation Request");
		ie = mgmt->u.reassoc_req.variable;
	} else
		return;

	if (ie > data + data_len)
		return;
	wpa_hexdump(MSG_MSGDUMP, "athrosx: (Re)AssocReq IEs",
		    ie, data + data_len - ie);
	athr_get_countrycode(drv);
	drv_event_assoc(drv->ctx, addr, ie, data + data_len - ie,
			stype == WLAN_FC_STYPE_REASSOC_REQ);
}


static void athr_ibss_event(struct driver_atheros_data *drv, const u8 *peer)
{
#ifdef CONFIG_IBSS_RSN
	union wpa_event_data data;
	struct iwreq iwr;

	if (!drv->assoc_event_sent) {
		wpa_supplicant_event(drv->ctx, EVENT_ASSOC, NULL);
		drv->assoc_event_sent = 1;
	}

	/* Check whether the event is for the current bssid */
	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if (ioctl(drv->ioctl_sock, SIOCGIWAP, &iwr) < 0)
		return;
	if (os_memcmp(iwr.u.ap_addr.sa_data, peer, ETH_ALEN) == 0)
		return; /* Do not generate peer event based on IBSS connection
			 * event */

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.ibss_rsn_start.peer, peer, ETH_ALEN);
	wpa_supplicant_event(drv->ctx, EVENT_IBSS_RSN_START, &data);
#endif /* CONFIG_IBSS_RSN */
}


static void athr_ibss_peer_lost(struct driver_atheros_data *drv,
				const u8 *peer)
{
#ifdef CONFIG_IBSS_RSN
	union wpa_event_data data;
	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.ibss_peer_lost.peer, peer, ETH_ALEN);
	wpa_supplicant_event(drv->ctx, EVENT_IBSS_PEER_LOST, &data);
#endif /* CONFIG_IBSS_RSN */
}


static void driver_atheros_event_wireless(struct driver_atheros_data *drv,
					  char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		os_memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_DEBUG, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (drv->we_version_compiled > 18 &&
			(iwe->cmd == IWEVMICHAELMICFAILURE ||
			 iwe->cmd == IWEVCUSTOM ||
			 iwe->cmd == IWEVASSOCREQIE ||
			 iwe->cmd == IWEVASSOCRESPIE ||
			 iwe->cmd == IWEVPMKIDCAND ||
			 iwe->cmd == IWEVGENIE)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			os_memcpy(dpos, pos + IW_EV_LCP_LEN,
				  sizeof(struct iw_event) - dlen);
		} else {
			os_memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case SIOCGIWAP:
			wpa_printf(MSG_DEBUG, "Wireless event: new AP: "
				   MACSTR,
				   MAC2STR((u8 *) iwe->u.ap_addr.sa_data));
			if (is_zero_ether_addr(
				    (const u8 *) iwe->u.ap_addr.sa_data) ||
			    os_memcmp(iwe->u.ap_addr.sa_data,
				      "\x44\x44\x44\x44\x44\x44", ETH_ALEN) ==
			    0) {
				os_free(drv->assoc_req_ies);
				drv->assoc_req_ies = NULL;
				os_free(drv->assoc_resp_ies);
				drv->assoc_resp_ies = NULL;
				wpa_supplicant_event(drv->ctx, EVENT_DISASSOC,
						     NULL);
			} else {
				driver_atheros_event_assoc_ies(drv);
				wpa_supplicant_event(drv->ctx, EVENT_ASSOC,
						     NULL);
			}
			break;
		case IWEVMICHAELMICFAILURE:
			if (custom + iwe->u.data.length > end) {
				wpa_printf(MSG_DEBUG, "WEXT: Invalid "
					   "IWEVMICHAELMICFAILURE length");
				return;
			}
			driver_atheros_event_wireless_michaelmicfailure(
				drv->ctx, custom, iwe->u.data.length);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end) {
				wpa_printf(MSG_DEBUG, "WEXT: Invalid "
					   "IWEVCUSTOM length");
				return;
			}
			buf = os_malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;
			os_memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';

			if (iwe->u.data.flags != 0) {
				driver_atheros_event_wireless_p2p_custom(
					drv, (int)iwe->u.data.flags,
					buf, len);
			} else {
				driver_atheros_event_wireless_custom(drv, buf);
			}
			os_free(buf);
			break;
		case SIOCGIWSCAN:
			wpa_printf(MSG_ERROR, "%s: scan result event - "
				   "SIOCGIWSCAN", __func__);
			eloop_cancel_timeout(driver_atheros_scan_timeout, drv,
					     drv->ctx);
			athr_get_countrycode(drv);
			if (!drv->drv_in_scan)
				wpa_printf(MSG_DEBUG, "athr: Driver was not "
					   "in scan");
			drv->drv_in_scan = 0;

			if (drv->ignore_scan_done) {
				wpa_printf(MSG_DEBUG, "athrosx: EV_SCAN_DONE "
					   "(ignore)");
				break;
			}
			wpa_supplicant_event(drv->ctx, EVENT_SCAN_RESULTS,
					     NULL);
			break;
		case IWEVASSOCREQIE:
			if (custom + iwe->u.data.length > end) {
				wpa_printf(MSG_DEBUG, "WEXT: Invalid "
					   "IWEVASSOCREQIE length");
				return;
			}

			driver_atheros_event_wireless_assocreqie(
				drv, custom, iwe->u.data.length);
			break;
		case IWEVASSOCRESPIE:
			if (custom + iwe->u.data.length > end) {
				wpa_printf(MSG_DEBUG, "WEXT: Invalid "
					   "IWEVASSOCRESPIE length");
				return;
			}
			driver_atheros_event_wireless_assocrespie(
				drv, custom, iwe->u.data.length);
			break;
		case IWEVPMKIDCAND:
			if (custom + iwe->u.data.length > end) {
				wpa_printf(MSG_DEBUG, "WEXT: Invalid "
					   "IWEVPMKIDCAND length");
				return;
			}
			driver_atheros_event_wireless_pmkidcand(
				drv, custom, iwe->u.data.length);
			break;
		case IWEVGENIE:
			get_sta_assoc_req_ie(drv, (u8 *) custom,
					     iwe->u.data.length);
			break;
		case IWEVEXPIRED:
			if (drv->last_assoc_mode == IEEE80211_MODE_IBSS) {
				athr_ibss_peer_lost(drv, (u8 *)
						    iwe->u.addr.sa_data);
				break;
			}
			drv_event_disassoc(drv->ctx,
					   (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			if (drv->last_assoc_mode == IEEE80211_MODE_IBSS)
				athr_ibss_event(drv,
						(u8 *) iwe->u.ap_addr.sa_data);
			break;
		default:
			wpa_printf(MSG_DEBUG, "WEXT: Unknown EVENT:%d",
				   iwe->cmd);
			break;
		}

		pos += iwe->len;
	}
}


static void driver_atheros_event_link(struct driver_atheros_data *drv,
				      char *buf, size_t len, int del)
{
	union wpa_event_data event;

	os_memset(&event, 0, sizeof(event));
	if (len > sizeof(event.interface_status.ifname))
		len = sizeof(event.interface_status.ifname) - 1;
	os_memcpy(event.interface_status.ifname, buf, len);
	event.interface_status.ievent = del ? EVENT_INTERFACE_REMOVED :
		EVENT_INTERFACE_ADDED;

	wpa_printf(MSG_DEBUG, "RTM_%sLINK, IFLA_IFNAME: Interface '%s' %s",
		   del ? "DEL" : "NEW",
		   event.interface_status.ifname,
		   del ? "removed" : "added");

	if (os_strcmp(drv->ifname, event.interface_status.ifname) == 0) {
		if (del)
			drv->if_removed = 1;
		else
			drv->if_removed = 0;
	}

	wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS, &event);
}


static int driver_atheros_own_ifname(struct driver_atheros_data *drv,
				     u8 *buf, size_t len)
{
	int attrlen, rta_len;
	struct rtattr *attr;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			if (os_strcmp(((char *) attr) + rta_len, drv->ifname)
			    == 0)
				return 1;
			else
				break;
		}
		attr = RTA_NEXT(attr, attrlen);
	}

	return 0;
}


static int driver_atheros_own_ifindex(struct driver_atheros_data *drv,
				      int ifindex, u8 *buf, size_t len)
{
	if (drv->ifindex == ifindex || drv->ifindex2 == ifindex)
		return 1;

	if (drv->if_removed && driver_atheros_own_ifname(drv, buf, len)) {
		drv->ifindex = if_nametoindex(drv->ifname);
		wpa_printf(MSG_DEBUG, "WEXT: Update ifindex for a removed "
			   "interface");
		driver_atheros_finish_drv_init(drv);
		return 1;
	}

	return 0;
}


static void driver_atheros_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi,
						u8 *buf, size_t len)
{
	struct driver_atheros_data *drv = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	if (drv->disabled)
		return;

	if (!driver_atheros_own_ifindex(drv, ifi->ifi_index, buf, len)) {
		wpa_printf(MSG_DEBUG, "Ignore event for foreign ifindex %d",
			   ifi->ifi_index);
		return;
	}

	wpa_printf(MSG_DEBUG, "RTM_NEWLINK: operstate=%d ifi_flags=0x%x "
		   "(%s%s%s%s)",
		   drv->operstate, ifi->ifi_flags,
		   (ifi->ifi_flags & IFF_UP) ? "[UP]" : "",
		   (ifi->ifi_flags & IFF_RUNNING) ? "[RUNNING]" : "",
		   (ifi->ifi_flags & IFF_LOWER_UP) ? "[LOWER_UP]" : "",
		   (ifi->ifi_flags & IFF_DORMANT) ? "[DORMANT]" : "");
	/*
	 * Some drivers send the association event before the operup event--in
	 * this case, lifting operstate in driver_atheros_set_operstate()
	 * fails. This will hit us when wpa_supplicant does not need to do
	 * IEEE 802.1X authentication
	 */
	if (drv->operstate == 1 &&
	    (ifi->ifi_flags & (IFF_LOWER_UP | IFF_DORMANT)) == IFF_LOWER_UP &&
	    !(ifi->ifi_flags & IFF_RUNNING))
		netlink_send_oper_ifla(drv->netlink, drv->ifindex,
				       -1, IF_OPER_UP);

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			driver_atheros_event_wireless(
				drv, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		} else if (attr->rta_type == IFLA_IFNAME) {
			/* check whether the link is UP or DOWN */
			if (ifi->ifi_flags & (IFF_UP | IFF_LOWER_UP))
				driver_atheros_event_link(
					drv, ((char *) attr) + rta_len,
					attr->rta_len - rta_len, 0);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void driver_atheros_event_rtm_dellink(void *ctx, struct ifinfomsg *ifi,
					     u8 *buf, size_t len)
{
	struct driver_atheros_data *drv = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	if (drv->disabled)
		return;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			driver_atheros_event_link(drv,
						   ((char *) attr) + rta_len,
						   attr->rta_len - rta_len, 1);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void
driver_atheros_l2_read(void *ctx, const u8 *src_addr, const u8 *buf,
		       size_t len)
{
	struct driver_atheros_data *drv = ctx;
	if (drv->disabled)
		return;
	wpa_printf(MSG_DEBUG, "athrosx: Received %d bytes from l2_packet "
		   "(mode=%d)", (int) len, drv->last_assoc_mode);
	if (drv->last_assoc_mode == IEEE80211_MODE_AP)
		drv_event_eapol_rx(drv->ctx, src_addr, buf, len);
}


static void athr_show_drv_version(void)
{
	FILE *f;
	char buf[256], *pos;

	f = fopen("/proc/athversion", "r");
	if (f == NULL)
		return;

	if (fgets(buf, sizeof(buf), f)) {
		pos = os_strchr(buf, '\n');
		if (pos)
			*pos = '\0';
		wpa_printf(MSG_DEBUG, "athr: Driver version %s", buf);
	}

	fclose(f);
}


/**
 * driver_atheros_init - Initialize WE driver interface
 * @ctx: context to be used when calling wpa_supplicant functions,
 * e.g., wpa_supplicant_event()
 * @ifname: interface name, e.g., wlan0
 * Returns: Pointer to private data, %NULL on failure
 */
void * driver_atheros_init(void *ctx, const char *ifname)
{
	struct driver_atheros_data *drv;
	struct netlink_config *cfg;
	int opmode;

	athr_show_drv_version();
	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->ctx = ctx;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));

	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket(PF_INET,SOCK_DGRAM)");
		goto err1;
	}

	if (linux_get_ifhwaddr(drv->ioctl_sock, drv->ifname, drv->own_addr) <
	    0)
		goto err1;

	cfg = os_zalloc(sizeof(*cfg));
	if (cfg == NULL)
		goto err1;
	cfg->ctx = drv;
	cfg->newlink_cb = driver_atheros_event_rtm_newlink;
	cfg->dellink_cb = driver_atheros_event_rtm_dellink;
	drv->netlink = netlink_init(cfg);
	if (drv->netlink == NULL) {
		os_free(cfg);
		goto err2;
	}

	drv->mlme_sock = -1;

	if (get80211param(drv, drv->ifname, IEEE80211_IOC_P2P_OPMODE, &opmode)
	    < 0) {
		wpa_printf(MSG_DEBUG, "athrosx: Failed to get interface P2P "
			   "opmode");
		/* Assume this is not P2P Device interface */
		opmode = 0;
	} else {
		wpa_printf(MSG_DEBUG, "athrosx: Interface P2P opmode: 0x%x",
			   opmode);

		drv->opmode = opmode;

		if (opmode == IEEE80211_M_STA) {
			/*
			 * Clear SSID to avoid undesired association when the
			 * interface is brought up. The driver should not
			 * really associate without being explicitly requested,
			 * but it does that for now..
			 */
			wpa_printf(MSG_DEBUG, "athr: Clear SSID");
			driver_atheros_set_ssid(drv, (u8 *) "", 0);
		}
	}

	if (driver_atheros_finish_drv_init(drv) < 0)
		goto err3;

	set80211param(drv, IEEE80211_IOC_SCAN_FLUSH, 0, 1);

	if (opmode != IEEE80211_M_P2P_DEVICE) {
		drv->l2 = l2_packet_init(drv->ifname, NULL, ETH_P_EAPOL,
					 driver_atheros_l2_read, drv, 0);
		if (drv->l2 == NULL)
			goto err3;
	} else {
		wpa_printf(MSG_DEBUG, " P2P_DEVICE Type, "
			   "WPA_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE set");
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE;
	}

	if (opmode == IEEE80211_M_P2P_DEVICE || opmode == IEEE80211_M_P2P_GO ||
	    opmode == IEEE80211_M_P2P_CLIENT)
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_CAPABLE |
			WPA_DRIVER_FLAGS_P2P_CONCURRENT;

	drv->capa.flags |= WPA_DRIVER_FLAGS_TDLS_SUPPORT;

	driver_atheros_set_auth_param(drv, IW_AUTH_WPA_ENABLED, 1);

	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to set wpa_supplicant-based "
			   "roaming", __FUNCTION__);
		goto err3;
	}

	return drv;

err3:
	netlink_deinit(drv->netlink);
err2:
	close(drv->ioctl_sock);
err1:
	os_free(drv);
	return NULL;
}


static int driver_atheros_finish_drv_init(struct driver_atheros_data *drv)
{
	if (linux_set_iface_flags(drv->ioctl_sock, drv->ifname, 1) < 0)
		return -1;

#if 0
	/*
	 * Make sure that the driver does not have any obsolete PMKID entries.
	 */
	driver_atheros_flush_pmkid(drv);
#endif

	athr_clear_bssid(drv);

	driver_atheros_get_range(drv);

	athr_get_countrycode(drv);

	drv->ifindex = if_nametoindex(drv->ifname);

#if 0
	netlink_send_oper_ifla(drv->netlink, drv->ifindex,
			       1, IF_OPER_DORMANT);
#endif

#ifdef CONFIG_TDLS
	athr_tdls_enable(drv, 1);
#endif /* CONFIG_TDLS */
	athr_no_stop_disassoc(drv, 0);

	return 0;
}


/**
 * driver_atheros_deinit - Deinitialize WE driver interface
 * @priv: Pointer to private wext data from driver_atheros_init()
 *
 * Shut down driver interface and processing of driver events. Free
 * private data buffer if one was allocated in driver_atheros_init().
 */
void driver_atheros_deinit(void *priv)
{
	struct driver_atheros_data *drv = priv;

	driver_atheros_set_auth_param(drv, IW_AUTH_WPA_ENABLED, 0);
	athr_no_stop_disassoc(drv, 0);

	if (drv->l2)
		l2_packet_deinit(drv->l2);

	eloop_cancel_timeout(driver_atheros_scan_timeout, drv, drv->ctx);

	athr_clear_bssid(drv);

	netlink_send_oper_ifla(drv->netlink, drv->ifindex, 0, IF_OPER_UP);
	netlink_deinit(drv->netlink);

	if (set80211param(drv, IEEE80211_PARAM_ROAMING, 0, 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable driver-based "
			   "roaming", __FUNCTION__);
	}

	if (drv->mlme_sock >= 0)
		eloop_unregister_read_sock(drv->mlme_sock);

	(void) linux_set_iface_flags(drv->ioctl_sock, drv->ifname, 0);

	close(drv->ioctl_sock);
	if (drv->mlme_sock >= 0)
		close(drv->mlme_sock);
	os_free(drv->assoc_req_ies);
	os_free(drv->assoc_resp_ies);
	os_free(drv);
}


/**
 * driver_atheros_scan_timeout - Scan timeout to report scan completion
 * @eloop_ctx: Unused
 * @timeout_ctx: ctx argument given to driver_atheros_init()
 *
 * This function can be used as registered timeout when starting a scan to
 * generate a scan completed event if the driver does not report this.
 */
static void driver_atheros_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct driver_atheros_data *drv = eloop_ctx;
	if (drv->disabled)
		return;
	wpa_printf(MSG_DEBUG, "Scan timeout - try to get results");
	wpa_supplicant_event(timeout_ctx, EVENT_SCAN_RESULTS, NULL);
}


/**
 * driver_atheros_scan - Request the driver to initiate scan
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @param: Scan parameters (specific SSID to scan for (ProbeReq), etc.)
 * Returns: 0 on success, -1 on failure
 */
static int driver_atheros_scan(void *priv,
			       struct wpa_driver_scan_params *params)
{
	struct driver_atheros_data *drv = priv;

	int timeout = 10;
	int ret = 0;
	size_t req_len;
	struct ieee80211_scan_req *req;
	size_t i;

	if (drv->drv_in_scan)
		wpa_printf(MSG_DEBUG, "athr: New scan request when driver "
			   "still in previous scan");
	if (drv->drv_in_remain_on_chan)
		wpa_printf(MSG_DEBUG, "athr: New scan request when driver in "
			   "remain-on-channel");
	set80211param(drv, IEEE80211_IOC_SCAN_FLUSH, 0, 1);

	drv->ignore_scan_done = 0;

	req_len = sizeof(*req) + params->extra_ies_len;
	req = os_zalloc(req_len);
	if (req == NULL)
		return -1;
	if (params->extra_ies) {
		req->ie_len = params->extra_ies_len;
		os_memcpy(req + 1, params->extra_ies, params->extra_ies_len);
	}

	req->num_ssid = params->num_ssids;
	for (i = 0; i < params->num_ssids; i++) {
		req->ssid_len[i] = params->ssids[i].ssid_len;
		os_memcpy(req->ssid[i], params->ssids[i].ssid,
			  params->ssids[i].ssid_len);
	}

	if (params->freqs) {
		i = 0;
		while (params->freqs[i]) {
			req->freq[i] = params->freqs[i];
			wpa_printf(MSG_DEBUG, "athr: Scan freq %d",
				   req->freq[i]);
			i++;
			if (i == MAX_SCANREQ_FREQ) {
				wpa_printf(MSG_DEBUG, "athr: Too many scan "
					   "frequencies - fall back to "
					   "scanning all channels");
				i = 0;
				break;
			}
		}
		req->num_freq = i;
		drv->prev_scan_type = ATHR_PARTIAL_SCAN;
	} else {
		drv->prev_scan_type = ATHR_FULL_SCAN;
	}

	if (set80211big(drv, IEEE80211_IOC_SCAN_REQ, req, req_len) < 0) {
#ifdef CONFIG_TDLS
		struct iwreq iwr;
		wpa_printf(MSG_DEBUG, "athr: IOC_SCAN_REQ did not work - try "
			   "to use SIOCSIWSCAN");
		os_memset(&iwr, 0, sizeof(iwr));
		os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
		if (ioctl(drv->ioctl_sock, SIOCSIWSCAN, &iwr) < 0) {
			perror("ioctl[SIOCSIWSCAN]");
			ret = -1;
		}
#else /* CONFIG_TDLS */
		ret = -1;
#endif /* CONFIG_TDLS */
	}

	os_free(req);

	timeout = 45;
	wpa_printf(MSG_DEBUG, "Scan requested (ret=%d) - scan timeout %d "
		   "seconds", ret, timeout);
	eloop_cancel_timeout(driver_atheros_scan_timeout, drv, drv->ctx);
	eloop_register_timeout(timeout, 0, driver_atheros_scan_timeout, drv,
			       drv->ctx);

	if (ret == 0)
		drv->drv_in_scan = 1;
	return ret;
}


static u8 * driver_atheros_giwscan(struct driver_atheros_data *drv,
					size_t *len)
{
	struct iwreq iwr;
	u8 *res_buf;
	size_t res_buf_len;

	res_buf_len = 65535;
	for (;;) {
		res_buf = os_malloc(res_buf_len);
		if (res_buf == NULL)
			return NULL;
		os_memset(&iwr, 0, sizeof(iwr));
		os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
		iwr.u.data.pointer = res_buf;
		iwr.u.data.length = res_buf_len;

		if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_SCAN_RESULTS, &iwr)
		    == 0)
			break;

		if (errno == E2BIG && res_buf_len < 65535) {
			os_free(res_buf);
			res_buf = NULL;
			res_buf_len *= 2;
			if (res_buf_len > 65535)
				res_buf_len = 65535; /* 16-bit length field */
			wpa_printf(MSG_DEBUG, "Scan results did not fit - "
				   "trying larger buffer (%lu bytes)",
				   (unsigned long) res_buf_len);
		} else {
			wpa_printf(MSG_ERROR, "athr: ioctl[SCAN_RESULTS] "
				   "failed: %s", strerror(errno));
			os_free(res_buf);
			return NULL;
		}
	}

	if (iwr.u.data.length > res_buf_len) {
		os_free(res_buf);
		return NULL;
	}
	*len = iwr.u.data.length;
	WPA_MEM_DEFINED(res_buf, *len);

	return res_buf;
}


#ifdef IEEE80211_IOC_P2P_FIND_BEST_CHANNEL
/**
 * Get the best channel after a scan; to be invoked after a scan
 * @drv: Pointer to private data from driver_atheros_init()
 * Returns: zero on success, -1 on failure
 */
static int driver_atheros_get_best_channel(struct driver_atheros_data *drv)
{
	int best_24, best_5, best_overall;
	int buf[3];
	int retv;
	int changed = 0;

	os_memset(buf, 0, sizeof(buf));

	retv = set80211big(drv, IEEE80211_IOC_P2P_FIND_BEST_CHANNEL, buf,
			   sizeof(buf));
	if (retv < 0) {
		wpa_printf(MSG_DEBUG, "%s set80211big returns %d",
			   __func__, retv);
		return -1;
	}

	best_5 = buf[0];
	best_24 = buf[1];
	best_overall = buf[2];

	wpa_printf(MSG_DEBUG, "athr: Best channel: 5 GHz: %d MHz  "
		   "2.4 GHz: %d MHz  overall: %d MHz",
		   best_5, best_24, best_overall);

	if (best_5 != drv->best_5_freq) {
		wpa_printf(MSG_DEBUG, "athr: Best 5 GHz channel changed from "
			   "%d to %d MHz", drv->best_5_freq, best_5);
		drv->best_5_freq = best_5;
		changed = 1;
	}

	if (best_24 != drv->best_24_freq) {
		wpa_printf(MSG_DEBUG, "athr: Best 2.4 GHz channel changed "
			   "from %d to %d MHz", drv->best_24_freq, best_24);
		drv->best_24_freq = best_24;
		changed = 1;
	}

	if (best_overall != drv->best_overall_freq) {
		wpa_printf(MSG_DEBUG, "athr: Best overall channel changed "
			   "from %d to %d MHz", drv->best_overall_freq,
			   best_overall);
		drv->best_overall_freq = best_overall;
		changed = 1;
	}

	if (changed) {
		union wpa_event_data data;
		data.best_chan.freq_24 = drv->best_24_freq;
		data.best_chan.freq_5 = drv->best_5_freq;
		data.best_chan.freq_overall = drv->best_overall_freq;
		wpa_supplicant_event(drv->ctx, EVENT_BEST_CHANNEL, &data);
	}

	return 0;
}
#endif /* IEEE80211_IOC_P2P_FIND_BEST_CHANNEL */


/**
 * driver_atheros_get_scan_results - Fetch the latest scan results
 * @priv: Pointer to private wext data from driver_atheros_init()
 * Returns: Scan results on success, -1 on failure
 */
static struct wpa_scan_results * driver_atheros_get_scan_results(void *priv)
{
	struct driver_atheros_data *drv = priv;
	size_t len;
	u8 *res_buf;
	char *pos, *end;
	struct wpa_scan_results *res;

	res_buf = driver_atheros_giwscan(drv, &len);
	if (res_buf == NULL)
		return NULL;

	wpa_printf(MSG_DEBUG, "%s: %d bytes of scan available",
		   __func__, (int) len);

	res = os_zalloc(sizeof(*res));
	if (res == NULL) {
		os_free(res_buf);
		return NULL;
	}

	pos = (char *) res_buf;
	end = (char *) res_buf + len;

	while (pos + sizeof(struct ieee80211req_scan_result) <= end) {
		struct wpa_scan_res *r;
		struct ieee80211req_scan_result *sr;
		struct wpa_scan_res **tmp;
		char * next;

		sr = (struct ieee80211req_scan_result *) pos;
		next = pos + sr->isr_len;
		if (next > end)
			break;
		pos += sizeof(*sr);
		if (pos + sr->isr_ssid_len + sr->isr_ie_len > end)
			break;
		wpa_printf(MSG_DEBUG, "%s: sr=%p next=%p sr->isr_ssid_len=%d "
			   "sr->isr_ie_len=%d",
			   __func__, sr, next, sr->isr_ssid_len,
			   sr->isr_ie_len);

		r = os_zalloc(sizeof(*r) + sr->isr_ie_len);
		if (r == NULL)
			break;

		os_memcpy(r->bssid, sr->isr_bssid, ETH_ALEN);
		r->freq = sr->isr_freq;
		if (r->freq > 0 && r->freq <= 13)
			r->freq = 2407 + r->freq * 5;
		else if (r->freq == 14)
			r->freq = 2484;
		else if (r->freq > 14 && r->freq < 200)
			r->freq = 5000 + r->freq * 5;
		r->beacon_int = sr->isr_intval;
		r->caps = sr->isr_capinfo;
		r->noise = sr->isr_noise;
		r->level = sr->isr_rssi;
		pos += sr->isr_ssid_len;
		r->ie_len = sr->isr_ie_len;
		os_memcpy(r + 1, pos, sr->isr_ie_len);
		tmp = os_realloc(res->res, (res->num + 1) *
				 sizeof(struct wpa_scan_res *));
		if (tmp == NULL) {
			os_free(r);
			break;
		}
		tmp[res->num++] = r;
		res->res = tmp;

		pos = next;
	}


	os_free(res_buf);

	wpa_printf(MSG_DEBUG, "Received %lu bytes of scan results (%lu BSSes)",
		   (unsigned long) len, (unsigned long) res->num);

#ifdef IEEE80211_IOC_P2P_FIND_BEST_CHANNEL
	if (drv->prev_scan_type == ATHR_FULL_SCAN)
		driver_atheros_get_best_channel(drv);
#endif /* IEEE80211_IOC_P2P_FIND_BEST_CHANNEL */

	return res;
}


static int driver_atheros_get_range(void *priv)
{
	struct driver_atheros_data *drv = priv;
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = os_zalloc(buflen);
	if (range == NULL)
		return -1;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		os_free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->has_capability = 1;
		drv->we_version_compiled = range->we_version_compiled;
		if (range->enc_capa & IW_ENC_CAPA_WPA) {
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK;
		}
		if (range->enc_capa & IW_ENC_CAPA_WPA2) {
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
				WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
		}
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_WEP40 |
			WPA_DRIVER_CAPA_ENC_WEP104;
		if (range->enc_capa & IW_ENC_CAPA_CIPHER_TKIP)
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_TKIP;
		if (range->enc_capa & IW_ENC_CAPA_CIPHER_CCMP)
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_CCMP;
		if (range->enc_capa & IW_ENC_CAPA_4WAY_HANDSHAKE)
			drv->capa.flags |= WPA_DRIVER_FLAGS_4WAY_HANDSHAKE;
		drv->capa.auth = WPA_DRIVER_AUTH_OPEN |
			WPA_DRIVER_AUTH_SHARED |
			WPA_DRIVER_AUTH_LEAP;
		drv->capa.max_scan_ssids = MAX_SCANREQ_SSID;

		drv->capa.max_remain_on_chan = 1000;

		wpa_printf(MSG_DEBUG, "  capabilities: key_mgmt 0x%x enc 0x%x "
			   "flags 0x%x",
			   drv->capa.key_mgmt, drv->capa.enc, drv->capa.flags);
	} else {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: too old (short) data - "
			   "assuming WPA is not supported");
	}

	os_free(range);
	return 0;
}


static int
driver_atheros_del_key(struct driver_atheros_data *drv, int key_idx,
		       const u8 *addr)
{
	struct ieee80211req_del_key wk;

	wpa_printf(MSG_DEBUG, "%s: keyidx=%d", __FUNCTION__, key_idx);
	os_memset(&wk, 0, sizeof(wk));
	wk.idk_keyix = key_idx;
	if (addr != NULL)
		os_memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);

	return set80211priv(drv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk), 1);
}


int
driver_atheros_set_key(const char *ifname, void *priv, enum wpa_alg alg,
		       const u8 *addr, int key_idx, int set_tx,
		       const u8 *seq, size_t seq_len,
		       const u8 *key, size_t key_len)
{
	struct driver_atheros_data *drv = priv;
	struct ieee80211req_key k;
	char *alg_name;
	u_int8_t cipher;

	if (alg == WPA_ALG_NONE)
		return driver_atheros_del_key(drv, key_idx, addr);

	switch (alg) {
	case WPA_ALG_WEP:
		alg_name = "WEP";
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
#ifdef ATH_GCM_SUPPORT
	case WPA_ALG_CCMP_256:
		alg_name = "CCMP 256";
		cipher = IEEE80211_CIPHER_AES_CCM_256;
		break;
	case WPA_ALG_GCMP:
		alg_name = "GCMP";
		cipher = IEEE80211_CIPHER_AES_GCM;
		break;
	case WPA_ALG_GCMP_256:
		alg_name = "GCMP 256";
		cipher = IEEE80211_CIPHER_AES_GCM_256;
		break;
#endif /* ATH_GCM_SUPPORT */
#ifdef CONFIG_IEEE80211W
        case WPA_ALG_IGTK:
                alg_name = "IGTK CMAC 128";
                cipher = IEEE80211_CIPHER_AES_CMAC;
                break;
#ifdef ATH_GCM_SUPPORT
        case WPA_ALG_BIP_CMAC_256:
                alg_name = "IGTK CMAC 256";
                cipher = IEEE80211_CIPHER_AES_CMAC_256;
                break;
        case WPA_ALG_BIP_GMAC_128:
                alg_name = "IGTK GMAC 128";
                cipher = IEEE80211_CIPHER_AES_GMAC;
                break;
        case WPA_ALG_BIP_GMAC_256:
                alg_name = "IGTK GMAC 256";
                cipher = IEEE80211_CIPHER_AES_GMAC_256;
                break;
#endif /* ATH_GCM_SUPPORT */
#endif /* CONFIG_IEEE80211W */
	default:
		wpa_printf(MSG_DEBUG, "%s: unknown/unsupported algorithm %d",
			   __FUNCTION__, alg);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: ifname=%s, alg=%s key_idx=%d set_tx=%d "
		   "seq_len=%lu key_len=%lu",
		   __FUNCTION__, drv->ifname, alg_name, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	if (seq_len > sizeof(u_int64_t)) {
		wpa_printf(MSG_DEBUG, "%s: seq_len %lu too big",
			   __FUNCTION__, (unsigned long) seq_len);
		return -2;
	}
	if (key_len > sizeof(k.ik_keydata)) {
		wpa_printf(MSG_DEBUG, "%s: key length %lu too big",
			   __FUNCTION__, (unsigned long) key_len);
		return -3;
	}

	os_memset(&k, 0, sizeof(k));
	k.ik_flags = IEEE80211_KEY_RECV;
	if (set_tx)
		k.ik_flags |= IEEE80211_KEY_XMIT;

	k.ik_type = cipher;

#ifndef IEEE80211_KEY_GROUP
#define IEEE80211_KEY_GROUP 0x04
#endif
	if (addr == NULL)
		wpa_printf(MSG_DEBUG, "athr: addr is NULL");
	else
		wpa_printf(MSG_DEBUG, "athr: addr = " MACSTR, MAC2STR(addr));
	if (addr && !is_broadcast_ether_addr(addr)) {
		if (alg != WPA_ALG_WEP && key_idx && !set_tx) {
			wpa_printf(MSG_DEBUG, "athr: RX GTK: set "
				   "IEEE80211_PARAM_MCASTCIPHER=%d", alg);
			driver_atheros_set_cipher(drv,
						  IEEE80211_PARAM_MCASTCIPHER,
						  driver_atheros_alg_to_cipher_suite(alg,key_len));
			os_memcpy(k.ik_macaddr, addr, IEEE80211_ADDR_LEN);
			wpa_printf(MSG_DEBUG, "athr: addr = " MACSTR,
				   MAC2STR(k.ik_macaddr));
			k.ik_flags |= IEEE80211_KEY_GROUP;
			k.ik_keyix = key_idx;
#ifdef CONFIG_TDLS
		} else if (key_idx < 0 && addr) {
			wpa_printf(MSG_DEBUG, "athr: Set TDLS key for " MACSTR,
				   MAC2STR(addr));
			wpa_printf(MSG_DEBUG,
				   " set IEEE80211_PARAM_UCASTCIPHER=%d", alg);
			driver_atheros_set_cipher(drv,
						  IEEE80211_PARAM_UCASTCIPHER,
						  driver_atheros_alg_to_cipher_suite(alg, key_len));
			os_memcpy(k.ik_macaddr, addr, IEEE80211_ADDR_LEN);
			k.ik_keyix = key_idx;
#endif /* CONFIG_TDLS */
		} else {
			wpa_printf(MSG_DEBUG,
				   " set IEEE80211_PARAM_UCASTCIPHER=%d", alg);
			driver_atheros_set_cipher(drv,
						  IEEE80211_PARAM_UCASTCIPHER,
						  driver_atheros_alg_to_cipher_suite(alg, key_len));
			os_memcpy(k.ik_macaddr, addr, IEEE80211_ADDR_LEN);
			wpa_printf(MSG_DEBUG, "addr = " MACSTR,
				   MAC2STR(k.ik_macaddr));
			k.ik_keyix = key_idx == 0 ? IEEE80211_KEYIX_NONE :
				key_idx;
		}
	} else {
		wpa_printf(MSG_DEBUG, "athr: TX GTK: set "
			   "IEEE80211_PARAM_MCASTCIPHER=%d", alg);
		driver_atheros_set_cipher(drv, IEEE80211_PARAM_MCASTCIPHER,
					  driver_atheros_alg_to_cipher_suite(alg, key_len));
		os_memset(k.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wpa_printf(MSG_DEBUG, "athr: addr = " MACSTR,
			   MAC2STR(k.ik_macaddr));
		k.ik_flags |= IEEE80211_KEY_GROUP;
		k.ik_keyix = key_idx;
	}

	if (k.ik_keyix != IEEE80211_KEYIX_NONE && set_tx)
		k.ik_flags |= IEEE80211_KEY_DEFAULT;

	k.ik_keylen = key_len;
	if (seq) {
#ifdef WORDS_BIGENDIAN
		/*
		 * k.ik_keyrsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
		u8 *keyrsc = (u8 *) &k.ik_keyrsc;
		for (i = 0; i < seq_len; i++)
			keyrsc[WPA_KEY_RSC_LEN - i - 1] = seq[i];
#else /* WORDS_BIGENDIAN */
		os_memcpy(&k.ik_keyrsc, seq, seq_len);
#endif /* WORDS_BIGENDIAN */
	}
	os_memcpy(k.ik_keydata, key, key_len);

	return set80211priv(drv, IEEE80211_IOCTL_SETKEY, &k, sizeof(k), 1);
}


static int
driver_atheros_set_countermeasures(void *priv, int enabled)
{
	struct driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_COUNTERMEASURES, enabled, 1);
}


#if 0
static int driver_atheros_mlme(struct driver_atheros_data *drv,
				const u8 *addr, int cmd, int reason_code)
{
	struct iwreq iwr;
	struct iw_mlme mlme;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	os_memset(&mlme, 0, sizeof(mlme));
	mlme.cmd = cmd;
	mlme.reason_code = reason_code;
	mlme.addr.sa_family = ARPHRD_ETHER;
	os_memcpy(mlme.addr.sa_data, addr, ETH_ALEN);
	iwr.u.data.pointer = (caddr_t) &mlme;
	iwr.u.data.length = sizeof(mlme);

	if (ioctl(drv->ioctl_sock, SIOCSIWMLME, &iwr) < 0) {
		perror("ioctl[SIOCSIWMLME]");
		ret = -1;
	}

	return ret;
}
#endif


static void athr_clear_bssid(struct driver_atheros_data *drv)
{
	struct iwreq iwr;
	const u8 null_bssid[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };

	/*
	 * Avoid trigger that the driver could consider as a request for a new
	 * IBSS to be formed.
	 */
	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if (ioctl(drv->ioctl_sock, SIOCGIWMODE, &iwr) < 0) {
		perror("ioctl[SIOCGIWMODE]");
		iwr.u.mode = IW_MODE_INFRA;
	}

	if (iwr.u.mode == IW_MODE_INFRA) {
		/* Clear the BSSID selection */
		if (driver_atheros_set_bssid(drv, null_bssid) < 0)
			wpa_printf(MSG_DEBUG, "athr: Failed to clear BSSID");
	}
}


static int driver_atheros_deauthenticate(void *priv, const u8 *addr,
					 int reason_code)
{
	struct driver_atheros_data *drv = priv;
	int ret;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	ret = driver_atheros_set_mlme(drv, IEEE80211_MLME_DEAUTH, NULL, NULL);
	athr_clear_bssid(drv);
	return ret;
}

#if 0
static int driver_atheros_disassociate(void *priv, const u8 *addr,
					int reason_code)
{
	struct driver_atheros_data *drv = priv;
	int ret;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	ret = driver_atheros_set_mlme(drv, IEEE80211_MLME_DISASSOC, NULL,
				      NULL);
	athr_clear_bssid(drv);
	return ret;
}
#endif


int driver_atheros_cipher2wext(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE:
		return IW_AUTH_CIPHER_NONE;
	case WPA_CIPHER_WEP40:
		return IW_AUTH_CIPHER_WEP40;
	case WPA_CIPHER_TKIP:
		return IW_AUTH_CIPHER_TKIP;
	case WPA_CIPHER_CCMP:
		return IW_AUTH_CIPHER_CCMP;
	case WPA_CIPHER_WEP104:
		return IW_AUTH_CIPHER_WEP104;
	default:
		return 0;
	}
}


int driver_atheros_keymgmt2wext(int keymgmt)
{
	switch (keymgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA:
		return IW_AUTH_KEY_MGMT_802_1X;
	case WPA_KEY_MGMT_PSK:
		return IW_AUTH_KEY_MGMT_PSK;
	default:
		return 0;
	}
}


static int
set80211priv(struct driver_atheros_data *drv, int op, void *data, int len,
	     int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	if (len <= IFNAMSIZ) {
		/*
		 * Argument data fits inline; put it there.
		 */
		if (op == IEEE80211_IOCTL_SET_APPIEBUF) {
			wpa_printf(MSG_DEBUG, "%s: APPIEBUF", __func__);
			iwr.u.data.pointer = data;
			iwr.u.data.length = len;
		} else
		os_memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->ioctl_sock, op, &iwr) < 0) {
		if (show_err) {
			wpa_printf(MSG_DEBUG, "%s: op=%x (%s) len=%d "
				   "name=%s failed: %d (%s)",
				   __func__, op,
				   athr_get_ioctl_name(op),
				   iwr.u.data.length, iwr.u.name,
				   errno, strerror(errno));
		}
		return -1;
	}
	return 0;
}

static int
driver_atheros_set_wpa_ie(struct driver_atheros_data *drv,
			  const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* NB: SETOPTIE is not fixed-size so must not be inlined */
	iwr.u.data.pointer = (void *) wpa_ie;
	iwr.u.data.length = wpa_ie_len;
	wpa_printf(MSG_DEBUG, "WPA IE: ifname:%s len=%d",
		   drv->ifname, (int)wpa_ie_len);

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_SETOPTIE, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "athr: ioctl[IEEE80211_IOCTL_SETOPTIE] "
			   "failed: %s", strerror(errno));
		return -1;
	}
	return 0;
}


static int
call80211ioctl(struct driver_atheros_data *drv, int op, struct iwreq *iwr)
{
	wpa_printf(MSG_DEBUG, "%s: %d op=0x%x (%s) subop=0x%x=%d "
		   "value=0x%x,0x%x",
		   __func__, drv->ioctl_sock, op,
		   athr_get_ioctl_name(op), iwr->u.mode, iwr->u.mode,
		   iwr->u.data.length, iwr->u.data.flags);
	if (ioctl(drv->ioctl_sock, op, iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: op=0x%x (%s) subop=0x%x=%d "
			   "value=0x%x,0x%x failed: %d (%s)",
			   __func__, op, athr_get_ioctl_name(op), iwr->u.mode,
			   iwr->u.mode, iwr->u.data.length,
			   iwr->u.data.flags, errno, strerror(errno));
		return -1;
	}
	return 0;
}


/* Use only to set a big param, get will not work. */
static int
set80211big(struct driver_atheros_data *drv, int op, const void *data, int len)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	iwr.u.data.pointer = (void *) data;
	iwr.u.data.length = len;
	iwr.u.data.flags = op;
	wpa_printf(MSG_DEBUG, "%s: op=0x%x=%d (%s) len=0x%x",
		   __func__, op, op, athr_get_param_name(op), len);

	return call80211ioctl(drv, IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr);
}


static int
driver_atheros_set_mlme(struct driver_atheros_data *drv, int op,
			const u8 *bssid, const u8 *ssid)
{
	struct ieee80211req_mlme mlme;
	int ret = 0;

	os_memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = op;
	if (bssid) {
		os_memcpy(mlme.im_macaddr, bssid, IEEE80211_ADDR_LEN);
		wpa_printf(MSG_DEBUG, "Associating.. AP BSSID=" MACSTR ", "
			   "ssid=%s, op=%d",
			   MAC2STR(bssid), ssid, op);
	}

	wpa_printf(MSG_DEBUG, " %s: OP mode = %d", __func__, op);

	if (set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme,
			 sizeof(mlme), 1) < 0) {
		wpa_printf(MSG_DEBUG, "%s: SETMLME[ASSOC] failed", __func__);
		ret = -1;
	}

	return ret;
}


static int driver_atheros_set_cipher(struct driver_atheros_data *drv, int type,
				     unsigned int suite)
{
	int cipher;

	wpa_printf(MSG_DEBUG, "athr: Set cipher type=%d suite=%d",
		   type, suite);

	switch (suite) {
	case WPA_CIPHER_CCMP:
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
#ifdef ATH_GCM_SUPPORT
        case WPA_CIPHER_CCMP_256:
                cipher = IEEE80211_CIPHER_AES_CCM_256;
                break;
        case WPA_CIPHER_GCMP_256:
                cipher = IEEE80211_CIPHER_AES_GCM_256;
                break;
        case WPA_CIPHER_GCMP:
                cipher = IEEE80211_CIPHER_AES_GCM;
                break;
#endif /* ATH_GCM_SUPPORT */
	case WPA_CIPHER_TKIP:
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_CIPHER_WEP104:
	case WPA_CIPHER_WEP40:
		if (type == IEEE80211_IOC_MCASTCIPHER)
			cipher = IEEE80211_CIPHER_WEP;
		else
			return -1;
		break;
	case WPA_CIPHER_NONE:
		cipher = IEEE80211_CIPHER_NONE;
		break;
	default:
		return -1;
	}

	wpa_printf(MSG_DEBUG, "athr: cipher=%d", cipher);

	return set80211param(drv, type, cipher, 1);
}

static int driver_atheros_set_auth_alg(struct driver_atheros_data *drv,
				       unsigned int key_mgmt_suite,
				       int auth_alg)
{
	int authmode;
	if (key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X)
		authmode = IEEE80211_AUTH_8021X;
	else if (key_mgmt_suite == WPA_KEY_MGMT_PSK)
		authmode = IEEE80211_AUTH_WPA;
	else if ((auth_alg & WPA_AUTH_ALG_OPEN) &&
		 (auth_alg & WPA_AUTH_ALG_SHARED))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_alg & WPA_AUTH_ALG_SHARED)
		authmode = IEEE80211_AUTH_SHARED;
	else
		authmode = IEEE80211_AUTH_OPEN;

	return set80211param(drv, IEEE80211_PARAM_AUTHMODE, authmode, 1);
}


static int
get80211param(struct driver_atheros_data *drv, char *ifname, int op, int *data)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);

	iwr.u.mode = op;

	if (call80211ioctl(drv, IEEE80211_IOCTL_GETPARAM, &iwr) < 0)
		return -1;
	*data = iwr.u.mode;
	return 0;
}


static int
set80211param(struct driver_atheros_data *drv, int op, int arg, int show_err)
{
	return set80211param_ifname(drv, drv->ifname, op, arg, show_err);
}

/*
 * Function to call a sub-ioctl for setparam.
 * data + 0 = mode = subioctl number
 * data +4 = int parameter.
 */
static int
set80211param_ifname(struct driver_atheros_data *drv, const char *ifname,
		     int op, int arg, int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.mode = op;
	os_memcpy(iwr.u.name + sizeof(__u32), &arg, sizeof(arg));

	wpa_printf(MSG_DEBUG, "%s: ifname=%s subioctl=%d (%s) arg=%d",
		   __func__, ifname, op, athr_get_param_name(op), arg);
	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		if (show_err)
			wpa_printf(MSG_ERROR, "athr: "
				   "ioctl[IEEE80211_IOCTL_SETPARAM] failed: "
				   "%s", strerror(errno));
		return -1;
	}
	return 0;
}


static int
driver_atheros_associate(void *priv,
			 struct wpa_driver_associate_params *params)
{
	struct driver_atheros_data *drv = priv;
	int ret = 0, privacy = 1;

	wpa_printf(MSG_DEBUG, "athr: Associate: mode=%d p2p=%d freq=%d",
		   params->mode, params->p2p, params->freq);

	drv->last_assoc_mode = params->mode;
	drv->assoc_event_sent = 0;

	if (params->mode == IEEE80211_MODE_AP)
		return driver_atheros_start_ap(drv, params);

	wpa_hexdump(MSG_DEBUG, "athr: Association IEs",
		    params->wpa_ie, params->wpa_ie_len);

	if (params->p2p) {
		int opmode;
		if (get80211param(drv, drv->ifname, IEEE80211_IOC_P2P_OPMODE,
				  &opmode) < 0)
			opmode = -1;
		if (opmode == IEEE80211_M_P2P_GO &&
		    set80211param(drv, IEEE80211_IOC_P2P_OPMODE,
				  IEEE80211_M_P2P_CLIENT, 1) < 0) {
			return -1;
		}
	}

	if (params->uapsd >= 0) {
		wpa_printf(MSG_DEBUG, "athr: Set UAPSD for station mode: 0x%x",
			   params->uapsd);
		if (set80211param(drv, IEEE80211_PARAM_UAPSDINFO,
				  params->uapsd, 1) < 0)
			return -1;
	}

	if (driver_atheros_set_ssid(drv, params->ssid, params->ssid_len) < 0)
		ret = -1;

	if (params->pairwise_suite == WPA_CIPHER_NONE &&
	    params->group_suite == WPA_CIPHER_NONE &&
	    params->key_mgmt_suite == WPA_KEY_MGMT_NONE)
		privacy = 0;

	if (params->key_mgmt_suite == WPA_KEY_MGMT_WPS) {
		wpa_printf(MSG_DEBUG, "athr: Set privacy off during WPS "
			   "provisioning");
		privacy = 0;
	}

	set80211param(drv, IEEE80211_PARAM_DROPUNENCRYPTED,
		      params->drop_unencrypted, 1);
	if (privacy) {
		if (params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X ||
		    params->key_mgmt_suite == WPA_KEY_MGMT_PSK) {
			wpa_printf(MSG_DEBUG, " *** KEY MGMT is 2");
		    if (params->wpa_ie_len &&
		        set80211param(drv, IEEE80211_PARAM_WPA,
		                      params->wpa_ie[0] == WLAN_EID_RSN ?
				      2 : 1, 1) < 0)
				ret = -1;
		} else if (set80211param(drv, IEEE80211_PARAM_WPA, 0, 1) < 0) {
			wpa_printf(MSG_DEBUG, " KEY MGMT is 0");
			ret = -1;
		}
	}

	if (driver_atheros_set_auth_alg(drv, params->key_mgmt_suite,
					params->auth_alg) < 0) {
		wpa_printf(MSG_DEBUG, "set_auth_alg failed suite=%d, alg=%d",
			   params->key_mgmt_suite, params->auth_alg);
		ret = -1;
	}

	if (params->wpa_ie_len) {
		if (params->key_mgmt_suite != WPA_KEY_MGMT_NONE &&
		    driver_atheros_set_cipher(drv, IEEE80211_IOC_UCASTCIPHER,
					      params->pairwise_suite) < 0)
			ret = -1;
		if (params->key_mgmt_suite != WPA_KEY_MGMT_NONE &&
		    driver_atheros_set_cipher(drv, IEEE80211_IOC_MCASTCIPHER,
					      params->group_suite) < 0)
			ret = -1;

		if (driver_atheros_set_wpa_ie(drv, params->wpa_ie,
					      params->wpa_ie_len) < 0)
			ret = -1;
	} else {
		set80211param(drv, IEEE80211_PARAM_CLR_APPOPT_IE, 1, 1);
	}

	/*
	 * Privacy flag seem to be set at all times for station. Otherwise
	 * it does not connect to GO which always has privacy flag set.
	 */
	if (set80211param(drv, IEEE80211_PARAM_PRIVACY, privacy, 1) < 0)
		ret = -1;

	if (params->key_mgmt_suite == WPA_KEY_MGMT_WPS)
		set80211param(drv, IEEE80211_IOC_WPS_MODE, 1, 1);
	else
		set80211param(drv, IEEE80211_IOC_WPS_MODE, 0, 1);

	if (params->bssid == NULL) {
		/* ap_scan=2 mode - driver takes care of AP selection and
		 * roaming */
		/* FIX: this does not seem to work; would probably need to
		 * change something in the driver */
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 1, 1) < 0)
			ret = -1;
	} else {
		if (set80211param(drv, IEEE80211_PARAM_ROAMING, 2, 1) < 0)
			ret = -1;
	}

	if (driver_atheros_set_mlme(drv, IEEE80211_MLME_ASSOC, params->bssid,
				    params->ssid) < 0)
		ret = -1;

	return ret;
}


#if 0

static int driver_atheros_pmksa(struct driver_atheros_data *drv,
				 u32 cmd, const u8 *bssid, const u8 *pmkid)
{
	struct iwreq iwr;
	struct iw_pmksa pmksa;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	os_memset(&pmksa, 0, sizeof(pmksa));
	pmksa.cmd = cmd;
	pmksa.bssid.sa_family = ARPHRD_ETHER;
	if (bssid)
		os_memcpy(pmksa.bssid.sa_data, bssid, ETH_ALEN);
	if (pmkid)
		os_memcpy(pmksa.pmkid, pmkid, IW_PMKID_LEN);
	iwr.u.data.pointer = (caddr_t) &pmksa;
	iwr.u.data.length = sizeof(pmksa);

	if (ioctl(drv->ioctl_sock, SIOCSIWPMKSA, &iwr) < 0) {
		if (errno != EOPNOTSUPP)
			perror("ioctl[SIOCSIWPMKSA]");
		ret = -1;
	}

	return ret;
}


static int driver_atheros_add_pmkid(void *priv, const u8 *bssid,
				    const u8 *pmkid)
{
	struct driver_atheros_data *drv = priv;
	return driver_atheros_pmksa(drv, IW_PMKSA_ADD, bssid, pmkid);
}


static int driver_atheros_remove_pmkid(void *priv, const u8 *bssid,
				       const u8 *pmkid)
{
	struct driver_atheros_data *drv = priv;
	return driver_atheros_pmksa(drv, IW_PMKSA_REMOVE, bssid, pmkid);
}


static int driver_atheros_flush_pmkid(void *priv)
{
	struct driver_atheros_data *drv = priv;
	return driver_atheros_pmksa(drv, IW_PMKSA_FLUSH, NULL, NULL);
}

#endif


int driver_atheros_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	struct driver_atheros_data *drv = priv;

	drv->capa.flags |= WPA_DRIVER_FLAGS_AP;
	drv->capa.flags |= WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC_DONE;
	os_memcpy(capa, &drv->capa, sizeof(*capa));
	return 0;
}


static struct hostapd_hw_modes * athr_add_11b(struct hostapd_hw_modes *modes,
					      u16 *num_modes)
{
	u16 m;
	struct hostapd_hw_modes *mode11g = NULL, *nmodes, *mode;
	int i, mode11g_idx = -1;

	/* If only 802.11g mode is included, use it to construct matching
	 * 802.11b mode data. */

	for (m = 0; m < *num_modes; m++) {
		if (modes[m].mode == HOSTAPD_MODE_IEEE80211B)
			return modes; /* 802.11b already included */
		if (modes[m].mode == HOSTAPD_MODE_IEEE80211G)
			mode11g_idx = m;
	}

	if (mode11g_idx < 0)
		return modes; /* 2.4 GHz band not supported at all */

	nmodes = os_realloc(modes, (*num_modes + 1) * sizeof(*nmodes));
	if (nmodes == NULL)
		return modes; /* Could not add 802.11b mode */

	mode = &nmodes[*num_modes];
	os_memset(mode, 0, sizeof(*mode));
	(*num_modes)++;
	modes = nmodes;

	mode->mode = HOSTAPD_MODE_IEEE80211B;

	mode11g = &modes[mode11g_idx];
	mode->num_channels = mode11g->num_channels;
	mode->channels = os_malloc(mode11g->num_channels *
				   sizeof(struct hostapd_channel_data));
	if (mode->channels == NULL) {
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}
	os_memcpy(mode->channels, mode11g->channels,
		  mode11g->num_channels * sizeof(struct hostapd_channel_data));

	mode->num_rates = 0;
	mode->rates = os_malloc(4 * sizeof(int));
	if (mode->rates == NULL) {
		os_free(mode->channels);
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}

	for (i = 0; i < mode11g->num_rates; i++) {
		if (mode11g->rates[i] != 10 && mode11g->rates[i] != 20 &&
		    mode11g->rates[i] != 55 && mode11g->rates[i] != 110)
			continue;
		mode->rates[mode->num_rates] = mode11g->rates[i];
		mode->num_rates++;
		if (mode->num_rates == 4)
			break;
	}

	if (mode->num_rates == 0) {
		os_free(mode->channels);
		os_free(mode->rates);
		(*num_modes)--;
		return modes; /* No 802.11b rates */
	}

	wpa_printf(MSG_DEBUG, "athr: Added 802.11b mode based on 802.11g "
		   "information");

	return modes;
}


static struct hostapd_hw_modes * athr_get_hw_feature_data(void *priv,
							  u16 *num_modes,
							  u16 *flags)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	struct ieee80211req_chaninfo chans;
	struct ieee80211req_chanlist act;
	unsigned int i;
	struct hostapd_hw_modes *modes = NULL, *n, *mode;
	struct hostapd_channel_data *nc;
	int m24 = -1, m5 = -1;
	int chanbw = 0;
	int bw_div = 1;
	*num_modes = 0;
	*flags = 0;

	os_memset(&chans, 0, sizeof(chans));
	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = &chans;
	iwr.u.data.length = sizeof(chans);

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_GETCHANINFO, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "athr: Failed to get channel info: %s",
			   strerror(errno));
		return NULL;
	}

	os_memset(&act, 0, sizeof(act));
	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = &act;
	iwr.u.data.length = sizeof(act);

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_GETCHANLIST, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "athr: Failed to get active channel "
			   "list: %s", strerror(errno));
		return NULL;
	}
	if (get80211param(drv, drv->ifname, IEEE80211_PARAM_CHANBW,
			  &chanbw) < 0) {
		wpa_printf(MSG_DEBUG, "athr: Failed to get HALF/QUARTER mode "
			   "list: %s", strerror(errno));
		return NULL;
	}

	if(chanbw == 1) 
		bw_div = 2;
	else if(chanbw == 2) 
		bw_div = 4;
	wpa_printf(MSG_DEBUG, "athr: Channel list");
	for (i = 0; i < chans.ic_nchans; i++) {
		struct ieee80211_channel *chan = &chans.ic_chans[i];
		int active;
		active = act.ic_channels[chan->ic_ieee / 8] &
			(1 << (chan->ic_ieee % 8));
		wpa_printf(MSG_DEBUG, " * %u MHz chan=%d%s%s%s%s%s%s",
			   chan->ic_freq, chan->ic_ieee,
			   active ? "" : " [INACTIVE]",
			   chan->ic_flags & IEEE80211_CHAN_PASSIVE ?
			   " [PASSIVE]" : "",
			   chan->ic_flagext & IEEE80211_CHAN_DISALLOW_ADHOC ?
			   " [NO-IBSS]" : "",
			   chan->ic_flagext & IEEE80211_CHAN_DFS ?
			   " [DFS]" : "",
			   chan->ic_flags & IEEE80211_CHAN_HT40PLUS ?
			   " [HT40+]" : "",
			   chan->ic_flags & IEEE80211_CHAN_HT40MINUS ?
			   " [HT40-]" : "");

		if (chan->ic_flags & IEEE80211_CHAN_5GHZ) {
			if (m5 < 0) {
				n = os_realloc(modes, (*num_modes + 1) *
					       sizeof(*mode));
				if (n == NULL)
					continue;
				os_memset(&n[*num_modes], 0, sizeof(*mode));
				m5 = *num_modes;
				(*num_modes)++;
				modes = n;
				mode = &modes[m5];
				mode->mode = HOSTAPD_MODE_IEEE80211A;
				mode->num_rates = 8;
				mode->rates = os_zalloc(mode->num_rates *
							sizeof(int));
				if (mode->rates) {
					mode->rates[0] = 60/bw_div;
					mode->rates[1] = 90/bw_div;
					mode->rates[2] = 120/bw_div;
					mode->rates[3] = 180/bw_div;
					mode->rates[4] = 240/bw_div;
					mode->rates[5] = 360/bw_div;
					mode->rates[6] = 480/bw_div;
					mode->rates[7] = 540/bw_div;
				}
			}
			mode = &modes[m5];
		} else if (chan->ic_flags & IEEE80211_CHAN_2GHZ) {
			if (m24 < 0) {
				n = os_realloc(modes, (*num_modes + 1) *
					       sizeof(*mode));
				if (n == NULL)
					continue;
				os_memset(&n[*num_modes], 0, sizeof(*mode));
				m24 = *num_modes;
				(*num_modes)++;
				modes = n;
				mode = &modes[m24];
				mode->mode = HOSTAPD_MODE_IEEE80211G;
				mode->num_rates = 12;
				mode->rates = os_zalloc(mode->num_rates *
							sizeof(int));
				if (mode->rates) {
					mode->rates[0] = 10;
					mode->rates[1] = 20;
					mode->rates[2] = 55;
					mode->rates[3] = 110;
					mode->rates[4] = 60;
					mode->rates[5] = 90;
					mode->rates[6] = 120;
					mode->rates[7] = 180;
					mode->rates[8] = 240;
					mode->rates[9] = 360;
					mode->rates[10] = 480;
					mode->rates[11] = 540;
				}
			}
			mode = &modes[m24];
		} else
			continue;

		nc = os_realloc(mode->channels, (mode->num_channels + 1) *
				sizeof(*nc));
		if (nc == NULL)
			continue;
		mode->channels = nc;
		nc = &mode->channels[mode->num_channels];
		os_memset(nc, 0, sizeof(*nc));
		mode->num_channels++;
		nc->chan = chan->ic_ieee;
		nc->freq = chan->ic_freq;
		if (!active)
			nc->flag |= HOSTAPD_CHAN_DISABLED;
		if (chan->ic_flags & IEEE80211_CHAN_PASSIVE)
			nc->flag |= HOSTAPD_CHAN_NO_IR;
		if (chan->ic_flags & IEEE80211_CHAN_HT40PLUS)
			nc->flag |= HOSTAPD_CHAN_HT40 | HOSTAPD_CHAN_HT40PLUS;
		if (chan->ic_flags & IEEE80211_CHAN_HT40MINUS)
			nc->flag |= HOSTAPD_CHAN_HT40 | HOSTAPD_CHAN_HT40MINUS;
		if (chan->ic_flagext & IEEE80211_CHAN_DISALLOW_ADHOC)
			nc->flag |= HOSTAPD_CHAN_NO_IR;
		if (chan->ic_flagext & IEEE80211_CHAN_DFS)
			nc->flag |= HOSTAPD_CHAN_RADAR;
	}

	return athr_add_11b(modes, num_modes);
}


int driver_atheros_set_operstate(void *priv, int state)
{
	struct driver_atheros_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: operstate %d->%d (%s)",
		   __func__, drv->operstate, state, state ? "UP" : "DORMANT");
	drv->operstate = state;
	return netlink_send_oper_ifla(drv->netlink, drv->ifindex, -1,
				      state ? IF_OPER_UP : IF_OPER_DORMANT);
}


int driver_atheros_get_version(struct driver_atheros_data *drv)
{
	return drv->we_version_compiled;
}


/* P2P Stuff */

static int driver_atheros_send_action(void *priv, unsigned int freq,
				      unsigned int wait,
				      const u8 *dst, const u8 *src,
				      const u8 *bssid,
				      const u8 *data, size_t data_len,
				      int no_cck)
{
	struct driver_atheros_data *drv = priv;
	struct ieee80211_p2p_send_action *act;
	int res;

	act = os_zalloc(sizeof(*act) + data_len);
	if (act == NULL)
		return -1;
	act->freq = freq;
	os_memcpy(act->dst_addr, dst, ETH_ALEN);
	os_memcpy(act->src_addr, src, ETH_ALEN);
	os_memcpy(act->bssid, bssid, ETH_ALEN);
	os_memcpy(act + 1, data, data_len);
	wpa_printf(MSG_DEBUG, "%s: freq=%d, wait=%u, dst=" MACSTR ", src="
		   MACSTR ", bssid=" MACSTR,
		   __func__, act->freq, wait, MAC2STR(act->dst_addr),
		   MAC2STR(act->src_addr), MAC2STR(act->bssid));
	wpa_hexdump(MSG_MSGDUMP, "athr: act", (u8 *) act, sizeof(*act));
	wpa_hexdump(MSG_MSGDUMP, "athr: data", data, data_len);

	res = set80211big(drv, IEEE80211_IOC_P2P_SEND_ACTION,
			  act, sizeof(*act) + data_len);
	os_free(act);
	return res;
}


static int driver_atheros_remain_on_channel(void *priv, unsigned int freq,
					    unsigned int duration)
{
	struct driver_atheros_data *drv = priv;
	struct ieee80211_p2p_set_channel sc;
	int res;

	drv->req_id++;
	wpa_printf(MSG_DEBUG, "athr: IOC_P2P_SET_CHANNEL: req_id=%u freq=%u "
		   "MHz dur=%u msec", drv->req_id, freq, duration);
	if (drv->drv_in_scan)
		wpa_printf(MSG_DEBUG, "athr: Remain-on-channel request when "
			   "driver still in scan");
	if (drv->drv_in_remain_on_chan)
		wpa_printf(MSG_DEBUG, "athr: Remain-on-channel request when "
			   "driver still in remain-on-channel");
	os_memset(&sc, 0, sizeof(sc));
	sc.freq = freq;
	sc.req_id = drv->req_id;
	sc.channel_time = duration;
	res = set80211big(drv, IEEE80211_IOC_P2P_SET_CHANNEL,
			  &sc, sizeof(sc));
	if (res == 0) {
		drv->pending_set_chan_freq = freq;
		drv->pending_set_chan_dur = duration;
		/*
		 * IEEE80211_EV_CHAN_START event is generated once the driver
		 * has completed the request.
		 */
	} else
		drv->pending_set_chan_freq = 0;
	return res;
}

static int driver_atheros_cancel_remain_on_channel(void *priv)
{
	int ret;
	struct driver_atheros_data *drv = priv;
	drv->pending_set_chan_freq = 0;
	ret = set80211param(drv, IEEE80211_IOC_P2P_CANCEL_CHANNEL, 0, 1);
	if (ret == 0)
		drv->drv_in_remain_on_chan = 0;
	return ret;
}

static int driver_atheros_probe_req_report(void *priv, int report)
{
	struct driver_atheros_data *drv = priv;
	drv->report_probe_req = report;
	return 0;
}


static int driver_atheros_if_add(void *priv, enum wpa_driver_if_type type,
				 const char *ifname, const u8 *addr,
				 void *bss_ctx, void **drv_priv,
				 char *force_ifname, u8 *if_addr,
				 const char *bridge, int use_existing, int setup_ap)
{
	struct driver_atheros_data *drv = priv;
	struct ieee80211_clone_params cp;
	struct ifreq ifr;
	int radio_idx = 0;

	setup_ap = setup_ap; /* avoid a warning */

	wpa_printf(MSG_DEBUG, "%s: ifname=%s type=%d", __func__, ifname, type);

	os_memset(&ifr, 0, sizeof(ifr));

	os_memset(&cp, 0, sizeof(cp));

	switch (type) {
	case WPA_IF_P2P_GROUP:
		wpa_printf(MSG_DEBUG, "athr: opmode=IEEE80211_M_P2P_CLIENT "
			   "(GO/client not yet known)");
		cp.icp_opmode = IEEE80211_M_P2P_CLIENT;
		break;
	case WPA_IF_P2P_CLIENT:
		wpa_printf(MSG_DEBUG, "athr: opmode=IEEE80211_M_P2P_CLIENT");
		cp.icp_opmode = IEEE80211_M_P2P_CLIENT;
		break;
	case WPA_IF_P2P_GO:
		wpa_printf(MSG_DEBUG, "athr: opmode=IEEE80211_M_P2P_GO");
		cp.icp_opmode = IEEE80211_M_P2P_GO;
		break;
	default:
		wpa_printf(MSG_ERROR, "athr: Unsupported if_add type %d",
			   type);
		return -1;
	}

	/* NB: default is to request a unique bssid/mac */
	cp.icp_flags = IEEE80211_CLONE_BSSID;

	/* Request driver to build interface name */
	os_strlcpy(cp.icp_name, "wlan", IFNAMSIZ);

	ifr.ifr_data = (void *) &cp;

#ifdef IEEE80211_IOC_P2P_RADIO_IDX
	if ((get80211param(drv, drv->ifname, IEEE80211_IOC_P2P_RADIO_IDX,
			   &radio_idx) < 0) || radio_idx < 0) {
		wpa_printf(MSG_ERROR, "athr: IEEE80211_IOC_P2P_RADIO_IDX(%s, "
			   "%d) failed: %s",
			   ifname, radio_idx, strerror(errno));
		return -1;
	}
#endif /* IEEE80211_IOC_P2P_RADIO_IDX */

	os_snprintf(ifr.ifr_name, IFNAMSIZ, "wifi%d", radio_idx);

	if (ioctl(drv->ioctl_sock, SIOC80211IFCREATE, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "athr: SIOC80211IFCREATE(%s, %s) "
			   "failed: %s",
			   ifr.ifr_name, ifname, strerror(errno));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SIOC80211IFCREATE: ifr_name=%s", ifr.ifr_name);
	ifname = ifr.ifr_name;
	os_strlcpy(force_ifname, ifr.ifr_name, IFNAMSIZ);

	/* return hardware local mac_addr to caller */
	if (linux_get_ifhwaddr(drv->ioctl_sock, ifname, if_addr) < 0) {
		wpa_printf(MSG_ERROR, "athr: Could not get interface address");
		os_memset(&ifr, 0, sizeof(ifr));
		os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
		if (ioctl(drv->ioctl_sock, SIOC80211IFDESTROY, &ifr) < 0)
			wpa_printf(MSG_ERROR, "athr: SIOC80211IFDESTROY(%s) "
				   "failed: %s", ifname, strerror(errno));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "athr: Added interface %s - " MACSTR,
		   ifname, MAC2STR(if_addr));

	return 0;
}


static int driver_atheros_if_remove(void *priv, enum wpa_driver_if_type type,
				    const char *ifname)
{
	struct driver_atheros_data *drv = priv;
	struct ifreq ifr;

	wpa_printf(MSG_DEBUG, " %s: ifname=%s, type %d", __func__, ifname,
		   type);
	if (type != WPA_IF_P2P_CLIENT && type != WPA_IF_P2P_GO &&
	    type != WPA_IF_P2P_GROUP) {
		wpa_printf(MSG_DEBUG, "%s: failed ifname=%s, type %d",
			   __func__, ifname, type);
		return -1;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(drv->ioctl_sock, SIOC80211IFDESTROY, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "athr: SIOC80211IFDESTROY(%s) "
			   "failed: %s", ifname, strerror(errno));
		return -1;
	}

	return 0;
}


static int driver_atheros_hapd_sta_deauth(void *priv, const u8 *own_addr,
					  const u8 *addr, int reason_code)
{
	struct driver_atheros_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=" MACSTR " reason_code=%d",
		   __func__, MAC2STR(addr), reason_code);
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme),
			   1);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to deauth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}


static int
driver_atheros_hapd_send_eapol(void *priv, const u8 *addr, const u8 *data,
			       size_t data_len, int encrypt,
			       const u8 *own_addr, u32 flags)
{
	struct driver_atheros_data *drv = priv;

	if (!drv->l2)
		return -1;
	return l2_packet_send(drv->l2, addr, ETH_P_EAPOL, data, data_len);
}


static int
atheros_set_wps_ie(void *priv, const u8 *ie, size_t len, u32 frametype)
{
#define IEEE80211_APPIE_MAX    1024 /* max appie buffer size */
	struct driver_atheros_data *drv = priv;
	u8 buf[IEEE80211_APPIE_MAX];
	struct ieee80211req_getset_appiebuf *beac_ie;

	wpa_printf(MSG_DEBUG, "WEXT: ifname=%s, Set APPIE frametype=%d len=%d",
		   drv->ifname, frametype, (int) len);

	beac_ie = (struct ieee80211req_getset_appiebuf *) buf;
	beac_ie->app_frmtype = frametype;
	beac_ie->app_buflen = len;
	memcpy(&(beac_ie->app_buf[0]), ie, len);

	return set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, beac_ie,
			    sizeof(struct ieee80211req_getset_appiebuf) + len,
			    1);
}


static int
driver_atheros_set_ap_wps_ie(void *priv, const struct wpabuf *beacon,
			     const struct wpabuf *proberesp,
			     const struct wpabuf *assocresp)
{
	struct driver_atheros_data *drv = priv;

	atheros_set_wps_ie(priv, assocresp ? wpabuf_head(assocresp) : NULL,
			   assocresp ? wpabuf_len(assocresp) : 0,
			   IEEE80211_APPIE_FRAME_ASSOC_RESP);
	if (atheros_set_wps_ie(priv, beacon ? wpabuf_head(beacon) : NULL,
			       beacon ? wpabuf_len(beacon) : 0,
			       IEEE80211_APPIE_FRAME_BEACON))
		return -1;
	if (atheros_set_wps_ie(priv,
			       proberesp ? wpabuf_head(proberesp) : NULL,
			       proberesp ? wpabuf_len(proberesp): 0,
			       IEEE80211_APPIE_FRAME_PROBE_RESP))
		return -1;
	if (drv->start_hostap) {
		drv->start_hostap = 0;

		if (set80211param(drv, IEEE80211_IOC_START_HOSTAP, 0, 1) < 0) {
			wpa_printf(MSG_DEBUG, "athr: Try START_HOSTAP again "
				   "after one second delay");
			os_sleep(1, 0);
			if (set80211param(drv, IEEE80211_IOC_START_HOSTAP,
					  0, 1) < 0)
				return -1;
		}
	}

	return 0;
}


static const u8 * driver_atheros_get_mac_addr(void *priv)
{
	struct driver_atheros_data *drv = priv;
	return drv->own_addr;
}

static int driver_atheros_set_param(void *priv, const char *param)
{
	struct driver_atheros_data *drv = priv;
	const char *pos, *end;

	if (param == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "athr: Set param '%s'", param);
	pos = os_strstr(param, "shared_interface=");
	if (pos) {
		pos += 17;
		end = os_strchr(pos, ' ');
		if (end == NULL)
			end = pos + os_strlen(pos);
		if (end - pos >= IFNAMSIZ) {
			wpa_printf(MSG_ERROR, "athr: Too long "
				   "shared_interface name");
			return -1;
		}
		os_memcpy(drv->shared_ifname, pos, end - pos);
		drv->shared_ifname[end - pos] = '\0';
		wpa_printf(MSG_DEBUG, "athr: Shared interface: %s",
			   drv->shared_ifname);
	}

	return 0;
}


static int driver_atheros_start_ap(struct driver_atheros_data *drv,
				   struct wpa_driver_associate_params *params)
{
	int opmode;
	if (get80211param(drv, drv->ifname, IEEE80211_IOC_P2P_OPMODE, &opmode)
	    < 0)
		opmode = -1;
	if (opmode == IEEE80211_M_P2P_CLIENT &&
	    set80211param(drv, IEEE80211_IOC_P2P_OPMODE,
			  IEEE80211_M_P2P_GO, 1) < 0)
		return -1;

	if (params->uapsd >= 0) {
		wpa_printf(MSG_DEBUG, "athr: Set UAPSD for AP mode: %d",
			   params->uapsd);
		if (set80211param(drv, IEEE80211_PARAM_UAPSDINFO,
				  params->uapsd, 1) < 0)
			return -1;
	}

	wpa_printf(MSG_DEBUG, " %s: Set channel = %d", __func__, params->freq.freq);

	if (driver_atheros_set_freq(drv, params->freq.freq) < 0)
		return -1;

	wpa_printf(MSG_DEBUG, "KEY MGMT SUITE=%d, PW SUITE=%d, MC SUITE=%d",
		   params->key_mgmt_suite, params->pairwise_suite,
		   params->group_suite);
	if (params->key_mgmt_suite == WPA_KEY_MGMT_PSK) {
		set80211param(drv, IEEE80211_IOC_WPA, 2, 1);
		set80211param(drv, IEEE80211_IOC_AUTHMODE,
			      IEEE80211_AUTH_RSNA, 1);
		set80211param(drv, IEEE80211_IOC_KEYMGTALGS,
			      WPA_ASE_8021X_PSK, 1);
	} else if (params->key_mgmt_suite == WPA_KEY_MGMT_NONE) {
		if (set80211param(drv, IEEE80211_PARAM_AUTHMODE,
				  IEEE80211_AUTH_OPEN, 1) < 0)
			return -1;
	} else {
		wpa_printf(MSG_INFO, "athrosx: Unsupported AP key_mgmt_suite "
			   "%d", params->key_mgmt_suite);
		return -1;
	}

	set80211param(drv, IEEE80211_IOC_WPS_MODE, 1, 1);

	driver_atheros_set_cipher(drv, IEEE80211_PARAM_UCASTCIPHER,
				  params->pairwise_suite);
	driver_atheros_set_cipher(drv, IEEE80211_PARAM_MCASTCIPHER,
				  params->group_suite);

	if (driver_atheros_set_ssid(drv, params->ssid, params->ssid_len) < 0)
		return -1;


	if (params->key_mgmt_suite == WPA_KEY_MGMT_NONE) {
		drv->start_hostap = 0;

		if (set80211param(drv, IEEE80211_IOC_START_HOSTAP, 0, 1) < 0) {
			wpa_printf(MSG_DEBUG, "athr: Try START_HOSTAP again "
				   "after one second delay");
			os_sleep(1, 0);
			if (set80211param(drv, IEEE80211_IOC_START_HOSTAP, 0,
					  1) < 0)
				return -1;
		}
	} else {
		wpa_printf(MSG_DEBUG, "athr: Delay START_HOSTAP until IEs are "
			   "configured");
		drv->start_hostap = 1;
	}

	return 0;
}

static int driver_atheros_deinit_ap(void *priv)
{
	struct driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "atheros: Deinit AP mode");
	return driver_atheros_set_mlme(drv, IEEE80211_MLME_STOP_BSS, NULL,
				       NULL);
}


struct noa_descriptor {
	u32 type_count;
	u32 duration;
	u32 interval;
	u32 start_time;
};

#define MAX_NOA_DESCRIPTORS 4
struct p2p_noa {
	u32 curr_tsf32;
	u8 index;
	u8 opp_ps;
	u8 ctwindow;
	u8 num_desc;
	struct noa_descriptor noa_desc[MAX_NOA_DESCRIPTORS];
};


static int athr_get_noa(void *priv, u8 *buf, size_t buf_len)
{
	struct driver_atheros_data *drv = priv;
	struct iwreq iwr;
	struct p2p_noa noa;
	u8 *pos;
	int i;

	wpa_printf(MSG_DEBUG, "athr: Get NoA");
	os_memset(&iwr, 0, sizeof(iwr));
	os_memset(&noa, 0, sizeof(noa));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.flags = IEEE80211_IOC_P2P_NOA_INFO;
	iwr.u.data.pointer = (void *) &noa;
	iwr.u.data.length = sizeof(noa);
	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "athr: ioctl[P2P_BIG_PARAM,"
			   "P2P_NOA_INFO] failed: %s", strerror(errno));
		return -1;
	}

	if (!noa.opp_ps && noa.ctwindow == 0 && noa.num_desc == 0)
		return 0;

	if (buf_len < (size_t) 2 + 13 * noa.num_desc) {
		wpa_printf(MSG_DEBUG, "athr: No room for returning NoA");
		return -1;
	}

	buf[0] = noa.index;
	buf[1] = (noa.opp_ps ? 0x80 : 0) | (noa.ctwindow & 0x7f);
	pos = buf + 2;
	for (i = 0; i < noa.num_desc; i++) {
		*pos++ = noa.noa_desc[i].type_count & 0xff;
		WPA_PUT_LE32(pos, noa.noa_desc[i].duration);
		pos += 4;
		WPA_PUT_LE32(pos, noa.noa_desc[i].interval);
		pos += 4;
		WPA_PUT_LE32(pos, noa.noa_desc[i].start_time);
		pos += 4;
	}

	return pos - buf;
}


static int athr_set_noa(void *priv, u8 count, int start, int duration)
{
	struct driver_atheros_data *drv = priv;
	struct ieee80211_p2p_go_noa noa;

	wpa_printf(MSG_DEBUG, "athr: Set NoA %d %d %d",
		   count, start, duration);
	os_memset(&noa, 0, sizeof(noa));
	noa.num_iterations = count;
	noa.offset_next_tbtt = start;
	noa.duration = duration;
	return set80211big(drv, IEEE80211_IOC_P2P_GO_NOA,
			   &noa, sizeof(noa));
}


static int athr_set_p2p_powersave(void *priv, int legacy_ps, int opp_ps,
				  int ctwindow)
{
	struct driver_atheros_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "athr: Set P2P Power Save: legacy=%d opp=%d "
		   "ctwindow=%d", legacy_ps, opp_ps, ctwindow);

	if (legacy_ps != -1) {
		if (legacy_ps == 99) {
			/* Forced sleep test mode */
			if (set80211param(drv, IEEE80211_PARAM_SLEEP, 1, 1) <
			    0)
				ret = -1;
		} else if (legacy_ps == 98) {
			/* Forced sleep test mode */
			if (set80211param(drv, IEEE80211_PARAM_SLEEP, 0, 1) <
			    0)
				ret = -1;
		} else if (legacy_ps == 97) {
			/* PS-Poll test mode */
			if (set80211param(drv, IEEE80211_PARAM_PSPOLL, 1, 1) <
			    0)
				ret = -1;
		} else if (legacy_ps == 96) {
			/* Disable PS-Poll test mode */
			if (set80211param(drv, IEEE80211_PARAM_PSPOLL, 0, 1) <
			    0)
				ret = -1;
		} else if (set80211param(drv, IEEE80211_PARAM_NETWORK_SLEEP,
					 legacy_ps, 1) < 0)
			ret = -1;
	}

	if (opp_ps != -1) {
		if (set80211param(drv, IEEE80211_IOC_P2P_GO_OPPPS, opp_ps, 1) <
		    0)
			ret = -1;
	}

	if (ctwindow != -1) {
		if (set80211param(drv, IEEE80211_IOC_P2P_GO_CTWINDOW, ctwindow,
				  1) < 0)
			ret = -1;
	}

	return ret;
}


static int athr_ampdu(void *priv, int ampdu)
{
	struct driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "athr: Set ampdu parameter to %d", ampdu);
	return set80211param(drv, IEEE80211_PARAM_AMPDU, ampdu, 1);
}


static int athr_set_intra_bss(void *priv, int enabled)
{
	struct driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "athr: Setting Intra BSS bridging to %d",
		   enabled);
	return set80211param(drv, IEEE80211_PARAM_APBRIDGE, enabled, 1);
}


static int athr_driver_acl(struct driver_atheros_data *drv, const char *cmd,
			   char *buf, size_t buf_len)
{
#define _MAX_ACL_MACADDR_BUF    (2048)
	/* Access Control List */
	const char *pos = cmd;
	struct sockaddr sa;
	const char *param;
	int i, cnt;
	int ret = -1;

	if ((param = os_strstr(pos, "maccmd ")) != NULL) {
		int val = atoi(param + 7);
		ret = set80211param(drv, IEEE80211_PARAM_MACCMD, val, 1);
	} else if ((param = os_strstr(pos, "getmaccmd")) != NULL) {
		if (get80211param(drv, drv->ifname, IEEE80211_PARAM_MACCMD,
				  &i) == 0) {
			ret = os_snprintf(buf, buf_len, "ACL is %s\n",
					  ((i == 0) ? "OPEN" :
					   ((i == 1) ? "ALLOW" : "DENY")));
		}
	} else if ((param = os_strstr(pos, "addmac ")) != NULL) {
		param += 7;
		os_memset(&sa, 0, sizeof(sa));
		if (hwaddr_aton(param, (u8 *) sa.sa_data)) {
			wpa_printf(MSG_ERROR, "athr: Invalid MAC ADDRESS");
		} else
			ret = set80211priv(drv, IEEE80211_IOCTL_ADDMAC,
					   &sa, sizeof(sa), 1);
	} else if ((param = os_strstr(pos, "delmac ")) != NULL) {
		param += 7;
		os_memset(&sa, 0, sizeof(sa));
		if (hwaddr_aton(param, (u8 *) sa.sa_data)) {
			wpa_printf(MSG_ERROR, "athr: Invalid MAC ADDRESS");
		} else
			ret = set80211priv(drv, IEEE80211_IOCTL_DELMAC,
					   &sa, sizeof(sa), 1);
	} else if ((param = os_strstr(pos, "getmac")) != NULL) {
		u8 *macl = NULL, *p;
		macl = os_zalloc(_MAX_ACL_MACADDR_BUF);
		if (!macl) {
			wpa_printf(MSG_ERROR, "athr: Alloc memory failed");
			return -1;
		}

		if (set80211priv(drv, IEEE80211_IOCTL_GET_MACADDR,
				 macl, _MAX_ACL_MACADDR_BUF, 1) < 0) {
			os_free(macl);
			return -1;
		}

		cnt = *((int *) macl);
		p = macl + sizeof(int);
		ret = 0;
		if (cnt) {
			for (i = 0; i < cnt; i++, p += ETH_ALEN) {
				ret += os_snprintf(buf + ret, buf_len - ret,
						   "[%3d] " MACSTR "\n",
						   i, MAC2STR(p));
			}
		} else
			ret = os_snprintf(buf, buf_len - ret,
					  "No ACL MAC Address exist!\n");
		os_free(macl);
	} else {
		wpa_printf(MSG_ERROR, "athr: UNKNOWN acl CMD");
	}

	return ret;
}

#ifdef ANDROID
/**
 * athr_driver_cmd - Driver escape function
 * @priv: Pointer to private wext data from driver_atheros_init()
 * @cmd: Command in
 * @buf: Buffer of result message
 * @buf_len: Buffer length of result message
 * Returns: "Length of result message" on success, -1 on failure
 */
static int athr_driver_cmd(void *priv, const char *cmd, char *buf,
			   size_t buf_len)
{
	struct driver_atheros_data *drv = priv;
	const char *sub_cmd;

	/* cmd in : driver [CMD] [SUBCMD] [PARAM] */
	sub_cmd = os_strstr(cmd, "acl ");
	if (sub_cmd)
		return athr_driver_acl(drv, sub_cmd + 4, buf, buf_len);

	wpa_printf(MSG_ERROR, "athr: UNKNOWN CMD");
	return -1;
}
#endif


static int athr_radio_disable(void *priv, int disabled)
{
	struct driver_atheros_data *drv = priv;

	wpa_printf(MSG_DEBUG, "athr: %s radio",
		   disabled ? "Disable" : "Enable");
	if (disabled && drv->disabled)
		return 0;
	if (!disabled && !drv->disabled)
		return 0;

	if (athr_no_stop_disassoc(drv, disabled) < 0) {
		wpa_printf(MSG_DEBUG, "athr: Failed to prepare driver for "
			   "silent radio disable operation");
		return -1;
	}

	if (linux_set_iface_flags(drv->ioctl_sock, drv->ifname, !disabled) < 0)
	{
		wpa_printf(MSG_DEBUG, "athr: Failed to set interface up/down");
		return -1;
	}

	drv->disabled = disabled;

	return 0;
}


#ifdef CONFIG_TDLS

static int athr_tdls_disc_req(struct driver_atheros_data *drv, const u8 *addr);

static int athr_send_tdls_msg(struct driver_atheros_data *drv, const u8 *dst,
			      u8 action_code, u8 dialog_token, const u8 *buf,
			      size_t len)
{
	u8 *data, *pos;
	int ret;
	u16 val;
	size_t dlen;

	dlen = ETH_ALEN + 2 + 2 + 2 + len;
	data = os_malloc(dlen);
	if (data == NULL)
		return -1;

	/* Command header for driver */
	pos = data;
	os_memcpy(pos, dst, ETH_ALEN);
	pos += ETH_ALEN;

	val = action_code;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = dialog_token;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = len;
	os_memcpy(pos, &val, 2);
	pos += 2;

	/* IEs to add to the message */
	os_memcpy(pos, buf, len);

	ret = atheros_set_wps_ie(drv, data, dlen,
				 IEEE80211_APPIE_FRAME_TDLS_FTIE);

	os_free(data);

	return ret;
}


static int athr_send_tdls_msg_error(struct driver_atheros_data *drv,
				    const u8 *dst, u8 action_code,
				    u16 status_code, u8 dialog_token)
{
	u8 *data, *pos;
	size_t dlen;
	int ret;
	u16 val;
	struct wpa_tdls_error {
		u16 tdls_action;
		u16 status;
		u16 dialog_token;
	} err;

	dlen = ETH_ALEN + 2 + 2 + sizeof(err);
	data = os_malloc(dlen);
	if (data == NULL)
		return -1;

	/* Command header for driver */
	pos = data;
	os_memcpy(pos, dst, ETH_ALEN);
	pos += ETH_ALEN;

	val = IEEE80211_TDLS_ERROR;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = sizeof(err);
	os_memcpy(pos, &val, 2);
	pos += 2;

	/* Error parameters */
	err.tdls_action = action_code;
	err.status = status_code;
	err.dialog_token = dialog_token;
	os_memcpy(pos, &err, sizeof(err));

	ret = atheros_set_wps_ie(drv, data, dlen,
				 IEEE80211_APPIE_FRAME_TDLS_FTIE);

	os_free(data);

	return ret;
}


static int athr_send_tdls_mgmt(void *priv, const u8 *dst, u8 action_code,
			       u8 dialog_token, u16 status_code,
			       const u8 *buf, size_t len)
{
	struct driver_atheros_data *drv = priv;

	wpa_printf(MSG_DEBUG, "athr: send_tdls_mgmt: dst=" MACSTR
		   " action_code=%u dialog_token=%u status_code=%u",
		   MAC2STR(dst), action_code, dialog_token, status_code);
	wpa_hexdump(MSG_DEBUG, "athr: send_tdls_mgmt: buf", buf, len);

	if (status_code == 0 &&
	    (action_code == WLAN_TDLS_SETUP_REQUEST ||
	     action_code == WLAN_TDLS_SETUP_RESPONSE ||
	     action_code == WLAN_TDLS_SETUP_CONFIRM))
		return athr_send_tdls_msg(drv, dst, action_code, dialog_token,
					  buf, len);

	/* TODO: Use status_code as reason code for teardown message */
	if (action_code == WLAN_TDLS_TEARDOWN)
		return athr_send_tdls_msg(drv, dst, action_code, dialog_token,
					  buf, len);

	if (action_code == WLAN_TDLS_SETUP_REQUEST ||
	    action_code == WLAN_TDLS_SETUP_RESPONSE ||
	    action_code == WLAN_TDLS_SETUP_CONFIRM)
		return athr_send_tdls_msg_error(drv, dst, action_code,
						status_code, dialog_token);

	if (action_code == WLAN_TDLS_DISCOVERY_REQUEST)
		return athr_tdls_disc_req(drv, dst);

	wpa_printf(MSG_INFO, "athr: Unsupported send_tdls_mgmt operation");
	return -1;
}


static int athr_tdls_enable(struct driver_atheros_data *drv, int enabled)
{
	return set80211param(drv, IEEE80211_PARAM_TDLS_ENABLE, enabled, 1);
}


static int athr_set_tdls_macaddr(struct driver_atheros_data *drv,
				 const u8 *addr)
{
	u32 val;
	val = WPA_GET_BE32(addr);
	if (set80211param(drv, IEEE80211_PARAM_TDLS_MACADDR1, val, 1) < 0)
		return -1;
	val = WPA_GET_BE16(addr + 4);
	if (set80211param(drv, IEEE80211_PARAM_TDLS_MACADDR2, val, 1) < 0)
		return -1;
	return 0;
}


static int athr_tdls_disc_req(struct driver_atheros_data *drv, const u8 *addr)
{
	wpa_printf(MSG_DEBUG, "athr: TDLS discovery request " MACSTR,
		   MAC2STR(addr));
	if (athr_set_tdls_macaddr(drv, addr) < 0)
		return -1;

	return set80211param(drv, IEEE80211_PARAM_TDLS_DISCOVERY_REQ, 1, 1);
}


static int athr_tdls_setup(struct driver_atheros_data *drv, const u8 *addr)
{
	wpa_printf(MSG_DEBUG, "athr: TDLS setup " MACSTR,
		   MAC2STR(addr));
	if (athr_set_tdls_macaddr(drv, addr) < 0)
		return -1;

	return set80211param(drv, IEEE80211_PARAM_SET_TDLS_RMAC, 1, 1);
}


static int athr_tdls_teardown(struct driver_atheros_data *drv, const u8 *addr)
{
	wpa_printf(MSG_DEBUG, "athr: TDLS teardown " MACSTR,
		   MAC2STR(addr));
	if (athr_set_tdls_macaddr(drv, addr) < 0)
		return -1;

	return set80211param(drv, IEEE80211_PARAM_CLR_TDLS_RMAC, 1, 1);
}


static int athr_tdls_disable_link(struct driver_atheros_data *drv,
				  const u8 *addr)
{
	u8 *data, *pos;
	size_t dlen;
	int ret;
	u16 val;

	wpa_printf(MSG_DEBUG, "athr: TDLS disable link " MACSTR,
		   MAC2STR(addr));

	dlen = ETH_ALEN + 2 + 2;
	data = os_malloc(dlen);
	if (data == NULL)
		return -1;

	/* Command header for driver */
	pos = data;
	os_memcpy(pos, addr, ETH_ALEN);
	pos += ETH_ALEN;

	val = IEEE80211_TDLS_TEARDOWN_DELETE_NODE;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = 0;
	os_memcpy(pos, &val, 2);

	ret = atheros_set_wps_ie(drv, data, dlen,
				 IEEE80211_APPIE_FRAME_TDLS_FTIE);

	os_free(data);

	return ret;
}


static int athr_tdls_enable_link(struct driver_atheros_data *drv,
				 const u8 *addr)
{
	u8 *data, *pos;
	size_t dlen;
	int ret;
	u16 val;

	wpa_printf(MSG_DEBUG, "athr: TDLS enable link " MACSTR, MAC2STR(addr));

	dlen = ETH_ALEN + 2 + 2;
	data = os_malloc(dlen);
	if (data == NULL)
		return -1;

	/* Command header for driver */
	pos = data;
	os_memcpy(pos, addr, ETH_ALEN);
	pos += ETH_ALEN;

	val = IEEE80211_TDLS_ENABLE_LINK;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = 0;
	os_memcpy(pos, &val, 2);

	ret = atheros_set_wps_ie(drv, data, dlen,
				 IEEE80211_APPIE_FRAME_TDLS_FTIE);

	os_free(data);

	return ret;
}


static int athr_tdls_oper(void *priv, enum tdls_oper oper, const u8 *peer)
{
	struct driver_atheros_data *drv = priv;

	if (oper == TDLS_DISABLE)
		return athr_tdls_enable(drv, 0);

	if (athr_tdls_enable(drv, 1) < 0)
		return -1;

	switch (oper) {
	case TDLS_ENABLE:
		/* Already enabled above */
		return 0;
	case TDLS_DISCOVERY_REQ:
		return athr_tdls_disc_req(drv, peer);
	case TDLS_SETUP:
		return athr_tdls_setup(drv, peer);
	case TDLS_TEARDOWN:
		return athr_tdls_teardown(drv, peer);
	case TDLS_ENABLE_LINK:
		return athr_tdls_enable_link(drv, peer);
	case TDLS_DISABLE_LINK:
		return athr_tdls_disable_link(drv, peer);
	default:
		wpa_printf(MSG_DEBUG, "athr: Unsupported TDLS operation %d",
			   oper);
		return -1;
	}
}


#endif /* CONFIG_TDLS */


static int athr_set_ap(void *priv, struct wpa_driver_ap_params *params)
{
	struct driver_atheros_data *drv = priv;
	wpa_printf(MSG_DEBUG, "athr: set_ap");
	return athr_set_intra_bss(drv, !params->isolate);
}


#ifdef CONFIG_IEEE80211V
static int athr_wnm_tfs(struct driver_atheros_data *drv, const u8* peer,
			u8 *ie, u16 *len, enum wnm_oper oper)
{
#define IEEE80211_APPIE_MAX    1024 /* max appie buffer size */
	u8 buf[IEEE80211_APPIE_MAX];
	struct ieee80211req_getset_appiebuf *tfs_ie;
	u16 val;

	wpa_printf(MSG_DEBUG, "athr: ifname=%s, WNM TFS IE oper=%d " MACSTR,
		   drv->ifname, oper, MAC2STR(peer));

	switch (oper) {
	case WNM_SLEEP_TFS_REQ_IE_ADD:
		tfs_ie = (struct ieee80211req_getset_appiebuf *) buf;
		tfs_ie->app_frmtype = IEEE80211_APPIE_FRAME_WNM;
		tfs_ie->app_buflen = IEEE80211_APPIE_MAX -
			sizeof(struct ieee80211req_getset_appiebuf);
		/* Command header for driver */
		os_memcpy(&(tfs_ie->app_buf[0]), peer, ETH_ALEN);
		val = oper;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN, &val, 2);
		val = 0;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2, &val, 2);

		if (set80211priv(drv, IEEE80211_IOCTL_GET_APPIEBUF, tfs_ie,
				 IEEE80211_APPIE_MAX, 1)) {
			wpa_printf(MSG_DEBUG, "%s: Failed to get WNM TFS IE: "
				   "%s", __func__, strerror(errno));
			return -1;
		}

		*len = tfs_ie->app_buflen;
		os_memcpy(ie, &(tfs_ie->app_buf[0]), *len);
		wpa_printf(MSG_DEBUG, "athr: 0x%2X len=%d\n",
			   tfs_ie->app_buf[0], *len);
		break;
	case WNM_SLEEP_TFS_REQ_IE_NONE:
		*len = 0;
		break;
	case WNM_SLEEP_TFS_RESP_IE_SET:
		if (*len > IEEE80211_APPIE_MAX -
		    sizeof(struct ieee80211req_getset_appiebuf)) {
			wpa_printf(MSG_DEBUG, "TFS Req IE(s) too large");
			return -1;
		}
		tfs_ie = (struct ieee80211req_getset_appiebuf *) buf;
		tfs_ie->app_frmtype = IEEE80211_APPIE_FRAME_WNM;
		tfs_ie->app_buflen = ETH_ALEN + 2 + 2 + *len;

		/* Command header for driver */
		os_memcpy(&(tfs_ie->app_buf[0]), peer, ETH_ALEN);
		val = oper;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN, &val, 2);
		val = *len;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2, &val, 2);

		/* copy the ie */
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2 + 2, ie, *len);

		if (set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, tfs_ie,
				 IEEE80211_APPIE_MAX, 1)) {
			wpa_printf(MSG_DEBUG, "%s: Failed to set WNM TFS IE: "
				   "%s", __func__, strerror(errno));
			return -1;
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "Unsupported TFS oper %d", oper);
		break;
	}

	return 0;
}


static int athr_wnm_sleep(struct driver_atheros_data *drv,
			  const u8 *peer, enum wnm_oper oper)
{
	u8 *data, *pos;
	size_t dlen;
	int ret;
	u16 val;

	wpa_printf(MSG_DEBUG, "athr: WNM-Sleep Oper %d" MACSTR,
		   oper, MAC2STR(peer));

	dlen = ETH_ALEN + 2 + 2;
	data = os_malloc(dlen);
	if (data == NULL)
		return -1;

	/* Command header for driver */
	pos = data;
	os_memcpy(pos, peer, ETH_ALEN);
	pos += ETH_ALEN;

	val = oper;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = 0;
	os_memcpy(pos, &val, 2);

	ret = atheros_set_wps_ie(drv, data, dlen, IEEE80211_APPIE_FRAME_WNM);

	os_free(data);

	return ret;
}


static int athr_wnm_oper(void *priv, enum wnm_oper oper, const u8 *peer,
			 u8 *buf, u16 *buf_len)
{
	struct driver_atheros_data *drv = priv;

	switch (oper) {
	case WNM_SLEEP_ENTER_CONFIRM:
	case WNM_SLEEP_ENTER_FAIL:
	case WNM_SLEEP_EXIT_CONFIRM:
	case WNM_SLEEP_EXIT_FAIL:
		return athr_wnm_sleep(drv, peer, oper);
	case WNM_SLEEP_TFS_REQ_IE_ADD:
	case WNM_SLEEP_TFS_REQ_IE_NONE:
	case WNM_SLEEP_TFS_RESP_IE_SET:
		return athr_wnm_tfs(drv, peer, buf, buf_len, oper);
	default:
		wpa_printf(MSG_DEBUG, "athr: Unsupported WNM operation %d",
			   oper);
		return -1;
	}
}
#endif /* CONFIG_IEEE80211V */


const struct wpa_driver_ops wpa_driver_athr_ops = {
	.name = "athr",
	.desc = "Atheros Linux driver",
	.get_bssid = driver_atheros_get_bssid,
	.get_ssid = driver_atheros_get_ssid,
	.set_key = driver_atheros_set_key,
	.set_countermeasures = driver_atheros_set_countermeasures,
	.scan2 = driver_atheros_scan,
	.get_scan_results2 = driver_atheros_get_scan_results,
	.deauthenticate = driver_atheros_deauthenticate,
	.associate = driver_atheros_associate,
	.init = driver_atheros_init,
	.deinit = driver_atheros_deinit,
	.get_capa = driver_atheros_get_capa,
	.get_hw_feature_data = athr_get_hw_feature_data,
	.set_operstate = driver_atheros_set_operstate,
	.send_action = driver_atheros_send_action,
	.remain_on_channel = driver_atheros_remain_on_channel,
	.cancel_remain_on_channel = driver_atheros_cancel_remain_on_channel,
	.probe_req_report = driver_atheros_probe_req_report,
	.sta_deauth = driver_atheros_hapd_sta_deauth,
	.hapd_send_eapol = driver_atheros_hapd_send_eapol,
	.if_add = driver_atheros_if_add,
	.if_remove = driver_atheros_if_remove,
	.set_ap_wps_ie = driver_atheros_set_ap_wps_ie,
	.get_mac_addr = driver_atheros_get_mac_addr,
	.deinit_ap = driver_atheros_deinit_ap,
	.get_noa = athr_get_noa,
	.set_noa = athr_set_noa,
	.set_param = driver_atheros_set_param,
	.set_p2p_powersave = athr_set_p2p_powersave,
	.ampdu = athr_ampdu,
#ifdef ANDROID
	.driver_cmd = athr_driver_cmd,
#endif
	.radio_disable = athr_radio_disable,
#ifdef CONFIG_TDLS
	.send_tdls_mgmt = athr_send_tdls_mgmt,
	.tdls_oper = athr_tdls_oper,
#endif /* CONFIG_TDLS */
	.set_ap = athr_set_ap,
#ifdef CONFIG_IEEE80211V
	.wnm_oper = athr_wnm_oper,
#endif /* CONFIG_IEEE80211V */
};
