#!/bin/sh
# Copyright (c) 2015-2018 Qualcomm Technologies, Inc.
#
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
# 2015-2016 Qualcomm Atheros, Inc.
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.

WIFIMON_DEBUG_OUTOUT=0

# Set this to a filename to log all commands executed.
# The output of relevant commands will be appended to the file.
WIFIMON_DEBUG_COMMAND_FILE=

WIFIMON_STATE_NOT_ASSOCIATED='NotAssociated'
WIFIMON_STATE_AUTOCONFIG_IN_PROGRESS='AutoConfigInProgress'
WIFIMON_STATE_MEASURING='Measuring'
WIFIMON_STATE_WPS_TIMEOUT='WPSTimeout'
WIFIMON_STATE_ASSOC_TIMEOUT='AssocTimeout'
WIFIMON_STATE_RE_BACKHAUL_GOOD='RE_BackhaulGood'
WIFIMON_STATE_RE_BACKHAUL_FAIR='RE_BackhaulFair'
WIFIMON_STATE_RE_BACKHAUL_POOR='RE_BackhaulPoor'
WIFIMON_STATE_RE_SWITCH_BSTA='RE_SwitchingBSTA'

WIFIMON_PIPE_NAME='/var/run/repacd.pipe'

. /lib/functions.sh
. /lib/functions/whc-network.sh

# State information
sta_iface_24g='' sta_iface_24g_config_name=''
sta_iface_5g='' sta_iface_5g_config_name='' unknown_ifaces=0
assoc_timeout_logged=0 wps_timeout_logged=0
wps_in_progress=0 wps_start_time=''
wps_stabilization=0 wps_assoc_count=0
assoc_start_time='' last_assoc_state=0
ping_running=0 last_ping_gw_ip=
rssi_num=0 rssi_filename=
bsta_max_preference=

# Config parameters
rssi_samples=''
backhaul_rssi_2='' backhaul_rssi_5='' backhaul_rssi_offset=''
min_wps_assoc=
assoc_timeout='' wps_timeout=''
measuring_cnt=0 measuring_attempts=0
force_bsses_down_on_all_bsta_switches=0
cnt_5g_attempts=0 max_5g_attempts=0

# Emit a message at debug level.
# input: $1 - the message to log
__repacd_wifimon_debug() {
    local stderr=''
    if [ "$WIFIMON_DEBUG_OUTOUT" -gt 0 ]; then
        stderr='-s'
    fi

    logger $stderr -t repacd.wifimon -p user.debug "$1"
}

# Log the output of a command to a file (when enabled).
# This is a nop unless WIFIMON_DEBUG_COMMAND_FILE is set.
# input: $1 - command output to log
__repacd_wifimon_dump_cmd() {
    if [ -n "$WIFIMON_DEBUG_COMMAND_FILE" ]; then
        touch $WIFIMON_DEBUG_COMMAND_FILE
        { date; echo "$1"; echo; } >> $WIFIMON_DEBUG_COMMAND_FILE
    fi
}

# Emit a message at info level.
__repacd_wifimon_info() {
    local stderr=''
    if [ "$WIFIMON_DEBUG_OUTOUT" -gt 0 ]; then
        stderr='-s'
    fi

    logger $stderr -t repacd.wifimon -p user.info "$1"
}

# Obtain a timestamp from the system.
#
# These timestamps will be monontonically increasing and be unaffected by
# any time skew (eg. via NTP or manual date commands).
#
# output: $1 - the timestamp as an integer (with any fractional time truncated)
__repacd_wifimon_get_timestamp() {
    timestamp=$(cut -d' ' -f1 < /proc/uptime | cut -d. -f 1)
    eval "$1=$timestamp"
}

# Terminate any background ping that may be running.
# If no background pings are running, this will be a nop.
__repacd_stop_ping() {
    if [ "$ping_running" -gt 0 ]; then
        kill "$(jobs -p)"
        ping_running=0
        __repacd_wifimon_debug "Stopped ping to GW IP $last_ping_gw_ip"
    fi

    if [ -n "$rssi_filename" ]; then
        # Clean up the temporary file
        rm -f $rssi_filename
        rssi_filename=
    fi
}

