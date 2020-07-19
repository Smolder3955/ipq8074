/*
 * WPA Supplicant - command line interface for wpa_supplicant daemon
 * Copyright (c) 2004-2010, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

/* 
 * Qualcomm Atheros, Inc. chooses to take this file subject 
 * only to the terms of the BSD license. 
 */

#ifndef ACFG_WSUPP_IF_H
#define ACFG_WSUPP_IF_H

#if 0
Dynamic interface add and operation without configuration files
---------------------------------------------------------------

wpa_supplicant can be started without any configuration files or
network interfaces. When used in this way, a global (i.e., per
        wpa_supplicant process) control interface is used to add and remove
network interfaces. Each network interface can then be configured
through a per-network interface control interface. For example,
        following commands show how to start wpa_supplicant without any
        network interfaces and then add a network interface and configure a
        network (SSID):

#first I have to load modules and create the first interface
            /etc/rc.d/rc.wlan up
            wlanconfig wlan create wlandev wifi0 wlanmode sta

# Start wpa_supplicant in the background
            wpa_supplicant -g/var/run/wpa_supplicant-global -B

# Add a new interface (wlan0, no configuration file, driver=wext, and
# enable control interface)
            wpa_cli -g/var/run/wpa_supplicant-global interface_add wlan0 "" athr /var/run/wpa_supplicant

# Configure a network using the newly added network interface:
            wpa_cli -iwlan0 add_network
            wpa_cli -iwlan0 set_network 0 ssid '"sony_P2P_11ng"'
            wpa_cli -iwlan0 set_network 0 key_mgmt WPA-PSK
            wpa_cli -iwlan0 set_network 0 psk '"sonysair"'
            wpa_cli -iwlan0 enable_network 0

# At this point, the new network interface should start trying to associate
# with the WPA-PSK network using SSID test.

# Remove network interface
            wpa_cli -g/var/run/wpa_supplicant-global interface_remove wlan0

#the interactive equivalent wpa_cli commands:
            add_network 
            set_network 0 ssid  "sony_P2P_11ng"
            set_network 0 psk "sonysair"
            set_network 0 key_mgmt WPA-PSK
            enable_network 0

            --if I do it from the cli after the commands I get the following events:
            <2>WPS-AP-AVAILABLE 
            <2>Trying to associate with 00:22:6b:7c:bd:dc (SSID='sony_P2P_11ng' freq=2432 MHz)
            <2>Associated with 00:22:6b:7c:bd:dc
            <2>WPA: Key negotiation completed with 00:22:6b:7c:bd:dc [PTK=CCMP GTK=CCMP]
            <2>CTRL-EVENT-CONNECTED - Connection to 00:22:6b:7c:bd:dc completed (auth) [id=0 id_str=]

            --then to use the interface I have to do:
            udhcpc -i wlan0 -s /etc/udhcpc.script
#endif

/**
 * The supplicant needs to be given one or more WLAN 
 * interfaces to configure. (e.g. ath0/ath1 etc). This file
 * defines APIs pertaining to such interfaces.
 */

/**
 * @brief Add a WLAN n/w interface for configuration
 * @param[in] mctx module context
 * @param[in] if_uctx user context for interface
 * @param[in] ifname WLAN interface name (e.g. ath0)
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_if_add(acfg_wsupp_hdl_t *mctx, 
        char *ifname);

/**
 * 
 * @param[in] mctx module context
 * @param[in] ifname WLAN interface name
 * 
 * @return status of operation
 */
acfg_status_t acfg_wsupp_if_remove(acfg_wsupp_hdl_t *mctx, 
        char *ifname);
#endif
