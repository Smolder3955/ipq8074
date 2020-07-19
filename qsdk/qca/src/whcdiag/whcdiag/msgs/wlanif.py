#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the WLAN interface module.
#

"""Module providing the message unpacking for the WLANIF module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following classes are exported:

    :class:`RawChannelUtilization`: a measurement of the channel utilization
        on a given band
    :class:`RawRSSI`: a measurement of the RSSI for a single STA
    :class:`BeaconReport`: beacon report sent by a STA

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import struct
import collections

from enum import Enum

from exception import MessageMalformedError
import common

#: Supported message identifiers for WLANIF
MessageID = Enum('MessageID', [('RAW_CHANNEL_UTILIZATION', 0),
                               ('RAW_RSSI', 1),
                               ('INTERFACE', 2),
                               ('BEACON_REPORT', 3)])

_RawChannelUtilization = struct.Struct('BB')
RawChannelUtilization = collections.namedtuple('RawChannelUtilization',
                                               ['band', 'utilization'])

_RawChannelUtilization_v2 = struct.Struct('BB')
RawChannelUtilization_v2 = collections.namedtuple('RawChannelUtilization',
                                                  ['channel_id', 'utilization'])

_RawRSSI = struct.Struct('6sBB')
RawRSSI = collections.namedtuple('RawRSSI', ['sta_addr', 'band', 'rssi'])

_RawRSSI_v2 = struct.Struct('6s%dsB' % common._BSSInfo.size)
RawRSSI_v2 = collections.namedtuple('RawRSSI', ['sta_addr', 'bss_info', 'rssi'])

_Interface = struct.Struct('6sBB')
Interface = collections.namedtuple('Interface', ['addr', 'channel', 'essid',
                                                 'ssid_len', 'ssid',
                                                 'ifname_len', 'ifname'])

_BeaconReport = struct.Struct('6s%dsb' % common._BSSInfo.size)
BeaconReport = collections.namedtuple('BeaconReport', ['sta_addr', 'bss_info', 'rcpi'])


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

    if msg_id == MessageID.RAW_CHANNEL_UTILIZATION.value:
        if version == common.Version.VERSION2:
            unpacker = _RawChannelUtilization_v2
            constructor = RawChannelUtilization_v2
        else:
            unpacker = _RawChannelUtilization
            constructor = RawChannelUtilization
    elif msg_id == MessageID.RAW_RSSI.value:
        if version == common.Version.VERSION2:
            unpacker = _RawRSSI_v2
            constructor = RawRSSI_v2
        else:
            unpacker = _RawRSSI
            constructor = RawRSSI
    elif msg_id == MessageID.INTERFACE.value:
        if version == common.Version.VERSION1:
            raise MessageMalformedError("Unsupported message ID: %d" % msg_id)
        else:
            unpacker = _Interface
            constructor = Interface
    elif msg_id == MessageID.BEACON_REPORT.value:
        if version == common.Version.VERSION1:
            raise MessageMalformedError("Unsupported message ID: %d" % msg_id)
        else:
            unpacker = _BeaconReport
            constructor = BeaconReport
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % msg_id)

    if len(buf) < unpacker.size + 1:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size + 1))

    # This message contains a MAC address, which we convert to a more
    # convenient string representation (a human readable one).
    if msg_id == MessageID.RAW_RSSI.value or msg_id == MessageID.BEACON_REPORT.value:
        fields = unpacker.unpack(buf[1:])
        payload = constructor._make([common.ether_ntoa(fields[0])] +
                                    list(fields)[1:])
    elif msg_id == MessageID.INTERFACE.value:
        # Variable length message
        # First unpack the fixed size portion
        fields = unpacker.unpack(buf[1:unpacker.size+1])
        fields = fields + (None, None, None, None)
        payload = constructor._make([common.ether_ntoa(fields[0])] +
                                    list(fields)[1:])
        (payload, buf) = common.get_variable_length_str(payload, buf[unpacker.size+1:],
                                                        'ssid_len', 'ssid')
        (payload, buf) = common.get_variable_length_str(payload, buf,
                                                        'ifname_len', 'ifname')
    else:
        fields = list(unpacker.unpack(buf[1:]))
        payload = constructor._make(fields)

    if hasattr(payload, 'bss_info'):
        (payload, buf) = common.get_bss_info(payload.bss_info, payload,
                                             'bss_info', 1)

    return common.check_band(payload)

__all__ = ['MessageID', 'RawChannelUtilization', 'RawChannelUtilization_v2',
           'RawRSSI', 'RawRSSI_v2', 'BeaconReport']
