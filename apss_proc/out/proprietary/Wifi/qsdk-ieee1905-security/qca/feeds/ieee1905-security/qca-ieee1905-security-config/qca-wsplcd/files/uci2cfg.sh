#!/bin/sh
# Copyright (c) 2013 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.

. /lib/functions.sh

IGNORE_NON_WIFISEC=0
IS_CURRENT_WIFISEC=0
# This counter is used to record the correct indices of VAPs in wireless config
# file.
TOTAL_NUM_VIFS=0

cfg_append() {
    if [ "$IGNORE_NON_WIFISEC" -gt 0 ] && [ "$IS_CURRENT_WIFISEC" -eq 0 ]; then
        return
    fi
    echo "$1"
}

cfg_radio_set() {
    local radioidx=$(($1 + 1))
    local cfg="$2"
    local val="$3"
    cfg_append "RADIO.$radioidx.$cfg=$val"
}


cfg_radio_add() {
    local device="$1"
    local radioidx=$(($2 + 1))
    local cfg="$3"
    local key="$4"
    local def="$5"
    local val

    config_get val $device $key $def
    [ -n "$val" ] && cfg_append "RADIO.$radioidx.$cfg=$val"
}

cfg_vif_set() {
    local vifidx=$(($1 + 1))
    local cfg="$2"
    local val="$3"
    cfg_append "WLAN.$vifidx.$cfg=$val"
}


cfg_vif_add() {
    local vif="$1"
    local vifidx=$(($2 + 1))
    local cfg="$3"
    local key="$4"
    local def="$5"
    local val

    config_get val $vif $key $def
    [ -n "$val" ] && cfg_append "WLAN.$vifidx.$cfg=$val"
}


cfg_vifsec_set() {
    IS_CURRENT_WIFISEC=1
    cfg_vif_set "$@"
    IS_CURRENT_WIFISEC=0
}


cfg_vifsec_add() {
    IS_CURRENT_WIFISEC=1
    cfg_vif_add "$@"
    IS_CURRENT_WIFISEC=0
}


scan_wifi() {
    local cfgfile="$1"
    DEVICES=
    config_load "${cfgfile:-wireless}"

    # Create a virtual interface list field for each wifi-device
    #
    # input: $1 section: section name of each wifi-device
    create_vifs_list() {
        local section="$1"
        append DEVICES "$section"
        config_set "$section" vifs ""
    }
    config_foreach create_vifs_list wifi-device

    # Append each wifi-iface to the virtual interface list of its associated wifi-device
    #
    # input: $1 section: section name of each wifi-iface
    append_vif() {
        local section="$1"
        config_get device "$section" device
        config_get vifs "$device" vifs
        append vifs "$section"
        config_set "$device" vifs "$vifs"
        # For wifi-iface (VAP), record its index and section name in variable
        # vifname_#. This will be used later when generating wsplcd config file
        # to match the VAP with correct index.
        eval "vifname_${TOTAL_NUM_VIFS}=$section"
        TOTAL_NUM_VIFS=$(($TOTAL_NUM_VIFS + 1))
    }
    config_foreach append_vif wifi-iface
}

