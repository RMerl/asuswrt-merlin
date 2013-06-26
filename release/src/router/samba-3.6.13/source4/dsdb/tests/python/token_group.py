#!/usr/bin/env python
# -*- coding: utf-8 -*-
# test tokengroups attribute against internal token calculation

import optparse
import sys
import os

sys.path.insert(0, "bin/python")
import samba
samba.ensure_external_module("testtools", "testtools")
samba.ensure_external_module("subunit", "subunit/python")

import samba.getopt as options

from samba.auth import system_session
from samba import ldb
from samba.samdb import SamDB
from samba.auth import AuthContext
from samba.ndr import ndr_pack, ndr_unpack
from samba import gensec
from samba.credentials import Credentials

from subunit.run import SubunitTestRunner
import unittest
import samba.tests

from samba.dcerpc import security
from samba.auth import AUTH_SESSION_INFO_DEFAULT_GROUPS, AUTH_SESSION_INFO_AUTHENTICATED, AUTH_SESSION_INFO_SIMPLE_PRIVILEGES


parser = optparse.OptionParser("ldap.py [options] <host>")
sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)
parser.add_option_group(options.VersionOptions(parser))
# use command line creds if available
credopts = options.CredentialsOptions(parser)
parser.add_option_group(credopts)
opts, args = parser.parse_args()

if len(args) < 1:
    parser.print_usage()
    sys.exit(1)

url = args[0]

lp = sambaopts.get_loadparm()
creds = credopts.get_credentials(lp)

class TokenTest(samba.tests.TestCase):

    def setUp(self):
        super(TokenTest, self).setUp()
        self.ldb = samdb
        self.base_dn = samdb.domain_dn()

        res = self.ldb.search("", scope=ldb.SCOPE_BASE, attrs=["tokenGroups"])
        self.assertEquals(len(res), 1)

        self.user_sid_dn = "<SID=%s>" % str(ndr_unpack(samba.dcerpc.security.dom_sid, res[0]["tokenGroups"][0]))

        session_info_flags = ( AUTH_SESSION_INFO_DEFAULT_GROUPS |
                               AUTH_SESSION_INFO_AUTHENTICATED |
                               AUTH_SESSION_INFO_SIMPLE_PRIVILEGES)
        session = samba.auth.user_session(self.ldb, lp_ctx=lp, dn=self.user_sid_dn,
                                          session_info_flags=session_info_flags)

        token = session.security_token
        self.user_sids = []
        for s in token.sids:
            self.user_sids.append(str(s))

    def test_rootDSE_tokenGroups(self):
        """Testing rootDSE tokengroups against internal calculation"""
        if not url.startswith("ldap"):
            self.fail(msg="This test is only valid on ldap")

        res = self.ldb.search("", scope=ldb.SCOPE_BASE, attrs=["tokenGroups"])
        self.assertEquals(len(res), 1)

        print("Geting tokenGroups from rootDSE")
        tokengroups = []
        for sid in res[0]['tokenGroups']:
            tokengroups.append(str(ndr_unpack(samba.dcerpc.security.dom_sid, sid)))

        sidset1 = set(tokengroups)
        sidset2 = set(self.user_sids)
        if len(sidset1.difference(sidset2)):
            print("token sids don't match")
            print("tokengroups: %s" % tokengroups)
            print("calculated : %s" % self.user_sids);
            print("difference : %s" % sidset1.difference(sidset2))
            self.fail(msg="calculated groups don't match against rootDSE tokenGroups")

    def test_dn_tokenGroups(self):
        print("Geting tokenGroups from user DN")
        res = self.ldb.search(self.user_sid_dn, scope=ldb.SCOPE_BASE, attrs=["tokenGroups"])
        self.assertEquals(len(res), 1)

        dn_tokengroups = []
        for sid in res[0]['tokenGroups']:
            dn_tokengroups.append(str(ndr_unpack(samba.dcerpc.security.dom_sid, sid)))

        sidset1 = set(dn_tokengroups)
        sidset2 = set(self.user_sids)
        if len(sidset1.difference(sidset2)):
            print("token sids don't match")
            print("tokengroups: %s" % tokengroups)
            print("calculated : %s" % sids);
            print("difference : %s" % sidset1.difference(sidset2))
            self.fail(msg="calculated groups don't match against user DN tokenGroups")
        
    def test_pac_groups(self):
        settings = {}
        settings["lp_ctx"] = lp
        settings["target_hostname"] = lp.get("netbios name")

        gensec_client = gensec.Security.start_client(settings)
        gensec_client.set_credentials(creds)
        gensec_client.want_feature(gensec.FEATURE_SEAL)
        gensec_client.start_mech_by_sasl_name("GSSAPI")

        auth_context = AuthContext(lp_ctx=lp, ldb=self.ldb, methods=[])

        gensec_server = gensec.Security.start_server(settings, auth_context)
        machine_creds = Credentials()
        machine_creds.guess(lp)
        machine_creds.set_machine_account(lp)
        gensec_server.set_credentials(machine_creds)

        gensec_server.want_feature(gensec.FEATURE_SEAL)
        gensec_server.start_mech_by_sasl_name("GSSAPI")

        client_finished = False
        server_finished = False
        server_to_client = ""
        
        """Run the actual call loop"""
        while client_finished == False and server_finished == False:
            if not client_finished:
                print "running client gensec_update"
                (client_finished, client_to_server) = gensec_client.update(server_to_client)
            if not server_finished:
                print "running server gensec_update"
                (server_finished, server_to_client) = gensec_server.update(client_to_server)

        session = gensec_server.session_info()

        token = session.security_token
        pac_sids = []
        for s in token.sids:
            pac_sids.append(str(s))

        sidset1 = set(pac_sids)
        sidset2 = set(self.user_sids)
        if len(sidset1.difference(sidset2)):
            print("token sids don't match")
            print("tokengroups: %s" % tokengroups)
            print("calculated : %s" % sids);
            print("difference : %s" % sidset1.difference(sidset2))
            self.fail(msg="calculated groups don't match against user PAC tokenGroups")


if not "://" in url:
    if os.path.isfile(url):
        url = "tdb://%s" % url
    else:
        url = "ldap://%s" % url

samdb = SamDB(url, credentials=creds, session_info=system_session(lp), lp=lp)

runner = SubunitTestRunner()
rc = 0
if not runner.run(unittest.makeSuite(TokenTest)).wasSuccessful():
    rc = 1
sys.exit(rc)
