/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <linux/hash.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_bsscolor.h>
#include <ieee80211_defines.h>
#include <wlan_scan_ucfg_api.h>
#include <ol_if_athvar.h>

#define HE_BSS_COLOR_MAX_LIFE_S   (30)
#define HE_BSS_COLOR_MAX_LIFE_MS  (HE_BSS_COLOR_MAX_LIFE_S*1000)

struct _unused_color_set {
    uint8_t unused_color[IEEE80211_HE_BSS_COLOR_MAX + 1];
    uint8_t count;
} unused_color_set;

static void ieee80211_prune_and_adjust_color_count_in_list
                    (struct ieee80211_bsscolor_handle *bsscolor_hdl);

static inline uint32_t
__get_mac_hash(uint8_t *mac)
{
    uint32_t x_low, x_high;
    uint64_t x_64;

    x_low  = ((uint32_t) mac[0]) | (((uint32_t) mac[1]) << 8) | \
                    (((uint32_t) mac[2]) << 16);
    x_high = ((uint32_t) mac[3]) | (((uint32_t) mac[4]) << 8) | \
                    (((uint32_t) mac[5]) << 16);
    x_64   = ((uint64_t) mac[0]) | (((uint64_t) mac[1]) << 8) | \
             (((uint64_t) mac[2]) << 16) | (((uint64_t) mac[3]) << 24) | \
             (((uint64_t) mac[4]) << 32) | (((uint64_t) mac[5]) << 40);

    x_64   = hash_64(x_64, 48);

    return (hash_32(x_low, 32) ^ hash_32(x_high, 32) ^ \
                (x_64 & 0xffffffff) ^ (x_64 >> 32));
}

