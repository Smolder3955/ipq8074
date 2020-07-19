#!/bin/sh
#
# Copyright (c) 2019 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#

[ -e /lib/functions.sh ] && . /lib/functions.sh

IFNAME=$1
CMD=$2
CONFIG=$3

parent=$(cat /sys/class/net/${IFNAME}/parent)
pairwise=

is_section_ifname() {
	local config=$1
	local ifname
	config_get ifname "$config" ifname
	[ "${ifname}" = "$2" ] && eval "$3=$config"
}

hex2string()
{
	I=0
	while [ $I -lt ${#1} ];
	do
		echo -en "\x"${1:$I:2}
		let "I += 2"
	done
}

get_pairwise() {
	if [ -f /sys/class/net/$parent/ciphercaps ]
	then
		cat /sys/class/net/$parent/ciphercaps | grep -i "gcmp"
		if [ $? -eq 0 ]
		then
			pairwise="CCMP CCMP-256 GCMP GCMP-256"
		else
			pairwise="CCMP"
		fi
	fi
}

case "$CMD" in
	DPP-CONF-RECEIVED)
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME remove_network all
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME add_network
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 key_mgmt "DPP"
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 ieee80211w 1
		get_pairwise
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 pairwise $pairwise
		;;
	DPP-CONFOBJ-AKM)
		encryption=
		sae=
		dpp=
		sae_require_mfp=
		case "$CONFIG" in
			dpp)
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 key_mgmt "DPP"
				encryption="dpp"
				ieee80211w=1
				dpp=1
				sae=0
				;;
			sae)
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 key_mgmt "DPP"
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME add_network
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 1 key_mgmt "SAE"
				encryption="sae"
				sae=1
				ieee80211w=2
				dpp=0
				;;
			psk+sae)
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 key_mgmt "DPP"
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME add_network
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 1 key_mgmt "SAE"
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME add_network
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 2 key_mgmt "WPA-PSK"
				encryption="sae+psk2"
				sae=1
				ieee80211w=1
				sae_require_mfp=1
				dpp=0
				;;
			psk)
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 0 key_mgmt "DPP"
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME add_network
				wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network 1 key_mgmt "WPA-PSK"
				encryption="psk2"
				ieee80211w=1
				dpp=0
				sae=0
				;;
		esac

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.encryption=$encryption
		uci set wireless.${sect}.sae=$sae
		uci set wireless.${sect}.sae_require_mfp=$sae_require_mfp
		uci set wireless.${sect}.dpp=$dpp
		uci set wireless.${sect}.ieee80211w=$ieee80211w
		uci commit wireless
		;;
	DPP-CONFOBJ-SSID)
		network_ids=
		i=0
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME list_network | wc -l  > /tmp/count_network_list.txt
		read network_ids < /tmp/count_network_list.txt

		while [ $i -lt $network_ids ]
		do
			wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network $i ssid $CONFIG
			i=$(expr $i + 1)
		done

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.ssid=$CONFIG
		uci commit wireless
		;;
	DPP-CONNECTOR)
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set dpp_connector $CONFIG

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.dpp_connector=$CONFIG
		uci commit wireless
		;;
	DPP-CONFOBJ-PASS)
		network_ids=
		i=1

		PASS_STR=$(hex2string "$CONFIG")
		get_pairwise
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME list_network | wc -l  > /tmp/count_network_list.txt
		read network_ids < /tmp/count_network_list.txt

		while [ $i -lt $network_ids ]
		do
			wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network $i psk $PASS_STR
			wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network $i pairwise $pairwise
			i=$(expr $i + 1)
		done

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.key=$PASS_STR
		uci commit wireless

		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME dpp_bootstrap_remove \*
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME disable
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME enable
		;;
	DPP-CONFOBJ-PSK)
		network_ids=
		i=1

		PASS_STR=$(hex2string "$CONFIG")
		get_pairwise
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME list_network | wc -l  > /tmp/count_network_list.txt
		read network_ids < /tmp/count_network_list.txt

		while [ $i -lt $network_ids ]
		do
			wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network $i psk $PASS_STR
			wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set_network $i pairwise $pairwise
			i=$(expr $i + 1)
		done

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.key=$PASS_STR
		uci commit wireless

		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME dpp_bootstrap_remove \*
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME disable
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME enable
		;;
	DPP-C-SIGN-KEY)
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set dpp_csign $CONFIG

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.dpp_csign=$CONFIG
		uci commit wireless
		;;
	DPP-NET-ACCESS-KEY)
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME set dpp_netaccesskey $CONFIG

		. /sbin/wifi detect
		sect=
		config_foreach is_section_ifname wifi-iface $IFNAME sect
		uci set wireless.${sect}.dpp_netaccesskey=$CONFIG
		uci commit wireless

		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME disable
		wpa_cli -i$IFNAME -p/var/run/wpa_supplicant-$IFNAME enable
		;;
esac
