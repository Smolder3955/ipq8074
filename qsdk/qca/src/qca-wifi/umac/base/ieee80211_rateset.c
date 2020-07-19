/*
 *
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include <ieee80211_rateset.h>
#include <ieee80211_channel.h>
#include <wlan_mlme_dp_dispatcher.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include <wlan_vdev_mgr_utils_api.h>

static void ieee80211_setbasic_half_rates(struct ieee80211_rateset *rs);
static void ieee80211_setbasic_quarter_rates(struct ieee80211_rateset *rs);

/*
 * Created rate table as it is difficult to access HAL_RATE_TABLE from here
 * This table is to get rate in terms of Kbps for corresponding ratecode
 *
 */
typedef struct {
    int count;
    struct {
        u_int32_t   rateKbps;   /* transfer rate in kbs */
        u_int8_t    ratecode;   /* rate for h/w descriptors */
    } info[12];
} LEGACY_RATE_TABLE;

LEGACY_RATE_TABLE rate_table = {
    12,
    {
            /*           RateKbps   Ratecode  */
		    /*   1 Mb */ {  1000,      0x1b },
			/*   2 Mb */ {  2000,      0x1a },
			/* 5.5 Mb */ {  5500,    0x19 },
			/*  11 Mb */ {  11000,     0x18 },
			/*   6 Mb */ {  6000,      0x0b },
			/*   9 Mb */ {  9000,      0x0f },
			/*  12 Mb */ {  12000,     0x0a },
			/*  18 Mb */ {  18000,     0x0e },
			/*  24 Mb */ {  24000,     0x09 },
			/*  36 Mb */ {  36000,     0x0d },
			/*  48 Mb */ {  48000,     0x08 },
			/*  54 Mb */ {  54000,     0x0c }
    }
};


/*
 * Check if the specified rate set supports ERP.
 * NB: the rate set is assumed to be sorted.
 */
int
ieee80211_iserp_rateset(struct ieee80211com *ic, struct ieee80211_rateset *rs)
{
#define N(a)    (sizeof(a) / sizeof(a[0]))
    static const int rates[] = { 2, 4, 11, 22, 12, 24, 48 };
    int i, j;

    if (rs->rs_nrates < N(rates))
        return 0;
    for (i = 0; i < N(rates); i++) {
        for (j = 0; j < rs->rs_nrates; j++) {
            int r = rs->rs_rates[j] & IEEE80211_RATE_VAL;
            if (rates[i] == r)
                break;
            if (j == rs->rs_nrates)
                return 0;
        }
    }
    return 1;
#undef N
}

static const struct ieee80211_rateset basic11g[IEEE80211_MODE_MAX] = {
    { 0 },                          /* IEEE80211_MODE_AUTO */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11A */
    { 2, {  2,  4         } },      /* IEEE80211_MODE_11B */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11G (mixed b/g) */
    { 0 },                          /* IEEE80211_MODE_FH */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_TURBO_A */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_TURBO_G (mixed b/g) */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT20 */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT20 (mixed b/g) */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT40PLUS */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT40MINUS */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT40PLUS (mixed b/g) */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT40MINUS (mixed b/g) */
};

static const struct ieee80211_rateset basic_rates_all[IEEE80211_MODE_MAX] = {
    { 0 },                          /* IEEE80211_MODE_AUTO */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11A */
    { 2, {  2,  4         } },      /* IEEE80211_MODE_11B */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11G (mixed b/g) */
    { 0 },                          /* IEEE80211_MODE_FH */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_TURBO_A */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_TURBO_G (mixed b/g) */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT20 */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT20 (mixed b/g) */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT40PLUS */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT40MINUS */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT40PLUS (mixed b/g) */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT40MINUS (mixed b/g) */
    { 4, {  2,  4, 11, 22 } },      /* IEEE80211_MODE_11NG_HT40 2Ghz, Auto HT40 */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11NA_HT40 5Ghz, Auto HT40 */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11AC_VHT20 5Ghz, VHT20 */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11AC_VHT40PLUS 5Ghz, VHT40 (Ext ch +1) */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11AC_VHT40MINUS 5Ghz, VHT40 (Ext ch -1) */
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11AC_VHT40  5Ghz, VHT40*/
    { 3, { 12, 24, 48     } },      /* IEEE80211_MODE_11AC_VHT80  5Ghz, VHT80*/
};

static const struct ieee80211_rateset basicpureg[] = {
    { 7, {2, 4, 11, 22, 12, 24, 48 } },
};

static const struct ieee80211_rateset pureg[] = {
    {8, {12, 18, 24, 36, 48, 72, 96, 108} },
};

static const struct ieee80211_rateset basic_half_rates[] = {
    {3, {6, 12, 24} },
};

static const struct ieee80211_rateset basic_quarter_rates[] = {
    {3, {3, 6, 12} },
};

/*
 * Search for 11g only rate
 */
int
ieee80211_find_puregrate(u_int8_t rate)
{
    int i;
    for (i = 0; i < pureg[0].rs_nrates; i++) {
        if (pureg[0].rs_rates[i] == IEEE80211_RV(rate))
            return 1;
    }
    return 0;
}

/*
 * Mark basic rates for the 11g rate table based on the pureg setting
 */
void
ieee80211_setpuregbasicrates(struct ieee80211_rateset *rs)
{
    int i, j;

    for (i = 0; i < rs->rs_nrates; i++) {
        rs->rs_rates[i] &= IEEE80211_RATE_VAL;
        for (j = 0; j < basicpureg[0].rs_nrates; j++) {
            if (basicpureg[0].rs_rates[j] == rs->rs_rates[i]) {
                rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
                break;
            }
        }
    }
}

/*
 * Mark the basic rates for the 11g rate table based on the
 * specified mode.  For 11b compatibility we mark only 11b
 * rates.  There's also a pseudo 11a-mode used to mark only
 * the basic OFDM rates; this is used to exclude 11b stations
 * from an 11g bss.
 */
void
ieee80211_set11gbasicrates(struct ieee80211_rateset *rs, enum ieee80211_phymode mode)
{
    int i, j;

    KASSERT(mode < IEEE80211_MODE_MAX, ("invalid mode %u", mode));
    for (i = 0; i < rs->rs_nrates; i++) {
        rs->rs_rates[i] &= IEEE80211_RATE_VAL;
        for (j = 0; j < basic11g[mode].rs_nrates; j++) {
            if (basic11g[mode].rs_rates[j] == rs->rs_rates[i]) {
                rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
                break;
            }
        }
    }
}

/*
 * Mark the basic rate per basic_rates_all table
 */
