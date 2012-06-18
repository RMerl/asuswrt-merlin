#!/usr/bin/python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2008
# Copyright (C) Matthias Dieter Wallnoefer 2009
#
# Based on the original in EJS:
# Copyright (C) Andrew Tridgell <tridge@samba.org> 2005
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

"""Convenience functions for using the SAM."""

import samba
import glue
import ldb
from samba.idmap import IDmapDB
import pwd
import time
import base64

__docformat__ = "restructuredText"

class SamDB(samba.Ldb):
    """The SAM database."""

    def __init__(self, url=None, lp=None, modules_dir=None, session_info=None,
                 credentials=None, flags=0, options=None):
        """Opens the SAM Database
        For parameter meanings see the super class (samba.Ldb)
        """

        self.lp = lp
        if url is None:
                url = lp.get("sam database")

        super(SamDB, self).__init__(url=url, lp=lp, modules_dir=modules_dir,
                session_info=session_info, credentials=credentials, flags=flags,
                options=options)

        glue.dsdb_set_global_schema(self)

    def connect(self, url=None, flags=0, options=None):
        super(SamDB, self).connect(url=self.lp.private_path(url), flags=flags,
                options=options)

    def domain_dn(self):
        # find the DNs for the domain
        res = self.search(base="",
                          scope=ldb.SCOPE_BASE,
                          expression="(defaultNamingContext=*)",
                          attrs=["defaultNamingContext"])
        assert(len(res) == 1 and res[0]["defaultNamingContext"] is not None)
        return res[0]["defaultNamingContext"][0]

    def enable_account(self, filter):
        """Enables an account
        
        :param filter: LDAP filter to find the user (eg samccountname=name)
        """
        res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                          expression=filter, attrs=["userAccountControl"])
        assert(len(res) == 1)
        user_dn = res[0].dn

        userAccountControl = int(res[0]["userAccountControl"][0])
        if (userAccountControl & 0x2):
            userAccountControl = userAccountControl & ~0x2 # remove disabled bit
        if (userAccountControl & 0x20):
            userAccountControl = userAccountControl & ~0x20 # remove 'no password required' bit

        mod = """
dn: %s
changetype: modify
replace: userAccountControl
userAccountControl: %u
""" % (user_dn, userAccountControl)
        self.modify_ldif(mod)
        
    def force_password_change_at_next_login(self, filter):
        """Forces a password change at next login
        
        :param filter: LDAP filter to find the user (eg samccountname=name)
        """
        res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                          expression=filter, attrs=[])
        assert(len(res) == 1)
        user_dn = res[0].dn

        mod = """
dn: %s
changetype: modify
replace: pwdLastSet
pwdLastSet: 0
""" % (user_dn)
        self.modify_ldif(mod)

    def newuser(self, username, unixname, password, force_password_change_at_next_login_req=False):
        """Adds a new user

        Note: This call adds also the ID mapping for winbind; therefore it works
        *only* on SAMBA 4.
        
        :param username: Name of the new user
        :param unixname: Name of the unix user to map to
        :param password: Password for the new user
        :param force_password_change_at_next_login_req: Force password change
        """
        self.transaction_start()
        try:
            user_dn = "CN=%s,CN=Users,%s" % (username, self.domain_dn())

            # The new user record. Note the reliance on the SAMLDB module which
            # fills in the default informations
            self.add({"dn": user_dn, 
                "sAMAccountName": username,
                "objectClass": "user"})

            # Sets the password for it
            self.setpassword("(dn=" + user_dn + ")", password,
              force_password_change_at_next_login_req)

            # Gets the user SID (for the account mapping setup)
            res = self.search(user_dn, scope=ldb.SCOPE_BASE,
                              expression="objectclass=*",
                              attrs=["objectSid"])
            assert len(res) == 1
            user_sid = self.schema_format_value("objectSid", res[0]["objectSid"][0])
            
            try:
                idmap = IDmapDB(lp=self.lp)

                user = pwd.getpwnam(unixname)

                # setup ID mapping for this UID
                idmap.setup_name_mapping(user_sid, idmap.TYPE_UID, user[2])

            except KeyError:
                pass
        except:
            self.transaction_cancel()
            raise
        self.transaction_commit()

    def setpassword(self, filter, password, force_password_change_at_next_login_req=False):
        """Sets the password for a user
        
        Note: This call uses the "userPassword" attribute to set the password.
        This works correctly on SAMBA 4 and on Windows DCs with
        "2003 Native" or higer domain function level.

        :param filter: LDAP filter to find the user (eg samccountname=name)
        :param password: Password for the user
        :param force_password_change_at_next_login_req: Force password change
        """
        self.transaction_start()
        try:
            res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                              expression=filter, attrs=[])
            assert(len(res) == 1)
            user_dn = res[0].dn

            setpw = """
dn: %s
changetype: modify
replace: userPassword
userPassword:: %s
""" % (user_dn, base64.b64encode(password))

            self.modify_ldif(setpw)

            if force_password_change_at_next_login_req:
                self.force_password_change_at_next_login(
                  "(dn=" + str(user_dn) + ")")

            #  modify the userAccountControl to remove the disabled bit
            self.enable_account(filter)
        except:
            self.transaction_cancel()
            raise
        self.transaction_commit()

    def setexpiry(self, filter, expiry_seconds, no_expiry_req=False):
        """Sets the account expiry for a user
        
        :param filter: LDAP filter to find the user (eg samccountname=name)
        :param expiry_seconds: expiry time from now in seconds
        :param no_expiry_req: if set, then don't expire password
        """
        self.transaction_start()
        try:
            res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                          expression=filter,
                          attrs=["userAccountControl", "accountExpires"])
            assert(len(res) == 1)
            user_dn = res[0].dn

            userAccountControl = int(res[0]["userAccountControl"][0])
            accountExpires     = int(res[0]["accountExpires"][0])
            if no_expiry_req:
                userAccountControl = userAccountControl | 0x10000
                accountExpires = 0
            else:
                userAccountControl = userAccountControl & ~0x10000
                accountExpires = glue.unix2nttime(expiry_seconds + int(time.time()))

            setexp = """
dn: %s
changetype: modify
replace: userAccountControl
userAccountControl: %u
replace: accountExpires
accountExpires: %u
""" % (user_dn, userAccountControl, accountExpires)

            self.modify_ldif(setexp)
        except:
            self.transaction_cancel()
            raise
        self.transaction_commit();

