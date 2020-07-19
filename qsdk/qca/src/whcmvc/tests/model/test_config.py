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
import os

from whcdiag.constants import BAND_TYPE

import whcmvc
from whcmvc.model.model_core import RSSIMapper
from whcmvc.model.model_core import DeviceDB, ModelBase
from whcmvc.model.model_core import CLIENT_TYPE, TRAFFIC_CLASS_TYPE, AP_TYPE
from whcmvc.model.model_core import CHANNEL_GROUP
from whcmvc.model import config


class TestConfig(unittest.TestCase):

    def test_config(self):
        """Verify basic functioning of the config import."""
        # Note that the values below need to be kept in sync with this sample
        # config file.
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, 'sample_config_2ap.yaml'))
        self.assert_(device_db)

        ###############################################################
        # Verify the ClientDevice objects
        ###############################################################
        mac = '00:11:22:33:44:55'
        client = device_db.get_client_device(mac)
        self.assert_(client)
        self.assertEquals('client1', client.name)
        self.assertEquals(mac, client.mac_addr)
        self.assertEquals(TRAFFIC_CLASS_TYPE.STREAMING, client.traffic_class)
        self.assertEquals(CLIENT_TYPE.PHONE, client.client_type)
        self.assert_(not client.serving_vap)

        mac = '11:11:11:00:00:09'
        client = device_db.get_client_device(mac)
        self.assert_(client)
        self.assertEquals('client2', client.name)
        self.assertEquals(mac, client.mac_addr)
        self.assertEquals(TRAFFIC_CLASS_TYPE.TELEPHONY, client.traffic_class)
        self.assertEquals(CLIENT_TYPE.TABLET, client.client_type)
        self.assert_(not client.serving_vap)

        mac = '00:33:66:33:66:99'
        client = device_db.get_client_device(mac)
        self.assert_(client)
        self.assertEquals('client3', client.name)
        self.assertEquals(mac, client.mac_addr)
        self.assertEquals(TRAFFIC_CLASS_TYPE.INTERACTIVE, client.traffic_class)
        self.assertEquals(CLIENT_TYPE.LAPTOP, client.client_type)
        self.assert_(not client.serving_vap)

        mac = '00:44:88:CC:00:44'
        client = device_db.get_client_device(mac)
        self.assert_(client)
        self.assertEquals('client4', client.name)
        self.assertEquals(mac.lower(), client.mac_addr)
        self.assertEquals(TRAFFIC_CLASS_TYPE.INTERACTIVE, client.traffic_class)
        self.assertEquals(CLIENT_TYPE.PC, client.client_type)
        self.assert_(not client.serving_vap)

        mac = '00:55:AA:FF:44:99'
        client = device_db.get_client_device(mac)
        self.assert_(client)
        self.assertEquals('client5', client.name)
        self.assertEquals(mac.lower(), client.mac_addr)
        self.assertEquals(TRAFFIC_CLASS_TYPE.INTERACTIVE, client.traffic_class)
        self.assertEquals(CLIENT_TYPE.GENERIC, client.client_type)
        self.assert_(not client.serving_vap)

        ###############################################################
        # Verify the AccessPoint objects
        ###############################################################
        ap1_id = 'ap1'
        ap1 = device_db.get_ap(ap1_id)
        self.assert_(ap1)
        self.assertEquals(ap1_id, ap1.ap_id)
        self.assertEquals(AP_TYPE.CAP, ap1.ap_type)

        ap2_id = 're1'
        ap2 = device_db.get_ap(ap2_id)
        self.assert_(ap2)
        self.assertEquals(ap2_id, ap2.ap_id)
        self.assertEquals(AP_TYPE.RE, ap2.ap_type)

        ###############################################################
        # Verify the VirtualAccessPoint objects
        ###############################################################
        vap1_id = '2.4g_1'
        vap1 = device_db.get_vap(vap1_id)
        self.assert_(vap1)
        self.assertEquals(vap1_id, vap1.vap_id)
        self.assertEquals(ap1, vap1.ap)
        self.assertEquals(BAND_TYPE.BAND_24G, vap1.band)
        self.assertEquals(7, vap1.channel)
        self.assertEquals(CHANNEL_GROUP.CG_24G, vap1.channel_group)
        self.assertEquals(75, vap1.overload_threshold)
        self.assertEquals('ath0', vap1.iface_name)
        self.assertEquals('wifi0', vap1.radio_iface)
        self.assertEquals(None, vap1.utilization)
        self.assertEquals([], vap1.get_associated_stations())

        vap2_id = '5g_1'
        vap2 = device_db.get_vap(vap2_id)
        self.assert_(vap2)
        self.assertEquals(vap2_id, vap2.vap_id)
        self.assertEquals(ap1, vap2.ap)
        self.assertEquals(BAND_TYPE.BAND_5G, vap2.band)
        self.assertEquals(100, vap2.channel)
        self.assertEquals(CHANNEL_GROUP.CG_5GH, vap2.channel_group)
        self.assertEquals(80, vap2.overload_threshold)
        self.assertEquals('ath1', vap2.iface_name)
        self.assertEquals('wifi1', vap2.radio_iface)
        self.assertEquals(None, vap2.utilization)
        self.assertEquals([], vap2.get_associated_stations())

        vap3_id = '2.4g_2'
        vap3 = device_db.get_vap(vap3_id)
        self.assert_(vap3)
        self.assertEquals(vap3_id, vap3.vap_id)
        self.assertEquals(ap1, vap3.ap)
        self.assertEquals(BAND_TYPE.BAND_24G, vap3.band)
        self.assertEquals(6, vap3.channel)
        self.assertEquals(CHANNEL_GROUP.CG_24G, vap3.channel_group)
        self.assertEquals(85, vap3.overload_threshold)
        self.assertEquals('ath2', vap3.iface_name)
        self.assertEquals('wifi0', vap3.radio_iface)
        self.assertEquals(None, vap3.utilization)
        self.assertEquals([], vap3.get_associated_stations())

        vap4_id = '5g_2'
        vap4 = device_db.get_vap(vap4_id)
        self.assert_(vap4)
        self.assertEquals(vap4_id, vap4.vap_id)
        self.assertEquals(ap1, vap4.ap)
        self.assertEquals(BAND_TYPE.BAND_5G, vap4.band)
        self.assertEquals(150, vap4.channel)
        self.assertEquals(CHANNEL_GROUP.CG_5GH, vap4.channel_group)
        self.assertEquals(70, vap4.overload_threshold)
        self.assertEquals('ath3', vap4.iface_name)
        self.assertEquals('wifi1', vap4.radio_iface)
        self.assertEquals(None, vap4.utilization)
        self.assertEquals([], vap4.get_associated_stations())

        # Verify the VAPs are maintained in the APs properly.
        self.assert_(vap1 in ap1.get_vaps())
        self.assert_(vap2 in ap1.get_vaps())
        self.assert_(vap3 in ap1.get_vaps())
        self.assert_(vap4 in ap1.get_vaps())

        # And the VAPs on the RE
        vap5_id = 're2.4g'
        vap5 = device_db.get_vap(vap5_id)
        self.assert_(vap5)
        self.assertEquals(vap5_id, vap5.vap_id)
        self.assertEquals(ap2, vap5.ap)
        self.assertEquals(BAND_TYPE.BAND_24G, vap5.band)
        self.assertEquals(7, vap1.channel)
        self.assertEquals(CHANNEL_GROUP.CG_24G, vap5.channel_group)
        self.assertEquals(65, vap5.overload_threshold)
        self.assertEquals('ath1', vap5.iface_name)
        self.assertEquals('wifi1', vap5.radio_iface)
        self.assertEquals(None, vap5.utilization)
        self.assertEquals([], vap5.get_associated_stations())

        vap6_id = 're5g'
        vap6 = device_db.get_vap(vap6_id)
        self.assert_(vap6)
        self.assertEquals(vap6_id, vap6.vap_id)
        self.assertEquals(ap2, vap6.ap)
        self.assertEquals(BAND_TYPE.BAND_5G, vap6.band)
        self.assertEquals(36, vap6.channel)
        self.assertEquals(CHANNEL_GROUP.CG_5GL, vap6.channel_group)
        self.assertEquals(60, vap6.overload_threshold)
        self.assertEquals('ath0', vap6.iface_name)
        self.assertEquals('wifi0', vap6.radio_iface)
        self.assertEquals(None, vap6.utilization)
        self.assertEquals([], vap6.get_associated_stations())

        ###############################################################
        # Validate error cases with unexpected values in the config
        # file.
        ###############################################################
        self.assertRaises(Exception, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'more_than_one_ap.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_band.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_channel_group.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_traffic_type.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_client_type.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_client_mac.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_ap_type.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_ap_mac.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'invalid_query_paths.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_channel.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_channel_group.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_overload_threshold.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_iface.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_radio.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_client_mac.yaml'))
        self.assertRaises(ValueError, config.create_device_db_from_yaml,
                          os.path.join(test_dir, 'missing_ap_mac.yaml'))

    def test_config_rssi_levels(self):
        """Verify basic functioning of the RSSI level update."""
        device_db = DeviceDB()
        model = ModelBase(device_db)

        # Note that the values below need to be kept in sync with this sample
        # config file.
        test_dir = os.path.dirname(__file__)
        data = whcmvc.read_config_file(
            os.path.join(test_dir, 'rssi_no_overrides.yaml'))

        config.update_rssi_levels_from_config_data(data['model'], model)
        self.assertEquals(RSSIMapper.HIGH_RSSI_THRESH, model.rssi_mapper.rssi_high)
        self.assertEquals(RSSIMapper.MODERATE_RSSI_THRESH, model.rssi_mapper.rssi_moderate)
        self.assertEquals(RSSIMapper.LOW_RSSI_THRESH, model.rssi_mapper.rssi_low)

        data = whcmvc.read_config_file(
            os.path.join(test_dir, 'rssi_override_high.yaml'))

        config.update_rssi_levels_from_config_data(data['model'], model)
        self.assertEquals(40, model.rssi_mapper.rssi_high)
        self.assertEquals(RSSIMapper.MODERATE_RSSI_THRESH, model.rssi_mapper.rssi_moderate)
        self.assertEquals(RSSIMapper.LOW_RSSI_THRESH, model.rssi_mapper.rssi_low)

        data = whcmvc.read_config_file(
            os.path.join(test_dir, 'rssi_override_all.yaml'))

        config.update_rssi_levels_from_config_data(data['model'], model)
        self.assertEquals(39, model.rssi_mapper.rssi_high)
        self.assertEquals(33, model.rssi_mapper.rssi_moderate)
        self.assertEquals(25, model.rssi_mapper.rssi_low)

        # Error case with override values that are inconsistent.
        data = whcmvc.read_config_file(
            os.path.join(test_dir, 'rssi_override_inconsistent.yaml'))
        self.assertRaises(ValueError, config.update_rssi_levels_from_config_data,
                          data['model'], model)
        self.assertEquals(39, model.rssi_mapper.rssi_high)
        self.assertEquals(33, model.rssi_mapper.rssi_moderate)
        self.assertEquals(25, model.rssi_mapper.rssi_low)

        # Invalid key is also rejected
        data = whcmvc.read_config_file(
            os.path.join(test_dir, 'rssi_override_invalid.yaml'))
        self.assertRaises(ValueError, config.update_rssi_levels_from_config_data,
                          data['model'], model)
        self.assertEquals(39, model.rssi_mapper.rssi_high)
        self.assertEquals(33, model.rssi_mapper.rssi_moderate)
        self.assertEquals(25, model.rssi_mapper.rssi_low)


if __name__ == '__main__':
    unittest.main()
