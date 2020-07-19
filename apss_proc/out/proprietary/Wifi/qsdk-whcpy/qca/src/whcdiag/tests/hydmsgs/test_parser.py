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
import collections
from enum import Enum

from whcdiag.hydmsgs import parser
from whcdiag.hydmsgs import common
from whcdiag.hydmsgs import heservice
from whcdiag.hydmsgs import msg
from whcdiag.hydmsgs import pcpservice
from whcdiag.hydmsgs import pcwservice
from whcdiag.hydmsgs import psservice
from whcdiag.hydmsgs import pstables
from whcdiag.hydmsgs import tdservice
from whcdiag.hydmsgs import ver
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.constants import TRANSPORT_PROTOCOL


class TestParser(unittest.TestCase):

    def test_unpack_msg(self):
        """Validate the top-level unpacking of messsages."""
        # Test 1: A heservice message
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x00\x20\x00\x01\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += '\x00\x01\x45\x00\x00\x03\xe8\x00\x00\x00\x01\x00\x00\x00\x02'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.HE,
                                               heservice.MessageID.SUMMARY,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(heservice.Summary._make(
            (1, common.InterfaceType.ETHER, 1000, 1, 2)), payload)

        # Test 2: A msg message
        string = '\x00\x02\x00\x04\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += '\x54\x65\x73\x74'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.MSG,
                                               msg.MessageID.CUSTOM_MESSAGE,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(msg.CustomMessage._make(("Test",)), payload)

        # Test 3: A pcpservice message
        string = '\x00\x10\x00\x04\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += '\x00\x11\x22\x33\x00\x00\x00\x32'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PCP,
                                               pcpservice.MessageID.GET_MU,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(pcpservice.GetMU._make((1122867, 50)), payload)

        # Test 4a: A pcwservice (2GHz) message - no version yet, so defaults to version 3
        string = '\x00\x04\x00\x02\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += mac + '\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00\x03\x00\x00\x00\x00'
        string += '\x00\x00\x00\x00'

        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PCW2,
                                               pcwservice.MessageID.RAW_AP,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(pcwservice.RawAP_v2._make(('11:11:11:00:00:01', 1, 2, 3, 0, [])),
                          payload)

        # Test 4b: A psservice message - no version yet, so defaults to version 3
        string = '\x00\x40\x00\x01\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        mac2 = '\x22\x22\x22\x00\x00\x01'
        string += '\x00\x00\x00\x33' + mac2
        string += '\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x01'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PS,
                                               psservice.MessageID.AGING,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(psservice.Aging_v3._make((51, '22:22:22:00:00:01',
                                                    0, 2147483648,
                                                    1, "ath0", "eth0.128")), payload)

        # A ver message - need to do this before the pcw messages, since
        # they need the version info
        string = '\x00\x01\x00\x01\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.VER,
                                               ver.MessageID.VERSION_1,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)

        # Test 4: A pcwservice (2GHz) message
        string = '\x00\x04\x00\x08\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += '\x00\x11\x22\x33\x00\x44\x55\x66\x00\x00\x00\x31'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PCW2,
                                               pcwservice.MessageID.GET_MU,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(pcwservice.MediumUtilization._make((1122867, 4478310, 49)), payload)

        # Test 4.1: A pcwservice (5GHz) message
        string = '\x00\x08\x00\x08\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += '\x00\x11\x22\x33\x00\x44\x55\x66\x00\x00\x00\x31'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PCW5,
                                               pcwservice.MessageID.GET_MU,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(pcwservice.MediumUtilization._make((1122867, 4478310, 49)), payload)

        # Test 5: A psservice message
        string = '\x00\x40\x00\x01\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        mac2 = '\x22\x22\x22\x00\x00\x01'
        string += '\x00\x00\x00\x33' + mac2
        string += '\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x01'
        string += '\x00\x00\x00\x02\x00\x00\x50\x00'
        string += '\x00\x04' + '\x61\x74\x68\x30'
        string += '\x00\x08' + '\x65\x74\x68\x30\x2e\x31\x32\x38'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PS,
                                               psservice.MessageID.AGING,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)
        self.assertEquals(psservice.Aging._make((51, '22:22:22:00:00:01',
                                                 0, 2147483648,
                                                 1, 2, 20480,
                                                 "ath0", "eth0.128")), payload)

        # Test 6: A tdservice message
        string = '\x00\x80\x00\x40\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        string += '\x00\x11\x22\x33\x00\x00\x00\x32'
        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.TD,
                                               tdservice.MessageID.AGING,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               False)),
                          header)

        # Test 8: A heservice tables message, with more_fragments set
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x81\x00\x00\x01\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac

        da = '\x11\x11\x11\x00\x00\x01'
        id = '\x11\x11\x11\x00\x00\x02'
        string += da + id + '\x00\x00\x00\x01\x00\x00\x00\x02\x03\x01\x45'
        string += '\x00\x04' + '\x65\x74\x68\x30'

        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.HE_TABLES,
                                               heservice.MessageID.DETAIL,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               True)),
                          header)

        row = heservice.HActiveRow._make(('11:11:11:00:00:01', '11:11:11:00:00:02', 1,
                                          2, 3, TRANSPORT_PROTOCOL.UDP,
                                          common.InterfaceType.ETHER,
                                          'eth0'))

        self.assertEquals(heservice.Detail._make(([[row]])), payload)

        # Test 9: A psservice tables message, with more_fragments set
        mac = '\x11\x11\x11\x00\x00\x01'
        string = '\x82\x00\x00\x01\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac

        da = '\x11\x11\x11\x00\x00\x01'
        id = '\x11\x11\x11\x00\x00\x02'
        string += da + id + '\x45\x35'
        string += '\x00\x04' + '\x65\x74\x68\x30'
        string += '\x00\x05' + '\x61\x74\x68\x30\x31'

        header, payload = parser.unpack_msg(string)
        self.assertEquals(common.Header._make((common.ModuleID.PS_TABLES,
                                               pstables.MessageID.HDEFAULT,
                                               0x12345678, 0x9abccdef,
                                               '11:11:11:00:00:01',
                                               True)),
                          header)

        row = pstables.HDefaultRow._make(('11:11:11:00:00:01', '11:11:11:00:00:02',
                                          common.InterfaceType.ETHER,
                                          common.InterfaceType.WLAN5G,
                                          'eth0', 'ath01'))

        self.assertEquals(pstables.HDefault._make(([[row]])), payload)

        # Test 10: An unhandled module
        string = '\x00\x03\x00\x04\x12\x34\x56\x78\x9a\xbc\xcd\xef' + mac
        self.assertRaises(MessageMalformedError, parser.unpack_msg, string)

    def test_reassembler(self):
        reassembler = parser.Reassembler()

        # Reassembling a header without the more_fragments attribute returns
        # that it should be processed immediately, and no other entries to process
        SomeTuple = collections.namedtuple('SomeTuple', ['field1', 'field2'])
        header = SomeTuple("x", "y")
        (should_process, entries) = reassembler.reassemble(header, "addr", "stored_obj")
        self.assertTrue(should_process)
        self.assertEquals(None, entries)

        # more_fragments True should store the entry, return should_process as False
        HeaderTuple = collections.namedtuple('HeaderTuple', ['more_fragments', 'id',
                                                             'sub_id'])
        MessageID = Enum('MessageID', [('ID_1', 1), ('ID_2', 2)])
        SubID = Enum('SubID', [('SUBID_1', 1), ('SUBID_2', 2)])

        header = HeaderTuple(True, MessageID.ID_1, SubID.SUBID_1)
        (should_process, entries) = reassembler.reassemble(header, "addr", "stored_obj1")
        self.assertFalse(should_process)
        self.assertEquals(None, entries)

        # Storing from a different source address doesn't cause the entries to be deleted
        (should_process, entries) = reassembler.reassemble(header, "addr2", "stored_obj_addr2")
        self.assertFalse(should_process)
        self.assertEquals(None, entries)

        # Store another with matching id and sub_id
        header2 = HeaderTuple(True, MessageID.ID_1, SubID.SUBID_1)
        (should_process, entries) = reassembler.reassemble(header2, "addr", "stored_obj2")
        self.assertFalse(should_process)
        self.assertEquals(None, entries)

        # Calling with more_fragments False should return both stored elements
        header3 = HeaderTuple(False, MessageID.ID_1, SubID.SUBID_1)
        (should_process, entries) = reassembler.reassemble(header3, "addr", "stored_obj3")
        self.assertTrue(should_process)
        self.assertEquals([(header, "stored_obj1"), (header2, "stored_obj2")], entries)

        # Calling again with more_fragments False should return no more entries
        (should_process, entries) = reassembler.reassemble(header3, "addr", "stored_obj3")
        self.assertTrue(should_process)
        self.assertEquals(None, entries)

        # Calling with a different id will clear out stored objects (stored_obj_addr2)
        header4 = HeaderTuple(True, MessageID.ID_2, SubID.SUBID_1)
        (should_process, entries) = reassembler.reassemble(header4, "addr2", "stored_obj4")
        self.assertFalse(should_process)
        self.assertEquals(None, entries)

        # Calling again will just return the previously stored object (stored_obj4)
        header5 = HeaderTuple(False, MessageID.ID_2, SubID.SUBID_1)
        (should_process, entries) = reassembler.reassemble(header5, "addr2", "stored_obj5")
        self.assertTrue(should_process)
        self.assertEquals([(header4, "stored_obj4")], entries)


if __name__ == '__main__':
    unittest.main()
