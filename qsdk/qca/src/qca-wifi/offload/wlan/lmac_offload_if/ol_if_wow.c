/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
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
 * LMAC WOW specific offload interface functions for UMAC
 */
#include "ol_if_athvar.h"
#include "wmi_unified_api.h"
#include "_ieee80211.h"
#include "ol_if_wow.h"
#include "qdf_mem.h"
#include "init_deinit_lmac.h"

#if ATH_PERF_PWR_OFFLOAD && ATH_WOW

int
ol_wow_get_support(struct ieee80211com *ic)
{
    return 1;
}

int
ol_wow_enable(struct ieee80211com *ic, int clearbssid)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    return wmi_unified_wow_enable_send(pdev_wmi_handle);
}

int
ol_wow_wakeup(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    return wmi_unified_wow_wakeup_event_send(pdev_wmi_handle);
}

int
ol_wow_add_wakeup_event(struct ieee80211com *ic, u_int32_t type)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wow_add_wakeup_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    param.type = type;

    return wmi_unified_wow_add_wakeup_cmd_send(pdev_wmi_handle, &param);
}

void
ol_wow_set_events(struct ieee80211com *ic, u_int32_t events)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    scn->scn_wowInfo->wakeUpEvents = events;
}

int
ol_wow_get_events(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    return (scn->scn_wowInfo->wakeUpEvents);
}

int
ol_wow_add_wakeup_pattern(struct ieee80211com *ic,
    u_int8_t *pattern_bytes, u_int8_t *mask_bytes, u_int32_t pattern_len)
{
    struct ol_wow_info  *wowInfo;
    OL_WOW_PATTERN *pattern;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wow_add_wakeup_pattern_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    ASSERT(scn->scn_wowInfo);
    wowInfo = scn->scn_wowInfo;

    qdf_mem_set(&param, sizeof(param), 0);
    // check for duplicate patterns
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (pattern->valid) {
            if (!qdf_mem_cmp(pattern_bytes, pattern->patternBytes, MAX_PATTERN_SIZE) &&
                (!qdf_mem_cmp(mask_bytes, pattern->maskBytes, MAX_PATTERN_SIZE)))
            {
                printk("Pattern added already \n");
                return 0;
            }
        }
    }

    // find a empty pattern entry
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (!pattern->valid) {
            break;
        }
    }

    if (i == MAX_NUM_USER_PATTERN) {
        printk("Error : All the %d pattern are in use. Cannot add a new pattern \n", MAX_NUM_PATTERN);
        return (-1);
    }

    printk("Pattern added to entry %d \n",i);

    // add the pattern
    pattern = &wowInfo->patterns[i];
    qdf_mem_copy(pattern->maskBytes, mask_bytes, MAX_PATTERN_SIZE);
    qdf_mem_copy(pattern->patternBytes, pattern_bytes, MAX_PATTERN_SIZE);
    param.mask_bytes = mask_bytes;
    param.pattern_bytes = pattern.bytes;
    pattern->patternId  = i;
    param.pattern_id = i;
    pattern->patternLen = pattern_len;
    pattern->valid = 1;

    return wmi_unified_wow_add_wakeup_pattern_send(pdev_wmi_handle, &param);
}

int
ol_wow_remove_wakeup_pattern(struct ieee80211com *ic, u_int8_t *pattern_bytes,
    u_int8_t *mask_bytes)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    u_int32_t i;
    struct ol_wow_info  *wowInfo;
    OL_WOW_PATTERN *pattern;
    struct wow_remove_wakeup_pattern_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    ASSERT(scn->scn_wowInfo);
    wowInfo = scn->scn_wowInfo;

    printk("%s: Remove wake up pattern \n", __func__);

    qdf_mem_set(&param, sizeof(param), 0);
    // remove the pattern and return if present
    for (i = 0; i < MAX_NUM_USER_PATTERN; i++) {
        pattern = &wowInfo->patterns[i];
        if (pattern->valid) {
            if (!qdf_mem_cmp(pattern_bytes, pattern->patternBytes, MAX_PATTERN_SIZE) &&
                !qdf_mem_cmp(mask_bytes, pattern->maskBytes, MAX_PATTERN_SIZE))
            {
                pattern->valid = 0;
                qdf_mem_zero(pattern->patternBytes, MAX_PATTERN_SIZE);
                qdf_mem_zero(pattern->maskBytes, MAX_PATTERN_SIZE);
                printk("Pattern Removed from entry %d \n",i);

                buf = wmi_buf_alloc(pdev_wmi_handle, len);
                if(!buf)
                {
                    printk("buf alloc failed\n");
                    return -ENOMEM;
                }
                param.pattern_id = i;
                return wmi_unified_wow_remove_wakeup_pattern_send(pdev_wmi_handle,
                    &param);
            }
        }
    }

    // pattern not found
    printk("%s : Error : Pattern not found \n", __func__);
    return (-1);
}

