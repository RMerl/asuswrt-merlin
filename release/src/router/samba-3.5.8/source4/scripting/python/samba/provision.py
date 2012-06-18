#
# Unix SMB/CIFS implementation.
# backend code for provisioning a Samba4 server

# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2008
# Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008-2009
# Copyright (C) Oliver Liebel <oliver@itc.li> 2008-2009
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

"""Functions for setting up a Samba configuration."""

from base64 import b64encode
import os
import sys
import pwd
import grp
import time
import uuid, glue
import socket
import param
import registry
import samba
import subprocess
import ldb

import shutil
from credentials import Credentials, DONT_USE_KERBEROS
from auth import system_session, admin_session
from samba import version, Ldb, substitute_var, valid_netbios_name
from samba import check_all_substituted
from samba import DS_DOMAIN_FUNCTION_2000, DS_DOMAIN_FUNCTION_2008, DS_DC_FUNCTION_2008, DS_DC_FUNCTION_2008_R2
from samba.samdb import SamDB
from samba.idmap import IDmapDB
from samba.dcerpc import security
from samba.ndr import ndr_pack
import urllib
from ldb import SCOPE_SUBTREE, SCOPE_ONELEVEL, SCOPE_BASE, LdbError, timestring
from ms_schema import read_ms_schema
from ms_display_specifiers import read_ms_ldif
from signal import SIGTERM
from dcerpc.misc import SEC_CHAN_BDC, SEC_CHAN_WKSTA

__docformat__ = "restructuredText"

def find_setup_dir():
    """Find the setup directory used by provision."""
    dirname = os.path.dirname(__file__)
    if "/site-packages/" in dirname:
        prefix = "/".join(dirname[:dirname.index("/site-packages/")].split("/")[:-2])
        for suffix in ["share/setup", "share/samba/setup", "setup"]:
            ret = os.path.join(prefix, suffix)
            if os.path.isdir(ret):
                return ret
    # In source tree
    ret = os.path.join(dirname, "../../../setup")
    if os.path.isdir(ret):
        return ret
    raise Exception("Unable to find setup directory.")

def get_schema_descriptor(domain_sid):
    sddl = "O:SAG:SAD:(A;CI;RPLCLORC;;;AU)(A;CI;RPWPCRCCLCLORCWOWDSW;;;SA)" \
           "(A;CI;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)" \
           "(OA;;CR;1131f6ad-9c07-11d1-f79f-00c04fc2dcd2;;ED)" \
           "(OA;;CR;89e95b76-444d-4c62-991a-0facbeda640c;;ED)" \
           "(OA;;CR;1131f6ad-9c07-11d1-f79f-00c04fc2dcd2;;BA)" \
           "(OA;;CR;89e95b76-444d-4c62-991a-0facbeda640c;;BA)" \
           "S:(AU;SA;WPCCDCWOWDSDDTSW;;;WD)" \
           "(AU;CISA;WP;;;WD)(AU;SA;CR;;;BA)" \
           "(AU;SA;CR;;;DU)(OU;SA;CR;e12b56b6-0a95-11d1-adbb-00c04fd8d5cd;;WD)" \
           "(OU;SA;CR;45ec5156-db7e-47bb-b53f-dbeb2d03c40f;;WD)"
    sec = security.descriptor.from_sddl(sddl, domain_sid)
    return b64encode(ndr_pack(sec))

def get_config_descriptor(domain_sid):
    sddl = "O:EAG:EAD:(OA;;CR;1131f6aa-9c07-11d1-f79f-00c04fc2dcd2;;ED)" \
           "(OA;;CR;1131f6ab-9c07-11d1-f79f-00c04fc2dcd2;;ED)" \
           "(OA;;CR;1131f6ac-9c07-11d1-f79f-00c04fc2dcd2;;ED)" \
           "(OA;;CR;1131f6aa-9c07-11d1-f79f-00c04fc2dcd2;;BA)" \
           "(OA;;CR;1131f6ab-9c07-11d1-f79f-00c04fc2dcd2;;BA)" \
           "(OA;;CR;1131f6ac-9c07-11d1-f79f-00c04fc2dcd2;;BA)" \
           "(A;;RPLCLORC;;;AU)(A;CI;RPWPCRCCDCLCLORCWOWDSDDTSW;;;EA)" \
           "(A;;RPWPCRCCDCLCLORCWOWDSDDTSW;;;SY)(A;CIIO;RPWPCRCCLCLORCWOWDSDSW;;;DA)" \
           "(OA;;CR;1131f6ad-9c07-11d1-f79f-00c04fc2dcd2;;ED)" \
           "(OA;;CR;89e95b76-444d-4c62-991a-0facbeda640c;;ED)" \
           "(OA;;CR;1131f6ad-9c07-11d1-f79f-00c04fc2dcd2;;BA)" \
           "(OA;;CR;89e95b76-444d-4c62-991a-0facbeda640c;;BA)" \
           "(OA;;CR;1131f6aa-9c07-11d1-f79f-00c04fc2dcd2;;S-1-5-21-3191434175-1265308384-3577286990-498)" \
           "S:(AU;SA;WPWOWD;;;WD)(AU;SA;CR;;;BA)(AU;SA;CR;;;DU)" \
           "(OU;SA;CR;45ec5156-db7e-47bb-b53f-dbeb2d03c40f;;WD)"
    sec = security.descriptor.from_sddl(sddl, domain_sid)
    return b64encode(ndr_pack(sec))


DEFAULTSITE = "Default-First-Site-Name"

# Exception classes

class ProvisioningError(Exception):
    """A generic provision error."""

class InvalidNetbiosName(Exception):
    """A specified name was not a valid NetBIOS name."""
    def __init__(self, name):
        super(InvalidNetbiosName, self).__init__("The name '%r' is not a valid NetBIOS name" % name)


class ProvisionPaths(object):
    def __init__(self):
        self.shareconf = None
        self.hklm = None
        self.hkcu = None
        self.hkcr = None
        self.hku = None
        self.hkpd = None
        self.hkpt = None
        self.samdb = None
        self.idmapdb = None
        self.secrets = None
        self.keytab = None
        self.dns_keytab = None
        self.dns = None
        self.winsdb = None
        self.private_dir = None
        self.ldapdir = None
        self.slapdconf = None
        self.modulesconf = None
        self.memberofconf = None
        self.fedoradsinf = None
        self.fedoradspartitions = None
        self.fedoradssasl = None
        self.olmmron = None
        self.olmmrserveridsconf = None
        self.olmmrsyncreplconf = None
        self.olcdir = None
        self.olslapd = None
        self.olcseedldif = None


class ProvisionNames(object):
    def __init__(self):
        self.rootdn = None
        self.domaindn = None
        self.configdn = None
        self.schemadn = None
        self.sambadn = None
        self.ldapmanagerdn = None
        self.dnsdomain = None
        self.realm = None
        self.netbiosname = None
        self.domain = None
        self.hostname = None
        self.sitename = None
        self.smbconf = None
    

class ProvisionResult(object):
    def __init__(self):
        self.paths = None
        self.domaindn = None
        self.lp = None
        self.samdb = None
        
class Schema(object):
    def __init__(self, setup_path, domain_sid, schemadn=None,
                 serverdn=None, sambadn=None, ldap_backend_type=None):
        """Load schema for the SamDB from the AD schema files and samba4_schema.ldif
        
        :param samdb: Load a schema into a SamDB.
        :param setup_path: Setup path function.
        :param schemadn: DN of the schema
        :param serverdn: DN of the server
        
        Returns the schema data loaded, to avoid double-parsing when then needing to add it to the db
        """
        
        self.ldb = Ldb()
        self.schema_data = read_ms_schema(setup_path('ad-schema/MS-AD_Schema_2K8_Attributes.txt'),
                                          setup_path('ad-schema/MS-AD_Schema_2K8_Classes.txt'))
        self.schema_data += open(setup_path("schema_samba4.ldif"), 'r').read()
        self.schema_data = substitute_var(self.schema_data, {"SCHEMADN": schemadn})
        check_all_substituted(self.schema_data)

        self.schema_dn_modify = read_and_sub_file(setup_path("provision_schema_basedn_modify.ldif"),
                                                  {"SCHEMADN": schemadn,
                                                   "SERVERDN": serverdn,
                                                   })

        descr = get_schema_descriptor(domain_sid)
        self.schema_dn_add = read_and_sub_file(setup_path("provision_schema_basedn.ldif"),
                                               {"SCHEMADN": schemadn,
                                                "DESCRIPTOR": descr
                                                })

        prefixmap = open(setup_path("prefixMap.txt"), 'r').read()
        prefixmap = b64encode(prefixmap)

        # We don't actually add this ldif, just parse it
        prefixmap_ldif = "dn: cn=schema\nprefixMap:: %s\n\n" % prefixmap
        self.ldb.set_schema_from_ldif(prefixmap_ldif, self.schema_data)


# Return a hash with the forward attribute as a key and the back as the value 
def get_linked_attributes(schemadn,schemaldb):
    attrs = ["linkID", "lDAPDisplayName"]
    res = schemaldb.search(expression="(&(linkID=*)(!(linkID:1.2.840.113556.1.4.803:=1))(objectclass=attributeSchema)(attributeSyntax=2.5.5.1))", base=schemadn, scope=SCOPE_ONELEVEL, attrs=attrs)
    attributes = {}
    for i in range (0, len(res)):
        expression = "(&(objectclass=attributeSchema)(linkID=%d)(attributeSyntax=2.5.5.1))" % (int(res[i]["linkID"][0])+1)
        target = schemaldb.searchone(basedn=schemadn, 
                                     expression=expression, 
                                     attribute="lDAPDisplayName", 
                                     scope=SCOPE_SUBTREE)
        if target is not None:
            attributes[str(res[i]["lDAPDisplayName"])]=str(target)
            
    return attributes

def get_dnsyntax_attributes(schemadn,schemaldb):
    attrs = ["linkID", "lDAPDisplayName"]
    res = schemaldb.search(expression="(&(!(linkID=*))(objectclass=attributeSchema)(attributeSyntax=2.5.5.1))", base=schemadn, scope=SCOPE_ONELEVEL, attrs=attrs)
    attributes = []
    for i in range (0, len(res)):
        attributes.append(str(res[i]["lDAPDisplayName"]))
        
    return attributes
    
    
