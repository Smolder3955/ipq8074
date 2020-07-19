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

from whcdiag.hydmsgs import common
from whcdiag.hydmsgs import pstables
from whcdiag.msgs.exception import MessageMalformedError


class TestPSTablesMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Invalid message ID
        msg = '\x00'
        self.assertRaises(
            MessageMalformedError, pstables.unpack_payload_from_bytes,
            0, msg)

        # H-Default message can have 0 rows, so 0 length is OK
        # However, should raise malformed error if non-integer number of rows
        msg = '\x00'
        self.assertRaises(
            MessageMalformedError, pstables.unpack_payload_from_bytes,
            pstables.MessageID.HDEFAULT.value, msg)

    def test_hdefault(self):
        """Verify the parsing of the H-Default message."""

        # Parse 0 length message
        string = ''
        payload, messageID = pstables.unpack_payload_from_bytes(pstables.MessageID.HDEFAULT.value,
                                                                string)

        self.assertEquals(pstables.HDefault._make(([], )), payload)
        self.assertEquals(pstables.MessageID.HDEFAULT, messageID)

        # 2 rows
        da = '\x11\x11\x11\x00\x00\x01'
        id = '\x11\x11\x11\x00\x00\x02'
        string = da + id + '\x45\x35'
        string += '\x00\x04' + '\x65\x74\x68\x30'
        string += '\x00\x05' + '\x61\x74\x68\x30\x31'
        da = '\x11\x11\x22\x00\x00\x01'
        id = '\x11\x11\x22\x00\x00\x02'
        string += da + id + '\x45\x35'
        string += '\x00\x04' + '\x65\x74\x68\x31'
        string += '\x00\x05' + '\x61\x74\x68\x31\x31'

        payload, messageID = pstables.unpack_payload_from_bytes(pstables.MessageID.HDEFAULT.value,
                                                                string)

        row = pstables.HDefaultRow._make(('11:11:11:00:00:01', '11:11:11:00:00:02',
                                          common.InterfaceType.ETHER,
                                          common.InterfaceType.WLAN5G,
                                          'eth0', 'ath01'))

        row1 = pstables.HDefaultRow._make(('11:11:22:00:00:01', '11:11:22:00:00:02',
                                           common.InterfaceType.ETHER,
                                           common.InterfaceType.WLAN5G,
                                           'eth1', 'ath11'))

        self.assertEquals(pstables.HDefault._make(([[row, row1]])), payload)
        self.assertEquals(pstables.MessageID.HDEFAULT, messageID)


if __name__ == '__main__':
    unittest.main()
