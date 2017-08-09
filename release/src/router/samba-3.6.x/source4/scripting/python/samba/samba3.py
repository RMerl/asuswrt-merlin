# Unix SMB/CIFS implementation.
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

"""Support for reading Samba 3 data files."""

__docformat__ = "restructuredText"

REGISTRY_VALUE_PREFIX = "SAMBA_REGVAL"
REGISTRY_DB_VERSION = 1

import os
import struct
import tdb


def fetch_uint32(tdb, key):
    try:
        data = tdb[key]
    except KeyError:
        return None
    assert len(data) == 4
    return struct.unpack("<L", data)[0]


def fetch_int32(tdb, key):
    try:
        data = tdb[key]
    except KeyError:
        return None
    assert len(data) == 4
    return struct.unpack("<l", data)[0]


class TdbDatabase(object):
    """Simple Samba 3 TDB database reader."""
    def __init__(self, file):
        """Open a file.

        :param file: Path of the file to open.
        """
        self.tdb = tdb.Tdb(file, flags=os.O_RDONLY)
        self._check_version()

    def _check_version(self):
        pass

    def close(self):
        """Close resources associated with this object."""
        self.tdb.close()


class Registry(TdbDatabase):
    """Simple read-only support for reading the Samba3 registry.

    :note: This object uses the same syntax for registry key paths as 
        Samba 3. This particular format uses forward slashes for key path 
        separators and abbreviations for the predefined key names. 
        e.g.: HKLM/Software/Bar.
    """
    def __len__(self):
        """Return the number of keys."""
        return len(self.keys())

    def keys(self):
        """Return list with all the keys."""
        return [k.rstrip("\x00") for k in self.tdb.iterkeys() if not k.startswith(REGISTRY_VALUE_PREFIX)]

    def subkeys(self, key):
        """Retrieve the subkeys for the specified key.

        :param key: Key path.
        :return: list with key names
        """
        data = self.tdb.get("%s\x00" % key)
        if data is None:
            return []
        (num, ) = struct.unpack("<L", data[0:4])
        keys = data[4:].split("\0")
        assert keys[-1] == ""
        keys.pop()
        assert len(keys) == num
        return keys

    def values(self, key):
        """Return a dictionary with the values set for a specific key.
        
        :param key: Key to retrieve values for.
        :return: Dictionary with value names as key, tuple with type and 
            data as value."""
        data = self.tdb.get("%s/%s\x00" % (REGISTRY_VALUE_PREFIX, key))
        if data is None:
            return {}
        ret = {}
        (num, ) = struct.unpack("<L", data[0:4])
        data = data[4:]
        for i in range(num):
            # Value name
            (name, data) = data.split("\0", 1)

            (type, ) = struct.unpack("<L", data[0:4])
            data = data[4:]
            (value_len, ) = struct.unpack("<L", data[0:4])
            data = data[4:]

            ret[name] = (type, data[:value_len])
            data = data[value_len:]

        return ret


class PolicyDatabase(TdbDatabase):
    """Samba 3 Account Policy database reader."""
    def __init__(self, file):
        """Open a policy database
        
        :param file: Path to the file to open.
        """
        super(PolicyDatabase, self).__init__(file)
        self.min_password_length = fetch_uint32(self.tdb, "min password length\x00")
        self.password_history = fetch_uint32(self.tdb, "password history\x00")
        self.user_must_logon_to_change_password = fetch_uint32(self.tdb, "user must logon to change pasword\x00")
        self.maximum_password_age = fetch_uint32(self.tdb, "maximum password age\x00")
        self.minimum_password_age = fetch_uint32(self.tdb, "minimum password age\x00")
        self.lockout_duration = fetch_uint32(self.tdb, "lockout duration\x00")
        self.reset_count_minutes = fetch_uint32(self.tdb, "reset count minutes\x00")
        self.bad_lockout_minutes = fetch_uint32(self.tdb, "bad lockout minutes\x00")
        self.disconnect_time = fetch_int32(self.tdb, "disconnect time\x00")
        self.refuse_machine_password_change = fetch_uint32(self.tdb, "refuse machine password change\x00")

        # FIXME: Read privileges as well


GROUPDB_DATABASE_VERSION_V1 = 1 # native byte format.
GROUPDB_DATABASE_VERSION_V2 = 2 # le format.

GROUP_PREFIX = "UNIXGROUP/"

