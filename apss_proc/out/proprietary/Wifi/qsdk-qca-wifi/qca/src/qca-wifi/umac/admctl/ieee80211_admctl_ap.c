/*
 * Copyright (c) 2013,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 *  WMM-AC AP traffic stream handling routines.
 */

#include "ieee80211_admctl_priv.h"

#if UMAC_SUPPORT_ADMCTL


struct ieee80211_admctl_rt ieee80211_admctl_rt_11a = {
	8, /* number of rates */
	{
 /*                                        	         ctrl tx Bits/  	*/
     /*                                     Kbps   RC    Rate Symbol 		*/
     /* 4   48 Mb */  { IEEE80211_T_OFDM,  48000,  0x00,   1,  192},
     /* 5   24 Mb */  { IEEE80211_T_OFDM,  24000,  0x01,   1,   96},
     /* 6   12 Mb */  { IEEE80211_T_OFDM,  12000,  0x02,   2,   48},
     /* 7    6 Mb */  { IEEE80211_T_OFDM,   6000,  0x03,   3,   24},
     /* 8   54 Mb */  { IEEE80211_T_OFDM,  54000,  0x04,   1,  216},
     /* 9   36 Mb */  { IEEE80211_T_OFDM,  36000,  0x05,   1,  144},
     /* 10   18 Mb */ { IEEE80211_T_OFDM,  18000,  0x06,   2,   72},
     /* 11    9 Mb */ { IEEE80211_T_OFDM,   9000,  0x07,   4,   36},
	}
};
struct ieee80211_admctl_rt ieee80211_admctl_rt_11b = {
	4, /* number of rates */
	{
 /*                                        	         ctrl tx Bits/  	*/
     /*                                     Kbps   RC    Rate Symbol 		*/
     /* 0   11 Mb */  { IEEE80211_T_CCK,   11000,  0x40,   3,     1},
     /* 1  5.5 Mb */  { IEEE80211_T_CCK,    5500,  0x41,   3,     1},
     /* 2    2 Mb */  { IEEE80211_T_CCK,    2000,  0x42,   3,     1},
     /* 3    1 Mb */  { IEEE80211_T_CCK,    1000,  0x43,   0,     1},
	}
};
struct ieee80211_admctl_rt ieee80211_admctl_rt_11g = {
	12, /* number of rates */
	{
 /*                                        	         ctrl tx Bits/  	*/
     /*                                     Kbps   RC    Rate Symbol 		*/
     /* 0   11 Mb */  { IEEE80211_T_CCK,   11000,  0x40,   0,     1},
     /* 1  5.5 Mb */  { IEEE80211_T_CCK,    5500,  0x41,   2,     1},
     /* 2    2 Mb */  { IEEE80211_T_CCK,    2000,  0x42,   3,     1},
     /* 3    1 Mb */  { IEEE80211_T_CCK,    1000,  0x43,   3,     1},
 	 /* 4   48 Mb */  { IEEE80211_T_OFDM,  48000,  0x00,   5,    192},
     /* 5   24 Mb */  { IEEE80211_T_OFDM,  24000,  0x01,   5,     96},
     /* 6   12 Mb */  { IEEE80211_T_OFDM,  12000,  0x02,   6,     48},
     /* 7    6 Mb */  { IEEE80211_T_OFDM,   6000,  0x03,   7,     24},
     /* 8   54 Mb */  { IEEE80211_T_OFDM,  54000,  0x04,   5,    216},
     /* 9   36 Mb */  { IEEE80211_T_OFDM,  36000,  0x05,   5,    144},
     /* 10   18 Mb */ { IEEE80211_T_OFDM,  18000,  0x06,   6,     72},
     /* 11    9 Mb */ { IEEE80211_T_OFDM,   9000,  0x07,   7,     36},
	}
};
struct ieee80211_admctl_rt ieee80211_admctl_rt_11na = {
	56, /* number of rates */
	{
 /*                                        	         ctrl   tx Bits/  	*/
     /*                                     Kbps   RC    Rate   Symbol 		*/
     /* 4   48 Mb */  { IEEE80211_T_OFDM,  48000,  0x00,   1,   192},
     /* 5   24 Mb */  { IEEE80211_T_OFDM,  24000,  0x01,   1,    96},
     /* 6   12 Mb */  { IEEE80211_T_OFDM,  12000,  0x02,   2,    48},
     /* 7    6 Mb */  { IEEE80211_T_OFDM,   6000,  0x03,   3,    24},
     /* 8   54 Mb */  { IEEE80211_T_OFDM,  54000,  0x04,   1,   216},
     /* 9   36 Mb */  { IEEE80211_T_OFDM,  36000,  0x05,   1,   144},
     /* 10   18 Mb */ { IEEE80211_T_OFDM,  18000,  0x06,   2,    72},
     /* 11    9 Mb */ { IEEE80211_T_OFDM,   9000,  0x07,   3,    36},
/* HT rates */
     /*12  6.5 Mb */ {  IEEE80211_T_HT,     6500,  0x80,   3,    26},
     /*13   13 Mb */ {  IEEE80211_T_HT,    13000,  0x81,   2,    52},
     /*14 19.5 Mb */ {  IEEE80211_T_HT,    19500,  0x82,   2,    78},
     /*15   26 Mb */ {  IEEE80211_T_HT,    26000,  0x83,   1,   104},
     /*16   39 Mb */ {  IEEE80211_T_HT,    39000,  0x84,   1,   156},
     /*17   52 Mb */ {  IEEE80211_T_HT,    52000,  0x85,   1,   208},
     /*18 58.5 Mb */ {  IEEE80211_T_HT,    58500,  0x86,   1,   234},
     /*19   65 Mb */ {  IEEE80211_T_HT,    65000,  0x87,   1,   260},
/* DS */
     /*20   13 Mb */ {  IEEE80211_T_HT,    13000,  0x90,    2,   52},
     /*21   26 Mb */ {  IEEE80211_T_HT,    26000,  0x91,    2,  104},
     /*22   39 Mb */ {  IEEE80211_T_HT,    39000,  0x92,    2,  156},
     /*23   52 Mb */ {  IEEE80211_T_HT,    52000,  0x93,    1,  208},
     /*24   78 Mb */ {  IEEE80211_T_HT,    78000,  0x94,    1,  312},
     /*25  104 Mb */ {  IEEE80211_T_HT,   104000,  0x95,    1,  416},
     /*26  117 Mb */ {  IEEE80211_T_HT,   117000,  0x96,    1,  468},
     /*27  130 Mb */ {  IEEE80211_T_HT,   130000,  0x97,    1,  520},
/* TS */
     /*28   19.5 Mb*/{  IEEE80211_T_HT,    19500,   0xa0,   2,   78},
     /*29   39 Mb */ {  IEEE80211_T_HT,    39000,   0xa1,   1,  156},
     /*30   58.5 Mb*/{  IEEE80211_T_HT,    58500,   0xa2,   1,  234},
     /*31   78 Mb */ {  IEEE80211_T_HT,    78000,   0xa3,   1,  312},
     /*32   117 Mb */{  IEEE80211_T_HT,   117000,   0xa4,   1,  468},
     /*33  156 Mb */ {  IEEE80211_T_HT,   156000,   0xa5,   1,  624},
     /*34  175.5 Mb*/{  IEEE80211_T_HT,   175500,   0xa6,   1,  702},
     /*35  195 Mb */ {  IEEE80211_T_HT,   195000,   0xa7,   1,  780},
/* 11n HT 40 rates */
     /*36 13.5 Mb */ {  IEEE80211_T_HT,    13500,   0x80,   2,   54},
     /*37 27.0 Mb */ {  IEEE80211_T_HT,    27000,   0x81,   1,  108},
     /*38 40.5 Mb */ {  IEEE80211_T_HT,    40500,   0x82,   1,  162},
     /*39   54 Mb */ {  IEEE80211_T_HT,    54000,   0x83,   1,  216},
     /*40   81 Mb */ {  IEEE80211_T_HT,    81500,   0x84,   1,  324},
     /*41  108 Mb */ {  IEEE80211_T_HT,   108000,   0x85,   1,  432},
     /*42 121.5Mb */ {  IEEE80211_T_HT,   121500,   0x86,   1,  486},
     /*43  135 Mb */ {  IEEE80211_T_HT,   135000,   0x87,   1,  540},
/* DS */
     /*44   27 Mb */ {  IEEE80211_T_HT,    27000,   0x90,   1,  108},
     /*45   54 Mb */ {  IEEE80211_T_HT,    54000,   0x91,   1,  216},
     /*46   81 Mb */ {  IEEE80211_T_HT,    81000,   0x92,   1,  324},
     /*47  108 Mb */ {  IEEE80211_T_HT,   108000,   0x93,   1,  432},
     /*48  162 Mb */ {  IEEE80211_T_HT,   162000,   0x94,   1,  648},
     /*49  216 Mb */ {  IEEE80211_T_HT,   216000,   0x95,   1,  864},
     /*50  243 Mb */ {  IEEE80211_T_HT,   243000,   0x96,   1,  972},
     /*51  270 Mb */ {  IEEE80211_T_HT,   270000,   0x97,   1, 1080},
/* TS */
     /*52  40.5 Mb */{  IEEE80211_T_HT,    40500,   0xa0,   1,  162},
     /*53   54 Mb */ {  IEEE80211_T_HT,    81000,   0xa1,   1,  324},
     /*54   81 Mb */ {  IEEE80211_T_HT,   121500,   0xa2,   1,  486},
     /*55  108 Mb */ {  IEEE80211_T_HT,   162000,   0xa3,   1,  648},
     /*56  162 Mb */ {  IEEE80211_T_HT,   243000,   0xa4,   1,  972},
     /*57  216 Mb */ {  IEEE80211_T_HT,   324000,   0xa5,   1, 1296},
     /*58  243 Mb */ {  IEEE80211_T_HT,   364500,   0xa6,   1, 1458},
     /*59  270 Mb */ {  IEEE80211_T_HT,   405000,   0xa7,   1, 1620},
	}
};

