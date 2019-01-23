#!/usr/bin/env python
#
# joins
# 
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

from samba.net import Net, LIBNET_JOIN_AUTOMATIC
from samba.netcmd import Command, CommandError, Option
from samba.dcerpc.misc import SEC_CHAN_WKSTA, SEC_CHAN_BDC
from samba.join import join_RODC, join_DC

class cmd_join(Command):
    """Joins domain as either member or backup domain controller [server connection needed]"""

    synopsis = "%prog join <dnsdomain> [DC | RODC | MEMBER] [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("--server", help="DC to join", type=str),
        Option("--site", help="site to join", type=str),
        ]

    takes_args = ["domain", "role?"]

    def run(self, domain, role=None, sambaopts=None, credopts=None,
            versionopts=None, server=None, site=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)
        net = Net(creds, lp, server=credopts.ipaddress)

        if site is None:
            site = "Default-First-Site-Name"

        netbios_name = lp.get("netbios name")

        if not role is None:
            role = role.upper()

        if role is None or role == "MEMBER":
            secure_channel_type = SEC_CHAN_WKSTA
        elif role == "DC":
            join_DC(server=server, creds=creds, lp=lp, domain=domain,
                    site=site, netbios_name=netbios_name)
            return
        elif role == "RODC":
            join_RODC(server=server, creds=creds, lp=lp, domain=domain,
                      site=site, netbios_name=netbios_name)
            return
        else:
            raise CommandError("Invalid role %s (possible values: MEMBER, BDC, RODC)" % role)

        (join_password, sid, domain_name) = net.join(domain,
                                                     netbios_name,
                                                     secure_channel_type,
                                                     LIBNET_JOIN_AUTOMATIC)

        self.outf.write("Joined domain %s (%s)\n" % (domain_name, sid))
