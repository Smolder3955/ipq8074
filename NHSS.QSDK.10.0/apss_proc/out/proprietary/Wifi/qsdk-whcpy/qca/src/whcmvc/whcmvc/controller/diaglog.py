#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

# Standard Python imports
import logging
import threading
import time

from whcdiag.constants import BAND_TYPE

from whcdiag.io.network import NetworkReaderRaw
from whcdiag.io.filesystem import FileWriterRaw
from whcdiag.msgs.wlanif import RawChannelUtilization, RawRSSI
from whcdiag.msgs.wlanif import RawChannelUtilization_v2, RawRSSI_v2
from whcdiag.msgs.stadb import AssociationUpdate, RSSIUpdate
from whcdiag.msgs.stadb import AssociationUpdate_v2, RSSIUpdate_v2
from whcdiag.msgs.stadb import ActivityUpdate, ActivityUpdate_v2
from whcdiag.msgs.stadb import DualBandUpdate
from whcdiag.msgs.bandmon import Utilization_v1, OverloadChange_v1
from whcdiag.msgs.bandmon import Utilization_v2, OverloadChange_v2
from whcdiag.msgs.bandmon import BlackoutChange
from whcdiag.msgs.exception import MessageMalformedError
from whcdiag.msgs.steerexec import SteerToBand, AbortSteerToBand
from whcdiag.msgs.steerexec import SteerEndStatusType
from whcdiag.msgs.steerexec import PostAssocSteer, SteerEnd
from whcdiag.msgs.steerexec import SteeringUnfriendly, SteeringUnfriendly_v2
from whcdiag.msgs.steerexec import SteeringProhibited, SteeringProhibited_v2
from whcdiag.msgs.steerexec import SteeringProhibitType
from whcdiag.msgs.steerexec import BTMCompliance
from whcdiag.msgs.estimator import ServingDataMetrics, STAPollutionChanged
from whcdiag.hydmsgs.pstables import HDefault
from whcdiag.hydmsgs.heservice import Detail

"""Process diagnostic logging messages and update the model.

This module exports the following:

    :class:`MsgHandler`: Active class that handles diagnostic log
        messages and updates the model.
"""

log = logging.getLogger('whcdiaglog')
"""The logger used for diagnostic logging operations, named ``whcdiaglog``."""


