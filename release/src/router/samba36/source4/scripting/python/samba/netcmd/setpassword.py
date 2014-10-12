#!/usr/bin/env python
#
# Sets a user password on a Samba4 server
# Copyright Jelmer Vernooij 2008
#
# Based on the original in EJS:
# Copyright Andrew Tridgell 2005
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

import samba.getopt as options
from samba.netcmd import Command, CommandError, Option
from getpass import getpass
from samba.auth import system_session
from samba.samdb import SamDB
from samba import gensec
import ldb

class cmd_setpassword(Command):
    """(Re)sets the password on a user account"""

    synopsis = "setpassword [username] [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
        Option("--filter", help="LDAP Filter to set password on", type=str),
        Option("--newpassword", help="Set password", type=str),
        Option("--must-change-at-next-login",
            help="Force password to be changed on next login",
            action="store_true"),
        ]

    takes_args = ["username?"]

    def run(self, username=None, filter=None, credopts=None, sambaopts=None,
            versionopts=None, H=None, newpassword=None,
            must_change_at_next_login=None):
        if filter is None and username is None:
            raise CommandError("Either the username or '--filter' must be specified!")

        password = newpassword
        if password is None:
            password = getpass("New Password: ")

        if filter is None:
            filter = "(&(objectClass=user)(sAMAccountName=%s))" % (username)

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)

        creds.set_gensec_features(creds.get_gensec_features() | gensec.FEATURE_SEAL)

        samdb = SamDB(url=H, session_info=system_session(),
                      credentials=creds, lp=lp)

        try:
            samdb.setpassword(filter, password,
                              force_change_at_next_login=must_change_at_next_login,
                              username=username)
        except Exception, e:
            raise CommandError('Failed to set password for user "%s"' % username, e)
        print "Changed password OK"
