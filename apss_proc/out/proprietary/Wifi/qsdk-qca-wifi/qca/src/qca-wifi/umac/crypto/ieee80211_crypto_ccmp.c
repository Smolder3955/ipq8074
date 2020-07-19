/*
 *
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 */

/*
 * IEEE 802.11i AES-CCMP crypto support.
 */
#include <ieee80211_crypto_ccmp_priv.h>
#include <ieee80211_crypto.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
/* number of references from net80211 layer */
static	int nrefs = 0;

static	void *ccmp_attach(struct ieee80211vap *, struct ieee80211_key *);
static	void ccmp_detach(struct ieee80211_key *);
static	int ccmp_setkey(struct ieee80211_key *);
static	int ccmp_encap(struct ieee80211_key *k, wbuf_t, u_int8_t keyid);
static	int ccmp_decap(struct ieee80211_key *, wbuf_t, int, struct ieee80211_rx_status *);
static	int ccmp_enmic(struct ieee80211_key *, wbuf_t, int, bool);
static	int ccmp_demic(struct ieee80211_key *, wbuf_t, int, int, struct ieee80211_rx_status *);
static int
ccmp_check_mic(struct ieee80211_key *key, wbuf_t wbuf0, int hdrlen, u_int64_t rx_pn);

#define     MAX_CCMP_PN_GAP_ERR_CHECK       1000/* Max. gap in CCMP PN, to suspect it as corrupted PN after MIC succ,
                                                keep this value higher than  MAX_CCMP_PN_GAP_ERR */

const struct ieee80211_cipher ccmp = {
    "AES-CCM",
    IEEE80211_CIPHER_AES_CCM,
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
    IEEE80211_WEP_MICLEN,
    0,
    ccmp_attach,
    ccmp_detach,
    ccmp_setkey,
    ccmp_encap,
    ccmp_decap,
    ccmp_enmic,
    ccmp_demic,
};

const struct ieee80211_cipher ccmp256 = {
    "AES-CCM 256",
    IEEE80211_CIPHER_AES_CCM_256,
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN+8,
    IEEE80211_WEP_MICLEN+8,
    0,
    ccmp_attach,
    ccmp_detach,
    ccmp_setkey,
    ccmp_encap,
    ccmp_decap,
    ccmp_enmic,
    ccmp_demic,
};

const struct ieee80211_cipher gcmp = {
    "AES-GCM 128",
    IEEE80211_CIPHER_AES_GCM,
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
    IEEE80211_WEP_MICLEN,
    0,
    ccmp_attach,
    ccmp_detach,
    ccmp_setkey,
    ccmp_encap,
    ccmp_decap,
    ccmp_enmic,
    ccmp_demic,
};

const struct ieee80211_cipher gcmp256 = {
    "AES-GCM 256",
    IEEE80211_CIPHER_AES_GCM_256,
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN+8,
    IEEE80211_WEP_MICLEN+8,
    0,
    ccmp_attach,
    ccmp_detach,
    ccmp_setkey,
    ccmp_encap,
    ccmp_decap,
    ccmp_enmic,
    ccmp_demic,
};
static void *
ccmp_attach(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    struct ccmp_ctx *ctx;
    struct ieee80211com *ic = vap->iv_ic;

    ctx = (struct ccmp_ctx *)OS_MALLOC(ic->ic_osdev, sizeof(struct ccmp_ctx), GFP_KERNEL);
    if (ctx == NULL) {
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_nomem_inc(vap->vdev_obj, 1);
#endif
        return NULL;
    }
    ctx->cc_vap = vap;
    ctx->cc_ic = vap->iv_ic;

    nrefs++;			/* NB: we assume caller locking */
    return ctx;
}

static void
ccmp_detach(struct ieee80211_key *k)
{
    struct ccmp_ctx *ctx = k->wk_private;

    OS_FREE(ctx);
    KASSERT(nrefs > 0, ("imbalanced attach/detach"));
    nrefs--;			/* NB: we assume caller locking */
}

