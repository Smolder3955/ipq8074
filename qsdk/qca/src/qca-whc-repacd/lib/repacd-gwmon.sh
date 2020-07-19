#!/bin/sh
# Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# 2015-2016 Qualcomm Atheros, Inc.
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.

GWMON_DEBUG_OUTOUT=0
GWMON_SWITCH_CONFIG_COMMAND=swconfig

GWMON_MODE_NO_CHANGE=0
GWMON_MODE_CAP=1
GWMON_MODE_NON_CAP=2
GWMON_KERNEL="4.4.60"
GWMON_PLATFORM=QCA
backhaul_mode_single=SINGLE
backhaul_rate_zero=0

. /lib/functions.sh
. /lib/functions/hyfi-iface.sh
. /lib/functions/hyfi-network.sh
. /lib/functions/repacd-wifimon.sh

config_load 'repacd'
config_get_bool gwmon_wxt repacd 'ForceWextMode' '0'

gwmon_cfg=
config_load 'qcacfg80211'
config_get gwmon_cfg 'config' 'enable' '1'

if [ "$gwmon_wxt" == "0" -a "$gwmon_cfg" == "1" ]; then
    REPACD_CFG80211=-cfg80211
else
    REPACD_CFG80211=
fi

prev_gw_link='' router_detected=0 gw_iface="" gw_switch_port=""
managed_network='' switch_iface="" vlan_group="" switch_ports=''
cpu_portmap=0
eswitch_support="0"
gw_mac=""
last_hop_count=255

IS_GW_REACHABLE=0
GW_NOT_REACHABLE_TIMESTAMP=0

#Check for Platform & Kernel version
dut_kernel=$(uname -a  | awk '{print $3}')
dut_platform=$(grep -w DISTRIB_RELEASE /etc/openwrt_release | awk -F "='" '{print $2}' | awk '{gsub(/.{3}/,"& ")}1' | awk '{print $1}')

# Emit a log message
# input: $1 - level: the symbolic log level
# input: $2 - msg: the message to log
__gwmon_log() {
    local stderr=''
    if [ "$GWMON_DEBUG_OUTOUT" -gt 0 ]; then
        stderr='-s'
    fi

    logger $stderr -t repacd.gwmon -p "user.$1" "$*"
}

# Emit a log message at debug level
# input: $1 - msg: the message to log
__gwmon_debug() {
    __gwmon_log 'debug' "$1"
}

# Emit a log message at info level
# input: $1 - msg: the message to log
__gwmon_info() {
    __gwmon_log 'info' "$1"
}

# Emit a log message at warning level
# input: $1 - msg: the message to log
__gwmon_warn() {
    __gwmon_log 'warn' "$1"
}

# Obtain a timestamp from the system.
#
# These timestamps will be monontonically increasing and be unaffected by
# any time skew (eg. via NTP or manual date commands).
#
# output: $1 - the timestamp as an integer (with any fractional time truncated)
__gwmon_get_timestamp() {
    timestamp=$(cut -d' ' -f1 < /proc/uptime | cut -d. -f 1)
    eval "$1=$timestamp"
}

__gwmon_find_switch() {
    local vlan_grp

    #Ignore value returned by eswitch_support in repacd. It is to be used by hyd only.
    __hyfi_get_switch_iface switch_iface eswitch_support

    if [ -n "$switch_iface" ]; then
        $GWMON_SWITCH_CONFIG_COMMAND dev switch0 set flush_arl 2>/dev/null
        vlan_grp="$(echo $switch_iface | awk -F. '{print $2}' 2>/dev/null)"
    fi

    if [ -z "$vlan_grp" ]; then
        vlan_group="1"
    else
        vlan_group="$vlan_grp"
    fi
}

__gwmon_get_switch_ports() {
    local config="$1"
    local vlan_group="$2"
    local ports vlan cpu_port __cpu_portmap

    config_get vlan "$config" vlan
    config_get ports "$config" ports

    [ ! "$vlan" = "$vlan_group" ] && return

    cpu_port=$(echo "$ports" | awk '{print $1}')
    ports=$(echo "$ports" | sed "s/$cpu_port //g")
    eval "$3='$ports'"

    cpu_port=$(echo "$cpu_port" | awk -Ft '{print $1}')

    case $cpu_port in
        0) __cpu_portmap=0x01;;
        1) __cpu_portmap=0x02;;
        2) __cpu_portmap=0x04;;
        3) __cpu_portmap=0x08;;
        4) __cpu_portmap=0x10;;
        5) __cpu_portmap=0x20;;
        6) __cpu_portmap=0x40;;
        7) __cpu_portmap=0x80;;
    esac
    eval "$4='$__cpu_portmap'"
}

