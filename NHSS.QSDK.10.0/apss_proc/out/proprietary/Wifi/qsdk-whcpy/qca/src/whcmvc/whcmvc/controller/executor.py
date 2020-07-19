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
import logging
import threading
import Queue

# Imports from this package
from command import CommandBase
from condition_monitor import ConditionMonitorBase

"""Synchronous and asynchronous target device exuction.

The executor infrastructure is responsible for executing periodic as
well as asynchronous commands on a target device.
"""

log = logging.getLogger('whcexecutor')
"""The logger used for executor operations, named ``whcexecutor``."""


class APExecutorBase(object):

    """Interface for all target command execution.

    This interface is provided primarily for testing purposes, allowing
    the executor to be mocked out.
    """

    class SetConditionMonitorMode(CommandBase):
        """Control which mode is used for condition monitors.

        This command can be used when switching between a manually
        polled mode of operation to a passive network event based
        mode of operation, and vice versa.
        """

        def __init__(self, executor, mode, callback=None):
            """Initialize the command.

            :param :class:`APExecutorBase` executor: The executor on
                which to operate
            :param :obj:`ConditionMonitorBase.Mode` mode: Whether the
                condition monitors are polled or event driven.
            :param :obj:`CommandNotifierIface` callback: The object to
                notify upon command completion.
            """
            CommandBase.__init__(self, callback)
            self._executor = executor
            self._mode = mode

        def _run_impl(self, ap_console):
            """Update the condition monitor mode."""
            self._executor._set_condition_monitor_mode(self._mode)

        def __eq__(self, rhs):
            """Compare two command objects for equality."""
            return self._executor == rhs._executor and self._mode == rhs._mode

    class DumpInitialStateCmd(CommandBase):
        """Dump initial state of all associated stations from LBD debug CLI.

        This command will be triggered at the beginning of network event based
        mode of operation.
        """

        def __init__(self, executor, cli_cmds, callback=None):
            """Initialize the command.

            :param :class:`APExecutorBase` executor: The executor on
                which to operate
            :param list cli_cmds: A list of CLI commands to run at startup
            :param :obj:`CommandNotifierIface` callback: The object to
                notify upon command completion.
            """
            CommandBase.__init__(self, callback)
            self._executor = executor
            self._cli_cmds = cli_cmds

        def _run_impl(self, ap_console):
            """Run the CLI command"""
            for cmd in self._cli_cmds:
                self._executor._run_cli_cmd(cmd)

        def __eq__(self, rhs):
            """Compare two command objects for equality."""
            return self._executor == rhs._executor and \
                set(self._cli_cmds) == set(rhs._cli_cmds)

    def __init__(self, ap_id, ap_console):
        """Construct a new AP executor object.

        :param string ap_id: The identifier to use in log messages for
            this AP.  Typically this will correspond to some label on
            the physical device.
        :param :class:`APConsoleBase` ap_console: The console object
            that can be used to run commands on the AP device.
        """
        self._ap_id = ap_id
        self._ap_console = ap_console

    def add_diaglog_handler(self, handler):
        """Add the diagnostic log handler to the executor

        Args:
            handler (:class:`MsgHandler`): the handler object to handle
                diagnostic logs
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def add_condition_monitor(self, monitor):
        """Add the provided monitor to those that execute periodically.

        :param :class:`ConditionMonitorBase` monitor: The monitor
            object to the scheduled monitors.

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def set_cli_console(self, cli_console):
        """Add the debug CLI console to the executor

        :param :class:`APConsoleBase` cli_console: The console object
            that can be used to run debug CLI commands.

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def activate_cli_console(self):
        """Activate the debug CLI console

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def run_async_cmd(self, cmd):
        """Schedule a command to be run asynchronously.

        When this function returns, the command will be run at some
        point in the future. No guarantees are made as to the exact
        time of execution, although commands will be run in FIFO order.

        :param :class:`CommandBase` cmd: The command to run.

        raises :exc:`NotImplementedError`
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    @property
    def ap_id(self):
        return self._ap_id

    @property
    def ap_console(self):
        return self._ap_console


class APActiveExecutor(APExecutorBase):

    """Active class for executing commands and monitors.

    Each instance of this class, when activated, will spawn a thread.
    This thread handles both asynchronous requests for a command to be
    run and periodic target monitors.
    """

    class ShutdownCmd(CommandBase):
        """Sentinel command used to shut down the thread.

        This command does not do anything if invoked.
        """

        def __init__(self):
            """Initialize the command."""
            CommandBase.__init__(self)

        def _run_impl(self, ap_console):
            """Execute the command (a nop)."""
            pass

    def __init__(self, ap_id, ap_console,
                 mode=ConditionMonitorBase.Mode.POLLED):
        """Construct a new AP active executor object.

        :param string ap_id: The identifier to use in log messages for
            this AP.  Typically this will correspond to some label on
            the physical device.
        :param :class:`APConsoleBase` ap_console: The console object
            that can be used to run commands on the AP device.
        :param :obj:`ConditionMonitorBase.Mode` mode: Whether the condition
            monitors start out being actively polled or not.
        """
        APExecutorBase.__init__(self, ap_id, ap_console)

        self._cmd_queue = Queue.Queue()
        self._condition_monitors = []

        self._condition_monitor_mode = mode

        self._diaglog_handlers = []

        self._cli_console = None

    def add_diaglog_handler(self, handler):
        """Add the diagnostic log handler to the executor

        The diagnostic log handler must be added prior to calling
        :method:`activate`.

        Args:
            handler (:class:`MsgHandler`): the handler object to handle
                diagnostic logs
        """
        self._diaglog_handlers.append(handler)

    def add_condition_monitor(self, monitor):
        """Add the provided monitor to those that execute periodically.

        The set of condition monitors must be set up prior to calling
        :method:`activate`.

        :param :class:`ConditionMonitorBase` monitor: The monitor
            object to the scheduled monitors.
        """
        self._condition_monitors.append(monitor)

    def set_cli_console(self, cli_console):
        """Add the debug CLI console to the executor

        Currently only support one CLI console per executor.

        :param :class:`APConsoleBase` cli_console: The console object
            that can be used to run debug CLI commands.
        """
        self._cli_console = cli_console

    def activate_cli_console(self):
        """Activate the debug CLI console"""
        if self._cli_console:
            self._cli_console.connect()

    def activate(self):
        """Start the executor instance running.

        This must be called after the condition monitors have been
        added. Once this is called, no further condition monitors
        should be added until after calling :meth:`shutdown`.
        """
        log.info("Activating executor for AP ID %s", self.ap_id)

        # Note that the thread is created here to allow for it to
        # be shut down and then started again. Python does not allow the
        # same thread object to be started twice.
        self._thread = threading.Thread(target=self._process_events)
        self._thread.start()

        if self._condition_monitor_mode == \
                ConditionMonitorBase.Mode.EVENT_DRIVEN:
            for handler in self._diaglog_handlers:
                handler.start()

    def shutdown(self):
        """Stop the executor instance.

        When this returns, no further commands will be processed nor
        will the periodic condition monitors be invoked.
        """
        log.info("Terminating executor for AP ID %s", self.ap_id)
        self.ap_console.shutdown()
        if self._cli_console:
            self._cli_console.shutdown()

        self.run_async_cmd(APActiveExecutor.ShutdownCmd())
        self._thread.join()

        # Delay the diaglog handler shutdown until after the executor has
        # completed to avoid trying the generate a log message from a
        # condition monitor.
        for handler in self._diaglog_handlers:
            handler.shutdown(None)

    def run_async_cmd(self, cmd):
        """Schedule a command to be run asynchronously.

        When this function returns, the command will be run at some
        point in the future. No guarantees are made as to the exact
        time of execution, although commands will be run in FIFO order.

        :param :class:`CommandBase` cmd: The command to run.

        raises :exc:`NotImplementedError`
            This method must be implemented by derived classes.
        """
        self._cmd_queue.put(cmd)

    def _set_condition_monitor_mode(self, mode):
        """Change the mode of operation to the value provided.

        :param :obj:`ConditionMonitorBase.Mode` mode: The desired new
            mode for the condition monitors.
        """
        if mode == self._condition_monitor_mode:
            return

        self._condition_monitor_mode = mode

        if mode == ConditionMonitorBase.Mode.POLLED:
            for handler in self._diaglog_handlers:
                handler.shutdown(None)

            for monitor in self._condition_monitors:
                if monitor.mode == ConditionMonitorBase.Mode.POLLED:
                    monitor.reset()
        elif mode == ConditionMonitorBase.Mode.EVENT_DRIVEN:
            for monitor in self._condition_monitors:
                if monitor.mode == ConditionMonitorBase.Mode.EVENT_DRIVEN:
                    monitor.reset()

            for handler in self._diaglog_handlers:
                handler.start()
        else:
            raise ValueError("Unexpected mode: %s", mode)

    def _compute_next_condition_monitor_time(self):
        """Determine the delay until a condition monitor should run.

        This will compute the smallest delay. In other words, it
        determines the condition monitor that needs to run closest to
        the current time.
        """
        return reduce(lambda cur_min, monitor:
                      min(cur_min,
                          monitor.get_next_sample_delta(
                              self._condition_monitor_mode)),
                      self._condition_monitors, float('inf'))

    def _run_condition_monitors(self):
        """Invoke the condition monitors that are past due."""
        for monitor in self._condition_monitors:
            if monitor.get_next_sample_delta(self._condition_monitor_mode) == 0:
                if monitor.uses_debug_cli:
                    if self._cli_console:
                        monitor.sample_conditions(self._cli_console)
                else:
                    monitor.sample_conditions(self.ap_console)

    def _process_events(self):
        """Execute commands from the queue as well as periodic tasks."""
        timeout = self._compute_next_condition_monitor_time()

        # For now, we only consider the commands from the queue
        while True:
            try:
                cmd = self._cmd_queue.get(timeout=timeout)
                if isinstance(cmd, APActiveExecutor.ShutdownCmd):
                    break

                log.debug("Running command obtained from queue")
                cmd.run(self.ap_console)
            except Queue.Empty:
                pass

            self._run_condition_monitors()
            timeout = self._compute_next_condition_monitor_time()

        log.info("Executor for AP ID %s terminated", self.ap_id)

    def _run_cli_cmd(self, cmd_str):
        """Run a command in debug CLI console

        Do nothing if no debug CLI console set for this executor.

        :param string cmd_str: The command to run as a single string.
        """
        if self._cli_console:
            self._cli_console.run_cmd(cmd_str)


class APExecutorDB(object):

    """Registry for all executors, identified by their AP ID.
    """

    def __init__(self):
        """Construct a new empty database.
        """
        self._registered_executors = {}

    def add_executor(self, executor):
        """Add the provided executor to the database.

        :param :class:`APExecutorBase` executor: The executor object to
            add.
        """
        if executor.ap_id in self._registered_executors:
            raise ValueError("Executor already registered with this AP ID")

        self._registered_executors[executor.ap_id] = executor

    def get_executor(self, ap_id):
        """Look up the executor using its associated AP ID.

        :param string ap_id: The unique identifier assigned for the
            desired AP executor.

        Returns a :class:`APExecutorBase` instance, or ``None`` if
        there is no executor matching that identifier.
        """
        return self._registered_executors.get(ap_id, None)

    def __iter__(self):
        """Allow for iteration over the registered executors."""
        for executor in self._registered_executors.values():
            yield executor

    def __len__(self):
        """Obtain the number of registered executors."""
        return len(self._registered_executors)
