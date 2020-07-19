/*
 * Copyright (c) 2016-2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *    2016  Qualcomm Atheros, Inc.
 *    All Rights Reserved.
 *    Qualcomm Atheros Confidential and Proprietary.
*/
#include "osif_private.h"
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include "ieee80211_radiotap.h"
#include <qdf_module.h>
#include "if_athvar.h"
#include "ol_if_athvar.h"
#include <qdf_nbuf.h>

/* Get actual radiotap header length in total */
int get_radiotap_total_len(u_int8_t ht, u_int8_t vht)
{
    int len;

    len = sizeof(struct ieee80211_radiotap_header);

    /* TSF bit0 */
    len = ALIGN(len, 8);
    len += 8;

    /* flags */
    len++;

    /* rate */
    len++;

    /* channel */
    len += 2;

    /* channel flags */
    len += 2;

    /* RX signal (dBM) */
    len++;

    /* RX signal noise floor (dBM) */
    len++;

    /* Rx Signal (dB) */
    len++;

    /* rx flags */
    len = ALIGN(len, 2);
    len += 2;

    /* u8 known, u8 flags, u8 mcs */
    if (ht) {
        len++;
        len++;
        len++;
    }

    /* AMPDU Status Flag */
    len = ALIGN(len, 4);
    len += 8;

    len = ALIGN(len, 2);
    /* u16 known, u8 flags, u8 bandwidth, u8 mcs_nss[4], u8 coding, u8 group_id, u16 partial_aid */
    if (vht) {

        /* known */
        len += 2;
        /* flags */
        len++;
        /* bandwidth */
        len++;
        /* mcs_nss */
        len += 4;
        /* coding */
        len++;
        /* group_id */
        len++;
        /* partial_aid */
        len += 2;
    }

    len = ALIGN(len, 2);
    len += sizeof(struct qdf_radiotap_vendor_ns_ath);
    return len;
}
qdf_export_symbol(get_radiotap_total_len);


typedef struct {
    u_int8_t    phy;         /* CCK/OFDM/HT */
    u_int32_t   rate_Kbps;   /* transfer rate in kbs */
    u_int8_t    rate_code;   /* rate for h/w descriptors */
    u_int8_t    mcs;         /* MCS for HT */
} rate_code_table;

#define NUM_RATES_DA    (39)
rate_code_table da_rates[NUM_RATES_DA] = {
/*   1 Mb */ {IEEE80211_T_CCK,     1000,    0x1b,   0xff},
/*   2 Mb */ {IEEE80211_T_CCK,     2000,    0x1a,   0xff},
/*   2 Mb_S */ {IEEE80211_T_CCK,     2000,    0x1e,   0xff},
/* 5.5 Mb */ {IEEE80211_T_CCK,     5500,    0x19,   0xff},
/* 5.5 Mb_S */ {IEEE80211_T_CCK,     5500,    0x1d,   0xff},
/*  11 Mb */ {IEEE80211_T_CCK,    11000,    0x18,   0xff},
/*  11 Mb_S */ {IEEE80211_T_CCK,    11000,    0x1c,   0xff},
/*   6 Mb */ {IEEE80211_T_OFDM,    6000,    0x0b,   0xff},
/*   9 Mb */ {IEEE80211_T_OFDM,    9000,    0x0f,   0xff},
/*  12 Mb */ {IEEE80211_T_OFDM,   12000,    0x0a,   0xff},
/*  18 Mb */ {IEEE80211_T_OFDM,   18000,    0x0e,   0xff},
/*  24 Mb */ {IEEE80211_T_OFDM,   24000,    0x09,   0xff},
/*  36 Mb */ {IEEE80211_T_OFDM,   36000,    0x0d,   0xff},
/*  48 Mb */ {IEEE80211_T_OFDM,   48000,    0x08,   0xff},
/*  54 Mb */ {IEEE80211_T_OFDM,   54000,    0x0c,   0xff},
/* 6.5 Mb */ {IEEE80211_T_HT,      6500,    0x80,    0},
/*  13 Mb */ {IEEE80211_T_HT,     13000,    0x81,    1},
/*19.5 Mb */ {IEEE80211_T_HT,     19500,    0x82,    2},
/*  26 Mb */ {IEEE80211_T_HT,     26000,    0x83,    3},
/*  39 Mb */ {IEEE80211_T_HT,     39000,    0x84,    4},
/*  52 Mb */ {IEEE80211_T_HT,     52000,    0x85,    5},
/*58.5 Mb */ {IEEE80211_T_HT,     58500,    0x86,    6},
/*  65 Mb */ {IEEE80211_T_HT,     65000,    0x87,    7},
/*  13 Mb */ {IEEE80211_T_HT,     13000,    0x88,    8},
/*  26 Mb */ {IEEE80211_T_HT,     26000,    0x89,    9},
/*  39 Mb */ {IEEE80211_T_HT,     39000,    0x8a,   10},
/*  52 Mb */ {IEEE80211_T_HT,     52000,    0x8b,   11},
/*  78 Mb */ {IEEE80211_T_HT,     78000,    0x8c,   12},
/* 104 Mb */ {IEEE80211_T_HT,    104000,    0x8d,   13},
/* 117 Mb */ {IEEE80211_T_HT,    117000,    0x8e,   14},
/* 130 Mb */ {IEEE80211_T_HT,    130000,    0x8f,   15},
/*19.5 Mb */ {IEEE80211_T_HT,     19500,    0x90,   16},
/*  39 Mb */ {IEEE80211_T_HT,     39000,    0x91,   17},
/*58.5 Mb */ {IEEE80211_T_HT,     58500,    0x92,   18},
/*  78 Mb */ {IEEE80211_T_HT,     78000,    0x93,   19},
/* 117 Mb */ {IEEE80211_T_HT,    117000,    0x94,   20},
/* 156 Mb */ {IEEE80211_T_HT,    156000,    0x95,   21},
/*175.5Mb */ {IEEE80211_T_HT,    175500,    0x96,   22},
/* 195 Mb */ {IEEE80211_T_HT,    195000,    0x97,   23},
};

