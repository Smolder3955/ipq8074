#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Text-based view for the Whole Home Coverage demo.

The following class is exported:

    :class:`ReadOnlyTextView`
        textual console view that lists the associated stations and
        utilization information but has no controls
"""

import sys
import logging

from whcmvc.view.base import ViewBase

log = logging.getLogger('whcconsoleview')
"""The logger used for model operations, named ``whcconsoleview``."""


class ReadOnlyTextView(ViewBase):

    """View class that displays the information to the console.

    Each time an update occurs, the screen is cleared and the new
    information is printed. Note that this printing is done in the
    model's context.
    """

    def __init__(self, model, controller, do_clear=True, compact_mode=False,
                 *args, **kwargs):
        """Initialize the view."""
        ViewBase.__init__(self, model)

        self._do_clear = do_clear
        self._compact_mode = compact_mode

        self.update_associated_stations(model)

    def update_associated_stations(self, model, ap_id=None):
        """Re-display the information after an associated stations update."""
        self._update_ap_info_display(model, ap_id)

    def update_utilizations(self, model, ap_id=None):
        """Re-display the information after a utilization update."""
        self._update_ap_info_display(model, ap_id)

    def update_throughputs(self, model, ap_id=None):
        """Re-display the information after a throughput update."""
        self._update_ap_info_display(model, ap_id)

    def update_paths(self, model, ap_id):
        """Not implemented as currently no way to visualize."""
        pass

    def _update_ap_info_display(self, model, ap_id):
        """Re-display the information after a model update."""
        self._clear_screen()
        for ap_id in self._ap_ids:
            ap = model.device_db.get_ap(ap_id)
            self._display_ap_info(model.device_db, ap, sys.stdout,
                                  self._compact_mode)

    def _clear_screen(self):
        """Clear the contents of the screen completely.

        This uses ASCII control codes, so it will only work on *nix
        systems.
        """
        if self._do_clear:
            sys.stdout.write("\x1b[2J\x1b[H")

__all__ = ['ReadOnlyTextView']
