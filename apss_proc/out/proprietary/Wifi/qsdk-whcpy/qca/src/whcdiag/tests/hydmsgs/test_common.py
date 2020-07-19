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
import collections

from whcdiag.hydmsgs import common
from whcdiag.msgs.exception import MessageMalformedError


class TestCommonMsgs(unittest.TestCase):

    def test_replace_iface_char_with_string(self):
        """Verify the replacement of the interface char with a string."""
        MsgType1 = collections.namedtuple('MsgType1',
                                          ['field1', 'iface_type', 'field2'])
        MsgType2 = collections.namedtuple('MsgType2',
                                          ['field3', 'field4'])

        # Valid interface types
        for iface_type in common.InterfaceType:
            test_type = MsgType1._make([1, iface_type.value, 2])
            test_type = common.replace_iface_char_with_string(test_type)
            test_type_after = MsgType1._make([1, iface_type, 2])
            self.assertEquals(test_type, test_type_after)

        # Invalid interface type will raise malformed message error
        test_type = MsgType1._make([1, ord('U'), 2])
        self.assertRaises(
            MessageMalformedError, common.replace_iface_char_with_string,
            test_type)

        # Not an error if the field isn't present, should return the same type
        test_type = MsgType2._make([3, 4])
        test_type_after = common.replace_iface_char_with_string(test_type)
        self.assertEquals(test_type, test_type_after)


if __name__ == '__main__':
    unittest.main()
