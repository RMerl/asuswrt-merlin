#!/usr/bin/python

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

def _in_source_tree():
    """Check whether the script is being run from the source dir. """
    return os.path.exists("%s/../../../selftest/skip" % os.path.dirname(__file__))


# When running, in-tree, make sure bin/python is in the PYTHONPATH
if _in_source_tree():
    import sys
    srcdir = "%s/../../.." % os.path.dirname(__file__)
    sys.path.append("%s/bin/python" % srcdir)
    default_ldb_modules_dir = "%s/bin/modules/ldb" % srcdir
else:
    default_ldb_modules_dir = None


import ldb
import glue

class Ldb(ldb.Ldb):
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
        elif default_ldb_modules_dir is not None:
            self.set_modules_dir(default_ldb_modules_dir)
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
        glue.ldb_register_samba_handlers(self)

        # TODO set debug
        def msg(l,text):
            print text
        #self.set_debug(msg)

        glue.ldb_set_utf8_casefold(self)

        # Allow admins to force non-sync ldb for all databases
        if lp is not None:
            nosync_p = lp.get("nosync", "ldb")
            if nosync_p is not None and nosync_p == true:
                flags |= FLG_NOSYNC

        if url is not None:
            self.connect(url, flags, options)

    def set_credentials(self, credentials):
        glue.ldb_set_credentials(self, credentials)

    def set_session_info(self, session_info):
        glue.ldb_set_session_info(self, session_info)

    def set_loadparm(self, lp_ctx):
        glue.ldb_set_loadparm(self, lp_ctx)

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
        """Erases user and computer objects from our AD. This is needed since the 'samldb' module denies the deletion of primary groups. Therefore all groups shouldn't be primary somewhere anymore."""

        try:
            res = self.search(base=dn, scope=ldb.SCOPE_SUBTREE, attrs=[],
                      expression="(|(objectclass=user)(objectclass=computer))")
        except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
            # Ignore no such object errors
            return
            pass

        try:
            for msg in res:
                self.delete(msg.dn)
        except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
            # Ignore no such object errors
            return

    def erase_except_schema_controlled(self):
        """Erase this ldb, removing all records, except those that are controlled by Samba4's schema."""

        basedn = ""

        # Try to delete user/computer accounts to allow deletion of groups
        self.erase_users_computers(basedn)

        # Delete the 'visible' records, and the invisble 'deleted' records (if this DB supports it)
        for msg in self.search(basedn, ldb.SCOPE_SUBTREE, 
                               "(&(|(objectclass=*)(distinguishedName=*))(!(distinguishedName=@BASEINFO)))",
                               [], controls=["show_deleted:0"]):
            try:
                self.delete(msg.dn)
            except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
                # Ignore no such object errors
                pass
            
        res = self.search(basedn, ldb.SCOPE_SUBTREE, 
                          "(&(|(objectclass=*)(distinguishedName=*))(!(distinguishedName=@BASEINFO)))",
                          [], controls=["show_deleted:0"])
        assert len(res) == 0

        # delete the specials
        for attr in ["@SUBCLASSES", "@MODULES", 
                     "@OPTIONS", "@PARTITION", "@KLUDGEACL"]:
            try:
                self.delete(attr)
            except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
                # Ignore missing dn errors
                pass

    def erase(self):
        """Erase this ldb, removing all records."""
        
        self.erase_except_schema_controlled()

        # delete the specials
        for attr in ["@INDEXLIST", "@ATTRIBUTES"]:
            try:
                self.delete(attr)
            except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
                # Ignore missing dn errors
                pass

    def erase_partitions(self):
        """Erase an ldb, removing all records."""

        def erase_recursive(self, dn):
            try:
                res = self.search(base=dn, scope=ldb.SCOPE_ONELEVEL, attrs=[], 
                                  controls=["show_deleted:0"])
            except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
                # Ignore no such object errors
                return
                pass
            
            for msg in res:
                erase_recursive(self, msg.dn)

            try:
                self.delete(dn)
            except ldb.LdbError, (ldb.ERR_NO_SUCH_OBJECT, _):
                # Ignore no such object errors
                pass

        res = self.search("", ldb.SCOPE_BASE, "(objectClass=*)", 
                         ["namingContexts"])
        assert len(res) == 1
        if not "namingContexts" in res[0]:
            return
        for basedn in res[0]["namingContexts"]:
            # Try to delete user/computer accounts to allow deletion of groups
            self.erase_users_computers(basedn)
            # Try and erase from the bottom-up in the tree
            erase_recursive(self, basedn)

    def load_ldif_file_add(self, ldif_path):
        """Load a LDIF file.

        :param ldif_path: Path to LDIF file.
        """
        self.add_ldif(open(ldif_path, 'r').read())

    def add_ldif(self, ldif):
        """Add data based on a LDIF string.

        :param ldif: LDIF text.
        """
        for changetype, msg in self.parse_ldif(ldif):
            assert changetype == ldb.CHANGETYPE_NONE
            self.add(msg)

    def modify_ldif(self, ldif):
        """Modify database based on a LDIF string.

        :param ldif: LDIF text.
        """
        for changetype, msg in self.parse_ldif(ldif):
            self.modify(msg)

    def set_domain_sid(self, sid):
        """Change the domain SID used by this LDB.

        :param sid: The new domain sid to use.
        """
        glue.samdb_set_domain_sid(self, sid)

    def domain_sid(self):
        """Read the domain SID used by this LDB.

        """
        glue.samdb_get_domain_sid(self)

    def set_schema_from_ldif(self, pf, df):
        glue.dsdb_set_schema_from_ldif(self, pf, df)

    def set_schema_from_ldb(self, ldb):
        glue.dsdb_set_schema_from_ldb(self, ldb)

    def write_prefixes_from_schema(self):
        glue.dsdb_write_prefixes_from_schema_to_ldb(self)

    def convert_schema_to_openldap(self, target, mapping):
        return glue.dsdb_convert_schema_to_openldap(self, target, mapping)

    def set_invocation_id(self, invocation_id):
        """Set the invocation id for this SamDB handle.
        
        :param invocation_id: GUID of the invocation id.
        """
        glue.dsdb_set_ntds_invocation_id(self, invocation_id)

    def set_opaque_integer(self, name, value):
        """Set an integer as an opaque (a flag or other value) value on the database
        
        :param name: The name for the opaque value
        :param value: The integer value
        """
        glue.dsdb_set_opaque_integer(self, name, value)


