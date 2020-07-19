#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Import the view config from a file and create the model elements.

The configuration file format is a simple YAML representation of the
client devices, access points, and virtual access points. This module
exposes a single function, :func:`create_device_db_from_config_data`,
which takes the dictionary representation of the YAML file and
constructs the fully populated :class:`DeviceDB`.
"""

_view_types = {}


def register_view(name, cls):
    """Register a view implementation with the factory.

    :param str name: the name of the view (as specified in the config file)
    :param :class:`ViewBase` cls: the class type for the view
    """
    _view_types[name] = cls


def create_view_from_config_data(config_data, model, controller, view_params):
    """Construct a view object based config data.

    :param dict config_data: the data used to configure the view
    :param :class:`ModelBase` model: the initialized model for the system
    :param dict view_params: the override parameters for the view
    """
    if config_data['type'] in _view_types:
        return _view_types[config_data['type']](model, controller, **view_params)
    else:
        raise ValueError("Invalid view type '%s'" % config_data['type'])