# Alias memberships are stored reverse, as memberships. The performance
# critical operation is to determine the aliases a SID is member of, not
# listing alias members. So we store a list of alias SIDs a SID is member of
# hanging of the member as key.
MEMBEROF_PREFIX = "MEMBEROF/"

class GroupMappingDatabase(TdbDatabase):
    """Samba 3 group mapping database reader."""
    def _check_version(self):
        assert fetch_int32(self.tdb, "INFO/version\x00") in (GROUPDB_DATABASE_VERSION_V1, GROUPDB_DATABASE_VERSION_V2)

    def groupsids(self):
        """Retrieve the SIDs for the groups in this database.

        :return: List with sids as strings.
        """
        for k in self.tdb.iterkeys():
            if k.startswith(GROUP_PREFIX):
                yield k[len(GROUP_PREFIX):].rstrip("\0")

    def get_group(self, sid):
        """Retrieve the group mapping information for a particular group.

        :param sid: SID of the group
        :return: None if the group can not be found, otherwise 
            a tuple with gid, sid_name_use, the NT name and comment.
        """
        data = self.tdb.get("%s%s\0" % (GROUP_PREFIX, sid))
        if data is None:
            return data
        (gid, sid_name_use) = struct.unpack("<lL", data[0:8])
        (nt_name, comment, _) = data[8:].split("\0")
        return (gid, sid_name_use, nt_name, comment)

    def aliases(self):
        """Retrieve the aliases in this database."""
        for k in self.tdb.iterkeys():
            if k.startswith(MEMBEROF_PREFIX):
                yield k[len(MEMBEROF_PREFIX):].rstrip("\0")


# High water mark keys
IDMAP_HWM_GROUP = "GROUP HWM\0"
IDMAP_HWM_USER = "USER HWM\0"

IDMAP_GROUP_PREFIX = "GID "
IDMAP_USER_PREFIX = "UID "

# idmap version determines auto-conversion
IDMAP_VERSION_V2 = 2

class IdmapDatabase(TdbDatabase):
    """Samba 3 ID map database reader."""
    def _check_version(self):
        assert fetch_int32(self.tdb, "IDMAP_VERSION\0") == IDMAP_VERSION_V2

    def uids(self):
        """Retrieve a list of all uids in this database."""
        for k in self.tdb.iterkeys():
            if k.startswith(IDMAP_USER_PREFIX):
                yield int(k[len(IDMAP_USER_PREFIX):].rstrip("\0"))

    def gids(self):
        """Retrieve a list of all gids in this database."""
        for k in self.tdb.iterkeys():
            if k.startswith(IDMAP_GROUP_PREFIX):
                yield int(k[len(IDMAP_GROUP_PREFIX):].rstrip("\0"))

    def get_user_sid(self, uid):
        """Retrieve the SID associated with a particular uid.

        :param uid: UID to retrieve SID for.
        :return: A SID or None if no mapping was found.
        """
        data = self.tdb.get("%s%d\0" % (IDMAP_USER_PREFIX, uid))
        if data is None:
            return data
        return data.rstrip("\0")

    def get_group_sid(self, gid):
        data = self.tdb.get("%s%d\0" % (IDMAP_GROUP_PREFIX, gid))
        if data is None:
            return data
        return data.rstrip("\0")

    def get_user_hwm(self):
        """Obtain the user high-water mark."""
        return fetch_uint32(self.tdb, IDMAP_HWM_USER)

    def get_group_hwm(self):
        """Obtain the group high-water mark."""
        return fetch_uint32(self.tdb, IDMAP_HWM_GROUP)