# Start a background ping to the gateway address (if it can be resolved).
# This helps ensure the RSSI values are updated (as firmware will not report
# updates if only beacons are being received on the STA interface).
# input: $1 - network: the name of the network being managed
# return: 0 if the ping was started or is already running; otherwise 1
__repacd_start_ping() {
    gw_ip=$(route -n | grep ^0.0.0.0 | grep "br-$1" | awk '{print $2}')
    if [ -n "$gw_ip" ]; then
        if [ ! "$gw_ip" = "$last_ping_gw_ip" ]; then
            # First need to kill the existing one due to the IP change.
            __repacd_stop_ping
            # This will leave ping_running set to 0.
        fi

        if [ "$ping_running" -eq 0 ]; then
            __repacd_wifimon_debug "Pinging GW IP $gw_ip"

            # Unfortunately the busybox ping command does not support an
            # interval. Thus, we can only ping once per second so there will
            # only be a handful of measurements over the course of our RSSI
            # sampling.
            ping "$gw_ip" > /dev/null &
            ping_running=1
            last_ping_gw_ip=$gw_ip
        fi

        # Ping is running now or was started.
        return 0
    fi

    __repacd_wifimon_info "Failed to resolve GW when starting ping; will re-attempt"
    return 1
}

# Determine if the gateway is reachable.
#
# Ideally this would be limited to only the 5 GHz STA interface, but there
# is no good way to do this (since packets would need to be received on the
# bridge interface).
#
# return: 0 if the gateway is reachable; otherwise 1
__repacd_is_gw_reachable() {
    if [ -n "$last_ping_gw_ip" ]; then
        if ping -c 1 -W 1 "${last_ping_gw_ip}" > /dev/null; then
            return 0
        fi
    fi

    # Gateway is unknown or is not reachable
    return 1
}

# Determine if the STA interface named is current associated and active.
#
# input: $1 - sta_iface: the name of the interface (eg. ath01)
# return: 0 if associated; 1 if not associated or empty interface name
__repacd_wifimon_is_active_assoc() {
    local sta_iface=$1
    if [ -n "$sta_iface" ]; then
        local assoc_str=
        assoc_str=$(iwconfig "$sta_iface")
        __repacd_wifimon_dump_cmd "State of $sta_iface: $assoc_str"

        if echo "$assoc_str" | grep 'Access Point: ' | grep -v 'Not-Associated' > /dev/null; then
            return 0
        else
            return 1
        fi
    else
        # An unknown STA interface is considered not associated.
        return 1
    fi
}

# Determine if the STA interface named is current associated.
#
# Note that for the purposes of this function, an empty interface name is
# considered associated. This is done because in some configurations, only
# one interface is enabled.
#
# input: $1 - sta_iface: the name of the interface (eg. ath01)
# return: 0 if associated or if the interface name is empty; otherwise 1
__repacd_wifimon_is_assoc() {
    local sta_iface=$1
    if [ -n "$sta_iface" ];
    then
        if __repacd_wifimon_is_active_assoc "$sta_iface"; then
            return 0
        else
            return 1
        fi
    else
        # An unknown STA interface is considered associated.
        return 0
    fi
}

# Determine if the STA association is stable enough to be able to start
# the next step of the process.
# return: 0 if the association is stable; non-zero if it is not yet deemed
#         stable
__repacd_wifimon_is_assoc_stable() {
    if [ "$wps_stabilization" -gt 0 ]; then
        if [ "$wps_assoc_count" -ge "$min_wps_assoc" ]; then
            return 0
        else
            return 1
        fi
    else
        # No stabilization in progress
        return 0
    fi
}

