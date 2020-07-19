/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2009-2010, Atheros Communications
 * implements glue code to bind P2P protpcol to P2P high level API.
 * WARNING: synchronization issues are not handled yet.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include "ieee80211_options.h"  /* include the compile time options */

#if UMAC_SUPPORT_P2P_KERNEL
#include "includes.h"
#include "common.h"
#include "p2p.h"
#include "ieee80211_p2p_device_priv.h"


struct reg_class_channel_map
{
    u_int32_t classid;
    u_int32_t num_channels;
    u_int32_t *channels;
};

/*
 * hard coded regulatory class array for USA.
 */
static u_int32_t usa_class_12_channels[] = {1,2,3,4,5,6,7,8,9,10,11};
static struct reg_class_channel_map usa_reg_class_channel_map[] =
{ {12, sizeof(usa_class_12_channels)/sizeof(u_int32_t),usa_class_12_channels} };

#define DEFAULT_COUNTRY "US"
#define WFA_OUI_BYTES  0x00,0x50,0xf2,0x04



/* p2p_scan - Request a P2P scan/search
 * @ctx: Callback context from cb_ctx
 * @full_scan: Whether this is a full scan of all channels
 * @freq: Specific frequency (MHz) to scan or 0 for no restriction
 * Returns: 0 on success, -1 on failure
 *
 * This callback function is used to request a P2P scan or search
 * operation to be completed. If full_scan is requested (non-zero),
 * this is request for the initial scan to find group owners from all
 * channels. If full_scan is not requested (zero), this is a request
 * for search phase scan of the social channels. In that case, non-zero
 * freq value can be used to indicate only a single social channel to
 * scan for (the known listen channel of the intended peer).
 *
 * The scan results are returned after this call by calling
 * p2p_scan_res_handler() for each scan result that has a P2P IE and
 * then calling p2p_scan_res_handled() to indicate that all scan
 * results have been indicated.
 */
static int
p2pkm_p2p_scan(void *ctx, int full_scan, int freq)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    return ieee80211_p2p_scan(p2p_handle,full_scan,freq,IEEE80211_P2P_SCAN_REQ_P2PKERNEL_SCAN );
}


/**
 * send_action - Transmit an Action frame
 * @ctx: Callback context from cb_ctx
 * @freq: Frequency in MHz for the channel on which to transmit
 * @dst: Destination MAC address
 * @src: Source MAC address
 * @bssid: BSSID
 * @buf: Frame body (starting from Category field)
 * @len: Length of buf in octets
 * Returns: 0 on success, -1 on failure
 *
 * The Action frame may not be transmitted immediately and the status
 * of the transmission must be reported by calling
 * p2p_send_action_cb() once the frame has either been transmitted or
 * it has been dropped due to excessive retries or other failure to
 * transmit.
 */
static int
p2pkm_send_action(void *ctx, unsigned int freq, const u8 *dst,
		  const u8 *src, const u8 *bssid, const u8 *buf, size_t len)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);

    if (ieee80211_p2p_send_action_frame(p2p_handle, freq,
                                   NULL,dst,src,bssid,buf,len) != 0) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d send_action failed \n",__func__, __LINE__);
        return -1;
    }
    /* successfully queued the action frame to HW */

    return 0;
}

/**
 * send_action_done - Notify that Action frame sequence was completed
 * @ctx: Callback context from cb_ctx
 *
 * This function is called when the Action frame sequence that was
 * started with send_action() has been completed, i.e., when there is
 * no need to wait for a response from the destination peer anymore.
 */
static void
p2pkm_send_action_done(void *ctx)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    /* p2p kerenel is done with action frames, cancel any pending  action frame scans */ 
    ieee80211_p2p_stop_send_action_scan(p2p_handle);
}

/**
 * send_probe_resp - Transmit a Probe Response frame
 * @ctx: Callback context from cb_ctx
 * @buf: Probe Response frame (including the header and body)
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to reply to Probe Request frames that were
 * indicated with a call to p2p_probe_req_rx(). The response is to be
 * sent on the same channel or to be dropped if the driver is not
 * anymore listening to Probe Request frames.
 */
