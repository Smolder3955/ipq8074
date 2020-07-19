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
import os

from mock import MagicMock

from whcmvc.model.model_core import ModelBase
from whcmvc.model import config

from whcmvc.controller.condition_monitor import ConditionMonitorBase
from whcmvc.controller.controller_core import Controller, DUMP_INIT_STATE_CMDS
from whcmvc.controller.executor import APExecutorBase
from whcmvc.controller.command import EnableBSteeringCmd


class TestControllerCore(unittest.TestCase):

    def _create_mock_executor(self, ap_id):
        """Create an executor with the necessary methods mocked."""
        executor = APExecutorBase(ap_id, None)
        executor.activate = MagicMock(name='activate')
        executor.run_async_cmd = MagicMock(name='run_async_cmd')
        executor.shutdown = MagicMock(name='shutdown')
        return executor

    def _create_mock_model(self, device_db):
        """Create a model with the necessary methods mocked."""
        model = ModelBase(device_db)
        model.execute_in_model_context = MagicMock(
            name='execute_in_model_context')
        model.reset_associated_stations = MagicMock(
            name='reset_associated_stations')
        model.reset_utilizations = MagicMock(
            name='reset_utilizations')
        model.reset_overload_status = MagicMock(
            name='reset_overload_status')
        model.reset_steering_blackout_status = MagicMock(
            name='reset_steering_blackout_status')
        return model

    def test_controller(self):
        """Test :class:`Controller`."""
        # Note that the values below need to be kept in sync with this sample
        # config file.
        test_dir = os.path.dirname(__file__)
        device_db = config.create_device_db_from_yaml(
            os.path.join(test_dir, '..', 'model', 'sample_config.yaml'))
        self.assert_(device_db)

        model = self._create_mock_model(device_db)

        controller = Controller(model)
        self.assertIsNotNone(controller.ap_executor_db)

        # Add some mocked out executors to confirm that they are called
        # in the activate/shutdown paths as well as when changing whether
        # band steering is enabled or not.
        executor1 = self._create_mock_executor('ap1')
        executor2 = self._create_mock_executor('ap2')

        controller.ap_executor_db.add_executor(executor1)
        controller.ap_executor_db.add_executor(executor2)

        controller.activate()

        self.assert_(executor1.activate.called)
        self.assert_(not executor1.shutdown.called)
        self.assert_(executor2.activate.called)
        self.assert_(not executor2.shutdown.called)

        executor1.activate.reset_mock()
        executor2.activate.reset_mock()

        # Verify that when band steering is enabled, two commands are
        # enqueued to each executor. The first will actually enable band
        # steering and the second disables the condition monitors.
        #
        # Also confirm that the command notifier resets the model once
        # the condition monitor mode changes are done.
        controller.enable_bsteering(True)
        self.assert_(executor1.run_async_cmd.called)
        self.assert_(executor2.run_async_cmd.called)

        self.assert_(not model.reset_associated_stations.called)
        self.assert_(not model.reset_utilizations.called)
        self.assert_(not model.reset_steering_blackout_status.called)
        self.assert_(not model.reset_overload_status.called)

        calls = executor1.run_async_cmd.call_args_list
        self.assertEquals(((APExecutorBase.SetConditionMonitorMode(
            executor1,
            ConditionMonitorBase.Mode.EVENT_DRIVEN),),), calls[0])
        self.assertEquals(((EnableBSteeringCmd(True, executor1),),), calls[1])

        cmd = calls[0][0][0]
        cmd._callback.notify_cmd_complete(cmd)
        self.assert_(not model.reset_associated_stations.called)
        self.assert_(not model.reset_utilizations.called)
        self.assert_(not model.reset_steering_blackout_status.called)
        self.assert_(not model.reset_overload_status.called)

        calls = executor2.run_async_cmd.call_args_list
        self.assertEquals(((APExecutorBase.SetConditionMonitorMode(
            executor2,
            ConditionMonitorBase.Mode.EVENT_DRIVEN),),), calls[0])
        self.assertEquals(((EnableBSteeringCmd(True, executor2),),), calls[1])

        # Completes both commands, so the model resets can occur.
        cmd = calls[0][0][0]
        cmd._callback.notify_cmd_complete(cmd)
        self.assert_(model.reset_associated_stations.called)
        self.assert_(model.reset_utilizations.called)
        self.assert_(model.reset_steering_blackout_status.called)
        self.assert_(model.reset_overload_status.called)

        executor1.run_async_cmd.reset_mock()
        executor2.run_async_cmd.reset_mock()
        model.reset_associated_stations.reset_mock()
        model.reset_utilizations.reset_mock()
        model.reset_steering_blackout_status.reset_mock()
        model.reset_overload_status.reset_mock()

        # The opposite is true when band steering is disabled. The first
        # command disables band steering and the second enables the condition
        # monitors.
        controller.enable_bsteering(False)
        self.assert_(executor1.run_async_cmd.called)
        self.assert_(executor2.run_async_cmd.called)

        self.assert_(not model.reset_associated_stations.called)
        self.assert_(not model.reset_utilizations.called)
        self.assert_(not model.reset_steering_blackout_status.called)
        self.assert_(not model.reset_overload_status.called)

        calls = executor1.run_async_cmd.call_args_list
        self.assertEquals(((EnableBSteeringCmd(False, executor1),),), calls[0])
        self.assertEquals(((APExecutorBase.SetConditionMonitorMode(
            executor1,
            ConditionMonitorBase.Mode.POLLED),),), calls[1])

        cmd = calls[1][0][0]
        cmd._callback.notify_cmd_complete(cmd)
        self.assert_(not model.reset_associated_stations.called)
        self.assert_(not model.reset_utilizations.called)
        self.assert_(not model.reset_steering_blackout_status.called)
        self.assert_(not model.reset_overload_status.called)

        calls = executor2.run_async_cmd.call_args_list
        self.assertEquals(((EnableBSteeringCmd(False, executor2),),), calls[0])
        self.assertEquals(((APExecutorBase.SetConditionMonitorMode(
            executor2,
            ConditionMonitorBase.Mode.POLLED),),), calls[1])

        executor1.run_async_cmd.reset_mock()
        executor2.run_async_cmd.reset_mock()

        # If enable band steering when it is already enabled, execute
        # SetConditionMonitorMode command and DumpInitialStateCmd command.
        # This is what happens on startup if LBD is already running.
        controller.enable_bsteering(True, already_enabled=True)
        self.assert_(executor1.run_async_cmd.called)
        self.assert_(executor2.run_async_cmd.called)

        self.assert_(not model.reset_associated_stations.called)
        self.assert_(not model.reset_utilizations.called)
        self.assert_(not model.reset_steering_blackout_status.called)
        self.assert_(not model.reset_overload_status.called)

        calls = executor1.run_async_cmd.call_args_list
        self.assertEquals(1, len(calls))
        self.assertEquals(((APExecutorBase.SetConditionMonitorMode(
            executor1,
            ConditionMonitorBase.Mode.EVENT_DRIVEN),),), calls[0])
        executor1.run_async_cmd.reset_mock()

        cmd = calls[0][0][0]
        cmd._callback.notify_cmd_complete(cmd)
        self.assert_(not model.reset_associated_stations.called)
        self.assert_(not model.reset_utilizations.called)
        self.assert_(not model.reset_steering_blackout_status.called)
        self.assert_(not model.reset_overload_status.called)

        calls = executor2.run_async_cmd.call_args_list
        self.assertEquals(1, len(calls))
        self.assertEquals(((APExecutorBase.SetConditionMonitorMode(
            executor2,
            ConditionMonitorBase.Mode.EVENT_DRIVEN),),), calls[0])
        executor2.run_async_cmd.reset_mock()

        # Completes both commands, so the model resets can occur, and
        # will dump all associated STAs
        cmd = calls[0][0][0]
        cmd._callback.notify_cmd_complete(cmd)
        self.assert_(model.reset_associated_stations.called)
        self.assert_(model.reset_utilizations.called)
        self.assert_(model.reset_overload_status.called)
        self.assert_(model.reset_steering_blackout_status.called)

        executor1.run_async_cmd.assert_called_once_with(
            APExecutorBase.DumpInitialStateCmd(executor1, DUMP_INIT_STATE_CMDS))
        executor2.run_async_cmd.assert_called_once_with(
            APExecutorBase.DumpInitialStateCmd(executor2, DUMP_INIT_STATE_CMDS))

        controller.shutdown()

        self.assert_(not executor1.activate.called)
        self.assert_(executor1.shutdown.called)
        self.assert_(not executor2.activate.called)
        self.assert_(executor2.shutdown.called)

if __name__ == '__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)

    unittest.main()
