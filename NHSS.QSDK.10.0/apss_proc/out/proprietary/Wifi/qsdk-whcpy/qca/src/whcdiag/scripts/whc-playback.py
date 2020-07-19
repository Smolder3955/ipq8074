#!/usr/bin/env python
#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

import time
import signal
import datetime
import socket
from contextlib import closing

from whcdiag.io.filesystem import FileReaderRaw
from whcdiag.io.network import NetworkReaderRaw
import whcdiag.msgs.parser as diagparser


class PlaybackSocketFactory(object):
    """Class containing socket(s) used for playback"""

    def __init__(self, ip_mapping):
        self._ip_mapping = ip_mapping

        # IP address to socket mapping
        self._socket_mapping = {}
        # Whether to suppress error log if there is not socket
        # mapped to the given IP address
        self._suppress_err = {}

    def __enter__(self):
        """Initialize socket(s) used to playback"""
        if self._ip_mapping is not None:
            for (src_addr, loopback_addr) in self._ip_mapping:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.bind((loopback_addr, 0))
                self._socket_mapping[src_addr] = sock
        else:
           # By default it maps to any IP address
           self._socket_mapping['*'] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        return self

    def __exit__(self, type, value, traceback):
        """Close the socket(s)"""
        for ip_addr, sock in self._socket_mapping.iteritems():
            sock.close()

        self._socket_mapping = {}
        self._suppress_err = {}

    def connect(self, playback_server):
        """Connect socket(s) to playback server"""
        for ip_addr, sock in self._socket_mapping.iteritems():
            sock.connect(playback_server)

    def get_sock(self, ip_addr):
        """Look for socket to playback for a given IP address

        If there is IP mapping, look for the corresponding IP address,
        otherwise return the generic socket
        """
        sock = self._socket_mapping.get(
                   ip_addr, self._socket_mapping.get('*', None))

        if sock is None and not self._suppress_err.get(ip_addr, False):
            logging.error("No IP mapping for %s" % ip_addr)
            self._suppress_err[ip_addr] = True

        return sock


class PlaybackInfo(object):
    """Class to assist in managing multiple playback log types"""

    def __init__(self, ip_mapping, file_name, port):
        self._socket_factory = PlaybackSocketFactory(ip_mapping)
        self._reader = FileReaderRaw(file_name)
        self._port = port
        self._msg = None

    def __enter__(self):
        """Initialize socket factory and file reader"""
        # Setup the socket factory
        self._socket_factory.__enter__()

        # Setup the file reader
        self._reader = self._reader.__enter__()
        self.update_next_message()

        return self

    def __exit__(self, type, value, traceback):
         """Cleanup underlying classes"""
         self._reader.__exit__(type, value, traceback)
         self._socket_factory.__exit__(type, value, traceback)

    def update_next_message(self):
        """Update the next message to be sent"""
        self._msg = self._reader.read_msg()

    def connect(self, dst_ip):
        """Connect to the dest IP / port combination provided"""
        self._socket_factory.connect((dst_ip, self._port))

    def get_sock(self):
        """Get the socket to use based on destination IP"""
        return self._socket_factory.get_sock(self._msg[1])

    @property
    def next_msg(self):
        return self._msg


class PlaybackInfoMaster(object):
    """Class to assist in managing multiple playback log types"""

    def __init__(self, inputs, dst_ip, ip_mapping):
        self._dst_ip = dst_ip
        self._playbacks = []
        
        for inp in inputs:
            playback = PlaybackInfo(ip_mapping, inp[0], inp[1])
            self._playbacks.append(playback)

    def __enter__(self):
        """Setup underlying classes and connect"""
        for playback in self._playbacks:
            playback.__enter__()
            playback.connect((self._dst_ip))

        return self

    def __exit__(self, type, value, traceback):
         """Cleanup underlying classes"""
         for playback in self._playbacks:
            playback.__exit__(type, value, traceback)

    def get_next_playback_info(self):
        """Get the playback instance with the soonest timestamp (or None if none remain)"""
        next_playback = None
        for playback in self._playbacks:
            if playback.next_msg:
                if not next_playback or next_playback.next_msg[0] > playback.next_msg[0]:
                    next_playback = playback

        return next_playback