static int
p2pkm_send_probe_resp(void *ctx, const struct wpabuf *buf)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wbuf_t wbuf;
    u_int8_t *frm,*data;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    if (p2p_handle->p2p_inprogress_scan_req_type != IEEE80211_P2P_SCAN_REQ_P2PKERNEL_LISTEN ) {
        return  -EINVAL; /* no p2p listen in progress */
    }
    if ( buf->used  >= MAX_TX_RX_PACKET_SIZE) {
        return  -ENOMEM;
    }
    wbuf = wbuf_alloc(p2p_handle->p2p_oshandle, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (!wbuf) {
        return -ENOMEM;
    }

    if (buf->ext_data) {
        data = buf->ext_data;
    } else {
        data = (u_int8_t *)&buf[1]; /* data folows wpabuf */
    }
    frm = (u_int8_t *)wbuf_header(wbuf);
    OS_MEMCPY(frm,data,buf->used);

    wbuf_set_pktlen(wbuf,buf->used);

    ieee80211_send_mgmt(p2p_handle->p2p_vap,vap->iv_bss,wbuf,true);

    return 0;
}

/**
 * start_listen - Start Listen state
 * @ctx: Callback context from cb_ctx
 * @freq: Frequency of the listen channel in MHz
 * @duration: Duration for the Listen state in milliseconds
 * Returns: 0 on success, -1 on failure
 *
 * This Listen state may not start immediately since the driver may
 * have other pending operations to complete first. Once the Listen
 * state has started, p2p_listen_cb() must be called to notify the P2P
 * module.
 */
static int
p2pkm_start_listen(void *ctx, unsigned int freq,
            unsigned int duration)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    return ieee80211_p2p_start_listen(p2p_handle,freq,0,duration, IEEE80211_P2P_SCAN_REQ_P2PKERNEL_LISTEN,
                                      IEEE80211_P2P_DEFAULT_SCAN_PRIORITY);
}
/**
 * stop_listen - Stop Listen state
 * @ctx: Callback context from cb_ctx
 *
 * This callback can be used to stop a Listen state operation that was
 * previously requested with start_listen().
 */
static void
p2pkm_stop_listen(void *ctx)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    return ieee80211_p2p_stop_listen(p2p_handle);
}

/** EVENT notifications to above umac */

/**
 * dev_found - Notification of a found P2P Device
 * @ctx: Callback context from cb_ctx
 * @addr: Source address of the message triggering this notification
 * @dev_addr: P2P Device Address of the found P2P Device
 * @pri_dev_type: Primary Device Type
 * @dev_name: Device Name
 * @config_methods: Configuration Methods
 * @dev_capab: Device Capabilities
 * @group_capab: Group Capabilities
 *
 * This callback is used to notify that a new P2P Device has been
 * found. This may happen, e.g., during Search state based on scan
 * results or during Listen state based on receive Probe Request and
 * Group Owner Negotiation Request.
 */
static void
p2pkm_dev_found(void *ctx, const u8 *addr, const u8 *dev_addr,
          const u8 *pri_dev_type, const char *dev_name,
          u16 config_methods, u8 dev_capab, u8 group_capab)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_event.type =  WLAN_P2PDEV_DEV_FOUND;
    p2p_event.u.dev_found.addr = addr;
    p2p_event.u.dev_found.dev_addr = dev_addr;
    p2p_event.u.dev_found.pri_dev_type = pri_dev_type;
    p2p_event.u.dev_found.dev_name = dev_name;
    p2p_event.u.dev_found.config_methods = config_methods;
    p2p_event.u.dev_found.dev_capab = dev_capab;
    p2p_event.u.dev_found.group_capab = group_capab;

    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);
    return;
}

/**
 * go_neg_req_rx - Notification of a receive GO Negotiation Request
 * @ctx: Callback context from cb_ctx
 * @src: Source address of the message triggering this notification
 *
 * This callback is used to notify that a P2P Device is requesting
 * group owner negotiation with us, but we do not have all the
 * necessary information to start GO Negotiation. This indicates that
 * the local user has not authorized the connection yet by providing a
 * PIN or PBC button press. This information can be provided with a
 * call to p2p_connect().
 */
static void
p2pkm_go_neg_req_rx(void *ctx, const u8 *src)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_event.type =  WLAN_P2PDEV_GO_NEGOTIATION_REQ_RECVD;
    p2p_event.u.go_neg_req_recvd.src_addr = src;

    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);

    return;
}