struct ieee80211_admctl_rt ieee80211_admctl_rt_11ng = {
	60, /* number of rates */
	{
	 /*                                        	         ctrl   tx Bits/  	*/
     /*                                     Kbps   RC    Rate   Symbol 		*/
     /* 0   11 Mb */  { IEEE80211_T_CCK,   11000,  0x40,   0,   1},
     /* 1  5.5 Mb */  { IEEE80211_T_CCK,    5500,  0x41,   2,   1},
     /* 2    2 Mb */  { IEEE80211_T_CCK,    2000,  0x42,   2,   1},
     /* 3    1 Mb */  { IEEE80211_T_CCK,    1000,  0x43,   3,   1},
 	 /* 4   48 Mb */  { IEEE80211_T_OFDM,  48000,  0x00,   5,   192},
     /* 5   24 Mb */  { IEEE80211_T_OFDM,  24000,  0x01,   5,    96},
     /* 6   12 Mb */  { IEEE80211_T_OFDM,  12000,  0x02,   6,    48},
     /* 7    6 Mb */  { IEEE80211_T_OFDM,   6000,  0x03,   7,    24},
     /* 8   54 Mb */  { IEEE80211_T_OFDM,  54000,  0x04,   5,   216},
     /* 9   36 Mb */  { IEEE80211_T_OFDM,  36000,  0x05,   5,   144},
     /* 10   18 Mb */ { IEEE80211_T_OFDM,  18000,  0x06,   6,    72},
     /* 11    9 Mb */ { IEEE80211_T_OFDM,   9000,  0x07,   7,    36},
/* HT rates */
     /*12  6.5 Mb */ {  IEEE80211_T_HT,     6500,  0x80,   7,    26},
     /*13   13 Mb */ {  IEEE80211_T_HT,    13000,  0x81,   6,    52},
     /*14 19.5 Mb */ {  IEEE80211_T_HT,    19500,  0x82,   6,    78},
     /*15   26 Mb */ {  IEEE80211_T_HT,    26000,  0x83,   5,   104},
     /*16   39 Mb */ {  IEEE80211_T_HT,    39000,  0x84,   5,   156},
     /*17   52 Mb */ {  IEEE80211_T_HT,    52000,  0x85,   5,   208},
     /*18 58.5 Mb */ {  IEEE80211_T_HT,    58500,  0x86,   5,   234},
     /*19   65 Mb */ {  IEEE80211_T_HT,    65000,  0x87,   5,   260},
/* DS */
     /*20   13 Mb */ {  IEEE80211_T_HT,    13000,  0x90,   7,   52},
     /*21   26 Mb */ {  IEEE80211_T_HT,    26000,  0x91,   6,  104},
     /*22   39 Mb */ {  IEEE80211_T_HT,    39000,  0x92,   6,  156},
     /*23   52 Mb */ {  IEEE80211_T_HT,    52000,  0x93,   5,  208},
     /*24   78 Mb */ {  IEEE80211_T_HT,    78000,  0x94,   5,  312},
     /*25  104 Mb */ {  IEEE80211_T_HT,   104000,  0x95,   5,  416},
     /*26  117 Mb */ {  IEEE80211_T_HT,   117000,  0x96,   5,  468},
     /*27  130 Mb */ {  IEEE80211_T_HT,   130000,  0x97,   5,  520},
/* TS */
     /*28   19.5 Mb*/{  IEEE80211_T_HT,    19500,   0xa0,  6,   78},
     /*29   39 Mb */ {  IEEE80211_T_HT,    39000,   0xa1,  5,  156},
     /*30   58.5 Mb*/{  IEEE80211_T_HT,    58500,   0xa2,  5,  234},
     /*31   78 Mb */ {  IEEE80211_T_HT,    78000,   0xa3,  5,  312},
     /*32   117 Mb */{  IEEE80211_T_HT,   117000,   0xa4,  5,  468},
     /*33  156 Mb */ {  IEEE80211_T_HT,   156000,   0xa5,  5,  624},
     /*34  175.5 Mb*/{  IEEE80211_T_HT,   175500,   0xa6,  5,  702},
     /*35  195 Mb */ {  IEEE80211_T_HT,   195000,   0xa7,  5,  780},
/* 11n HT 40 rates */
     /*36 13.5 Mb */ {  IEEE80211_T_HT,    13500,   0x80,  6,   54},
     /*37 27.0 Mb */ {  IEEE80211_T_HT,    27000,   0x81,  5,  108},
     /*38 40.5 Mb */ {  IEEE80211_T_HT,    40500,   0x82,  5,  162},
     /*39   54 Mb */ {  IEEE80211_T_HT,    54000,   0x83,  5,  216},
     /*40   81 Mb */ {  IEEE80211_T_HT,    81500,   0x84,  5,  324},
     /*41  108 Mb */ {  IEEE80211_T_HT,   108000,   0x85,  5,  432},
     /*42 121.5Mb */ {  IEEE80211_T_HT,   121500,   0x86,  5,  486},
     /*43  135 Mb */ {  IEEE80211_T_HT,   135000,   0x87,  5,  540},
/* DS */
     /*44   27 Mb */ {  IEEE80211_T_HT,    27000,   0x90,  5,  108},
     /*45   54 Mb */ {  IEEE80211_T_HT,    54000,   0x91,  5,  216},
     /*46   81 Mb */ {  IEEE80211_T_HT,    81000,   0x92,  5,  324},
     /*47  108 Mb */ {  IEEE80211_T_HT,   108000,   0x93,  5,  432},
     /*48  162 Mb */ {  IEEE80211_T_HT,   162000,   0x94,  5,  648},
     /*49  216 Mb */ {  IEEE80211_T_HT,   216000,   0x95,  5,  864},
     /*50  243 Mb */ {  IEEE80211_T_HT,   243000,   0x96,  5,  972},
     /*51  270 Mb */ {  IEEE80211_T_HT,   270000,   0x97,  5, 1080},
/* TS */
     /*52  40.5 Mb */{  IEEE80211_T_HT,    40500,   0xa0,  5,  162},
     /*53   54 Mb */ {  IEEE80211_T_HT,    81000,   0xa1,  5,  324},
     /*54   81 Mb */ {  IEEE80211_T_HT,   121500,   0xa2,  5,  486},
     /*55  108 Mb */ {  IEEE80211_T_HT,   162000,   0xa3,  5,  648},
     /*56  162 Mb */ {  IEEE80211_T_HT,   243000,   0xa4,  5,  972},
     /*57  216 Mb */ {  IEEE80211_T_HT,   324000,   0xa5,  5, 1296},
     /*58  243 Mb */ {  IEEE80211_T_HT,   364500,   0xa6,  5, 1458},
     /*59  270 Mb */ {  IEEE80211_T_HT,   405000,   0xa7,  5, 1620},
	}
};

