#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import unittest

from whcdiag.hydmsgs import pcpservice
from whcdiag.msgs.exception import MessageMalformedError


class TestPCPServiceMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Invalid message ID
        msg = '\x02\x11'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            0, msg)

        # Test 2: Message too short for RawMU
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x00\x00\x05\x00\x00\x00\x06\x00\x00\x00\x07\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.RAW_MU.value, msg)

        # Test 3: Message too short for RawLC
        string = '\x00\x00\x00\x01\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.RAW_LC.value, msg)

        # Test 4: Message too short for GetMU
        string = '\x00\x00\x00\x01\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.GET_MU.value, msg)

        # Test 5: Message too short for GetLC
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01' + mac
        string += '\x00\x02\x00\x03\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.GET_LC.value, msg)

        # Test 6: Message too short for Events
        string = '\x00\x01\x00\x02\x00\x03\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.EVENTS.value, msg)

        # Test 7: Message too short for Summary
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x00\x00\x05\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.SUMMARY.value, msg)

        # Test 8: Message too short for RawHostIF
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, pcpservice.unpack_payload_from_bytes,
            pcpservice.MessageID.RAW_HOST_IF.value, msg)

    def test_raw_MU(self):
        """Verify the parsing of the raw MU message."""
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x00\x00\x05\x00\x00\x00\x06\x00\x00\x00\x07\x00\x00\x00\x08'
        payload, messageID = pcpservice.unpack_payload_from_bytes(pcpservice.MessageID.RAW_MU.value,
                                                                  string)

        self.assertEquals(pcpservice.RawMU._make(
            (1, 2, 3, 4, 5, 6, 7, 8)), payload)
        self.assertEquals(pcpservice.MessageID.RAW_MU, messageID)

    def test_raw_LC(self):
        """Verify the parsing of the raw LC message."""

        # 0 links
        string = '\x00\x00\x00\x01\x00\x00\x00\x00'
        payload, messageID = pcpservice.unpack_payload_from_bytes(pcpservice.MessageID.RAW_LC.value,
                                                                  string)

        self.assertEquals(pcpservice.RawLC._make((1, 0, [])), payload)
        self.assertEquals(pcpservice.MessageID.RAW_LC, messageID)

        # 1 link
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01\x00\x00\x00\x01'
        string += mac + '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        payload, messageID = pcpservice.unpack_payload_from_bytes(pcpservice.MessageID.RAW_LC.value,
                                                                  string)

        link = pcpservice.RawLCLink._make(('11:11:11:00:00:01', 1, 2, 3, 4))
        self.assertEquals(pcpservice.RawLC._make((1, 1, [link])), payload)
        self.assertEquals(pcpservice.MessageID.RAW_LC, messageID)

    def test_get_MU(self):
        """Verify the parsing of the GetMU message."""

        string = '\x00\x00\x00\x01\x00\x00\x00\x02'
        payload, messageID = pcpservice.unpack_payload_from_bytes(pcpservice.MessageID.GET_MU.value,
                                                                  string)

        self.assertEquals(pcpservice.GetMU._make((1, 2)), payload)
        self.assertEquals(pcpservice.MessageID.GET_MU, messageID)

    def test_get_LC(self):
        """Verify the parsing of the GetLC message."""
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01' + mac
        string += '\x00\x02\x00\x03\x00\x00\x00\x04'
        payload, messageID = pcpservice.unpack_payload_from_bytes(pcpservice.MessageID.GET_LC.value,
                                                                  string)

        self.assertEquals(pcpservice.GetLC._make(
            (1, '11:11:11:00:00:01', 2, 3, 4)), payload)
        self.assertEquals(pcpservice.MessageID.GET_LC, messageID)

    def test_events(self):
        """Verify the parsing of the events message."""
        string = '\x00\x01\x00\x02\x00\x03\x00\x04'
        payload, messageID = pcpservice.unpack_payload_from_bytes(pcpservice.MessageID.EVENTS.value,
                                                                  string)

        self.assertEquals(pcpservice.Events._make((1, 2, 3, 4)), payload)
        self.assertEquals(pcpservice.MessageID.EVENTS, messageID)

    def test_summary(self):
        """Verify the parsing of the SUMMARY message."""
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x00\x00\x05\x00\x00\x00\x06'
        payload, messageID = pcpservice.unpack_payload_from_bytes(
            pcpservice.MessageID.SUMMARY.value, string)

        self.assertEquals(pcpservice.Summary._make(
            ('11:11:11:00:00:01', 1, 2, 3, 4, 5, 6)), payload)
        self.assertEquals(pcpservice.MessageID.SUMMARY, messageID)

    def test_raw_host_IF(self):
        """Verify the parsing of the RawHostIF message."""
        string = '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03'
        payload, messageID = pcpservice.unpack_payload_from_bytes(
            pcpservice.MessageID.RAW_HOST_IF.value, string)

        self.assertEquals(pcpservice.RawHostIF._make((1, 2, 3)), payload)
        self.assertEquals(pcpservice.MessageID.RAW_HOST_IF, messageID)


if __name__ == '__main__':
    unittest.main()
