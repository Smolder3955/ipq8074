/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 *
 */

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
#include <osdep.h>

#include <ieee80211_crypto_tkip_priv.h>

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

static	void *tkip_attach(struct ieee80211vap *, struct ieee80211_key *);
static	void tkip_detach(struct ieee80211_key *);
static	int tkip_setkey(struct ieee80211_key *);
static	int tkip_encap(struct ieee80211_key *, wbuf_t wbuf, u_int8_t keyid);
static	int tkip_enmic(struct ieee80211_key *, wbuf_t, int, bool);
static	int tkip_decap(struct ieee80211_key *, wbuf_t, int, struct ieee80211_rx_status *);
static	int tkip_demic(struct ieee80211_key *, wbuf_t, int, int, struct ieee80211_rx_status *);

const struct ieee80211_cipher tkip  = {
    "TKIP",
    IEEE80211_CIPHER_TKIP,
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
    IEEE80211_WEP_CRCLEN,
    IEEE80211_WEP_MICLEN,
    tkip_attach,
    tkip_detach,
    tkip_setkey,
    tkip_encap,
    tkip_decap,
    tkip_enmic,
    tkip_demic,
};

#define     MAX_TKIP_PN_GAP_ERR    1  /* Max. gap in TKIP PN before doing MIC sanity check */


static void *
tkip_attach(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    struct tkip_ctx *ctx;
    struct ieee80211com *ic = vap->iv_ic;

    ctx = (struct tkip_ctx *)OS_MALLOC(ic->ic_osdev, sizeof(struct tkip_ctx), GFP_KERNEL);
    if (ctx == NULL) {
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_nomem_inc(vap->vdev_obj, 1);
#endif
        return NULL;
    }

    ctx->tc_vap = vap;
    ctx->tc_ic = vap->iv_ic;
    ctx->rx_phase1_done = 0;
    ctx->tx_phase1_done = 0;
    return ctx;
}

static void
tkip_detach(struct ieee80211_key *k)
{
    struct tkip_ctx *ctx = k->wk_private;
    OS_FREE(ctx);
}

static int
tkip_setkey(struct ieee80211_key *k)
{
    struct tkip_ctx *ctx = k->wk_private;

    if (k->wk_keylen != (256/NBBY)) {
        IEEE80211_DPRINTF(ctx->tc_vap, IEEE80211_MSG_CRYPTO,
                          "%s: Invalid key length %u, expecting %u\n",
                          __func__, k->wk_keylen, 256/NBBY);
        return 0;
    }

    /* NB:  The caller must setup the keyrsc[] and keytsc */
    ctx->rx_phase1_done = 0;
    ctx->tx_phase1_done = 0;
    return 1;
}

/*
 * Add privacy headers and do any s/w encryption required.
 */
