#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""wxPython graphical view for the Whole Home Coverage demo.

The following class is exported:

    :class:`GraphicalView`
        wxPython-based graphical view that lists the associated stations
        and utilization information along with allowing control of the
        demo algorithm
"""

import sys
import logging
import cStringIO
import copy

import wx

from whcmvc.view.base import ViewBase

from whcmvc.model.model_core import ModelBase, AP_TYPE, TRAFFIC_CLASS_TYPE

log = logging.getLogger('whcgraphicalview')
"""The logger used for view operations, named ``whcgraphicalview``."""

#######################################################################
# GUI components (panels, windows, apps)
#######################################################################

class ConfigPanel(wx.Panel):

    """Panel for configuration of the band steering."""

    def __init__(self, window, view, *args, **kwargs):
        """Initialize the panel with the appropriate controls.

        The state of the controls will be determined by the
        configuration parameters.
        """

        wx.Panel.__init__(self, window, *args, **kwargs)

        self._view = view

        self._disable_controls = False
        if GraphicalApp.DISABLE_CONTROLS_KEYWORD in view.params:
            self._disable_controls = \
                    view.params[GraphicalApp.DISABLE_CONTROLS_KEYWORD]

        top_sizer = wx.BoxSizer(wx.HORIZONTAL)

        box = wx.StaticBox(self, label='Current Band Steering Status')
        font = box.GetFont()
        font.SetWeight(wx.BOLD)
        box.SetFont(font)

        current_pane = wx.StaticBoxSizer(box, wx.VERTICAL)
        self._current_bsteering_status = wx.StaticText(self, label='')
        current_pane.Add(self._current_bsteering_status)

        top_sizer.Add(current_pane, proportion=1, flag=wx.EXPAND)

        box = wx.StaticBox(self, label='Enable/Disable Band Steering feature')
        font = box.GetFont()
        font.SetWeight(wx.BOLD)
        box.SetFont(font)

        settings_pane = wx.StaticBoxSizer(box, wx.VERTICAL)

        button_sizer = wx.BoxSizer(wx.HORIZONTAL)

        self._enable_button = wx.Button(self, label='Enable')
        button_sizer.Add(self._enable_button)

        self._disable_button = wx.Button(self, label='Disable')
        button_sizer.Add(self._disable_button)

        settings_pane.Add(button_sizer)

        self._enable_button.Bind(wx.EVT_BUTTON, self._handle_enable)
        self._disable_button.Bind(wx.EVT_BUTTON, self._handle_disable)

        top_sizer.Add(settings_pane, proportion=1, flag=wx.EXPAND)

        self.SetSizerAndFit(top_sizer)

    def update_gui_bs_enable(self, enable, disable_button=False):
        """Update GUI buttons and status label based on bsteering status.

        Args:
            enable (bool): whether band steering is enabled or not
            disable_button (bool): whether enable/disable button should all be disabled
        """
        if enable:
            self._current_bsteering_status.SetLabel("ON")
            self._enable_button.Disable()

            if self._disable_controls or disable_button:
                self._disable_button.Disable()
            else:
                self._disable_button.Enable()
        else:
            self._current_bsteering_status.SetLabel("OFF")
            self._disable_button.Disable()

            if self._disable_controls or disable_button:
                self._enable_button.Disable()
            else:
                self._enable_button.Enable()

    def _handle_enable(self, event):
        """Enable band steering feature"""
        self._view.controller.enable_bsteering(True)
        self.update_gui_bs_enable(True)

    def _handle_disable(self, event):
        """Disable band steering feature"""
        self._view.controller.enable_bsteering(False)
        self.update_gui_bs_enable(False)


class APInfoPanel(wx.Panel):

    """Panel for showing the associated stations on each AP.

    This will eventually show the utilization information as well.
    """

    def __init__(self, window, view, device_db, *args, **kwargs):
        """Initialize the panel with the appropriate data."""
        wx.Panel.__init__(self, window, *args, **kwargs)

        self._view = view

        self._compact_mode = False
        if GraphicalApp.COMPACT_MODE_KEYWORD in view.params:
            self._compact_mode = view.params[GraphicalApp.COMPACT_MODE_KEYWORD]

        box = wx.StaticBox(self, label='AP Information')
        font = box.GetFont()
        font.SetWeight(wx.BOLD)
        box.SetFont(font)

        # Create a static text element for each AP and lay them out
        # horizontally.
        box_sizer = wx.StaticBoxSizer(box, wx.VERTICAL)
        self._ap_info_labels = []
        for ap_id in view.ap_ids:
            text = wx.StaticText(self, label='')

            # Use a smaller font if requested so as to fit better in the
            # available screen real estate.
            fontSize = 10 if self._compact_mode else 12
            font = wx.Font(fontSize, wx.TELETYPE, wx.NORMAL, wx.NORMAL)
            text.SetFont(font)

            self._ap_info_labels.append(text)
            box_sizer.Add(self._ap_info_labels[-1])

        self.update_from_device_db(device_db)

        top_sizer = wx.BoxSizer(wx.HORIZONTAL)
        top_sizer.Add(box_sizer)

        self.SetSizerAndFit(top_sizer)

    def update_from_device_db(self, device_db):
        """Update the label that shows the association information.

        :param :class:`DeviceDB` device_db: snapshot of the current
            state of the system
        """
        # Update each AP's StaticText control with the new data.
        i = 0
        for ap_id in self._view.ap_ids:
            ap = device_db.get_ap(ap_id)

            output = cStringIO.StringIO()
            self._view._display_ap_info(device_db, ap, output,
                                        self._compact_mode)
            self._ap_info_labels[i].SetLabel(output.getvalue())
            output.close()

            i += 1


class MainWindow(wx.Frame):

    """Main window for controlling the demo app."""

    def __init__(self, model, view, *args, **kwargs):
        """Create and display the main window."""
        wx.Frame.__init__(self, *args, **kwargs)

        self._model = model

        top_sizer = wx.FlexGridSizer(rows=2, cols=1)
        self._config_panel = ConfigPanel(self, view, -1)
        top_sizer.Add(self._config_panel, flag=wx.EXPAND)

        self._ap_info_panel = APInfoPanel(self, view, model.device_db, -1)
        top_sizer.Add(self._ap_info_panel)

        # Set up a handler to close the entire app when the config window is
        # closed.
        self.Bind(wx.EVT_CLOSE, self._window_closed)

        # Register for future updates to the AP info
        self.Connect(wx.ID_ANY, wx.ID_ANY, EVT_DEVICE_DB_UPDATED,
                     self._update_from_device_db)

        self.SetSizerAndFit(top_sizer)
        self.Show(True)

    def _update_from_device_db(self, event):
        """Event handler when the device database is updated."""
        self._ap_info_panel.update_from_device_db(event.device_db)

    def _window_closed(self, event):
        """React to the window closed event by shutting down the model."""
        self.Destroy()
        self._model.shutdown(blocking=False)

    def update_bsteering_status_panel(self, lbd_enable, hyd_enable):
        """Update config panel based on band steering status"""
        # TODO: If HYD is enabled, disable control button since it cannot control
        #       HYD for now. We may have a three mode selector: Disable/LBD/HYD
        #       when controlling HYD is fully supported
        self._config_panel.update_gui_bs_enable(lbd_enable or hyd_enable, hyd_enable)

class GraphicalApp(wx.App):

    """wxPython app class that constructs and runs the GUI."""

    COMPACT_MODE_KEYWORD = 'compact_mode'

    DISABLE_CONTROLS_KEYWORD = 'disable_controls'

    def __init__(self, model, view):
        """Initialize the wxPython application."""
        # This needs to come before constructing the base class so that
        # it is set up for the OnInit() function.
        self._model = model
        self._view = view
        wx.App.__init__(self)

    def OnInit(self):
        self._main_window = MainWindow(self._model, self._view,
                None, title='Whole Home Coverage - Steering Demo')

        return True

    @property
    def main_window(self): return self._main_window

#######################################################################
# Events generated from model context into GUI context
#######################################################################

EVT_DEVICE_DB_UPDATED = wx.NewId()

class DeviceDBUpdatedEvent(wx.PyEvent):

    """Event indicating that the model's device database was updated."""

    def __init__(self, device_db):
        """Initialize the event with a snapshot of the device DB.

        :param :class:`DeviceDB` device_db: a snapshot of the current
            device database
        """
        wx.PyEvent.__init__(self)
        self.SetEventType(EVT_DEVICE_DB_UPDATED)

        self._device_db = device_db

    @property
    def device_db(self): return self._device_db

