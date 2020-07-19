#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""wxPython based GUI package for the Whole Home Coverage demo.

The following modules are exported:

    :mod:`console`
        textual console view

    :mod:`gui`
        wxPython view
"""

import whcmvc.view.config

from console import ReadOnlyTextView
from gui import GraphicalView

whcmvc.view.config.register_view('text', ReadOnlyTextView)
whcmvc.view.config.register_view('graphical', GraphicalView)

__all__ = ['console', 'gui']
