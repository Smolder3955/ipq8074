#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Import the view config from a file and create the model elements.

The configuration file format is a simple YAML representation of the
client devices, access points, and virtual access points. This module
exposes a single function, :func:`create_device_db_from_yaml`, which
reads the configuration file provided and constructs the fully
populated :class:`DeviceDB`.
"""


from controller_core import Controller
from console import APConsoleTelnet, APConsoleSSH, APConsoleNop, APConsoleIndirect
from executor import APActiveExecutor
from condition_monitor import AssociatedStationsMonitor, UtilizationsMonitor
from condition_monitor import ThroughputsMonitor, PathMonitor
from diaglog import MsgHandler
from whcmvc.model.handlers import AssociatedStationsHandler, UtilizationsHandler
from whcmvc.model.handlers import ThroughputsHandler
from whcmvc.model.model_core import AP_TYPE, QUERY_PATHS
import whcdiag.msgs.parser as diagparser
import whcdiag.hydmsgs.parser as hyddiagparser
import socket

# Currently shell prompt and port number of LBD debug CLI are both
# hard coded, since it is not expected to be configurable now.
LBD_CLI_PORT = 7787
LBD_CLI_SHELL_PROMPT = "@"
LBD_CLI_LOOPBACK_HOST = "127.0.0.1"


def create_console_from_config_data(ap_id, config_data):
    """Construct and connect the appropriate console object.

    The console object created is based on the type specified.
    """
    console_type = config_data['type']

    # Remove 'type' as it is not expected by the derived types
    config_data.pop('type')
    if console_type.lower() == 'telnet':
        console = APConsoleTelnet(ap_id, **config_data)
        console.connect()
        return console
    elif console_type.lower() == 'ssh':
        console = APConsoleSSH(ap_id, **config_data)
        console.connect()
        return console
    elif console_type.lower() == 'discon':
        console = APConsoleNop(ap_id, **config_data)
        console.connect()
        return console
    else:
        raise ValueError("Invalid console type '%s'" % console_type)


def create_controller_from_config_data(config_data, model, discon,
                                       diaglog_outfile, hyd_diaglog_outfile):
    """Construct a view object based config data.

    :param dict config_data: the data used to configure the view
    :param :class:`ModelBase` model: the fully populated model
    :param bool discon: whether to operate in disconnected mode or not
    :param bool diaglog_outfile: file into which to write received lbd diagnostic
        logs (or None to not write them at all)
    :param bool hyd_diaglog_outfile: file into which to write received hyd diagnostic
        logs (or None to not write them at all)
    """
    controller = Controller(model, discon)

    diaglog_config = {}
    if 'diaglog' in config_data:
        diaglog_config = config_data['diaglog']

    if diaglog_outfile:
        diaglog_config['output_file'] = diaglog_outfile

    hyd_diaglog_config = {}
    if 'hyd_diaglog' in config_data:
        hyd_diaglog_config = config_data['hyd_diaglog']

        if hyd_diaglog_outfile:
            hyd_diaglog_config['output_file'] = hyd_diaglog_outfile

        # Create handler for hyd logs
        reassembler = hyddiagparser.Reassembler()
        hyd_diaglog_handler = MsgHandler(model, hyddiagparser, reassembler,
                                         **hyd_diaglog_config)
    else:
        # Only setup hyd handling if hyd is specifically included from the config
        hyd_diaglog_handler = None

    # Create handler for lbd logs
    diaglog_handler = MsgHandler(model, diagparser, None, **diaglog_config)

    # Now create the console and executor for each of the APs.
    for ap_id, console_config in config_data['ap_consoles'].items():
        whc_cli_port = LBD_CLI_PORT
        if 'cli_port' in console_config:
            whc_cli_port = console_config.pop('cli_port')

        ip_addr = socket.gethostbyname(console_config['hostname'])
        ap = model.device_db.get_ap(ap_id)
        ap.set_ip_addr(ip_addr)

        diaglog_handler.add_ap(ap, ip_addr)
        if hyd_diaglog_handler:
            hyd_diaglog_handler.add_ap(ap, ip_addr)

        console_config_copy = console_config.copy()
        if discon:
            console_config_copy['type'] = 'discon'

        console = create_console_from_config_data(ap_id, console_config_copy)
        executor = APActiveExecutor(ap_id, console)

        # Condition monitors are not relevant when operating in disconnected
        # mode, as we are purely driven by logs.
        if not discon:
            handler = AssociatedStationsHandler(model)
            associated_stations_monitor = AssociatedStationsMonitor(ap, handler)
            executor.add_condition_monitor(associated_stations_monitor)

            handler = UtilizationsHandler(model)
            utilization_monitor = UtilizationsMonitor(ap, handler)
            executor.add_condition_monitor(utilization_monitor)

            handler = ThroughputsHandler(model)
            throughput_monitor = ThroughputsMonitor(ap, handler)
            executor.add_condition_monitor(throughput_monitor)

            # Only start the path monitor if the hyd dialog handler is created,
            # and either configured to:
            #  - query paths from all devices
            #  - query paths from the CAP only and this AP is the CAP)
            if hyd_diaglog_handler and (model.device_db.query_paths == QUERY_PATHS.ALL or
                                        (model.device_db.query_paths == QUERY_PATHS.CAP_ONLY and
                                         ap.ap_type == AP_TYPE.CAP)):
                path_monitor = PathMonitor(ap, hyd_diaglog_handler,
                                           model.device_db.dominant_rate_threshold)
                executor.add_condition_monitor(path_monitor)

        if hyd_diaglog_handler:
            executor.add_diaglog_handler(hyd_diaglog_handler)

        executor.add_diaglog_handler(diaglog_handler)

        # When running in disconnected mode, avoid running any CLI command
        # since the AP is probably not up or not listening on the port
        if not discon:
            whc_cli_indir_console = create_console_from_config_data(
                ap_id, console_config.copy())
            whc_telnet = APConsoleIndirect(ap_id, LBD_CLI_LOOPBACK_HOST, whc_cli_port,
                                           whc_cli_indir_console,
                                           shell_prompt=LBD_CLI_SHELL_PROMPT)
            executor.set_cli_console(whc_telnet)
        else:
            # This will be the nop console, so provide it so that it has
            # something to work with.
            executor.set_cli_console(console)

        controller.ap_executor_db.add_executor(executor)

    return controller
