#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the diagnostic logging module
#

"""Module providing the message unpacking for the WLANIF module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following classes are exported:

    :class:`StringMessage`: a raw string included in a message

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import collections

from enum import Enum

from exception import MessageMalformedError

#: Supported message identifiers for diaglog
MessageID = Enum('MessageID', [('STRING_MESSAGE', 0)])

StringMessage = collections.namedtuple('StringMessage', ['message'])


def unpack_payload_from_bytes(version, big_endian, buf):
    """Unpack the payload portion of the message provided.

    Args:
        version (int): the version number of the message
        big_endian (bool): whether the payload is encoded in big endian
            format or not
        buf (str): the entire payload to be unpacked

    Returns:
        the unpacked message as a namedtuple of the right type

    Raises:
        :class:`MessageMalformedError`: Unsupported message ID or band
    """
    if len(buf) == 0:
        raise MessageMalformedError("Message ID is missing")

    msg_id = ord(buf[0])

    if msg_id == MessageID.STRING_MESSAGE.value:
        return StringMessage._make((buf[1:],))
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % msg_id)

__all__ = ['MessageID', 'StringMessage']
