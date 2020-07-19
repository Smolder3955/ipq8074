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
from whcdiag.hydmsgs import psservice
from whcdiag.hydmsgs import pstables
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.hydmsgs.ver import MessageID as MessageVersion


class TestPSServiceMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""

        # Version 1/2 tests

        mac = '\x11\x11\x11\x00\x00\x01'
        msg = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        msg += '\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x05\x00\x00\x00\x06'
        msg += '\x00\x04' + '\x61\x74\x68\x30'
        msg += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32'
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            0, msg, MessageVersion.VERSION_1)

        # Test 2: Message too short for aging
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.AGING.value, msg, MessageVersion.VERSION_2)

        # Test 3: Message too short for failover
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.FAILOVER.value, msg, MessageVersion.VERSION_1)

        # Test 4: Message too short for load balancing
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.LOAD_BALANCING.value, msg, MessageVersion.VERSION_2)

        # Test 5: Message missing interfaces
        msg = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        msg += '\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x05\x00\x00\x00'
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.LOAD_BALANCING.value, msg, MessageVersion.VERSION_2)

        # Version 3 tests

        # Test 1: Invalid message ID
        mac = '\x11\x11\x11\x00\x00\x01'
        msg = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        msg += '\x00\x00\x00\x03\x00\x00\x00\x04'
        msg += '\x00\x04' + '\x61\x74\x68\x30'
        msg += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32'
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            0, msg, MessageVersion.VERSION_3)

        # Test 2: Message too short for aging
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.AGING.value, msg, MessageVersion.VERSION_3)

        # Test 3: Message too short for failover
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.FAILOVER.value, msg, MessageVersion.VERSION_3)

        # Test 4: Message too short for load balancing
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.LOAD_BALANCING.value, msg, MessageVersion.VERSION_3)

        # Test 5: H-Default message can have 0 rows, so 0 length is OK
        # However, should raise malformed error if non-integer number of rows
        msg = '\x00'
        self.assertRaises(
            MessageMalformedError, psservice.unpack_payload_from_bytes,
            psservice.MessageID.HDEFAULT.value, msg, MessageVersion.VERSION_3)

    def test_aging(self):
        """Verify the parsing of the aging message."""

        # Version 3
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.AGING.value, string, MessageVersion.VERSION_3)

        self.assertEquals(psservice.Aging_v3._make(
            (1, '11:11:11:00:00:01', 2, 3, 4,
             'ath0', 'eth0.128')), payload)
        self.assertEquals(psservice.MessageID.AGING, messageID)

        # Version 2 / 1
        string = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x05\x00\x00\x00\x06'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.AGING.value, string, MessageVersion.VERSION_1)

        aging = psservice.Aging._make((1, '11:11:11:00:00:01', 2, 3, 4, 5, 6,
                                       'ath0', 'eth0.128'))
        self.assertEquals(aging, payload)
        self.assertEquals(psservice.MessageID.AGING, messageID)

        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.AGING.value, string, MessageVersion.VERSION_2)
        self.assertEquals(aging, payload)
        self.assertEquals(psservice.MessageID.AGING, messageID)

    def test_failover(self):
        """Verify the parsing of the failover message."""

        # Version 3
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.FAILOVER.value, string, MessageVersion.VERSION_3)

        self.assertEquals(psservice.Failover_v3._make(
            (1, '11:11:11:00:00:01', 2, 3, 4,
             'ath0', 'eth0.128')), payload)
        self.assertEquals(psservice.MessageID.FAILOVER, messageID)

        # Version 2 / 1
        string = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x05\x00\x00\x00\x06'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.FAILOVER.value, string, MessageVersion.VERSION_1)

        failover = psservice.Failover._make((1, '11:11:11:00:00:01', 2, 3, 4, 5, 6,
                                             'ath0', 'eth0.128'))
        self.assertEquals(failover, payload)
        self.assertEquals(psservice.MessageID.FAILOVER, messageID)

        ayload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.FAILOVER.value, string, MessageVersion.VERSION_2)
        self.assertEquals(failover, payload)
        self.assertEquals(psservice.MessageID.FAILOVER, messageID)

    def test_load_balancing(self):
        """Verify the parsing of the load balancing message."""

        # Version 3
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x03\x00\x00\x00\x04'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.LOAD_BALANCING.value,
            string, MessageVersion.VERSION_3)

        self.assertEquals(psservice.LoadBalancing_v3._make(
            (1, '11:11:11:00:00:01', 2, 3, 4,
             'ath0', 'eth0.128')), payload)
        self.assertEquals(psservice.MessageID.LOAD_BALANCING, messageID)

        # Version 2 / 1
        string = '\x00\x00\x00\x01' + mac + '\x00\x00\x00\x02'
        string += '\x00\x00\x00\x03\x00\x00\x00\x04\x00\x00\x00\x05\x00\x00\x00\x06'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.LOAD_BALANCING.value,
            string, MessageVersion.VERSION_1)

        lb = psservice.LoadBalancing._make((1, '11:11:11:00:00:01', 2, 3, 4, 5, 6,
                                            'ath0', 'eth0.128'))
        self.assertEquals(lb, payload)
        self.assertEquals(psservice.MessageID.LOAD_BALANCING, messageID)

        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.LOAD_BALANCING.value,
            string, MessageVersion.VERSION_2)
        self.assertEquals(lb, payload)
        self.assertEquals(psservice.MessageID.LOAD_BALANCING, messageID)

    def test_hdefault(self):
        """Verify the parsing of the H-Default message."""

        # Parse 0 length message
        string = ''
        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.HDEFAULT.value, string, MessageVersion.VERSION_3)

        self.assertEquals(pstables.HDefault._make(([], )), payload)
        self.assertEquals(pstables.MessageID.HDEFAULT, messageID)

        # 1 row
        da = '\x11\x11\x11\x00\x00\x01'
        id = '\x11\x11\x11\x00\x00\x02'
        string = da + id + '\x45\x35'
        string += '\x00\x04' + '\x65\x74\x68\x30'
        string += '\x00\x05' + '\x61\x74\x68\x30\x31'

        payload, messageID = psservice.unpack_payload_from_bytes(
            psservice.MessageID.HDEFAULT.value, string, MessageVersion.VERSION_3)

        row = pstables.HDefaultRow._make(('11:11:11:00:00:01', '11:11:11:00:00:02',
                                          common.InterfaceType.ETHER,
                                          common.InterfaceType.WLAN5G,
                                          'eth0', 'ath01'))

        self.assertEquals(pstables.HDefault._make(([[row]])), payload)
        self.assertEquals(pstables.MessageID.HDEFAULT, messageID)


if __name__ == '__main__':
    unittest.main()