void
ieee80211_set_default_basicrates(struct ieee80211_rateset *rs, enum ieee80211_phymode mode)
{
    int i, j;

    KASSERT(mode < IEEE80211_MODE_MAX, ("invalid mode %u", mode));
    for (i = 0; i < rs->rs_nrates; i++) {
        rs->rs_rates[i] &= IEEE80211_RATE_VAL;
        for (j = 0; j < basic_rates_all[mode].rs_nrates; j++) {
            if (basic_rates_all[mode].rs_rates[j] == rs->rs_rates[i]) {
                rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
                break;
            }
        }
    }
}

/* Set basic rate based on supported value from HAL
 * XXX: Is there a better way to do it? */
void
ieee80211_setbasicrates(struct ieee80211_rateset *rs, struct ieee80211_rateset *rs_sup)
{
    int i, j;
    u_int8_t rate;

    for (i = 0; i < rs->rs_nrates; i++) {
        for (j = 0; j < rs_sup->rs_nrates; j++) {
            rate = rs_sup->rs_rates[j];
            if ((rs->rs_rates[i] == (rate & IEEE80211_RATE_VAL)) &&
                (rate & IEEE80211_RATE_BASIC)) {
                /* find the corresponding basic rate */
                rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
            }
        }
    }
}

static void
ieee80211_setbasic_half_rates(struct ieee80211_rateset *rs)
{
    int i, j;

    for (i = 0; i < rs->rs_nrates; i++) {
        rs->rs_rates[i] &= IEEE80211_RATE_VAL;
        for (j = 0; j < basic_half_rates->rs_nrates; j++) {
            if (basic_half_rates->rs_rates[j] == rs->rs_rates[i]) {
                rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
                break;
            }
        }
    }
}

static void
ieee80211_setbasic_quarter_rates(struct ieee80211_rateset *rs)
{
    int i, j;

    for (i = 0; i < rs->rs_nrates; i++) {
        rs->rs_rates[i] &= IEEE80211_RATE_VAL;
        for (j = 0; j < basic_quarter_rates->rs_nrates; j++) {
            if (basic_quarter_rates->rs_rates[j] == rs->rs_rates[i]) {
                rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
                break;
            }
        }
    }
}

static void
ieee80211_sort_rate(struct ieee80211_rateset *rs)
{
#define RV(v)   ((v) & IEEE80211_RATE_VAL)
    int i, j;
    u_int8_t r;
    for (i = 0; i < rs->rs_nrates;i++ ) {
        for (j = i + 1; j < rs->rs_nrates; j++) {
            if (RV(rs->rs_rates[i]) > RV(rs->rs_rates[j])) {
                r = rs->rs_rates[i];
                rs->rs_rates[i] = rs->rs_rates[j];
                rs->rs_rates[j] = r;
            }
        }
    }
#undef RV
}

static void
ieee80211_xsect_rate(struct ieee80211_rateset *rs1, struct ieee80211_rateset *rs2,
             struct ieee80211_rateset *srs)
{
#define RV(v)   ((v) & IEEE80211_RATE_VAL)
    int i, j;
    int k;

    k = 0;
    for (i = 0; i < rs1->rs_nrates; i++) {
        for (j = 0; j < rs2->rs_nrates; j++) {
            if (RV(rs1->rs_rates[i]) == RV(rs2->rs_rates[j])) {
                srs->rs_rates[k++] = rs1->rs_rates[i];
                break;
            }
        }
    }
    srs->rs_nrates = k;
#undef RV
}

static int
ieee80211_brs_rate_check(struct ieee80211_rateset *srs, struct ieee80211_rateset *nrs)
{
#define RV(v)   ((v) & IEEE80211_RATE_VAL)
    int i, j;
    for (i = 0; i < srs->rs_nrates; i++) {
        if (srs->rs_rates[i] & IEEE80211_RATE_BASIC) {
            for (j = 0; j < nrs->rs_nrates; j++) {
                if (RV(srs->rs_rates[i]) == RV(nrs->rs_rates[j])) {
                    break;
                }
            }
            if (j == nrs->rs_nrates) {
                return 0;
            }
        }
    }
    return 1;
#undef RV
}

static int
ieee80211_fixed_rate_check(struct ieee80211_node *ni, struct ieee80211_rateset *nrs)
{
#define RV(v)   ((v) & IEEE80211_RATE_VAL)
    return 1;
#undef RV
}

int
ieee80211_fixed_htrate_check(struct ieee80211_node *ni, struct ieee80211_rateset *nrs)
{
#define RV(v)   ((v) & IEEE80211_RATE_VAL)
    return 1;
#undef RV
}

/* get num of spatial streams from mcs rate */
static INLINE int
ieee80211_mcs_to_numstreams(int mcs)
{
   int numstreams = 0;
   /* single stream mcs rates */
   if ((mcs <= 7) || (mcs == 32))
       numstreams = 1;
   /* two streams mcs rates */
   if (((mcs >= 8) && (mcs <= 15)) || ((mcs >= 33) && (mcs <= 38)))
       numstreams = 2;
   /* three streams mcs rates */
   if (((mcs >= 16) && (mcs <= 23)) || ((mcs >= 39) && (mcs <= 52)))
       numstreams = 3;
   /* four streams mcs rates */
   if (((mcs >= 24) && (mcs <= 31)) || ((mcs >= 53) && (mcs <= 76)))
       numstreams = 4;
   return numstreams;
}

/*
 * Install received rate set information in the node's state block.
 */
int
ieee80211_setup_rates(struct ieee80211_node *ni,
                      const u_int8_t *rates,
                      const u_int8_t *xrates,
                      int flags)
{
    struct ieee80211_rateset *rs = &ni->ni_rates;
    struct ieee80211_rateset rrs, *irs;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;

    memset(&rrs, 0, sizeof(rrs));
    memset(rs, 0, sizeof(*rs));
    rrs.rs_nrates = rates[1];
    OS_MEMCPY(rrs.rs_rates, rates + 2, rrs.rs_nrates);
    if (xrates != NULL) {
        u_int8_t nxrates;
        /*
         * Tack on 11g extended supported rate element.
         */
        nxrates = xrates[1];
        if (rrs.rs_nrates + nxrates > IEEE80211_RATE_MAXSIZE) {

            nxrates = IEEE80211_RATE_MAXSIZE - rrs.rs_nrates;
            IEEE80211_NOTE(vap, IEEE80211_MSG_XRATE, ni,
                "extended rate set too large;"
                " only using %u of %u rates",
                nxrates, xrates[1]);
#ifdef QCA_SUPPORT_CP_STATS
            vdev_cp_stats_rx_rs_too_big_inc(vap->vdev_obj, 1);
#endif
        }
        OS_MEMCPY(rrs.rs_rates + rrs.rs_nrates, xrates+2, nxrates);
        rrs.rs_nrates += nxrates;
    }
    if (IEEE80211_IS_CHAN_HALF(ni->ni_chan)) {
        irs =  IEEE80211_HALF_RATES(ic);
    } else if (IEEE80211_IS_CHAN_QUARTER(ni->ni_chan)) {
        irs =  IEEE80211_QUARTER_RATES(ic);
    } else
        irs =  &vap->iv_op_rates[ieee80211_chan2mode(ni->ni_chan)];

   /*
    * When user will disable any selective rates then irs should be
    * the bss node rate set not the VAP's opearational rate set.
    */
    if (vap->iv_disabled_legacy_rate_set)
        irs = &vap->iv_bss->ni_rates;

    if (flags & IEEE80211_F_DOSORT)
        ieee80211_sort_rate(&rrs);

    if (flags & IEEE80211_F_DOXSECT)
        ieee80211_xsect_rate(irs, &rrs, rs);
    else
        OS_MEMCPY(rs, &rrs, sizeof(rrs));

    if (flags & IEEE80211_F_DOFRATE)
        if (!ieee80211_fixed_rate_check(ni, rs))
            return 0;

    if (flags & IEEE80211_F_DOBRS)
        if (!ieee80211_brs_rate_check(irs, rs))
            return 0;

    /* ieee80211_xsect_rate() called above alters the already sorted
     * rate set. So do the sorting again. */
    if (flags & IEEE80211_F_DOSORT)
        ieee80211_sort_rate(rs);

    return 1;
}