static u_int32_t get_ht_mcs_from_ratecode(u_int8_t ht_ratecode)
{
    int i;
    u_int32_t ht_mcs = 0;

    for (i=0; i<NUM_RATES_DA; i++){
        if (ht_ratecode == da_rates[i].rate_code) {
            ht_mcs = da_rates[i].mcs;
            break;
        }
    }

    if (ht_mcs == 0xff) {
        printk("%s: no mcs found for ht ratecode=0x%x\n",__FUNCTION__, ht_ratecode);
        return -EINVAL;
    }

    return ht_mcs;
}

static u_int32_t get_pream_from_ratecode(u_int8_t ratecode)
{
    int i;
    u_int32_t pream = IEEE80211_T_MAX;

    for (i=0; i<NUM_RATES_DA; i++){
        if (ratecode == da_rates[i].rate_code) {
            pream = da_rates[i].phy;
            break;
        }
    }

    if (pream == IEEE80211_T_MAX) {
        printk("%s: preamble not found for ratecode=0x%x\n",__FUNCTION__, ratecode);
        return -EINVAL;
    }
    return pream;
}

static void add_radiotap_header_da(os_if_t osif, struct sk_buff *skb, ieee80211_recv_status *rs)
{
    osif_dev *osifp = (osif_dev *)osif;
    struct ieee80211vap *vap = osifp->os_if;
    struct ieee80211com *ic = wlan_vap_get_devhandle(vap);
    struct ath_softc_net80211 *scn_da = ATH_SOFTC_NET80211(ic);
    u_int32_t *it_present_p = NULL;
    u_int32_t it_present_mask = 0;
    u_int8_t *p = NULL;
    u_int16_t rt_total_len;
    struct ieee80211_radiotap_header *rthdr = NULL;
    struct ah_rx_radiotap_header rh;
    struct sk_buff *tskb = NULL;
    u_int8_t cck=0, ofdm=0, ht=0, vht=0, pream=IEEE80211_T_MAX;
    u_int8_t bw=0, sgi=0, stbc=0, mcs=0;
    u_int8_t radiotap_mcs_details = 0;
    /* 11AX TODO (Phase II) - Check if chan_flags needs to be changed to
     * u_int64_t */
    u_int16_t chan_flags = 0;

    qdf_mem_zero(&rh, sizeof(struct ah_rx_radiotap_header));

    pream = get_pream_from_ratecode(rs->rs_ratephy1);
    switch (pream){
    case IEEE80211_T_CCK:
        cck = 1;
        break;
    case IEEE80211_T_OFDM:
        ofdm = 1;
        break;
    case IEEE80211_T_HT:
        ht = 1;
        break;
    default:
        printk("%s: pream=%d not valid\n",__FUNCTION__, pream);
        return;
    }
    /* populate rh */
    if (scn_da->sc_ops->ath_fill_radiotap_hdr) {
        scn_da->sc_ops->ath_fill_radiotap_hdr(scn_da->sc_dev, &rh, NULL, skb);
    }

    rt_total_len = get_radiotap_total_len(ht, vht);
    if (qdf_nbuf_headroom(skb) < rt_total_len) {
        tskb = qdf_nbuf_realloc_headroom(skb, rt_total_len);
        if(tskb!=NULL){
            /*
             * qdf_nbuf_realloc_headroom won't do skb_clone as skb_realloc_headroom does.
             * so, no free's needed here.
            */
            skb = tskb;
        } else {
            qdf_print(" %s[%d] skb_realloc_headroom failed",__func__,__LINE__);
            return;
        }
    }
    rthdr = (struct ieee80211_radiotap_header *)qdf_nbuf_push_head(skb, rt_total_len);
    qdf_mem_zero(rthdr, rt_total_len);

    rthdr->it_version = 0;
    rthdr->it_len = cpu_to_le16(rt_total_len);
    it_present_p = &rthdr->it_present;
    p = (u_int8_t *)(it_present_p + 1);

    /* present mask */
    it_present_mask = BIT(IEEE80211_RADIOTAP_FLAGS) | BIT(IEEE80211_RADIOTAP_CHANNEL) \
                            | BIT(IEEE80211_RADIOTAP_RX_FLAGS);
    put_unaligned_le32(it_present_mask, it_present_p);

    /* 8-byte alignment */
    while((p - (u_int8_t *)rthdr) & 7) {
        *p++ = 0;
    }
    /* TSF bit0 */
    put_unaligned_le64(rh.tsf, p);
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_TSFT));
    p += 8;

    /* flags */
    if(rs->rs_fcs_error) {
        *p |= IEEE80211_RADIOTAP_F_BADFCS;
    }
    if (rh.wr_mcs.flags & RADIOTAP_MCS_FLAGS_SHORT_GI) {
        *p |= IEEE80211_RADIOTAP_F_SHORTGI;
        sgi = 1;
    }
    p++;

    /* rate */
    if (ht) {
        //HT
        *p = 0;
    } else {
        //Legacy rates
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_RATE));
        //radiotap needs rate in units of 100 Kbps
        *p = DIV_ROUND_UP(rs->rs_datarate/100, 5);
    }
    p++;

    /* channel */
    put_unaligned_le16(rs->rs_freq, p);
    p += 2;
    /* channel flags */
    chan_flags = ic->ic_curchan->ic_flags;
    if (ofdm || ht) {
        chan_flags |= IEEE80211_CHAN_OFDM;
    } else if (cck) {
        chan_flags |= IEEE80211_CHAN_CCK;
    }
    put_unaligned_le16(chan_flags, p);
    p += 2;

    /* RX signal */
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_DBM_ANTSIGNAL));
    *p = ic->ic_get_cur_chan_nf(ic) + rs->rs_rssi;
    p++;

    /* RX signal noise floor */
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_DBM_ANTNOISE));
    *p = ic->ic_get_cur_chan_nf(ic);
    p++;

    /* rx flags */
    if ((p - (u_int8_t *)rthdr) & 1)
        *p++ = 0;
    p += 2;

    if (ht) {
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_MCS));
        radiotap_mcs_details |= IEEE80211_RADIOTAP_MCS_HAVE_STBC | \
                        IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_GI | \
                        IEEE80211_RADIOTAP_MCS_HAVE_BW;
        if (rh.wr_mcs.flags & RADIOTAP_MCS_FLAGS_40MHZ) {
            bw = 40;
        } else {
            bw = 20;
        }
        if (rh.wr_rx_flags & AH_RADIOTAP_11NF_RX_STBC) {
            stbc = 1;
        }
        *p++ = radiotap_mcs_details;
        if (sgi)
            *p |= IEEE80211_RADIOTAP_MCS_SGI;
        if (bw == 20)
            *p |= IEEE80211_RADIOTAP_MCS_BW_20;
        else if (bw == 40)
            *p |= IEEE80211_RADIOTAP_MCS_BW_40;
        *p |= stbc << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
        p++;
        mcs = get_ht_mcs_from_ratecode(rs->rs_ratephy1);
        *p++ = mcs;
    }

    return;
}