static int
tkip_encap(struct ieee80211_key *k, wbuf_t wbuf0, u_int8_t keyid)
{
    struct tkip_ctx *ctx = k->wk_private;
    struct ieee80211vap *vap = ctx->tc_vap;
    struct ieee80211com *ic = ctx->tc_ic;
    u_int8_t *ivp;
    int hdrlen;
    size_t pktlen;
    struct ieee80211_frame *wh = (struct ieee80211_frame *) wbuf_header(wbuf0);
    wbuf_t wbuf = wbuf0;
    bool istxfrag = (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
                    (((le16toh(*((u_int16_t *)&(wh->i_seq[0]))) >>
                    IEEE80211_SEQ_FRAG_SHIFT) & IEEE80211_SEQ_FRAG_MASK) > 0);
    /*
     * Handle TKIP counter measures requirement.
     */
    if (IEEE80211_VAP_IS_COUNTERM_ENABLED(vap)) {

        IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
                           "Discard frame due to countermeasures (%s)", __func__);
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_tkipcm_inc(vap->vdev_obj, 1);
#endif
        return 0;
    }

    if (vap->iv_opmode == IEEE80211_M_MONITOR) {
        /*
          For offchan encrypted TX, key's saved in mon vap.
          For 3-addr Qos frame(26 bytes) encryption, don't do 4-byte alignment,
          otherwise, the dot11 header in OTA pkts will have 2 more bytes, which's from LLC
        */
        hdrlen = ieee80211_hdrsize(wbuf_header(wbuf0));
    }else{
        hdrlen = ieee80211_hdrspace(ic, wbuf_header(wbuf0));
    }

    /*
     * Copy down 802.11 header and add the IV, KeyID, and ExtIV.
     */
#ifndef QCA_PARTNER_PLATFORM
    ivp = (u_int8_t*)wbuf_push(wbuf0, tkip.ic_header);
    wh = (struct ieee80211_frame *) wbuf_header(wbuf0); /* recompute wh */
    memmove(ivp, ivp + tkip.ic_header, hdrlen);
    ivp += hdrlen;
#else
    if (wbuf_is_encap_done(wbuf0)) {
        ivp = (u_int8_t *)wbuf_header(wbuf0);
    } else {
        ivp = (u_int8_t*)wbuf_push(wbuf0, tkip.ic_header);
        wh = (struct ieee80211_frame *) wbuf_header(wbuf0); /* recompute wh */
        memmove(ivp, ivp + tkip.ic_header, hdrlen);
    }
    ivp += hdrlen;
#endif
    if(ieee80211com_has_pn_check_war(ic))
        wbuf_set_cboffset(wbuf, hdrlen);
    ivp[0] = k->wk_keytsc >> 8;		/* TSC1 */
    ivp[1] = (ivp[0] | 0x20) & 0x7f;	/* WEP seed */
    ivp[2] = k->wk_keytsc >> 0;		/* TSC0 */
    ivp[3] = keyid | IEEE80211_WEP_EXTIV;	/* KeyID | ExtID */
    ivp[4] = k->wk_keytsc >> 16;		/* TSC2 */
    ivp[5] = k->wk_keytsc >> 24;		/* TSC3 */
    ivp[6] = k->wk_keytsc >> 32;		/* TSC4 */
    ivp[7] = k->wk_keytsc >> 40;		/* TSC5 */

    wbuf = wbuf0;
    pktlen = wbuf_get_pktlen(wbuf0);
    while (wbuf_next(wbuf) != NULL) {
        wbuf = wbuf_next(wbuf);
        pktlen += wbuf_get_pktlen(wbuf);
    }

    /* For a MFP frame, build the MDHR IE portion */
    if (IEEE80211_IS_MFP_FRAME(wh)) {
        if (wbuf!= wbuf0 || 0 != wbuf_append(wbuf, sizeof(struct ieee80211_ccx_mhdr_ie))) {
            /* NB: should not happen */
            IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                               wh->i_addr1,
                               "No room for Michael MIC, tailroom %u",
                               wbuf_get_tailroom(wbuf));
            /* XXX statistic */
        }
        pktlen += sizeof(struct ieee80211_ccx_mhdr_ie);
        tkip_mhdrie_create(wbuf);
    }

    /*
     * Finally, do software encrypt if neeed.
     */
 
    if ((k->wk_flags & IEEE80211_KEY_SWENCRYPT) || 
        ((k->wk_flags & IEEE80211_KEY_MFP) && IEEE80211_IS_MFP_FRAME(wh) &&
         (vap->iv_ic->ic_get_mfpsupport(vap->iv_ic) != IEEE80211_MFP_HW_CRYPTO))) {

        /*
         * If this is a fragment, it should not enmic each individual fragment.
         * In this case, ieee80211_check_and_fragment has already call enmic on the
         * entire original frame.
         */
        if (!istxfrag) {
            /* Compute the MIC */
            if (!tkip_enmic(k, wbuf0, 1, true)) {
                return 0;
            }
        }

        /* Encrypt the frame now. */
        if (!tkip_encrypt(ctx, k, wbuf0, hdrlen))
            return 0;
        // wbuf_append(wbuf, tkip.ic_trailer);
        /* NB: tkip_encrypt handles wk_keytsc */
    } else
        k->wk_keytsc++;

    return 1;
}

/*
 * Add MIC to the frame as needed.
 * Parameter encap: 
 * 0 : If encap not performed
 * 1 : If encap already performed and tkip header is allocated
 */
