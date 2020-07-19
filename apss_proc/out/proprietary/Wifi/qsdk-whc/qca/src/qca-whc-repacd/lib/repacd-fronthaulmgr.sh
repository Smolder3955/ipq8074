#!/bin/sh
# Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.

FRONTHAULMGRMON_DEBUG_OUTOUT=0

# This file assumes the gwmon and wifimon (the appropriate sub-version)
# have already been sourced.

. /lib/functions/whc-network.sh

is_front_haul_VAPs_brought_down=0

# Config Parameters
manage_fronthaul_and_backhaul_independently=0
fronthaul_VAPs_bringdown_time=0

# Local parameters
ap_ifaces_24g='' ap_ifaces_5g=''

# Emit a message at debug level.
# input: $1 - the message to log
__repacd_fronthaulmgrmon_debug() {
    local stderr=''
    if [ "$FRONTHAULMGRMON_DEBUG_OUTOUT" -gt 0 ]; then
        stderr='-s'
    fi

    logger $stderr -t repacd.fronthaulmgrmon -p user.debug "$1"
}

# Emit a message at info level.
__repacd_fronthaulmgrmon_info() {
    local stderr=''
    if [ "$BACKHAULMGRMON_DEBUG_OUTOUT" -gt 0 ]; then
        stderr='-s'
    fi

    logger $stderr -t repacd.fronthaulmgrmon -p user.info "$1"
}

__repacd_fronthaulmgrmon_get_fronthaul_vaps() {
    local config="$1"
    local network_to_match="$2"
    local network iface disabled mode device hwmode

    config_get network "$config" network
    config_get iface "$config" ifname
    config_get disabled "$config" disabled '0'
    config_get mode "$config" mode
    config_get bssid "$config" bssid
    config_get device "$config" device
    config_get hwmode "$device" hwmode

    if [ "$hwmode" != "11ad" ]; then
        if [ "$network" = "$network_to_match" ] && [ -n "$iface" ] && \
			[ "$mode" = "ap" ] && [ "$disabled" -eq 0 ]; then

            if whc_is_5g_vap "$config"; then
                __repacd_fronthaulmgrmon_debug "5 GHz AP VAP ($iface) found"
                if [ -z "${ap_ifaces_5g}" ]; then
                    ap_ifaces_5g="$iface"
                else
                    ap_ifaces_5g="$ap_ifaces_5g $iface"
                fi
            else
                __repacd_fronthaulmgrmon_debug "2.4 GHz AP VAP ($iface) found"
                if [ -z "${ap_ifaces_24g}" ]; then
                    ap_ifaces_24g="$iface"
                else
                    ap_ifaces_24g="$ap_ifaces_24g $iface"
                fi
            fi
        fi
    fi
}

# Check whether all of the provided interfaces are up
#
# input: $1 - list of AP VAPs to check
#
# return 0 if all VAPs are up, otherwise non-zero
__repacd_fronthaulmgrmon_all_AP_VAPs_up() {
    local ap_ifaces="$1"
    if [ -n "${ap_ifaces}" ]; then
        # Check whether all VAPs are up
        for item in $ap_ifaces
        do
            if ip link show ${item} | grep -q 'state DOWN'; then
                __repacd_fronthaulmgrmon_debug "Fronthaul $item is down; will re-attempt"
                return 1
            fi
        done
    fi

    return 0
}

__repacd_fronthaulmgrmon_bringup_AP_VAPs() {
    if [ -z "${ap_ifaces_5g}" ]; then
        __repacd_fronthaulmgrmon_debug "No 5 GHz AP VAPs found"
    else
        # Bring Up all 5G AP VAPs
        for item in $ap_ifaces_5g
        do
            ifconfig "$item" up
        done
    fi

    if [ -z "${ap_ifaces_24g}" ]; then
        __repacd_fronthaulmgrmon_debug "No 2.4 GHz AP VAPs found"
    else
        # Bring Up all 2.4G AP VAPs
        for item in $ap_ifaces_24g
        do
            ifconfig "$item" up
        done
    fi
}