def check_install(lp, session_info, credentials):
    """Check whether the current install seems ok.
    
    :param lp: Loadparm context
    :param session_info: Session information
    :param credentials: Credentials
    """
    if lp.get("realm") == "":
        raise Exception("Realm empty")
    ldb = Ldb(lp.get("sam database"), session_info=session_info, 
            credentials=credentials, lp=lp)
    if len(ldb.search("(cn=Administrator)")) != 1:
        raise ProvisioningError("No administrator account found")


def findnss(nssfn, names):
    """Find a user or group from a list of possibilities.
    
    :param nssfn: NSS Function to try (should raise KeyError if not found)
    :param names: Names to check.
    :return: Value return by first names list.
    """
    for name in names:
        try:
            return nssfn(name)
        except KeyError:
            pass
    raise KeyError("Unable to find user/group %r" % names)


findnss_uid = lambda names: findnss(pwd.getpwnam, names)[2]
findnss_gid = lambda names: findnss(grp.getgrnam, names)[2]


def read_and_sub_file(file, subst_vars):
    """Read a file and sub in variables found in it
    
    :param file: File to be read (typically from setup directory)
     param subst_vars: Optional variables to subsitute in the file.
    """
    data = open(file, 'r').read()
    if subst_vars is not None:
        data = substitute_var(data, subst_vars)
    check_all_substituted(data)
    return data


def setup_add_ldif(ldb, ldif_path, subst_vars=None):
    """Setup a ldb in the private dir.
    
    :param ldb: LDB file to import data into
    :param ldif_path: Path of the LDIF file to load
    :param subst_vars: Optional variables to subsitute in LDIF.
    """
    assert isinstance(ldif_path, str)

    data = read_and_sub_file(ldif_path, subst_vars)
    ldb.add_ldif(data)


def setup_modify_ldif(ldb, ldif_path, subst_vars=None):
    """Modify a ldb in the private dir.
    
    :param ldb: LDB object.
    :param ldif_path: LDIF file path.
    :param subst_vars: Optional dictionary with substitution variables.
    """
    data = read_and_sub_file(ldif_path, subst_vars)

    ldb.modify_ldif(data)


def setup_ldb(ldb, ldif_path, subst_vars):
    """Import a LDIF a file into a LDB handle, optionally substituting variables.

    :note: Either all LDIF data will be added or none (using transactions).

    :param ldb: LDB file to import into.
    :param ldif_path: Path to the LDIF file.
    :param subst_vars: Dictionary with substitution variables.
    """
    assert ldb is not None
    ldb.transaction_start()
    try:
        setup_add_ldif(ldb, ldif_path, subst_vars)
    except:
        ldb.transaction_cancel()
        raise
    ldb.transaction_commit()


def setup_file(template, fname, subst_vars):
    """Setup a file in the private dir.

    :param template: Path of the template file.
    :param fname: Path of the file to create.
    :param subst_vars: Substitution variables.
    """
    f = fname

    if os.path.exists(f):
        os.unlink(f)

    data = read_and_sub_file(template, subst_vars)
    open(f, 'w').write(data)


def provision_paths_from_lp(lp, dnsdomain):
    """Set the default paths for provisioning.

    :param lp: Loadparm context.
    :param dnsdomain: DNS Domain name
    """
    paths = ProvisionPaths()
    paths.private_dir = lp.get("private dir")
    paths.dns_keytab = "dns.keytab"

    paths.shareconf = os.path.join(paths.private_dir, "share.ldb")
    paths.samdb = os.path.join(paths.private_dir, lp.get("sam database") or "samdb.ldb")
    paths.idmapdb = os.path.join(paths.private_dir, lp.get("idmap database") or "idmap.ldb")
    paths.secrets = os.path.join(paths.private_dir, lp.get("secrets database") or "secrets.ldb")
    paths.dns = os.path.join(paths.private_dir, dnsdomain + ".zone")
    paths.namedconf = os.path.join(paths.private_dir, "named.conf")
    paths.namedtxt = os.path.join(paths.private_dir, "named.txt")
    paths.krb5conf = os.path.join(paths.private_dir, "krb5.conf")
    paths.winsdb = os.path.join(paths.private_dir, "wins.ldb")
    paths.s4_ldapi_path = os.path.join(paths.private_dir, "ldapi")
    paths.phpldapadminconfig = os.path.join(paths.private_dir, 
                                            "phpldapadmin-config.php")
    paths.ldapdir = os.path.join(paths.private_dir, 
                                 "ldap")
    paths.slapdconf = os.path.join(paths.ldapdir, 
                                   "slapd.conf")
    paths.slapdpid = os.path.join(paths.ldapdir, 
                                   "slapd.pid")
    paths.modulesconf = os.path.join(paths.ldapdir, 
                                     "modules.conf")
    paths.memberofconf = os.path.join(paths.ldapdir, 
                                      "memberof.conf")
    paths.fedoradsinf = os.path.join(paths.ldapdir, 
                                     "fedorads.inf")
    paths.fedoradspartitions = os.path.join(paths.ldapdir, 
                                            "fedorads-partitions.ldif")
    paths.fedoradssasl = os.path.join(paths.ldapdir, 
                                      "fedorads-sasl.ldif")
    paths.fedoradssamba = os.path.join(paths.ldapdir, 
                                        "fedorads-samba.ldif")
    paths.olmmrserveridsconf = os.path.join(paths.ldapdir, 
                                            "mmr_serverids.conf")
    paths.olmmrsyncreplconf = os.path.join(paths.ldapdir, 
                                           "mmr_syncrepl.conf")
    paths.olcdir = os.path.join(paths.ldapdir, 
                                 "slapd.d")
    paths.olcseedldif = os.path.join(paths.ldapdir, 
                                 "olc_seed.ldif")
    paths.hklm = "hklm.ldb"
    paths.hkcr = "hkcr.ldb"
    paths.hkcu = "hkcu.ldb"
    paths.hku = "hku.ldb"
    paths.hkpd = "hkpd.ldb"
    paths.hkpt = "hkpt.ldb"

    paths.sysvol = lp.get("path", "sysvol")

    paths.netlogon = lp.get("path", "netlogon")

    paths.smbconf = lp.configfile

    return paths


def guess_names(lp=None, hostname=None, domain=None, dnsdomain=None,
                serverrole=None, rootdn=None, domaindn=None, configdn=None,
                schemadn=None, serverdn=None, sitename=None, sambadn=None):
    """Guess configuration settings to use."""

    if hostname is None:
        hostname = socket.gethostname().split(".")[0].lower()

    netbiosname = hostname.upper()
    if not valid_netbios_name(netbiosname):
        raise InvalidNetbiosName(netbiosname)

    hostname = hostname.lower()

    if dnsdomain is None:
        dnsdomain = lp.get("realm")

    if serverrole is None:
        serverrole = lp.get("server role")

    assert dnsdomain is not None
    realm = dnsdomain.upper()

    if lp.get("realm").upper() != realm:
        raise Exception("realm '%s' in %s must match chosen realm '%s'" %
                        (lp.get("realm"), lp.configfile, realm))
    
    dnsdomain = dnsdomain.lower()

    if serverrole == "domain controller":
        if domain is None:
            domain = lp.get("workgroup")
        if domaindn is None:
            domaindn = "DC=" + dnsdomain.replace(".", ",DC=")
        if lp.get("workgroup").upper() != domain.upper():
            raise Exception("workgroup '%s' in smb.conf must match chosen domain '%s'",
                        lp.get("workgroup"), domain)
    else:
        domain = netbiosname
        if domaindn is None:
            domaindn = "CN=" + netbiosname
        
    assert domain is not None
    domain = domain.upper()
    if not valid_netbios_name(domain):
        raise InvalidNetbiosName(domain)
        
    if netbiosname.upper() == realm.upper():
        raise Exception("realm %s must not be equal to netbios domain name %s", realm, netbiosname)
        
    if hostname.upper() == realm.upper():
        raise Exception("realm %s must not be equal to hostname %s", realm, hostname)
        
    if domain.upper() == realm.upper():
        raise Exception("realm %s must not be equal to domain name %s", realm, domain)

    if rootdn is None:
       rootdn = domaindn
       
    if configdn is None:
        configdn = "CN=Configuration," + rootdn
    if schemadn is None:
        schemadn = "CN=Schema," + configdn
    if sambadn is None:
        sambadn = "CN=Samba"

    if sitename is None:
        sitename=DEFAULTSITE

    names = ProvisionNames()
    names.rootdn = rootdn
    names.domaindn = domaindn
    names.configdn = configdn
    names.schemadn = schemadn
    names.sambadn = sambadn
    names.ldapmanagerdn = "CN=Manager," + rootdn
    names.dnsdomain = dnsdomain
    names.domain = domain
    names.realm = realm
    names.netbiosname = netbiosname
    names.hostname = hostname
    names.sitename = sitename
    names.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)
 
    return names
    

def make_smbconf(smbconf, setup_path, hostname, domain, realm, serverrole, 
                 targetdir):
    """Create a new smb.conf file based on a couple of basic settings.
    """
    assert smbconf is not None
    if hostname is None:
        hostname = socket.gethostname().split(".")[0].lower()

    if serverrole is None:
        serverrole = "standalone"

    assert serverrole in ("domain controller", "member server", "standalone")
    if serverrole == "domain controller":
        smbconfsuffix = "dc"
    elif serverrole == "member server":
        smbconfsuffix = "member"
    elif serverrole == "standalone":
        smbconfsuffix = "standalone"

    assert domain is not None
    assert realm is not None

    default_lp = param.LoadParm()
    #Load non-existant file
    if os.path.exists(smbconf):
        default_lp.load(smbconf)
    
    if targetdir is not None:
        privatedir_line = "private dir = " + os.path.abspath(os.path.join(targetdir, "private"))
        lockdir_line = "lock dir = " + os.path.abspath(targetdir)

        default_lp.set("lock dir", os.path.abspath(targetdir))
    else:
        privatedir_line = ""
        lockdir_line = ""

    sysvol = os.path.join(default_lp.get("lock dir"), "sysvol")
    netlogon = os.path.join(sysvol, realm.lower(), "scripts")

    setup_file(setup_path("provision.smb.conf.%s" % smbconfsuffix), 
               smbconf, {
            "HOSTNAME": hostname,
            "DOMAIN": domain,
            "REALM": realm,
            "SERVERROLE": serverrole,
            "NETLOGONPATH": netlogon,
            "SYSVOLPATH": sysvol,
            "PRIVATEDIR_LINE": privatedir_line,
            "LOCKDIR_LINE": lockdir_line
            })