int
ol_wow_get_wakeup_pattern(struct ieee80211com *ic, u_int8_t *wake_pattern_bytes,
    u_int32_t *wake_pattern_size, u_int32_t *pattern_id)
{
    return 0;
}

int
ol_wow_get_wakeup_reason(struct ieee80211com *ic)
{
    struct ol_wow_info *wowInfo;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ASSERT(scn->scn_wowInfo);
    wowInfo = scn->scn_wowInfo;
    return wowInfo->wowWakeupReason;
}

int
ol_wow_matchpattern_exact(struct ieee80211com *ic)
{
    return 0;
}

void
ol_wow_set_duration(struct ieee80211com *ic, u_int16_t duration)
{
}

void
ol_wow_set_timeout(struct ieee80211com *ic, u_int32_t timeoutinsecs)
{
}

u_int32_t
ol_wow_get_timeout(struct ieee80211com *ic)
{
    return 0;
}

static int
wow_wakeup_host_event(ol_scn_t sc, u_int8_t *datap, u_int32_t len)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ol_wow_info *wowInfo;
    wowInfo = soc->scn_wowInfo;
    wowInfo->wowWakeupReason = ((EVENT_INFO *)datap)->wake_reason;

    return 0;
}

int
ol_ath_wow_soc_attach(ol_ath_soc_softc_t *soc)
{
    wmi_unified_t wmi_handle;

    wmi_handle = lmac_get_wmi_unified_hdl(soc->psoc_obj);
    wmi_unified_register_event_handler(wmi_handle, wmi_wow_wakeup_host_event_id,
                                       wow_wakeup_host_event, WMI_RX_UMAC_CTX);

}

void
ol_ath_wow_soc_detach(ol_ath_soc_softc_t *soc)
{
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    wmi_unified_unregister_event_handler((void *)wmi_handle, wmi_wow_wakeup_host_event_id);
}
/* Intialization functions */
int
ol_ath_wow_attach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    scn->scn_wowInfo = (struct ol_wow_info *)qdf_mem_malloc(sizeof(struct ol_wow_info));
    if (scn->scn_wowInfo == NULL) {
        printk("%s: wowInfo allocation failed\n", __func__);
        return -1;
    }

    qdf_mem_zero(scn->scn_wowInfo, sizeof(struct ol_wow_info));

    /* Install WOW APIs */
    ic->ic_wow_get_support = ol_wow_get_support;
    ic->ic_wow_enable = ol_wow_enable;
    ic->ic_wow_wakeup = ol_wow_wakeup;
    ic->ic_wow_set_events = ol_wow_set_events;
    ic->ic_wow_get_events = ol_wow_get_events;
    ic->ic_wow_add_wakeup_event = ol_wow_add_wakeup_event;
    ic->ic_wow_add_wakeup_pattern = ol_wow_add_wakeup_pattern;
    ic->ic_wow_remove_wakeup_pattern = ol_wow_remove_wakeup_pattern;
    ic->ic_wow_get_wakeup_pattern = ol_wow_get_wakeup_pattern;
    ic->ic_wow_get_wakeup_reason = ol_wow_get_wakeup_reason;
    ic->ic_wow_matchpattern_exact = ol_wow_matchpattern_exact;
    ic->ic_wow_set_duration = ol_wow_set_duration;
    ic->ic_wow_set_timeout = ol_wow_set_timeout;
    ic->ic_wow_get_timeout = ol_wow_get_timeout;

    return 0;
}

int
ol_ath_wow_detach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (!scn->scn_wowInfo) {
        return 0;
    }

    qdf_mem_free(scn->scn_wowInfo);


    /* Uninstall WOW APIs */
    ic->ic_wow_get_support = NULL;
    ic->ic_wow_enable = NULL;
    ic->ic_wow_wakeup = NULL;
    ic->ic_wow_set_events = NULL;
    ic->ic_wow_get_events = NULL;
    ic->ic_wow_add_wakeup_event = NULL;
    ic->ic_wow_add_wakeup_pattern = NULL;
    ic->ic_wow_remove_wakeup_pattern = NULL;
    ic->ic_wow_get_wakeup_pattern = NULL;
    ic->ic_wow_get_wakeup_reason = NULL;
    ic->ic_wow_matchpattern_exact = NULL;
    ic->ic_wow_set_duration = NULL;
    ic->ic_wow_set_timeout = NULL;
    ic->ic_wow_get_timeout = NULL;

    return 0;
}

#endif
