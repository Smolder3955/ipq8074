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
from whcdiag.hydmsgs import hetables
from whcdiag.hydmsgs import common
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.constants import TRANSPORT_PROTOCOL


class TestHeTablesMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Invalid message ID
        msg = ''
        self.assertRaises(
            MessageMalformedError, hetables.unpack_payload_from_bytes,
            0, msg)

        # Test 2: Detail message can have 0 rows, so 0 length is OK
        # However, should raise malformed error if non-integer number of rows
        msg = '\x00'
        self.assertRaises(
            MessageMalformedError, hetables.unpack_payload_from_bytes,
            hetables.MessageID.HACTIVE.value, msg)

    def test_hactive(self):
        """Verify the parsing of the detail message."""

        # Parse 0 length message
        string = ''
        payload, messageID = hetables.unpack_payload_from_bytes(hetables.MessageID.HACTIVE.value,
                                                                string)

        self.assertEquals(heservice.Detail._make(([], )), payload)
        self.assertEquals(heservice.MessageID.DETAIL, messageID)

        # 1 row
        da = '\x11\x11\x11\x00\x00\x01'
        id = '\x11\x11\x11\x00\x00\x02'
        string = da + id + '\x00\x00\x00\x01\x00\x00\x00\x02\x03\x01\x45'
        string += '\x00\x04' + '\x65\x74\x68\x30'

        payload, messageID = hetables.unpack_payload_from_bytes(hetables.MessageID.HACTIVE.value,
                                                                string)

        row = heservice.HActiveRow._make(('11:11:11:00:00:01', '11:11:11:00:00:02', 1,
                                          2, 3, TRANSPORT_PROTOCOL.UDP, common.InterfaceType.ETHER,
                                          'eth0'))

        self.assertEquals(heservice.Detail._make(([[row]])), payload)
        self.assertEquals(heservice.MessageID.DETAIL, messageID)


if __name__ == '__main__':
    unittest.main()