static int
tkip_enmic(struct ieee80211_key *k, wbuf_t wbuf0, int force, bool encap)
{
    struct tkip_ctx *ctx = k->wk_private;
    wbuf_t wbuf;
    struct ieee80211_frame *wh = (struct ieee80211_frame *) wbuf_header(wbuf0);
    struct ieee80211vap *vap = ctx->tc_vap;
    struct ieee80211com *ic = ctx->tc_ic;
    int hdrlen;
    size_t pktlen;
    u_int8_t mic[IEEE80211_WEP_MICLEN];

    wbuf = wbuf0;
    pktlen = wbuf_get_pktlen(wbuf0);
    while (wbuf_next(wbuf) != NULL) {
        wbuf = wbuf_next(wbuf);
        pktlen += wbuf_get_pktlen(wbuf);
    }

    if (force || (k->wk_flags & IEEE80211_KEY_SWENMIC)) {

        u_int8_t *wbuf_micp;

#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_tkipenmic_inc(vap->vdev_obj, 1);
#endif

        if (0 != wbuf_append(wbuf, tkip.ic_miclen)) {
            /* NB: should not happen */
            IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                               wh->i_addr1,
                               "No room for Michael MIC, tailroom %u",
                               wbuf_get_tailroom(wbuf));
            /* XXX statistic */
            return 0;
        }

        if (encap)
            hdrlen = ieee80211_hdrspace(ic, wh) + tkip.ic_header;
        else
            hdrlen = ieee80211_hdrspace(ic, wh);
        wbuf_micp = (u_int8_t *) wbuf_header(wbuf) + pktlen;
        if (michael_mic(ctx, k->wk_txmic,
                        wbuf0, hdrlen, (u_int16_t)((int)pktlen - hdrlen), mic) != EOK) {
            return 0;
        }
        OS_MEMCPY(wbuf_micp, mic, tkip.ic_miclen);
        pktlen += tkip.ic_miclen;
    }

    return 1;
}

