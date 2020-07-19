#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Message definitions for the steering executor module.
#

"""Module providing the message unpacking for the steerexec module.

The following types are exported:

    :obj:`MessageID`: identifier for the message

The following classes are exported:

    :class:`SteerToBand`: steering of a specific STA to a band started
    :class:`AbortSteerToBand`: an attempt to steer a specific STA to a
        band was aborted
    :class:`SteeringUnfriendly`: whether a STA is marked as steering
        unfriendly has changed
    :class:`SteeringProhibited`: whether a STA is currently prohibited
        from steering has changed
    :class:`BTMCompliance`: whether the BTM compliance state of a STA
        has changed

The following functions are exported:

    :func:`unpack_payload_from_bytes`: unpack the message payload from a
        buffer
"""

import struct
import collections

from enum import Enum

from whcdiag.msgs import common
from exception import MessageMalformedError

#: Supported message identifiers for steerexec
MessageID = Enum('MessageID', [('STEER_TO_BAND', 0),  # V1
                               ('PRE_ASSOC_STEER', 0),  # V2
                               ('ABORT_STEER_TO_BAND', 1),  # V1
                               ('STEER_END', 1),  # V2
                               ('STEERING_UNFRIENDLY', 2),
                               ('STEERING_PROHIBITED', 3),
                               ('BTM_COMPLIANCE', 4),  # V2
                               ('POST_ASSOC_STEER', 5)])  # V2


# Steering prohibited constants.
SteeringProhibitType = Enum('SteeringProhibitType', [('PROHIBIT_NONE', 0),
                                                     ('PROHIBIT_SHORT', 1),
                                                     ('PROHIBIT_LONG', 2),
                                                     ('PROHIBIT_REMOTE', 3),
                                                     ('PROHIBIT_INVALID', 4)])

# BTM compliance constants.
BTMComplianceType = Enum('BTMComplianceType', [('BTM_COMPLIANCE_IDLE', 0),
                                               ('BTM_COMPLIANCE_ACTIVE_UNFRIENDLY', 1),
                                               ('BTM_COMPLIANCE_ACTIVE', 2),
                                               ('BTM_COMPLIANCE_INVALID', 3)])

# Steer type constants.
SteerType = Enum('SteerType', [('STEER_TYPE_NONE', 0),
                               ('STEER_TYPE_LEGACY', 1),
                               ('STEER_TYPE_BTM_AND_BLACKLIST', 2),
                               ('STEER_TYPE_BTM', 3),
                               ('STEER_TYPE_BTM_AND_BLACKLIST_ACTIVE', 4),
                               ('STEER_TYPE_BTM_ACTIVE', 5),
                               ('STEER_TYPE_PREASSOCIATION', 6),
                               ('STEER_TYPE_BTM_BE', 7),
                               ('STEER_TYPE_BTM_BE_ACTIVE', 8),
                               ('STEER_TYPE_BTM_BLACKLIST_BE', 9),
                               ('STEER_TYPE_BTM_BLACKLIST_BE_ACTIVE', 10),
                               ('STEER_TYPE_LEGACY_BE', 11),
                               ('STEER_TYPE_INVALID', 12)])

# Steer end status constants.
SteerEndStatusType = Enum('SteerEndStatusType',
                          [('STATUS_SUCCESS', 0),
                           ('STATUS_ABORT_AUTH_REJECT', 1),
                           ('STATUS_ABORT_LOW_RSSI', 2),
                           ('STATUS_ABORT_CHANGE_TARGET', 3),
                           ('STATUS_ABORT_USER', 4),
                           ('STATUS_BTM_REJECT', 5),
                           ('STATUS_BTM_RESPONSE_TIMEOUT', 6),
                           ('STATUS_ASSOC_TIMEOUT', 7),
                           ('STATUS_CHANNEL_CHANGE', 8),
                           ('STATUS_PREPARE_FAIL', 9),
                           ('STATUS_UNEXPECTED_BSS', 10),
                           ('STATUS_INVALID', 11)])

# Steer reason constants.
SteerReasonType = Enum('SteerReasonType',
                       [('REASON_USER', 0),
                        ('REASON_ACTIVE_UPGRADE', 1),
                        ('REASON_ACTIVE_DOWNGRADE_RATE', 2),
                        ('REASON_ACTIVE_DOWNGRADE_RSSI', 3),
                        ('REASON_IDLE_UPGRADE', 4),
                        ('REASON_IDLE_DOWNGRADE', 5),
                        ('REASON_ACTIVE_OFFLOAD', 6),
                        ('REASON_IDLE_OFFLOAD', 7),
                        ('REASON_ACTIVE_AP_STEERING', 8),
                        ('REASON_IDLE_AP_STEERING', 9),
                        ('REASON_AP_REQUEST', 10),
                        ('REASON_INTERFERENCE_AVOIDANCE', 11),
                        ('REASON_INVALID', 12)])

