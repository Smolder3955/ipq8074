#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import unittest

from whcdiag.msgs import common
from whcdiag.msgs import wlanif
from whcdiag.msgs.exception import MessageMalformedError

from whcdiag.constants import BAND_TYPE


class TestWlanIFMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Message not even long enough for message ID
        msg = ''
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 2: Invalid message ID
        msg = '\x04\x11'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        # Test 3: Invalid raw channel utilization length (v1)
        msg = '\x00\x00'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 4: Invalid raw channel utilization length (v2)
        msg = '\x00\x64'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 5: Invalid raw RSSI length
        msg = '\x01\xbe\xba\x1d\x23\x32\xfe\x00'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 6: Invalid raw RSSI length (v2)
        msg = '\x01\xbe\xba\x1d\x23\x32\xfe\x64\x00\x00'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 7: Invalid Interface length
        msg = '\x02\xbe\xba\x1d\x23\x32\xfe\x64\x00\x00'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 6: Invalid beacon report length
        msg = '\x03\xbe\xba\x1d\x23\x32\xfe\x64\x00\x00'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

    def test_raw_channel_utilization(self):
        """Verify the parsing of the channel utilization message v1."""
        # Test 1: Measurement on 2.4 GHz
        msg = '\x00\x00\x16'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(wlanif.RawChannelUtilization._make(
            (BAND_TYPE.BAND_24G, 22)), payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        msg = '\x00\x00\x25'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(wlanif.RawChannelUtilization._make(
            (BAND_TYPE.BAND_24G, 37)), payload)

        # Test 3: Utilization measurement on 5 GHz now
        msg = '\x00\x01\x50'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(wlanif.RawChannelUtilization._make(
            (BAND_TYPE.BAND_5G, 80)), payload)

        # Test 4: Same in big endian
        msg = '\x00\x01\x50'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(wlanif.RawChannelUtilization._make(
            (BAND_TYPE.BAND_5G, 80)), payload)

        # Test 5: The invalid enumerated band is rejected
        msg = '\x00\x02\x37'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        # Test 6: Band that is not even contained within the enum
        msg = '\x00\x03\x37'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        """Verify the parsing of the channel utilization message v2."""
        # Test 1: Measurement on APID = 0xFF, channel = 100, essId = 0
        msg = '\x00\x64\x16'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(wlanif.RawChannelUtilization_v2._make(
            (100, 22)), payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        msg = '\x00\x64\x16'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(wlanif.RawChannelUtilization_v2._make(
            (100, 22)), payload)

    def test_raw_rssi(self):
        """Verify the parsing of the RSSI message v1."""
        # Test 1: Measurement on 2.4 GHz
        mac = '\x42\x62\x89\x67\x4d\xb4'
        msg = '\x01' + mac + '\x00\x16'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(wlanif.RawRSSI._make(
            (common.ether_ntoa(mac), BAND_TYPE.BAND_24G, 22)),
            payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        msg = '\x01' + mac + '\x00\x25'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(wlanif.RawRSSI._make(
            (common.ether_ntoa(mac), BAND_TYPE.BAND_24G, 37)),
            payload)

        # Test 3: RSSI measurement on 5 GHz now
        mac = '\xec\x40\xe4\xe7\xf5\xa5'
        msg = '\x01' + mac + '\x01\x50'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(wlanif.RawRSSI._make(
            (common.ether_ntoa(mac), BAND_TYPE.BAND_5G, 80)),
            payload)

        # Test 4: Same in big endian
        msg = '\x01' + mac + '\x01\x47'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(wlanif.RawRSSI._make(
            (common.ether_ntoa(mac), BAND_TYPE.BAND_5G, 71)),
            payload)

        # Test 5: The invalid enumerated band is rejected
        msg = '\x01' + mac + '\x02\x37'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        # Test 6: Band that is not even contained within the enum
        msg = '\x01' + mac + '\x03\x37'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        """Verify the parsing of the RSSI message v2."""
        # Test 1: Measurement on APID = 0xFF, channel = 100, essId = 0
        msg = '\x01' + mac + '\xFF\x64\x00\x16'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(wlanif.RawRSSI_v2._make(
            (common.ether_ntoa(mac), common.BSSInfo(0xFF, 100, 0), 22)),
            payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        msg = '\x01' + mac + '\xFF\x64\x00\x16'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(wlanif.RawRSSI_v2._make(
            (common.ether_ntoa(mac), common.BSSInfo(0xFF, 100, 0), 22)),
            payload)

    def test_interface(self):
        """Verify the parsing of the interface message v2."""
        mac = '\xec\x40\xe4\xe7\xf5\xa5'
        # SSID length + string (null terminated)
        ssid = '\x09' + '\x74\x65\x73\x74\x73\x73\x69\x64\x00'
        # iface name length + string
        iface = '\x04' + '\x61\x74\x68\x30'
        # Fixed length portion: mac, channel, essid
        msg = '\x02' + mac + '\x06\x01' + ssid + iface
        # Test 1: Valid message
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(wlanif.Interface._make(
            (common.ether_ntoa(mac), 6, 1, 9, "testssid\x00", 4, "ath0")),
            payload)

        # Test 2: Same, but in big endian
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(wlanif.Interface._make(
            (common.ether_ntoa(mac), 6, 1, 9, "testssid\x00", 4, "ath0")),
            payload)

        # Test 3: Invalid SSID length
        ssid = '\xff' + '\x74\x65\x73\x74\x73\x73\x69\x64'
        msg = '\x02' + mac + '\x06\x01' + ssid + iface
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, True, msg)

        # Test 4: Invalid interface length
        ssid = '\x08' + '\x74\x65\x73\x74\x73\x73\x69\x64'
        iface = '\xff' + '\x61\x74\x68\x30\x00'
        msg = '\x02' + mac + '\x06\x01' + ssid + iface
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION2, True, msg)

        # Test 5: Not supported in version 1
        iface = '\x05' + '\x61\x74\x68\x30\x00'
        msg = '\x02' + mac + '\x06\x01' + ssid + iface
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

    def test_beacon_report(self):
        mac = '\x42\x62\x89\x67\x4d\xb4'
        # Test 1: Measurement on APID = 0xFF, channel = 100, essId = 0,
        #         RCPI = -10
        msg = '\x03' + mac + '\xFF\x64\x00\xF6'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(wlanif.BeaconReport._make(
            (common.ether_ntoa(mac), common.BSSInfo(0xFF, 100, 0), -10)),
            payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        msg = '\x03' + mac + '\xFF\x64\x00\xF6'
        payload = wlanif.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(wlanif.BeaconReport._make(
            (common.ether_ntoa(mac), common.BSSInfo(0xFF, 100, 0), -10)),
            payload)

        # Test 3: Not supported in version 1
        msg = '\x03' + mac + '\xFF\x64\x00\xF6'
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, wlanif.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

if __name__ == '__main__':
    unittest.main()
