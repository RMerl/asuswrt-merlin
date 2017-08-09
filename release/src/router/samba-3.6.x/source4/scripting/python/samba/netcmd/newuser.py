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
from samba.netcmd import Command, CommandError, Option
import ldb

from getpass import getpass
from samba.auth import system_session
from samba.samdb import SamDB

class cmd_newuser(Command):
    """Creates a new user"""

    synopsis = "newuser [options] <username> [<password>]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptions,
    }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
        Option("--must-change-at-next-login",
            help="Force password to be changed on next login",
            action="store_true"),
        Option("--use-username-as-cn",
            help="Force use of username as user's CN",
            action="store_true"),
	Option("--userou",
	   help="Alternative location (without domainDN counterpart) to default CN=Users in which new user object will be created",
	   type=str),
        Option("--surname", help="User's surname", type=str),
        Option("--given-name", help="User's given name", type=str),
        Option("--initials", help="User's initials", type=str),
        Option("--profile-path", help="User's profile path", type=str),
        Option("--script-path", help="User's logon script path", type=str),
        Option("--home-drive", help="User's home drive letter", type=str),
        Option("--home-directory", help="User's home directory path", type=str),
        Option("--job-title", help="User's job title", type=str),
        Option("--department", help="User's department", type=str),
        Option("--company", help="User's company", type=str),
        Option("--description", help="User's description", type=str),
        Option("--mail-address", help="User's email address", type=str),
        Option("--internet-address", help="User's home page", type=str),
        Option("--telephone-number", help="User's phone number", type=str),
        Option("--physical-delivery-office", help="User's office location", type=str),
    ]

    takes_args = ["username", "password?"]

    def run(self, username, password=None, credopts=None, sambaopts=None,
            versionopts=None, H=None, must_change_at_next_login=None,
            use_username_as_cn=None, userou=None, surname=None, given_name=None, initials=None,
            profile_path=None, script_path=None, home_drive=None, home_directory=None,
            job_title=None, department=None, company=None, description=None,
            mail_address=None, internet_address=None, telephone_number=None, physical_delivery_office=None):

        if password is None:
            password = getpass("New Password: ")

        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp)

        try:
            samdb = SamDB(url=H, session_info=system_session(),
                          credentials=creds, lp=lp)
            samdb.newuser(username, password,
                          force_password_change_at_next_login_req=must_change_at_next_login,
                          useusernameascn=use_username_as_cn, userou=userou, surname=surname, givenname=given_name, initials=initials,
                          profilepath=profile_path, homedrive=home_drive, scriptpath=script_path, homedirectory=home_directory,
                          jobtitle=job_title, department=department, company=company, description=description,
                          mailaddress=mail_address, internetaddress=internet_address,
                          telephonenumber=telephone_number, physicaldeliveryoffice=physical_delivery_office)
        except Exception, e:
            raise CommandError('Failed to create user "%s"' % username, e)

        print("User %s created successfully" % username)