_SteerToBand = struct.Struct('6sBB')
SteerToBand = collections.namedtuple('SteerToBand',
                                     ['sta_addr', 'assoc_band',
                                      'target_band'])

_PreAssocSteer = struct.Struct('6sBBB')
PreAssocSteer = collections.namedtuple('PreAssocSteer',
                                       ['sta_addr', 'transaction',
                                        'channel_count', 'channels'])

_PostAssocSteer = struct.Struct('6sBBB')
PostAssocSteer = collections.namedtuple('PostAssocSteer',
                                        ['sta_addr', 'transaction', 'steer_type',
                                         'reason', 'assoc_bss',
                                         'candidate_count', 'candidate_bsses'])

_AbortSteerToBand = struct.Struct('6sB')
AbortSteerToBand = collections.namedtuple('AbortSteerToBand',
                                          ['sta_addr', 'disabled_band'])

_SteerEnd = struct.Struct('6sBBB')
SteerEnd = collections.namedtuple('SteerEnd',
                                  ['sta_addr', 'transaction',
                                   'steer_type', 'status'])

_SteeringUnfriendly = struct.Struct('6sB')
SteeringUnfriendly = collections.namedtuple('SteeringUnfriendly',
                                            ['sta_addr', 'is_unfriendly'])

_SteeringUnfriendlyLE_v2 = struct.Struct('<6sBL')  # little endian
_SteeringUnfriendlyBE_v2 = struct.Struct('>6sBL')  # big endian
SteeringUnfriendly_v2 = collections.namedtuple('SteeringUnfriendly',
                                               ['sta_addr', 'is_unfriendly',
                                                'consecutive_failures'])

_SteeringProhibited = struct.Struct('6sB')
SteeringProhibited = collections.namedtuple('SteeringProhibited',
                                            ['sta_addr', 'is_prohibited'])

_SteeringProhibited_v2 = struct.Struct('6sB')
SteeringProhibited_v2 = collections.namedtuple('SteeringProhibited',
                                               ['sta_addr', 'prohibit_type'])

_BTMComplianceLE = struct.Struct('<6sBBLL')  # little endian
_BTMComplianceBE = struct.Struct('>6sBBLL')  # big endian
BTMCompliance = collections.namedtuple('BTMCompliance',
                                       ['sta_addr', 'is_unfriendly',
                                        'compliance_state', 'consecutive_failures',
                                        'consecutive_failures_active'])


def check_steering_prohibit_type(msg, attr_name='prohibit_type'):
    """Check whether the steering prohibit type is valid.

    Args:
        msg (:class:`collections.namedtuple`): named tuple to check if
            the prohibit_type field is valid (if one even exists); the value of
            the field should just be the raw integer value for the prohibit type
        attr_name (str): the name of the attribute to check for

    Returns:
        the message with the steering prohibit attribute converted to the SteeringProhibitType enum
        (if it existed)

    Raises:
        :class:`MessageMalformedError`: Prohibit type is invalid
    """
    if hasattr(msg, attr_name):
        prohibit_type = getattr(msg, attr_name)

        # Sanity check the prohibit_type and generate an error if it is not one
        # of the values expected.
        try:
            SteeringProhibitType(prohibit_type)
        except ValueError:
            raise MessageMalformedError("Invalid prohibit_type %d" % prohibit_type)

        vals = {attr_name: SteeringProhibitType(getattr(msg, attr_name))}
        return msg._replace(**vals)
    else:
        return msg


def check_btm_compliance_type(msg, attr_name='compliance_state'):
    """Check whether the BTM compliance type is valid.

    Args:
        msg (:class:`collections.namedtuple`): named tuple to check if
            the compliance_state field is valid (if one even exists); the value of
            the field should just be the raw integer value for the BTM compliance type
        attr_name (str): the name of the attribute to check for

    Returns:
        the message with the BTM compliance attribute converted to the BTMComplianceType enum
        (if it existed)

    Raises:
        :class:`MessageMalformedError`: Prohibit type is invalid
    """
    if hasattr(msg, attr_name):
        btm_compliance_type = getattr(msg, attr_name)

        # Sanity check the btm_compliance_type and generate an error if it is not one
        # of the values expected.
        try:
            BTMComplianceType(btm_compliance_type)
        except ValueError:
            raise MessageMalformedError("Invalid btm_compliance_type %d" % btm_compliance_type)

        vals = {attr_name: BTMComplianceType(getattr(msg, attr_name))}
        return msg._replace(**vals)
    else:
        return msg