static int
ccmp_setkey(struct ieee80211_key *k)
{
    struct ccmp_ctx *ctx = k->wk_private;

    if ((k->wk_keylen != (128/NBBY)) && (k->wk_keylen != (256/NBBY)) ){
        IEEE80211_DPRINTF(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
                          "%s: Invalid key length %u, expecting %u\n",
                          __func__, k->wk_keylen, 128/NBBY);
        return 0;
    }

    /* Always set up the software key structure.
     * If encrypted frame is received even when not using encryption,
     * software crypto may kick in.
     */
    rijndael_set_key(&ctx->cc_aes, k->wk_key, k->wk_keylen*NBBY);
    return 1;
}

/*
 * Add privacy headers appropriate for the specified key.
 */
static int
ccmp_encap(struct ieee80211_key *k, wbuf_t wbuf, u_int8_t keyid)
{
    struct ccmp_ctx *ctx = k->wk_private;
    struct ieee80211com *ic = ctx->cc_ic;
    struct ieee80211vap *vap = ctx->cc_vap;
    u_int8_t *ivp;
    int hdrlen;
    int is4addr, isqos, owl_wdswar = 0;
    struct ieee80211_frame *wh;
    struct ieee80211_node *ni = wlan_wbuf_get_peer_node(wbuf);
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    is4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
    isqos = IEEE80211_QOS_HAS_SEQ(wh);

    if (ni != NULL)
        owl_wdswar = (ni->ni_flags & IEEE80211_NODE_OWL_WDSWAR);


    if (vap->iv_opmode == IEEE80211_M_MONITOR) {
        /*
          For offchan encrypted TX, key's saved in mon vap.
          For 3-addr Qos frame(26 bytes) encryption, don't do 4-byte alignment,
          otherwise, the dot11 header in OTA pkts will have 2 more bytes, which's from LLC
        */
        hdrlen = ieee80211_hdrsize(wbuf_header(wbuf));
    }else{
        hdrlen = ieee80211_hdrspace(ic, wbuf_header(wbuf));
    }

    /*
     * Copy down 802.11 header and add the IV, KeyID, and ExtIV.
     */
#ifndef QCA_PARTNER_PLATFORM
    ivp = (u_int8_t *)wbuf_push(wbuf, ccmp.ic_header);

    /*
     * refresh the wh pointer,
     * wbuf header  pointer is changed with wbuf_push.
     */
    wh = (struct ieee80211_frame *) wbuf_header(wbuf); /* recompute wh */

    memmove(ivp, ivp + ccmp.ic_header, hdrlen);
#else
    if (wbuf_is_encap_done(wbuf)) {
        ivp = (u_int8_t *)wbuf_header(wbuf);
    }
    else {
        ivp = (u_int8_t *)wbuf_push(wbuf, ccmp.ic_header);
        memmove(ivp, ivp + ccmp.ic_header, hdrlen);
        /*
         * refresh the wh pointer,
         * wbuf header  pointer is changed with wbuf_push.
         */
        wh = (struct ieee80211_frame *) wbuf_header(wbuf); /* recompute wh */
    }
#endif
    ivp += hdrlen;

    /*
     * Due to OWL specific HW bug, increment key tsc by 16, since
     * we're copying the TID into bits [3:0] of IV0.
     * XXX: Need logic to not implement workaround for SOWL or greater.
     */
    if (is4addr && isqos && owl_wdswar)
        k->wk_keytsc += 16;
    else
        k->wk_keytsc++;		/* XXX wrap at 48 bits */

    if(ieee80211com_has_pn_check_war(ic))
        wbuf_set_cboffset(wbuf,hdrlen);

    ivp[0] = k->wk_keytsc >> 0;		/* PN0 */
    ivp[1] = k->wk_keytsc >> 8;		/* PN1 */
    ivp[2] = 0;				/* Reserved */
    ivp[3] = keyid | IEEE80211_WEP_EXTIV;	/* KeyID | ExtID */
    ivp[4] = k->wk_keytsc >> 16;		/* PN2 */
    ivp[5] = k->wk_keytsc >> 24;		/* PN3 */
    ivp[6] = k->wk_keytsc >> 32;		/* PN4 */
    ivp[7] = k->wk_keytsc >> 40;		/* PN5 */

    /*
     * Finally, do software encrypt if neeed.
     */
    if (k->wk_flags & IEEE80211_KEY_SWENCRYPT) {
#if !ATH_DRIVER_SIM /* simulator doesn't perform any hardware encryption - disabling software encryption & decryption */
        if (!ccmp_encrypt(k, wbuf, hdrlen, IEEE80211_IS_MFP_FRAME(wh))) {
            return 0;
        }
#endif
    }
    if (((k->wk_flags & IEEE80211_KEY_MFP) && IEEE80211_IS_MFP_FRAME(wh)) ||
        (ni && ieee80211_is_pmf_enabled(ni->ni_vap, ni))){
        if (ic->ic_get_mfpsupport(ic) != IEEE80211_MFP_HW_CRYPTO) {
            /* HW MFP is not enabled - do software encrypt */
            if (!ccmp_encrypt(k, wbuf, hdrlen, 1)) {
                return 0;
            }
        }
    }

    return 1;
}