static void
ieee80211_compute_tx_rateset(struct ieee80211com *ic, struct ieee80211_node *ni,
                             struct ieee80211_rateset *trs, u_int8_t tx_streams)
{
    struct ieee80211_rateset *srs = &ic->ic_sup_ht_rates[ieee80211_chan2mode(ni->ni_chan)];
    u_int8_t i, count = 0;

    /*
     *  See tables under section 20.6 of the 802.11n HT (Amendment 5) spec
     */
    OS_MEMSET(trs, 0, sizeof(struct ieee80211_rateset));
    for ( i=0; i < srs->rs_nrates; i++) {
        if ( (srs->rs_rates[i] < 8) || (srs->rs_rates[i] == 32)) {
            trs->rs_rates[count++] = srs->rs_rates[i];
        }
    }
    trs->rs_nrates = count;

    if  (tx_streams > 1) { /* Case 2,3,4 */
        for ( i = 0; i < srs->rs_nrates; i++) {
            if (((srs->rs_rates[i] > 7) && (srs->rs_rates[i] <  16)) ||
                 ((srs->rs_rates[i] > 32) && (srs->rs_rates[i] <  39))) {
                trs->rs_rates[count++] = srs->rs_rates[i];
            }
        }
        trs->rs_nrates = count;
    }

    if (tx_streams > 2) { /* Case 3,4 */
        for ( i = 0; i < srs->rs_nrates; i++) {
            if (((srs->rs_rates[i] > 15) && (srs->rs_rates[i] <  24)) ||
                 ((srs->rs_rates[i] > 38) && (srs->rs_rates[i] <  53))) {
                trs->rs_rates[count++] = srs->rs_rates[i];
            }
        }
        trs->rs_nrates = count;
    }

    if (tx_streams > 3) { /* Case 4 */
        for ( i = 0; i < srs->rs_nrates; i++) {
            if (((srs->rs_rates[i] > 23) && (srs->rs_rates[i] <  32)) ||
                 ((srs->rs_rates[i] > 52) && (srs->rs_rates[i] <  77))) {
                trs->rs_rates[count++] = srs->rs_rates[i];
            }
        }
        trs->rs_nrates = count;
    }

    return;
}


/*
 * Install received ht rate set information in the node's state block.
 */
int
ieee80211_setup_ht_rates(struct ieee80211_node *ni,
                         u_int8_t *ie,
                         int flags)
{
    struct ieee80211_ie_htcap_cmn *htcap = (struct ieee80211_ie_htcap_cmn *)ie;
    struct ieee80211_rateset *rs = &ni->ni_htrates;
    struct ieee80211_rateset rrs, irs;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    int i,j;
    int numstreams, max_numstreams;
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);

    if (tx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        tx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    memset(&rrs, 0, sizeof(rrs));
    memset(rs, 0, sizeof(*rs));
    j = 0;
    max_numstreams = 0;


    if (htcap != NULL) {
        if (ieee80211_ic_wep_tkip_htrate_is_set(ic)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
               &&  RSN_CIPHER_IS_TKIP(&ni->ni_rsn)
#else
               && wlan_crypto_vdev_has_ucastcipher(vap->vdev_obj, (1 << WLAN_CRYPTO_CIPHER_TKIP))
#endif
           ) {
            /*
             * WAR for Tx FIFO underruns with MCS15 in WEP/TKIP mode. Exclude
             * MCS15 from rates if TKIP encryption is set in HT20 mode
             */
            htcap->hc_mcsset[IEEE80211_RX_MCS_2_STREAM_BYTE_OFFSET] &= 0x7f;
        }
        for (i=0; i < IEEE80211_HT_RATE_SIZE; i++) {
            if (htcap->hc_mcsset[i/8] & (1<<(i%8))) {
                if( vap->iv_disable_htmcs && (vap->iv_disabled_ht_mcsset[i/8] & (1<<(i%8))) ) {
                        continue;
                }
                rrs.rs_rates[j++] = i;
                /* update the num of streams supported */
                numstreams = ieee80211_mcs_to_numstreams(i);
                if (max_numstreams < numstreams)
                    max_numstreams = numstreams;
            }
            if (j == IEEE80211_RATE_MAXSIZE) {
                IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_XRATE, ni,
                        "ht extended rate set too large;"
                        " only using %u rates",j);
#ifdef QCA_SUPPORT_CP_STATS
                vdev_cp_stats_rx_rs_too_big_inc(ni->ni_vap->vdev_obj, 1);
#endif
                break;
            }
        }
    }

    ni->ni_streams = MIN(max_numstreams, tx_streams);
    /* RX NSS */
    ni->ni_rxstreams = max_numstreams;
    if (htcap != NULL) {

        if ((htcap->hc_mcsset[IEEE80211_TX_MCS_OFFSET] & IEEE80211_TX_MCS_SET) && (htcap->hc_mcsset[IEEE80211_TX_MCS_OFFSET] & IEEE80211_TX_RX_MCS_SET_NOT_EQUAL) ) {
            ni->ni_txstreams = ((htcap->hc_mcsset[IEEE80211_TX_MCS_OFFSET] & IEEE80211_TX_MAXIMUM_STREAMS_MASK) >> 2) + 1;
        }
        else {
            ni->ni_txstreams = max_numstreams;
        }
    }

    rrs.rs_nrates = j;
    ieee80211_compute_tx_rateset(ic, ni, &irs, tx_streams);

    if (flags & IEEE80211_F_DOSORT)
        ieee80211_sort_rate(&rrs);

    if (flags & IEEE80211_F_DOXSECT)
        ieee80211_xsect_rate(&irs, &rrs, rs);
    else
        OS_MEMCPY(rs, &rrs, sizeof(rrs));

    if (flags & IEEE80211_F_DOFRATE)
        if (!ieee80211_fixed_rate_check(ni, rs))
            return 0;

    if (flags & IEEE80211_F_DOBRS)
        if (!ieee80211_brs_rate_check(&irs, rs))
            return 0;
    return 1;
}

