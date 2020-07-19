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
import datetime
import collections

from whcdiag.io.filesystem import FileReaderRaw
import whcdiag.msgs.parser as diagparser
import whcdiag.hydmsgs.parser as hyddiagparser


def print_msg(output_fh, timestamp_str, src_addr, header, payload):
    """Process a single parsed log.

    Args:
        output_fh (:object:`file`): the file handle to use for output
        timestamp_str (str): the time at which the log was generated
            in a human-readable format
        src_addr (str): the address of the node that emitted the
            log
        header (str): log header
        payload (str): log payload
    """
    if header:
        print >>output_fh, "%s\t%s\t%s\t%s" % (timestamp_str, src_addr,
                                               header, payload)
    else:
        print >>output_fh, "%s\t%s\t%s" % (timestamp_str, src_addr, payload)


def output_msg(output_fh, timestamp, src_addr, data, include_date,
               include_header, parser, reassembler):
    """Process a single message read from the file.

    Args:
        output_fh (:object:`file`): the file handle to use for output
        timestamp (float): the time at which the log was generated
            (as returned from :func:`time.time`)
        src_addr (str): the address of the node that emitted the
            log
        data (str): the actual data that was logged
        include_date (bool): whether to include the full date or only
            the time
        include_header (bool): whether to include the diagnostic
            logging header information or not
        parser (:object:`class`): parser to use to parse messages
        reassembler (:object:`class`): used to reassemble fragmented logs
    """
    header, payload = parser.unpack_msg(data)
    
    dt = datetime.datetime.fromtimestamp(timestamp)
    if include_date:
        timestamp_str = dt.isoformat()
    else:
        timestamp_str = dt.time().isoformat()

    if reassembler:
        if include_header:
            object_to_store = (timestamp_str, header, payload)
        else:
            object_to_store = (timestamp_str, None, payload)

        (should_output_current, entries) = reassembler.reassemble(
            header, src_addr, object_to_store)

        # Output any reassembled entries
        if entries:
            for entry in entries:
                print_msg(output_fh, entry[1][0], src_addr, entry[1][1], entry[1][2])

    else:
        should_output_current = True

    # Output the current entry if required
    if should_output_current:
        if include_header:
            print_msg(output_fh, timestamp_str, src_addr, header, payload)
        else:
            print_msg(output_fh, timestamp_str, src_addr, None, payload)


def parse_logs(input_file, output_fh, *args, **kwargs):
    """Read all of the logs out of the file and output them.

    Args:
        input_file (str): the name of the input file
        output_fh (:object:`file`): the file handle to use for output
    """
    with FileReaderRaw(input_file) as reader:
        num_msgs = 0
        msg = reader.read_msg()
        while msg is not None:
            output_msg(output_fh, *msg, **kwargs)
            num_msgs += 1

            msg = reader.read_msg()

        logging.info("Processed %d messages", num_msgs)


if __name__ == '__main__':
    import argparse
    import logging
    import sys
    import os

    parser = argparse.ArgumentParser(description="Parse and display " +
                                                 "previously captured " +
                                                 "diagnostic logs " +
                                                 "to a file")

    parser.add_argument('-i', '--input', help='Capture file to parse',
                        required=True)
    parser.add_argument('-o', '--output',
                        help='Output filename (stdout if not specified)',
                        required=False)
    parser.add_argument('-a', '--append', help='Append to log file',
                        default=False, action='store_true')
    parser.add_argument('--hyd', help='Parse as hyd format',
                        action='store_true')

    format_group = parser.add_argument_group('Output format options')
    format_group.add_argument('-t', '--time', action='store_true',
                              default=False,
                              help='Output only the timestamp (not the date)')
    format_group.add_argument('--no-header', action='store_true',
                              default=False,
                              help='Do not output the diaglog header')

    log_group = parser.add_argument_group('Logging options')
    log_group.add_argument('-v', '--verbose', action='store_true',
                           default=False,
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

    if args.hyd:
        parser = hyddiagparser
        reassembler = hyddiagparser.Reassembler()
    else:
        parser = diagparser
        reassembler = None

    kwargs = { 'include_date' : not args.time,
               'include_header': not args.no_header,
               'parser' : parser,
               'reassembler' : reassembler }
    if args.output is None:
        parse_logs(args.input, sys.stdout, **kwargs)
    else:
        mode = "w+" if args.append else "w"
        with open(args.output, mode) as output_fh:
            parse_logs(args.input, output_fh, **kwargs)
