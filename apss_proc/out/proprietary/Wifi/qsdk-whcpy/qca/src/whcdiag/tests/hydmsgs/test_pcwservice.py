#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import unittest

from whcdiag.hydmsgs import pcwservice
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.hydmsgs.ver import MessageID as MessageVersion


class TestPCWServiceMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Invalid message ID
        msg = '\x02\x11'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            0, msg, MessageVersion.VERSION_1)

        # Test 2: Message too short for raw STA
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x00\x00\x05\x00\x00\x00\x06\x00\x00\x00\x07\x00\x00\x00\x08'
        string += '\x00\x00\x00\x09\x00\x00\x00\x0a\x00\x00\x00\x0b\x00\x00\x00\x0c'
        string += '\x00\x00\x00\x0d\x00\x00\x00\x0e'
        string += '\x00\x0f\x00\x10\x00\x11\x00\x12\x00\x13\x00\x14\x00\x15\x00\x16'
        string += '\x00\x17\x00\x18\x00\x19\x00'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.RAW_STA.value, msg, MessageVersion.VERSION_1)

        # Test 3: Message too short for raw AP
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x00'
        string += '\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.RAW_AP.value, msg, MessageVersion.VERSION_1)

        # Test 4: Message too short for raw STA AP
        string = '\x00\x00\x00\x00\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.RAW_STA_AP.value, msg, MessageVersion.VERSION_1)

        # Test 5: Message too short for get MU
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.GET_MU.value, msg, MessageVersion.VERSION_1)

        # Test 5a: get MU not supported in version 2
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.GET_MU.value, msg, MessageVersion.VERSION_2)

        # Test 6: Message too short for get LC
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01\x00\x00\x00\x02' + mac
        string += '\x00\x03\x00\x04\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.GET_LC.value, msg, MessageVersion.VERSION_1)

        # Test 6a: get LC not supported in version 2
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01\x00\x00\x00\x02' + mac
        string += '\x00\x03\x00\x04\x00\x00\x00\x05'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.GET_LC.value, msg, MessageVersion.VERSION_2)

        # Test 7: Message too short for events
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x01\x00\x02\x00\x03\x00\x04\x00' + mac
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.EVENTS.value, msg, MessageVersion.VERSION_1)

        # Test 8: Message too short for summary
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcwservice.unpack_payload_from_bytes,
            pcwservice.MessageID.SUMMARY.value, msg, MessageVersion.VERSION_1)

    def test_raw_sta(self):
        """Verify the parsing of the raw STA message."""
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x00\x00\x05\x00\x00\x00\x06\x00\x00\x00\x07\x00\x00\x00\x08'
        string += '\x00\x00\x00\x09\x00\x00\x00\x0a\x00\x00\x00\x0b\x00\x00\x00\x0c'
        string += '\x00\x00\x00\x0d\x00\x00\x00\x0e'
        string += '\x00\x0f\x00\x10\x00\x11\x00\x12\x00\x13\x00\x14\x00\x15\x00\x16'
        string += '\x00\x17\x00\x18\x00\x19\x00\x1a'

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_STA.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.RawSTA._make(
            (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
             19, 20, 21, 22, 23, 24, 25, 26)), payload)
        self.assertEquals(pcwservice.MessageID.RAW_STA, messageID)

        # Test version 2
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + string

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_STA.value, string, MessageVersion.VERSION_2)

        self.assertEquals(pcwservice.RawSTA_v2._make(
            ('11:11:11:00:00:01', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
             16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26)), payload)
        self.assertEquals(pcwservice.MessageID.RAW_STA, messageID)

    def test_raw_ap(self):
        """Verify the parsing of the raw AP message."""

        # 0 links
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x00'
        string += '\x00\x00\x00\x00'
        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_AP.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.RawAP._make((1, 2, 3, 0, [])), payload)
        self.assertEquals(pcwservice.MessageID.RAW_AP, messageID)

        # 1 link
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x01'
        string += mac + '\x00\x00\x00\x05\x00\x00\x00\x06'
        string += '\x00\x00\x00\x07\x00\x00\x00\x08\x00\x00\x00\x09\x00\x00\x00\x0a'
        string += '\x00\x00\x00\x0b\x00\x00\x00\x0c\x00\x00\x00\x0d\x00\x00\x00\x0e'
        string += '\x00\x00\x00\x0f'

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_AP.value, string, MessageVersion.VERSION_1)
        link = pcwservice.APLink._make(('11:11:11:00:00:01', 5, 6, 7, 8, 9,
                                        10, 11, 12, 13, 14, 15))
        self.assertEquals(pcwservice.RawAP._make(
            (1, 2, 3, 1, [link])), payload)
        self.assertEquals(pcwservice.MessageID.RAW_AP, messageID)

        # Test version 2
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + string

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_AP.value, string, MessageVersion.VERSION_2)
        self.assertEquals(pcwservice.RawAP_v2._make(
            ('11:11:11:00:00:01', 1, 2, 3, 1, [link])), payload)
        self.assertEquals(pcwservice.MessageID.RAW_AP, messageID)

    def test_raw_sta_ap(self):
        """Verify the parsing of the raw STA AP message."""
        # 0 links
        string = '\x00\x00\x00\x00\x00\x00\x00\x01'
        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_STA_AP.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.RawSTAAP._make((0, 1, [])), payload)
        self.assertEquals(pcwservice.MessageID.RAW_STA_AP, messageID)

        # 1 link
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01\x00\x00\x00\x01'
        string += mac + '\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x05'
        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_STA_AP.value, string, MessageVersion.VERSION_1)

        link = pcwservice.STAAPLink._make(('11:11:11:00:00:01', 2, 3, 4, 5))
        self.assertEquals(pcwservice.RawSTAAP._make((1, 1, [link])), payload)
        self.assertEquals(pcwservice.MessageID.RAW_STA_AP, messageID)

        # Test version 2
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + string

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.RAW_STA_AP.value, string, MessageVersion.VERSION_2)

        self.assertEquals(pcwservice.RawSTAAP_v2._make(('11:11:11:00:00:01',
                                                        1, 1, [link])), payload)
        self.assertEquals(pcwservice.MessageID.RAW_STA_AP, messageID)

    def test_get_mu(self):
        """Verify the parsing of the get MU message."""
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03'

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.GET_MU.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.MediumUtilization._make(
            (1, 2, 3)), payload)
        self.assertEquals(pcwservice.MessageID.GET_MU, messageID)

    def test_get_lc(self):
        """Verify the parsing of the get LC message."""
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01\x00\x00\x00\x02' + mac
        string += '\x00\x03\x00\x04\x00\x00\x00\x05'

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.GET_LC.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.LinkCapacity._make(
            (1, 2, '11:11:11:00:00:01', 3, 4, 5)), payload)
        self.assertEquals(pcwservice.MessageID.GET_LC, messageID)

    def test_events(self):
        """Verify the parsing of the events message."""
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x01\x00\x02\x00\x03\x00\x04\x00\x05' + mac

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.EVENTS.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.Events._make(
            (1, 2, 3, 4, 5, '11:11:11:00:00:01')), payload)
        self.assertEquals(pcwservice.MessageID.EVENTS, messageID)

    def test_summary(self):
        """Verify the parsing of the summary message."""
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'

        payload, messageID = pcwservice.unpack_payload_from_bytes(
            pcwservice.MessageID.SUMMARY.value, string, MessageVersion.VERSION_1)

        self.assertEquals(pcwservice.Summary._make(
            ('11:11:11:00:00:01', 1, 2, 3, 4)), payload)
        self.assertEquals(pcwservice.MessageID.SUMMARY, messageID)


if __name__ == '__main__':
    unittest.main()