void
ieee80211_get_vht_rates(u_int16_t map, ieee80211_vht_rate_t *vht)
{
    int i = 0;
    u_int8_t n;

    OS_MEMSET(vht,0,sizeof(ieee80211_vht_rate_t));

    for ( ; i < MAX_VHT_STREAMS; i++) {
        n  = map & 0x03;
        vht->rates[i] = n;
        if (n < 3) {
            /*
              This is to get nss based on vht_map,
              for some config e.g. 11,11,01,11, original code will get only nss = 1,
              which's wrong, nss should be 2 in this case.
            */
            vht->num_streams = i + 1;
        }
        map = map >> 2;
    }
}

u_int16_t
ieee80211_get_vht_rate_map(ieee80211_vht_rate_t *vht)
{
    int i = 0;
    u_int16_t map = 0;

    for ( ; i < MAX_VHT_STREAMS; i++) {
        map |= ((vht->rates[i]) << (i*2));
    }
    return(map);
}

static void
ieee80211_get_vht_intersect_rates( ieee80211_vht_rate_t *irs,
                  ieee80211_vht_rate_t *srs, ieee80211_vht_rate_t *drs)
{
    int i = 0;
    irs->num_streams = MIN(srs->num_streams, drs->num_streams);

    /* Intialiaze the rates for all streams with unsupported val (0x3) */
    OS_MEMSET(irs->rates,0x3, MAX_VHT_STREAMS);

    for ( ; i < irs->num_streams; i++ ) {
        irs->rates[i] = MIN(srs->rates[i], drs->rates[i]);
    }
}

static int
ieee80211_vht_basic_rate_check( ieee80211_vht_rate_t *irs, ieee80211_vht_rate_t *brs)
{
    int i = 0;

    if (irs->num_streams < brs->num_streams) {
        return 0;
    }

    for ( ; i < brs->num_streams; i++) {
        if ( irs->rates[i] <  brs->rates[i]) {
            return 0;
        }
    }

    return 1;
}

/* Derive NSS values supported by peer based on VHT capabilities
 * advertised
 * @arg1 - struct ieee80211_node
 * @arg2 - Rx stream of node as advertised in vhtcap
 *
 * Return - 1: Success
 *          0: Failure
 */
bool
ieee80211_derive_nss_from_cap(struct ieee80211_node *ni, ieee80211_vht_rate_t *rx_rrs)
{
    u_int32_t vhtcap = ni->ni_vhtcap;
    bool status = true;

/* Compute NSS values of peer for 160MHz and 80+80MHz based on the "supported channel width"
 * and "EXT NSS support" field combinations advertised in VHT capabilities information IE.
 * NSS support of STA transmitting the VHT Capabilities Information field is determined as
 * a function of PPDU bandwidth (*Max VHT NSS)
 */
    /*
     * Supp_ch_width  EXT_NSS_Supp  Upto_80Mhz  160Mhz  80+80Mhz
     *      0              0           1           -       -
     *      0              1           1          1/2      -
     *      0              2           1          1/2     1/2
     *      0              3           1          3/4     3/4
     *      1              0           1           1       -
     *      1              1           1           1      1/2
     *      1              2           1           1      3/4
     *      1              3           2           2       1
     *      2              0           1           1       1
     *      2              3           2           1       1
     */
    switch (vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK)
    {
	case IEEE80211_EXTNSS_MAP_23_80F2_160F1_80P80F1:
            rx_rrs->num_streams *= 2;
            ni->ni_bw160_nss = (rx_rrs->num_streams / 2);
            ni->ni_bw80p80_nss = (rx_rrs->num_streams / 2);
            break;

        case IEEE80211_EXTNSS_MAP_13_80F2_160F2_80P80F1:
            rx_rrs->num_streams *= 2;
            ni->ni_bw160_nss = rx_rrs->num_streams;
            ni->ni_bw80p80_nss = (rx_rrs->num_streams / 2);
            break;

        case IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75:
            ni->ni_bw160_nss = ((rx_rrs->num_streams * 3) / 4);
            ni->ni_bw80p80_nss = ((rx_rrs->num_streams * 3) / 4);
            break;

	case IEEE80211_EXTNSS_MAP_12_80F1_160F1_80P80FDOT75:
            ni->ni_bw160_nss = (rx_rrs->num_streams);
            ni->ni_bw80p80_nss = ((rx_rrs->num_streams * 3) / 4);
            break;

        case IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5:
            ni->ni_bw160_nss = (rx_rrs->num_streams / 2);
            ni->ni_bw80p80_nss = (rx_rrs->num_streams / 2);
            break;

        case IEEE80211_EXTNSS_MAP_11_80F1_160F1_80P80FDOT5:
            ni->ni_bw160_nss = (rx_rrs->num_streams);
            ni->ni_bw80p80_nss = (rx_rrs->num_streams / 2);
            break;

	case IEEE80211_EXTNSS_MAP_01_80F1_160FDOT5_80P80NONE:
            ni->ni_bw160_nss = (rx_rrs->num_streams / 2);
            ni->ni_bw80p80_nss = 0;
            break;

        case IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1:
            ni->ni_bw160_nss = rx_rrs->num_streams;
            ni->ni_bw80p80_nss = rx_rrs->num_streams;
            break;

        case IEEE80211_EXTNSS_MAP_10_80F1_160F1_80P80NONE:
            ni->ni_bw160_nss = rx_rrs->num_streams;
            ni->ni_bw80p80_nss = 0;
            break;

        default:
            /* Return failure if invalid combination is received. caller will deny assoc */
            status = false;
            break;
    }

    return status;
}

