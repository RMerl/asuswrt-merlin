#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2008
#
# Based on the original in EJS:
# Copyright (C) Andrew Tridgell <tridge@samba.org> 2005
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

"""Samba 4."""

__docformat__ = "restructuredText"

import os
import sys

def source_tree_topdir():
    '''return the top level directory (the one containing the source4 directory)'''
    paths = [ "../../..", "../../../.." ]
    for p in paths:
        topdir = os.path.normpath(os.path.join(os.path.dirname(__file__), p))
        if os.path.exists(os.path.join(topdir, 'source4')):
            return topdir
    raise RuntimeError("unable to find top level source directory")

def in_source_tree():
    '''return True if we are running from within the samba source tree'''
    try:
        topdir = source_tree_topdir()
    except RuntimeError:
        return False
    return True



import ldb
from samba._ldb import Ldb as _Ldb

class Ldb(_Ldb):
    """Simple Samba-specific LDB subclass that takes care
    of setting up the modules dir, credentials pointers, etc.

    Please note that this is intended to be for all Samba LDB files,
    not necessarily the Sam database. For Sam-specific helper
    functions see samdb.py.
    """

    def __init__(self, url=None, lp=None, modules_dir=None, session_info=None,
                 credentials=None, flags=0, options=None):
        """Opens a Samba Ldb file.

        :param url: Optional LDB URL to open
        :param lp: Optional loadparm object
        :param modules_dir: Optional modules directory
        :param session_info: Optional session information
        :param credentials: Optional credentials, defaults to anonymous.
        :param flags: Optional LDB flags
        :param options: Additional options (optional)

        This is different from a regular Ldb file in that the Samba-specific
        modules-dir is used by default and that credentials and session_info
        can be passed through (required by some modules).
        """

        if modules_dir is not None:
            self.set_modules_dir(modules_dir)
        elif lp is not None:
            self.set_modules_dir(os.path.join(lp.get("modules dir"), "ldb"))

        if session_info is not None:
            self.set_session_info(session_info)

        if credentials is not None:
            self.set_credentials(credentials)

        if lp is not None:
            self.set_loadparm(lp)

        # This must be done before we load the schema, as these handlers for
        # objectSid and objectGUID etc must take precedence over the 'binary
        # attribute' declaration in the schema
        self.register_samba_handlers()

        # TODO set debug
        def msg(l, text):
            print text
        #self.set_debug(msg)

        self.set_utf8_casefold()

        # Allow admins to force non-sync ldb for all databases
        if lp is not None:
            nosync_p = lp.get("nosync", "ldb")
            if nosync_p is not None and nosync_p == True:
                flags |= ldb.FLG_NOSYNC

        self.set_create_perms(0600)

        if url is not None:
            self.connect(url, flags, options)

    def searchone(self, attribute, basedn=None, expression=None,
                  scope=ldb.SCOPE_BASE):
        """Search for one attribute as a string.

        :param basedn: BaseDN for the search.
        :param attribute: Name of the attribute
        :param expression: Optional search expression.
        :param scope: Search scope (defaults to base).
        :return: Value of attribute as a string or None if it wasn't found.
        """
        res = self.search(basedn, scope, expression, [attribute])
        if len(res) != 1 or res[0][attribute] is None:
            return None
        values = set(res[0][attribute])
        assert len(values) == 1
        return self.schema_format_value(attribute, values.pop())

    def erase_users_computers(self, dn):
        """Erases user and computer objects from our AD.

        This is needed since the 'samldb' module denies the deletion of primary
        groups. Therefore all groups shouldn't be primary somewhere anymore.
        """

        try:
            res = self.search(base=dn, scope=ldb.SCOPE_SUBTREE, attrs=[],
                      expression="(|(objectclass=user)(objectclass=computer))")
        except ldb.LdbError, (errno, _):
            if errno == ldb.ERR_NO_SUCH_OBJECT:
                # Ignore no such object errors
                return
            else:
                raise

        try:
            for msg in res:
                self.delete(msg.dn, ["relax:0"])
        except ldb.LdbError, (errno, _):
            if errno != ldb.ERR_NO_SUCH_OBJECT:
                # Ignore no such object errors
                raise

    def erase_except_schema_controlled(self):
        """Erase this ldb.

        :note: Removes all records, except those that are controlled by
            Samba4's schema.
        """

        basedn = ""

        # Try to delete user/computer accounts to allow deletion of groups
        self.erase_users_computers(basedn)

        # Delete the 'visible' records, and the invisble 'deleted' records (if this DB supports it)
        for msg in self.search(basedn, ldb.SCOPE_SUBTREE,
                       "(&(|(objectclass=*)(distinguishedName=*))(!(distinguishedName=@BASEINFO)))",
                       [], controls=["show_deleted:0", "show_recycled:0"]):
            try:
                self.delete(msg.dn, ["relax:0"])
            except ldb.LdbError, (errno, _):
                if errno != ldb.ERR_NO_SUCH_OBJECT:
                    # Ignore no such object errors
                    raise

        res = self.search(basedn, ldb.SCOPE_SUBTREE,
            "(&(|(objectclass=*)(distinguishedName=*))(!(distinguishedName=@BASEINFO)))", [], controls=["show_deleted:0", "show_recycled:0"])
        assert len(res) == 0

        # delete the specials
        for attr in ["@SUBCLASSES", "@MODULES",
                     "@OPTIONS", "@PARTITION", "@KLUDGEACL"]:
            try:
                self.delete(attr, ["relax:0"])
            except ldb.LdbError, (errno, _):
                if errno != ldb.ERR_NO_SUCH_OBJECT:
                    # Ignore missing dn errors
                    raise

    def erase(self):
        """Erase this ldb, removing all records."""
        self.erase_except_schema_controlled()

        # delete the specials
        for attr in ["@INDEXLIST", "@ATTRIBUTES"]:
            try:
                self.delete(attr, ["relax:0"])
            except ldb.LdbError, (errno, _):
                if errno != ldb.ERR_NO_SUCH_OBJECT:
                    # Ignore missing dn errors
                    raise

    def load_ldif_file_add(self, ldif_path):
        """Load a LDIF file.

        :param ldif_path: Path to LDIF file.
        """
        self.add_ldif(open(ldif_path, 'r').read())

    def add_ldif(self, ldif, controls=None):
        """Add data based on a LDIF string.

        :param ldif: LDIF text.
        """
        for changetype, msg in self.parse_ldif(ldif):
            assert changetype == ldb.CHANGETYPE_NONE
            self.add(msg, controls)

    def modify_ldif(self, ldif, controls=None):
        """Modify database based on a LDIF string.

        :param ldif: LDIF text.
        """
        for changetype, msg in self.parse_ldif(ldif):
            if changetype == ldb.CHANGETYPE_ADD:
                self.add(msg, controls)
            else:
                self.modify(msg, controls)


