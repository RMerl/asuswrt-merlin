# backend code for upgrading from Samba3
# Copyright Jelmer Vernooij 2005-2007
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

"""Support code for upgrading from Samba 3 to Samba 4."""

__docformat__ = "restructuredText"

import grp
import ldb
import time
import pwd

from samba import Ldb, registry
from samba.param import LoadParm
from samba.provision import provision

def import_sam_policy(samldb, policy, dn):
    """Import a Samba 3 policy database."""
    samldb.modify_ldif("""
dn: %s
changetype: modify
replace: minPwdLength
minPwdLength: %d
pwdHistoryLength: %d
minPwdAge: %d
maxPwdAge: %d
lockoutDuration: %d
samba3ResetCountMinutes: %d
samba3UserMustLogonToChangePassword: %d
samba3BadLockoutMinutes: %d
samba3DisconnectTime: %d

""" % (dn, policy.min_password_length,
    policy.password_history, policy.minimum_password_age,
    policy.maximum_password_age, policy.lockout_duration,
    policy.reset_count_minutes, policy.user_must_logon_to_change_password,
    policy.bad_lockout_minutes, policy.disconnect_time))


def import_sam_account(samldb,acc,domaindn,domainsid):
    """Import a Samba 3 SAM account.

    :param samldb: Samba 4 SAM Database handle
    :param acc: Samba 3 account
    :param domaindn: Domain DN
    :param domainsid: Domain SID."""
    if acc.nt_username is None or acc.nt_username == "":
        acc.nt_username = acc.username

    if acc.fullname is None:
        try:
            acc.fullname = pwd.getpwnam(acc.username)[4].split(",")[0]
        except KeyError:
            pass

    if acc.fullname is None:
        acc.fullname = acc.username

    assert acc.fullname is not None
    assert acc.nt_username is not None

    samldb.add({
        "dn": "cn=%s,%s" % (acc.fullname, domaindn),
        "objectClass": ["top", "user"],
        "lastLogon": str(acc.logon_time),
        "lastLogoff": str(acc.logoff_time),
        "unixName": acc.username,
        "sAMAccountName": acc.nt_username,
        "cn": acc.nt_username,
        "description": acc.acct_desc,
        "primaryGroupID": str(acc.group_rid),
        "badPwdcount": str(acc.bad_password_count),
        "logonCount": str(acc.logon_count),
        "samba3Domain": acc.domain,
        "samba3DirDrive": acc.dir_drive,
        "samba3MungedDial": acc.munged_dial,
        "samba3Homedir": acc.homedir,
        "samba3LogonScript": acc.logon_script,
        "samba3ProfilePath": acc.profile_path,
        "samba3Workstations": acc.workstations,
        "samba3KickOffTime": str(acc.kickoff_time),
        "samba3BadPwdTime": str(acc.bad_password_time),
        "samba3PassLastSetTime": str(acc.pass_last_set_time),
        "samba3PassCanChangeTime": str(acc.pass_can_change_time),
        "samba3PassMustChangeTime": str(acc.pass_must_change_time),
        "objectSid": "%s-%d" % (domainsid, acc.user_rid),
        "lmPwdHash:": acc.lm_password,
        "ntPwdHash:": acc.nt_password,
        })


def import_sam_group(samldb, sid, gid, sid_name_use, nt_name, comment, domaindn):
    """Upgrade a SAM group.

    :param samldb: SAM database.
    :param gid: Group GID
    :param sid_name_use: SID name use
    :param nt_name: NT Group Name
    :param comment: NT Group Comment
    :param domaindn: Domain DN
    """

    if sid_name_use == 5: # Well-known group
        return None

    if nt_name in ("Domain Guests", "Domain Users", "Domain Admins"):
        return None

    if gid == -1:
        gr = grp.getgrnam(nt_name)
    else:
        gr = grp.getgrgid(gid)

    if gr is None:
        unixname = "UNKNOWN"
    else:
        unixname = gr.gr_name

    assert unixname is not None

    samldb.add({
        "dn": "cn=%s,%s" % (nt_name, domaindn),
        "objectClass": ["top", "group"],
        "description": comment,
        "cn": nt_name,
        "objectSid": sid,
        "unixName": unixname,
        "samba3SidNameUse": str(sid_name_use)
        })


