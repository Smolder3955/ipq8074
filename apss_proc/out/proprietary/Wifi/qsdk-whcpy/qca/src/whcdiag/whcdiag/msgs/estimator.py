#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the estimator module.
#

"""Module providing the message unpacking for the Estimator module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following classes are exported:

    :class:`ServingDataMetrics`: measurement of throughput, MCS, and
        airtime on the serving bSS

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
MessageID = Enum('MessageID', [('SERVING_DATA_METRICS', 0),
                               ('NON_SERVING_DATA_METRICS', 1),
                               ('STA_INTERFERENCE_DETECTED', 2),
                               ('STA_POLLUTION_CHANGED', 3),
                               ('INTERFERENCE_STATS', 4)])

# Pollution change reason constants
STAPollutionChangeReason = Enum('PollutionChangeReason', [('POLLUTION_CHANGE_DETECTION', 0),
                                                          ('POLLUTION_CHANGE_AGING', 1),
                                                          ('POLLUTION_CHANGE_REMOTE', 2),
                                                          ('POLLUTION_CHANGE_INVALID', 3)])

_ServingDataMetricsFormat = '6s%dsLLLB' % common._BSSInfo.size
_ServingDataMetricsLE = struct.Struct('<' + _ServingDataMetricsFormat)  # little endian
_ServingDataMetricsBE = struct.Struct('>' + _ServingDataMetricsFormat)  # big endian
ServingDataMetrics = collections.namedtuple('ServingDataMetrics',
                                            ['addr', 'bss_info',
                                             'dl_throughput',
                                             'ul_throughput', 'last_tx_rate',
                                             'airtime'])

_NonServingDataMetricsFormat = '6s%dsLB' % common._BSSInfo.size
_NonServingDataMetricsLE = struct.Struct('<' + _NonServingDataMetricsFormat)  # little endian
_NonServingDataMetricsBE = struct.Struct('>' + _NonServingDataMetricsFormat)  # big endian
NonServingDataMetrics = collections.namedtuple('NonServingDataMetrics',
                                               ['addr', 'bss_info',
                                                'capacity', 'airtime'])

_STAInterferenceDetected = struct.Struct('6s%dsB' % common._BSSInfo.size)
STAInterferenceDetected = collections.namedtuple('STAInterferenceDetected',
                                                 ['addr', 'bss_info', 'interference_detected'])

_STAPollutionChanged = struct.Struct('6s%dsBB' % common._BSSInfo.size)
STAPollutionChanged = collections.namedtuple('STAPollutionChanged',
                                             ['addr', 'bss_info', 'polluted',
                                              'reason'])

_InterferenceStatsFormat = '6s%dsBHQL' % common._BSSInfo.size
_InterferenceStatsLE = struct.Struct('<' + _InterferenceStatsFormat)  # little endian
_InterferenceStatsBE = struct.Struct('>' + _InterferenceStatsFormat)  # big endian
InterferenceStats = collections.namedtuple('InterferenceStats',
                                           ['addr', 'bss_info', 'rssi', 'tx_rate',
                                            'byte_count_delta', 'packet_count_delta'])


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

    if version == common.Version.VERSION1:
        raise MessageMalformedError("Estimator messages not supported in v1")

    msg_id = ord(buf[0])

    if msg_id == MessageID.SERVING_DATA_METRICS.value:
        unpacker = _ServingDataMetricsBE if big_endian else _ServingDataMetricsLE
        constructor = ServingDataMetrics
    elif msg_id == MessageID.NON_SERVING_DATA_METRICS.value:
        unpacker = _NonServingDataMetricsBE if big_endian else _NonServingDataMetricsLE
        constructor = NonServingDataMetrics
    elif msg_id == MessageID.STA_INTERFERENCE_DETECTED.value:
        unpacker = _STAInterferenceDetected
        constructor = STAInterferenceDetected
    elif msg_id == MessageID.STA_POLLUTION_CHANGED.value:
        unpacker = _STAPollutionChanged
        constructor = STAPollutionChanged
    elif msg_id == MessageID.INTERFERENCE_STATS.value:
        unpacker = _InterferenceStatsBE if big_endian else _InterferenceStatsLE
        constructor = InterferenceStats
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % msg_id)

    if len(buf) < unpacker.size + 1:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size + 1))

    fields = list(unpacker.unpack(buf[1:]))
    payload = constructor._make(fields)

    # Transform special fields to make them more friendly.
    if hasattr(payload, 'addr'):
        vals = {'addr': common.ether_ntoa(getattr(payload, 'addr'))}
        payload = payload._replace(**vals)

    if hasattr(payload, 'bss_info'):
        (payload, buf) = common.get_bss_info(payload.bss_info, payload,
                                             'bss_info', 1)

    if hasattr(payload, 'reason'):
        payload = check_pollution_change_reason(payload)

    return common.check_band(payload)


def check_pollution_change_reason(payload):
    """Check whether the pollution change reason code is valid

    Args:
        payload (:class:`collections.namedtuple`): named tuple to check if
            the reason code is valid; the value of the field should just be
            the raw integer value for the reason code

    Returns:
        the message with the reason code converted to the STAPollutionChangeReason enum

    Raises:
        :class:`MessageMalformedError`: reason code is invalid
    """
    reason = getattr(payload, 'reason')
    try:
        STAPollutionChangeReason(reason)
    except ValueError:
        raise MessageMalformedError("Invalid reason code %d" % reason)

    vals = {'reason': STAPollutionChangeReason(reason)}
    return payload._replace(**vals)


__all__ = ['MessageID', 'ServingDataMetrics']
