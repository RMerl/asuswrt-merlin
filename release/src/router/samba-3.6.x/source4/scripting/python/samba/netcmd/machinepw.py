#!/usr/bin/env python
#
# Machine passwords
# Copyright Jelmer Vernooij 2010
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

from samba import Ldb
from samba.auth import system_session
from samba.netcmd import Command, CommandError


class cmd_machinepw(Command):
    """Gets a machine password out of our SAM"""

    synopsis = "%prog machinepw <accountname>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_args = ["secret"]

    def run(self, secret, sambaopts=None, credopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)
        url = lp.get("secrets database")
        secretsdb = Ldb(url=url, session_info=system_session(),
            credentials=creds, lp=lp)
        
        result = secretsdb.search(attrs=["secret"], 
            expression="(&(objectclass=primaryDomain)(samaccountname=%s))" % secret)

        if len(result) != 1:
            raise CommandError("search returned %d records, expected 1" % len(result))

        self.outf.write("%s\n" % result[0]["secret"])