static void ieee80211_bsscolor_post_color_selection(struct ieee80211com *ic) {
    struct ieee80211vap *vap;
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                                            "%s>>", __func__);
    if (!bsscolor_hdl->bcca_not_reqd) {
        ic->ic_he_bsscolor_change_tbtt = bsscolor_hdl->bsscolor_change_count;

        /* reset the color count for each vap */
        TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
            if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
                vap->iv_he_bsscolor_change_count   = 0;
                if (vap->iv_is_up) {
                    vap->iv_he_bsscolor_change_ongoing = true;
                }
            }
        }

        /* keep bsscolor disabled for the time leading upto
         * bsscolor change TBTT
         */
        ieee80211_set_ic_heop_bsscolor_disabled_bit(ic, true);

        /* register for offload beacon tx status evnet handler */
        if (ic->ic_register_beacon_tx_status_event_handler) {
            ic->ic_register_beacon_tx_status_event_handler(ic, false);
        }
    } else {
        ieee80211_set_ic_heop_bsscolor_disabled_bit(ic, false);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_list_update_opp_timer_start(
                      struct ieee80211_bsscolor_handle *bsscolor_hdl) {
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    if(!bsscolor_hdl->color_list_update_opp_timer_running) {
        bsscolor_hdl->color_list_update_opp_timer_running = true;
        qdf_timer_mod(&bsscolor_hdl->color_list_update_opp_timer,
                        bsscolor_hdl->color_list_update_opp_timer_period);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_collision_ap_timer_start(
                      struct ieee80211_bsscolor_handle *bsscolor_hdl) {
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    if(bsscolor_hdl->dot11_bss_color_collision_ap_timer_state != RUNNING) {
        bsscolor_hdl->dot11_bss_color_collision_ap_timer_state = RUNNING;
        qdf_timer_mod(&bsscolor_hdl->dot11_bss_color_collision_ap_timer,
                            bsscolor_hdl->bsscolor_collision_ap_period);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_update_recvd_bsscolor_map_timer_start(
                      struct ieee80211_bsscolor_handle *bsscolor_hdl) {
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    if(!bsscolor_hdl->update_recvd_bsscolor_map_timer_running) {
        bsscolor_hdl->update_recvd_bsscolor_map_timer_running = true;
        qdf_timer_mod(&bsscolor_hdl->update_recvd_bsscolor_map_timer,
                      IEEE80211_BSS_COLOR_RECVD_BSSCOLOR_MAP_UPDATE_PERIOD);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_list_update_opp_timer_cancel(
                      struct ieee80211_bsscolor_handle *bsscolor_hdl) {
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    if (bsscolor_hdl->color_list_update_opp_timer_running) {
        qdf_timer_stop(&bsscolor_hdl->color_list_update_opp_timer);
        bsscolor_hdl->color_list_update_opp_timer_running = false;
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_collision_ap_timer_cancel(
                      struct ieee80211_bsscolor_handle *bsscolor_hdl) {
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    if (bsscolor_hdl->dot11_bss_color_collision_ap_timer_state
            == RUNNING) {
        qdf_timer_stop(&bsscolor_hdl->dot11_bss_color_collision_ap_timer);
        bsscolor_hdl->dot11_bss_color_collision_ap_timer_state = INACTIV;
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_update_recvd_bsscolor_map_timer_cancel(
                      struct ieee80211_bsscolor_handle *bsscolor_hdl) {
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    if(bsscolor_hdl->update_recvd_bsscolor_map_timer_running) {
        qdf_timer_stop(&bsscolor_hdl->update_recvd_bsscolor_map_timer),
        bsscolor_hdl->update_recvd_bsscolor_map_timer_running = false;
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

/**
 * ieee80211_bsscolor_collision_ap_timeout - timout function
 * for dot11BSSColorCollisionAPPeriod.
 *
 * Search for a free slot for an unused BSS Color will be forfieted
 * at this timeout. BSS Color will be disabled and BCCA will be done
 */
static os_timer_func(ieee80211_bsscolor_collision_ap_timeout)
{
    struct ieee80211com *ic                        = NULL;
    struct ieee80211_bsscolor_handle  *bsscolor_hdl = NULL;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    bsscolor_hdl = &ic->ic_bsscolor_hdl;

    /* cancel colorListUpdateOppurtunityPeriod if already running */
    ieee80211_bsscolor_list_update_opp_timer_cancel(bsscolor_hdl);

    qdf_spin_lock_bh(&bsscolor_hdl->bsscolor_list_lock);

    switch (bsscolor_hdl->state) {
        case IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTED:
            /* post processing following color selection */
            ieee80211_bsscolor_post_color_selection(ic);
        break;
        case IEEE80211_BSS_COLOR_CTX_STATE_ACTIVE:
        case IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTING:
            /* if not already in user_disabled state then change
             * state to color disabled state and disable color
             */
            bsscolor_hdl->state = IEEE80211_BSS_COLOR_CTX_STATE_COLOR_DISABLED;
            ieee80211_set_ic_heop_bsscolor_disabled_bit(ic, true);
        break;
        default:
            /* do nothing */
        break;
    }

    bsscolor_hdl->dot11_bss_color_collision_ap_timer_state = EXPIRED;

    qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

/**
 * ieee80211_bsscolor_list_update_opp_period_timeout - timout function
 * for BSS Color Availability List update period
 *
 * At this timeout an attempt will be made to see if a free slot for
 * unused BSS Color is found
 */
static os_timer_func(ieee80211_bsscolor_list_update_opp_period_timeout)
{
    struct ieee80211com *ic                        = NULL;
    struct ieee80211_bsscolor_handle *bsscolor_hdl = NULL;
    bool reinvoke_bsscolor_selection               = false;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    bsscolor_hdl = &ic->ic_bsscolor_hdl;

    qdf_spin_lock_bh(&bsscolor_hdl->bsscolor_list_lock);

    switch (bsscolor_hdl->state) {
        case IEEE80211_BSS_COLOR_CTX_STATE_COLOR_DISABLED:
        case IEEE80211_BSS_COLOR_CTX_STATE_COLOR_USER_DISABLED:
            /* In user_disabled state. Cancel dot11BSSColorCollisionAPPeriod */
            ieee80211_bsscolor_collision_ap_timer_cancel(bsscolor_hdl);
        break;
        case IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTED:
            if (bsscolor_hdl->dot11_bss_color_collision_ap_timer_state
                    == EXPIRED) {
                /* Post processing following color selection */
                ieee80211_bsscolor_post_color_selection(ic);
            }
        break;
        default:
            reinvoke_bsscolor_selection = true;
    }

	bsscolor_hdl->color_list_update_opp_timer_running = false;

    qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);

    /* unprotected call to selection API to avoid dead lock
     * as the same lock will be acquired in the selection
     * API
     */
    if (reinvoke_bsscolor_selection) {
        ieee80211_select_new_bsscolor(ic);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

/**
 * ieee80211_bsscolor_update_recvd_bsscolor_map - timout function
 * for updating received bsscolor map from fw
 *
 * At this timeout an attempt will be made to update the received
 * bsscolor map based on the ongoing obss scan
 */
static os_timer_func(ieee80211_bsscolor_update_recvd_bsscolor_map_timeout) {
    struct ieee80211com *ic                        = NULL;
    struct ieee80211_bsscolor_handle *bsscolor_hdl = NULL;
    struct ieee80211_bsscolor *bsscolor_list       = NULL;
    uint8_t color;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    bsscolor_hdl  = &ic->ic_bsscolor_hdl;
    bsscolor_list = (struct ieee80211_bsscolor *) bsscolor_hdl->bsscolor_list;

    bsscolor_hdl->update_recvd_bsscolor_map_timer_running = false;

    /* Run update recvd_bsscolor_map algo if there are
     * used colors in the received bitmap from fw */
    if (!IEEE80211_IS_COLOR_MAP_FREE(bsscolor_hdl->recvd_bsscolor_map_low,
                                     bsscolor_hdl->recvd_bsscolor_map_high)) {
        /* Prune the mac_ref_color list and get fresh color
         * counts
         */
        qdf_spin_lock_bh(&bsscolor_hdl->bsscolor_list_lock);
        /* According to 11ax draft 3.0, section 27.16.2.2.1,
         * the HE AP shall set the BSS Color Disabled subfield
         * to 1 in the HE Operation element that it transmits
         * if the BSS color collision persists for a duration
         * of at least dot11BSSColorCollisionAPPeriod.
         *
         * Following function will set the Disabled bit to 1.
         * It will perform BCCA if a color is already found.
         * Otherwise, it will avoid BCCA
         */
        ieee80211_prune_and_adjust_color_count_in_list(bsscolor_hdl);

        for (color = IEEE80211_HE_BSS_COLOR_MIN + 1;
                color <= IEEE80211_HE_BSS_COLOR_MAX; color++) {
            if (bsscolor_list[color].color_count == 0) {
                /* color is a fee color. mark it as free color
                 * in the received color bitmap from fw */
                IEEE80211_MARK_COLOR_FREE_IN_BITMAP(color);
            }
        }
        qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);
    }

    /* since list is updated now we need to check again
     * whether the color map is free or not */
    if (!IEEE80211_IS_COLOR_MAP_FREE(bsscolor_hdl->recvd_bsscolor_map_low,
                                     bsscolor_hdl->recvd_bsscolor_map_high)) {
        /* restart timer */
        ieee80211_bsscolor_update_recvd_bsscolor_map_timer_start(bsscolor_hdl);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}


/** ieee80211_bsscolor_timer_init - initialize timers and related params
 *
 * This function inializes dot11BSSColorCollisionAPPeriod and colorList-
 * UpdateOppurtunityPeriod timers.
 */
static void
ieee80211_bsscolor_timer_init(struct ieee80211com *ic)
{
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    /* initialize is_running flags for both the timers */
    bsscolor_hdl->dot11_bss_color_collision_ap_timer_state = INACTIV;
    bsscolor_hdl->color_list_update_opp_timer_running      = false;

    /* initialize time-period for dot11BSSColorCollisionAPPeriod */
    bsscolor_hdl->bsscolor_collision_ap_period =
                                    IEEE80211_BSS_COLOR_COLLISION_AP_PERIOD;

    /* initialize timer-period for colorListUpdateOppurtunityPeriod */
    IEEE80211_BSS_COLOR_LIST_UPDATE_OPP_PERIOD_GET(
            bsscolor_hdl->color_list_update_opp_timer_period,
            bsscolor_hdl->bsscolor_collision_ap_period);

    /* initialize timer dot11BSSColorCollisionAPPeriod */
    qdf_timer_init(NULL, &bsscolor_hdl->dot11_bss_color_collision_ap_timer,
            ieee80211_bsscolor_collision_ap_timeout, (void*) ic,
            QDF_TIMER_TYPE_WAKE_APPS);

    /* initialize tiemr colorListUpdateOppurtunityPeriod */
    qdf_timer_init(NULL, &bsscolor_hdl->color_list_update_opp_timer,
            ieee80211_bsscolor_list_update_opp_period_timeout, (void*) ic,
            QDF_TIMER_TYPE_WAKE_APPS);

    /* initialize timer updateReceivedBsscolorMapTimer */
    qdf_timer_init(NULL, &bsscolor_hdl->update_recvd_bsscolor_map_timer,
            ieee80211_bsscolor_update_recvd_bsscolor_map_timeout, (void*) ic,
            QDF_TIMER_TYPE_WAKE_APPS);

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_bsscolor_collision_detection_hdler(
        struct ieee80211com *ic,
        uint32_t bsscolor_map_low,
        uint32_t bsscolor_map_high) {
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                    "%s>> bitmap_low: %x bitmap_high: %x", __func__,
                        bsscolor_map_low, bsscolor_map_high);

    ieee80211_bsscolor_update_recvd_bsscolor_map_timer_cancel(bsscolor_hdl);

    bsscolor_hdl->recvd_bsscolor_map_low  = bsscolor_map_low;
    bsscolor_hdl->recvd_bsscolor_map_high = bsscolor_map_high;

    if (!IEEE80211_IS_COLOR_MAP_FREE(bsscolor_map_low, bsscolor_map_high)) {
        /* start timer to peridically check and update
         * the received bsscolor map from fw on collision
         * detection */
        ieee80211_bsscolor_update_recvd_bsscolor_map_timer_start(bsscolor_hdl);
    }

    /* invoke selection API after bsscolor
     * collision detected */
    ieee80211_select_new_bsscolor(ic);

    /* Later Phase: update color map with received bitmaps from fw */
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                                        "%s<<", __func__);
}

/** ieee80211_override_bsscolor_collsion_ap_period - override
 *  dot11bsscolorcollisionAPPeriod.
 *
 *  It is expected that input bsscolor_collision_ap_period
 *  has to be between 50-255 seconds
 */
void ieee80211_override_bsscolor_collsion_ap_period(struct ieee80211com *ic,
        uint8_t bsscolor_collision_ap_period) {
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    /* convert to millisecond and save in bss color context */
    bsscolor_hdl->bsscolor_collision_ap_period = (bsscolor_collision_ap_period
                                                    * 1000);

    /* get new value of colorListUpdateOppurtunityPeriod
     * based on the new value of dot11BSSColorCollisionAPPeriod
     */
    IEEE80211_BSS_COLOR_LIST_UPDATE_OPP_PERIOD_GET(
            bsscolor_hdl->color_list_update_opp_timer_period,
            bsscolor_hdl->bsscolor_collision_ap_period);

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}
EXPORT_SYMBOL(ieee80211_override_bsscolor_collsion_ap_period);

/** ieee80211_override_bsscolor_change_count - override
 *  bsscolor_change_count.
 *
 *  the color change count is interms of beacon interval
 */
void ieee80211_override_bsscolor_change_count(struct ieee80211com *ic,
        uint16_t bsscolor_change_count) {
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    bsscolor_hdl->bsscolor_change_count = bsscolor_change_count;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}
EXPORT_SYMBOL(ieee80211_override_bsscolor_change_count);

/** ieee80211_prune_and_adjust_color_count_in_list - prunes
 * and readjusts color list
 *
 * the call to this function must be from a lock protected
 * critical section in bsscolor module to protect shared
 * data in this function
 */
static void
ieee80211_prune_and_adjust_color_count_in_list
                    (struct ieee80211_bsscolor_handle *bsscolor_hdl)
{
    qdf_time_t      now, time_delta;
    struct ieee80211_bsscolor *bsscolor_list =
                 (struct ieee80211_bsscolor *) bsscolor_hdl->bsscolor_list;
    struct __macref_color_node *ref_node   = NULL;
    qdf_list_t *list                       = &bsscolor_hdl->macref_color_list;
    qdf_list_node_t *next_list             = NULL;
    QDF_STATUS status;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    qdf_mem_zero(bsscolor_list,
        ((sizeof(struct ieee80211_bsscolor))*(IEEE80211_HE_BSS_COLOR_MAX + 1)));

    status = qdf_list_peek_front(list, &next_list);

    while (status == QDF_STATUS_SUCCESS) {
        now        = qdf_system_ticks();
        ref_node   =
            qdf_container_of(next_list, struct __macref_color_node, node);
        time_delta = qdf_system_ticks_to_msecs(now - ref_node->lastrx_ts);

        status = qdf_list_peek_next(list, &ref_node->node, &next_list);
        if (time_delta > HE_BSS_COLOR_MAX_LIFE_MS) {
            /* since the life of this entry has expirred
             * drecrease the color count and remove the
             * node from the list
             */

            /* decrease the color count */
            if (bsscolor_list[ref_node->color].color_count) {
                bsscolor_list[ref_node->color].color_count--;
            }

            /* remove node and get the next node */
            QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                "%s Deleting the node: %d", __func__, ref_node->color);
            if (qdf_list_remove_node(list, &ref_node->node)
                    == QDF_STATUS_SUCCESS) {
                qdf_mem_free(ref_node);
            } else {
                /* break out of the loop */
                break;
            }
        } else {
            /* the node stays in the list */

            /* increase the color count */
            bsscolor_list[ref_node->color].color_count++;
        }
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

void
ieee80211_bsscolor_update_bsscolor_list(struct __macref_color_node *inode,
	struct ieee80211_bsscolor_handle *bsscolor_hdl)
{
    qdf_list_t *list                  = &bsscolor_hdl->macref_color_list;

    struct __macref_color_node *ref_node   = NULL;
    qdf_list_node_t *next_list             = NULL;
    bool found                             = false;
    QDF_STATUS status;

    qdf_spin_lock_bh(&bsscolor_hdl->bsscolor_list_lock);

    /* add this node to the list if it does not
     * already appear in the list. If it already
     * exists in the list then update ts and color
     */
    status = qdf_list_peek_front(list, &next_list);

    while (status == QDF_STATUS_SUCCESS) {
        ref_node = qdf_container_of(next_list, struct __macref_color_node, node);
        if (ref_node->mac_ref == inode->mac_ref) {
            found = true;
            break;
        }
        status = qdf_list_peek_next(list, &ref_node->node, &next_list);
    }

    if (!found) {
        qdf_list_insert_back(list, &inode->node);
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO_LOW,
                "%s Added new node to list: 0x%x color: %d",
                __func__, inode->mac_ref, inode->color);
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                "%s Node already exists in list: ",__func__);
        if (ref_node->color != inode->color) {
            QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                "%s Updating color in existing node. mac_ref: 0x%x color: %d",
                __func__, ref_node->mac_ref, inode->color);
            ref_node->color = inode->color;
        }

        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                "%s Updating ts: %lu in existing node. mac_ref: 0x%x ",
                    __func__, inode->lastrx_ts, ref_node->mac_ref);
        ref_node->lastrx_ts = inode->lastrx_ts;
        qdf_mem_free(inode);
    }

    qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);
}

/*
 * Call back fucntion to trace BSS Color IE and
 * build BSS color list on operating channel
 */
QDF_STATUS
ieee80211_process_beacon_for_bsscolor_cache_update(void *iarg,
                                           wlan_scan_entry_t scan_entry)
{
    struct ieee80211com *ic                = (struct ieee80211com *)iarg;
    struct ieee80211_ath_channel *se_chan  = wlan_util_scan_entry_channel(scan_entry);
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;
    struct ieee80211_ie_heop *he_op        = NULL;
    struct ieee80211_ath_channel *cur_chan = NULL;
    struct __macref_color_node *node       = NULL;
#if SUPPORT_11AX_D3
    struct he_op_bsscolor_info heop_bsscolor_info;
#else
    struct he_op_param heop_param;
#endif
    uint8_t num_ap_vaps_up                 = 0, cur_ieeechan;
    uint8_t se_ieeechan                    = ieee80211_chan2ieee(ic, se_chan);

    /* No need to process beacons here if we are
     * non-HE target, not in AP mode or in inactive
     * state
     */
    if ((!ic->ic_he_target) ||
        (bsscolor_hdl->state == IEEE80211_BSS_COLOR_CTX_STATE_INACTIVE)) {
        return EOK;
    }

    num_ap_vaps_up = ieee80211_get_num_ap_vaps_up(ic);

    if (num_ap_vaps_up) {
        cur_chan =  ic->ic_curchan;
    } else {
        return EOK;
    }

    if (!cur_chan || (cur_chan == IEEE80211_CHAN_ANYC)) {
        return EOK;
    }

    if (!IEEE80211_IS_CHAN_11AX(cur_chan)) {
        return EOK;
    }

    cur_ieeechan = ieee80211_chan2ieee(ic, cur_chan);

    /* if current channel does not match scan
     * entry, just ignore and don't populate
     */
    if (se_ieeechan != cur_ieeechan) {
        /* please do not remove following log.
         * intended to be enabled in debug build
         * only
         */
        /* QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO_DEBUG,
                "%s Channel does not match. Ignoring beacon",__func__); */
        return EOK;
    }

    he_op = (struct ieee80211_ie_heop *) util_scan_entry_heop(scan_entry);

    if (!he_op) {
        /* please do not remove following log.
         * intended to be enabled in debug build
         * only
         */
        /* QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO_DEBUG,
                "%s HE OP not found. Ignoring beacon",__func__); */
        return EOK;
    }

#if SUPPORT_11AX_D3
    qdf_mem_copy(&heop_bsscolor_info, &he_op->heop_bsscolor_info,
            sizeof(he_op->heop_bsscolor_info));

    if ((heop_bsscolor_info.bss_color < IEEE80211_HE_BSS_COLOR_MIN) ||
        (heop_bsscolor_info.bss_color > IEEE80211_HE_BSS_COLOR_MAX))  {
        /* please do not remove following log.
         * intended to be enabled in debug build
         * only
         */
        /* QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                "%s Invalid color. Ignoring beacon",__func__); */
        return EOK;
    }
#else
    qdf_mem_copy(&heop_param, he_op->heop_param, sizeof(he_op->heop_param));

    if ((heop_param.bss_color < IEEE80211_HE_BSS_COLOR_MIN) ||
        (heop_param.bss_color > IEEE80211_HE_BSS_COLOR_MAX))  {
        /* please do not remove following log.
         * intended to be enabled in debug build
         * only
         */
        /* QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                "%s Invalid color. Ignoring beacon",__func__); */
        return EOK;
    }
#endif

    node = (struct __macref_color_node *)
                      qdf_mem_malloc(sizeof(struct __macref_color_node));

    if (node) {
        node->mac_ref   = __get_mac_hash(scan_entry->bssid.bytes);
#if SUPPORT_11AX_D3
        node->color     = heop_bsscolor_info.bss_color;
#else
        node->color     = heop_param.bss_color;
#endif
        node->lastrx_ts = qdf_system_ticks();

        ieee80211_bsscolor_update_bsscolor_list(node, bsscolor_hdl);
    }

    return EOK;
}
EXPORT_SYMBOL(ieee80211_process_beacon_for_bsscolor_cache_update);

/* must be called from a lock protected critical section
 * in bsscolor module to protect the shared data in this
 * function
 */
static void
ieee80211_get_unused_color_set(struct ieee80211_bsscolor_handle *bsscolor_hdl)
{
    uint8_t color;
    struct ieee80211_bsscolor *bsscolor_list    =
                    (struct ieee80211_bsscolor *) bsscolor_hdl->bsscolor_list;
    struct __unused_color_set *unused_color_set = &bsscolor_hdl->unused_color_set;
    bool free = true;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                                            "%s>>", __func__);

    unused_color_set->count = 0;
    for (color = IEEE80211_HE_BSS_COLOR_MIN;
            color <= IEEE80211_HE_BSS_COLOR_MAX; color++) {
        if (!IEEE80211_IS_COLOR_MAP_FREE(bsscolor_hdl->recvd_bsscolor_map_low,
                                     bsscolor_hdl->recvd_bsscolor_map_high)) {
            IEEE80211_IS_COLOR_FREE_IN_BITMAP(color, free);
        }

        if (free && bsscolor_list[color].color_count == 0) {
            unused_color_set->unused_color[unused_color_set->count] = color;
            unused_color_set->count++;
        } else {
            free = true;
        }
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
        "%s no_unused_color: %d <<", __func__, unused_color_set->count);
}

static bool
ieee80211_is_user_requsted_color_free(uint8_t req_color,
                                  struct __unused_color_set *unused_color_set
                                  )
{
    uint8_t count;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
                                            "%s>>", __func__);

    for (count = 0; count < unused_color_set->count; count++) {
        if (unused_color_set->unused_color[count] == req_color) {
            QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                    QDF_TRACE_LEVEL_INFO, "%s<< true", __func__);
            return true;
        }
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                  QDF_TRACE_LEVEL_INFO, "%s<< false", __func__);

    return false;
}


static void
ieee80211_bsscolor_init(struct ieee80211com *ic)
{
    uint32_t rand;
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    /* initialize bss_color to a random value in the valid range */
    do {
        OS_GET_RANDOM_BYTES(&rand, 4);
        ic->ic_he_bsscolor = (rand + OS_GET_TICKS()) %
                                (IEEE80211_HE_BSS_COLOR_MAX + 1);
    } while (ic->ic_he_bsscolor == IEEE80211_BSS_COLOR_INVALID);

    ic->ic_bsscolor_hdl.selected_bsscolor = IEEE80211_BSS_COLOR_INVALID;
    bsscolor_hdl->bsscolor_change_count = IEEE80211_DEFAULT_BSSCOLOR_CHG_COUNT;
    bsscolor_hdl->recvd_bsscolor_map_low  = 0;
    bsscolor_hdl->recvd_bsscolor_map_high = 0;
}

/** __ieee80211_select_new_bsscolor - defn. of color selection API
 *
 * This is the private (static) definition of the bss-color
 * selection API. This private API is in turn called by
 * ieee80211_select_new_bsscolor which is exposed outside
 * this file and module. This function is lock-protected
 * to protect the core-data of the selection algo from
 * parallel access
 */
static void
__ieee80211_select_new_bsscolor(struct ieee80211com *ic)
{
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;
    struct __unused_color_set *unused_color_set    = &bsscolor_hdl->unused_color_set;
    uint32_t random_bytes                      = 0;
    uint8_t req_color                          = ic->ic_he_bsscolor;
    uint8_t sel_color, sel_color_idx;
    ieee80211_bss_color_ctx_state prev_state   = bsscolor_hdl->state;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    bsscolor_hdl->state = IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTING;

    if (ic->ic_he_bsscolor_override) {
        sel_color = req_color;
    } else {
        /* Select a random bss color from the unused set */

        /* Prune the mac_ref_color list and get fresh color
         * counts
         */
        ieee80211_prune_and_adjust_color_count_in_list(bsscolor_hdl);

        /* Get the unused set first */
        ieee80211_get_unused_color_set(bsscolor_hdl);

        if (unused_color_set->count == 0) {
            /* All colors are already used by neighboring bss's.
             * Will perform BCCA if color remains unavailable even
             * after dot11BSSColorCollisionAPPeriod.
             */
            sel_color     = IEEE80211_BSS_COLOR_INVALID;
        } else {
            /* Select user requested color if it is free */
            if (req_color &&
                    ieee80211_is_user_requsted_color_free
                        (req_color, unused_color_set)) {
                sel_color     = req_color;
            } else {

                /* Select random color from the usused color set.
                 * Retry till non-zero color is found */
                do {
                OS_GET_RANDOM_BYTES(&random_bytes, 4);
                sel_color_idx = (random_bytes + OS_GET_TICKS()) %
                                                    unused_color_set->count;
                sel_color     = unused_color_set->unused_color[sel_color_idx];
                } while((unused_color_set->count > 1) && (sel_color == 0));

                if (sel_color) {
                    /* User requested color could not be set.
                     * Overwrite user-requested value with
                     * what was selected so that the current
                     * selected bsscolor is returned to the
                     * user-space
                     */
                    ic->ic_he_bsscolor = sel_color;
                } else {
                    sel_color = IEEE80211_BSS_COLOR_INVALID;
                }
            }
            /* Cancel colorListUpdateOppurtunityPeriod if already running */
            ieee80211_bsscolor_list_update_opp_timer_cancel(bsscolor_hdl);
            /* Cancel dot11BSSColorCollisionAPPeriod */
            ieee80211_bsscolor_collision_ap_timer_cancel(bsscolor_hdl);
        }
    }

    if (sel_color != IEEE80211_BSS_COLOR_INVALID) {
        bsscolor_hdl->prev_bsscolor     = bsscolor_hdl->selected_bsscolor;
        bsscolor_hdl->selected_bsscolor = sel_color;
        if (prev_state ==
                IEEE80211_BSS_COLOR_CTX_STATE_ACTIVE) {
            /* if the transition to selected-state is from
             * the initial active-state then no BCCA required
             */
            bsscolor_hdl->bcca_not_reqd = true;
        } else {
            bsscolor_hdl->bcca_not_reqd = false;
        }
        bsscolor_hdl->state = IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTED;
        bsscolor_hdl->selected_bsscolor_tstamp    = qdf_system_ticks();
    }

    /* even in the failed case reattempt from caller
     * while in selected state
     */
    bsscolor_hdl->state = IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTED;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                    "%s<< sel_color: %d state: %d", __func__,
                    sel_color, bsscolor_hdl->state);
}

/** ieee80211_select_new_bsscolor - color selection API accessible
 * to outer files and modules
 *
 * This bss color selection API is accessible to
 * outer files and modules. This funciton calls the
 * internal API __ieee80211_select_new_bsscolor
 * and also starts dot11bsscolorcollisionAPPeriod and
 * colorListUpdateOppurtunityPeriod if not already running
 */
void
ieee80211_select_new_bsscolor(struct ieee80211com *ic) {
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO_LOW,
                                            "%s>>", __func__);

    qdf_spin_lock_bh(&bsscolor_hdl->bsscolor_list_lock);

    /* if in user_disabled state then exit algo */
    if ((bsscolor_hdl->state ==
            IEEE80211_BSS_COLOR_CTX_STATE_COLOR_USER_DISABLED) ||
        (bsscolor_hdl->state ==
            IEEE80211_BSS_COLOR_CTX_STATE_COLOR_SELECTING)) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                "%s<< select failed. state: %d",
                __func__, bsscolor_hdl->state);
        qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);
        return;
    }

    __ieee80211_select_new_bsscolor(ic);

    if (bsscolor_hdl->bcca_not_reqd &&
           (bsscolor_hdl->selected_bsscolor != IEEE80211_BSS_COLOR_INVALID)) {
        ieee80211_bsscolor_post_color_selection(ic);
    } else {
        /* cancel colorListUpdateOppurtunityPeriod if already running */
        ieee80211_bsscolor_list_update_opp_timer_cancel(bsscolor_hdl);
        /* cancel dot11BSSColorCollisionAPPeriod if already running */
        ieee80211_bsscolor_collision_ap_timer_cancel(bsscolor_hdl);
        /* No BSS Color available. Start dot11BSSColorCollisionAPPeriod
         * if not already running
         */
        ieee80211_bsscolor_collision_ap_timer_start(bsscolor_hdl);
        /* Start colorListUpdateOppurtunityPeriod if not already
         * running
         */
        ieee80211_bsscolor_list_update_opp_timer_start(bsscolor_hdl);
    }


    qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO_LOW,
            "%s<< state: %d", __func__, bsscolor_hdl->state);
}
EXPORT_SYMBOL(ieee80211_select_new_bsscolor);

int ieee80211_set_ic_heop_bsscolor_disabled_bit(
                            struct ieee80211com *ic,
                            bool disable) {
    struct ieee80211vap *vap;
    struct wlan_objmgr_vdev    *vdev_obj;
    uint8_t color = 0;
    int retval = 0;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
            "%s>> disable: %s", __func__, disable ? "true" : "false");

    if (disable) {
#if SUPPORT_11AX_D3
        ic->ic_he.heop_bsscolor_info |=
                                IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK;
#else
        ic->ic_he.heop_param         |=
                                IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK;
#endif
    } else {
#if SUPPORT_11AX_D3
        ic->ic_he.heop_bsscolor_info &=
                                ~IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK;
#else
        ic->ic_he.heop_param         &=
                                ~IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK;
#endif
        color = ic->ic_bsscolor_hdl.selected_bsscolor;
    }

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if ((vap->iv_opmode == IEEE80211_M_HOSTAP)
                && vap->iv_is_up) {
            if (ic->ic_vap_set_param) {
                retval = ic->ic_vap_set_param(vap,
                          IEEE80211_CONFIG_HE_BSS_COLOR, color);

                if(!retval) {
                    wlan_vdev_beacon_update(vap);
                } else {
                    vdev_obj = vap->vdev_obj;
                    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR,
                           QDF_TRACE_LEVEL_ERROR,
                          "%s: bsscolor update to fw failed for vap "
                          "id: 0x%x", __func__,
                          vdev_obj->vdev_objmgr.vdev_id);
                }
            }
        }
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);

    return retval;
}
EXPORT_SYMBOL(ieee80211_set_ic_heop_bsscolor_disabled_bit);

