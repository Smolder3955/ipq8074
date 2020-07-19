#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2013-2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import os
import re
import telnetlib
import logging
import threading

import paramiko

import whcmvc  # to get LOG_LEVEL_DUMP

"""Interact with target device via its shell.

Additional implementations can be created by deriving from
:class:`APConsoleBase`.
"""

log = logging.getLogger('whcconsole')
"""The logger used for console operations, named ``whcconsole``."""


class APConsoleBase(object):

    """Abstract base class for all logic that interacts with a target.

    The interface is designed around each console instance being
    connected to a single target device, with the endpoint provided
    at creation time. Derived classes must implement the actual
    connection logic and provide a handle that can be used to send data
    to the target and retrieve data back.
    """

    # This is the prompt that QCA-based target devices provide after
    # login by default (logged in as the root user in that user's home
    # directory).
    DEFAULT_SHELL_PROMPT = '~ #'

    def __init__(self, ap_id, shell_prompt=DEFAULT_SHELL_PROMPT):
        """Construct a new AP console object.

        :param string ap_id: The identifier to use in log messages
            for this device.  Typically this will correspond to some
            label on the device.
        :param string shell_prompt: The characters to match for the
            shell prompt.
        """
        self._ap_id = ap_id
        self._shell_prompt = shell_prompt

        self._shell_prompt_regex = re.compile(self._shell_prompt + ' ')

        # Flag will be set when the console is terminated
        self._is_terminated = threading.Event()
        self._is_terminated.clear()

    def connect(self, timeout=None):
        """Synchronously connect to the target device.

        When this funciton returns, the target must be ready to receive
        commands.

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def run_cmd(self, cmd_str, prompt=None, recon=None):
        """Invoke the command on the target and return the result.

        :param string cmd_str: The command to run on the target.
        :param string prompt: if None, use default shell prompt; otherwise
                              use this specified prompt
        :param :func: recon: if None, use default re-connect function on
                             failure; otherwise, use this give re-connect
                             function

        Return the command output as a list of strings, or empty list
            if the console has been shut down
        """
        if self._is_terminated.is_set():
            return []

        return self._run_cmd(cmd_str, prompt, recon)

    def _run_cmd(self, cmd_str, prompt=None, recon=None):
        """Invoke the command on the target and return the result.

        :param string cmd_str: The command to run on the target.
        :param string prompt: if None, use default shell prompt; otherwise
                              use this specified prompt
        :param :func: recon: if None, use default re-connect function on
                             failure; otherwise, use this give re-connect
                             function

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.

        .. note::

            The device must be connected prior to invoking this method.
            Otherwise, the behavior is undefined.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def disconnect(self):
        """Synchronously disconnect from the target device.

        When this function returns, no further commands may be run on
        the target without first calling :method:`connect` again.

        raises :exc:NotImplementedError
            This method must be implemented by derived classes.
        """
        raise NotImplementedError("Must be implemented by derived classes")

    def shutdown(self):
        """Stop all activities of current console session

        Will stop any reconnection attempt
        """
        self._is_terminated.set()

    def _is_prompt_match(self, output):
        """Determine if the end of the output matches the shell prompt.

        Return True if it does match; otherwise return False.
        """
        return self._shell_prompt_regex.search(output)

    def _reconnect(self):
        """Re-establish the connection to the AP

        raises :exc:RuntimeError if the console object has been shutdown
        """
        self.disconnect()
        while not self._is_terminated.is_set():
            try:
                self.connect(timeout=1)
                # Reconnect success
                return
            except Exception as e:
                log.error("Failed to reconnect to %s (%s). Try again...",
                          self._ap_id, str(e))

        raise RuntimeError("Console terminated")

    @property
    def ap_id(self):
        return self._ap_id


class APConsoleNop(APConsoleBase):

    """Implementation that nops all console operations.
    """

    def __init__(self, ap_id, hostname, password=None, username='root',
                 shell_prompt=APConsoleBase.DEFAULT_SHELL_PROMPT):
        """Initialize the new AP console object.

        :param string ap_id: The identifier to use in log messages for
            this AP device.  Typically this will correspond to some
            label on the device.
        :param string hostname: The hostname or IP address to use to
            connect to the device, as a string.
        :param string password: The password to use when connecting as
            the specified user.
        :param string username: The username to use when conencting.
        """
        APConsoleBase.__init__(self, ap_id, shell_prompt=shell_prompt)

    def connect(self, timeout=None):
        """Establish the connection to the AP device."""
        # No connection needed as this is a nop implementation.
        pass

    def _run_cmd(self, cmd_str, prompt=None, recon=None):
        """Execute a command and return its output.

        :param string cmd_str: The command to run as a single string.
        :param string prompt: not used
        :param :func: recon: not used

        Return the command output as a list of strings.
        """
        return []

    def disconnect(self):
        """Tear down the connection to the AP device."""
        pass


