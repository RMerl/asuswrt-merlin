#!/usr/bin/env python
#
# Export keytab
#
# Copyright Jelmer Vernooij 2010 <jelmer@samba.org>
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

from samba.net import Net

from samba.netcmd import (
    Command,
    SuperCommand,
    )

class cmd_export_keytab(Command):
    """Dumps kerberos keys of the domain into a keytab"""
    synopsis = "%prog export keytab <keytab>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_options = [
        ]

    takes_args = ["keytab"]

    def run(self, keytab, credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        net = Net(None, lp, server=credopts.ipaddress)
        net.export_keytab(keytab=keytab)


class cmd_export(SuperCommand):
    """Dumps the sam of the domain we are joined to [server connection needed]"""

    subcommands = {}
    subcommands["keytab"] = cmd_export_keytab()