int
ieee80211_setup_vht_rates(struct ieee80211_node *ni,
                         u_int8_t *ie,
                         int flags)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    ieee80211_vht_rate_t tsrs,rsrs,brs,rx_rrs, tx_rrs, tx_irs, rx_irs;
    u_int8_t rx_chainmask = ieee80211com_get_rx_chainmask(ic);
    struct ieee80211_bwnss_map nssmap;
    u_int16_t     tx_mcs_map, rx_mcs_map;

    if (!vap->iv_set_vht_mcsmap) {
        tx_mcs_map = ic->ic_vhtcap_max_mcs.tx_mcs_set.mcs_map;
        rx_mcs_map = ic->ic_vhtcap_max_mcs.rx_mcs_set.mcs_map;
    } else {
        tx_mcs_map = vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map;
        rx_mcs_map = vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map;
    }

    /* Our Supported Tx rates */
    ieee80211_get_vht_rates(tx_mcs_map, &tsrs);
    /* Our Supported Rx rates */
    ieee80211_get_vht_rates(rx_mcs_map, &rsrs);
    /* Our Basic rates */
    ieee80211_get_vht_rates(ic->ic_vhtop_basic_mcs, &brs);

    /* Received Rx Rates - vht parse had already copied the info to ni */
    ieee80211_get_vht_rates(ni->ni_rx_vhtrates, &rx_rrs);
    /* Received Tx Rates - vht parse has already copied this info to ni */
    ieee80211_get_vht_rates(ni->ni_tx_vhtrates, &tx_rrs);

    if (ic->ic_ext_nss_capable) {
         ieee80211_derive_nss_from_cap(ni, &rx_rrs);
     }

    /* Negotiate Rx NSS based on channel BW */
    if (ni->ni_chwidth == IEEE80211_CWM_WIDTH160) {
        qdf_mem_zero(&nssmap, sizeof(nssmap));
        if (!(ni->ni_prop_ie_used) && !(ni->ni_ext_nss_capable)) {
            /* Using VHT160 has advantage only if the max NSS supported in VHT80
             * mode is lese than double the max NSS supported in VHT160 mode. As
             * per systems team, even for same throughput (nss in VHT160 is half
             * of nss in VHT80, it is better to use VHT80 */
            if (!ieee80211_get_bw_nss_mapping(vap, &nssmap, rx_chainmask) &&
                (tx_rrs.num_streams < (nssmap.bw_nss_160) * 2)) {
                tx_rrs.num_streams = nssmap.bw_nss_160;
            } else {
                /* Use 80MHz if the NSS supported at 160MHz is too low */
                ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
            }
        } else {
            if (ni->ni_ext_nss_support) {
                u_int8_t tx_chainmask = ieee80211com_get_tx_chainmask(ic);
                if (!ieee80211_get_bw_nss_mapping(vap, &nssmap, tx_chainmask)) {
                    ni->ni_bw160_nss = QDF_MIN(nssmap.bw_nss_160, ni->ni_bw160_nss);
                    ni->ni_bw80p80_nss = QDF_MIN(nssmap.bw_nss_160, ni->ni_bw80p80_nss);
                }
            } else {
                u_int8_t tx_chainmask = ieee80211com_get_tx_chainmask(ic);
                if (!ieee80211_get_bw_nss_mapping(vap, &nssmap, tx_chainmask)) {
                    ni->ni_bw160_nss = QDF_MIN(nssmap.bw_nss_160, ni->ni_bw160_nss);
                } else {
                    ni->ni_bw160_nss = 0;
                    ni->ni_bw80p80_nss = 0;
                }
           }
        }
    } else {
        ni->ni_bw160_nss = 0;
        ni->ni_bw80p80_nss = 0;
    }

    /* Intersection (SRC TX & DST RX) and (SRC RX & DST TX) */
    if (flags & IEEE80211_F_DOXSECT) {
        /* Rate control needs intersection of SRC TX rates and DST RX rates */
        ieee80211_get_vht_intersect_rates(&tx_irs, &rx_rrs, &tsrs);
        ieee80211_get_vht_intersect_rates(&rx_irs, &tx_rrs, &rsrs);
    } else {
        tx_irs = rx_rrs; /* dst rx rates */
        rx_irs = tx_rrs; /* dst tx rates */
    }

    /* Rate control expects the max streams among the values
     * set in HT, VHT and HE caps. Setting to max of HT and
     * VHT here
     */
    ni->ni_streams = QDF_MAX(ni->ni_streams, tx_irs.num_streams);

    /* Check to ensure that max peer txstreams is taken if the HTCapabilities
     * and VTH Capabilities have populated different rate values */
    if ((rx_irs.num_streams > ni->ni_txstreams)) {
        ni->ni_txstreams = rx_irs.num_streams;
    }

    if ((tx_irs.num_streams > ni->ni_rxstreams)) {
        ni->ni_rxstreams = tx_irs.num_streams;
    }

    ni->ni_tx_vhtrates = ieee80211_get_vht_rate_map(&tx_irs);
    ni->ni_rx_vhtrates = ieee80211_get_vht_rate_map(&rx_irs);

    if (flags & IEEE80211_F_DOBRS) {
        if (!ieee80211_vht_basic_rate_check(&tx_irs, &brs)) {
             IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"%s: Mismatch in Tx basic rate set\n", __func__);
             return 0;
        }

        if (!ieee80211_vht_basic_rate_check(&rx_irs, &brs)) {
             IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"%s: Mismatch in Rx basic rate set\n", __func__);
             return 0;
        }

    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
            "Negotiated: txrates = %x rxrates = %x\n", ni->ni_tx_vhtrates, ni->ni_rx_vhtrates);
    return 1;
}

void
ieee80211_setup_basic_ht_rates(struct ieee80211_node *ni, u_int8_t *ie)
{
#define RV(v)   ((v) & IEEE80211_RATE_VAL)
    struct ieee80211_ie_htinfo_cmn *htinfo = (struct ieee80211_ie_htinfo_cmn *)ie;
    struct ieee80211_rateset *rs = &ni->ni_htrates;
    int i,j;

    if (rs->rs_nrates) {
        for (i=0; i < IEEE80211_HT_RATE_SIZE; i++) {
            if (htinfo->hi_basicmcsset[i/8] & (1<<(i%8))) {
                for (j = 0; j < rs->rs_nrates; j++) {
                    if (RV(rs->rs_rates[j]) == i) {
                        rs->rs_rates[j] |= IEEE80211_RATE_BASIC;
                    }
                }
            }
        }
    } else {
        IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_XRATE, ni,
            "ht rate set %s;", "empty");
    }
#undef RV
}

void
ieee80211_init_rateset(struct ieee80211com *ic)
{

    /*
     * When 11g is supported, force the rate set to
     * include basic rates suitable for a mixed b/g bss.
     */
    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11G))
        ieee80211_set11gbasicrates(
            &ic->ic_sup_rates[IEEE80211_MODE_11G],
            IEEE80211_MODE_11G);

    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT20))
        ieee80211_set11gbasicrates(
            &ic->ic_sup_rates[IEEE80211_MODE_11NG_HT20],
            IEEE80211_MODE_11NG_HT20);

    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40PLUS))
        ieee80211_set11gbasicrates(
            &ic->ic_sup_rates[IEEE80211_MODE_11NG_HT40PLUS],
            IEEE80211_MODE_11NG_HT40PLUS);

    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40MINUS))
        ieee80211_set11gbasicrates(
            &ic->ic_sup_rates[IEEE80211_MODE_11NG_HT40MINUS],
            IEEE80211_MODE_11NG_HT40MINUS);

    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11A)) {
        ieee80211_setbasic_half_rates(IEEE80211_HALF_RATES(ic));
    }

    if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11A)) {
        ieee80211_setbasic_quarter_rates(IEEE80211_QUARTER_RATES(ic));
    }
}

