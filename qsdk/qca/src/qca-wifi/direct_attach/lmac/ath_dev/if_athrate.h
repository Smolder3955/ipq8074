/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
#ifndef _ATH_RATECTRL_H_
#define _ATH_RATECTRL_H_

/*
 * Interface definitions for transmit rate control modules for the
 * Atheros driver.
 *
 * A rate control module is responsible for choosing the transmit rate
 * for each data frame.  Management+control frames are always sent at
 * a fixed rate.
 *
 * Only one module may be present at a time; the driver references
 * rate control interfaces by symbol name.  If multiple modules are
 * to be supported we'll need to switch to a registration-based scheme
 * as is currently done, for example, for authentication modules.
 *
 * An instance of the rate control module is attached to each device
 * at attach time and detached when the device is destroyed.  The module
 * may associate data with each device and each node (station).  Both
 * sets of storage are opaque except for the size of the per-node storage
 * which must be provided when the module is attached.
 *
 * The rate control module is notified for each state transition and
 * station association/reassociation.  Otherwise it is queried for a
 * rate for each outgoing frame and provided status from each transmitted
 * frame.  Any ancillary processing is the responsibility of the module
 * (e.g. if periodic processing is required then the module should setup
 * it's own timer).
 *
 * In addition to the transmit rate for each frame the module must also
 * indicate the number of attempts to make at the specified rate.  If this
 * number is != ATH_TXMAXTRY then an additional callback is made to setup
 * additional transmit state.  The rate control code is assumed to write
 * this additional data directly to the transmit descriptor.
 */
struct ath_softc;
struct ath_node;
struct ath_desc;
struct ath_tx_status;
struct ath_vap;
struct ieee80211_rateset;

#define ATH_RC_PROBE_ALLOWED            0x00000001
#define ATH_RC_MINRATE_LASTRATE         0x00000002
#define ATH_RC_SHORT_PREAMBLE           0x00000004

/* Per packet rate passed in from UMAC 
   Some Enterprise customers want to send
   per packet rate. (This is not same as global 
   fixed rate) */

struct ath_rc_pp {
    u_int8_t rate;
    u_int8_t tries;
};

struct ath_rc_series {
    u_int8_t    rix;
    u_int8_t    tries;
    u_int16_t   flags;
    u_int32_t   max4msframelen;
};

/* rcs_flags definition */
#define ATH_RC_DS_FLAG               0x01
#define ATH_RC_CW40_FLAG             0x02    /* CW 40 */
#define ATH_RC_SGI_FLAG              0x04    /* Short Guard Interval */
#define ATH_RC_HT_FLAG               0x08    /* HT */
#define ATH_RC_RTSCTS_FLAG           0x10    /* RTS-CTS */
#define ATH_RC_TX_STBC_FLAG          0x20    /* TX STBC */
#define ATH_RC_RX_STBC_FLAG          0xC0    /* RX STBC ,2 bits */
#define ATH_RC_RX_STBC_FLAG_S           6
#define ATH_RC_WEP_TKIP_FLAG         0x100    /* WEP/TKIP encryption */
#define ATH_RC_TS_FLAG               0x200
#define ATH_RC_TRAINING_FLAG         0x8000
#define ATH_RC_UAPSD_FLAG            0x400    /* UAPSD Rate Control */
#ifdef  ATH_SUPPORT_TxBF
#define ATH_RC_TXBF_FLAG             0x800
#define ATH_RC_CEC_FLAG              0x3000   /*CEC (Channel Estimation Capability) ,2 bits */
#define ATH_RC_CEC_FLAG_S                12
#define ATH_RC_SOUNDING_FLAG         0x4000
#endif

/*
 * packet duration with maxium allowable retries, in us.
 * The time based retry feature is disabled if this value is 0
 */
#define ATH_RC_DURATION_WITH_RETRIES 0
/*
 * total duration (queue time + retry time) in us, after which a frame is dropped
 * The time based retry feature is disabled if this value is 0
 */
#define ATH_RC_TOTAL_DELAY_TIMEOUT 0

struct ath_hal;

/*
 * Attach/detach a rate control module.
 */
struct atheros_softc *
ath_rate_attach(struct ath_hal *ah);

void
ath_rate_detach(struct atheros_softc *asc);

struct atheros_vap *
ath_rate_create_vap(struct atheros_softc *asc, struct ath_vap *athdev_vap);

void
ath_rate_free_vap(struct atheros_vap *avp);

struct atheros_node *
ath_rate_node_alloc(struct atheros_vap *avp);

void
ath_rate_node_free(struct atheros_node *anode);

/*
 * State storage handling.
 */
/*
 * Initialize per-node state already allocated for the specified
 * node; this space can be assumed initialized to zero.
 */
void ath_rate_node_init(struct atheros_node *);
/*
 * Cleanup any per-node state prior to the node being reclaimed.
 */
void ath_rate_node_cleanup(struct atheros_node *);
/*
 * Return current transmit rate
 */
u_int32_t
ath_rate_node_gettxrate(struct ath_node *an);
/*
 * Return maximum transmit phy rate negotiated i.e. link speed
 */
u_int32_t
ath_rate_getmaxphyrate(struct ath_softc *sc, struct ath_node *an);
/*
 * Update rate control state on station associate/reassociate
 * (when operating as an ap or for nodes discovered when operating
 * in ibss mode).
 */
