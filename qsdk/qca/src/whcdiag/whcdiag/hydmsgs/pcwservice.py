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

"""Module providing the message unpacking for the heService module.

The following types are exported:

    :obj:`MessageID`: identifier for the message
"""

import struct
import collections

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError
from ver import MessageID as MessageVersion
import common

#: Supported message identifiers for pcwService
MessageID = Enum('MessageID', [('RAW_STA', 1), ('RAW_AP', 2),
                               ('RAW_STA_AP', 4), ('GET_MU', 8),
                               ('GET_LC', 16), ('EVENTS', 32),
                               ('SUMMARY', 64)])

_RawSTA = struct.Struct('>IIIIIIIIIIIIIIHHHHHHHHHHHH')
RawSTA = collections.namedtuple('RawSTA',
                                ['utility',
                                 'tx_buf',
                                 'throughput',
                                 'capacity',
                                 'aggregation',
                                 'phy_err',
                                 'per',
                                 'msdu_size',
                                 'tcp_raw_throughput',
                                 'udp_raw_throughput',
                                 'tcp_full_capacity',
                                 'udp_full_capacity',
                                 'tcp_available_capacity',
                                 'udp_available_capacity',
                                 'no_bufs_0',
                                 'no_bufs_1',
                                 'no_bufs_2',
                                 'no_bufs_3',
                                 'exc_retries_0',
                                 'exc_retries_1',
                                 'exc_retries_2',
                                 'exc_retries_3',
                                 'tx_pkt_cnt_0',
                                 'tx_pkt_cnt_1',
                                 'tx_pkt_cnt_2',
                                 'tx_pkt_cnt_3'])

_RawSTA_v2 = struct.Struct('>6sIIIIIIIIIIIIIIHHHHHHHHHHHH')
RawSTA_v2 = collections.namedtuple('RawSTA',
                                   ['iface', 'utility',
                                    'tx_buf',
                                    'throughput',
                                    'capacity',
                                    'aggregation',
                                    'phy_err',
                                    'per',
                                    'msdu_size',
                                    'tcp_raw_throughput',
                                    'udp_raw_throughput',
                                    'tcp_full_capacity',
                                    'udp_full_capacity',
                                    'tcp_available_capacity',
                                    'udp_available_capacity',
                                    'no_bufs_0',
                                    'no_bufs_1',
                                    'no_bufs_2',
                                    'no_bufs_3',
                                    'exc_retries_0',
                                    'exc_retries_1',
                                    'exc_retries_2',
                                    'exc_retries_3',
                                    'tx_pkt_cnt_0',
                                    'tx_pkt_cnt_1',
                                    'tx_pkt_cnt_2',
                                    'tx_pkt_cnt_3'])

_APLink = struct.Struct('>6sIIIIIIIIIII')
APLink = collections.namedtuple('APLink',
                                ['mac',
                                 'capacity',
                                 'aggregation',
                                 'phy_err',
                                 'per',
                                 'msdu_size',
                                 'tcp_raw_throughput',
                                 'udp_raw_throughput',
                                 'tcp_full_capacity',
                                 'udp_full_capacity',
                                 'tcp_available_capacity',
                                 'udp_available_capacity'])

_RawAP = struct.Struct('>IIII')
RawAP = collections.namedtuple('RawAP',
                               ['utility',
                                'tx_buf',
                                'throughput',
                                'num_links',
                                'ap_links'])

_RawAP_v2 = struct.Struct('>6sIIII')
RawAP_v2 = collections.namedtuple('RawAP',
                                  ['iface', 'utility',
                                   'tx_buf',
                                   'throughput',
                                   'num_links',
                                   'ap_links'])

_STAAPLink = struct.Struct('>6sIIII')
STAAPLink = collections.namedtuple('STAAPLink',
                                   ['mac', 'tcp_full_capacity',
                                    'udp_full_capacity', 'tcp_available_capacity',
                                    'udp_available_capacity'])

_RawSTAAP = struct.Struct('>II')
RawSTAAP = collections.namedtuple('RawSTAAP',
                                  ['num_links',
                                   'medium_utilization',
                                   'ap_links'])

_RawSTAAP_v2 = struct.Struct('>6sII')
RawSTAAP_v2 = collections.namedtuple('RawSTAAP',
                                     ['iface', 'num_links',
                                      'medium_utilization',
                                      'ap_links'])