def setup_name_mappings(samdb, idmap, sid, domaindn, root_uid, nobody_uid,
                        users_gid, wheel_gid):
    """setup reasonable name mappings for sam names to unix names.

    :param samdb: SamDB object.
    :param idmap: IDmap db object.
    :param sid: The domain sid.
    :param domaindn: The domain DN.
    :param root_uid: uid of the UNIX root user.
    :param nobody_uid: uid of the UNIX nobody user.
    :param users_gid: gid of the UNIX users group.
    :param wheel_gid: gid of the UNIX wheel group."""

    idmap.setup_name_mapping("S-1-5-7", idmap.TYPE_UID, nobody_uid)
    idmap.setup_name_mapping("S-1-5-32-544", idmap.TYPE_GID, wheel_gid)
    
    idmap.setup_name_mapping(sid + "-500", idmap.TYPE_UID, root_uid)
    idmap.setup_name_mapping(sid + "-513", idmap.TYPE_GID, users_gid)

def setup_samdb_partitions(samdb_path, setup_path, message, lp, session_info, 
                           credentials, names,
                           serverrole, ldap_backend=None, 
                           erase=False):
    """Setup the partitions for the SAM database. 
    
    Alternatively, provision() may call this, and then populate the database.
    
    :note: This will wipe the Sam Database!
    
    :note: This function always removes the local SAM LDB file. The erase 
        parameter controls whether to erase the existing data, which 
        may not be stored locally but in LDAP.
    """
    assert session_info is not None

    # We use options=["modules:"] to stop the modules loading - we
    # just want to wipe and re-initialise the database, not start it up

    try:
        samdb = Ldb(url=samdb_path, session_info=session_info, 
                      credentials=credentials, lp=lp, options=["modules:"])
        # Wipes the database
        samdb.erase_except_schema_controlled()
    except LdbError:
        os.unlink(samdb_path)
        samdb = Ldb(url=samdb_path, session_info=session_info, 
                      credentials=credentials, lp=lp, options=["modules:"])
         # Wipes the database
        samdb.erase_except_schema_controlled()
        

    #Add modules to the list to activate them by default
    #beware often order is important
    #
    # Some Known ordering constraints:
    # - rootdse must be first, as it makes redirects from "" -> cn=rootdse
    # - objectclass must be before password_hash, because password_hash checks
    #   that the objectclass is of type person (filled in by objectclass
    #   module when expanding the objectclass list)
    # - partition must be last
    # - each partition has its own module list then
    modules_list = ["resolve_oids",
                    "rootdse",
                    "acl",
                    "paged_results",
                    "ranged_results",
                    "anr",
                    "server_sort",
                    "asq",
                    "extended_dn_store",
                    "extended_dn_in",
                    "rdn_name",
                    "objectclass",
                    "descriptor",
                    "samldb",
                    "password_hash",
                    "operational",
                    "kludge_acl"]
    tdb_modules_list = [
                    "subtree_rename",
                    "subtree_delete",
                    "linked_attributes",
                    "extended_dn_out_ldb"]
    modules_list2 = ["show_deleted",
                    "partition"]
 
    domaindn_ldb = "users.ldb"
    configdn_ldb = "configuration.ldb"
    schemadn_ldb = "schema.ldb"
    if ldap_backend is not None:
        domaindn_ldb = ldap_backend.ldapi_uri
        configdn_ldb = ldap_backend.ldapi_uri
        schemadn_ldb = ldap_backend.ldapi_uri
        
        if ldap_backend.ldap_backend_type == "fedora-ds":
            backend_modules = ["nsuniqueid", "paged_searches"]
            # We can handle linked attributes here, as we don't have directory-side subtree operations
            tdb_modules_list = ["linked_attributes", "extended_dn_out_dereference"]
        elif ldap_backend.ldap_backend_type == "openldap":
            backend_modules = ["entryuuid", "paged_searches"]
            # OpenLDAP handles subtree renames, so we don't want to do any of these things
            tdb_modules_list = ["extended_dn_out_dereference"]

    elif serverrole == "domain controller":
        tdb_modules_list.insert(0, "repl_meta_data")
        backend_modules = []
    else:
        backend_modules = ["objectguid"]

    if tdb_modules_list is None:
        tdb_modules_list_as_string = ""
    else:
        tdb_modules_list_as_string = ","+",".join(tdb_modules_list)
        
    samdb.transaction_start()
    try:
        message("Setting up sam.ldb partitions and settings")
        setup_add_ldif(samdb, setup_path("provision_partitions.ldif"), {
                "SCHEMADN": names.schemadn, 
                "SCHEMADN_LDB": schemadn_ldb,
                "SCHEMADN_MOD2": ",objectguid",
                "CONFIGDN": names.configdn,
                "CONFIGDN_LDB": configdn_ldb,
                "DOMAINDN": names.domaindn,
                "DOMAINDN_LDB": domaindn_ldb,
                "SCHEMADN_MOD": "schema_fsmo,instancetype",
                "CONFIGDN_MOD": "naming_fsmo,instancetype",
                "DOMAINDN_MOD": "pdc_fsmo,instancetype",
                "MODULES_LIST": ",".join(modules_list),
                "TDB_MODULES_LIST": tdb_modules_list_as_string,
                "MODULES_LIST2": ",".join(modules_list2),
                "BACKEND_MOD": ",".join(backend_modules),
        })

        samdb.load_ldif_file_add(setup_path("provision_init.ldif"))

        message("Setting up sam.ldb rootDSE")
        setup_samdb_rootdse(samdb, setup_path, names)

    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()
    
def secretsdb_self_join(secretsdb, domain, 
                        netbiosname, domainsid, machinepass, 
                        realm=None, dnsdomain=None,
                        keytab_path=None, 
                        key_version_number=1,
                        secure_channel_type=SEC_CHAN_WKSTA):
    """Add domain join-specific bits to a secrets database.
    
    :param secretsdb: Ldb Handle to the secrets database
    :param machinepass: Machine password
    """
    attrs=["whenChanged",
           "secret",
           "priorSecret",
           "priorChanged",
           "krb5Keytab",
           "privateKeytab"]
    

    msg = ldb.Message(ldb.Dn(secretsdb, "flatname=%s,cn=Primary Domains" % domain));
    msg["secureChannelType"] = str(secure_channel_type)
    msg["flatname"] = [domain]
    msg["objectClass"] = ["top", "primaryDomain"]
    if realm is not None:
      if dnsdomain is None:
        dnsdomain = realm.lower()
      msg["objectClass"] = ["top", "primaryDomain", "kerberosSecret"]
      msg["realm"] = realm
      msg["saltPrincipal"] = "host/%s.%s@%s" % (netbiosname.lower(), dnsdomain.lower(), realm.upper())
      msg["msDS-KeyVersionNumber"] = [str(key_version_number)]
      msg["privateKeytab"] = ["secrets.keytab"];


    msg["secret"] = [machinepass]
    msg["samAccountName"] = ["%s$" % netbiosname]
    msg["secureChannelType"] = [str(secure_channel_type)]
    msg["objectSid"] = [ndr_pack(domainsid)]
    
    res = secretsdb.search(base="cn=Primary Domains", 
                           attrs=attrs, 
                           expression=("(&(|(flatname=%s)(realm=%s)(objectSid=%s))(objectclass=primaryDomain))" % (domain, realm, str(domainsid))), 
                           scope=SCOPE_ONELEVEL)
    
    for del_msg in res:
      if del_msg.dn is not msg.dn:
        secretsdb.delete(del_msg.dn)

    res = secretsdb.search(base=msg.dn, attrs=attrs, scope=SCOPE_BASE)

    if len(res) == 1:
      msg["priorSecret"] = res[0]["secret"]
      msg["priorWhenChanged"] = res[0]["whenChanged"]

      if res["privateKeytab"] is not None:
        msg["privateKeytab"] = res[0]["privateKeytab"]

      if res["krb5Keytab"] is not None:
        msg["krb5Keytab"] = res[0]["krb5Keytab"]

      for el in msg:
        el.set_flags(ldb.FLAG_MOD_REPLACE)
        secretsdb.modify(msg)
    else:
      secretsdb.add(msg)


def secretsdb_setup_dns(secretsdb, setup_path, realm, dnsdomain, 
                        dns_keytab_path, dnspass):
    """Add DNS specific bits to a secrets database.
    
    :param secretsdb: Ldb Handle to the secrets database
    :param setup_path: Setup path function
    :param machinepass: Machine password
    """
    setup_ldb(secretsdb, setup_path("secrets_dns.ldif"), { 
            "REALM": realm,
            "DNSDOMAIN": dnsdomain,
            "DNS_KEYTAB": dns_keytab_path,
            "DNSPASS_B64": b64encode(dnspass),
            })


def setup_secretsdb(path, setup_path, session_info, credentials, lp):
    """Setup the secrets database.

    :param path: Path to the secrets database.
    :param setup_path: Get the path to a setup file.
    :param session_info: Session info.
    :param credentials: Credentials
    :param lp: Loadparm context
    :return: LDB handle for the created secrets database
    """
    if os.path.exists(path):
        os.unlink(path)
    secrets_ldb = Ldb(path, session_info=session_info, credentials=credentials,
                      lp=lp)
    secrets_ldb.erase()
    secrets_ldb.load_ldif_file_add(setup_path("secrets_init.ldif"))
    secrets_ldb = Ldb(path, session_info=session_info, credentials=credentials,
                      lp=lp)
    secrets_ldb.transaction_start()
    secrets_ldb.load_ldif_file_add(setup_path("secrets.ldif"))

    if credentials is not None and credentials.authentication_requested():
        if credentials.get_bind_dn() is not None:
            setup_add_ldif(secrets_ldb, setup_path("secrets_simple_ldap.ldif"), {
                    "LDAPMANAGERDN": credentials.get_bind_dn(),
                    "LDAPMANAGERPASS_B64": b64encode(credentials.get_password())
                    })
        else:
            setup_add_ldif(secrets_ldb, setup_path("secrets_sasl_ldap.ldif"), {
                    "LDAPADMINUSER": credentials.get_username(),
                    "LDAPADMINREALM": credentials.get_realm(),
                    "LDAPADMINPASS_B64": b64encode(credentials.get_password())
                    })

    return secrets_ldb

def setup_registry(path, setup_path, session_info, lp):
    """Setup the registry.
    
    :param path: Path to the registry database
    :param setup_path: Function that returns the path to a setup.
    :param session_info: Session information
    :param credentials: Credentials
    :param lp: Loadparm context
    """
    reg = registry.Registry()
    hive = registry.open_ldb(path, session_info=session_info, 
                         lp_ctx=lp)
    reg.mount_hive(hive, registry.HKEY_LOCAL_MACHINE)
    provision_reg = setup_path("provision.reg")
    assert os.path.exists(provision_reg)
    reg.diff_apply(provision_reg)