def substitute_var(text, values):
    """substitute strings of the form ${NAME} in str, replacing
    with substitutions from subobj.
    
    :param text: Text in which to subsitute.
    :param values: Dictionary with keys and values.
    """

    for (name, value) in values.items():
        assert isinstance(name, str), "%r is not a string" % name
        assert isinstance(value, str), "Value %r for %s is not a string" % (value, name)
        text = text.replace("${%s}" % name, value)

    return text


def check_all_substituted(text):
    """Make sure that all substitution variables in a string have been replaced.
    If not, raise an exception.
    
    :param text: The text to search for substitution variables
    """
    if not "${" in text:
        return
    
    var_start = text.find("${")
    var_end = text.find("}", var_start)
    
    raise Exception("Not all variables substituted: %s" % text[var_start:var_end+1])


def valid_netbios_name(name):
    """Check whether a name is valid as a NetBIOS name. """
    # See crh's book (1.4.1.1)
    if len(name) > 15:
        return False
    for x in name:
        if not x.isalnum() and not x in " !#$%&'()-.@^_{}~":
            return False
    return True


def dom_sid_to_rid(sid_str):
    """Converts a domain SID to the relative RID.

    :param sid_str: The domain SID formatted as string
    """

    return glue.dom_sid_to_rid(sid_str)


version = glue.version