def import_idmap(samdb,samba3_idmap,domaindn):
    """Import idmap data.

    :param samdb: SamDB handle.
    :param samba3_idmap: Samba 3 IDMAP database to import from
    :param domaindn: Domain DN.
    """
    samdb.add({
        "dn": domaindn,
        "userHwm": str(samba3_idmap.get_user_hwm()),
        "groupHwm": str(samba3_idmap.get_group_hwm())})

    for uid in samba3_idmap.uids():
        samdb.add({"dn": "SID=%s,%s" % (samba3_idmap.get_user_sid(uid), domaindn),
                          "SID": samba3_idmap.get_user_sid(uid),
                          "type": "user",
                          "unixID": str(uid)})

    for gid in samba3_idmap.uids():
        samdb.add({"dn": "SID=%s,%s" % (samba3_idmap.get_group_sid(gid), domaindn),
                          "SID": samba3_idmap.get_group_sid(gid),
                          "type": "group",
                          "unixID": str(gid)})


def import_wins(samba4_winsdb, samba3_winsdb):
    """Import settings from a Samba3 WINS database.

    :param samba4_winsdb: WINS database to import to
    :param samba3_winsdb: WINS database to import from
    """
    version_id = 0

    for (name, (ttl, ips, nb_flags)) in samba3_winsdb.items():
        version_id+=1

        type = int(name.split("#", 1)[1], 16)

        if type == 0x1C:
            rType = 0x2
        elif type & 0x80:
            if len(ips) > 1:
                rType = 0x2
            else:
                rType = 0x1
        else:
            if len(ips) > 1:
                rType = 0x3
            else:
                rType = 0x0

        if ttl > time.time():
            rState = 0x0 # active
        else:
            rState = 0x1 # released

        nType = ((nb_flags & 0x60)>>5)

        samba4_winsdb.add({"dn": "name=%s,type=0x%s" % tuple(name.split("#")),
                           "type": name.split("#")[1],
                           "name": name.split("#")[0],
                           "objectClass": "winsRecord",
                           "recordType": str(rType),
                           "recordState": str(rState),
                           "nodeType": str(nType),
                           "expireTime": ldb.timestring(ttl),
                           "isStatic": "0",
                           "versionID": str(version_id),
                           "address": ips})

    samba4_winsdb.add({"dn": "cn=VERSION",
                       "cn": "VERSION",
                       "objectClass": "winsMaxVersion",
                       "maxVersion": str(version_id)})

def enable_samba3sam(samdb, ldapurl):
    """Enable Samba 3 LDAP URL database.

    :param samdb: SAM Database.
    :param ldapurl: Samba 3 LDAP URL
    """
    samdb.modify_ldif("""
dn: @MODULES
changetype: modify
replace: @LIST
@LIST: samldb,operational,objectguid,rdn_name,samba3sam
""")

    samdb.add({"dn": "@MAP=samba3sam", "@MAP_URL": ldapurl})


smbconf_keep = [
    "dos charset",
    "unix charset",
    "display charset",
    "comment",
    "path",
    "directory",
    "workgroup",
    "realm",
    "netbios name",
    "netbios aliases",
    "netbios scope",
    "server string",
    "interfaces",
    "bind interfaces only",
    "security",
    "auth methods",
    "encrypt passwords",
    "null passwords",
    "obey pam restrictions",
    "password server",
    "smb passwd file",
    "private dir",
    "passwd chat",
    "password level",
    "lanman auth",
    "ntlm auth",
    "client NTLMv2 auth",
    "client lanman auth",
    "client plaintext auth",
    "read only",
    "hosts allow",
    "hosts deny",
    "log level",
    "debuglevel",
    "log file",
    "smb ports",
    "large readwrite",
    "max protocol",
    "min protocol",
    "unicode",
    "read raw",
    "write raw",
    "disable netbios",
    "nt status support",
    "announce version",
    "announce as",
    "max mux",
    "max xmit",
    "name resolve order",
    "max wins ttl",
    "min wins ttl",
    "time server",
    "unix extensions",
    "use spnego",
    "server signing",
    "client signing",
    "max connections",
    "paranoid server security",
    "socket options",
    "strict sync",
    "max print jobs",
    "printable",
    "print ok",
    "printer name",
    "printer",
    "map system",
    "map hidden",
    "map archive",
    "preferred master",
    "prefered master",
    "local master",
    "browseable",
    "browsable",
    "wins server",
    "wins support",
    "csc policy",
    "strict locking",
    "preload",
    "auto services",
    "lock dir",
    "lock directory",
    "pid directory",
    "socket address",
    "copy",
    "include",
    "available",
    "volume",
    "fstype",
    "panic action",
    "msdfs root",
    "host msdfs",
    "winbind separator"]