def playback_msg(sock, data):
    """Play back a single message read from the file.

    Args:
        sock (:object:`socket`): the socket on which to send the data
            (this is a connected dgram socket and thus send can be
            used to send to the right destination)
        data (str): the actual log message to send
    """
    sock.send(data)


def playback_file(inputs, dst_ip, speed, ip_mapping):
    """Play back all of the logs from the file.

    For each message in the file, send it to the destination and then
    delay for the elapsed time before sending the next message.

    Args:
        inputs (list): list of tuples containing the input file and destination port
        dst_ip (str): the destination IP address
        speed (int): the scaling factor for delays between messages (to
            allow for faster or slower than real time playback)
        ip_mapping (list of tuple of strings): the mapping from IP address
            in playback file to the IP address used to send the log
    """
    with PlaybackInfoMaster(inputs, dst_ip, ip_mapping) as playback_info:
        
        logging.info("Playing back at %dx speed", speed)
        num_msgs = 0
        last_time = None
        next_playback_info = playback_info.get_next_playback_info()
        while next_playback_info:
            if last_time is not None:
                time.sleep((next_playback_info.next_msg[0] - last_time) / speed)
            logging.debug("Injecting message #%d", num_msgs + 1)
            sock = next_playback_info.get_sock()
            if sock is not None:
                playback_msg(sock, next_playback_info.next_msg[-1])
                last_time = next_playback_info.next_msg[0]
                num_msgs += 1

            next_playback_info.update_next_message()
            next_playback_info = playback_info.get_next_playback_info()

        logging.info("Processed %d messages", num_msgs)


if __name__ == '__main__':
    import argparse
    import logging
    import sys
    import os

    parser = argparse.ArgumentParser(description="Playback " +
                                                 "previously captured " +
                                                 "diagnostic logs ")

    parser.add_argument('-i', '--input', help='Capture file to playback')
    parser.add_argument('--hyd-input', help='HYD capture file to playback')
    parser.add_argument('-d', '--dst-ip', default='127.0.0.1',
                        help='Destination IP address for the playback')
    parser.add_argument('-p', '--port', default=NetworkReaderRaw.DEFAULT_PORT,
                        type=int,
                        help='Port to which to play back the messages')
    parser.add_argument('--hyd-port', default=NetworkReaderRaw.DEFAULT_HYD_PORT,
                        type=int,
                        help='Port to which to play back hyd messages')

    def ip_addr_mapper(s):
        try:
            ip1, ip2 = s.split(":")
            socket.inet_pton(socket.AF_INET, ip1)
            socket.inet_pton(socket.AF_INET, ip2)
            return (ip1, ip2)
        except:
            raise argparse.ArgumentTypeError(
                "IP mapping must be in format IPV4 address:IPV4 address")

    parser.add_argument('--ipmap', type=ip_addr_mapper, action='append')

    playback_group = parser.add_argument_group('Playback options')
    parser.add_argument('-s', '--speed', default=1, type=int,
                        help='Playback speed (for faster/slower playback')

    log_group = parser.add_argument_group('Logging options')
    log_group.add_argument('-v', '--verbose', action='store_true',
                           default=False,
                           help='Enable debug level logging')
    log_group.add_argument('-l', '--logfile', default=None,
                           help='Specify filename to use for debug logging')

    args = parser.parse_args()

    if not args.input and not args.hyd_input:
        raise argparse.ArgumentTypeError("Must provide at least a hyd or lbd input file")

    level = logging.INFO
    if args.verbose:
        level = logging.DEBUG

    format = '%(asctime)-15s %(levelname)-8s %(name)-15s %(message)s'
    logging.basicConfig(filename=args.logfile, level=level,
                        format=format)

    inputs = []
    if args.input:
        inputs.append((args.input, args.port))
    if args.hyd_input:
        inputs.append((args.hyd_input, args.hyd_port))

    playback_file(inputs, args.dst_ip, args.speed, args.ipmap)
