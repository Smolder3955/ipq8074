#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#
# This file defines constants used accross multiple modules.

from enum import Enum

# Band type constants.
BAND_TYPE = Enum('BAND_TYPE', [('BAND_24G', 0), ('BAND_5G', 1), ('BAND_INVALID', 2)])

# Transport protocols.
TRANSPORT_PROTOCOL = Enum('TRANSPORT_PROTOCOL', [('OTHER', 0), ('UDP', 1)])

# AP ID constants
AP_ID_SELF = 255

# Utilization constants
# Maximum number of seconds allowed between two utilization updates if they
# can be aggregated
UTIL_AGGR_SECS = 2

# Exports
__all__ = ['BAND_TYPE']