def setup_idmapdb(path, setup_path, session_info, lp):
    """Setup the idmap database.

    :param path: path to the idmap database
    :param setup_path: Function that returns a path to a setup file
    :param session_info: Session information
    :param credentials: Credentials
    :param lp: Loadparm context
    """
    if os.path.exists(path):
        os.unlink(path)

    idmap_ldb = IDmapDB(path, session_info=session_info,
                        lp=lp)

    idmap_ldb.erase()
    idmap_ldb.load_ldif_file_add(setup_path("idmap_init.ldif"))
    return idmap_ldb


def setup_samdb_rootdse(samdb, setup_path, names):
    """Setup the SamDB rootdse.

    :param samdb: Sam Database handle
    :param setup_path: Obtain setup path
    """
    setup_add_ldif(samdb, setup_path("provision_rootdse_add.ldif"), {
        "SCHEMADN": names.schemadn, 
        "NETBIOSNAME": names.netbiosname,
        "DNSDOMAIN": names.dnsdomain,
        "REALM": names.realm,
        "DNSNAME": "%s.%s" % (names.hostname, names.dnsdomain),
        "DOMAINDN": names.domaindn,
        "ROOTDN": names.rootdn,
        "CONFIGDN": names.configdn,
        "SERVERDN": names.serverdn,
        })
        

def setup_self_join(samdb, names,
                    machinepass, dnspass, 
                    domainsid, invocationid, setup_path,
                    policyguid, policyguid_dc, domainControllerFunctionality):
    """Join a host to its own domain."""
    assert isinstance(invocationid, str)
    setup_add_ldif(samdb, setup_path("provision_self_join.ldif"), { 
              "CONFIGDN": names.configdn, 
              "SCHEMADN": names.schemadn,
              "DOMAINDN": names.domaindn,
              "SERVERDN": names.serverdn,
              "INVOCATIONID": invocationid,
              "NETBIOSNAME": names.netbiosname,
              "DEFAULTSITE": names.sitename,
              "DNSNAME": "%s.%s" % (names.hostname, names.dnsdomain),
              "MACHINEPASS_B64": b64encode(machinepass),
              "DNSPASS_B64": b64encode(dnspass),
              "REALM": names.realm,
              "DOMAIN": names.domain,
              "DNSDOMAIN": names.dnsdomain,
              "SAMBA_VERSION_STRING": version,
              "DOMAIN_CONTROLLER_FUNCTIONALITY": str(domainControllerFunctionality)})

    setup_add_ldif(samdb, setup_path("provision_group_policy.ldif"), { 
              "POLICYGUID": policyguid,
              "POLICYGUID_DC": policyguid_dc,
              "DNSDOMAIN": names.dnsdomain,
              "DOMAINSID": str(domainsid),
              "DOMAINDN": names.domaindn})
    
    # add the NTDSGUID based SPNs
    ntds_dn = "CN=NTDS Settings,CN=%s,CN=Servers,CN=Default-First-Site-Name,CN=Sites,CN=Configuration,%s" % (names.hostname, names.domaindn)
    names.ntdsguid = samdb.searchone(basedn=ntds_dn, attribute="objectGUID",
                                     expression="", scope=SCOPE_BASE)
    assert isinstance(names.ntdsguid, str)

    # Setup fSMORoleOwner entries to point at the newly created DC entry
    setup_modify_ldif(samdb, setup_path("provision_self_join_modify.ldif"), {
              "DOMAIN": names.domain,
              "DNSDOMAIN": names.dnsdomain,
              "DOMAINDN": names.domaindn,
              "CONFIGDN": names.configdn,
              "SCHEMADN": names.schemadn, 
              "DEFAULTSITE": names.sitename,
              "SERVERDN": names.serverdn,
              "NETBIOSNAME": names.netbiosname,
              "NTDSGUID": names.ntdsguid
              })


def setup_samdb(path, setup_path, session_info, credentials, lp, 
                names, message, 
                domainsid, domainguid, policyguid, policyguid_dc,
                fill, adminpass, krbtgtpass, 
                machinepass, invocationid, dnspass,
                serverrole, schema=None, ldap_backend=None):
    """Setup a complete SAM Database.
    
    :note: This will wipe the main SAM database file!
    """

    # Do NOT change these default values without discussion with the team and reslease manager.  
    domainFunctionality = DS_DOMAIN_FUNCTION_2008
    forestFunctionality = DS_DOMAIN_FUNCTION_2008
    domainControllerFunctionality = DS_DC_FUNCTION_2008

    # Also wipes the database
    setup_samdb_partitions(path, setup_path, message=message, lp=lp,
                           credentials=credentials, session_info=session_info,
                           names=names, 
                           ldap_backend=ldap_backend, serverrole=serverrole)

    if (schema == None):
        schema = Schema(setup_path, domainsid, schemadn=names.schemadn, serverdn=names.serverdn,
            sambadn=names.sambadn, ldap_backend_type=ldap_backend.ldap_backend_type)

    # Load the database, but importantly, use Ldb not SamDB as we don't want to load the global schema
    samdb = Ldb(session_info=session_info, 
                credentials=credentials, lp=lp)

    message("Pre-loading the Samba 4 and AD schema")

    # Load the schema from the one we computed earlier
    samdb.set_schema_from_ldb(schema.ldb)

    # And now we can connect to the DB - the schema won't be loaded from the DB
    samdb.connect(path)

    # Load @OPTIONS
    samdb.load_ldif_file_add(setup_path("provision_options.ldif"))

    if fill == FILL_DRS:
        return samdb

    samdb.transaction_start()
    try:
        message("Erasing data from partitions")
        # Load the schema (again).  This time it will force a reindex,
        # and will therefore make the erase_partitions() below
        # computationally sane
        samdb.set_schema_from_ldb(schema.ldb)
        samdb.erase_partitions()
    
        # Set the domain functionality levels onto the database.
        # Various module (the password_hash module in particular) need
        # to know what level of AD we are emulating.

        # These will be fixed into the database via the database
        # modifictions below, but we need them set from the start.
        samdb.set_opaque_integer("domainFunctionality", domainFunctionality)
        samdb.set_opaque_integer("forestFunctionality", forestFunctionality)
        samdb.set_opaque_integer("domainControllerFunctionality", domainControllerFunctionality)

        samdb.set_domain_sid(str(domainsid))
        if serverrole == "domain controller":
            samdb.set_invocation_id(invocationid)

        message("Adding DomainDN: %s" % names.domaindn)
        if serverrole == "domain controller":
            domain_oc = "domainDNS"
        else:
            domain_oc = "samba4LocalDomain"

#impersonate domain admin
        admin_session_info = admin_session(lp, str(domainsid))
        samdb.set_session_info(admin_session_info)

        setup_add_ldif(samdb, setup_path("provision_basedn.ldif"), {
                "DOMAINDN": names.domaindn,
                "DOMAIN_OC": domain_oc
                })

        message("Modifying DomainDN: " + names.domaindn + "")
        if domainguid is not None:
            domainguid_mod = "replace: objectGUID\nobjectGUID: %s\n-" % domainguid
        else:
            domainguid_mod = ""

        setup_modify_ldif(samdb, setup_path("provision_basedn_modify.ldif"), {
            "CREATTIME": str(int(time.time()) * 1e7), # seconds -> ticks
            "DOMAINSID": str(domainsid),
            "SCHEMADN": names.schemadn, 
            "NETBIOSNAME": names.netbiosname,
            "DEFAULTSITE": names.sitename,
            "CONFIGDN": names.configdn,
            "SERVERDN": names.serverdn,
            "POLICYGUID": policyguid,
            "DOMAINDN": names.domaindn,
            "DOMAINGUID_MOD": domainguid_mod,
            "DOMAIN_FUNCTIONALITY": str(domainFunctionality),
            "SAMBA_VERSION_STRING": version
            })

        message("Adding configuration container")
        descr = get_config_descriptor(domainsid);
        setup_add_ldif(samdb, setup_path("provision_configuration_basedn.ldif"), {
            "CONFIGDN": names.configdn, 
            "DESCRIPTOR": descr,
            })
        message("Modifying configuration container")
        setup_modify_ldif(samdb, setup_path("provision_configuration_basedn_modify.ldif"), {
            "CONFIGDN": names.configdn, 
            "SCHEMADN": names.schemadn,
            })

        # The LDIF here was created when the Schema object was constructed
        message("Setting up sam.ldb schema")
        samdb.add_ldif(schema.schema_dn_add)
        samdb.modify_ldif(schema.schema_dn_modify)
        samdb.write_prefixes_from_schema()
        samdb.add_ldif(schema.schema_data)
        setup_add_ldif(samdb, setup_path("aggregate_schema.ldif"), 
                       {"SCHEMADN": names.schemadn})

        message("Setting up sam.ldb configuration data")
        setup_add_ldif(samdb, setup_path("provision_configuration.ldif"), {
            "CONFIGDN": names.configdn,
            "NETBIOSNAME": names.netbiosname,
            "DEFAULTSITE": names.sitename,
            "DNSDOMAIN": names.dnsdomain,
            "DOMAIN": names.domain,
            "SCHEMADN": names.schemadn,
            "DOMAINDN": names.domaindn,
            "SERVERDN": names.serverdn,
            "FOREST_FUNCTIONALALITY": str(forestFunctionality)
            })

        message("Setting up display specifiers")
        display_specifiers_ldif = read_ms_ldif(setup_path('display-specifiers/DisplaySpecifiers-Win2k8R2.txt'))
        display_specifiers_ldif = substitute_var(display_specifiers_ldif, {"CONFIGDN": names.configdn})
        check_all_substituted(display_specifiers_ldif)
        samdb.add_ldif(display_specifiers_ldif)

        message("Adding users container")
        setup_add_ldif(samdb, setup_path("provision_users_add.ldif"), {
                "DOMAINDN": names.domaindn})
        message("Modifying users container")
        setup_modify_ldif(samdb, setup_path("provision_users_modify.ldif"), {
                "DOMAINDN": names.domaindn})
        message("Adding computers container")
        setup_add_ldif(samdb, setup_path("provision_computers_add.ldif"), {
                "DOMAINDN": names.domaindn})
        message("Modifying computers container")
        setup_modify_ldif(samdb, setup_path("provision_computers_modify.ldif"), {
                "DOMAINDN": names.domaindn})
        message("Setting up sam.ldb data")
        setup_add_ldif(samdb, setup_path("provision.ldif"), {
            "CREATTIME": str(int(time.time()) * 1e7), # seconds -> ticks
            "DOMAINDN": names.domaindn,
            "NETBIOSNAME": names.netbiosname,
            "DEFAULTSITE": names.sitename,
            "CONFIGDN": names.configdn,
            "SERVERDN": names.serverdn,
            "POLICYGUID_DC": policyguid_dc
            })

        if fill == FILL_FULL:
            message("Setting up sam.ldb users and groups")
            setup_add_ldif(samdb, setup_path("provision_users.ldif"), {
                "DOMAINDN": names.domaindn,
                "DOMAINSID": str(domainsid),
                "CONFIGDN": names.configdn,
                "ADMINPASS_B64": b64encode(adminpass),
                "KRBTGTPASS_B64": b64encode(krbtgtpass),
                })

            if serverrole == "domain controller":
                message("Setting up self join")
                setup_self_join(samdb, names=names, invocationid=invocationid, 
                                dnspass=dnspass,  
                                machinepass=machinepass, 
                                domainsid=domainsid, policyguid=policyguid,
                                policyguid_dc=policyguid_dc,
                                setup_path=setup_path,
                                domainControllerFunctionality=domainControllerFunctionality)

                ntds_dn = "CN=NTDS Settings,CN=%s,CN=Servers,CN=Default-First-Site-Name,CN=Sites,CN=Configuration,%s" % (names.hostname, names.domaindn)
                names.ntdsguid = samdb.searchone(basedn=ntds_dn,
                  attribute="objectGUID", expression="", scope=SCOPE_BASE)
                assert isinstance(names.ntdsguid, str)

    except:
        samdb.transaction_cancel()
        raise

    samdb.transaction_commit()
    return samdb


