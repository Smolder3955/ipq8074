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

import unittest
import getpass
import time

from whcmvc.controller.console import \
    APConsoleBase, APConsoleSSH, APConsoleTelnet, APConsoleNop, APConsoleIndirect


class TestAPConsole(unittest.TestCase):

    def test_base(self):
        """Verify the basic functionality of :class:`APConsoleBase`."""
        console = APConsoleBase('1')
        self.assertRaises(NotImplementedError, console.connect)
        self.assertRaises(NotImplementedError, console.disconnect)
        self.assertRaises(NotImplementedError, console.run_cmd,
                          'echo "Hello World"')

        # After console shutdown, any command will return empty result
        console.shutdown()
        output = console.run_cmd('echo "Hello World"')
        self.assertEquals(0, len(output))
        output = console.run_cmd('s0me RAndom 3tr!ng')
        self.assertEquals(0, len(output))

    @unittest.skip("Requires proper ssh setup")
    def test_ssh_console(self):
        """Verify the basic functionality of :class:`APConsoleSSH`."""
        # The shell prompt used here is consistent with the default zsh prompt
        # configuration in our images.
        console = APConsoleSSH('dev-dummy', 'localhost',
                               username=getpass.getuser(), password=None,
                               shell_prompt=']%')
        console.connect()

        # Simple command for which it is easy to predict the output.
        output = console.run_cmd('echo "Hello World"')
        self.assertEquals("Hello World", output[-1])

        # Verify that we can also run commands that require shell operations
        # (such as pipes).
        output = console.run_cmd('echo -e "multi\nline\noutput" | grep --color=never out')
        self.assertEquals("output", output[-1])

        # benchmark: Confirm that a simple command takes less than 10 ms
        #            to execute.
        iterations = 1000
        start = time.time()
        for i in xrange(iterations):
            output = console.run_cmd('echo "Hello World"')
        end = time.time()
        cmd_time = (end - start) / iterations
        self.assertLess(cmd_time, 0.010)

        print "[APConsoleSSH] Estimated time per command: %r" % cmd_time

        console.disconnect()

    @unittest.skip("Requires an AP device to be connected with default IP")
    def test_telnet_console(self):
        """Verify the basic functionality of :class:`APConsoleTelnet`."""
        console = APConsoleTelnet('dev-dummy', '192.168.1.2', username='root',
                                  password='5up')
        console.connect()

        # Simple command for which it is easy to predict the output.
        output = console.run_cmd('echo "Hello World"')
        self.assertEquals("Hello World", output[-1])

        # Verify that we can also run commands that require shell operations
        # (such as pipes).
        output = console.run_cmd('echo -e "multi\nline\noutput" | grep out')
        self.assertEquals("output", output[-1])

        # benchmark: Confirm that a simple command takes less than 10 ms
        #            to execute.
        iterations = 1000
        start = time.time()
        for i in xrange(iterations):
            output = console.run_cmd('echo "Hello World"')
        end = time.time()
        cmd_time = (end - start) / iterations
        self.assertLess(cmd_time, 0.010)

        print "[APConsoleTelnet] Estimated time per command: %r" % cmd_time

        console.disconnect()

    def test_nop_console(self):
        """Verify the basic functionality of :class:`APConsoleNop`."""
        console = APConsoleNop('dev-dummy', '192.168.1.2', username='root')
        console.connect()

        # Any commands run result in an empty list.
        output = console.run_cmd('echo "Hello World"')
        self.assertEquals(0, len(output))

        # Even more complex operations that require a shell result in no
        # output.
        output = console.run_cmd('echo -e "multi\nline\noutput" | grep out')
        self.assertEquals(0, len(output))

        console.disconnect()

    @unittest.skip("Requires an AP device to be connected with default IP")
    def test_indirect_console(self):
        """Verify the basic functionality of :class:`APConsoleTelnet`."""
        tunnel_console = APConsoleTelnet('dev-dummy', '192.168.1.1', shell_prompt='/#')
        console = APConsoleIndirect('dev-indir', '127.0.0.1', 23, tunnel_console,
                                    shell_prompt='/#')
        console.connect()

        # Simple command for which it is easy to predict the output.
        output = console.run_cmd('echo "Hello World"')
        self.assertEquals("Hello World", output[-1])

        # Verify that we can also run commands that require shell operations
        # (such as pipes).
        output = console.run_cmd('echo -e "multi\nline\noutput" | grep out')
        self.assertEquals("output", output[-1])

        # Reconnect console, verify the connection is still valid
        console._reconnect()
        output = console.run_cmd('echo "Hello World"')
        self.assertEquals("Hello World", output[-1])

        console.disconnect()


if __name__ == '__main__':
    unittest.main()
