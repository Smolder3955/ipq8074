#!/bin/sh
# Copyright (c) 2016 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.

. /lib/functions.sh
. /lib/ipq806x.sh

board=$(ipq806x_board_name)

configure_bond() {
	local iface1
	local iface2
	local bond_ifname
	local lan_ifaces
	local bond_mode
	local conf_mac

	config_load fst && {
		config_get_bool disabled config 'disabled' '1'
		config_get iface1 config interface1
		config_get iface2 config interface2
		config_get bond_ifname config mux_interface
		config_get conf_mac config interface2_mac
	}

	bond_mode=$(uci show network.bond.mode 2>/dev/null | grep -oE "[0-7]")

	config_load network && {
		if [ $disabled -eq 0 ]; then {
			echo "Enabling FST bonding configuration"
			lan_ifaces="$(uci show network.lan.ifname | grep -oE "'.*'" | sed "s:$bond_ifname::" | sed "s/'//g") $bond_ifname"

			uci set network.lan.ifname="$lan_ifaces"
			uci set network.bond='interface'
			uci set network.bond.ifname=''$bond_ifname''
			uci set network.bond.type='bonding'
			uci set network.bond.slaves=''$iface1' '$iface2''
			# L2DA mode is mode 7
			uci set network.bond.mode='7'
			if [ -z $conf_mac ]; then
				uci set network.bond.l2da_multimac='1'
			else
				uci delete network.bond.l2da_multimac 2>/dev/null
			fi
			uci set wireless.@wifi-iface[0].network='bonding'
			uci commit network
			uci commit wireless
		} elif [ "$bond_mode" == "7" ] ; then {
			echo "Disabling FST bonding configuration"
			lan_ifaces="$(uci show network.lan.ifname | grep -oE "'.*'" | sed "s:$bond_ifname::" | sed "s/'//g")"

			uci set network.lan.ifname="$lan_ifaces"
			uci delete network.bond.ifname
			uci delete network.bond.mode 2>/dev/null
			uci delete network.bond.l2da_multimac 2>/dev/null
			uci delete network.bond.slaves 2>/dev/null
			uci set wireless.@wifi-iface[0].network='lan'
			uci commit network
			uci commit wireless
		}
		fi
	}
}

fst_start() {
	local config_state
	local iface1

	fst_stop

	config_load fst && {
		config_get iface1 config interface1
		config_get_bool disabled config 'disabled' '1'
		if [ "$disabled" -eq 0 ]; then {
			fst_verify_config
			config_state=$?

			if [ "$config_state" -ne 0 ]; then
				fst_disable
				return 1
			fi

			# using the non-11ad interface as default slave
			/usr/sbin/fstman /var/run/hostapd/global -s $iface1 &
			echo "FST manager started"
		}
		else
			return 1
		fi
	}
}

fst_stop() {
	local fstman_id

	fstman_id=$(ps | grep [f]stman | awk '{print $1}')

	# This "if" statement checks if "$fstman_id" is integer or not
	if [ "$fstman_id" -eq "$fstman_id" ] 2>/dev/null; then
		echo "Stopping FST manager"
		killall fstman
	fi
}

fst_set_mac_addr() {
	local iface2
	local conf_mac
	local iface2_curr_mac
	local iface2_wifi_index
	local iface2_perm_mac

	config_load fst && {
		config_get_bool disabled config 'disabled' '1'
		config_get iface2 config interface2
		config_get conf_mac config interface2_mac
	}

	iface2_wifi_index=$(fst_get_wifi_index $iface2)
	if [ -z $iface2_wifi_index ]; then
		return
	fi

	if [ $disabled -eq 0 ] && [ $conf_mac ] ; then
		iface2_curr_mac=$(ifconfig $iface2 | grep HW | awk '{print $5}')

		if [ $iface2_curr_mac != $conf_mac ]; then
			echo "fst_set_mac_addr: set $iface2 to mac address $conf_mac"
			$(ifconfig $iface2 down)
			$(ifconfig $iface2 hw ether $conf_mac)
			uci set wireless.@wifi-iface[$iface2_wifi_index].macaddr=''$conf_mac''
		fi
	else
		#TODO: make this cleanup conditional based on whether previous fst config took place
		iface2_perm_mac=$(ethtool -P $iface2 | awk '{print $3}' | awk '{print tolower($0)}')
		iface2_curr_mac=$(ifconfig $iface2 | grep HWaddr | awk '{print $5}' | awk '{print tolower($0)}')

		if [ "$iface2_perm_mac" != "$iface2_curr_mac" ] ; then
			$(ifconfig $iface2 down)
			$(ifconfig $iface2 hw ether $(ethtool -P $iface2 | awk '{print $3}'))
			uci delete wireless.@wifi-iface[$iface2_wifi_index].macaddr 2>/dev/null

		fi
	fi

	uci commit wireless
}

# this function gets interface name and returns the index in the uci wifi array of this interface.
# for example: index=$(fst_get_wifi_index wlan0) that can be used as wireless.@wifi-iface[$index]...
fst_get_wifi_index() {
	local iface_name=$1
	local iface_parent
	local iface_wifi_index

	iface_parent=$(fst_get_parent_entry $iface_name)
	if [ $iface_parent ]; then
		iface_wifi_index=$(uci show wireless | grep "wifi-iface" | grep "$iface_parent" | grep -oE "wifi-iface\[[0-9]\]" | grep -oE "[0-9]")
	fi

	echo $iface_wifi_index
}

