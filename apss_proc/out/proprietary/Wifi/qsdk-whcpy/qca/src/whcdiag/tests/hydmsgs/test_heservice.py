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

from whcdiag.hydmsgs import heservice
from whcdiag.hydmsgs import common
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.constants import TRANSPORT_PROTOCOL


class TestHeServiceMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Invalid message ID
        msg = '\x00\x01\x45\x00\x00\x03\xe8\x00\x00\x00\x01\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, heservice.unpack_payload_from_bytes,
            0, msg)

        # Test 2: Message too short for summary
        self.assertRaises(
            MessageMalformedError, heservice.unpack_payload_from_bytes,
            heservice.MessageID.SUMMARY.value, msg)

        # Test 3: Detail message can have 0 rows, so 0 length is OK
        # However, should raise malformed error if non-integer number of rows
        msg = '\x00'
        self.assertRaises(
            MessageMalformedError, heservice.unpack_payload_from_bytes,
            heservice.MessageID.DETAIL.value, msg)

    def test_summary(self):
        """Verify the parsing of the summary message."""
        string = '\x00\x01\x45\x00\x00\x03\xe8\x00\x00\x00\x01\x00\x00\x00\x02'
        payload, messageID = heservice.unpack_payload_from_bytes(heservice.MessageID.SUMMARY.value,
                                                                 string)

        self.assertEquals(heservice.Summary._make((1, common.InterfaceType.ETHER,
                                                   1000, 1, 2)), payload)
        self.assertEquals(heservice.MessageID.SUMMARY, messageID)

    def test_detail(self):
        """Verify the parsing of the detail message."""

        # Parse 0 length message
        string = ''
        payload, messageID = heservice.unpack_payload_from_bytes(heservice.MessageID.DETAIL.value,
                                                                 string)

        self.assertEquals(heservice.Detail._make(([], )), payload)
        self.assertEquals(heservice.MessageID.DETAIL, messageID)

        # 1 row
        da = '\x11\x11\x11\x00\x00\x01'
        id = '\x11\x11\x11\x00\x00\x02'
        string = da + id + '\x00\x00\x00\x01\x00\x00\x00\x02\x03\x01\x45'
        string += '\x00\x04' + '\x65\x74\x68\x30'

        payload, messageID = heservice.unpack_payload_from_bytes(heservice.MessageID.DETAIL.value,
                                                                 string)

        row = heservice.HActiveRow._make(('11:11:11:00:00:01', '11:11:11:00:00:02', 1,
                                          2, 3, TRANSPORT_PROTOCOL.UDP, common.InterfaceType.ETHER,
                                          'eth0'))

        self.assertEquals(heservice.Detail._make(([[row]])), payload)
        self.assertEquals(heservice.MessageID.DETAIL, messageID)


if __name__ == '__main__':
    unittest.main()
