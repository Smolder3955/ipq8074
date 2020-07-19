#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the band monitor module.
#

"""Module providing the message unpacking for the bandmon module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following classes are exported:

    :class:`OverloadChange`: which bands are considered overloaded has
        changed
    :class:`Utilization`: the average utilization on a band

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import struct
import collections

from enum import Enum

from exception import MessageMalformedError
import common

#: Supported message identifiers for bandmon
MessageID = Enum('MessageID', [('OVERLOAD_CHANGE', 0), ('UTILIZATION', 1),
                               ('BLACKOUT_CHANGE', 2)])

_OverloadChange_v1 = struct.Struct('BB')
OverloadChange_v1 = collections.namedtuple('OverloadChange',
                                           ['previous_overload_24g',
                                            'previous_overload_5g',
                                            'current_overload_24g',
                                            'current_overload_5g'])

_OverloadChange_v2 = struct.Struct('B')
OverloadChange_v2 = collections.namedtuple('OverloadChange',
                                           ['overloaded_channels'])

_Utilization_v1 = struct.Struct('BB')
Utilization_v1 = collections.namedtuple('Utilization', ['band', 'utilization'])

_Utilization_v2 = struct.Struct('BB')
Utilization_v2 = collections.namedtuple('Utilization', ['channel_id',
                                                        'utilization'])

_BlackoutChange = struct.Struct('B')
BlackoutChange = collections.namedtuple('BlackoutChange',
                                        ['is_blackout_start'])


def _unpack_overloaded_channels(num_channels, buf):
    """Extract the overloaded channels and return them.

    Args:
        num_channels (int): the number of channels specified in the message
        buf (str): the portion of the message containing the channels

    Returns:
        the list of channels

    Raises:
        :class:`MessageMalformedError`: message does not provide the
            specified number of channels
    """
    if num_channels != len(buf):
        raise MessageMalformedError("Unexpected length (%d) for %d channels" %
                                    (num_channels, len(buf)))

    return [ord(x) for x in buf]


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

    if version == common.Version.VERSION1:
        if msg_id == MessageID.OVERLOAD_CHANGE.value:
            unpacker = _OverloadChange_v1
            constructor = OverloadChange_v1
        elif msg_id == MessageID.UTILIZATION.value:
            unpacker = _Utilization_v1
            constructor = Utilization_v1
        else:
            raise MessageMalformedError("Unsupported message ID: %d" % msg_id)
    elif version == common.Version.VERSION2:
        if msg_id == MessageID.OVERLOAD_CHANGE.value:
            unpacker = _OverloadChange_v2
            constructor = OverloadChange_v2
        elif msg_id == MessageID.UTILIZATION.value:
            unpacker = _Utilization_v2
            constructor = Utilization_v2
        elif msg_id == MessageID.BLACKOUT_CHANGE.value:
            unpacker = _BlackoutChange
            constructor = BlackoutChange
        else:
            raise MessageMalformedError("Unsupported message ID: %d" % msg_id)
    else:
        raise MessageMalformedError("Invalid version number: %d" % version)

    if len(buf) < unpacker.size + 1:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size + 1))

    fields = unpacker.unpack(buf[1:(unpacker.size + 1)])

    if msg_id == MessageID.OVERLOAD_CHANGE.value:
        if version == common.Version.VERSION1:
            # Special handling is needed for an overload change as the overload
            # status for each band is encoded in bits.
            fields = (bool(fields[0] & 0x1), bool(fields[0] & 0x2),
                      bool(fields[1] & 0x1), bool(fields[1] & 0x2))
        elif version == common.Version.VERSION2:
            fields = (_unpack_overloaded_channels(fields[0],
                                                  buf[1 + unpacker.size:]),)

    payload = constructor._make(fields)

    return common.check_band(payload)

__all__ = ['MessageID', 'OverloadChange_v1', 'OverloadChange_v2',
           'Utilization_v1', 'Utilization_v2', 'BlackoutChange']