/*
 * Add MIC to the frame as needed.
 */
static int
ccmp_enmic(struct ieee80211_key *k, wbuf_t wbuf, int force, bool encap)
{
    return 1;
}

static INLINE u_int64_t
READ_6(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
{
    uint32_t iv32 = (b0 << 0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
    uint16_t iv16 = (b4 << 0) | (b5 << 8);
    return (((u_int64_t)iv16) << 32) | iv32;
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame. The specified key should be correct but
 * is also verified.
 */
static int
ccmp_decap(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen, struct ieee80211_rx_status *rs)
{
    struct ccmp_ctx *ctx = k->wk_private;
    struct ieee80211vap *vap = ctx->cc_vap;
    struct ieee80211_frame *wh;
    uint8_t *ivp, *origHdr;
    u_int64_t pn;
    u_int8_t tid;
    struct ieee80211_mac_stats *mac_stats;
    u_int32_t hwmfp=0;
    uint8_t  update_keyrsc = 1;
    bool is_mcast = 0;
    /*
     * Header should have extended IV and sequence number;
     * verify the former and validate the latter.
     */
    origHdr = (u_int8_t *)wbuf_header(wbuf);
    wh = (struct ieee80211_frame *)origHdr;
    is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
    mac_stats = is_mcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;
    if (IEEE80211_IS_MFP_FRAME(wh)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s MFP frame\n", __func__);
        hwmfp = vap->iv_ic->ic_get_mfpsupport(vap->iv_ic);
    }

    if (rs->rs_flags & IEEE80211_RX_DECRYPT_ERROR) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s decrypt error\n", __func__);
        if ((IEEE80211_IS_MFP_FRAME(wh)) && (hwmfp != IEEE80211_MFP_HW_CRYPTO)) {
            /* It may not be real crypto error, but hardware didn't do correct
             * decryption. Try software decrypt later.
             */
            rs->rs_flags &= ~IEEE80211_RX_DECRYPT_ERROR;
            rs->rs_flags |= IEEE80211_RX_MIC_ERROR;
        } else {
            /* The frame already failed decryption in hardware,
             * just update statistics and return. */
#ifdef QCA_SUPPORT_CP_STATS
            is_mcast ? vdev_mcast_cp_stats_rx_ccmpmic_inc(vap->vdev_obj, 1):
                       vdev_ucast_cp_stats_rx_ccmpmic_inc(vap->vdev_obj, 1);
            WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_ccmpmic);
#endif
            return 0;
        }
    }

    ivp = origHdr + hdrlen;
    if ((ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV) == 0) {
        /*
         * No extended IV; discard frame.
         */
        IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
                           "%s", "Missing ExtIV for AES-CCM cipher");
#ifdef QCA_SUPPORT_CP_STATS
            is_mcast ? vdev_mcast_cp_stats_rx_ccmpformat_inc(vap->vdev_obj, 1):
                       vdev_ucast_cp_stats_rx_ccmpformat_inc(vap->vdev_obj, 1);
#endif
        return 0;
    }
    tid = IEEE80211_NON_QOS_SEQ;
    if (IEEE80211_QOS_HAS_SEQ(wh)) {
        if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK)
             == IEEE80211_FC1_DIR_DSTODS ) {
            tid = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        } else {
            tid = ((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        }
    }

    /* NB: assume IEEEE80211_WEP_MINLEN covers the extended IV */
    pn = READ_6(ivp[0], ivp[1], ivp[4], ivp[5], ivp[6], ivp[7]);

     /* ********************************************************
        * Fixed for EV88475: The Fujishi SR-M20AP1 ((Ralink chip) Firmware: V2.04)
        * will send a first encryption frame with pn set as 0.
        * So it needs to ensure not to do the replay violation check when pn is 0.
        ***********************************************************/
#ifdef QCA_SUPPORT_CP_STATS
    if (is_mcast ? (vdev_mcast_cp_stats_rx_decryptok_get(vap->vdev_obj) ||
                    vdev_mcast_cp_stats_rx_ccmpreplay_get(vap->vdev_obj) ||
                    vdev_mcast_cp_stats_rx_ccmpmic_get(vap->vdev_obj)
                   ):
                   (vdev_ucast_cp_stats_rx_decryptok_get(vap->vdev_obj) ||
                    vdev_ucast_cp_stats_rx_ccmpreplay_get(vap->vdev_obj) ||
                    vdev_ucast_cp_stats_rx_ccmpmic_get(vap->vdev_obj)
                   )
#endif
        && pn <= k->wk_keyrsc[tid]) {
        /*
         * Replay violation.
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: CCMP Replay! Throw away. new pn=0x%x%x, "
                          "current pn=0x%x%x. seq=0x%x,Rsvd=0x%x,keyID=0x%x,tid=%d\n",
                           __func__,
                           (u_int32_t)(pn>>32), (u_int32_t)pn,
                           (u_int32_t)(k->wk_keyrsc[tid] >> 32), (u_int32_t)k->wk_keyrsc[tid],
                           *(u_int16_t*)(wh->i_seq),
                           ivp[2], ivp[3],
                           tid
                           );
        ieee80211_notify_replay_failure(vap, wh, k, pn);
#ifdef QCA_SUPPORT_CP_STATS
        is_mcast ? vdev_mcast_cp_stats_rx_ccmpreplay_inc(vap->vdev_obj, 1):
                   vdev_ucast_cp_stats_rx_ccmpreplay_inc(vap->vdev_obj, 1);
#endif
        return 0;
    }

    /*
     * Check if the device handled the decrypt in hardware.
     * If so we just strip the header; otherwise we need to
     * handle the decrypt in software.  Note that for the
     * latter we leave the header in place for use in the
     * decryption work.
     *
     * When there is KEYMISS is set, likely that that frame is
     * not decrypted. If VAP is chosen to allow this selective
     * decrypt, and frame is unitcast try doing s/w decryption
     * of the frame.
     */
    if ((k->wk_flags & IEEE80211_KEY_SWDECRYPT) ||
            ((vap->iv_ccmpsw_seldec) && (rs->rs_flags & IEEE80211_RX_KEYMISS) &&
             (!IEEE80211_IS_MULTICAST(wh->i_addr1)))) {
#if !ATH_DRIVER_SIM /* simulator doesn't perform any hardware encryption - disabling software encryption & decryption */
        if (ccmp_decrypt(k, pn, wbuf, hdrlen, 0) == 0) {
#ifdef QCA_SUPPORT_CP_STATS
            is_mcast ? vdev_mcast_cp_stats_rx_ccmpmic_inc(vap->vdev_obj, 1):
                       vdev_ucast_cp_stats_rx_ccmpmic_inc(vap->vdev_obj, 1);
            WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_ccmpmic);
#endif
            return 0;
        }
#endif
    }

    if (IEEE80211_IS_MFP_FRAME(wh)) {

        u_int64_t savetsc;
        u_int8_t  savemic[IEEE80211_WEP_MICLEN];
        u_int8_t  *micp;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s MFP frame\n", __func__);
        if (hwmfp  != IEEE80211_MFP_HW_CRYPTO) {
            if (hwmfp  == IEEE80211_MFP_QOSDATA) {
                /* Legacy hardware. Tried decryption and ran into MIC error.
                 * Reconstruct the original frame by encrypting it.
                 */

                /* tell encrypt to use received sequence counter instead of transmit */
                savetsc = k->wk_keytsc;
                k->wk_keytsc = pn;

                /* save MIC from original packet */
                micp = (u_int8_t *)wbuf_header(wbuf) + wbuf_get_pktlen(wbuf) - IEEE80211_WEP_MICLEN;
                OS_MEMCPY(&savemic[0], micp, IEEE80211_WEP_MICLEN);
                wbuf_trim(wbuf, ccmp.ic_trailer);
                ccmp_encrypt(k, wbuf, hdrlen, 0);
                k->wk_keytsc = savetsc;
                OS_MEMCPY(micp, &savemic[0], IEEE80211_WEP_MICLEN);
            }

            /* if hardware was set to MFP passthrough, it will come here */
#if !ATH_DRIVER_SIM /* simulator doesn't perform any hardware encryption - disabling software encryption & decryption */
            if (ccmp_decrypt(k, pn, wbuf, hdrlen, 1) == 0) {
                rs->rs_flags |= IEEE80211_RX_MIC_ERROR;
#ifdef QCA_SUPPORT_CP_STATS
                is_mcast ? vdev_mcast_cp_stats_rx_ccmpmic_inc(vap->vdev_obj, 1):
                           vdev_ucast_cp_stats_rx_ccmpmic_inc(vap->vdev_obj, 1);
                WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_ccmpmic);
#endif
                return 0;
            }
#endif
        }
    }

    // Due to hardware bug and sw bug (extraview 55537), we can get corrupted
    // frame that has a bad PN. The PN upper bits tend to get corrupted.
    // The PN should be a monotically increasing counter. If we detected a big jump,
    // then we will throw away this frame.
    if ((k->wk_keyrsc[tid] > 1) && (pn > (k->wk_keyrsc[tid] + MAX_CCMP_PN_GAP_ERR_CHECK))) {
        /* PN jump wrt keyrsc is > MAX_CCMP_PN_GAP_ERR_CHECK - PN of current frame is suspected */
        if (k->wk_keyrsc_suspect[tid]) {
            /* Check whether PN of the current frame is following prev PN seq or not */
            if (pn <  k->wk_keyrsc_suspect[tid]) {
                /* PN number of the curr frame < PN no of prev rxed frame
                 * As we are not sure about prev suspect PN, to detect replay,
                 * check the current PN with global PN */
                if (pn < k->wk_keyglobal) {
                    /* Replay violation */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                                      "%s: CCMP Replay on direct attach! Throw away. new pn=0x%x%x, "
                                      "current pn=0x%x%x. seq=0x%x,Rsvd=0x%x,keyID=0x%x,tid=%d\n",
                                      __func__,
                                      (u_int32_t)(pn>>32), (u_int32_t)pn,
                                      (u_int32_t)(k->wk_keyrsc[tid] >> 32), (u_int32_t)k->wk_keyrsc[tid],
                                      *(u_int16_t*)(wh->i_seq),
                                      ivp[2], ivp[3],
                                      tid);
                    ieee80211_notify_replay_failure(vap, wh, k, pn);
#ifdef QCA_SUPPORT_CP_STATS
                    is_mcast ? vdev_mcast_cp_stats_rx_ccmpreplay_inc(vap->vdev_obj, 1):
                               vdev_ucast_cp_stats_rx_ccmpreplay_inc(vap->vdev_obj, 1);
#endif
                    return 0;
                } else {
                    /* Current PN is following global PN, so mark this as suspected PN
                     * Don't update keyrsc & keyglobal */
                    k->wk_keyrsc_suspect[tid] = pn;
                    update_keyrsc = 0;
                }
            } else if (pn < ( k->wk_keyrsc_suspect[tid] + MAX_CCMP_PN_GAP_ERR_CHECK)) {
                /* Current PN is following prev suspected PN seq
                 * Update keyrsc & keyglobal (update_keyrsc = 1;) */
            } else {
                /* Current PN is neither following prev suspected PN nor prev Keyrsc
                 * Mark this as new suspect and don't update keyrsc & keyglobal */
                k->wk_keyrsc_suspect[tid] = pn;
                update_keyrsc = 0;
            }
        } else {
            /* New Jump in PN observed
             * So mark this PN as suspected and don't update keyrsc/keyglobal */
            k->wk_keyrsc_suspect[tid] = pn;
            update_keyrsc = 0;
        }
    } else {
        /* Valid PN, update keyrsc & keyglobal (update_keyrsc = 1;) */
    }

