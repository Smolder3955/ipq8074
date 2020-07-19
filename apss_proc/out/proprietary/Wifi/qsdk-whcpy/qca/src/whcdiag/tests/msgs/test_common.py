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
import collections

from whcdiag.msgs import common
from whcdiag.msgs.exception import MessageMalformedError

from whcdiag.constants import BAND_TYPE


class TestCommonMsgs(unittest.TestCase):

    def test_little_endian(self):
        """Verify the basic parsing of the header in little endian form."""
        # Test 1: Invalid version number
        self.assertRaises(
            MessageMalformedError, common.unpack_header_from_bytes,
            '\x00\x00\x12\x34\x56\x78\x00\x00\x00\x01\x01')

        # Test 2: Test version 1 header
        # Test 2.1: Valid version and format (with 2 bytes of payload)
        msg = '\x10\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x02\x11\x22'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 2, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               False, 24, 0x9c629, 0x673d6,
                                               common.ModuleID.BANDMON)),
                          header)

        # Test 2,2: Another valid version and format, but with a different
        #           number of payload bytes
        msg = '\x10\xa2\x0e\x52\x77\x51\xd9\x03\x05\x00\x07\xaa\xcc\xff'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 3, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               False, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.STAMON)),
                          header)

        # Test 3: Repeat test 2 with version 2 header
        # Test 3.1: Valid version and format (with 2 bytes of payload)
        msg = '\x20\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x02\x11\x22'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 2, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION2,
                                               False, 24, 0x9c629, 0x673d6,
                                               common.ModuleID.BANDMON)),
                          header)

        # Test 3,2: Another valid version and format, but with a different
        #           number of payload bytes
        msg = '\x20\xa2\x0e\x52\x77\x51\xd9\x03\x05\x00\x07\xaa\xcc\xff'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 3, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION2,
                                               False, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.STAMON)),
                          header)

    def test_big_endian(self):
        """Verify the basic parsing of the header in big endian form."""
        # Test 1: Invalid version number
        self.assertRaises(
            MessageMalformedError, common.unpack_header_from_bytes,
            '\x01\x00\x12\x34\x56\x78\x00\x00\x00\x01\x01')

        # Test 2: Test version 1 header
        # Test 2.1: Valid version and format (with 2 bytes of payload)
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x02\x11\x22'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 2, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               True, 24, 0x9c629, 0x673d6,
                                               common.ModuleID.BANDMON)),
                          header)

        # Test 2.2: Another valid version and format, but with a different
        #           number of payload bytes
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x07\xaa\xcc\xff'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 3, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               True, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.STAMON)),
                          header)

        # Test 3: Repeat test 2 with version 2 header
        # Test 3.1: Valid version and format (with 2 bytes of payload)
        msg = '\x21\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x02\x11\x22'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 2, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION2,
                                               True, 24, 0x9c629, 0x673d6,
                                               common.ModuleID.BANDMON)),
                          header)

        # Test 3.2: Another valid version and format, but with a different
        #           number of payload bytes
        msg = '\x21\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x07\xaa\xcc\xff'
        n_bytes, header = common.unpack_header_from_bytes(msg)
        self.assertEquals(len(msg) - 3, n_bytes)
        self.assertEquals(common.Header._make((common.Version.VERSION2,
                                               True, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.STAMON)),
                          header)

    def test_check_band(self):
        """Confirm that the check_band function operates properly."""
        MsgType1 = collections.namedtuple('MsgType1',
                                          ['field1', 'band', 'field2'])
        MsgType2 = collections.namedtuple('MsgType2',
                                          ['field3', 'field4'])

        common.check_band(MsgType1._make([1, BAND_TYPE.BAND_24G, 2]))
        common.check_band(MsgType1._make([1, BAND_TYPE.BAND_5G, 2]))

        self.assertRaises(
            MessageMalformedError,
            common.check_band,
            MsgType1._make([1, BAND_TYPE.BAND_INVALID.value, 2]))

        common.check_band(MsgType1._make([1, BAND_TYPE.BAND_INVALID, 2]),
                          allow_invalid=True)

        common.check_band(MsgType2._make([3, 4]))

    def test_ether_ntoa(self):
        """Test the function that formats an ethernet address as a string."""
        self.assertRaises(ValueError, common.ether_ntoa,
                          '\x11\x22\x33\x44\x55')  # too short
        self.assertRaises(ValueError, common.ether_ntoa,
                          '\x11\x22\x33\x44\x55\x66\x77')  # too long

        self.assertEquals('ff:c1:c4:62:22:53',
                          common.ether_ntoa('\xff\xc1\xc4\x62\x22\x53'))

    def test_ether_aton(self):
        """Test the function that packs an ethernet address."""
        self.assertRaises(ValueError, common.ether_aton,
                          'ff:c1:c4:62:22')  # too short
        self.assertRaises(ValueError, common.ether_aton,
                          'ff:c1:c4:62:22:53:53')  # too long
        self.assertRaises(ValueError, common.ether_aton,
                          'ff.c1.c4.62.22.53')  # wrong separator

        self.assertEquals('\xff\xc1\xc4\x62\x22\x53',
                          common.ether_aton('ff:c1:c4:62:22:53'))

        # Inverse of ether_ntoa
        self.assertEquals('\xff\xc1\xc4\x62\x22\x53',
                          common.ether_aton(common.ether_ntoa('\xff\xc1\xc4\x62\x22\x53')))

if __name__ == '__main__':
    unittest.main()