cfg_get_wifi() {
    local vifidx=0
    local managed_network=$1
    local network=""

    scan_wifi
    for device in $DEVICES; do
        local radioidx=${device#wifi}
        local vidx_per_radio=0
        local channel
        local wsplcd_unmanaged

        # All of the radio parameters are not used in IEEE1905.1 cloning
        config_get channel "$device" channel '0'
	config_get macaddr "$device" macaddr
        [ "$channel" = "auto" ] && channel=0
        cfg_radio_set $radioidx Channel $channel
        config_get disabled "$device" disabled '0'
        [ "$disabled" = "1" ] && radioena=0 || radioena=1
        cfg_radio_set $radioidx  RadioEnabled $radioena
	cfg_radio_set $radioidx  macaddr $macaddr
        # UCI based targets have different definition of "txpower" from mib based targets
        cfg_radio_add  $device  $radioidx X_ATH-COM_Powerlevel    txpower
        cfg_radio_add  $device  $radioidx X_ATH-COM_Rxchainmask   rxchainmask
        cfg_radio_add  $device  $radioidx X_ATH-COM_Txchainmask   txchainmask
        # X_ATH-COM_TBRLimit is not implemented in QSDK
        # cfg_radio_add  $device  $radioidx X_ATH-COM_TBRLimit      tbrlimit
        cfg_radio_add  $device  $radioidx X_ATH-COM_AMPDUEnabled  AMPDU
        cfg_radio_add  $device  $radioidx X_ATH-COM_AMPDULimit    AMPDULim
        # X_ATH-COM_AMPDUFrames is not implemented in QSDK
        # cfg_radio_add  $device  $radioidx X_ATH-COM_AMPDUFrames   AMPDUFrames

        config_get hwmode "$device" hwmode auto
        config_get htmode "$device" htmode auto

        case "$hwmode:$htmode" in
        # The parsing stops at the first match so we need to make sure
        # these are in the right orders (most generic at the end)
            *ng:HT20) hwmode=ng20;;
            *ng:HT40-) hwmode=ng40minus;;
            *ng:HT40+) hwmode=ng40plus;;
            *ng:HT40) hwmode=ng40;;
            *ng:*) hwmode=ng20;;
            *na:HT20) hwmode=na20;;
            *na:HT40-) hwmode=na40minus;;
            *na:HT40+) hwmode=na40plus;;
            *na:HT40) hwmode=na40;;
            *na:*) hwmode=na40;;
            *ac:HT20) hwmode=acvht20;;
            *ac:HT40+) hwmode=acvht40plus;;
            *ac:HT40-) hwmode=acvht40minus;;
            *ac:HT40) hwmode=acvht40;;
            *ac:HT80) hwmode=acvht80;;
            *ac:HT160) hwmode=acvht160;;
            *ac:HT80_80) hwmode=acvht80_80;;
            *ac:*) hwmode=acvht80;;
            *b:*) hwmode=b;;
            *bg:*) hwmode=g;;
            *g:*) hwmode=g;;
            *a:*) hwmode=a;;
            *) hwmode=auto;;
        esac

        config_get_bool device_unmanaged "$device" wsplcd_unmanaged '0'

        config_get vifs "$device" vifs

        # determine vif name
        for vif in $vifs; do
            local vifname

            config_get network "$vif" network
            [ "$network" = "$managed_network" ] || continue

            config_get_bool disabled "$vif" disabled 0
            [ "$disabled" = 0 ] ||
            {
                config_set "$vif" ifname ""
                continue
            }

            [ $vidx_per_radio -gt 0 ] && vifname="ath${radioidx}${vidx_per_radio}" || vifname="ath${radioidx}"

            config_get ifname "$vif" ifname
            config_set "$vif" ifname "${ifname:-$vifname}"
            vidx_per_radio=$(($vidx_per_radio + 1))
        done

        for vif in $vifs; do
            local bssid enc
            local beacontype wepencrmode wepauthmode wpaencrmode wpaauthmode wpa2encrmode wpa2authmode
            local vapidx=0
            local vifidx

            config_get network "$vif" network
            [ "$network" = "$managed_network" ] || continue

            # First need to find the correct VAP index for the current vif. This
            # can be done by looking for the matching interface name from
            # vifname_${vapidx} variables,
            while [ $vapidx -lt $TOTAL_NUM_VIFS ]
            do
                local name=$(eval "echo \$vifname_${vapidx}")
                if [ "$name" == "$vif" ];
                then
                    vifidx=$vapidx
                    break
                fi
                vapidx=$(($vapidx + 1))
            done

            [ -n "$vifidx" ] || continue

            config_get_bool disabled "$vif" disabled 0
            [ "$disabled" = "1" ] && vifena=0 || vifena=1
            cfg_vif_set $vifidx Enable $vifena

            cfg_vif_set $vifidx X_ATH-COM_RadioIndex $(($radioidx +1))
            config_get ifname "$vif" ifname

            bssid=`ifconfig $ifname 2>&1 | awk '/HWaddr/ {print \$5}'`
            cfg_vif_set $vifidx BSSID $bssid

            cfg_vifsec_add $vif $vifidx SSID ssid
            [ "$hwmode" == "auto" ] && {
                local is5G=`iwconfig $ifname 2>&1 | grep Frequency:5`
		[ -n "$is5G" ] && hwmode="na40minus" || hwmode="ng20"
            }
            cfg_vifsec_set $vifidx Standard $hwmode
            cfg_vif_set $vifidx Channel $channel

            config_get enc "$vif" encryption "none"
            case "$enc" in
                none)
                    beacontype="None"
                ;;
                *wep*)
                    beacontype="Basic"
                    wepencrmode="WEPEncryption"
                    case "$enc" in
                        *shared*)
                            wepauthmode="SharedAuthentication"
                        ;;
                        *mixed*)
                            wepauthmode="Both"
                        ;;
                        *)
                            wepauthmode="None"
                        ;;
                    esac
                ;;
                *mixed*)
                    beacontype="WPAand11i"
                    wpa2authmode="PSKAuthentication"
                    wpa2encrmode="TKIPandAESEncryption"
                    case "$enc" in
                        *psk*)
                            wpa2authmode="PSKAuthentication"
                        ;;
                        *wpa*)
                            wpa2authmode="EAPAuthentication"
                        ;;
                    esac
                ;;
                *psk2*)
                    beacontype="11i"
                    wpa2authmode="PSKAuthentication"
                    wpa2encrmode="AESEncryption"
                ;;
                *wpa2*)
                    beacontype="11i"
                    wpa2authmode="EAPAuthentication"
                    wpa2encrmode="AESEncryption"
                ;;
                *psk*)
                    beacontype="WPA"
                    wpaauthmode="PSKAuthentication"
                    wpaencrmode="TKIPEncryption"
                ;;
                *wpa*)
                    beacontype="WPA"
                    wpaauthmode="EAPAuthentication"
                    wpaencrmode="TKIPEncryption"
                ;;
                8021x)
                    beacontype="Basic"
                    wepencrmode="WEPEncryption"
                    wepauthmode="EAPAuthentication"
                ;;
                ccmp)
                    beacontype="11i"
                    wpa2authmode="WPA3Authentication"
                    wpa2encrmode="AESEncryption"
                ;;
            esac


            # explicit override for crypto setting
            case "$enc" in
                *tkip+aes|*tkip+ccmp|*aes+tkip|*ccmp+tkip) crypto="TKIPandAESEncryption";;
                *aes|*ccmp) crypto="AESEncryption";;
                *tkip) crypto="TKIPEncryption";;
            esac

            [ -n "$crypto" ] &&
            case "$beacontype" in
                WPA)
                    wpaencrmode=$crypto
                ;;
                11i)
                    wpa2encrmode=$crypto
                ;;
            esac

            cfg_vifsec_set $vifidx BeaconType $beacontype
            cfg_vifsec_set $vifidx BasicEncryptionModes $wepencrmode
            cfg_vifsec_set $vifidx BasicAuthenticationMode $wepauthmode
            cfg_vifsec_set $vifidx WPAEncryptionModes ${wpaencrmode}
            cfg_vifsec_set $vifidx WPAAuthenticationMode $wpaauthmode
            cfg_vifsec_set $vifidx IEEE11iEncryptionModes ${wpa2encrmode}
            cfg_vifsec_set $vifidx IEEE11iAuthenticationMode $wpa2authmode

            config_get key "$vif" key
            case "$enc" in
                *wep*)#WEP key
                    key="${key:-1}"
                    case "$key" in
                        [1234])
                            for idx in 1 2 3 4; do
                                local zidx
                                zidx=$(($idx - 1))
                                config_get ckey "$vif" "key${idx}"
                                [ -n "$ckey" ] && \
                                    cfg_vifsec_set $vifidx WEPKey.${idx}.WEPKey $ckey
                            done
                            cfg_vifsec_set $vifidx WEPKeyIndex $key
                        ;;
                        *)
                            cfg_vifsec_set $vifidx WEPKey.1.WEPKey $key
                            cfg_vifsec_set $vifidx WEPKeyIndex 1
                            [ -n "$wep_rekey" ] && append "$var" "wep_rekey_period=$wep_rekey" "$N"
                        ;;
                    esac
                ;;
                *)#WPA psk
                    if [ ${#key} -eq 64 ]; then
                        cfg_vifsec_set $vifidx PreSharedKey.1.PreSharedKey "$key"
                    else
                        cfg_vifsec_set $vifidx KeyPassphrase "$key"
                    fi
                ;;
            esac

            cfg_vif_add $vif $vifidx BasicDataTransmitRates mcast_rate

            config_get wds "$vif" wds
            case "$wds" in
                1|on|enabled) wds=1;;
                *) wds=0;;
            esac

            config_get mode "$vif" mode
            case "$mode" in
                ap)
                    [ "$wds" = 1 ] && opmode=RootAP || opmode=InfrastructureAccessPoint
                ;;
                sta)
                    [ "$wds" = 1 ] && opmode=WDSStation || opmode=WirelessStation
                ;;
                adhoc)
                    # don't support adhoc
                ;;
                wds)
                    opmode=WDSStation
                ;;
            esac
	    cfg_vif_set $vifidx DeviceOperationMode $opmode

        # WPA3: Add SAE Configurations
            #SAE Enable
            config_get sae "$vif" sae
            [ -n "$sae" ] &&
            case "$sae" in
                1|on|enabled)
                        sae=1;;
                *)
                        sae=0;;
            esac
            [ -n "$sae" ] && cfg_vifsec_set $vifidx EnableSAE $sae

            #SAE_PASSWORD
            config_get sae_password "$vif" sae_password
            [ -n "$sae_password" ] && cfg_vifsec_set $vifidx SAEPassword "$sae_password"

            #SAE ANTI CLOGGING THRESHOLD
            config_get sae_anti_clogging_threshold "$vif" sae_anti_clogging_threshold
            [ -n "$sae_anti_clogging_threshold" ] && cfg_vifsec_set $vifidx SAEAntiCloggingThreshold $sae_anti_clogging_threshold

            #SAE SYNC
            config_get sae_sync "$vif" sae_sync
            [ -n "$sae_sync" ] && cfg_vifsec_set $vifidx SAESync $sae_sync

            #SAE GROUPS
            config_get sae_groups "$vif" sae_groups
            [ -n "$sae_groups" ] && cfg_vifsec_set $vifidx SAEGroups "$sae_groups"

            #SAE REQUIRE MFP
            config_get sae_require_mfp "$vif" sae_require_mfp
            [ -n "$sae_require_mfp" ] && cfg_vifsec_set $vifidx SAERequireMFP $sae_require_mfp

        #OWE: Add OWE Configurations
            #OWE Enable
            config_get owe "$vif" owe
            [ -n "$owe" ] &&
            case "$owe" in
                1|on|enabled)
                        owe=1;;
                *)
                        owe=0;;
            esac
            [ -n "$owe" ] && cfg_vifsec_set $vifidx EnableOWE $owe

            config_get owe_groups "$vif" owe_groups
            [ -n "$owe_groups" ] && cfg_vifsec_set $vifidx OWEGroups "$owe_groups"

            config_get owe_transition_ifname  "$vif" owe_transition_ifname
            [ -n "$owe_transition_ifname" ] && cfg_vifsec_set $vifidx OWETransIfname "$owe_transition_ifname"

            config_get owe_transition_ssid  "$vif" owe_transition_ssid
            [ -n "$owe_transition_ssid" ] && cfg_vifsec_set $vifidx OWETransSSID "$owe_transition_ssid"

            config_get owe_transition_bssid  "$vif" owe_transition_bssid
            [ -n "$owe_transition_bssid" ] && cfg_vifsec_set $vifidx OWETransBSSID "$owe_transition_bssid"

        #SUITE_B: Add SUITE_B Configurations
            config_get suite_b  "$vif" suite_b
            [ -n "$suite_b" ] && cfg_vifsec_set $vifidx SuiteB "$suite_b"


        # IEEE802.11i Management Frame Protection (MFP)
            # MFP Enable
            config_get ieee80211w "$vif" ieee80211w
            [ -n "$ieee80211w" ] && cfg_vifsec_set $vifidx IEEE80211w $ieee80211w

        # X_ATH-COM Authentication fields

            config_get auth_server "$vif" auth_server
            [ -n "$auth_server" ] && cfg_vifsec_set $vifidx X_ATH-COM_AuthServerAddr $auth_server

            config_get auth_port "$vif" auth_port
            [ -n "$auth_port" ] && cfg_vifsec_set $vifidx X_ATH-COM_AuthServerPort $auth_port

            config_get auth_secret "$vif" auth_secret
            [ -n "$auth_secret" ] && cfg_vifsec_set $vifidx X_ATH-COM_AuthServerSecret $auth_secret

            config_get nasid "$vif" nasid
            [ -n "$nasid" ] && cfg_vifsec_set $vifidx X_ATH-COM_NASID $nasid

            # RTS and Rragmentation have different definitions from mib based targets
            cfg_vif_add $vif $vifidx RTS rts
            cfg_vif_add $vif $vifidx Fragmentation frag
            cfg_vif_add $vif $vifidx X_ATH-COM_SSIDHide hidden
            cfg_vif_set $vifidx X_ATH-COM_APModuleEnable $vifena
            cfg_vif_add $vif $vifidx X_ATH-COM_WPSPin wps_pin "12345670"
            cfg_vif_set $vifidx X_ATH-COM_VapIfname "$ifname"
            if [ "$enc" = "none"  ] ; then
                cfg_vif_set $vifidx X_ATH-COM_WPSConfigured NOTCONFIGURED
            else
                cfg_vif_set $vifidx X_ATH-COM_WPSConfigured CONFIGURED
            fi;


            cfg_vif_add $vif $vifidx X_ATH-COM_ShortGI shortgi
            cfg_vif_add $vif $vifidx X_ATH-COM_CWMEnable 1
            cfg_vif_add $vif $vifidx X_ATH-COM_WMM wmm

            # Note that disablecoext and HT40Coexist are logical negations of
            # one another, hence the inverted logic here. libstorage takes
            # care of mapping back to the UCI setting.
            config_get_bool disablecoext "$vif" disablecoext
            if [ "$disablecoext" = 1 ] ; then
                cfg_vif_set $vifidx X_ATH-COM_HT40Coexist 0
            else
                cfg_vif_set $vifidx X_ATH-COM_HT40Coexist 1
            fi

            if [ "$device_unmanaged" -gt 0 ] ; then
                wsplcd_unmanaged=1
            else
                config_get_bool wsplcd_unmanaged "$vif" wsplcd_unmanaged 0
            fi
            cfg_vif_set $vifidx WsplcdUnmanaged $wsplcd_unmanaged

            # HBR is not implemented in QSDK
            # cfg_vif_set $vifidx X_ATH-COM_HBREnable 0
            # cfg_vif_set $vifidx X_ATH-COM_HBRPERLow 20
            # cfg_vif_set $vifidx X_ATH-COM_HBRPERHigh 35

            # Multicast Enhancement is not implemented in QSDK
            # cfg_vif_set $vifidx X_ATH-COM_MEMode Translate
            # cfg_vif_set $vifidx X_ATH-COM_MELength 32
            # cfg_vif_set $vifidx X_ATH-COM_METimer 30000
            # cfg_vif_set $vifidx X_ATH-COM_METimeout 120000
            # cfg_vif_set $vifidx X_ATH-COM_MEDropMcast 1

        done

    done

}

case "$1" in
    "wifisec")
        IGNORE_NON_WIFISEC=1
        network_name=$2
        cfg_get_wifi $network_name
    ;;
    "wifi")
        network_name=$2
        cfg_get_wifi $network_name
    ;;
    *)
        network_name=$1
        cfg_get_wifi $network_name
    ;;
esac
