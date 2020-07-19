#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Module providing common functionality for all views.

The following class is exported:

    :class:`ViewBase`
        base class for all views that provides some common formatting
        and other functionality
"""

import operator
import logging

from whcmvc.model.model_core import ModelBase, AP_TYPE
from whcdiag.msgs.steerexec import BTMComplianceType, SteeringProhibitType

log = logging.getLogger('whcviewbase')
"""The logger used for view base class operations, named ``whcviewbase``."""


class ViewBase(ModelBase.Observer):

    """Base class for all view implementations.

    This class provides common formatting functionality along with
    the appropriate handling of APs to order them in a convenient
    manner.
    """

    def __init__(self, model):
        """Initialize the view base class."""
        ModelBase.Observer.__init__(self)

        # Get the list of devices and put the CAP first followed by the
        # REs in order by their ID.
        cap = None
        range_extenders = {}

        aps = model.device_db.get_aps()
        for ap in aps:
            if ap.ap_type == AP_TYPE.CAP:
                cap = ap
            else:
                range_extenders[ap.ap_id] = ap

        self._ap_ids = [cap.ap_id]
        self._num_client_devices = model.device_db.get_num_client_devices()

        if len(range_extenders) > 0:
            re_ids = range_extenders.keys()
            re_ids.sort()
            self._ap_ids += [range_extenders[ap_id].ap_id for ap_id in re_ids]

        # Build up a sorted representation of the VAP IDs as well.
        self._vap_ids = []
        for ap_id in self._ap_ids:
            ap = model.device_db.get_ap(ap_id)
            vaps = self._get_sorted_vaps(ap)
            self._vap_ids += map(lambda vap: vap.vap_id, vaps)

    def activate(self):
        """Activate the view (nothing to do)."""
        pass

    def shutdown(self):
        """Shutdown the view (nothing to do)."""
        pass

    def update_associated_stations(self, model, ap_id=None):
        """Re-display the information after an associated stations update."""
        raise NotImplementedError("Must be implemented by derived classes")

    def update_utilizations(self, model, ap_id=None):
        """Re-display the information after utilizations update."""
        raise NotImplementedError("Must be implemented by derived classes")

    def update_throughputs(self, model, ap_id=None):
        """Re-display the information after throughput update."""
        raise NotImplementedError("Must be implemented by derived classes")

    def update_paths(self, model, ap_id=None):
        """Re-display the path selection information."""
        raise NotImplementedError("Must be implemented by derived classes")

    @property
    def ap_ids(self):
        return self._ap_ids

    @property
    def vap_ids(self):
        return self._vap_ids

    def _get_sorted_vaps(self, ap):
        vaps = ap.get_vaps()
        vaps.sort(key=operator.attrgetter('vap_id'))
        return vaps

    def _btm_compliance_to_flag(self, is_btm_unfriendly, compliance):
        """Convert the BTM compliance enum value to a single character flag.

        Args:
            is_btm_unfriendly (bool): whether the STA is completely BTM
                unfriendly (no BTM is allowed for idle nor active)
            compliance (:class:`BTMComplianceType`): whether the STA is
                considered BTM compliant or not

        Returns:
            a single character string representing the prohibit state
        """
        if is_btm_unfriendly:
            return 'u'
        else:
            return {
                BTMComplianceType.BTM_COMPLIANCE_IDLE: 'i',
                BTMComplianceType.BTM_COMPLIANCE_ACTIVE_UNFRIENDLY: '@',
                BTMComplianceType.BTM_COMPLIANCE_ACTIVE: 'a'
            }.get(compliance, ' ')

    def _prohibit_to_flag(self, prohibit):
        """Convert the prohibit enum value to a single character flag.

        Args:
            prohibit (:class:`SteeringProhibitType`): whether the prohibit
                timer is running or not

        Returns:
            a single character string representing the prohibit state
        """
        return {
            SteeringProhibitType.PROHIBIT_SHORT: 'p',
            SteeringProhibitType.PROHIBIT_LONG: 'P'
        }.get(prohibit, ' ')

    def _get_sta_flags_str(self, client, compact):
        """Obtain the flags for the STA.

        Args:
            client (:class:`ClientDevice`): the STA information
            compact (bool): whether to generate the compact or complete flags

        Returns:
            string representation of the flags, padded out to the appropriate
            number of characters
        """
        dual_band = '2' if client.is_dual_band else ' '
        active_flag = 'A' if client.is_active else ' '
        steered_flag = 'S' if client.is_steered else ' '

        flags_str = '%s%s%s' % (dual_band, active_flag, steered_flag)

        if not compact:
            btm_flag = self._btm_compliance_to_flag(client.is_btm_unfriendly,
                                                    client.btm_compliance)
            prohibit_flag = self._prohibit_to_flag(client.prohibit_type)
            unfriendly_flag = 'U' if client.is_unfriendly else ' '
            polluted_flag = 'I' if client.is_polluted else ' '

            flags_str += '%s%s%s%s' % (btm_flag, prohibit_flag, unfriendly_flag,
                                       polluted_flag)

        return flags_str

    def _get_ap_header_string(self, ap):
        """Get the string to be displayed in the AP header.

        Args:
            ap (:class:`AccessPoint`): the object representing the AP to
                be displayed

        Returns:
            the string to be displayed as the header
        """
        blackout_status = 'Steering Blackout' if ap.steering_blackout else ''
        return "%-40s  %-40s" % (ap.ap_id, blackout_status)

    def _display_ap_info(self, device_db, ap, fh, compact=False):
        """Print all of the information for one access point."""
        DISPLAY_WIDTH = 82 if compact else 110

        print >>fh, "=" * DISPLAY_WIDTH + "  "
        print >>fh, self._get_ap_header_string(ap)
        print >>fh, "-" * DISPLAY_WIDTH + "  "

        # Print the associated station information.
        vaps = self._get_sorted_vaps(ap)
        for vap in vaps:
            throughput = (vap.tx_throughput + vap.rx_throughput) * 8
            mbps = throughput / 1000000.0

            print >>fh, "%-7sChannel: %-5s\t Medium Utilization: %-5s%-7s%-15s%s" % (
                vap.vap_id,
                vap._channel,
                "%d%%" % vap.utilization if vap.utilization else "NA",
                "(%d%%)" % vap.utilization_aggr.value if vap.utilization_aggr.value else "",
                " (Overloaded)" if vap.overloaded else "",
                " Throughput (Mbps): %-12.4g" % mbps
            )

            stations = vap.get_associated_stations()
            for station in stations:
                client = device_db.get_client_device(station[0])
                if client:
                    rssi = '%s dB' % (station[1].rssi if station[1].rssi is not None else 'NA')
                    tput = '%4s Mbps' % (station[1].tput if station[1].tput is not None else 'NA')
                    rate = '%4s Mbps' % (station[1].rate if station[1].rate is not None else 'NA')
                    airtime = '%3s%%' % \
                        (station[1].airtime if station[1].airtime is not None else 'NA')
                    if not compact:
                        fmt = "%s  %-20s %-20s Tput: %s (%s)    Rate: %s" + \
                              "    RSSI: %s"
                        print >>fh, fmt % (self._get_sta_flags_str(client, compact),
                                           client.name, client.mac_addr,
                                           tput, airtime, rate, rssi)
                    else:
                        print >>fh, "%s  %-20s Tput: %s    RSSI: %s" % \
                            (self._get_sta_flags_str(client, compact),
                             client.name, tput, rssi)

            print >>fh, "\n" * (self._num_client_devices - len(stations) + 1)


__all__ = ['ViewBase']