static __inline u_int64_t
READ_6(u_int8_t b0, u_int8_t b1, u_int8_t b2, u_int8_t b3, u_int8_t b4, u_int8_t b5)
{
    u_int32_t iv32 = (b0 << 0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
    u_int16_t iv16 = (b4 << 0) | (b5 << 8);
    return (((u_int64_t)iv16) << 32) | iv32;
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame.  If necessary, decrypt the frame using
 * the specified key.
 */
static int
tkip_decap(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen,struct ieee80211_rx_status *rs)
{
    struct tkip_ctx *ctx = k->wk_private;
    struct ieee80211vap *vap = ctx->tc_vap;
    struct ieee80211_frame *wh;
    u_int8_t *ivp, *origHdr;
    u_int8_t tid;
    struct ieee80211_mac_stats *mac_stats;
    bool is_mcast = 0;

    /*
     * Header should have extended IV and sequence number;
     * verify the former and validate the latter.
     */
    origHdr = (u_int8_t*)wbuf_header(wbuf);
    wh = (struct ieee80211_frame *)origHdr;
    is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
    mac_stats = is_mcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;

    if (rs->rs_flags & IEEE80211_RX_DECRYPT_ERROR) {
        /* The frame already failed decryption in hardware,
         * just update statistics and return. */
#ifdef QCA_SUPPORT_CP_STATS
        is_mcast ? vdev_mcast_cp_stats_rx_tkipicv_inc(vap->vdev_obj, 1):
                   vdev_ucast_cp_stats_rx_tkipicv_inc(vap->vdev_obj, 1);
        WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_tkipicv);
#endif
        return 0;
    }

    ivp = origHdr + hdrlen;
    if ((ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV) == 0) {
        /*
         * No extended IV; discard frame.
         */
        IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
                           "%s", "missing ExtIV for TKIP cipher");
#ifdef QCA_SUPPORT_CP_STATS
        is_mcast ? vdev_mcast_cp_stats_rx_tkipformat_inc(vap->vdev_obj, 1):
                   vdev_ucast_cp_stats_rx_tkipformat_inc(vap->vdev_obj, 1);
#endif
        return 0;
    }
    /*
     * Handle TKIP counter measures requirement.
     */
    if (IEEE80211_VAP_IS_COUNTERM_ENABLED(vap)) {
        IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
                           "discard frame due to countermeasures (%s)", __func__);
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_tkipcm_inc(vap->vdev_obj, 1);
#endif
        return 0;
    }

    tid = IEEE80211_NON_QOS_SEQ;
    if (IEEE80211_QOS_HAS_SEQ(wh))  {
        if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK)
             == IEEE80211_FC1_DIR_DSTODS ) {
            tid = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        } else {
            tid = ((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        }
    }

    ctx->rx_rsc = READ_6(ivp[2], ivp[0], ivp[4], ivp[5], ivp[6], ivp[7]);
    if (ctx->rx_rsc <= k->wk_keyrsc[tid]) {
        /*
         * Replay violation; notify upper layer.
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: replay violation caught rx_rsc %x, keyrsc %x, tid %x\n", 
                  __func__, ctx->rx_rsc, k->wk_keyrsc[tid], tid);
        ieee80211_notify_replay_failure(vap, wh, k, ctx->rx_rsc);
#ifdef QCA_SUPPORT_CP_STATS
        is_mcast ? vdev_mcast_cp_stats_rx_tkipreplay_inc(vap->vdev_obj, 1):
                   vdev_ucast_cp_stats_rx_tkipreplay_inc(vap->vdev_obj, 1);
#endif
        return 0;
    }
    /*
     * NB: We can't update the rsc in the key until MIC is verified.
     *
     * We assume we are not preempted between doing the check above
     * and updating wk_keyrsc when stripping the MIC in tkip_demic.
     * Otherwise we might process another packet and discard it as
     * a replay.
     */

    /*
     * Check if the device handled the decrypt in hardware.
     * If so we just strip the header; otherwise we need to
     * handle the decrypt in software.
     */
    if ((k->wk_flags & IEEE80211_KEY_SWDECRYPT) &&
        !tkip_decrypt(ctx, k, wbuf, hdrlen)) {
        return 0;
    }
    if (IEEE80211_IS_MFP_FRAME(wh)) {
        u_int32_t hwmfp;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s MFP frame\n", __func__);
        hwmfp = vap->iv_ic->ic_get_mfpsupport(vap->iv_ic);
        if (hwmfp  == IEEE80211_MFP_PASSTHRU) {
            /* It is a TKIP MFP frame and it is passthru 
             */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s MFP PASSTHRU calling tkip_decrypt\n", __func__);
             if (!tkip_decrypt(ctx, k, wbuf, hdrlen)) {
                /* Decrypt failed */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s tkip_decrypt failed\n", __func__);
                return 0;
            }
        }
    }


    /*
     * Copy up 802.11 header and strip crypto bits.
     */
    memmove(origHdr + tkip.ic_header, origHdr, hdrlen);
    wbuf_pull(wbuf, tkip.ic_header);
    while (wbuf_next(wbuf) != NULL)
        wbuf = wbuf_next(wbuf);
    wbuf_trim(wbuf, tkip.ic_trailer);
    return 1;
}

/*
 * Verify and strip MIC from the frame.
 */
static int
tkip_demic(struct ieee80211_key *k, wbuf_t wbuf0, int hdrlen, int force, struct ieee80211_rx_status *rs)
{
    struct tkip_ctx *ctx = k->wk_private;
    struct ieee80211vap *vap = ctx->tc_vap;
    wbuf_t wbuf;
    u_int16_t pktlen;
    struct ieee80211_frame *wh;
    u_int8_t tid;
    struct ieee80211_mac_stats *mac_stats;
    struct tkip_countermeasure *mac_counterm;
    int pn_check = 0;  /* check if a bad PN is received */
    int HW_demic_error = 0; /* hardware detects MIC error */
    bool is_mcast = 0;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf0);
    is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
    mac_stats = is_mcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;
    mac_counterm = IEEE80211_IS_MULTICAST(wh->i_addr1) ? &vap->iv_multicast_counterm : &vap->iv_unicast_counterm;

    tid = IEEE80211_NON_QOS_SEQ;
    if (IEEE80211_QOS_HAS_SEQ(wh))  {
        if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK)
             == IEEE80211_FC1_DIR_DSTODS ) {
            tid = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        } else {
            tid = ((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        }
    }

    /*
     * XXX TKIP-only: For frames that needs defrag or frames we have
     * a demic error in hardware, we do it again in software.
     */
    if (wbuf_next(wbuf0) != NULL)
        force = 1;

    if (rs->rs_flags & IEEE80211_RX_MIC_ERROR) {
        HW_demic_error = 1;
        force = 1;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
             "%s: HW mic error is detected.\n", __func__);
    }


    /* Due to hardware bug and sw bug (extraview 55537), we can get corrupted 
     * frame that has a bad PN. The PN upper bits tend to get corrupted.
     * The PN should be a monotically increasing counter. If we detected a big jump,
     * then we will force mic recalculation for this frame.
     */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
               "%s: check for PN gap: TID=%d, from %x %x to %x %x. "
               "Frame seq=0x%x\n",
               __func__,
               tid,
               (u_int32_t)(k->wk_keyrsc[tid] >> 32), (u_int32_t)k->wk_keyrsc[tid],
               (uint32_t)((ctx->rx_rsc)>>32), (uint32_t)(ctx->rx_rsc),
               *(u_int16_t*)(wh->i_seq)
               );
    if ( ctx->rx_rsc > (k->wk_keyrsc[tid] + MAX_TKIP_PN_GAP_ERR)) {
        /* PN gap detected, check MIC for possible error */
        pn_check = 1;
        force = 1;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
               "%s: PN gap detected: TID=%d, from %x %x to %x %x. "
               "Frame seq=0x%x\n",
               __func__,
               tid,
               (u_int32_t)(k->wk_keyrsc[tid] >> 32), (u_int32_t)k->wk_keyrsc[tid],
               (uint32_t)((ctx->rx_rsc)>>32), (uint32_t)(ctx->rx_rsc),
               *(u_int16_t*)(wh->i_seq)
               );
    }

    wbuf = wbuf0;
    pktlen = wbuf_get_pktlen(wbuf);
    while (wbuf_next(wbuf) != NULL) {
        wbuf = wbuf_next(wbuf);
        pktlen += wbuf_get_pktlen(wbuf);
    }

    /*
     * TKIP management frame with MFP has MHDR IE field.
     */
    if (IEEE80211_IS_MFP_FRAME(wh)) {
        /* MFP frame */
        if (!tkip_mhdrie_check(wbuf0, pktlen, tkip.ic_miclen)) {
            u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

            mac_stats->ims_rx_tkipmic++;
            IEEE80211_NODE_STAT_ADDRBASED(vap, wh->i_addr2, rx_tkipmic);

            /* NB: 802.11 layer handles statistic and debug msg */
            ieee80211_notify_michael_failure(vap, wh, k->wk_keyix);

            /*
             * IEEE 802.11i section 8.3.2.4 TKIP countermeasures procedures:
             *   For a Supplicants STA that receives 2 frames with MIC error within
             *   60 secounts, the station need to invoke countermeasures procedures. 
             */
            if ((now - mac_counterm->timestamp) > 60000) {
                mac_counterm->mic_count_in_60s = 1;
                mac_counterm->timestamp = now;
            } else {
                if (++mac_counterm->mic_count_in_60s == 2) {
                    /*
                     * If 2 MIC error were received by the station within 60 seconds,
                     * the station need to deauthenticate from the AP and wait for 60s
                     * before (re)establishing a TKIP association with the same AP.
                     */
#ifdef QCA_SUPPORT_CP_STATS
                    is_mcast ? vdev_mcast_cp_stats_rx_countermeasure_inc(vap->vdev_obj, 1):
                              vdev_ucast_cp_stats_rx_countermeasure_inc(vap->vdev_obj, 1);
#endif
                    mac_counterm->timestamp = now;
                }
            }
            return 0;
        }
    }

    /* NB: wbuf left pointing at last in chain */
    if (k->wk_flags & IEEE80211_KEY_SWDEMIC || force) {
        u8 mic[IEEE80211_WEP_MICLEN];
        u8 mic0[IEEE80211_WEP_MICLEN];

        if (michael_mic(ctx, k->wk_rxmic, 
                    wbuf0, hdrlen, pktlen - (hdrlen + tkip.ic_miclen),
                        mic) != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                  "%s: michael_mic failed.\n", __func__);
            return 0;
        }
        /* XXX assert pktlen >= tkip.ic_miclen */
        wbuf_copydata(wbuf0, pktlen - tkip.ic_miclen,
                      tkip.ic_miclen, (caddr_t) mic0);
        if (memcmp(mic, mic0, tkip.ic_miclen)) {

            u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

            if (pn_check && !HW_demic_error) {
                /* The MIC is corrupted. so PN is most likely wrong. Do not use this frame. */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: PN gap but no HW mic error is detected, drop this frame.\n", __func__);
                return 0;
            }    
                
            mac_stats->ims_rx_tkipmic++;
            IEEE80211_NODE_STAT_ADDRBASED(vap, wh->i_addr2, rx_tkipmic);

            /* NB: 802.11 layer handles statistic and debug msg */
            ieee80211_notify_michael_failure(vap, wh, k->wk_keyix);

            /*
             * IEEE 802.11i section 8.3.2.4 TKIP countermeasures procedures:
             *   For a Supplicants STA that receives 2 frames with MIC error within
             *   60 secounts, the station need to invoke countermeasures procedures. 
             */
            if ((now - mac_counterm->timestamp) > 60000) {
                mac_counterm->mic_count_in_60s = 1;
                mac_counterm->timestamp = now;
            } else {
                if (++mac_counterm->mic_count_in_60s == 2) {
                    /*
                     * If 2 MIC error were received by the station within 60 seconds,
                     * the station need to deauthenticate from the AP and wait for 60s
                     * before (re)establishing a TKIP association with the same AP.
                     */
#ifdef QCA_SUPPORT_CP_STATS
                    is_mcast ? vdev_mcast_cp_stats_rx_countermeasure_inc(vap->vdev_obj, 1):
                               vdev_ucast_cp_stats_rx_countermeasure_inc(vap->vdev_obj, 1);
#endif
                    mac_counterm->timestamp = now;
                }
            }
            return 0;
        }
    }
    /*
     * Strip MIC from the tail.
     */
    wbuf_trim(wbuf, tkip.ic_miclen);
    if (IEEE80211_IS_MFP_FRAME(wh)) {
        /* Strip MHDR IE */
        wbuf_trim(wbuf, sizeof(struct ieee80211_ccx_mhdr_ie));
    }

    /*
     * Ok to update rsc now that MIC has been verified.
     */
    tid = IEEE80211_NON_QOS_SEQ;
    if (IEEE80211_QOS_HAS_SEQ(wh)) 
        tid = ((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_TID;
    k->wk_keyrsc[tid] = ctx->rx_rsc;

    return 1;
}

/*
 * Craft pseudo header used to calculate the MIC.
 */
static void
michael_mic_hdr(const struct ieee80211_frame *wh0, u8 hdr[16])
{
    const struct ieee80211_frame_addr4 *wh =
        (const struct ieee80211_frame_addr4 *) wh0;

    switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
    case IEEE80211_FC1_DIR_NODS:
        IEEE80211_ADDR_COPY(hdr, wh->i_addr1); /* DA */
        IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr2);
        break;
    case IEEE80211_FC1_DIR_TODS:
        IEEE80211_ADDR_COPY(hdr, wh->i_addr3); /* DA */
        IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr2);
        break;
    case IEEE80211_FC1_DIR_FROMDS:
        IEEE80211_ADDR_COPY(hdr, wh->i_addr1); /* DA */
        IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr3);
        break;
    case IEEE80211_FC1_DIR_DSTODS:
        IEEE80211_ADDR_COPY(hdr, wh->i_addr3); /* DA */
        IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr4);
        break;
    }

    /*
     * Bit 7 is IEEE80211_FC0_SUBTYPE_QOS for data frame, but
     * it could also be set for deauth, disassoc, action, etc. for 
     * a mgt type frame. It comes into picture for MFP.
     */
    if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
        if (IEEE80211_IS_MFP_FRAME(wh)) {
            hdr[12] = IEEE80211_MFP_TID;
        }
        else {
            u_int8_t tid = IEEE80211_NON_QOS_SEQ;
            if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK)
                 == IEEE80211_FC1_DIR_DSTODS ) {
                tid = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] & IEEE80211_QOS_TID;
            } else {
                tid = ((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_TID;
            }
            hdr[12] = tid;    
        }
    } else
        hdr[12] = 0;
    hdr[13] = hdr[14] = hdr[15] = 0; /* reserved */
}

