/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#ifndef _IEEE80211_BSS_COLOR_H
#define _IEEE80211_BSS_COLOR_H

#include <qdf_timer.h>

/* Max value for dot11BSSColorCollisionAPPeriod */
#define IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_MAX (255000)
/* dot11BSSColorCollisionAPPeriod: set default value as 50s */
#define IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD           \
            (IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_MAX/5)

#define IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_DIVISOR_LOW  (3)
#define IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_DIVISOR_HIGH (5)

/* set default update period for the received bsscolor map from fw
 * to the below pre-configured value of 5 mins
 */
#define IEEE80211_BSS_COLOR_RECVD_BSSCOLOR_MAP_UPDATE_PERIOD (300000)

/*
 * Default Parameter values for color collision cfg cmd
 * IEEE80211_BSS_COLOR_COLLISION_DETECTION_STA_PERIOD_MS: sta detection period in ms
 * IEEE80211_BSS_COLOR_COLLISION_DETECTION_AP_PERIOD_MS: ap detection period in ms
 * IEEE80211_BSS_COLOR_COLLISION_SCAN_PERIOD_MS: scan period in ms
 * IEEE80211_BSS_COLOR_COLLISION_FREE_SLOT_EXPIRY_MS: free slot expiry in ms
 */
#define IEEE80211_BSS_COLOR_COLLISION_DETECTION_STA_PERIOD_MS 10000
#define IEEE80211_BSS_COLOR_COLLISION_DETECTION_AP_PERIOD_MS   5000
#define IEEE80211_BSS_COLOR_COLLISION_SCAN_PERIOD_MS            200
#define IEEE80211_BSS_COLOR_COLLISION_FREE_SLOT_EXPIRY_MS     50000

#define IEEE80211_BSS_COLOR_INVALID                    (64)
#define IEEE80211_BSS_COLOR_MID                        (32)
#define IEEE80211_MAX_MACREF_COLOR_NODE_CACH_SIZE      (1024)
#define IEEE80211_DEFAULT_BSSCOLOR_CHG_COUNT           (10)

#define IEEE80211_BSS_COLOR_LIST_UPDATE_OPP_PERIOD_GET(                   \
                                opp_timer_period, ap_period)              \
do {                                                                      \
    if (ap_period < (                                                     \
                IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_MAX/2)) {         \
        opp_timer_period =                                                \
                (ap_period /                                              \
                 IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_DIVISOR_LOW);    \
    } else {                                                              \
        opp_timer_period =                                                \
                (ap_period /                                              \
                 IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD_DIVISOR_HIGH);   \
    }                                                                     \
} while(0)

#define IEEE80211_MARK_COLOR_FREE_IN_BITMAP(color)                        \
do {                                                                      \
    if (color < IEEE80211_BSS_COLOR_MID) {                                \
        bsscolor_hdl->recvd_bsscolor_map_low &= ~(1 << color);            \
    } else {                                                              \
        bsscolor_hdl->recvd_bsscolor_map_high &=                          \
                     ~(1 << (color - IEEE80211_BSS_COLOR_MID));           \
    }                                                                     \
} while(0)

#define IEEE80211_IS_COLOR_FREE_IN_BITMAP(color, free)                    \
do {                                                                      \
    if (color < IEEE80211_BSS_COLOR_MID) {                                \
        free = !(bsscolor_hdl->recvd_bsscolor_map_low & (1 << color));    \
    } else {                                                              \
        free = !(bsscolor_hdl->recvd_bsscolor_map_high &                  \
                         (1 << (color - IEEE80211_BSS_COLOR_MID)));       \
    }                                                                     \
} while (0)

#define IEEE80211_IS_COLOR_MAP_FREE(map_low, map_high) !(map_low | map_high)

typedef enum {
    IEEE80211_BSS_COLOR_CTX_STATE_INACTIVE,
    IEEE80211_BSS_COLOR_CTX_STATE_ACTIVE,
    IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTING,
    IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTED,
    IEEE80211_BSS_COLOR_CTX_STATE_COLOR_DISABLED,
    IEEE80211_BSS_COLOR_CTX_STATE_COLOR_USER_DISABLED,
} ieee80211_bss_color_ctx_state;

