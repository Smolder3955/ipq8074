/*
 * Copyright (c) 2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <net/net_namespace.h>
#include <net/netlink.h>

#include <qdf_types.h>
#include <ieee80211_ioctl.h>
#include <osif_net.h>
//#include <qdf_io.h>
#include <qdf_str.h>
#include <qdf_trace.h>

//#include <../qdf/linux/src/i_qdf_types.h>
#include <acfg_api_types.h>
#include <acfg_event_types.h>
#include <ieee80211_var.h>

/**
 * IW Events
 */
typedef uint32_t  (*acfg_event_t)(__qdf_softc_t *sc,
				acfg_ev_data_t *data);

#define PROTO_IWEVENT(name)    \
	static uint32_t  acfg_iwevent_##name(__qdf_softc_t   *, \
				acfg_ev_data_t *)

PROTO_IWEVENT(scan_done);
PROTO_IWEVENT(assoc_ap);
PROTO_IWEVENT(assoc_sta);
PROTO_IWEVENT(chan_start);
PROTO_IWEVENT(chan_end);
PROTO_IWEVENT(rx_mgmt);
PROTO_IWEVENT(sent_action);
PROTO_IWEVENT(leave_ap);
PROTO_IWEVENT(gen_ie);
PROTO_IWEVENT(assoc_req_ie);
PROTO_IWEVENT(wsupp_generic);
PROTO_IWEVENT(mic_fail);
PROTO_IWEVENT(assocreqie);
PROTO_IWEVENT(auth_ap);
PROTO_IWEVENT(deauth_ap);
PROTO_IWEVENT(auth_sta);
PROTO_IWEVENT(deauth_sta);
PROTO_IWEVENT(disassoc_sta);
PROTO_IWEVENT(disassoc_ap);
PROTO_IWEVENT(wapi);

#define EVENT_IDX(x)  [ x ]

/**
 * @brief Table of functions to dispatch iw events
 */
acfg_event_t    qdf_iw_events[] = {
	EVENT_IDX(ACFG_EV_SCAN_DONE)    = acfg_iwevent_scan_done,
	EVENT_IDX(ACFG_EV_ASSOC_AP)     = acfg_iwevent_assoc_ap,
	EVENT_IDX(ACFG_EV_ASSOC_STA)    = acfg_iwevent_assoc_sta,
	EVENT_IDX(ACFG_EV_CHAN_START)   = acfg_iwevent_chan_start,
	EVENT_IDX(ACFG_EV_CHAN_END)     = acfg_iwevent_chan_end,
	EVENT_IDX(ACFG_EV_RX_MGMT)      = acfg_iwevent_rx_mgmt,
	EVENT_IDX(ACFG_EV_SENT_ACTION)  = acfg_iwevent_sent_action,
	EVENT_IDX(ACFG_EV_LEAVE_AP)     = acfg_iwevent_leave_ap,
	EVENT_IDX(ACFG_EV_GEN_IE)       = acfg_iwevent_gen_ie,
	EVENT_IDX(ACFG_EV_ASSOC_REQ_IE) = acfg_iwevent_assoc_req_ie,
	EVENT_IDX(ACFG_EV_MIC_FAIL)     = acfg_iwevent_mic_fail,
	EVENT_IDX(ACFG_EV_PROBE_REQ)    = acfg_iwevent_assocreqie,

	EVENT_IDX(ACFG_EV_AUTH_AP)    = acfg_iwevent_auth_ap,
	EVENT_IDX(ACFG_EV_DEAUTH_AP)    = acfg_iwevent_deauth_ap,
	EVENT_IDX(ACFG_EV_AUTH_STA)    = acfg_iwevent_auth_sta,
	EVENT_IDX(ACFG_EV_DEAUTH_STA)    = acfg_iwevent_deauth_sta,
	EVENT_IDX(ACFG_EV_DISASSOC_STA)    = acfg_iwevent_disassoc_sta,
	EVENT_IDX(ACFG_EV_DISASSOC_AP)    = acfg_iwevent_disassoc_ap,
	EVENT_IDX(ACFG_EV_WAPI)     = acfg_iwevent_wapi,

	EVENT_IDX(ACFG_EV_WSUPP_RAW_MESSAGE)            =
					acfg_iwevent_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_AP_STA_CONNECTED)       =
					acfg_iwevent_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_CONNECTED)    =
					acfg_iwevent_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_DISCONNECTED) =
					acfg_iwevent_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_TERMINATING)  =
					acfg_iwevent_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_SCAN_RESULTS) =
					acfg_iwevent_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPS_EVENT_ENROLLEE_SEEN) =
					acfg_iwevent_wsupp_generic,
};