void
ieee80211_rateset_vattach(struct ieee80211vap *vap)
{
    int i;
    struct ieee80211com *ic = vap->iv_ic;
    /*
     * XXX: copy supported rateset to operational rateset
     */
    for (i = 0; i < IEEE80211_MODE_MAX; i++) {
        OS_MEMCPY(&vap->iv_op_rates[i],
                  &ic->ic_sup_rates[i],
                  sizeof(struct ieee80211_rateset));
    }
}

void
ieee80211_init_node_rates(struct ieee80211_node *ni, struct ieee80211_ath_channel *chan)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    if (IEEE80211_IS_CHAN_HALF(chan)) {
        OS_MEMCPY(&ni->ni_rates, IEEE80211_HALF_RATES(ic),
                  sizeof(struct ieee80211_rateset));
        return;
    } else if (IEEE80211_IS_CHAN_QUARTER(chan)) {
        OS_MEMCPY(&ni->ni_rates, IEEE80211_QUARTER_RATES(ic),
                  sizeof(struct ieee80211_rateset));
        return;
    } else {
        OS_MEMCPY(&ni->ni_rates,
                  &vap->iv_op_rates[ieee80211_chan2mode(chan)],
                  sizeof(struct ieee80211_rateset));
    }
    OS_MEMCPY(&ni->ni_htrates,
              &ic->ic_sup_ht_rates[ieee80211_chan2mode(chan)],
              sizeof(struct ieee80211_rateset));
    OS_MEMCPY(&ni->ni_tx_vhtrates,
              &vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map, sizeof(ni->ni_tx_vhtrates));
    OS_MEMCPY(&ni->ni_rx_vhtrates,
              &vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map, sizeof(ni->ni_rx_vhtrates));
   /*
    * Since the VAP operational rates are being overwritten to ni_rates while initiating
    * node rates; removing the rates from the bss node, at which user doesnot want to operate.
    * Applicable in HOSTAP mode only.
    */
    if((ni == vap->iv_bss) && vap->iv_disabled_legacy_rate_set) {
        ieee80211_disable_legacy_rates(vap);
    }
}

int
ieee80211_check_node_rates(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_rateset *srs;

    srs = &vap->iv_op_rates[ieee80211_chan2mode(ni->ni_chan)];
    if (!ieee80211_brs_rate_check(srs, &ni->ni_rates))
        return 0;
    if (!ieee80211_fixed_rate_check(ni, &ni->ni_rates))
        return 0;

    return 1;
}

/*
 * Check rate set suitability
 */
int
ieee80211_check_rate(struct ieee80211vap *vap, struct ieee80211_ath_channel *chan, struct ieee80211_rateset *rrs, int is_ht)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rateset *srs;
    int i,j;
	u_int32_t fixed_rate = 0;
	int fixed_ratecode;

    if (IEEE80211_IS_CHAN_HALF(chan)) {
        srs = &ic->ic_sup_half_rates;
    } else if (IEEE80211_IS_CHAN_QUARTER(chan)) {
        srs = &ic->ic_sup_quarter_rates;
    } else {
        srs = &vap->iv_op_rates[ieee80211_chan2mode(chan)];
    }

    if (vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_LEGACY)  {
		fixed_ratecode=RV(vap->iv_fixed_rate.series);
		for (i = 0; i < rate_table.count; i++) {
		    if (rate_table.info[i].ratecode == fixed_ratecode) {
		        fixed_rate = rate_table.info[i].rateKbps;
		        break;
		    }
		}
		if (i == rate_table.count)
        {
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s: This fixed rate is not suitable for association\n", __func__);
           return 0;
         }

        fixed_rate = (fixed_rate * 2)/1000;  /*Converting from 0.5Kbps to 1Mbps unit*/
        for (i = 0; i < rrs->rs_nrates; i++) {
            if (RV(rrs->rs_rates[i]) == fixed_rate)
                break;
        }
        if (i == rrs->rs_nrates)
            return 0;
    }

    for (i = 0; i < rrs->rs_nrates; i++) {

        if (rrs->rs_rates[i] & IEEE80211_RATE_BASIC) {
            for (j = 0; j < srs->rs_nrates; j++) {
                if (RV(rrs->rs_rates[i]) == RV(srs->rs_rates[j]))
                    break;
            }
            if (j == srs->rs_nrates)
            {
                if(rrs->rs_rates[i] == 0xff && is_ht)
                {
                    continue;
                }
                else {
                    return 0;
                }
            }
        }
    }
    return 1;
#undef RV
}

/*
 * Check HT rate set suitability
 */
int
ieee80211_check_ht_rate(struct ieee80211vap *vap, struct ieee80211_ath_channel *chan, struct ieee80211_rateset *rrs)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rateset srs;
    int i,j;
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);
    struct ieee80211_node *ni = ieee80211_try_ref_bss_node(vap);
    if (NULL == ni)
        return 1;

    if (tx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        tx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    ieee80211_compute_tx_rateset(ic, ni, &srs, tx_streams);

    if ((vap->iv_fixed_rate.mode == IEEE80211_FIXED_RATE_MCS) &&
        (vap->iv_fixed_rate.mode != IEEE80211_FIXED_RATE_NONE)) {
        for (i = 0; i < rrs->rs_nrates; i++) {
            if (RV(rrs->rs_rates[i]) == RV(vap->iv_fixed_rate.series))
                break;
        }
        if (i == rrs->rs_nrates)
            return 0;
    }
    for (i = 0; i < rrs->rs_nrates; i++)
    {
        if (rrs->rs_rates[i] & IEEE80211_RATE_BASIC) {
            for (j = 0; j < srs.rs_nrates; j++) {
                if (RV(rrs->rs_rates[i]) == RV(srs.rs_rates[j]))
                    break;
            }
            if (j == srs.rs_nrates)
                return 0;
        }
    }
    return 1;
#undef RV
}


/* Check for the 11b cck rates only */

int
ieee80211_check_11b_rates(struct ieee80211vap *vap, struct ieee80211_rateset *rrs)
{
    int i;

#define	RV(v)	((v) & IEEE80211_RATE_VAL)
    for (i = 0; i < rrs->rs_nrates; i++) {
      if (RV(rrs->rs_rates[i]) != 0x2 && RV(rrs->rs_rates[i]) != 0x4 &&
          RV(rrs->rs_rates[i]) != 0xb && RV(rrs->rs_rates[i]) != 0x16) {
         return 0;
      }
    }
#undef RV
    return 1;
}

