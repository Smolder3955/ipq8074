#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the band monitor module.
#

"""Module providing the message unpacking for the msg module.

The following types are exported:

    :obj:`MessageID`: identifier for the message
"""

import struct
import collections

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError

#: Supported message identifiers for custom message module
MessageID = Enum('MessageID', [('CUSTOM_MESSAGE', 0)])

CustomMessage = collections.namedtuple('CustomMessage', ['string'])


def unpack_payload_from_bytes(sub_id, buf):
    """Unpack the payload portion of the message provided.

    Args:
        sub_id (int): the type of the message
        buf (str): the entire payload to be unpacked

    Returns:
        tuple containing the unpacked header and message ID:

        payload (namedtuple): the unpacked message as a namedtuple of the right type
        MessageID (Enum): the type of the message converted to an Enum

    Raises:
        :class:`MessageMalformedError`: Invalid message
    """

    # The sub_id is the length of the string
    if len(buf) == 0 or sub_id == 0:
        raise MessageMalformedError("Custom string is missing")

    string_val = struct.unpack('%ds' % sub_id, buf[:sub_id])
    msg = CustomMessage._make(string_val)

    return (msg, MessageID.CUSTOM_MESSAGE)

__all__ = ['MessageID']
