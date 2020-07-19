#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for common diagnostic logging header.
#

"""Module providing the common types for HYD diagnostic logging.

The following types are exported:

    :obj:`ModuleID`: identifier for the module from which a log is generated

The following classes are exported:

    :class:`Header`: named tuple corresponding to the common diagnostic
        logging header

The following functions are exported:

    :func:`unpack_header_from_bytes`: unpack the header portion from an
        entire log message
"""

import struct
import collections
import itertools

from enum import Enum

from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.msgs.common import ether_ntoa
from whcdiag.msgs.common import get_variable_length_str
from whcdiag.constants import TRANSPORT_PROTOCOL

#: Modules that generate diagnostic logs
ModuleID = Enum('ModuleID', [('VER', 1), ('MSG', 2), ('PCW2', 4), ('PCW5', 8),
                             ('PCP', 16), ('HE', 32), ('PS', 64), ('TD', 128),
                             ('HE_TABLES', 256), ('PS_TABLES', 512)])

# MSB in the ModuleID field is set to indicates more fragments are coming for this log
MORE_FRAGMENTS = 1 << 15

# Supported interface types
InterfaceType = Enum('InterfaceType', [('WLAN2G', ord('2')), ('WLAN5G', ord('5')),
                                       ('PLC', ord('P')), ('ETHER', ord('E')),
                                       ('MOCA', ord('M'))])

# Overall format string
_HeaderFormat = struct.Struct('>HHII6s')
Header = collections.namedtuple('Header', ['id', 'sub_id',
                                           'timestamp_secs', 'timestamp_usecs',
                                           'mac', 'more_fragments'])


def unpack_header_from_bytes(buf):
    """Unpack the header portion of the message provided.

    Args:
        buf (str): the entire message to be unpacked

    Returns:
        tuple containing the unpacked header and number of bytes consumed:

        hdr (:obj:`Header`): the fields in the header
        n (int): the number of bytes consumed in buf
    """

    fields = _HeaderFormat.unpack(buf[0:_HeaderFormat.size])

    # Convert the module ID into an enum value.
    fields = list(fields)

    # Check if the more_fragments bit is set
    if fields[0] & MORE_FRAGMENTS:
        more_fragments = True
        fields[0] &= ~MORE_FRAGMENTS
    else:
        more_fragments = False

    fields.append(more_fragments)

    try:
        fields[0] = ModuleID(fields[0])
    except:
        raise MessageMalformedError("Unsupported module ID: %d" % fields[0])

    header = Header._make(itertools.chain(fields))

    header = replace_mac_address_with_string(header)

    return (_HeaderFormat.size, header)


def replace_mac_address_with_string(payload, attr_name='mac'):
    """Replace the MAC address in the tuple with a formatted string.

    Args:
        payload (tuple): tuple containing a message
        attr_name (str): string containing the name of the MAC address field

    Returns:
        tuple updated with the MAC address in attr_name (if present)
            replaced with a string representation
    """

    if hasattr(payload, attr_name):
        mac_addr = getattr(payload, attr_name)
        mac_addr = ether_ntoa(mac_addr)

        vals = {attr_name: mac_addr}
        payload = payload._replace(**vals)

    return payload


def replace_iface_char_with_string(payload, attr_name='iface_type'):
    """Replace the interface character in the tuple with a string.

    Args:
        payload (tuple): tuple containing a message
        attr_name (str): string containing the name of the interface type field

    Returns:
        tuple updated with the interface type in attr_name (if present)
            replaced with a string representation
    """

    if hasattr(payload, attr_name):
        iface_type = getattr(payload, attr_name)
        try:
            iface = InterfaceType(iface_type)
        except:
            raise MessageMalformedError("Unsupported interface type: %c" % chr(iface_type))

        vals = {attr_name: iface}
        payload = payload._replace(**vals)

    return payload


def unpack_link(payload, buf, link_count_name, link_name, unpacker, constructor):
    """Unpack an array of links from the buffer, and store in the tuple.

    Args:
        payload (tuple): tuple containing a message
        buf (str): the entire message to be unpacked
        link_count_name (str): name of the link count attribute in the tuple
        link_name (str): name of the link attribute in the tuple
        unpacker (Struct): struct to use to unpack each link
        constructor (tuple): constructor to use to make each link

    Returns:
        tuple updated with the array of links in link_name
    """

    links = []
    link_count = getattr(payload, link_count_name)
    for i in range(0, link_count):
        fields = unpacker.unpack(buf[:unpacker.size])
        fields = list(fields)
        link = constructor._make(fields)
        link = replace_mac_address_with_string(link)
        links.append(link)
        buf = buf[unpacker.size:]

    vals = {link_name: links}
    payload = payload._replace(**vals)
    return payload


def unpack_table(buf, unpacker, constructor, mac_names, iface_types,
                 variable_length_string_names, sub_class=None):
    """Unpack a table from the buffer, updating some fields as needed

    The entire buffer should be consumed, there is no length field indicating
    the number of rows.

    Args:
        buf (str): the entire message to be unpacked
        unpacker (Struct): struct to use to unpack each row
        constructor (tuple): constructor to use to make each row
        mac_names (list): list of field names that contain MAC addresses
        iface_types (list): list of field names that interface types
        variable_length_string_names (list): list of field names that contain
            variable length strings
        sub_class (str): string containing the name of the sub_class (transport protocol)
            field

    Returns:
        array of table rows

    Raises:
        :class:`MessageMalformedError`: There are not an integer number of rows
            in the table
    """

    rows = []
    while len(buf) >= unpacker.size:
        fields = unpacker.unpack(buf[:unpacker.size])
        fields = list(fields)
        for string_name in variable_length_string_names:
            fields.append("")
        row = constructor._make(fields)
        buf = buf[unpacker.size:]

        # now read out the variable length fields
        for string_name in variable_length_string_names:
            (row, buf) = get_variable_length_str(row, buf, None, string_name, 2)

        # Update the MAC names
        for name in mac_names:
            row = replace_mac_address_with_string(row, name)

        # Update the interface types
        for iface_type in iface_types:
            row = replace_iface_char_with_string(row, iface_type)

        # Update the subclass
        if sub_class:
            vals = {sub_class: TRANSPORT_PROTOCOL(row.sub_class)}
            row = row._replace(**vals)

        rows.append(row)

    # Should have consumed all of buf at this point
    if len(buf):
        raise MessageMalformedError("Non-integer number of rows: %d bytes remain" % len(buf))

    return rows

__all__ = ['ModuleID', 'Header', 'unpack_header_from_bytes']