# this function gets vap interface name and returns the parent entry that corresponds to this interface
# for example: parent_entry=$(fst_get_parent_entry wlan0) might return "radio0" that can be used as wireless.$parent_entry...
fst_get_parent_entry() {
	local iface_name=$1
	local iface_parent
	local iface_path

	# search parent by "parent" in sysfs
	iface_parent=$(cat /sys/class/net/$iface_name/parent 2> /dev/null)
	if [ $iface_parent ]; then
		echo $iface_parent
		return
	fi

	# search parent by pci path
	iface_path=$(find /sys/devices -name $iface_name | sed 's/\/sys\/devices\///' | sed "s:\/net\/$iface_name::" | grep "pci")
	if [ $iface_path ]; then
		iface_parent=$(uci show wireless | grep "$iface_path" | grep -oE "radio[0-9]")
		echo $iface_parent
		return
	fi
}

# this function return 0 in case of success and 1 in case of failure
fst_verify_config() {
	local iface1_name
	local iface2_name
	local iface1_parent
	local iface2_parent
	local iface1_wifi_index
	local iface2_wifi_index
	local iface1_hwmode
	local iface2_hwmode
	local iface1_mode
	local iface2_mode
	local iface1_disabled
	local iface2_disabled
	local iface1_encryption
	local iface2_encryption
	local iface1_key
	local iface2_key

	config_load fst && {
		config_get iface1_name config interface1
		config_get iface2_name config interface2

		# get the physical interface and the index at @wifi-iface
		iface1_parent=$(fst_get_parent_entry $iface1_name)
		if [ -z $iface1_parent ] ; then
			echo "ERROR, can't run FST: couldn't get parent of $iface1_name"
			return 1
		fi

		iface1_wifi_index=$(fst_get_wifi_index $iface1_name)

		# get the radio entry and the index at @wifi-iface
		iface2_parent=$(fst_get_parent_entry $iface2_name)
		if [ -z $iface2_parent ] ; then
			echo "ERROR, can't run FST: couldn't get parent of $iface2_name"
			return 1
		fi

		iface2_wifi_index=$(fst_get_wifi_index $iface2_name)

		# verify both interfaces enabled
		iface1_disabled=$(uci get wireless.$iface1_parent.disabled 2> /dev/null)
		iface2_disabled=$(uci get wireless.$iface2_parent.disabled 2> /dev/null)

		if [ $iface1_disabled -eq 1 ] || [ $iface2_disabled -eq 1 ] ; then
			echo "ERROR in FST configuration: at least one of the interfaces disabled."
			return 1
		fi

		# verify first interface is not 11ad and second interface is 11ad
		iface1_hwmode=$(uci get wireless.$iface1_parent.hwmode 2> /dev/null)
		iface2_hwmode=$(uci get wireless.$iface2_parent.hwmode 2> /dev/null)

		if [ "$iface1_hwmode" == "11ad" ] || [ "$iface2_hwmode" != "11ad" ] ; then
			echo "ERROR in FST configuration: interface1 configured as 11ad or interface2 not configured as 11ad."
			return 1
		fi

		# verify both interfaces in AP mode
		iface1_mode=$(uci get wireless.@wifi-iface[$iface1_wifi_index].mode 2> /dev/null)
		iface2_mode=$(uci get wireless.@wifi-iface[$iface2_wifi_index].mode 2> /dev/null)

		if [ "$iface1_mode" != "ap" ] || [ "$iface2_mode" != "ap" ] ; then
			echo "ERROR in FST configuration: at least one of the interfaces not in AP mode."
			return 1
		fi

		# verify matching encryption
		iface1_encryption=$(uci get wireless.@wifi-iface[$iface1_wifi_index].encryption 2> /dev/null)
		iface2_encryption=$(uci get wireless.@wifi-iface[$iface2_wifi_index].encryption 2> /dev/null)

		if [ "$iface1_encryption" != "none" ] && [ "$iface2_encryption" != "none" ] ; then
			# verify same password
			iface1_key=$(uci get wireless.@wifi-iface[$iface1_wifi_index].key 2> /dev/null)
			iface2_key=$(uci get wireless.@wifi-iface[$iface2_wifi_index].key 2> /dev/null)

			if [ "$iface1_key" != "$iface2_key" ] ; then
				echo "ERROR in FST configuration: the interfaces have different keys."
				return 1
			fi
		elif [ "$iface1_encryption" != "none" ] || [ "$iface2_encryption" != "none" ] ; then
			echo "ERROR in FST configuration: encryption method not matching between the interfaces."
			return 1
		fi

		return 0
	}
}

fst_disable() {
	echo "Disabling FST."
	uci set fst.config.disabled='1'
	uci commit fst
	/etc/init.d/network restart
}

case "$1" in
	start) fst_start;;
	stop) fst_stop;;
	configure) configure_bond;;
	set_mac_addr) fst_set_mac_addr;;
esac
