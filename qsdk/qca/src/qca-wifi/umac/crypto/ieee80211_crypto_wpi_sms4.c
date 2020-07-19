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
/*
 * WPI SMS4 Crypto support
 */
#include <osdep.h>

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#if ATH_SUPPORT_WAPI

#include "ieee80211_crypto_wpi_sms4_priv.h"

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
static	int nrefs = 0;

static	void *wpi_sms4_attach(struct ieee80211vap *, struct ieee80211_key *);
static	void wpi_sms4_detach(struct ieee80211_key *);
static	int wpi_sms4_setkey(struct ieee80211_key *);
static	int wpi_sms4_encap(struct ieee80211_key *, wbuf_t, u_int8_t);
static  int wpi_sms4_decap(struct ieee80211_key *, wbuf_t, int, struct ieee80211_rx_status *);
static	int wpi_sms4_enmic(struct ieee80211_key *, wbuf_t, int, bool);
static	int wpi_sms4_demic(struct ieee80211_key *, wbuf_t, int, int, struct ieee80211_rx_status *);

static const struct ieee80211_cipher wpi_sms4 = {
	"WPI_SMS4",
	IEEE80211_CIPHER_WAPI,
	IEEE80211_WPI_SMS4_KIDLEN +IEEE80211_WPI_SMS4_PADLEN + IEEE80211_WPI_SMS4_IVLEN,
	IEEE80211_WPI_SMS4_MICLEN,
	0,
	wpi_sms4_attach,
	wpi_sms4_detach,
	wpi_sms4_setkey,
	wpi_sms4_encap,
	wpi_sms4_decap,
	wpi_sms4_enmic,
	wpi_sms4_demic,
};
#if 0	
int wpi_cipher(
	u_int8_t *in,
	u_int8_t *out,
	int enc_length,
	int data_type,
	int QOS_en,
	int Endian_mode
	);
#endif
static void *
wpi_sms4_attach(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    struct sms4_ctx *ctx;
    struct ieee80211com *ic = vap->iv_ic;

    ctx = (struct sms4_ctx *)OS_MALLOC(ic->ic_osdev, sizeof(struct sms4_ctx), GFP_KERNEL);
    if (ctx == NULL) {
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_nomem_inc(vap->vdev_obj, 1);
#endif
        return NULL;
    }
	ctx->sms4c_vap = vap;
	ctx->sms4c_ic = vap->iv_ic;
	nrefs++;
    
    return ctx;
}

static void
wpi_sms4_detach(struct ieee80211_key *k)
{
    struct sms4_ctx *ctx = k->wk_private;

    KASSERT(nrefs > 0, ("imbalanced attach/detach"));
    OS_FREE(ctx);
    nrefs--;			/* NB: we assume caller locking */
}

static int
wpi_sms4_setkey(struct ieee80211_key *k)
{
    struct sms4_ctx *ctx = k->wk_private;
	if (k->wk_keylen != (256/NBBY)) 
	{
        IEEE80211_DPRINTF(ctx->sms4c_vap, IEEE80211_MSG_CRYPTO,
                          "%s: Invalid key length %u, expecting %u\n",
                          __func__, k->wk_keylen, 256/NBBY);
		return 0;
	}
	k->wk_keytsc = 1;		/* TSC starts at 1 */
	return 1;
}

void update_send_iv(u_int32_t *iv, int type)
{
	longint_add(iv, type+1, IEEE80211_WPI_SMS4_IVLEN/4);
}

/*
 * Add privacy headers appropriate for the specified key.
 */

static int
wpi_sms4_encap(struct ieee80211_key *k, wbuf_t wbuf, u_int8_t keyid)
{
    struct sms4_ctx *ctx = k->wk_private;
	struct ieee80211vap *vap = ctx->sms4c_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_frame *wh;
	int hdrlen;
	u_int8_t iv[IEEE80211_WPI_SMS4_IVLEN] = {0,};
	int ismsk = 0;
    u_int8_t *ivp;
    int i;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	hdrlen = ieee80211_hdrspace(ic, wbuf_header(wbuf));

	if((wh->i_addr1[0] & 0x01) != 0)
	{
		update_send_iv((u_int32_t *)k->wk_txiv, 0);
		ismsk = 1;
	}
	else
	{
		update_send_iv((u_int32_t *)k->wk_txiv, 1);
	}
	qdf_mem_copy(iv, k->wk_txiv, sizeof(k->wk_txiv));

#ifndef QCA_PARTNER_PLATFORM
    ivp = (u_int8_t *)wbuf_push(wbuf, wpi_sms4.ic_header);
    memmove(ivp, ivp + wpi_sms4.ic_header, hdrlen);
#else
    if (wbuf_is_encap_done(wbuf)) {
        ivp = (u_int8_t *)wbuf_header(wbuf);
    } else {
        ivp = (u_int8_t *)wbuf_push(wbuf, wpi_sms4.ic_header);
        memmove(ivp, ivp + wpi_sms4.ic_header, hdrlen);
    }
#endif

    ivp += hdrlen;
    ivp[0] = (keyid >> 6) & 0xFF;
    ivp[1] = 0;
    ivp += 2;
   	for(i=0; i<IEEE80211_WPI_SMS4_IVLEN; i++)
	{
		ivp[i] = (iv[IEEE80211_WPI_SMS4_IVLEN-1-i])&0xff;	
	}
    /*
     * MIC will be appended by DMA (Tx desc control register 8: ds_ctl6),
     * so encap function does not need to append the MIC data to wbuf here.
     */
    k->wk_keytsc++;
	return 1;
}