FILL_FULL = "FULL"
FILL_NT4SYNC = "NT4SYNC"
FILL_DRS = "DRS"


def provision(setup_dir, message, session_info, 
              credentials, smbconf=None, targetdir=None, samdb_fill=FILL_FULL,
              realm=None, 
              rootdn=None, domaindn=None, schemadn=None, configdn=None, 
              serverdn=None,
              domain=None, hostname=None, hostip=None, hostip6=None, 
              domainsid=None, adminpass=None, ldapadminpass=None, 
              krbtgtpass=None, domainguid=None, 
              policyguid=None, policyguid_dc=None, invocationid=None,
              machinepass=None, 
              dnspass=None, root=None, nobody=None, users=None, 
              wheel=None, backup=None, aci=None, serverrole=None, 
              ldap_backend_extra_port=None, ldap_backend_type=None,
              sitename=None,
              ol_mmr_urls=None, ol_olc=None, 
              setup_ds_path=None, slapd_path=None, nosync=False,
              ldap_dryrun_mode=False):
    """Provision samba4
    
    :note: caution, this wipes all existing data!
    """

    def setup_path(file):
      return os.path.join(setup_dir, file)

    if domainsid is None:
      domainsid = security.random_sid()
    else:
      domainsid = security.dom_sid(domainsid)


    # create/adapt the group policy GUIDs
    if policyguid is None:
        policyguid = str(uuid.uuid4())
    policyguid = policyguid.upper()
    if policyguid_dc is None:
        policyguid_dc = str(uuid.uuid4())
    policyguid_dc = policyguid_dc.upper()

    if adminpass is None:
        adminpass = glue.generate_random_str(12)
    if krbtgtpass is None:
        krbtgtpass = glue.generate_random_str(12)
    if machinepass is None:
        machinepass  = glue.generate_random_str(12)
    if dnspass is None:
        dnspass = glue.generate_random_str(12)
    if ldapadminpass is None:
        #Make a new, random password between Samba and it's LDAP server
        ldapadminpass=glue.generate_random_str(12)        


    root_uid = findnss_uid([root or "root"])
    nobody_uid = findnss_uid([nobody or "nobody"])
    users_gid = findnss_gid([users or "users"])
    if wheel is None:
        wheel_gid = findnss_gid(["wheel", "adm"])
    else:
        wheel_gid = findnss_gid([wheel])

    if targetdir is not None:
        if (not os.path.exists(os.path.join(targetdir, "etc"))):
            os.makedirs(os.path.join(targetdir, "etc"))
        smbconf = os.path.join(targetdir, "etc", "smb.conf")
    elif smbconf is None:
        smbconf = param.default_path()

    # only install a new smb.conf if there isn't one there already
    if not os.path.exists(smbconf):
        make_smbconf(smbconf, setup_path, hostname, domain, realm, serverrole, 
                     targetdir)

    lp = param.LoadParm()
    lp.load(smbconf)

    names = guess_names(lp=lp, hostname=hostname, domain=domain, 
                        dnsdomain=realm, serverrole=serverrole, sitename=sitename,
                        rootdn=rootdn, domaindn=domaindn, configdn=configdn, schemadn=schemadn,
                        serverdn=serverdn)

    paths = provision_paths_from_lp(lp, names.dnsdomain)

    if hostip is None:
        try:
            hostip = socket.getaddrinfo(names.hostname, None, socket.AF_INET, socket.AI_CANONNAME, socket.IPPROTO_IP)[0][-1][0]
        except socket.gaierror, (socket.EAI_NODATA, msg):
            hostip = None

    if hostip6 is None:
        try:
            hostip6 = socket.getaddrinfo(names.hostname, None, socket.AF_INET6, socket.AI_CANONNAME, socket.IPPROTO_IP)[0][-1][0]
        except socket.gaierror, (socket.EAI_NODATA, msg): 
            hostip6 = None

    if serverrole is None:
        serverrole = lp.get("server role")

    assert serverrole in ("domain controller", "member server", "standalone")
    if invocationid is None and serverrole == "domain controller":
        invocationid = str(uuid.uuid4())

    if not os.path.exists(paths.private_dir):
        os.mkdir(paths.private_dir)

    ldapi_url = "ldapi://%s" % urllib.quote(paths.s4_ldapi_path, safe="")
    
    schema = Schema(setup_path, domainsid, schemadn=names.schemadn, serverdn=names.serverdn,
        sambadn=names.sambadn, ldap_backend_type=ldap_backend_type)
    
    secrets_credentials = credentials
    provision_backend = None
    if ldap_backend_type:
        # We only support an LDAP backend over ldapi://

        provision_backend = ProvisionBackend(paths=paths, setup_path=setup_path,
                                             lp=lp, credentials=credentials, 
                                             names=names,
                                             message=message, hostname=hostname,
                                             root=root, schema=schema,
                                             ldap_backend_type=ldap_backend_type,
                                             ldapadminpass=ldapadminpass,
                                             ldap_backend_extra_port=ldap_backend_extra_port,
                                             ol_mmr_urls=ol_mmr_urls, 
                                             slapd_path=slapd_path,
                                             setup_ds_path=setup_ds_path,
                                             ldap_dryrun_mode=ldap_dryrun_mode)

        # Now use the backend credentials to access the databases
        credentials = provision_backend.credentials
        secrets_credentials = provision_backend.adminCredentials
        ldapi_url = provision_backend.ldapi_uri

    # only install a new shares config db if there is none
    if not os.path.exists(paths.shareconf):
        message("Setting up share.ldb")
        share_ldb = Ldb(paths.shareconf, session_info=session_info, 
                        credentials=credentials, lp=lp)
        share_ldb.load_ldif_file_add(setup_path("share.ldif"))

     
    message("Setting up secrets.ldb")
    secrets_ldb = setup_secretsdb(paths.secrets, setup_path, 
                                  session_info=session_info, 
                                  credentials=secrets_credentials, lp=lp)

    message("Setting up the registry")
    setup_registry(paths.hklm, setup_path, session_info, 
                   lp=lp)

    message("Setting up idmap db")
    idmap = setup_idmapdb(paths.idmapdb, setup_path, session_info=session_info,
                          lp=lp)

    message("Setting up SAM db")
    samdb = setup_samdb(paths.samdb, setup_path, session_info=session_info, 
                        credentials=credentials, lp=lp, names=names,
                        message=message, 
                        domainsid=domainsid, 
                        schema=schema, domainguid=domainguid,
                        policyguid=policyguid, policyguid_dc=policyguid_dc,
                        fill=samdb_fill, 
                        adminpass=adminpass, krbtgtpass=krbtgtpass,
                        invocationid=invocationid, 
                        machinepass=machinepass, dnspass=dnspass,
                        serverrole=serverrole, ldap_backend=provision_backend)

    if serverrole == "domain controller":
        if paths.netlogon is None:
            message("Existing smb.conf does not have a [netlogon] share, but you are configuring a DC.")
            message("Please either remove %s or see the template at %s" % 
                    ( paths.smbconf, setup_path("provision.smb.conf.dc")))
            assert(paths.netlogon is not None)

        if paths.sysvol is None:
            message("Existing smb.conf does not have a [sysvol] share, but you are configuring a DC.")
            message("Please either remove %s or see the template at %s" % 
                    (paths.smbconf, setup_path("provision.smb.conf.dc")))
            assert(paths.sysvol is not None)            
            
        # Set up group policies (domain policy and domain controller policy)

        policy_path = os.path.join(paths.sysvol, names.dnsdomain, "Policies",
                                   "{" + policyguid + "}")
        os.makedirs(policy_path, 0755)
        open(os.path.join(policy_path, "GPT.INI"), 'w').write(
                                   "[General]\r\nVersion=65543")
        os.makedirs(os.path.join(policy_path, "MACHINE"), 0755)
        os.makedirs(os.path.join(policy_path, "USER"), 0755)

        policy_path_dc = os.path.join(paths.sysvol, names.dnsdomain, "Policies",
                                   "{" + policyguid_dc + "}")
        os.makedirs(policy_path_dc, 0755)
        open(os.path.join(policy_path_dc, "GPT.INI"), 'w').write(
                                   "[General]\r\nVersion=2")
        os.makedirs(os.path.join(policy_path_dc, "MACHINE"), 0755)
        os.makedirs(os.path.join(policy_path_dc, "USER"), 0755)

        if not os.path.isdir(paths.netlogon):
            os.makedirs(paths.netlogon, 0755)

    if samdb_fill == FILL_FULL:
        setup_name_mappings(samdb, idmap, str(domainsid), names.domaindn,
                            root_uid=root_uid, nobody_uid=nobody_uid,
                            users_gid=users_gid, wheel_gid=wheel_gid)

        message("Setting up sam.ldb rootDSE marking as synchronized")
        setup_modify_ldif(samdb, setup_path("provision_rootdse_modify.ldif"))

        # Only make a zone file on the first DC, it should be replicated with DNS replication
        if serverrole == "domain controller":
            secretsdb_self_join(secrets_ldb, domain=domain,
                                realm=names.realm,
                                dnsdomain=names.dnsdomain,
                                netbiosname=names.netbiosname,
                                domainsid=domainsid, 
                                machinepass=machinepass,
                                secure_channel_type=SEC_CHAN_BDC)

            secretsdb_setup_dns(secrets_ldb, setup_path, 
                                realm=names.realm, dnsdomain=names.dnsdomain,
                                dns_keytab_path=paths.dns_keytab,
                                dnspass=dnspass)

            domainguid = samdb.searchone(basedn=domaindn, attribute="objectGUID")
            assert isinstance(domainguid, str)

            create_zone_file(paths.dns, setup_path, dnsdomain=names.dnsdomain,
                             domaindn=names.domaindn, hostip=hostip,
                             hostip6=hostip6, hostname=names.hostname,
                             dnspass=dnspass, realm=names.realm,
                             domainguid=domainguid, ntdsguid=names.ntdsguid)

            create_named_conf(paths.namedconf, setup_path, realm=names.realm,
                              dnsdomain=names.dnsdomain, private_dir=paths.private_dir)

            create_named_txt(paths.namedtxt, setup_path, realm=names.realm,
                              dnsdomain=names.dnsdomain, private_dir=paths.private_dir,
                              keytab_name=paths.dns_keytab)
            message("See %s for an example configuration include file for BIND" % paths.namedconf)
            message("and %s for further documentation required for secure DNS updates" % paths.namedtxt)

            create_krb5_conf(paths.krb5conf, setup_path,
                             dnsdomain=names.dnsdomain, hostname=names.hostname,
                             realm=names.realm)
            message("A Kerberos configuration suitable for Samba 4 has been generated at %s" % paths.krb5conf)

    #Now commit the secrets.ldb to disk
    secrets_ldb.transaction_commit()

    if provision_backend is not None: 
      if ldap_backend_type == "fedora-ds":
        ldapi_db = Ldb(provision_backend.ldapi_uri, lp=lp, credentials=credentials)

        # delete default SASL mappings
        res = ldapi_db.search(expression="(!(cn=samba-admin mapping))", base="cn=mapping,cn=sasl,cn=config", scope=SCOPE_ONELEVEL, attrs=["dn"])

        # configure in-directory access control on Fedora DS via the aci attribute (over a direct ldapi:// socket)
        for i in range (0, len(res)):
          dn = str(res[i]["dn"])
          ldapi_db.delete(dn)

          aci = """(targetattr = "*") (version 3.0;acl "full access to all by samba-admin";allow (all)(userdn = "ldap:///CN=samba-admin,%s");)""" % names.sambadn

          m = ldb.Message()
          m["aci"] = ldb.MessageElement([aci], ldb.FLAG_MOD_REPLACE, "aci")
        
          m.dn = ldb.Dn(1, names.domaindn)
          ldapi_db.modify(m)

          m.dn = ldb.Dn(1, names.configdn)
          ldapi_db.modify(m)

          m.dn = ldb.Dn(1, names.schemadn)
          ldapi_db.modify(m)

      # if an LDAP backend is in use, terminate slapd after final provision and check its proper termination
      if provision_backend.slapd.poll() is None:
        #Kill the slapd
        if hasattr(provision_backend.slapd, "terminate"):
          provision_backend.slapd.terminate()
        else:
          # Older python versions don't have .terminate()
          import signal
          os.kill(provision_backend.slapd.pid, signal.SIGTERM)
            
        #and now wait for it to die
        provision_backend.slapd.communicate()
            
    # now display slapd_command_file.txt to show how slapd must be started next time
        message("Use later the following commandline to start slapd, then Samba:")
        slapd_command = "\'" + "\' \'".join(provision_backend.slapd_command) + "\'"
        message(slapd_command)
        message("This slapd-Commandline is also stored under: " + paths.ldapdir + "/ldap_backend_startup.sh")

        setup_file(setup_path("ldap_backend_startup.sh"), paths.ldapdir + "/ldap_backend_startup.sh", {
                "SLAPD_COMMAND" : slapd_command})

    
    create_phpldapadmin_config(paths.phpldapadminconfig, setup_path, 
                               ldapi_url)

    message("Please install the phpLDAPadmin configuration located at %s into /etc/phpldapadmin/config.php" % paths.phpldapadminconfig)

    message("Once the above files are installed, your Samba4 server will be ready to use")
    message("Server Role:           %s" % serverrole)
    message("Hostname:              %s" % names.hostname)
    message("NetBIOS Domain:        %s" % names.domain)
    message("DNS Domain:            %s" % names.dnsdomain)
    message("DOMAIN SID:            %s" % str(domainsid))
    if samdb_fill == FILL_FULL:
        message("Admin password:    %s" % adminpass)
    if provision_backend:
        if provision_backend.credentials.get_bind_dn() is not None:
            message("LDAP Backend Admin DN: %s" % provision_backend.credentials.get_bind_dn())
        else:
            message("LDAP Admin User:       %s" % provision_backend.credentials.get_username())

        message("LDAP Admin Password:   %s" % provision_backend.credentials.get_password())
  
    result = ProvisionResult()
    result.domaindn = domaindn
    result.paths = paths
    result.lp = lp
    result.samdb = samdb
    return result