/**
 * go_neg_completed - Notification of GO Negotiation results
 * @ctx: Callback context from cb_ctx
 * @res: GO Negotiation results
 *
 * This callback is used to notify that Group Owner Negotiation has
 * been completed. Non-zero struct p2p_go_neg_results::status indicates
 * failed negotiation. In case of success, this function is responsible
 * for creating a new group interface (or using the existing interface
 * depending on driver features), setting up the group interface in
 * proper mode based on struct p2p_go_neg_results::role_go and
 * initializing WPS provisioning either as a Registrar (if GO) or as an
 * Enrollee. Successful WPS provisioning must be indicated by calling
 * p2p_wps_success_cb().
 */
static void
p2pkm_go_neg_completed(void *ctx, struct p2p_go_neg_results *res)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_event.type =  WLAN_P2PDEV_GO_NEGOTIATION_COMPLETE;
    p2p_event.u.go_neg_results.status = res->status;
    p2p_event.u.go_neg_results.role_go = res->role_go;
    p2p_event.u.go_neg_results.freq = res->freq;
    p2p_event.u.go_neg_results.ssid = res->ssid;
    p2p_event.u.go_neg_results.ssid_len = res->ssid_len;
    p2p_event.u.go_neg_results.passphrase = res->passphrase;
    p2p_event.u.go_neg_results.peer_device_addr = res->peer_device_addr;
    p2p_event.u.go_neg_results.peer_interface_addr = res->peer_interface_addr;
    p2p_event.u.go_neg_results.wps_pbc = res->wps_pbc;
    p2p_event.u.go_neg_results.wps_pin = res->wps_pin;

    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);

    return;
}


/**
 * sd_request - Callback on Service Discovery Request
 * @ctx: Callback context from cb_ctx
 * @freq: Frequency (in MHz) of the channel
 * @sa: Source address of the request
 * @dialog_token: Dialog token
 * @update_indic: Service Update Indicator from the source of request
 * @tlvs: P2P Service Request TLV(s)
 * @tlvs_len: Length of tlvs buffer in octets
 *
 * This callback is used to indicate reception of a service discovery
 * request. Response to the query must be indicated by calling
 * p2p_sd_response() with the context information from the arguments to
 * this callback function.
 *
 * This callback handler can be set to %NULL to indicate that service
 * discovery is not supported.
 */
static void
p2pkm_sd_request(void *ctx, int freq, const u8 *sa, u8 dialog_token,
           u16 update_indic, const u8 *tlvs, size_t tlvs_len)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_event.type =  WLAN_P2PDEV_SD_REQUEST_RECEIVED;
    p2p_event.u.sd_request.freq =  freq;
    p2p_event.u.sd_request.src_addr =  sa;
    p2p_event.u.sd_request.dialog_token =  dialog_token;
    p2p_event.u.sd_request.update_indic =  update_indic;
    p2p_event.u.sd_request.tlvs =  tlvs;
    p2p_event.u.sd_request.tlvs_len =  tlvs_len;

    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);

}

/**
 * sd_response - Callback on Service Discovery Response
 * @ctx: Callback context from cb_ctx
 * @sa: Source address of the request
 * @update_indic: Service Update Indicator from the source of response
 * @tlvs: P2P Service Response TLV(s)
 * @tlvs_len: Length of tlvs buffer in octets
 *
 * This callback is used to indicate reception of a service discovery
 * response. This callback handler can be set to %NULL if no service
 * discovery requests are used.
 */
static void
p2pkm_sd_response(void *ctx, const u8 *sa, u16 update_indic,
            const u8 *tlvs, size_t tlvs_len)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_event.type =  WLAN_P2PDEV_SD_RESPONSE_RECEIVED;
    p2p_event.u.sd_response.src_addr =  sa;
    p2p_event.u.sd_response.update_indic =  update_indic;
    p2p_event.u.sd_response.tlvs =  tlvs;
    p2p_event.u.sd_response.tlvs_len =  tlvs_len;

    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);

}

static void
p2pkm_prov_disc_req(void *ctx, const u8 *peer, u16 config_methods)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n", __func__, __LINE__);
    p2p_event.type = WLAN_P2PDEV_PROV_DISC_REQ;
    p2p_event.u.prov_disc_req.peer = peer;
    p2p_event.u.prov_disc_req.config_methods = config_methods;

    ieee80211_p2p_deliver_event(p2p_handle, &p2p_event);
}