/*
 * Add MIC to the frame as needed.
 */
static int
wpi_sms4_enmic(struct ieee80211_key *k, wbuf_t wbuf, int force, bool encap)
{
	return 1;
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame. The specified key should be correct but
 * is also verified.
 */
static int
wpi_sms4_decap(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen, struct ieee80211_rx_status *rs)
{
    struct sms4_ctx *ctx = k->wk_private;
	struct ieee80211vap *vap = ctx->sms4c_vap;
	struct ieee80211_frame *wh = NULL;
    u_int8_t  *iv;
    int i, tid;
	u_int8_t ivp[IEEE80211_WPI_SMS4_IVLEN]={0,};
    int ismcast;
    struct ieee80211_mac_stats *mac_stats;
	/*
	 * Header should have extended IV and sequence number;
	 * verify the former and validate the latter.
	 */
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
    mac_stats = ismcast? &vap->iv_multicast_stats : &vap->iv_unicast_stats;

    /* Get TID */
    tid = IEEE80211_NON_QOS_SEQ;
    if (IEEE80211_QOS_HAS_SEQ(wh)) {
        if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS ) {
            tid = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        } else {
            tid = ((struct ieee80211_qosframe *)wh)->i_qos[0] & IEEE80211_QOS_TID;
        }
    }
    

    /*
	 * Check if the device handled the decrypt in hardware.
	 * If so we just strip the header; otherwise we need to
	 * handle the decrypt in software.  Note that for the
	 * latter we leave the header in place for use in the
	 * decryption work.
	 */
    if (rs->rs_flags & IEEE80211_RX_DECRYPT_ERROR) {
        /* 
         * Drop frames failed decryption in hardware
         * Prevent update stale iv after rekey
         */
#ifdef QCA_SUPPORT_CP_STATS
        ismcast ? vdev_mcast_cp_stats_rx_wpimic_inc(vap->vdev_obj, 1):
                  vdev_ucast_cp_stats_rx_wpimic_inc(vap->vdev_obj, 1);
        WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_wpimic);
#endif
        return 0;
    }

    /*
     * Copy up 802.11 header and strip crypto bits.
     */

    iv = wbuf_header(wbuf) + hdrlen + 2;/*header for wapi +2 = IV*/
	
	for(i=0; i<IEEE80211_WPI_SMS4_IVLEN; i++)
	{
		ivp[i] = (iv[IEEE80211_WPI_SMS4_IVLEN-1-i])&0xff;	
	}
	if(memcmp(k->wk_recviv, ivp, IEEE80211_WPI_SMS4_IVLEN) >= 0)
	{
		/*WPI replay*/
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, 
                        wh->i_addr1, "%s", "WAPI IV replay attack");
#ifdef QCA_SUPPORT_CP_STATS
                ismcast ? vdev_mcast_cp_stats_rx_wpireplay_inc(vap->vdev_obj, 1):
                          vdev_ucast_cp_stats_rx_wpireplay_inc(vap->vdev_obj, 1);
#endif
		return 0;
	} else {
	    if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            if((ivp[15] & 0x01 ) != 0) {
                IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                    wh->i_addr1, "%s",  "AP WAPI IV is even");
                return 0;
            }
        }
        else {
            if(ismcast) {
                if(memcmp(k->wk_recviv, ivp, IEEE80211_WPI_SMS4_IVLEN) >= 0) {
                    /*WPI replay*/
                    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, 
                                    wh->i_addr1, "%s", "STA WAPI IV replay attack");
#ifdef QCA_SUPPORT_CP_STATS
                    ismcast ? vdev_mcast_cp_stats_rx_wpireplay_inc(vap->vdev_obj, 1):
                              vdev_ucast_cp_stats_rx_wpireplay_inc(vap->vdev_obj, 1);
#endif
                    return 0;
                }
            } else {
                if((ivp[15] & 0x01 ) == 0) {
                    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                        wh->i_addr1, "%s",  "STA WAPI IV is odd");
                    return 0;
                }
            }
        }            
	}    
    memcpy(k->wk_recviv, ivp, IEEE80211_WPI_SMS4_IVLEN);

    memmove(wbuf_header(wbuf) + wpi_sms4.ic_header, wbuf_header(wbuf), hdrlen);
    wbuf_pull(wbuf, wpi_sms4.ic_header);
    while (wbuf_next(wbuf) != NULL)
        wbuf = wbuf_next(wbuf);
    wbuf_trim(wbuf, wpi_sms4.ic_trailer);
    /*
     * Ok to update rsc now.
     */
    k->wk_keyrsc[tid] ++;
    return 1;
}

/*
 * Verify and strip MIC from the frame.
 */
static int
wpi_sms4_demic(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen, int force, struct ieee80211_rx_status * rs)
{
	return 1;
}

/*
 * Module attach
 */
void
ieee80211_crypto_register_sms4(struct ieee80211com *ic)
{
    ieee80211_crypto_register(ic, &wpi_sms4);
    return;
}

void
ieee80211_crypto_unregister_sms4(struct ieee80211com *ic)
{
    ieee80211_crypto_unregister(ic, &wpi_sms4);
    return;
}
#endif
#endif /*ATH_SUPPORT_WAPI*/