int ieee80211_is_bcca_ongoing_for_any_vap(struct ieee80211com *ic) {
    struct ieee80211vap *vap;

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap->iv_he_bsscolor_change_ongoing) {
           return (OL_ATH_VAP_NET80211(vap)->av_if_id + 1);
        }
    }

    return 0;
}
EXPORT_SYMBOL(ieee80211_is_bcca_ongoing_for_any_vap);

void
ieee80211_setup_bsscolor(struct ieee80211vap *vap,
                         struct ieee80211_ath_channel *chan)
{
    struct ieee80211com *ic                        = vap->iv_ic;
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    /* Ignore BSS Color setup request incase it is
     * not HE target, not AP mode or if not already
     * BSS Color attached
     */
    if ((!ic->ic_he_target) ||
        (vap->iv_opmode != IEEE80211_M_HOSTAP) ||
        (bsscolor_hdl->state == IEEE80211_BSS_COLOR_CTX_STATE_INACTIVE)) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
            "%s  HE target: %s AP mode: %s Attached: %s",
             __func__, ic->ic_he_target ? "true" : "false",
             (vap->iv_opmode == IEEE80211_M_HOSTAP) ? "true" : "false",
             (bsscolor_hdl->state == IEEE80211_BSS_COLOR_CTX_STATE_ACTIVE) ?
             "true" : "false");
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                                "%s<<", __func__);
        return;
    }

    if (!chan) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
            "%s  failed : Chan Null",__func__);
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                                "%s<<", __func__);
        return;
    }

    if (!IEEE80211_IS_CHAN_11AX(chan)) {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
            "%s  failed : not 11AX channel",__func__);
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                                "%s<<", __func__);
        return;
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
            "%s  Registering BSS Color SE handlers",__func__);

    /* iterating through the scan-db at this point will ensure
     * that we have looked into the available scan-entries before
     * selecting a new color below. though we are registering a
     * call-back to get new beacons we will miss the beacons
     * received during acs-scan before selecting the color for
     * the first time below required during vdev_start in case
     * if we do not iterate through the scan-db here.
     */
    ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
       ieee80211_process_beacon_for_bsscolor_cache_update, (void *)ic);

    /* We need to select a new color right here
     * so that it can be used while doing vdev start
     */
    if (bsscolor_hdl->selected_bsscolor == IEEE80211_BSS_COLOR_INVALID) {
        ieee80211_select_new_bsscolor(ic);
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO,
            "%s  Selected BSS Color already active",__func__);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

