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
from whcdiag.msgs import diaglog
from whcdiag.msgs.exception import MessageMalformedError


class TestDiagLogMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Message not even long enough for message ID
        msg = ''
        self.assertRaises(
            MessageMalformedError, diaglog.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 2: Invalid message ID
        msg = '\x01\x11'
        self.assertRaises(
            MessageMalformedError, diaglog.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, diaglog.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

    def test_string_message(self):
        """Verify the parsing of the generic string message."""
        # Test 1: One byte string
        msg = '\x00a'
        payload = diaglog.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(diaglog.StringMessage._make(
            (msg[1:],)), payload)

        # Same in big endian (no difference)
        payload = diaglog.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(diaglog.StringMessage._make(
            (msg[1:],)), payload)

        # Test 2: Multi-byte string
        msg = '\x00hello world'
        payload = diaglog.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(diaglog.StringMessage._make(
            (msg[1:],)), payload)

if __name__ == '__main__':
    unittest.main()
