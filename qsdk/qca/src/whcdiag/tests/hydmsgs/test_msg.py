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

from whcdiag.hydmsgs import msg
from whcdiag.msgs.exception import MessageMalformedError


class TestMsgMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Empty string
        string = ''
        self.assertRaises(
            MessageMalformedError, msg.unpack_payload_from_bytes,
            5, string)

        # Test 2: 0 length
        self.assertRaises(
            MessageMalformedError, msg.unpack_payload_from_bytes,
            0, string)

    def test_msg(self):
        """Verify the parsing of the custom message."""
        string = '\x54\x65\x73\x74'
        payload, messageID = msg.unpack_payload_from_bytes(4, string)

        self.assertEquals(msg.CustomMessage._make(("Test",)), payload)
        self.assertEquals(msg.MessageID.CUSTOM_MESSAGE, messageID)


if __name__ == '__main__':
    unittest.main()
