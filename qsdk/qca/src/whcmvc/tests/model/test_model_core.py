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
import os
import copy
from mock import MagicMock, patch
import time

from whcdiag.constants import BAND_TYPE, UTIL_AGGR_SECS

from whcmvc.model import config
from whcmvc.model.model_core import RSSIMapper, RSSI_LEVEL
from whcmvc.model.model_core import ClientDevice, ServedSTAMetrics, TrackedQuantity
from whcmvc.model.model_core import AccessPoint, VirtualAccessPoint, Path
from whcmvc.model.model_core import DeviceDB, ModelBase, ActiveModel
from whcmvc.model.model_core import TRAFFIC_CLASS_TYPE, CLIENT_TYPE, AP_TYPE, CHANNEL_GROUP
from whcdiag.msgs.steerexec import BTMComplianceType, SteeringProhibitType
from whcdiag.hydmsgs.heservice import HActiveRow
from whcdiag.hydmsgs.pstables import HDefaultRow
from whcdiag.constants import TRANSPORT_PROTOCOL


class TestModelCore(unittest.TestCase):

    def _verify_level_mapping(self, mapper):
        self.assertEquals(RSSI_LEVEL.HIGH, mapper.get_level(mapper.rssi_high + 1))
        self.assertEquals(RSSI_LEVEL.HIGH, mapper.get_level(mapper.rssi_high))

        self.assertEquals(RSSI_LEVEL.MODERATE, mapper.get_level(mapper.rssi_high - 1))
        self.assertEquals(RSSI_LEVEL.MODERATE, mapper.get_level(mapper.rssi_moderate))

        self.assertEquals(RSSI_LEVEL.LOW, mapper.get_level(mapper.rssi_moderate - 1))
        self.assertEquals(RSSI_LEVEL.LOW, mapper.get_level(mapper.rssi_low))

        self.assertEquals(RSSI_LEVEL.WEAK, mapper.get_level(mapper.rssi_low - 1))
        self.assertEquals(RSSI_LEVEL.NONE, mapper.get_level(None))

    def test_rssi_mapper(self):
        """Verify basic functioning of :class:`RSSIMapper`."""
        mapper = RSSIMapper()

        # Check that the defaults are used.
        self.assertEquals(mapper.HIGH_RSSI_THRESH, mapper.rssi_high)
        self.assertEquals(mapper.MODERATE_RSSI_THRESH, mapper.rssi_moderate)
        self.assertEquals(mapper.LOW_RSSI_THRESH, mapper.rssi_low)

        # Verify the default mapping.
        self._verify_level_mapping(mapper)

        # Now update the mapping with valid ones
        mapper.update_levels({RSSI_LEVEL.HIGH: mapper.HIGH_RSSI_THRESH + 2})
        self.assertEquals(mapper.HIGH_RSSI_THRESH + 2, mapper.rssi_high)
        self.assertEquals(mapper.MODERATE_RSSI_THRESH, mapper.rssi_moderate)
        self.assertEquals(mapper.LOW_RSSI_THRESH, mapper.rssi_low)
        self._verify_level_mapping(mapper)

        # Update with all values specified
        mapper.update_levels({RSSI_LEVEL.HIGH: mapper.HIGH_RSSI_THRESH + 3,
                              RSSI_LEVEL.MODERATE: mapper.MODERATE_RSSI_THRESH - 1,
                              RSSI_LEVEL.LOW: mapper.LOW_RSSI_THRESH - 4})
        self.assertEquals(mapper.HIGH_RSSI_THRESH + 3, mapper.rssi_high)
        self.assertEquals(mapper.MODERATE_RSSI_THRESH - 1, mapper.rssi_moderate)
        self.assertEquals(mapper.LOW_RSSI_THRESH - 4, mapper.rssi_low)
        self._verify_level_mapping(mapper)

        # Verify invalid settings generate an exception.
        # Test #1: Low is greater than moderate
        self.assertRaises(ValueError, mapper.update_levels,
                          {RSSI_LEVEL.HIGH: mapper.HIGH_RSSI_THRESH + 3,
                           RSSI_LEVEL.MODERATE: mapper.MODERATE_RSSI_THRESH,
                           RSSI_LEVEL.LOW: mapper.MODERATE_RSSI_THRESH + 1})

        # Settings are unchanged
        self.assertEquals(mapper.HIGH_RSSI_THRESH + 3, mapper.rssi_high)
        self.assertEquals(mapper.MODERATE_RSSI_THRESH - 1, mapper.rssi_moderate)
        self.assertEquals(mapper.LOW_RSSI_THRESH - 4, mapper.rssi_low)
        self._verify_level_mapping(mapper)

        # Test #2: Moderate is greater than high
        self.assertRaises(ValueError, mapper.update_levels,
                          {RSSI_LEVEL.HIGH: mapper.HIGH_RSSI_THRESH,
                           RSSI_LEVEL.MODERATE: mapper.HIGH_RSSI_THRESH + 1,
                           RSSI_LEVEL.LOW: mapper.LOW_RSSI_THRESH + 1})

        # Settings are unchanged
        self.assertEquals(mapper.HIGH_RSSI_THRESH + 3, mapper.rssi_high)
        self.assertEquals(mapper.MODERATE_RSSI_THRESH - 1, mapper.rssi_moderate)
        self.assertEquals(mapper.LOW_RSSI_THRESH - 4, mapper.rssi_low)
        self._verify_level_mapping(mapper)

    def test_client_device(self):
        """Verify basic functioning of :class:`ClientDevice`."""
        name = 'client1'
        mac_addr = '00:11:22:33:44:55'
        traffic_class = TRAFFIC_CLASS_TYPE.STREAMING
        client_type = CLIENT_TYPE.LAPTOP
        dev1 = ClientDevice(name, mac_addr, traffic_class, client_type)
        self.assertEquals(name, dev1.name)
        self.assertEquals(mac_addr, dev1.mac_addr)
        self.assertEquals(traffic_class, dev1.traffic_class)
        self.assertEquals(client_type, dev1.client_type)

        # Default values for the flags
        self.assertEquals(False, dev1.steering_pending_vap)
        self.assertEquals(False, dev1.is_steered)
        self.assert_(not dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)
        self.assert_(not dev1.is_polluted)

        # This value is opaque to ClientDevice, so we just use a dummy value.
        dummy_ap1 = 123
        dev1.update_serving_vap(dummy_ap1)
        self.assertEquals(dummy_ap1, dev1.serving_vap)

        dummy_ap2 = 456
        dev1.update_serving_vap(dummy_ap2)
        self.assertEquals(dummy_ap2, dev1.serving_vap)

        name = 'client2'
        mac_addr = '01:21:22:37:49:5D'
        traffic_class = TRAFFIC_CLASS_TYPE.INTERACTIVE
        dev2 = ClientDevice(name, mac_addr, traffic_class)
        self.assertEquals(name, dev2.name)
        self.assertEquals(mac_addr.lower(), dev2.mac_addr)
        self.assertEquals(traffic_class, dev2.traffic_class)
        self.assertEquals(CLIENT_TYPE.GENERIC, dev2.client_type)

        dev2.update_serving_vap(dummy_ap1)
        self.assertEquals(dummy_ap2, dev1.serving_vap)
        self.assertEquals(dummy_ap1, dev2.serving_vap)

        # Verify update pollution state function
        # Mark polluted on serving VAP
        dev1.update_pollution_state(dummy_ap2, True)
        self.assert_(dev1.is_polluted)

        # Clear polluted on serving VAP
        dev1.update_pollution_state(dummy_ap2, False)
        self.assert_(not dev1.is_polluted)

        # Mark polluted on non serving VAP, not mark STA as polluted yet
        dev1.update_pollution_state(dummy_ap1, True)
        self.assert_(not dev1.is_polluted)

        # dev1 changes its serving VAP, which is marked as polluted
        dev1.update_serving_vap(dummy_ap1)
        self.assertEquals(dummy_ap1, dev1.serving_vap)
        self.assert_(dev1.is_polluted)

        # Mark serving VAP as polluted multiple times, then clear, make sure no duplicate
        for i in range(10):
            dev1.update_pollution_state(dummy_ap1, True)
            self.assert_(dev1.is_polluted)
        dev1.update_pollution_state(dummy_ap1, False)
        self.assert_(not dev1.is_polluted)

        # Verify update_steering_status function
        ap = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')
        vap_24g = VirtualAccessPoint(ap, "vap_24g", BAND_TYPE.BAND_24G, 'ath0',
                                     'wifi0', 1, CHANNEL_GROUP.CG_24G, 70)
        vap_5g = VirtualAccessPoint(ap, "vap_5g", BAND_TYPE.BAND_5G, 'ath1',
                                    'wifi1', 100, CHANNEL_GROUP.CG_5GH, 75)

        dev3 = ClientDevice("dev3", "random mac", traffic_class)
        self.assertEquals(None, dev3.serving_vap)
        self.assertEquals(None, dev3.target_band)
        self.assertEquals(False, dev3.steering_pending_vap)
        self.assertEquals(False, dev3.is_steered)

        # dev3 associated on 2.4 GHz band on its own
        dev3.update_serving_vap(vap_24g)
        self.assertEquals(vap_24g, dev3.serving_vap)
        self.assertEquals(None, dev3.target_band)
        self.assertEquals(False, dev3.steering_pending_vap)
        self.assertEquals(False, dev3.is_steered)

        # dev3 is being steered to 5 GHz
        dev3.update_steering_status(BAND_TYPE.BAND_24G, BAND_TYPE.BAND_5G, None, None)
        self.assertEquals(vap_24g, dev3.serving_vap)
        self.assertEquals(BAND_TYPE.BAND_5G, dev3.target_band)
        self.assertEquals(True, dev3.steering_pending_band)
        self.assertEquals(False, dev3.is_steered)

        # dev3 disassociates on 2.4 GHz, should clear the source band
        dev3.update_serving_vap(None)
        dev3.update_steering_status(None, dev3.target_band, None, None)
        self.assertEquals(None, dev3.serving_vap)
        self.assertEquals(BAND_TYPE.BAND_5G, dev3.target_band)
        self.assertEquals(False, dev3.steering_pending_band)
        self.assertEquals(False, dev3.is_steered)

        # dev3 completes steering by associating on 5 GHz
        dev3.update_serving_vap(vap_5g)
        self.assertEquals(vap_5g, dev3.serving_vap)
        self.assertEquals(BAND_TYPE.BAND_5G, dev3.target_band)
        self.assertEquals(False, dev3.steering_pending_band)
        self.assertEquals(True, dev3.is_steered)

        # === Verify updates to the various flags ===

        # Update just a single value and ensure others are not changed
        dev1.update_flags(is_active=True)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        dev1.update_flags(is_dual_band=False)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        dev1.update_flags(btm_compliance=BTMComplianceType.BTM_COMPLIANCE_IDLE)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_IDLE, dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        dev1.update_flags(prohibit_type=SteeringProhibitType.PROHIBIT_LONG)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_IDLE, dev1.btm_compliance)
        self.assertEquals(SteeringProhibitType.PROHIBIT_LONG, dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        dev1.update_flags(is_unfriendly=True)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_IDLE, dev1.btm_compliance)
        self.assertEquals(SteeringProhibitType.PROHIBIT_LONG, dev1.prohibit_type)
        self.assert_(dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        dev1.update_flags(is_btm_unfriendly=True)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_IDLE, dev1.btm_compliance)
        self.assertEquals(SteeringProhibitType.PROHIBIT_LONG, dev1.prohibit_type)
        self.assert_(dev1.is_unfriendly)
        self.assert_(dev1.is_btm_unfriendly)

        # Update multiple values in one shot
        dev1.update_flags(is_unfriendly=False, is_btm_unfriendly=False,
                          prohibit_type=SteeringProhibitType.PROHIBIT_NONE)
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_IDLE, dev1.btm_compliance)
        self.assertEquals(SteeringProhibitType.PROHIBIT_NONE, dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        dev1.update_flags(is_active=False, is_dual_band=True,
                          btm_compliance=BTMComplianceType.BTM_COMPLIANCE_ACTIVE)
        self.assert_(not dev1.is_active)
        self.assert_(dev1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_ACTIVE, dev1.btm_compliance)
        self.assertEquals(SteeringProhibitType.PROHIBIT_NONE, dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        # Resetting flags should put them all back to None (which will look
        # like a false value).
        dev1.reset_flags()
        self.assert_(not dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)
        self.assert_(not dev1.is_polluted)

    def test_served_sta_metrics(self):
        """Verify basic functioning of :class:`ServedSTAMetrics`."""
        metrics = ServedSTAMetrics()
        self.assertEquals(None, metrics.rssi)
        self.assertEquals(None, metrics.tput)
        self.assertEquals(None, metrics.rate)
        self.assertEquals(None, metrics.airtime)
        self.assertEquals(0, metrics.data_seq)

        metrics.update_rssi(32, RSSI_LEVEL.HIGH)
        self.assertEquals(32, metrics.rssi)
        self.assertEquals(RSSI_LEVEL.HIGH, metrics.rssi_level)
        self.assertEquals(1, metrics.rssi_seq)
        self.assertEquals(0, metrics.data_seq)

        metrics.update_rssi(20, RSSI_LEVEL.LOW)
        self.assertEquals(20, metrics.rssi)
        self.assertEquals(RSSI_LEVEL.LOW, metrics.rssi_level)
        self.assertEquals(2, metrics.rssi_seq)
        self.assertEquals(0, metrics.data_seq)

        metrics.update_data_metrics(15, 130, 11)
        self.assertEquals(15, metrics.tput)
        self.assertEquals(130, metrics.rate)
        self.assertEquals(11, metrics.airtime)
        self.assertEquals(2, metrics.rssi_seq)
        self.assertEquals(1, metrics.data_seq)

        metrics.update_data_metrics(37, 95, 32)
        self.assertEquals(37, metrics.tput)
        self.assertEquals(95, metrics.rate)
        self.assertEquals(32, metrics.airtime)
        self.assertEquals(2, metrics.rssi_seq)
        self.assertEquals(2, metrics.data_seq)

    def test_access_point(self):
        """Verify basic functioning of :class:`AccessPoint`."""
        ap1_id = 'deviceX'
        mac_addr1 = '11:11:11:00:00:01'
        ap1 = AccessPoint(ap1_id, AP_TYPE.CAP, mac_addr1)
        self.assertEquals(ap1_id, ap1.ap_id)
        self.assertEquals(AP_TYPE.CAP, ap1.ap_type)
        self.assertEquals(mac_addr1, ap1.mac_address)
        self.assertEquals([], ap1.get_vaps())

        # Add a dummy object as a VAP. For now, it does not need to
        # be a real VirtualAccessPoint object.
        vap1_1 = 101
        ap1.add_vap(vap1_1)
        self.assertEquals([vap1_1], ap1.get_vaps())

        vap1_2 = 1002
        ap1.add_vap(vap1_2)
        self.assertEquals([vap1_1, vap1_2], ap1.get_vaps())

        # Separate AP device does not share any of the VAPs.
        ap2_id = 'deviceY'
        mac_addr2 = '11:11:11:00:00:02'
        ap2 = AccessPoint(ap2_id, AP_TYPE.RE, mac_addr2)
        self.assertEquals(ap2_id, ap2.ap_id)
        self.assertEquals(AP_TYPE.RE, ap2.ap_type)
        self.assertEquals(mac_addr2, ap2.mac_address)
        self.assertEquals([], ap2.get_vaps())

        vap2_1 = 21
        vap2_2 = 222
        ap2.add_vap(vap2_1)
        ap2.add_vap(vap2_2)
        self.assertEquals([vap2_1, vap2_2], ap2.get_vaps())

        # Create some real VAPs: 3 * 2.4G VAP, 1 * 5G VAP
        ap = AccessPoint('deviceZ', AP_TYPE.CAP, '11:11:11:00:00:03')
        self.assertEquals([], ap.get_vaps())
        self.assertEquals(None, ap.get_vap(BAND_TYPE.BAND_5G))
        self.assertEquals(None, ap.get_vap(BAND_TYPE.BAND_24G))
        self.assertEquals(None, ap.get_vap_by_channel(1))
        self.assertEquals(None, ap.get_vap_by_channel(100))

        vap1 = VirtualAccessPoint(ap1, '5g_1', BAND_TYPE.BAND_5G, 'ath0',
                                  'wifi0', 100, CHANNEL_GROUP.CG_5GH, 80)
        vap2 = VirtualAccessPoint(ap1, '2g_1', BAND_TYPE.BAND_24G, 'ath1',
                                  'wifi1', 1, CHANNEL_GROUP.CG_24G, 70)
        vap3 = VirtualAccessPoint(ap1, '2g_2', BAND_TYPE.BAND_24G, 'ath2',
                                  'wifi1', 11, CHANNEL_GROUP.CG_24G, 75)
        vap4 = VirtualAccessPoint(ap1, '2g_3', BAND_TYPE.BAND_24G, 'ath3',
                                  'wifi1', 6, CHANNEL_GROUP.CG_24G, 65)
        ap.add_vap(vap1)
        ap.add_vap(vap2)
        ap.add_vap(vap3)
        ap.add_vap(vap4)

        self.assertEquals([vap1, vap2, vap3, vap4], ap.get_vaps())
        self.assertEquals(vap1, ap.get_vap(BAND_TYPE.BAND_5G))
        self.assertEquals(vap1, ap.get_vap_by_channel(100))
        # For multiple VAPs on the same band, return the first one
        self.assertEquals(vap2, ap.get_vap(BAND_TYPE.BAND_24G))
        self.assertEquals(vap2, ap.get_vap_by_channel(1))

        # No VAP for invalid band or channel
        self.assertEquals(None, ap.get_vap(BAND_TYPE.BAND_INVALID))
        self.assertEquals(None, ap.get_vap_by_channel(-1))

        # Update the blackout status
        self.assert_(not ap.steering_blackout)
        ap.update_steering_blackout(True)
        self.assert_(ap.steering_blackout)
        ap.update_steering_blackout(False)
        self.assert_(not ap.steering_blackout)

    def test_virtual_access_point(self):
        """Verify basic functioning of :class:`VirtualAccessPoint`."""
        rssi_mapper = RSSIMapper()

        ap = AccessPoint('device1', AP_TYPE.CAP, '11:11:11:00:00:01')
        vap_id = 'device1-2g'
        band = BAND_TYPE.BAND_24G
        iface_name = 'ath0'
        radio_iface = 'wifi1'
        channel = 1
        channel_group = CHANNEL_GROUP.CG_24G
        overload_threshold = 70
        vap1 = VirtualAccessPoint(ap, vap_id, band, iface_name, radio_iface,
                                  channel, channel_group, overload_threshold)
        self.assertEquals(vap_id, vap1.vap_id)
        self.assertEquals(band, vap1.band)
        self.assertEquals(iface_name, vap1.iface_name)
        self.assertEquals(radio_iface, vap1.radio_iface)
        self.assertEquals(channel, vap1.channel)
        self.assertEquals(channel_group, vap1.channel_group)
        self.assertEquals(overload_threshold, vap1.overload_threshold)
        self.assertEquals([vap1], ap.get_vaps())

        # Empty associated stations by default.
        self.assertEquals([], vap1.get_associated_stations())

        vap1_assoc_stas = [('00:01:02:03:04:05',
                            ServedSTAMetrics().update_rssi(32, RSSI_LEVEL.HIGH)),
                           ('00:01:02:03:44:55',
                            ServedSTAMetrics().update_rssi(20, RSSI_LEVEL.LOW))]
        vap1.update_associated_stations(map(lambda x: (x[0], x[1].rssi,
                                                       x[1].rate),
                                            vap1_assoc_stas), rssi_mapper)
        self.assertEquals(vap1_assoc_stas, vap1.get_associated_stations())

        vap_id = 'device2-5g'
        band = BAND_TYPE.BAND_5G
        iface_name = 'ath4'
        radio_iface = 'wifi0'
        channel = 100
        channel_group = CHANNEL_GROUP.CG_5GH
        overload_threshold = 83
        vap2 = VirtualAccessPoint(ap, vap_id, band, iface_name, radio_iface,
                                  channel, channel_group, overload_threshold)
        self.assertEquals(vap_id, vap2.vap_id)
        self.assertEquals(band, vap2.band)
        self.assertEquals(iface_name, vap2.iface_name)
        self.assertEquals(radio_iface, vap2.radio_iface)
        self.assertEquals(channel, vap2.channel)
        self.assertEquals(channel_group, vap2.channel_group)
        self.assertEquals(overload_threshold, vap2.overload_threshold)
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([vap1, vap2], ap.get_vaps())

        vap2_assoc_stas = [('22:21:22:03:04:05',
                            ServedSTAMetrics().update_rssi(16, RSSI_LEVEL.LOW)),
                           ('22:21:22:03:44:55',
                            ServedSTAMetrics().update_rssi(39, RSSI_LEVEL.HIGH))]
        vap2.update_associated_stations(map(lambda x: (x[0], x[1].rssi,
                                                       x[1].rate),
                                            vap2_assoc_stas), rssi_mapper)
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # unchanged
        self.assertEquals(vap1_assoc_stas, vap1.get_associated_stations())

        # Verify the list is copied internally and a copy of it is returned.
        vap1_assoc_stas_orig = list(vap1_assoc_stas)
        vap1_assoc_stas[1] = ('11:11:11:00:00:00',
                              ServedSTAMetrics().update_rssi(10, RSSI_LEVEL.WEAK))
        self.assertEquals(vap1_assoc_stas_orig, vap1.get_associated_stations())

        sta_list = vap1.get_associated_stations()
        sta_list[0] = ('AA:BB:CC:DD:EE:FF',
                       ServedSTAMetrics().update_rssi(22, RSSI_LEVEL.MODERATE))
        self.assertEquals(vap1_assoc_stas_orig, vap1.get_associated_stations())

        # unchanged by the above
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Add two new associated stations
        new_sta_mac_1 = '11:22:33:44:55:66'
        new_sta_mac_2 = 'ff:ee:dd:bb:aa:00'
        vap2.add_associated_station(new_sta_mac_1)
        vap2.add_associated_station(new_sta_mac_2)

        vap2_assoc_stas.append((new_sta_mac_1, ServedSTAMetrics()))
        vap2_assoc_stas.append((new_sta_mac_2, ServedSTAMetrics()))
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Cannot add an existing station
        vap2.add_associated_station(new_sta_mac_1)
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Update RSSI of associated stations
        rssi_1 = 50
        vap2.update_associated_station_rssi(new_sta_mac_1, rssi_1, RSSI_LEVEL.HIGH)

        index = vap2_assoc_stas.index((new_sta_mac_1, ServedSTAMetrics()))
        vap2_assoc_stas[index][1].update_rssi(rssi_1, RSSI_LEVEL.HIGH)
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Update data metrics of associated station
        tput_1 = 27
        rate_1 = 150
        airtime_1 = 16
        vap2.update_associated_station_data_metrics(new_sta_mac_1, tput_1,
                                                    rate_1, airtime_1)

        vap2_assoc_stas[index][1].update_data_metrics(tput_1, rate_1, airtime_1)
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Update activity of associated station
        dev1 = ClientDevice('dev1', new_sta_mac_1, TRAFFIC_CLASS_TYPE.STREAMING)
        self.assert_(vap2.update_associated_station_activity(
            new_sta_mac_1, dev1, is_active=True
        ))
        self.assert_(dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        self.assert_(vap2.update_associated_station_activity(
            new_sta_mac_1, dev1, is_active=False,
        ))
        self.assert_(not dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        # No change with an unknown device
        self.assert_(not vap2.update_associated_station_activity(
            new_sta_mac_1, None, is_active=True,
        ))
        self.assert_(not dev1.is_active)
        self.assert_(not dev1.is_dual_band)
        self.assert_(not dev1.btm_compliance)
        self.assert_(not dev1.prohibit_type)
        self.assert_(not dev1.is_unfriendly)
        self.assert_(not dev1.is_btm_unfriendly)

        # Delete an associated station
        self.assertEquals((new_sta_mac_2, ServedSTAMetrics()),
                          vap2.del_associated_station(new_sta_mac_2))

        vap2_assoc_stas.remove((new_sta_mac_2, ServedSTAMetrics()))
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Confirm that updates now fail due to it not being associated.
        self.assert_(not vap2.update_associated_station_rssi(new_sta_mac_2, 10,
                                                             RSSI_LEVEL.WEAK))
        self.assert_(not vap2.update_associated_station_data_metrics(
            new_sta_mac_2, 5, 25, 20))
        self.assert_(not vap2.update_associated_station_activity(
            new_sta_mac_2, dev1, is_active=True,
        ))

        # Cannot delete a non-existing station
        self.assertEquals(None, vap2.del_associated_station(new_sta_mac_2))
        self.assertEquals(vap2_assoc_stas, vap2.get_associated_stations())

        # Update utilization info
        self.assertEquals(None, vap1.utilization_raw.value)
        self.assertEquals(0, vap1.utilization_raw.seq)
        self.assertEquals(None, vap1.utilization)
        self.assertEquals(None, vap1.utilization_avg.value)
        self.assertEquals(0, vap1.utilization_avg.seq)

        vap1.update_utilization_raw(10)
        self.assertEquals(10, vap1.utilization_raw.value)
        self.assertEquals(1, vap1.utilization_raw.seq)
        self.assertEquals(10, vap1.utilization)
        self.assertEquals(None, vap1.utilization_avg.value)
        self.assertEquals(0, vap1.utilization_avg.seq)

        vap1.update_utilization_avg(15)
        self.assertEquals(10, vap1.utilization_raw.value)
        self.assertEquals(1, vap1.utilization_raw.seq)
        self.assertEquals(15, vap1.utilization)
        self.assertEquals(15, vap1.utilization_avg.value)
        self.assertEquals(1, vap1.utilization_avg.seq)

        vap1.update_utilization_raw(22)
        self.assertEquals(22, vap1.utilization_raw.value)
        self.assertEquals(2, vap1.utilization_raw.seq)
        self.assertEquals(22, vap1.utilization)
        self.assertEquals(15, vap1.utilization_avg.value)
        self.assertEquals(1, vap1.utilization_avg.seq)

        # Update overload status
        self.assertEquals(False, vap1.overloaded)
        self.assertEquals(False, vap2.overloaded)
        vap2.update_overload(True)
        self.assertEquals(False, vap1.overloaded)
        self.assertEquals(True, vap2.overloaded)

        # Update throughput
        self.assertEquals(0, vap1.tx_throughput)
        self.assertEquals(0, vap1.rx_throughput)
        vap1.update_throughput((10, 15))
        self.assertEquals(10, vap1.tx_throughput)
        self.assertEquals(15, vap1.rx_throughput)

        self.assertEquals(0, vap2.tx_throughput)
        self.assertEquals(0, vap2.rx_throughput)
        vap2.update_throughput((5.2, 1.7))
        self.assertEquals(5.2, vap2.tx_throughput)
        self.assertEquals(1.7, vap2.rx_throughput)
        self.assertEquals(10, vap1.tx_throughput)  # no impact to first VAP
        self.assertEquals(15, vap1.rx_throughput)

    def _check_paths(self, ap, path_list):
        """Check the set of paths on an AP match the expectation.

        :param class:`AccessPoint` ap: AP to check paths for.
        :param list path_list: List of tuples containing the expected paths.
            Each tuple contains the (da, class:`VirtualAccessPoint`, class:`AccessPoint`,
                                     :class:`VirtualAccessPoint`)
        """
        paths = ap.get_paths()

        self.assertEquals(len(path_list), len(paths))

        for test_path in path_list:
            self.assertTrue(test_path[0] in paths, test_path[0])
            ret_path = paths[test_path[0]]
            self.assertEqual(ret_path.intf, test_path[1], test_path[0])
            self.assertEqual(ret_path.next_hop, test_path[2], test_path[0])
            self.assertEqual(ret_path.active_intf, test_path[3], test_path[0])

    def _make_hactive_table(self, rows):
        """Create a H-Active table represented as a list.

        :param list rows: List of tuples containing the H-Active information.
            Each tuple contains the (da, id, rate, hash, interface)

        Returns a list of NamedTuples of type HActiveRow
        """
        table = []
        for row in rows:
            # None in fields we don't care about in the model
            hactive = HActiveRow(row[0], row[1], None, row[2],
                                 row[3], TRANSPORT_PROTOCOL.OTHER, None, row[4])
            table.append(hactive)

        return table

    def _make_hdefault_table(self, rows):
        """Create a H-Default table represented as a list.

        :param list rows: List of tuples containing the H-Default information.
            Each tuple contains the (da, id, interface)

        Returns a list of NamedTuples of type HDefaultRow
        """
        table = []
        for row in rows:
            # None in fields we don't care about in the model
            hdefault = HDefaultRow(row[0], row[1], None, None, row[2], None)
            table.append(hdefault)

        return table

    def test_device_db(self):
        """Verify the proper functioning of :class:`DeviceDB`."""
        db = DeviceDB()
        self.assertEquals([], db.get_aps())
        self.assertEquals([], db.get_vaps())

        ###############################################################
        # Verify handling of AccessPoint objects.
        ###############################################################
        ap1_mac_addr = '11:11:11:00:00:01'
        ap1 = AccessPoint('device1', AP_TYPE.CAP, ap1_mac_addr)
        db.add_ap(ap1)
        self.assertEquals(ap1, db.get_ap(ap1.ap_id))
        self.assertEquals(None, db.get_ap('deviceX'))

        # Confirm multiple APs can be added
        ap2_mac_addr = '11:11:11:00:00:02'
        ap2 = AccessPoint('device2', AP_TYPE.RE, ap2_mac_addr)
        db.add_ap(ap2)
        self.assertEquals(ap1, db.get_ap(ap1.ap_id))
        self.assertEquals(ap2, db.get_ap(ap2.ap_id))
        self.assertEquals(None, db.get_ap('deviceX'))

        # Verify duplicate registration is rejected
        self.assertRaises(ValueError, db.add_ap, ap1)

        ###############################################################
        # Verify handling of VirtualAccessPoint objects.
        ###############################################################
        vap1_id = 'device1-2g'
        vap1 = VirtualAccessPoint(ap1, vap1_id, BAND_TYPE.BAND_24G, 'ath1',
                                  'wifi1', 1, CHANNEL_GROUP.CG_24G, 70)
        db.add_vap(vap1)
        self.assertEquals(vap1, db.get_vap(vap1.vap_id))
        self.assertEquals(None, db.get_vap('deviceX-60g'))

        vap2_id = 'device2-5g'
        vap2 = VirtualAccessPoint(ap1, vap2_id, BAND_TYPE.BAND_5G, 'ath2',
                                  'wifi0', 100, CHANNEL_GROUP.CG_5GH, 80)
        db.add_vap(vap2)
        self.assertEquals(vap1, db.get_vap(vap1.vap_id))
        self.assertEquals(vap2, db.get_vap(vap2.vap_id))

        # Confirm multiple 2.4 GHz and 5 GHz APs can be added.
        vap3_id = 'device1-5g'
        vap3 = VirtualAccessPoint(ap1, vap3_id, BAND_TYPE.BAND_5G, 'ath3',
                                  'wifi0', 128, CHANNEL_GROUP.CG_5GH, 82)
        db.add_vap(vap3)

        vap4_id = 'device2-2g'
        vap4 = VirtualAccessPoint(ap1, vap4_id, BAND_TYPE.BAND_24G, 'ath0',
                                  'wifi1', 6, CHANNEL_GROUP.CG_24G, 75)
        db.add_vap(vap4)

        self.assertEquals(vap1, db.get_vap(vap1.vap_id))
        self.assertEquals(vap2, db.get_vap(vap2.vap_id))
        self.assertEquals(vap3, db.get_vap(vap3.vap_id))
        self.assertEquals(vap4, db.get_vap(vap4.vap_id))

        # Verify APs that already have VAPs can also be added.
        ap3 = AccessPoint('device3', AP_TYPE.RE, '11:11:11:00:00:03')
        vap5_id = 'device3-2g'
        vap5 = VirtualAccessPoint(ap3, vap5_id, BAND_TYPE.BAND_24G, 'ath0',
                                  'wifi1', 11, CHANNEL_GROUP.CG_24G, 70)
        db.add_vap(vap5)

        vap6_id = 'device3-5g'
        vap6 = VirtualAccessPoint(ap3, vap6_id, BAND_TYPE.BAND_5G, 'ath1',
                                  'wifi0', 165, CHANNEL_GROUP.CG_5GH, 78)
        db.add_vap(vap6)

        self.assertEquals(vap6, db.get_vap(vap6.vap_id))
        self.assertEquals(vap5, db.get_vap(vap5.vap_id))

        # Verify duplicate registration is rejected
        self.assertRaises(ValueError, db.add_vap, vap1)

        ###############################################################
        # Verify handling of ClientDevice objects.
        ###############################################################
        name1 = 'devA'
        mac_addr1 = '00:11:22:33:44:55'
        dev1 = ClientDevice(name1, mac_addr1, TRAFFIC_CLASS_TYPE.STREAMING)
        db.add_client_device(dev1)
        self.assertEquals(dev1, db.get_client_device(dev1.mac_addr))
        self.assertEquals(None, db.get_client_device('AA:BB:CC:DD:EE:FF'))

        name2 = 'devB'
        mac_addr2 = '22:21:22:23:24:25'
        dev2 = ClientDevice(name2, mac_addr2, TRAFFIC_CLASS_TYPE.INTERACTIVE)
        db.add_client_device(dev2)
        self.assertEquals(dev1, db.get_client_device(dev1.mac_addr))
        self.assertEquals(dev2, db.get_client_device(dev2.mac_addr))

        # Confirm more than 2 client devices can be added with the
        # same traffic type.
        name3 = 'devC'
        mac_addr3 = '33:00:33:00:33:00'
        dev3 = ClientDevice(name3, mac_addr3, TRAFFIC_CLASS_TYPE.INTERACTIVE)
        db.add_client_device(dev3)

        name4 = 'devD'
        mac_addr4 = '44:00:44:00:44:00'
        dev4 = ClientDevice(name4, mac_addr4, TRAFFIC_CLASS_TYPE.STREAMING)
        db.add_client_device(dev4)

        self.assertEquals(dev1, db.get_client_device(dev1.mac_addr))
        self.assertEquals(dev2, db.get_client_device(dev2.mac_addr))
        self.assertEquals(dev3, db.get_client_device(dev3.mac_addr))
        self.assertEquals(dev4, db.get_client_device(dev4.mac_addr))

        # Verify duplicate registration is rejected
        self.assertRaises(ValueError, db.add_client_device, dev1)

        ###############################################################
        # Verify handling of Paths.
        ###############################################################

        current_time = 49
        time_mock = MagicMock(return_value=current_time)
        with patch('time.time', time_mock):
            # Initially, no device has any paths
            self.assertEquals({}, ap1.get_paths())
            self.assertEquals({}, ap2.get_paths())

            # Add a path to ap1 from H-Active
            hactive_table = self._make_hactive_table(
                [(mac_addr1, ap2_mac_addr, 1000, 10, 'ath0'),
                 (mac_addr1, ap2_mac_addr, 1001, 11, 'ath1'),
                 (mac_addr2, ap2_mac_addr, 500001, 12, 'ath0'),
                 (mac_addr2, ap2_mac_addr, 600001, 13, 'athX')])

            ap1.update_paths_from_hactive(hactive_table, db, False)

            # For mac_addr1: Path should be the second one - highest rate,
            # and no existing value with the same hash
            # For mac_addr2: Path should be the second one - highest rate,
            # and above the dominant_flow_rate_threshold
            # Note that it's OK for VAP to be none
            self._check_paths(ap1, [(mac_addr1, vap1, ap2, vap1),
                                    (mac_addr2, None, ap2, None)])

            # Do the same update, but split in 2 with more_fragments set
            # Should result in the same set of paths
            hactive_table = self._make_hactive_table(
                [(mac_addr1, ap2_mac_addr, 1000, 10, 'ath0'),
                 (mac_addr1, ap2_mac_addr, 1001, 11, 'ath1')])

            ap1.update_paths_from_hactive(hactive_table, db, True)

            self._check_paths(ap1, [(mac_addr1, vap1, ap2, vap1),
                                    (mac_addr2, None, ap2, None)])

            hactive_table = self._make_hactive_table(
                [(mac_addr2, ap2_mac_addr, 500001, 12, 'ath0'),
                 (mac_addr2, ap2_mac_addr, 600001, 13, 'athX')])

            ap1.update_paths_from_hactive(hactive_table, db, False)

            self._check_paths(ap1, [(mac_addr1, vap1, ap2, vap1),
                                    (mac_addr2, None, ap2, None)])

            # Update mac_addr_1 with the same hash, even though another flow
            # has a higher flow rate, it is still below the threshold
            # Update mac_addr_2 with a different flow, since it has a higher flow rate
            hactive_table = self._make_hactive_table(
                [(mac_addr1, ap2_mac_addr, 2000, 10, 'ath0'),
                 (mac_addr1, ap2_mac_addr, 1001, 11, 'ath1'),
                 (mac_addr2, ap2_mac_addr, 700001, 12, 'ath0'),
                 (mac_addr2, ap2_mac_addr, 600001, 13, 'athX')])

            ap1.update_paths_from_hactive(hactive_table, db, False)
            self._check_paths(ap1, [(mac_addr1, vap1, ap2, vap1),
                                    (mac_addr2, vap4, ap2, vap4)])

            # Update mac_addr_1 from H-Default - will set the backup path;
            #                                    active path remains as is
            # Add entry for mac_addr_3 from H-Default
            hdefault_table = self._make_hdefault_table(
                [(mac_addr1, ap1_mac_addr, 'ath2'),
                 (mac_addr3, ap2_mac_addr, 'athX')])

            ap1.update_paths_from_hdefault(hdefault_table, db, False)
            self._check_paths(ap1, [(mac_addr1, vap1, ap2, vap1),
                                    (mac_addr2, vap4, ap2, vap4),
                                    (mac_addr3, None, ap2, None)])

            # Advance time past the aging threshold, and update again with H-Default
            # H-Active only entry 2 should be aged out, entry 1 should swap to H-Default path
            current_time += 6
            time_mock.return_value = current_time

            ap1.update_paths_from_hdefault(hdefault_table, db, False)
            self._check_paths(ap1, [(mac_addr1, vap2, ap1, None),
                                    (mac_addr3, None, ap2, None)])

            # Update with mac_addr1 updated and more_fragments True -
            # update mac_addr1 but mac_addr3 should not be deleted
            hdefault_table = self._make_hdefault_table(
                [(mac_addr1, ap2_mac_addr, 'ath2')])
            ap1.update_paths_from_hdefault(hdefault_table, db, True)
            self._check_paths(ap1, [(mac_addr1, vap2, ap2, None),
                                    (mac_addr3, None, ap2, None)])

            # Update with mac_addr3 updated and more_fragments False -
            # mac_addr3 updated
            hdefault_table = self._make_hdefault_table(
                [(mac_addr3, ap2_mac_addr, 'ath1')])
            ap1.update_paths_from_hdefault(hdefault_table, db, False)
            self._check_paths(ap1, [(mac_addr1, vap2, ap2, None),
                                    (mac_addr3, vap1, ap2, None)])

            # Update with an empty H-Default table and more_fragments set to True -
            # should leave paths unchanged
            ap1.update_paths_from_hdefault([], db, True)
            self._check_paths(ap1, [(mac_addr1, vap2, ap2, None),
                                    (mac_addr3, vap1, ap2, None)])
            # Update with an empty H-Default table and more_fragments set to False -
            # should remove all paths
            ap1.update_paths_from_hdefault([], db, False)
            self.assertEquals({}, ap1.get_paths())

            # Update with H-Active again, should bring back mac_addr2 and mac_addr1
            ap1.update_paths_from_hactive(hactive_table, db, False)
            self._check_paths(ap1, [(mac_addr1, vap4, ap2, vap4),
                                    (mac_addr2, vap4, ap2, vap4)])

            # Update with an empty H-Active - goes back to no paths
            ap1.update_paths_from_hactive([], db, False)
            self.assertEquals({}, ap1.get_paths())

    def _create_mock_observer(self):
        """Create an :class:`Observer` instance.

        The update methods will be mocked out.
        """
        observer = ModelBase.Observer()
        observer.update_associated_stations = MagicMock(
            name='update_associated_stations'
        )
        observer.update_utilizations = MagicMock(
            name='update_utilizations'
        )
        observer.update_throughputs = MagicMock(
            name='update_throughputs'
        )
        observer.update_paths = MagicMock(
            name='update_paths'
        )
        return observer

    def test_model_base(self):
        """Verify functionality of :class:`ModelBase`.

        Note that some functionality is validated in the test case for
        :class:`ActiveModel` for simplicity.
        """
        device_db = DeviceDB()
        model = ModelBase(device_db)
        self.assertRaises(NotImplementedError,
                          model.update_associated_stations, [], "fakeap")
        self.assertRaises(NotImplementedError,
                          model.update_utilizations, 0, "fakeap")
        self.assertRaises(NotImplementedError,
                          model.execute_in_model_context, None)
        self.assertRaises(NotImplementedError,
                          model.add_associated_station, "vap", "mac", "ap")
        self.assertRaises(NotImplementedError,
                          model.del_associated_station, "mac", "ap")
        self.assertRaises(NotImplementedError,
                          model.update_associated_station_rssi, "vap",
                          "mac", 0, "ap")
        self.assertRaises(NotImplementedError,
                          model.update_associated_station_data_metrics, "vap",
                          "mac", 0, 0, 0, "ap")
        self.assertRaises(NotImplementedError,
                          model.update_associated_station_activity, "vap",
                          "mac", "ap", is_active=True)
        self.assertRaises(NotImplementedError,
                          model.update_station_flags,
                          "mac", "ap", is_unfriendly=True)
        self.assertRaises(NotImplementedError,
                          model.update_overload_status, None, "fakeap")
        self.assertRaises(NotImplementedError,
                          model.update_steering_blackout_status, None, "fakeap")
        self.assertRaises(NotImplementedError,
                          model.update_throughputs, None, "fakeap")
        self.assertRaises(NotImplementedError,
                          model.update_station_steering_status, None)
        self.assertRaises(NotImplementedError,
                          model.update_paths_from_hactive, [], "fakeap", False)
        self.assertRaises(NotImplementedError,
                          model.update_paths_from_hdefault, [], "fakeap", False)
        self.assertRaises(NotImplementedError,
                          model.update_station_pollution_state, "mac", "ap", "vap", False)

        self.assertRaises(NotImplementedError,
                          model.reset_associated_stations)
        self.assertRaises(NotImplementedError,
                          model.reset_utilizations)
        self.assertRaises(NotImplementedError,
                          model.reset_overload_status)
        self.assertRaises(NotImplementedError,
                          model.reset_steering_blackout_status)

    def test_active_model(self):
        """Verify functionality of :class:`ActiveModel`."""
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, 'sample_config.yaml')
        )
        self.assert_(device_db)

        ap1_id = 'ap1'

        vap1_id = '2.4g_1'
        vap2_id = '5g_1'
        vap3_id = '2.4g_2'
        vap4_id = '5g_2'

        client1_mac = '00:11:22:33:44:55'
        client2_mac = '00:22:44:66:88:aa'
        client3_mac = '00:33:66:33:66:99'

        # First get handles to the relevant objects.
        ap1 = device_db.get_ap(ap1_id)
        self.assert_(ap1)
        vap1 = device_db.get_vap(vap1_id)
        self.assert_(vap1)
        vap2 = device_db.get_vap(vap2_id)
        self.assert_(vap2)
        vap3 = device_db.get_vap(vap3_id)
        self.assert_(vap3)
        vap4 = device_db.get_vap(vap4_id)
        self.assert_(vap4)

        client1 = device_db.get_client_device(client1_mac)
        self.assert_(client1)
        client2 = device_db.get_client_device(client2_mac)
        self.assert_(client2)
        client3 = device_db.get_client_device(client3_mac)
        self.assert_(client3)

        ###############################################################
        # No observers registered
        ###############################################################
        model = ActiveModel(device_db)

        # Case 1: A client shows up on a single VAP. Other VAPs have no
        #         associated clients.
        client1_orig_metrics = ServedSTAMetrics().update_rssi(10,
                                                              RSSI_LEVEL.WEAK)

        # This is the simplest way to make sure the command has executed.
        # We may revisit this once the observers are added.
        model.activate()
        model.update_associated_stations(
            {vap1_id: [(client1_mac, client1_orig_metrics.rssi,
                        client1_orig_metrics.rate)]}, vap1.ap.ap_id
        )
        # VAP 1 resolved utilization
        vap1_utilization = 11
        model.update_utilizations({'data': {vap1_id: vap1_utilization},
                                   'type': 'raw'}, vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals([(client1_mac, client1_orig_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([], vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap1_utilization, vap1.utilization)
        self.assertEquals(None, vap2.utilization)
        self.assertEquals(None, vap3.utilization)
        self.assertEquals(None, vap4.utilization)

        self.assertEquals(vap1, client1.serving_vap)
        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(None, client3.serving_vap)

        ###############################################################
        # One observer registered
        ###############################################################

        observer1 = self._create_mock_observer()
        model.register_observer(observer1)

        # Case 2: Two new clients show up. Previous client does not move.
        client2_metrics = ServedSTAMetrics().update_rssi(25,
                                                         RSSI_LEVEL.MODERATE)
        client3_metrics = ServedSTAMetrics().update_rssi(37,
                                                         RSSI_LEVEL.HIGH)

        model.activate()
        model.update_associated_stations(
            {vap3_id: [(client2_mac, client2_metrics.rssi,
                        client2_metrics.rate),
                       (client3_mac, client3_metrics.rssi,
                        client3_metrics.rate)]},
            vap3.ap.ap_id
        )

        model.update_paths_from_hactive([], ap1_id, False)
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap3.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        observer1.update_paths.assert_called_once_with(model, ap1_id)
        observer1.update_paths.reset_mock()

        self.assertEquals([(client1_mac, client1_orig_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap1, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 3.1: Client 1 moves to a new VAP altogether (but stays on
        #           the same band).
        client1_metrics = ServedSTAMetrics().update_rssi(44,
                                                         RSSI_LEVEL.HIGH)
        model.activate()
        model.update_associated_stations(
            {vap3_id: [(client1_mac, client1_metrics.rssi,
                        client1_metrics.rate),
                       (client2_mac, client2_metrics.rssi,
                        client2_metrics.rate),
                       (client3_mac, client3_metrics.rssi,
                        client3_metrics.rate)]},
            vap3.ap.ap_id
        )
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap3.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        # Stale until it is updated.
        self.assertEquals([(client1_mac, client1_orig_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client1_mac, client1_metrics),
                           (client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap3, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 3.2: Original serving VAP for client 1 updates to say it
        #           is no longer there.
        model.activate()
        model.update_associated_stations({vap1_id: []}, vap1.ap.ap_id)
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        self.assertEquals([], vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client1_mac, client1_metrics),
                           (client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap3, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 4.0: Association/Disassociation/Update RSSI with invalid parameters
        # Association/UpdateRSSI on an invalid VAP, ignore it
        # Also ignored is an update data metrics or flags on an invalid VAP
        model.activate()
        model.add_associated_station("randomVap", client1_mac, vap1.ap.ap_id)
        model.update_associated_station_rssi("", client1_mac, 10, vap1.ap.ap_id)
        model.update_associated_station_data_metrics("", client1_mac, 10, 30,
                                                     33, vap1.ap.ap_id)
        model.update_associated_station_activity("", client1_mac, vap1.ap.ap_id,
                                                 is_active=True)
        model.shutdown()
        self.assert_(not observer1.update_associated_stations.called)

        # Disassociation/UpdateRSSI a non-existing station, ignore it
        # Same with a data metrics update
        model.activate()
        model.del_associated_station("ff:ff:ff:ff:ff:ff", vap1.ap.ap_id)
        model.update_associated_station_rssi(vap1_id, "ff:ff:ff:ff:ff:ff",
                                             10, vap1.ap.ap_id)
        model.update_associated_station_data_metrics(
            vap1_id, "ff:ff:ff:ff:ff:ff", 10, 30, 33, vap1.ap.ap_id)
        model.shutdown()
        self.assert_(not observer1.update_associated_stations.called)

        # Case 4.1: Receive an event indicating client 1 disassociates on VAP3
        model.activate()
        model.del_associated_station(client1_mac, vap3.ap.ap_id)
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap3.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        self.assertEquals([], vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(None, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 4.2: Receive an event indicating client 1 associates on VAP1
        model.activate()
        model.add_associated_station(vap1_id, client1_mac, vap1.ap.ap_id)
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        client1_metrics = ServedSTAMetrics()
        self.assertEquals([(client1_mac, client1_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap1, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 4.3.1: Receive an RSSI update for client 1 on a different VAP;
        #             ignore it
        client1_new_rssi = 11
        model.activate()
        model.update_associated_station_rssi(vap2_id, client1_mac,
                                             client1_new_rssi, vap2.ap.ap_id)
        model.shutdown()
        self.assert_(not observer1.update_associated_stations.called)

        # Case 4.3.2: Receive a STA data metrics update for client 1 on a
        #             different VAP; ignore it
        client1_new_tput = 10
        client1_new_rate = 30
        client1_new_airtime = 33
        model.activate()
        model.update_associated_station_data_metrics(
            vap2_id, client1_mac, client1_new_tput, client1_new_rate,
            client1_new_airtime, vap2.ap.ap_id
        )
        model.shutdown()
        self.assert_(not observer1.update_associated_stations.called)

        # Case 4.3.3: Receive a STA activity update for client 1 on a different
        #             VAP; ignore it
        model.activate()
        model.update_associated_station_activity(
            vap2_id, client1_mac, vap2.ap.ap_id, is_active=True
        )
        model.shutdown()
        self.assert_(not observer1.update_associated_stations.called)

        # Case 4.4.1: Receive an RSSI update for client 1 on the serving VAP
        model.activate()
        model.update_associated_station_rssi(vap1_id, client1_mac,
                                             client1_new_rssi, vap1.ap.ap_id)
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap2.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        client1_metrics.update_rssi(client1_new_rssi, RSSI_LEVEL.WEAK)
        self.assertEquals([(client1_mac, client1_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap1, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 4.4.2: Receive a STA data metrics update for client 1 on the
        #             serving VAP
        model.activate()
        model.update_associated_station_data_metrics(
            vap1_id, client1_mac, client1_new_tput, client1_new_rate,
            client1_new_airtime, vap1.ap.ap_id
        )
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        client1_metrics.update_data_metrics(client1_new_tput, client1_new_rate,
                                            client1_new_airtime)
        self.assertEquals([(client1_mac, client1_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client2_mac, client2_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(vap1, client1.serving_vap)
        self.assertEquals(vap3, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 4.4.3: Receive a STA activity update for client 1 on the serving
        #             VAP
        model.activate()
        model.update_associated_station_activity(
            vap1_id, client1_mac, vap1.ap.ap_id, is_active=True,
        )
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_associated_stations.reset_mock()

        self.assert_(client1.is_active)
        self.assert_(not client1.is_dual_band)
        self.assert_(not client1.btm_compliance)
        self.assert_(not client1.prohibit_type)
        self.assert_(not client1.is_unfriendly)
        self.assert_(not client1.is_btm_unfriendly)

        # Case 4.4.4: Receive a STA flags update for client 1
        model.activate()
        model.update_station_flags(
            client1_mac, vap1.ap.ap_id, is_unfriendly=True,
            btm_compliance=BTMComplianceType.BTM_COMPLIANCE_ACTIVE_UNFRIENDLY
        )
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer1.update_associated_stations.reset_mock()

        self.assert_(client1.is_active)
        self.assert_(not client1.is_dual_band)
        self.assertEquals(BTMComplianceType.BTM_COMPLIANCE_ACTIVE_UNFRIENDLY,
                          client1.btm_compliance)
        self.assert_(not client1.prohibit_type)
        self.assert_(client1.is_unfriendly)
        self.assert_(not client1.is_btm_unfriendly)

        # Case 5: VAPs update utilization info
        vap1_utilization = 0
        vap2_utilization = -22
        vap3_utilization = 100
        vap4_utilization = 144
        model.activate()
        # First only VAP2 update its invalid utilization, should do nothing
        model.update_utilizations({'data': {vap2_id: vap2_utilization},
                                   'type': 'raw'}, vap2.ap.ap_id)
        self.assert_(not observer1.update_utilizations.called)
        self.assertEquals(None, vap2.utilization)
        # Then all VAPs update, but VAP4's utilization is invalid
        vap2_utilization = 22
        model.update_utilizations({'data': {vap1_id: vap1_utilization,
                                   vap2_id: vap2_utilization,
                                   vap3_id: vap3_utilization,
                                   vap4_id: vap4_utilization}, 'type': 'raw'}, vap1.ap.ap_id)
        model.shutdown()
        observer1.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_utilizations.reset_mock()

        self.assertEquals(vap1_utilization, vap1.utilization)
        self.assertEquals(vap2_utilization, vap2.utilization)
        self.assertEquals(vap3_utilization, vap3.utilization)
        self.assertEquals(None, vap4.utilization)

        # Perform an update with the average utilization.
        model.activate()
        model.update_utilizations({'data': {vap1_id: vap1_utilization + 5,
                                   vap2_id: vap2_utilization - 2,
                                   vap3_id: vap3_utilization - 1,
                                   vap4_id: vap4_utilization}, 'type': 'average'}, vap1.ap.ap_id)
        model.shutdown()
        observer1.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_utilizations.reset_mock()

        self.assertEquals(vap1_utilization + 5, vap1.utilization)
        self.assertEquals(vap1_utilization + 5, vap1.utilization_avg.value)
        self.assertEquals(1, vap1.utilization_avg.seq)
        self.assertEquals(vap2_utilization - 2, vap2.utilization)
        self.assertEquals(vap2_utilization - 2, vap2.utilization_avg.value)
        self.assertEquals(1, vap2.utilization_avg.seq)
        self.assertEquals(vap3_utilization - 1, vap3.utilization)
        self.assertEquals(vap3_utilization - 1, vap3.utilization_avg.value)
        self.assertEquals(1, vap3.utilization_avg.seq)
        self.assertEquals(None, vap4.utilization)
        self.assertEquals(None, vap4.utilization_avg.value)

        ###############################################################
        # Multiple observers registered
        ###############################################################

        observer2 = self._create_mock_observer()
        model.register_observer(observer2)

        # Case 6: Client 2 moves to a different band but on the same AP.
        #         Other clients remain on the same band but have updated
        #         RSSIs.
        client1_orig_metrics = copy.deepcopy(client1_metrics)
        client1_metrics = ServedSTAMetrics().update_rssi(client1_metrics.rssi + 1,
                                                         RSSI_LEVEL.WEAK)
        client2_metrics = ServedSTAMetrics().update_rssi(client2_metrics.rssi + 10,
                                                         RSSI_LEVEL.HIGH)
        client3_metrics = ServedSTAMetrics().update_rssi(client3_metrics.rssi - 1,
                                                         RSSI_LEVEL.HIGH)
        model.activate()
        model.update_associated_stations(
            {vap3_id: [(client1_mac, client1_metrics.rssi,
                        client1_metrics.rate),
                       (client3_mac, client3_metrics.rssi,
                        client3_metrics.rate)],
             vap4_id: [(client2_mac, client2_metrics.rssi,
                        client2_metrics.rate)]},
            vap3.ap.ap_id
        )

        model.update_paths_from_hdefault([], ap1_id, False)
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap3.ap.ap_id)
        observer1.update_associated_stations.reset_mock()
        observer2.update_associated_stations.assert_called_once_with(model, vap3.ap.ap_id)
        observer2.update_associated_stations.reset_mock()

        observer1.update_paths.assert_called_once_with(model, ap1_id)
        observer1.update_paths.reset_mock()
        observer2.update_paths.assert_called_once_with(model, ap1_id)
        observer2.update_paths.reset_mock()

        self.assertEquals([(client1_mac, client1_orig_metrics)],
                          vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([(client1_mac, client1_metrics),
                           (client3_mac, client3_metrics)],
                          vap3.get_associated_stations())
        self.assertEquals([(client2_mac, client2_metrics)],
                          vap4.get_associated_stations())

        self.assertEquals(vap3, client1.serving_vap)
        self.assertEquals(vap4, client2.serving_vap)
        self.assertEquals(vap3, client3.serving_vap)

        # Case 7: Client 3 moves to a different AP and band. Client 1 only
        #         moves to a different band.
        #
        # Note: In the current implementation, the associated station info
        #       for VAPs on different APs would not be updated in the same
        #       call, but there's no reason to disallow this, so it is tested
        #       here.
        client1_metrics = ServedSTAMetrics().update_rssi(client1_metrics.rssi + 3,
                                                         RSSI_LEVEL.LOW)
        client3_metrics = ServedSTAMetrics().update_rssi(client3_metrics.rssi + 6,
                                                         RSSI_LEVEL.HIGH)
        model.activate()
        model.update_associated_stations(
            {vap1_id: [],
             vap2_id: [(client3_mac, client3_metrics.rssi,
                        client3_metrics.rate)],
             vap3_id: [],
             vap4_id: [(client1_mac, client1_metrics.rssi,
                        client1_metrics.rate),
                       (client2_mac, client2_metrics.rssi,
                        client2_metrics.rate)]},
            vap1.ap.ap_id
        )
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_associated_stations.reset_mock()
        observer2.update_associated_stations.assert_called_once_with(model, vap1.ap.ap_id)
        observer2.update_associated_stations.reset_mock()

        self.assertEquals([], vap1.get_associated_stations())
        self.assertEquals([(client3_mac, client3_metrics)],
                          vap2.get_associated_stations())
        self.assertEquals([], vap3.get_associated_stations())
        self.assertEquals([(client1_mac, client1_metrics),
                           (client2_mac, client2_metrics)],
                          vap4.get_associated_stations())

        self.assertEquals(vap4, client1.serving_vap)
        self.assertEquals(vap4, client2.serving_vap)
        self.assertEquals(vap2, client3.serving_vap)

        # Case 8: If a client device is no longer reported on a VAP, its
        #         serving AP should be invalidated.
        observer3 = self._create_mock_observer()
        model.register_observer(observer3)

        model.activate()
        model.update_associated_stations(
            {vap1_id: [],
             vap2_id: [],
             vap3_id: [],
             vap4_id: [(client1_mac, client1_metrics.rssi,
                        client1_metrics.rate),
                       (client2_mac, client2_metrics.rssi,
                        client2_metrics.rate)]},
            vap4.ap.ap_id
        )
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(model, vap4.ap.ap_id)
        observer1.update_associated_stations.reset_mock()
        observer2.update_associated_stations.assert_called_once_with(model, vap4.ap.ap_id)
        observer2.update_associated_stations.reset_mock()
        observer3.update_associated_stations.assert_called_once_with(model, vap4.ap.ap_id)
        observer3.update_associated_stations.reset_mock()

        self.assertEquals([], vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([], vap3.get_associated_stations())
        self.assertEquals([(client1_mac, client1_metrics),
                           (client2_mac, client2_metrics)],
                          vap4.get_associated_stations())

        self.assertEquals(vap4, client1.serving_vap)
        self.assertEquals(vap4, client2.serving_vap)
        self.assertEquals(None, client3.serving_vap)

        # Case 9: Update utilizations, should notify every observer
        model.activate()
        model.update_utilizations({'data': {vap1_id: vap1_utilization,
                                   vap2_id: vap2_utilization,
                                   vap3_id: vap3_utilization,
                                   vap4_id: vap4_utilization}, 'type': 'raw'}, vap1.ap.ap_id)
        model.shutdown()
        observer1.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_utilizations.reset_mock()
        observer2.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer2.update_utilizations.reset_mock()
        observer3.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer3.update_utilizations.reset_mock()

        self.assertEquals(vap1_utilization, vap1.utilization)
        self.assertEquals(vap2_utilization, vap2.utilization)
        self.assertEquals(vap3_utilization, vap3.utilization)
        self.assertEquals(None, vap4.utilization)

        # Case 10: VAPs update overload status
        vap1_overloaded = True
        vap2_overloaded = False
        vap3_overloaded = True
        vap4_overloaded = True
        model.activate()
        model.update_overload_status({vap1_id: vap1_overloaded,
                                      vap2_id: vap2_overloaded,
                                      vap3_id: vap3_overloaded,
                                      vap4_id: vap4_overloaded},
                                     vap1.ap.ap_id)
        model.shutdown()
        observer1.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_utilizations.reset_mock()
        observer2.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer2.update_utilizations.reset_mock()
        observer3.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer3.update_utilizations.reset_mock()

        self.assertEquals(vap1_overloaded, vap1.overloaded)
        self.assertEquals(vap2_overloaded, vap2.overloaded)
        self.assertEquals(vap3_overloaded, vap3.overloaded)
        self.assertEquals(vap4.overloaded, vap4.overloaded)

        # Case 11: VAPs update throughput
        vap1_throughputs = (213, 17)
        vap2_throughputs = (47, 109)
        vap3_throughputs = (0.3, 91.5)
        vap4_throughputs = (42.9, 82)
        model.activate()
        model.update_throughputs({vap1_id: vap1_throughputs,
                                  vap2_id: vap2_throughputs,
                                  vap3_id: vap3_throughputs,
                                  vap4_id: vap4_throughputs},
                                 vap1.ap.ap_id)
        model.shutdown()

        observer1.update_throughputs.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_throughputs.reset_mock()
        observer2.update_throughputs.assert_called_once_with(model, vap1.ap.ap_id)
        observer2.update_throughputs.reset_mock()
        observer3.update_throughputs.assert_called_once_with(model, vap1.ap.ap_id)
        observer3.update_throughputs.reset_mock()

        self.assertEquals(vap1_throughputs[0], vap1.tx_throughput)
        self.assertEquals(vap1_throughputs[1], vap1.rx_throughput)
        self.assertEquals(vap2_throughputs[0], vap2.tx_throughput)
        self.assertEquals(vap2_throughputs[1], vap2.rx_throughput)
        self.assertEquals(vap3_throughputs[0], vap3.tx_throughput)
        self.assertEquals(vap3_throughputs[1], vap3.rx_throughput)
        self.assertEquals(vap4_throughputs[0], vap4.tx_throughput)
        self.assertEquals(vap4_throughputs[1], vap4.rx_throughput)

        # Case 12: AP steering blackout changse
        ap1_blackout = False
        self.assertEquals(ap1_blackout, vap1.ap.steering_blackout)

        ap1_blackout = True
        model.activate()
        model.update_steering_blackout_status(ap1_blackout, vap1.ap.ap_id)

        model.shutdown()
        observer1.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer1.update_utilizations.reset_mock()
        observer2.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer2.update_utilizations.reset_mock()
        observer3.update_utilizations.assert_called_once_with(model, vap1.ap.ap_id)
        observer3.update_utilizations.reset_mock()

        self.assertEquals(ap1_blackout, vap1.ap.steering_blackout)

        # Case 13: Reset the associated stations clears all the lists.
        model.activate()
        model.reset_associated_stations()
        model.shutdown()

        observer1.update_associated_stations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer1.update_associated_stations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer1.update_associated_stations.reset_mock()
        observer2.update_associated_stations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer2.update_associated_stations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer2.update_associated_stations.reset_mock()
        observer3.update_associated_stations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer3.update_associated_stations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer3.update_associated_stations.reset_mock()

        self.assertEquals([], vap1.get_associated_stations())
        self.assertEquals([], vap2.get_associated_stations())
        self.assertEquals([], vap3.get_associated_stations())
        self.assertEquals([], vap4.get_associated_stations())

        self.assertEquals(None, client1.serving_vap)
        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(None, client3.serving_vap)

        # Case 14: Reset the utilization information
        model.activate()
        model.reset_utilizations()
        model.shutdown()

        observer1.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer1.update_utilizations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer1.update_utilizations.reset_mock()
        observer2.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer2.update_utilizations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer2.update_utilizations.reset_mock()
        observer3.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer3.update_utilizations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer3.update_utilizations.reset_mock()

        self.assertEquals(None, vap1.utilization)
        self.assertEquals(None, vap2.utilization)
        self.assertEquals(None, vap3.utilization)
        self.assertEquals(None, vap4.utilization)

        # Case 15: Reset overload status
        model.activate()
        model.reset_overload_status()
        model.shutdown()

        observer1.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer1.update_utilizations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer1.update_utilizations.reset_mock()
        observer2.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer2.update_utilizations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer2.update_utilizations.reset_mock()
        observer3.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer3.update_utilizations.assert_called_once_with(
            model, vap3.ap.ap_id
        )
        observer3.update_utilizations.reset_mock()

        self.assertEquals(False, vap1.overloaded)
        self.assertEquals(False, vap2.overloaded)
        self.assertEquals(False, vap3.overloaded)
        self.assertEquals(False, vap4.overloaded)

        # Case 16: Reset throughputs
        model.activate()
        model.reset_throughputs()
        model.shutdown()

        for observer in (observer1, observer2, observer3):
            observer.update_throughputs.assert_called_once_with(
                model, vap1.ap.ap_id
            )
            observer.update_throughputs.reset_mock()

        self.assertEquals(0, vap1.tx_throughput)
        self.assertEquals(0, vap1.rx_throughput)
        self.assertEquals(0, vap2.tx_throughput)
        self.assertEquals(0, vap2.rx_throughput)
        self.assertEquals(0, vap3.tx_throughput)
        self.assertEquals(0, vap3.rx_throughput)
        self.assertEquals(0, vap4.tx_throughput)
        self.assertEquals(0, vap4.rx_throughput)

        # Case 17: Reset steering blackout
        model.activate()
        model.reset_steering_blackout_status()
        model.shutdown()

        observer1.update_utilizations.assert_called_once_with(
            model, vap1.ap.ap_id
        )
        observer1.update_utilizations.reset_mock()

        self.assertEquals(False, vap1.ap.steering_blackout)

    def test_active_model_update_steering_status(self):
        """Verify update_station_steering_status function in :class:`ActiveModel`."""
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, 'sample_config.yaml')
        )
        self.assert_(device_db)

        vap1_id = '2.4g_1'
        vap2_id = '5g_1'
        vap4_id = '5g_2'

        client1_mac = '00:11:22:33:44:55'
        client2_mac = '00:22:44:66:88:AA'

        # First get handles to the relevant objects.
        vap1 = device_db.get_vap(vap1_id)
        self.assert_(vap1)
        vap2 = device_db.get_vap(vap2_id)
        self.assert_(vap2)
        vap4 = device_db.get_vap(vap4_id)
        self.assert_(vap4)

        client1 = device_db.get_client_device(client1_mac)
        self.assert_(client1)
        client2 = device_db.get_client_device(client2_mac)
        self.assert_(client2)

        model = ActiveModel(device_db)

        # Case 0: On start, client's steering status should be reset
        self.assertEquals(False, client1.steering_pending_vap)
        self.assertEquals(False, client1.is_steered)
        self.assertEquals(None, client1.target_band)
        self.assertEquals(None, client1.target_vaps)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)
        self.assertEquals(None, client2.target_vaps)

        ###############################################################
        # Verify pre-association steering
        ###############################################################
        # Pre-association steer client 1 to 2.4 GHz
        model.activate()
        model.update_station_steering_status(client1_mac, start=True,
                                             assoc_band=BAND_TYPE.BAND_INVALID,
                                             target_band=BAND_TYPE.BAND_24G)
        model.shutdown()

        self.assertEquals(None, client1.serving_vap)
        self.assertEquals(True, client1.steering_pending_band)
        self.assertEquals(False, client1.is_steered)
        self.assertEquals(BAND_TYPE.BAND_24G, client1.target_band)

        # Client 1 fails steering, associated on 5 GHz instead
        model.activate()
        model.add_associated_station(vap2.vap_id, client1_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client1_mac, assoc_band=vap2.band)
        model.shutdown()

        self.assertEquals(vap2, client1.serving_vap)
        self.assertEquals(False, client1.steering_pending_band)
        self.assertEquals(False, client1.is_steered)
        self.assertEquals(None, client1.target_band)

        # Client 1 is disassociated on 5 GHz by its own
        model.activate()
        model.del_associated_station(client1_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client1_mac,
                                             assoc_band=BAND_TYPE.BAND_INVALID)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_band)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)

        # Pre-association steer client 1 to 2.4 GHz again
        model.activate()
        model.update_station_steering_status(client1_mac, start=True,
                                             assoc_band=BAND_TYPE.BAND_INVALID,
                                             target_band=BAND_TYPE.BAND_24G)
        model.shutdown()

        self.assertEquals(None, client1.serving_vap)
        self.assertEquals(True, client1.steering_pending_band)
        self.assertEquals(False, client1.is_steered)
        self.assertEquals(BAND_TYPE.BAND_24G, client1.target_band)

        # This time, it completes steering, associated on 2.4 GHz
        model.activate()
        model.add_associated_station(vap1.vap_id, client1_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client1_mac, assoc_band=vap1.band)
        model.shutdown()

        self.assertEquals(vap1, client1.serving_vap)
        self.assertEquals(False, client1.steering_pending_band)
        self.assertEquals(True, client1.is_steered)
        self.assertEquals(BAND_TYPE.BAND_24G, client1.target_band)

        # Reset associated station will reset steering status
        model.activate()
        model.reset_associated_stations()
        model.shutdown()

        self.assertEquals(None, client1.serving_vap)
        self.assertEquals(False, client1.steering_pending_band)
        self.assertEquals(False, client1.steering_pending_vap)
        self.assertEquals(False, client1.is_steered)
        self.assertEquals(None, client1.target_band)

        ###############################################################
        # Verify post-association steering
        ###############################################################
        # client 2 is associated on 2.4 GHz
        model.activate()
        model.add_associated_station(vap1.vap_id, client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_band=vap1.channel)
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)
        self.assertEquals(None, client2.target_vaps)

        # Post-association steer client 2.4 GHz to 5 GHz
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=vap1,
                                             target_vaps=[vap2])
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap2], client2.target_vaps)

        # Client 2 disassociates on 2.4 GHz
        model.activate()
        model.del_associated_station(client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap2], client2.target_vaps)

        # Client 2 completes steering by associating on 5 GHZ
        model.activate()
        model.add_associated_station(vap2.vap_id, client2_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=vap2)
        model.shutdown()

        self.assertEquals(vap2, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(True, client2.is_steered)
        self.assertEquals([vap2], client2.target_vaps)

        # Client 2 is steered to 2.4 GHz
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=vap2,
                                             target_vaps=[vap1])
        model.shutdown()

        self.assertEquals(vap2, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap1], client2.target_vaps)

        # Client 2 disassociates on 5 GHz
        model.activate()
        model.del_associated_station(client2_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap1], client2.target_vaps)

        # The steering is aborted
        model.activate()
        model.update_station_steering_status(client2_mac, abort=True)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_vaps)

        # Client 2 associates on 2.4 GHz by its own after steering abort,
        # should not be marked as being steered
        model.activate()
        model.add_associated_station(vap1.vap_id, client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_band=vap1.channel)
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_vaps)

        # Try to steer client 2 to 5 GHz again
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=vap1,
                                             target_vaps=[vap2])
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap2], client2.target_vaps)

        # Client 2 disassociates on 2.4 GHz
        model.activate()
        model.del_associated_station(client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap2], client2.target_vaps)

        # Client 2 comes back on 2.4 GHz by its own before steering abort,
        # probably due to the abort event lost, should not be marked as being steered
        model.activate()
        model.add_associated_station(vap1.vap_id, client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=vap1)
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)

        # client 2 is associated on channel 7
        model.activate()
        model.reset_associated_stations()
        model.add_associated_station(vap1.vap_id, client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_band=vap1)
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)
        self.assertEquals(None, client2.target_vaps)

        # Post-association steer client 2.4 GHz to 5 GHz (both channels)
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=vap1,
                                             target_vaps=[vap4, vap2])
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap4, vap2], client2.target_vaps)

        # Client 2 disassociates on channel 7
        model.activate()
        model.del_associated_station(client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap4, vap2], client2.target_vaps)

        # Client 2 completes steering by associating on channel 100
        model.activate()
        model.add_associated_station(vap2.vap_id, client2_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=vap2)
        model.shutdown()

        self.assertEquals(vap2, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(True, client2.is_steered)
        self.assertEquals([vap4, vap2], client2.target_vaps)

        # Client 2 is steered to channel 7
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=vap2,
                                             target_vaps=[vap1])
        model.shutdown()

        self.assertEquals(vap2, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap1], client2.target_vaps)

        # Client 2 disassociates on channel 100
        model.activate()
        model.del_associated_station(client2_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap1], client2.target_vaps)

        # Client 2 associates on channel 150 on its own
        # should not be marked as being steered
        model.activate()
        model.add_associated_station(vap4.vap_id, client2_mac, vap4.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=vap4)
        model.shutdown()

        self.assertEquals(vap4, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_vaps)

        # client 2 is associated on channel 7
        model.activate()
        model.reset_associated_stations()
        model.add_associated_station(vap1.vap_id, client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_band=vap1.channel)
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)
        self.assertEquals(None, client2.target_vaps)

        # Post-association steer client 2.4 GHz to 5 GHz (both channels)
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=vap1,
                                             target_vaps=[vap4, vap2])
        model.shutdown()

        self.assertEquals(vap1, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap4, vap2], client2.target_vaps)

        # Client 2 disassociates on channel 7
        model.activate()
        model.del_associated_station(client2_mac, vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([vap4, vap2], client2.target_vaps)

        # Client 2 completes steering by associating on channel 100
        model.activate()
        model.add_associated_station(vap2.vap_id, client2_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=vap2)
        model.shutdown()

        self.assertEquals(vap2, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(True, client2.is_steered)
        self.assertEquals([vap4, vap2], client2.target_vaps)

        # Client 2 disassociates on channel 100
        model.activate()
        model.del_associated_station(client2_mac, vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=vap2.ap.ap_id)
        model.shutdown()

        self.assertEquals(None, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_vaps)

        # Client 2 associates on channel 150 on its own
        # should not be marked as being steered
        model.activate()
        model.add_associated_station(vap4.vap_id, client2_mac, vap4.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=vap4)
        model.shutdown()

        self.assertEquals(vap4, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_vaps)

        ###############################################################
        # Verify the generic function to execute a partial in model
        # context
        ###############################################################

    def test_active_mode_update_pollution_state(self):
        """Verify update_station_pollution_state function in :class:`ActiveModel`."""
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, 'sample_config_2ap.yaml')
        )
        self.assert_(device_db)

        cap_vap1_id = '2.4g_1'
        cap_vap2_id = '5g_1'
        cap_vap3_id = '2.4g_2'
        cap_vap4_id = '5g_2'
        re_vap1_id = 're2.4g'
        re_vap2_id = 're5g'

        client1_mac = '00:11:22:33:44:55'
        client2_mac = '11:11:11:00:00:09'

        # First get handles to the relevant objects.
        cap_vap1 = device_db.get_vap(cap_vap1_id)
        self.assert_(cap_vap1)
        cap_vap2 = device_db.get_vap(cap_vap2_id)
        self.assert_(cap_vap2)
        cap_vap3 = device_db.get_vap(cap_vap3_id)
        self.assert_(cap_vap3)
        cap_vap4 = device_db.get_vap(cap_vap4_id)
        self.assert_(cap_vap4)
        re_vap1 = device_db.get_vap(re_vap1_id)
        self.assert_(re_vap1)
        re_vap2 = device_db.get_vap(re_vap2_id)
        self.assert_(re_vap2)

        client1 = device_db.get_client_device(client1_mac)
        self.assert_(client1)
        client2 = device_db.get_client_device(client2_mac)
        self.assert_(client2)

        model = ActiveModel(device_db)

        observer1 = self._create_mock_observer()
        model.register_observer(observer1)

        # On start, clients are not polluted
        self.assert_(not client1.is_polluted)
        self.assert_(not client2.is_polluted)

        # Before association, receive pollution update
        # Should not mark client as polluted
        model.activate()
        model.update_station_pollution_state(client1_mac, cap_vap1.ap.ap_id,
                                             cap_vap1, True)
        model.update_station_pollution_state(client2_mac, cap_vap1.ap.ap_id,
                                             cap_vap1, True)
        model.shutdown()

        calls = observer1.update_associated_stations.call_args_list
        self.assertEquals(2, len(calls))
        for i in xrange(len(calls)):
            self.assertEquals(model, calls[i][0][0])
            self.assertEquals(cap_vap1.ap.ap_id, calls[i][0][1])
        self.assert_(not client1.is_polluted)
        self.assert_(not client2.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Client 1 associated on polluted VAP, mark it as polluted
        model.activate()
        model.update_associated_stations(
            {cap_vap1_id: [(client1_mac, 40, 433)]}, cap_vap1.ap.ap_id
        )
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
        self.assert_(client1.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Client 2 associated on a non-polluted VAP, not mark it as polluted
        model.activate()
        model.update_associated_stations(
            {cap_vap2_id: [(client2_mac, 40, 433)]}, cap_vap2.ap.ap_id
        )
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap2.ap.ap_id)
        self.assert_(not client2.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Client 1 moved to RE, which is not marked as polluted
        model.activate()
        model.update_associated_stations(
            {re_vap1_id: [(client1_mac, 40, 433)]}, re_vap1.ap.ap_id
        )
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, re_vap1.ap.ap_id)
        self.assert_(not client1.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Pollution state on CAP is cleared for Client 1
        model.activate()
        model.update_station_pollution_state(client1_mac, cap_vap1.ap.ap_id,
                                             cap_vap1, False)
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
        self.assert_(not client1.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Client 1 moves back to CAP VAP1, no longer polluted
        model.activate()
        model.update_associated_stations(
            {cap_vap1_id: [(client1_mac, 40, 433)]}, cap_vap1.ap.ap_id
        )
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
        self.assert_(not client1.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Client 2 moves to CAP VAP1, still marked as polluted
        model.activate()
        model.update_associated_stations(
            {cap_vap1_id: [(client2_mac, 40, 433)]}, cap_vap1.ap.ap_id
        )
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
        self.assert_(client2.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Mark client 2 as polluted repeatedly, then clear
        for i in xrange(10):
            model.activate()
            model.update_station_pollution_state(client2_mac, cap_vap1.ap.ap_id,
                                                 cap_vap1, True)
            model.shutdown()
            observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
            self.assert_(client2.is_polluted)
            observer1.update_associated_stations.reset_mock()

        model.activate()
        model.update_station_pollution_state(client2_mac, cap_vap1.ap.ap_id,
                                             cap_vap1, False)
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
        self.assert_(not client2.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Clear pollution again, expect same result
        model.activate()
        model.update_station_pollution_state(client2_mac, cap_vap1.ap.ap_id,
                                             cap_vap1, False)
        model.shutdown()
        observer1.update_associated_stations.assert_called_once_with(model, cap_vap1.ap.ap_id)
        self.assert_(not client2.is_polluted)
        observer1.update_associated_stations.reset_mock()

        # Error cases: pollution state change for unknown STA, do nothing
        model.activate()
        model.update_station_pollution_state("07:08:09:10:11:12", cap_vap1.ap.ap_id,
                                             cap_vap1, True)
        model.shutdown()
        self.assert_(not observer1.update_associated_stations.called)

    def test_active_model_multi_AP(self):
        """Verify :class:`ActiveModel` in multiple AP setup."""
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, 'sample_config_2ap.yaml')
        )
        self.assert_(device_db)

        cap_vap1_id = '2.4g_1'
        cap_vap2_id = '5g_1'
        cap_vap3_id = '2.4g_2'
        cap_vap4_id = '5g_2'
        re_vap1_id = 're2.4g'
        re_vap2_id = 're5g'

        client1_mac = '00:11:22:33:44:55'
        client2_mac = '11:11:11:00:00:09'

        # First get handles to the relevant objects.
        cap_vap1 = device_db.get_vap(cap_vap1_id)
        self.assert_(cap_vap1)
        cap_vap2 = device_db.get_vap(cap_vap2_id)
        self.assert_(cap_vap2)
        cap_vap3 = device_db.get_vap(cap_vap3_id)
        self.assert_(cap_vap3)
        cap_vap4 = device_db.get_vap(cap_vap4_id)
        self.assert_(cap_vap4)
        re_vap1 = device_db.get_vap(re_vap1_id)
        self.assert_(re_vap1)
        re_vap2 = device_db.get_vap(re_vap2_id)
        self.assert_(re_vap2)

        client1 = device_db.get_client_device(client1_mac)
        self.assert_(client1)
        client2 = device_db.get_client_device(client2_mac)
        self.assert_(client2)

        model = ActiveModel(device_db)

        # Verify same channel VAPs
        vaps = device_db.get_same_channel_vaps(cap_vap1.channel)
        # Two VAPs on channel 7
        self.assertEquals(2, len(vaps))
        self.assertTrue(cap_vap1 in vaps)
        self.assertTrue(re_vap1 in vaps)
        self.assertFalse(cap_vap3 in vaps)

        vaps = device_db.get_same_channel_vaps(cap_vap4.channel)
        # Only one VAP on channel 150
        self.assertEquals(1, len(vaps))
        self.assertTrue(cap_vap4 in vaps)

        # Case 1: test AP steering
        # On start, client's steering status should be reset
        self.assertEquals(False, client1.steering_pending_vap)
        self.assertEquals(False, client1.is_steered)
        self.assertEquals(None, client1.target_band)
        self.assertEquals(None, client1.target_vaps)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)
        self.assertEquals(None, client2.target_vaps)

        # client 2 is associated on CAP 2.4 GHz
        model.activate()
        model.del_associated_station(client2_mac, cap_vap1.ap.ap_id)
        model.add_associated_station(cap_vap1.vap_id, client2_mac, cap_vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=cap_vap1)
        model.shutdown()

        self.assertEquals(cap_vap1, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals(None, client2.target_band)
        self.assertEquals(None, client2.target_vaps)

        # Post-association steer client from CAP 2.4 GHz to RE 5 GHz and CAP 5 GHz
        model.activate()
        model.update_station_steering_status(client2_mac, start=True,
                                             assoc_vap=cap_vap1,
                                             target_vaps=[re_vap2, cap_vap4])
        model.shutdown()

        self.assertEquals(cap_vap1, client2.serving_vap)
        self.assertEquals(True, client2.steering_pending_vap)
        self.assertEquals(False, client2.is_steered)
        self.assertEquals([re_vap2, cap_vap4], client2.target_vaps)

        # Sometimes association on target AP comes earlier than disassociation
        # on original AP. Make sure we handle this case properly
        # Client 2 completes steering by associating on RE 5 GHZ
        model.activate()
        model.del_associated_station(client2_mac, re_vap2.ap.ap_id)
        model.add_associated_station(re_vap2.vap_id, client2_mac, re_vap2.ap.ap_id)
        model.update_station_steering_status(client2_mac, assoc_vap=re_vap2)
        model.shutdown()

        self.assertEquals(re_vap2, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(True, client2.is_steered)
        self.assertEquals([re_vap2, cap_vap4], client2.target_vaps)

        # Receive disassociation late, should ignore
        model.activate()
        model.del_associated_station(client2_mac, cap_vap1.ap.ap_id)
        model.update_station_steering_status(client2_mac,
                                             assoc_vap=None,
                                             ap_id=cap_vap1.ap.ap_id)
        model.shutdown()

        self.assertEquals(re_vap2, client2.serving_vap)
        self.assertEquals(False, client2.steering_pending_vap)
        self.assertEquals(True, client2.is_steered)
        self.assertEquals([re_vap2, cap_vap4], client2.target_vaps)

        # Case 2: test aggregated utilization
        cap_vap1_util = 11
        model.activate()
        model.update_utilizations({'data': {cap_vap1_id: cap_vap1_util},
                                   'type': 'average'}, cap_vap1.ap.ap_id)
        model.shutdown()
        # No aggregated util is computed since no update from RE
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_avg.value)
        self.assertEquals(None, cap_vap1.utilization_aggr.value)
        self.assertEquals(None, re_vap1.utilization_avg.value)
        self.assertEquals(None, re_vap1.utilization_aggr.value)
        # Verify that CAP's util is fresh
        self.assertTrue(cap_vap1.utilization_avg.age < UTIL_AGGR_SECS)
        self.assertTrue(re_vap1.utilization_avg.age > UTIL_AGGR_SECS)

        re_vap1_util = 22
        model.activate()
        model.update_utilizations({'data': {re_vap1_id: re_vap1_util},
                                   'type': 'average'}, re_vap1.ap.ap_id)
        model.shutdown()
        # Will use higher value as aggregated one
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_avg.value)
        self.assertEquals(re_vap1_util, cap_vap1.utilization_aggr.value)
        self.assertEquals(re_vap1_util, re_vap1.utilization_avg.value)
        self.assertEquals(re_vap1_util, re_vap1.utilization_aggr.value)
        # Verify both have fresh util
        self.assertTrue(cap_vap1.utilization_avg.age < UTIL_AGGR_SECS)
        self.assertTrue(re_vap1.utilization_avg.age < UTIL_AGGR_SECS)

        time.sleep(UTIL_AGGR_SECS)

        # New util update comes 2 seconds later, will not compute
        # aggregated value since RE's util is not ready
        cap_vap1_util = 33
        model.activate()
        model.update_utilizations({'data': {cap_vap1_id: cap_vap1_util},
                                   'type': 'average'}, cap_vap1.ap.ap_id)
        model.shutdown()
        # Only CAP has fresh util
        self.assertTrue(cap_vap1.utilization_avg.age < UTIL_AGGR_SECS)
        self.assertTrue(re_vap1.utilization_avg.age > UTIL_AGGR_SECS)
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_avg.value)
        self.assertEquals(re_vap1_util, cap_vap1.utilization_aggr.value)
        self.assertEquals(re_vap1_util, re_vap1.utilization_avg.value)
        self.assertEquals(re_vap1_util, re_vap1.utilization_aggr.value)

        # Receive util update from RE, compute aggregated util
        re_vap1_util = 32
        model.activate()
        model.update_utilizations({'data': {re_vap1_id: re_vap1_util},
                                   'type': 'average'}, re_vap1.ap.ap_id)
        model.shutdown()
        # Will use higher value as aggregated one
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_avg.value)
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_aggr.value)
        self.assertEquals(re_vap1_util, re_vap1.utilization_avg.value)
        self.assertEquals(cap_vap1_util, re_vap1.utilization_aggr.value)

        # For channel with only one VAP, aggregated util is computed for each update
        cap_vap4_util = 50
        model.activate()
        model.update_utilizations({'data': {cap_vap4_id: cap_vap4_util},
                                   'type': 'average'}, cap_vap4.ap.ap_id)
        model.shutdown()
        self.assertEquals(cap_vap4_util, cap_vap4.utilization_avg.value)
        self.assertEquals(cap_vap4_util, cap_vap4.utilization_aggr.value)

        cap_vap4_util = 60
        model.activate()
        model.update_utilizations({'data': {cap_vap4_id: cap_vap4_util},
                                   'type': 'average'}, cap_vap4.ap.ap_id)
        model.shutdown()
        self.assertEquals(cap_vap4_util, cap_vap4.utilization_avg.value)
        self.assertEquals(cap_vap4_util, cap_vap4.utilization_aggr.value)

        # Will not affect other channel
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_avg.value)
        self.assertEquals(cap_vap1_util, cap_vap1.utilization_aggr.value)
        self.assertEquals(re_vap1_util, re_vap1.utilization_avg.value)
        self.assertEquals(cap_vap1_util, re_vap1.utilization_aggr.value)

    def test_tracked_quantity(self):
        """Verify basic functioning of :class:`TrackedQuantity`."""
        util = TrackedQuantity()

        # After init, no value is available
        self.assertEquals(None, util.value)
        self.assertEquals(0, util.seq)
        self.assertEquals(float('inf'), util.age)

        # Update value and check that sequence number and timestamp are
        # updated
        util.update(22)
        self.assertEquals(22, util.value)
        self.assertEquals(1, util.seq)
        self.assertTrue(util.age < 0.1)

        time.sleep(1)
        # Verify age has increased
        self.assertEquals(22, util.value)
        self.assertEquals(1, util.seq)
        self.assertTrue(util.age >= 1)

        # One more update
        util.update(44)
        self.assertEquals(44, util.value)
        self.assertEquals(2, util.seq)
        self.assertTrue(util.age < 0.1)

        # Reset utilization
        util.update(None, 0)
        self.assertEquals(None, util.value)
        self.assertEquals(0, util.seq)

    def test_path(self):
        """Verify basic functioning of :class:`Path`."""
        new_path = Path()

        # After init, no value is available
        self.assertEquals(None, new_path.timestamp)
        self.assertEquals(None, new_path.next_hop)
        self.assertEquals(None, new_path.intf)

        current_time = 49
        time_mock = MagicMock(return_value=current_time)
        with patch('time.time', time_mock):

            # Update from H-Active
            # None in fields we don't care about in the model
            hactive = HActiveRow(None, None, None, 1000,
                                 10, TRANSPORT_PROTOCOL.OTHER, None, None)
            new_path.update_from_hactive("NextHopAP", "NextHopIntf", hactive)

            # Should be able to fetch all attributes
            self.assertEquals(new_path.timestamp, current_time)
            self.assertEquals("NextHopAP", new_path.next_hop)
            self.assertEquals("NextHopIntf", new_path.intf)

            # Not generated from H-Default
            self.assertFalse(new_path.is_updated_from_hdefault())

            # Can't fallback to H-Default path
            self.assertFalse(new_path.fallback_to_hdefault())

            current_time += 1
            time_mock.return_value = current_time

            # Update the backup path
            new_path.update_backup_path("BackupNextHopAP", "BackupNextHopIntf")
            # Primary path unchanged
            self.assertEquals(new_path.timestamp, current_time - 1)
            self.assertEquals("NextHopAP", new_path.next_hop)
            self.assertEquals("NextHopIntf", new_path.intf)
            self.assertFalse(new_path.is_updated_from_hdefault())

            current_time += 1
            time_mock.return_value = current_time

            # Fallback to the default path
            self.assertTrue(new_path.fallback_to_hdefault())

            # Primary path should equal backup path
            self.assertEquals(new_path.timestamp, current_time)
            self.assertEquals("BackupNextHopAP", new_path.next_hop)
            self.assertEquals("BackupNextHopIntf", new_path.intf)
            self.assertTrue(new_path.is_updated_from_hdefault())

            current_time += 1
            time_mock.return_value = current_time

            # Set back to H-Active
            new_path.update_from_hactive("NextHopAP", "NextHopIntf", hactive)
            self.assertEquals(new_path.timestamp, current_time)
            self.assertEquals("NextHopAP", new_path.next_hop)
            self.assertEquals("NextHopIntf", new_path.intf)
            self.assertFalse(new_path.is_updated_from_hdefault())

            current_time += 1
            time_mock.return_value = current_time

            # Set to H-Default
            new_path.update_from_hdefault("HDefaultNextHopAP", "HDefaultNextHopIntf")
            self.assertEquals(new_path.timestamp, current_time)
            self.assertEquals("HDefaultNextHopAP", new_path.next_hop)
            self.assertEquals("HDefaultNextHopIntf", new_path.intf)
            self.assertTrue(new_path.is_updated_from_hdefault())


if __name__ == '__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)

    unittest.main()
