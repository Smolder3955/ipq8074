#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""View class for the Javscript-based demo GUI.

The following class is exported:

    :class:`JSView`
        Javascript+HTML+CSS-based graphical view that shows the topology,
        throughput, and utilization information
"""

import logging
import copy
import collections

from whcmvc.view.base import ViewBase
from whcmvc.model.model_core import AP_TYPE

log = logging.getLogger('whcjsview')
"""The logger used for view operations, named ``whcjsview``."""

#######################################################################
# View classes and related infrastructure that operate in both model and
# GUI context
#######################################################################


class JSView(ViewBase):

    """View class that processes data correctly for the javascript GUI

    Each time an update occurs, the appropriate data is sent back
    through the WebSocket
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
        self._ws = []
        self._util_seq = collections.defaultdict(lambda: {})
        self._aggr_util_seq = collections.defaultdict(lambda: None)

        self._rssi_seq = collections.defaultdict(lambda: 0)
        self._data_seq = collections.defaultdict(lambda: 0)

        self._overload_state = collections.defaultdict(
            lambda: collections.defaultdict(lambda: False))

        # Dictionary mapping from AP ID to the paths for that AP. The paths
        # are represented as a dictionary mapping from client ID to VAP name.
        self._path_state = collections.defaultdict(
            lambda: collections.defaultdict(lambda: None))

        # Remember band steering status
        self._bs_enabled = self._check_bs_status()

    def _resolve_metrics(self, force_full_dump=False):
        """Creates two dictionaries containing the per client metrics.

        Args:
            force_full_dump (bool): whether the update should be incremental
                or a full dump

        Returns:
            tuple where the first element is the mapping from client device
            name to RSSI and the second is the mapping from client device name
            to throughput (in Mbps)
        """
        rssi_info = {}
        tput_info = {}
        for vap in self._controller.model.device_db.get_vaps():
            for mac_addr, metrics in vap.get_associated_stations():
                client = self._controller.model.device_db.get_client_device(mac_addr)
                if client:
                    if force_full_dump or \
                            (metrics.rssi is not None and
                             metrics.rssi_seq != self._rssi_seq[client.name]):
                        rssi_info[client.name] = (metrics.rssi, metrics.rssi_level)
                        self._rssi_seq[client.name] = metrics.rssi_seq

                    if force_full_dump or \
                            (metrics.tput is not None and
                             metrics.data_seq != self._data_seq[client.name]):
                        tput_info[client.name] = metrics.tput
                        self._data_seq[client.name] = metrics.data_seq

        return (rssi_info, tput_info)

    def add_websocket(self, ws):
        """Add a listening websocket."""
        self._ws.append(ws)

        # Send out initial snapshot of the system (what devices are present).
        client_devs = self._controller.model.device_db.get_client_devices()

        # Current stats and path information
        rssi_info, tput_info = self._resolve_metrics(force_full_dump=True)
        self._send_client_msgs(ws, client_devs, rssi_info, tput_info)

        path_info = {}
        for ap in self._controller.model.device_db.get_aps():
            self._add_to_path_update(self._controller.model, ap, path_info,
                                     force_full_dump=True)

        if path_info:
            ws.sendPathUpdate(path_info)

    def rem_websocket(self, retired_ws):
        for ws in self._ws:
            if ws is retired_ws:
                self._ws.remove(ws)
                return

    def activate(self):
        """Activate the view."""
        if self._bs_enabled:
            self._controller.enable_bsteering(True, already_enabled=True)

    def shutdown(self):
        """Shutdown the view."""
        for ws in self._ws:
            self._ws.remove(ws)
            ws.shutdown()

    def _send_client_msgs(self, ws, client_devs, rssi_info, tput_info):
        """Send all messages that are generated on a per-client basis.

        This includes association information, throughput information (if
        updated), steering status, and the various flags that track the
        internal state of the algorithms on a per-client basis.

        Args:
            ws (:class:`SteeringServerProtocol`): the instance on which to
                send the message
            client_devs (list of :class:`ClientDevice`s): the STA devices in
                the system
            force_full_dump (bool): whether the update should be incremental
                or a full dump
        """
        ws.sendAssociationUpdate(client_devs, rssi_info)

        if tput_info:
            ws.sendTputUpdate(tput_info)

        ws.sendSteeringStatus(client_devs)
        ws.sendFlagUpdate(client_devs)

    def update_associated_stations(self, model, ap_id=None):
        """Re-display the information after an associated stations update."""
        if len(self._ws) == 0:
            return

        blackout_result = {}
        for ap in model.device_db.get_aps():
            blackout_result[ap.ap_id] = ap.steering_blackout

        client_devs = self._controller.model.device_db.get_client_devices()
        rssi_info, tput_info = self._resolve_metrics()
        for ws in self._ws:
            self._send_client_msgs(ws, client_devs, rssi_info, tput_info)
            ws.sendBlackoutUpdate(blackout_result)

        # If the association information changed, we also need to update the
        # paths (as an AP that is now serivng a device should not show any
        # paths for it).
        if ap_id:
            self.update_paths(model, ap_id)

    def update_utilizations(self, model, ap_id=None):
        """Re-display the information after a utilization update."""
        if len(self._ws) == 0:
            return
        device_db = copy.deepcopy(model.device_db)
        util_result = collections.defaultdict(lambda: collections.defaultdict(lambda: {}))

        # To prevent unnecessary updates, record when something has changed.
        util_updated = False
        overload_updated = False

        cap = None
        for ap in device_db.get_aps():
            # As we are going through the list, grab the CAP so that we can
            # later populate the aggregate utilization.
            if ap.ap_type == AP_TYPE.CAP:
                cap = ap

            for vap in ap.get_vaps():
                if vap.vap_id not in self._util_seq[ap.ap_id]:
                    self._util_seq[ap.ap_id][vap.vap_id] = {'raw': -1, 'avg': -1}

                raw_val = vap.utilization_raw.value
                raw_seq = vap.utilization_raw.seq
                avg_val = vap.utilization_avg.value
                avg_seq = vap.utilization_avg.seq

                if raw_val is None and avg_val is None:
                    continue

                if raw_seq != self._util_seq[ap.ap_id][vap.vap_id]['raw']:
                    # A new raw utilization value is available
                    if raw_val is not None:
                        util_result[ap.ap_id][vap.vap_id]['raw'] = raw_val
                        util_updated = True
                if avg_seq != self._util_seq[ap.ap_id][vap.vap_id]['avg']:
                    # A new average utilization value is available
                    if avg_val is not None:
                        util_result[ap.ap_id][vap.vap_id]['avg'] = avg_val
                        util_updated = True

                self._util_seq[ap.ap_id][vap.vap_id]['raw'] = raw_seq
                self._util_seq[ap.ap_id][vap.vap_id]['avg'] = avg_seq

                if self._overload_state[ap.ap_id][vap.vap_id] != vap.overloaded:
                    self._overload_state[ap.ap_id][vap.vap_id] = vap.overloaded
                    overload_updated = True

        if cap:
            for vap in cap.get_vaps():
                aggr_val = vap.utilization_aggr.value
                aggr_seq = vap.utilization_aggr.seq

                if aggr_val is None:
                    continue

                if aggr_seq != self._aggr_util_seq[vap.vap_id]:
                    util_result[vap.channel_group.name] = aggr_val
                    self._aggr_util_seq[vap.vap_id] = aggr_seq
                    util_updated = True

        for ws in self._ws:
            if util_updated:
                ws.sendUtilUpdate(util_result)

            if overload_updated:
                ws.sendOverloadUpdate(self._overload_state)

    def update_throughputs(self, model, ap_id=None):
        """Re-display the information after a throughput update."""
        pass

    def _add_to_path_update(self, model, ap, path_info, force_full_dump=False):
        """Send the complete path information for a given AP.

        Args:
            model (:class:`ModelBase`): the instance that contains the updated
                information
            ap (:class:`AccessPoint`): the AP on which paths were updated
            path_info (dict): the dictionary into which to place the path
                info
            force_full_dump (bool): whether the update should be incremental
                or a full dump"""
        paths = ap.get_paths()

        # Since there may not always be an H-Active entry for an associated
        # STA (and thus it may not have a path), loop over the client devices
        # to see whether the path needs to be updated or needs to be deleted.
        for client in model.device_db.get_client_devices():
            if client.mac_addr in paths:
                # Currently has a path; see if it has changed
                path = paths[client.mac_addr]

                # We are only interested in clients that we are tracking and that
                # are currently associated
                if client.serving_vap is not None:
                    if client.serving_vap.ap.ap_id != ap.ap_id:
                        # Client is on a different AP. See if the path has changed.
                        vap = getattr(path.active_intf, 'vap_id', None)
                        if self._path_state[ap.ap_id][client.name] != vap or \
                                force_full_dump:
                            path_info[client.name] = vap
                            self._path_state[ap.ap_id][client.name] = vap
                    else:   # this AP is serving, so remove its path
                        if self._path_state[ap.ap_id][client.name] is not None:
                            path_info[client.name] = None
                            self._path_state[ap.ap_id][client.name] = None
            else:
                # No path exists for the client; check if that is a change
                if self._path_state[ap.ap_id][client.name] is not None:
                    path_info[client.name] = None
                    self._path_state[ap.ap_id][client.name] = None

    def update_paths(self, model, ap_id):
        """React to updated path information in the model."""
        if len(self._ws) == 0:
            return

        path_info = {}

        # For now we only report the paths from the CAP.
        ap = model.device_db.get_ap(ap_id)
        if ap and ap.ap_type == AP_TYPE.CAP:
            self._add_to_path_update(model, ap, path_info)

        if path_info:
            for ws in self._ws:
                ws.sendPathUpdate(path_info)

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
        return (status_list[0][1] or status_list[0][2]) if len(status_list) > 0 else False

    @property
    def params(self):
        return self._params

    @property
    def controller(self):
        return self._controller


__all__ = ['JSView']