/*
 * Compute frame tx time to get frame duration
 */
static inline u_int32_t
admctl_compute_tx_time(const struct ieee80211_admctl_rt *rt, u_int8_t rix, u_int32_t frame_len,bool short_preamble,u_int8_t rate_factor)
{
    u_int32_t bps, num_bits, num_symbols, phy_time, tx_time;
    u_int16_t kbps;

	kbps = IEEE80211_ADMCTL_GET_KBPS(rt,rix);

	if (kbps == 0) {
		return 0;
	}

    switch (IEEE80211_ADMCTL_GET_PHY(rt, rix)) {

    case IEEE80211_T_OFDM:
#define OFDM_SIFS_TIME        6
#define OFDM_PREAMBLE_TIME    20
#define OFDM_PLCP_BITS        22
#define OFDM_SYMBOL_TIME       4
        bps				= IEEE80211_ADMCTL_GET_BITSPERSYMBOL(rt, rix);
        num_bits        = OFDM_PLCP_BITS + (frame_len << 3);
        num_symbols 	= howmany(num_bits, bps);
        tx_time         = OFDM_SIFS_TIME + OFDM_PREAMBLE_TIME + (num_symbols * OFDM_SYMBOL_TIME);
        if (rate_factor) {
            tx_time <<= rate_factor;
        }
        break;
#undef OFDM_PREAMBLE_TIME
#undef OFDM_PLCP_BITS
#undef OFDM_SYMBOL_TIME

    case IEEE80211_T_CCK:
#define CCK_PREAMBLE_BITS   144
#define CCK_PLCP_BITS        48
        phy_time     = CCK_PREAMBLE_BITS + CCK_PLCP_BITS;
        if (short_preamble && (kbps>1000)) {
            phy_time >>= 1;
        }
        num_bits = frame_len << 3;
        tx_time = phy_time + howmany((num_bits * 1000), kbps);
        break;
#undef CCK_PREAMBLE_BITS
#undef CCK_PLCP_BITS

    default:
        tx_time = 0;
        break;
    }

    return tx_time;
}

