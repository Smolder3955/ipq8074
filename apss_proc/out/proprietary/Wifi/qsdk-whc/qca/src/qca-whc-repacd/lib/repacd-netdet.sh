#!/bin/sh
# Copyright (c) 2016 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.


# NOTE: At boot, even if we add 'list interface "wan"' to /etc/config/lldpd,
# lldpd doesn't listen on eth0.  We have to restart lldpd and then it starts
# listening.

NETDET_DEBUG_OUTOUT=0

NETDET_MODE_DB=/etc/ap.mode

NETDET_RESULT_ROOTAP=0
NETDET_RESULT_RE=1
NETDET_RESULT_INDETERMINATE=2

NETDET_CAP_BRIDGE_ROUTER_RESULT_ROUTER=0
NETDET_CAP_BRIDGE_ROUTER_RESULT_BRIDGE=1

NETDET_IS_WIFISON_DEVICE_VISIBLE_TRUE=0
NETDET_IS_WIFISON_DEVICE_VISIBLE_FALSE=1

NETDET_LLDP_WIFISON_CUSTOM_TLV_OUI=33,44,55
NETDET_LLDP_WIFISON_CUSTOM_TLV_SUBTYPE=44
NETDET_LLDP_WIFISON_CUSTOM_TLV_DATA=45,45,45,45,45

NETDET_CURRENT_MODE_ROOTAP=0
NETDET_CURRENT_MODE_RE=1

#Note: On some systems, the lan bridge might be named br0 or something
#else other than br-lan
NETDET_LANBRIDGE="br-lan"

# Emit a message at debug level.
# input: $1 - the message to log
__netdet_debug() {
    local stderr=''
    if [ "$NETDET_DEBUG_OUTOUT" -gt 0 ]; then
        stderr='-s'
    fi

    logger $stderr -t repacd.netdet -p user.debug "$1"
}

# Check to see if lldpd is listening on a particular port
__netdet_is_lldpd_listening() {
    local port=$1
    lldpcli show configuration | grep "Interface pattern: " | grep $port > /dev/null
}

# Are there any Wi-Fi SON neighbors reachable via a particular port?
__netdet_has_wifison_neighbors() {
    local port=$1
    lldpcli show neighbors ports $port details | grep "TLV:.*OUI: ${NETDET_LLDP_WIFISON_CUSTOM_TLV_OUI}, SubType: ${NETDET_LLDP_WIFISON_CUSTOM_TLV_SUBTYPE},.* ${NETDET_LLDP_WIFISON_CUSTOM_TLV_DATA}$" > /dev/null
}

# Does a particular interface have an IPv4 address?
__netdet_has_address() {
    local intf=$1
    ifconfig $intf | grep "inet addr:[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}" > /dev/null
}

# Is dhcpc running on a particular interface?
__netdet_is_running_dhcp_client() {
    local intf=$1
    ps -w | grep dhcpc | grep $intf > /dev/null
}

# Does a particular interface have a DHCP-assigned IPv4 address?
__netdet_has_dhcp_address() {
    local intf=$1
    __netdet_is_running_dhcp_client $intf && __netdet_has_address $intf
}

# Can you see WiFiSON devices on a particular port?
__netdet_is_wifison_device_visible() {
    local port=$1
    if ! __netdet_is_lldpd_listening $port; then
        __netdet_debug "warning: lldpd wasn't listening on $port so restarting"
        repacd_netdet_lldpd_init restart
    fi
    local timeout=30 # secs
    local currtime=$(date +%s)
    local timeouttime=$(($currtime + $timeout))
    while [ $currtime -le $timeouttime ]; do
        if __netdet_has_wifison_neighbors $port; then
            return $NETDET_IS_WIFISON_DEVICE_VISIBLE_TRUE
        fi
        sleep 1
        currtime=$(date +%s)
    done
    return $NETDET_IS_WIFISON_DEVICE_VISIBLE_FALSE
}

# Return a list of all the network interfaces that we care about
__netdet_get_interfaces() {
    ifconfig | grep "^\(eth\|ath\|br\)" | awk '{ print $1 }'
}