def provision_become_dc(setup_dir=None,
                        smbconf=None, targetdir=None, realm=None, 
                        rootdn=None, domaindn=None, schemadn=None,
                        configdn=None, serverdn=None,
                        domain=None, hostname=None, domainsid=None, 
                        adminpass=None, krbtgtpass=None, domainguid=None, 
                        policyguid=None, policyguid_dc=None, invocationid=None,
                        machinepass=None, 
                        dnspass=None, root=None, nobody=None, users=None, 
                        wheel=None, backup=None, serverrole=None, 
                        ldap_backend=None, ldap_backend_type=None,
                        sitename=None, debuglevel=1):

    def message(text):
        """print a message if quiet is not set."""
        print text

    glue.set_debug_level(debuglevel)

    return provision(setup_dir, message, system_session(), None,
              smbconf=smbconf, targetdir=targetdir, samdb_fill=FILL_DRS,
              realm=realm, rootdn=rootdn, domaindn=domaindn, schemadn=schemadn,
              configdn=configdn, serverdn=serverdn, domain=domain,
              hostname=hostname, hostip="127.0.0.1", domainsid=domainsid,
              machinepass=machinepass, serverrole="domain controller",
              sitename=sitename)


def setup_db_config(setup_path, dbdir):
    """Setup a Berkeley database.
    
    :param setup_path: Setup path function.
    :param dbdir: Database directory."""
    if not os.path.isdir(os.path.join(dbdir, "bdb-logs")):
        os.makedirs(os.path.join(dbdir, "bdb-logs"), 0700)
        if not os.path.isdir(os.path.join(dbdir, "tmp")):
            os.makedirs(os.path.join(dbdir, "tmp"), 0700)

    setup_file(setup_path("DB_CONFIG"), os.path.join(dbdir, "DB_CONFIG"),
               {"LDAPDBDIR": dbdir})
    
class ProvisionBackend(object):
    def __init__(self, paths=None, setup_path=None, lp=None, credentials=None, 
                 names=None, message=None, 
                 hostname=None, root=None, 
                 schema=None, ldapadminpass=None,
                 ldap_backend_type=None, ldap_backend_extra_port=None,
                 ol_mmr_urls=None, 
                 setup_ds_path=None, slapd_path=None, 
                 nosync=False, ldap_dryrun_mode=False):
        """Provision an LDAP backend for samba4
        
        This works for OpenLDAP and Fedora DS
        """

        self.ldapi_uri = "ldapi://" + urllib.quote(os.path.join(paths.ldapdir, "ldapi"), safe="")
        
        if not os.path.isdir(paths.ldapdir):
            os.makedirs(paths.ldapdir, 0700)
            
        if ldap_backend_type == "existing":
            #Check to see that this 'existing' LDAP backend in fact exists
            ldapi_db = Ldb(self.ldapi_uri, credentials=credentials)
            search_ol_rootdse = ldapi_db.search(base="", scope=SCOPE_BASE,
                                                expression="(objectClass=OpenLDAProotDSE)")

            # If we have got here, then we must have a valid connection to the LDAP server, with valid credentials supplied
            # This caused them to be set into the long-term database later in the script.
            self.credentials = credentials
            self.ldap_backend_type = "openldap" #For now, assume existing backends at least emulate OpenLDAP
            return
    
        # we will shortly start slapd with ldapi for final provisioning. first check with ldapsearch -> rootDSE via self.ldapi_uri
        # if another instance of slapd is already running 
        try:
            ldapi_db = Ldb(self.ldapi_uri)
            search_ol_rootdse = ldapi_db.search(base="", scope=SCOPE_BASE,
                                                expression="(objectClass=OpenLDAProotDSE)");
            try:
                f = open(paths.slapdpid, "r")
                p = f.read()
                f.close()
                message("Check for slapd Process with PID: " + str(p) + " and terminate it manually.")
            except:
                pass
            
            raise ProvisioningError("Warning: Another slapd Instance seems already running on this host, listening to " + self.ldapi_uri + ". Please shut it down before you continue. ")
        
        except LdbError, e:
            pass

        # Try to print helpful messages when the user has not specified the path to slapd
        if slapd_path is None:
            raise ProvisioningError("Warning: LDAP-Backend must be setup with path to slapd, e.g. --slapd-path=\"/usr/local/libexec/slapd\"!")
        if not os.path.exists(slapd_path):
            message (slapd_path)
            raise ProvisioningError("Warning: Given Path to slapd does not exist!")

        schemadb_path = os.path.join(paths.ldapdir, "schema-tmp.ldb")
        try:
            os.unlink(schemadb_path)
        except OSError:
            pass


        # Put the LDIF of the schema into a database so we can search on
        # it to generate schema-dependent configurations in Fedora DS and
        # OpenLDAP
        os.path.join(paths.ldapdir, "schema-tmp.ldb")
        schema.ldb.connect(schemadb_path)
        schema.ldb.transaction_start()
    
        # These bits of LDIF are supplied when the Schema object is created
        schema.ldb.add_ldif(schema.schema_dn_add)
        schema.ldb.modify_ldif(schema.schema_dn_modify)
        schema.ldb.add_ldif(schema.schema_data)
        schema.ldb.transaction_commit()

        self.credentials = Credentials()
        self.credentials.guess(lp)
        #Kerberos to an ldapi:// backend makes no sense
        self.credentials.set_kerberos_state(DONT_USE_KERBEROS)

        self.adminCredentials = Credentials()
        self.adminCredentials.guess(lp)
        #Kerberos to an ldapi:// backend makes no sense
        self.adminCredentials.set_kerberos_state(DONT_USE_KERBEROS)

        self.ldap_backend_type = ldap_backend_type

        if ldap_backend_type == "fedora-ds":
            provision_fds_backend(self, paths=paths, setup_path=setup_path,
                                  names=names, message=message, 
                                  hostname=hostname,
                                  ldapadminpass=ldapadminpass, root=root, 
                                  schema=schema,
                                  ldap_backend_extra_port=ldap_backend_extra_port, 
                                  setup_ds_path=setup_ds_path,
                                  slapd_path=slapd_path,
                                  nosync=nosync,
                                  ldap_dryrun_mode=ldap_dryrun_mode)
            
        elif ldap_backend_type == "openldap":
            provision_openldap_backend(self, paths=paths, setup_path=setup_path,
                                       names=names, message=message, 
                                       hostname=hostname,
                                       ldapadminpass=ldapadminpass, root=root, 
                                       schema=schema,
                                       ldap_backend_extra_port=ldap_backend_extra_port, 
                                       ol_mmr_urls=ol_mmr_urls, 
                                       slapd_path=slapd_path,
                                       nosync=nosync,
                                       ldap_dryrun_mode=ldap_dryrun_mode)
        else:
            raise ProvisioningError("Unknown LDAP backend type selected")

        self.credentials.set_password(ldapadminpass)
        self.adminCredentials.set_username("samba-admin")
        self.adminCredentials.set_password(ldapadminpass)

        # Now start the slapd, so we can provision onto it.  We keep the
        # subprocess context around, to kill this off at the successful
        # end of the script
        self.slapd = subprocess.Popen(self.slapd_provision_command, close_fds=True, shell=False)
    
        while self.slapd.poll() is None:
            # Wait until the socket appears
            try:
                ldapi_db = Ldb(self.ldapi_uri, lp=lp, credentials=self.credentials)
                search_ol_rootdse = ldapi_db.search(base="", scope=SCOPE_BASE,
                                                    expression="(objectClass=OpenLDAProotDSE)")
                # If we have got here, then we must have a valid connection to the LDAP server!
                return
            except LdbError, e:
                time.sleep(1)
                pass
        
        raise ProvisioningError("slapd died before we could make a connection to it")


