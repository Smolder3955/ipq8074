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
import itertools

from whcdiag.msgs import common
from whcdiag.msgs import estimator
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.msgs.estimator import STAPollutionChangeReason


class TestEstimatorMsgs(unittest.TestCase):

    def test_invalid_msg(self):
        """Verify invalid messages are rejected."""
        # Test 1: Message not even long enough for message ID
        msg = ''
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 2: Invalid message ID
        msg = '\x09\x11'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, True, msg)

        # Test 3: Invalid serving data rate estimate
        msg = '\x00\x00\x01\x02\x03\x04\x05\xff\x01\x01' + \
              '\x12\x34\x00\x00\x56\x78\x00\x00\x9a\xbc\x00\x00'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 4: Invalid non-serving data rate estimate
        msg = '\x01\x00\x01\x02\x03\x04\x05\xff\x01\x01' + \
              '\x9a\xbc\x00\x00'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 5: Invalid STA interference detected
        msg = '\x02\x00\x01\x02\x03\x04\x05\xff\x01\x01'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 6: Invalid STA interference detected
        msg = '\x03\x00\x01\x02\x03\x04\x05\xff\x01'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

        # Test 7: Serving data rate estimate with version 1 is rejected
        msg = '\x00\x00\x01\x02\x03\x04\x05\xff\x01\x01' + \
              '\x12\x34\x00\x00\x56\x78\x00\x00\x9a\xbc\x00\x00\x0a'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 8: Non-serving data rate estimate with version 1 is rejected
        msg = '\x01\x00\x01\x02\x03\x04\x05\xff\x01\x01' + \
              '\x9a\xbc\x00\x00\x0a'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 9: STA interference detected with version 1 is rejected
        msg = '\x02\x00\x01\x02\x03\x04\x05\xff\x01\x01\x01'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 10: STA pollution cleared with version 1 is rejected
        msg = '\x03\x00\x01\x02\x03\x04\x05\xff\x01\x01'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 11: Interference stats with version 1 is rejected
        msg = '\x04\x00\x01\x02\x03\x04\x05\xff\x01\x01\x20\x01\x01'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION1, False, msg)

        # Test 12: Interference stats message too short
        msg = '\x04\x00\x01\x02\x03\x04\x05\xff\x01\x01\x20\x01'
        self.assertRaises(
            MessageMalformedError, estimator.unpack_payload_from_bytes,
            common.Version.VERSION2, False, msg)

    def test_serving_data_metrics_msg(self):
        """Verify the parsing of the serving data metrics message."""
        # Test 1: Measurement on a BSS on channel 6
        mac = '\x42\x62\x89\x67\x4d\xb4'
        msg = '\x00' + mac + '\xff\x06\x01\x2d\x00\x00\x00\x08\x00\x00\x00' + \
              '\x50\x00\x00\x00\x42'
        payload = estimator.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(estimator.ServingDataMetrics._make(
            (common.ether_ntoa(mac), common.BSSInfo(255, 6, 1), 45, 8,
             80, 66)),
            payload)

        # Test 2: Similar but in big endian format
        msg = '\x00' + mac + '\x02\x24\x00\x00\x00\x00\x2d\x00\x00\x00\x08' + \
              '\x00\x00\x00\x50\x42'
        payload = estimator.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(estimator.ServingDataMetrics._make(
            (common.ether_ntoa(mac), common.BSSInfo(2, 36, 0), 45, 8,
             80, 66)),
            payload)

    def test_non_serving_data_metrics_msg(self):
        """Verify the parsing of the non-serving data metrics message."""
        # Test 1: Measurement on a BSS on channel 6
        mac = '\x42\x62\x89\x67\x4d\xb4'
        msg = '\x01' + mac + '\xff\x06\x01' + \
              '\x50\x00\x00\x00\x42'
        payload = estimator.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(estimator.NonServingDataMetrics._make(
            (common.ether_ntoa(mac), common.BSSInfo(255, 6, 1), 80, 66)),
            payload)

        # Test 2: Similar but in big endian format
        msg = '\x01' + mac + '\x02\x24\x00' + \
              '\x00\x00\x00\x50\x42'
        payload = estimator.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(estimator.NonServingDataMetrics._make(
            (common.ether_ntoa(mac), common.BSSInfo(2, 36, 0), 80, 66)),
            payload)

    def test_sta_interference_detected_msg(self):
        """Verify the parsing of the STA interference detected message."""
        # Test 1: Interference detected or not on a BSS on channel 11
        for big_endian in (False, True):
            for detected in (False, True):
                mac = '\x42\x62\x89\x67\x4d\xb4'
                msg = '\x02' + mac + '\xff\x0b\x01' + chr(detected)
                payload = estimator.unpack_payload_from_bytes(
                    common.Version.VERSION2, big_endian, msg)
                self.assertEquals(estimator.STAInterferenceDetected._make(
                    (common.ether_ntoa(mac), common.BSSInfo(255, 11, 1), detected)),
                    payload)

    def test_sta_pollution_changed_msg(self):
        """Verify the parsing of the STA pollution changed message."""
        # Test 1: Pollution changed on a BSS on channel 149
        test_cases = itertools.product([False, True],
                                       [STAPollutionChangeReason.POLLUTION_CHANGE_DETECTION,
                                        STAPollutionChangeReason.POLLUTION_CHANGE_AGING,
                                        STAPollutionChangeReason.POLLUTION_CHANGE_REMOTE,
                                        STAPollutionChangeReason.POLLUTION_CHANGE_INVALID])
        for big_endian in (False, True):
            mac = '\x42\x62\x89\x67\x4d\xb4'
            for test_case in test_cases:
                msg = '\x03' + mac + '\xff\x95\x01' + \
                      chr(test_case[0]) + chr(test_case[1].value)
                payload = estimator.unpack_payload_from_bytes(
                    common.Version.VERSION2, big_endian, msg)
                self.assertEquals(estimator.STAPollutionChanged._make(
                    (common.ether_ntoa(mac), common.BSSInfo(255, 149, 1), test_case[0],
                     test_case[1])),
                    payload)

        # Test 2: Invalid pollution change reason
        for big_endian in (False, True):
            mac = '\x42\x62\x89\x67\x4d\xb4'
            for changed in (False, True):
                msg = '\x03' + mac + '\xff\x95\x01' + chr(changed) + \
                      chr(STAPollutionChangeReason.POLLUTION_CHANGE_INVALID.value + 1)
                self.assertRaises(
                    MessageMalformedError,
                    estimator.unpack_payload_from_bytes,
                    common.Version.VERSION2, big_endian, msg)

    def test_interference_stats_msg(self):
        """Verify the parsing of the interference stats message"""
        # Test 1: Stats on a BSS on channel 36
        mac = '\x42\x62\x89\x67\x4d\xb4'
        msg = '\x04' + mac + '\xff\x24\x00\x20\x01\x02' + \
              '\x01\x02\x03\x04\x05\x06\x07\x08' + \
              '\x0a\x0b\x0c\x0d'

        payload = estimator.unpack_payload_from_bytes(
            common.Version.VERSION2, False, msg)
        self.assertEquals(estimator.InterferenceStats._make(
            (common.ether_ntoa(mac), common.BSSInfo(255, 36, 0), 32, 513,
             578437695752307201, 218893066)),
            payload)

        # Test 2: Similar but in big endian format (Tx rate changes)
        payload = estimator.unpack_payload_from_bytes(
            common.Version.VERSION2, True, msg)
        self.assertEquals(estimator.InterferenceStats._make(
            (common.ether_ntoa(mac), common.BSSInfo(255, 36, 0), 32, 258,
             72623859790382856, 168496141)),
            payload)


if __name__ == '__main__':
    unittest.main()
