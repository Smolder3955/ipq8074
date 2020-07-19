#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import time
import re
import logging
import enum
import struct
import whcdiag.hydmsgs.common as hyd_common
import whcdiag.msgs.common as diag_common
import whcdiag.hydmsgs.heservice as heservice
import whcdiag.hydmsgs.pstables as pstables
from whcdiag.constants import TRANSPORT_PROTOCOL

from whcmvc.controller import wifi_commands as Cmds

"""Classes that periodically monitor conditions on target devices.

Currently this module exports the following monitors:

    :class:`UtilizationsMonitor`
        periodically samples hardware counters and computes a medium
        utilization value

    :class:`AssociatedStationMonitor`
        periodically queries the VAPs to determine which stations are
        associated
"""

log = logging.getLogger('whcmonitor')
"""The logger used for condition monitor classes, named ``whcmonitor``."""


class ConditionMonitorBase(object):

    """Base interface for all periodic monitors of AP conditions.

    Derived classes must implement the methods that determine the next
    sampling time as well as the actual action to run.
    """

    # The polled mode is when all data is coming from periodic sampling.
    #
    # The event driven mode is when most information is sent to the
    # controller via some sort of asynchronous logging mechanism.
    #
    # Both is for monitors that should execute regardless of the
    # mechanism for the bulk of the data.
    Mode = enum.Enum('Mode', 'POLLED EVENT_DRIVEN BOTH')

    def __init__(self, interval, mode=Mode.POLLED, uses_debug_cli=False):
        """Initialize a new condition monitor.

        Args:
            interval (float): the periodicity for the condition monitor
            mode (:obj:`Mode`): the modes of operation in which this
                condition monitor should be executed
        """
        self._interval = interval
        self._mode = mode
        self._uses_debug_cli = uses_debug_cli
        self.reset()

    def sample_conditions(self, ap_console):
        """Perform the actual sampling.

        This method should not be overridden by derived classes.
        Instead, they should override :meth:`_sample_condition_impl`.

        :param :class:`APConsoleBase` ap_console: The console object to
            use for command execution.
        """
        self._sample_conditions_impl(ap_console)

        # Record the time the sample was taken so that hte next one can
        # be scheduled correctly.
        self._last_sample_time = time.time()

    def _sample_conditions_impl(self, ap_console):
        """Hook function for actual condition monitors.

        Derived classes must override this function to perform the
        actual monitoring of the hybrid device.

        :param :class:`APConsoleBase` ap_console: The console object to
            use for command execution.

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Derived classes must implement this method")

    def get_next_sample_delta(self, mode):
        """Compute the amount of time until the next sample.

        If the sampling period has elapsed, this will return 0.

        Args:
            mode (:obj:`Mode`): the current mode of operation for the
                executor (one of `ACTIVE` or `PASSIVE`)
        """
        if self.should_sample_in_mode(mode):
            delta_time = self._last_sample_time + self._interval - time.time()
            if delta_time <= 0:
                return 0  # should expire now
            else:
                return delta_time
        else:
            return float('inf')

    def reset(self):
        """Reset any state in the condition monitor.

        This is used any time the monitor is started/restarted.
        """
        self._last_sample_time = 0  # want first interval to happen immediately
        self._reset_impl()

    def _reset_impl(self):
        """Hook function for actual condition monitors to reset their state.

        This is a nop in the base class. Derived classes should override
        this if necessary.
        """
        pass

    def should_sample_in_mode(self, mode):
        """Determine whether this monitor samples in the provided mode.

        Args:
            mode (:obj:`Mode`): the mode of operation
        """
        return self.mode == mode or self.mode == self.Mode.BOTH

    @property
    def uses_debug_cli(self):
        return self._uses_debug_cli

    @property
    def mode(self):
        return self._mode


class AssociatedStationsMonitor(ConditionMonitorBase):

    """Monitor for the associated stations and their RSSI.

    This monitor will check at a specific interval for the associated
    stations on a list of interfaces and report back the list of the
    device MAC addresses along with their RSSI.
    """

    MAC_ADDR_REGEX = ':'.join(['[0-9a-fA-F]{2}'] * 6)
    TEN_DOT_2_HEADER_REGEX = r'^ADDR\s+AID\s+CHAN\s+TXRATE\s+RXRATE\s+RSSI\s+IDLE'
    TEN_DOT_4_VALS_REGEX = re.compile(r"""
        ADDR\s*:(?P<addr>%s)\s+
        AID:\s*\d+\s+
        CHAN:\s*\d+\s+
        TXRATE:\s*(?P<txrate>\d+)M\s+
        RXRATE:\s*(?P<rxrate>\d+)M\s+
        RSSI:\s*(?P<rssi>\d+)""" % MAC_ADDR_REGEX, re.VERBOSE)

    def __init__(self, ap, handler, interval=0.500):
        """Initialize a new associated stations monitor.

        :param :class:`AccessPoint` ap: the AP on whose all VAPs to
            check the list of associated stations on all its VAPs
        :param :class:`AssociatedStationsHandlerIface` handler: the
            handler to inform with the current set of associated
            stations
        :param float interval: how frequently (in seconds) to check
            for the associated stations
        """
        ConditionMonitorBase.__init__(self, interval)

        self._ap = ap
        self._handler = handler

        self._header_re = re.compile(self.TEN_DOT_2_HEADER_REGEX)

    def _sample_conditions_impl(self, ap_console):
        """Actually perform the sampling and notify the handler.

        :param :class:`APConsoleBase` ap_console: The console object to
            use for command execution.
        """
        stations = {}
        for vap in self._ap.get_vaps():
            output = ap_console.run_cmd("wlanconfig %s list" % vap.iface_name)
            stations[vap.vap_id] = self._parse_output(output)

        self._handler.update_associated_stations(stations, self._ap.ap_id)

    def _parse_output(self, output):
        """Extract the associated stations and RSSI values.

        :param str output: the output from the wlanconfig command, as a
            list of strings
        """
        if len(output) > 0 and self._header_re.match(output[0]):
            return self._parse_output_with_header(output[1:])
        else:
            return self._parse_output_no_header(output)

    def _parse_output_with_header(self, output):
        """Extract the associated stations and RSSI values for 10.2.

        The 10.2 wlanconfig has a header and then one STA per line.

        :param str output: the output from the wlanconfig command, as a
            list of strings
        """
        stations = []
        for station_str in output:
            fields = station_str.split()
            try:
                # TxRate is in the third field.
                txrate = fields[3].replace('M', '')
                # MAC address is in first field. The RSSI is in the 6th one.
                stations.append((fields[0], int(fields[5]), int(txrate)))
            except Exception:
                log.error('Failed to parse assoc STAs output string: "%s"',
                          station_str)

        return stations

    def _parse_output_no_header(self, output):
        """Extract the associated stations and RSSI values for 10.4.

        The 10.4 wlanconfig has no header and has the data tagged with
        keywords.

        :param str output: the output from the wlanconfig command, as a
            list of strings
        """
        stations = []
        for station_str in output:
            match = self.TEN_DOT_4_VALS_REGEX.match(station_str)
            if match:
                stations.append((match.group('addr'), int(match.group('rssi')),
                                 int(match.group('txrate'))))

        return stations


class UtilizationsMonitor(ConditionMonitorBase):

    """Monitor medium utilization value

    This monitor will check at a specific interval for the acsreport output
    on a list of interfaces and report back the medium utilization
    """

    def __init__(self, ap, handler, interval=5):
        """Initialize a new utilization monitor

        :param :class:`AccessPoint` ap: the AP containing the VAPs on
            which to check the utilization
        :param :class:`UtilizationsHandlerIface` handler: the
            handler to inform with the current set of channel
            utilizations
        :param float interval: how frequently (in seconds) to check
            for the channel utilization
        """
        ConditionMonitorBase.__init__(self, interval)

        self._ap = ap
        self._handler = handler

    def _sample_conditions_impl(self, ap_console):
        """Actually perform the sampling and notify the handler.

        :param :class:`APConsoleBase` ap_console: The console object to
            use for command execution.
        """
        utilizations = {}
        for vap in self._ap.get_vaps():
            if vap.vap_id in self._freq_map and self._freq_map[vap.vap_id][0] > 0:
                self._freq_map[vap.vap_id][1] -= 1
                output = ap_console.run_cmd(Cmds.GET_ACS_REPORT_FMT %
                                            (vap.iface_name,
                                             self._freq_map[vap.vap_id][0]))
                if self._parse_output(output, vap.vap_id, utilizations) or \
                        self._freq_map[vap.vap_id][1] <= 0:
                    self._freq_map[vap.vap_id][0] = 0
            else:
                output = ap_console.run_cmd(Cmds.GET_WIFI_FREQUENCY_FMT % vap.iface_name)
                chan, freq = self._parse_channel(output)
                if chan == 0:
                    continue

                ap_console.run_cmd(Cmds.SET_ACS_SCANLIST_FMT % (vap.iface_name, chan))
                ap_console.run_cmd(Cmds.START_ACS_REPORT_FMT % vap.iface_name)
                self._freq_map[vap.vap_id] = [freq, 3]  # Max retry three times

        log.debug("Utilization information %r", utilizations)
        if len(utilizations) > 0:
            # This mode does no averaging, so report it as a raw utilization
            # measurement.
            utilizations = {'data': utilizations, 'type': 'raw'}
            self._handler.update_utilizations(utilizations, self._ap.ap_id)

    def _reset_impl(self):
        """Reset the state for the condition monitor.

        This will initiate new channel utilization reports the next time
        conditions are sampled.
        """
        # Dict recording the acsreport status of each VAP. Key is the VAP ID;
        # value is a list of [frequency, retry_count]. If the frequency is
        # not present or is 0, this means an acsreport scan is in progress on
        # that frequency; otherwise, no acsreport is in progress.
        self._freq_map = {}

    def _parse_channel(self, output):
        """Parse output from iwconfig to get Wi-Fi channel

        :param str output: the output from the iwconfig command
        Sample valid output:
        ['          Mode:Master  Frequency:5.745 GHz  Access Point: 8C:FD:F0:00:B4:A2   ']

        :return the channel number parsed, or 0 for invalid output, and the corresponding frequency
        """
        try:
            return Cmds.parse_channel(output[0].split()[1])
        except Exception:
            # Invalid output from iwconfig
            log.error("Failed to parse output %r" % output)
            return 0, 0

    def _parse_output(self, output, vap_id, utilizations):
        """Parse output from wifitool acsreport to get utilization

        Args:
            output (str): the output from the wifitool acsreport command, as a
                list of strings.
                Sample valid output:
                    [' 2412(  1)    0        0         0   -105       1           0          0   ']
            vap_id (int): the VirtualAccessPoint ID whose utilization is calculated
            utilizations (:obj:`dict`): the utilizations to be filled
        Returns:
            True if a valid utilization is parsed; otherwise return False
        """
        utilization = Cmds.parse_util(output)
        if utilization > 0:
            utilizations[vap_id] = utilization
            return True

        return False


class ThroughputsMonitor(ConditionMonitorBase):

    """Monitor throughput on a VAP

    This monitor will check at a specific interval for the athstats output
    on a given interface and then compute the TX and RX throughputs.
    """

    TX_BYTES_REGEX = re.compile('^Tx Data Payload Bytes\s+= (\d+)')
    RX_BYTES_REGEX = re.compile('^Rx Data Payload Bytes\s+= (\d+)')

    def __init__(self, ap, handler, interval=2):
        """Initialize a new throughput monitor

        Args:
            ap (:obj:`AccessPoint`): the AP on which to check the
                throughput
            handler (:obj:`ThroughputsHandlerIface`): the
                handler to inform with the measured throughput
            interval (float): how frequently (in seconds) to check
                the throughput
        """
        ConditionMonitorBase.__init__(self, interval,
                                      ConditionMonitorBase.Mode.BOTH)

        self._ap = ap
        self._handler = handler

        self._byte_stats = {}

    def _sample_conditions_impl(self, ap_console):
        """Actually perform the sampling and notify the handler.

        Args:
            ap_console (:obj:`APConsoleBase`): The console object to
                use for command execution.
        """
        throughputs = {}
        for vap in self._ap.get_vaps():
            sample_time = time.time()
            output = ap_console.run_cmd(Cmds.GET_APSTATS_VAP_TXRX %
                                        vap.iface_name)
            (tx_bytes, rx_bytes) = self._parse_output(output)
            if tx_bytes is not None and rx_bytes is not None:
                if vap.radio_iface in self._byte_stats:
                    (old_time, old_tx, old_rx) = self._byte_stats[vap.radio_iface]

                    # Note that here an overflow of the byte counts is not
                    # considered. At least on Akronite, these appear to be
                    # 64-bit counters, so they should not overflow in
                    # practical time frames in which a demo may be given.
                    #
                    # However, if this GUI is ever used for a MIPS-based
                    # platform, this may need to be reconsidered, as they
                    # may only use 32-bit counters.
                    tx_throughput = (tx_bytes - old_tx) / (sample_time - old_time)
                    rx_throughput = (rx_bytes - old_rx) / (sample_time - old_time)

                    throughputs[vap.vap_id] = (tx_throughput, rx_throughput)

                # Record the sample as the starting point for the next delta
                # bytes computation.
                self._byte_stats[vap.radio_iface] = (sample_time, tx_bytes,
                                                     rx_bytes)

        if len(throughputs) > 0:
            log.debug("Throughput information for %s: %r", self._ap.ap_id,
                      throughputs)
            self._handler.update_throughputs(throughputs, self._ap.ap_id)

    def _reset_impl(self):
        """Reset the state for the condition monitor.

        This will initiate new throughput measurements the next time
        conditions are sampled.
        """
        self._byte_stats = {}

    def _parse_output(self, output):
        """Parse the output from athstats for the byte counters.

        Args:
            output (str): the output from the athstats command
                (already filtered to just the relevant lines)

        Returns:
            a tuple of the TX byte count and RX byte count or a tuple
            of two None values if the output did not match the expected
            format
        """
        if len(output) >= 2:
            tx_bytes_match = self.TX_BYTES_REGEX.match(output[0])
            rx_bytes_match = self.RX_BYTES_REGEX.match(output[1])
            if tx_bytes_match and rx_bytes_match:
                return (int(tx_bytes_match.group(1)), int(rx_bytes_match.group(1)))

        return (None, None)


class PathMonitor(ConditionMonitorBase):

    """Monitor H-Default / H-Active paths

    This monitor will check at a specific interval for the contents of H-Active
    and H-Default (from the hyd debug CLI), and output them as diag logs.
    """

    def __init__(self, ap, handler, rate_threshold, interval=2):
        """Initialize a new path monitor

        Args:
            ap (:obj:`AccessPoint`): the AP on which to check the
                paths
            handler (:obj:`MsgHandler`): diaglog handler
            rate_threshold (float): ignore H-Active rows with a rate less than this value
                (in bps)
            interval (float): how frequently (in seconds) to check
                the paths
        """
        ConditionMonitorBase.__init__(self, interval,
                                      ConditionMonitorBase.Mode.BOTH,
                                      True)

        self._ap = ap
        self._handler = handler
        self._rate_threshold = rate_threshold

    def _sample_conditions_impl(self, ap_console):
        """Actually perform the sampling and notify the handler.

        Args:
            ap_console (:obj:`APConsoleBase`): The console object to
                use for command execution.
        """

        # Get H-Active
        ha_output = ap_console.run_cmd('he s')

        # Get H-Default
        hd_output = ap_console.run_cmd('hy hd')

        # Prune the output: Skip the first four entries (header)
        ha_output = ha_output[4:]
        # Parse and pack into the diaglog format
        buf = self._parse_hactive(ha_output)

        # Inject as a dialog log
        if len(buf):
            self._create_and_send_dialog_log(hyd_common.ModuleID.HE.value,
                                             heservice.MessageID.DETAIL.value,
                                             buf)

        # Prune the output: Skip the first two entries (header)
        hd_output = hd_output[2:]
        # Parse and pack into the diaglog format
        buf = self._parse_hdefault(hd_output)

        # Inject as a dialog log
        if len(buf):
            self._create_and_send_dialog_log(hyd_common.ModuleID.PS_TABLES.value,
                                             pstables.MessageID.HDEFAULT.value,
                                             buf)

    def _parse_hdefault(self, output):
        """Parse the output from the H-Default dump.

        Args:
            output (str): the output from the 'hy hd' command
                (already filtered to just the relevant lines)

        Returns:
            a buffer containing the packed contents of H-Default in the pstables
            H-Default diaglog format
        """
        buf = ''
        for line in output:
            split = line.split()
            if len(split) != 8:
                # No more full lines
                break

            # We don't know the interface type here - it will be filled in
            # by the model
            # Pack the fixed length portion
            da_addr = diag_common.ether_aton(split[1])
            id_addr = diag_common.ether_aton(split[2])
            buf += pstables._HDefaultRow.pack(da_addr, id_addr,
                                              hyd_common.InterfaceType.WLAN5G.value,
                                              hyd_common.InterfaceType.WLAN5G.value)

            # Pack the variable length interface names
            buf += self._pack_variable_length_string(split[4])
            buf += self._pack_variable_length_string(split[6])

        return buf

    def _pack_variable_length_string(self, string_to_pack):
        """Pack a variable length string.

        Args:
            string_to_pack (str): string to pack

        Returns:
            a buffer containing the packed string, and its length
        """
        format_str = '>H%ds' % len(string_to_pack)
        return struct.pack(format_str, len(string_to_pack), string_to_pack)

    def _parse_hactive(self, output):
        """Parse the output from the H-Active dump.

        Args:
            output (str): the output from the 'he s' command
                (already filtered to just the relevant lines)

        Returns:
            a buffer containing the packed contents of H-Active in the heservice
            Detail diaglog format
        """
        buf = ''
        for line in output:
            split = line.split()
            if len(split) != 14:
                # No more full lines
                break

            # Only pack rows that exceed the rate threshold
            if int(split[4]) < self._rate_threshold:
                continue

            if split[13].find("U") == -1:
                sub_class = TRANSPORT_PROTOCOL.OTHER.value
            else:
                sub_class = TRANSPORT_PROTOCOL.UDP.value
            # We don't know the interface type here - it will be filled in
            # by the model
            # Pack the fixed length portion
            da_addr = diag_common.ether_aton(split[2])
            id_addr = diag_common.ether_aton(split[3])
            buf += heservice._HActiveRow.pack(da_addr, id_addr, int(split[11], base=16),
                                              int(split[4]), int(split[1], base=16),
                                              sub_class,
                                              hyd_common.InterfaceType.WLAN5G.value)

            # Pack the variable length interface name
            buf += self._pack_variable_length_string(split[5])

        return buf

    def _create_and_send_dialog_log(self, msg_id, sub_id, buf):
        """Create and send a dialog log to the handler.

        Args:
            msg_id (int): message ID to use for the log (identifies the module)
            sub_id (int): message sub ID to use for the log (identifies the message type)
            buf (str): the message payload (already packed)
        """
        # Leave timestamp and MAC address empty for now
        # Pack the header
        header = hyd_common._HeaderFormat.pack(msg_id, sub_id, 0, 0,
                                               '\0x00\0x00\0x00\0x00\0x00\0x00')
        buf = header + buf

        # Send to diaglog listener
        self._handler.process_msg([self._ap.ip_address, 0], buf)