class SecretsDatabase(TdbDatabase):
    """Samba 3 Secrets database reader."""
    def get_auth_password(self):
        return self.tdb.get("SECRETS/AUTH_PASSWORD")

    def get_auth_domain(self):
        return self.tdb.get("SECRETS/AUTH_DOMAIN")

    def get_auth_user(self):
        return self.tdb.get("SECRETS/AUTH_USER")

    def get_domain_guid(self, host):
        return self.tdb.get("SECRETS/DOMGUID/%s" % host)

    def ldap_dns(self):
        for k in self.tdb.iterkeys():
            if k.startswith("SECRETS/LDAP_BIND_PW/"):
                yield k[len("SECRETS/LDAP_BIND_PW/"):].rstrip("\0")

    def domains(self):
        """Iterate over domains in this database.

        :return: Iterator over the names of domains in this database.
        """
        for k in self.tdb.iterkeys():
            if k.startswith("SECRETS/SID/"):
                yield k[len("SECRETS/SID/"):].rstrip("\0")

    def get_ldap_bind_pw(self, host):
        return self.tdb.get("SECRETS/LDAP_BIND_PW/%s" % host)
    
    def get_afs_keyfile(self, host):
        return self.tdb.get("SECRETS/AFS_KEYFILE/%s" % host)

    def get_machine_sec_channel_type(self, host):
        return fetch_uint32(self.tdb, "SECRETS/MACHINE_SEC_CHANNEL_TYPE/%s" % host)

    def get_machine_last_change_time(self, host):
        return fetch_uint32(self.tdb, "SECRETS/MACHINE_LAST_CHANGE_TIME/%s" % host)
            
    def get_machine_password(self, host):
        return self.tdb.get("SECRETS/MACHINE_PASSWORD/%s" % host)

    def get_machine_acc(self, host):
        return self.tdb.get("SECRETS/$MACHINE.ACC/%s" % host)

    def get_domtrust_acc(self, host):
        return self.tdb.get("SECRETS/$DOMTRUST.ACC/%s" % host)

    def trusted_domains(self):
        for k in self.tdb.iterkeys():
            if k.startswith("SECRETS/$DOMTRUST.ACC/"):
                yield k[len("SECRETS/$DOMTRUST.ACC/"):].rstrip("\0")

    def get_random_seed(self):
        return self.tdb.get("INFO/random_seed")

    def get_sid(self, host):
        return self.tdb.get("SECRETS/SID/%s" % host.upper())


SHARE_DATABASE_VERSION_V1 = 1
SHARE_DATABASE_VERSION_V2 = 2

class ShareInfoDatabase(TdbDatabase):
    """Samba 3 Share Info database reader."""
    def _check_version(self):
        assert fetch_int32(self.tdb, "INFO/version\0") in (SHARE_DATABASE_VERSION_V1, SHARE_DATABASE_VERSION_V2)

    def get_secdesc(self, name):
        """Obtain the security descriptor on a particular share.
        
        :param name: Name of the share
        """
        secdesc = self.tdb.get("SECDESC/%s" % name)
        # FIXME: Run ndr_pull_security_descriptor
        return secdesc


class Shares(object):
    """Container for share objects."""
    def __init__(self, lp, shareinfo):
        self.lp = lp
        self.shareinfo = shareinfo

    def __len__(self):
        """Number of shares."""
        return len(self.lp) - 1

    def __iter__(self):
        """Iterate over the share names."""
        return self.lp.__iter__()


ACB_DISABLED = 0x00000001
ACB_HOMDIRREQ = 0x00000002
ACB_PWNOTREQ = 0x00000004
ACB_TEMPDUP = 0x00000008
ACB_NORMAL = 0x00000010
ACB_MNS = 0x00000020
ACB_DOMTRUST = 0x00000040
ACB_WSTRUST = 0x00000080
ACB_SVRTRUST = 0x00000100
ACB_PWNOEXP = 0x00000200
ACB_AUTOLOCK = 0x00000400
ACB_ENC_TXT_PWD_ALLOWED = 0x00000800
ACB_SMARTCARD_REQUIRED = 0x00001000
ACB_TRUSTED_FOR_DELEGATION = 0x00002000
ACB_NOT_DELEGATED = 0x00004000
ACB_USE_DES_KEY_ONLY = 0x00008000
ACB_DONT_REQUIRE_PREAUTH = 0x00010000
ACB_PW_EXPIRED = 0x00020000
ACB_NO_AUTH_DATA_REQD = 0x00080000

acb_info_mapping = {
        'N': ACB_PWNOTREQ,  # 'N'o password.
        'D': ACB_DISABLED,  # 'D'isabled.
        'H': ACB_HOMDIRREQ, # 'H'omedir required.
        'T': ACB_TEMPDUP,   # 'T'emp account.
        'U': ACB_NORMAL,    # 'U'ser account (normal).
        'M': ACB_MNS,       # 'M'NS logon user account. What is this ?
        'W': ACB_WSTRUST,   # 'W'orkstation account.
        'S': ACB_SVRTRUST,  # 'S'erver account. 
        'L': ACB_AUTOLOCK,  # 'L'ocked account.
        'X': ACB_PWNOEXP,   # No 'X'piry on password
        'I': ACB_DOMTRUST,  # 'I'nterdomain trust account.
        ' ': 0
        }

