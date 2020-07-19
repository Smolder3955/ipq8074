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

"""Module providing the message unpacking for the pcpService module.

The following types are exported:

    :obj:`MessageID`: identifier for the message
"""

import struct
import collections

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError
import common

#: Supported message identifiers for pcpService
MessageID = Enum('MessageID', [('RAW_MU', 1), ('RAW_LC', 2),
                               ('GET_MU', 4), ('GET_LC', 8),
                               ('EVENTS', 16), ('SUMMARY', 32),
                               ('RAW_HOST_IF', 64)])

_RawMU = struct.Struct('>IIIIIIII')
RawMU = collections.namedtuple('RawMU',
                               ['measure_time', 'TDMA_time', 'non_HPAVLN_time',
                                'CAP0_time', 'CAP1_time',
                                'CAP2_time', 'CAP3_time', 'mu'])

_RawLCLink = struct.Struct('>6sIIII')
RawLCLink = collections.namedtuple('RawLCLink',
                                   ['mac', 'tx_udp_rate', 'rx_udp_rate',
                                    'tx_tcp_rate', 'rx_tcp_rate'])

_RawLC = struct.Struct('>II')
RawLC = collections.namedtuple('RawLC',
                               ['measure_time', 'num_links', 'links'])

_GetMU = struct.Struct('>II')
GetMU = collections.namedtuple('GetMU', ['measure_time', 'mu'])

_GetLC = struct.Struct('>I6sHHI')
GetLC = collections.namedtuple('GetLC', ['measure_time', 'mac',
                                         'full', 'udp', 'lc'])

_Events = struct.Struct('>HHHH')
Events = collections.namedtuple('Events',
                                ['oversubscribed', 'host_interface_congestion',
                                 'mu_change', 'lc_change'])

_Summary = struct.Struct('>6sIIIIII')
Summary = collections.namedtuple('Summary',
                                 ['mac', 'mu', 'max_mu', 'lc_plc_avail',
                                  'lc_plc_full', 'lc_e2e_avail', 'lc_e2e_full'])

_RawHostIF = struct.Struct('>III')
RawHostIF = collections.namedtuple('RawHostIF',
                                   ['num_flows', 'host_iface_usage',
                                    'host_iface_speed'])


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

    if sub_id == MessageID.RAW_MU.value:
        unpacker = _RawMU
        constructor = RawMU
    elif sub_id == MessageID.RAW_LC.value:
        unpacker = _RawLC
        constructor = RawLC
    elif sub_id == MessageID.GET_MU.value:
        unpacker = _GetMU
        constructor = GetMU
    elif sub_id == MessageID.GET_LC.value:
        unpacker = _GetLC
        constructor = GetLC
    elif sub_id == MessageID.EVENTS.value:
        unpacker = _Events
        constructor = Events
    elif sub_id == MessageID.SUMMARY.value:
        unpacker = _Summary
        constructor = Summary
    elif sub_id == MessageID.RAW_HOST_IF.value:
        unpacker = _RawHostIF
        constructor = RawHostIF
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)

    if len(buf) < unpacker.size:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size))

    fields = unpacker.unpack(buf[0:unpacker.size])
    if sub_id == MessageID.RAW_LC.value:
        # Variable length message
        # First unpack the fixed size portion
        fields = list(fields) + [0]
        payload = constructor._make(fields)
        buf = buf[unpacker.size:]
        # Now unpack the links
        payload = common.unpack_link(payload, buf, 'num_links', 'links',
                                     _RawLCLink, RawLCLink)
    else:
        payload = constructor._make(fields)

    payload = common.replace_mac_address_with_string(payload)
    return (payload, MessageID(sub_id))

__all__ = ['MessageID']