static void
p2pkm_prov_disc_resp(void *ctx, const u8 *peer, u16 config_methods)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t vap = p2p_handle->p2p_vap;
    wlan_p2p_event p2p_event;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n", __func__, __LINE__);
    p2p_event.type = WLAN_P2PDEV_PROV_DISC_RESP;
    p2p_event.u.prov_disc_resp.peer = peer;
    p2p_event.u.prov_disc_resp.config_methods = config_methods;

    ieee80211_p2p_deliver_event(p2p_handle, &p2p_event);
}

/**
 * creates an instance of p2p object .
 * @param dev_handle    : handle to the radio .
 * @param bssid         : MAC address that was previously allocated.
 *
 * @return  on success return an opaque handle to newly created p2p object.
 *          on failure returns NULL.
 */
ieee80211_p2p_kernel_t ieee80211_p2p_kernel_create(wlan_p2p_t p2p_handle, wlan_p2p_params *p2p_params)
{
    struct p2p_config p2pc;
    ieee80211_p2p_kernel_t p2p_kernel_handle;
    u_int8_t   random_byte;
    u_int32_t social_channels[IEEE80211_P2P_NUM_SOCIAL_CHANNELS] = {IEEE80211_P2P_SOCIAL_CHANNELS};
    int i,j;

    memset(&p2pc, 0, sizeof(p2pc));
   /* country - Country code to use in P2P operations */
   strncpy(p2pc.country,DEFAULT_COUNTRY,sizeof(p2pc.country));

	/* reg_class - Regulatory class for own listen channel */
    /* For now hard code the channel to US */
	p2pc.op_reg_class = p2pc.reg_class = 12; /* US/12 channels 1 to 11 , hard coded need to change */

    /* get a random byte from os */
    OS_GET_RANDOM_BYTES(&random_byte,1);

	/* op_channel - Own operational channel */
	p2pc.op_channel = social_channels[random_byte % IEEE80211_P2P_NUM_SOCIAL_CHANNELS];

    /* listen channel */
	p2pc.channel = social_channels[(random_byte+1) % IEEE80211_P2P_NUM_SOCIAL_CHANNELS];

	/* channels - Own supported regulatory classes and channels */
	/* struct p2p_channels channels; */

    p2pc.channels.reg_classes = sizeof(usa_reg_class_channel_map)/sizeof (struct reg_class_channel_map);
     for (i = 0; i < p2pc.channels.reg_classes; i++) {
        p2pc.channels.reg_class[i].reg_class = usa_reg_class_channel_map[i].classid;
	p2pc.channels.reg_class[i].channels = usa_reg_class_channel_map[i].num_channels;
        for(j=0;j<usa_reg_class_channel_map[i].num_channels; ++j) {
            p2pc.channels.reg_class[i].channel[j] = usa_reg_class_channel_map[i].channels[j];
        }
     }


	/* pri_dev_type - Primary Device Type (see WPS) */
    if (p2p_params) {
        OS_MEMCPY(p2pc.pri_dev_type,p2p_params->pri_dev_type, sizeof(p2pc.pri_dev_type));

	    /* sec_dev_type - Optional secondary device types */
        OS_MEMCPY(p2pc.sec_dev_type, p2p_params->pri_dev_type, sizeof(p2pc.sec_dev_type));
    } else {
        /* construct a default primary device type */
        char oui_bytes[] = { WFA_OUI_BYTES };

        /* pri_dev_type - Primary Device Type (see WPS) */

        /* device id 2 bytes */
        p2pc.pri_dev_type[0] = 0;
        p2pc.pri_dev_type[1] = 1;

        /* WFA OUI */
        p2pc.pri_dev_type[2] = oui_bytes[0];
        p2pc.pri_dev_type[3] = oui_bytes[1];
        p2pc.pri_dev_type[4] = oui_bytes[2];
        p2pc.pri_dev_type[5] = oui_bytes[3];

        /* Sub Category ID */
        p2pc.pri_dev_type[6] = 0;
        p2pc.pri_dev_type[7] = 1;

        /* sec_dev_type - Optional secondary device types */
        OS_MEMZERO(p2pc.sec_dev_type, sizeof(p2pc.sec_dev_type));
    }

	/* dev_addr - P2P Device Address */
    IEEE80211_ADDR_COPY(p2pc.dev_addr, wlan_vap_get_macaddr(p2p_handle->p2p_vap));

	/* dev_name - Device Name */
	p2pc.dev_name = p2p_handle->p2p_dev_name;

	/* num_sec_dev_types - Number of sec_dev_type entries */
	p2pc.num_sec_dev_types = 0;

	/* msg_ctx - Context to use with wpa_msg function */
	p2pc.msg_ctx = (void *) p2p_handle->p2p_vap;

	/* cb_ctx - Context to use with callback functions */
	p2pc.cb_ctx = p2p_handle;

	/* p2p_scan - Request a P2P scan/search */
	p2pc.p2p_scan = p2pkm_p2p_scan;

	/* send_action - Transmit an Action frame */
	p2pc.send_action = p2pkm_send_action;

	/* send_action_done - Notify that Action frame sequence was completed */
	p2pc.send_action_done = p2pkm_send_action_done;

	/* send_probe_resp - Transmit a Probe Response frame */
	p2pc.send_probe_resp = p2pkm_send_probe_resp;

	/* start_listen - Start Listen state */
	p2pc.start_listen = p2pkm_start_listen;

    /* stop_listen - Stop Listen state */
	p2pc.stop_listen = p2pkm_stop_listen;

	/* dev_found - Notification of a found P2P Device */
	p2pc.dev_found = p2pkm_dev_found;

	/* go_neg_req_rx - Notification of a receive GO Negotiation Request */
	p2pc.go_neg_req_rx = p2pkm_go_neg_req_rx;

	/* go_neg_completed - Notification of GO Negotiation results */
	p2pc.go_neg_completed = p2pkm_go_neg_completed;;

    /* sd_request - Callback on Service Discovery Request */
    p2pc.sd_request = p2pkm_sd_request;

    /* sd_response - Callback on Service Discovery Response */
    p2pc.sd_response = p2pkm_sd_response;

    p2pc.prov_disc_req = p2pkm_prov_disc_req;
    p2pc.prov_disc_resp = p2pkm_prov_disc_resp;

    /* attach the p2p kernel module */
    p2p_kernel_handle = p2p_init(&p2pc);

    if (!p2p_kernel_handle ) {
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s p2p kernel module init failed \n",__func__);
        return NULL;
    }

    return p2p_kernel_handle;
}