# "userAccountControl" flags
UF_NORMAL_ACCOUNT = glue.UF_NORMAL_ACCOUNT
UF_TEMP_DUPLICATE_ACCOUNT = glue.UF_TEMP_DUPLICATE_ACCOUNT
UF_SERVER_TRUST_ACCOUNT = glue.UF_SERVER_TRUST_ACCOUNT
UF_WORKSTATION_TRUST_ACCOUNT = glue.UF_WORKSTATION_TRUST_ACCOUNT
UF_INTERDOMAIN_TRUST_ACCOUNT = glue.UF_INTERDOMAIN_TRUST_ACCOUNT
UF_PASSWD_NOTREQD = glue.UF_PASSWD_NOTREQD
UF_ACCOUNTDISABLE = glue.UF_ACCOUNTDISABLE

# "groupType" flags
GTYPE_SECURITY_BUILTIN_LOCAL_GROUP = glue.GTYPE_SECURITY_BUILTIN_LOCAL_GROUP
GTYPE_SECURITY_GLOBAL_GROUP = glue.GTYPE_SECURITY_GLOBAL_GROUP
GTYPE_SECURITY_DOMAIN_LOCAL_GROUP = glue.GTYPE_SECURITY_DOMAIN_LOCAL_GROUP
GTYPE_SECURITY_UNIVERSAL_GROUP = glue.GTYPE_SECURITY_UNIVERSAL_GROUP
GTYPE_DISTRIBUTION_GLOBAL_GROUP = glue.GTYPE_DISTRIBUTION_GLOBAL_GROUP
GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP = glue.GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP
GTYPE_DISTRIBUTION_UNIVERSAL_GROUP = glue.GTYPE_DISTRIBUTION_UNIVERSAL_GROUP

# "sAMAccountType" flags
ATYPE_NORMAL_ACCOUNT = glue.ATYPE_NORMAL_ACCOUNT
ATYPE_WORKSTATION_TRUST = glue.ATYPE_WORKSTATION_TRUST
ATYPE_INTERDOMAIN_TRUST = glue.ATYPE_INTERDOMAIN_TRUST
ATYPE_SECURITY_GLOBAL_GROUP = glue.ATYPE_SECURITY_GLOBAL_GROUP
ATYPE_SECURITY_LOCAL_GROUP = glue.ATYPE_SECURITY_LOCAL_GROUP
ATYPE_SECURITY_UNIVERSAL_GROUP = glue.ATYPE_SECURITY_UNIVERSAL_GROUP
ATYPE_DISTRIBUTION_GLOBAL_GROUP = glue.ATYPE_DISTRIBUTION_GLOBAL_GROUP
ATYPE_DISTRIBUTION_LOCAL_GROUP = glue.ATYPE_DISTRIBUTION_LOCAL_GROUP
ATYPE_DISTRIBUTION_UNIVERSAL_GROUP = glue.ATYPE_DISTRIBUTION_UNIVERSAL_GROUP

# "domainFunctionality", "forestFunctionality" flags in the rootDSE */
DS_DOMAIN_FUNCTION_2000 = glue.DS_DOMAIN_FUNCTION_2000
DS_DOMAIN_FUNCTION_2003_MIXED = glue.DS_DOMAIN_FUNCTION_2003_MIXED
DS_DOMAIN_FUNCTION_2003 = glue.DS_DOMAIN_FUNCTION_2003
DS_DOMAIN_FUNCTION_2008 = glue.DS_DOMAIN_FUNCTION_2008
DS_DOMAIN_FUNCTION_2008_R2 = glue.DS_DOMAIN_FUNCTION_2008_R2

# "domainControllerFunctionality" flags in the rootDSE */
DS_DC_FUNCTION_2000 = glue.DS_DC_FUNCTION_2000
DS_DC_FUNCTION_2003 = glue.DS_DC_FUNCTION_2003
DS_DC_FUNCTION_2008 = glue.DS_DC_FUNCTION_2008
DS_DC_FUNCTION_2008_R2 = glue.DS_DC_FUNCTION_2008_R2