__gwmon_set_hop_count() {
    local config=$1
    local iface mode

    config_get mode "$config" mode
    config_get iface "$config" ifname

    if [ "$mode" = "ap" ] && [ "$gw_connected_mode" != "CAP" ]; then
        __gwmon_info "Setting intf [$iface] hop count $2"
        if [ -z $REPACD_CFG80211 ]; then
            iwpriv "$iface" set_whc_dist "$2"
        else
            cfg80211tool "$iface" set_whc_dist "$2"
        fi
    fi
}

# Determine the number of hops a given AP interface of RE is from the
# root AP.
# input: $1 - iface: the name of the AP interface (eg. ath0)
__gwmon_get_hop_count() {
    local iface=$1
    local command_result

    if [ -z "$iface" ]; then
        return 0
    fi

    if [ -z $REPACD_CFG80211 ]; then
        command_result=`iwpriv $iface get_whc_dist | awk -F':' '{print $2}'`
    else
        command_result=`cfg80211tool $iface get_whc_dist | awk -F':' '{print $2}'`
    fi

    if [ "$command_result" -eq 255 ]; then
        return 0
    fi
    return 1
}

# Check MAC learning in ssdk_sh command with Port Status
__gwmon_check_mac_portstatus() {
    local sh_ssdk
    local sh_mac

    sh_mac="$(echo "$1" | sed 's/:/-/g')"
    sh_ssdk=$(ssdk_sh fdb entry show |grep  $sh_mac | awk -F':' '{print $5}')
    for port_tmp in $sh_ssdk
    do
        if [ "$port_tmp" -gt 0 ]; then
            gw_switch_port=$port_tmp
            return 0
        fi
    done
    return 1
}

