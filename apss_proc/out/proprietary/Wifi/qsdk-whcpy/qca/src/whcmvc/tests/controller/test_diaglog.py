#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import unittest
import socket
import time

from mock import MagicMock

from whcdiag.constants import BAND_TYPE

from whcdiag.io.network import NetworkReaderRaw
from whcdiag.msgs import common
from whcdiag.msgs.steerexec import SteeringProhibitType, BTMComplianceType

from whcmvc.model.model_core import ModelBase, AccessPoint, VirtualAccessPoint
from whcmvc.model.model_core import AP_TYPE, CHANNEL_GROUP

from whcmvc.controller.diaglog import MsgHandler
import whcdiag.msgs.parser as diagparser


class TestDiaglog(unittest.TestCase):

    _complete_times = 0

    def _create_device_db(self):
        """Create a fresh device database for each test"""
        self._ap1 = AccessPoint('ap1', AP_TYPE.CAP, '11:11:11:00:00:01')
        self._vap1 = VirtualAccessPoint(self._ap1, '2.4g_id_1', BAND_TYPE.BAND_24G, 'ath1',
                                        'wifi1', 6, CHANNEL_GROUP.CG_24G, 70)
        self._vap2 = VirtualAccessPoint(self._ap1, '5g_id_1', BAND_TYPE.BAND_5G, 'ath0',
                                        'wifi0', 100, CHANNEL_GROUP.CG_5GH, 80)
        self._ap1_addr = ("192.168.1.1", 12345)

        self._ap2 = AccessPoint('ap2', AP_TYPE.RE, '22:22:22:00:00:01')
        self._vap3 = VirtualAccessPoint(self._ap2, '2.4g_id_2', BAND_TYPE.BAND_24G, 'ath1',
                                        'wifi1', 6, CHANNEL_GROUP.CG_24G, 75)
        self._vap4 = VirtualAccessPoint(self._ap2, '5g_id_2', BAND_TYPE.BAND_5G, 'ath0',
                                        'wifi0', 100, CHANNEL_GROUP.CG_5GH, 85)
        self._ap2_addr = ("192.168.1.2", 22345)

        self._ap3 = AccessPoint('ap3', AP_TYPE.RE, '33:33:33:00:00:01')
        self._vap5 = VirtualAccessPoint(self._ap3, '5g_id_3', BAND_TYPE.BAND_5G, 'ath0',
                                        'wifi0', 100, CHANNEL_GROUP.CG_5GH, 70)
        self._ap3_addr = ("192.168.1.3", 32345)

        self._unknown_addr = ("192.168.1.3", 32345)

    def _create_mock_model(self):
        """Create a mocked out :class:`ModelBase` instance."""
        model = ModelBase("fakeDB")
        model.add_associated_station = MagicMock(
            name='add_associated_station')
        model.del_associated_station = MagicMock(
            name='del_associated_station')
        model.update_associated_station_rssi = MagicMock(
            name='update_associated_station_rssi')
        model.update_associated_station_data_metrics = MagicMock(
            name='update_associated_station_data_metrics')
        model.update_associated_station_activity = MagicMock(
            name='update_associated_station_activity')
        model.update_station_flags = MagicMock(
            name='update_station_flags')
        model.update_utilizations = MagicMock(
            name='update_utilizations')
        model.update_overload_status = MagicMock(
            name='update_overload_status')
        model.update_steering_blackout_status = MagicMock(
            name='update_steering_blackout_status')
        model.update_station_steering_status = MagicMock(
            name='update_station_steering_status')
        model.update_station_pollution_state = MagicMock(
            name='update_station_pollution_state')
        return model

    def _dummy_complete_action(self):
        """Dummy shutdown callback function.

        Only record the number of times called
        """
        self._complete_times += 1

    def test_msg_handler_lifecycle(self):
        """Verify the start/shutdown functionality of :class:`MsgHandler`"""
        self._create_device_db()
        model = self._create_mock_model()
        handler = MsgHandler(model, diagparser, None)
        handler.add_ap(self._ap1, "127.0.0.1")

        # Before start, shutdown should do nothing
        handler.shutdown(self._dummy_complete_action)

        # Need to make sure there's enough time for the shutdown complete
        # action  thread to finish.
        time.sleep(0.5)
        self.assertEquals(0, self._complete_times)

        # Start handler
        handler.start()

        # Give it some time to make sure it starts running.
        time.sleep(0.5)

        # Start twice should do nothing
        handler.start()

        # Receive a utilization update, should update model
        send_sock = socket.socket(type=socket.SOCK_DGRAM)
        send_sock.bind(('localhost', 0))
        dest_addr = ('localhost', NetworkReaderRaw.DEFAULT_PORT)
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x00\x37'
        send_sock.sendto(msg, dest_addr)

        # Need to make sure there's enough time for the receive thread to
        # process all of the messages.
        time.sleep(0.5)

        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap1.vap_id: 55}, 'type': 'average'}, self._ap1.ap_id)
        model.update_utilizations.reset_mock()

        # Receive an invalid message, should ignore it
        msg = 'helloworld'
        send_sock.sendto(msg, dest_addr)

        # Need to make sure there's enough time for the receive thread to
        # process all of the messages.
        time.sleep(0.5)
        self.assert_(not model.update_utilizations.called)
        self.assert_(not model.update_associated_station_rssi.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.del_associated_station.called)

        handler.shutdown(self._dummy_complete_action)

        # Need to make sure there's enough time for the shutdown complete
        # action  thread to finish.
        time.sleep(0.5)
        self.assertEquals(1, self._complete_times)

        send_sock.close()

    def test_msg_handler_process_msgs(self):
        """Test process_msgs function, make sure it will update model properly

        Note that we only test valid messages here, since invalid messages will be
        handled by diagparser and not affect the logic here.
        """
        self._create_device_db()
        model = self._create_mock_model()
        handler = MsgHandler(model, diagparser, None)

        mac = '\x20\x02\xaf\xb7\x37\xa2'

        # Case 0: Message from unknown IP address will be ignored
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x00\x37'
        handler.process_msg(self._ap1_addr, msg)
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x01' + mac + '\x01\x10'
        handler.process_msg(self._ap1_addr, msg)
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + '\x01\x01\x01'
        handler.process_msg(self._ap1_addr, msg)
        self.assert_(not model.update_utilizations.called)
        self.assert_(not model.update_associated_station_rssi.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.del_associated_station.called)

        # Register AP1 with diaglog handler
        handler.add_ap(self._ap1, self._ap1_addr[0])

        # Case 1.1: Utilization on 2.4 GHz event should update model
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x00\x37'
        handler.process_msg(self._ap1_addr, msg)
        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap1.vap_id: 55}, 'type': 'average'}, self._ap1.ap_id)
        model.update_utilizations.reset_mock()

        # Case 1.2: RawChannelUtilization on 5 GHz event should update model
        msg = '\x10\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x01\x00\x01\x16'
        handler.process_msg(self._ap1_addr, msg)
        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap2.vap_id: 22}, 'type': 'raw'}, self._ap1.ap_id)
        model.update_utilizations.reset_mock()

        # Case 2.1: RSSIUpdate on 5 GHz event should update model
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x01' + mac + '\x01\x10'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_rssi.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), 16, self._ap1.ap_id)
        model.update_associated_station_rssi.reset_mock()

        # Case 2.2: RawRSSI on 2.4 GHz event should udpate model
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x01\x01' + mac + '\x00\x20'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_rssi.assert_called_once_with(
            self._vap1.vap_id, common.ether_ntoa(mac), 32, self._ap1.ap_id)
        model.update_associated_station_rssi.reset_mock()

        # Case 3.1: Association on 5 GHz event should update model
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + '\x01\x01\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id)
        model.add_associated_station.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), assoc_band=self._vap2.band)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=True)   # starts as active on association
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            is_dual_band=True)  # mark as dual band
        model.del_associated_station.reset_mock()
        model.add_associated_station.reset_mock()
        model.update_associated_station_activity.reset_mock()
        model.update_station_steering_status.reset_mock()
        model.update_station_flags.reset_mock()

        # Case 3.2: Disassociation event should update model
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + '\x02\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), assoc_band=BAND_TYPE.BAND_INVALID)
        model.del_associated_station.reset_mock()
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.update_associated_station_activity.called)
        self.assert_(not model.update_station_flags.called)
        model.update_station_steering_status.reset_mock()

        # Case 4: OverloadChange event should update model
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x03\x02'
        handler.process_msg(self._ap1_addr, msg)
        model.update_overload_status.assert_called_once_with(
            {self._vap1.vap_id: False, self._vap2.vap_id: True},
            self._ap1._ap_id)
        model.update_overload_status.reset_mock()

        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x00\x03'
        handler.process_msg(self._ap1_addr, msg)
        model.update_overload_status.assert_called_once_with(
            {self._vap1.vap_id: True, self._vap2.vap_id: True},
            self._ap1._ap_id)
        model.update_overload_status.reset_mock()

        # Case 5: SteerToBand event
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x00' + mac + '\x01\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), start=True, assoc_band=BAND_TYPE.BAND_5G,
            target_band=BAND_TYPE.BAND_24G)
        model.update_station_steering_status.reset_mock()

        # Case 6: AbortSteerToBand event
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x01' + mac + '\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), abort=True)
        model.update_station_steering_status.reset_mock()

        # Case 7a: ActivityUpdate with active
        # Activity update
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\x01\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=True)
        model.update_associated_station_activity.reset_mock()

        # Case 7b: ActivityUpdate with idle
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\x01\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=False)
        model.update_associated_station_activity.reset_mock()

        # Case 8a: SteeringUnfriendly with marked as unfriendly
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x02' + mac + '\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_unfriendly=True)
        model.update_station_flags.reset_mock()

        # Case 8b: SteeringUnfriendly with marked as friendly
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x02' + mac + '\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_unfriendly=False)
        model.update_station_flags.reset_mock()

        # Case 9a: Dual band update - is dual band
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x03' + mac + '\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_dual_band=True)
        model.update_station_flags.reset_mock()

        # Case 9b: Dual band update - is not dual band
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x03' + mac + '\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_dual_band=False)
        model.update_station_flags.reset_mock()

        # Case 10a: Steering prohibited - is prohibited
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x03' + mac + '\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            prohibit_type=SteeringProhibitType.PROHIBIT_LONG)
        model.update_station_flags.reset_mock()

        # Case 10b: Steering prohibited - is not prohibited
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x03' + mac + '\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            prohibit_type=SteeringProhibitType.PROHIBIT_NONE)
        model.update_station_flags.reset_mock()

        # Case 11: Other messages are ignored
        # Diaglog message
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x08\x00abc'
        handler.process_msg(self._ap1_addr, msg)
        self.assert_(not model.update_utilizations.called)
        self.assert_(not model.update_associated_station_rssi.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.del_associated_station.called)
        self.assert_(not model.update_overload_status.called)

    def test_msg_handler_process_msgs_not_found(self):
        """Cases where something is logged that does not match with the model
        """
        self._create_device_db()
        model = self._create_mock_model()
        handler = MsgHandler(model, diagparser, None)

        mac = '\x20\x02\xaf\xb7\x37\xa2'

        # Register AP3 with diaglog handler
        handler.add_ap(self._ap3, self._ap3_addr[0])

        # Case 1.1: Utilization on 2.4 GHz event should update model
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x00\x37'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_utilizations.called)
        model.update_utilizations.reset_mock()

        # Case 1.2: RawChannelUtilization on 5 GHz event should update model
        msg = '\x10\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x01\x00\x00\x16'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_utilizations.called)
        model.update_utilizations.reset_mock()

        # Case 2.1: RSSIUpdate on 2.4 GHz
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x01' + mac + '\x00\x10'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_rssi.called)
        model.update_associated_station_rssi.reset_mock()

        # Case 2.2: RawRSSI on 2.4 GHz
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x01\x01' + mac + '\x00\x20'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_rssi.called)
        model.update_associated_station_rssi.reset_mock()

        # Case 3: Association on 2.4 GHz event should update model
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + '\x00\x01\x01'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.del_associated_station.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.update_station_steering_status.called)
        self.assert_(not model.update_associated_station_activity.called)
        self.assert_(not model.update_station_flags.called)
        model.del_associated_station.reset_mock()
        model.add_associated_station.reset_mock()
        model.update_associated_station_activity.reset_mock()
        model.update_station_steering_status.reset_mock()
        model.update_station_flags.reset_mock()

        # Case 4: ActivityUpdate with active
        # Activity update
        msg = '\x10\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\x00\x01'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_activity.called)
        model.update_associated_station_activity.reset_mock()

        # Case 5: OverloadChange event should update model
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x03\x02'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_overload_status.called)
        model.update_overload_status.reset_mock()

    def test_msg_handler_process_msgs_v2(self):
        """Test process_msgs function, make sure it will update model properly

        Note that we only test valid messages here, since invalid messages will be
        handled by diagparser and not affect the logic here.
        """
        self._create_device_db()
        model = self._create_mock_model()
        handler = MsgHandler(model, diagparser, None)
        handler.add_ap(self._ap1, self._ap1_addr[0])

        # Case 1.1: Utilization on 2.4 GHz event should update model
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x06\x37'
        handler.process_msg(self._ap1_addr, msg)
        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap1.vap_id: 55}, 'type': 'average'}, self._ap1.ap_id)
        model.update_utilizations.reset_mock()

        # Case 1.2: RawChannelUtilization on 5 GHz event should update model
        msg = '\x20\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x01\x00\x64\x16'
        handler.process_msg(self._ap1_addr, msg)
        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap2.vap_id: 22}, 'type': 'raw'}, self._ap1.ap_id)
        model.update_utilizations.reset_mock()

        mac = '\x20\x02\xaf\xb7\x37\xa2'
        # Case 2.1: RSSIUpdate on 5 GHz event should update model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x01' + mac + '\xFF\x64\x00\x10'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_rssi.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), 16, self._ap1.ap_id)
        model.update_associated_station_rssi.reset_mock()

        # Case 2.2: RawRSSI on 2.4 GHz event should udpate model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x01\x01' + mac + '\xFF\x06\x00\x20'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_rssi.assert_called_once_with(
            self._vap1.vap_id, common.ether_ntoa(mac), 32, self._ap1.ap_id)
        model.update_associated_station_rssi.reset_mock()

        # Case 3.1: Association on 5 GHz event should update model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + \
              '\xFF\x64\x00\x01\x01\x01\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id)
        model.add_associated_station.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), assoc_vap=self._vap2)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=True)   # starts as active on association
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            is_dual_band=True)  # mark as dual band
        model.del_associated_station.reset_mock()
        model.add_associated_station.reset_mock()
        model.update_station_steering_status.reset_mock()
        model.update_associated_station_activity.reset_mock()
        model.update_station_flags.reset_mock()

        # Case 3.2: Disassociation event should update model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + \
              '\xFF\x64\x00\x00\x01\x01\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), assoc_vap=None, ap_id=self._ap1.ap_id)
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.reset_mock()
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.update_associated_station_activity.called)
        self.assert_(not model.update_station_flags.called)
        model.update_station_steering_status.reset_mock()

        # Case 4: OverloadChange event should update model
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x01\x64'
        handler.process_msg(self._ap1_addr, msg)
        model.update_overload_status.assert_called_once_with(
            {self._vap1.vap_id: False, self._vap2.vap_id: True},
            self._ap1._ap_id)
        model.update_overload_status.reset_mock()

        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x02\x06\x64'
        handler.process_msg(self._ap1_addr, msg)
        model.update_overload_status.assert_called_once_with(
            {self._vap1.vap_id: True, self._vap2.vap_id: True},
            self._ap1._ap_id)
        model.update_overload_status.reset_mock()

        # Case 5: PostAssocSteer event
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x64\x00\x01\xFF\x06\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), start=True, assoc_vap=self._vap2,
            target_vaps=[self._vap1])
        model.update_station_steering_status.reset_mock()

        # Case 6a: SteerEnd event with a non-success
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x01' + mac + '\x51\x01\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), abort=True)
        model.update_station_steering_status.reset_mock()

        # Case 6b: SteerEnd event with a success; should be ignored
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x01' + mac + '\x51\x01\x00'
        handler.process_msg(self._ap1_addr, msg)
        self.assert_(not model.update_station_steering_status.called)

        # Case 7: Serving data metrics event
        msg = '\x20\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x09\x00' + mac + \
              '\xff\x06\x01\x2d\x00\x00\x00\x08\x00\x00\x00\x50\x00\x00\x00\x42'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_data_metrics.assert_called_once_with(
            self._vap1.vap_id, common.ether_ntoa(mac), 45 + 8, 80, 66,
            self._ap1.ap_id)
        model.update_associated_station_data_metrics.reset_mock()

        # Case 8a: Activity update - active
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\xFF\x64\x00\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=True)
        model.update_associated_station_activity.reset_mock()

        # Case 8b: Activity update - idle
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\xFF\x64\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=False)
        model.update_associated_station_activity.reset_mock()

        # Case 9a: SteeringUnfriendly event - unfriendly
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x02' + mac + \
              '\x01\x00\x00\x00\x05'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_unfriendly=True)
        model.update_station_flags.reset_mock()

        # Case 9b: SteeringUnfriendly event - friendly
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x02' + mac + \
              '\x00\x00\x00\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_unfriendly=False)
        model.update_station_flags.reset_mock()

        # Case 10a: Dual band update - is dual band
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x03' + mac + '\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_dual_band=True)
        model.update_station_flags.reset_mock()

        # Case 10b: Dual band update - is not dual band
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x03' + mac + '\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, is_dual_band=False)
        model.update_station_flags.reset_mock()

        # Case 11a: Steering prohibited - long prohibit
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x03' + mac + '\x02'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            prohibit_type=SteeringProhibitType.PROHIBIT_LONG)
        model.update_station_flags.reset_mock()

        # Case 11b: Steering prohibited - short prohibit
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x03' + mac + '\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            prohibit_type=SteeringProhibitType.PROHIBIT_SHORT)
        model.update_station_flags.reset_mock()

        # Case 11c: Steering prohibited - no prohibit
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x03' + mac + '\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            prohibit_type=SteeringProhibitType.PROHIBIT_NONE)
        model.update_station_flags.reset_mock()

        # Case 12a: BTMCompliance - idle friendly
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x04' + mac + \
              '\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            btm_compliance=BTMComplianceType.BTM_COMPLIANCE_IDLE,
            is_btm_unfriendly=False)
        model.update_station_flags.reset_mock()

        # Case 12b: BTMCompliance - active unfriendly but still BTM friendly
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x04' + mac + \
              '\x00\x01\x00\x00\x00\x00\x00\x00\x00\x02'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            btm_compliance=BTMComplianceType.BTM_COMPLIANCE_ACTIVE_UNFRIENDLY,
            is_btm_unfriendly=False)
        model.update_station_flags.reset_mock()

        # Case 12c: BTMCompliance - active unfriendly and not BTM friendly
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x04' + mac + \
              '\x01\x01\x00\x00\x00\x00\x00\x00\x00\x02'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            btm_compliance=BTMComplianceType.BTM_COMPLIANCE_ACTIVE_UNFRIENDLY,
            is_btm_unfriendly=True)
        model.update_station_flags.reset_mock()

        # Case 12d: BTMCompliance - active and BTM friendly
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x04' + mac + \
              '\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            btm_compliance=BTMComplianceType.BTM_COMPLIANCE_ACTIVE,
            is_btm_unfriendly=False)
        model.update_station_flags.reset_mock()

        # Case 13a: BlackoutChange - blackout true
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x02\x01'
        handler.process_msg(self._ap1_addr, msg)
        model.update_steering_blackout_status.assert_called_once_with(
            True, self._ap1._ap_id)
        model.update_steering_blackout_status.reset_mock()

        # Case 13b: BlackoutChange - blackout false
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x02\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_steering_blackout_status.assert_called_once_with(
            False, self._ap1._ap_id)
        model.update_steering_blackout_status.reset_mock()

        # Case 14a: Pollution state change - polluted
        msg = "\x20\x66\x14\x9e\x33\x57\x07\x4a\x05\x00\x09\x03" + mac + \
              "\xff\x06\x00\x01\x02"
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_pollution_state.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, self._vap1, True)
        model.update_station_pollution_state.reset_mock()

        # Case 14b: Pollution state change - not polluted
        msg = "\x21\x66\x14\x9e\x33\x57\x07\x4a\x05\x00\x09\x03" + mac + \
              "\xff\x64\x00\x00\x02"
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_pollution_state.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id, self._vap2, False)
        model.update_station_pollution_state.reset_mock()

        # Case 14c: Pollution state change from unknown BSS, ignore
        msg = "\x20\x66\x14\x9e\x33\x57\x07\x4a\x05\x00\x09\x03" + mac + \
              "\xff\x01\x00\x00\x02"
        handler.process_msg(self._ap1_addr, msg)
        self.assert_(not model.update_station_pollution_state.called)

        # Case 15: Other events should be ignored
        # Diaglog message
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x08\x00abc'
        handler.process_msg(self._ap1_addr, msg)
        self.assert_(not model.update_utilizations.called)
        self.assert_(not model.update_associated_station_rssi.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.del_associated_station.called)
        self.assert_(not model.update_overload_status.called)

    def test_msg_handler_process_msgs_v2_not_found(self):
        """Test process_msgs function, make sure it will update model properly

        Note that we only test valid messages here, since invalid messages will be
        handled by diagparser and not affect the logic here.
        """
        self._create_device_db()
        model = self._create_mock_model()
        handler = MsgHandler(model, diagparser, None)
        handler.add_ap(self._ap3, self._ap3_addr[0])

        # Case 1.1: Utilization on 2.4 GHz
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x06\x37'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_utilizations.called)
        model.update_utilizations.reset_mock()

        # Case 1.2: RawChannelUtilization on 2.4 GHz
        msg = '\x20\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x01\x00\x06\x16'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_utilizations.called)
        model.update_utilizations.reset_mock()

        mac = '\x20\x02\xaf\xb7\x37\xa2'
        # Case 2.1: RSSIUpdate on 2.4 GHz
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x01' + mac + '\xFF\x06\x00\x10'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_rssi.called)
        model.update_associated_station_rssi.reset_mock()

        # Case 2.2: RawRSSI on 2.4 GHz
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x01\x01' + mac + '\xFF\x06\x00\x20'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_rssi.called)
        model.update_associated_station_rssi.reset_mock()

        # Case 3.1: Association on 5 GHz
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + \
              '\xFF\x06\x00\x01\x01\x01\x00\x00'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.del_associated_station.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.update_station_steering_status.called)
        self.assert_(not model.update_associated_station_activity.called)
        self.assert_(not model.update_station_flags.called)
        model.del_associated_station.reset_mock()
        model.add_associated_station.reset_mock()
        model.update_station_steering_status.reset_mock()
        model.update_associated_station_activity.reset_mock()
        model.update_station_flags.reset_mock()

        # Case 3.2: Disassociation
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + \
              '\xFF\x06\x00\x00\x01\x01\x00\x00'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.del_associated_station.called)
        self.assert_(not model.update_station_steering_status.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.update_associated_station_activity.called)
        self.assert_(not model.update_station_flags.called)
        model.del_associated_station.reset_mock()
        model.add_associated_station.reset_mock()
        model.update_station_steering_status.reset_mock()
        model.update_associated_station_activity.reset_mock()
        model.update_station_flags.reset_mock()

        # Case 4: OverloadChange event should update model
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x01\x06'
        handler.process_msg(self._ap3_addr, msg)
        model.update_overload_status.assert_called_once_with(
            {self._vap5.vap_id: False}, self._ap3._ap_id)
        model.update_overload_status.reset_mock()

        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x00\x02\x06\x64'
        handler.process_msg(self._ap3_addr, msg)
        model.update_overload_status.assert_called_once_with(
            {self._vap5.vap_id: True}, self._ap3._ap_id)
        model.update_overload_status.reset_mock()

        # Case 5a: PostAssocSteer event with assoc invalid
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x06\x00\x01\xFF\x64\x00'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_station_steering_status.called)
        model.update_station_steering_status.reset_mock()

        # Case 5b: PostAssocSteer event with all targets invalid
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x06\x00\x01\xFF\x0b\x00'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_station_steering_status.called)
        model.update_station_steering_status.reset_mock()

        # Case 5c: PostAssocSteer event with one target invalid
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x64\x00\x02\xFF\x0b\x00\xFF\x64\x00'
        handler.process_msg(self._ap3_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), start=True, assoc_vap=self._vap5,
            target_vaps=[self._vap5])
        model.update_station_steering_status.reset_mock()

        # Case 6: Serving data metrics event
        msg = '\x20\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x09\x00' + mac + \
              '\xff\x06\x01\x2d\x00\x00\x00\x08\x00\x00\x00\x50\x00\x00\x00\x42'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_data_metrics.called)
        model.update_associated_station_data_metrics.reset_mock()

        # Case 7a: Activity update - active
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\xFF\x06\x00\x01'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_activity.called)
        model.update_associated_station_activity.reset_mock()

        # Case 7b: Activity update - idle
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x02' + mac + '\xFF\x06\x00\x00'
        handler.process_msg(self._ap3_addr, msg)
        self.assert_(not model.update_associated_station_activity.called)
        model.update_associated_station_activity.reset_mock()

    def test_msg_handler_multi_ap(self):
        """Test process_msgs when there are multiple AP registered.

        Make sure it will update model with the correct AP properly.
        Note that we only test valid messages here, since invalid messages will be
        handled by diagparser and not affect the logic here.
        """
        self._create_device_db()
        model = self._create_mock_model()
        handler = MsgHandler(model, diagparser, None)
        handler.add_ap(self._ap1, self._ap1_addr[0])
        # Each AP must have unique IP address
        self.assertRaises(ValueError, handler.add_ap, self._ap2, self._ap1_addr[0])

        mac = '\x20\x02\xaf\xb7\x37\xa2'

        # Case 0: PostAssocSteer event, including 2 targets, should ignore remote BSS
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x64\x00\x02\xFF\x06\x00\x00\x64\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), start=True, assoc_vap=self._vap2,
            target_vaps=[self._vap1])
        model.update_station_steering_status.reset_mock()

        # Add second AP
        handler.add_ap(self._ap2, self._ap2_addr[0])

        # Case 1.1: Utilization on 2.4 GHz event for AP1 should update model
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x06\x37'
        handler.process_msg(self._ap1_addr, msg)
        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap1.vap_id: 55}, 'type': 'average'}, self._ap1.ap_id)
        model.update_utilizations.reset_mock()

        # Case 1.2: RawChannelUtilization on 5 GHz event for AP2 should update model
        msg = '\x20\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x01\x00\x64\x16'
        handler.process_msg(self._ap2_addr, msg)
        model.update_utilizations.assert_called_once_with(
            {'data': {self._vap4.vap_id: 22}, 'type': 'raw'}, self._ap2.ap_id)
        model.update_utilizations.reset_mock()

        # Case 2.1: RSSIUpdate on 5 GHz event on AP1 should update model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x01' + mac + '\xFF\x64\x00\x10'
        handler.process_msg(self._ap1_addr, msg)
        model.update_associated_station_rssi.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), 16, self._ap1.ap_id)
        model.update_associated_station_rssi.reset_mock()

        # Case 2.2: RawRSSI on 2.4 GHz event on AP2 should udpate model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x01\x01' + mac + '\xFF\x06\x00\x20'
        handler.process_msg(self._ap2_addr, msg)
        model.update_associated_station_rssi.assert_called_once_with(
            self._vap3.vap_id, common.ether_ntoa(mac), 32, self._ap2.ap_id)
        model.update_associated_station_rssi.reset_mock()

        # Case 3.1: PostAssocSteer event, including 2 targets, one from local,
        #           one from the other AP
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x64\x00\x02\xFF\x06\x00\x00\x64\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), start=True, assoc_vap=self._vap2,
            target_vaps=[self._vap1, self._vap4])
        model.update_station_steering_status.reset_mock()

        # Case 3.2: PostAssocSteer event, including 2 targets, one from the
        #           other AP, one from unknown AP, will ignore the unknown AP for now
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x05' + mac + \
              '\x51\x01\x01\xFF\x64\x00\x02\x00\x64\x00\x01\x64\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), start=True, assoc_vap=self._vap2,
            target_vaps=[self._vap4])
        model.update_station_steering_status.reset_mock()

        # Case 4.1: Association on 5 GHz event should update model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + \
              '\xFF\x64\x00\x01\x01\x01\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        # Will disassociate on both APs
        calls = model.del_associated_station.call_args_list
        self.assertEquals(2, len(calls))
        self.assertEquals(((common.ether_ntoa(mac), self._ap1.ap_id),), calls[0])
        self.assertEquals(((common.ether_ntoa(mac), self._ap2.ap_id),), calls[1])
        model.add_associated_station.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), assoc_vap=self._vap2)
        model.update_associated_station_activity.assert_called_once_with(
            self._vap2.vap_id, common.ether_ntoa(mac), self._ap1.ap_id,
            is_active=True)   # starts as active on association
        model.update_station_flags.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id,
            is_dual_band=True)  # mark as dual band
        model.del_associated_station.reset_mock()
        model.add_associated_station.reset_mock()
        model.update_station_steering_status.reset_mock()
        model.update_associated_station_activity.reset_mock()
        model.update_station_flags.reset_mock()

        # Case 4.2: Disassociation event should update model
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x05\x00' + mac + \
              '\xFF\x64\x00\x00\x01\x01\x00\x00'
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.assert_called_once_with(
            common.ether_ntoa(mac), self._ap1.ap_id)
        model.update_station_steering_status.assert_called_once_with(
            common.ether_ntoa(mac), assoc_vap=None, ap_id=self._ap1.ap_id)
        handler.process_msg(self._ap1_addr, msg)
        model.del_associated_station.reset_mock()
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.update_associated_station_activity.called)
        self.assert_(not model.update_station_flags.called)
        model.update_station_steering_status.reset_mock()

        # Case 5.1: Pollution state change on remote BSS
        msg = "\x20\x66\x14\x9e\x33\x57\x07\x4a\x05\x00\x09\x03" + mac + \
              "\x00\x06\x00\x01\x02"
        handler.process_msg(self._ap2_addr, msg)
        model.update_station_pollution_state.assert_called_once_with(
            common.ether_ntoa(mac), self._ap2.ap_id, self._vap1, True)
        model.update_station_pollution_state.reset_mock()

        # Case 5.2 Pollution state change on unknown remote BSS is ignored
        msg = "\x20\x66\x14\x9e\x33\x57\x07\x4a\x05\x00\x09\x03" + mac + \
              "\x01\x06\x00\x01\x02"
        handler.process_msg(self._ap2_addr, msg)
        self.assert_(not model.update_station_pollution_state.called)

        # Case 0: Message with unknown IP address should do nothing
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x06\x37'
        handler.process_msg(self._unknown_addr, msg)
        msg = '\x20\x0b\x29\x1f\x10\x01\x96\x0f\x07\x00\x01\x01' + mac + '\xFF\x06\x00\x20'
        handler.process_msg(self._unknown_addr, msg)
        self.assert_(not model.update_utilizations.called)
        self.assert_(not model.update_associated_station_rssi.called)
        self.assert_(not model.add_associated_station.called)
        self.assert_(not model.del_associated_station.called)


if __name__ == '__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)

    unittest.main()
