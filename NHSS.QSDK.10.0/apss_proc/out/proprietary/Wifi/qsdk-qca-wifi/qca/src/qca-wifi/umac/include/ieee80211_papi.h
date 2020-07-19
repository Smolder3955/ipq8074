/*
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * @@-COPYRIGHT-END-@@
 */

#ifndef _UMAC_IEEE80211_PAPI__
#define _UMAC_IEEE80211_PAPI_

// forward declarations
struct ieee80211vap;
struct ieee80211_node;
struct ieee80211_rx_status;

#if ATH_PARAMETER_API
/*
 * @brief Inform the parameter api module that a station is now
 *        associated/disassociated.
 *
 * @param [in] vap     the VAP on which station associated/disassociated
 * @param [in] ni      the station who associated/disassociated
 * @param [in] event   event of type PAPI_STA_EVENT
 */
void ieee80211_papi_send_assoc_event(struct ieee80211vap *vap,
                                     const struct ieee80211_node *ni,
                                     u_int8_t event);

/*
 * @brief Inform the parameter api module that a station has sent
 *        neighbor report request.
 *
 * @param [in] vap     the VAP on which neigbor report request received
 * @param [in] ni      the station who sent neighbor report request
 * @param [in] event   event of type PAPI_STA_EVENT
 */
void ieee80211_papi_send_nrreq_event(struct ieee80211vap *vap,
                                     const struct ieee80211_node *ni,
                                     u_int8_t event);

/*
 * @brief Inform user-space application about received
 *        probe request from non associated stations
 *
 * @param [in] vap     the VAP on which probe request received
 * @param [in] ni      the sta who sent the probe request
 * @param [in] wbuf    probe request buffer
 * @param [in] rs      rx status
 */
void ieee80211_papi_send_probe_req_event(struct ieee80211vap *vap,
                                         const struct ieee80211_node *ni,
                                         wbuf_t wbuf,
                                         struct ieee80211_rx_status *rs);

#endif /* #if ATH_PARAMETER_API */
#endif /* _UMAC_IEEE80211_PAPI__ */