# __gwmon_check_gateway_iface_lan_iface
# input: $1 ethernet interfaces part of lan
# input: $2 Gateway interface
# returns: 0 if gateway interface matches with ether interface
# and assign gateway interface to swicth_iface
__gwmon_check_gateway_iface_lan_iface() {
    local ether_iface
    local gwiface

    # Get the parent interface if gwiface or ether_iface got created for vlan.
    gwiface=${1//.[0-9]*/}
    ether_iface=${2//.[0-9]*/}

    if [ "$ether_iface" = "$gwiface" ]; then
        return 0
    fi
    return 1
}

# Check GW reachability over Ethernet backhaul,
# Set hop count correctly to prevent isolated island condition
__gwmon_prevent_island_loop() {
    local retries=3
    local gw_ip next_hop_count=255

    while [ "$retries" -gt 0 ]; do
        gw_ip=$(route -n | grep ^0.0.0.0 | grep "br-$1" | awk '{print $2}')
        [ -z "$gw_ip" ] && break

        # Ping returns zero if at least one response was heard from the specified host
        if ping -W 2 "$gw_ip" -c1 > /dev/null; then
            __gwmon_debug "Ping to GW IP[$gw_ip] success"
            if __gwmon_get_hop_count $ap_iface_5g; then
                new_hop_count=1
                config_load wireless
                config_foreach __gwmon_set_hop_count wifi-iface $new_hop_count
            fi
            next_hop_count=1
            break
        else
            # no ping response was received, retry
            retries=$((retries -1))
            __gwmon_debug "Ping to GW IP[$gw_ip] failed ($retries retries left)"
        fi
    done

    if [ $last_hop_count -ne $next_hop_count ]; then
        __gwmon_info "Changing hop_count from $last_hop_count to $next_hop_count"
        config_load wireless
        config_foreach __gwmon_set_hop_count wifi-iface $next_hop_count
        last_hop_count=$next_hop_count
    fi
}

__gwmon_check_gwinterface() {
    local gw_iface_found=1
    local giface
    local siface

    # Check the inteface is the vlan of ether_iface
    giface=${1//.[0-9]*/}
    siface=${2//.[0-9]*/}
    if [ "$siface" = "$giface" ]; then
        eval "$3=$gw_iface_found"
        __gwmon_info "Both Switch and Gw Interface Match"
    else
        eval "$3=0"
    fi
}

# Check the link status of the interface connected to the gataway
# returns: 0 if gateway is detected; non-zero if it is not
__gwmon_check_gw_iface_link() {
    local ret
    local interf

    __gwmon_check_gwinterface $gw_iface $switch_iface interf
    if [ "$interf" -gt 0 ]; then
        local link_status

        # Before we check local link status, make sure gw_iface (eth) is up
        ret=$(ifconfig $gw_iface | grep "UP[A-Z' ']*RUNNING")
        __gwmon_info "Checking ifconfig $gw_iface is up and running, ret= $ret"
        [ -z "$ret" ] && prev_gw_link="down" && return 1

        link_status=$($GWMON_SWITCH_CONFIG_COMMAND dev switch0 port $gw_switch_port get link |awk -F':' '{print $3}'|awk -F ' ' '{print $1}')
        if [ ! "$link_status" = "up" ]; then
            link_status="down"
        fi

        if [ ! "$link_status" = "down" ]; then
            # link is up
            if [ ! "$prev_gw_link" = "up" ]; then
                __gwmon_info "Link to GW UP"
                prev_gw_link="up"
            fi
            # Check if GW is reachable, set appropriate hop count to avoid isolated island condition
            __gwmon_prevent_island_loop $managed_network
            return 0
        fi
    else
        ret=$(ifconfig $gw_iface | grep "UP[A-Z' ']*RUNNING")
        [ -n "$ret" ] && return 0
    fi

    if [ ! "$prev_gw_link" = "down" ]; then
        __gwmon_info "Link to GW DOWN"
        prev_gw_link="down"
    fi
    return 1
}

# __gwmon_check_gateway
# input: $1 1905.1 managed bridge
# output: $2 Gateway interface
# returns: 0 if gateway is detected; non-zero if not detected
__gwmon_check_gateway() {
    local gw_ip gw_br_port __gw_iface
    local ether_ifaces_full ether_ifaces
    local ether_iface ret
    local interface_gw
    current_backhaul_5g_rate=`iwconfig $sta_iface_5g | grep "Rate:" | awk '{print $2}' | cut -d: -f 2 | awk '{print $1}'`
    current_backhaul_24g_rate=`iwconfig $sta_iface_24g | grep "Rate:" | awk '{print $2}' | cut -d: -f 2 | awk '{print $1}'`

    gw_ip=$(route -n | grep ^0.0.0.0 | grep "br-$1" | awk '{print $2}')
    [ -z "$gw_ip" ] && return 1

    if ping -W 2 "$gw_ip" -c1 > /dev/null; then
        if [ "$IS_GW_REACHABLE" -eq 0 ]; then
            __gwmon_info "GW ($gw_ip) reachable"
        fi
        IS_GW_REACHABLE=1
        GW_NOT_REACHABLE_TIMESTAMP=0
    else
        if [ "$IS_GW_REACHABLE" -eq 1 ]; then
            __gwmon_info "GW ($gw_ip) NOT reachable"
            if [ "$backhaul_mode_configured" = "$backhaul_mode_single" ] && [ "$current_backhaul_5g_rate" = "$backhaul_rate_zero" ] && [ "$current_backhaul_24g_rate" = "$backhaul_rate_zero" ]; then
                __repacd_wifimon_bring_iface_up $sta_iface_24g
                __repacd_wifimon_debug "2g VAP brought up since 5g brought down & AP is operating in SINGLE backhaul mode"
            fi
        fi
        IS_GW_REACHABLE=0
        if [ "$GW_NOT_REACHABLE_TIMESTAMP" -eq 0 ]; then
            __gwmon_get_timestamp GW_NOT_REACHABLE_TIMESTAMP
        fi
    fi

    gw_mac=$(grep -w "$gw_ip" /proc/net/arp | grep -w "$1" | awk '{print $4}')
    [ -z "$gw_mac" ] && return 1

    # Instead of using hyctl (which may not be installed if not running the
    # full Hy-Fi build), use brctl instead. One additional step of mapping
    # the port number to an interface name is needed though.
    gw_br_port=$(brctl showmacs "br-$1" | grep -i "$gw_mac" | awk '{print $1}')
    [ -z "$gw_br_port" ] && return 1

    __gw_iface=$(brctl showstp "br-$1" | grep \("$gw_br_port"\) | awk '{print $1}')
    [ -z "$__gw_iface" ] && return 1

    # Get all Ethernet interfaces
    hyfi_get_ether_ifaces "$1" ether_ifaces_full
    hyfi_strip_list "$ether_ifaces_full" ether_ifaces

    # Check if this interface belongs to our network
    for ether_iface in $ether_ifaces; do
        if [ "$ether_iface" = "$__gw_iface" ]; then
            gw_iface=$__gw_iface
            __gwmon_info "Detected Gateway on interface $gw_iface"

            # Hawkeye platform has separate gmac for each switch port so gw_iface & switch_iface are same
            if [ -z "$switch_iface" ]; then
                if __gwmon_check_gateway_iface_lan_iface  "$gw_iface" "$ether_iface"; then
                    switch_iface="$gw_iface"
                fi
            fi

            __gwmon_check_gwinterface "$gw_iface" "$switch_iface" interface_gw
            if [ "$interface_gw" -gt 0 ]; then
                if ! __gwmon_check_mac_portstatus $gw_mac; then
                    __gwmon_warn "invalid port map portmap"
                    gw_switch_port=9
                    return 1
                fi
                __gwmon_info "gwmon_check_gateway Detected over ethernet =$gw_switch_port"
            fi
            __repacd_wifimon_bring_iface_down $sta_iface_5g
            __repacd_wifimon_bring_iface_down $sta_iface_24g
            return 0
        fi
    done

    # also check the loop prevention code to see if it believes we have
    # an upstream facing Ethernet interface
    local num_upstream
    num_upstream=$(lp_numupstream)
    if [ "${num_upstream}" -gt 0 ]; then
        return 0
    fi

    return 1
}

# Determine if the GW is reachable and update the router_detected global
# variable accordingly.
__gwmon_update_router_detected() {
    if __gwmon_check_gateway "$managed_network"; then
        router_detected=1
    else
        router_detected=0
    fi
}

# Check whether the configured mode matches the mode that is determined by
# checking for connectivity to the gateway.
#
# input: $1 cur_role: the current mode that is configured
# input: $2 start_mode: the mode in which the auto-configuration script is being
#                       run; This is used by the init script to help indicate
#                       that it was an explicit change into this mode.
#                       If the mode was CAP, then it should take some time
#                       before it is willing to switch back to non-CAP due
#                       to lack of a gateway.
# input: $3 managed_network: the logical name for the network interfaces to
#                            monitor
#
# return: value indicating the desired mode of operation
#  - $GWMON_MODE_CAP to act as the main AP
#  - $GWMON_MODE_NON_CAP to switch to being a secondary AP
#  - $GWMON_MODE_NO_CHANGE for now change in the mode
__gwmon_init() {
    local cur_mode=$1
    local start_mode=$2
    local eth_mon_enabled

    managed_network=$3
    __gwmon_find_switch "$managed_network"
    [ -n "$switch_iface" ] && __gwmon_info "found switch on $switch_iface VLAN=$vlan_group"

    config_load repacd
    config_get gw_connected_mode repacd 'GatewayConnectedMode' 'AP'
    config_get eth_mon_enabled repacd 'EnableEthernetMonitoring' '0'

    config_load hyd
    config_get backhaul_mode_configured 'hy' 'ForwardingMode' 'APS'
    __repacd_wifimon_debug "Backhaul mode is $backhaul_mode_configured"

    config_load network
    config_foreach __gwmon_get_switch_ports switch_vlan "$vlan_group" switch_ports cpu_portmap
    __gwmon_info "switch ports in the $managed_network network: $switch_ports"

    __gwmon_update_router_detected

    if [ "$cur_mode" = "CAP" ]; then
        if [ "$router_detected" -eq 0 ]; then
            if [ "$eth_mon_enabled" -eq 0 ] && [ ! "$start_mode" = "CAP" ]; then
                return $GWMON_MODE_NON_CAP
            else
                local retries=3

                while [ "$retries" -gt 0 ]; do
                    __gwmon_update_router_detected
                    [ "$router_detected" -gt 0 ] && break
                    retries=$((retries - 1))
                    __gwmon_debug "redetecting gateway ($retries retries left)"
                done

                # If gateway was still not detected after our attempts,
                # indicate we should change to non-CAP mode.
                if [ "$router_detected" -eq 0 ]; then
                    if [ "$eth_mon_enabled" -eq 0 ]; then
                        return $GWMON_MODE_NON_CAP
                    else
                        return $GWMON_MODE_NO_CHANGE
                    fi
                fi
            fi
        fi
    else   # non-CAP mode
        if [ "$router_detected" -eq 1 ]; then
            local mixedbh
            mixedbh=$(uci get repacd.repacd.EnableMixedBackhaul 2>/dev/null)
            if [ "$mixedbh" != "1" ]; then
                return $GWMON_MODE_CAP
            fi
        fi
    fi

    return $GWMON_MODE_NO_CHANGE
}

# return: 2 to indicate CAP mode; 1 for non-CAP mode; 0 for no change
__gwmon_check() {
    if [ "$router_detected" -eq 0 ]; then
        __gwmon_update_router_detected

        if [ "$router_detected" -gt 0 ]; then
            local mixedbh
            mixedbh=$(uci get repacd.repacd.EnableMixedBackhaul 2>/dev/null)
            # if we want to support mixed backhaul, e.g., if we want to
            # enable both WiFi and Ethernet backhaul, then we stay in
            # non-cap mode so the STA interfaces remain up.  otherwise,
            # set to cap mode which brings down the STA interfaces.
            if [ "$mixedbh" != "1" ]; then
                return $GWMON_MODE_CAP
            fi
        fi
    else
        if ! __gwmon_check_gw_iface_link "$managed_network"; then
            # Gateway is gone
            router_detected=0
            gw_iface=""
            gw_switch_port=""
            return $GWMON_MODE_NON_CAP
        fi
    fi

    return $GWMON_MODE_NO_CHANGE
}
