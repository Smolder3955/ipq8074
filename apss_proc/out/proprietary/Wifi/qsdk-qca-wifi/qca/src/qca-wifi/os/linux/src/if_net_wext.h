/*
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

#ifndef __QDF_NET_WEXT_PVT_H
#define __QDF_NET_WEXT_PVT_H

/* scan section */
/**
 * @brief
 */
struct iwscanreq
{
    int8_t      *current_ev;
    int8_t      *end_buf;
    int32_t      mode;
    struct iw_request_info *info;
};

/**
 * @brief Scan result data returned SIOCGIWSCAN
 */
struct acfg_scan_result {
    uint16_t    isr_len;        /* length (mult of 4) */
    uint16_t    isr_freq;        /* MHz */
    uint32_t    isr_flags;        /* channel flags */
    uint8_t     isr_noise;
    uint8_t     isr_rssi;
    uint8_t     isr_intval;        /* beacon interval */
    uint16_t    isr_capinfo;        /* capabilities */
    uint8_t     isr_erp;        /* ERP element */
    uint8_t     isr_bssid[ADDR_LEN];
    uint8_t     isr_nrates;
    uint8_t     isr_rates[RATE_MAXSIZE];
    uint8_t     isr_ssid_len;        /* SSID length */
    uint16_t    isr_ie_len;        /* IE length */
    uint8_t     isr_pad[4];
    /* variable length SSID followed by IE data */
};

/**
 * @brief IE List in Scan results
 */
struct acfg_ie_list {
    uint8_t  *se_wpa_ie ;
    uint8_t  *se_rsn_ie;
    uint8_t  *se_ath_ie ;
    uint8_t  *se_wps_ie;
    uint8_t  *se_wmeinfo ;
    uint8_t  *se_wmeparam ;
    uint8_t  *htcap_ie;
    uint8_t  *htinfo_ie;
    uint8_t  *xrates_ie;
    uint8_t  *vendor;
};


/* unaligned little endian access */
#define LE_READ_4(p)					\
    ((uint32_t)					\
     ((((const uint8_t *)(p))[0]      ) |		\
      (((const uint8_t *)(p))[1] <<  8) |		\
      (((const uint8_t *)(p))[2] << 16) |		\
      (((const uint8_t *)(p))[3] << 24)))

#define BE_READ_4(p)                        \
    ((uint32_t)                            \
     ((((const uint8_t *)(p))[0] << 24) |      \
      (((const uint8_t *)(p))[1] << 16) |      \
      (((const uint8_t *)(p))[2] <<  8) |      \
      (((const uint8_t *)(p))[3]      )))

#define ARPHRD_ETHER            1
#define MAX_IE_LENGTH           (30 + 255 * 2) /* "xxx_ie=" + encoded IE */


/* Element ID  */
#define IEEE80211_ELEMID_RSN 48
#define IEEE80211_ELEMID_VENDOR 221
#define IEEE80211_CAPINFO_PRIVACY           0x0010

/* OUI Definitions */
#define WPS_OUI  0xf25000
#define WPS_OUI_TYPE  0x04
#define WSC_OUI 0x0050f204

#define	WME_OUI			0xf25000
#define	WME_OUI_TYPE		0x02
#define WME_PARAM_OUI_SUBTYPE 0x01
#define WME_INFO_OUI_SUBTYPE 0x00

#define	WPA_OUI_TYPE		0x01
#define	WPA_OUI			0xf25000

#define	ATH_OUI			0x7f0300		/* Atheros OUI */
#define	ATH_OUI_TYPE		0x01

#define IEEE80211_RATE_MAXSIZE 36
#define IEEE80211_ADDR_LEN      6

/**
 * @brief Channel Number to Frequency
 *
 * @param chan
 *
 * @return
 */
static uint32_t
__ieee80211_ieee2mhz(uint32_t chan)
{
    if(chan == 14)  return 2484;
    if(chan < 14)   return 2407 + (chan * 5);
    if( chan < 27 ) return 2512 + ((chan - 15) * 20);

    return 5000 + (chan * 5);
}