/* Check if 11g rate is available in ni->ni_rates*/
int
ieee80211_node_has_11g_rate(struct ieee80211_node *ni)
{
    int i;
    struct ieee80211_rateset *rs = &ni->ni_rates;

    for (i = 0; i < rs->rs_nrates; i++) {
        if(ieee80211_find_puregrate(IEEE80211_RV(rs->rs_rates[i])))
            return 1;
    }
    return 0;
}

/*
 * Set multicast rate
 */
void
ieee80211_set_mcast_rate(struct ieee80211vap *vap)
{
#define IEEE80211_NG_BASIC_RATE	11000
#define IEEE80211_NA_BASIC_RATE	6000
    vap->iv_mcast_rate = 0;
    if (vap->iv_mcast_fixedrate != 0) {
        vap->iv_mcast_rate = vap->iv_mcast_fixedrate;
    } else {
        if (IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
            vap->iv_mcast_rate = IEEE80211_NG_BASIC_RATE;
        if (IEEE80211_IS_CHAN_5GHZ(vap->iv_bsschan))
            vap->iv_mcast_rate = IEEE80211_NA_BASIC_RATE;
    }
#undef IEEE80211_NG_BASIC_RATE
#undef IEEE80211_NA_BASIC_RATE
    wlan_vdev_set_mcast_rate(vap->vdev_obj, vap->iv_mcast_rate);
}

/*
 * Public Rateset APIs
 */

/*
 * Query supported data rates
 */
int
wlan_get_supported_rates(wlan_dev_t devhandle,
                         enum ieee80211_phymode mode,
                         u_int8_t *rates, u_int32_t len,
                         u_int32_t *nrates)
{
    struct ieee80211com *ic = devhandle;
    struct ieee80211_rateset *rs;
    int i;

    KASSERT(mode != IEEE80211_MODE_AUTO, ("Unsupported PHY mode\n"));

    rs = &ic->ic_sup_rates[mode];
    if (rs->rs_nrates > len)
        return -EINVAL;

    for (i = 0; i < len; i++) {
        rates[i] = IEEE80211_RV(rs->rs_rates[i]);
    }
    *nrates = rs->rs_nrates;

    return 0;
}

/*
 * Query operational data rates
 */
int
wlan_get_operational_rates(wlan_if_t vaphandle,
                           enum ieee80211_phymode mode,
                           u_int8_t *rates, u_int32_t len,
                           u_int32_t *nrates)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rateset *rs;
    int i;

    KASSERT(mode != IEEE80211_MODE_AUTO, ("Unsupported PHY mode\n"));

    rs = &vap->iv_op_rates[mode];
    if (rs->rs_nrates > len)
        return -EINVAL;

    for (i = 0; i < len; i++) {
        rates[i] = IEEE80211_RV(rs->rs_rates[i]);
    }
    *nrates = rs->rs_nrates;

    return 0;
}

/*
 * Set operational data rates
 */
int
wlan_set_operational_rates(wlan_if_t vaphandle,
                           enum ieee80211_phymode mode,
                           u_int8_t *rates, u_int32_t nrates)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rateset *rs, *rs_sup;
    int i;

    KASSERT(mode != IEEE80211_MODE_AUTO, ("Unsupported PHY mode\n"));

    if (nrates > IEEE80211_RATE_MAXSIZE)
        return -EINVAL;

    rs = &vap->iv_op_rates[mode];
    rs_sup = &ic->ic_sup_rates[mode];

    for (i = 0; i < nrates; i++) {
        rs->rs_rates[i] = rates[i];
    }
    rs->rs_nrates = (u_int8_t)nrates;

    /* set basic rate */
    ieee80211_setbasicrates(rs, rs_sup);

    return 0;
}

/*
 * Query operational data rates
 */
int
wlan_get_bss_rates(wlan_if_t vaphandle,
                           u_int8_t *rates, u_int32_t len,
                           u_int32_t *nrates)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *ni = NULL;
    struct ieee80211_rateset *rs;
    int i;

    ni = ieee80211_try_ref_bss_node(vap);
    if (NULL == ni)
        return -EINVAL;
    rs = &ni->ni_rates;
    if (rs->rs_nrates > len){
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    for (i = 0; i < len; i++) {
        rates[i] = IEEE80211_RV(rs->rs_rates[i]);
    }
    *nrates = rs->rs_nrates;
    ieee80211_free_node(ni);

    return 0;
}

/*
 * Query operational data rates
 */
int
wlan_get_bss_ht_rates(wlan_if_t vaphandle,
                           u_int8_t *rates, u_int32_t len,
                           u_int32_t *nrates)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *ni = NULL;
    struct ieee80211_rateset *rs;
    int i;

    ni = ieee80211_try_ref_bss_node(vap);
    if (NULL == ni)
        return -EINVAL;
    rs = &ni->ni_htrates;
    if (rs->rs_nrates > len){
        ieee80211_free_node(ni);
        return -EINVAL;
    }

    for (i = 0; i < len; i++) {
        rates[i] = IEEE80211_RV(rs->rs_rates[i]);
    }
    *nrates = rs->rs_nrates;
    ieee80211_free_node(ni);

    return 0;
}

/*
 * Query multiple stream from data rates
 */
u_int8_t
ieee80211_is_multistream(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    u_int8_t             rate;
    u_int8_t             has_multistream;

    rate = ni->ni_htrates.rs_rates[ni->ni_htrates.rs_nrates - 1];
    if  (rate > IEEE80211_RATE_SINGLE_STREAM_MCS_MAX)
        has_multistream = (u_int8_t) (ic->ic_rx_chainmask > 1);
    else
        has_multistream = 0;

    return has_multistream;
}

/*
 * Compute NSS for a given chain mask
 * Host-side default maxNSS values to be computed.
 *
 * 1. maxNSS_le80MHz (maxNSS for <= 80 MHz)
 * maxNSS_le80MHz = total number of bits set to 1 in chainmask
 *
 * 2. maxNSS_160MHz (maxNSS for 160/80+80 MHz)
 * maxNSS_160MHz = maxNSS_le80MHz/2 (with rounding down)
 *
 * 3. maxNSS_le80MHz_sadfs (maxNSS for <= 80 MHz, applicable when static aDFS is enabled)
 * maxNSS_le80MHz_sadfs = maxNSS_le80MHz – 1
 *
 * 4. maxNSS_160MHz_sadfs (maxNSS for 160/80+80 MHz, applicable when static aDFS is enabled)
 * maxNSS_160MHz_sadfs = (maxNSS_le80MHz – 1)/2    (with rounding down)
 *
 */