# Determine if the STA is associated and update the state accordingly.
# input: $1 - network: the name of the network being managed
# input: $2 - cur_re_mode: the currently configured range extender mode
# input: $3 - cur_re_submode: the currently configured range extender sub-mode
# input: $4 - whether this is a check during init for a restart triggered
#             by mode switching
# output: $5 - state: the variable to update with the new state name (if there
#                     was a change)
# output: $6 - re_mode: the desired range extender mode
# output: $7 - re_submode: the desired range extender sub-mode
# return: 0 if associated; otherwise 1
__repacd_wifimon_check_associated() {
    local network=$1
    local associated=0

    if __repacd_wifimon_is_assoc $sta_iface_24g \
        && __repacd_wifimon_is_assoc $sta_iface_5g; then
        associated=1
    fi

    if [ "$associated" -gt 0 ]; then
        # Only update the LED state if we transitioned from not associated
        # to associated.
        if [ "$last_assoc_state" -eq 0 ]; then
            if [ "$wps_in_progress" -gt 0 ]; then
                # If WPS was triggered, it could take some time for the
                # interfaces to settle into their final state. Thus, update
                # the start time for the measurement to the point at which
                # the WPS button was pressed.
                assoc_start_time=$wps_start_time

                # Clear this as we only want to extend the association time
                # for this one instance. All subsequent ones should be based
                # on the time we detect a disassociation (unless WPS is
                # triggered again).
                wps_start_time=''

                # Clear this flag so that we now use the association timer
                # instead of the WPS one.
                wps_in_progress=0

                wps_stabilization=1
            fi

            if [ "$wps_stabilization" -gt 0 ]; then
                wps_assoc_count=$((wps_assoc_count + 1))
                __repacd_wifimon_debug "Assoc post WPS (#$wps_assoc_count)"
            fi

            if __repacd_wifimon_is_assoc_stable; then
                # Assume we are going into the measuring state. This may be
                # overruled if we decide to switch to a different bSTA
                # interface.
                eval "$5=$WIFIMON_STATE_MEASURING"
                assoc_start_time=''
                last_assoc_state=1
                wps_stabilization=0

                # Restart the measurements. We do not remember any past
                # ones as they might not reflect the current state (eg.
                # if the root AP was moved).
                rssi_num=0

                if [ "$wps_stabilization" -gt 0 ]; then
                    # Once WPS completes, switch to the most preferred bSTA.
                    local sta_iface_config=''
                    if [ -n "$sta_iface_24g" ]; then
                        sta_iface_config="$sta_iface_24g_config_name"
                    else
                        sta_iface_config="$sta_iface_5g_config_name"
                    fi

                    # Be optimistic that the most preferred bSTA will have a
                    # good link, so don't force the BSSes down.
                    local force_bsses_down=0
                    __repacd_wifimon_select_next_bsta "$sta_iface_config" \
                        $force_bsses_down \
                        "$WIFIMON_STATE_MEASURING" "$5"

                    # Although we're stable, since in most cases we're going to
                    # change the bSTA radio, we don't want to proceed into the
                    # measuring state below. If it turns out we didn't switch
                    # to a new radio, we'll pick up the measurements in the
                    # next iteration.
                    return 0
                fi
            else
                # Pretend like we are not associated since we need it to be
                # stable.
                return 1
            fi
        fi

        # Association is considered stable. Measure the link RSSI.
        if [ "$rssi_num" -le "$rssi_samples" ] && \
            __repacd_start_ping "$network"; then
            __repacd_wifimon_measure_link "$network" "$5"
        fi
        return 0
    elif [ "$wps_in_progress" -eq 0 ] && [ "$wps_stabilization" -eq 0 ]; then
        # Record the first time we detected ourselves as not being associated.
        # This will drive a timer in the check function that will change the
        # state if we stay disassociated for too long.
        if [ -z "$assoc_start_time" ]; then
            __repacd_wifimon_get_timestamp assoc_start_time
        fi

        last_assoc_state=0
        eval "$5=$WIFIMON_STATE_NOT_ASSOCIATED"
    elif [ "$wps_in_progress" -gt 0 ]; then
        if [ "$wps_timeout_logged" -eq 0 ] && [ "$assoc_timeout_logged" -eq 0 ]; then
            # Suppress logs after we've timed out
            __repacd_wifimon_debug "Auto config in progress - not assoc"
        fi

        wps_assoc_count=0
    fi

    # Not associated and WPS is in progress. No LED update.
    return 1
}

# Find the median from the samples file.
# This is a crude way to compute the median when the number of
# samples is odd. It is not strictly correct for an even number
# of samples since it does not compute the average of the two
# samples in the middle and rather just takes the lower one, but
# this should be sufficient for our purposes. The average is not
# performed due to the values being on the logarithmic scale and
# because shell scripts do not directly support floating point
# arithmetic.
# input: $1 - Samples filename
# input: $2 - Number of samples in the samples file
# output: $3 - computed Median
__repacd_wifimon_compute_median() {
    local median median_index
    local samples_filename="$1"

    median_index=$((($2 + 1) / 2))
    median=$(sort -n < "$samples_filename" | head -n $median_index | tail -n 1)

    eval "$3=$median"
}

