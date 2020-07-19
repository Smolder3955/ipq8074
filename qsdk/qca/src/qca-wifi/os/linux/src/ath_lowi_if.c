/*
 * Copyright (c) 2016, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "linux/if.h"
#include "linux/socket.h"
#include "linux/netlink.h"
#include <net/sock.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/cache.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include "sys/queue.h"
#include <ieee80211_var.h>

#if ATH_SUPPORT_LOWI
#include "ath_lowi_if.h"

struct lowi_if_netlink *lowi_if_nl = NULL;

/**
 * @brief Send a unicast netlink message to LOWI user application.
 *
 * @param  skb  data for LOWI application
 */
void ath_lowi_if_netlink_send(void *buf, int len)
{
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh;
    u_int8_t *nldata = NULL;
    u_int32_t pid;

    if (!lowi_if_nl || !buf) {
        return;
    }

    pid = lowi_if_nl->lowi_if_pid;
    if (WLAN_DEFAULT_LOWI_IF_NETLINK_PID == pid) {
        // No user space daemon has requested events yet, so drop it.
        return;
    }

    skb = nlmsg_new(len, GFP_ATOMIC);
    if (!skb) {
        qdf_print("%s: No memory for skb", __func__);
        return;
    }

    nlh = nlmsg_put(skb, pid, 0, 0, len, 0);
    if (!nlh) {
        qdf_print("%s: nlmsg_put() failed", __func__);
        kfree_skb(skb);
        return;
    }

    nldata = NLMSG_DATA(nlh);
    memcpy(nldata, buf, len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    NETLINK_CB(skb).pid = 0;        /* from kernel */
#else
    NETLINK_CB(skb).portid = 0;     /* from kernel */
#endif
    NETLINK_CB(skb).dst_group = 0;  /* unicast */
    netlink_unicast(lowi_if_nl->lowi_if_sock, skb, pid, MSG_DONTWAIT);
}

/**
 * @brief Handle a netlink message sent from LOWI user space application
 *
 * @param 2
 * @param 6
 * @param struct sk_buff *__skb  the buffer containing the data to receive
 * @param len  the length of the data to receive
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
static void ath_lowi_if_netlink_receive(struct sk_buff *__skb)
#else
static void ath_lowi_if_netlink_receive(struct sock *sk, int len)
#endif
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh = NULL;
    u_int8_t *data = NULL;
    u_int32_t pid;

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
    if ((skb = skb_get(__skb)) != NULL){
#else
    if ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
#endif
        /* process netlink message pointed by skb->data */
        nlh = (struct nlmsghdr *)skb->data;
        pid = nlh->nlmsg_pid;
        data = NLMSG_DATA(nlh);
        lowi_if_nl->lowi_if_pid = pid;
        /* Send netlinlk msg for processing */
        ath_lowi_if_processmsg((void *) data);
        kfree_skb(skb);
    }
}
/**
  * @brief Initialize the netlink socket for the LOWI interface driver module.
  *
  * @return  0 on success; -ENODEV on failure
  */