def provision_openldap_backend(result, paths=None, setup_path=None, names=None,
                               message=None, 
                               hostname=None, ldapadminpass=None, root=None, 
                               schema=None, 
                               ldap_backend_extra_port=None,
                               ol_mmr_urls=None, 
                               slapd_path=None, nosync=False,
                               ldap_dryrun_mode=False):

    #Allow the test scripts to turn off fsync() for OpenLDAP as for TDB and LDB
    nosync_config = ""
    if nosync:
        nosync_config = "dbnosync"
        
    lnkattr = get_linked_attributes(names.schemadn,schema.ldb)
    refint_attributes = ""
    memberof_config = "# Generated from Samba4 schema\n"
    for att in  lnkattr.keys():
        if lnkattr[att] is not None:
            refint_attributes = refint_attributes + " " + att 
            
            memberof_config += read_and_sub_file(setup_path("memberof.conf"),
                                                 { "MEMBER_ATTR" : att ,
                                                   "MEMBEROF_ATTR" : lnkattr[att] })
            
    refint_config = read_and_sub_file(setup_path("refint.conf"),
                                      { "LINK_ATTRS" : refint_attributes})
    
    attrs = ["linkID", "lDAPDisplayName"]
    res = schema.ldb.search(expression="(&(objectclass=attributeSchema)(searchFlags:1.2.840.113556.1.4.803:=1))", base=names.schemadn, scope=SCOPE_ONELEVEL, attrs=attrs)
    index_config = ""
    for i in range (0, len(res)):
        index_attr = res[i]["lDAPDisplayName"][0]
        if index_attr == "objectGUID":
            index_attr = "entryUUID"
            
        index_config += "index " + index_attr + " eq\n"

# generate serverids, ldap-urls and syncrepl-blocks for mmr hosts
    mmr_on_config = ""
    mmr_replicator_acl = ""
    mmr_serverids_config = ""
    mmr_syncrepl_schema_config = "" 
    mmr_syncrepl_config_config = "" 
    mmr_syncrepl_user_config = "" 
       
    
    if ol_mmr_urls is not None:
        # For now, make these equal
        mmr_pass = ldapadminpass
        
        url_list=filter(None,ol_mmr_urls.split(' ')) 
        if (len(url_list) == 1):
            url_list=filter(None,ol_mmr_urls.split(',')) 
                     
            
            mmr_on_config = "MirrorMode On"
            mmr_replicator_acl = "  by dn=cn=replicator,cn=samba read"
            serverid=0
            for url in url_list:
                serverid=serverid+1
                mmr_serverids_config += read_and_sub_file(setup_path("mmr_serverids.conf"),
                                                          { "SERVERID" : str(serverid),
                                                            "LDAPSERVER" : url })
                rid=serverid*10
                rid=rid+1
                mmr_syncrepl_schema_config += read_and_sub_file(setup_path("mmr_syncrepl.conf"),
                                                                {  "RID" : str(rid),
                                                                   "MMRDN": names.schemadn,
                                                                   "LDAPSERVER" : url,
                                                                   "MMR_PASSWORD": mmr_pass})
                
                rid=rid+1
                mmr_syncrepl_config_config += read_and_sub_file(setup_path("mmr_syncrepl.conf"),
                                                                {  "RID" : str(rid),
                                                                   "MMRDN": names.configdn,
                                                                   "LDAPSERVER" : url,
                                                                   "MMR_PASSWORD": mmr_pass})
                
                rid=rid+1
                mmr_syncrepl_user_config += read_and_sub_file(setup_path("mmr_syncrepl.conf"),
                                                              {  "RID" : str(rid),
                                                                 "MMRDN": names.domaindn,
                                                                 "LDAPSERVER" : url,
                                                                 "MMR_PASSWORD": mmr_pass })
    # OpenLDAP cn=config initialisation
    olc_syncrepl_config = ""
    olc_mmr_config = "" 
    # if mmr = yes, generate cn=config-replication directives
    # and olc_seed.lif for the other mmr-servers
    if ol_mmr_urls is not None:
        serverid=0
        olc_serverids_config = ""
        olc_syncrepl_seed_config = ""
        olc_mmr_config += read_and_sub_file(setup_path("olc_mmr.conf"),{})
        rid=1000
        for url in url_list:
            serverid=serverid+1
            olc_serverids_config += read_and_sub_file(setup_path("olc_serverid.conf"),
                                                      { "SERVERID" : str(serverid),
                                                        "LDAPSERVER" : url })
            
            rid=rid+1
            olc_syncrepl_config += read_and_sub_file(setup_path("olc_syncrepl.conf"),
                                                     {  "RID" : str(rid),
                                                        "LDAPSERVER" : url,
                                                        "MMR_PASSWORD": mmr_pass})
            
            olc_syncrepl_seed_config += read_and_sub_file(setup_path("olc_syncrepl_seed.conf"),
                                                          {  "RID" : str(rid),
                                                             "LDAPSERVER" : url})
                
        setup_file(setup_path("olc_seed.ldif"), paths.olcseedldif,
                   {"OLC_SERVER_ID_CONF": olc_serverids_config,
                    "OLC_PW": ldapadminpass,
                    "OLC_SYNCREPL_CONF": olc_syncrepl_seed_config})
    # end olc
                
    setup_file(setup_path("slapd.conf"), paths.slapdconf,
               {"DNSDOMAIN": names.dnsdomain,
                "LDAPDIR": paths.ldapdir,
                "DOMAINDN": names.domaindn,
                "CONFIGDN": names.configdn,
                "SCHEMADN": names.schemadn,
                "MEMBEROF_CONFIG": memberof_config,
                "MIRRORMODE": mmr_on_config,
                "REPLICATOR_ACL": mmr_replicator_acl,
                "MMR_SERVERIDS_CONFIG": mmr_serverids_config,
                "MMR_SYNCREPL_SCHEMA_CONFIG": mmr_syncrepl_schema_config,
                "MMR_SYNCREPL_CONFIG_CONFIG": mmr_syncrepl_config_config,
                "MMR_SYNCREPL_USER_CONFIG": mmr_syncrepl_user_config,
                "OLC_SYNCREPL_CONFIG": olc_syncrepl_config,
                "OLC_MMR_CONFIG": olc_mmr_config,
                "REFINT_CONFIG": refint_config,
                "INDEX_CONFIG": index_config,
                "NOSYNC": nosync_config})
        
    setup_db_config(setup_path, os.path.join(paths.ldapdir, "db", "user"))
    setup_db_config(setup_path, os.path.join(paths.ldapdir, "db", "config"))
    setup_db_config(setup_path, os.path.join(paths.ldapdir, "db", "schema"))
    
    if not os.path.exists(os.path.join(paths.ldapdir, "db", "samba",  "cn=samba")):
        os.makedirs(os.path.join(paths.ldapdir, "db", "samba",  "cn=samba"), 0700)
        
    setup_file(setup_path("cn=samba.ldif"), 
               os.path.join(paths.ldapdir, "db", "samba",  "cn=samba.ldif"),
               { "UUID": str(uuid.uuid4()), 
                 "LDAPTIME": timestring(int(time.time()))} )
    setup_file(setup_path("cn=samba-admin.ldif"), 
               os.path.join(paths.ldapdir, "db", "samba",  "cn=samba", "cn=samba-admin.ldif"),
               {"LDAPADMINPASS_B64": b64encode(ldapadminpass),
                "UUID": str(uuid.uuid4()), 
                "LDAPTIME": timestring(int(time.time()))} )
    
    if ol_mmr_urls is not None:
        setup_file(setup_path("cn=replicator.ldif"),
                   os.path.join(paths.ldapdir, "db", "samba",  "cn=samba", "cn=replicator.ldif"),
                   {"MMR_PASSWORD_B64": b64encode(mmr_pass),
                    "UUID": str(uuid.uuid4()),
                    "LDAPTIME": timestring(int(time.time()))} )
        

    mapping = "schema-map-openldap-2.3"
    backend_schema = "backend-schema.schema"

    backend_schema_data = schema.ldb.convert_schema_to_openldap("openldap", open(setup_path(mapping), 'r').read())
    assert backend_schema_data is not None
    open(os.path.join(paths.ldapdir, backend_schema), 'w').write(backend_schema_data)

    # now we generate the needed strings to start slapd automatically,
    # first ldapi_uri...
    if ldap_backend_extra_port is not None:
        # When we use MMR, we can't use 0.0.0.0 as it uses the name
        # specified there as part of it's clue as to it's own name,
        # and not to replicate to itself
        if ol_mmr_urls is None:
            server_port_string = "ldap://0.0.0.0:%d" % ldap_backend_extra_port
        else:
            server_port_string = "ldap://" + names.hostname + "." + names.dnsdomain +":%d" % ldap_backend_extra_port
    else:
        server_port_string = ""

    # Prepare the 'result' information - the commands to return in particular
    result.slapd_provision_command = [slapd_path]

    result.slapd_provision_command.append("-F" + paths.olcdir)

    result.slapd_provision_command.append("-h")

    # copy this command so we have two version, one with -d0 and only ldapi, and one with all the listen commands
    result.slapd_command = list(result.slapd_provision_command)
    
    result.slapd_provision_command.append(result.ldapi_uri)
    result.slapd_provision_command.append("-d0")

    uris = result.ldapi_uri
    if server_port_string is not "":
        uris = uris + " " + server_port_string

    result.slapd_command.append(uris)

    # Set the username - done here because Fedora DS still uses the admin DN and simple bind
    result.credentials.set_username("samba-admin")
    
    # If we were just looking for crashes up to this point, it's a
    # good time to exit before we realise we don't have OpenLDAP on
    # this system
    if ldap_dryrun_mode:
        sys.exit(0)

    # Finally, convert the configuration into cn=config style!
    if not os.path.isdir(paths.olcdir):
        os.makedirs(paths.olcdir, 0770)

        retcode = subprocess.call([slapd_path, "-Ttest", "-f", paths.slapdconf, "-F", paths.olcdir], close_fds=True, shell=False)

