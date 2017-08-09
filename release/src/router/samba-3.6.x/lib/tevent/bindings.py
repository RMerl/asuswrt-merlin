#!/usr/bin/python
#
#   Python integration for tevent - tests
#
#   Copyright (C) Jelmer Vernooij 2010
#
#     ** NOTE! The following LGPL license applies to the tevent
#     ** library. This does NOT imply that all of Samba is released
#     ** under the LGPL
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 3 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public
#   License along with this library; if not, see <http://www.gnu.org/licenses/>.

import signal
import _tevent
from unittest import TestCase

class BackendListTests(TestCase):

    def test_backend_list(self):
        self.assertTrue(isinstance(_tevent.backend_list(), list))


class CreateContextTests(TestCase):

    def test_by_name(self):
        ctx = _tevent.Context(_tevent.backend_list()[0])
        self.assertTrue(ctx is not None)

    def test_no_name(self):
        ctx = _tevent.Context()
        self.assertTrue(ctx is not None)


class ContextTests(TestCase):

    def setUp(self):
        super(ContextTests, self).setUp()
        self.ctx = _tevent.Context()

    def test_signal_support(self):
        self.assertTrue(type(self.ctx.signal_support) is bool)

    def test_reinitialise(self):
        self.ctx.reinitialise()

    def test_loop_wait(self):
        self.ctx.loop_wait()

    def test_add_signal(self):
        sig = self.ctx.add_signal(signal.SIGINT, 0, lambda callback: None)
        self.assertTrue(isinstance(sig, _tevent.Signal))