# Measure the RSSI to the serving AP and update the state accordingly.
# input: $1 - network: the name of the network being monitored
# output: $2 - state: the variable to update with the new state name (if there
#                     was a change)
__repacd_wifimon_measure_link() {
    local rssi

    if ! __repacd_is_gw_reachable; then
        if [ -n "$last_ping_gw_ip" ]; then
            __repacd_wifimon_debug "GW ${last_ping_gw_ip} not reachable"
        else
            __repacd_wifimon_debug "GW unknown"
        fi
        return
    fi

    if [ "$rssi_num" -eq 0 ]; then
        if [ "$measuring_cnt" -gt 0 ]; then
            __repacd_wifimon_debug "Measurement failed attempt # $measuring_cnt"
        fi
        measuring_cnt=$((measuring_cnt + 1))
    fi

    if [ "$measuring_cnt" -gt "$measuring_attempts" ]; then
        # Could not complete measurements with this interface as a bSTA.
        #
        # Try switching to the next most preferred interface.
        # Since this interface was so flaky, the next one might be too, so
        # tell the fronthaul manager to force the BSSes down at the next
        # startup.
        local force_bsses_down=1
        __repacd_wifimon_select_next_bsta "$sta_iface_config_name" \
            $force_bsses_down "$WIFIMON_STATE_RE_BACKHAUL_POOR" "$2"
        return
    fi

    # Currently only one STA interface is allocated at a time. Determine the
    # active one so that it can be used to check the RSSI.
    local sta_iface=''
    local rssi_threshold=''
    local rssi_led_state=''  # state of LEDs if above RSSI threshold
    if [ -n "$sta_iface_5g" ]; then
        sta_iface="$sta_iface_5g"
        sta_iface_config_name="$sta_iface_5g_config_name"
        rssi_threshold="$backhaul_rssi_5"
        rssi_led_state="$WIFIMON_STATE_RE_BACKHAUL_GOOD"
    else
        sta_iface="$sta_iface_24g"
        sta_iface_config_name="$sta_iface_24g_config_name"
        rssi_threshold="$backhaul_rssi_2"
        rssi_led_state="$WIFIMON_STATE_RE_BACKHAUL_FAIR"
    fi

    rssi=$(iwconfig $sta_iface | grep 'Signal level' | awk -F'=' '{print $3}' | awk '{print $1}')

    # We explicitly ignore clearly bogus values. -95 dBm has been seen in
    # some instances where the STA is not associated by the time the RSSI
    # check is done. The check against 0 tries to guard against scenarios
    # where the firmware has yet to report an RSSI value (although this may
    # never happen if the RSSI gets primed through the association messaging).
    if [ "$rssi" -gt -95 ] && [ "$rssi" -lt 0 ]; then
        if [ "$rssi_num" -lt "$rssi_samples" ]; then
            __repacd_wifimon_debug "RSSI sample #$rssi_num = $rssi dBm"

            # Ignore the very first sample since it is taken at the same time
            # the ping is started (and thus the RSSI might not have been
            # updated).
            if [ "$rssi_num" -eq 0 ]; then
                rssi_filename=$(mktemp /tmp/repacd-rssi.XXXXXX)
            else
                # Not the first sample
                echo "$rssi" >> "$rssi_filename"
            fi
            rssi_num=$((rssi_num + 1))
        elif [ "$rssi_num" -eq "$rssi_samples" ]; then
            __repacd_wifimon_debug "RSSI sample #$rssi_num = $rssi dBm"

            # We will take one more sample and then draw the conclusion.
            # No further measurements will be taken (although this may be
            # changed in the future).
            echo "$rssi" >> "$rssi_filename"

            # We got the required number of samples, now derive the median rssi.
            local rssi_median
            __repacd_wifimon_compute_median "$rssi_filename" $rssi_num rssi_median
            __repacd_wifimon_debug "Median RSSI = $rssi_median dBm"
            __repacd_wifimon_debug "RSSI threshold = $rssi_threshold dBm"

            measuring_cnt=0
            rssi_num=$((rssi_num + 1))  # to prevent future samples

            # We have our measurement, so the ping is no longer needed.
            __repacd_stop_ping

            # In case we disassociate after this, we will want to start the
            # association timer again, so clear our state of the last time we
            # started it so that it can be started afresh upon disassociation.
            assoc_start_time=''

            if [ "$rssi_median" -gt "$rssi_threshold" ]; then
                local keep_24g_bsta_threshold=$((rssi_threshold + backhaul_rssi_offset))
                if [ "$sta_iface" = "$sta_iface_24g" ]; then
                    if [ "$cnt_5g_attempts" -ge "$max_5g_attempts" ] ||
                        [ "$rssi_median" -lt "$keep_24g_bsta_threshold" ]; then
                        eval "$2=$rssi_led_state"
                    else
                        # 2.4 GHz is good enough that 5 GHz might be viable. Try it again.
                        __repacd_wifimon_debug "2.4 GHz good; considering 5 GHz"

                        # Let's be optimistic that 5 GHz will be good enough
                        # and thus we should leave the BSSes up at startup.
                        local force_bsses_down=0
                        __repacd_wifimon_select_next_bsta "$sta_iface_config_name" \
                            $force_bsses_down "$rssi_led_state" "$2"
                        return
                    fi
                else
                    # 5 GHz bSTA with sufficient; we are done
                    eval "$2=$rssi_led_state"
                fi
            elif [ "$sta_iface" = "$sta_iface_24g" ]; then
                __repacd_wifimon_debug "2.4 GHz bSTA weak; not re-attempting 5 GHz"
                eval "$2=$WIFIMON_STATE_RE_BACKHAUL_POOR"
            else
                # Try next best bSTA radio
                __repacd_wifimon_debug "5 GHz bSTA too weak; trying next radio"

                # Until we know we can get a good bSTA link, let's be
                # conservative and bring the BSSes down at startup.
                local force_bsses_down=1
                __repacd_wifimon_select_next_bsta "$sta_iface_config_name" \
                    $force_bsses_down "$WIFIMON_STATE_RE_BACKHAUL_POOR" "$2"
                return
            fi
        fi
    fi
}