/**
 * delete p2p kernel object.
 * @param p2p_kernel_handle : handle to the p2p object .
 *
 */
void ieee80211_p2p_kernel_delete(ieee80211_p2p_kernel_t p2p_kernel_handle)
{
    p2p_deinit(p2p_kernel_handle);
}


/**
 * start p2p device discovery SM.
 * @param p2p_handle        : handle to the p2p object .
 * @param timeout           :  timeout in seconds (0 means no timeout).
 * @param flags             : flags to control the SM behavior.
 * @return on success returns 0. -ve otherwise.
 *  perform Device discovery , service discovery procedures based on the flags set.
 */
int wlan_p2p_start_discovery(wlan_p2p_t p2p_handle,u_int32_t timeout, u_int32_t flags)
{
    wlan_if_t vap = p2p_handle->p2p_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    return p2p_find(p2p_handle->p2p_kernel_handle, timeout);
}

int wlan_p2p_listen(wlan_p2p_t p2p_handle, u_int32_t timeout)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n", __func__, __LINE__);
    return p2p_listen(p2p_handle->p2p_kernel_handle, timeout);
}

/**
 * start p2p GO negotiation SM.
 * @param p2p_handle         : handle to the p2p object .
 * @param peer_addr          : mac address of the device to do GO negotiation
 * @param go_intent          : Local GO intent value (1..15)
 * @param own_interface_addr : Intended interface address to use with the group
 * @param force_freq         : The only allowed channel frequency in MHz or 0
 * @param pin                : PIN or NULL to indicate PBC
 * @return on success returns 0. -ve otherwise.
 *  perform GO negotiation procedure.
 */
int wlan_p2p_start_GO_negotiation(wlan_p2p_t p2p_handle,u_int8_t *peer_addr,
      int go_intent, u_int8_t *own_interface_addr, int force_freq, char *pin)
{
    wlan_if_t vap = p2p_handle->p2p_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    return p2p_connect(p2p_handle->p2p_kernel_handle, peer_addr, pin,
		       go_intent, own_interface_addr, force_freq);
}

