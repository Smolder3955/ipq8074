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
from whcdiag.msgs import steerexec
from whcdiag.msgs.exception import MessageMalformedError

from whcdiag.constants import BAND_TYPE
from whcdiag.msgs.steerexec import SteeringProhibitType
from whcdiag.msgs.steerexec import BTMComplianceType
from whcdiag.msgs.steerexec import SteerType
from whcdiag.msgs.steerexec import SteerEndStatusType
from whcdiag.msgs.steerexec import SteerReasonType


class TestSteerExecMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Message not even long enough for message ID
        msg = ''
        self.assertRaises(
            MessageMalformedError, steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 2: Invalid message ID
        msg = '\x04\x11'
        self.assertRaises(
            MessageMalformedError, steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)
        self.assertRaises(
            MessageMalformedError, steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, True, msg)
        msg = '\x06\x11'
        self.assertRaises(
            MessageMalformedError, steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)
        self.assertRaises(
            MessageMalformedError, steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, True, msg)

        # Test 3: Steer to band message is too short (v1)
        mac = '\xff\x4e\x71\x6e\x19\x32'
        msg = '\x00' + mac + '\x01'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 4: Pre-association steer message is too short (v2)
        mac = '\xff\x4e\x71\x6e\x19\x32'
        msg = '\x00' + mac + '\x01\x01'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 5: Abort steer to band message too short (v1)
        mac = '\x24\x53\xf4\xc3\x8d\xc5'
        msg = '\x01' + mac
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 6: Steer end message too short (v2)
        mac = '\x24\x53\xf4\xc3\x8d\xc5'
        msg = '\x01' + mac + '\x01\x00'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 7: Steering unfriendly message is too short
        mac = '\x38\x61\x8d\x47\x84\xa3'
        msg = '\x02' + mac
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 7a: Steering unfriendly message is too short (v2)
        mac = '\x38\x61\x8d\x47\x84\xa3'
        msg = '\x02' + mac + '\x00'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 8: Steering prohibited message is too short
        mac = '\x1f\xcf\x50\xc2\x83\xcb'
        msg = '\x03' + mac
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 9: BTM compliance message is too short
        mac = '\x1f\xcf\x50\xc2\x83\xcb'
        msg = '\x04' + mac + '\x00\x00\x00\x01\x00\x01'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 10: Post-association steer message is too short
        mac = '\x1f\xcf\x50\xc2\x83\xcb'
        msg = '\x05' + mac + '\x01\x01\x01\x01\x01\x01\x01\x01\x01'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

    def test_steer_to_band(self):
        """Verify the parsing of the steer to band message."""
        # Test 1: Valid steer to band messages (Version 1)
        test_cases = itertools.product([BAND_TYPE.BAND_24G,
                                        BAND_TYPE.BAND_5G,
                                        BAND_TYPE.BAND_INVALID],
                                       [BAND_TYPE.BAND_24G,
                                        BAND_TYPE.BAND_5G])
        for test_case in test_cases:
            # Steer to band with the given associated band (the first
            # element of test_case) and target band (the second element
            # of test_case).
            mac = '\x94\x45\x8d\x13\xb7\x86'
            msg = '\x00' + mac + chr(test_case[0].value) + chr(test_case[1].value)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, False, msg)
            self.assertEquals(steerexec.SteerToBand._make(
                (common.ether_ntoa(mac), test_case[0], test_case[1])),
                payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, True, msg)
            self.assertEquals(steerexec.SteerToBand._make(
                (common.ether_ntoa(mac), test_case[0], test_case[1])),
                payload)

        # Test 2: Invalid target band (Version 1)
        mac = '\x49\xcd\x30\xda\x0c\x4c'
        msg = '\x00' + mac + '\x01\x02'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

    def test_pre_assoc_steer(self):
        """Verify the parsing of the pre-association steer message."""
        mac = '\x94\x45\x8d\x13\xb7\x86'
        transaction = '\x30'
        channel_count = '\x01'
        channel1 = '\x0a'
        channel2 = '\xfe'
        # Test 1: Single channel
        msg = '\x00' + mac + transaction + channel_count + channel1
        payload = steerexec.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(steerexec.PreAssocSteer._make(
            (common.ether_ntoa(mac), 0x30, 1, [0x0a])),
            payload)

        # Same as above but in big-endian (which makes no difference)
        payload = steerexec.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(steerexec.PreAssocSteer._make(
            (common.ether_ntoa(mac), 0x30, 1, [0x0a])),
            payload)

        # Test 2: Two channels
        channel_count = '\x02'
        msg = '\x00' + mac + transaction + channel_count + channel1 + channel2
        payload = steerexec.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(steerexec.PreAssocSteer._make(
            (common.ether_ntoa(mac), 0x30, 2, [0x0a, 0xfe])),
            payload)

        # Test 3: Incorrect channel count raises an error
        msg = '\x00' + mac + transaction + 'x00' + channel1 + channel2
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        msg = '\x00' + mac + transaction + 'x03' + channel1 + channel2
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

    def test_abort_steer_to_band(self):
        """Verify the parsing of the abort steer to band message."""
        # Test 1: Valid steer to band messages
        for disabled_band in [BAND_TYPE.BAND_24G, BAND_TYPE.BAND_5G]:
            # Abort steer to band with the given disabled band.
            mac = '\x94\x45\x8d\x13\xb7\x86'
            msg = '\x01' + mac + chr(disabled_band.value)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, False, msg)
            self.assertEquals(steerexec.AbortSteerToBand._make(
                (common.ether_ntoa(mac), disabled_band)),
                payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, True, msg)
            self.assertEquals(steerexec.AbortSteerToBand._make(
                (common.ether_ntoa(mac), disabled_band)),
                payload)

        # Test 2: Invalid disabled band
        mac = '\x49\xcd\x30\xda\x0c\x4c'
        msg = '\x01' + mac + '\x02'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

    def test_steer_end(self):
        """Verify the parsing of the steer end message."""
        test_cases = itertools.product([SteerType.STEER_TYPE_NONE,
                                        SteerType.STEER_TYPE_LEGACY,
                                        SteerType.STEER_TYPE_BTM_AND_BLACKLIST,
                                        SteerType.STEER_TYPE_BTM,
                                        SteerType.STEER_TYPE_BTM_AND_BLACKLIST_ACTIVE,
                                        SteerType.STEER_TYPE_BTM_ACTIVE,
                                        SteerType.STEER_TYPE_PREASSOCIATION,
                                        SteerType.STEER_TYPE_BTM_BE,
                                        SteerType.STEER_TYPE_BTM_BE_ACTIVE,
                                        SteerType.STEER_TYPE_BTM_BLACKLIST_BE,
                                        SteerType.STEER_TYPE_BTM_BLACKLIST_BE_ACTIVE,
                                        SteerType.STEER_TYPE_LEGACY_BE],
                                       [SteerEndStatusType.STATUS_SUCCESS,
                                        SteerEndStatusType.STATUS_ABORT_AUTH_REJECT,
                                        SteerEndStatusType.STATUS_ABORT_LOW_RSSI,
                                        SteerEndStatusType.STATUS_ABORT_CHANGE_TARGET,
                                        SteerEndStatusType.STATUS_ABORT_USER,
                                        SteerEndStatusType.STATUS_BTM_REJECT,
                                        SteerEndStatusType.STATUS_BTM_RESPONSE_TIMEOUT,
                                        SteerEndStatusType.STATUS_ASSOC_TIMEOUT,
                                        SteerEndStatusType.STATUS_CHANNEL_CHANGE,
                                        SteerEndStatusType.STATUS_PREPARE_FAIL,
                                        SteerEndStatusType.STATUS_UNEXPECTED_BSS])

        # Test 1: Valid steer end messages
        for test_case in test_cases:
            # Test all combinations for steer type and status.
            mac = '\x94\x45\x8d\x13\xb7\x86'
            transaction = '\x33'
            msg = '\x01' + mac + transaction + chr(test_case[0].value) + chr(test_case[1].value)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, False, msg)
            self.assertEquals(steerexec.SteerEnd._make(
                (common.ether_ntoa(mac), 0x33, test_case[0], test_case[1])),
                payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, True, msg)
            self.assertEquals(steerexec.SteerEnd._make(
                (common.ether_ntoa(mac), 0x33, test_case[0], test_case[1])),
                payload)

        # Test 2: Invalid steer type
        mac = '\x49\xcd\x30\xda\x0c\x4c'
        msg = '\x01' + mac + transaction + '\x0d\x00'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 3: Invalid status
        mac = '\x49\xcd\x30\xda\x0c\x4c'
        msg = '\x01' + mac + transaction + '\x01\x0c'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

    def test_steering_unfriendly(self):
        """Verify the parsing of the steering unfriendly message."""
        # Test 1: Valid steering unfriendly messages
        for is_unfriendly in [False, True]:
            mac = '\x6e\xee\xb2\x94\x33\x6f'
            msg = '\x02' + mac + chr(is_unfriendly)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, False, msg)
            self.assertEquals(steerexec.SteeringUnfriendly._make(
                (common.ether_ntoa(mac), is_unfriendly)),
                payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, True, msg)
            self.assertEquals(steerexec.SteeringUnfriendly._make(
                (common.ether_ntoa(mac), is_unfriendly)),
                payload)

        # Test 2: Valid steering unfriendly messages (v2)
        for is_unfriendly in [False, True]:
            mac = '\x6e\xee\xb2\x94\x33\x6f'
            msg = '\x02' + mac + chr(is_unfriendly) + '\x07\x00\x00\x00'
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, False, msg)
            self.assertEquals(steerexec.SteeringUnfriendly_v2._make(
                (common.ether_ntoa(mac), is_unfriendly, 7)),
                payload)

            # Same as above but in big-endian
            msg = '\x02' + mac + chr(is_unfriendly) + '\x00\x00\x00\x07'
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, True, msg)
            self.assertEquals(steerexec.SteeringUnfriendly_v2._make(
                (common.ether_ntoa(mac), is_unfriendly, 7)),
                payload)

    def test_steering_prohibited(self):
        """Verify the parsing of the steering prohibited message."""
        # Test 1: Valid steering prohibited messages Version 2
        for prohibit_type in SteeringProhibitType:
            # Abort steer to band with the given disabled band.
            mac = '\x6e\xee\xb2\x94\x33\x6f'
            msg = '\x03' + mac + chr(prohibit_type.value)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, False, msg)
            self.assertEquals(steerexec.SteeringProhibited._make(
                (common.ether_ntoa(mac), prohibit_type)),
                payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, True, msg)
            self.assertEquals(steerexec.SteeringProhibited._make(
                (common.ether_ntoa(mac), prohibit_type)),
                payload)

        # Test 2: Invalid steering prohibited type Version 2
        mac = '\x6e\xee\xb2\x94\x33\x6f'
        msg = '\x03' + mac + '\x05'
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 3: Valid steering prohibited messages Version 1
        for is_prohibited in [False, True]:
            # Abort steer to band with the given disabled band.
            mac = '\x6e\xee\xb2\x94\x33\x6f'
            msg = '\x03' + mac + chr(is_prohibited)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, False, msg)
            self.assertEquals(steerexec.SteeringProhibited._make(
                (common.ether_ntoa(mac), is_prohibited)),
                payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION1, True, msg)
            self.assertEquals(steerexec.SteeringProhibited._make(
                (common.ether_ntoa(mac), is_prohibited)),
                payload)

    def test_btm_compliance(self):
        """Verify the parsing of the BTM compliance message."""
        # Test 1: Valid BTM compliance messages
        for compliance in BTMComplianceType:
            for is_unfriendly in [False, True]:
                # Changing to all the valid compliance states.
                mac = '\x6e\xee\xb2\x94\x33\x6f'
                countFailure = '\x0a\x00\x00\x00'
                countFailureActive = '\x01\x00\x00\x00'
                msg = '\x04' + mac + chr(is_unfriendly) + chr(compliance.value) + \
                      countFailure + countFailureActive
                payload = steerexec.unpack_payload_from_bytes(common.Version.VERSION2, False, msg)
                self.assertEquals(steerexec.BTMCompliance._make(
                    (common.ether_ntoa(mac), is_unfriendly,
                     compliance, 10, 1)), payload)

                # Same as above but in big-endian
                countFailure = '\x00\x00\x00\x0a'
                countFailureActive = '\x00\x00\x00\x01'
                msg = '\x04' + mac + chr(is_unfriendly) + chr(compliance.value) + \
                      countFailure + countFailureActive
                payload = steerexec.unpack_payload_from_bytes(
                    common.Version.VERSION2, True, msg)
                self.assertEquals(steerexec.BTMCompliance._make(
                    (common.ether_ntoa(mac), is_unfriendly,
                     compliance, 10, 1)), payload)

        # Test 2: Invalid BTM compliance type
        mac = '\x6e\xee\xb2\x94\x33\x6f'
        countFailure = '\x0a\x00\x00\x00'
        countFailureActive = '\x01\x00\x00\x00'
        msg = '\x04' + mac + '\x01' + '\x04' + countFailure + countFailureActive
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 3: Not supported in version 1
        mac = '\x6e\xee\xb2\x94\x33\x6f'
        msg = '\x04' + mac + '\x03' + '\x01' + countFailure + countFailureActive
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

    def test_post_assoc_steer(self):
        """Verify the parsing of the post-association steer message."""
        test_cases = itertools.product([SteerType.STEER_TYPE_NONE,
                                        SteerType.STEER_TYPE_LEGACY,
                                        SteerType.STEER_TYPE_BTM_AND_BLACKLIST,
                                        SteerType.STEER_TYPE_BTM,
                                        SteerType.STEER_TYPE_BTM_AND_BLACKLIST_ACTIVE,
                                        SteerType.STEER_TYPE_BTM_ACTIVE,
                                        SteerType.STEER_TYPE_PREASSOCIATION,
                                        SteerType.STEER_TYPE_BTM_BE,
                                        SteerType.STEER_TYPE_BTM_BE_ACTIVE,
                                        SteerType.STEER_TYPE_BTM_BLACKLIST_BE,
                                        SteerType.STEER_TYPE_BTM_BLACKLIST_BE_ACTIVE,
                                        SteerType.STEER_TYPE_LEGACY_BE],
                                       [SteerReasonType.REASON_USER,
                                        SteerReasonType.REASON_ACTIVE_UPGRADE,
                                        SteerReasonType.REASON_ACTIVE_DOWNGRADE_RATE,
                                        SteerReasonType.REASON_ACTIVE_DOWNGRADE_RSSI,
                                        SteerReasonType.REASON_IDLE_UPGRADE,
                                        SteerReasonType.REASON_IDLE_DOWNGRADE,
                                        SteerReasonType.REASON_ACTIVE_OFFLOAD,
                                        SteerReasonType.REASON_IDLE_OFFLOAD,
                                        SteerReasonType.REASON_AP_REQUEST,
                                        SteerReasonType.REASON_INTERFERENCE_AVOIDANCE,
                                        SteerReasonType.REASON_INVALID])

        for test_case in test_cases:
            # Type of steering.
            mac = '\x94\x45\x8d\x13\xb7\x86'
            transaction = '\x10'
            assoc_ap = '\xff'
            assoc_channel = '\x0b'
            assoc_ess = '\x00'
            ap1 = '\xff'
            channel1 = '\x0a'
            ess1 = '\x00'
            msg = '\x05' + mac + transaction + chr(test_case[0].value) + \
                chr(test_case[1].value) + \
                assoc_ap + assoc_channel + assoc_ess + '\x01' + ap1 + channel1 + ess1
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, False, msg)
            self.assertEquals(steerexec.PostAssocSteer._make(
                (common.ether_ntoa(mac), 0x10, test_case[0], test_case[1],
                 common.BSSInfo(0xFF, 0x0B, 0),
                 1, [common.BSSInfo(0xFF, 0x0A, 0)])), payload)

            # Same as above but in big-endian (which makes no difference)
            payload = steerexec.unpack_payload_from_bytes(
                common.Version.VERSION2, True, msg)
            self.assertEquals(steerexec.PostAssocSteer._make(
                (common.ether_ntoa(mac), 0x10, test_case[0], test_case[1],
                 common.BSSInfo(0xFF, 0x0B, 0),
                 1, [common.BSSInfo(0xFF, 0x0A, 0)])), payload)

        # Test 2: Invalid steer type (Version 2)
        mac = '\x49\xcd\x30\xda\x0c\x4c'
        msg = '\x05' + mac + transaction + '\x0d' + '\x01' + \
              assoc_ap + assoc_channel + assoc_ess + '\x01' + ap1 + channel1 + ess1
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 3: Invalid steer reason (Version 2)
        mac = '\x49\xcd\x30\xda\x0c\x4c'
        msg = '\x05' + mac + transaction + '\x01' + '\x0d' + \
              assoc_ap + assoc_channel + assoc_ess + '\x01' + ap1 + channel1 + ess1
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 4: Two candidate BSSes
        ap2 = '\xff'
        channel2 = '\x0c'
        ess2 = '\x00'
        msg = '\x05' + mac + transaction + chr(test_case[0].value) + \
              chr(test_case[1].value) + \
              assoc_ap + assoc_channel + assoc_ess + '\x02' + ap1 + channel1 + ess1 + \
              ap2 + channel2 + ess2
        payload = steerexec.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(steerexec.PostAssocSteer._make(
            (common.ether_ntoa(mac), 0x10, test_case[0], test_case[1],
             common.BSSInfo(0xFF, 0x0B, 0),
             2, [common.BSSInfo(0xFF, 0x0A, 0), common.BSSInfo(0xFF, 0x0C, 0)])),
            payload)

        # Test 5: Can't be parsed with version 1
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 6: Incorrect candidate count raises an error
        msg = '\x05' + mac + transaction + '\x01' + '\x01' + \
              assoc_ap + assoc_channel + assoc_ess + '\x00' + ap1 + channel1 + ess1 + \
              ap2 + channel2 + ess2
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        msg = '\x05' + mac + transaction + '\x01' + '\x01' + \
              assoc_ap + assoc_channel + assoc_ess + '\x03' + ap1 + channel1 + ess1 + \
              ap2 + channel2 + ess2
        self.assertRaises(
            MessageMalformedError,
            steerexec.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

if __name__ == '__main__':
    unittest.main()