# Determine the radio that is next most preferred radio.
#
# If the current radio's preference value is not set, pick the highest
# preference radio.
#
# input: $1 config: section name
# input: $2 cur_pref: the preference level for the current bSTA
# output: $3 preferred_radio: the radio with the highest preference, but below
#                             the cur_pref value if it is set
__repacd_wifimon_resolve_next_bsta_radio() {
    local config="$1"
    local cur_pref="$2"

    local bsta_preference=''
    config_get bsta_preference "$config" repacd_map_bsta_preference 0

    # Skip radios that have no preference set. Maybe the OEM never wants
    # to use that radio.
    if [ -n "$bsta_preference" ]; then
        if [ -n "$cur_pref" ]; then
            if [ "$bsta_preference" -lt "$cur_pref" ] &&
                [ "$bsta_preference" -gt "$bsta_max_preference" ]; then
                eval "$3=$config"
                bsta_max_preference="$bsta_preference"
            fi
        else
            if [ "$bsta_preference" -gt "$bsta_max_preference" ]; then
                eval "$3=$config"
                bsta_max_preference="$bsta_preference"
            fi
        fi
    fi
}

# Mark the desired radio as selected for the bSTA.
#
# If a radio is marked as selected (using the repacd_map_bsta_selected config
# option), it will be used. If instead none is marked, the radio with the
# highest repacd_map_bsta_pref value will be used.
#
# input: $1 config: section name
# input: $2 selected_radio: the radio that should be selected
__repacd_wifimon_select_bsta_radio() {
    local config="$1"
    local selected_radio="$2"

    if [ "$config" = "$selected_radio" ]; then
        uci_set wireless "$config" repacd_map_bsta_selected '1'
    else
        uci_set wireless "$config" repacd_map_bsta_selected '0'
    fi
}

# Update the radio on which to instantiate the bSTA.
#
# If the currently selected bSTA is 2.4 GHz, select the highest priority radio.
# Otherwise, select the highest priority radio that is less than the current
# priority.
#
# input: $1 - config: the name of the config section for the bSTA
# input: $2 - force_bsses_down: whether to force the BSSes down on the next
#                               startup
# input: $3 - state_no_bsta_radios: the state to transition into if there are
#                                   no other bSTA radios to switch to
# output: $4 - state: the variable to update with the new state name (if there
#                     was a change)
__repacd_wifimon_select_next_bsta() {
    local config="$1"
    local force_bsses_down="$2"
    local state_no_bsta_radios="$3"

    # Resolve the current preference value
    local device
    config_load wireless
    config_get device "$config" device

    local cur_pref
    config_get cur_pref "$device" repacd_map_bsta_preference

    if [ -n "$sta_iface_24g" ]; then
        # We want to take the maximum one
        cur_pref=''
    fi

    local preferred_radio=''
    bsta_max_preference=0
    config_foreach __repacd_wifimon_resolve_next_bsta_radio wifi-device \
        "$cur_pref" preferred_radio

    if [ "$device" != "$preferred_radio" ]; then
        __repacd_wifimon_debug "Selected $preferred_radio as next bSTA"

        # If switching from 5 GHz to 2.4 GHz, increment the count to record we
        # have tried all 5 GHz bSTAs. The assumption here is that 2.4 GHz will
        # always come after 5 GHz in the preference order.
        if [ -n "$sta_iface_5g" ] && ! whc_is_5g_radio "$preferred_radio"; then
            cnt_5g_attempts=$((cnt_5g_attempts + 1))
            uci_set repacd MAPWiFiLink '5gAttemptsCount' "$cnt_5g_attempts"
            uci_commit repacd

            __repacd_wifimon_debug "Completed 5 GHz bSTA attempt #$cnt_5g_attempts"
        fi

        config_foreach __repacd_wifimon_select_bsta_radio wifi-device \
            "$preferred_radio"
        uci_commit wireless

        if [ "$force_bsses_down_on_all_bsta_switches" -gt 0 ]; then
            force_bsses_down=1
        fi

        # Overwriting the ForceDownOnStart to 0 for all cases for now
        # to prevent deadlock on any BSTA switch
        __repacd_wifimon_debug "Updating ForceDownOnStart to 0"
        uci_set repacd FrontHaulMgr ForceDownOnStart '0'
        uci_commit repacd

        eval "$4=$WIFIMON_STATE_RE_SWITCH_BSTA"
    else
        __repacd_wifimon_debug "No other bSTA radios available"
        eval "$4=$state_no_bsta_radios"
    fi
}