static inline u_int32_t admctl_get_frame_dur(struct ieee80211_node *ni, u_int32_t ratekbps, u_int32_t framelen, int acklen,u_int8_t half_gi)
{
#define L_STF                   8
#define L_LTF                   8
#define L_SIG                   4
#define HT_SIG                  8
#define HT_STF                  4
#define HT_LTF(_ns)             (4 * (_ns))
#define OFDM_PLCP_BITS   22
#define SYMBOL_TIME(_ns)        ((_ns) << 2)            // ns * 4 us
#define SYMBOL_TIME_HALFGI(_ns) (((_ns) * 18 + 4) / 5)  // ns * 3.6 us
#define RATECODE_TO_NSS(_rcode) (((_rcode) >> 4) & 0x3)

	int rate_factor,cw_width=0, short_preamble =0, i;
    u_int8_t rc = 0;
    u_int32_t ack_duration,duration;
	u_int32_t nbits, nsymbits, nsymbols;
    u_int16_t streams = 0 ;
	const struct ieee80211_admctl_rt *rt;
    int cix = 0;
    int rix = 0;
	enum ieee80211_phymode mode;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_ath_channel *chan = vap->iv_bsschan;
	enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);

	if (IEEE80211_IS_CHAN_QUARTER(chan)) {
		rate_factor =2;
	} else if (IEEE80211_IS_CHAN_HALF(chan)) {
		rate_factor =1;
	} else {
		rate_factor =0;
	}

	mode = vap->iv_cur_mode;
	rt = (struct ieee80211_admctl_rt*)(ic->ic_admctl_rt[mode]);

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		 if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) &&
                (ic_cw_width == IEEE80211_CWM_WIDTH40)) {
                cw_width = 1;
            }
	}

	if (IEEE80211_IS_SHPREAMBLE_ENABLED(ic) &&
            !IEEE80211_IS_BARKER_ENABLED(ic) &&
            ieee80211node_has_cap(ni, IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
            short_preamble = 1;
    }

    for (i = 0; i < rt->rateCount; i++) {
        /* FIXME: table entry must be selected based on ratekbps as well
         * as number of streams. some rates like 13000 have multiple entries
         * and only the first one is selected currently which may be incorrect.
         */
        if ((rt->info[i].rate_kbps) == ratekbps) {
            rc =  rt->info[i].rate_code;
            rix = i;
            cix = rt->info[rix].control_rate;
            break;
        }
    }
    /* no matching rate found */
    if (i == rt->rateCount)
        return 0;

    ack_duration = admctl_compute_tx_time(rt,cix,acklen,short_preamble,rate_factor);

    /*
     * for legacy rates, use old function to compute packet duration
     */
    if (!IS_RATECODE_HT(rc)) {
        duration = admctl_compute_tx_time(rt, rix, framelen,short_preamble,rate_factor);
        /* subtract SIFS time, since it is accounted twice */
        if (IEEE80211_IS_CHAN_2GHZ(chan)) {
            duration += ack_duration - 10; /* 2G SIFS time */
        }
        else {
            duration += ack_duration - 16; /* 5G SIFS time */
        }
        return duration;
    }

    /*
     * find number of symbols: PLCP + data
     */
    nbits = (framelen << 3) + OFDM_PLCP_BITS;
    nsymbits = IEEE80211_ADMCTL_GET_BITSPERSYMBOL(rt,rix);
    nsymbols = (nbits + nsymbits - 1) / nsymbits;

    if (!half_gi) {
        duration = SYMBOL_TIME(nsymbols);
	} else {
        duration = SYMBOL_TIME_HALFGI(nsymbols);
	}

    /*
     * addup duration for legacy/ht training and signal fields
     */
    streams = 1+ RATECODE_TO_NSS(rc);

	duration += ack_duration + L_STF + L_LTF + L_SIG + HT_SIG + HT_STF + HT_LTF(streams);

	return duration;
#undef L_STF
#undef L_LTF
#undef L_SIG
#undef HT_SIG
#undef HT_STF
#undef HT_LTF
#undef OFDM_PLCP_BITS
#undef SYMBOL_TIME
#undef SYMBOL_TIME_HALFGI
}

