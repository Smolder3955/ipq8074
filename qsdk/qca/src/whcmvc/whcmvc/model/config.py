#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Import the system config from a file and create the model elements.

The configuration file format is a simple YAML representation of the
client devices, access points, and virtual access points. This module
exposes a single function, :func:`create_device_db_from_yaml`, which
reads the configuration file provided and constructs the fully
populated :class:`DeviceDB`.
"""

import whcmvc  # for config helper
from model_core import *
from whcdiag.constants import BAND_TYPE


def get_traffic_class_enum(purpose, traffic_class_str):
    """Convert the traffic class string to its enum representation."""
    try:
        return TRAFFIC_CLASS_TYPE[str(traffic_class_str).upper()]
    except KeyError:
        raise ValueError("Unexpected traffic class '%s' for %s" %
                         (traffic_class_str, purpose))


def get_band_enum(desc, band_spec):
    """Convert the band string to its enum representation."""
    if band_spec == 2 or band_spec == 2.4:
        return BAND_TYPE.BAND_24G
    elif band_spec == 5:
        return BAND_TYPE.BAND_5G
    else:
        raise ValueError("Unexpected band '%r'%s" %
                         (band_spec, desc))


def get_channel_group_enum(vap_id, group_spec):
    """Convert the channel group to its enum representation."""
    try:
        return CHANNEL_GROUP['CG_' + str(group_spec).upper()]
    except KeyError:
        raise ValueError("Unexpected channel group '%s' for VAP '%s'" %
                         (group_spec, vap_id))


def get_client_type_enum(client_name, type_spec):
    """Convert the client type to its enum representation."""
    try:
        return CLIENT_TYPE[str(type_spec).upper()]
    except KeyError:
        raise ValueError("Unexpected client type '%s' for client '%s'" %
                         (type_spec, client_name))


def get_ap_type_enum(ap_name, type_spec):
    """Convert the AP type to its enum representation."""
    try:
        return AP_TYPE[str(type_spec).upper()]
    except KeyError:
        raise ValueError("Unexpected type '%s' for AP '%s'" %
                         (type_spec, ap_name))


def get_rssi_level_enum(level_spec):
    """Convert the RSSI level to its enum representation."""
    try:
        return RSSI_LEVEL[str(level_spec).upper()]
    except KeyError:
        raise ValueError("Unexpected RSSI level '%s'" % level_spec)


def get_query_paths_enum(query_paths_spec):
    """Convert the query paths variable to its enum representation."""
    try:
        return QUERY_PATHS[str(query_paths_spec).upper()]
    except KeyError:
        raise ValueError("Unexpected query paths variable '%s'" % query_paths_spec)


def create_client_devices(device_db, device_specs):
    """Create :class:`ClientDevice` objects based on the data provided.

    The devices are added to the database after creation.

    :param :class:`DeviceDB` device_db: The database into which to put
        the created objects.
    :param dict device_specs: The dictionary containing the device
        specifications. The key is the name of the device (a human
        readable description) and the value is another dictionary with
        the ``traffic_type`` and ``mac_addr`` keys.
    """
    for name, attrs in device_specs.iteritems():
        kwargs = {}
        if 'client_type' in attrs:
            kwargs['client_type'] = get_client_type_enum(name, attrs['client_type'])

        kwargs['traffic_class'] = TRAFFIC_CLASS_TYPE.INTERACTIVE
        if 'traffic_type' in attrs:
            kwargs['traffic_class'] = get_traffic_class_enum(name, attrs['traffic_type'])

        if 'mac_addr' not in attrs:
            raise ValueError("mac_addr missing for client '%s'" % (name))
        if not isinstance(attrs['mac_addr'], str):
            raise ValueError("mac_addr for client '%s' " % (name) +
                             "is not a string; add quotes around it")

        client = ClientDevice(name, attrs['mac_addr'], **kwargs)
        device_db.add_client_device(client)


def create_virtual_access_points(ap, vap_specs):
    """Create :class:`VirtualAccessPoint` objects based on the data provided.

    The devices are added to the AP after creation.

    :param :class:`DeviceDB` device_db: The database into which to put
        the created objects.
    :param dict vap_specs: The dictionary containing the virtual access
        point specifications. The parameters are ``iface_name``, which
        contains the VAP's interface name (eg. ath0), and ``band``,
        which should be either '2.4' or '5'.
    """
    for name, attrs in vap_specs.iteritems():
        band = get_band_enum(" for VAP '%s'" % name, attrs['band'])

        if 'iface_name' not in attrs:
            raise ValueError("iface_name missing for VAP '%s' in AP '%s'" %
                             (name, ap.ap_id))

        if 'radio_iface' not in attrs:
            raise ValueError("radio_iface missing for VAP '%s' in AP '%s'" %
                             (name, ap.ap_id))

        if 'channel' not in attrs:
            raise ValueError("channel missing for VAP '%s' in AP '%s'" %
                             (name, ap.ap_id))

        if 'channel_group' not in attrs:
            raise ValueError("channel_group missing for VAP '%s' in AP '%s'" %
                             (name, ap.ap_id))

        if 'overload_threshold' not in attrs:
            raise ValueError("overload_threshold missing for VAP '%s' in AP '%s'" %
                             (name, ap.ap_id))

        channel_group = get_channel_group_enum(name, attrs['channel_group'])

        # This adds the VAP to the AP as a side effect.
        VirtualAccessPoint(ap, name, band, attrs['iface_name'],
                           attrs['radio_iface'], attrs['channel'],
                           channel_group, attrs['overload_threshold'])


def create_access_points(device_db, ap_specs):
    """Create :class:`AccessPoint` objects based on the data provided.

    The devices are added to the database after creation.

    :param :class:`DeviceDB` device_db: The database into which to put
        the created objects.
    :param dict ap_specs: The dictionary containing the access point
        specifications. Currently the only item is a dictionary of VAPs
        under the ``vaps`` key.
    """
    for name, attrs in ap_specs.iteritems():
        ap_type = AP_TYPE.CAP
        if 'type' in attrs:
            ap_type = get_ap_type_enum(name, attrs['type'])
        if 'mac_addr' not in attrs:
            raise ValueError("mac_addr missing for AP '%s'" % (name))
        if isinstance(attrs['mac_addr'], int):
            raise ValueError("mac_addr is being interpretted as an integer; " +
                             "add quotes around it to force it to be a string")

        ap = AccessPoint(name, ap_type, attrs['mac_addr'])
        create_virtual_access_points(ap, attrs['vaps'])
        device_db.add_ap(ap)


def create_device_db_from_config_data(data):
    """Construct and return a :class:`DeviceDB` object from the data."""
    config = {}
    if 'configuration' in data:
        config = data['configuration']
        if 'query_paths' in config:
            config['query_paths'] = get_query_paths_enum(config['query_paths'])

    device_db = DeviceDB(**config)

    create_client_devices(device_db, data['client_devices'])

    # This also creates the VAPs contained with the APs.
    create_access_points(device_db, data['access_points'])

    return device_db


def create_device_db_from_yaml(filename):
    """Construct a :class:`DeviceDB` object based on the file.

    :param string filename: The YAML file containing the definitions
        of the system elements.
    """
    data = whcmvc.read_config_file(filename)
    return create_device_db_from_config_data(data['model'])


def update_rssi_levels_from_config_data(data, model):
    """Update the :class:`RSSIMapper` within the model based on config data.

    This allows the RSSI thresholds that define the various levels to be
    overridden by the config file.

    Args:
        data (dict): the model section of the config file
        model (:obj:`ModelBase`): the current model instance
    """
    rssi_levels_data = data.get('rssi_levels', {})
    rssi_levels = {}
    for key, val in rssi_levels_data.items():
        rssi_levels[get_rssi_level_enum(key)] = val
    model.rssi_mapper.update_levels(rssi_levels)
