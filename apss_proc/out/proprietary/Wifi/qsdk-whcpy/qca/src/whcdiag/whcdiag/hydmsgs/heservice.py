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

"""Module providing the message unpacking for the heService module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import struct
import collections

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError
import common

# Supported message identifiers for heService
MessageID = Enum('MessageID', [('SUMMARY', 1), ('DETAIL', 2)])

_HActiveRow = struct.Struct('>6s6sIIBBB')
HActiveRow = collections.namedtuple('HActiveRow',
                                    ['da', 'id', 'priority', 'rate',
                                     'hash', 'sub_class', 'iface_type',
                                     'iface'])

_Summary = struct.Struct('>HBIII')
Summary = collections.namedtuple('Summary', ['ignored', 'iface_type', 'rate',
                                 'flows1', 'flows2'])

Detail = collections.namedtuple('Detail', ['hactive_rows'])


def unpack_hactive_table(buf):
    """Unpack the buffer into a H-Active table.

    Args:
        buf (str): the entire payload to be unpacked

    Returns:
        tuple containing the unpacked header and message ID:

        payload (namedtuple): the unpacked message as a Detail namedtuple
        MessageID (Enum): the type of the message converted to an Enum
    """
    rows = common.unpack_table(buf, _HActiveRow, HActiveRow, ['da', 'id'],
                               ['iface_type'], ['iface'], 'sub_class')
    detail = Detail(rows)

    return (detail, MessageID.DETAIL)


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
    if sub_id == MessageID.SUMMARY.value:
        unpacker = _Summary
        constructor = Summary
    elif sub_id == MessageID.DETAIL.value:
        return unpack_hactive_table(buf)
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)

    if len(buf) < unpacker.size:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size))

    fields = unpacker.unpack(buf[0:unpacker.size])
    payload = constructor._make(fields)

    # Replace the interface type with the correct string
    payload = common.replace_iface_char_with_string(payload)

    # Replace the MAC address with the correct format
    payload = common.replace_mac_address_with_string(payload)

    return (payload, MessageID(sub_id))


__all__ = ['MessageID']
