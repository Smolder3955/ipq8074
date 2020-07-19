#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import logging

"""Command objects for execution by an AP Executor.

Each command performs some action on the target device in the context of
the AP Executor and invokes an optional callback when complete.
"""

log = logging.getLogger('whccommand')
"""The logger used for commands, named ``whccommand``."""


class CommandNotificationIface(object):

    """Interface class for notifications about commands.

    If the entity creating commands needs to be informed upon command
    completion, it should derive from this class and provide an
    appropriate definition for the desired notify_* functions.

    .. note::

        The notify_* functions are called within the AP Executor thread
        context. They should not block indefinitely or perform other
        long running operations.
    """

    def __init__(self):
        """Initialize a new command notification object."""
        pass

    def notify_cmd_complete(self, cmd):
        """Notify the appropriate entity that a command finished.

        :param :class:`CommandBase` cmd: The command that just
            completed execution.
        """
        # Do nothing by default
        pass


class CommandBase(object):

    """Abstract base class for all command implementations.

    All command objects are constructed with an optional callback
    object. As the commands are executed asynchronously, this callback
    is used to signal the requestor of the command that the execution
    is complete.
    """

    def __init__(self, callback=None):
        """Construct a new command object.

        :param :class:`CommandCompletionIface` callback: The object to
            inform (via :meth:`notify_cmd_complete`) of command
            execution completion.
        """
        self._callback = callback

    def run(self, ap_console):
        """Execute the command in the caller's context.

        Upon completion of the command, this invokes the callback if
        one was provided at construction time.

        Derived classes should not override this method. Rather, they
        should override :meth:`run_impl`.

        :param :class:`APConsoleBase` ap_console: The console object to
            use when executing the commands.
        """
        self._run_impl(ap_console)

        if self._callback is not None:
            self._callback.notify_cmd_complete(self)

    def _run_impl(self, ap_console):
        """Execute the actual body of the command.

        This function must be overridden by derived classes. This is
        where the actual running of commands on the target device is to
        be placed.

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Derived classes must implement this method")

    @property
    def callback(self):
        """Obtain the callback object.

        This is only intended for testing purposes.
        """
        return self._callback


class EnableBSteeringCmd(CommandBase):

    """Command that enable/disable lbd."""

    def __init__(self, enable, executor, callback=None):
        """Initialize the command.

        :param bool enable: whether this command is to enable or disable LBD
        :param :class: `APExecutorBase` executor: the executor to activate debug
            CLI console
        :param :class:`CommandNotificationIface` callback: The object
            to invoke upon completion of the command (if any).
        """
        CommandBase.__init__(self, callback)

        self._enable = enable
        self._executor = executor

    def _run_impl(self, ap_console):
        """Execute the enable bsteering command on the AP via its console.

        :param :class:`APConsoleBase` ap_console: The handle to the
            console to use when running the command.
        """
        ap_console.run_cmd(self._generate_cmd())
        if self._enable:
            self._executor.activate_cli_console()

    def _generate_cmd(self):
        """Construct the raw command to run on the target."""
        if self._enable:
            return 'uci set lbd.config.Enable=1 && uci commit lbd && /etc/init.d/lbd start'
        else:
            return '/etc/init.d/lbd stop && uci set lbd.config.Enable=0 && uci commit lbd'

    def __eq__(self, rhs):
        """Compare two command objects for equality.

        This will ignore the callback handle.
        """
        return self._enable == rhs._enable

    def __str__(self):
        """Returns a summary of the command to be run."""
        return self._generate_cmd()


__all__ = ['CommandNotificationIface', 'EnableBSteeringCmd']
