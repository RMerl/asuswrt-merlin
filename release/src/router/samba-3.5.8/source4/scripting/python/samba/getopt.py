#!/usr/bin/python

# Samba-specific bits for optparse
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
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

"""Support for parsing Samba-related command-line options."""

import optparse
from credentials import Credentials, DONT_USE_KERBEROS, MUST_USE_KERBEROS
from hostconfig import Hostconfig

__docformat__ = "restructuredText"

class SambaOptions(optparse.OptionGroup):
    """General Samba-related command line options."""
    def __init__(self, parser):
        optparse.OptionGroup.__init__(self, parser, "Samba Common Options")
        self.add_option("-s", "--configfile", action="callback",
                        type=str, metavar="FILE", help="Configuration file", 
                        callback=self._load_configfile)
        self._configfile = None

    def get_loadparm_path(self):
        """Return the path to the smb.conf file specified on the command line.  """
        return self._configfile

    def _load_configfile(self, option, opt_str, arg, parser):
        self._configfile = arg

    def get_loadparm(self):
        """Return a loadparm object with data specified on the command line.  """
        import os, param
        lp = param.LoadParm()
        if self._configfile is not None:
            lp.load(self._configfile)
        elif os.getenv("SMB_CONF_PATH") is not None:
            lp.load(os.getenv("SMB_CONF_PATH"))
        else:
            lp.load_default()
        return lp

    def get_hostconfig(self):
        return Hostconfig(self.get_loadparm())


class VersionOptions(optparse.OptionGroup):
    """Command line option for printing Samba version."""
    def __init__(self, parser):
        optparse.OptionGroup.__init__(self, parser, "Version Options")


class CredentialsOptions(optparse.OptionGroup):
    """Command line options for specifying credentials."""
    def __init__(self, parser):
        self.no_pass = False
        optparse.OptionGroup.__init__(self, parser, "Credentials Options")
        self.add_option("--simple-bind-dn", metavar="DN", action="callback",
                        callback=self._set_simple_bind_dn, type=str,
                        help="DN to use for a simple bind")
        self.add_option("--password", metavar="PASSWORD", action="callback",
                        help="Password", type=str, callback=self._set_password)
        self.add_option("-U", "--username", metavar="USERNAME", 
                        action="callback", type=str,
                        help="Username", callback=self._parse_username)
        self.add_option("-W", "--workgroup", metavar="WORKGROUP", 
                        action="callback", type=str,
                        help="Workgroup", callback=self._parse_workgroup)
        self.add_option("-N", "--no-pass", action="store_true",
                        help="Don't ask for a password")
        self.add_option("-k", "--kerberos", metavar="KERBEROS", 
                        action="callback", type=str,
                        help="Use Kerberos", callback=self._set_kerberos)
        self.creds = Credentials()

    def _parse_username(self, option, opt_str, arg, parser):
        self.creds.parse_string(arg)

    def _parse_workgroup(self, option, opt_str, arg, parser):
        self.creds.set_domain(arg)

    def _set_password(self, option, opt_str, arg, parser):
        self.creds.set_password(arg)

    def _set_kerberos(self, option, opt_str, arg, parser):
        if bool(arg) or arg.lower() == "yes":
            self.creds.set_kerberos_state(MUST_USE_KERBEROS)
        else:
            self.creds.set_kerberos_state(DONT_USE_KERBEROS)

    def _set_simple_bind_dn(self, option, opt_str, arg, parser):
        self.creds.set_bind_dn(arg)

    def get_credentials(self, lp):
        """Obtain the credentials set on the command-line.

        :param lp: Loadparm object to use.
        :return: Credentials object
        """
        self.creds.guess(lp)
        if not self.no_pass:
            self.creds.set_cmdline_callbacks()
        return self.creds
