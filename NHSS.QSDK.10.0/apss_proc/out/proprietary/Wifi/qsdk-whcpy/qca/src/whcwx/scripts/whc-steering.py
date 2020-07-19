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


def run_steering(config_filename, view_override, view_params, discon,
                 diaglog_file, hyd_dialog_file):
    import whcmvc
    config_data = whcmvc.read_config_file(args.config)

    if view_override:
        config_data['view']['type'] = view_override

    (model, view, controller) = whcmvc.assemble_system(config_data,
                                                       view_params,
                                                       discon,
                                                       diaglog_file,
                                                       hyd_dialog_file)

    whcmvc.run_system(model, view, controller)

if __name__ == '__main__':
    import argparse
    import logging
    import sys
    import os

    parser = argparse.ArgumentParser(description="Whole Home Coverage band steering")
    parser.add_argument('-c', '--config',
                        help='System configuration YAML file', required=True)

    parser.add_argument('--view', choices=['text', 'graphical'],
                        help='Override the view specified in the config')
    parser.add_argument('-d', '--discon', default=False, action='store_true',
                        help='Operate in disconnected mode (no AP connection)')
    parser.add_argument('--diaglog-file', default=None,
                        help='Record the lbd diagnostic logs to the specified ' +
                             'file')
    parser.add_argument('--hyd-diaglog-file', default=None,
                        help='Record the hyd diagnostic logs to the specified ' +
                             'file')

    view_group = parser.add_argument_group('View options')
    view_group.add_argument('--compact',
                            action='store_true', default=False,
                            help='Format the output to fit on a low ' +
                                 'resolution display')

    log_group = parser.add_argument_group('Logging options')
    log_group.add_argument('-v', '--verbose', action='store_true', default=False,
                           help='Enable debug level logging')
    log_group.add_argument('-l', '--logfile', default='debug-log.txt',
                           help='Specify filename to use for debug logging')

    args = parser.parse_args()

    level = logging.INFO
    if args.verbose:
        level = logging.DEBUG

    format = '%(asctime)-15s %(levelname)-8s %(name)-15s %(message)s'
    logging.basicConfig(filename=args.logfile, level=level,
                        format=format)

    from whcwx.gui import GraphicalApp
    view_params = {
        GraphicalApp.COMPACT_MODE_KEYWORD: args.compact,
        GraphicalApp.DISABLE_CONTROLS_KEYWORD: args.discon
    }

    run_steering(args.config, args.view, view_params, args.discon,
                 args.diaglog_file, args.hyd_diaglog_file)
