#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# Exceptions for message parsing logic.
#


class MessageMalformedError(Exception):
    """Generic exception indicating that a diaglog message was malformed.

    This is used to indicate messages that cannot be parsed or that have
    fields with unexpected values.
    """

    def __init__(self, msg):
        """Create a new exception with the associated message."""
        Exception.__init__(self, msg)

__all__ = ['MessageMalformedError']
