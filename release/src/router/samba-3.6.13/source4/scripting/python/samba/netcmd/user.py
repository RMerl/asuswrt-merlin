#!/usr/bin/env python
#
# user management
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

class cmd_user_add(Command):
    """Create a new user."""
    synopsis = "%prog user add <name> [<password>]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_args = ["name", "password?"]

    def run(self, name, password=None, credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp )
        net = Net(creds, lp, server=credopts.ipaddress)
        net.create_user(name)
        if password is not None:
            net.set_password(name, creds.get_domain(), password, creds)


class cmd_user_delete(Command):
    """Delete a user."""
    synopsis = "%prog user delete <name>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_args = ["name"]

    def run(self, name, credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)
        net = Net(creds, lp, server=credopts.ipaddress)
        net.delete_user(name)


class cmd_user(SuperCommand):
    """User management [server connection needed]"""

    subcommands = {}
    subcommands["add"] = cmd_user_add()
    subcommands["delete"] = cmd_user_delete()

