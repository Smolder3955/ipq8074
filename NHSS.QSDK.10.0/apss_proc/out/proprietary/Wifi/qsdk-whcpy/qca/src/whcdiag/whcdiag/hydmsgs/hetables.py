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

"""Module providing the message unpacking of the H-Active table for the heService module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError
from heservice import unpack_hactive_table

# Supported message identifiers for heService tables
MessageID = Enum('MessageID', [('HACTIVE', 1)])


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
    if sub_id == MessageID.HACTIVE.value:
        return unpack_hactive_table(buf)
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)


__all__ = ['MessageID']