class APConsoleSSH(APConsoleBase):

    """Provide ability to run commands on the AP over SSH.

    Newer AP devices that run QSDK have a dropbear SSH daemon running.
    Thus, it is easy to connect to them and run commands via SSH.
    """

    # Number of bytes to receive at a time when reading from the SSH
    # socket.
    _MAX_RECV_BYTES = 1024

    def __init__(self, ap_id, hostname, password, username='root',
                 shell_prompt=APConsoleBase.DEFAULT_SHELL_PROMPT):
        """Initialize the new AP console object.

        :param string ap_id: The identifier to use in log messages for
            this AP device.  Typically this will correspond to some
            label on the device.
        :param string hostname: The hostname or IP address to use to
            connect to the device, as a string.
        :param string password: The password to use when connecting as
            the specified user.
        :param string username: The username to use when conencting.
        """
        APConsoleBase.__init__(self, ap_id, shell_prompt=shell_prompt)

        self._hostname = hostname
        self._username = username
        self._password = password

        self._client = paramiko.SSHClient()
        self._load_host_keys()

    def connect(self, timeout=None):
        """Establish the SSH connection to the AP device."""
        self._client.connect(self._hostname, username=self._username,
                             password=self._password)
        self._shell_chan = self._client.invoke_shell()
        self._read_until_prompt()

    def _run_cmd(self, cmd_str, prompt=None, recon=None):
        """Execute a command via SSH and return its output.

        :param string cmd_str: The command to run as a single string.
        :param string prompt: not used
        :param :func: recon: not used

        Return the command output as a list of strings.
        """
        self._shell_chan.send(cmd_str + "\n")
        lines = self._read_until_prompt()

        # The first line will be the command that was injected, so only
        # return the remaining lines.
        return lines[1:]

    def disconnect(self):
        """Tear down the SSH connection to the AP device."""
        self._shell_chan.close()
        self._client.close()

    def _load_host_keys(self):
        """Load the system and user-specific SSH host keys.

        Also set up the default missing host key policy to warning, as
        this will be more friendly for testers.
        """
        self._client.load_system_host_keys()
        self._client.set_missing_host_key_policy(paramiko.WarningPolicy())

        # Attempt to obtain user-specific keys so that warnings are not
        # typical.
        try:
            paramiko.util.load_host_keys(
                os.path.expanduser('~/.ssh/known_hosts'))
        except IOError:
            log.warning('Unable to open user host keys file')

    def _read_until_prompt(self):
        """Read from the target and return all but the prompt line.

        This assumes that the prompt will be by itself on a line. This
        will generally be true so long as a command does not output
        a line without a newline.

        This returns the lines of text captured as a list of strings.
        """
        cum_output = ''
        while not self._is_prompt_match(cum_output):
            output = self._shell_chan.recv(APConsoleSSH._MAX_RECV_BYTES)
            cum_output = ''.join([cum_output, output])

        return cum_output.split("\r\n")[:-1]