#        We can't do this, as OpenLDAP is strange.  It gives an error
#        output to the above, but does the conversion sucessfully...
#
#        if retcode != 0:
#            raise ProvisioningError("conversion from slapd.conf to cn=config failed")

        if not os.path.exists(os.path.join(paths.olcdir, "cn=config.ldif")):
            raise ProvisioningError("conversion from slapd.conf to cn=config failed")

        # Don't confuse the admin by leaving the slapd.conf around
        os.remove(paths.slapdconf)        
          

def provision_fds_backend(result, paths=None, setup_path=None, names=None,
                          message=None, 
                          hostname=None, ldapadminpass=None, root=None, 
                          schema=None,
                          ldap_backend_extra_port=None,
                          setup_ds_path=None,
                          slapd_path=None,
                          nosync=False, 
                          ldap_dryrun_mode=False):

    if ldap_backend_extra_port is not None:
        serverport = "ServerPort=%d" % ldap_backend_extra_port
    else:
        serverport = ""
        
    setup_file(setup_path("fedorads.inf"), paths.fedoradsinf, 
               {"ROOT": root,
                "HOSTNAME": hostname,
                "DNSDOMAIN": names.dnsdomain,
                "LDAPDIR": paths.ldapdir,
                "DOMAINDN": names.domaindn,
                "LDAPMANAGERDN": names.ldapmanagerdn,
                "LDAPMANAGERPASS": ldapadminpass, 
                "SERVERPORT": serverport})

    setup_file(setup_path("fedorads-partitions.ldif"), paths.fedoradspartitions, 
               {"CONFIGDN": names.configdn,
                "SCHEMADN": names.schemadn,
                "SAMBADN": names.sambadn,
                })

    setup_file(setup_path("fedorads-sasl.ldif"), paths.fedoradssasl, 
               {"SAMBADN": names.sambadn,
                })

    setup_file(setup_path("fedorads-samba.ldif"), paths.fedoradssamba,
                {"SAMBADN": names.sambadn, 
                 "LDAPADMINPASS": ldapadminpass
                })

    mapping = "schema-map-fedora-ds-1.0"
    backend_schema = "99_ad.ldif"
    
    # Build a schema file in Fedora DS format
    backend_schema_data = schema.ldb.convert_schema_to_openldap("fedora-ds", open(setup_path(mapping), 'r').read())
    assert backend_schema_data is not None
    open(os.path.join(paths.ldapdir, backend_schema), 'w').write(backend_schema_data)

    result.credentials.set_bind_dn(names.ldapmanagerdn)

    # Destory the target directory, or else setup-ds.pl will complain
    fedora_ds_dir = os.path.join(paths.ldapdir, "slapd-samba4")
    shutil.rmtree(fedora_ds_dir, True)

    result.slapd_provision_command = [slapd_path, "-D", fedora_ds_dir, "-i", paths.slapdpid];
    #In the 'provision' command line, stay in the foreground so we can easily kill it
    result.slapd_provision_command.append("-d0")

    #the command for the final run is the normal script
    result.slapd_command = [os.path.join(paths.ldapdir, "slapd-samba4", "start-slapd")]

    # If we were just looking for crashes up to this point, it's a
    # good time to exit before we realise we don't have Fedora DS on
    if ldap_dryrun_mode:
        sys.exit(0)

    # Try to print helpful messages when the user has not specified the path to the setup-ds tool
    if setup_ds_path is None:
        raise ProvisioningError("Warning: Fedora DS LDAP-Backend must be setup with path to setup-ds, e.g. --setup-ds-path=\"/usr/sbin/setup-ds.pl\"!")
    if not os.path.exists(setup_ds_path):
        message (setup_ds_path)
        raise ProvisioningError("Warning: Given Path to slapd does not exist!")

    # Run the Fedora DS setup utility
    retcode = subprocess.call([setup_ds_path, "--silent", "--file", paths.fedoradsinf], close_fds=True, shell=False)
    if retcode != 0:
        raise ProvisioningError("setup-ds failed")

    # Load samba-admin
    retcode = subprocess.call([
        os.path.join(paths.ldapdir, "slapd-samba4", "ldif2db"), "-s", names.sambadn, "-i", paths.fedoradssamba],
        close_fds=True, shell=False)
    if retcode != 0:
        raise("ldib2db failed")

def create_phpldapadmin_config(path, setup_path, ldapi_uri):
    """Create a PHP LDAP admin configuration file.

    :param path: Path to write the configuration to.
    :param setup_path: Function to generate setup paths.
    """
    setup_file(setup_path("phpldapadmin-config.php"), path, 
            {"S4_LDAPI_URI": ldapi_uri})


def create_zone_file(path, setup_path, dnsdomain, domaindn, 
                     hostip, hostip6, hostname, dnspass, realm, domainguid,
                     ntdsguid):
    """Write out a DNS zone file, from the info in the current database.

    :param path: Path of the new zone file.
    :param setup_path: Setup path function.
    :param dnsdomain: DNS Domain name
    :param domaindn: DN of the Domain
    :param hostip: Local IPv4 IP
    :param hostip6: Local IPv6 IP
    :param hostname: Local hostname
    :param dnspass: Password for DNS
    :param realm: Realm name
    :param domainguid: GUID of the domain.
    :param ntdsguid: GUID of the hosts nTDSDSA record.
    """
    assert isinstance(domainguid, str)

    if hostip6 is not None:
        hostip6_base_line = "            IN AAAA    " + hostip6
        hostip6_host_line = hostname + "        IN AAAA    " + hostip6
    else:
        hostip6_base_line = ""
        hostip6_host_line = ""

    if hostip is not None:
        hostip_base_line = "            IN A    " + hostip
        hostip_host_line = hostname + "        IN A    " + hostip
    else:
        hostip_base_line = ""
        hostip_host_line = ""

    setup_file(setup_path("provision.zone"), path, {
            "DNSPASS_B64": b64encode(dnspass),
            "HOSTNAME": hostname,
            "DNSDOMAIN": dnsdomain,
            "REALM": realm,
            "HOSTIP_BASE_LINE": hostip_base_line,
            "HOSTIP_HOST_LINE": hostip_host_line,
            "DOMAINGUID": domainguid,
            "DATESTRING": time.strftime("%Y%m%d%H"),
            "DEFAULTSITE": DEFAULTSITE,
            "NTDSGUID": ntdsguid,
            "HOSTIP6_BASE_LINE": hostip6_base_line,
            "HOSTIP6_HOST_LINE": hostip6_host_line,
        })


def create_named_conf(path, setup_path, realm, dnsdomain,
                      private_dir):
    """Write out a file containing zone statements suitable for inclusion in a
    named.conf file (including GSS-TSIG configuration).
    
    :param path: Path of the new named.conf file.
    :param setup_path: Setup path function.
    :param realm: Realm name
    :param dnsdomain: DNS Domain name
    :param private_dir: Path to private directory
    :param keytab_name: File name of DNS keytab file
    """

    setup_file(setup_path("named.conf"), path, {
            "DNSDOMAIN": dnsdomain,
            "REALM": realm,
            "REALM_WC": "*." + ".".join(realm.split(".")[1:]),
            "PRIVATE_DIR": private_dir
            })

def create_named_txt(path, setup_path, realm, dnsdomain,
                      private_dir, keytab_name):
    """Write out a file containing zone statements suitable for inclusion in a
    named.conf file (including GSS-TSIG configuration).
    
    :param path: Path of the new named.conf file.
    :param setup_path: Setup path function.
    :param realm: Realm name
    :param dnsdomain: DNS Domain name
    :param private_dir: Path to private directory
    :param keytab_name: File name of DNS keytab file
    """

    setup_file(setup_path("named.txt"), path, {
            "DNSDOMAIN": dnsdomain,
            "REALM": realm,
            "DNS_KEYTAB": keytab_name,
            "DNS_KEYTAB_ABS": os.path.join(private_dir, keytab_name),
            "PRIVATE_DIR": private_dir
        })

def create_krb5_conf(path, setup_path, dnsdomain, hostname, realm):
    """Write out a file containing zone statements suitable for inclusion in a
    named.conf file (including GSS-TSIG configuration).
    
    :param path: Path of the new named.conf file.
    :param setup_path: Setup path function.
    :param dnsdomain: DNS Domain name
    :param hostname: Local hostname
    :param realm: Realm name
    """

    setup_file(setup_path("krb5.conf"), path, {
            "DNSDOMAIN": dnsdomain,
            "HOSTNAME": hostname,
            "REALM": realm,
        })


