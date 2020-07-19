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
import itertools

from whcdiag.msgs import common
from whcdiag.msgs import bandmon
from whcdiag.msgs.exception import MessageMalformedError

from whcdiag.constants import BAND_TYPE


class TestBandMonMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Message not even long enough for message ID
        msg = ''
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 2: Invalid message ID
        msg = '\x02\x11'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        msg = '\x03\x11'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, True, msg)

        # Test 3: Invalid length for overload change
        msg = '\x00\x00'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Too short - 1 channel specified but is not there
        msg = '\x00\x01'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Too long - 3 channels specified but only two there
        msg = '\x00\x03\x01\x64'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 4: Invalid length for utilization
        msg = '\x01\x00'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        msg = '\x01\x00'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 5: Invalid length for blackout change
        msg = '\x02'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

    def test_overload_change_v1(self):
        """Verify the parsing of the overload change message for version 1."""
        # Test each of the possible combinations of previous and current
        # overload status.
        single_value = itertools.product([False, True], [False, True])
        for combo in itertools.product(single_value, repeat=2):
            msg = '\x00'
            msg += chr(combo[0][0] | (combo[0][1] << 1))  # previous overload
            msg += chr(combo[1][0] | (combo[1][1] << 1))  # current overload

            # This is just the tuples flattened out to a list.
            expected_params = [x for y in combo for x in y]

            payload = bandmon.unpack_payload_from_bytes(
                common.Version.VERSION1, False, msg)
            self.assertEquals(bandmon.OverloadChange_v1._make(
                expected_params), payload)

            payload = bandmon.unpack_payload_from_bytes(
                common.Version.VERSION1, True, msg)
            self.assertEquals(bandmon.OverloadChange_v1._make(
                expected_params), payload)

    def test_overload_change_v2(self):
        """Verify the parsing of the overload change message for version 2."""
        # Test 1: No overloaded channels.
        msg = '\x00\x00'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.OverloadChange_v2._make(([],)), payload)

        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.OverloadChange_v2._make(([],)), payload)

        # Test 2: A single overloaded channel
        msg = '\x00\x01\x06'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.OverloadChange_v2._make(([6],)), payload)

        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.OverloadChange_v2._make(([6],)), payload)

        # Test 3: Multiple overloaded channels
        msg = '\x00\x02\x0b\x40'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.OverloadChange_v2._make(([11, 64],)), payload)

        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.OverloadChange_v2._make(([11, 64],)), payload)

    def test_utilization_v1(self):
        """Verify the parsing of the utilization message for version 1."""
        # Test 1: Measurement on 2.4 GHz
        msg = '\x01\x00\x37'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(bandmon.Utilization_v1._make(
            (BAND_TYPE.BAND_24G, 55)), payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(bandmon.Utilization_v1._make(
            (BAND_TYPE.BAND_24G, 55)), payload)

        # Test 3: Utilization measurement on 5 GHz now
        msg = '\x01\x01\x25'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION1, False, msg)
        self.assertEquals(bandmon.Utilization_v1._make(
            (BAND_TYPE.BAND_5G, 37)), payload)

        # Test 4: Same in big endian
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION1, True, msg)
        self.assertEquals(bandmon.Utilization_v1._make(
            (BAND_TYPE.BAND_5G, 37)), payload)

        # Test 5: The invalid enumerated band is rejected
        msg = '\x01\x02\x16'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

        # Test 6: Band that is not even contained within the enum
        msg = '\x01\x03\x34'
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, bandmon.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)

    def test_utilization_v2(self):
        """Verify the parsing of the utilization message for version 2."""
        # Test 1: Measurement on channel 11
        msg = '\x01\x0b\x37'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.Utilization_v2._make(
            (11, 55)), payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.Utilization_v2._make(
            (11, 55)), payload)

        # Test 3: Utilization measurement on channel 100
        msg = '\x01\x64\x25'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.Utilization_v2._make(
            (100, 37)), payload)

        # Test 4: Same in big endian
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.Utilization_v2._make(
            (100, 37)), payload)

    def test_blackout_change(self):
        """Verify the parsing of the blackout change message."""
        # Test 1: Blackout start
        msg = '\x02\x01'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.BlackoutChange._make((True,)), payload)

        # Test 2: Similar but in big endian format (which makes no difference)
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.BlackoutChange._make((True,)), payload)

        # Test 3: Blackout end
        msg = '\x02\x00'
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(bandmon.BlackoutChange._make((False,)), payload)

        # Test 4: Same in big endian
        payload = bandmon.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(bandmon.BlackoutChange._make((False,)), payload)

if __name__ == '__main__':
    unittest.main()