#######################################################################
# View class and related infrastructure that operate in both model and
# GUI context
#######################################################################

class GraphicalView(ViewBase):

    """View class that displays the information to the console.

    Each time an update occurs, the screen is cleared and the new
    information is printed. Note that this printing is done in the
    model's context.
    """

    def __init__(self, model, controller, *args, **kwargs):
        """Initialize the view.

        The mandatory parameters are:

            :param :class:`ModelBase` model: the current complete model
                state
            :param :class:`Controller` controller: the handle to the
                controller of the system

        The optional keyword arguments are as follows:

            :param boolean manual_steering_enabled: whether to include
                the manual steering pane or not
            :param boolean steer_to_vap_enabled: whether to allow manual
                steering to a VAP (if the manual steering pane is
                enabled
        """
        ViewBase.__init__(self, model)

        self._controller = controller
        self._params = kwargs

        self._app = GraphicalApp(model, self)

        # Remember band steering status
        self._lbd_enabled, self._hyd_enabled = self._check_bs_status()

    def activate(self):
        """Activate the view."""
        # Update view accordingly based on band steering status
        self._app.main_window.update_bsteering_status_panel(
            self._lbd_enabled, self._hyd_enabled)
        if self._lbd_enabled or self._hyd_enabled:
            self._controller.enable_bsteering(True, already_enabled=True)

        self._app.MainLoop()

    def shutdown(self):
        """Shutdown the view (nothing to do)."""
        pass

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
        """Re-display all of the AP information after a model update."""
        # A deep copy is performed here to ensure the GUI is accessing a
        # consistent database instead of one that may be in the process of
        # being updated in the model thread context.
        device_db = copy.deepcopy(model.device_db)
        wx.PostEvent(self._app.main_window, DeviceDBUpdatedEvent(device_db))

    def _check_bs_status(self):
        """Check if band steering is enabled

        This function makes a synchronous call into controller
        to get band steering status as this call is expected to
        return immediately and there is no need for model to know
        about band steering status.

        Returns:
            True if band steering is enabled; otherwise return False
        """
        status_list = self._controller.check_bsteering()
        # For now only consider single AP case, so check the first status
        return (status_list[0][1], status_list[0][2]) if len(status_list) > 0 else (False, False)

    @property
    def params(self):
        return self._params

    @property
    def controller(self):
        return self._controller

__all__ = ['GraphicalView']