/*
 * Convert tspec ie struct to traffic stream info struct
 */
static inline void admctl_tspec_to_tsinfo(struct ieee80211_wme_tspec* tspec, ieee80211_tspec_info* tsinfo)
{
    struct ieee80211_tsinfo_bitmap *tsflags;

    tsflags = (struct ieee80211_tsinfo_bitmap *) &(tspec->ts_tsinfo);

    tsinfo->direction = tsflags->direction;
    tsinfo->psb = tsflags->psb;
    tsinfo->dot1Dtag = tsflags->dot1Dtag;
    tsinfo->tid = tsflags->tid;
    tsinfo->aggregation = tsflags->reserved3;
    tsinfo->acc_policy_edca = tsflags->one;
    tsinfo->acc_policy_hcca = tsflags->zero;
    tsinfo->traffic_type = tsflags->reserved1;
    tsinfo->ack_policy = tsflags->reserved2;
    tsinfo->max_msdu_size = le16toh(*((u_int16_t *) &tspec->ts_max_msdu[0]));
    tsinfo->norminal_msdu_size = le16toh(*((u_int16_t *) &tspec->ts_nom_msdu[0]));
    tsinfo->min_srv_interval = le32toh(*((u_int32_t *) &tspec->ts_min_svc[0]));
    tsinfo->max_srv_interval = le32toh(*((u_int32_t *) &tspec->ts_max_svc[0]));
    tsinfo->inactivity_interval = le32toh(*((u_int32_t *) &tspec->ts_inactv_intv[0]));
    tsinfo->suspension_interval = le32toh(*((u_int32_t *) &tspec->ts_susp_intv[0]));
    tsinfo->srv_start_time = le32toh(*((u_int32_t *) &tspec->ts_start_svc[0]));
    tsinfo->min_data_rate = le32toh(*((u_int32_t *) &tspec->ts_min_rate[0]));
    tsinfo->mean_data_rate = le32toh(*((u_int32_t *) &tspec->ts_mean_rate[0]));
    tsinfo->max_burst_size = le32toh(*((u_int32_t *) &tspec->ts_max_burst[0]));
    tsinfo->min_phy_rate = le32toh(*((u_int32_t *) &tspec->ts_min_phy[0]));
    tsinfo->peak_data_rate = le32toh(*((u_int32_t *) &tspec->ts_peak_rate[0]));
    tsinfo->delay_bound = le32toh(*((u_int32_t *) &tspec->ts_delay[0]));
    tsinfo->surplus_bw = le16toh(*((u_int16_t *) &tspec->ts_surplus[0]));
    tsinfo->medium_time = le16toh(*((u_int16_t *) &tspec->ts_medium_time[0]));
}

