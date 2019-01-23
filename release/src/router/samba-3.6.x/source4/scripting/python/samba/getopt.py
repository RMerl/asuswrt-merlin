#!/usr/bin/env python

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

__docformat__ = "restructuredText"

import optparse, os
from samba.credentials import (
    Credentials,
    DONT_USE_KERBEROS,
    MUST_USE_KERBEROS,
    )
from samba.hostconfig import Hostconfig
import sys


class SambaOptions(optparse.OptionGroup):
    """General Samba-related command line options."""

    def __init__(self, parser):
        from samba.param import LoadParm
        optparse.OptionGroup.__init__(self, parser, "Samba Common Options")
        self.add_option("-s", "--configfile", action="callback",
                        type=str, metavar="FILE", help="Configuration file",
                        callback=self._load_configfile)
        self.add_option("-d", "--debuglevel", action="callback",
                        type=int, metavar="DEBUGLEVEL", help="debug level",
                        callback=self._set_debuglevel)
        self.add_option("--option", action="callback",
                        type=str, metavar="OPTION", help="set smb.conf option from command line",
                        callback=self._set_option)
        self.add_option("--realm", action="callback",
                        type=str, metavar="REALM", help="set the realm name",
                        callback=self._set_realm)
        self._configfile = None
        self._lp = LoadParm()

    def get_loadparm_path(self):
        """Return the path to the smb.conf file specified on the command line.  """
        return self._configfile

    def _load_configfile(self, option, opt_str, arg, parser):
        self._configfile = arg

    def _set_debuglevel(self, option, opt_str, arg, parser):
        self._lp.set('debug level', str(arg))

    def _set_realm(self, option, opt_str, arg, parser):
        self._lp.set('realm', arg)

    def _set_option(self, option, opt_str, arg, parser):
        if arg.find('=') == -1:
            print("--option takes a 'a=b' argument")
            sys.exit(1)
        a = arg.split('=')
        self._lp.set(a[0], a[1])

    def get_loadparm(self):
        """Return a loadparm object with data specified on the command line.  """
        if self._configfile is not None:
            self._lp.load(self._configfile)
        elif os.getenv("SMB_CONF_PATH") is not None:
            self._lp.load(os.getenv("SMB_CONF_PATH"))
        else:
            self._lp.load_default()
        return self._lp

    def get_hostconfig(self):
        return Hostconfig(self.get_loadparm())


class VersionOptions(optparse.OptionGroup):
    """Command line option for printing Samba version."""
    def __init__(self, parser):
        optparse.OptionGroup.__init__(self, parser, "Version Options")
        self.add_option("--version", action="callback",
                callback=self._display_version,
                help="Display version number")

    def _display_version(self, option, opt_str, arg, parser):
        import samba
        print samba.version
        sys.exit(0)


class CredentialsOptions(optparse.OptionGroup):
    """Command line options for specifying credentials."""
    def __init__(self, parser):
        self.no_pass = True
        self.ipaddress = None
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
        self.add_option("", "--ipaddress", metavar="IPADDRESS",
                        action="callback", type=str,
                        help="IP address of server", callback=self._set_ipaddress)
        self.creds = Credentials()

    def _parse_username(self, option, opt_str, arg, parser):
        self.creds.parse_string(arg)

    def _parse_workgroup(self, option, opt_str, arg, parser):
        self.creds.set_domain(arg)

    def _set_password(self, option, opt_str, arg, parser):
        self.creds.set_password(arg)
        self.no_pass = False

    def _set_ipaddress(self, option, opt_str, arg, parser):
        self.ipaddress = arg

    def _set_kerberos(self, option, opt_str, arg, parser):
        if arg.lower() in ["yes", 'true', '1']:
            self.creds.set_kerberos_state(MUST_USE_KERBEROS)
        elif arg.lower() in ["no", 'false', '0']:
            self.creds.set_kerberos_state(DONT_USE_KERBEROS)
        else:
            raise optparse.BadOptionErr("invalid kerberos option: %s" % arg)

    def _set_simple_bind_dn(self, option, opt_str, arg, parser):
        self.creds.set_bind_dn(arg)

    def get_credentials(self, lp, fallback_machine=False):
        """Obtain the credentials set on the command-line.

        :param lp: Loadparm object to use.
        :return: Credentials object
        """
        self.creds.guess(lp)
        if self.no_pass:
            self.creds.set_cmdline_callbacks()

        # possibly fallback to using the machine account, if we have
        # access to the secrets db
        if fallback_machine and not self.creds.authentication_requested():
            try:
                self.creds.set_machine_account(lp)
            except Exception:
                pass

        return self.creds

class CredentialsOptionsDouble(CredentialsOptions):
    """Command line options for specifying credentials of two servers."""
    def __init__(self, parser):
        CredentialsOptions.__init__(self, parser)
        self.no_pass2 = True
        self.add_option("--simple-bind-dn2", metavar="DN2", action="callback",
                        callback=self._set_simple_bind_dn2, type=str,
                        help="DN to use for a simple bind")
        self.add_option("--password2", metavar="PASSWORD2", action="callback",
                        help="Password", type=str, callback=self._set_password2)
        self.add_option("--username2", metavar="USERNAME2",
                        action="callback", type=str,
                        help="Username for second server", callback=self._parse_username2)
        self.add_option("--workgroup2", metavar="WORKGROUP2",
                        action="callback", type=str,
                        help="Workgroup for second server", callback=self._parse_workgroup2)
        self.add_option("--no-pass2", action="store_true",
                        help="Don't ask for a password for the second server")
        self.add_option("--kerberos2", metavar="KERBEROS2",
                        action="callback", type=str,
                        help="Use Kerberos", callback=self._set_kerberos2)
        self.creds2 = Credentials()

    def _parse_username2(self, option, opt_str, arg, parser):
        self.creds2.parse_string(arg)

    def _parse_workgroup2(self, option, opt_str, arg, parser):
        self.creds2.set_domain(arg)

    def _set_password2(self, option, opt_str, arg, parser):
        self.creds2.set_password(arg)
        self.no_pass2 = False

    def _set_kerberos2(self, option, opt_str, arg, parser):
        if bool(arg) or arg.lower() == "yes":
            self.creds2.set_kerberos_state(MUST_USE_KERBEROS)
        else:
            self.creds2.set_kerberos_state(DONT_USE_KERBEROS)

    def _set_simple_bind_dn2(self, option, opt_str, arg, parser):
        self.creds2.set_bind_dn(arg)

    def get_credentials2(self, lp, guess=True):
        """Obtain the credentials set on the command-line.

        :param lp: Loadparm object to use.
        :param guess: Try guess Credentials from environment
        :return: Credentials object
        """
        if guess:
            self.creds2.guess(lp)
        elif not self.creds2.get_username():
                self.creds2.set_anonymous()

        if self.no_pass2:
            self.creds2.set_cmdline_callbacks()
        return self.creds2
