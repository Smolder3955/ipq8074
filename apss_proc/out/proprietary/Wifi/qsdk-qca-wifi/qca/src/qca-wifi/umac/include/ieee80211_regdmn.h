/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *  Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#ifndef _NET80211_IEEE80211_REGDMN_H
#define _NET80211_IEEE80211_REGDMN_H

#include <ieee80211_var.h>

int ieee80211_set_country_code(struct ieee80211com *ic, char* isoName, u_int16_t cc, enum ieee80211_clist_cmd cmd);
void ieee80211_update_spectrumrequirement(struct ieee80211vap *vap, bool *thread_started);
int ieee80211_update_country_allowed(struct ieee80211com *ic);
void ieee80211_set_regclassids(struct ieee80211com *ic, const u_int8_t *regclassids, u_int nregclass);

#define FULL_BW 20
#define HALF_BW 10
#define QRTR_BW  5

#define BW_WITHIN(min, bw, max) ((min) <= (bw) && (bw) <= (max))

/* Offset between two HT20 channels is 20MHz */
#define CHAN_HT40_OFFSET 20

#define MAX_CHANNELS_PER_OPERATING_CLASS  24
typedef enum {
	IEEE80211_MIN_2G_CHANNEL = 1,
	IEEE80211_MAX_2G_CHANNEL = 14,
	IEEE80211_MIN_5G_CHANNEL = 36,
	IEEE80211_MAX_5G_CHANNEL = 169,
} IEEE80211_MIN_MAX_CHANNELS;

/* Supported STA Bands*/
typedef enum {
	IEEE80211_2G_BAND,
	IEEE80211_5G_BAND,
	IEEE80211_INVALID_BAND,
} IEEE80211_STA_BAND;

typedef struct regdmn_op_class_map {
    uint8_t op_class;
    enum ieee80211_cwm_width ch_width;
    uint8_t sec20_offset;
    uint8_t ch_set[MAX_CHANNELS_PER_OPERATING_CLASS];
} regdmn_op_class_map_t;

uint8_t
regdmn_get_new_opclass_from_channel(struct ieee80211com *ic,
                                    struct ieee80211_ath_channel *channel);

void regdmn_update_ic_channels(
        struct wlan_objmgr_pdev *pdev,
        struct ieee80211com *ic,
        uint32_t mode_select,
        struct regulatory_channel *curr_chan_list,
        struct ieee80211_ath_channel *chans,
        u_int maxchans,
        u_int *nchans,
        uint32_t low_2g,
        uint32_t high_2g,
        uint32_t low_5g,
        uint32_t high_5g);
/* Get sta band capabilities from supporting opclass */
uint8_t regdmn_get_band_cap_from_op_class(struct ieee80211com *ic,
                          uint8_t no_of_opclass,const  uint8_t *opclass);

void regdmn_get_channel_list_from_op_class(
        uint8_t reg_class,
        struct ieee80211_node *ni);
uint8_t regdmn_get_opclass (uint8_t *country_iso, struct ieee80211_ath_channel *channel);

uint8_t regdmn_get_map_opclass(u_int8_t minch, u_int8_t maxch, mapapcap_t *apcap,
                               int8_t pow, struct map_op_chan_t *map_op_chan);

#endif /* _NET80211_IEEE80211_REGDMN_H */
