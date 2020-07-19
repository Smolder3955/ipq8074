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

from whcdiag.hydmsgs import ver
from whcdiag.msgs.exception import MessageMalformedError


class TestVerMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Invalid message ID (version)
        msg = ''
        self.assertRaises(
            MessageMalformedError, ver.unpack_payload_from_bytes,
            0, msg)

    def test_version(self):
        """Verify the parsing of the version message."""
        string = ''
        for version in ver.MessageID:
            payload, messageID = ver.unpack_payload_from_bytes(version.value,
                                                               string)

            self.assertEquals(ver.Version._make((version, )), payload)
            self.assertEquals(version, messageID)


if __name__ == '__main__':
    unittest.main()
