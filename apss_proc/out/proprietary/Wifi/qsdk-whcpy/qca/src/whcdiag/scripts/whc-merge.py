#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

from whcdiag.io.filesystem import FileReaderRaw, FileWriterRaw


def merge_logs(input_files, output_fh):
    """Read all of the logs out of the file and output them.

    Args:
        input_files (list of str): the names of the input files
        output_fh (:object:`file`): the file handle to use for output
    """
    msg1 = None
    msg2 = None
    with FileReaderRaw(input_files[0]) as reader1:
        with FileReaderRaw(input_files[1]) as reader2:
            if not msg1:
                msg1 = reader1.read_msg()
            if not msg2:
                msg2 = reader2.read_msg()

            while msg1 is not None and msg2 is not None:
                if msg1[0] < msg2[0]:
                    output_fh.output_msg(*msg1)
                    msg1 = reader1.read_msg()
                else:  # second before first, or equal
                    output_fh.output_msg(*msg2)
                    msg2 = reader2.read_msg()

            # Reached the end of one of the files, so just take the remainder
            # of the other one and output it.
            if msg1 is None:
                while msg2 is not None:
                    output_fh.output_msg(*msg2)
                    msg2 = reader2.read_msg()
            elif msg2 is None:
                while msg1 is not None:
                    output_fh.output_msg(*msg1)
                    msg1 = reader1.read_msg()


if __name__ == '__main__':
    import argparse
    import logging

    parser = argparse.ArgumentParser(description="Merge two " +
                                                 "previously captured " +
                                                 "diagnostic logging " +
                                                 "files")

    parser.add_argument('-i', '--input', help='Capture file to merge',
                        required=True, action='append')
    parser.add_argument('-o', '--output', help='Output filename',
                        required=True)

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

    if len(args.input) != 2:
        parser.exit(1, "Exactly 2 input files must be specified")

    with FileWriterRaw(args.output) as output_fh:
        merge_logs(args.input, output_fh)
