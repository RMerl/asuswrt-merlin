#!/usr/bin/env python
#
# spn management
#
# Copyright Matthieu Patou mat@samba.org 2010
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
import ldb
import re
from samba import provision
from samba.samdb import SamDB
from samba.auth import system_session
from samba.netcmd import (
    Command,
    CommandError,
    SuperCommand,
    Option
    )

def _get_user_realm_domain(user):
    """ get the realm or the domain and the base user
        from user like:
        * username
        * DOMAIN\username
        * username@REALM
    """
    baseuser = user
    realm = ""
    domain = ""
    m = re.match(r"(\w+)\\(\w+$)", user)
    if m:
        domain = m.group(1)
        baseuser = m.group(2)
        return (baseuser.lower(), domain.upper(), realm)
    m = re.match(r"(\w+)@(\w+)", user)
    if m:
        baseuser = m.group(1)
        realm = m.group(2)
    return (baseuser.lower(), domain, realm.upper())

class cmd_spn_list(Command):
    """List spns of a given user."""
    synopsis = "%prog spn list <user>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_args = ["user"]

    def run(self, user, credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)
        paths = provision.provision_paths_from_lp(lp, lp.get("realm"))
        sam = SamDB(paths.samdb, session_info=system_session(),
                    credentials=creds, lp=lp)
        # TODO once I understand how, use the domain info to naildown
        # to the correct domain
        (cleaneduser, realm, domain) = _get_user_realm_domain(user)
        print cleaneduser
        res = sam.search(expression="samaccountname=%s" % cleaneduser,
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["servicePrincipalName"])
        if len(res) >0:
            spns = res[0].get("servicePrincipalName")
            found = False
            flag = ldb.FLAG_MOD_ADD
            if spns != None:
                print "User %s has the following servicePrincipalName: " %  str(res[0].dn)
                for e in spns:
                    print "\t %s" % (str(e))

            else:
                print "User %s has no servicePrincipalName" % str(res[0].dn)
        else:
            raise CommandError("User %s not found" % user)

class cmd_spn_add(Command):
    """Create a new spn."""
    synopsis = "%prog spn add [--force] <name> <user>"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }
    takes_options = [
        Option("--force", help="Force the addition of the spn"\
                               " even it exists already", action="store_true"),
            ]
    takes_args = ["name", "user"]

    def run(self, name, user,  force=False, credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)
        paths = provision.provision_paths_from_lp(lp, lp.get("realm"))
        sam = SamDB(paths.samdb, session_info=system_session(),
                    credentials=creds, lp=lp)
        res = sam.search(expression="servicePrincipalName=%s" % name,
                            scope=ldb.SCOPE_SUBTREE,
                            )
        if len(res) != 0  and not force:
            raise CommandError("Service principal %s already"
                                   " affected to another user" % name)

        (cleaneduser, realm, domain) = _get_user_realm_domain(user)
        res = sam.search(expression="samaccountname=%s" % cleaneduser,
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["servicePrincipalName"])
        if len(res) >0:
            res[0].dn
            msg = ldb.Message()
            spns = res[0].get("servicePrincipalName")
            tab = []
            found = False
            flag = ldb.FLAG_MOD_ADD
            if spns != None:
                for e in spns:
                    if str(e) == name:
                        found = True
                    tab.append(str(e))
                flag = ldb.FLAG_MOD_REPLACE
            tab.append(name)
            msg.dn = res[0].dn
            msg["servicePrincipalName"] = ldb.MessageElement(tab, flag,
                                                "servicePrincipalName")
            if not found:
                sam.modify(msg)
            else:
                raise CommandError("Service principal %s already"
                                       " affected to %s" % (name, user))
        else:
            raise CommandError("User %s not found" % user)


class cmd_spn_delete(Command):
    """Delete a spn."""
    synopsis = "%prog spn delete <name> [user]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_args = ["name", "user?"]

    def run(self, name, user=None, credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)
        paths = provision.provision_paths_from_lp(lp, lp.get("realm"))
        sam = SamDB(paths.samdb, session_info=system_session(),
                    credentials=creds, lp=lp)
        res = sam.search(expression="servicePrincipalName=%s" % name,
                            scope=ldb.SCOPE_SUBTREE,
                            attrs=["servicePrincipalName", "samAccountName"])
        if len(res) >0:
            result = None
            if user is not None:
                (cleaneduser, realm, domain) = _get_user_realm_domain(user)
                for elem in res:
                    if str(elem["samAccountName"]).lower() == cleaneduser:
                        result = elem
                if result is None:
                    raise CommandError("Unable to find user %s with"
                                           " spn %s" % (user, name))
            else:
                if len(res) != 1:
                    listUser = ""
                    for r in res:
                        listUser = "%s\n%s" % (listUser, str(r.dn))
                    raise CommandError("More than one user has the spn %s "\
                           "and no specific user was specified, list of users"\
                           " with this spn:%s" % (name, listUser))
                else:
                    result=res[0]


            msg = ldb.Message()
            spns = result.get("servicePrincipalName")
            tab = []
            if spns != None:
                for e in spns:
                    if str(e) != name:
                        tab.append(str(e))
                flag = ldb.FLAG_MOD_REPLACE
            msg.dn = result.dn
            msg["servicePrincipalName"] = ldb.MessageElement(tab, flag,
                                            "servicePrincipalName")
            sam.modify(msg)
        else:
            raise CommandError("Service principal %s not affected" % name)

class cmd_spn(SuperCommand):
    """SPN management [server connection needed]"""

    subcommands = {}
    subcommands["add"] = cmd_spn_add()
    subcommands["list"] = cmd_spn_list()
    subcommands["delete"] = cmd_spn_delete()

