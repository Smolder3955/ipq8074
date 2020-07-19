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
import threading
import time
import socket

import mock

from whcdiag.io.network import NetworkReaderRaw


class TestNetworkReaderRaw(unittest.TestCase):

    def test_receive_msgs(self):
        """Verify the basic functionality of :meth:`receive_msgs`."""
        handler = mock.MagicMock(name='MsgHandler')

        with NetworkReaderRaw() as reader:
            # Spawn a thread to run the loop that receives messages.
            recv_thread = threading.Thread(target=reader.receive_msgs,
                                           args=(handler,))
            recv_thread.start()

            # Inject multiple messages and make sure they get dispatched to the
            # mock handler.
            send_sock = socket.socket(type=socket.SOCK_DGRAM)
            send_sock.bind(('localhost', 0))
            dest_addr = ('localhost', NetworkReaderRaw.DEFAULT_PORT)
            msgs = ['hello', 'world', 'third']
            for msg in msgs:
                send_sock.sendto(msg, dest_addr)

            # Need to make sure there's enough time for the receive thread to
            # process all of the messages.
            time.sleep(1)
            reader.terminate_receive()
            recv_thread.join()

        calls = handler.process_msg.call_args_list
        self.assertEquals(3, len(calls))
        for i in xrange(len(calls)):
            addr, data = calls[i][0]
            self.assertEquals(send_sock.getsockname(), addr)
            self.assertEquals(msgs[i], data)

if __name__ == '__main__':
    unittest.main()
