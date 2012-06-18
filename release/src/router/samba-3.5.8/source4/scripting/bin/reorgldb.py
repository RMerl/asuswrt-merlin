#!/usr/bin/python
#
# Copyright (C) Matthieu Patou <mat@matws.net> 2009
# This script realize an offline reorganisation of an LDB
# file it helps to reduce (sometime drastically) the
# size of LDB files.
import sys
import optparse
import os
sys.path.insert(0, "bin/python")

import samba
from samba.credentials import DONT_USE_KERBEROS
from samba.auth import system_session
from samba import Ldb, substitute_var, valid_netbios_name, check_all_substituted
from ldb import SCOPE_SUBTREE, SCOPE_ONELEVEL, SCOPE_BASE, LdbError
import ldb
import samba.getopt as options
from samba.samdb import SamDB
from samba import param
from samba.provision import ProvisionPaths, ProvisionNames,provision_paths_from_lp, Schema

parser = optparse.OptionParser("provision [options]")
sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)
parser.add_option_group(options.VersionOptions(parser))
credopts = options.CredentialsOptions(parser)
parser.add_option_group(credopts)
parser.add_option("--database", type="string", metavar="FILE",
        help="LDB to reorganize")
opts = parser.parse_args()[0]
lp = sambaopts.get_loadparm()
smbconf = lp.configfile

if not opts.database:
	print "Parameter database is mandatory"
	sys.exit(1)
creds = credopts.get_credentials(lp)
creds.set_kerberos_state(DONT_USE_KERBEROS)
session = system_session()
empty = ldb.Message()
newname="%s.new"%(opts.database)
if os.path.exists(newname):
	os.remove(newname)
old_ldb = Ldb(opts.database, session_info=session, credentials=creds,lp=lp)
new_ldb = Ldb(newname,session_info=session, credentials=creds,lp=lp)

new_ldb.transaction_start()
res = old_ldb.search(expression="(dn=*)",base="", scope=SCOPE_SUBTREE)
for i in range(0,len(res)):
	if str(res[i].dn) == "@BASEINFO":
		continue
	if str(res[i].dn).startswith("@INDEX:"):
		continue
	delta = new_ldb.msg_diff(empty,res[i])
	delta.dn = res[i].dn
	delta.remove("distinguishedName")
	new_ldb.add(delta)

new_ldb.transaction_commit()
