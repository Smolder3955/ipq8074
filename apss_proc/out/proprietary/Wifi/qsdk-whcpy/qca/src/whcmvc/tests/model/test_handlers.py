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

import unittest
from mock import MagicMock

from whcmvc.model.model_core import ModelBase, DeviceDB
from whcmvc.model.handlers import AssociatedStationsHandlerIface, AssociatedStationsHandler
from whcmvc.model.handlers import UtilizationsHandlerIface, UtilizationsHandler
from whcmvc.model.handlers import ThroughputsHandlerIface, ThroughputsHandler


class TestModelHandlers(unittest.TestCase):

    ap_id = "cap"

    def _create_mock_model(self):
        """Create a new mock model object.

        The :meth:`update_associated_stations` method will be mocked
        out.
        """
        device_db = DeviceDB()
        model = ModelBase(device_db)
        model.update_associated_stations = MagicMock(
            name='update_associated_stations')
        model.update_utilizations = MagicMock(
            name='update_utilizations')
        model.update_throughputs = MagicMock(
            name='update_throughputs')
        return model

    def test_associated_stations_iface(self):
        """Verify functionality of :class:`AssociatedStationsHandlerIface`."""
        handler = AssociatedStationsHandlerIface()
        self.assertRaises(NotImplementedError,
                          handler.update_associated_stations, {}, "fakeap")

    def test_associated_stations(self):
        """Verify functionality of :class:`AssociatedStationsHandler`."""
        model = self._create_mock_model()
        handler = AssociatedStationsHandler(model)

        sta_info = {'vap1': [('00:11:22:33:44:55', 12),
                             ('22:22:22:22:22:22', 33)],
                    'vap2': [('33:00:33:00:33:00', 25),
                             ('44:04:44:04:44:04', 47)]}
        handler.update_associated_stations(sta_info, self.ap_id)
        model.update_associated_stations.assert_called_once_with(sta_info, self.ap_id)

    def test_utilizations_iface(self):
        """Verify functionality of :class:`UtilizationsHandlerIface`."""
        handler = UtilizationsHandlerIface()
        self.assertRaises(NotImplementedError, handler.update_utilizations, {}, "fakeap")

    def test_utilizations(self):
        """Verify functionality of :class:`UtilizationsHandler`."""
        model = self._create_mock_model()
        handler = UtilizationsHandler(model)

        utilizations = {'vap1': 0.123,
                        'vap2': 0.789}
        handler.update_utilizations(utilizations, self.ap_id)
        model.update_utilizations.assert_called_once_with(utilizations, self.ap_id)

    def test_throughputs_iface(self):
        """Verify functionality of :class:`ThroughputsHandlerIface`."""
        handler = ThroughputsHandlerIface()
        self.assertRaises(NotImplementedError, handler.update_throughputs, {}, "fakeap")

    def test_throughputs(self):
        """Verify functionality of :class:`ThroughputsHandler`."""
        model = self._create_mock_model()
        handler = ThroughputsHandler(model)

        throughputs = {'vap1': (21.37, 0.47),
                       'vap2': (0.348, 72.91)}
        handler.update_throughputs(throughputs, self.ap_id)
        model.update_throughputs.assert_called_once_with(throughputs, self.ap_id)

if __name__ == '__main__':
    unittest.main()
