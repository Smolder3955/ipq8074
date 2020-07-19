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
import threading

from executor import APExecutorDB, APExecutorBase
from condition_monitor import ConditionMonitorBase
from command import EnableBSteeringCmd, CommandNotificationIface

"""Core representation of the controller.

This module exports the following:

    :class:`Controller`
        entity that serves as a container for the underlying controller
        objects
"""

DUMP_INIT_STATE_CMDS = ['stadb diaglog assoc',
                        'estimator diaglog pollution',
                        'steerexec diaglog all']

log = logging.getLogger('whccontroller_core')
"""The logger used for controller class, named ``whccontroller_core``."""


class Controller(object):

    """Class that manages all of the controller components.

    This contains the current AP executors.
    """

    class ResetModelCmdNotifier(CommandNotificationIface):

        """Class for tracking when commands have completed.

        This class will count the number of commands that have
        completed and perform the model reset when all commands
        have completed.
        """

        def __init__(self, model, executor_db, dump_init_state):
            """Initialize the notifier with the expected command count.

            Args:
                executor_db (:object:`APExecutorBase`): Registry for all executors
                model (:object:`Model`): The model to reset when all
                    commands have completed.
                dump_init_state (bool): Flag indicating whether to dump information
                    of all associated STAs after model reset
            """
            self._model = model
            self._ap_executor_db = executor_db
            self._dump_init_state = dump_init_state

            # Expected number of commands which should complete equals the
            # number of executors.
            self._expected_num_cmds = len(self._ap_executor_db)
            self._completed_cmd_count = 0
            self._lock = threading.Lock()

        def notify_cmd_complete(self, cmd):
            """React to the completion of a command."""
            with self._lock:
                self._completed_cmd_count += 1
                if self._is_done():
                    self._model.reset_associated_stations()
                    self._model.reset_utilizations()
                    self._model.reset_overload_status()
                    self._model.reset_steering_blackout_status()
                    if self._dump_init_state:
                        for executor in self._ap_executor_db:
                            init_cmd = APExecutorBase.DumpInitialStateCmd(
                                executor, DUMP_INIT_STATE_CMDS)
                            executor.run_async_cmd(init_cmd)

        def _is_done(self):
            """Determine whether all commands have completed.

            This must be called with the lock held."""
            return self._completed_cmd_count == self._expected_num_cmds

    def __init__(self, model, discon_mode=False):
        """Initialize a new empty controller.

        Args:
            model (:object:`Model`): the model to associate with this
                controller instance
            discon_mode (bool): whether to operate in disconnected mode
                (where no interactions with the AP device are expected)
        """
        self._model = model
        self._discon_mode = discon_mode

        self._ap_executor_db = APExecutorDB()

    def activate(self):
        """Start all active components of the controller."""
        for executor in self._ap_executor_db:
            executor.activate()

    def shutdown(self):
        """Shut down all active components of the controller."""
        for executor in self._ap_executor_db:
            executor.shutdown()

    def enable_bsteering(self, enable, already_enabled=False):
        """Enable/Disable band steering feature.

        Args:
            enable (bool): whether to enable or disable band steering
            already_enabled (bool): whether the band steering is already enabled.
                If it is enabled already, do not restart it.
        """
        # When enabling band steering, we want to reset the model only
        # after the polling based monitoring has been disabled. Otherwise,
        # it is possible for the monitoring to update the model after it
        # has been reset (before the command has been processed).
        notifier = Controller.ResetModelCmdNotifier(
            self._model, self._ap_executor_db,
            enable and already_enabled)  # Only dump STAs when it's already enabled

        for executor in self._ap_executor_db:
            cmd = EnableBSteeringCmd(enable, executor)
            # When turning off band steering, we want to disable lbd first
            # and then change the condition monitors. The inverse is true
            # when enabling band steering.
            if not enable:
                executor.run_async_cmd(cmd)

            mode = ConditionMonitorBase.Mode.EVENT_DRIVEN if enable \
                else ConditionMonitorBase.Mode.POLLED
            mon_cmd = APExecutorBase.SetConditionMonitorMode(executor, mode,
                                                             callback=notifier)
            executor.run_async_cmd(mon_cmd)

            if enable and not already_enabled:
                executor.run_async_cmd(cmd)

    def check_bsteering(self):
        """Check if band steering is enabled.

        Returns:
            A list of tuples whose first element is the AP ID, second element is True
                if LBD is enabled on the AP, False otherwise, third element is True if
                HYD is enabled on the AP, False otherwise
        """
        bsteering_status_list = []
        if self._discon_mode:
            for executor in self._ap_executor_db:
                bsteering_status_list.append((executor.ap_id, True, True))
        else:
            for executor in self._ap_executor_db:
                bsteering_status_list.append(
                    (executor.ap_id,
                     len(executor.ap_console.run_cmd('pgrep "/usr/sbin/lbd"')) > 0,
                     len(executor.ap_console.run_cmd('pgrep "/usr/sbin/hyd"')) > 0))
        return bsteering_status_list

    @property
    def model(self):
        return self._model

    @property
    def ap_executor_db(self):
        return self._ap_executor_db

# Exports
__all__ = ['Controller']
