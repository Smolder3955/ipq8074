#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#


def run_system(model, view, controller):
    """Launch all of the active components and wait for shutdown."""
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


if __name__ == '__main__':

    from twisted.internet import reactor
    from twisted.web.server import Site
    from twisted.web.static import File
    from autobahn.twisted.resource import WebSocketResource

    import argparse
    import logging
    import sys
    import os
    from pkg_resources import resource_filename

    import whcmvc
    import whcviz
    from whcviz.server import SteeringServerWebSocketFactory, SteeringServerProtocol

    # Make sure the pythonutils package is the path
    bindir = os.path.abspath(os.path.dirname(sys.argv[0]))
    sys.path.append(os.path.join(bindir, "..", "..", "lib", "python2.7"))

    # Argument parsing
    parser = argparse.ArgumentParser(description="Whole Home Coverage band steering")
    parser.add_argument('-c', '--config',
                        help='System configuration YAML file', required=True)

    parser.add_argument('-d', '--discon', default=False, action='store_true',
                        help='Operate without a connection to an AP')
    parser.add_argument('--diaglog-file', default=None,
                        help='Record the diagnostic logs to the specified ' +
                             'file')
    parser.add_argument('--hyd-diaglog-file', default=None,
                        help='Record the hyd diagnostic logs to the specified ' +
                             'file')

    ws_group = parser.add_argument_group('WebSocket options')
    ws_group.add_argument('--server',
                          default="localhost",
                          help='The server that will be running this script')
    ws_group.add_argument('--port',
                          default="8080", type=int,
                          help='The port over which the WebSockets will ' +
                               'communicate. Must be the same for both ' +
                               'the client and server')

    log_group = parser.add_argument_group('Logging options')
    log_group.add_argument('-v', '--verbose', action='store_true', default=False,
                           help='Enable debug level logging')
    log_group.add_argument('-l', '--logfile', default='debug-log.txt',
                           help='Specify filename to use for debug logging')
    log_group.add_argument('--stderr', action='store_true', default=False,
                           help='Log to stderr instead of to a file')

    args = parser.parse_args()

    level = logging.INFO
    if args.verbose:
        level = logging.DEBUG

    format = '%(asctime)-15s %(levelname)-8s %(name)-15s %(message)s'
    if args.stderr:
        logging.basicConfig(level=level, format=format)
    else:
        logging.basicConfig(filename=args.logfile, level=level,
                            format=format)

    log = logging.getLogger('whcviz')

    # Reading in config data
    config_data = whcmvc.read_config_file(args.config)

    # Force javascript view type since the rest of the init logic assumes
    # this.
    config_data['view']['type'] = 'javascript'

    # Assemble the system
    view_params = {}
    (model, view, controller) = whcmvc.assemble_system(config_data,
                                                       view_params,
                                                       args.discon,
                                                       args.diaglog_file,
                                                       args.hyd_diaglog_file)

    # Create the websocket
    factory = SteeringServerWebSocketFactory("ws://" + args.server + ":" + str(args.port),
                                             debug=False)
    factory.protocol = SteeringServerProtocol
    factory.setView(view)
    factory.setModel(model, log)
    wsResource = WebSocketResource(factory)

    # Create the webserver - the document root is picked up as a resource of
    # the package. By using pkg_resources, it will work both in develop and
    # installed mode (where the files may be zipped).
    docroot = resource_filename(whcviz.__name__, 'htdocs')
    log.debug("Document root set to %s", docroot)
    webResource = File(docroot)

    # Add the websocket resource to the webserver
    webResource.putChild("websocket", wsResource)
    site = Site(webResource)

    # Create shutdown handler
    def shutdown_handler(signum, frame):
        log.info("Shutting down")
        # Don't want to block in signal handler context.
        model.shutdown(blocking=False)
        reactor.stop()
        print ""

    # Set signal handlers
    import signal
    signal.signal(signal.SIGINT, shutdown_handler)
    signal.signal(signal.SIGTERM, shutdown_handler)

    # Run system on a subthread
    reactor.callInThread(run_system, model, view, controller)

    reactor.listenTCP(args.port, site)
    reactor.run()  # Blocking function call