_MediumUtilization = struct.Struct('>III')
MediumUtilization = collections.namedtuple('MediumUtilization',
                                           ['measure_time',
                                            'receive_time',
                                            'mu'])

_LinkCapacity = struct.Struct('>II6sHHI')
LinkCapacity = collections.namedtuple('LinkCapacity',
                                      ['measure_time',
                                       'receive_time',
                                       'mac',
                                       'full',
                                       'udp',
                                       'lc'])

_Events = struct.Struct('>HHHHH6s')
Events = collections.namedtuple('Events',
                                ['oversubscribed',
                                 'medium_utilization_changed',
                                 'link_capacity_change',
                                 'buffer_threshold',
                                 'excessive_retries',
                                 'bad_link_da'])

_Summary = struct.Struct('>6sIIII')
Summary = collections.namedtuple('Summary', ['mac', 'mu', 'max_mu',
                                 'lc_available', 'lc_full'])


def unpack_payload_from_bytes(sub_id, buf, version):
    """Unpack the payload portion of the message provided.

    Args:
        sub_id (int): the type of the message
        buf (str): the entire payload to be unpacked
        version (Enum): version to use for parsing this message

    Returns:
        tuple containing the unpacked header and message ID:

        payload (namedtuple): the unpacked message as a namedtuple of the right type
        MessageID (Enum): the type of the message converted to an Enum

    Raises:
        :class:`MessageMalformedError`: Invalid message
    """
    if sub_id == MessageID.RAW_STA.value:
        if version == MessageVersion.VERSION_1:
            unpacker = _RawSTA
            constructor = RawSTA
        else:
            unpacker = _RawSTA_v2
            constructor = RawSTA_v2
    elif sub_id == MessageID.RAW_AP.value:
        if version == MessageVersion.VERSION_1:
            unpacker = _RawAP
            constructor = RawAP
        else:
            unpacker = _RawAP_v2
            constructor = RawAP_v2
    elif sub_id == MessageID.RAW_STA_AP.value:
        if version == MessageVersion.VERSION_1:
            unpacker = _RawSTAAP
            constructor = RawSTAAP
        else:
            unpacker = _RawSTAAP_v2
            constructor = RawSTAAP_v2
    elif sub_id == MessageID.GET_MU.value:
        # Should only be generated in version 1
        if version != MessageVersion.VERSION_1:
            raise MessageMalformedError("GET_MU should only be generated in version 1")
        unpacker = _MediumUtilization
        constructor = MediumUtilization
    elif sub_id == MessageID.GET_LC.value:
        # Should only be generated in version 1
        if version != MessageVersion.VERSION_1:
            raise MessageMalformedError("GET_LC should only be generated in version 1")
        unpacker = _LinkCapacity
        constructor = LinkCapacity
    elif sub_id == MessageID.EVENTS.value:
        unpacker = _Events
        constructor = Events
    elif sub_id == MessageID.SUMMARY.value:
        unpacker = _Summary
        constructor = Summary
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)

    if len(buf) < unpacker.size:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size))

    fields = unpacker.unpack(buf[0:unpacker.size])

    if sub_id == MessageID.RAW_AP.value or sub_id == MessageID.RAW_STA_AP.value:
        # Variable length messages
        # First unpack the fixed size portion
        fields = list(fields) + [0]
        payload = constructor._make(fields)
        buf = buf[unpacker.size:]

        # Now unpack the links
        if sub_id == MessageID.RAW_AP.value:
            payload = common.unpack_link(payload, buf, 'num_links', 'ap_links',
                                         _APLink, APLink)
        else:
            payload = common.unpack_link(payload, buf, 'num_links', 'ap_links',
                                         _STAAPLink, STAAPLink)
    else:
        payload = constructor._make(list(fields))

    if sub_id == MessageID.EVENTS.value:
        payload = common.replace_mac_address_with_string(payload, 'bad_link_da')
    else:
        payload = common.replace_mac_address_with_string(payload)

    if version != MessageVersion.VERSION_1 and (sub_id == MessageID.RAW_AP.value or
                                                sub_id == MessageID.RAW_STA_AP.value or
                                                sub_id == MessageID.RAW_STA.value):
        payload = common.replace_mac_address_with_string(payload, 'iface')

    return (payload, MessageID(sub_id))

__all__ = ['MessageID']