/*
Standard radiotap defined fields
    bit field
    0   TSFT
    1   Flags
    2   Rate
    3   Channel
    4   FHSS
    5   Antenna signal
    6   Antenna noise
    7   Lock quality
    8   TX attenuation
    9   dB TX attenuation
    10  dBm TX power
    11  Antenna
    12  dB antenna signal
    13  dB antenna noise
    14  RX flags
    19  MCS
    20  A-MPDU status
    21  VHT
    22-28   Reserved
    29      Radiotap Namespace
    29 + 32*n   Radiotap Namespace
    30  Vendor Namespace
    30 + 32*n   Vendor Namespace
    31  reserved: another bitmap follows
    31 + 32*n   reserved: another bitmap follows
*/
void osif_mon_add_radiotap_header(os_if_t osif, struct sk_buff *skb, ieee80211_recv_status *rs)
{
    osif_dev *osifp = (osif_dev *)osif;
    struct ieee80211vap *vap = osifp->os_if;
    struct ieee80211com *ic = wlan_vap_get_devhandle(vap);
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_radiotap_header *rthdr = NULL;
    struct sk_buff *tskb = NULL;
    struct qdf_radiotap_vendor_ns_ath *radiotap_vendor_ns_ath;
    u_int32_t *it_present_p = NULL;
    u_int32_t it_present_mask=0;
    u_int8_t *p = NULL;
    u_int8_t cck_tbl[] = {2,4,11,22,4,11,22,22,11,4,2,22,11,4};
    u_int32_t mcs=0, nss=0, signal=0, base=0, pream_type=0, disp_rate=0;
    u_int32_t bw=0, sgi=0, ldpc=0, stbc=0, groupid=0, su_ppdu=0, txbf=0;
    u_int32_t partial_aid=0, nsts_u0=0, nsts_u1=0, nsts_u2=0, nsts_u3=0;
    u_int32_t ldpc_u0=0, ldpc_u1=0, ldpc_u2=0, ldpc_u3=0;
    u_int32_t sig_a_1=0, sig_a_2=0, sig_b=0, nsts_su=0;
    u_int32_t rate_phy1=0, rate_phy2=0, rate_phy3=0, pream_indicate;
    u_int8_t cck=0, ofdm=0, ht=0, vht=0;
    /* 11AX TODO (Phase II) - Check if chan_flags needs to be changed to
     * u_int64_t */
    u_int16_t chan_flags = 0;
    u_int16_t ampdu_flags = 0;
    u_int8_t radiotap_mcs_details = 0;
    u_int16_t radiotap_vht_details = 0;
    u_int8_t vht_coding_details = 0;
    u_int16_t rt_total_len;

    if (!osifp->osif_is_mode_offload) {
        /* for legacy DA radio chips */
        add_radiotap_header_da(osif, skb, rs);
        return;
    }

    if (!rs) {
	/* radiotap header is already added */
	return;
    }
    /*     rs_ratephy coding
           [b3 - b0]
           0 -> OFDM
           1 -> CCK
           2 -> HT
           3 -> VHT

           OFDM / CCK:
           [b7  - b4 ] => LSIG rate
           [b23 - b8 ] => service field (b'12 static/dynamic, b'14..b'13 BW for VHT)
           [b31 - b24 ] => Reserved

           HT / VHT:
           [b15 - b4 ] => SIG A_2 12 LSBs
           [b31 - b16] => SIG A_1 16 LSBs

           HT / VHT MU:
           [b27 - b4 ] => SIG A_1 24 LSBs

           rs_ratephy2 coding
           [b23 - b0 ] => SIG_A_2 24 LSBs

           rs_ratephy3 coding
           [b28 - b0 ] => SIG_B   29 LSBs
    */
    rate_phy1 = rs->rs_ratephy1;
    rate_phy2 = rs->rs_ratephy2;
    rate_phy3 = rs->rs_ratephy3;
    pream_type =  rate_phy1 & PHY_PREAM_TYPE_MASK;
    pream_indicate = ((rate_phy1 & 0xFFFFFF0) >> 4);

    switch (pream_type) {
        case PHY_PREAM_TYPE_OFDM: /* OFDM */
            ofdm = 1;
            mcs = (rate_phy1 >> PHY_RATE_MCS_SHIFT) & PHY_RATE_MCS_MASK;
            base = (mcs & PHY_RATE_BASE_MASK) ? 9 : 6;
            mcs &= ~PHY_RATE_BASE_MASK;
            mcs = base << (11 - mcs);
            mcs = (mcs > 54) ? 54 : mcs;
            signal = rate_phy1 & (1 << PHY_SIGNALING_SHIFT);
            bw = PHY_BANDWIDTH_ENCODE_SHIFT << ((rate_phy1 >> PHY_BANDWIDTH_SHIFT) & PHY_BANDWIDTH_MASK);
            break;
        case PHY_PREAM_TYPE_CCK: /* CCK */
            cck = 1;
            mcs = (rate_phy1 >> PHY_RATE_MCS_SHIFT) & PHY_RATE_MCS_MASK;
            base = (mcs & PHY_RATE_BASE_MASK) ? 1 : 0;
            /* for CCK only 1 to 7 are valid mcs values */
            if ((mcs > 0) && (mcs < 14))
                disp_rate = cck_tbl[mcs-1];
            else
                disp_rate = 0;
            break;
        case PHY_PREAM_TYPE_HT: /* HT */
            ht = 1;
            sig_a_1 = (rate_phy1 >> PHY_RATE_MCS_SHIFT) & PHY_SIG_A1_MASK;
            sig_a_2 = (rate_phy2) & PHY_SIG_A1_MASK;
            bw = PHY_BANDWIDTH_ENCODE_SHIFT << ((sig_a_1 >> PHY_BANDWIDTH_SHIFT_HT) & PHY_BANDWIDTH_MASK_HT);
            mcs = sig_a_1  & PHY_RATE_MCS_MASK_HT;
            sgi = (sig_a_2 >> PHY_SGI_SHIFT) & PHY_SGI_MASK;
            ldpc = (sig_a_2 >> PHY_LDPC_SHIFT) & PHY_LDPC_MASK;
            stbc = ((sig_a_2 >> PHY_STBC_SHIFT) & PHY_STBC_MASK)?1:0;
            nss = (mcs >> PHY_NSS_SHIFT) + 1;
            break;
        case PHY_PREAM_TYPE_VHT: /* VHT */
            vht = 1;
            sig_a_1 = (rate_phy1 >> PHY_RATE_MCS_SHIFT) & PHY_SIG_A1_MASK_VHT;
            sig_a_2 = (rate_phy2) & PHY_SIG_A2_MASK_VHT;
            bw = PHY_BANDWIDTH_ENCODE_SHIFT << (sig_a_1 & PHY_BANDWIDTH_MASK_VHT);
            sgi = sig_a_2 & PHY_SGI_MASK;
            ldpc = (sig_a_2 >> PHY_LDPC_SHIFT_VHT) & PHY_LDPC_MASK;
            stbc = (sig_a_1 >> PHY_STBC_SHIFT_VHT) & PHY_STBC_MASK_VHT;
            groupid = (sig_a_1 >> PHY_GROUPID_SHIFT) & PHY_GROUPID_MASK;
            su_ppdu = 0;

            if ((groupid == 0) || (groupid == 63))
              su_ppdu = 1;
            if (su_ppdu) {
              nsts_su = (sig_a_1 >> PHY_NSTS_SU_SHIFT) & PHY_NSTS_MASK;
              if(stbc)
                 nss = nsts_su >> 2;
              else
                 nss = nsts_su;
              ++nss;
              mcs = (sig_a_2 >> PHY_RATE_MCS_SHIFT) & PHY_RATE_MCS_MASK_VHT;
              txbf = (sig_a_2 >> PHY_TXBF_SHIFT) & PHY_TXBF_MASK;
              partial_aid = (sig_a_1 >> PHY_PARTIAL_AID_SHIFT) & PHY_PARTIAL_AID_MASK;
            } else {
              txbf = (sig_a_2 >> PHY_TXBF_SHIFT) & PHY_TXBF_MASK;
              ldpc_u0 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U0) & PHY_LDPC_MASK;
              ldpc_u1 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U1) & PHY_LDPC_MASK;
              ldpc_u2 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U2) & PHY_LDPC_MASK;
              ldpc_u3 = (sig_a_2 >> PHY_LDPC_SHIFT_VHT_U3) & PHY_LDPC_MASK;
              nsts_u0 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U0) & PHY_NSTS_MASK;
              nsts_u1 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U1) & PHY_NSTS_MASK;
              nsts_u2 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U2) & PHY_NSTS_MASK;
              nsts_u3 = (sig_a_1 >> PHY_NSTS_MU_SHIFT_U3) & PHY_NSTS_MASK;
              nss = nsts_u1;
              switch (bw) {
                  case 20:
                      sig_b = (rate_phy3) & PHY_SIG_B_MASK_VHT_BW20;
                      mcs = (sig_b >> PHY_RATE_MCS_SHIFT_VHT_MU_BW20) & PHY_RATE_MCS_MASK_VHT;
                      break;
                  case 40:
                      sig_b = (rate_phy3) & PHY_SIG_B_MASK_VHT_BW40;
                      mcs = (sig_b >> PHY_RATE_MCS_SHIFT_VHT_MU_BW40) & PHY_RATE_MCS_MASK_VHT;
                      break;
                  case 80:
                      sig_b = (rate_phy3) & PHY_SIG_B_MASK_VHT_BW80;
                      mcs = (sig_b >> PHY_RATE_MCS_SHIFT_VHT_MU_BW80) & PHY_RATE_MCS_MASK_VHT;
                      break;
                  case 160:
                      //TODO: need to add for correct mcs for 160Mhz MU-MIMO
                      break;
              }
            }
            break;
        default:
            qdf_print("Unknown preamble! ");
    }

    /* prepare space in skb for radiotap header */
    rt_total_len = get_radiotap_total_len(ht, vht);
    if (qdf_nbuf_headroom(skb) < rt_total_len) {
        tskb = qdf_nbuf_realloc_headroom(skb, rt_total_len);
        if(tskb!=NULL){
            /*
             * qdf_nbuf_realloc_headroom won't do skb_clone as skb_realloc_headroom does.
             * so, no free's needed here.
            */
            skb = tskb;
        } else {
            qdf_print(" %s[%d] skb_realloc_headroom failed",__func__,__LINE__);
            return;
        }
    }
    rthdr = (struct ieee80211_radiotap_header *)qdf_nbuf_push_head(skb, rt_total_len);
    qdf_mem_zero(rthdr, rt_total_len);

    /* radiotap version */
    rthdr->it_version = 0;
    rthdr->it_len = cpu_to_le16(rt_total_len);
    it_present_p = &rthdr->it_present;
    p = (u_int8_t *)(it_present_p + 1);

    /* init present mask */
    it_present_mask = BIT(IEEE80211_RADIOTAP_FLAGS) | BIT(IEEE80211_RADIOTAP_CHANNEL) \
                          | BIT(IEEE80211_RADIOTAP_RX_FLAGS) | BIT(IEEE80211_RADIOTAP_AMPDU_STATUS);
    put_unaligned_le32(it_present_mask, it_present_p);

    /*
     * TODO
     *  @rx chains: bitmask of receive chains for which separate signal strength
     *  values were filled.
    */
    /* TSF bit0 */
    //TODO: use wb instead of tsf?
    while((p - (u_int8_t *)rthdr) & 7) {
        //8-byte alignment
        *p++ = 0;
    }
    put_unaligned_le64(rs->rs_tstamp.tsf, p);
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_TSFT));
    p += 8;

    /* flags */
    if (pream_type == PHY_PREAM_TYPE_CCK)
    {
       if ((pream_indicate == 5) || (pream_indicate == 6) || (pream_indicate == 7))
       {
          *p |= IEEE80211_RADIOTAP_F_PREAM;
       }
    }

    if(rs->rs_fcs_error) {
        *p |= IEEE80211_RADIOTAP_F_BADFCS;
    }
    if (sgi) {
        *p |= IEEE80211_RADIOTAP_F_SHORTGI;
    }
    p++;

    /* rate */
    if (ht || vht) {
        /* non-legacy rates will be parsed later */
        *p = 0;
    } else {
         rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_RATE));
        /* radiotap rate is in 500 kbps, mcs here is in Mbps */
        if (ofdm) {
            *p = DIV_ROUND_UP(mcs*10, 5);
        } else {
            *p = DIV_ROUND_UP((disp_rate*10)/2, 5);
        }
    }
    p++;

    /* channel */
    put_unaligned_le16(rs->rs_freq, p);
    p += 2;
    /* channel flags */
    chan_flags = ic->ic_curchan->ic_flags;
    if (ofdm || ht || vht) {
        chan_flags |= IEEE80211_CHAN_OFDM;
    } else if (cck) {
        chan_flags |= IEEE80211_CHAN_CCK;
    }
    put_unaligned_le16(chan_flags, p);
    p += 2;

    /* RX signal */
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_DBM_ANTSIGNAL));
    *p = ic->ic_get_cur_chan_nf(ic) - DEFAULT_CHAN_NOISE_FLOOR + rs->rs_rssi + DEFAULT_CHAN_REF_NOISE_FLOOR;
    p++;

    /* RX signal noise floor */
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_DBM_ANTNOISE));
    *p = ic->ic_get_cur_chan_nf(ic);
    p++;

    /* RX signal with respect to noise floor  */
    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_DB_ANTSIGNAL));
    *p = rs->rs_rssi;
    p++;

    /* rx flags */
    if ((p - (u_int8_t *)rthdr) & 1)
        *p++ = 0;
    p += 2;

    if (ht) {
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_MCS));
        radiotap_mcs_details |= IEEE80211_RADIOTAP_MCS_HAVE_STBC | \
                        IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_GI | \
                        IEEE80211_RADIOTAP_MCS_HAVE_BW;
        *p++ = radiotap_mcs_details;
        if (sgi)
            *p |= IEEE80211_RADIOTAP_MCS_SGI;
        if (bw == 20)
            *p |= IEEE80211_RADIOTAP_MCS_BW_20;
        else if (bw == 40)
            *p |= IEEE80211_RADIOTAP_MCS_BW_40;
        *p |= stbc << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
        p++;
        *p++ = mcs;
    }

    /* AMPDU details */
    while((p - (u_int8_t *)rthdr) & 3){
        //4-byte alignment
        *p++ = 0;
    }

    put_unaligned_le32(0, p); // AMPDU Reference number
    p += 4;
    //3rd and 4th bits represents the AMPDU Status flags
    ampdu_flags |= ((rs->rs_flags & 0x0c00) >> 8);
    put_unaligned_le16(ampdu_flags, p);
    p += 4;

    /*
     * VHT Field should be 2-byte aligned
     */
    p = ((u_int8_t *)rthdr) + ALIGN(p-((u_int8_t *)rthdr),2);

    if (vht) {
        /* u16 known, u8 flags, u8 bandwidth, u8 mcs_nss[4], u8 coding, u8 group_id, u16 partial_aid */
        rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_VHT));

        /* known */
        radiotap_vht_details |= IEEE80211_RADIOTAP_VHT_KNOWN_STBC | \
                        IEEE80211_RADIOTAP_VHT_KNOWN_GI | IEEE80211_RADIOTAP_VHT_KNOWN_BEAMFORMED | IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH | \
                        IEEE80211_RADIOTAP_VHT_KNOWN_GROUP_ID | IEEE80211_RADIOTAP_VHT_KNOWN_PARTIAL_AID;
        /* TODO: for 80+80, need to clear IEEE80211_RADIOTAP_VHT_KNOWN_BANDWIDTH */
        put_unaligned_le16(radiotap_vht_details, p);
        p += 2;

        /* flags */
        if(stbc)
            *p |= IEEE80211_RADIOTAP_VHT_FLAG_STBC;
        if(sgi)
            *p |= IEEE80211_RADIOTAP_VHT_FLAG_SGI;
        if(txbf)
            *p |= IEEE80211_RADIOTAP_VHT_FLAG_BEAMFORMED;
        p++;

        /* bandwidth */
        if(bw == 80)
            *p = 4;
        else if(bw == 8080)
            /* TOTO: for 80+80 need to fix this */
            *p = 11;
        else if(bw == 160)
            *p = 11;
        else if(bw == 40)
            *p = 1;
        else /* 20 */
            *p = 0;
        p++;

        /* mcs_nss */
        if (su_ppdu) {
            /* For SU PPDUs, only the first user will have a nonzero NSS field. */
            *p = (mcs << 4)|nss;
            p += 4;
        } else {
            /* MU-PPDU */
            *p++ = (mcs << 4)|nss;
            *p++ = (mcs << 4)|nss;
            *p++ = (mcs << 4)|nss;
            *p++ = (mcs << 4)|nss;
        }

        /* coding */
        if (su_ppdu) {
            *p++ = ldpc;
        } else {
            if(ldpc_u0)
                vht_coding_details |= 1;
            if(ldpc_u1)
                vht_coding_details |= 1<<1;
            if(ldpc_u2)
                vht_coding_details |= 1<<2;
            if(ldpc_u3)
                vht_coding_details |= 1<<3;
            *p++ = vht_coding_details;
        }

        /* group_id  SU_PPDU(0 or 63) else it's MU_PPDU */
        *p = groupid;
        p++;

        /* partial_aid */
        put_unaligned_le16(partial_aid, p);
        p += 2;
    }

    rthdr->it_present |= cpu_to_le32(BIT(IEEE80211_RADIOTAP_VENDOR_SPECIFIC));

    /*
     * Vendor Namespace should be 2-byte aligned
     */
    p = ((u_int8_t *)rthdr) + ALIGN(p-((u_int8_t *)rthdr),2);

    radiotap_vendor_ns_ath = (struct qdf_radiotap_vendor_ns_ath *)p;
    /*
     * Copy Atheros OUI - 3 bytes (4th byte is 0)
     */
    put_unaligned_le32(ATH_OUI, radiotap_vendor_ns_ath->hdr.oui);
    /*
     * Name space selector = 0
     * We only will have one namespace for now
     */
    radiotap_vendor_ns_ath->hdr.selector = 0;
    put_unaligned_le16(
            sizeof(*radiotap_vendor_ns_ath) -
            sizeof(radiotap_vendor_ns_ath->hdr),
            &radiotap_vendor_ns_ath->hdr.skip_length);
    put_unaligned_le32(rs->rs_lsig_word, &radiotap_vendor_ns_ath->lsig);
    put_unaligned_le32(scn->soc->device_id, &radiotap_vendor_ns_ath->device_id);

    return;
}