/*
 * Compute the medium time for the requested tspec
 */
static inline u_int16_t admctl_compute_medium_time(struct ieee80211_node *ni, struct ieee80211vap *vap,
                                             ieee80211_tspec_info *tsinfo)
{
    int pps, duration;
    u_int16_t nominal_msdu =  tsinfo->norminal_msdu_size & 0x7fff;
    int sec_encap_sz = 0, pad_sz = 0;
    u_int32_t nominal_mpdu = 0, nominal_mpdu_subframe = 0;
    struct ieee80211_key *k = &ni->ni_ucastkey;
    int min_phy_rate_kbps = tsinfo->min_phy_rate/1000;
    u_int64_t mtime;
    u_int16_t mediumtime = 0;

    /* pps = Ceiling((Mean Data Rate / 8) / Nominal MSDU Size) */
    pps = ((tsinfo->mean_data_rate / 8 ) + nominal_msdu - 1) /  nominal_msdu;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "norminal_msdu_size 0x%x min_data_rate 0x%x \n",
             tsinfo->norminal_msdu_size, tsinfo->mean_data_rate);

    if (k->wk_valid) {
        sec_encap_sz = k->wk_cipher->ic_header;
    }
    nominal_mpdu = nominal_msdu + sizeof(struct ieee80211_qosframe)
                         + sec_encap_sz + IEEE80211_CRC_LEN;
    if (tsinfo->ack_policy == ADMCTL_ACKPOLICY_HT_IMM) {
        if (tsinfo->max_burst_size == 0) {
            tsinfo->max_burst_size = 1;
        }
        pad_sz = 3 - ((nominal_mpdu + 3) % 4);
        nominal_mpdu_subframe = ADMCTL_MPDU_DELIMITER_SZ + nominal_mpdu + pad_sz;
        nominal_mpdu = tsinfo->max_burst_size * nominal_mpdu_subframe - pad_sz;
        pps  = pps / tsinfo->max_burst_size;
        duration = admctl_get_frame_dur(ni, min_phy_rate_kbps, nominal_mpdu, ADMCTL_BA_SIZE,0);
    } else {
        duration = admctl_get_frame_dur(ni, min_phy_rate_kbps, nominal_mpdu, ADMCTL_ACK_SIZE,0);
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,
                      "nominal_msdu = %d nominal_mpdu = %d pps %d "
                      "surplus %d min_phy_rate = %d  duration = %d\n",
		      nominal_msdu, nominal_mpdu, pps, tsinfo->surplus_bw,
                      min_phy_rate_kbps, duration);

    mtime = (pps * duration);
    mtime = (mtime * (unsigned long)tsinfo->surplus_bw) / 0x2000;
    mediumtime = (u_int16_t)((mtime + 31) / 32); /* in 1/32 us uinits */

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Calculated medium time %d\n", mediumtime);
    return mediumtime;
}

/*
 * Check if the vap enough available medium time to accommadate
 * this request
 */
int ieee80211_check_medium_time(struct ieee80211vap *vap,
                                struct ieee80211_node *ni, u_int16_t medium_time)
{
    u_int8_t channel_util;
    u_int16_t available_time;
    /* Check - if the AP has enough available medium time to
     * accept this TSEPC
     * NOTE: currently qbssload is used to determine the available_time
     * but this could be changed to use the absolute used time
     * from the frame exchange time for more accurate
     * available_time calculation
     */
    if (ieee80211_vap_qbssload_is_set(vap)) {
        available_time = ADMCTL_MEDIUM_TIME_MAX;
        if (medium_time > available_time - vap->iv_ic->ic_mediumtime_reserved)
            return -1;

        /* BSS channel untilization feature is enabled */
        /* check the channel untilization to allow the new TSPEC */
        if (vap->iv_sta_assoc) {
            channel_util = vap->chanutil_info.value;
            available_time = ((ADMCTL_MEDIUM_TIME_MAX *
                              (ADMCTL_MAX_CHANNEL_UTIL - channel_util)) /
                               ADMCTL_MAX_CHANNEL_UTIL);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "available_time - %d"
                              "requested medium_time %d \n", available_time, medium_time);
            if (available_time > ((medium_time * 3)/2)) {
                return 0;
            }
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s Not enough available_time "
                                  "to accept the new TSPEC", __func__);
                return -1;
            }
        }
    }
    else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s : Bssload is not enabled "
                           "accepting all TSPECs \n", __func__);
    }
    return 0;
}