#define PROTO_EVENT(name)  \
	static uint32_t   acfg_event_##name(__qdf_softc_t *, \
				acfg_ev_data_t *)

PROTO_EVENT(scan_done);
PROTO_EVENT(assoc_ap);
PROTO_EVENT(assoc_sta);
PROTO_EVENT(chan_start);
PROTO_EVENT(chan_end);
PROTO_EVENT(rx_mgmt);
PROTO_EVENT(sent_action);
PROTO_EVENT(leave_ap);
PROTO_EVENT(gen_ie);
PROTO_EVENT(assoc_req_ie);
PROTO_EVENT(wsupp_generic);


acfg_event_t   acfg_events[] = {
	EVENT_IDX(ACFG_EV_SCAN_DONE)    = acfg_event_scan_done,
	EVENT_IDX(ACFG_EV_ASSOC_AP)     = acfg_event_assoc_ap,
	EVENT_IDX(ACFG_EV_ASSOC_STA)    = acfg_event_assoc_sta,
	EVENT_IDX(ACFG_EV_CHAN_START)   = acfg_event_chan_start,
	EVENT_IDX(ACFG_EV_CHAN_END)     = acfg_event_chan_end,
	EVENT_IDX(ACFG_EV_RX_MGMT)      = acfg_event_rx_mgmt,
	EVENT_IDX(ACFG_EV_SENT_ACTION)  = acfg_event_sent_action,
	EVENT_IDX(ACFG_EV_LEAVE_AP)     = acfg_event_leave_ap,
	EVENT_IDX(ACFG_EV_GEN_IE)       = acfg_event_gen_ie,
	EVENT_IDX(ACFG_EV_ASSOC_REQ_IE) = acfg_event_assoc_req_ie,
	EVENT_IDX(ACFG_EV_WSUPP_RAW_MESSAGE)            =
					acfg_event_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_AP_STA_CONNECTED)       =
					acfg_event_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_CONNECTED)    =
					acfg_event_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_DISCONNECTED) =
					acfg_event_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_TERMINATING)  =
					acfg_event_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPA_EVENT_SCAN_RESULTS) =
					acfg_event_wsupp_generic,
	EVENT_IDX(ACFG_EV_WSUPP_WPS_EVENT_ENROLLEE_SEEN) =
					acfg_event_wsupp_generic,
};


/**
 * @brief Indicate the ACFG Eevnt to the upper layer
 *
 * @param hdl
 * @param event
 *
 * @return
 */
uint32_t
__qdf_net_indicate_event(qdf_net_handle_t   hdl, void *vevent)
{
	struct net_device *dev = NULL;
	struct sk_buff *skb;
	int err;
    struct nl_event_info ev_info = {0};

	acfg_os_event_t    *event =  (acfg_os_event_t    *) vevent;
	__qdf_softc_t   *sc = NULL;
	acfg_event_t   fn;

	qdf_trace(QDF_DEBUG_FUNCTRACE,"QDF EVENT Received : id - %d \n",
					event->id);
	if (hdl != NULL) {
		dev = hdl_to_netdev(hdl);
		if (!net_eq(dev_net(dev), &init_net))
			return QDF_STATUS_E_FAILURE;
	}

	skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
	if (!skb)
		return QDF_STATUS_E_NOMEM;

	ev_info.type    = RTM_NEWLINK;
	ev_info.ev_type = TYPE_SENT_EVENT;
	ev_info.ev_len  = sizeof(acfg_os_event_t);
	ev_info.pid     = 0;
	ev_info.seq     = 0;
	ev_info.flags   = 0;
	ev_info.event   = event;

	err = nl_ev_fill_info(skb, dev, &ev_info);
	if (err < 0) {
		kfree_skb(skb);
		return QDF_STATUS_E_FAILURE;
	}

	NETLINK_CB(skb).dst_group = RTNLGRP_LINK;

	/* Send event to acfg */
	rtnl_notify(skb, &init_net, 0, RTNLGRP_NOTIFY, NULL, GFP_ATOMIC);

	/* Send iw event */
	if(hdl != NULL) {
		sc = hdl_to_softc(hdl);
		fn = qdf_iw_events[event->id];
		fn(sc, &event->data);
	}

	return QDF_STATUS_SUCCESS ;
}
EXPORT_SYMBOL(__qdf_net_indicate_event);

