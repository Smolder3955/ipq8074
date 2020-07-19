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

import unittest
import time
import os
import os.path

from whcdiag.io.filesystem import FileWriterRaw, FileReaderRaw


class TestFileRawIO(unittest.TestCase):

    def test_nop_output(self):
        """Verify no failures when no filename is provided.

        This effectively configures the writer as a null object."""
        # Write some records to a file first.
        with FileWriterRaw(None) as writer:
            record = (time.time(), '192.168.0.1', 'log1')
            writer.output_msg(record[0], record[1], record[2])

            record = (time.time(), '192.168.0.8', 'log #2')
            writer.output_msg(record[0], record[1], record[2])

            record = (time.time(), '192.168.0.4', 'longer log #3')
            writer.output_msg(record[0], record[1], record[2])

    def test_basic_input_output(self):
        """Verify the basic functionality of writing and reading logs."""
        outfile = 'log1.bin'
        if os.path.exists(outfile):
            os.unlink(outfile)  # clean up a previous run

        # Write some records to a file first.
        records = []
        with FileWriterRaw(outfile) as writer:
            record = (time.time(), '192.168.0.1', 'log1')
            writer.output_msg(record[0], record[1], record[2])
            records.append(record)

            record = (time.time(), '192.168.0.8', 'log #2')
            writer.output_msg(record[0], record[1], record[2])
            records.append(record)

            record = (time.time(), '192.168.0.4', 'longer log #3')
            writer.output_msg(record[0], record[1], record[2])
            records.append(record)

        self.assert_(os.path.exists(outfile))

        # Now use the file reader to make sure it can obtain the same
        # data.
        with FileReaderRaw(outfile) as reader:
            for golden_record in records:
                record = reader.read_msg()
                self.assertEquals(golden_record, record)

            self.assertEquals(None, reader.read_msg())

    def test_append_output(self):
        """Verify the same functionality but appending to the file."""
        outfile = 'log2.bin'
        if os.path.exists(outfile):
            os.unlink(outfile)  # clean up a previous run

        # Write one record first.
        records = []
        with FileWriterRaw(outfile) as writer:
            record = (time.time(), '192.168.1.100', 'truncate log')
            writer.output_msg(record[0], record[1], record[2])
            records.append(record)

        self.assert_(os.path.exists(outfile))

        # Now do another round of writes but in append mode.
        with FileWriterRaw(outfile, append=True) as writer:
            record = (time.time(), '192.168.2.70', 'append #1')
            writer.output_msg(record[0], record[1], record[2])
            records.append(record)

            record = (time.time(), '192.168.3.215', 'append #2')
            writer.output_msg(record[0], record[1], record[2])
            records.append(record)

        # Now read the result, making sure both the original and the
        # subsequent two logs are there.
        with FileReaderRaw(outfile) as reader:
            for golden_record in records:
                record = reader.read_msg()
                self.assertEquals(golden_record, record)

            self.assertEquals(None, reader.read_msg())

if __name__ == '__main__':
    unittest.main()