def check_steer_type(msg, attr_name='steer_type'):
    """Check whether the steer type is valid.

    Args:
        msg (:class:`collections.namedtuple`): named tuple to check if
            the steer_type field is valid (if one even exists); the value of
            the field should just be the raw integer value for the steer type
        attr_name (str): the name of the attribute to check for

    Returns:
        the message with the steer type attribute converted to the SteerType enum
        (if it existed)

    Raises:
        :class:`MessageMalformedError`: Steer type is invalid
    """
    if hasattr(msg, attr_name):
        steer_type = getattr(msg, attr_name)

        # Sanity check the steer_type and generate an error if it is not one
        # of the values expected.
        try:
            SteerType(steer_type)
        except ValueError:
            raise MessageMalformedError("Invalid steer_type %d" % steer_type)

        vals = {attr_name: SteerType(getattr(msg, attr_name))}
        return msg._replace(**vals)
    else:
        return msg


def check_status_type(msg, attr_name='status'):
    """Check whether the status type is valid.

    Args:
        msg (:class:`collections.namedtuple`): named tuple to check if
            the status field is valid (if one even exists); the value of
            the field should just be the raw integer value for the status
        attr_name (str): the name of the attribute to check for

    Returns:
        the message with the status attribute converted to the SteerEndStatusType enum
        (if it existed)

    Raises:
        :class:`MessageMalformedError`: Steer type is invalid
    """
    if hasattr(msg, attr_name):
        status = getattr(msg, attr_name)

        # Sanity check the status and generate an error if it is not one
        # of the values expected.
        try:
            SteerEndStatusType(status)
        except ValueError:
            raise MessageMalformedError("Invalid status %d" % status)

        vals = {attr_name: SteerEndStatusType(getattr(msg, attr_name))}
        return msg._replace(**vals)
    else:
        return msg


def check_reason_type(msg, attr_name='reason'):
    """Check whether the reason type is valid.

    Args:
        msg (:class:`collections.namedtuple`): named tuple to check if
            the reason field is valid (if one even exists); the value of
            the field should just be the raw integer value for the reason
        attr_name (str): the name of the attribute to check for

    Returns:
        the message with the reason attribute converted to the SteerReasonType enum
        (if it existed)

    Raises:
        :class:`MessageMalformedError`: Steer type is invalid
    """
    if hasattr(msg, attr_name):
        reason = getattr(msg, attr_name)

        # Sanity check the status and generate an error if it is not one
        # of the values expected.
        try:
            SteerReasonType(reason)
        except ValueError:
            raise MessageMalformedError("Invalid reason %d" % reason)

        vals = {attr_name: SteerReasonType(getattr(msg, attr_name))}
        return msg._replace(**vals)
    else:
        return msg


def get_pre_assoc_channel_list(payload, buf, unpacker):
    # Check if the length matches the number of channels
    channel_count = getattr(payload, 'channel_count')
    if channel_count > 2 or channel_count < 1:
        raise MessageMalformedError("Pre-association steer message has invalid channel count: %d" %
                                    channel_count)
    if (len(buf) != unpacker.size + channel_count):
        raise MessageMalformedError("Pre-association steer message has invalid length: %d" %
                                    len(buf))

    # Replace the channel number with an array containing both channels
    channel_array = [getattr(payload, 'channels')]
    if channel_count == 2:
        temp = struct.unpack('B', buf[unpacker.size+1])
        channel_array.append(temp[0])

    vals = {'channels': channel_array}
    return payload._replace(**vals)


