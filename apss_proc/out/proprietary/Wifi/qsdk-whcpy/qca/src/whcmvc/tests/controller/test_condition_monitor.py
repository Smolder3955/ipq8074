#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import unittest

from mock import MagicMock, patch, call

from whcdiag.constants import BAND_TYPE
from whcmvc.controller.wifi_commands import GET_APSTATS_VAP_TXRX

from whcmvc.controller.console import APConsoleBase
from whcmvc.controller.diaglog import MsgHandler

from whcmvc.model.handlers import AssociatedStationsHandlerIface
from whcmvc.model.handlers import UtilizationsHandlerIface
from whcmvc.model.handlers import ThroughputsHandlerIface
from whcmvc.model.model_core import AccessPoint, VirtualAccessPoint
from whcmvc.model.model_core import AP_TYPE, CHANNEL_GROUP

from whcmvc.controller.condition_monitor import ConditionMonitorBase
from whcmvc.controller.condition_monitor import AssociatedStationsMonitor
from whcmvc.controller.condition_monitor import UtilizationsMonitor
from whcmvc.controller.condition_monitor import ThroughputsMonitor
from whcmvc.controller.condition_monitor import PathMonitor


class TestConditionMonitor(unittest.TestCase):

    WLANCONFIG_LIST_HEADING = \
        ''.join(["ADDR               AID CHAN TXRATE RXRATE RSSI IDLE  ",
                 "TXSEQ  RXSEQ  CAPS        ACAPS     ERP    STATE ",
                 "MAXRATE(DOT11) HTCAPS"])

    WLANCONFIG_10_DOT_4_TEMPLATE = \
        ''.join(["ADDR :{addr} AID:   {aid} CHAN:  {chan} TXRATE: {txrate}M ",
                 "RXRATE:   {rxrate}M RSSI:  {rssi} IDLE:   0 TXSEQ:     0 ",
                 "RXSEQ:  65535 CAPS:  ESs ACAPS:      ERP:  0 STATE:         f",
                 " MAXRATE(DOT11):             0 HTCAPS:            ",
                 "AP ASSOCTIME:00:38:58 WME MODE: IEEE80211_MODE_11NG_HT20\n",
                 "  PSMODE: 0"])

    TX_THROUGHPUT_TEMPLATE = 'Tx Data Payload Bytes   = %d'
    RX_THROUGHPUT_TEMPLATE = 'Rx Data Payload Bytes   = %d'

    HE_S_HEADER_TEMPLATE = ''.join(["HeaderLine1\nHeaderLine2\nHeaderLine3\nHeaderLine4"])
    HE_S_ROW_TEMPLATE = \
        ''.join(["[000] {hash} {da} {id} {rate} {intf} (17) 40 190778 0 0 ",
                 "{priority} 0 {sub_type}-S---"])

    HY_HD_HEADER_TEMPLATE = ''.join(["HeaderLine1\nHeaderLine2"])
    HY_HD_ROW_TEMPLATE = ''.join(["0: {da} {id} (17) {intf_udp} (17) {intf_other} 0"])

    def _create_mock_console(self):
        """Create a mocked out :class:`APConsoleBase` instance.

        The :meth:`run_cmd` method will have a mock installed.
        """
        console = APConsoleBase('dev1')
        console.run_cmd = MagicMock(name='run_cmd')
        return console

    def _reset_mock_console(self, console):
        """Reset the run_cmd mock."""
        console.run_cmd.reset_mock()

    def _create_mock_station_handler(self):
        """Create a mocked out :class:`AssociatedStationsHandlerIface`.

        The :meth:`update_associated_stations` will have a mock
        installed.
        """
        handler = AssociatedStationsHandlerIface()
        handler.update_associated_stations = MagicMock(
            name='update_associated_stations')
        return handler

    def _reset_mock_station_handler(self, handler):
        """Reset the update_associated_stations mock."""
        handler.update_associated_stations.reset_mock()

    def _create_mock_utilization_handler(self):
        """Create a mocked out :class:`UtilizationsHandlerIface`.

        The :meth:`update_utilizations` will have a mock installed
        """
        handler = UtilizationsHandlerIface()
        handler.update_utilizations = MagicMock(
            name='update_utilizations')
        return handler

    def _create_mock_throughput_handler(self):
        """Create a mocked out :class:`ThroughputsHandlerIface`.

        The :meth:`update_throughputs` will have a mock installed
        """
        handler = ThroughputsHandlerIface()
        handler.update_throughputs = MagicMock(
            name='update_throughputs')
        return handler

    def _create_mock_diaglog_handler(self):
        """Create a mocked out :class:`MsgHandler`.

        The :meth:`update_throughputs` will have a mock installed
        """
        handler = MsgHandler("model", "hyddiag", "assembler")
        handler.process_msg = MagicMock(
            name='process_msg')
        return handler

    def _create_columnar_sta_report(self, mac, txrate, rxrate, rssi):
        """Create the wlanconfig line for a single STA.

        This uses the column-based format from the 10.2 codebase.

        Most of the fields are defaulted to the same value as they are
        currently irrelevant. Only the MAC address, rates, and RSSI are
        populated.
        """
        fmt = ''.join(
            ["%s    1    6   %dM     %dM   %d    0      4     160   ESs"
             "         0         1f              0             PM WME"])
        return fmt % (mac, txrate, rxrate, rssi)

    def _create_tagged_sta_report(self, mac, txrate, rxrate, rssi):
        """Create the wlanconfig line for a single STA.

        This uses the tag-based format from the 10.4 codebase.

        Most of the fields are defaulted to the same value as they are
        currently irrelevant. Only the MAC address, rates, and RSSI are
        populated.
        """
        return self.WLANCONFIG_10_DOT_4_TEMPLATE.format(addr=mac, aid=1,
                                                        chan=11, txrate=txrate,
                                                        rxrate=rxrate,
                                                        rssi=rssi).split("\n")

    def test_base(self):
        """Verify the basic functionality of :class:`ConditionMonitorBase`."""
        console = self._create_mock_console()

        interval = 0.25  # 250 ms
        monitor = ConditionMonitorBase(interval)
        self.assertRaises(NotImplementedError, monitor.sample_conditions,
                          console)

        # Plug in a dummy implementation so we can verify the interval
        # handling of the base class.
        monitor._sample_conditions_impl = MagicMock(
            name='_sample_conditions_impl')

        current_time = 37
        time_mock = MagicMock(return_value=current_time)
        with patch('time.time', time_mock):
            self.assertEquals(0, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            monitor.sample_conditions(console)
            self.assertAlmostEquals(interval, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            current_time += 0.1
            time_mock.return_value = current_time
            self.assertAlmostEquals(interval - 0.1, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            # Verify that once the time has passed, it indicates there is no
            # more time remaining.
            current_time += interval + 0.01
            time_mock.return_value = current_time
            self.assertEquals(0, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            monitor.sample_conditions(console)
            self.assertAlmostEquals(interval, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            # Slightly less than full time.
            current_time += interval - 0.001
            time_mock.return_value = current_time
            self.assertAlmostEquals(0.001, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            # Advance time by the exact amount. This may not be exactly the
            # time, but it should be close enough for the monitor to consider
            # it time to execute.
            time_mock.return_value += 0.001
            self.assertEquals(0, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            monitor.sample_conditions(console)
            self.assertEquals(interval, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            # Advance the time by a bit and then reset it before the next
            # sampling time. The monitor should immediately indicate it is
            # ready to be sampled.
            current_time += 0.1
            time_mock.return_value = current_time
            self.assertAlmostEquals(interval - 0.1,
                                    monitor.get_next_sample_delta(
                                        ConditionMonitorBase.Mode.POLLED),
                                    places=2)

            monitor.reset()
            self.assertAlmostEquals(0, monitor.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

    def test_base_get_sample_mode(self):
        """Test the various condition monitor modes.

        The primary purpose of this test is to confirm that the next
        sampling time is returned properly based on the current mode
        versus the mode of the condition monitor.
        """
        console = self._create_mock_console()

        interval_poll = 0.7  # 700 ms
        monitor_poll = ConditionMonitorBase(
            interval_poll, mode=ConditionMonitorBase.Mode.POLLED)

        interval_ev = 0.25  # 250 ms
        monitor_ev = ConditionMonitorBase(
            interval_ev, mode=ConditionMonitorBase.Mode.EVENT_DRIVEN)

        interval_both = 0.4  # 400 ms
        monitor_both = ConditionMonitorBase(
            interval_both, mode=ConditionMonitorBase.Mode.BOTH)

        # Plug in a dummy implementation so we can verify the interval
        # handling of the base class.
        monitor_poll._sample_conditions_impl = MagicMock(
            name='_sample_conditions_impl')
        monitor_ev._sample_conditions_impl = MagicMock(
            name='_sample_conditions_impl')
        monitor_both._sample_conditions_impl = MagicMock(
            name='_sample_conditions_impl')

        current_time = 49
        time_mock = MagicMock(return_value=current_time)
        with patch('time.time', time_mock):
            # First sample should be immediate, but only if it is relevant for
            # the mode.
            self.assertEquals(0, monitor_poll.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))
            self.assertEquals(float('inf'), monitor_ev.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))
            self.assertEquals(0, monitor_both.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            monitor_poll.sample_conditions(console)
            monitor_both.sample_conditions(console)
            self.assertAlmostEquals(interval_poll, monitor_poll.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))
            self.assertEquals(float('inf'), monitor_ev.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))
            self.assertAlmostEquals(interval_both, monitor_both.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            # Advance time a little and make sure the remaining time is
            # computed properly.
            current_time += 0.1
            time_mock.return_value = current_time
            self.assertAlmostEquals(interval_poll - 0.1, monitor_poll.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))
            self.assertEquals(float('inf'), monitor_ev.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))
            self.assertAlmostEquals(interval_both - 0.1, monitor_both.get_next_sample_delta(
                ConditionMonitorBase.Mode.POLLED))

            monitor_both.sample_conditions(console)

            # Now switch to event driven mode, which should involve both of them.
            current_time += 0.3
            time_mock.return_value = current_time
            self.assertEquals(float('inf'), monitor_poll.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))
            self.assertEquals(0, monitor_ev.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))
            self.assertAlmostEquals(interval_both - 0.3, monitor_both.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))

            monitor_ev.sample_conditions(console)
            monitor_both.sample_conditions(console)
            self.assertEquals(float('inf'), monitor_poll.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))
            self.assertAlmostEquals(interval_ev, monitor_ev.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))
            self.assertAlmostEquals(interval_both, monitor_both.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))

            # Advance time some and confirm
            current_time += 0.2
            time_mock.return_value = current_time
            self.assertEquals(float('inf'), monitor_poll.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))
            self.assertAlmostEquals(interval_ev - 0.2, monitor_ev.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))
            self.assertAlmostEquals(interval_both - 0.2, monitor_both.get_next_sample_delta(
                ConditionMonitorBase.Mode.EVENT_DRIVEN))

    def test_associated_stations_10dot2(self):
        """Verify the functionality of :class:`AssociatedStationsMonitor`."""
        ###############################################################
        # Monitoring single VAP.
        ###############################################################
        station_handler = self._create_mock_station_handler()
        console = self._create_mock_console()

        ap1 = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')
        vap1_1 = VirtualAccessPoint(ap1, 'device1-2g', BAND_TYPE.BAND_24G,
                                    'ath0', 'wifi0', 1, CHANNEL_GROUP.CG_24G,
                                    70)
        monitor = AssociatedStationsMonitor(ap1, station_handler)

        # Empty output, may happen if the command is executed after console
        # shutdown, make sure it does not crash
        console.run_cmd.return_value = []
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: []}, ap1.ap_id)

        # Invalid associated stations output, make sure it does not crash
        console.run_cmd.return_value = [self.WLANCONFIG_LIST_HEADING,
                                        "Some random string 298h32nc03n!!"]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: []}, ap1.ap_id)

        # No associated stations.
        console.run_cmd.return_value = [self.WLANCONFIG_LIST_HEADING]

        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: []}, ap1.ap_id)

        # Single associated station.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        sta1_mac = '00:a0:c6:00:00:01'
        sta1_txrate = 75
        sta1_rxrate = 55
        sta1_rssi = 47
        console.run_cmd.return_value = \
            [self.WLANCONFIG_LIST_HEADING,
             self._create_columnar_sta_report(sta1_mac, sta1_txrate,
                                              sta1_rxrate, sta1_rssi)]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate)]}, ap1.ap_id)

        # Multiple associated stations. This test also verifies that the
        # reset call does not change the behavior.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        monitor.reset()

        sta1_mac = '00:a0:c6:00:00:21'
        sta1_txrate = 42
        sta1_rxrate = 50
        sta1_rssi = 23

        sta2_mac = '00:a0:c6:00:00:22'
        sta2_txrate = 47
        sta2_rxrate = 38
        sta2_rssi = 37

        console.run_cmd.return_value = \
            [self.WLANCONFIG_LIST_HEADING,
             self._create_columnar_sta_report(sta1_mac, sta1_txrate,
                                              sta1_rxrate, sta1_rssi),
             self._create_columnar_sta_report(sta2_mac, sta2_txrate,
                                              sta2_rxrate, sta2_rssi)]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate),
                             (sta2_mac, sta2_rssi, sta2_txrate)]},
            ap1.ap_id)

        ###############################################################
        # Monitoring multiple VAPs.
        ###############################################################
        station_handler = self._create_mock_station_handler()
        console = self._create_mock_console()

        ap2 = AccessPoint('device1', AP_TYPE.RE, '11:11:11:00:00:01')
        vap2_1 = VirtualAccessPoint(ap2, 'device2-5g', BAND_TYPE.BAND_5G,
                                    'ath2', 'wifi1', 149, CHANNEL_GROUP.CG_5GH,
                                    70)
        vap2_2 = VirtualAccessPoint(ap2, 'device2-24g', BAND_TYPE.BAND_24G,
                                    'ath1', 'wifi0', 1, CHANNEL_GROUP.CG_24G,
                                    80)
        monitor = AssociatedStationsMonitor(ap2, station_handler)

        # No associated stations on either.
        console.run_cmd.return_value = [self.WLANCONFIG_LIST_HEADING]

        monitor.sample_conditions(console)
        calls = console.run_cmd.call_args_list
        self.assertEquals([(("wlanconfig %s list" % vap2_1.iface_name,),),
                           (("wlanconfig %s list" % vap2_2.iface_name,),)],
                          calls)
        station_handler.update_associated_stations.assert_called_with(
            {vap2_1.vap_id: [], vap2_2.vap_id: []}, ap2.ap_id)

        # Associated station on the second VAP but not the other.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        sta1_mac = '00:a0:c6:00:22:04'
        sta1_txrate = 77
        sta1_rxrate = 85
        sta1_rssi = 36
        console.run_cmd.side_effect = [
            [self.WLANCONFIG_LIST_HEADING],
            [self.WLANCONFIG_LIST_HEADING,
             self._create_columnar_sta_report(sta1_mac, sta1_txrate,
                                              sta1_rxrate, sta1_rssi)]
        ]

        monitor.sample_conditions(console)
        calls = console.run_cmd.call_args_list
        self.assertEquals([(("wlanconfig %s list" % vap2_1.iface_name,),),
                           (("wlanconfig %s list" % vap2_2.iface_name,),)],
                          calls)
        station_handler.update_associated_stations.assert_called_with(
            {vap2_1.vap_id: [],
             vap2_2.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate)]}, ap2.ap_id)

        # Associated stations on both VAPs.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        sta1_mac = '00:a0:c6:00:22:14'
        sta1_txrate = 15
        sta1_rxrate = 32
        sta1_rssi = 17

        sta2_mac = '00:a0:c6:00:22:24'
        sta2_txrate = 27
        sta2_rxrate = 16
        sta2_rssi = 16

        sta3_mac = '00:a0:c6:00:22:34'
        sta3_txrate = 155
        sta3_rxrate = 89
        sta3_rssi = 15

        sta4_mac = '00:a0:c6:00:22:44'
        sta4_txrate = 305
        sta4_rxrate = 343
        sta4_rssi = 28

        sta5_mac = '00:a0:c6:00:22:54'
        sta5_txrate = 788
        sta5_rxrate = 800
        sta5_rssi = 27

        sta6_mac = '00:a0:c6:00:22:64'
        sta6_txrate = 450
        sta6_rxrate = 420
        sta6_rssi = 26

        sta7_mac = '00:a0:c6:00:22:74'
        sta7_txrate = 47
        sta7_rxrate = 16
        sta7_rssi = 25

        console.run_cmd.side_effect = [
            [self.WLANCONFIG_LIST_HEADING,
             self._create_columnar_sta_report(sta1_mac, sta1_txrate,
                                              sta1_rxrate, sta1_rssi),
             self._create_columnar_sta_report(sta2_mac, sta2_txrate,
                                              sta2_rxrate, sta2_rssi),
             self._create_columnar_sta_report(sta3_mac, sta3_txrate,
                                              sta3_rxrate, sta3_rssi)],
            [self.WLANCONFIG_LIST_HEADING,
             self._create_columnar_sta_report(sta4_mac, sta4_txrate,
                                              sta4_rxrate, sta4_rssi),
             self._create_columnar_sta_report(sta5_mac, sta5_txrate,
                                              sta5_rxrate, sta5_rssi),
             self._create_columnar_sta_report(sta6_mac, sta6_txrate,
                                              sta6_rxrate, sta6_rssi),
             self._create_columnar_sta_report(sta7_mac, sta7_txrate,
                                              sta7_rxrate, sta7_rssi)]
        ]

        monitor.sample_conditions(console)
        calls = console.run_cmd.call_args_list
        self.assertEquals([(("wlanconfig %s list" % vap2_1.iface_name,),),
                           (("wlanconfig %s list" % vap2_2.iface_name,),)],
                          calls)
        station_handler.update_associated_stations.assert_called_with(
            {vap2_1.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate),
                             (sta2_mac, sta2_rssi, sta2_txrate),
                             (sta3_mac, sta3_rssi, sta3_txrate)],
             vap2_2.vap_id: [(sta4_mac, sta4_rssi, sta4_txrate),
                             (sta5_mac, sta5_rssi, sta5_txrate),
                             (sta6_mac, sta6_rssi, sta6_txrate),
                             (sta7_mac, sta7_rssi, sta7_txrate)]},
            ap2.ap_id)

    def test_associated_stations_10dot4(self):
        """Verify the functionality of :class:`AssociatedStationsMonitor`.

        This uses the output format from the 10.4 version of wlanconfig."""
        ###############################################################
        # Monitoring single VAP.
        ###############################################################
        station_handler = self._create_mock_station_handler()
        console = self._create_mock_console()

        ap1 = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')
        vap1_1 = VirtualAccessPoint(ap1, 'device1-2g', BAND_TYPE.BAND_24G,
                                    'ath0', 'wifi0', 1, CHANNEL_GROUP.CG_24G,
                                    70)
        monitor = AssociatedStationsMonitor(ap1, station_handler)

        # Empty output, may happen if the command is executed after console
        # shutdown, make sure it does not crash
        console.run_cmd.return_value = []
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: []}, ap1.ap_id)

        # Invalid associated stations output, make sure it does not crash
        console.run_cmd.return_value = ["Some random string 298h32nc03n!!"]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: []}, ap1.ap_id)

        # No associated stations.
        console.run_cmd.return_value = []

        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: []}, ap1.ap_id)

        # Single associated station.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        sta1_mac = '00:a0:c6:00:00:01'
        sta1_txrate = 75
        sta1_rxrate = 55
        sta1_rssi = 47
        console.run_cmd.return_value = \
            self.WLANCONFIG_10_DOT_4_TEMPLATE.format(addr=sta1_mac, aid=1,
                                                     chan=11, txrate=sta1_txrate,
                                                     rxrate=sta1_rxrate,
                                                     rssi=sta1_rssi).split("\n")
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate)]}, ap1.ap_id)

        # Multiple associated stations. This test also verifies that the
        # reset call does not change the behavior.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        monitor.reset()

        sta1_mac = '00:a0:c6:00:00:21'
        sta1_txrate = 42
        sta1_rxrate = 50
        sta1_rssi = 23

        sta2_mac = '00:a0:c6:00:00:22'
        sta2_txrate = 47
        sta2_rxrate = 38
        sta2_rssi = 37

        console.run_cmd.return_value = \
            self.WLANCONFIG_10_DOT_4_TEMPLATE.format(addr=sta1_mac, aid=1,
                                                     chan=11, txrate=sta1_txrate,
                                                     rxrate=sta1_rxrate,
                                                     rssi=sta1_rssi).split("\n") + \
            self.WLANCONFIG_10_DOT_4_TEMPLATE.format(addr=sta2_mac, aid=1,
                                                     chan=11, txrate=sta2_txrate,
                                                     rxrate=sta2_rxrate,
                                                     rssi=sta2_rssi).split("\n")

        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            "wlanconfig %s list" % vap1_1.iface_name)
        station_handler.update_associated_stations.assert_called_with(
            {vap1_1.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate),
                             (sta2_mac, sta2_rssi, sta2_txrate)]},
            ap1.ap_id)

        ###############################################################
        # Monitoring multiple VAPs.
        ###############################################################
        station_handler = self._create_mock_station_handler()
        console = self._create_mock_console()

        ap2 = AccessPoint('device1', AP_TYPE.RE, '11:11:11:00:00:01')
        vap2_1 = VirtualAccessPoint(ap2, 'device2-5g', BAND_TYPE.BAND_5G,
                                    'ath2', 'wifi1', 149, CHANNEL_GROUP.CG_5GH,
                                    85)
        vap2_2 = VirtualAccessPoint(ap2, 'device2-24g', BAND_TYPE.BAND_24G,
                                    'ath1', 'wifi0', 1, CHANNEL_GROUP.CG_24G,
                                    80)
        monitor = AssociatedStationsMonitor(ap2, station_handler)

        # No associated stations on either.
        console.run_cmd.return_value = []

        monitor.sample_conditions(console)
        calls = console.run_cmd.call_args_list
        self.assertEquals([(("wlanconfig %s list" % vap2_1.iface_name,),),
                           (("wlanconfig %s list" % vap2_2.iface_name,),)],
                          calls)
        station_handler.update_associated_stations.assert_called_with(
            {vap2_1.vap_id: [], vap2_2.vap_id: []}, ap2.ap_id)

        # Associated station on the second VAP but not the other.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        sta1_mac = '00:a0:c6:00:22:04'
        sta1_txrate = 77
        sta1_rxrate = 85
        sta1_rssi = 36
        console.run_cmd.side_effect = [
            [],
            self._create_tagged_sta_report(sta1_mac, sta1_txrate,
                                           sta1_rxrate, sta1_rssi)
        ]

        monitor.sample_conditions(console)
        calls = console.run_cmd.call_args_list
        self.assertEquals([(("wlanconfig %s list" % vap2_1.iface_name,),),
                           (("wlanconfig %s list" % vap2_2.iface_name,),)],
                          calls)
        station_handler.update_associated_stations.assert_called_with(
            {vap2_1.vap_id: [],
             vap2_2.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate)]}, ap2.ap_id)

        # Associated stations on both VAPs.
        self._reset_mock_console(console)
        self._reset_mock_station_handler(station_handler)

        sta1_mac = '00:a0:c6:00:22:14'
        sta1_txrate = 15
        sta1_rxrate = 32
        sta1_rssi = 17

        sta2_mac = '00:a0:c6:00:22:24'
        sta2_txrate = 27
        sta2_rxrate = 16
        sta2_rssi = 16

        sta3_mac = '00:a0:c6:00:22:34'
        sta3_txrate = 155
        sta3_rxrate = 89
        sta3_rssi = 15

        sta4_mac = '00:a0:c6:00:22:44'
        sta4_txrate = 305
        sta4_rxrate = 343
        sta4_rssi = 28

        sta5_mac = '00:a0:c6:00:22:54'
        sta5_txrate = 788
        sta5_rxrate = 800
        sta5_rssi = 27

        sta6_mac = '00:a0:c6:00:22:64'
        sta6_txrate = 450
        sta6_rxrate = 420
        sta6_rssi = 26

        sta7_mac = '00:a0:c6:00:22:74'
        sta7_txrate = 47
        sta7_rxrate = 16
        sta7_rssi = 25

        console.run_cmd.side_effect = [
            self._create_tagged_sta_report(sta1_mac, sta1_txrate,
                                           sta1_rxrate, sta1_rssi) +
            self._create_tagged_sta_report(sta2_mac, sta2_txrate,
                                           sta2_rxrate, sta2_rssi) +
            self._create_tagged_sta_report(sta3_mac, sta3_txrate,
                                           sta3_rxrate, sta3_rssi),
            self._create_tagged_sta_report(sta4_mac, sta4_txrate,
                                           sta4_rxrate, sta4_rssi) +
            self._create_tagged_sta_report(sta5_mac, sta5_txrate,
                                           sta5_rxrate, sta5_rssi) +
            self._create_tagged_sta_report(sta6_mac, sta6_txrate,
                                           sta6_rxrate, sta6_rssi) +
            self._create_tagged_sta_report(sta7_mac, sta7_txrate,
                                           sta7_rxrate, sta7_rssi)
        ]

        monitor.sample_conditions(console)
        calls = console.run_cmd.call_args_list
        self.assertEquals([(("wlanconfig %s list" % vap2_1.iface_name,),),
                           (("wlanconfig %s list" % vap2_2.iface_name,),)],
                          calls)
        station_handler.update_associated_stations.assert_called_with(
            {vap2_1.vap_id: [(sta1_mac, sta1_rssi, sta1_txrate),
                             (sta2_mac, sta2_rssi, sta2_txrate),
                             (sta3_mac, sta3_rssi, sta3_txrate)],
             vap2_2.vap_id: [(sta4_mac, sta4_rssi, sta4_txrate),
                             (sta5_mac, sta5_rssi, sta5_txrate),
                             (sta6_mac, sta6_rssi, sta6_txrate),
                             (sta7_mac, sta7_rssi, sta7_txrate)]},
            ap2.ap_id)

    def test_utilization(self):
        """Verify the functionality of :class:`UtilizationsMonitor`."""

        IWCONFIG_CMD = 'iwconfig %s | grep Frequency'
        SETCHANLIST_CMD = 'wifitool %s setchanlist %d'
        IWPRIV_ACSREPORT_CMD = 'iwpriv %s acsreport 1'
        WIFITOOL_ACSREPORT_CMD = 'wifitool %s acsreport | grep %d'
        IWCONFIG_OUTPUT = '          Mode:Master  Frequency:%f GHz  AccessPoint: %s    '
        ACSREPORT_OUTPUT = ' %d(  %d)    0        0         0   -105     ' + \
                           '%d           0          0   '

        utilization_handler = self._create_mock_utilization_handler()
        console = self._create_mock_console()
        ap1 = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')

        ###############################################################
        # Monitoring single VAP.
        ###############################################################

        vap1 = VirtualAccessPoint(ap1, 'device1-2g', BAND_TYPE.BAND_24G,
                                  'ath0', 'wifi1', 1, CHANNEL_GROUP.CG_24G,
                                  80)
        monitor = UtilizationsMonitor(ap1, utilization_handler)

        # Empty output, may happen if the command is executed after console
        # shutdown, make sure it does not crash
        console.run_cmd.return_value = []
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(IWCONFIG_CMD % vap1.iface_name)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 1st sample: Invalid iwconfig output, should not generate event
        console.run_cmd.return_value = ["Some random string"]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(IWCONFIG_CMD % vap1.iface_name)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 2nd sample: Valid iwconfig output, should not generate event
        freq = 2.462
        chan = 11
        expected_util = 1
        iwconfig_output = [IWCONFIG_OUTPUT % (freq, '00:11:22:33:44:55')]
        console.run_cmd.side_effect = [iwconfig_output, [], []]
        monitor.sample_conditions(console)
        expected = [call(IWCONFIG_CMD % vap1.iface_name),
                    call(SETCHANLIST_CMD % (vap1.iface_name, chan)),
                    call(IWPRIV_ACSREPORT_CMD % vap1.iface_name)]
        self.assert_(console.run_cmd.call_args_list == expected)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 3rd-5th samples: Try read acsreport three times, all return invalid acsreport output,
        #                  should not generate event
        for i in [3, 4, 5]:
            console.run_cmd.side_effect = [['Some random string']]
            monitor.sample_conditions(console)
            console.run_cmd.assert_called_once_with(
                WIFITOOL_ACSREPORT_CMD % (vap1.iface_name, int(freq * 1000))
            )
            self.assert_(not utilization_handler.update_utilizations.called)
            console.run_cmd.reset_mock()

        # 6th sample: Valid iwconfig output, should not generate event
        console.run_cmd.side_effect = [iwconfig_output, [], []]
        expected = [call(IWCONFIG_CMD % vap1.iface_name),
                    call(SETCHANLIST_CMD % (vap1.iface_name, chan)),
                    call(IWPRIV_ACSREPORT_CMD % vap1.iface_name)]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 7th sample: Invalid utilization value, should not be reported
        invalid_util = 101
        acsreport_output = [
            ACSREPORT_OUTPUT % (int(freq * 1000), chan, invalid_util)
        ]
        console.run_cmd.side_effect = [acsreport_output]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_once_with(
            WIFITOOL_ACSREPORT_CMD % (vap1.iface_name, int(freq * 1000))
        )
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 8th sample: Valid acsreport output, should generate event with the utilization
        expected_util = 15
        acsreport_output = [
            ACSREPORT_OUTPUT % (int(freq * 1000), chan, expected_util)
        ]
        console.run_cmd.side_effect = [acsreport_output]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_once_with(
            WIFITOOL_ACSREPORT_CMD % (vap1.iface_name, int(freq * 1000))
        )
        utilization_handler.update_utilizations.assert_called_once_with(
            {'data': {vap1.vap_id: expected_util}, 'type': 'raw'}, ap1.ap_id
        )

        console.run_cmd.reset_mock()
        utilization_handler.update_utilizations.reset_mock()

        # Special cases: Reset after triggering the acsreport means the next
        #                time sampling is triggered, it starts from scratch.

        # 9th sample: Valid iwconfig output, no event
        expected_util = 27
        # Trigger iwpriv acsreport in first sampling
        console.run_cmd.side_effect = [iwconfig_output, [], []]
        expected = [call(IWCONFIG_CMD % vap1.iface_name),
                    call(SETCHANLIST_CMD % (vap1.iface_name, chan)),
                    call(IWPRIV_ACSREPORT_CMD % vap1.iface_name)]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)
        console.run_cmd.reset_mock()

        monitor.reset()  # should restart utilization monitoring

        # 1st sample (after reset): Valid iwconfig output, no event
        console.run_cmd.side_effect = [iwconfig_output, [], []]
        monitor.sample_conditions(console)
        self.assertEquals(console.run_cmd.call_args_list, expected)
        console.run_cmd.reset_mock()

        # Check wifitool acsreport output in second sampling
        acsreport_output = [
            ACSREPORT_OUTPUT % (int(freq * 1000), chan, expected_util)
        ]
        console.run_cmd.side_effect = [acsreport_output]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_once_with(WIFITOOL_ACSREPORT_CMD %
                                                (vap1.iface_name,
                                                 int(freq * 1000)))
        utilization_handler.update_utilizations.assert_called_once_with(
            {'data': {vap1.vap_id: expected_util}, 'type': 'raw'}, ap1.ap_id)

        console.run_cmd.reset_mock()
        utilization_handler.update_utilizations.reset_mock()

        ###############################################################
        # Monitoring multiple VAPs.
        ###############################################################
        vap2 = VirtualAccessPoint(ap1, 'device1-5g', BAND_TYPE.BAND_5G,
                                  'ath1', 'wifi0', 149, CHANNEL_GROUP.CG_5GH,
                                  75)

        monitor = UtilizationsMonitor(ap1, utilization_handler)

        # 1st sample, invalid iwconfig output on both band, should not generate event
        console.run_cmd.side_effect = [["Some random string 2.4 GHz"], ["Some random string 5 GHz"]]
        expected = [call(IWCONFIG_CMD % vap1.iface_name),
                    call(IWCONFIG_CMD % vap2.iface_name)]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 2nd sample: Invalid iwconfig output on 2.4 GHz band, valid iwconfig output on 5 GHz band,
        #             should not generate event
        iwconfig_output_24g = ["Invalid iwconfig output 2.4 GHz"]
        freq_5g = 5.5
        chan_5g = 100
        iwconfig_output_5g = [IWCONFIG_OUTPUT % (freq_5g, "ff:ee:bb:cc:aa:00")]
        console.run_cmd.side_effect = [iwconfig_output_24g, iwconfig_output_5g, [], []]
        expected = [
            call(IWCONFIG_CMD % vap1.iface_name),
            call(IWCONFIG_CMD % vap2.iface_name),
            call(SETCHANLIST_CMD % (vap2.iface_name, chan_5g)),
            call(IWPRIV_ACSREPORT_CMD % vap2.iface_name)
        ]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 3rd sample: Valid iwconfig output on 2.4 GHz band, valid acsreport on 5 GHz band,
        #             should generate event with 5 GHz utilization
        freq_24g = 2.462
        chan_24g = 11
        expected_util_5g = 20
        iwconfig_output_24g = [IWCONFIG_OUTPUT % (freq_24g, "00:11:22:33:44:55")]
        acsreport_output_5g = [
            ACSREPORT_OUTPUT % (int(freq_5g * 1000), chan_5g, expected_util_5g)
        ]
        console.run_cmd.side_effect = [iwconfig_output_24g, [], [], acsreport_output_5g]
        expected = [
            call(IWCONFIG_CMD % vap1.iface_name),
            call(SETCHANLIST_CMD % (vap1.iface_name, chan_24g)),
            call(IWPRIV_ACSREPORT_CMD % vap1.iface_name),
            call(WIFITOOL_ACSREPORT_CMD % (vap2.iface_name, int(freq_5g * 1000)))
        ]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)

        utilization_handler.update_utilizations.assert_called_once_with(
            {'data': {vap2.vap_id: expected_util_5g}, 'type': 'raw'}, ap1.ap_id
        )

        console.run_cmd.reset_mock()
        utilization_handler.update_utilizations.reset_mock()

        # 4th sample: Invalid acsreport output on 2.4 GHz band, invalid
        #             iwconfig output on 5 GHz band, should not generate event
        acsreport_output_24g = [' Invalid acsreport output 2.4 GHz']
        iwconfig_output_5g = ["Invalid iwconfig output 5 GHz"]
        console.run_cmd.side_effect = [acsreport_output_24g, iwconfig_output_5g]
        expected = [
            call(WIFITOOL_ACSREPORT_CMD % (vap1.iface_name, int(freq_24g * 1000))),
            call(IWCONFIG_CMD % vap2.iface_name)
        ]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 5th sample: Invalid acsreport output on 2.4 GHz band, valid iwconfig
        #             output on 5 GHz band, should not generate event
        iwconfig_output_5g = [IWCONFIG_OUTPUT % (freq_5g, "ff:ee:bb:cc:aa:00")]
        console.run_cmd.side_effect = [acsreport_output_24g, iwconfig_output_5g, [], []]
        expected = [
            call(WIFITOOL_ACSREPORT_CMD % (vap1.iface_name, int(freq_24g * 1000))),
            call(IWCONFIG_CMD % vap2.iface_name),
            call(SETCHANLIST_CMD % (vap2.iface_name, chan_5g)),
            call(IWPRIV_ACSREPORT_CMD % vap2.iface_name)
        ]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)
        self.assert_(not utilization_handler.update_utilizations.called)
        console.run_cmd.reset_mock()

        # 6th sample: Valid acsreport on both bands, should generate event
        #             with utilizations of both bands
        expected_util_24g = 24
        acsreport_output_24g = [
            ACSREPORT_OUTPUT % (int(freq_24g * 1000), chan_24g, expected_util_24g)
        ]
        expected_util_5g = 5
        acsreport_output_5g = [
            ACSREPORT_OUTPUT % (int(freq_5g * 1000), chan_5g, expected_util_5g)
        ]
        console.run_cmd.side_effect = [acsreport_output_24g, acsreport_output_5g]
        expected = [
            call(WIFITOOL_ACSREPORT_CMD % (vap1.iface_name, int(freq_24g * 1000))),
            call(WIFITOOL_ACSREPORT_CMD % (vap2.iface_name, int(freq_5g * 1000)))
        ]
        monitor.sample_conditions(console)
        self.assert_(console.run_cmd.call_args_list == expected)

        utilization_handler.update_utilizations.assert_called_once_with(
            {'data': {vap1.vap_id: expected_util_24g, vap2.vap_id: expected_util_5g},
             'type': 'raw'}, ap1.ap_id
        )

        console.run_cmd.reset_mock()
        utilization_handler.update_utilizations.reset_mock()

    def test_throughput(self):
        """Verify the functionality of :class:`ThroughputsMonitor`."""
        ###############################################################
        # Monitoring single VAP.
        ###############################################################
        throughput_handler = self._create_mock_throughput_handler()
        console = self._create_mock_console()

        ap1 = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')
        vap1_1 = VirtualAccessPoint(ap1, 'device1-2g', BAND_TYPE.BAND_24G,
                                    'ath0', 'wifi0', 1, CHANNEL_GROUP.CG_24G,
                                    75)
        monitor = ThroughputsMonitor(ap1, throughput_handler)

        # Empty output, may happen if the command is executed after console
        # shutdown, make sure it does not crash
        console.run_cmd.return_value = []
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            GET_APSTATS_VAP_TXRX % vap1_1.iface_name)
        self.assert_(not throughput_handler.update_throughputs.called)

        # Invalid RX throughput output, make sure it does not crash
        console.run_cmd.return_value = [self.TX_THROUGHPUT_TEMPLATE % 100,
                                        "bogus RX throughputs"]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            GET_APSTATS_VAP_TXRX % vap1_1.iface_name)
        self.assert_(not throughput_handler.update_throughputs.called)

        # Again with a bogus TX throughput
        console.run_cmd.return_value = ["bogus TX throughput",
                                        self.RX_THROUGHPUT_TEMPLATE % 1000]
        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            GET_APSTATS_VAP_TXRX % vap1_1.iface_name)
        self.assert_(not throughput_handler.update_throughputs.called)

        # No throughput output.
        console.run_cmd.return_value = []

        monitor.sample_conditions(console)
        console.run_cmd.assert_called_with(
            GET_APSTATS_VAP_TXRX % vap1_1.iface_name)
        self.assert_(not throughput_handler.update_throughputs.called)

        current_time = 49
        time_mock = MagicMock(return_value=current_time)
        with patch('time.time', time_mock):
            # First sample, no throughput can be logged.
            sample1 = (7319, 9271)
            console.run_cmd.return_value = \
                [self.TX_THROUGHPUT_TEMPLATE % sample1[0],
                 self.RX_THROUGHPUT_TEMPLATE % sample1[1]]
            monitor.sample_conditions(console)
            console.run_cmd.assert_called_with(
                GET_APSTATS_VAP_TXRX % vap1_1.iface_name)
            self.assert_(not throughput_handler.update_throughputs.called)

            # Second sample results in the difference in bytes being logged
            # as a throughput (accounting for the elapsed time).
            current_time += 2
            elapsed_time = 2
            time_mock.return_value = current_time

            sample2 = (758194, 921847)
            console.run_cmd.return_value = \
                [self.TX_THROUGHPUT_TEMPLATE % sample2[0],
                 self.RX_THROUGHPUT_TEMPLATE % sample2[1]]
            monitor.sample_conditions(console)
            console.run_cmd.assert_called_with(
                GET_APSTATS_VAP_TXRX % vap1_1.iface_name)

            (expected_tx_tput, expected_rx_tput) = \
                (self._compute_tput(sample1[0], sample2[0], elapsed_time),
                 self._compute_tput(sample1[1], sample2[1], elapsed_time))
            throughput_handler.update_throughputs.assert_called_with(
                {vap1_1.vap_id: (expected_tx_tput, expected_rx_tput)},
                vap1_1.ap.ap_id)

            throughput_handler.update_throughputs.reset_mock()

            # After a reset, we require two more samples to get a new
            # throughput.
            monitor.reset()
            sample1 = (7823671, 87471367)
            console.run_cmd.return_value = [
                self.TX_THROUGHPUT_TEMPLATE % sample1[0],
                self.RX_THROUGHPUT_TEMPLATE % sample1[1]
            ]
            monitor.sample_conditions(console)
            console.run_cmd.assert_called_with(
                GET_APSTATS_VAP_TXRX % vap1_1.iface_name)
            self.assert_(not throughput_handler.update_throughputs.called)

            # Second sample results in the difference in bytes being logged
            # as a throughput (accounting for the elapsed time).
            current_time += 3
            elapsed_time = 3
            time_mock.return_value = current_time

            sample2 = (9747167, 97324742)
            console.run_cmd.return_value = [
                self.TX_THROUGHPUT_TEMPLATE % sample2[0],
                self.RX_THROUGHPUT_TEMPLATE % sample2[1]
            ]
            monitor.sample_conditions(console)
            console.run_cmd.assert_called_with(
                GET_APSTATS_VAP_TXRX % vap1_1.iface_name)

            (expected_tx_tput, expected_rx_tput) = \
                (self._compute_tput(sample1[0], sample2[0], elapsed_time),
                 self._compute_tput(sample1[1], sample2[1], elapsed_time))
            throughput_handler.update_throughputs.assert_called_with(
                {vap1_1.vap_id: (expected_tx_tput, expected_rx_tput)},
                vap1_1.ap.ap_id)

        ###############################################################
        # Monitoring multiple VAPs.
        ###############################################################
        throughput_handler = self._create_mock_throughput_handler()
        console = self._create_mock_console()

        ap2 = AccessPoint('device1', AP_TYPE.RE, '11:11:11:00:00:01')
        vap2_1 = VirtualAccessPoint(ap2, 'device2-5g', BAND_TYPE.BAND_5G,
                                    'ath2', 'wifi1', 149, CHANNEL_GROUP.CG_5GH,
                                    75)
        vap2_2 = VirtualAccessPoint(ap2, 'device2-24g', BAND_TYPE.BAND_24G,
                                    'ath1', 'wifi0', 1, CHANNEL_GROUP.CG_24G,
                                    80)
        monitor = ThroughputsMonitor(ap2, throughput_handler)

        current_time = 77
        time_mock = MagicMock(return_value=current_time)
        with patch('time.time', time_mock):
            # First sample, no throughput can be logged.
            sample1_1 = (1259, 731)
            sample1_2 = (7438, 7571)
            console.run_cmd.side_effect = [
                [self.TX_THROUGHPUT_TEMPLATE % sample1_1[0],
                 self.RX_THROUGHPUT_TEMPLATE % sample1_1[1]],
                [self.TX_THROUGHPUT_TEMPLATE % sample1_2[0],
                 self.RX_THROUGHPUT_TEMPLATE % sample1_2[1]]]
            monitor.sample_conditions(console)

            expected = [
                call(GET_APSTATS_VAP_TXRX % vap2_1.iface_name),
                call(GET_APSTATS_VAP_TXRX % vap2_2.iface_name)
            ]
            self.assert_(console.run_cmd.call_args_list == expected)
            self.assert_(not throughput_handler.update_throughputs.called)
            console.run_cmd.reset_mock()

            # Second sample results in the difference in bytes being logged
            # as a throughput (accounting for the elapsed time).
            current_time += 4
            elapsed_time = 4
            time_mock.return_value = current_time

            sample2_1 = (8743, 1287)
            sample2_2 = (84327, 98173)
            console.run_cmd.side_effect = [
                [self.TX_THROUGHPUT_TEMPLATE % sample2_1[0],
                 self.RX_THROUGHPUT_TEMPLATE % sample2_1[1]],
                [self.TX_THROUGHPUT_TEMPLATE % sample2_2[0],
                 self.RX_THROUGHPUT_TEMPLATE % sample2_2[1]]]

            monitor.sample_conditions(console)
            self.assert_(console.run_cmd.call_args_list == expected)

            (expected_tx_tput_1, expected_rx_tput_1) = \
                (self._compute_tput(sample1_1[0], sample2_1[0], elapsed_time),
                 self._compute_tput(sample1_1[1], sample2_1[1], elapsed_time))
            (expected_tx_tput_2, expected_rx_tput_2) = \
                (self._compute_tput(sample1_2[0], sample2_2[0], elapsed_time),
                 self._compute_tput(sample1_2[1], sample2_2[1], elapsed_time))
            throughput_handler.update_throughputs.assert_called_with(
                {vap2_1.vap_id: (expected_tx_tput_1, expected_rx_tput_1),
                 vap2_2.vap_id: (expected_tx_tput_2, expected_rx_tput_2)},
                vap2_1.ap.ap_id)

            # For sake of brevity, another test is not done as it would be
            # identical to the single VAP case.

    def _compute_tput(self, sample1, sample2, elapsed_secs):
        # Throughput is expressed in bytes / second here.
        return (sample2 - sample1) / elapsed_secs

    def _create_he_s_row(self, row):
        """ Create an output row from the 'he s' command from the template"""
        return self.HE_S_ROW_TEMPLATE.format(hash=row[0], da=row[1], id=row[2],
                                             rate=row[3], intf=row[4], priority=row[5],
                                             sub_type=row[6])

    def _create_hy_hd_row(self, row):
        """ Create an output row from the 'hy hd' command from the template"""
        return self.HY_HD_ROW_TEMPLATE.format(da=row[0], id=row[1],
                                              intf_udp=row[2], intf_other=row[3])

    def _check_diaglog_calls(self, diaglog_handler):
        """Check the diaglog output is correct.

        Note this is hard-coded to the provided inputs (so would need to extend if
        more logs are to be tested)

        Args:
            diaglog_handler (mocked out :class:`MsgHandler`): diaglog handler
        """
        expected_hactive_log = '\x00 \x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00x00\x00' \
            'x\x00\x00\x00\x00\x00\x02\x11\x11\x11\x00\x00\x02\x00\x00' \
            '\x00\x00\x00\x07\xa1!\x11\x005\x00\x04ath1'

        expected_hdefault_log = '\x02\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00x00\x00' \
            'x\x00\x00\x00\x00\x00\x03\x11\x11\x11\x00\x00\x0355\x00' \
            '\x05ath01\x00\x04ath3\x00\x00\x00\x00\x00\x04\x11\x11\x11' \
            '\x00\x00\x0455\x00\x05ath02\x00\x04ath4'

        expected_hactive_output = call([None, 0], expected_hactive_log)
        expected_hdefault_output = call([None, 0], expected_hdefault_log)

        # Should be one call each for H-Active and H-Default
        self.assert_(len(diaglog_handler.process_msg.mock_calls) == 2)
        self.assert_(diaglog_handler.process_msg.mock_calls[0] == expected_hactive_output)
        self.assert_(diaglog_handler.process_msg.mock_calls[1] == expected_hdefault_output)

    def test_paths(self):
        """Verify the functionality of :class:`PathMonitor`."""
        diaglog_handler = self._create_mock_diaglog_handler()
        console = self._create_mock_console()

        ap1 = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')
        monitor = PathMonitor(ap1, diaglog_handler, 500000)

        # Empty output, may happen if the command is executed after console
        # shutdown, make sure it does not crash
        console.run_cmd.return_value = []
        monitor.sample_conditions(console)

        self.assert_(len(console.run_cmd.mock_calls) == 2)
        self.assert_(console.run_cmd.mock_calls[0] == call.run_cmd('he s'))
        self.assert_(console.run_cmd.mock_calls[1] == call.run_cmd('hy hd'))
        self.assert_(not diaglog_handler.process_msg.called)

        console.run_cmd.reset_mock()

        # Invalid path output, make sure it does not crash
        console.run_cmd.return_value = ["bogus paths"]
        monitor.sample_conditions(console)

        self.assert_(len(console.run_cmd.mock_calls) == 2)
        self.assert_(console.run_cmd.mock_calls[0] == call.run_cmd('he s'))
        self.assert_(console.run_cmd.mock_calls[1] == call.run_cmd('hy hd'))
        self.assert_(not diaglog_handler.process_msg.called)

        console.run_cmd.reset_mock()

        # Test with headers only for both commands, no data
        console.run_cmd.side_effect = [self.HE_S_HEADER_TEMPLATE, self.HY_HD_HEADER_TEMPLATE]
        monitor.sample_conditions(console)

        self.assert_(len(console.run_cmd.mock_calls) == 2)
        self.assert_(console.run_cmd.mock_calls[0] == call.run_cmd('he s'))
        self.assert_(console.run_cmd.mock_calls[1] == call.run_cmd('hy hd'))
        self.assert_(not diaglog_handler.process_msg.called)

        console.run_cmd.reset_mock()

        # Test with 2 rows of valid data each
        # Note first row will be ignored since rate is less than 0.5Mbps
        ha_1 = ("0x10", "00:00:0:00:00:01", "11:11:11:00:00:01", "1000", "ath0", "0x80",
                "U----")
        ha_2 = ("0x11", "00:00:0:00:00:02", "11:11:11:00:00:02", "500001", "ath1", "0x00",
                "O----")
        hd_1 = ("00:00:0:00:00:03", "11:11:11:00:00:03", "ath01", "ath3")
        hd_2 = ("00:00:0:00:00:04", "11:11:11:00:00:04", "ath02", "ath4")
        he_s_side_effect = self.HE_S_HEADER_TEMPLATE.split('\n')
        he_s_side_effect += [self._create_he_s_row(ha_1), self._create_he_s_row(ha_2)]
        hy_hd_side_effect = self.HY_HD_HEADER_TEMPLATE.split('\n')
        hy_hd_side_effect += [self._create_hy_hd_row(hd_1), self._create_hy_hd_row(hd_2)]
        console.run_cmd.side_effect = [he_s_side_effect, hy_hd_side_effect]

        monitor.sample_conditions(console)

        self.assert_(len(console.run_cmd.mock_calls) == 2)
        self.assert_(console.run_cmd.mock_calls[0] == call.run_cmd('he s'))
        self.assert_(console.run_cmd.mock_calls[1] == call.run_cmd('hy hd'))
        self._check_diaglog_calls(diaglog_handler)


if __name__ == '__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)

    unittest.main()
