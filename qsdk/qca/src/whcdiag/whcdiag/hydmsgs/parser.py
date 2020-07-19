#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import collections
import logging

import common
import ver
import msg
import pcwservice
import pcpservice
import heservice
import hetables
import psservice
import pstables
import tdservice

from whcdiag.msgs.exception import MessageMalformedError

# Store the version number per MAC address
version = {}


def get_version(mac):
    """Get the logging version used by the provided MAC address.

    Args:
        mac (str): MAC address of device

    Returns:
        version number used by mac.  Defaults to version 3 if no version logs
        have been parsed
    """
    if mac not in version:
        logging.info("%s: No version log received, defaulting to version 3 for parsing",
                     mac)
        version[mac] = ver.MessageID.VERSION_3

        return ver.MessageID.VERSION_3
    else:
        return version[mac]


def unpack_msg(buf):
    """Unpack a single diagnostic logging message.

    Args:
        buf (str): the raw message to unpack

    Returns:
        tuple of the diagnostic logging header and the payload, each as
        an appropriate namedtuple

    Raises:
        :class:`MessageMalformedError`: Message could not be parsed
    """
    n_bytes, header = common.unpack_header_from_bytes(buf)
    payload_bytes = buf[n_bytes:]

    if header.id == common.ModuleID.VER:
        (payload, sub_id) = ver.unpack_payload_from_bytes(header.sub_id,
                                                          payload_bytes)
        # Update the version based on this message (should always be the first received)
        version[header.mac] = sub_id
        logging.info("%s: hyd using version %d", header.mac, sub_id.value)
    elif header.id == common.ModuleID.MSG:
        (payload, sub_id) = msg.unpack_payload_from_bytes(header.sub_id,
                                                          payload_bytes)
    elif header.id == common.ModuleID.PCW2:
        msg_version = get_version(header.mac)
        (payload, sub_id) = pcwservice.unpack_payload_from_bytes(header.sub_id,
                                                                 payload_bytes,
                                                                 msg_version)
    elif header.id == common.ModuleID.PCW5:
        msg_version = get_version(header.mac)
        (payload, sub_id) = pcwservice.unpack_payload_from_bytes(header.sub_id,
                                                                 payload_bytes,
                                                                 msg_version)
    elif header.id == common.ModuleID.PCP:
        (payload, sub_id) = pcpservice.unpack_payload_from_bytes(header.sub_id,
                                                                 payload_bytes)
    elif header.id == common.ModuleID.HE:
        (payload, sub_id) = heservice.unpack_payload_from_bytes(header.sub_id,
                                                                payload_bytes)
    elif header.id == common.ModuleID.HE_TABLES:
        (payload, sub_id) = hetables.unpack_payload_from_bytes(header.sub_id,
                                                               payload_bytes)
    elif header.id == common.ModuleID.PS:
        msg_version = get_version(header.mac)
        (payload, sub_id) = psservice.unpack_payload_from_bytes(header.sub_id,
                                                                payload_bytes,
                                                                msg_version)
    elif header.id == common.ModuleID.PS_TABLES:
        (payload, sub_id) = pstables.unpack_payload_from_bytes(header.sub_id,
                                                               payload_bytes)
    elif header.id == common.ModuleID.TD:
        (payload, sub_id) = tdservice.unpack_payload_from_bytes(header.sub_id,
                                                                payload_bytes)
    else:
        raise MessageMalformedError("Unhandled module ID %d" % header.id.value)

    # Replace the sub_id in the header with the correct enum depending on
    # id type
    vals = {"sub_id": sub_id}
    header = header._replace(**vals)

    return (header, payload)


class Reassembler(object):

    """Used to reassemble fragmented logs.

    Attempts to delete incomplete logs.  If a log is received with more_fragments == 1,
    the next log from that address should have the same ID and SubID.  If not, it is
    assumed that the next fragment is lost, and the incomplete log is deleted.
    Multiple fragments of the same type can be stored, and when a log of the correct
    type is received, all the fragments are returned.

    Note that this simple scheme can't handle all cases (for example if the log
    contained multiple fragments, and the middle fragment is lost, the first
    fragment is lost, etc).  In these cases the lost fragments may be detected during
    log processing, or there will be a brief period where state may be out of sync
    until the next complete log is received.
    """

    def __init__(self):
        self._stored_data = collections.defaultdict(list)

    def _matches_stored_entry(self, src_addr, id, sub_id):
        """Check if a new log matches any stored ones for the same source address.

        Args:
            src_addr (str): source address of the log
            id (Enum): ID of the log
            sub_id (Enum): SubID of the log

        Returns:
            True if the new log matches stored logs, False if either there are
            not stored logs, or the new log doesn't match the stored logs.
        """
        if src_addr not in self._stored_data:
            # Not found
            return False

        first_entry = self._stored_data[src_addr][0]
        # There is a stored packet - check for match
        if id == first_entry[0].id and sub_id == first_entry[0].sub_id:
            # Found and match
            return True
        else:
            # Found and mismatch
            logging.error("%s: Message ID %s, Sub ID %s is missing fragments, deleting",
                          src_addr, first_entry[0].id.name, first_entry[0].sub_id.name)

            # Clear the dictionary entry
            del self._stored_data[src_addr]
            return False

    def reassemble(self, header, src_addr, object_to_store):
        """Attempt to reassemble a log.

        Args:
            header (namedtuple): log header
            src_addr (str): source address of the log
            object_to_store (object): object to store with the header

        Returns:
            tuple containing whether the log should be processed immediately,
            and a list of any other data that should also be processed immediately.
        """
        if not hasattr(header, 'more_fragments'):
            # Header doesn't contain more_fragments, so nothing to reassemble
            return (True, None)

        if header.more_fragments:
            self._matches_stored_entry(src_addr, header.id, header.sub_id)

            # Don't output this payload yet - store and wait for the
            # final fragment
            self._stored_data[src_addr].append((header, object_to_store))

            return (False, None)
        else:
            # No more fragments - make sure this is a continuation of the
            # stored packet (if any)
            ret_data = None

            if self._matches_stored_entry(src_addr, header.id,
                                          header.sub_id):
                # Matches - return all
                ret_data = self._stored_data[src_addr]

                # Clear the dictionary entry
                del self._stored_data[src_addr]

            return (True, ret_data)


__all__ = ['unpack_msg']