# Determine if a provided amount of time has elapsed.
# input: $1 - start_time: the timestamp (in seconds)
# input: $2 - duration: the amount of time to check against (in seconds)
# return: 0 on timeout; non-zero if no timeout
__repacd_wifimon_is_timeout() {
    local start_time=$1
    local duration=$2

    # Check if the amount of elapsed time exceeds the timeout duration.
    local cur_time
    __repacd_wifimon_get_timestamp cur_time
    local elapsed_time=$((cur_time - start_time))
    if [ "$elapsed_time" -gt "$duration" ]; then
        return 0
    fi

    return 1
}

# Check whether the given interface is the STA interface on the desired
# network and the desired band.
#
# input: $1 - config: the name of the interface config section
# input: $2 - network: the name of the network to which the STA interface
#                      must belong to be matched
# output: $3 - iface: the resolved STA interface name on 2.4 GHz (if found)
# output: $4 - iface_config_name: the resolved name of the config section
#                                 for the STA interface on 2.4 GHz (if found)
# output: $5 - iface: the resolved STA interface name on 5 GHz (if found)
# output: $6 - iface_config_name: the resolved name of the config section
#                                 for the STA interface on 5 GHz (if found)
# output: $7 - unknown_ifaces: whether any Wi-Fi interfaces are as yet
#                              unknown (in terms of their interface name)
__repacd_wifimon_is_sta_iface() {
    local config="$1"
    local network_to_match="$2"
    local iface disabled mode device hwmode

    config_get network "$config" network
    config_get iface "$config" ifname
    config_get disabled "$config" disabled '0'
    config_get mode "$config" mode
    config_get device "$config" device
    config_get hwmode "$device" hwmode

    if [ "$hwmode" != "11ad" ]; then
        if [ "$network" = "$network_to_match" ] && [ -n "$iface" ] && [ "$mode" = "sta" ] && \
            [ "$disabled" -eq 0 ]; then
            if whc_is_5g_vap "$config"; then
                eval "$5=$iface"
                eval "$6=$config"
            else
                eval "$3=$iface"
                eval "$4=$config"
            fi
        elif [ -z "$iface" ] && [ "$disabled" -eq 0 ]; then
            # If an interface is showing as enabled but no name is known for it,
            # mark it as such. Without doing this, we can resolve the interface
            # names improperly.
            eval "$7=1"
        fi
    fi
}

# Initialize the sta_iface_5g variable with the STA interface that is enabled
# on the specified network (if any).
# input: $1 - network: the name of the network being managed
__repacd_wifimon_get_sta_iface() {
    unknown_ifaces=0

    config_load wireless
    config_foreach __repacd_wifimon_is_sta_iface wifi-iface "$1" \
        sta_iface_24g sta_iface_24g_config_name \
        sta_iface_5g sta_iface_5g_config_name unknown_ifaces

    if [ "$unknown_ifaces" -gt 0 ]; then
        # Clear out everything because we cannot be certain we have the
        # right names (eg. interfaces may not all be up yet).
        sta_iface_24g=
        sta_iface_24g_config_name=
        sta_iface_5g=
        sta_iface_5g_config_name=
    fi
}

