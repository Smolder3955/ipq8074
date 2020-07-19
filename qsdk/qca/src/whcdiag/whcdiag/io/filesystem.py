#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2014-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

"""Facilities for writing/reading diagnostic logging data from the filesystem.

The following classes are exported:

    :class:`FileWriterRaw`: write raw log records to a file, along with
        host timestamp and size information
    :class:`FileReaderRaw`: read raw log records from a file, along with
        host timestamp and size information
"""

import struct
import socket

HEADER_OUTPUT_FORMAT = struct.Struct('<d4sH')


class FileWriterRaw(object):
    """Simple writer of diagnostic logging data to the filesystem.

    This writer will write individual log records to a file, in a format
    suitable for later reading by :class:`FileReaderRaw`. It does not do
    any parsing of the data.
    """

    def __init__(self, filename, append=False):
        """Initialize the writer with the output filename.

        Args:
            filename (str): the name of the file to which to write the logs
                (or None to make all operations a nop)
            append (bool): whether to append to or truncate the file
        """
        self._filename = filename
        self._append = append

    def output_msg(self, timestamp, ip_addr, msg):
        """Output a single message to the file.

        Args:
            msg (str): the packed string to output
        """
        if self._filename:
            ip_addr_str = socket.inet_aton(ip_addr)
            self._file.write(HEADER_OUTPUT_FORMAT.pack(timestamp, ip_addr_str,
                                                       len(msg)))
            self._file.write(msg)

    def __enter__(self):
        """Entry function which opens the file in the appropriate mode."""
        if self._filename:
            mode = "a" if self._append else "w"
            self._file = open(self._filename, mode)
            return self
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """Exit function which properly closes the file."""
        if self._filename:
            self._file.close()


class FileReaderRaw(object):
    """Simple reader of diagnostic logging data from a file.

    This reader will read until the end of the file and provide a
    callback for each record read. It does not do any parsing of th e
    data.
    """

    def __init__(self, filename):
        """Initialize the reader with the input filename.

        Args:
            filename (str): the name of the file to read from
        """
        self._filename = filename

    def read_msg(self):
        """Read a single message from the file and pass it to the handler.

        Precondition: The file was already opened by using this object in
        a with statement.

        Returns: tuple consisting of the following
            timestamp (float): the time at which the log was generated
                (as returned from :func:`time.time`)
            src_addr (str): the address of the node that emitted the
                log
            data (str): the actual data that was logged

            None is returned if the end of the file has been reached.
        """
        hdr = self._file.read(HEADER_OUTPUT_FORMAT.size)
        if hdr == '':
            # Hit the end of the file.
            return None

        (timestamp, src_addr_str, data_len) = HEADER_OUTPUT_FORMAT.unpack(hdr)
        src_addr = socket.inet_ntoa(src_addr_str)
        return (timestamp, src_addr, self._file.read(data_len))

    def __enter__(self):
        """Entry function which opens the file for reading."""
        self._file = open(self._filename, "r")
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """Exit function which properly closes the file."""
        self._file.close()