uint32_t
__qdf_send_custom_wireless_event(qdf_net_handle_t   hdl, void  *event)
{
	struct net_device *dev = NULL;
	union iwreq_data wreq = {{0}};
	char buf[200] = {0};

	if (hdl != NULL)
		dev = hdl_to_netdev(hdl);

	if (dev == NULL)
		return QDF_STATUS_E_FAILURE;

	memcpy(buf, (char *)event, strlen(event));
	wreq.data.length = strlen(buf);
	wireless_send_event(dev, IWEVCUSTOM, &wreq, buf);

	return QDF_STATUS_SUCCESS;
}
EXPORT_SYMBOL(__qdf_send_custom_wireless_event);

static const char*
__qdf_net_ether_sprintf(const uint8_t *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

/**
 * @brief Send ACFG scan done event
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_scan_done(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	return QDF_STATUS_SUCCESS;
}


/**
 * @brief Send MIC Failure event through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_mic_fail(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	union iwreq_data wreq = {{0}};
	char buf[200] = {0};

	acfg_wsupp_custom_message_t *custom =
			(acfg_wsupp_custom_message_t *)data;

	if(qdf_str_len(custom->raw_message) < sizeof(buf))
		qdf_mem_copy(buf, custom->raw_message, qdf_str_len(custom->raw_message));
	else
		return QDF_STATUS_E_FAILURE;

	buf[qdf_str_len(custom->raw_message)] = '\0';
	wreq.data.length = qdf_str_len(buf);
	/* dispatch wireless event indicating MIC Failure */
	wireless_send_event(hdl_to_netdev(sc), IWEVCUSTOM, &wreq, buf);

	return QDF_STATUS_SUCCESS;
}


/**
 * @brief Send Probe Request frame through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_assocreqie(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};
	char *buf;
	int bufsize;

	acfg_probe_req_t *pr_req = (acfg_probe_req_t *)data;

	bufsize = pr_req->len;
	buf = qdf_mem_malloc(bufsize);
	if (buf == NULL) return -ENOMEM;
	memset(buf, 0, bufsize);
	wreq.data.length = bufsize;
	memcpy(buf, pr_req->data, pr_req->len);

	/*
	 * IWEVASSOCREQIE is HACK for IWEVCUSTOM to overcome 256 bytes limitation
	 *
	 * dispatch wireless event indicating probe request frame
	 */
	wireless_send_event(hdl_to_netdev(sc), IWEVASSOCREQIE, &wreq, buf);

	qdf_mem_free(buf);
	return QDF_STATUS_SUCCESS;
}


/**
 * @brief Send scan done through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_scan_done(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	union iwreq_data wreq = {{0}};

	/* dispatch wireless event indicating scan completed */
	wireless_send_event(hdl_to_netdev(sc), SIOCGIWSCAN, &wreq, NULL);

	return QDF_STATUS_SUCCESS;
}