#if ATH_VOW_EXT_STATS
    if(rs->vow_extstats_offset)
    {
        uint16_t es_offset = rs->vow_extstats_offset & 0xFFFF; /* lower 16 bits contains UDP checksum offset */
        uint8_t *bp;

        /* Get the pointer to UDP checksum in the payload */
        bp =  (uint8_t *)wbuf_raw_data(wbuf) + es_offset;

        /* zero out the udp checksum as pkt content got modified to add vow ext stats (in ath layer)*/
        *bp = 0x0;
        *(bp+1) = 0x0;
    }
#endif

    /*
     * Copy up 802.11 header and strip crypto bits.
     */
    memmove(origHdr + ccmp.ic_header, origHdr, hdrlen);
    wbuf_pull(wbuf, ccmp.ic_header);
    wbuf_trim(wbuf, ccmp.ic_trailer);

    if(update_keyrsc)
    {
        /*
         * Ok to update rsc now.
         */
        k->wk_keyrsc[tid] = pn;
        k->wk_keyglobal = pn;
        k->wk_keyrsc_suspect[tid] = 0;
    }
    return 1;
}

/*
 * Verify and strip MIC from the frame.
 */
static int
ccmp_demic(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen, int force, struct ieee80211_rx_status *rs)
{
    return 1;
}