int ath_lowi_if_netlink_init(void)
{

#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,10,0)
    extern struct net init_net;
    struct netlink_kernel_cfg cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.groups = 1;
    cfg.input = &ath_lowi_if_netlink_receive;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    extern struct net init_net;

    struct netlink_kernel_cfg cfg = {
        .groups = 1,
        .input  = ath_lowi_if_netlink_receive,
    };
#endif

    if (!lowi_if_nl) {
        lowi_if_nl = (struct lowi_if_netlink *)
            qdf_mem_malloc(sizeof(struct lowi_if_netlink));
        if (!lowi_if_nl) {
            return -ENODEV;
        }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)/* >= (3,10,0) */
        lowi_if_nl->lowi_if_sock = (struct sock *)netlink_kernel_create(
                                                                              &init_net,
                                                                              NETLINK_LOWI_IF_EVENT,
                                                                              &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
        lowi_if_nl->lowi_if_sock = (struct sock *)netlink_kernel_create(
                                                                              &init_net,
                                                                              NETLINK_LOWI_IF_EVENT,
                                                                              THIS_MODULE, &cfg);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
        lowi_if_nl->lowi_if_sock = (struct sock *) netlink_kernel_create(
                                                                              &init_net,
                                                                              NETLINK_LOWI_IF_EVENT, 1,
                                                                              &ath_lowi_if_netlink_receive,
                                                                              NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,22)
        lowi_if_nl->lowi_if_sock = (struct sock *) netlink_kernel_create(
                                                                              NETLINK_LOWI_IF_EVENT, 1,
                                                                              &ath_lowi_if_netlink_receive,
                                                                              NULL, THIS_MODULE);

#else
        lowi_if_nl->lowi_if_sock = (struct sock *)netlink_kernel_create(
                                                                             NETLINK_LOWI_IF_EVENT, 1,
                                                                             &ath_lowi_if_netlink_receive,
                                                                             THIS_MODULE);
#endif
        if (!lowi_if_nl->lowi_if_sock) {
            qdf_mem_free(lowi_if_nl);
            lowi_if_nl = NULL;
            qdf_print("%s NETLINK_KERNEL_CREATE FAILED", __func__);
            return -ENODEV;
        }
        atomic_set(&lowi_if_nl->lowi_if_refcnt, 1);
        lowi_if_nl->lowi_if_pid = WLAN_DEFAULT_LOWI_IF_NETLINK_PID;
        qdf_print("%s LOWI Netlink successfully created", __func__);
    } else {
        atomic_inc(&lowi_if_nl->lowi_if_refcnt);
        qdf_print("%s Incremented LOWI netlink ref count: %d", __func__, atomic_read(&lowi_if_nl->lowi_if_refcnt));
    }
    return 0;
}
/**
  * @brief Close and destroy the netlink socket for the LOWI interface driver module.
  *
  * @return 0 on success; non-zero on failure
  */
int ath_lowi_if_netlink_delete(void)
{
    if (!lowi_if_nl) {
        qdf_print("%s lowi_if_nl is NULL", __func__);
        return -ENODEV;
    }
    qdf_print("%s Going to decrement current LOWI netlink ref count: %d", __func__, atomic_read(&lowi_if_nl->lowi_if_refcnt));
    if (!atomic_dec_return(&lowi_if_nl->lowi_if_refcnt)) {
        if (lowi_if_nl->lowi_if_sock) {
	    netlink_kernel_release(lowi_if_nl->lowi_if_sock);
        }
        qdf_mem_free(lowi_if_nl);
        lowi_if_nl = NULL;
    }
    return 0;
}


/* Function     : ath_lowi_if_processmsg
 * Arguments    : data from the nl socket skb
 * Functionality: Process LOWI frame, and call appropriate functionality
 * Return       : Void
 */
