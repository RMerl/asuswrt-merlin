#!/usr/bin/env python
#
# Vampire
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
    Option,
    SuperCommand,
    CommandError
    )

class cmd_vampire(Command):
    """Join and synchronise a remote AD domain to the local server [server connection needed]"""
    synopsis = "%prog vampire [options] <domain>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_options = [
        Option("--target-dir", help="Target directory.", type=str),
        Option("--force", help="force run", action='store_true', default=False),
        ]

    takes_args = ["domain"]

    def run(self, domain, target_dir=None, credopts=None, sambaopts=None, versionopts=None, force=False):
        if not force:
            raise CommandError("samba-tool vampire is deprecated, please use samba-tool join. Use --force to override")
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)
        net = Net(creds, lp, server=credopts.ipaddress)
        (domain_name, domain_sid) = net.vampire(domain=domain, target_dir=target_dir)
        self.outf.write("Vampired domain %s (%s)\n" % (domain_name, domain_sid))