int
ieee80211_bsscolor_attach(struct ieee80211com *ic)
{
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG, "%s>> %pK %s",
            __func__, ic, bsscolor_hdl->state ? "true": "false");

    if (ic->ic_he_target &&
            bsscolor_hdl->state == IEEE80211_BSS_COLOR_CTX_STATE_INACTIVE) {
        /* initialize bsscolor_hdl by memsetting to 0 */
        qdf_mem_set(bsscolor_hdl, sizeof(struct ieee80211_bsscolor_handle), 0);

        /* create lock for list */
        qdf_spinlock_create(&bsscolor_hdl->bsscolor_list_lock);

        /* create list */
        qdf_list_create(&bsscolor_hdl->macref_color_list,
                IEEE80211_MAX_MACREF_COLOR_NODE_CACH_SIZE);

        /* initialize state */
        bsscolor_hdl->state = IEEE80211_BSS_COLOR_CTX_STATE_ACTIVE;

        /* initialize bss_color to a valid random value */
        ieee80211_bsscolor_init(ic);

        /* initialize relevant timers */
        ieee80211_bsscolor_timer_init(ic);

        /* call back for the collision detection event handler to call */
        bsscolor_hdl->ieee80211_bss_color_collision_detection_hdler_cb =
                    ieee80211_bsscolor_collision_detection_hdler;

        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                    "%s  BSS Color Attach Complete",__func__);
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                    "%s  BSS Color Attach Skiped. "
                    "HE target: %s Attached: %s",
                    __func__, ic->ic_he_target ? "true" : "false",
                    bsscolor_hdl->state ? "true" : "false");
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);

    return EOK;
}