def substitute_var(text, values):
    """Substitute strings of the form ${NAME} in str, replacing
    with substitutions from values.

    :param text: Text in which to subsitute.
    :param values: Dictionary with keys and values.
    """

    for (name, value) in values.items():
        assert isinstance(name, str), "%r is not a string" % name
        assert isinstance(value, str), "Value %r for %s is not a string" % (value, name)
        text = text.replace("${%s}" % name, value)

    return text


def check_all_substituted(text):
    """Check that all substitution variables in a string have been replaced.

    If not, raise an exception.

    :param text: The text to search for substitution variables
    """
    if not "${" in text:
        return

    var_start = text.find("${")
    var_end = text.find("}", var_start)

    raise Exception("Not all variables substituted: %s" %
        text[var_start:var_end+1])


def read_and_sub_file(file_name, subst_vars):
    """Read a file and sub in variables found in it

    :param file_name: File to be read (typically from setup directory)
     param subst_vars: Optional variables to subsitute in the file.
    """
    data = open(file_name, 'r').read()
    if subst_vars is not None:
        data = substitute_var(data, subst_vars)
        check_all_substituted(data)
    return data


def setup_file(template, fname, subst_vars=None):
    """Setup a file in the private dir.

    :param template: Path of the template file.
    :param fname: Path of the file to create.
    :param subst_vars: Substitution variables.
    """
    if os.path.exists(fname):
        os.unlink(fname)

    data = read_and_sub_file(template, subst_vars)
    f = open(fname, 'w')
    try:
        f.write(data)
    finally:
        f.close()


def valid_netbios_name(name):
    """Check whether a name is valid as a NetBIOS name. """
    # See crh's book (1.4.1.1)
    if len(name) > 15:
        return False
    for x in name:
        if not x.isalnum() and not x in " !#$%&'()-.@^_{}~":
            return False
    return True


def import_bundled_package(modulename, location):
    """Import the bundled version of a package.

    :note: This should only be called if the system version of the package
        is not adequate.

    :param modulename: Module name to import
    :param location: Location to add to sys.path (can be relative to
        ${srcdir}/lib)
    """
    if in_source_tree():
        sys.path.insert(0, os.path.join(source_tree_topdir(), "lib", location))
        sys.modules[modulename] = __import__(modulename)
    else:
        sys.modules[modulename] = __import__(
            "samba.external.%s" % modulename, fromlist=["samba.external"])


def ensure_external_module(modulename, location):
    """Add a location to sys.path if an external dependency can't be found.

    :param modulename: Module name to import
    :param location: Location to add to sys.path (can be relative to
        ${srcdir}/lib)
    """
    try:
        __import__(modulename)
    except ImportError:
        import_bundled_package(modulename, location)


from samba import _glue
version = _glue.version
interface_ips = _glue.interface_ips
set_debug_level = _glue.set_debug_level
get_debug_level = _glue.get_debug_level
unix2nttime = _glue.unix2nttime
nttime2string = _glue.nttime2string
nttime2unix = _glue.nttime2unix
unix2nttime = _glue.unix2nttime
generate_random_password = _glue.generate_random_password