void
ccmp_init_blocks(rijndael_ctx *ctx, struct ieee80211_frame *wh,
                 u_int64_t pn, u_int32_t dlen,
                 uint8_t b0[AES_BLOCK_LEN], uint8_t aad[2 * AES_BLOCK_LEN],
                 uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN], uint8_t mfp)
{
#define	IS_4ADDRESS(wh)                                                 \
    ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
#define	IS_QOS_DATA(wh)	IEEE80211_QOS_HAS_SEQ(wh)

    /* CCM Initial Block:
     * Flag (Include authentication header, M=3 (8-octet MIC),
     *       L=1 (2-octet Dlen))
     * Nonce: 0x00 | A2 | PN
     * Dlen */
    b0[0] = 0x59;
    /* NB: b0[1] set below */
    IEEE80211_ADDR_COPY(b0 + 2, wh->i_addr2);
    b0[8] = pn >> 40;
    b0[9] = pn >> 32;
    b0[10] = pn >> 24;
    b0[11] = pn >> 16;
    b0[12] = pn >> 8;
    b0[13] = pn >> 0;
    b0[14] = (uint8_t)((dlen >> 8) & 0xff);
    b0[15] = (uint8_t)(dlen & 0xff);


    /* AAD:
     * FC with bits 4..6 and 11..13 masked to zero; 14 is always one
     * A1 | A2 | A3
     * SC with bits 4..15 (seq#) masked to zero
     * A4 (if present)
     * QC (if present)
     */
    aad[0] = 0;	/* AAD length >> 8 */
    /* NB: aad[1] set below */
    /* aad[2] = wh->i_fc[0] & 0x8f; */	/* XXX magic #s */
    if (mfp) {
        aad[2] = wh->i_fc[0] & 0xff;	/* XXX magic #s */
    } else {
        /* we  should mask bits 4, 5, 6 so we should AND with 0x8f */
        aad[2] = wh->i_fc[0] & 0xcf;	/* XXX magic #s */
    }
    /* Masking bits RETRY, PM, MORE_DATA (11, 12, 13) */
    aad[3] = wh->i_fc[1] & 0xc7;	/* XXX magic #s */
    /* Bit 14 (PROTECTED) should always be 1 */
    aad[3] |= 0x40;
    /* NB: we know 3 addresses are contiguous */
#ifdef ATH_RNWF
#pragma prefast(suppress:6202, "we know 3 addresses are contiguous")
#endif
    OS_MEMCPY(aad + 4, wh->i_addr_all, 3 * IEEE80211_ADDR_LEN);
    aad[22] = wh->i_seq[0] & IEEE80211_SEQ_FRAG_MASK;
    aad[23] = 0; /* all bits masked */
    /*
     * Construct variable-length portion of AAD based
     * on whether this is a 4-address frame/QOS frame.
     * We always zero-pad to 32 bytes before running it
     * through the cipher.
     *
     * We also fill in the priority bits of the CCM
     * initial block as we know whether or not we have
     * a QOS frame.
     */
    if (IS_4ADDRESS(wh)) {
        IEEE80211_ADDR_COPY(aad + 24,
                            ((struct ieee80211_frame_addr4 *)wh)->i_addr4);
        if (IS_QOS_DATA(wh)) {
            struct ieee80211_qosframe_addr4 *qwh4 =
                (struct ieee80211_qosframe_addr4 *) wh;
            aad[30] = qwh4->i_qos[0] & 0x0f;/* just priority bits */
            aad[31] = 0;
            b0[1] = aad[30] | (mfp << 4);
            aad[1] = 22 + IEEE80211_ADDR_LEN + 2;
        } else {
            *(u_int16_t *)&aad[30] = 0;
            b0[1] = 0 | (mfp << 4);
            aad[1] = 22 + IEEE80211_ADDR_LEN;
        }
    } else {
        if (IS_QOS_DATA(wh)) {
            struct ieee80211_qosframe *qwh =
                (struct ieee80211_qosframe*) wh;
            aad[24] = qwh->i_qos[0] & 0x0f;	/* just priority bits */
            aad[25] = 0;
            b0[1] = aad[24] | (mfp << 4);
            aad[1] = 22 + 2;
        } else {
            *(u_int16_t *)&aad[24] = 0;
            b0[1] = 0 | (mfp << 4);
            aad[1] = 22;
        }
        *(u_int16_t *)&aad[26] = 0;
        *(u_int32_t *)&aad[28] = 0;
    }

    /* Start with the first block and AAD */
    rijndael_encrypt(ctx, b0, auth);
    xor_block(auth, aad, AES_BLOCK_LEN);
    rijndael_encrypt(ctx, auth, auth);
    xor_block(auth, &aad[AES_BLOCK_LEN], AES_BLOCK_LEN);
    rijndael_encrypt(ctx, auth, auth);
    b0[0] &= 0x07;
    b0[14] = b0[15] = 0;
    rijndael_encrypt(ctx, b0, s0);
#undef	IS_QOS_DATA
#undef	IS_4ADDRESS
}