class APConsoleTelnet(APConsoleBase):

    """Provide ability to run commands on the AP over telnet.

    This should be used with LSDK-based images, as these do not run
    SSH.
    """

    # Number of bytes to receive at a time when reading from the telnet
    # socket.
    _MAX_RECV_BYTES = 1024

    def __init__(self, ap_id, hostname, password=None, username='root',
                 shell_prompt=APConsoleBase.DEFAULT_SHELL_PROMPT,
                 port=23):
        """Initialize the new AP console object.

        Args:
            ap_id (str): The identifier to use in log messages for
                this AP device.  Typically this will correspond to some
                label on the device.
            hostname (str): The hostname or IP address to use to
                connect to the device, as a string.
            password (str): The password to use when connecting as
                the specified user.
            username (str): The username to use when conencting.
            shell_prompt (str): the shell prompt for this console.
        """
        APConsoleBase.__init__(self, ap_id, shell_prompt=shell_prompt)

        self._hostname = hostname
        self._username = username
        self._password = password
        self._port = port

        self._client = None  # gets created in connect

    def connect(self, timeout=None):
        """Establish the telnet connection to the AP device."""
        self._client = telnetlib.Telnet(self._hostname, self._port,
                                        timeout=timeout)

        if self._password is not None:
            self._client.read_until("login: ")
            self._client.write(self._username + "\n")
            self._client.read_until("Password: ")
            self._client.write(self._password + "\n")

        self._read_until_prompt()

    def _run_cmd(self, cmd_str, prompt=None, recon=None):
        """Execute a command via Telnet and return its output.

        :param string cmd_str: The command to run as a single string.
        :param string prompt: if None, use default shell prompt; otherwise
                              use this specified prompt
        :param :func: recon: if None, use default re-connect function on
                             failure; otherwise, use this give re-connect
                             function

        Return the command output as a list of strings.
        """
        lines = None
        while lines is None:
            log.log(whcmvc.LOG_LEVEL_DUMP, "Running '%s' on '%s'", cmd_str,
                    self.ap_id)
            self._client.write(cmd_str + "\n")
            lines = self._read_until_prompt(prompt)

            if lines is None:
                log.warning("Reconnecting on '%s' due to command failure",
                            self.ap_id)
                try:
                    recon() if recon is not None else self._reconnect()
                except:
                    log.info("Give up running '%s' due to reconnection error",
                             cmd_str)
                    return []

        # The first line will be the command that was injected, so only
        # return the remaining lines.
        return lines[1:]

    def disconnect(self):
        """Tear down the Telnet connection to the AP device."""
        self._client.close()

    def _read_until_prompt(self, prompt=None):
        """Read from the target and return all but the prompt line.

        This assumes that the prompt will be by itself on a line. This
        will generally be true so long as a command does not output
        a line without a newline.

        :param string prompt: if None, use default shell prompt; otherwise
                              use this specified prompt

        This returns the lines of text captured as a list of strings.
        """
        # Normally we should not time out. If a timeout does occur, we want
        # to try to recover by disconnecting and reconnecting. This is why
        # the timeout value is rather long when commands should be exuected
        # quickly.
        expect_prompt_regex = self._shell_prompt_regex
        if prompt is not None:
            expect_prompt_regex = re.compile(prompt + ' ')
        (match_index, match, output) = self._client.expect(
            [expect_prompt_regex], timeout=1.0)

        if match_index == -1:
            log.error("Failed to match prompt; text='%s'", output)
            return None

        if output.find("\r") == -1:
            return output.split("\n")[:-1]
        else:
            return output.split("\r\n")[:-1]


class APConsoleIndirect(APConsoleBase):

    """Provide ability to run commands on the AP over an underlying console.

    This is currently used by HYD debug CLI as it only allows connection
    from localhost.
    """

    def __init__(self, ap_id, hostname, port, tunnel_console,
                 shell_prompt=APConsoleBase.DEFAULT_SHELL_PROMPT):
        """Initialize the new indirect AP console object.

        Args:
            ap_id (str): The identifier to use in log messages for this
                device. Typically this will correspond to some label on
                the device.
            hostname (str): The hostname or IP address to use to
                connect to the device, as a string.
            port (int): The port number to use to connect to the device
            tunnel_console (:class:`APConsoleBase`): the indirect console
                via which to establish a connection to this console
            shell_prompt (str): the shell prompt for this console.
        """
        APConsoleBase.__init__(self, ap_id, shell_prompt=shell_prompt)

        self._hostname = hostname
        self._port = port
        self._shell_prompt = shell_prompt
        self._tunnel_console = tunnel_console

    def connect(self, timeout=None):
        """Establish the telnet connection to the AP device."""
        self._tunnel_console.connect(timeout)
        self.run_cmd("telnet %s %d" % (self._hostname, self._port))
        # TODO: Handle login credentials

    def _run_cmd(self, cmd_str, prompt=None, recon=None):
        """Execute a command via underlying tunnel console and return its output.

        :param string cmd_str: The command to run as a single string.
        :param string prompt: not used
        :param :func: recon: not used

        Return the command output as a list of strings.
        """
        return self._tunnel_console.run_cmd(cmd_str, prompt=self._shell_prompt,
                                            recon=self._reconnect)

    def disconnect(self):
        """Tear down the indirect connection to the AP device."""
        self._tunnel_console.disconnect()