void ath_lowi_if_processmsg(void *data)
{
    struct ieee80211com *ic = NULL;
    struct ani_header *ani_hdr;
    char *buf;
    void *req;
    unsigned int  msg_len;
    int msgsubType, req_id;
    wmi_host_channel wmi_ch;
    int first_radio = 1;

    /** NOTE: LOWI is supported on offload only. LOWI host interface is not tied to radio or vap **/
    /** HOST finds correct pdev (FW instance) to send message by looking into LOWI message **/
    /** For e.g. Measurement Request is send to radio that support channel in LOWI request **/
    /** Radio/VAP for FTMRR, Where are you frame are selected where STA MAC is associated **/
    /** LCI/LCR are sent to any radio that have at least one VAP enabled **/

    ic = ath_lowi_if_get_ic(&first_radio, NULL, NULL);
    if(!ic){
        qdf_print("%s: ERROR: No offload-capable radio with active VAP was found. Dropping LOWI message", __func__);
        return;
    }

    /* Process message */
    ani_hdr = (struct ani_header *)data;
    buf = (char *)ani_hdr;
    msg_len = ani_hdr->msg_length;

    /* Process packets based on their types */
    switch(ani_hdr->msg_type)
    {
        case ANI_MSG_APP_REG_REQ:
            /* Send registration response message to Lowi */
            ath_lowi_if_nlsend_reg_resp(ic);
            return;

        case ANI_MSG_OEM_DATA_REQ:
            req = (void *)(buf + ANI_HDR_LEN);
            /* Check msgsubType */
            memcpy(&msgsubType, req, sizeof(msgsubType));
            switch(msgsubType) {
                case TARGET_OEM_CAPABILITY_REQ:
                    /* Send this request to FW */
                    if(ic->ic_lowi_frame_send)
                       ic->ic_lowi_frame_send(ic, msg_len, req, msgsubType);
                    return;

                case TARGET_OEM_MEASUREMENT_REQ:
                    /* Check the channel info in the message */
                    /* Msmt frame: msgtype(4), reqId(4), numBSSID(4), chanInfo(24) */
                    memcpy(&wmi_ch, req + LOWI_MESSAGE_CHANNEL_INFO_POSITION, sizeof(wmi_ch));
                    qdf_print("%s: Received RTT measurement request from LOWI ", __func__);
                    /* Find radio that supports this channel */
                    ic = ath_lowi_if_get_ic(NULL, &wmi_ch.mhz, NULL);
                    if (!ic) {
                            qdf_print("%s: ERROR: No Radio Found supporting channel (%d mhz). Dropping ranging request.", __func__, wmi_ch.mhz);
                        return;
                    }
                    /* Send message to FW */
                    if(ic->ic_lowi_frame_send)
                       ic->ic_lowi_frame_send(ic, msg_len, req, msgsubType);
                    return;

                case TARGET_OEM_CONFIGURE_LCR:
                case TARGET_OEM_CONFIGURE_LCI:
                    qdf_print("%s: Received LCI/LCR request from LOWI ", __func__);
                    /* Send this to first vap-enabled radio (i.e. FW instance) */
                    if(ic->ic_lowi_frame_send)
                       ic->ic_lowi_frame_send(ic, msg_len, req, msgsubType);
                    return;

                case TARGET_OEM_CONFIGURE_WRU:
                    qdf_print("%s: Received Where Are You frame request from LOWI ", __func__);
                    ieee80211_lowi_send_wru_frame(req + LOWI_MESSAGE_WRU_POSITION); //remove subtype(4),req_id(4)
                    req_id = *((u_int8_t *)req+LOWI_MESSAGE_REQ_ID_POSITION);
                    ath_lowi_if_send_report_resp(ic, req_id, req+LOWI_MESSAGE_WRU_POSITION, LOWI_LCI_REQ_WRU_OK);
                    return;

                case TARGET_OEM_CONFIGURE_FTMRR:
                    qdf_print("%s: Received FTMRR frame request from LOWI ", __func__);
                    ieee80211_lowi_send_ftmrr_frame(req + LOWI_MESSAGE_FTMRR_POSITION); //remove subtype(4),req_id(4)
                    req_id = *((u_int8_t *)req+LOWI_MESSAGE_REQ_ID_POSITION);
                    ath_lowi_if_send_report_resp(ic, req_id, req+LOWI_MESSAGE_FTMRR_POSITION, LOWI_FTMRR_OK);
                    return;

                default:
                    qdf_print("%s: ERROR: UNKNOWN message sub-type received for ANI_MSG_OEM_DATA_REQ lowi message", __func__);
                    return;
            }

        case(ANI_MSG_CHANNEL_INFO_REQ):
            /*Send channel info response message to Lowi */
            ath_lowi_if_nlsend_chinfo_resp(ic);
            return;

        default:
            qdf_print("%s: ERROR: UNKNOWN LOWI message received", __func__);
    }
}


/* Prepares and sends Error Report Response frame to LOWI with provided response code */
/* Function     : ath_lowi_if_send_report_resp
 * Arguments    : ic, request id, dest mac, error code
 * Functionality: Prepares and sends error report response frame to Lowi server
 * Return       : void
 */
void ath_lowi_if_send_report_resp(struct ieee80211com *ic, int req_id, u_int8_t* dest_mac, int err_code)
{
    struct ani_err_msg_report err_report;
    char *buf;

    /* Set msg type and length */
    err_report.msg_type = ANI_MSG_OEM_DATA_RSP;
    err_report.msg_length = ANI_ERR_REP_RESP_LEN; //20
    err_report.msg_subtype = TARGET_OEM_ERROR_REPORT_RSP;
    err_report.req_id = req_id;
    memcpy(&err_report.dest_mac, dest_mac, IEEE80211_ADDR_LEN);
    err_report.err_code = err_code;

    /* Allocate buffer for entire message including header */
    buf = OS_MALLOC(ic->ic_osdev, sizeof(struct ani_err_msg_report), GFP_KERNEL);
    if (buf == NULL) {
        return;
    }

    /* Copy message */
    memcpy(buf, &err_report, sizeof(struct ani_err_msg_report));
    ath_lowi_if_netlink_send(buf, sizeof(struct ani_err_msg_report));
    OS_FREE(buf);
    return;
}