def decode_acb(text):
    """Decode a ACB field.

    :param text: ACB text
    :return: integer with flags set.
    """
    assert not "[" in text and not "]" in text
    ret = 0
    for x in text:
        ret |= acb_info_mapping[x]
    return ret


class SAMUser(object):
    """Samba 3 SAM User.
    
    :note: Unknown or unset fields are set to None.
    """
    def __init__(self, name, uid=None, lm_password=None, nt_password=None, acct_ctrl=None, 
                 last_change_time=None, nt_username=None, fullname=None, logon_time=None, logoff_time=None,
                 acct_desc=None, group_rid=None, bad_password_count=None, logon_count=None,
                 domain=None, dir_drive=None, munged_dial=None, homedir=None, logon_script=None,
                 profile_path=None, workstations=None, kickoff_time=None, bad_password_time=None,
                 pass_last_set_time=None, pass_can_change_time=None, pass_must_change_time=None,
                 user_rid=None, unknown_6=None, nt_password_history=None,
                 unknown_str=None, hours=None, logon_divs=None):
        self.username = name
        self.uid = uid
        self.lm_password = lm_password
        self.nt_password = nt_password
        self.acct_ctrl = acct_ctrl
        self.pass_last_set_time = last_change_time
        self.nt_username = nt_username
        self.fullname = fullname
        self.logon_time = logon_time
        self.logoff_time = logoff_time
        self.acct_desc = acct_desc
        self.group_rid = group_rid
        self.bad_password_count = bad_password_count
        self.logon_count = logon_count
        self.domain = domain
        self.dir_drive = dir_drive
        self.munged_dial = munged_dial
        self.homedir = homedir
        self.logon_script = logon_script
        self.profile_path = profile_path
        self.workstations = workstations
        self.kickoff_time = kickoff_time
        self.bad_password_time = bad_password_time
        self.pass_can_change_time = pass_can_change_time
        self.pass_must_change_time = pass_must_change_time
        self.user_rid = user_rid
        self.unknown_6 = unknown_6
        self.nt_password_history = nt_password_history
        self.unknown_str = unknown_str
        self.hours = hours
        self.logon_divs = logon_divs

    def __eq__(self, other): 
        if not isinstance(other, SAMUser):
            return False
        return self.__dict__ == other.__dict__


class SmbpasswdFile(object):
    """Samba 3 smbpasswd file reader."""
    def __init__(self, file):
        self.users = {}
        f = open(file, 'r')
        for l in f.readlines():
            if len(l) == 0 or l[0] == "#":
                continue # Skip comments and blank lines
            parts = l.split(":")
            username = parts[0]
            uid = int(parts[1])
            acct_ctrl = 0
            last_change_time = None
            if parts[2] == "NO PASSWORD":
                acct_ctrl |= ACB_PWNOTREQ
                lm_password = None
            elif parts[2][0] in ("*", "X"):
                # No password set
                lm_password = None
            else:
                lm_password = parts[2]

            if parts[3][0] in ("*", "X"):
                # No password set
                nt_password = None
            else:
                nt_password = parts[3]

            if parts[4][0] == '[':
                assert "]" in parts[4]
                acct_ctrl |= decode_acb(parts[4][1:-1])
                if parts[5].startswith("LCT-"):
                    last_change_time = int(parts[5][len("LCT-"):], 16)
            else: # old style file
                if username[-1] == "$":
                    acct_ctrl &= ~ACB_NORMAL
                    acct_ctrl |= ACB_WSTRUST

            self.users[username] = SAMUser(username, uid, lm_password, nt_password, acct_ctrl, last_change_time)

        f.close()

    def __len__(self):
        return len(self.users)

    def __getitem__(self, name):
        return self.users[name]

    def __iter__(self):
        return iter(self.users)

    def close(self): # For consistency
        pass


TDBSAM_FORMAT_STRING_V0 = "ddddddBBBBBBBBBBBBddBBwdwdBwwd"
TDBSAM_FORMAT_STRING_V1 = "dddddddBBBBBBBBBBBBddBBwdwdBwwd"
TDBSAM_FORMAT_STRING_V2 = "dddddddBBBBBBBBBBBBddBBBwwdBwwd"
TDBSAM_USER_PREFIX = "USER_"


class LdapSam(object):
    """Samba 3 LDAP passdb backend reader."""
    def __init__(self, url):
        self.ldap_url = url