/**
 * @brief Send ACFG Assoc with AP event
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_assoc_ap(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief Send Assoc with AP event through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_assoc_ap(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	memcpy(wreq.addr.sa_data, data->assoc_ap.bssid, ACFG_MACADDR_LEN);
	wreq.addr.sa_family = ARPHRD_ETHER;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: Station Joined = %s\n",
			__qdf_net_ether_sprintf(wreq.addr.sa_data));

	wireless_send_event(hdl_to_netdev(sc), IWEVREGISTERED, &wreq, NULL);

	return QDF_STATUS_SUCCESS;
}


/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_assoc_sta(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief Send Sta join through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_assoc_sta(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	union iwreq_data wreq = {{0}};

	memcpy(wreq.addr.sa_data, data->assoc_sta.bssid, ACFG_MACADDR_LEN);
	wreq.addr.sa_family = ARPHRD_ETHER;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: Associated to AP = %s\n",
		__qdf_net_ether_sprintf(wreq.addr.sa_data));

	wireless_send_event(hdl_to_netdev(sc), SIOCGIWAP, &wreq, NULL);

	return QDF_STATUS_SUCCESS;
}


static uint32_t
acfg_iwevent_auth_ap(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

static uint32_t
acfg_iwevent_deauth_ap(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

static uint32_t
acfg_iwevent_auth_sta(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}


static uint32_t
acfg_iwevent_deauth_sta(__qdf_softc_t *sc, acfg_ev_data_t  *data)
{
	return QDF_STATUS_SUCCESS;
}


static uint32_t
acfg_iwevent_disassoc_sta(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	return QDF_STATUS_SUCCESS;
}


static uint32_t
acfg_iwevent_disassoc_ap(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief Send WAPI event through IW
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_wapi(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	union iwreq_data wreq = {{0}};
	char *buf;
	int  bufsize;
#define WPS_FRAM_TAG_SIZE 30

	acfg_wsupp_custom_message_t *custom =
			(acfg_wsupp_custom_message_t *)data;

	bufsize = sizeof(acfg_wsupp_custom_message_t) + WPS_FRAM_TAG_SIZE;
	buf = qdf_mem_malloc(bufsize);
	if (buf == NULL) return -ENOMEM;
	memset(buf, 0, bufsize);
	wreq.data.length = bufsize;
	memcpy(buf, custom->raw_message, sizeof(acfg_wsupp_custom_message_t));

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: WAPI \n");
	/*
	 * IWEVASSOCREQIE is HACK for IWEVCUSTOM to overcome 256 bytes
	 * limitation dispatch wireless event indicating probe request frame
	 */
	//wireless_send_event(hdl_to_netdev(sc), IWEVCUSTOM, &wreq, buf);
	wireless_send_event(hdl_to_netdev(sc), IWEVASSOCREQIE, &wreq, buf);

	qdf_mem_free(buf);

return QDF_STATUS_SUCCESS;
}



/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_wsupp_generic(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_wsupp_generic(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_chan_start(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_chan_end(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_rx_mgmt(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_sent_action(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_leave_ap(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_gen_ie(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_event_assoc_req_ie(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_chan_start(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	wreq.data.flags = IEEE80211_EV_CHAN_START;
	wreq.data.length = sizeof(acfg_chan_start_t);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: CHAN START\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVCUSTOM,
			&wreq, (void *)&data->chan_start);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_chan_end(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	wreq.data.flags = IEEE80211_EV_CHAN_END;
	wreq.data.length = sizeof(acfg_chan_end_t);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: CHAN END\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVCUSTOM, &wreq,
				(void *)&data->chan_end);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_rx_mgmt(__qdf_softc_t   *sc, acfg_ev_data_t    *data)
{
	union iwreq_data wreq = {{0}};

	wreq.data.flags = IEEE80211_EV_RX_MGMT;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: RX MGMT\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVCUSTOM, &wreq, NULL);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_sent_action(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	wreq.data.flags = IEEE80211_EV_P2P_SEND_ACTION_CB;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: SENT ACTION\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVCUSTOM, &wreq, NULL);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_leave_ap(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	wreq.addr.sa_family = ARPHRD_ETHER;
	qdf_mem_copy(wreq.addr.sa_data, data->leave_ap.mac, ETH_ALEN);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: LEAVE AP\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVEXPIRED, &wreq, NULL);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_gen_ie(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	wreq.data.length = data->gen_ie.len;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: GEN IE\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVGENIE,
			&wreq, (void *)data->gen_ie.data);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
acfg_iwevent_assoc_req_ie(__qdf_softc_t *sc, acfg_ev_data_t *data)
{
	union iwreq_data wreq = {{0}};

	wreq.data.length = data->gen_ie.len;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "QDF_EVENT: ASSOC REQ IE\n");

	wireless_send_event(hdl_to_netdev(sc), IWEVASSOCREQIE, &wreq,
				 (void *)data->gen_ie.data);

	return QDF_STATUS_SUCCESS;
}