# Enable/disable DHCP server
__repacd_netdet_dhcpd() {
    case $1 in
        disable | off | stop)
            uci set dhcp.lan.ignore=1
            ;;
        enable | on | start)
            uci set dhcp.lan.ignore=0
            ;;
        *)
            echo "error: unknown parameter: $1" >&2
            return 2
            ;;
    esac
    uci commit dhcp
    /etc/init.d/dnsmasq restart
}

# Returns the determined mode (cap, re, or unknown)
netdet_get_mode_db() {
    local mode="unknown"
    if [ -f $NETDET_MODE_DB ]; then
        mode="$(cat $NETDET_MODE_DB)"
    fi
    echo $mode
}

# Saves the given mode to the database
netdet_set_mode_db() {
    local mode="$1"
    local tmpfile="$(dirname $NETDET_MODE_DB)/.$(basename $NETDET_MODE_DB)"
    echo "$mode" > $tmpfile
    mv $tmpfile $NETDET_MODE_DB
}

# Add our custom WiFi SON TLV to lldpd
netdet_set_custom_wifison_lldp_tlv() {
    # remove any existing custom TLVs
    lldpcli unconfigure lldp custom-tlv > /dev/null
    lldpcli configure lldp custom-tlv oui $NETDET_LLDP_WIFISON_CUSTOM_TLV_OUI subtype $NETDET_LLDP_WIFISON_CUSTOM_TLV_SUBTYPE oui-info $NETDET_LLDP_WIFISON_CUSTOM_TLV_DATA > /dev/null
}

# Set lldpd interfaces
netdet_set_lldpd_interfaces() {
    lldpcli configure system interface pattern $(__netdet_get_interfaces | tr '\n' ',')
}

# Set lldp tx interval
# input: $1 - the interval in seconds
netdet_set_lldpd_tx_interval() {
    lldpcli configure lldp tx-interval $1
}

# Do WAN/LAN detection
netdet_wan_lan_detect() {
    __netdet_debug "running wan/lan detection"
    local is_uplink_no_wifison="false"
    local is_uplink_with_wifison="false"
    local int="eth0"
    __netdet_is_wifison_device_visible $int
    case $? in
    $NETDET_IS_WIFISON_DEVICE_VISIBLE_TRUE)
        is_uplink_with_wifison="true"
        __netdet_debug "$int: LAN (with visible Wi-Fi SON device)"
        ;;
    $NETDET_IS_WIFISON_DEVICE_VISIBLE_FALSE)
        is_uplink_no_wifison="true"
        __netdet_debug "$int: WAN"
        ;;
    *)
        __netdet_debug "got unknown return from __netdet_is_wifison_device_visible"
        ;;
    esac
    if [ "$is_uplink_no_wifison" == "true" ]; then
        __netdet_debug "looks like a root AP"
        return $NETDET_RESULT_ROOTAP
    elif [ "$is_uplink_with_wifison" == "true" ]; then
        __netdet_debug "looks like a range extender"
        return $NETDET_RESULT_RE
    else
        __netdet_debug "indeterminate"
        return $NETDET_RESULT_INDETERMINATE
    fi
}

# determines if the local device should be configured for bridge mode or router
# mode.  the decision is based on whether the WAN interface is configured on
# a private network.
netdet_detect_cap_bridge_router_mode() {
    local dev
    local ipaddr
    local net
    local privnets

    __netdet_debug "detecting whether this cap should be a bridge or router"

    dev=$(uci get network.wan.ifname 2>/dev/null)
    [ -z "${dev}" ] && {
        dev=$NETDET_LANBRIDGE
    }

    ipaddr=$(ip addr show dev $dev | grep "inet " | awk '{print $2}')
    [ -z '${ipaddr}' ] && {
        # assume router so the firewall will be up when the interface comes up
        __netdet_debug "no IP addr configured, should configure as router"
        return $NETDET_CAP_BRIDGE_ROUTER_RESULT_ROUTER
    }
    net=$(ipcalc.sh ${ipaddr} | grep NETWORK)

    privnets=$(uci get repacd.repacd.rfc1918 2>/dev/null)
    [ -z "${privnets}" ] && {
        privnets='10.0.0.0/8 172.16.0.0/12 192.168.0.0/16'
    }

    __netdet_debug "checking the wan iface against the list of private networks"
    for privcidr in ${privnets}; do
        local privnet=$(ipcalc.sh ${privcidr} | grep NETWORK)
        [ "${net}" = "${privnet}" ] && {
            __netdet_debug "the wan iface is on a private network, should configure as bridge"
            return $NETDET_CAP_BRIDGE_ROUTER_RESULT_BRIDGE
        }
    done

    __netdet_debug "should configure as router"
    return $NETDET_CAP_BRIDGE_ROUTER_RESULT_ROUTER
}