/* Function     : ath_lowi_if_nlsend_reg_resp
 * Arguments    : Radio struct
 * Functionality: Send registration response message
 * Return       : Void
 */
void ath_lowi_if_nlsend_reg_resp(struct ieee80211com *ic)
{
    struct ani_header ani_hdr;
    struct ani_reg_resp rresp;
    struct ani_intf_info *intf_info;
    int num_intf, i;
    char *buf;

    /* Currently assuming one vap. This info is sufficient for registration */
    num_intf = 1;

    /* Set msg type and length */
    ani_hdr.msg_type = ANI_MSG_APP_REG_RSP;
    ani_hdr.msg_length = ANI_REG_RESP_LEN + (num_intf * ANI_INTF_INFO_LEN);
    rresp.num_intf = num_intf;
    /* Allocate buffer for entire message including header */
    buf = OS_MALLOC(ic->ic_osdev, ANI_HDR_LEN + ani_hdr.msg_length, GFP_KERNEL);
    if (buf == NULL) {
        return;
    }

    /* Copy header and reg resp */
    memcpy(buf, &ani_hdr, ANI_HDR_LEN);
    memcpy(((char *)buf + ANI_HDR_LEN), &rresp, ANI_REG_RESP_LEN);
    /* Point to start of intf_info part of resp msg */
    intf_info = (struct ani_intf_info *)(((char *)buf) + ANI_HDR_LEN + ANI_REG_RESP_LEN);
    /* For each intf (vap), fill the info */
    for(i=0; i<num_intf; i++) {
        intf_info->intf_id = INTF_ID_FOR_SOFT_AP;
        intf_info->vdev_id = 0; /* TBD: Find correct vdev */
        intf_info++;
    }
    qdf_print("LOWI is now up, registration completed");
    ath_lowi_if_netlink_send(buf, (ANI_HDR_LEN + ANI_REG_RESP_LEN + (num_intf * ANI_INTF_INFO_LEN)));
    OS_FREE(buf);
}


/* Function     : ath_lowi_if_nlsend_response
 * Arguments    : ic, data from FW, datalen, msgType, msgSubtype
 * Functionality: Sends response received from FW to Lowi by adding ANI header
 * Return       : Void
 */
void ath_lowi_if_nlsend_response(struct ieee80211com *ic,
        u_int8_t *data, u_int16_t datalen, u_int8_t msgtype, u_int32_t msg_subtype, u_int8_t error_code)
{
    struct ani_header_rsp ani_hdr;
    char *buf;

    /* Set msg type and length */
    ani_hdr.msg_type = msgtype;
    ani_hdr.msg_length = datalen + LOWI_MESSAGE_MSGSUBTYPE_LEN; //HOST needs to add msgsubtype
    ani_hdr.msg_subtype = msg_subtype;

    if(msg_subtype == TARGET_OEM_ERROR_REPORT_RSP) //Error report response
    {
        qdf_print("%s: Error report recevied from FW: Error Code: %d", __func__, error_code);
    }
    /* Allocate buffer for entire message including header */
    buf = OS_MALLOC(ic->ic_osdev, ANI_HDR_LEN_RSP + datalen, GFP_KERNEL);
    if (buf == NULL) {
        return;
    }

    /* Copy header and resp */
    memcpy(buf, &ani_hdr, ANI_HDR_LEN_RSP);
    memcpy(((char *)buf + ANI_HDR_LEN_RSP), data, datalen);
    ath_lowi_if_netlink_send(buf, (ANI_HDR_LEN_RSP + datalen));
    OS_FREE(buf);
}
EXPORT_SYMBOL(ath_lowi_if_nlsend_response);

/* Function     : ath_lowi_if_nlsend_chinfo_resp
 * Arguments    : Radio struct
 * Functionality: Sends all supported channels on all radios to LOWI application
 * Return       : Void
 */
