#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the band monitor module.
#

"""Module providing the message unpacking for the ver message.

The following classes are exported:



The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import collections

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError

MessageID = Enum('MessageID', [("VERSION_1", 1), ("VERSION_2", 2), ("VERSION_3", 3)])

Version = collections.namedtuple('Version', ['version'])


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

    # Version is a special case - there is no payload, and the sub_id
    # contains the version number
    try:
        v = Version(MessageID(sub_id))
    except:
        raise MessageMalformedError("Unsupported version: %d" % sub_id)

    return (v, v.version)

__all__ = ['Version']