class MsgHandler(NetworkReaderRaw.MsgHandler):

    """Active class to handle diagnostic logging messages.

    This class is responsible for maintaining a snapshot of the current
    system state and updating the model when appropriate.
    """

    def __init__(self, model, parser, reassembler, ip_addr='',
                 port=NetworkReaderRaw.DEFAULT_PORT,
                 output_file=None):
        """Construct a new message handler.

        Args:
            model (:class:`ModelBase`): the model to update
            parser (:class:`parser`): parser to use on diag logs
            ip_addr (str): the IP address on which to listen, or the
                empty string to listen on all IPs
            port (int): the port on which to listen
            output_file (str): the file to write the received diagnostic
                logging messages to
        """
        NetworkReaderRaw.MsgHandler.__init__(self)
        self._model = model
        self._thread = None
        self._ip_addr = ip_addr
        self._port = port
        self._parser = parser
        self._reassembler = reassembler

        self._num_msgs = 0
        self._network_reader = None

        self._lock = threading.Lock()
        self._network_thread = None
        self._thread_running = False

        self._output_file = output_file
        self._file_writer = None

        self._ap_ip_mapping = {}

    def add_ap(self, ap, ip_addr):
        """Add an AP whose diaglog messages are handled in this handler

        Args:
            ap (:class:`AccessPoint`): the AP to add
            ip_addr (string): The IP address assigned to the AP

        Raise :exc:`ValueError` the IP address has been registered to another AP.
        """
        if ip_addr in self._ap_ip_mapping and self._ap_ip_mapping[ip_addr] is not ap:
            raise ValueError("Access Point IP addresses must be unique")

        self._ap_ip_mapping[ip_addr] = ap

        # TODO: Static mapping 0 to the other AP in the network before we get
        #       topology message giving us more accurate information
        if len(self._ap_ip_mapping) == 2:
            ap_list = self._ap_ip_mapping.values()
            for i in range(0, 2):
                ap_list[i].add_neighbor(0, ap_list[1 - i])

    def get_ap_by_ip(self, ip_addr):
        """Look up the AP using its IP address

        Args:
            ip_addr (string): The IP address of the AP

        Return:
            A :class:`AccessPoint` instance, or ``None`` if there is no AP matching
            the IP address
        """
        return self._ap_ip_mapping.get(ip_addr, None)

    def start(self):
        """Start listening for messages and passing them to the model."""
        with self._lock:
            if not self._thread_running:
                self._network_thread = threading.Thread(
                    target=self.receive_msgs, args=())
                self._network_thread.start()
                log.info("Diagnostic logs handler started.")
                self._thread_running = True

    def receive_msgs(self):
        """The function to receive messages.

        Note that this function will keep running until shutdown, and should
        be run in a seperate thread.
        """
        with NetworkReaderRaw(self._ip_addr, self._port) as self._network_reader:
            with FileWriterRaw(self._output_file) as self._file_writer:
                self._network_reader.receive_msgs(self)

        with self._lock:
            self._thread_running = False

    def shutdown(self, completion_action, *args, **kwargs):
        """Initiate the shutdown procedure and invoke an action when complete.

        This stops the listening for diagnostic logging messages, including
        terminating the thread that is listening. When the thread has
        terminated, the completion action will be invoked with whatever
        arguments are provided. When providing the completion action, caller
        must keep in mind that this callback will happen in a different
        thread context.

        Args:
            completion_action (:func:): the function to invoke upon
                completion
            args (:obj:`list`): positional arguments to pass to the
                completion function
            kwargs (:obj:`dict`): keyword arguments to pass to the
                completion function
        """
        def shutdown_action(network_reader, network_thread,
                            completion_action, *args, **kwargs):
            """Perform a clean shutdown of the network thread.

            Once the clean shutdown completes, invoke the completion
            handler.
            """
            network_reader.terminate_receive()
            network_thread.join()
            if completion_action:
                completion_action(*args, **kwargs)
            log.info("Diagnostic logs handler stopped.")

        if not self._network_reader:
            # Not yet started, nothing to shutdown
            return

        shutdown_args = [self._network_reader, self._network_thread,
                         completion_action] + list(args)
        thread = threading.Thread(target=shutdown_action,
                                  args=shutdown_args)
        thread.daemon = True
        thread.start()

    def log_msg(self, src_addr, data):
        """Output the log message to a file (if enabled).

        Args:
            src_addr (:obj:`tuple`): the tuple of the IP address (as a
                string) and port of the device that sent the message
            data (str): the packed string which is the message
        """
        if self._file_writer:
            self._file_writer.output_msg(time.time(), src_addr[0], data)

    def process_msg(self, src_addr, data):
        """Process a single diaglog message and update the model.

        Args:
            src_addr (:obj:`tuple`): the tuple of the IP address (as a
                string) and port of the device that sent the message
            data (str): the packed string which is the message
        """
        self._num_msgs += 1
        try:
            header, payload = self._parser.unpack_msg(data)
        except MessageMalformedError as e:
            log.error("Ignore invalid message: %s", repr(data))
            log.error(e)
            return

        if self._reassembler:
            (should_output_current, entries) = self._reassembler.reassemble(
                header, src_addr[0], (data, header, payload))

            # Output any reassembled entries
            if entries:
                for entry in entries:
                    self._handle_msg(src_addr, entry[1][0], entry[1][1], entry[1][2])
        else:
            should_output_current = True

        if should_output_current:
            self._handle_msg(src_addr, data, header, payload)

    def _lookup_vap_by_band(self, src_ap, band, payload):
        """Find the VAP on an AP for a given band.

        Args:
            src_ap (:class:`AccessPoint`): the AP on which to find the VAP
            band (:class:`BAND_TYPE`): the band to resolve
            payload (:class:`collections.namedtuple`): the message being
                processed (used for error messages only)

        Returns:
            the resolved :class:`VirtualAcessPoint`, or `None` if one is
                not found
        """
        vap = src_ap.get_vap(band)
        if not vap:
            log.error("Cannot locate VAP on AP ID '%s' by band for msg: %r",
                      src_ap.ap_id, payload)
        return vap

    def _lookup_vap_by_channel(self, src_ap, channel, payload):
        """Find the VAP on an AP for a given channel.

        Args:
            src_ap (:class:`AccessPoint`): the AP on which to find the VAP
            channel (int): the channel to resolve
            payload (:class:`collections.namedtuple`): the message being
                processed (used for error messages only)

        Returns:
            the resolved :class:`VirtualAcessPoint`, or `None` if one is
                not found
        """
        vap = src_ap.get_vap_by_channel(channel)
        if not vap:
            log.error("Cannot locate VAP on AP ID '%s' by channel for msg: %r",
                      src_ap.ap_id, payload)
        return vap

    def _lookup_vap_by_bss_info(self, src_ap, bss_info, payload):
        """Find the VAP on an AP for a given channel.

        Args:
            src_ap (:class:`AccessPoint`): the AP on which to find the VAP
            bss_info (:class:`BSSInfo`): the AP ID, channel ID, and ESS ID
                used to resolve the target BSS
            payload (:class:`collections.namedtuple`): the message being
                processed (used for error messages only)

        Returns:
            the resolved :class:`VirtualAcessPoint`, or `None` if one is
                not found
        """
        vap = src_ap.get_vap_by_bss_info(bss_info)
        if not vap:
            log.error("Cannot locate VAP on AP ID '%s' by BSSInfo for msg: %r",
                      src_ap.ap_id, payload)
        return vap

    def _handle_msg(self, src_addr, data, header, payload):
        # Let the log be written to a file as well.
        self.log_msg(src_addr, data)

        src_ap = self.get_ap_by_ip(src_addr[0])
        if not src_ap:
            log.error("Receive message from unknown IP addr: %s", src_addr)
            return

        if isinstance(payload, RawChannelUtilization):
            vap = self._lookup_vap_by_band(src_ap, payload.band, payload)
            if vap:
                util_update = {'data': {vap.vap_id: payload.utilization}, 'type': 'raw'}
                self._model.update_utilizations(util_update, src_ap.ap_id)
                log.debug("%s: Utilization update on VAP %s", src_ap.ap_id, vap.vap_id)
        elif isinstance(payload, Utilization_v1):
            vap = self._lookup_vap_by_band(src_ap, payload.band, payload)
            if vap:
                util_update = {'data': {vap.vap_id: payload.utilization}, 'type': 'average'}
                self._model.update_utilizations(util_update, src_ap.ap_id)
                log.debug("%s: Utilization update on VAP %s", src_ap.ap_id, vap.vap_id)
        elif isinstance(payload, RawChannelUtilization_v2):
            vap = self._lookup_vap_by_channel(src_ap, payload.channel_id, payload)
            if vap:
                util_update = {'data': {vap.vap_id: payload.utilization}, 'type': 'raw'}
                self._model.update_utilizations(util_update, src_ap.ap_id)
                log.debug("%s: Utilization update on VAP %s", src_ap.ap_id, vap.vap_id)
        elif isinstance(payload, Utilization_v2):
            vap = self._lookup_vap_by_channel(src_ap, payload.channel_id, payload)
            if vap:
                util_update = {'data': {vap.vap_id: payload.utilization}, 'type': 'average'}
                self._model.update_utilizations(util_update, src_ap.ap_id)
                log.debug("%s: Utilization update on VAP %s", src_ap.ap_id, vap.vap_id)
        elif isinstance(payload, RawRSSI) or isinstance(payload, RSSIUpdate):
            vap = self._lookup_vap_by_band(src_ap, payload.band, payload)
            if vap:
                self._model.update_associated_station_rssi(
                    vap.vap_id, payload.sta_addr,
                    payload.rssi, src_ap.ap_id)
                log.debug("%s: RSSI update for %s (%d)", src_ap.ap_id, payload.sta_addr,
                          payload.rssi)
        elif isinstance(payload, RawRSSI_v2) or isinstance(payload, RSSIUpdate_v2):
            vap = self._lookup_vap_by_bss_info(src_ap, payload.bss_info,
                                               payload)
            if vap:
                self._model.update_associated_station_rssi(
                    vap.vap_id, payload.sta_addr,
                    payload.rssi, src_ap.ap_id)
                log.debug("%s: RSSI update for %s (%d)", src_ap.ap_id, payload.sta_addr,
                          payload.rssi)
        elif isinstance(payload, AssociationUpdate):
            vap = self._lookup_vap_by_band(src_ap, payload.band, payload)
            if vap:
                # An association event implicitly marks the STA as disassociated
                # on the other band and all other APs.
                self._model.del_associated_station(payload.sta_addr,
                                                   src_ap.ap_id)
                map(lambda ap:
                    self._model.del_associated_station(payload.sta_addr,
                                                       ap.ap_id),
                    src_ap.neighbors)
                self._model.add_associated_station(vap.vap_id,
                                                   payload.sta_addr,
                                                   src_ap.ap_id)
                log.info("%s: %s associated on %s", src_ap.ap_id, payload.sta_addr,
                         vap.vap_id)
                self._model.update_associated_station_activity(
                    vap.vap_id, payload.sta_addr, src_ap.ap_id,
                    is_active=payload.is_active)
                self._model.update_station_flags(
                    payload.sta_addr, src_ap.ap_id,
                    is_dual_band=payload.is_dual_band)
            elif payload.band == BAND_TYPE.BAND_INVALID:
                self._model.del_associated_station(payload.sta_addr,
                                                   src_ap.ap_id)
                log.info("%s: %s disassociated", src_ap.ap_id, payload.sta_addr)

            # Do not update steering status if the VAP was not found
            if vap or payload.band == BAND_TYPE.BAND_INVALID:
                self._model.update_station_steering_status(payload.sta_addr,
                                                           assoc_band=payload.band)
        elif isinstance(payload, AssociationUpdate_v2):
            vap = self._lookup_vap_by_bss_info(src_ap, payload.bss_info,
                                               payload)
            if vap:
                if payload.is_associated:
                        # An association event implicitly marks the STA as disassociated
                        # on the other VAPs and all other APs.
                        self._model.del_associated_station(payload.sta_addr,
                                                           src_ap.ap_id)
                        map(lambda ap:
                            self._model.del_associated_station(payload.sta_addr,
                                                               ap.ap_id),
                            src_ap.neighbors)
                        self._model.add_associated_station(vap.vap_id,
                                                           payload.sta_addr,
                                                           src_ap.ap_id)
                        log.info("%s: %s associated on %s", src_ap.ap_id, payload.sta_addr,
                                 vap.vap_id)
                        self._model.update_station_steering_status(payload.sta_addr,
                                                                   assoc_vap=vap)
                        self._model.update_associated_station_activity(
                            vap.vap_id, payload.sta_addr, src_ap.ap_id,
                            is_active=payload.is_active)
                        self._model.update_station_flags(
                            payload.sta_addr, src_ap.ap_id,
                            is_dual_band=payload.is_dual_band)
                else:
                    self._model.del_associated_station(payload.sta_addr,
                                                       src_ap.ap_id)
                    log.info("%s: %s disassociated", src_ap.ap_id, payload.sta_addr)
                    self._model.update_station_steering_status(
                        payload.sta_addr, assoc_vap=None, ap_id=src_ap.ap_id)
        elif isinstance(payload, ActivityUpdate):
            vap = self._lookup_vap_by_band(src_ap, payload.band, payload)
            if vap:
                self._model.update_associated_station_activity(
                    vap.vap_id, payload.sta_addr, src_ap.ap_id,
                    is_active=payload.is_active)
                log.debug("%s: Activity update for %s (%r)", src_ap.ap_id,
                          payload.sta_addr, payload.is_active)
        elif isinstance(payload, ActivityUpdate_v2):
            vap = self._lookup_vap_by_bss_info(src_ap, payload.bss_info,
                                               payload)
            if vap:
                self._model.update_associated_station_activity(
                    vap.vap_id, payload.sta_addr, src_ap.ap_id,
                    is_active=payload.is_active)
                log.debug("%s: Activity update for %s (%r)", src_ap.ap_id,
                          payload.sta_addr, payload.is_active)
        elif isinstance(payload, DualBandUpdate):
            self._model.update_station_flags(payload.sta_addr,
                                             src_ap.ap_id,
                                             is_dual_band=payload.is_dual_band)
            log.info("%s: Dual band change for %s (%r)", src_ap.ap_id,
                     payload.sta_addr, payload.is_dual_band)
        elif isinstance(payload, OverloadChange_v1):
            vap_24g = self._lookup_vap_by_band(src_ap, BAND_TYPE.BAND_24G, payload)
            vap_5g = self._lookup_vap_by_band(src_ap, BAND_TYPE.BAND_5G, payload)
            if vap_24g and vap_5g:
                status = {vap_24g.vap_id: payload.current_overload_24g,
                          vap_5g.vap_id: payload.current_overload_5g}
                self._model.update_overload_status(status, src_ap.ap_id)
                log.info("%s: Overload status update: %r", src_ap.ap_id, status)
        elif isinstance(payload, OverloadChange_v2):
            status = dict(map(lambda v: (v.vap_id, False), src_ap.get_vaps()))
            for channel in payload.overloaded_channels:
                vap = self._lookup_vap_by_channel(src_ap, channel, payload)
                if vap:
                    status[vap.vap_id] = True

            self._model.update_overload_status(status, src_ap.ap_id)
            log.info("%s: Overload status update: %r", src_ap.ap_id, status)
        elif isinstance(payload, BlackoutChange):
            status = payload.is_blackout_start
            self._model.update_steering_blackout_status(status, src_ap.ap_id)
            log.info("%s: Steering blackout status update: %r", src_ap.ap_id, status)
        elif isinstance(payload, SteerToBand):
            self._model.update_station_steering_status(payload.sta_addr,
                                                       start=True,
                                                       assoc_band=payload.assoc_band,
                                                       target_band=payload.target_band)
            log.info("%s: %s being steered from band %s to band %s", src_ap.ap_id,
                     payload.sta_addr, payload.assoc_band, payload.target_band)
        elif isinstance(payload, PostAssocSteer):
            assoc_vap = self._lookup_vap_by_bss_info(src_ap, payload.assoc_bss,
                                                     payload)
            if assoc_vap:
                target_vaps = [self._lookup_vap_by_bss_info(src_ap, bss,
                                                            payload)
                               for bss in payload.candidate_bsses]
                # Eliminate any VAP that cannot be resolved
                target_vaps = [vap for vap in target_vaps if vap is not None]

                self._model.update_station_steering_status(payload.sta_addr,
                                                           start=True,
                                                           assoc_vap=assoc_vap,
                                                           target_vaps=target_vaps)
                log.info("%s: %s being steered from %s to %s (transaction %s)",
                         src_ap.ap_id, payload.sta_addr, assoc_vap.vap_id,
                         [vap.vap_id for vap in target_vaps], payload.transaction)
        elif isinstance(payload, AbortSteerToBand):
            self._model.update_station_steering_status(payload.sta_addr,
                                                       abort=True)
            log.info("%s: Abort steering %s; disabled_band: %s", src_ap.ap_id,
                     payload.sta_addr, payload.disabled_band)
        elif isinstance(payload, SteerEnd):
            if payload.status != SteerEndStatusType.STATUS_SUCCESS:
                # Only aborts need to be passed through, as a success should
                # already have been handled through the association update.
                self._model.update_station_steering_status(payload.sta_addr,
                                                           abort=True)

            log.info("%s: Steering ends %s; transaction: %s; type: %s; status: %s",
                     src_ap.ap_id, payload.sta_addr, payload.transaction,
                     payload.steer_type, payload.status)
        elif isinstance(payload, SteeringUnfriendly) or \
                isinstance(payload, SteeringUnfriendly_v2):
            self._model.update_station_flags(payload.sta_addr,
                                             src_ap.ap_id,
                                             is_unfriendly=payload.is_unfriendly)
            log.info("%s: Unfriendliness change for %s (%r)", src_ap.ap_id, payload.sta_addr,
                     payload.is_unfriendly)
        elif isinstance(payload, SteeringProhibited):
            # Convert the boolean into the enum used in phase 2 so that the
            # display logic can be consistent.
            prohibit_type = SteeringProhibitType.PROHIBIT_NONE
            if payload.is_prohibited:
                prohibit_type = SteeringProhibitType.PROHIBIT_LONG
            self._model.update_station_flags(payload.sta_addr,
                                             src_ap.ap_id,
                                             prohibit_type=prohibit_type)
            log.info("%s: Prohibit change for %s (%s)", src_ap.ap_id, payload.sta_addr,
                     prohibit_type)
        elif isinstance(payload, SteeringProhibited_v2):
            self._model.update_station_flags(payload.sta_addr,
                                             src_ap.ap_id,
                                             prohibit_type=payload.prohibit_type)
            log.info("%s: Prohibit change for %s (%s)", src_ap.ap_id, payload.sta_addr,
                     payload.prohibit_type)
        elif isinstance(payload, BTMCompliance):
            self._model.update_station_flags(payload.sta_addr,
                                             src_ap.ap_id,
                                             btm_compliance=payload.compliance_state,
                                             is_btm_unfriendly=payload.is_unfriendly)
            log.info("%s: BTM compliance change for %s (%s)", src_ap.ap_id, payload.sta_addr,
                     payload.compliance_state)
        elif isinstance(payload, ServingDataMetrics):
            vap = self._lookup_vap_by_bss_info(src_ap, payload.bss_info, payload)
            if vap:
                tput = payload.dl_throughput + payload.ul_throughput
                self._model.update_associated_station_data_metrics(
                    vap.vap_id, payload.addr, tput,
                    payload.last_tx_rate, payload.airtime, src_ap.ap_id)
                log.debug("%s: Data metrics update for %s (tput=%d, rate=%d, airtime=%d%%)",
                          src_ap.ap_id, payload.addr, tput, payload.last_tx_rate, payload.airtime)
        elif isinstance(payload, STAPollutionChanged):
            vap = self._lookup_vap_by_bss_info(src_ap, payload.bss_info, payload)
            if vap:
                self._model.update_station_pollution_state(payload.addr, src_ap.ap_id,
                                                           vap, payload.polluted)
                log.debug("%s: Pollution %s on %s for %s",
                          src_ap.ap_id, "detected" if payload.polluted else "cleared",
                          payload.bss_info, payload.addr)
        elif isinstance(payload, Detail):
            self._model.update_paths_from_hactive(payload[0], src_ap.ap_id,
                                                  header.more_fragments)
            log.debug("%s: Path update from H-Active", src_ap.ap_id)
        elif isinstance(payload, HDefault):
            self._model.update_paths_from_hdefault(payload[0], src_ap.ap_id,
                                                   header.more_fragments)
            log.debug("%s: Path update from H-Default", src_ap.ap_id)
        else:
            log.debug("%s: Ignore message %s", src_ap.ap_id, payload)
            pass
