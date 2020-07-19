#ifndef ACFG_WSUPP_NW_H
#define ACFG_WSUPP_NW_H

/**
 * Once a WLAN interface is added, a network profile is created
 * with SSID, key information etc. This file lists APIs related
 * to the network profile.
 */

/**
 * 
 * @brief Add a new network to an interface - it must later be configured
 * and enabled before use.
 * @param[in] mctx module context
 * @param[in] nw_uctx user context for network
 * @param[in] ifname interface name for the driver VAP
 * @param[out] network_id for the new network
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_nw_create(acfg_wsupp_hdl_t *mctx, 
        char *ifname,
        int *network_id);

/**
 * @brief disconnects, and removes a network
 * @param[in] mctx module context
 * @param[in] ifname interface name for the driver VAP
 * @param[out] network_id for the new network
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_nw_delete(acfg_wsupp_hdl_t *mctx, 
        char *ifname,
        int network_id);

/**
 * @brief sets an item number to the passed string.
 * once a network is enabled, some attributes may no longer be set.
 * @param[in] mctx module context
 * @param[in] network_id for the new network
 * @param[in] item number to be added
 * @param[in] in_string describing ssid, psk etc.
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_nw_set(acfg_wsupp_hdl_t *mctx,
        int network_id,
        enum acfg_network_item item,
        char *in_string);

#endif
