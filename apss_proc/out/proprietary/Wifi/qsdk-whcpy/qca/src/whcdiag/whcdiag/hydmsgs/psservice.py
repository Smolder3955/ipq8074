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
import common
from whcdiag.msgs.common import get_variable_length_str
from pstables import unpack_hdefault_table
from ver import MessageID as MessageVersion

#: Supported message identifiers for psService
MessageID = Enum('MessageID', [('AGING', 1), ('FAILOVER', 2),
                               ('LOAD_BALANCING', 4), ('HDEFAULT', 8)])

# Unpacker is the same for each case (same structure)
_hActiveUnpacker = struct.Struct('>I6sIIIII')
_hActiveUnpacker_v3 = struct.Struct('>I6sIII')
Aging = collections.namedtuple('Aging',
                               ['hash', 'mac', 'sub_class', 'priority',
                                'psw_enable', 'psw_use', 'flow_rate',
                                'old_intf', 'new_intf'])
Aging_v3 = collections.namedtuple('Aging',
                                  ['hash', 'mac', 'sub_class', 'priority',
                                   'flow_rate', 'old_intf', 'new_intf'])

Failover = collections.namedtuple('Failover',
                                  ['hash', 'mac', 'sub_class', 'priority',
                                   'psw_enable', 'psw_use', 'flow_rate',
                                   'old_intf', 'new_intf'])
Failover_v3 = collections.namedtuple('Failover',
                                     ['hash', 'mac', 'sub_class', 'priority',
                                      'flow_rate', 'old_intf', 'new_intf'])

LoadBalancing = collections.namedtuple('LoadBalancing',
                                       ['hash', 'mac', 'sub_class', 'priority',
                                        'psw_enable', 'psw_use', 'flow_rate',
                                        'old_intf', 'new_intf'])
LoadBalancing_v3 = collections.namedtuple('LoadBalancing',
                                          ['hash', 'mac', 'sub_class', 'priority',
                                           'flow_rate', 'old_intf', 'new_intf'])


def unpack_payload_from_bytes(sub_id, buf, msg_version):
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

    if sub_id == MessageID.AGING.value:
        if msg_version == MessageVersion.VERSION_1 or \
                msg_version == MessageVersion.VERSION_2:
            constructor = Aging
        else:
            constructor = Aging_v3
    elif sub_id == MessageID.FAILOVER.value:
        if msg_version == MessageVersion.VERSION_1 or \
                msg_version == MessageVersion.VERSION_2:
            constructor = Failover
        else:
            constructor = Failover_v3
    elif sub_id == MessageID.LOAD_BALANCING.value:
        if msg_version == MessageVersion.VERSION_1 or \
                msg_version == MessageVersion.VERSION_2:
            constructor = LoadBalancing
        else:
            constructor = LoadBalancing_v3
    elif sub_id == MessageID.HDEFAULT.value:
        return unpack_hdefault_table(buf)
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)

    if msg_version == MessageVersion.VERSION_1 or \
            msg_version == MessageVersion.VERSION_2:
                unpacker = _hActiveUnpacker
    else:
        unpacker = _hActiveUnpacker_v3

    if len(buf) < unpacker.size + 1:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size + 1))

    # Unpack the fixed length portion
    fields = unpacker.unpack(buf[0:unpacker.size])
    # Two variable length strings after the fixed length portion
    fields = list(fields) + ["", ""]
    payload = constructor._make(fields)

    # Unpack the variable length strings
    buf = buf[unpacker.size:]
    (payload, buf) = get_variable_length_str(payload, buf, None, 'old_intf', 2)
    (payload, buf) = get_variable_length_str(payload, buf, None, 'new_intf', 2)

    payload = common.replace_mac_address_with_string(payload)
    return (payload, MessageID(sub_id))

__all__ = ['MessageID']
