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
"""

import struct
import collections

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.msgs.common import get_variable_length_str, get_variable_length_str_raw
import common

#: Supported message identifiers for tdService
MessageID = Enum('MessageID', [('LINK_UP', 1), ('LINK_DOWN', 2),
                               ('REMOTE_IF_UP', 4), ('REMOTE_IF_DOWN', 8),
                               ('REMOTE_DEV_UP', 16), ('REMOTE_DEV_DOWN', 32),
                               ('AGING', 64), ('BDA_CHANGE', 128),
                               ('ENTRY', 256)])

# No unpacker for link up / link down - it is just a variable length string
# for the interface name
LinkUp = collections.namedtuple('LinkUp',
                                ['iface'])
LinkDown = collections.namedtuple('LinkDown',
                                  ['iface'])

# Common unpacker for all the remote events - they are all just a MAC address
_tdMacAddrUnpacker = struct.Struct('6s')
RemoteIfaceUp = collections.namedtuple('RemoteIfaceUp', ['mac'])
RemoteIfaceDown = collections.namedtuple('RemoteIfaceDown', ['mac'])
RemoteDevUp = collections.namedtuple('RemoteDevUp', ['mac'])
RemoteDevDown = collections.namedtuple('RemoteDevDown', ['mac'])
BridgedDAChange = collections.namedtuple('BridgedDAChange', ['mac'])

Interface = collections.namedtuple('Interface', ['type', 'connected', 'mac'])

Entry = collections.namedtuple('Entry', ['mac',
                                         'num_ifaces',
                                         'iface_list',
                                         'num_bridged_da',
                                         'bridged_da_list'])


def unpack_td_entry(buf):
    """Unpack a td entry.

    Args:
        buf (str): the entire payload to be unpacked

    Returns:
        entry (namedtuple): the unpacked message as a namedtuple

    Raises:
        :class:`MessageMalformedError`: Invalid message
    """

    # Needs to be at least big enough for 1 MAC address, the interface
    # list length, and the bridged DA list length
    if len(buf) < 14:
        raise MessageMalformedError("Message too short: %d (need 14)" %
                                    (len(buf)))

    # Get the MAC address
    mac = _tdMacAddrUnpacker.unpack(buf[0:_tdMacAddrUnpacker.size])
    mac = common.ether_ntoa(mac[0])
    buf = buf[_tdMacAddrUnpacker.size:]

    # Get the interface list length
    num_ifaces = struct.unpack('>I', buf[:4])
    num_ifaces = num_ifaces[0]
    buf = buf[4:]

    # Read the interface list
    # Make sure there is enough buffer remaining - need at least 10 bytes per interface,
    # plus the length of the bridged DA list
    min_len_remaining = (10 * num_ifaces + 4)
    if len(buf) < min_len_remaining:
        raise MessageMalformedError("Message too short: %d (need %d), %d interfaces present" %
                                    (len(buf), min_len_remaining, num_ifaces))
    iface_list = []
    for i in range(0, num_ifaces):
        # Unpack the interface type string
        (buf, length, type_string) = get_variable_length_str_raw(buf)

        # Unpack whether or not the interface is connected
        connected = struct.unpack('>H', buf[:2])
        buf = buf[2:]

        # Unpack the MAC address
        iface_name = _tdMacAddrUnpacker.unpack(buf[0:_tdMacAddrUnpacker.size])
        buf = buf[_tdMacAddrUnpacker.size:]

        iface = Interface(type_string, connected[0], common.ether_ntoa(iface_name[0]))
        iface_list.append(iface)

    # Read the number of bridged DAs
    num_bridged_das = struct.unpack('>I', buf[:4])
    num_bridged_das = num_bridged_das[0]
    buf = buf[4:]

    # Read the bridged DA list
    if len(buf) < 6 * num_bridged_das:
        raise MessageMalformedError("Message too short: %d (need %d), %d bridged DAs present" %
                                    (len(buf), min_len_remaining, num_bridged_das))
    bridged_da_list = []
    for i in range(0, num_bridged_das):
        da = _tdMacAddrUnpacker.unpack(buf[0:_tdMacAddrUnpacker.size])
        buf = buf[_tdMacAddrUnpacker.size:]
        bridged_da_list.append(common.ether_ntoa(da[0]))

    entry = Entry(mac, num_ifaces, iface_list, num_bridged_das, bridged_da_list)

    return entry


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
    if sub_id == MessageID.LINK_UP.value or sub_id == MessageID.LINK_DOWN.value:
        if sub_id == MessageID.LINK_UP.value:
            link_event = LinkUp("")
        else:
            link_event = LinkDown("")
        (link_event, buf) = get_variable_length_str(link_event, buf, None,
                                                    'iface', 2)
        return (link_event, MessageID(sub_id))
    elif sub_id == MessageID.REMOTE_IF_UP.value:
        unpacker = _tdMacAddrUnpacker
        constructor = RemoteIfaceUp
    elif sub_id == MessageID.REMOTE_IF_DOWN.value:
        unpacker = _tdMacAddrUnpacker
        constructor = RemoteIfaceDown
    elif sub_id == MessageID.REMOTE_DEV_UP.value:
        unpacker = _tdMacAddrUnpacker
        constructor = RemoteDevUp
    elif sub_id == MessageID.REMOTE_DEV_DOWN.value:
        unpacker = _tdMacAddrUnpacker
        constructor = RemoteDevDown
    elif sub_id == MessageID.AGING.value:
        # Aging is an empty event
        return (None, MessageID.AGING)
    elif sub_id == MessageID.BDA_CHANGE.value:
        unpacker = _tdMacAddrUnpacker
        constructor = BridgedDAChange
    elif sub_id == MessageID.ENTRY.value:
        return (unpack_td_entry(buf), MessageID.ENTRY)
    else:
        raise MessageMalformedError("Unsupported message ID: %d" % sub_id)

    if len(buf) < unpacker.size:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size))

    fields = unpacker.unpack(buf[0:unpacker.size])
    payload = constructor._make(fields)

    payload = common.replace_mac_address_with_string(payload)
    return (payload, MessageID(sub_id))

__all__ = ['MessageID']
