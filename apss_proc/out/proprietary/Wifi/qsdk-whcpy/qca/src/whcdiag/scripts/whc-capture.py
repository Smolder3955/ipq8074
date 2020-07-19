#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import time
import signal

from whcdiag.io.network import NetworkReaderRaw
from whcdiag.io.filesystem import FileWriterRaw

class OutputToFileHandler(NetworkReaderRaw.MsgHandler):
    """Simple handler that outputs all messages to a writer."""

    def __init__(self, writer):
        """Create the handler with the writer to use for output.

        Args:
            writer (:object:): object which supports a
                :method:`output_msg`, taking a timestamp, IP address
                string, and the data to be written
        """
        NetworkReaderRaw.MsgHandler.__init__(self)
        self._writer = writer
        self._num_msgs = 0

    def process_msg(self, src_addr, data):
        """Output a single message to the writer.

        Args:
            src_addr (str): the tuple of the IP address and port of the
                device that sent the message
            data (str): the packed string which is the message
        """
        self._num_msgs += 1
        self._writer.output_msg(time.time(), src_addr[0], data)

    @property
    def num_msgs(self):
        """The number of messages processed by the handler."""
        return self._num_msgs

def capture_logs(output_file, ip_addr, port):
    """Continuously capture logs until terminated with Ctrl-C."""
    def sighandler(signum, frame):
        """Terminate the receive loop due to a signal."""
        reader.terminate_receive()

    signal.signal(signal.SIGINT, sighandler)
    signal.signal(signal.SIGTERM, sighandler)

    with NetworkReaderRaw(ip_addr, port) as reader:
        with FileWriterRaw(output_file) as writer:
            handler = OutputToFileHandler(writer)
            reader.receive_msgs(handler)
            logging.info("Processed %d messages", handler.num_msgs)


if __name__ == '__main__':
    import argparse
    import logging
    import sys
    import os

    parser = argparse.ArgumentParser(description="Capture diagnostic logs " +
                                                 "to a file")
    parser.add_argument('-o', '--output', help='Output filename',
                        required=True)
    parser.add_argument('-a', '--append', help='Append to log file',
                        default=False, action='store_true')

    endpoint_group = parser.add_argument_group('Listen options')
    endpoint_group.add_argument('-i', '--ip',
                                help='IP address on which to list for logs',
                                type=str, default='')
    endpoint_group.add_argument('-p', '--port',
                                help='Port on which to listen for logs',
                                type=int,
                                default=NetworkReaderRaw.DEFAULT_PORT)

    log_group = parser.add_argument_group('Logging options')
    log_group.add_argument('-v', '--verbose', action='store_true', default=False,
                           help='Enable debug level logging')
    log_group.add_argument('-l', '--logfile', default=None,
                           help='Specify filename to use for debug logging')

    args = parser.parse_args()

    level = logging.INFO
    if args.verbose:
        level = logging.DEBUG

    format = '%(asctime)-15s %(levelname)-8s %(name)-15s %(message)s'
    logging.basicConfig(filename=args.logfile, level=level,
                        format=format)

    capture_logs(args.output, args.ip, args.port)
