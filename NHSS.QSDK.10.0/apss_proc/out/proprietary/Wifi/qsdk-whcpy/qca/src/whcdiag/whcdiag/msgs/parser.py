#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import common
import wlanif
import bandmon
import stadb
import steerexec
import diaglog
import estimator
import exception


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

    if header.module_id == common.ModuleID.WLANIF:
        payload = wlanif.unpack_payload_from_bytes(header.version,
                                                   header.big_endian,
                                                   payload_bytes)
    elif header.module_id == common.ModuleID.BANDMON:
        payload = bandmon.unpack_payload_from_bytes(header.version,
                                                    header.big_endian,
                                                    payload_bytes)
    elif header.module_id == common.ModuleID.STADB:
        payload = stadb.unpack_payload_from_bytes(header.version,
                                                  header.big_endian,
                                                  payload_bytes)
    elif header.module_id == common.ModuleID.STEEREXEC:
        payload = steerexec.unpack_payload_from_bytes(header.version,
                                                      header.big_endian,
                                                      payload_bytes)
    elif header.module_id == common.ModuleID.DIAGLOG:
        payload = diaglog.unpack_payload_from_bytes(header.version,
                                                    header.big_endian,
                                                    payload_bytes)
    elif header.module_id == common.ModuleID.ESTIMATOR:
        payload = estimator.unpack_payload_from_bytes(header.version,
                                                      header.big_endian,
                                                      payload_bytes)
    else:
        raise exception.MessageMalformedError("Unhandled module ID %d" %
                                              header.module_id)

    return (header, payload)

__all__ = ['unpack_msg']
