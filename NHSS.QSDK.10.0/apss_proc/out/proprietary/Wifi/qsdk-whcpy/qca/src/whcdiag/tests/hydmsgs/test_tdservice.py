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

from whcdiag.hydmsgs import tdservice
from whcdiag.msgs.exception import MessageMalformedError


class TestTDServiceMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Invalid message ID
        msg = '\x00\x04' + '\x61\x74\x68'
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            0, msg)

        # Test 2: Message too short for link up
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.LINK_UP.value, msg)

        # Test 3: Message too short for link down
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.LINK_DOWN.value, msg)

        # Test 4: Message too short for remote IF up
        msg = '\x11\x11\x11\x00\x00'
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.REMOTE_IF_UP.value, msg)

        # Test 5: Message too short for remote IF down
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.REMOTE_IF_DOWN.value, msg)

        # Test 6: Message too short for remote dev up
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.REMOTE_DEV_UP.value, msg)

        # Test 7: Message too short for remote dev down
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.REMOTE_DEV_DOWN.value, msg)

        # Test 8: Aging is a NULL message, so can take an empty string

        # Test 9: Message too short for bridged DA change
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.BDA_CHANGE.value, msg)

        # Test 10: Message too short for entry
        mac = '\x11\x11\x11\x00\x00\x01'
        msg = mac + '\x00\x00\x00\x00\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.ENTRY.value, msg)

        # Test 11: Message too short for number of interfaces
        msg = mac + '\x00\x00\x00\x01'
        msg += '\x00\x00' + '\x00\x01' + '\x00\x00\x00\x01\x00'
        msg += '\x00\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.ENTRY.value, msg)

        # Test 12: Message too short for number of bridged DAs
        msg = mac + '\x00\x00\x00\x00'
        msg += '\x00\x00\x00\x01'
        msg += '\x00\x00\x00\x01\x01'
        self.assertRaises(
            MessageMalformedError, tdservice.unpack_payload_from_bytes,
            tdservice.MessageID.ENTRY.value, msg)

    def test_link_up(self):
        """Verify the parsing of the link up message."""
        string = '\x00\x04' + '\x61\x74\x68\x30'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.LINK_UP.value, string)

        self.assertEquals(tdservice.LinkUp._make(('ath0', )), payload)
        self.assertEquals(tdservice.MessageID.LINK_UP, messageID)

    def test_link_down(self):
        """Verify the parsing of the link down message."""
        string = '\x00\x04' + '\x61\x74\x68\x30'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.LINK_DOWN.value, string)

        self.assertEquals(tdservice.LinkDown._make(('ath0', )), payload)
        self.assertEquals(tdservice.MessageID.LINK_DOWN, messageID)

    def test_remote_if_up(self):
        """Verify the parsing of the remote IF up message."""
        string = '\x11\x11\x11\x00\x00\x01'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.REMOTE_IF_UP.value, string)

        self.assertEquals(tdservice.RemoteIfaceUp._make(
            ('11:11:11:00:00:01', )), payload)
        self.assertEquals(tdservice.MessageID.REMOTE_IF_UP, messageID)

    def test_remote_if_down(self):
        """Verify the parsing of the remote IF down message."""
        string = '\x11\x11\x11\x00\x00\x01'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.REMOTE_IF_DOWN.value, string)

        self.assertEquals(tdservice.RemoteIfaceDown._make(
            ('11:11:11:00:00:01', )), payload)
        self.assertEquals(tdservice.MessageID.REMOTE_IF_DOWN, messageID)

    def test_remote_dev_up(self):
        """Verify the parsing of the remote dev up message."""
        string = '\x11\x11\x11\x00\x00\x01'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.REMOTE_DEV_UP.value, string)

        self.assertEquals(tdservice.RemoteDevUp._make(
            ('11:11:11:00:00:01', )), payload)
        self.assertEquals(tdservice.MessageID.REMOTE_DEV_UP, messageID)

    def test_remote_dev_down(self):
        """Verify the parsing of the remote dev down message."""
        string = '\x11\x11\x11\x00\x00\x01'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.REMOTE_DEV_DOWN.value, string)

        self.assertEquals(tdservice.RemoteDevDown._make(
            ('11:11:11:00:00:01', )), payload)
        self.assertEquals(tdservice.MessageID.REMOTE_DEV_DOWN, messageID)

    def test_aging(self):
        """Verify the parsing of the aging message."""
        string = ''
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.AGING.value, string)

        self.assertEquals(None, payload)
        self.assertEquals(tdservice.MessageID.AGING, messageID)

    def test_bridged_da_change(self):
        """Verify the parsing of the bridged DA change message."""
        string = '\x11\x11\x11\x00\x00\x01'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.BDA_CHANGE.value, string)

        self.assertEquals(tdservice.BridgedDAChange._make(
            ('11:11:11:00:00:01', )), payload)
        self.assertEquals(tdservice.MessageID.BDA_CHANGE, messageID)

    def test_entry(self):
        """Verify the parsing of the entry message."""
        # No interfaces, no bridged DAs
        mac = '\x11\x11\x11\x00\x00\x01'
        string = mac + '\x00\x00\x00\x00\x00\x00\x00\x00'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.ENTRY.value, string)

        self.assertEquals(tdservice.Entry._make(
            ('11:11:11:00:00:01', 0, [], 0, [])), payload)
        self.assertEquals(tdservice.MessageID.ENTRY, messageID)

        # Two interfaces, two bridged DAs
        string = mac + '\x00\x00\x00\x02'
        string += '\x00\x06' + '\x57\x4c\x41\x4e\x35\x47' + '\x00\x01' + '\x00\x00\x00\x01\x00\x01'
        string += '\x00\x05' + '\x45\x54\x48\x45\x52' + '\x00\x00' + '\x00\x00\x00\x01\x00\x02'
        string += '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x01\x01\x01' + '\x00\x00\x00\x01\x01\x02'
        payload, messageID = tdservice.unpack_payload_from_bytes(
            tdservice.MessageID.ENTRY.value, string)

        iface1 = tdservice.Interface._make(('WLAN5G', 1, '00:00:00:01:00:01'))
        iface2 = tdservice.Interface._make(('ETHER', 0, '00:00:00:01:00:02'))
        self.assertEquals(tdservice.Entry._make(
            ('11:11:11:00:00:01', 2,
             [iface1, iface2], 2,
             ['00:00:00:01:01:01', '00:00:00:01:01:02'])), payload)
        self.assertEquals(tdservice.MessageID.ENTRY, messageID)


if __name__ == '__main__':
    unittest.main()
