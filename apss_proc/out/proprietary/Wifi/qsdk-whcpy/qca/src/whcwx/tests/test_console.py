#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import os
import unittest

from whcmvc.model.model_core import ModelBase, RSSIMapper, ServedSTAMetrics, RSSI_LEVEL
from whcmvc.model import config
from whcmvc.controller.controller_core import Controller

from whcdiag.msgs.steerexec import BTMComplianceType, SteeringProhibitType

from whcwx.console import ReadOnlyTextView


class TestConsole(unittest.TestCase):

    def _test_read_only_text_view(self, compact_mode):
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, 'sample_config.yaml')
        )
        self.assert_(device_db)

        rssi_mapper = RSSIMapper()
        model = ModelBase(device_db)

        vap1_id = '2.4g_1'
        vap2_id = '5g_1'
        vap3_id = '2.4g_2'
        vap4_id = '5g_2'

        client1_mac = '00:11:22:33:44:55'
        client2_mac = '00:22:44:66:88:AA'
        client3_mac = '00:33:66:33:66:99'

        # First get handles to the relevant objects.
        vap1 = device_db.get_vap(vap1_id)
        self.assert_(vap1)
        vap2 = device_db.get_vap(vap2_id)
        self.assert_(vap2)
        vap3 = device_db.get_vap(vap3_id)
        self.assert_(vap3)
        vap4 = device_db.get_vap(vap4_id)
        self.assert_(vap4)

        client1 = device_db.get_client_device(client1_mac)
        self.assert_(client1)
        client2 = device_db.get_client_device(client2_mac)
        self.assert_(client2)
        client3 = device_db.get_client_device(client3_mac)
        self.assert_(client3)

        # We do not clear the screen to allow for the unittest output to also
        # be present.

        # The verification of the output is currently manual. We could create
        # golden files, but since the output format may change some before
        # being finalized, I decided to wait on that.
        controller = Controller(model)
        view = ReadOnlyTextView(model, controller, do_clear=False,
                                compact_mode=compact_mode)

        # Display with an empty set of associated stations.
        view.update_associated_stations(model)

        # Now put a single client on one VAP.
        client1_metrics = ServedSTAMetrics().update_rssi(26, RSSI_LEVEL.MODERATE) \
                                            .update_data_metrics(13, 170, 10)
        client1.update_flags(is_active=False, is_dual_band=True,
                             is_unfriendly=True)
        vap1.update_associated_stations([(client1_mac, client1_metrics.rssi,
                                          client1_metrics.rate)], rssi_mapper)
        view.update_associated_stations(model)

        # VAP 1 also got utilization info
        vap1.update_utilization_raw(11)
        view.update_utilizations(model)

        # Two devices on one VAP; third device on a different AP.
        client1_metrics.update_rssi(22, RSSI_LEVEL.LOW)

        client2_metrics = ServedSTAMetrics().update_rssi(33, RSSI_LEVEL.HIGH) \
                                            .update_data_metrics(105, 450, 25)
        client2.update_flags(is_active=True,
                             btm_compliance=BTMComplianceType.BTM_COMPLIANCE_IDLE)

        client3_metrics = ServedSTAMetrics().update_rssi(11, RSSI_LEVEL.WEAK)
        client3.update_flags(prohibit_type=SteeringProhibitType.PROHIBIT_LONG,
                             is_btm_unfriendly=True)
        vap1.update_associated_stations([(client1_mac, client1_metrics.rssi,
                                          client1_metrics.rate),
                                         (client2_mac, client2_metrics.rssi,
                                          client2_metrics.rate)], rssi_mapper)
        vap4.update_associated_stations([(client3_mac, client3_metrics.rssi,
                                          client3_metrics.rate)], rssi_mapper)
        view.update_associated_stations(model)

        # Utilization updates on VAP 1 and 4
        vap1.update_utilization_raw(99)
        vap4.update_utilization_avg(44)
        view.update_utilizations(model)

        # Update steering blackout on AP
        vap1.ap.update_steering_blackout(True)
        view.update_utilizations(model)

        vap1.ap.update_steering_blackout(False)
        view.update_utilizations(model)

        # Now all devices on one VAP but without the original VAP being
        # updated.
        vap3.update_associated_stations([(client1_mac, client1_metrics.rssi,
                                          client1_metrics.rate),
                                         (client2_mac, client2_metrics.rssi,
                                          client2_metrics.rate),
                                         (client3_mac, client3_metrics.rssi,
                                          client3_metrics.rate)], rssi_mapper)
        view.update_associated_stations(model)

        # All VAPs but VAP one get valid utilization info
        vap1.update_utilization_raw(None)
        vap2.update_utilization_raw(22)
        vap3.update_utilization_raw(33)
        vap4.update_utilization_avg(40)
        view.update_utilizations(model)

        # Throughput valid on one VAP
        vap1.update_throughput((20, 53))
        view.update_throughputs(model)

        # Now valid on the 5 GHz VAP as well
        vap2.update_throughput((137.3, 32.9))
        view.update_throughputs(model)

        # Now update the original VAP to remove the devices.
        vap1.update_associated_stations([], rssi_mapper)
        vap4.update_associated_stations([], rssi_mapper)
        view.update_associated_stations(model)

    def test_read_only_text_view(self):
        """Verify basic functioning of the :class:`ReadOnlyTextView`."""
        self._test_read_only_text_view(False)
        self._test_read_only_text_view(True)


if __name__ == '__main__':
    unittest.main()
