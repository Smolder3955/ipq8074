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

from mock import MagicMock

from whcmvc.controller.console import APConsoleBase
from whcmvc.controller.command import CommandBase, CommandNotificationIface
from whcmvc.controller.command import EnableBSteeringCmd
from whcmvc.controller.executor import APExecutorBase


class TestCommand(unittest.TestCase):

    def _create_mock_console(self):
        """Create a mocked out :class:`APConsoleBase` instance.

        The :meth:`run_cmd` method will have a mock installed.
        """
        console = APConsoleBase('dev1')
        console.run_cmd = MagicMock(name='run_cmd')
        return console

    def _create_mock_notifier(self):
        """Create a mocked out :class:`CommandNotificationIface`."""
        notifier = CommandNotificationIface()
        notifier.notify_cmd_complete = MagicMock(name='notify_cmd_complete')
        return notifier

    def _create_mock_executor(self, ap_id, ap_console):
        """Create a mocked out :class:`APExecutorBase` instance.

        The :meth:`activate_cli_console` method will have a mock installed"""
        executor = APExecutorBase(ap_id, ap_console)
        executor.activate_cli_console = MagicMock(name='activate_cli_console')
        return executor

    def test_base(self):
        """Verify the basic functionality of :class:`CommandBase`."""
        console = self._create_mock_console()

        # Cannot run the base class
        cmd = CommandBase()
        self.assertRaises(NotImplementedError, cmd.run, console)

        # Even if a notification class is provided, it still cannot be
        # run.
        notifier = self._create_mock_notifier()
        cmd = CommandBase(notifier)
        self.assertRaises(NotImplementedError, cmd.run, console)
        self.assert_(not notifier.notify_cmd_complete.called)

    def test_enable_bsteering_cmd(self):
        """Verify the basic functionality of :class:`EnableBSteeringCmd`."""
        console = self._create_mock_console()
        executor = self._create_mock_executor("MockExecutor", console)

        # Test enable command
        cmd = EnableBSteeringCmd(True, executor)
        cmd.run(console)
        console.run_cmd.assert_called_with(
            'uci set lbd.config.Enable=1 && '
            'uci commit lbd && '
            '/etc/init.d/lbd start')
        self.assert_(executor.activate_cli_console.called)
        console.run_cmd.reset_mock()
        executor.activate_cli_console.reset_mock()

        # Test disable command
        cmd = EnableBSteeringCmd(False, executor)
        cmd.run(console)
        console.run_cmd.assert_called_with(
            '/etc/init.d/lbd stop && '
            'uci set lbd.config.Enable=0 && '
            'uci commit lbd')
        self.assert_(not executor.activate_cli_console.called)


if __name__ == '__main__':
    unittest.main()
