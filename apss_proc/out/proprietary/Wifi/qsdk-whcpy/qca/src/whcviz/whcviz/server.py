#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

from autobahn.twisted.websocket import WebSocketServerProtocol, \
    WebSocketServerFactory
from autobahn.websocket import protocol
import json
import collections
import enum


class SteeringServerWebSocketFactory(WebSocketServerFactory):

    @staticmethod
    def getNodes(model):
        """Extract information about the nodes in the system from the mode.

        Args:
            model (:class:`ModelBase`): the state of the system (at startup)

        Returns:
            Tuple where each element is a list of dictionaries. The first
            tuple element contains the AP nodes and the second one contains the
            client nodes. Each AP node dictionary has that node's identifier
            and type. Each client node dictionary has that node's name, client
            type, and MAC address. For example:

                ([{'id': 'client1', 'type': CLIENT_TYPE.PHONE,
                   'mac_address': '00:11:22:33:44:55'}],
                 [{'id': 'AP148', 'type': 'CAP'}])
        """
        clientNodes = map(lambda dev: {'id': dev.name, 'type': dev.client_type.name,
                                       'mac_address': dev.mac_addr},
                          model.device_db.get_client_devices())

        apNodes = map(lambda ap: {'id': ap.ap_id, 'type': ap.ap_type.name},
                      model.device_db.get_aps())
        return (apNodes, clientNodes)

    @staticmethod
    def createLookupDict(nodes):
        """From a list of dictionaries, create a dict lookup by MAC address.

        Args:
            nodes (list of dictionaries): a list of client node information
                (as returned by getNodes)

        Returns:
            a dictionary mapping from MAC address to the node information
        """
        return dict((node['mac_address'], node) for node in nodes)

    @staticmethod
    def createAPInfoDict(model):
        result = collections.defaultdict(lambda: {})
        for ap in model.device_db.get_aps():
            result[ap.ap_id]['type'] = ap.ap_type.name
            result[ap.ap_id]['vaps'] = {}
            for vap in ap.get_vaps():
                result[ap.ap_id]['vaps'][vap.vap_id] = \
                    {'channel_group': vap.channel_group.name,
                     'channel': vap.channel,
                     'overload': vap.overload_threshold}
        return result

    @staticmethod
    def createClientInfoDict(model):
        result = collections.defaultdict(lambda: {})
        for client in model.device_db.get_client_devices():
            result[client.name]['type'] = client.client_type.name
            result[client.name]['mac_address'] = client.mac_addr
            result[client.name]['traffic_class'] = client.traffic_class.name

        return result

    def setView(self, view):
        self.view = view

    def setModel(self, model, log):
        (self.aps, clients) = self.getNodes(model)
        self.ap_info_dict = self.createAPInfoDict(model)
        self.client_info_dict = self.createClientInfoDict(model)
        self.log = log

    def buildProtocol(self, addr):
        p = self.protocol()
        p.factory = self
        p.setView(self.view)
        p.setConfig(self.ap_info_dict, self.aps, self.client_info_dict,
                    self.log)
        return p


