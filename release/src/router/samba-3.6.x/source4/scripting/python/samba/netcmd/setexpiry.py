#!/usr/bin/env python
#
# Sets the user password expiry on a Samba4 server
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

from samba.netcmd import Command, CommandError, Option

import samba.getopt as options

from samba.auth import system_session
from samba.samdb import SamDB

class cmd_setexpiry(Command):
    """Sets the expiration of a user account"""

    synopsis = "setexpiry [username] [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
        Option("--filter", help="LDAP Filter to set password on", type=str),
        Option("--days", help="Days to expiry", type=int),
        Option("--noexpiry", help="Password does never expire", action="store_true"),
    ]

    takes_args = ["username?"]

    def run(self, username=None, sambaopts=None, credopts=None,
            versionopts=None, H=None, filter=None, days=None, noexpiry=None):
        if username is None and filter is None:
            raise CommandError("Either the username or '--filter' must be specified!")

        if filter is None:
            filter = "(&(objectClass=user)(sAMAccountName=%s))" % (username)

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)

        if days is None:
            days = 0

        samdb = SamDB(url=H, session_info=system_session(),
            credentials=creds, lp=lp)

        samdb.setexpiry(filter, days*24*3600, no_expiry_req=noexpiry)
