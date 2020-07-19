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

"""Module providing the message unpacking of the H-Default table for the psService module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import struct
import collections
from enum import Enum

import common
from whcdiag.msgs.exception import MessageMalformedError

# Supported message identifiers for psService tables
MessageID = Enum('MessageID', [('HDEFAULT', 1)])

_HDefaultRow = struct.Struct('>6s6sBB')
HDefaultRow = collections.namedtuple('HDefaultRow',
                                     ['da', 'id', 'iface_type_udp',
                                      'iface_type_other', 'iface_udp',
                                      'iface_other'])

HDefault = collections.namedtuple('HDefault', ['hdefault_rows'])


def unpack_hdefault_table(buf):
    """Unpack the buffer into a H-Default table.

    Args:
        buf (str): the entire payload to be unpacked

    Returns:
        tuple containing the unpacked header and message ID:

        payload (namedtuple): the unpacked message as a HDefault namedtuple
        MessageID (Enum): the type of the message converted to an Enum
    """
    rows = common.unpack_table(buf, _HDefaultRow, HDefaultRow, ['da', 'id'],
                               ['iface_type_udp', 'iface_type_other'],
                               ['iface_udp', 'iface_other'])
    hdefault = HDefault(rows)
    return (hdefault, MessageID.HDEFAULT)


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
    if sub_id == MessageID.HDEFAULT.value:
        return unpack_hdefault_table(buf)
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)


__all__ = ['MessageID']