# Initialize the Wi-Fi monitoring logic with the name of the network being
# monitored.
# input: $1 - network: the name of the network being managed
# input: $2 - cur_re_mode: the current operating range extender mode
# input: $3 - cur_re_submode: the current operating range extender submode
# input: $4 - autoconfig: whether it was an auto-config restart
# output: $5 - state: the name of the initial state
# output: $6 - new_re_mode: the resolved range extender mode
# output: $7 - new_re_submode: the resolved range extender submode
repacd_wifimon_init() {
    # Resolve the STA interfaces.
    # Here we assume that if we have the 5 GHz interface, that is sufficient,
    # as not all modes will have a 2.4 GHz interface.
    __repacd_wifimon_get_sta_iface "$1"
    if [ -n "$sta_iface_5g" ] || [ -n "$sta_iface_24g" ]; then
        if [ -n "$sta_iface_24g" ]; then
            __repacd_wifimon_debug "Resolved 2.4 GHz STA interface to $sta_iface_24g"
            __repacd_wifimon_debug "2.4 GHz STA interface section $sta_iface_24g_config_name"
        fi

        if [ -n "$sta_iface_5g" ]; then
            __repacd_wifimon_debug "Resolved 5 GHz STA interface to $sta_iface_5g"
            __repacd_wifimon_debug "5 GHz STA interface section $sta_iface_5g_config_name"
        fi

        # First resolve the config parameters.
        config_load repacd
        config_get min_wps_assoc 'MAPWiFiLink' 'MinAssocCheckPostWPS' '5'
        config_get wps_timeout 'MAPWiFiLink' 'WPSTimeout' '120'
        config_get assoc_timeout 'MAPWiFiLink' 'AssociationTimeout' '300'
        config_get rssi_samples 'MAPWiFiLink' 'RSSINumMeasurements' '5'
        config_get backhaul_rssi_2 'MAPWiFiLink' 'BackhaulRSSIThreshold_2' '-78'
        config_get backhaul_rssi_5 'MAPWiFiLink' 'BackhaulRSSIThreshold_5' '-78'
        config_get backhaul_rssi_offset 'MAPWiFiLink' 'BackhaulRSSIThreshold_offset' '10'
        config_get max_5g_attempts 'MAPWiFiLink' 'Max5gAttempts' '3'
        config_get cnt_5g_attempts 'MAPWiFiLink' '5gAttemptsCount' '0'
        config_get measuring_attempts 'MAPWiFiLink' 'MaxMeasuringStateAttempts' '3'
        config_get force_bsses_down_on_all_bsta_switches 'MAPWiFiLink' \
            'ForceBSSesDownOnAllBSTASwitches' '0'

        # Create ourselves a named pipe so we can be informed of WPS push
        # button events.
        if [ -e $WIFIMON_PIPE_NAME ]; then
            rm -f $WIFIMON_PIPE_NAME
        fi

        mkfifo $WIFIMON_PIPE_NAME

        # If already associated, go to the InProgress state.
        __repacd_wifimon_check_associated "$1" "$2" "$3" "$4" "$5" "$6" "$7"
    fi
    # Otherwise, must be operating in CAP mode.
}

# Check the status of the Wi-Fi link (WPS, association, and RSSI).
# input: $1 - network: the name of the network being managed
# input: $2 - cur_re_mode: the currently configured range extender mode
# input: $3 - cur_re_submode: the currently configured range extender sub-mode
# output: $4 - state: the name of the new state (only set upon a change)
# output: $5 - re_mode: the desired range extender mode (updated only once
#                       the link to the AP is considered stable)
# output: $6 - re_submode: the desired range extender submode (updated only once
#                       the link to the AP is considered stable)
repacd_wifimon_check() {
    local wps_pressed=0
    if [ -n "$sta_iface_24g" ] || [ -n "$sta_iface_5g" ]; then
        local wps_pbc
        if read -r -t 1 wps_pbc <>$WIFIMON_PIPE_NAME; then
            __repacd_wifimon_debug "Received $wps_pbc on wifimon pipe"

            eval "$4=$WIFIMON_STATE_AUTOCONFIG_IN_PROGRESS"
            wps_in_progress=1
            __repacd_wifimon_get_timestamp wps_start_time
        fi

        if __repacd_wifimon_check_associated "$1" "$2" "$3" 0 "$4" "$5" "$6"; then
            assoc_timeout_logged=0
            wps_timeout_logged=0
        else  # not associated
            # If the WPS button was pressed, do not proceed to checking the
            # association timeouts. That will happen in subsequent checks.
            if [ "$wps_pressed" -gt 0 ]; then
                return 1
            fi

            if [ "$wps_in_progress" -gt 0 ]; then
                if __repacd_wifimon_is_timeout $wps_start_time $wps_timeout; then
                    if [ "$wps_timeout_logged" -eq 0 ]; then
                        __repacd_wifimon_debug "WPS timeout"
                        wps_timeout_logged=1
                    fi

                    eval "$4=$WIFIMON_STATE_WPS_TIMEOUT"
                fi
            else
                if __repacd_wifimon_is_timeout $assoc_start_time $assoc_timeout; then
                    if [ "$assoc_timeout_logged" -eq 0 ]; then
                        __repacd_wifimon_debug "Association timeout"
                        assoc_timeout_logged=1
                    fi

                    if [ -n "$sta_iface_5g" ]; then
                        # Try falling back to another interface
                        __repacd_wifimon_debug "Trying next bSTA radio"

                        # Since this bSTA could not associate, assume the next
                        # one might not be able to either. Thus, tell the
                        # fronthaul manager to force the BSSes down at startup.
                        local force_bsses_down=1
                        __repacd_wifimon_select_next_bsta "$sta_iface_5g_config_name" \
                            $force_bsses_down \
                            "$WIFIMON_STATE_AUTOCONFIG_IN_PROGRESS" "$4"
                    else
                        # Already on 2.4 GHz. If we cannot associate here, we
                        # probably cannot associate on 5 GHz.
                        eval "$4=$WIFIMON_STATE_ASSOC_TIMEOUT"
                    fi
                fi
            fi
        fi
    fi
}

