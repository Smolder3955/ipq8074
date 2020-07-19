#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Model-View-Controller implementation of the Whole Home Coverage Demo

The following packages are exported:

    :mod:`whcmvc.model`
        modules for maintaining the current state of the demo system

    :mod:`whcmvc.view`
        modules for graphical and text-based interfaces

    :mod:`whcmvc.controller`
        modules for the core algorithm and target interaction
"""

import yaml
import logging

from whcmvc.model.model_core import ActiveModel
import whcmvc.model.config
import whcmvc.view.config
import whcmvc.controller.config

LOG_LEVEL_DUMP = logging.DEBUG / 2


def read_config_file(config_file):
    """Read the data from the config file into a dictionary.

    Returns the dictionary representation of the config file. This is
    suitable to pass to the other functions in the model, view, and
    controller packages.
    """
    with open(config_file) as infile:
        return yaml.safe_load(infile)


def assemble_system(config_data, view_params, discon, diaglog_outfile,
                    hyd_diaglog_outfile=None):
    """Construct the model, view, and controller based on the config.

    :param str config_file: path to the configuration file to use
    :param dict view_params: override parameters to provide to the view
        config handler
    :param bool discon: whether to operate in disconnected mode or not
    :param bool diaglog_outfile: file into which to write received lbd diagnostic
        logs (or None to not write them at all)
    :param bool hyd_diaglog_outfile: file into which to write received hyd diagnostic
        logs (or None to not write them at all)
    """
    # Create the model first.
    device_db = whcmvc.model.config.create_device_db_from_config_data(
        config_data['model'])
    model = ActiveModel(device_db)

    whcmvc.model.config.update_rssi_levels_from_config_data(
        config_data['model'], model)

    # Create the controller next.
    controller = whcmvc.controller.config.create_controller_from_config_data(
        config_data['controller'], model, discon, diaglog_outfile, hyd_diaglog_outfile)

    # Finally create the view.
    view = whcmvc.view.config.create_view_from_config_data(
        config_data['view'], model, controller, view_params)

    model.register_observer(view)

    return (model, view, controller)


def run_system(model, view, controller):
    """Launch all of the active components and wait for shutdown."""
    # Set up SIGINT (Ctrl-C) and SIGTERM to terminate the model.
    def shutdown_handler(signum, frame):
        # Don't want to block in signal handler context.
        model.shutdown(blocking=False)

    import signal
    signal.signal(signal.SIGINT, shutdown_handler)
    signal.signal(signal.SIGTERM, shutdown_handler)

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


__all__ = ['model', 'controller', 'view', 'LOG_LEVEL_DUMP',
           'read_config_file', 'assemble_system', 'run_system']