int
ath_rate_newassoc(struct ath_softc *sc, struct ath_node *an,
                  int isnew, unsigned int capflag,
                  struct ieee80211_rateset *negotiated_rates,
                  struct ieee80211_rateset *negotiated_htrates);
#if QCA_AIRTIME_FAIRNESS
/*
 * Get estimate of airtime using PER and MCS (and NSS and Channel BW)
 */
u_int32_t
ath_rate_atf_airtime_estimate(struct ath_softc *sc,
                              struct ath_node *an, u_int32_t tput, u_int32_t *possible_tput);
#endif
/*
 * Update/reset rate control state for 802.11 state transitions.
 * Important mostly as the analog to ath_rate_newassoc when operating
 * in station mode.
 */
void
ath_rate_newstate(struct ath_softc *sc, struct ath_vap *avp, int up);

/*
 * Update rate control state to reflect the changes in SM power save modes.
 */
void
ath_rate_node_update(struct ath_node *);

/*
 * Transmit handling.
 */
/*
 * Return the transmit info for a data packet.  If multi-rate state
 * is to be setup then try0 should contain a value other than ATH_TXMATRY
 * and ath_rate_setupxtxdesc will be called after deciding if the frame
 * can be transmitted with multi-rate retry.
 */
void
ath_rate_findrate(struct ath_softc *sc, struct ath_node *an,
                  int shortPreamble, u_int32_t frameLen, int numTries,
                  unsigned int rcflag, u_int8_t ac, struct ath_rc_series[],
                  int *isProbe, int isretry, u_int32_t bf_flags, 
                  struct ath_rc_pp *rc_pp);
/*
 * Return rate index for given Dot11 Rate.
 */
u_int8_t
ath_rate_findrateix(struct ath_softc *sc, struct ath_node *an, u_int8_t dot11Rate);

#if UNIFIED_SMARTANTENNA
void
ath_smart_ant_set_fixedrate(struct ath_softc *sc, struct ath_node *an,
                  int shortPreamble, u_int32_t *rate_array, u_int32_t *antenna_array,
                  unsigned int rcflag, u_int8_t ac, struct ath_rc_series series[],
                  int *isProbe, int isretry, u_int32_t bf_flags);

void ath_smart_ant_rate_prepare_rateset(struct ath_softc *sc, struct ath_node *an, void *prate_info);
#endif
/*
 * Update rate control state for a packet associated with the
 * supplied transmit descriptor.  The routine is invoked both
 * for packets that were successfully sent and for those that
 * failed (consult the descriptor for details).
 */
#ifdef ATH_SUPPORT_VOWEXT
void
ath_rate_tx_complete(struct ath_softc *sc, struct ath_node *an,
                     struct ath_desc *ds, struct ath_rc_series series[],
                     u_int8_t ac, int nframes, int nbad, int n_head_fail,
                     int n_tail_fail, int rts_retry_fail, struct ath_rc_pp *pp_rc);
void
ath_rate_tx_complete_11n(struct ath_softc *sc, struct ath_node *an,
                         struct ath_tx_status *ts, struct ath_rc_series rcs[],
                         u_int8_t ac, int nframes, int nbad, int n_head_fail, 
                         int n_tail_fail, int rts_retry_fail, struct ath_rc_pp *pp_rc);
#else
void
ath_rate_tx_complete(struct ath_softc *sc, struct ath_node *an, struct ath_desc *ds,
                     struct ath_rc_series series[], u_int8_t ac, int nframes, int nbad,
                     int rts_retry_fail, struct ath_rc_pp *pp_rc);
void
ath_rate_tx_complete_11n(struct ath_softc *sc, struct ath_node *an,
                         struct ath_tx_status *ts, struct ath_rc_series rcs[],
                         u_int8_t ac, int nframes, int nbad, int rts_retry_fail,
                         struct ath_rc_pp *pp_rc);
#endif


int
ath_rate_table_init(void);

typedef void (*SetDefAntenna_callback)(void *, u_int);

u_int32_t ath_rate_maprix(struct ath_softc *sc, u_int16_t curmode, int isratecode);
int32_t ath_rate_set_mcast_rate(struct ath_softc *sc, u_int32_t req_rate);


/*
 * Return the maximum number of tx chains appropriate for transmitting the
 * rate with the specified index.
 * Most rates have no restrictions on the number of tx chains used, but
 * rates using 64-QAM are limited to use no more chains than the rate's
 * number of spatial streams.  If multiple signals that are only
 * distinguished by cyclic delay diversity are transmitted, the combination
 * of partly-correlated signals causes signal degradation.  This degradation,
 * though small, is large enough to affect 64-QAM's closely-packed
 * constellation points.  Thus, 64-QAM rates are restricted to only use
 * the same number of tx chains as the rate's spatial streams, so each
 * chain transmits an independent signal.
 */
u_int8_t ath_rate_max_tx_chains(struct ath_softc *sc, u_int8_t rix, u_int16_t bfrcsflags);
u_int8_t ath_get_tx_chainmask(struct ath_softc *sc);


u_int32_t ath_ratecode_to_ratekbps(struct ath_softc *sc, int ratecode);

#endif /* _ATH_RATECTRL_H_ */