# Hook function that, in SON mode, would determine whether to bring up/down the
# 2.4 GHz backhaul. In Multi-AP SIG mode, this is not currently relevant
# (given the 1 backhaul link assumption).
repacd_wifimon_independent_channel_check() {
    /bin/true
}

# Terminate the Wi-Fi link monitoring, cleaning up any state in preparation
# for shutdown.
repacd_wifimon_fini() {
    /bin/true
}

# Resolve the credentials for any VAP that has bBSS support.
#
# input: $1 - config: the name of the interface config section
# input: $2 - network: the name of the network to which the AP interface
#                      must belong to be matched
# output: $3 - ssid: the backhaul SSID
# output: $4 - key: the backhaul key (aka. passphrase)
# output: $5 - enc: the backhaul encryption mode
__repacd_wifimon_resolve_bbss_creds() {
    local config="$1"
    local network_to_match="$2"

    local network mode device hwmode disabled
    local ssid key enc map map_type

    config_get network "$config" network
    config_get mode "$config" mode
    config_get device "$config" device
    config_get hwmode "$device" hwmode
    config_get disabled "$config" disabled '0'

    config_get ssid "$config" ssid
    config_get key "$config" key
    config_get enc "$config" encryption
    config_get map "$config" map '0'
    config_get map_type "$config" MapBSSType

    if [ "$hwmode" != "11ad" ]; then
        if [ "$network" = "$network_to_match" ] && [ "$mode" = "ap" ] && \
            [ "$disabled" -eq 0 ] && [ "$map" -gt 0 ]; then
            local bbss=$(( map_type & 64 ))
            if [ ${bbss} -eq 64 ]; then
                # Has bBSS capabilty, so record the credentials
                eval "$3='${ssid}'"
                eval "$4='${key}'"
                eval "$5='${enc}'"
            fi
        fi
    fi
}

# Set the credentials on the bSTA VAP(s).
#
# input: $1 - config: the name of the interface config section
# input: $2 - network: the name of the network to which the STA interface
#                      must belong to be matched
# input: $3 - ssid: the backhaul SSID
# input: $4 - key: the backhaul key (aka. passphrase)
# input: $5 - enc: the backhaul encryption mode
__repacd_wifimon_set_bsta_creds() {
    local config="$1"
    local network_to_match="$2"
    local ssid="$3"
    local key="$4"
    local enc="$5"

    local network mode hwmode device disabled
    local ssid key enc map map_type

    config_get network "$config" network
    config_get mode "$config" mode
    config_get device "$config" device
    config_get hwmode "$device" hwmode
    config_get disabled "$config" disabled '0'

    config_get map "$config" map '0'
    config_get map_type "$config" MapBSSType

    if [ "$hwmode" != "11ad" ]; then
        if [ "$network" = "$network_to_match" ] && [ "$mode" = "sta" ] && \
            [ "$map" -gt 0 ]; then
            if [ "$map_type" -eq 128 ]; then  # bSTA
                uci_set wireless "$config" ssid "$ssid"
                uci_set wireless "$config" key "$key"
                uci_set wireless "$config" encryption "$enc"
                uci_commit wireless
            fi
        fi
    fi
}

# Copy the backhaul credentials from a bBSS interface (or a combined
# fBSS + bBSS) to the bSTA interface. This ensures the credentials obtained
# during Ethernet onboarding can be used when switching into RE mode.
repacd_wifimon_config_bsta() {
    bbss_ssid=''
    bbss_key=''
    bbss_enc=''

    # First resolve the SSID, passphrase, and encryption from the bBSS
    # capable AP interfaces.
    config_load wireless
    config_foreach __repacd_wifimon_resolve_bbss_creds wifi-iface "$1" \
        bbss_ssid bbss_key bbss_enc

    # Now apply these values to the bSTA interface.
    config_foreach __repacd_wifimon_set_bsta_creds wifi-iface "$1" \
        "${bbss_ssid}" "${bbss_key}" "${bbss_enc}"
}