void osif_mon_add_prism_header(os_if_t osif, struct sk_buff *skb, ieee80211_recv_status *rs)
{
    osif_dev *osifp = (osif_dev *)osif;
    struct ieee80211vap *vap = osifp->os_if;
    struct net_device *dev = osifp->netdev;
    struct ieee80211com *ic = wlan_vap_get_devhandle(vap);

    wlan_ng_prism2_header *phdr;
    wlan_ng_prism2_header template = {
        .msgcode = (DIDmsg_lnxind_wlansniffrm),
        .msglen = (sizeof(wlan_ng_prism2_header)),

        .hosttime.did = (DIDmsg_lnxind_wlansniffrm_hosttime),
        .hosttime.status = 0,
        .hosttime.len = (4),

        .mactime.did = (DIDmsg_lnxind_wlansniffrm_mactime),
        .mactime.status = 0,
        .mactime.len = (4),

        .istx.did = (DIDmsg_lnxind_wlansniffrm_istx),
        .istx.status = 0,
        .istx.len = (4),
        .istx.data = (P80211ENUM_truth_false),

        .frmlen.did = (DIDmsg_lnxind_wlansniffrm_frmlen),
        .frmlen.status = 0,
        .frmlen.len = (4),

        .channel.did = (DIDmsg_lnxind_wlansniffrm_channel),
        .channel.status = 0,
        .channel.len = (4),

        .rssi.did = (DIDmsg_lnxind_wlansniffrm_rssi),
        .rssi.status = P80211ENUM_msgitem_status_no_value,
        .rssi.len   = (4),
        .signal.did = (DIDmsg_lnxind_wlansniffrm_rate_phy1),
        .signal.status = 0,
        .signal.len = (4),

        .noise.did = (DIDmsg_lnxind_wlansniffrm_rate_phy2),
        .noise.status = 0,
        .noise.len = (4),
        /*
         * The older version of the Wireshark application had a bug where
         * it would decode noise as rate. This is fixed in the latest
         * version. Validated with WireShark version 1.6.8. If you
         * would still want to use the older application please change
         * DIDmsg_lnxind_wlansniffrm_rate to DIDmsg_lnxind_wlansniffrm_noise
         * below.
         */
        .rate.did = (DIDmsg_lnxind_wlansniffrm_rate_phy3),
        .rate.status = 0,
        .rate.len = (4),
    };

    if (!rs) {
	/* prism header is already added */
	return;
    }

    if (skb_headroom(skb) < sizeof(wlan_ng_prism2_header)) {
        int delta = sizeof(wlan_ng_prism2_header) - skb_headroom(skb);
        skb = qdf_nbuf_realloc_headroom(skb, SKB_DATA_ALIGN(delta));
        if (skb == NULL) {
            qdf_print(" %s[%d] skb_realloc_headroom failed ",__func__,__LINE__);
            return ;
        }
    }

    phdr = (wlan_ng_prism2_header *) skb_push(skb, sizeof(wlan_ng_prism2_header));
    OS_MEMZERO(phdr, sizeof(wlan_ng_prism2_header));
    *phdr = template;

    phdr->hosttime.data = (jiffies);
    phdr->mactime.data = (rs->rs_tstamp.tsf);
    phdr->frmlen.data = skb->len;
    phdr->channel.data = wlan_mhz2ieee(ic, rs->rs_freq, 0);
    phdr->rssi.data = (rs->rs_rssi);
    phdr->signal.data = (rs->rs_rssi);

#if 0 /* Legacy rates */
    phdr->rate.data = (rs->rs_datarate/500);
#else /* VHT rates */
    phdr->signal.data = rs->rs_ratephy1;
    phdr->noise.data = rs->rs_ratephy2;
    phdr->rate.data = rs->rs_ratephy3;
#endif

    /*
     * Vendor info data that is added as part of Radiotap,
     * is not added in PRISM header
     */

    if (strlcpy(phdr->devname, dev->name, sizeof(phdr->devname)) >= sizeof(phdr->devname)) {
        qdf_print("source too long");
        return;
    }

    return;
}