class TdbSam(TdbDatabase):
    """Samba 3 TDB passdb backend reader."""
    def _check_version(self):
        self.version = fetch_uint32(self.tdb, "INFO/version\0") or 0
        assert self.version in (0, 1, 2)

    def usernames(self):
        """Iterate over the usernames in this Tdb database."""
        for k in self.tdb.iterkeys():
            if k.startswith(TDBSAM_USER_PREFIX):
                yield k[len(TDBSAM_USER_PREFIX):].rstrip("\0")

    __iter__ = usernames
    
    def __getitem__(self, name):
        data = self.tdb["%s%s\0" % (TDBSAM_USER_PREFIX, name)]
        user = SAMUser(name)
    
        def unpack_string(data):
            (length, ) = struct.unpack("<L", data[:4])
            data = data[4:]
            if length == 0:
                return (None, data)
            return (data[:length].rstrip("\0"), data[length:])

        def unpack_int32(data):
            (value, ) = struct.unpack("<l", data[:4])
            return (value, data[4:])

        def unpack_uint32(data):
            (value, ) = struct.unpack("<L", data[:4])
            return (value, data[4:])

        def unpack_uint16(data):
            (value, ) = struct.unpack("<H", data[:2])
            return (value, data[2:])

        (logon_time, data) = unpack_int32(data)
        (logoff_time, data) = unpack_int32(data)
        (kickoff_time, data) = unpack_int32(data)

        if self.version > 0:
            (bad_password_time, data) = unpack_int32(data)
            if bad_password_time != 0:
                user.bad_password_time = bad_password_time
        (pass_last_set_time, data) = unpack_int32(data)
        (pass_can_change_time, data) = unpack_int32(data)
        (pass_must_change_time, data) = unpack_int32(data)

        if logon_time != 0:
            user.logon_time = logon_time
        user.logoff_time = logoff_time
        user.kickoff_time = kickoff_time
        if pass_last_set_time != 0:
            user.pass_last_set_time = pass_last_set_time
        user.pass_can_change_time = pass_can_change_time

        (user.username, data) = unpack_string(data)
        (user.domain, data) = unpack_string(data)
        (user.nt_username, data) = unpack_string(data)
        (user.fullname, data) = unpack_string(data)
        (user.homedir, data) = unpack_string(data)
        (user.dir_drive, data) = unpack_string(data)
        (user.logon_script, data) = unpack_string(data)
        (user.profile_path, data) = unpack_string(data)
        (user.acct_desc, data) = unpack_string(data)
        (user.workstations, data) = unpack_string(data)
        (user.unknown_str, data) = unpack_string(data)
        (user.munged_dial, data) = unpack_string(data)

        (user.user_rid, data) = unpack_int32(data)
        (user.group_rid, data) = unpack_int32(data)

        (user.lm_password, data) = unpack_string(data)
        (user.nt_password, data) = unpack_string(data)

        if self.version > 1:
            (user.nt_password_history, data) = unpack_string(data)

        (user.acct_ctrl, data) = unpack_uint16(data)
        (_, data) = unpack_uint32(data) # remove_me field
        (user.logon_divs, data) = unpack_uint16(data)
        (hours, data) = unpack_string(data)
        user.hours = []
        for entry in hours:
            for i in range(8):
                user.hours.append(ord(entry) & (2 ** i) == (2 ** i))
        (user.bad_password_count, data) = unpack_uint16(data)
        (user.logon_count, data) = unpack_uint16(data)
        (user.unknown_6, data) = unpack_uint32(data)
        assert len(data) == 0
        return user


def shellsplit(text):
    """Very simple shell-like line splitting.
    
    :param text: Text to split.
    :return: List with parts of the line as strings.
    """
    ret = list()
    inquotes = False
    current = ""
    for c in text:
        if c == "\"":
            inquotes = not inquotes
        elif c in ("\t", "\n", " ") and not inquotes:
            ret.append(current)
            current = ""
        else:
            current += c
    if current != "":
        ret.append(current)
    return ret