__repacd_fronthaulmgrmon_bringdown_AP_VAPs() {
    if [ -z "${ap_ifaces_5g}" ]; then
        __repacd_fronthaulmgrmon_debug "No 5 GHz AP VAPs found"
    else
        # Bring Down all 5G AP VAPs
        for item in $ap_ifaces_5g
        do
            ifconfig "$item" down
        done
    fi

    if [ -z "${ap_ifaces_24g}" ]; then
        __repacd_fronthaulmgrmon_debug "No 2.4 GHz AP VAPs found"
    else
        # Bring Down all 2.4G AP VAPs
        for item in $ap_ifaces_24g
        do
            ifconfig "$item" down
        done
    fi
}

# Initialize the fronthaul manager, taking action to bring down the fronthaul
# VAPs if the feature is enabled and the gateway is not reachable.
repacd_fronthaulmgrmon_init() {
    local force_down_on_start

    # First resolve the config parameters.
    config_load repacd
    config_get manage_fronthaul_and_backhaul_independently 'FrontHaulMgr' \
        'ManageFrontAndBackHaulsIndependently' '0'
    config_get fronthaul_VAPs_bringdown_time 'FrontHaulMgr' \
        'FrontHaulMgrTimeout' '3600'
    config_get force_down_on_start 'FrontHaulMgr' 'ForceDownOnStart' 0

    if [ "$manage_fronthaul_and_backhaul_independently" -gt 0 ]; then
        config_load wireless
        config_foreach __repacd_fronthaulmgrmon_get_fronthaul_vaps wifi-iface "lan"

        __repacd_fronthaulmgrmon_debug "Resolved 2.4 GHz: $ap_ifaces_24g"
        __repacd_fronthaulmgrmon_debug "Resolved 5 GHz: $ap_ifaces_5g"

        if [ "$IS_GW_REACHABLE" -gt 0 ]; then
            __repacd_fronthaulmgrmon_debug "Bringing up Front-haul VAPs"
            __repacd_fronthaulmgrmon_bringup_AP_VAPs
            is_front_haul_VAPs_brought_down=0
        elif [ "$force_down_on_start" -gt 0 ]; then
            __repacd_fronthaulmgrmon_debug "Bringing down fronthaul VAPs on start"
            __repacd_fronthaulmgrmon_bringdown_AP_VAPs
            is_front_haul_VAPs_brought_down=1
        fi
    fi
}

# Perform the periodic check to determine whether the fronthual interface
# states need to be updated or not.
repacd_fronthaulmgrmon_check() {
    if [ "$manage_fronthaul_and_backhaul_independently" -gt 0 ]; then
        if [ "$IS_GW_REACHABLE" -eq 0 ]; then
            if [ "$is_front_haul_VAPs_brought_down" -eq 0 ]; then
                if  __repacd_wifimon_is_timeout "$GW_NOT_REACHABLE_TIMESTAMP" \
                    $fronthaul_VAPs_bringdown_time; then
                    __repacd_fronthaulmgrmon_debug "Expired $fronthaul_VAPs_bringdown_time sec, Bringing down Front-haul VAPs"
                    __repacd_fronthaulmgrmon_bringdown_AP_VAPs
                    is_front_haul_VAPs_brought_down=1
                fi
            fi
        else
            if [ "$is_front_haul_VAPs_brought_down" -gt 0 ]; then
                __repacd_fronthaulmgrmon_debug "Bringing up Front-haul VAPs"
                __repacd_fronthaulmgrmon_bringup_AP_VAPs

                if __repacd_fronthaulmgrmon_all_AP_VAPs_up "${ap_ifaces_5g}" &&
                    __repacd_fronthaulmgrmon_all_AP_VAPs_up "${ap_ifaces_24g}"; then
                    __repacd_fronthaulmgrmon_debug "All Front-haul VAPs are up"
                    is_front_haul_VAPs_brought_down=0
                fi
            fi
        fi
    fi
}
