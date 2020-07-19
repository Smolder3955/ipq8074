#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Core representation of the system state.

System state consists of a set of devices. These devices are composed of
APs which operate on a specific band. Each AP can have 0 or more STAs
associated to it.

This module exports the following:

    :class:`AccessPoint`
        a physical device that can contain 1 or more VAPs

    :class:`VirtualAccessPoint`
        a single Wi-Fi access point (VAP) on a given device, operating
        on a specific band

    :class:`ClientDevice`
        a station device which is associated to a specific access point

    :class:`DeviceDB`
        registry of APs, VAPs, and client devices

    :class:`ActiveModel`
        threaded implementation of model
"""

# Standard Python imports
import threading
import Queue
import functools
import logging
from collections import OrderedDict, namedtuple
from datetime import datetime
import time
import operator
import collections

# Imports from other components
from enum import Enum

import whcmvc
from whcdiag.constants import BAND_TYPE, AP_ID_SELF, UTIL_AGGR_SECS, TRANSPORT_PROTOCOL

TRAFFIC_CLASS_TYPE = Enum('TRAFFIC_CLASS_TYPE',
                          [('STREAMING', 0), ('TELEPHONY', 1),
                           ('DOWNLOAD', 2), ('INTERACTIVE', 3)])

CLIENT_TYPE = Enum('CLIENT_TYPE', [('PHONE', 0), ('TABLET', 1),
                                   ('LAPTOP', 2), ('PC', 3),
                                   ('GENERIC', 4)])

AP_TYPE = Enum('AP_TYPE', [('CAP', 0), ('RE', 1)])

CHANNEL_GROUP = Enum('CHANNEL_GROUP', [('CG_24G', 0), ('CG_5GL', 1),
                                       ('CG_5GH', 2)])

RSSI_LEVEL = Enum('RSSI_LEVEL', [('NONE', 0), ('WEAK', 1), ('LOW', 2),
                                 ('MODERATE', 3), ('HIGH', 4)])

# Indicate whether to query paths from all devices in the network, the CAP
# only, or none
QUERY_PATHS = Enum('QUERY_PATHS', [('ALL', 0), ('CAP_ONLY', 1), ('NONE', 2)])

# Default dominant rate threshold (in bps)
DOMINANT_RATE_THRESHOLD = 500000

# Default age before a path entry is considered stale (in seconds)
PATH_ENTRY_MAX_AGE = 5

log = logging.getLogger('whcmodelcore')
"""The logger used for model operations, named ``whcmodelcore``."""


class RSSIMapper(object):

    """Helper class for mapping from a numeric RSSI value to a level.

    This is primarily for use in the views that want to display a more
    friendly representation of the RSSI (eg. signal strength bars).
    """

    # Uplink RSSI values (in dB) above which (inclusive) the RSSI is considered
    # that level.
    LOW_RSSI_THRESH = 15
    MODERATE_RSSI_THRESH = 22
    HIGH_RSSI_THRESH = 29

    def __init__(self):
        """Create the mapper instance with default levels."""
        self._thresholds = {}
        self._thresholds[RSSI_LEVEL.LOW] = RSSIMapper.LOW_RSSI_THRESH
        self._thresholds[RSSI_LEVEL.MODERATE] = RSSIMapper.MODERATE_RSSI_THRESH
        self._thresholds[RSSI_LEVEL.HIGH] = RSSIMapper.HIGH_RSSI_THRESH

    def update_levels(self, new_levels):
        """Update the level thresholds per the values provided.

        Args:
            new_levels (dict): dictionary where the keys are :obj:`RSSI_LEVEL`
                values and the values are the thresholds
        Raises:
            :class:`ValueError` if the levels are inconsistent
        """
        low_rssi = new_levels.get(RSSI_LEVEL.LOW, self.rssi_low)
        moderate_rssi = new_levels.get(RSSI_LEVEL.MODERATE, self.rssi_moderate)
        high_rssi = new_levels.get(RSSI_LEVEL.HIGH, self.rssi_high)

        self._check_level_consistency(low_rssi, moderate_rssi, high_rssi)

        self._thresholds[RSSI_LEVEL.LOW] = low_rssi
        self._thresholds[RSSI_LEVEL.MODERATE] = moderate_rssi
        self._thresholds[RSSI_LEVEL.HIGH] = high_rssi

    def get_level(self, rssi):
        """Determine the level corresponding to the given RSSI.

        Args:
            rssi (int): the measured uplink RSSI value

        Return:
            the :obj:`RSSI_LEVEL` instance corresponding to that numeric value
        """
        if rssi is None:
            return RSSI_LEVEL.NONE
        elif rssi >= self.rssi_high:
            return RSSI_LEVEL.HIGH
        elif rssi >= self.rssi_moderate:
            return RSSI_LEVEL.MODERATE
        elif rssi >= self.rssi_low:
            return RSSI_LEVEL.LOW
        else:
            return RSSI_LEVEL.WEAK

    def _check_level_consistency(self, low_rssi, moderate_rssi, high_rssi):
        """Determine if the thresholds provided are consistent.

        This confirms that the low threshold is less than the moderate one,
        which in turn is less than the high one.

        Args:
            low_rssi (int): the level below which RSSI is considered weak
            moderate_rssi (int): the level below which RSSI is considered low
            high_rssi (int): the level below which RSSI is considered moderate
        Raises:
            :class:`ValueError` if the levels are inconsistent
        """
        if low_rssi > moderate_rssi:
            raise ValueError("Low threshold %d is greater than moderate %d" %
                             (low_rssi, moderate_rssi))
        if moderate_rssi > high_rssi:
            raise ValueError("Moderate threshold %d is greater than high %d" %
                             (moderate_rssi, high_rssi))

    @property
    def rssi_high(self):
        return self._thresholds[RSSI_LEVEL.HIGH]

    @property
    def rssi_moderate(self):
        return self._thresholds[RSSI_LEVEL.MODERATE]

    @property
    def rssi_low(self):
        return self._thresholds[RSSI_LEVEL.LOW]


class ClientDevice(object):

    """Current state for a given client device.

    Client devices are those devices such as smartphones, tablets, and
    laptops which act solely as Wi-Fi stations. This class captures
    both static and dynamic information about these devices.
    """

    def __init__(self, name, mac_addr, traffic_class,
                 client_type=CLIENT_TYPE.GENERIC):
        """Initialize a new client device.

        :param string mac_addr: The MAC address of the device as a
            colon-separated hexadecimal string.
        :param :obj:`TRAFFIC_CLASS_TYPE` traffic_class: The
            pre-configured type of traffic generated by this client
            device.
        :param :obj:`CLIENT_TYPE` client_type: The class of device this
            client represents. This is currently only intended for use
            by the view (if it even needs it).
        """
        self._name = name
        self._mac_addr = mac_addr.lower()  # canonical form
        self._traffic_class = traffic_class
        self._client_type = client_type
        self._serving_vap = None

        self.reset_flags()

        # Place holder for steering status
        self._src_band = None
        self._target_band = None
        self._src_vap = None
        self._target_vaps = None

        # Place holder for polluted VAPs
        self._polluted_vaps = set()

    def update_serving_vap(self, vap):
        """Set the serving AP for this device to the one provided.

        :param :class:`VirtualAccessPoint` vap: The VAP that is
            currently serving the station.
        """
        self._serving_vap = vap

    def update_steering_status(self, src_band, target_band,
                               src_vap, target_vaps):
        """Update the steering status for the client

        Args:
            src_band (:obj:`BAND_TYPE`): the band from which the client
                is steered. It implies that the steering is in process.
            target_band (:obj:`BAND_TYPE`): the band to which the client
                is steered. It implies that the current association is a
                result of a steering.
            src_vap (:obj: `VirtualAccessPoint`): the VAP from which the client
                is steered from. It implies that the steering is in process.
            target_vaps (list of :obj:`VirtualAccessPoint`): the VAPs to which
                the client is steered. It implies that the current association
                is a result of a steering.
        """
        self._src_band = src_band
        self._target_band = target_band
        self._src_vap = src_vap
        self._target_vaps = target_vaps

    def update_flags(self, is_active=None, is_dual_band=None,
                     btm_compliance=None, prohibit_type=None,
                     is_unfriendly=None, is_btm_unfriendly=None):
        """Update the various flags associated with this STA

        For any of the keyword arguments that are set to None, no update
        will be done.

        Args:
            is_active (bool): whether the STA is active or not
            is_dual_band (bool): whether the STA is known to be dual
                band or not
            btm_compliance (:class:`BTMComplianceType`): the current
                classification for this STA's BTM compliance
            prohibit_type (:class:`SteeringProhibitType`): whether the
                STA is prohibited from steering and if so, the type of
                prohibition
            is_unfriendly (bool): whether the STA is current considered
                unfriendly or not
            is_btm_unfriendly (bool): whether the STA is current considered
                BTM unfriendly or not
        """
        if is_active is not None:
            self._is_active = is_active
        if is_dual_band is not None:
            self._is_dual_band = is_dual_band
        if btm_compliance is not None:
            self._btm_compliance = btm_compliance
        if prohibit_type is not None:
            self._prohibit_type = prohibit_type
        if is_unfriendly is not None:
            self._is_unfriendly = is_unfriendly
        if is_btm_unfriendly is not None:
            self._is_btm_unfriendly = is_btm_unfriendly

    def update_pollution_state(self, vap, polluted):
        """Update pollution state for this STA

        Args:
            vap (:class:`VirtualAccessPoint`): the VAP on which pollution state
                changes
            polluted (bool): whether the VAP is polluted or not
        """
        if polluted:
            self._polluted_vaps.add(vap)
        elif vap in self._polluted_vaps:
            self._polluted_vaps.remove(vap)

    def reset_flags(self):
        self._is_active = None
        self._is_dual_band = None
        self._btm_compliance = None
        self._prohibit_type = None
        self._is_unfriendly = None
        self._is_btm_unfriendly = None
        self._polluted_vaps = set()

    @property
    def name(self):
        return self._name

    @property
    def mac_addr(self):
        return self._mac_addr

    @property
    def traffic_class(self):
        return self._traffic_class

    @property
    def client_type(self):
        return self._client_type

    @property
    def serving_vap(self):
        return self._serving_vap

    @property
    def target_band(self):
        return self._target_band

    @property
    def target_vaps(self):
        return self._target_vaps

    @property
    def steering_pending_band(self):
        return self._src_band is not None

    @property
    def steering_pending_vap(self):
        return self._src_vap is not None

    @property
    def is_steered(self):
        """Check if the client device is being steered to current band/bss"""
        return (self._serving_vap is not None and
                self._serving_vap.band == self._target_band) \
            or (self._serving_vap is not None and
                self._target_vaps is not None and
                self._serving_vap in self._target_vaps)

    @property
    def is_active(self):
        return self._is_active

    @property
    def is_dual_band(self):
        return self._is_dual_band

    @property
    def btm_compliance(self):
        return self._btm_compliance

    @property
    def prohibit_type(self):
        return self._prohibit_type

    @property
    def is_unfriendly(self):
        return self._is_unfriendly

    @property
    def is_btm_unfriendly(self):
        return self._is_btm_unfriendly

    @property
    def is_polluted(self):
        return self._serving_vap in self._polluted_vaps


class TrackedQuantity(object):
    """Class representing a quantity with metadata for update tracking.

    This quantity is tracked with a sequence number and a timestamp so that
    users of it can tell when it has been changed (if they keep this info).
    """

    def __init__(self):
        """Initialize with no current quantity."""
        self._value = None
        self._seq = 0
        self._timestamp = None

    def update(self, value, seq=None):
        """Record a new value and update the tracking information.

        :param int/float value: the new value to store
        :param int seq: a number to force the sequence number to be
        """
        self._value = value
        if seq is None:
            self._seq += 1
        else:
            self._seq = seq
        self._timestamp = datetime.now()

    @property
    def value(self):
        return self._value

    @property
    def seq(self):
        return self._seq

    @property
    def age(self):
        return float('inf') if self._timestamp is None \
            else (datetime.now() - self._timestamp).total_seconds()


class ServedSTAMetrics(object):

    """Measured and estimated metrics for a STA on a VAP.

    This is a snapshot in time for the various metrics that are
    collected on a per-STA basis on a given BSS. All of this
    information is lost when the STA transitions to a new BSS (either
    on its own or by being steered).
    """

    DataMetrics = namedtuple('DataMetrics', ['tput', 'rate', 'airtime'])
    RSSIMetrics = namedtuple('RSSIMetrics', ['rssi', 'level'])

    def __init__(self):
        """Initialize the STA with invalid values for all metrics."""
        self._rssi_metrics = TrackedQuantity()
        self._rssi = None
        self._rssi_level = RSSI_LEVEL.NONE
        self._data_metrics = TrackedQuantity()
        self._tput = None
        self._rate = None
        self._airtime = None

    def update_rssi(self, rssi, rssi_level):
        """Set the STA's uplink RSSI to the new value.

        Args:
            rssi (int): the new uplink RSSI
            rssi_level (:obj:`RSSI_LEVEL`): the level to which the raw RSSI
                value corresponds
        Returns:
            itself (to allow for chained calls)
        """
        self._rssi_metrics.update(self.RSSIMetrics._make([rssi, rssi_level]))
        return self

    def update_data_metrics(self, tput, rate, airtime):
        """Update the STA's serving data metrics.

        Args:
            tput (int): the combined STA Tx and Rx throughput, in Mbps
            rate (int): the estimated PHY rate in use by the STA, in Mbps
            airtime (int): the percentage of medium time used by the STA

        Returns:
            itself (to allow for chained calls)
        """
        self._data_metrics.update(self.DataMetrics._make([tput, rate, airtime]))
        return self

    @property
    def rssi(self):
        return getattr(self._rssi_metrics.value, 'rssi', None)

    @property
    def rssi_level(self):
        return getattr(self._rssi_metrics.value, 'level', None)

    @property
    def rssi_seq(self):
        return self._rssi_metrics.seq

    @property
    def tput(self):
        return getattr(self._data_metrics.value, 'tput', None)

    @property
    def rate(self):
        return getattr(self._data_metrics.value, 'rate', None)

    @property
    def data_seq(self):
        return self._data_metrics.seq

    @property
    def airtime(self):
        return getattr(self._data_metrics.value, 'airtime', None)

    def __eq__(self, rhs):
        return self._rssi == rhs._rssi and self._rssi_level == rhs._rssi_level and \
            self._tput == rhs._tput and \
            self._rate == rhs._rate and self._airtime == rhs._airtime


class VirtualAccessPoint(object):

    """An active Wi-Fi access point on a specific band.

    Access points operate on a specific band and channel and can have
    associated stations. Additionally, they have an interface name
    which is the name of the VAP interface (eg. ath0) on target.
    """

    def __init__(self, ap, vap_id, band, iface_name, radio_iface,
                 channel, channel_group, overload_threshold):
        """Initialize a new access point.

        Args:
            ap (:class:`AccessPoint`): The access point device that
                owns this VAP.
            vap_id (string): Identifier to use to identify this VAP
                instance within the overall system. This should be
                unique across all access points.
            band (:obj:`BAND_TYPE`): The band on which the access
                point operates.
            iface_name (string): Name of the interface on the target
                device corresponding to this VAP.
            radio_iface (string): Name of the interface corresponding
                to the radio on which the VAP operates.
            channel (int): Channel on which the VAP is operating.
            channel_group (:obj:`CHANNEL_GROUP`): the group of channels on
                which this VAP operations (where channel groups subdivide
                the band)
            overload_threshold (int): the point at which the channel is
                considered overloaded (as a percentage)
        """
        self._ap = ap
        self._vap_id = vap_id
        self._band = band
        self._channel_group = channel_group
        self._channel = channel
        self._overload_threshold = overload_threshold
        self._iface_name = iface_name
        self._radio_iface = radio_iface
        self._associated_stations = OrderedDict()
        self._utilization = None
        self._utilization_raw = TrackedQuantity()
        self._utilization_aggr = TrackedQuantity()
        self._utilization_avg = TrackedQuantity()
        self._tx_throughput = 0
        self._rx_throughput = 0
        self._overloaded = False

        self._ap.add_vap(self)

    def add_associated_station(self, sta_mac):
        """Add a single associated station.

        It will have a zero RSSI value by default, and should be updated
        later. Do nothing if the station has already been added before.

        Args:
            sta_mac (str): The MAC address of the new associated station
        """
        if sta_mac in self._associated_stations:
            return

        self._associated_stations[sta_mac] = ServedSTAMetrics()

    def del_associated_station(self, sta_mac):
        """Delete a single associated station.

        Args:
            sta_mac (str): The MAC address of the associated station

        Returns:
            A tuple of station MAC address and RSSI value if exists;
            otherwise return None
        """
        if sta_mac in self._associated_stations:
            # The behaviors of pop are different in `dict` and `OrderedDict`.
            # If use `dict`, this should just return pop(sta_mac).
            return (sta_mac, self._associated_stations.pop(sta_mac))
        return None

    def update_associated_station_rssi(self, sta_mac, rssi, rssi_level):
        """Update the RSSI value of a specific station

        If the station does not exist, do nothing.

        Args:
            sta_mac (str): The MAC address of the associated station
            rssi (int): the RSSI measurement
            rssi_level (:obj:`RSSI_LEVEL`): the level to which the numeric
                RSSI maps
        Returns:
            True if it's updated successfully; otherwise return False
        """
        if sta_mac in self._associated_stations:
            self._associated_stations[sta_mac].update_rssi(rssi, rssi_level)
            return True
        return False

    def update_associated_station_data_metrics(self, sta_mac, tput,
                                               rate, airtime):
        """Update the data metrics of a specific station

        If the station does not exist, do nothing.

        Args:
            sta_mac (str): The MAC address of the associated station
            tput (int): the throughput (in Mbps)
            rate (int): the estimated PHY rate (in Mbps)
            airtime (int): the estimated airtime (as a percentage)
        Returns:
            True if it's updated successfully; otherwise return False
        """
        if sta_mac in self._associated_stations:
            self._associated_stations[sta_mac].update_data_metrics(tput, rate, airtime)
            return True
        return False

    def update_associated_station_activity(self, sta_mac, client_dev,
                                           is_active):
        """Update whether a specific STA is active or not

        If the station does not exist, do nothing.

        Args:
            sta_mac (str): The MAC address of the associated station
            client_dev (:class:`ClientDevice`): the device object for this
                STA
            is_active (bool): whether this STA is considered active or not

        Returns:
            True if it's updated successfully; otherwise return False
        """
        if client_dev and sta_mac in self._associated_stations:
            client_dev.update_flags(is_active=is_active)
            return True
        return False

    def update_associated_stations(self, station_list, rssi_mapper):
        """Update the list of associated stations to that provided.

        All stations which do not appear in the new list will be
        removed from this AP.

        Args:
            station_list (list of 3-tuples): the first element in the
                tuple is the MAC address, the second is the RSSI, and
                the third is the last Tx rate (in Mbps)
            rssi_mapper (:class:`RSSIMapper`): the instance to use to map
                numeric RSSI values to a level
        """
        self._associated_stations = \
            OrderedDict(map(
                lambda x: (
                    x[0],
                    # Only the rate is known. The other values are set to None.
                    ServedSTAMetrics().update_rssi(x[1], rssi_mapper.get_level(x[1]))
                                      .update_data_metrics(None, x[2], None)
                ), station_list))

    def get_associated_stations(self):
        """Obtain a copy of the list of associated stations.

        This copy is a shallow copy. Returns a list of tuples containing
        the MAC address and :class:`ServedSTAMetrics`.
        """
        return self._associated_stations.items()

    def update_utilization_raw(self, utilization, seq=None):
        """Update the raw utilization of this VAP

        :param float utilization: the monitored utilization of this VAP
        :param int seq: a number to force the sequence number to be
        """
        self._utilization_raw.update(utilization, seq)

        # For backwards compatibility:
        self._utilization = utilization

    def update_utilization_avg(self, utilization, seq=None):
        """Update the utilization of this VAP

        :param float utilization: the monitored utilization of this VAP
        :param int seq: a number to force the sequence number to be
        """
        self._utilization_avg.update(utilization, seq)
        # For backwards compatibility:
        self._utilization = utilization

    def update_utilization_aggr(self, utilization, seq=None):
        """Update aggregated utilization of this VAP.

        :param float utilization: the aggregated utilization of this VAP
        :param int seq: a number to force the sequence number to be
        """
        self._utilization_aggr.update(utilization, seq)

    def compute_utilization_aggr(self, utilization):
        """Compute aggregated utilization

        :param int utilization: utilization reported from others

        Returns the larger of self utilization and other's utilization, or None
            if self utilization is not available or too old
        """
        if self._utilization_avg.value is None or \
                self._utilization_avg.age > UTIL_AGGR_SECS:
            return None

        return max(self._utilization_avg.value, utilization)

    def update_overload(self, overloaded):
        """Update the overload status of this VAP

        Args:
            overloaded (bool): the overload status to set
        """
        self._overloaded = overloaded

    def update_throughput(self, throughput):
        """Update the throughput of this VAP

        Args:
            throughput (tuple): the TX and RX throughputs
        """
        self._tx_throughput = throughput[0]
        self._rx_throughput = throughput[1]

    @property
    def ap(self):
        return self._ap

    @property
    def vap_id(self):
        return self._vap_id

    @property
    def band(self):
        return self._band

    @property
    def channel(self):
        return self._channel

    @property
    def channel_group(self):
        return self._channel_group

    @property
    def overload_threshold(self):
        return self._overload_threshold

    @property
    def iface_name(self):
        return self._iface_name

    @property
    def radio_iface(self):
        return self._radio_iface

    @property
    def utilization(self):
        return self._utilization

    @property
    def utilization_raw(self):
        return self._utilization_raw

    @property
    def utilization_avg(self):
        return self._utilization_avg

    @property
    def utilization_aggr(self):
        return self._utilization_aggr

    @property
    def overloaded(self):
        return self._overloaded

    @property
    def tx_throughput(self):
        return self._tx_throughput

    @property
    def rx_throughput(self):
        return self._rx_throughput


class Path(object):
    """Representation of a path to a device."""

    # Enum describing if the path is generated from H-Default or H-Active
    GENERATED_FROM = Enum('GENERATED_FROM', [('HDEFAULT', 0), ('HACTIVE', 1)])

    def __init__(self):
        self._next_hop = None
        self._intf = None
        self._rate = None
        self._hash = None
        self._generated_from = None
        self._traffic_type = None
        self._timestamp = None
        self._hdefault_next_hop = None
        self._hdefault_intf = None

    def update_from_hactive(self, next_hop, intf, entry):
        """Update the path from a H-Active entry.

        Args:
            next_hop (:class:`AccessPoint`): next hop AP
            intf (:class:`VirtualAccessPoint`): egress VAP
            entry (NamedTuple): H-Active entry as a NamedTuple
        """
        self._next_hop = next_hop
        self._intf = intf
        self._rate = entry.rate
        self._hash = entry.hash
        self._traffic_type = entry.sub_class
        self._generated_from = self.GENERATED_FROM.HACTIVE
        self._timestamp = time.time()

    def update_from_hdefault(self, next_hop, intf):
        """Update the path from a H-Default entry.

        Args:
            next_hop (:class:`AccessPoint`): next hop AP
            intf (:class:`VirtualAccessPoint`): egress VAP
        """
        self._next_hop = next_hop
        self._hdefault_next_hop = next_hop
        self._intf = intf
        self._hdefault_intf = intf
        # Default to using UDP interface
        # Rate and hash are 0 for H-Default
        self._rate = 0
        self._hash = 0
        self._traffic_type = TRANSPORT_PROTOCOL.UDP
        self._generated_from = self.GENERATED_FROM.HDEFAULT
        self._timestamp = time.time()

    def fallback_to_hdefault(self):
        """Attempt to fallback to the H-Default path.

        Returns:
            True if successful (there is a H-Default path), False otherwise
        """
        if self._hdefault_next_hop or self._hdefault_intf:
            # Fallback to H-Default information
            self._next_hop = self._hdefault_next_hop
            self._intf = self._hdefault_intf
            self._generated_from = self.GENERATED_FROM.HDEFAULT
            self._timestamp = time.time()

            return True
        else:
            return False

    def update_backup_path(self, next_hop, intf):
        """Update the backup H-Default path.

        Args:
            next_hop (:class:`AccessPoint`): next hop AP
            intf (:class:`VirtualAccessPoint`): egress VAP
        """
        self._hdefault_next_hop = next_hop
        self._hdefault_intf = intf

    def is_updated_from_hdefault(self):
        """ Returns True if path is generated from H-Default"""
        return self._generated_from == self.GENERATED_FROM.HDEFAULT

    def is_path_same(self, original_path_info):
        """Check if this path matches the original_path_info.

        Args:
            original_path_info (tuple): path info containing (next_hop,
                intf, is_updated_from_hdefault)
        """
        return original_path_info[0] == self._next_hop and \
            original_path_info[1] == self._intf and \
            original_path_info[2] == self.is_updated_from_hdefault()

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def next_hop(self):
        return self._next_hop

    @property
    def intf(self):
        return self._intf

    @property
    def active_intf(self):
        return self._intf if not self.is_updated_from_hdefault() else None

    @property
    def hash(self):
        return self._hash


class AccessPoint(object):
    """Representation of the actual access point device.

    :class:`AccessPoint` objects contain :class:`VirtualAccessPoint`
    objects along with a unique identifier which can be used for
    lookup.
    """
    def __init__(self, ap_id, ap_type, mac_address):
        """Initialize a new access point.

        :param string ap_id: The unique identifier for the device.
        :param Enum ap_type: The type of AP to create
        :param string mac_address: The AL MAC address of the AP
        """
        self._ap_id = ap_id
        self._mac_address = mac_address.lower()  # canonical form
        self._ip_address = None
        self._ap_type = ap_type
        self._vaps = []
        self._steering_blackout = False
        self._neighbors = {}
        self._paths = {}
        self._hdefault_entries = set()
        self._hactive_entries = set()

        # Set of H-Active entries updated in the last log
        self._hactive_entries_updated = set()

        # Set of H-Default entries updated in the last log
        self._hdefault_entries_updated = set()

    def add_vap(self, vap):
        """Add the VAP to this APs list of VAPs.

        :param :class:`VirtualAccessPoint` vap: The VAP object to add.
        """
        self._vaps.append(vap)

    def get_vaps(self):
        """Get a shallow copy of the list of VAPs on this access point."""
        return list(self._vaps)

    def get_paths(self):
        """Get a shallow copy of the list of paths on this access point."""
        return self._paths.copy()

    def get_vap(self, band):
        """Get the VAP of a given band.

        If there are multiple VAP on the given band, return the first one.

        Args:
            band (:obj:`BAND_TYPE`): The band on which to look for a VAP

        Returns:
            The VAP on the given band, or None if not found
        """
        if band.value >= BAND_TYPE.BAND_INVALID.value:
            return None

        for vap in self._vaps:
            if vap.band == band:
                return vap

        return None

    def get_vap_by_channel(self, channel):
        """Get the VAP that is operating on the provided channel.

        Looks up the given channel to see if any of the VAPs have
        this channel from the config.

        Args:
            channel (int): The channel on which to look for a VAP

        Returns:
            The VAP on the given channel, or None if not found
        """
        for vap in self._vaps:
            if channel == vap._channel:
                return vap
        return None

    def get_vap_by_bss_info(self, bss_info):
        """Get the VAP corresponding to the provided BSS info.

        It will first do lookup by AP ID to determine if the BSS is from
        self or neighbor APs, then does a lookup by channel. In the future,
        it will do the lookup by channel and ESS identifier.

        Args:
            bss_info (:obj:`BSSInfo`): The information for the BSS for
                which to look up the VAP

        Returns:
            The VAP with the matching BSS info, or None if not found
        """
        if bss_info.ap == AP_ID_SELF:
            return self.get_vap_by_channel(bss_info.channel)

        neighbor_ap = self._neighbors.get(bss_info.ap, None)
        if not neighbor_ap:
            return None

        return neighbor_ap.get_vap_by_channel(bss_info.channel)

    def _get_vap_by_iface_name(self, iface_name):
        """Get the VAP with a particular interface name.

        Args:
            iface_name (string): The interface name

        Returns:
            The VAP with the given name, or None if not found
        """
        for vap in self._vaps:
            if iface_name == vap._iface_name:
                return vap
        return None

    def set_ip_addr(self, ip_addr):
        """Set the IP address of the AP.

        Args:
            ip_addr (string): IP address in string format
        """
        self._ip_address = ip_addr

    def update_steering_blackout(self, blackout_status):
        """Set the current steering blackout status.

        Args:
            blackout_status (bool): whether steering is currently disallowed
                due to recent steering or not
        """
        self._steering_blackout = blackout_status

    def add_neighbor(self, neighbor_id, ap):
        """Add another AP to the neighbor table

        Args:
            neighbor_id (int): the index from topology database
            ap (:class:`AccessPoint`): the neighbor AP
        """
        if neighbor_id in self._neighbors:
            log.debug("Replace neighbor %d from %s to %s", neighbor_id,
                      self._neighbors[neighbor_id].ap_id, ap.ap_id)
        self._neighbors[neighbor_id] = ap

    def _get_max_rate_entry(self, entries):
        """Get the table row with the highest rate field

        Args:
            entries (list): list of named tuples to search
        Returns:
            The named tuple with the highest rate field
        """
        return max(entries, key=operator.attrgetter('rate'))

    def _get_path(self, da):
        """Get or create a path object for a destination address

        Args:
            da (str): destination address
        Returns:
            Path object
        """
        original_path_info = (None, None, None)
        if da in self._paths:
            # If we already have an entry, return it
            update_path = self._paths[da]
            original_path_info = (update_path.next_hop,
                                  update_path.intf,
                                  update_path.is_updated_from_hdefault())
        else:
            # Else make a new path object
            update_path = Path()

        return (update_path, original_path_info)

    def _log_path_change(self, da, update_path, reason_text):
        if update_path.intf:
            iface_name = update_path.intf.iface_name
        else:
            iface_name = "Unknown"

        if update_path.next_hop:
            next_hop = update_path.next_hop.ap_id
        else:
            next_hop = "Unknown"

        generated_from = "H-Default" if update_path.is_updated_from_hdefault() else "H-Active"

        log.info("(%s)[%s](%s) -> %s (Generated from %s, Reason: %s)" %
                 (self._ap_id, da, iface_name, next_hop, generated_from, reason_text))

    def _set_path(self, da, update_path, update_set, original_path_info):
        """Set the path object for a destination address, and update the given
           set to indicate da is present

        Args:
            da (str): destination address
            update_path (:class:`Path`): path to destination address
            update_set (set): set of destination addresses
        """
        if da not in self._paths or not update_path.is_path_same(original_path_info):
            self._log_path_change(da, update_path, "Update")
        self._paths[da] = update_path
        update_set.add(da)

    def _update_path_from_hactive(self, entry, device_db, updated_set):
        """Set the path to a destination from a H-Active table row

        Args:
            entry (NamedTuple): H-Active namedtuple to update the path from
            device_db (class:`DeviceDB`): database of all devices in the network
            updated_set (set): set of destination addresses
        """
        (update_path, original_path_info) = self._get_path(entry.da)

        update_path.update_from_hactive(device_db.get_ap_by_mac_addr(entry.id),
                                        self._get_vap_by_iface_name(entry.iface),
                                        entry)

        self._set_path(entry.da, update_path, updated_set, original_path_info)

    def _fallback_to_hdefault(self, da):
        """Either fallback to H-Default info, or delete if no H-Default data

        Args:
            da (str): destination address
        """
        if not self._paths[da].fallback_to_hdefault():
            # No H-Default information either - delete
            log.info("(%s)[%s] Can't fallback to H-Default path, deleting" %
                     (self._ap_id, da))
            del self._paths[da]
        else:
            self._log_path_change(da, self._paths[da], "Fallback")

    def _update_path_from_hdefault(self, entry, device_db, updated_set):
        """Set the path to a destination from a H-Default table row

        Args:
            entry (NamedTuple): H-Default namedtuple to update the path from
            device_db (class:`DeviceDB`): database of all devices in the network
            updated_set (set): set of destination addresses
        """
        (update_path, original_path_info) = self._get_path(entry.da)

        update_path.update_from_hdefault(device_db.get_ap_by_mac_addr(entry.id),
                                         self._get_vap_by_iface_name(entry.iface_udp))

        self._set_path(entry.da, update_path, updated_set, original_path_info)

    def _update_backup_path_from_hdefault(self, entry, device_db, updated_set):
        """Set the backup H-Default path to a destination from a H-Default table row

        Args:
            entry (NamedTuple): H-Default namedtuple to update the path from
            device_db (class:`DeviceDB`): database of all devices in the network
            updated_set (set): set of destination addresses
        """
        self._paths[entry.da].update_backup_path(device_db.get_ap_by_mac_addr(entry.id),
                                                 self._get_vap_by_iface_name(entry.iface_udp))

        updated_set.add(entry.da)

    def _update_or_remove_hdefault(self, da):
        """Clear the backup H-Default path to a destination

        Args:
            da (str): destination address
        """
        if self._paths[da].is_updated_from_hdefault():
            # Generated from H-Default - delete
            del self._paths[da]
            log.info("(%s)[%s] Deleting H-Default based path" %
                     (self._ap_id, da))
        else:
            # Clear the backup H-Default info
            self._paths[da].update_backup_path(None, None)

    def _exceeds_age_threshold(self, entry_timestamp, max_age):
        """Check if the age of a path exceeds the maximum"""
        return time.time() - entry_timestamp > max_age

    def update_paths_from_hactive(self, hactive_table, device_db, more_fragments):
        """Set all relevant paths to destinations from a H-Active table

        Args:
            hactive_table (list): list of H-Active rows (in format HActiveRow)
            device_db (class:`DeviceDB`): database of all devices in the network
            more_fragments (bool): set to True if more fragments are coming for
                this set of H-Active path updates
        """
        # Convert the table from a list to a dictionary, keyed on MAC address
        hactive_dict = collections.defaultdict(list)
        for row in hactive_table:
            hactive_dict[row.da].append(row)

        # For each destination in the database, update the saved path if necessary
        for da in hactive_dict:
            # Get the highest flow rate entry
            max_rate_row = self._get_max_rate_entry(hactive_dict[da])

            # Check if we at least reach the dominant rate threshold
            if max_rate_row.rate <= device_db.dominant_rate_threshold:
                # Do we already have a H-Active based saved path for this DA?
                if da in self._paths and not self._paths[da].is_updated_from_hdefault():
                    # Do we have an updated H-Active row for this path?
                    update = next((x for x in hactive_dict[da] if x.hash == self._paths[da].hash),
                                  None)
                    if update:
                        # Update the existing flow
                        self._update_path_from_hactive(update, device_db,
                                                       self._hactive_entries_updated)
                        continue

            # Any other condition - update to the highest rate entry
            self._update_path_from_hactive(max_rate_row, device_db,
                                           self._hactive_entries_updated)

        if more_fragments:
            # There are more fragments coming in this log, so can't do cleanup yet
            return

        # At the end of the log, so cleanup now

        # Are there any entries for which we previously had an H-Active entry, but
        # now do not?
        removed_from_hactive = self._hactive_entries - self._hactive_entries_updated

        for entry in removed_from_hactive:
            self._fallback_to_hdefault(entry)

        # Update the set of H-Active entries
        self._hactive_entries = self._hactive_entries_updated
        # Clear the updated set for the next log
        self._hactive_entries_updated = set()

    def update_paths_from_hdefault(self, hdefault_table, device_db, more_fragments):
        """Set all relevant paths to destinations from a H-Default table

        Args:
            hdefault_table (list): list of H-Default rows (in format HDefaultRow)
            device_db (class:`DeviceDB`): database of all devices in the network
            more_fragments (bool): set to True if more fragments are coming for
                this set of H-Default path updates
        """
        for row in hdefault_table:
            # Only update from H-Default if we don't have an entry at all,
            # or it was generated from H-Default already

            if row.da not in self._paths or self._paths[row.da].is_updated_from_hdefault() or \
                    self._exceeds_age_threshold(self._paths[row.da].timestamp,
                                                device_db.path_entry_max_age):
                # Add an entry
                self._update_path_from_hdefault(row, device_db,
                                                self._hdefault_entries_updated)

                # Remove from the H-Active set if present
                self._hactive_entries.discard(row.da)
            else:
                # Update the 'backup' H-Default based path
                self._update_backup_path_from_hdefault(row, device_db,
                                                       self._hdefault_entries_updated)

        if more_fragments:
            # There are more fragments coming in this log, so can't do cleanup yet
            return

        # At the end of the log, so cleanup now

        # Are there any entries for which we previously had H-Default info, but
        # now do not?
        removed = self._hdefault_entries - self._hdefault_entries_updated

        for entry in removed:
            self._update_or_remove_hdefault(entry)

        # Update the set of H-Default entries
        self._hdefault_entries = self._hdefault_entries_updated
        # Clear the updated set for the next log
        self._hdefault_entries_updated = set()

        # Check if there are any H-Active only entries which should be aged out
        hactive_only_entries = self._hactive_entries - self._hdefault_entries

        for entry in hactive_only_entries:
            if self._exceeds_age_threshold(self._paths[entry].timestamp,
                                           device_db.path_entry_max_age):
                del self._paths[entry]
                self._hactive_entries.remove(entry)
                log.info("(%s)[%s] Deleting H-Active based path" %
                         (self._ap_id, entry))

    @property
    def ap_id(self):
        return self._ap_id

    @property
    def ip_address(self):
        return self._ip_address

    @property
    def mac_address(self):
        return self._mac_address

    @property
    def ap_type(self):
        return self._ap_type

    @property
    def steering_blackout(self):
        return self._steering_blackout

    @property
    def neighbors(self):
        return self._neighbors.values()


class DeviceDB(object):

    """A registry of devices, indexed by unique identifiers.

    See the specific classes for the recommended format for these
    identifiers.
    """

    def __init__(self, dominant_rate_threshold=DOMINANT_RATE_THRESHOLD,
                 path_entry_max_age=PATH_ENTRY_MAX_AGE,
                 query_paths=QUERY_PATHS.NONE):
        self._dominant_rate_threshold = dominant_rate_threshold
        self._path_entry_max_age = path_entry_max_age
        self._query_paths = query_paths

        """Initialize an empty device database."""
        self._registered_client_devices = {}
        self._registered_aps = {}
        self._registered_vaps = {}

    def add_ap(self, ap):
        """Add an access point to the database.

        All VAPs that have been added to the AP will also be added to
        the database. Any that are added to the AP after this call can
        be added to the database using :meth:`add_vap`.

        :param :class:`AccessPoint` ap: The AP object to add.

        Raises :exc:`ValueError` if the there already is a registered
        access point with the same identifier.
        """
        if ap.ap_id in self._registered_aps:
            raise ValueError("AP IDs must be unique")

        self._registered_aps[ap.ap_id] = ap

        for vap in ap.get_vaps():
            self.add_vap(vap)

    def get_ap(self, ap_id):
        """Look up the access point using its identifier.

        :param string ap_id: The unique AP identifier.

        Returns a :class:`AccessPoint` instance, or ``None`` if
        there is no AP matching that identifier.
        """
        return self._registered_aps.get(ap_id, None)

    def get_ap_by_mac_addr(self, mac_addr):
        """Look up the access point using its MAC address.

        :param string mac_addr: The unique MAC address.

        Returns a :class:`AccessPoint` instance, or ``None`` if
        there is no AP matching that MAC address.
        """
        for device in self._registered_aps.itervalues():
            if device.mac_address.lower() == mac_addr.lower():
                return device

        return None

    def get_aps(self):
        """Obtain a list of all registered APs."""
        return self._registered_aps.values()

    def add_vap(self, vap):
        """Add a virtual access point to the database.

        :param :class:`VirtualAccessPoint` vap: The VAP object to add.

        Raises :exc:`ValueError` if the there already is a registered
        access point with the same identifier.
        """
        if vap.vap_id in self._registered_vaps:
            raise ValueError("VAP IDs must be unique")

        self._registered_vaps[vap.vap_id] = vap

    def get_vap(self, vap_id):
        """Look up the virtual access point using its identifier.

        :param string vap_id: The unique VAP identifier.

        Returns a :class:`VirtualAccessPoint` instance, or ``None`` if
        there is no VAP matching that identifier.
        """
        return self._registered_vaps.get(vap_id, None)

    def get_vaps(self):
        """Obtain a list of all registered VAPs."""
        return self._registered_vaps.values()

    def get_same_channel_vaps(self, channel):
        """Obtain a list of VAPs that on the given channel

        :param int channel: the given channel

        Returns a list of :class:`VirtualAccessPoint` instance, or empty list if
        there is no VAP on the given channel.
        """
        return [vap for vap in self.get_vaps() if vap.channel == channel]

    def add_client_device(self, client_device):
        """Add a client device to the database.

        :param :class:`ClientDevice` client_device: The device object
            to add.

        Raises :exc:`ValueError` if the there already is a registered
        client device with the same MAC address.
        """
        if client_device.mac_addr in self._registered_client_devices:
            raise ValueError("Client device MAC addresses must be unique")

        self._registered_client_devices[client_device.mac_addr] = client_device

    def get_client_device(self, mac_addr):
        """Look up the client device using its MAC address.

        :param string mac_adddr: The MAC address of the client device,
            as a hexadecimal string separated by colons (in other words,
            the standard representation).

        Returns a :class:`ClientDevice` instance, or ``None`` if there
        is no client device matching that identifier.
        """
        # Use the MAC address in all lowercase as that is what is used when
        # populating the dictionary.
        return self._registered_client_devices.get(mac_addr.lower(), None)

    def get_client_devices(self):
        """Obtain a list of all registered client devices."""
        return self._registered_client_devices.values()

    def get_num_client_devices(self):
        return len(self._registered_client_devices)

    @property
    def dominant_rate_threshold(self):
        return self._dominant_rate_threshold

    @property
    def path_entry_max_age(self):
        return self._path_entry_max_age

    @property
    def query_paths(self):
        return self._query_paths


class ModelBase(object):

    """Base class for all model implementations.

    The model stores state about the system. The static state consists
    of the set of access points and preconfigured client devices. The
    dynamic state consists of channel utilization as measured by access
    points along with information about which client devices are
    associated to each access point and what their signal strength is.
    """

    class Observer(object):

        """Interface for observers of the model data."""

        def __init__(self):
            """Initialize a new observer instance."""
            pass

        def update_associated_stations(self, model, ap_id):
            """React to updated associated station information.

            :param :class:`ModelBase` model: the model that contains
                the updated station information
            :param str ap_id: the ID of the AP whose info is contained in
                this update event

            raises :exc:NotImplementedError
                This method must be implemented by derived classes.
            """
            raise NotImplementedError("Must be implemented by derived classes")

        def update_utilizations(self, model, ap_id):
            """Update the utilizations on the VAPs provided

            :param :class:`ModelBase` model: the model that contains
                the updated utilization information
            :param str ap_id: the ID of the AP whose info is contained in
                this update event

            raises :exc:NotImplementedError
                This method must be implemented by derived classes.
            """
            raise NotImplementedError("Must be implemented by derived classes")

        def update_throughputs(self, model, ap_id):
            """Update the throughputs on the VAPs provided

            :param :class:`ModelBase` model: the model that contains
                the updated throughput information
            :param str ap_id: the ID of the AP whose info is contained in
                this update event

            raises :exc:NotImplementedError
                This method must be implemented by derived classes.
            """
            raise NotImplementedError("Must be implemented by derived classes")

        def update_paths(self, model, ap_id):
            """React to the paths being updated for a given AP.

            :param :class:`ModelBase` model: the model that contains the new
                path information
            :param str ap_id: the ID of the AP whose path info was updated

            raises: exc:`NotImplementedError`
                This method must be implemented by derived classes.
            """
            raise NotImplementedError("Must be implemented by derived classes")

    def __init__(self, device_db):
        """Initialize the model with a provided database.

        :param :class:`DeviceDB` device_db: the device database to use;
            this can be empty or already populated
        """
        self._device_db = device_db
        self._rssi_mapper = RSSIMapper()

        self._observers = []

    def register_observer(self, observer):
        """Register a new observer instance with the model.

        This observer will be notified of any changes in associated
        station or medium utilization information.

        :param :class:`Observer` observer: the observer to register
        """
        self._observers.append(observer)

    def notify_observers_associated_stations(self, ap_id):
        """Notify all observers of updated associated station info.

        :param str ap_id: the ID of the AP which is updated
        """
        map(lambda o: o.update_associated_stations(self, ap_id), self._observers)

    def notify_observers_utilizations(self, ap_id):
        """Notify all observers of updated utilization info.

        :param str ap_id: the ID of the AP which is updated
        """
        map(lambda o: o.update_utilizations(self, ap_id), self._observers)

    def notify_observers_throughputs(self, ap_id):
        """Notify all observers of updated throughput info.

        :param str ap_id: the ID of the AP which is updated
        """
        map(lambda o: o.update_throughputs(self, ap_id), self._observers)

    def notify_observers_paths(self, ap_id):
        """Notify all observers of updated path info.

        :param str ap_id: the ID of the AP which is updated
        """
        map(lambda o: o.update_paths(self, ap_id), self._observers)

    def update_associated_stations(self, stations, ap_id):
        """Update the associated stations on the VAPs specified by ap_id provided.

        :param dict stations: dictionary where the keys are the VAP
            identifiers and each value is a list consisting of tuples,
            with the first element in the tuple being a client device
            MAC address and the second entry being an RSSI in dB
        :param str ap_id: the ID of the AP whose info is contained in
            this update event

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_utilizations(self, utilizations, ap_id):
        """Update the utilizations on the VAPs provided

        :param dict utilizations: dictionary where the keys are the VAP
            identifiers and values are the utilizations of the VAP
        :param str ap_id: the ID of the AP whose info is contained in
            this update event

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_throughputs(self, throughputs, ap_id):
        """Update the throughput on the VAPs provided

        :param dict throughputs: dictionary where the keys are the VAP
            identifiers and values are a tuple of the TX and RX
            throughputs of the VAP
        :param str ap_id: the ID of the AP whose info is contained in
            this update event

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def reset_associated_stations(self):
        """Reset all VAPs to contain no associated stations.

        raises :exc:`NotImplementedError`
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def reset_utilizations(self):
        """Reset the utilization information for all VAPs.

        raises :exc:`NotImplementedError`
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def reset_overload_status(self):
        """Reset the overload status for all VAPs.

        raises :exc:`NotImplementedError`
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def reset_steering_blackout_status(self):
        """Reset the steering blackout status for all APs.

        raises :exc:`NotImplementedError`
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def execute_in_model_context(self, partial):
        """Execute a partial function, either in a sync or async manner.

        :param boolean async: whether to execute the partial function in
            the callers context or another context
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def add_associated_station(self, vap_id, sta_mac, ap_id):
        """Add a new associated station.

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): the MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def del_associated_station(self, sta_mac, ap_id):
        """Remove a station on disassociation event.

        Args:
            sta_mac (str): the MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_associated_station_rssi(self, vap_id, sta_mac, rssi, ap_id):
        """Update the RSSI of an associated station.

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): the MAC address of the associated station
            rssi (int): the RSSI measurement
            ap_id (str): the ID of the AP whose info is contained in
                         this update event

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_associated_station_data_metrics(self, vap_id, sta_mac,
                                               tput, rate, airtime, ap_id):
        """Update the data metrics of a specific station

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): The MAC address of the associated station
            tput (int): the throughput (in Mbps)
            rate (int): the estimated PHY rate (in Mbps)
            airtime (int): the estimated airtime (as a percentage)
            ap_id (str): the ID of the AP whose info is contained in
                         this update event

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_associated_station_activity(self, vap_id, sta_mac, ap_id,
                                           is_active):
        """Update whether a STA is active or not

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): The MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
            is_active (bool): whether this STA is considered active or not

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_station_flags(self, sta_mac, ap_id, **kwargs):
        """Update the flags for a specific station

        Args:
            sta_mac (str): The MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
            kwargs (dict): see :func:`ClientDevice.update_flags` for the valid
                flags

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_overload_status(self, overload_status, ap_id):
        """Update the overload information on the VAPs provided.

        Args:
            overload_status (:obj:`dict`): dictionary where the keys are the VAP
                identifiers and values are the overload status of the VAP
            ap_id (str): the ID of the AP whose info is contained in
                this update event

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_steering_blackout_status(self, blackout_status, ap_id):
        """Update whether steering is disallowed on the AP.

        Args:
            blackout_status (bool): whether steering is disallowed due to
                recent active steering or not
            ap_id (str): the ID of the AP whose info is contained in
                this update event

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_station_steering_status(self, sta_mac, start=False,
                                       abort=False,
                                       assoc_band=BAND_TYPE.BAND_INVALID,
                                       target_band=BAND_TYPE.BAND_INVALID,
                                       assoc_vap=None,
                                       target_vaps=None,
                                       ap_id=None):
        """Update the steering status of a given station

        Use either band or VAP, but not both

        Args:
            sta_mac (str): the MAC address of the station
            start (bool): flag indicating if a steering is started
            abort (bool): flag indicating if a steering is aborted
            assoc_band (:obj:`BAND_TYPE`): the current association band
            target_band (:obj:`BAND_TYPE`): the band to which the
                station is steered
            assoc_vap (:obj:`VirtualAccessPoint`): the current association VAP
            target_vaps (list of :obj:`VirtualAccessPoint`): the VAPs to which the
                station is steered.
            ap_id (str): the AP sent disassociation message

        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def update_paths_from_hactive(self, hactive_table, ap_id, more_fragments):
        raise NotImplementedError("Must be implemented by derived classes")

    def update_paths_from_hdefault(self, hdefault_table, ap_id, more_fragments):
        raise NotImplementedError("Must be implemented by derived classes")

    def update_station_pollution_state(self, sta_mac, ap_id, vap, polluted):
        """Update pollution state for a specific station

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            sta_mac (str): The MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
            vap (:class:`VirtualAccessPoint`):  the VAP on which pollution is detected
            channel_num (int): The channel number on which pollution state changes
            polluted (bool): whether the channel is marked as polluted or not
        Raises:
            :class:`NotImplementedError`: This method must be implemented
                by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    @property
    def device_db(self):
        return self._device_db

    @property
    def rssi_mapper(self):
        return self._rssi_mapper


class ActiveModel(ModelBase):

    """Model implementation that runs its own thread for updates."""

    def __init__(self, device_db):
        """Initialize a new model instance.

        :param :class:`DeviceDB` device_db: the device database to use;
            this can be empty or already populated
        """
        ModelBase.__init__(self, device_db)

        self._cmd_queue = Queue.Queue()

    def activate(self):
        """Start the model instance running.

        Once this is called, all updates to the model data should be
        done via the public API.
        """
        log.info("Activating model")

        # Note that the thread is created here to allow for it to
        # be shut down and then started again. Python does not allow the
        # same thread object to be started twice.
        self._shutdown_requested = False
        self._thread = threading.Thread(target=self._process_events)
        self._thread.start()

    def shutdown(self, blocking=True):
        """Stop the model instance.


        :param boolean blocking: whether to block waiting for the model
            to terminate or not; after this is called, no further model
            updates will be processed
        """
        log.info("Terminating model")
        self._cmd_queue.put(functools.partial(self._signal_shutdown))

        if blocking:
            self._thread.join()

    def wait_for_shutdown(self, timeout):
        """Wait until the thread is shut down.

        Return `True` if the thread was shut down."""
        self._thread.join(timeout=timeout)
        return not self._thread.isAlive()

    def add_associated_station(self, vap_id, sta_mac, ap_id):
        """Add a new associated station.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): the MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
        """
        self._cmd_queue.put(functools.partial(self._add_associated_station,
                                              vap_id, sta_mac, ap_id))

    def _add_associated_station(self, vap_id, sta_mac, ap_id):
        """Perform the addition in the Model thread context."""
        vap = self.device_db.get_vap(vap_id)
        if vap:
            vap.add_associated_station(sta_mac)

            client = self.device_db.get_client_device(sta_mac)
            if client:
                client.update_serving_vap(vap)

            self.notify_observers_associated_stations(ap_id)

    def del_associated_station(self, sta_mac, ap_id):
        """Remove a station on disassociation event.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            sta_mac (str): the MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
        """
        self._cmd_queue.put(functools.partial(self._del_associated_station,
                                              sta_mac, ap_id))

    def _del_associated_station(self, sta_mac, ap_id):
        """Perform the delete in the Model thread context."""
        client = self.device_db.get_client_device(sta_mac)
        if client:
            vap = client.serving_vap
            if vap and vap.ap.ap_id == ap_id:
                # Only perform delete when the message comes from the
                # associated AP, since in the multi-AP scenario, sometimes
                # the disassociation message comes later than association
                # message
                vap.del_associated_station(sta_mac)
                client.update_serving_vap(None)

                self.notify_observers_associated_stations(ap_id)

    def update_associated_station_rssi(self, vap_id, sta_mac, rssi, ap_id):
        """Update the RSSI of an associated station.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): the MAC address of the associated station
            rssi (int): the RSSI measurement
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
        """
        self._cmd_queue.put(functools.partial(self._update_associated_station_rssi,
                                              vap_id, sta_mac, rssi, ap_id))

    def _update_associated_station_rssi(self, vap_id, sta_mac, rssi, ap_id):
        """Perform the update in the Model thread context."""
        vap = self.device_db.get_vap(vap_id)
        rssi_level = self.rssi_mapper.get_level(rssi)
        if vap and vap.update_associated_station_rssi(sta_mac, rssi, rssi_level):
            self.notify_observers_associated_stations(ap_id)

    def update_associated_station_data_metrics(self, vap_id, sta_mac,
                                               tput, rate, airtime, ap_id):
        """Update the data metrics of a specific station

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): The MAC address of the associated station
            tput (int): the throughput (in Mbps)
            rate (int): the estimated PHY rate (in Mbps)
            airtime (int): the estimated airtime (as a percentage)
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
        """
        self._cmd_queue.put(functools.partial(
            self._update_associated_station_data_metrics, vap_id, sta_mac,
            tput, rate, airtime, ap_id)
        )

    def _update_associated_station_data_metrics(self, vap_id, sta_mac,
                                                tput, rate, airtime, ap_id):
        """Perform the update in the Model thread context."""
        vap = self.device_db.get_vap(vap_id)
        if vap and vap.update_associated_station_data_metrics(sta_mac, tput, rate, airtime):
            self.notify_observers_associated_stations(ap_id)

    def update_associated_station_activity(self, vap_id, sta_mac, ap_id,
                                           is_active):
        """Update whether the given station is active.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            vap_id (str): the ID of the VAP that the stations is associated on
            sta_mac (str): The MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
            is_active (bool): whether this STA is considered active or not
        """
        self._cmd_queue.put(functools.partial(
            self._update_associated_station_activity, vap_id, sta_mac,
            ap_id, is_active)
        )

    def _update_associated_station_activity(self, vap_id, sta_mac, ap_id,
                                            is_active):
        """Perform the update in the Model thread context."""
        vap = self.device_db.get_vap(vap_id)
        client = self.device_db.get_client_device(sta_mac)
        if vap and vap.update_associated_station_activity(sta_mac, client,
                                                          is_active):
            self.notify_observers_associated_stations(ap_id)

    def update_station_pollution_state(self, sta_mac, ap_id, vap, polluted):
        """Update pollution state for a specific station

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            sta_mac (str): The MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
            vap (:class:`VirtualAccessPoint`):  the VAP on which pollution is detected
            channel_num (int): The channel number on which pollution state changes
            polluted (bool): whether the channel is marked as polluted or not
        """
        self._cmd_queue.put(functools.partial(
            self._update_station_pollution_state, sta_mac, ap_id, vap, polluted))

    def _update_station_pollution_state(self, sta_mac, ap_id, vap, polluted):
        """Perform the update in the Model thread context"""
        client = self.device_db.get_client_device(sta_mac)
        if client:
            client.update_pollution_state(vap, polluted)
            self.notify_observers_associated_stations(ap_id)

    def update_station_flags(self, sta_mac, ap_id, **kwargs):
        """Update the flags for a specific station

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        Args:
            sta_mac (str): The MAC address of the associated station
            ap_id (str): the ID of the AP whose info is contained in
                         this update event
            kwargs (dict): see :func:`ClientDevice.update_flags` for the valid
                flags
        """
        self._cmd_queue.put(functools.partial(
            self._update_station_flags, sta_mac, ap_id, **kwargs)
        )

    def _update_station_flags(self, sta_mac, ap_id, **kwargs):
        """Perform the update in the Model thread context."""
        client = self.device_db.get_client_device(sta_mac)
        if client:
            client.update_flags(**kwargs)
            self.notify_observers_associated_stations(ap_id)

    def update_station_steering_status(self, sta_mac, start=False,
                                       abort=False,
                                       assoc_band=BAND_TYPE.BAND_INVALID,
                                       target_band=BAND_TYPE.BAND_INVALID,
                                       assoc_vap=None,
                                       target_vaps=None,
                                       ap_id=None):
        """Update the steering status of a given station

        Use either band or VAP, but not both

        This call is asynchronous.

        Args:
            sta_mac (str): the MAC address of the station
            start (bool): flag indicating if a steering is started
            abort (bool): flag indicating if a steering is aborted
            assoc_band (:obj:`BAND_TYPE`): the current association band
            target_band (:obj:`BAND_TYPE`): the band to which the
                station is steered.
            assoc_vap (:obj:`VirtualAccessPoint`): the current association VAP
            target_vaps (list of :obj:`VirtualAccessPoint`): the VAPs to which the
                station is steered.
            ap_id (str): the AP sent disassociation message
        """
        self._cmd_queue.put(functools.partial(self._update_station_steering_status,
                                              sta_mac, start, abort, assoc_band,
                                              target_band, assoc_vap,
                                              target_vaps, ap_id))

    def _update_station_steering_status(self, sta_mac, start=False,
                                        abort=False,
                                        assoc_band=BAND_TYPE.BAND_INVALID,
                                        target_band=BAND_TYPE.BAND_INVALID,
                                        assoc_vap=None,
                                        target_vaps=None,
                                        ap_id=None):
        """Perform the update in the Model thread context.

        Currently this call does not notify any observer, as the observer is
        only interested in knowing if the association is the result of a steering
        and this should have been updated in add_associated_station.
        """
        client = self.device_db.get_client_device(sta_mac)
        if client:
            # Using bands (deprecated)
            if (assoc_band != BAND_TYPE.BAND_INVALID or target_band != BAND_TYPE.BAND_INVALID) \
                    and assoc_vap is None and target_vaps is None:
                if start:
                    # Start steering
                    client.update_steering_status(assoc_band, target_band, None, None)
                elif client.steering_pending_band and assoc_band == BAND_TYPE.BAND_INVALID:
                    # During steering, sta disassociates.
                    # This should happen during post-association steering.
                    client.update_steering_status(None, client.target_band, None, None)
                elif client.steering_pending_band and assoc_band == client.target_band:
                    # During steering, sta associates on target band.
                    # This should happen during pre-association steering.
                    client.update_steering_status(None, client.target_band, None, None)
                elif abort or client.target_band != assoc_band:
                    # Abort steering, or associate on a different band
                    client.update_steering_status(None, None, None, None)
            # Using BSSes
            else:
                if assoc_vap is None and client.serving_vap is not None and \
                        ap_id != client.serving_vap.ap.ap_id:
                    # Disassociation from a different AP, ignore
                    return
                if start:
                    # Start steering
                    client.update_steering_status(None, None, assoc_vap, target_vaps)
                elif client.steering_pending_vap and assoc_vap is None:
                    # During steering, sta disassociates.
                    # This should happen during post-association steering.
                    client.update_steering_status(None, None, None, client.target_vaps)
                elif client.steering_pending_vap and assoc_vap in client.target_vaps:
                    # During steering, sta associates on target BSSes.
                    client.update_steering_status(None, None, None, client.target_vaps)
                elif abort or client.target_vaps is None or \
                        assoc_vap not in client.target_vaps:
                    # Abort steering, or associate on a different channel
                    client.update_steering_status(None, None, None, None)

    def update_associated_stations(self, stations, ap_id):
        """Update the associated stations on the VAPs provided.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :param dict stations: dictionary where the keys are the VAP
            identifiers and each value is a list consisting of tuples,
            with the first element in the tuple being a client device
            MAC address and the second entry being an RSSI in dB
        :param str ap_id: the ID of the AP whose info is contained in
            this update event
        """
        self._cmd_queue.put(functools.partial(self._update_associated_stations,
                                              stations, ap_id))

    def _update_associated_stations(self, stations, ap_id):
        """Perform the update in the Model thread context."""
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating stations with %r", stations)
        for vap_id, stas in stations.items():
            vap = self.device_db.get_vap(vap_id)
            if vap:
                old_stas_set = self._to_sta_set(vap.get_associated_stations())
                vap.update_associated_stations(stas, self.rssi_mapper)

                # As a future optimization, we could potentially only do this
                # if the set of associated stations changed or an RSSI value
                # changed.

                for client_mac, rssi, txrate in stas:
                    client = self.device_db.get_client_device(client_mac)
                    if client:
                        client.update_serving_vap(vap)
                    else:
                        log.debug("Cannot find client device %s", client_mac)

                # Find those that are in the old set but not in the new one
                # and mark their serving VAP as invalid.
                removed_stas = old_stas_set - self._to_sta_set(stas)
                for client_mac in removed_stas:
                    client = self.device_db.get_client_device(client_mac)
                    if client and client.serving_vap == vap:
                        client.update_serving_vap(None)
                    else:
                        log.error("Cannot find client device %s", client_mac)
            else:
                log.error("VAP ID %s not known; cannot update STAs",
                          vap_id, stas)

        self.notify_observers_associated_stations(ap_id)

    def update_utilizations(self, utilizations, ap_id):
        """Update the utilizations information on the VAPs provided.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.update_utilizations
        """
        self._cmd_queue.put(functools.partial(self._update_utilizations,
                                              utilizations, ap_id))

    def _update_utilizations(self, utilizations, ap_id):
        """Perform the update in the Model thread context

        Only notify observer if receiving valid utilization
        """
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating utilizations with %r",
                utilizations)
        updated = False
        for vap_id, utilization in utilizations['data'].items():
            vap = self.device_db.get_vap(vap_id)
            if vap:
                if utilization >= 0 and utilization <= 100:
                    if utilizations['type'] == 'raw':
                        vap.update_utilization_raw(utilization)
                        updated = True
                    elif utilizations['type'] == 'average':
                        vap.update_utilization_avg(utilization)
                        updated = True
                        self._update_aggr_utilization(vap.channel)
                else:
                    log.error("%d is not a valid utilization value" % utilization)
            else:
                log.error("VAP ID %s not known; cannot update utilization",
                          vap_id)

        if updated:
            self.notify_observers_utilizations(ap_id)

    def _update_aggr_utilization(self, channel):
        """Compute aggregated utilization on a given channel and update VAPs

        Only compute aggregated utilization when every VAP on this channel
        gets utilization update within last two seconds.

        :param int channel: the given channel to compute aggregated utilization
        """
        vaps = self.device_db.get_same_channel_vaps(channel)
        aggr_util = reduce(lambda cur_max, vap:
                           None if cur_max is None
                           else vap.compute_utilization_aggr(cur_max),
                           vaps, 0)

        if aggr_util is not None:
            map(lambda vap: vap.update_utilization_aggr(aggr_util), vaps)

    def update_overload_status(self, overload_status, ap_id):
        """Update the overload information on the VAPs provided.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.update_overload_status
        """
        self._cmd_queue.put(functools.partial(self._update_overload_status,
                                              overload_status, ap_id))

    def _update_overload_status(self, overload_status, ap_id):
        """Perform the update in the Model thread context

        Only notify observer if the overload status changed.
        """
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating overload status with %r",
                overload_status)
        updated = False
        for vap_id, overloaded in overload_status.iteritems():
            vap = self.device_db.get_vap(vap_id)
            if vap:
                if vap.overloaded != overloaded:
                    vap.update_overload(overloaded)
                    updated = True
            else:
                log.error("VAP ID %s not known; cannot update overload",
                          vap_id)

        if updated:
            self.notify_observers_utilizations(ap_id)

    def update_steering_blackout_status(self, blackout_status, ap_id):
        """Update whether steering is disallowed on the AP.

        Args:
            blackout_status (bool): whether steering is disallowed due to
                recent active steering or not
            ap_id (str): the ID of the AP whose info is contained in
                this update event
        """
        self._cmd_queue.put(functools.partial(
            self._update_steering_blackout_status, blackout_status, ap_id))

    def _update_steering_blackout_status(self, blackout_status, ap_id):
        """Perform the update in the Model thread context

        Only notify observer if the blackout status changed.
        """
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating blackout status with %r",
                blackout_status)
        updated = False
        ap = self.device_db.get_ap(ap_id)
        if ap:
            if ap.steering_blackout != blackout_status:
                ap.update_steering_blackout(blackout_status)
                updated = True
        else:
            log.error("AP ID %s not known; cannot update blackout status",
                      ap_id)

        if updated:
            # For now, we're reusing the same observer callback since steering
            # blackout is tied to utilization events.
            self.notify_observers_utilizations(ap_id)

    def update_throughputs(self, throughputs, ap_id):
        """Update the throughput information on the VAPs provided.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.update_throughputs
        """
        self._cmd_queue.put(functools.partial(self._update_throughputs,
                                              throughputs, ap_id))

    def _update_throughputs(self, throughputs, ap_id):
        """Perform the update in the Model thread context."""
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating throughputs with %r",
                throughputs)
        for vap_id, throughput in throughputs.items():
            vap = self.device_db.get_vap(vap_id)
            if vap:
                vap.update_throughput(throughput)
            else:
                log.error("VAP ID %s not known; cannot update throughput",
                          vap_id)

        self.notify_observers_throughputs(ap_id)

    def update_paths_from_hactive(self, hactive_table, ap_id, more_fragments):
        """Update all paths from a H-Active table.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.update_paths_from_hactive
        """
        self._cmd_queue.put(functools.partial(self._update_paths_from_hactive,
                                              hactive_table, ap_id, more_fragments))

    def _update_paths_from_hactive(self, hactive_table, ap_id, more_fragments):
        """Perform the update in the Model thread context."""
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating paths from H-Active with %r",
                hactive_table)

        ap = self.device_db.get_ap(ap_id)
        if ap:
            ap.update_paths_from_hactive(hactive_table, self.device_db, more_fragments)
            self.notify_observers_paths(ap_id)
        else:
            log.error("AP ID %s not known; cannot update paths",
                      ap_id)

    def update_paths_from_hdefault(self, hdefault_table, ap_id, more_fragments):
        """Update all paths from a H-Default table.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.update_paths_from_hdefault
        """
        self._cmd_queue.put(functools.partial(self._update_paths_from_hdefault,
                                              hdefault_table, ap_id, more_fragments))

    def _update_paths_from_hdefault(self, hdefault_table, ap_id, more_fragments):
        """Perform the update in the Model thread context."""
        log.log(whcmvc.LOG_LEVEL_DUMP, "Updating paths from H-Default with %r",
                hdefault_table)

        ap = self.device_db.get_ap(ap_id)
        if ap:
            ap.update_paths_from_hdefault(hdefault_table, self.device_db,
                                          more_fragments)
            self.notify_observers_paths(ap_id)
        else:
            log.error("AP ID %s not known; cannot update paths",
                      ap_id)

    def reset_associated_stations(self):
        """Reset the associated station information on all APs.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.reset_associated_stations
        """
        self._cmd_queue.put(functools.partial(self._reset_associated_stations))

    def _reset_associated_stations(self):
        """Set all APs to have no associated stations."""
        # First make sure all client devices no longer have a serving VAP
        # set.
        for dev in self.device_db.get_client_devices():
            dev.update_serving_vap(None)
            dev.update_steering_status(None, None, None, None)
            dev.reset_flags()

        for ap in self.device_db.get_aps():
            for vap in ap.get_vaps():
                vap.update_associated_stations([], self.rssi_mapper)

            self.notify_observers_associated_stations(ap.ap_id)

    def reset_utilizations(self):
        """Reset the utilization information on all APs.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.reset_utilizations
        """
        self._cmd_queue.put(functools.partial(self._reset_utilizations))

    def _reset_utilizations(self):
        """Set the utilization to be invalid for all APs."""
        for ap in self.device_db.get_aps():
            for vap in ap.get_vaps():
                vap.update_utilization_raw(None, 0)
                vap.update_utilization_avg(None, 0)
                vap.update_utilization_aggr(None, 0)

            self.notify_observers_utilizations(ap.ap_id)

    def reset_overload_status(self):
        """Reset the overload status on all APs.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.reset_overload_status
        """
        self._cmd_queue.put(functools.partial(self._reset_overload_status))

    def _reset_overload_status(self):
        """Set the overloaded to be False for all VAPs."""
        for ap in self.device_db.get_aps():
            for vap in ap.get_vaps():
                vap.update_overload(False)

            self.notify_observers_utilizations(ap.ap_id)

    def reset_steering_blackout_status(self):
        """Reset the steeering blackout status on all APs.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.reset_steering_blackout_status
        """
        self._cmd_queue.put(functools.partial(
            self._reset_steering_blackout_status))

    def _reset_steering_blackout_status(self):
        """Set the steering blackout to be False for all APs."""
        for ap in self.device_db.get_aps():
            ap.update_steering_blackout(False)

            self.notify_observers_utilizations(ap.ap_id)

    def reset_throughputs(self):
        """Reset the throughput information on all APs.

        This call is asynchronous. Components that need to be informed
        of updates should register as observers.

        :see ModelBase.reset_throughputs
        """
        self._cmd_queue.put(functools.partial(self._reset_throughputs))

    def _reset_throughputs(self):
        """Set the throughput to be invalid for all APs."""
        for ap in self.device_db.get_aps():
            for vap in ap.get_vaps():
                vap.update_throughput((0, 0))

            self.notify_observers_throughputs(ap.ap_id)

    def _to_sta_set(self, sta_tuples):
        return set(sta[0] for sta in sta_tuples)

    def _process_events(self):
        """Execute partial commands from the queue."""
        while not self._shutdown_requested:
            cmd = self._cmd_queue.get()
            cmd()

        log.info("Model terminated")

    def _signal_shutdown(self):
        """Mark this model as having a shutdown requested."""
        self._shutdown_requested = True


# Exports
__all__ = ['AccessPoint', 'VirtualAccessPoint', 'ClientDevice', 'DeviceDB',
           'ActiveModel', 'TRAFFIC_CLASS_TYPE', 'CLIENT_TYPE', 'AP_TYPE',
           'CHANNEL_GROUP', 'RSSI_LEVEL', 'QUERY_PATHS']