#define	CCMP_CALC_MIC(_b, _pos, _len) do {      \
	/* Authentication */				        \
	xor_block(_b, _pos, _len);			        \
	rijndael_encrypt(&ctx->cc_aes, _b, _b);		\
    } while (0)

//
// Check the MIC of this decrypted frame.
// NOTE: wbuf already contains the ccmp.ic_header and ccmp.ic_trailer
// Return 1 if MIC is good. Return 0 if MIC is bad.
//
static int
ccmp_check_mic(struct ieee80211_key *key, wbuf_t wbuf0, int hdrlen, u_int64_t rx_pn)
{
    struct ccmp_ctx *ctx = key->wk_private;
    struct ieee80211_frame *wh;
    wbuf_t wbuf = wbuf0;
    int data_len, space;
    uint8_t aad[2 * AES_BLOCK_LEN], b0[AES_BLOCK_LEN], b[AES_BLOCK_LEN],
        s0[AES_BLOCK_LEN];
    uint8_t *pos;
    int mfp;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    mfp = IEEE80211_IS_MFP_FRAME(wh);

    data_len = wbuf_get_pktlen(wbuf) - (hdrlen + ccmp.ic_header) - ccmp.ic_trailer;
    ccmp_init_blocks(&ctx->cc_aes, wh, rx_pn,
                     data_len, b0, aad, b, s0, mfp);

    pos = wbuf_header(wbuf) + hdrlen + ccmp.ic_header;
    /* NB: assumes header is entirely in first mbuf */
    space = wbuf_get_pktlen(wbuf) - (hdrlen + ccmp.ic_header) - ccmp.ic_trailer;
    for (;;) {
        if (space > data_len)
            space = data_len;
        /*
         * Do full blocks.
         */
        while (space >= AES_BLOCK_LEN) {
            CCMP_CALC_MIC(b, pos, AES_BLOCK_LEN);
            pos += AES_BLOCK_LEN, space -= AES_BLOCK_LEN;
            data_len -= AES_BLOCK_LEN;
        }
        if (data_len <= 0)		/* no more data */
            break;
        wbuf = wbuf_next(wbuf);
        if (wbuf == NULL) {		/* last buffer */
            if (space != 0) {
                /*
                 * Short last block.
                 */
                CCMP_CALC_MIC(b, pos, space);
            }
            break;
        }
#if 1 /* assume only one chunk */
        break;
#else
        if (space != 0) {
            uint8_t *pos_next;
            int space_next;
            int len, dl, sp;
            struct mbuf *n;

            /*
             * Block straddles one or more mbufs, gather data
             * into the block buffer b, apply the cipher, then
             * scatter the results back into the mbuf chain.
             * The buffer will automatically get space bytes
             * of data at offset 0 copied in+out by the
             * CCMP_ENCRYPT request so we must take care of
             * the remaining data.
             */
            n = m;
            dl = data_len;
            sp = space;
            for (;;) {
                pos_next = mtod(n, uint8_t *);
                len = min(dl, AES_BLOCK_LEN);
                space_next = len > sp ? len - sp : 0;
                if (n->m_len >= space_next) {
                    /*
                     * This mbuf has enough data; just grab
                     * what we need and stop.
                     */
                    xor_block(b+sp, pos_next, space_next);
                    break;
                }
                /*
                 * This mbuf's contents are insufficient,
                 * take 'em all and prepare to advance to
                 * the next mbuf.
                 */
                xor_block(b+sp, pos_next, n->m_len);
                sp += n->m_len, dl -= n->m_len;
                n = n->m_next;
                if (n == NULL)
                    break;
            }

            CCMP_CALC_MIC(b, pos, space);

            /* NB: just like above, but scatter data to mbufs */
            dl = data_len;
            sp = space;
            for (;;) {
                pos_next = mtod(m, uint8_t *);
                len = min(dl, AES_BLOCK_LEN);
                space_next = len > sp ? len - sp : 0;
                if (m->m_len >= space_next) {
                    xor_block(pos_next, e+sp, space_next);
                    break;
                }
                xor_block(pos_next, e+sp, m->m_len);
                sp += m->m_len, dl -= m->m_len;
                m = m->m_next;
                if (m == NULL)
                    goto done;
            }
            /*
             * Do bookkeeping.  m now points to the last mbuf
             * we grabbed data from.  We know we consumed a
             * full block of data as otherwise we'd have hit
             * the end of the mbuf chain, so deduct from data_len.
             * Otherwise advance the block number (i) and setup
             * pos+space to reflect contents of the new mbuf.
             */
            data_len -= AES_BLOCK_LEN;
            i++;
            pos = pos_next + space_next;
            space = m->m_len - space_next;
        } else {
            /*
             * Setup for next buffer.
             */
            pos = mtod(m, uint8_t *);
            space = m->m_len;
        }
#endif
    }
    xor_block(b, s0, ccmp.ic_trailer);

    // Compare the calculated MIC and the received MIC.
    pos = (u_int8_t *) wbuf_header(wbuf0) + wbuf_get_pktlen(wbuf0) - ccmp.ic_trailer;
    if (memcmp(b, pos, ccmp.ic_trailer) != 0) {
        return 0;   // Bad mic
    }
    return 1;
}
#undef CCMP_CALC_MIC

// Module attachment

void
ieee80211_crypto_register_ccmp(struct ieee80211com *ic)
{
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_CCM))
        ieee80211_crypto_register(ic, &ccmp);
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_CCM_256))
        ieee80211_crypto_register(ic, &ccmp256);
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_GCM))
        ieee80211_crypto_register(ic, &gcmp);
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_GCM_256))
        ieee80211_crypto_register(ic, &gcmp256);
}

void
ieee80211_crypto_unregister_ccmp(struct ieee80211com *ic)
{
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_CCM))
        ieee80211_crypto_unregister(ic, &ccmp);
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_CCM_256))
        ieee80211_crypto_unregister(ic, &ccmp256);
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_GCM))
        ieee80211_crypto_unregister(ic, &gcmp);
    if(ieee80211com_has_ciphercap(ic, IEEE80211_CIPHER_AES_GCM_256))
        ieee80211_crypto_unregister(ic, &gcmp256);
}
#endif