void ath_lowi_if_nlsend_chinfo_resp(struct ieee80211com *ic)
{
    struct ani_header ani_hdr;
    struct ani_chinfo_resp chresp;
    char *buf, *bufptr;
    int num_channels;

    //Allocate buf for MAX CHANNELS + ANI header
    buf = OS_MALLOC(ic->ic_osdev,
                      ANI_HDR_LEN +
                      (ANI_CHINFO_RESP_LEN +
                      (IEEE80211_CHINFO_MAX_CHANNELS * ANI_CH_INFO_LEN)),
                      GFP_KERNEL);
    if (buf == NULL) {
        return;
    }

    /* Point to start of ch_info part of resp msg */
    bufptr = (((char *)buf) + ANI_HDR_LEN + ANI_CHINFO_RESP_LEN);
    num_channels = 0;

    /* TBD: Add osif call to provide all supported channels on all radios */
    /* Set msg type and msg length based on num channels added */
    ani_hdr.msg_type = ANI_MSG_CHANNEL_INFO_RSP;
    ani_hdr.msg_length = ANI_CHINFO_RESP_LEN + (num_channels * ANI_CH_INFO_LEN);
    chresp.num_chids = num_channels;

    /* Copy header and chinfo resp */
    memcpy(buf, &ani_hdr, ANI_HDR_LEN);
    memcpy(((char *)buf + ANI_HDR_LEN), &chresp, ANI_CHINFO_RESP_LEN);

    ath_lowi_if_netlink_send(buf, (ANI_HDR_LEN + ANI_CHINFO_RESP_LEN + (num_channels * ANI_CH_INFO_LEN)));
    OS_FREE(buf);
}


/* Function to retrieve radio (ic) based on matching preference */
/* One one of the arguments is expected to have value, others will be NULL */
/* Any: reutrn first radio with valid vap, Channel: radio with supporting channel */
/* Mac: radio which has this mac address in associated station list */
struct ieee80211com* ath_lowi_if_get_ic(void *any, int* channel_mhz, char* sta_mac)
{
    struct ieee80211com *ic = NULL;
    //struct ol_ath_softc_net80211 *scn = NULL;
    ol_ath_soc_softc_t *soc;
    int soc_idx, scn_idx;
    struct ieee80211_ath_channel* c;
    int j;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_peer *peer;

    /* Iterate through all radios */
    for (soc_idx = 0; soc_idx < GLOBAL_SOC_SIZE; soc_idx++) {
        soc = ol_global_soc[soc_idx];
        if (soc == NULL)
            continue;
        for (scn_idx = 0; scn_idx < wlan_psoc_get_pdev_count(soc->psoc_obj); scn_idx++) {
            pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
            if(pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                 continue;
            }

            ic = wlan_pdev_get_mlme_ext_obj(pdev);
            if(!ic) {
                 qdf_print("%s: ic is NULL (id: %d) ", __func__, scn_idx);
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                 continue;
            }

            /* Skip Direct Attach radios */
            if(!ic->ic_is_mode_offload(ic)) {
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                continue;
            }
            /* Consider this radio only if it has at least 1 active vap */
            if(osif_get_num_active_vaps(ic) > 0)
            {
                /*If 'any' argument is non-null*/
                if(any){
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                    return ic;
                }
                if(channel_mhz){
                    /* Go through supported channels for this radio and match with freq */
                    for(j=0; j<ic->ic_nchans; j++){
                        c = &ic->ic_channels[j];
                        if(c->ic_freq == (u_int16_t) *channel_mhz){
                            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                                 IEEE80211_MSG_WIFIPOS,
                                 "%s: Found matching freq (%d mhz) on scn_idx: %d\n",
                                 __func__, *channel_mhz, scn_idx);
                            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                            return ic;
                        }
                    }
                }
             }
             if(sta_mac) {
                   peer = wlan_objmgr_get_peer(soc->psoc_obj, scn_idx, (uint8_t
*)sta_mac,
                                                       WLAN_MLME_NB_ID);
                   if (peer != NULL) {
                       /* Skip Direct Attach radios */
                       /* Consider this radio only if it has at least 1 active vap */
                       if (ic->ic_is_mode_offload(ic) && (osif_get_num_active_vaps(ic) > 0)) {
                           wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
                           wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                           return ic;
                       }
                       wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
                    }
             }
             wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        }
    } /* for loop */

    /* Reached here, no match was found */
    return NULL;
}

EXPORT_SYMBOL(ath_lowi_if_netlink_send);
EXPORT_SYMBOL(ath_lowi_if_netlink_init);
EXPORT_SYMBOL(ath_lowi_if_netlink_delete);

#endif