class SteeringServerProtocol(WebSocketServerProtocol):

    from enum import Enum

    MSG_TYPE = Enum("MSG_TYPE", [("FULL_UPDATE", 1), ("TEXT", 2),
                    ("TPUT_UPDATE", 3), ("ASSOCIATION_UPDATE", 4),
                    ("STEERING_UPDATE", 5), ("UTIL_UPDATE", 6),
                    ("BLACKOUT_UPDATE", 7), ("OVERLOAD_THRESH", 8),
                    ("OVERLOAD_UPDATE", 9), ("STEERING_ATTEMPT", 10),
                    ("AP_INFO", 11), ("FLAG_UPDATE", 12),
                    ("PATH_UPDATE", 13), ("CLIENT_INFO", 14)])

    def setView(self, view):
        self.view = view

    def setConfig(self, ap_info_dict, aps, client_info_dict, log):
        """Initialize this instance with the start state.

        Args:
            ap_info_dict (dict): static configuration info about each AP,
                indexed by the AP identifier
            client_info_dict (dict): static configuration info about each
                client device, indexed by the client name
            log (:class:`logging.Logger`): the handle to use for all logging
        """
        self.ap_info_dict = ap_info_dict
        self.client_info_dict = client_info_dict
        self.client_flags_dict = dict(map(lambda x: (x, {}), client_info_dict.keys()))
        self._last_serving_vap = collections.defaultdict(lambda: None)
        self.aps = aps
        self.log = log

    def onConnect(self, request):
        self.clients = []
        self.links = []
        self.tputs = {}
        self.steering_pending = {}
        self.blackout_statuses = {}
        self.band_format = {"2.4g": "2.4", "5g":  "5.1",
                            "5g1":  "5.1", "5g2": "5.2"}
        self.log.info("Client connecting: {0}".format(request.peer))

    def onOpen(self):
        self.log.info("WebSocket connection open.")
        self.clients = []
        self.sendAPInfo(self.ap_info_dict)
        self.sendClientInfo(self.client_info_dict)
        self.view.add_websocket(self)

    def onMessage(self, payload, isBinary):
        if isBinary:
            self.log.info("Binary message received: {0} bytes".format(len(payload)))
        else:
            self.log.info("Text message received: {0}".format(payload.decode('utf8')))

        # echo back message verbatim
        # self.sendMessage(payload, isBinary)

    def onClose(self, wasClean, code, reason):
        self.log.info("WebSocket connection closed: {0}".format(reason))
        self.view.rem_websocket(self)

    def shutdown(self):
        self.sendClose(code=protocol.WebSocketProtocol.CLOSE_STATUS_CODE_NORMAL,
                       reason=u"Terminating application")

    def sendNodes(self, nodes):
        toReturn = {'nodes': self.aps+nodes}
        toReturn = SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.FULL_UPDATE, toReturn)
        self.sendMessage(json.dumps(toReturn))

    def sendLinks(self, links):
        toReturn = {'links': links}
        toReturn = SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.FULL_UPDATE, toReturn)
        self.sendMessage(json.dumps(toReturn))

    def sendBoth(self, nodes, links, msg_type=MSG_TYPE.FULL_UPDATE):
        toReturn = {'nodes': self.aps+nodes,
                    'links': links}
        toReturn = SteeringServerProtocol.buildMessage(msg_type, toReturn)
        self.sendMessage(json.dumps(toReturn))

    def sendText(self, msg):
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.TEXT, msg)))

    def sendTputUpdate(self, tputs):
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.TPUT_UPDATE, tputs)))

    def sendUtilUpdate(self, utils):
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.UTIL_UPDATE, utils)))

    def sendAPInfo(self, ap_info):
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.AP_INFO, ap_info)))

    def sendClientInfo(self, client_info):
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.CLIENT_INFO, client_info)))

    def sendPathUpdate(self, path_info):
        """Send a web socket message with updated path information.

        Args:
            path_info (dict): dictionary of path information per AP
                (identified by AP ID), with the values themselves being
                a dictionary mapping from client ID to the VAP on which
                their traffic will be sent by this AP when trying to reach
                that client
        """
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.PATH_UPDATE, path_info)))

    def sendOverloadUpdate(self, overloads):
        self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
            SteeringServerProtocol.MSG_TYPE.OVERLOAD_UPDATE, overloads)))

    def sendBlackoutUpdate(self, blackouts):
        for ap in blackouts:
            if ap in self.blackout_statuses:
                if blackouts[ap] != self.blackout_statuses[ap]:
                    result = {}
                    result[ap] = blackouts[ap]
                    self.blackout_statuses[ap] = blackouts[ap]
                    self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                        SteeringServerProtocol.MSG_TYPE.BLACKOUT_UPDATE, result)))
            else:
                if blackouts[ap] == 1:  # Case for if websocket connects during blackout
                    result = {}
                    result[ap] = blackouts[ap]
                    self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                        SteeringServerProtocol.MSG_TYPE.BLACKOUT_UPDATE, result)))
            self.blackout_statuses[ap] = blackouts[ap]

    def sendFlagUpdate(self, clients):
        updated = {}
        updated['active'] = False
        updated['dual_band'] = False
        updated['btm_compliance'] = False
        updated['prohibit_type'] = False
        updated['unfriendly'] = False
        updated['btm_unfriendly'] = False
        updated['polluted'] = False

        def checkUpdates(flag_name, flag, client_name):
            if flag is None:
                if flag_name not in self.client_flags_dict[client_name]:
                    updated[flag_name] = True
                    self.client_flags_dict[client_name][flag_name] = 0
            else:
                if isinstance(flag, enum.EnumMeta) or isinstance(flag, enum.Enum):
                    flag = flag.name
                if flag_name in self.client_flags_dict[client_name]:
                    if self.client_flags_dict[client_name][flag_name] != flag:
                        updated[flag_name] = True
                        self.client_flags_dict[client_name][flag_name] = flag
                else:
                    updated[flag_name] = True
                    self.client_flags_dict[client_name][flag_name] = flag

        for client in clients:
            if client.name in self.client_flags_dict:
                checkUpdates('active', client.is_active, client.name)
                checkUpdates('dual_band', client.is_dual_band, client.name)
                checkUpdates('btm_compliance', client.btm_compliance, client.name)
                checkUpdates('prohibit_type', client.prohibit_type, client.name)
                checkUpdates('unfriendly', client.is_unfriendly, client.name)
                checkUpdates('btm_unfriendly', client.is_btm_unfriendly, client.name)
                checkUpdates('polluted', client.is_polluted, client.name)

        updated_total = False
        for key in updated:
            if(updated[key]):
                updated_total = True

        if updated_total:
            self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                SteeringServerProtocol.MSG_TYPE.FLAG_UPDATE, {'clients': self.client_flags_dict,
                                                              'flags_updated': updated})))

    def _is_serving_vap_change(self, client):
        """Check if the previously serving VAP recorded is different.

        Args:
            client (:class:`ClientDevice`): the client for which to check
                whether the serving VAP has changed

        Returns:
            True if the serving VAP has changed; otherwise False
        """
        return self._last_serving_vap[client.name] != \
            getattr(client.serving_vap, 'vap_id', None)

    def _record_serving_vap(self, client):
        """Record the current VAP that is serving the client.

        Args:
            client (:class:`ClientDevice`): the client for which to record
                the serving VAP
        """
        self._last_serving_vap[client.name] = \
            getattr(client.serving_vap, 'vap_id', None)

    def sendAssociationUpdate(self, client_devs, rssi_info):
        """Send updated association and RSSI information.

        In addition to this basic information, also include an indication of
        whether the client was steered or not.

        Args:
            clients (list of :class:`ClientDevice`): all of the clients being
                managed
            rssi_info (dict): mapping from client name to a tuple where the
                first element is the numeric RSSI value and the second element
                is an :obj:`RSSI_LEVEL`
        """
        assoc_info = collections.defaultdict(lambda: {})
        updated = False
        for client in client_devs:
            if self._is_serving_vap_change(client) or client.name in rssi_info:
                if client.serving_vap is not None:
                    assoc_info[client.name]['serving_vap'] = client.serving_vap.vap_id

                    # Only provide RSSI information if it has changed.
                    if client.name in rssi_info and rssi_info[client.name][0] is not None:
                        assoc_info[client.name]['rssi'] = rssi_info[client.name][0]
                        assoc_info[client.name]['signal_strength'] = rssi_info[client.name][1].name

                    assoc_info[client.name]['was_steered'] = client.is_steered
                    assoc_info[client.name]['polluted'] = client.is_polluted
                else:
                    assoc_info[client.name]['serving_vap'] = None

                self._record_serving_vap(client)
                updated = True

        if updated:
            self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                SteeringServerProtocol.MSG_TYPE.ASSOCIATION_UPDATE, assoc_info)))

    def sendSteeringStatus(self, clients):
        updated = False
        for client in clients:
            client_change = False
            if client.name in self.steering_pending:
                if client.steering_pending_vap:
                    if set(client.target_vaps) != set(self.steering_pending[client.name]):
                        self.steering_pending[client.name] = client.target_vaps
                        updated = True
                        client_change = True
                    else:
                        # The same, state hasn't changed
                        pass
                else:  # Client needs to be removed from list
                    del self.steering_pending[client.name]
                    updated = True
                    client_change = True
            else:  # Client not in self.steering_pending
                if client.steering_pending_vap:
                    self.steering_pending[client.name] = client.target_vaps
                    updated = True
                    client_change = True

            if client_change:
                payload = {}
                payload['name'] = client.name

                # If the client associated on the given BSS due to steering and
                # we have not already generated a steer_end for it, indicate that
                # to the frontend.
                if client.is_steered:
                    payload['type'] = "end_steer"
                    self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                        SteeringServerProtocol.MSG_TYPE.STEERING_UPDATE, payload)))
                else:
                    payload['type'] = "steer"
                    self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                        SteeringServerProtocol.MSG_TYPE.STEERING_UPDATE, payload)))

        if updated and len(self.steering_pending):
            # Need to convert the pending VAP objects to just their IDs.
            pending_steers = {k: map(lambda vap: vap.vap_id, v)
                              for k, v in self.steering_pending.items()}
            self.sendMessage(json.dumps(SteeringServerProtocol.buildMessage(
                SteeringServerProtocol.MSG_TYPE.STEERING_ATTEMPT, pending_steers)))

    @staticmethod
    def buildMessage(msg_type, payload):
        msg = {}
        msg['type'] = msg_type.value
        msg['payload'] = payload
        return msg


def run_system(model, view, controller):
    """Launch all of the active components and wait for shutdown."""
    try:
        model.activate()
        controller.activate()
        view.activate()

        while not model.wait_for_shutdown(timeout=1.0):
            pass
    except:
        import traceback
        traceback.print_exc()
        model.shutdown()
    finally:
        # Model is done. Terminate everything else.
        controller.shutdown()
        view.shutdown()