def get_post_assoc_candidates(payload, buf, unpacker):
    # Get the associated BSS
    (payload, buf) = common.get_bss_info(buf[unpacker.size+1:], payload, 'assoc_bss', 1)

    temp = struct.unpack('B', buf[:1])
    candidate_count = temp[0]
    vals = {'candidate_count': candidate_count}
    payload = payload._replace(**vals)

    if candidate_count > 2 or candidate_count < 1:
        raise MessageMalformedError("Post-association steer message has " +
                                    "invalid candidate count: %d" % candidate_count)
    if (len(buf) != 1 + candidate_count * common._BSSInfo.size):
        raise MessageMalformedError("Post-association steer message has invalid length: %d %d" %
                                    (len(buf), candidate_count))

    # Get the candidates
    (payload, buf) = common.get_bss_info(buf[1:], payload, 'candidate_bsses',
                                         candidate_count, force_array=True)

    return payload


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

    band_attrs = None
    check_steer = False
    check_status = False
    if version == common.Version.VERSION1:
        if msg_id == MessageID.STEER_TO_BAND.value:
            unpacker = _SteerToBand
            constructor = SteerToBand
            band_attrs = [('assoc_band', True), ('target_band', False)]
        elif msg_id == MessageID.ABORT_STEER_TO_BAND.value:
            unpacker = _AbortSteerToBand
            constructor = AbortSteerToBand
            band_attrs = [('disabled_band', False)]
        elif msg_id == MessageID.STEERING_UNFRIENDLY.value:
            unpacker = _SteeringUnfriendly
            constructor = SteeringUnfriendly
        elif msg_id == MessageID.STEERING_PROHIBITED.value:
            unpacker = _SteeringProhibited
            constructor = SteeringProhibited
        else:
            raise MessageMalformedError("Unsupported message ID: %d" % msg_id)
    elif version == common.Version.VERSION2:
        if msg_id == MessageID.PRE_ASSOC_STEER.value:
            unpacker = _PreAssocSteer
            constructor = PreAssocSteer
        elif msg_id == MessageID.STEER_END.value:
            unpacker = _SteerEnd
            constructor = SteerEnd
            check_steer = True
            check_status = True
        elif msg_id == MessageID.STEERING_UNFRIENDLY.value:
            unpacker = _SteeringUnfriendlyBE_v2 if big_endian else _SteeringUnfriendlyLE_v2
            constructor = SteeringUnfriendly_v2
        elif msg_id == MessageID.STEERING_PROHIBITED.value:
            unpacker = _SteeringProhibited_v2
            constructor = SteeringProhibited_v2
        elif msg_id == MessageID.BTM_COMPLIANCE.value:
            unpacker = _BTMComplianceBE if big_endian else _BTMComplianceLE
            constructor = BTMCompliance
        elif msg_id == MessageID.POST_ASSOC_STEER.value:
            unpacker = _PostAssocSteer
            constructor = PostAssocSteer
            check_steer = True
        else:
            raise MessageMalformedError("Unsupported message ID: %d" % msg_id)
    else:
        raise MessageMalformedError("Invalid version number: %d" % version)

    if len(buf) < unpacker.size + 1:
        raise MessageMalformedError("Message too short: %d (need %d)" %
                                    (len(buf), unpacker.size + 1))

    if msg_id == MessageID.PRE_ASSOC_STEER.value and version == common.Version.VERSION2:
        fields = unpacker.unpack(buf[1:unpacker.size+1])
        payload = constructor._make([common.ether_ntoa(fields[0])] +
                                    list(fields)[1:])
    elif msg_id == MessageID.POST_ASSOC_STEER.value and version == common.Version.VERSION2:
        # Variable length messages
        # First unpack the fixed size portion
        fields = unpacker.unpack(buf[1:unpacker.size+1])
        fields = fields + (0, 0, 0)
        payload = constructor._make([common.ether_ntoa(fields[0])] +
                                    list(fields)[1:])
    else:
        fields = unpacker.unpack(buf[1:])
        payload = constructor._make([common.ether_ntoa(fields[0])] +
                                    list(fields)[1:])

    if (msg_id == MessageID.PRE_ASSOC_STEER.value and version == common.Version.VERSION2):
        payload = get_pre_assoc_channel_list(payload, buf, unpacker)

    elif(msg_id == MessageID.POST_ASSOC_STEER.value and version == common.Version.VERSION2):
        payload = get_post_assoc_candidates(payload, buf, unpacker)

        payload = check_reason_type(payload)

    if band_attrs is not None:
        for attr in band_attrs:
            payload = common.check_band(payload, allow_invalid=attr[1],
                                        attr_name=attr[0])

    if msg_id == MessageID.STEERING_PROHIBITED.value and version == common.Version.VERSION2:
        payload = check_steering_prohibit_type(payload)

    if msg_id == MessageID.BTM_COMPLIANCE.value:
        payload = check_btm_compliance_type(payload)

    if check_steer:
        payload = check_steer_type(payload)

    if check_status:
        payload = check_status_type(payload)

    return payload

__all__ = ['MessageID', 'SteeringProhibitType', 'BTMComplianceType',
           'SteerType', 'SteerEndStatusType',
           'SteerToBand', 'AbortSteerToBand',
           'SteeringUnfriendly', 'SteeringProhibited', 'BTMCompliance']