/*
 * 802.11ax neighbour BSS Color info
 */
struct ieee80211_bsscolor {
    uint32_t color_count;
};

struct __macref_color_node {
    qdf_list_node_t node;
    uint32_t mac_ref;       /* mac_ref is the hash of the 48 bit long mac */
    uint8_t  color;         /* color is color corresponding to the bss denoted
                             * by the mac_ref
                             */
    qdf_time_t lastrx_ts;   /* timestamp when the color was last updated */
};

struct __unused_color_set {
    uint8_t unused_color[IEEE80211_HE_BSS_COLOR_MAX + 1];
    uint8_t count;
};

struct ieee80211_bsscolor_handle {
    os_timer_t dot11_bss_color_collision_ap_timer; /* max time period within
                                                    * which AP is supposed
                                                    * to find an unique random
                                                    * BSS Color */
    os_timer_t color_list_update_opp_timer;        /* color list update opportunity
                                                    * period */
    os_timer_t update_recvd_bsscolor_map_timer;    /* timer to update the received
                                                    * bsscolor map from fw in
                                                    * bsscolor collision detection
                                                    * event */
    enum {
        INACTIV,
        RUNNING,
        EXPIRED,
    } dot11_bss_color_collision_ap_timer_state;
    bool color_list_update_opp_timer_running;
    bool update_recvd_bsscolor_map_timer_running;
    uint32_t bsscolor_collision_ap_period; /* this variable will hold current
                                            * dot11bsscolorcollisionAPPeriod
                                            * in the units of millisecond */
    uint32_t color_list_update_opp_timer_period;
    uint16_t bsscolor_change_count;        /* bsscolor change count. user can
                                            * overwrite this */
    ieee80211_bss_color_ctx_state state;
    uint8_t    selected_bsscolor;          /* selected BSS color */
    uint8_t    prev_bsscolor;              /* BSS color before disabling */
    bool       bcca_not_reqd;              /* BCCA required or not */
    qdf_time_t selected_bsscolor_tstamp;   /* last selected bss color timestamp */
    struct ieee80211_bsscolor  bsscolor_list[IEEE80211_HE_BSS_COLOR_MAX + 1];
    qdf_list_t macref_color_list;          /* list of mac references and color.
                                            * look at struct __macref_color_node
                                            * above */
    qdf_spinlock_t bsscolor_list_lock;
    uint32_t recvd_bsscolor_map_low;       /* lower half of last received
                                            * bsscolor map in bsscolor collision
                                            * detection event from fw */
    uint32_t recvd_bsscolor_map_high;      /* upper half of last received
                                            * bsscolor map in bsscolor collision
                                            * detection event from fw */
    struct __unused_color_set unused_color_set;
    void (*ieee80211_bss_color_collision_detection_hdler_cb)
        (struct ieee80211com *ic, uint32_t bsscolor_bitmap_low,
         uint32_t bsscolor_bitmap_high);
};

/* BCCA IE update status */
enum bcca_ie_status{
    BCCA_NA         = 0,
    BCCA_START      = 1,
    BCCA_ONGOING    = 2,
};

int ieee80211_bsscolor_attach(struct ieee80211com *ic);

int ieee80211_bsscolor_detach(struct ieee80211com *ic);

void ieee80211_setup_bsscolor(struct ieee80211vap *vap,
                              struct ieee80211_ath_channel *chan);
void ieee80211_select_new_bsscolor(struct ieee80211com *ic);
int ieee80211_set_ic_heop_bsscolor_disabled_bit(
                        struct ieee80211com *ic, bool disable);
int ieee80211_is_bcca_ongoing_for_any_vap(struct ieee80211com *ic);
void ieee80211_override_bsscolor_collsion_ap_period(
      struct ieee80211com *ic, uint8_t bsscolor_collision_ap_period);
void ieee80211_override_bsscolor_change_count(struct ieee80211com *ic,
                                      uint16_t bsscolor_change_count);
QDF_STATUS ieee80211_process_beacon_for_bsscolor_cache_update(void *arg,
                                                 wlan_scan_entry_t se);

#endif /* _IEEE80211_BSS_COLOR_H */