static inline int8_t
isxrates(const uint8_t *frm)
{
    return(frm[0] == 50);
}

static inline int8_t
iswpsoui(const uint8_t *frm)
{
    return frm[1] > 3 && BE_READ_4(frm+2) == WSC_OUI;
}

static inline int8_t
iswmeoui(const uint8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

static inline int8_t
iswmeparam(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) ==
                ((WME_OUI_TYPE<<24)|WME_OUI)) &&
            (frm[6] == WME_PARAM_OUI_SUBTYPE));
}

static inline int8_t
iswmeinfo(const u_int8_t *frm)
{
    return ((frm[1] > 5) && (LE_READ_4(frm+2) ==
                ((WME_OUI_TYPE<<24)|WME_OUI)) &&
            (frm[6] == WME_INFO_OUI_SUBTYPE));
}

static inline int8_t
iswpaoui(const uint8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}


static inline int8_t
isatherosoui(const uint8_t *frm)
{
    return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}



/**
 * @brief Encode a WPA or RSN information element as a custom
 *        element using the hostap format.
 * @param buf
 * @param bufsize
 * @param ie
 * @param ielen
 * @param leader
 * @param leader_len
 *
 * @return
 */
static inline uint32_t
__encode_ie(void *buf, size_t bufsize,
        const uint8_t *ie, size_t ielen,
        const int8_t *leader, size_t leader_len)
{
    uint8_t *p;
    int32_t  i;

    if (bufsize < leader_len)
        return 0;

    p = buf;
    qdf_mem_copy(p, leader, leader_len);

    bufsize -= leader_len;
    p       += leader_len;

    for (i = 0; i < ielen && bufsize > 2; i++) {
        p += sprintf(p, "%02x", ie[i]);
        bufsize -= 2;
    }

    return (i == ielen ? p - (uint8_t *)buf : 0);
}

/**
 * @brief Populate IE structure
 *
 * @param vp
 * @param ielen
 * @param ie
 */
static inline void
__parseie(uint8_t *vp, int32_t ielen, struct acfg_ie_list *ie )
{

    while (ielen > 0) {
        switch (vp[0]) {
            case IEEE80211_ELEMID_VENDOR:
                if (iswpaoui(vp))
                    ie->se_wpa_ie = vp;
                else if(iswmeparam(vp) || iswmeoui(vp))
                    ie->se_wmeparam = vp;
                else if(iswmeinfo(vp) || iswmeoui(vp))
                    ie->se_wmeinfo = vp;
                else if (isatherosoui(vp))
                    ie->se_ath_ie = vp;
                else if (iswpsoui(vp) && !ie->se_wps_ie)
                    ie->se_wps_ie = vp;
                else if (isxrates(vp))
                    ie->xrates_ie = vp;
                else
                    ie->vendor = NULL;
                break;
            case IEEE80211_ELEMID_RSN:
                ie->se_rsn_ie = vp;
                break;
            default:
                break;
        }
        ielen -= 2+vp[1];
        vp += 2+vp[1];
    }
}


/**
 * @brief Convert Rssi to WE Quality format
 *
 * @param iq
 * @param rssi
 */
static inline void
__set_quality(struct iw_quality *iq, uint32_t rssi)
{
    if(rssi >= 42)
        iq->qual = 94 ;
    else if(rssi >= 30)
        iq->qual = 85 + ((94-85)*(rssi-30) + (42-30)/2) / (42-30) ;
    else if(rssi >= 5)
        iq->qual = 5 + ((rssi - 5) * (85-5) + (30-5)/2) / (30-5) ;
    else if(rssi >= 1)
        iq->qual = rssi ;
    else
        iq->qual = 0 ;

    iq->noise = 161;        /* -95dBm */
    iq->level = iq->noise + rssi ;
    iq->updated = 0xf;
}


#endif /*__QDF_NET_PVT_H*/

