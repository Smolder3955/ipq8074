#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Web based GUI package for the Whole Home Coverage demo.

The following modules are exported:

    :mod:`server`
        web socket server that integrates with the WHC Model-View-Controller
        framework
"""

import whcmvc.view.config

from view import JSView

whcmvc.view.config.register_view('javascript', JSView)

__all__ = ['server']
