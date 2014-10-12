#!/usr/bin/python
# Blackbox tests for ndrdump
# Copyright (C) 2008 Andrew Tridgell <tridge@samba.org>
# Copyright (C) 2008 Andrew Bartlett <abartlet@samba.org>
# Copyright (C) 2010 Jelmer Vernooij <jelmer@samba.org>
# based on test_smbclient.sh

"""Blackbox tests for ndrdump."""

import os
from samba.tests import BlackboxTestCase

for p in [ "../../../../../source4/librpc/tests", "../../../../../librpc/tests"]:
    data_path_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), p))
    print data_path_dir
    if os.path.exists(data_path_dir):
        break


class NdrDumpTests(BlackboxTestCase):
    """Blackbox tests for ndrdump."""

    def data_path(self, name):
        return os.path.join(data_path_dir, name)

    def test_ndrdump_with_in(self):
        self.check_run("ndrdump samr samr_CreateUser in %s" % (self.data_path("samr-CreateUser-in.dat")))

    def test_ndrdump_with_out(self):
        self.check_run("ndrdump samr samr_CreateUser out %s" % (self.data_path("samr-CreateUser-out.dat")))

    def test_ndrdump_context_file(self):
        self.check_run("ndrdump --context-file %s samr samr_CreateUser out %s" % (self.data_path("samr-CreateUser-in.dat"), self.data_path("samr-CreateUser-out.dat")))

    def test_ndrdump_with_validate(self):
        self.check_run("ndrdump --validate samr samr_CreateUser in %s" % (self.data_path("samr-CreateUser-in.dat")))