int
ieee80211_bsscolor_detach(struct ieee80211com *ic)
{
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;
    struct __macref_color_node *lnode;
    qdf_list_t *list                   = &bsscolor_hdl->macref_color_list;
    QDF_STATUS status;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                    "%s>> %pK", __func__, ic);

    if (ic->ic_he_target &&
            bsscolor_hdl->state != IEEE80211_BSS_COLOR_CTX_STATE_INACTIVE) {

        /* flush queue - till the list is empty
         * remove from the end
         */
        qdf_spin_lock_bh(&bsscolor_hdl->bsscolor_list_lock);
        while (!qdf_list_empty(list)) {
            status = qdf_list_remove_back(list, (qdf_list_node_t **) &lnode);
            if (status == QDF_STATUS_SUCCESS) {
                qdf_mem_free(lnode);
            }
        }
        qdf_spin_unlock_bh(&bsscolor_hdl->bsscolor_list_lock);

        /* destroy list */
        qdf_list_destroy(list);

        /* destroy lock */
        qdf_spinlock_destroy(&bsscolor_hdl->bsscolor_list_lock);

        bsscolor_hdl->state = IEEE80211_BSS_COLOR_CTX_STATE_INACTIVE;

        /* timer clean-up */
        qdf_timer_free(&bsscolor_hdl->dot11_bss_color_collision_ap_timer);
        qdf_timer_free(&bsscolor_hdl->color_list_update_opp_timer);
        qdf_timer_free(&bsscolor_hdl->update_recvd_bsscolor_map_timer);

        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                    "%s  BSS Color Detach Complete",__func__);
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                    "%s  BSS Color Detach Skiped. "
                    "HE target: %s Detached: %s",
                    __func__, ic->ic_he_target ? "true" : "false",
                    bsscolor_hdl->state ? "false" : "true");
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);

    return EOK;
}