u_int8_t
ieee80211_compute_nss(struct ieee80211com *ic, u_int8_t chainmask, struct ieee80211_bwnss_map *nssmap)
{
    u_int8_t nss_80 = 0;
    u_int32_t mask = chainmask;

    /* Find number of bits SET */
    while (mask)
    {
        mask = (mask & (mask-1));
        nss_80++;
    }
    nssmap->bw_nss_80 = nss_80;
    nssmap->bw_nss_160 = nss_80 >> 1;
    /* TODO: SADFS  need to be added once feature is in */
    return 0;
}
/*
 * Get number of usable streams.  It is a function of number
 * of spatial streams and configured chain mask.
 */
u_int8_t
ieee80211_getstreams(struct ieee80211com *ic, u_int8_t chainmask)
{
    /* Number of streams is equal to total number of bits set to 1 in chainmask */
    u_int8_t streams = 0;
    u_int32_t mask = chainmask;

    /* Find number of bits SET */
    while (mask)
    {
        mask = (mask & (mask-1));
        streams++;
    }
    /*
     * keep ic_spatialstreams based code for older chipsets undisturbed.
     */
    if (!ic->ic_is_mode_offload(ic)) {
        switch (chainmask) {
            case 7:    /* 0111 */
            case 11:   /* 1011 */
            case 13:   /* 1101 */
            case 14:   /* 1110 */
                switch (ic->ic_spatialstreams) {
                    default:
                    case 1:
                        streams = 1;
                        break;
                    case 2:
                        streams = 2;
                        break;
                    case 3:
                    case 4:
                        /* 4 streams limited to 3 by chainmask */
                        streams = 3;
                        break;
                }
                break;
            default:
                break;
        }
    }
    return streams;
}
/*
 * Dropping the selected rates that user wants to disable in the node itself.
 * Also selecting the lowest available basic rate for mgmt rate and RTS CTS rate.
 */
int
ieee80211_disable_legacy_rates(struct ieee80211vap *vap)
{
    struct ieee80211_node *ni = vap->iv_bss;
    u_int8_t i, j = 0,k,is_mgt_rate_set = 0;
    u_int16_t disabled_legacy_rate_set = 0;
    int rate_in_kbps,found = 0;
    bool value;
    uint32_t bcn_rate;
    uint32_t mgmt_rate;
    struct wlan_vdev_mgr_cfg mlme_cfg;

    switch (vap->iv_des_mode)
    {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_TURBO_A:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NA_HT40:
        case IEEE80211_MODE_11AC_VHT20:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AC_VHT80_80:
        case IEEE80211_MODE_11AXA_HE20:
        case IEEE80211_MODE_11AXA_HE40PLUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
        case IEEE80211_MODE_11AXA_HE40:
        case IEEE80211_MODE_11AXA_HE80:
        case IEEE80211_MODE_11AXA_HE160:
        case IEEE80211_MODE_11AXA_HE80_80:
            /* In above phy modes, the bits which are applicable for CCK rates (1,2,5.5 and 11 Mbps) needed to be avoided */
            disabled_legacy_rate_set = vap->iv_disabled_legacy_rate_set >> 4;
            break;
        default:
            disabled_legacy_rate_set = vap->iv_disabled_legacy_rate_set;
            if (!vap->iv_ic->cck_tx_enable) {
                qdf_print("Disabling CCK Rates");
                disabled_legacy_rate_set |= 0xF;
            }
            break;
    }

    for (i = 0; i < ni->ni_rates.rs_nrates; i++) {
        if (disabled_legacy_rate_set & (1<<i))
            continue;
        if (!is_mgt_rate_set) {
            for (k = 0; k < basicpureg[0].rs_nrates; k++) {
                if ((ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) == basicpureg[0].rs_rates[k]) {
                    mlme_cfg.value = ((basicpureg[0].rs_rates[k] * 1000) / 2);
                    wlan_util_vdev_mlme_set_param(vap->vdev_mlme,
                            WLAN_MLME_CFG_TX_MGMT_RATE, mlme_cfg);
                    is_mgt_rate_set = 1;
                    break;
                }
            }
        }
        ni->ni_rates.rs_rates[j++] = ni->ni_rates.rs_rates[i];
    }
    ni->ni_rates.rs_nrates = j;

    /* This code will work when a user has already set a specific rate for
     * beacon and then try to disable some rates.
     *
     * If user is not disabling the rate that is already set as a beacon rate
     * then beacons will continue with that rate; but if user will disable
     * the modified rate for beacon itself, then beacons will choose the
     * lowest available basic rate.
     *
     * If no beacon rate configured already, we populate beacon rate to be
     * the same as management rate calculated from above. This is required
     * for latest FW versions as MGMT vdev param does not configure the beacon
     * frame rates. As a result of this assignment, beacon rate set vdev param
     * will be sent to target during vap initialization.
     */

    wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
            WLAN_MLME_CFG_BCN_TX_RATE, &bcn_rate);
    wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
            WLAN_MLME_CFG_TX_MGMT_RATE, &mgmt_rate);
    if (bcn_rate) {
        for (i = 0; i < ni->ni_rates.rs_nrates; i++) {
            rate_in_kbps = (((ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL)* 1000) / 2);

            if (bcn_rate == rate_in_kbps) {
                found = 1;
                break;
            }
        }

        if(!(vap->iv_ic->ic_is_mode_offload(vap->iv_ic))){
            if (found) {
                value = vap->iv_modify_bcn_rate(vap);
            } else {
                mlme_cfg.value = mgmt_rate;
                wlan_util_vdev_mlme_set_param(vap->vdev_mlme,
                        WLAN_MLME_CFG_BCN_TX_RATE, mlme_cfg);
                value = vap->iv_modify_bcn_rate(vap);

                qdf_print("%s:MSG: User has disabled the previously set modified rate for beacon.",__func__);
                qdf_info("%s:MSG: Hence going for the lowest available basic rate for beacon: %d (Kbps)",__func__, mlme_cfg.value);
            }

            if(value == false){
                qdf_print("%s : This rate is not allowed. Please try a valid rate.",__func__);
                return -EINVAL;
            }
        } else {
            if (!found) {
                mlme_cfg.value = mgmt_rate;
                wlan_util_vdev_mlme_set_param(vap->vdev_mlme,
                        WLAN_MLME_CFG_BCN_TX_RATE, mlme_cfg);

                qdf_print("%s:MSG: User has disabled the previously set modified rate for beacon.",__func__);
                qdf_info("%s:MSG: Hence going for the lowest available basic rate for beacon: %d (Kbps)",__func__, mlme_cfg.value);
            }
        }
    } else {
        mlme_cfg.value = mgmt_rate;
        wlan_util_vdev_mlme_set_param(vap->vdev_mlme,
                WLAN_MLME_CFG_BCN_TX_RATE, mlme_cfg);
    }

    return 0;
}
