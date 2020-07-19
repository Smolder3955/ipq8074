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

from whcdiag.msgs import parser
from whcdiag.msgs import common
from whcdiag.msgs import wlanif
from whcdiag.msgs import stadb
from whcdiag.msgs import bandmon
from whcdiag.msgs import steerexec
from whcdiag.msgs import diaglog
from whcdiag.msgs import estimator


class TestParser(unittest.TestCase):

    def test_unpack_msg(self):
        """Validate the top-level unpacking of messsages."""
        # Test 1: A wlanif message
        msg = '\x10\x18\x29\xc6\x09\x00\xd6\x73\x06\x00\x01\x00\x00\x16'
        header, payload = parser.unpack_msg(msg)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               False, 24, 0x9c629, 0x673d6,
                                               common.ModuleID.WLANIF)),
                          header)
        self.assertEquals(wlanif.RawChannelUtilization._make(
            (common.BAND_TYPE.BAND_24G, 22)), payload)

        # Test 2: A bandmon message
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x02\x01\x00\x37'
        header, payload = parser.unpack_msg(msg)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               True, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.BANDMON)),
                          header)
        self.assertEquals(bandmon.Utilization_v1._make(
            (common.BAND_TYPE.BAND_24G, 55)), payload)

        # Test 3: A stadb message
        mac = '\xb2\xdd\x69\xa1\xb3\xe3'
        msg = '\x10\xa2\x0e\x52\x77\x51\xd9\x03\x05\x00\x05\x00' + mac + \
              '\x00\x01\x01'
        header, payload = parser.unpack_msg(msg)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               False, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.STADB)),
                          header)
        self.assertEquals(stadb.AssociationUpdate._make(
            (common.ether_ntoa(mac), common.BAND_TYPE.BAND_24G, True,
             True)), payload)

        # Test 4: A steerexec message
        mac = '\x6e\xee\xb2\x94\x33\x6f'
        msg = '\x11\x18\x00\x09\xc6\x29\x00\x06\x73\xd6\x06\x02' + mac + '\x01'
        header, payload = parser.unpack_msg(msg)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               True, 24, 0x9c629, 0x673d6,
                                               common.ModuleID.STEEREXEC)),
                          header)
        self.assertEquals(steerexec.SteeringUnfriendly._make(
            (common.ether_ntoa(mac), True)),
            payload)

        # Test 5: A diaglog message
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x08\x00abc'
        header, payload = parser.unpack_msg(msg)
        self.assertEquals(common.Header._make((common.Version.VERSION1,
                                               True, 162, 0x5177520e, 0x503d9,
                                               common.ModuleID.DIAGLOG)),
                          header)
        self.assertEquals(diaglog.StringMessage._make(('abc',)), payload)

        # Test 6: An estmiator message
        mac = '\x42\x62\x89\x67\x4d\xb4'
        msg = '\x20\x12\x60\x24\x87\x75\x15\x37\x00\x00\x09' + \
              '\x00' + mac + '\xff\x06\x01\x2d\x00\x00\x00\x08\x00\x00\x00' + \
              '\x50\x00\x00\x00\x42'
        header, payload = parser.unpack_msg(msg)
        self.assertEquals(common.Header._make((common.Version.VERSION2,
                                               False, 18, 0x75872460, 0x3715,
                                               common.ModuleID.ESTIMATOR)),
                          header)
        self.assertEquals(estimator.ServingDataMetrics._make(
            (common.ether_ntoa(mac), common.BSSInfo(255, 6, 1),
             45, 8, 80, 66)),
            payload)

        # Test 7: An unhandled module
        msg = '\x11\xa2\x51\x77\x52\x0e\x00\x05\x03\xd9\x70\x12\x11\x22'
        self.assertRaises(ValueError, parser.unpack_msg, msg)

if __name__ == '__main__':
    unittest.main()
