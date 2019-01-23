#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Unix SMB/CIFS implementation.
# Copyright © Jelmer Vernooij <jelmer@samba.org> 2008
#   
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#   
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#   
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""Tests for samba.messaging."""

from samba.messaging import Messaging
from samba.tests import TestCase

class MessagingTests(TestCase):

    def get_context(self, *args, **kwargs):
        kwargs["messaging_path"] = "."
        return Messaging(*args, **kwargs)
    
    def test_register(self):
        x = self.get_context()
        def callback():
            pass
        msg_type = x.register(callback)
        x.deregister(callback, msg_type)

    def test_assign_server_id(self):
        x = self.get_context()
        self.assertTrue(isinstance(x.server_id, tuple))
        self.assertEquals(3, len(x.server_id))

    def test_ping_speed(self):
        server_ctx = self.get_context((0, 1))
        def ping_callback(src, data):
                server_ctx.send(src, data)
        def exit_callback():
                print "received exit"
        msg_ping = server_ctx.register(ping_callback)
        msg_exit = server_ctx.register(exit_callback)

        def pong_callback():
                print "received pong"
        client_ctx = self.get_context((0, 2))
        msg_pong = client_ctx.register(pong_callback)

        client_ctx.send((0, 1), msg_ping, "testing")
        client_ctx.send((0, 1), msg_ping, "")