class WinsDatabase(object):
    """Samba 3 WINS database reader."""
    def __init__(self, file):
        self.entries = {}
        f = open(file, 'r')
        assert f.readline().rstrip("\n") == "VERSION 1 0"
        for l in f.readlines():
            if l[0] == "#": # skip comments
                continue
            entries = shellsplit(l.rstrip("\n"))
            name = entries[0]
            ttl = int(entries[1])
            i = 2
            ips = []
            while "." in entries[i]:
                ips.append(entries[i])
                i+=1
            nb_flags = int(entries[i][:-1], 16)
            assert not name in self.entries, "Name %s exists twice" % name
            self.entries[name] = (ttl, ips, nb_flags)
        f.close()

    def __getitem__(self, name):
        return self.entries[name]

    def __len__(self):
        return len(self.entries)

    def __iter__(self):
        return iter(self.entries)

    def items(self):
        """Return the entries in this WINS database."""
        return self.entries.items()

    def close(self): # for consistency
        pass


class ParamFile(object):
    """Simple smb.conf-compatible file parser

    Does not use a parameter table, unlike the "normal".
    """

    def __init__(self, sections=None):
        self._sections = sections or {}

    def _sanitize_name(self, name):
        return name.strip().lower().replace(" ","")

    def __repr__(self):
        return "ParamFile(%r)" % self._sections

    def read(self, filename):
        """Read a file.

        :param filename: Path to the file
        """
        section = None
        for i, l in enumerate(open(filename, 'r').xreadlines()):
            l = l.strip()
            if not l or l[0] == '#' or l[0] == ';':
                continue
            if l[0] == "[" and l[-1] == "]":
                section = self._sanitize_name(l[1:-1])
                self._sections.setdefault(section, {})
            elif "=" in l:
               (k, v) = l.split("=", 1) 
               self._sections[section][self._sanitize_name(k)] = v
            else:
                raise Exception("Unable to parser line %d: %r" % (i+1,l))

    def get(self, param, section=None):
        """Return the value of a parameter.

        :param param: Parameter name
        :param section: Section name, defaults to "global"
        :return: parameter value as string if found, None otherwise.
        """
        if section is None:
            section = "global"
        section = self._sanitize_name(section)
        if not section in self._sections:
            return None
        param = self._sanitize_name(param)
        if not param in self._sections[section]:
            return None
        return self._sections[section][param].strip()

    def __getitem__(self, section):
        return self._sections[section]

    def get_section(self, section):
        return self._sections.get(section)

    def add_section(self, section):
        self._sections[self._sanitize_name(section)] = {}

    def set_string(self, name, value):
        self._sections["global"][name] = value

    def get_string(self, name):
        return self._sections["global"].get(name)


class Samba3(object):
    """Samba 3 configuration and state data reader."""
    def __init__(self, libdir, smbconfpath):
        """Open the configuration and data for a Samba 3 installation.

        :param libdir: Library directory
        :param smbconfpath: Path to the smb.conf file.
        """
        self.smbconfpath = smbconfpath
        self.libdir = libdir
        self.lp = ParamFile()
        self.lp.read(self.smbconfpath)

    def libdir_path(self, path):
        if path[0] == "/" or path[0] == ".":
            return path
        return os.path.join(self.libdir, path)

    def get_conf(self):
        return self.lp

    def get_sam_db(self):
        lp = self.get_conf()
        backends = (lp.get("passdb backend") or "").split(" ")
        if ":" in backends[0]:
            (name, location) = backends[0].split(":", 2)
        else:
            name = backends[0]
            location = None
        if name == "smbpasswd":
            return SmbpasswdFile(self.libdir_path(location or "smbpasswd"))
        elif name == "tdbsam":
            return TdbSam(self.libdir_path(location or "passdb.tdb"))
        elif name == "ldapsam":
            if location is not None:
                return LdapSam("ldap:%s" % location)
            return LdapSam(lp.get("ldap server"))
        else:
            raise NotImplementedError("unsupported passdb backend %s" % backends[0])

    def get_policy_db(self):
        return PolicyDatabase(self.libdir_path("account_policy.tdb"))

    def get_registry(self):
        return Registry(self.libdir_path("registry.tdb"))

    def get_secrets_db(self):
        return SecretsDatabase(self.libdir_path("secrets.tdb"))

    def get_shareinfo_db(self):
        return ShareInfoDatabase(self.libdir_path("share_info.tdb"))

    def get_idmap_db(self):
        return IdmapDatabase(self.libdir_path("winbindd_idmap.tdb"))

    def get_wins_db(self):
        return WinsDatabase(self.libdir_path("wins.dat"))

    def get_shares(self):
        return Shares(self.get_conf(), self.get_shareinfo_db())

    def get_groupmapping_db(self):
        return GroupMappingDatabase(self.libdir_path("group_mapping.tdb"))