/*
 * process the tspec request and fill in the tspec response
 */
int ieee80211_admctl_process_addts_req(struct ieee80211_node *ni, ieee80211_tspec_info* tsinfo, int dialog_token)
{
    struct ieee80211vap *vap = ni->ni_vap;
    u_int8_t *macaddr = ni->ni_macaddr;
    u_int8_t ac;
    u_int16_t medium_time;
    struct ieee80211_admctl_tsentry *tsentry;
    struct ieee80211_admctl_priv *admctl_priv = ni->ni_admctl_priv;

    /* Get tspec */
    ac = TID_TO_WME_AC(tsinfo->dot1Dtag);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s : ADDTS requested for AC %d \n", __func__, ac);
    /* check ACM set for this accesscategory */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s \n", "Processing ADDTS Req");

    /* compute medium_time */
    medium_time = admctl_compute_medium_time(ni, vap, tsinfo);
    if (ieee80211_check_medium_time(vap, ni, medium_time)) {
        /* medium_time is not available */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s \n", "ADDTS REFUSED - No available medium time ");
        wlan_send_addts_resp(vap, macaddr, tsinfo, ADMCTL_ADDTS_RESP_REFUSED, dialog_token);
        return 0;
    }

    /* admission control is required for this AC */
    tsentry = ieee80211_find_tsentry(admctl_priv, tsinfo->tid);
    if (!tsentry) {
        tsentry = ieee80211_add_replace_tsentry(ni, ac, tsinfo);
    }
    else {
        if (tsentry->ts_ac == ac) {
            if (tsinfo->direction == tsentry->ts_info.direction) {
                u_int16_t old_mediumtime;
                /* replace tspec, but restore previous medium time */
                old_mediumtime = tsentry->ts_info.medium_time;
                OS_MEMCPY(&tsentry->ts_info, tsinfo, sizeof(ieee80211_tspec_info));
                ieee80211_parse_psb(ni, tsinfo->direction, tsinfo->psb, ac, WME_UAPSD_AC_INVAL, WME_UAPSD_AC_INVAL);
                tsentry->ts_info.medium_time = old_mediumtime;
            } else {
                ieee80211_remove_ac_tsentries(admctl_priv, ac);
                tsentry = ieee80211_add_tsentry(admctl_priv, ac, tsinfo->tid);
                if (tsentry) {
                    OS_MEMCPY(&tsentry->ts_info, tsinfo, sizeof(ieee80211_tspec_info));
                    tsentry->ts_info.medium_time = 0;
                }
                ieee80211_parse_psb(ni, tsinfo->direction, tsinfo->psb, ac, WME_UAPSD_AC_INVAL, WME_UAPSD_AC_INVAL);
            }
        }
        else {
             IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s \n", "ADDTS REFUSED for INVALID PARAM");
             wlan_send_addts_resp(vap, macaddr, tsinfo, ADMCTL_ADDTS_RESP_INVALID_PARAM, dialog_token);
             return 0;
        }
    }

    if (tsentry) {
        if (tsentry->ts_info.medium_time > medium_time)
            vap->iv_ic->ic_mediumtime_reserved -= tsentry->ts_info.medium_time - medium_time;
        else
            vap->iv_ic->ic_mediumtime_reserved += medium_time - tsentry->ts_info.medium_time;

        /* update medium_time */
        tsentry->ts_info.medium_time = medium_time;

        if ((tsinfo->direction == ADMCTL_TSPEC_UPLINK) ||
               (tsinfo->direction == ADMCTL_TSPEC_BIDI) ) {
            tsinfo->medium_time = tsentry->ts_info.medium_time;
        }
        else {
            /* For downlink only TSPEC set the medium time to 0 */
            tsinfo->medium_time = 0;
        }
    }

    if (wlan_get_wmm_param(vap, WLAN_WME_ACM, 1, ac) == 0) {
        tsinfo->medium_time = 0;
    }

    /* send addts response */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s \n", "ADDTS Accepted");
    wlan_send_addts_resp(vap, macaddr, tsinfo, 0, dialog_token);

    return 0;
}

int ieee80211_admctl_send_delts(struct ieee80211vap *vap, u_int8_t *macaddr, u_int8_t tid)
{
    int error = 0;
    u_int8_t ac;
    struct ieee80211_admctl_tsentry *tsentry;
    struct ieee80211_admctl_priv *admctl_priv;
    struct ieee80211_node *ni;

    ni = ieee80211_find_txnode(vap, macaddr);
    if (ni == NULL) {
        error = -EINVAL;
        return error;
    }
    admctl_priv = ni->ni_admctl_priv;

    /* Get tspec */
    ac = TID_TO_WME_AC(tid);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s : DELTS requested for AC %d \n", __func__, ac);

    /* admission control is required for this AC */
    tsentry = ieee80211_find_tsentry(admctl_priv, tid);
    if (!tsentry) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,
                          "%s : could not find TS entry for AC %d \n", __func__, ac);
        ieee80211_free_node(ni);
        return -EINVAL;
    }
    error =  wlan_send_delts(vap, macaddr, &tsentry->ts_info);
    if (!error) {
        error = ieee80211_remove_tsentry(admctl_priv, tid);
        if (!error) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s : could not remove the tsentry \n", __func__);
            ieee80211_free_node(ni);
            return -EINVAL;
        }
    }
    else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s : wlan_send_delts is failed \n", __func__);
    }

    ieee80211_free_node(ni);
    return error;
}