def upgrade_smbconf(oldconf,mark):
    """Remove configuration variables not present in Samba4

    :param oldconf: Old configuration structure
    :param mark: Whether removed configuration variables should be
        kept in the new configuration as "samba3:<name>"
    """
    data = oldconf.data()
    newconf = LoadParm()

    for s in data:
        for p in data[s]:
            keep = False
            for k in smbconf_keep:
                if smbconf_keep[k] == p:
                    keep = True
                    break

            if keep:
                newconf.set(s, p, oldconf.get(s, p))
            elif mark:
                newconf.set(s, "samba3:"+p, oldconf.get(s,p))

    return newconf

SAMBA3_PREDEF_NAMES = {
        'HKLM': registry.HKEY_LOCAL_MACHINE,
}

def import_registry(samba4_registry, samba3_regdb):
    """Import a Samba 3 registry database into the Samba 4 registry.

    :param samba4_registry: Samba 4 registry handle.
    :param samba3_regdb: Samba 3 registry database handle.
    """
    def ensure_key_exists(keypath):
        (predef_name, keypath) = keypath.split("/", 1)
        predef_id = SAMBA3_PREDEF_NAMES[predef_name]
        keypath = keypath.replace("/", "\\")
        return samba4_registry.create_key(predef_id, keypath)

    for key in samba3_regdb.keys():
        key_handle = ensure_key_exists(key)
        for subkey in samba3_regdb.subkeys(key):
            ensure_key_exists(subkey)
        for (value_name, (value_type, value_data)) in samba3_regdb.values(key).items():
            key_handle.set_value(value_name, value_type, value_data)


def upgrade_provision(samba3, logger, credentials, session_info,
                      smbconf, targetdir):
    oldconf = samba3.get_conf()

    if oldconf.get("domain logons") == "True":
        serverrole = "domain controller"
    else:
        if oldconf.get("security") == "user":
            serverrole = "standalone"
        else:
            serverrole = "member server"

    domainname = oldconf.get("workgroup")
    realm = oldconf.get("realm")
    netbiosname = oldconf.get("netbios name")

    secrets_db = samba3.get_secrets_db()

    if domainname is None:
        domainname = secrets_db.domains()[0]
        logger.warning("No domain specified in smb.conf file, assuming '%s'",
                domainname)

    if realm is None:
        if oldconf.get("domain logons") == "True":
            logger.warning("No realm specified in smb.conf file and being a DC. That upgrade path doesn't work! Please add a 'realm' directive to your old smb.conf to let us know which one you want to use (generally it's the upcased DNS domainname).")
            return
        else:
            realm = domainname.upper()
            logger.warning("No realm specified in smb.conf file, assuming '%s'",
                    realm)

    domainguid = secrets_db.get_domain_guid(domainname)
    domainsid = secrets_db.get_sid(domainname)
    if domainsid is None:
        logger.warning("Can't find domain secrets for '%s'; using random SID",
            domainname)

    if netbiosname is not None:
        machinepass = secrets_db.get_machine_password(netbiosname)
    else:
        machinepass = None

    result = provision(logger=logger,
                       session_info=session_info, credentials=credentials,
                       targetdir=targetdir, realm=realm, domain=domainname,
                       domainguid=domainguid, domainsid=domainsid,
                       hostname=netbiosname, machinepass=machinepass,
                       serverrole=serverrole)

    import_wins(Ldb(result.paths.winsdb), samba3.get_wins_db())

    # FIXME: import_registry(registry.Registry(), samba3.get_registry())

    # FIXME: import_idmap(samdb,samba3.get_idmap_db(),domaindn)

    groupdb = samba3.get_groupmapping_db()
    for sid in groupdb.groupsids():
        (gid, sid_name_use, nt_name, comment) = groupdb.get_group(sid)
        # FIXME: import_sam_group(samdb, sid, gid, sid_name_use, nt_name, comment, domaindn)

    # FIXME: Aliases

    passdb = samba3.get_sam_db()
    for name in passdb:
        user = passdb[name]
        #FIXME: import_sam_account(result.samdb, user, domaindn, domainsid)

    if hasattr(passdb, 'ldap_url'):
        logger.info("Enabling Samba3 LDAP mappings for SAM database")

        enable_samba3sam(result.samdb, passdb.ldap_url)

