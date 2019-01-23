#!/usr/bin/env python

# Unix SMB/CIFS implementation. Tests for ntacls manipulation
# Copyright (C) Matthieu Patou <mat@matws.net> 2009-2010
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

"""Tests for samba.ntacls."""

from samba.ntacls import setntacl, getntacl, XattrBackendError
from samba.dcerpc import xattr, security
from samba.param import LoadParm
from samba.tests import TestCase, TestSkipped
import random
import os

class NtaclsTests(TestCase):

    def test_setntacl(self):
        random.seed()
        lp = LoadParm()
        path = os.environ['SELFTEST_PREFIX']
        acl = "O:S-1-5-21-2212615479-2695158682-2101375467-512G:S-1-5-21-2212615479-2695158682-2101375467-513D:(A;OICI;0x001f01ff;;;S-1-5-21-2212615479-2695158682-2101375467-512)"
        tempf = os.path.join(path,"pytests"+str(int(100000*random.random())))
        ntacl = xattr.NTACL()
        ntacl.version = 1
        open(tempf, 'w').write("empty")
        lp.set("posix:eadb",os.path.join(path,"eadbtest.tdb"))
        setntacl(lp, tempf, acl, "S-1-5-21-2212615479-2695158682-2101375467")
        os.unlink(tempf)

    def test_setntacl_getntacl(self):
        random.seed()
        lp = LoadParm()
        path = None
        path = os.environ['SELFTEST_PREFIX']
        acl = "O:S-1-5-21-2212615479-2695158682-2101375467-512G:S-1-5-21-2212615479-2695158682-2101375467-513D:(A;OICI;0x001f01ff;;;S-1-5-21-2212615479-2695158682-2101375467-512)"
        tempf = os.path.join(path,"pytests"+str(int(100000*random.random())))
        ntacl = xattr.NTACL()
        ntacl.version = 1
        open(tempf, 'w').write("empty")
        lp.set("posix:eadb",os.path.join(path,"eadbtest.tdb"))
        setntacl(lp,tempf,acl,"S-1-5-21-2212615479-2695158682-2101375467")
        facl = getntacl(lp,tempf)
        anysid = security.dom_sid(security.SID_NT_SELF)
        self.assertEquals(facl.info.as_sddl(anysid),acl)
        os.unlink(tempf)

    def test_setntacl_getntacl_param(self):
        random.seed()
        lp = LoadParm()
        acl = "O:S-1-5-21-2212615479-2695158682-2101375467-512G:S-1-5-21-2212615479-2695158682-2101375467-513D:(A;OICI;0x001f01ff;;;S-1-5-21-2212615479-2695158682-2101375467-512)"
        path = os.environ['SELFTEST_PREFIX']
        tempf = os.path.join(path,"pytests"+str(int(100000*random.random())))
        ntacl = xattr.NTACL()
        ntacl.version = 1
        open(tempf, 'w').write("empty")
        setntacl(lp,tempf,acl,"S-1-5-21-2212615479-2695158682-2101375467","tdb",os.path.join(path,"eadbtest.tdb"))
        facl=getntacl(lp,tempf,"tdb",os.path.join(path,"eadbtest.tdb"))
        domsid=security.dom_sid(security.SID_NT_SELF)
        self.assertEquals(facl.info.as_sddl(domsid),acl)
        os.unlink(tempf)

    def test_setntacl_invalidbackend(self):
        random.seed()
        lp = LoadParm()
        acl = "O:S-1-5-21-2212615479-2695158682-2101375467-512G:S-1-5-21-2212615479-2695158682-2101375467-513D:(A;OICI;0x001f01ff;;;S-1-5-21-2212615479-2695158682-2101375467-512)"
        path = os.environ['SELFTEST_PREFIX']
        tempf = os.path.join(path,"pytests"+str(int(100000*random.random())))
        ntacl = xattr.NTACL()
        ntacl.version = 1
        open(tempf, 'w').write("empty")
        self.assertRaises(XattrBackendError, setntacl, lp, tempf, acl, "S-1-5-21-2212615479-2695158682-2101375467","ttdb", os.path.join(path,"eadbtest.tdb"))

    def test_setntacl_forcenative(self):
        if os.getuid() == 0:
            raise TestSkipped("Running test as root, test skipped")
        random.seed()
        lp = LoadParm()
        acl = "O:S-1-5-21-2212615479-2695158682-2101375467-512G:S-1-5-21-2212615479-2695158682-2101375467-513D:(A;OICI;0x001f01ff;;;S-1-5-21-2212615479-2695158682-2101375467-512)"
        path = os.environ['SELFTEST_PREFIX']
        tempf = os.path.join(path,"pytests"+str(int(100000*random.random())))
        ntacl = xattr.NTACL()
        ntacl.version = 1
        open(tempf, 'w').write("empty")
        lp.set("posix:eadb", os.path.join(path,"eadbtest.tdb"))
        self.assertRaises(Exception, setntacl, lp, tempf ,acl,
            "S-1-5-21-2212615479-2695158682-2101375467","native")
        os.unlink(tempf)