# modifies the local device to run in bridge mode
netdet_configure_cap_bridge_mode() {
    __netdet_debug "configuring cap in bridge mode"
    uci set dhcp.lan.ignore=1
    uci commit dhcp
    /etc/init.d/dnsmasq restart

    uci set network.lan.ifname='eth0 eth1'
    uci set network.lan.proto='dhcp'
    uci delete network.wan
    uci commit network
    /etc/init.d/network restart

    uci set repacd.GatewayConnectedMode='CAP'
    uci commit repacd
    /etc/init.d/repacd restart
}

# modifies the local device to run in router mode
netdet_configure_cap_router_mode() {
    __netdet_debug "configuring cap in router mode"
    uci set network.lan.ifname='eth1'
    uci set network.lan.proto='static'
    uci set network.lan.ipaddr='192.168.1.1'

    uci set network.wan=interface
    uci set network.wan.ifname='eth0'
    uci set network.wan.proto='dhcp'

    uci commit network
    /etc/init.d/network restart

    uci delete dhcp.lan.ignore
    uci commit dhcp
    /etc/init.d/dnsmasq restart
    /etc/init.d/repacd restart
}

# setup lldpd parameters since these don't persist across restart
repacd_netdet_lldpd_setup() {
    netdet_set_lldpd_interfaces
    netdet_set_lldpd_tx_interval 10
    netdet_set_custom_wifison_lldp_tlv
}

repacd_netdet_init() {
    repacd_netdet_lldpd_setup
}

repacd_netdet_lldpd_init() {
    /etc/init.d/lldpd $1
    if [ $1 != "stop" ]; then
        while ! [ -e /var/run/lldpd.socket ]; do
            sleep 1
        done
        repacd_netdet_lldpd_setup
    fi
}

# Get the device's currently configured mode
repacd_netdet_get_current_device_mode() {
    if uci get network.wan > /dev/null; then
        return $NETDET_CURRENT_MODE_ROOTAP
    else
        return $NETDET_CURRENT_MODE_RE
    fi
}

# Configure this device as either a rootap or an re
repacd_netdet_set_current_device_mode() {
    local mode="$1"
    case $mode in
        rootap)
            netdet_detect_cap_bridge_router_mode
            local result=$?
            case $result in
                $NETDET_CAP_BRIDGE_ROUTER_RESULT_ROUTER)
                    netdet_configure_cap_router_mode
                ;;
                $NETDET_CAP_BRIDGE_ROUTER_RESULT_BRIDGE)
                    netdet_configure_cap_bridge_mode
                ;;
                *)
                    echo "error: unknown mode: $result" >&2
                    return 3
                ;;
            esac
            ;;
        re)
            __repacd_netdet_dhcpd disable
            uci set network.lan.ifname='eth0 eth1'
            uci set network.lan.proto=dhcp
            uci delete network.wan
            uci commit network
            /etc/init.d/network restart
            /etc/init.d/repacd restart
            ;;
        *)
            echo "error: unknown mode: $mode" >&2
            return 2
            ;;
    esac
}

repacd_netdet_wait_for_dhcp_addr() {
    local intf=$1
    local timeout=$2
    if [ -z "$timeout" ]; then
        timeout=15
    fi
    local endtime=$(( $(date +%s) + $timeout ))
    while ! __netdet_has_dhcp_address $intf; do
        sleep 1
        if [ $(date +%s) -gt $endtime ]; then
            return 1
        fi
    done
    return 0
}