/**
 * stop p2p SM. (stops both discovery as well as GO negotiation procedure).
 * @param p2p_handle         : handle to the p2p object .
 * @return on success returns 0. -ve otherwise.
 */
int wlan_p2p_stop(wlan_p2p_t p2p_handle)
{
    wlan_if_t vap = p2p_handle->p2p_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_stop_find(p2p_handle->p2p_kernel_handle);
    ieee80211_p2p_stop(p2p_handle); /* cancel any p2p operations */
    return 0;
}

/**
 * wlan_p2p_sd_request - Schedule a service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @param dst                : Destination peer or %NULL to apply for all peers
 * @param tlvs               : P2P Service Query TLV(s)
 * Returns                   : Reference to the query or %NULL on failure
 *
 * Response to the query is indicated with the WLAN_P2PDEV_SD_RESPONSE_RECEIVED event.
 */
wlan_p2p_sd_query_id wlan_p2p_sd_request(wlan_p2p_t p2p_handle, const u_int8_t *dst,
		      const wlan_p2p_buf *tlvs)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    struct wpabuf wpa_buf;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    wpa_buf.used = tlvs->buf_len;
    wpa_buf.size = tlvs->buf_len;
    wpa_buf.ext_data = tlvs->buf;

    return (wlan_p2p_sd_query_id) p2p_sd_request(p2p_handle->p2p_kernel_handle, dst,&wpa_buf);
}

/**
 * wlan_p2p_sd_response - Send response to a service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @param freq               : Frequency from WLAN_P2PDEV_SD_REQUEST_RECEIVED
 * @param dst                : Destination peer or %NULL to apply for all peers
 * @param resp_tlvs          : P2P Service Query TLV(s)
 * @return on success returns 0. -ve otherwise.
 *
 * This function is called as a response to the request indicated with
 * WLAN_P2PDEV_SD_REQUEST_RECEIVED.
 */
int wlan_p2p_sd_response(wlan_p2p_t p2p_handle, u_int32_t freq, const u_int8_t *dst,
                         u_int8_t dialog_token ,const wlan_p2p_buf *tlvs)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    struct wpabuf wpa_buf;

    wpa_buf.used = tlvs->buf_len;
    wpa_buf.size = tlvs->buf_len;
    wpa_buf.ext_data = tlvs->buf;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_sd_response(p2p_handle->p2p_kernel_handle,freq, dst,dialog_token,&wpa_buf);

    return 0;
}

/**
 * wlan_p2p_sd_cancel_request - Cancel a pending service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @req                      : Query reference returned from wlan_p2p_sd_request()
 */
void wlan_p2p_sd_cancel_request(wlan_p2p_t p2p_handle, wlan_p2p_sd_query_id req)
{
    wlan_if_t vap = p2p_handle->p2p_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_sd_cancel_request(p2p_handle->p2p_kernel_handle,req);
}

/**
 * wlan_p2p_sd_service_update - Indicate a change in local services
 * @param p2p_handle         : handle to the p2p object .
 *
 * This function needs to be called whenever there is a change in availability
 * of the local services. This will increment the Service Update Indicator
 * value which will be used in SD Request and Response frames.
 */
void wlan_p2p_sd_service_update(wlan_p2p_t p2p_handle)
{
    wlan_if_t vap = p2p_handle->p2p_vap;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d \n",__func__, __LINE__);
    p2p_sd_service_update(p2p_handle->p2p_kernel_handle);
}

int wlan_p2p_prov_disc_req(wlan_p2p_t p2p_handle, const u_int8_t *peer_addr,
			   u_int16_t config_methods)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n", __func__, __LINE__);
    return p2p_prov_disc_req(p2p_handle->p2p_kernel_handle, peer_addr,
			     config_methods);
}

/**
 * Flush all P2P module state.
 * @param p2p_handle        : handle to the p2p object.
 */
void wlan_p2p_flush(wlan_p2p_t p2p_handle)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n", __func__, __LINE__);
    p2p_flush(p2p_handle->p2p_kernel_handle);
}

#else
#include "ieee80211_p2p_device_priv.h"

