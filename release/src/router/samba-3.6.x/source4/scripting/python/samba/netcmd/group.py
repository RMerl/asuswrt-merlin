#!/usr/bin/env python
#
# Adds a new user to a Samba4 server
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

import samba.getopt as options
from samba.netcmd import Command, SuperCommand, CommandError, Option
import ldb

from getpass import getpass
from samba.auth import system_session
from samba.samdb import SamDB
from samba.dsdb import (
    GTYPE_SECURITY_DOMAIN_LOCAL_GROUP,
    GTYPE_SECURITY_GLOBAL_GROUP,
    GTYPE_SECURITY_UNIVERSAL_GROUP,
    GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP,
    GTYPE_DISTRIBUTION_GLOBAL_GROUP,
    GTYPE_DISTRIBUTION_UNIVERSAL_GROUP,
)

security_group = dict({"Domain": GTYPE_SECURITY_DOMAIN_LOCAL_GROUP, "Global": GTYPE_SECURITY_GLOBAL_GROUP, "Universal": GTYPE_SECURITY_UNIVERSAL_GROUP})
distribution_group = dict({"Domain": GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP, "Global": GTYPE_DISTRIBUTION_GLOBAL_GROUP, "Universal": GTYPE_DISTRIBUTION_UNIVERSAL_GROUP})


class cmd_group_add(Command):
    """Creates a new group"""

    synopsis = "%prog group add [options] <groupname>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
        Option("--groupou",
	   help="Alternative location (without domainDN counterpart) to default CN=Users in which new user object will be created",
	   type=str),
        Option("--group-scope", type="choice", choices=["Domain", "Global", "Universal"],
            help="Group scope (Domain | Global | Universal)"),
        Option("--group-type", type="choice", choices=["Security", "Distribution"],
            help="Group type (Security | Distribution)"),
        Option("--description", help="Group's description", type=str),
        Option("--mail-address", help="Group's email address", type=str),
        Option("--notes", help="Groups's notes", type=str),
    ]

    takes_args = ["groupname"]

    def run(self, groupname, credopts=None, sambaopts=None,
            versionopts=None, H=None, groupou=None, group_scope=None,
            group_type=None, description=None, mail_address=None, notes=None):

        if (group_type or "Security") == "Security":
              gtype = security_group.get(group_scope, GTYPE_SECURITY_GLOBAL_GROUP)
        else:
              gtype = distribution_group.get(group_scope, GTYPE_DISTRIBUTION_GLOBAL_GROUP)

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)

        try:
            samdb = SamDB(url=H, session_info=system_session(),
                          credentials=creds, lp=lp)
            samdb.newgroup(groupname, groupou=groupou, grouptype = gtype,
                          description=description, mailaddress=mail_address, notes=notes)
        except Exception, e:
            raise CommandError('Failed to create group "%s"' % groupname, e)


class cmd_group_delete(Command):
    """Delete a group"""

    synopsis = "%prog group delete <groupname>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
    ]

    takes_args = ["groupname"]

    def run(self, groupname, credopts=None, sambaopts=None, versionopts=None, H=None):

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)

        try:
            samdb = SamDB(url=H, session_info=system_session(),
                          credentials=creds, lp=lp)
            samdb.deletegroup(groupname)
        except Exception, e:
            raise CommandError('Failed to remove group "%s"' % groupname, e)


class cmd_group_add_members(Command):
    """Add (comma-separated list of) group members"""

    synopsis = "%prog group addmembers <groupname> <listofmembers>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
    ]

    takes_args = ["groupname", "listofmembers"]

    def run(self, groupname, listofmembers, credopts=None, sambaopts=None,
            versionopts=None, H=None):

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)

        try:
            samdb = SamDB(url=H, session_info=system_session(),
                          credentials=creds, lp=lp)
            samdb.add_remove_group_members(groupname, listofmembers, add_members_operation=True)
        except Exception, e:
            raise CommandError('Failed to add members "%s" to group "%s"' % (listofmembers, groupname), e)


class cmd_group_remove_members(Command):
    """Remove (comma-separated list of) group members"""

    synopsis = "%prog group removemembers <groupname> <listofmembers>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
    ]

    takes_args = ["groupname", "listofmembers"]

    def run(self, groupname, listofmembers, credopts=None, sambaopts=None,
            versionopts=None, H=None):

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)

        try:
            samdb = SamDB(url=H, session_info=system_session(),
                          credentials=creds, lp=lp)
            samdb.add_remove_group_members(groupname, listofmembers, add_members_operation=False)
        except Exception, e:
            raise CommandError('Failed to remove members "%s" from group "%s"' % (listofmembers, groupname), e)


class cmd_group(SuperCommand):
    """Group management"""

    subcommands = {}
    subcommands["add"] = cmd_group_add()
    subcommands["delete"] = cmd_group_delete()
    subcommands["addmembers"] = cmd_group_add_members()
    subcommands["removemembers"] = cmd_group_remove_members()