/*
 * Handler for add tspec request.
 */
int ieee80211_admctl_addts(struct ieee80211_node *ni, struct ieee80211_wme_tspec* tspec)
{

    struct ieee80211vap *vap = ni->ni_vap;
    u_int8_t ac;
    struct ieee80211_admctl_tsentry *tsentry;
    struct ieee80211_admctl_priv *admctl_priv = ni->ni_admctl_priv;
    ieee80211_tspec_info ts;

    admctl_tspec_to_tsinfo(tspec, &ts);
    ac = TID_TO_WME_AC(ts.dot1Dtag);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "ADDTS requested for AC %d \n", ac);
    /* check ACM set for this accesscategory */
    if (wlan_get_wmm_param(vap, WLAN_WME_ACM, 1, ac)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s \n", "Processing ADDTS Req");
        /* admission control is required for this AC */
        tsentry = ieee80211_find_tsentry(admctl_priv, ts.tid);
        if (!tsentry) {
            /* create the traffic stream */
            tsentry = ieee80211_add_tsentry(admctl_priv, ac, ts.tid);
            if (tsentry) {
                /* copy the ts info */
                OS_MEMCPY(&tsentry->ts_info, &ts, sizeof(ieee80211_tspec_info));
            }
        }
        else {
        }

        if (tsentry) {
            /* compute medium_time - in units of 32 us */
            tsentry->ts_info.medium_time = admctl_compute_medium_time(ni, vap, &ts);
            if ((ts.direction == ADMCTL_TSPEC_UPLINK) ||
               (ts.direction == ADMCTL_TSPEC_BIDI) ) {
                *((u_int16_t *) &tspec->ts_medium_time[0]) =
                    htole16((u_int16_t)(tsentry->ts_info.medium_time));
            }
            else {
                /* For downlink only TSPEC set the medium time to 0 */
                *((u_int16_t *) &tspec->ts_medium_time[0]) = 0;
            }
        }
    } else {
        /* Addmission control is not required. Ignoring addts req */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s \n", "Ignoring ADDTS : ACM is not set");
    }
    return IEEE80211_STATUS_SUCCESS;
}

int ieee80211_admctl_process_delts_req(struct ieee80211_node *ni, ieee80211_tspec_info* tsinfo)
{
    int error = 0;

    error = ieee80211_remove_tsentry(ni->ni_admctl_priv, tsinfo->tid);

    return error;

}

static inline void
ieee80211_admctl_rt_setup(struct ieee80211com *ic, enum ieee80211_phymode mode, void *rt )
{
    if ( mode>= IEEE80211_MODE_MAX ) {
        return;
    }
    ic->ic_admctl_rt[mode] = rt;
    return;
}

static int ieee80211_admctl_setup_rt(struct ieee80211com *ic)
{
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11A, (void*)(&ieee80211_admctl_rt_11a));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11G, (void*)(&ieee80211_admctl_rt_11g));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11B, (void*)(&ieee80211_admctl_rt_11b));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11NA_HT20, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11NA_HT40PLUS, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11NA_HT40MINUS, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11NG_HT20, (void*)(&ieee80211_admctl_rt_11ng));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11NG_HT40PLUS, (void*)(&ieee80211_admctl_rt_11ng));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11NG_HT40MINUS, (void*)(&ieee80211_admctl_rt_11ng));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXG_HE20, (void*)(&ieee80211_admctl_rt_11ng));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXG_HE40, (void*)(&ieee80211_admctl_rt_11ng));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXG_HE40PLUS, (void*)(&ieee80211_admctl_rt_11ng));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXG_HE40MINUS, (void*)(&ieee80211_admctl_rt_11ng));
    /* XXX: to be fixed when 11AC support is added; for now use 11na table  for VHT mode */
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT20, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT40PLUS, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT40MINUS, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT40, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT80, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT160, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AC_VHT80_80, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE20, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE40PLUS, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE40MINUS, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE40, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE80, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE160, (void*)(&ieee80211_admctl_rt_11na));
    ieee80211_admctl_rt_setup(ic, IEEE80211_MODE_11AXA_HE80_80, (void*)(&ieee80211_admctl_rt_11na));
    return 0;
}

int ieee80211_admctl_ap_attach(struct ieee80211com *ic)
{
    ieee80211_admctl_setup_rt(ic);
    return 0;
}

int ieee80211_admctl_ap_detach(struct ieee80211com *ic)
{
    return 0;
}
#endif /* UMAC_SUPPORT_ADMCTL */
