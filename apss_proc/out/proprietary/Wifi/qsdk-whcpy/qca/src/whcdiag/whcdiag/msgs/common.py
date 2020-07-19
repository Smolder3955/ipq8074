#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for common diagnostic logging header.
#

"""Module providing the common types for diagnostic logging.

The following types are exported:

    :obj:`Version`: the version number in the header
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

from exception import MessageMalformedError
from whcdiag.constants import BAND_TYPE

#: Protocol version number
Version = Enum('Version', [('VERSION1', 1), ('VERSION2', 2)])

#: Modules that generate diagnostic logs
ModuleID = Enum('ModuleID', [('WLANIF', 1), ('BANDMON', 2), ('STADB', 5),
                             ('STEEREXEC', 6), ('STAMON', 7), ('DIAGLOG', 8),
                             ('ESTIMATOR', 9)])

# Overall format string (without byte ordering indicator)
_HeaderFormatString = 'BLLB'

_LittleEndianHeader = struct.Struct('<' + _HeaderFormatString)
_BigEndianHeader = struct.Struct('>' + _HeaderFormatString)

Header = collections.namedtuple('Header', ['version', 'big_endian', 'seq_num',
                                           'timestamp_secs', 'timestamp_usecs',
                                           'module_id'])

_BSSInfo = struct.Struct('BBB')
BSSInfo = collections.namedtuple('BSSInfo', ['ap', 'channel', 'ess'])


def unpack_header_from_bytes(buf):
    """Unpack the header portion of the message provided.

    Args:
        buf (str): the entire message to be unpacked

    Returns:
        tuple containing the unpacked header and number of bytes consumed:

        hdr (:obj:`Header`): the fields in the header
        n (int): the number of bytes consumed in buf

    Raises:
        :class:`MessageMalformedError`: Unsupported version number
    """
    version_endianness = ord(buf[0])
    version = (version_endianness & 0xf0) >> 4
    big_endian = bool(version_endianness & 0x0f)

    if version != Version.VERSION1.value and version != Version.VERSION2.value:
        raise MessageMalformedError("Unsupported version: %d" % version)

    header_format = _BigEndianHeader if big_endian else _LittleEndianHeader
    fields = header_format.unpack(buf[1:1 + header_format.size])

    # Convert the module ID into an enum value.
    fields = list(fields)
    fields[-1] = ModuleID(fields[-1])

    header = Header._make(itertools.chain([Version(version), big_endian], fields))
    return (1 + header_format.size, header)


def check_band(msg, allow_invalid=False, attr_name='band'):
    """Check whether the band is valid (if one exists).

    Args:
        msg (:class:`collections.namedtuple`): named tuple to check if
            the band field is valid (if one even exists); the value of
            the field should just be the raw integer value for the band
        allow_invalid (bool): whether the invalid band should be permitted
            or not
        attr_name (str): the name of the attribute to check for

    Returns:
        the message with the band attribute converted to the BAND_TYPE enum
        (if it existed)

    Raises:
        :class:`MessageMalformedError`: Band is invalid
    """
    if hasattr(msg, attr_name):
        band = getattr(msg, attr_name)

        # Sanity check the band and generate an error if it is not one
        # of the values expected.
        band_invalid = False
        try:
            BAND_TYPE(band)
        except ValueError:
            band_invalid = True

        if band_invalid or (not allow_invalid and
                            band == BAND_TYPE.BAND_INVALID.value):
            raise MessageMalformedError("Invalid band %d" % band)
        else:
            vals = {attr_name: BAND_TYPE(getattr(msg, attr_name))}
            return msg._replace(**vals)
    else:
        return msg


def ether_ntoa(ether_addr):
    """Convert a packed ethernet address into a string.

    This is meant to act similarly to the C ether_ntoa function.

    Args:
        ether_addr (str): the packed MAC address (must be length 6)

    Returns:
        the MAC address formatted as normal (with colon separators)

    Raises:
        :class:`ValueError`: Address is of improper length
    """
    if len(ether_addr) != 6:
        raise ValueError("Malformed MAC address")

    return ':'.join(['%02x' % ord(x) for x in ether_addr])


def ether_aton(ether_addr):
    """Convert a string representation of an ethernet address to a packed format.

    This is meant to act similarly to the C ether_aton function.

    Args:
        ether_addr (str): the MAC address formatted as normal (with colon separators)

    Returns:
        the packed MAC address (of length 6)

    Raises:
        :class:`ValueError`: Address is of improper length
    """
    str = ether_addr.split(':')
    if len(str) != 6:
        raise ValueError("Malformed MAC address")

    ret_str = ""
    for split_string in str:
        ret_str += chr(int(split_string, base=16))

    return ret_str


def get_bss_info(buf, payload, attr_name, bss_count, force_array=False):
    """ Convert a string containing a lbd_bssInfo_t structure into a tuple

    Will place the tuple into the payload in the attribute identified by attr_name.
    If there is a single BSS present, will just put the tuple in the payload,
    otherwise will put the array of tuples in the payload.

    Args:
       buf (str): buffer containing the lbd_bssInfo_t structure
       payload (tuple): named tuple to place the parsed lbd_bssInfo_t structure in
       attr_name (str): string containing the name of the lbd_bssInfo_t tuple
       bss_count (int): number of BSSes to parse
       force_array (bool): if True, force the value to an array even if it
            only has one entry

    Returns:
        payload with the lbd_bssInfo_t tuple added
        buf updated to the start of the next character past the lbd_bssInfo_t structure

    """
    bss_array = []
    for i in range(0, bss_count):
        bss_array.append(BSSInfo._make(_BSSInfo.unpack(buf[:_BSSInfo.size])))
        buf = buf[_BSSInfo.size:]

    if bss_count == 1 and not force_array:
        vals = {attr_name: bss_array[0]}
    else:
        vals = {attr_name: bss_array}
    payload = payload._replace(**vals)

    return (payload, buf)


def get_variable_length_str(payload, buf, length_name, string_name, length_size=1,
                            big_endian=True):
    """Unpack a variable length string from the buffer.

    Args:
        payload (namedtuple): processed message
        buf (str): the entire payload to be unpacked
        length_name (str): name of the parameter to store the string length
        string_name (str): name of the parameter to store the string
        length_size (int): the size of the string length parameter

    Returns:
        the unpacked message as a namedtuple of the right type
        buf updated to point beyond the end of the variable length string

    Raises:
        :class:`MessageMalformedError`: Incorrectly formed string
    """

    (buf, length_val, string_val) = get_variable_length_str_raw(buf, length_size, big_endian)

    if length_name:
        vals = {length_name: length_val}
        payload = payload._replace(**vals)

    # Get the string
    vals = {string_name: string_val}
    payload = payload._replace(**vals)

    return (payload, buf)


def get_variable_length_str_raw(buf, length_size=2, big_endian=True):
    """Unpack a variable length string from the buffer.

    Args:
        buf (str): the entire payload to be unpacked
        length_size (int): the size of the string length parameter
        big_endian (bool): set to true if the string length is big endian

    Returns:
        buf updated to point beyond the end of the variable length string
        length_val length of the variable length string
        string_val variable length string

    Raises:
        :class:`MessageMalformedError`: Incorrectly formed string
    """

    if len(buf) < length_size:
        raise MessageMalformedError("Message too short for fetching length of "
                                    "variable length string: %d" % len(buf))
    if length_size == 1:
        temp = struct.unpack('B', buf[:length_size])
    elif length_size == 2:
        if big_endian:
            temp = struct.unpack('>H', buf[:length_size])
        else:
            temp = struct.unpack('<H', buf[:length_size])
    else:
        raise MessageMalformedError("Unsupported size of length of variable length string"
                                    ": %d" % length_size)
    length_val = temp[0]

    # Get the string
    if len(buf) < length_val+length_size:
        raise MessageMalformedError("Message too short for variable length string: %d (need %d)" %
                                    (len(buf), length_val+length_size))
    temp = struct.unpack('%ds' % length_val, buf[length_size:length_val+length_size])
    string_val = temp[0]

    buf = buf[length_val+length_size:]

    return (buf, length_val, string_val)


__all__ = ['Version', 'ModuleID', 'Header', 'unpack_header_from_bytes',
           'check_band', 'ether_ntoa', 'get_bss_info', 'get_variable_length_str', '_BSSInfo']