/* DUMMY IMPLEMENTATIONS */
/**
 * start p2p device discovery SM.
 * @param p2p_handle        : handle to the p2p object .
 * @param timeout           :  timeout in seconds (0 means no timeout).
 * @param flags             : flags to control the SM behavior.
 * @return on success returns 0. -ve otherwise.
 *  perform Device discovery , service discovery procedures based on the flags set.
 */
int wlan_p2p_start_discovery(wlan_p2p_t p2p_handle,u_int32_t timeout, u_int32_t flags)
{
    return 0;
}

int wlan_p2p_listen(wlan_p2p_t p2p_handle, u_int32_t timeout)
{
    return 0;
}

/**
 * start p2p GO negotiation SM.
 * @param p2p_handle         : handle to the p2p object .
 * @param peer_addr          : mac address of the device to do GO negotiation
 * @param go_intent          : Local GO intent value (1..15)
 * @param own_interface_addr : Intended interface address to use with the group
 * @param force_freq         : The only allowed channel frequency in MHz or 0
 * @param flags              : flags to control the SM behavior.
 * @return on success returns 0. -ve otherwise.
 *  perform GO negotiation procedure.
 */
int wlan_p2p_start_GO_negotiation(wlan_p2p_t p2p_handle,u_int8_t *perr_addr,
      int go_intent, u_int8_t *own_interface_addr, int force_freq, char *pin)
{

    return 0;
}

#if 0
/**
 * stop p2p SM. (stops both discovery as well as GO negotiation procedure).
 * @param p2p_handle         : handle to the p2p object .
 * @return on success returns 0. -ve otherwise.
 */
int wlan_p2p_stop(wlan_p2p_t p2p_handle)
{
    ieee80211_p2p_stop(p2p_handle); /* cancel any p2p operations */
    return 0;
}
#endif

/**
 * wlan_p2p_sd_request - Schedule a service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @param dst                : Destination peer or %NULL to apply for all peers
 * @param tlvs               : P2P Service Query TLV(s)
 * Returns                   : Reference to the query or %NULL on failure
 *
 * Response to the query is indicated with the WLAN_P2PDEV_SD_RESPONSE_RECEIVED event.
 */
wlan_p2p_sd_query_id wlan_p2p_sd_request(wlan_p2p_t p2p_handle, const u_int8_t *dst,
		      const wlan_p2p_buf *tlvs)
{
    return (wlan_p2p_sd_query_id)0;
}

/**
 * wlan_p2p_sd_response - Send response to a service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @param freq               : Frequency from WLAN_P2PDEV_SD_REQUEST_RECEIVED
 * @param dst                : Destination peer or %NULL to apply for all peers
 * @param resp_tlvs          : P2P Service Query TLV(s)
 * @return on success returns 0. -ve otherwise.
 *
 * This function is called as a response to the request indicated with
 * WLAN_P2PDEV_SD_REQUEST_RECEIVED.
 */
int wlan_p2p_sd_response(wlan_p2p_t p2p_handle, u_int32_t freq, const u_int8_t *dst,
                         u_int8_t dialog_token ,const wlan_p2p_buf *resp_tlvs)
{
    return 0;
}

/**
 * wlan_p2p_sd_cancel_request - Cancel a pending service discovery query
 * @param p2p_handle         : handle to the p2p object .
 * @req                      : Query reference returned from wlan_p2p_sd_request()
 */
void wlan_p2p_sd_cancel_request(wlan_p2p_t p2p_handle, wlan_p2p_sd_query_id req)
{

}

/**
 * wlan_p2p_sd_service_update - Indicate a change in local services
 * @param p2p_handle         : handle to the p2p object .
 *
 * This function needs to be called whenever there is a change in availability
 * of the local services. This will increment the Service Update Indicator
 * value which will be used in SD Request and Response frames.
 */
void wlan_p2p_sd_service_update(wlan_p2p_t p2p_handle)
{

}

int wlan_p2p_prov_disc_req(wlan_p2p_t p2p_handle, const u_int8_t *peer_addr,
			   u_int16_t config_methods)
{
    return 0;
}

/**
 * Flush all P2P module state.
 * @param p2p_handle        : handle to the p2p object.
 */
void wlan_p2p_flush(wlan_p2p_t p2p_handle)
{
}

#endif
