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

# Standard Python imports
import unittest
import threading
import time

# Third party imports
from mock import MagicMock

# Imports from this package
from whcmvc.controller.executor import APExecutorBase, APActiveExecutor, APExecutorDB
from whcmvc.controller.condition_monitor import ConditionMonitorBase
from whcmvc.controller.console import APConsoleBase
from whcmvc.controller.command import CommandNotificationIface, CommandBase
from whcmvc.controller.diaglog import MsgHandler


class TestAPExecutor(unittest.TestCase):

    class TestCommandNotificationHandler(CommandNotificationIface):

        """Notification handler that records the callback.

        This is used so that the caller can wait until the notification
        has occurred before checking that the command was executed."""

        def __init__(self, num_expected_calls=1):
            """Initialize a new command notification object."""
            self._notify_event = threading.Event()
            self._num_expected_calls = num_expected_calls
            self._num_calls = 0
            self._call_params = []

        def notify_cmd_complete(self, cmd):
            """Record that the command has completed execution.

            :param :class:`CommandBase` cmd: The command that just
                completed execution.
            """
            self._call_params.append((cmd))
            self._num_calls += 1

            if self._num_calls == self._num_expected_calls:
                self._notify_event.set()

        def wait_for_notification(self):
            """Wait for the command completion notification.

            This will block until the notification occurs."""
            self._notify_event.wait()

        def get_call_params(self):
            """Get the list of call parameters recorded.

            All parameters to :meth:`notify_cmd_complete` (other than
            the ``self`` parameter) will be in a tuple within this
            list.
            """
            return list(self._call_params)

    def _create_mock_console(self):
        """Create a mocked out :class:`APConsoleBase` instance.

        The :meth:`run_cmd` method will have a mock installed.
        """
        console = APConsoleBase('dev1')
        console.run_cmd = MagicMock(name='run_cmd')
        console.connect = MagicMock(name='connect')
        console.disconnect = MagicMock(name='disconnect')
        console.shutdown = MagicMock(name='shutdown')
        return console

    def _create_mock_cmd(self, notification_handle):
        """Create a mocked out :class:`CommandBase` instance.

        :param :class:`CommandNotificationBase` notification_handle:
            The object to notify upon command completion.
        """
        cmd = CommandBase(notification_handle)
        cmd._run_impl = MagicMock(name='_run_impl')
        return cmd

    def _create_mock_diaglog_handler(self, model, ap):
        """Create a mocked out :class:`MsgHandler` instance.

        Args:
            model (:obj:): fake model to udpate
            ap (:obj:): fake AP object
        """
        handler = MsgHandler(model, ap, None)
        handler.start = MagicMock(name='start')
        handler.shutdown = MagicMock(name='shutdown')
        return handler

    def test_base(self):
        """Verify the proper functioning of :class:`APExecutorBase`."""
        console = APConsoleBase('dev2')
        executor = APExecutorBase('ap1', console)
        self.assertRaises(NotImplementedError, executor.add_condition_monitor,
                          None)
        self.assertRaises(NotImplementedError, executor.add_diaglog_handler,
                          None)
        self.assertRaises(NotImplementedError, executor.run_async_cmd, None)
        self.assertRaises(NotImplementedError, executor.set_cli_console, None)

    def test_active_executor(self):
        """Verify the proper functioning of :class:`APActiveExecutor`."""
        console = self._create_mock_console()
        executor = APActiveExecutor('ap1', console)
        executor.activate()

        # Inject a command and make sure it gets invoked and the notification
        # is generated.
        handler = TestAPExecutor.TestCommandNotificationHandler()
        cmd = self._create_mock_cmd(handler)
        executor.run_async_cmd(cmd)

        handler.wait_for_notification()
        self.assertEquals([cmd], handler.get_call_params())
        cmd._run_impl.assert_called_with(console)

        executor.shutdown()
        console.shutdown.assert_called_once_with()
        console.shutdown.reset_mock()

        # Inject a couple of commands to make sure each one gets invoked
        # in FIFO order. To ensure we can tell the ordering, we do not
        # activate the executor until after the two commands have been
        # injected.
        handler = TestAPExecutor.TestCommandNotificationHandler(
            num_expected_calls=2)
        cmd1 = self._create_mock_cmd(handler)
        executor.run_async_cmd(cmd1)

        cmd2 = self._create_mock_cmd(handler)
        executor.run_async_cmd(cmd2)

        executor.activate()
        handler.wait_for_notification()
        self.assertEquals([cmd1, cmd2], handler.get_call_params())
        cmd1._run_impl.assert_called_with(console)
        cmd2._run_impl.assert_called_with(console)

        executor.shutdown()
        console.shutdown.assert_called_once_with()

    class RecordingMonitor(ConditionMonitorBase):

        """Dummy monitor that records the time of invocation."""

        def __init__(self, interval, mode=ConditionMonitorBase.Mode.POLLED):
            """Initialize a new recording monitor."""
            ConditionMonitorBase.__init__(self, interval, mode)

            self._call_times = []
            self._reset_called = False

        def _sample_conditions_impl(self, ap_console):
            """Perform the monitor action by sampling the time."""
            self._call_times.append(time.time())

        def _reset_impl(self):
            """Reset the state to begin sampling again."""
            self._call_times = []
            self._reset_called = True

        def get_call_times(self):
            """Obtain the times recorded for the calls.

            Note that this is not protected by any lock, so it should
            only be called once the executor thread has been shut down.
            """
            return self._call_times

        def clear_state(self):
            """Clear out what has been recorded about the calls made."""
            self._call_times = []
            self._reset_called = False

        @property
        def reset_called(self):
            """Flag indicating whether reset was called."""
            return self._reset_called

    def _verify_call_deltas(self, call_times, interval, min_count):
        """Confirm that the call times are approximately spaced right.

        :param list of time.time values call_times: the recorded times
            for the invocations
        :param float interval: the expected delta between calls
        :param int min_count: the minimum number of times the call must
            be made
        """
        deltas = []
        last_call_time = None
        for call_time in call_times:
            if last_call_time is not None:
                deltas.append(call_time - last_call_time)
            last_call_time = call_time

        # This should handle an off by 1 error. We could be a bit more
        # strict about this, but it may not be worth it.
        self.assertGreaterEqual(len(call_times), min_count)

        # Verify the deltas are within 5 ms of the expected.
        for delta in deltas:
            self.assertAlmostEqual(delta, interval, delta=0.005)

    def test_active_executor_monitors(self):
        """Verify condition monitors are executed in :class:`APActiveExecutor`."""
        console = self._create_mock_console()
        executor = APActiveExecutor('ap1', console)

        monitor1 = TestAPExecutor.RecordingMonitor(0.500)
        monitor2 = TestAPExecutor.RecordingMonitor(0.200)

        executor.add_condition_monitor(monitor1)
        executor.add_condition_monitor(monitor2)

        executor.activate()

        time.sleep(0.7)

        # Verify a command injected in between when a monitor should have
        # been invoked does not impact the timing.
        handler = TestAPExecutor.TestCommandNotificationHandler(
            num_expected_calls=1)
        cmd1 = self._create_mock_cmd(handler)
        executor.run_async_cmd(cmd1)

        time.sleep(1.3)

        executor.shutdown()
        console.shutdown.assert_called_once_with()

        self._verify_call_deltas(monitor1.get_call_times(), 0.500, 3)
        self._verify_call_deltas(monitor2.get_call_times(), 0.200, 9)

    def test_active_executor_cli_console(self):
        """Verify CLI console is executed in :class:`APActiveExecutor`."""
        console = self._create_mock_console()
        executor = APActiveExecutor('ap1', console)
        executor.activate()

        cli_console = self._create_mock_console()
        cli_cmd = ["fake cli command", "another cli cmd"]

        # Case 1: Before set cli_console, run_cli_cmd does nothing
        cmd = APExecutorBase.DumpInitialStateCmd(executor, cli_cmd)
        executor.run_async_cmd(cmd)

        time.sleep(1)
        self.assert_(not cli_console.connect.called)
        self.assert_(not cli_console.disconnect.called)
        self.assert_(not cli_console.run_cmd.called)

        # Case 2: Set cli_console, verify cli_cmd is executed
        executor.set_cli_console(cli_console)
        cmd = APExecutorBase.DumpInitialStateCmd(executor, cli_cmd)
        executor.run_async_cmd(cmd)

        time.sleep(1)
        calls = cli_console.run_cmd.call_args_list
        self.assertEquals(len(cli_cmd), len(calls))
        for i in xrange(len(cli_cmd)):
            self.assertEquals(cli_cmd[i], calls[i][0][0])

        executor.shutdown()
        console.shutdown.assert_called_once_with()
        cli_console.shutdown.assert_called_once_with()

    def test_active_executor_diaglog_handler(self):
        """Verify diagnostic logging handler are executed in :class:`APActiveExecutor`."""
        console = self._create_mock_console()
        diaglog_handler = self._create_mock_diaglog_handler('model', 'ap1')

        # Case 1: Executor activates with condition monitor mode enabled,
        #         should not start diaglog handler
        executor = APActiveExecutor('ap1', console)
        executor.add_diaglog_handler(diaglog_handler)
        executor.activate()
        self.assert_(not diaglog_handler.start.called)
        executor.shutdown()
        diaglog_handler.shutdown.assert_called_once_with(None)
        diaglog_handler.shutdown.reset_mock()
        console.shutdown.assert_called_once_with()
        console.shutdown.reset_mock()

        # Case 2: Executor activates with condition monitor mode for async
        #         events, should start diaglog handler
        executor = APActiveExecutor('ap1', console,
                                    mode=ConditionMonitorBase.Mode.EVENT_DRIVEN)
        executor.add_diaglog_handler(diaglog_handler)
        executor.activate()
        self.assert_(diaglog_handler.start.called)
        diaglog_handler.start.reset_mock()

        # Case 3: When condition monitor mode is switched to polled, shutdown
        #         diaglog handler
        cmd = APExecutorBase.SetConditionMonitorMode(
            executor, ConditionMonitorBase.Mode.POLLED)
        executor.run_async_cmd(cmd)
        time.sleep(1)
        diaglog_handler.shutdown.assert_called_once_with(None)
        diaglog_handler.shutdown.reset_mock()

        # Case 4: When condition monitor mode is switched to event driven,
        #         start diaglog handler
        cmd = APExecutorBase.SetConditionMonitorMode(
            executor, ConditionMonitorBase.Mode.EVENT_DRIVEN)
        executor.run_async_cmd(cmd)
        time.sleep(1)
        self.assert_(diaglog_handler.start.called)
        diaglog_handler.start.reset_mock()

        executor.shutdown()
        diaglog_handler.shutdown.assert_called_once_with(None)
        diaglog_handler.shutdown.reset_mock()
        console.shutdown.assert_called_once_with()
        console.shutdown.reset_mock()

    def test_active_executor_condition_monitor_mode(self):
        """Verify condition monitors during mode switches.

        Condition monitors operating in polled mode should not be told
        to sample conditions if the executor is operating in event
        driven mode. However, ones that are active in only event driven
        mode or in both modes should be sampled.
        When mode switches occur, condition monitors should be reset and
        then be asked for sampling times again.
        """
        console = self._create_mock_console()
        executor = APActiveExecutor('ap1', console)

        monitor1 = TestAPExecutor.RecordingMonitor(0.500)
        monitor2 = TestAPExecutor.RecordingMonitor(0.200)
        monitor3 = TestAPExecutor.RecordingMonitor(
            0.600, ConditionMonitorBase.Mode.EVENT_DRIVEN)
        monitor4 = TestAPExecutor.RecordingMonitor(
            0.400, ConditionMonitorBase.Mode.BOTH)

        executor.add_condition_monitor(monitor1)
        executor.add_condition_monitor(monitor2)
        executor.add_condition_monitor(monitor3)
        executor.add_condition_monitor(monitor4)

        executor.activate()

        # Case 1: Let some time elapse and then switch into event driven mode.
        time.sleep(2.1)

        # Before switching to event driven mode, verify call times.
        mon1_call_times = monitor1.get_call_times()
        mon2_call_times = monitor2.get_call_times()
        mon3_call_times = monitor3.get_call_times()
        mon4_call_times = monitor4.get_call_times()
        self._verify_call_deltas(mon1_call_times, 0.500, 4)
        self._verify_call_deltas(mon2_call_times, 0.200, 10)
        self._verify_call_deltas(mon4_call_times, 0.400, 5)
        # No sample should be done for monitor 3 due to mode not match
        self.assert_(0 == len(mon3_call_times))

        # Switch to event driven mode
        cmd = APExecutorBase.SetConditionMonitorMode(
            executor, ConditionMonitorBase.Mode.EVENT_DRIVEN)
        executor.run_async_cmd(cmd)

        time.sleep(1)

        # The event driven monitors will be reset
        self.assert_(monitor3.reset_called)
        self.assert_(not monitor4.reset_called)

        monitor3.clear_state()

        # Case 2: Switch back to polling mode. The monitors that operate
        #         in polled mode and both modes will be sampled. The
        #         polled ones will also get reset. The event driven
        #         ones will not be sampled (as the assumption is that
        #         the polling logic will be sufficient to grab any
        #         information needed).
        cmd = APExecutorBase.SetConditionMonitorMode(
            executor, ConditionMonitorBase.Mode.POLLED)
        executor.run_async_cmd(cmd)

        time.sleep(0.9)

        executor.shutdown()
        console.shutdown.assert_called_once_with()

        self.assert_(monitor1.reset_called)
        self.assert_(monitor2.reset_called)
        self.assert_(not monitor3.reset_called)
        self.assert_(not monitor4.reset_called)

        self._verify_call_deltas(monitor1.get_call_times(), 0.500, 2)
        self._verify_call_deltas(monitor2.get_call_times(), 0.200, 5)
        self.assertEquals(0, len(monitor3.get_call_times()))
        self._verify_call_deltas(monitor4.get_call_times(), 0.400, 7)

    def test_executor_db(self):
        """Test basic functionality of :class:`APExecutorDB`."""
        db = APExecutorDB()

        ap_id1 = 'dev1'
        exec1 = APExecutorBase(ap_id1, self._create_mock_console())
        db.add_executor(exec1)
        self.assertEquals(1, len(db))
        self.assertEquals(exec1, db.get_executor(ap_id1))
        self.assertEquals(None, db.get_executor('devX'))

        ap_id2 = 'dev2'
        exec2 = APExecutorBase(ap_id2, self._create_mock_console())
        db.add_executor(exec2)
        self.assertEquals(2, len(db))
        self.assertEquals(exec1, db.get_executor(ap_id1))
        self.assertEquals(exec2, db.get_executor(ap_id2))
        self.assertEquals(None, db.get_executor('devX'))

        # Verify duplicate registrations are rejected.
        self.assertRaises(ValueError, db.add_executor, exec1)


if __name__ == '__main__':
    unittest.main()