int
michael_mic(struct tkip_ctx *ctx, const u8 *key,
            wbuf_t wbuf, u_int off, u_int16_t data_len,
            u8 mic[IEEE80211_WEP_MICLEN])
{
    u_int8_t hdr[16];
    u32 l, r;
    const u_int8_t *data;
    u_int space;

    michael_mic_hdr((struct ieee80211_frame *) wbuf_header(wbuf), hdr);

    l = get_le32(key);
    r = get_le32(key + 4);

    /* Michael MIC pseudo header: DA, SA, 3 x 0, Priority */
    l ^= get_le32(hdr);
    michael_block(l, r);
    l ^= get_le32(&hdr[4]);
    michael_block(l, r);
    l ^= get_le32(&hdr[8]);
    michael_block(l, r);
    l ^= get_le32(&hdr[12]);
    michael_block(l, r);

    /* first buffer has special handling */
    data = (u_int8_t*)wbuf_header(wbuf) + off;
    space = wbuf_get_pktlen(wbuf) - off;
    for (;;) {
        if (space > data_len)
            space = data_len;
        /* collect 32-bit blocks from current buffer */
        while (space >= sizeof(u_int32_t)) {
            l ^= get_le32(data);
            michael_block(l, r);
            data += sizeof(u_int32_t), space -= sizeof(u_int32_t);
            data_len -= sizeof(u_int32_t);
        }
        if (data_len < sizeof(u_int32_t))
            break;
        wbuf = wbuf_next(wbuf);
        if (wbuf == NULL) {
            IEEE80211_DPRINTF(ctx->tc_vap, IEEE80211_MSG_ANY,
                "%s: out of data, data_len %zu\n", __func__,
                (size_t)data_len);
            return EINVAL; 
        }
        if (space != 0) {
            const u_int8_t *data_next;
            /*
             * Block straddles buffers, split references.
             */
            data_next = (u_int8_t*)wbuf_header(wbuf);
            if (wbuf_get_pktlen(wbuf) < sizeof(u_int32_t) - space) {
                    IEEE80211_DPRINTF(ctx->tc_vap, IEEE80211_MSG_ANY,
                            "%s: not enough data in following buffer, "
                            "pktlen %u need %zu\n", __func__,
                            wbuf_get_pktlen(wbuf), sizeof(u_int32_t) - space);
                    return EINVAL;
            }
            switch (space) {
            case 1:
                l ^= get_le32_split(data[0], data_next[0],
                                    data_next[1], data_next[2]);
                data = data_next + 3;
                space = wbuf_get_pktlen(wbuf) - 3;
                break;
            case 2:
                l ^= get_le32_split(data[0], data[1],
                                    data_next[0], data_next[1]);
                data = data_next + 2;
                space = wbuf_get_pktlen(wbuf) - 2;
                break;
            case 3:
                l ^= get_le32_split(data[0], data[1],
                                    data[2], data_next[0]);
                data = data_next + 1;
                space = wbuf_get_pktlen(wbuf) - 1;
                break;
            }
            michael_block(l, r);
            data_len -= sizeof(u_int32_t);
        } else {
            /*
             * Setup for next buffer.
             */
            data = (u_int8_t*)wbuf_header(wbuf);
            space = wbuf_get_pktlen(wbuf);
        }
    }
    /* Last block and padding (0x5a, 4..7 x 0) */
    switch (data_len) {
    case 0:
        l ^= get_le32_split(0x5a, 0, 0, 0);
        break;
    case 1:
        l ^= get_le32_split(data[0], 0x5a, 0, 0);
        break;
    case 2:
        l ^= get_le32_split(data[0], data[1], 0x5a, 0);
        break;
    case 3:
        l ^= get_le32_split(data[0], data[1], data[2], 0x5a);
        break;
    }
    michael_block(l, r);
    /* l ^= 0; */
    michael_block(l, r);

    put_le32(mic, l);
    put_le32(mic + 4, r);

    return EOK;
}

/* Module attachment */
void
ieee80211_crypto_register_tkip(struct ieee80211com *ic)
{
    ieee80211_crypto_register(ic, &tkip);
}
void
ieee80211_crypto_unregister_tkip(struct ieee80211com *ic)
{
    ieee80211_crypto_unregister(ic, &tkip);
}
#endif
