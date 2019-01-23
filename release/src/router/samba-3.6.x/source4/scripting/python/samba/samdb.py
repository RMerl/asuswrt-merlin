#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007-2010
# Copyright (C) Matthias Dieter Wallnoefer 2009
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

"""Convenience functions for using the SAM."""

import samba
import ldb
import time
import base64
from samba import dsdb
from samba.ndr import ndr_unpack, ndr_pack
from samba.dcerpc import drsblobs, misc

__docformat__ = "restructuredText"


class SamDB(samba.Ldb):
    """The SAM database."""

    hash_oid_name = {}

    def __init__(self, url=None, lp=None, modules_dir=None, session_info=None,
                 credentials=None, flags=0, options=None, global_schema=True,
                 auto_connect=True, am_rodc=None):
        self.lp = lp
        if not auto_connect:
            url = None
        elif url is None and lp is not None:
            url = lp.get("sam database")

        super(SamDB, self).__init__(url=url, lp=lp, modules_dir=modules_dir,
            session_info=session_info, credentials=credentials, flags=flags,
            options=options)

        if global_schema:
            dsdb._dsdb_set_global_schema(self)

        if am_rodc is not None:
            dsdb._dsdb_set_am_rodc(self, am_rodc)

    def connect(self, url=None, flags=0, options=None):
        if self.lp is not None:
            url = self.lp.private_path(url)

        super(SamDB, self).connect(url=url, flags=flags,
                options=options)

    def am_rodc(self):
        return dsdb._am_rodc(self)

    def domain_dn(self):
        return str(self.get_default_basedn())

    def enable_account(self, search_filter):
        """Enables an account

        :param search_filter: LDAP filter to find the user (eg
            samccountname=name)
        """
        res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                          expression=search_filter, attrs=["userAccountControl"])
        assert(len(res) == 1)
        user_dn = res[0].dn

        userAccountControl = int(res[0]["userAccountControl"][0])
        if userAccountControl & 0x2:
            # remove disabled bit
            userAccountControl = userAccountControl & ~0x2
        if userAccountControl & 0x20:
             # remove 'no password required' bit
            userAccountControl = userAccountControl & ~0x20

        mod = """
dn: %s
changetype: modify
replace: userAccountControl
userAccountControl: %u
""" % (user_dn, userAccountControl)
        self.modify_ldif(mod)

    def force_password_change_at_next_login(self, search_filter):
        """Forces a password change at next login

        :param search_filter: LDAP filter to find the user (eg
            samccountname=name)
        """
        res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                          expression=search_filter, attrs=[])
        assert(len(res) == 1)
        user_dn = res[0].dn

        mod = """
dn: %s
changetype: modify
replace: pwdLastSet
pwdLastSet: 0
""" % (user_dn)
        self.modify_ldif(mod)

    def newgroup(self, groupname, groupou=None, grouptype=None,
                 description=None, mailaddress=None, notes=None, sd=None):
        """Adds a new group with additional parameters

        :param groupname: Name of the new group
        :param grouptype: Type of the new group
        :param description: Description of the new group
        :param mailaddress: Email address of the new group
        :param notes: Notes of the new group
        :param sd: security descriptor of the object
        """

        group_dn = "CN=%s,%s,%s" % (groupname, (groupou or "CN=Users"), self.domain_dn())

        # The new user record. Note the reliance on the SAMLDB module which
        # fills in the default informations
        ldbmessage = {"dn": group_dn,
            "sAMAccountName": groupname,
            "objectClass": "group"}

        if grouptype is not None:
            ldbmessage["groupType"] = "%d" % grouptype

        if description is not None:
            ldbmessage["description"] = description

        if mailaddress is not None:
            ldbmessage["mail"] = mailaddress

        if notes is not None:
            ldbmessage["info"] = notes

        if sd is not None:
            ldbmessage["nTSecurityDescriptor"] = ndr_pack(sd)

        self.add(ldbmessage)

    def deletegroup(self, groupname):
        """Deletes a group

        :param groupname: Name of the target group
        """

        groupfilter = "(&(sAMAccountName=%s)(objectCategory=%s,%s))" % (groupname, "CN=Group,CN=Schema,CN=Configuration", self.domain_dn())
        self.transaction_start()
        try:
            targetgroup = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                               expression=groupfilter, attrs=[])
            if len(targetgroup) == 0:
                raise Exception('Unable to find group "%s"' % groupname)
            assert(len(targetgroup) == 1)
            self.delete(targetgroup[0].dn)
        except Exception:
            self.transaction_cancel()
            raise
        else:
            self.transaction_commit()

    def add_remove_group_members(self, groupname, listofmembers,
                                  add_members_operation=True):
        """Adds or removes group members

        :param groupname: Name of the target group
        :param listofmembers: Comma-separated list of group members
        :param add_members_operation: Defines if its an add or remove
            operation
        """

        groupfilter = "(&(sAMAccountName=%s)(objectCategory=%s,%s))" % (groupname, "CN=Group,CN=Schema,CN=Configuration", self.domain_dn())
        groupmembers = listofmembers.split(',')

        self.transaction_start()
        try:
            targetgroup = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                               expression=groupfilter, attrs=['member'])
            if len(targetgroup) == 0:
                raise Exception('Unable to find group "%s"' % groupname)
            assert(len(targetgroup) == 1)

            modified = False

            addtargettogroup = """
dn: %s
changetype: modify
""" % (str(targetgroup[0].dn))

            for member in groupmembers:
                targetmember = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                                    expression="(|(sAMAccountName=%s)(CN=%s))" % (member, member), attrs=[])

                if len(targetmember) != 1:
                    continue

                if add_members_operation is True and (targetgroup[0].get('member') is None or str(targetmember[0].dn) not in targetgroup[0]['member']):
                    modified = True
                    addtargettogroup += """add: member
member: %s
""" % (str(targetmember[0].dn))

                elif add_members_operation is False and (targetgroup[0].get('member') is not None and str(targetmember[0].dn) in targetgroup[0]['member']):
                    modified = True
                    addtargettogroup += """delete: member
member: %s
""" % (str(targetmember[0].dn))

            if modified is True:
                self.modify_ldif(addtargettogroup)

        except Exception:
            self.transaction_cancel()
            raise
        else:
            self.transaction_commit()

    def newuser(self, username, password,
            force_password_change_at_next_login_req=False,
            useusernameascn=False, userou=None, surname=None, givenname=None,
            initials=None, profilepath=None, scriptpath=None, homedrive=None,
            homedirectory=None, jobtitle=None, department=None, company=None,
            description=None, mailaddress=None, internetaddress=None,
            telephonenumber=None, physicaldeliveryoffice=None, sd=None,
            setpassword=True):
        """Adds a new user with additional parameters

        :param username: Name of the new user
        :param password: Password for the new user
        :param force_password_change_at_next_login_req: Force password change
        :param useusernameascn: Use username as cn rather that firstname +
            initials + lastname
        :param userou: Object container (without domainDN postfix) for new user
        :param surname: Surname of the new user
        :param givenname: First name of the new user
        :param initials: Initials of the new user
        :param profilepath: Profile path of the new user
        :param scriptpath: Logon script path of the new user
        :param homedrive: Home drive of the new user
        :param homedirectory: Home directory of the new user
        :param jobtitle: Job title of the new user
        :param department: Department of the new user
        :param company: Company of the new user
        :param description: of the new user
        :param mailaddress: Email address of the new user
        :param internetaddress: Home page of the new user
        :param telephonenumber: Phone number of the new user
        :param physicaldeliveryoffice: Office location of the new user
        :param sd: security descriptor of the object
        :param setpassword: optionally disable password reset
        """

        displayname = ""
        if givenname is not None:
            displayname += givenname

        if initials is not None:
            displayname += ' %s.' % initials

        if surname is not None:
            displayname += ' %s' % surname

        cn = username
        if useusernameascn is None and displayname is not "":
            cn = displayname

        user_dn = "CN=%s,%s,%s" % (cn, (userou or "CN=Users"), self.domain_dn())

        dnsdomain = ldb.Dn(self, self.domain_dn()).canonical_str().replace("/", "")
        user_principal_name = "%s@%s" % (username, dnsdomain)
        # The new user record. Note the reliance on the SAMLDB module which
        # fills in the default informations
        ldbmessage = {"dn": user_dn,
                      "sAMAccountName": username,
                      "userPrincipalName": user_principal_name,
                      "objectClass": "user"}

        if surname is not None:
            ldbmessage["sn"] = surname

        if givenname is not None:
            ldbmessage["givenName"] = givenname

        if displayname is not "":
            ldbmessage["displayName"] = displayname
            ldbmessage["name"] = displayname

        if initials is not None:
            ldbmessage["initials"] = '%s.' % initials

        if profilepath is not None:
            ldbmessage["profilePath"] = profilepath

        if scriptpath is not None:
            ldbmessage["scriptPath"] = scriptpath

        if homedrive is not None:
            ldbmessage["homeDrive"] = homedrive

        if homedirectory is not None:
            ldbmessage["homeDirectory"] = homedirectory

        if jobtitle is not None:
            ldbmessage["title"] = jobtitle

        if department is not None:
            ldbmessage["department"] = department

        if company is not None:
            ldbmessage["company"] = company

        if description is not None:
            ldbmessage["description"] = description

        if mailaddress is not None:
            ldbmessage["mail"] = mailaddress

        if internetaddress is not None:
            ldbmessage["wWWHomePage"] = internetaddress

        if telephonenumber is not None:
            ldbmessage["telephoneNumber"] = telephonenumber

        if physicaldeliveryoffice is not None:
            ldbmessage["physicalDeliveryOfficeName"] = physicaldeliveryoffice

        if sd is not None:
            ldbmessage["nTSecurityDescriptor"] = ndr_pack(sd)

        self.transaction_start()
        try:
            self.add(ldbmessage)

            # Sets the password for it
            if setpassword:
                self.setpassword("(samAccountName=%s)" % username, password,
                                 force_password_change_at_next_login_req)
        except Exception:
            self.transaction_cancel()
            raise
        else:
            self.transaction_commit()

    def setpassword(self, search_filter, password,
            force_change_at_next_login=False, username=None):
        """Sets the password for a user

        :param search_filter: LDAP filter to find the user (eg
            samccountname=name)
        :param password: Password for the user
        :param force_change_at_next_login: Force password change
        """
        self.transaction_start()
        try:
            res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                              expression=search_filter, attrs=[])
            if len(res) == 0:
                raise Exception('Unable to find user "%s"' % (username or search_filter))
            if len(res) > 1:
                raise Exception('Matched %u multiple users with filter "%s"' % (len(res), search_filter))
            user_dn = res[0].dn
            setpw = """
dn: %s
changetype: modify
replace: unicodePwd
unicodePwd:: %s
""" % (user_dn, base64.b64encode(("\"" + password + "\"").encode('utf-16-le')))

            self.modify_ldif(setpw)

            if force_change_at_next_login:
                self.force_password_change_at_next_login(
                  "(dn=" + str(user_dn) + ")")

            #  modify the userAccountControl to remove the disabled bit
            self.enable_account(search_filter)
        except Exception:
            self.transaction_cancel()
            raise
        else:
            self.transaction_commit()

    def setexpiry(self, search_filter, expiry_seconds, no_expiry_req=False):
        """Sets the account expiry for a user

        :param search_filter: LDAP filter to find the user (eg
            samaccountname=name)
        :param expiry_seconds: expiry time from now in seconds
        :param no_expiry_req: if set, then don't expire password
        """
        self.transaction_start()
        try:
            res = self.search(base=self.domain_dn(), scope=ldb.SCOPE_SUBTREE,
                          expression=search_filter,
                          attrs=["userAccountControl", "accountExpires"])
            assert(len(res) == 1)
            user_dn = res[0].dn

            userAccountControl = int(res[0]["userAccountControl"][0])
            accountExpires     = int(res[0]["accountExpires"][0])
            if no_expiry_req:
                userAccountControl = userAccountControl | 0x10000
                accountExpires = 0
            else:
                userAccountControl = userAccountControl & ~0x10000
                accountExpires = samba.unix2nttime(expiry_seconds + int(time.time()))

            setexp = """
dn: %s
changetype: modify
replace: userAccountControl
userAccountControl: %u
replace: accountExpires
accountExpires: %u
""" % (user_dn, userAccountControl, accountExpires)

            self.modify_ldif(setexp)
        except Exception:
            self.transaction_cancel()
            raise
        else:
            self.transaction_commit()

    def set_domain_sid(self, sid):
        """Change the domain SID used by this LDB.

        :param sid: The new domain sid to use.
        """
        dsdb._samdb_set_domain_sid(self, sid)

    def get_domain_sid(self):
        """Read the domain SID used by this LDB. """
        return dsdb._samdb_get_domain_sid(self)

    domain_sid = property(get_domain_sid, set_domain_sid,
        "SID for the domain")

    def set_invocation_id(self, invocation_id):
        """Set the invocation id for this SamDB handle.

        :param invocation_id: GUID of the invocation id.
        """
        dsdb._dsdb_set_ntds_invocation_id(self, invocation_id)

    def get_invocation_id(self):
        """Get the invocation_id id"""
        return dsdb._samdb_ntds_invocation_id(self)

    invocation_id = property(get_invocation_id, set_invocation_id,
        "Invocation ID GUID")

    def get_oid_from_attid(self, attid):
        return dsdb._dsdb_get_oid_from_attid(self, attid)

    def get_attid_from_lDAPDisplayName(self, ldap_display_name,
            is_schema_nc=False):
        return dsdb._dsdb_get_attid_from_lDAPDisplayName(self,
            ldap_display_name, is_schema_nc)

    def set_ntds_settings_dn(self, ntds_settings_dn):
        """Set the NTDS Settings DN, as would be returned on the dsServiceName
        rootDSE attribute.

        This allows the DN to be set before the database fully exists

        :param ntds_settings_dn: The new DN to use
        """
        dsdb._samdb_set_ntds_settings_dn(self, ntds_settings_dn)

    def get_ntds_GUID(self):
        """Get the NTDS objectGUID"""
        return dsdb._samdb_ntds_objectGUID(self)

    def server_site_name(self):
        """Get the server site name"""
        return dsdb._samdb_server_site_name(self)

    def load_partition_usn(self, base_dn):
        return dsdb._dsdb_load_partition_usn(self, base_dn)

    def set_schema(self, schema):
        self.set_schema_from_ldb(schema.ldb)

    def set_schema_from_ldb(self, ldb_conn):
        dsdb._dsdb_set_schema_from_ldb(self, ldb_conn)

    def dsdb_DsReplicaAttribute(self, ldb, ldap_display_name, ldif_elements):
        return dsdb._dsdb_DsReplicaAttribute(ldb, ldap_display_name, ldif_elements)

    def get_attribute_from_attid(self, attid):
        """ Get from an attid the associated attribute

        :param attid: The attribute id for searched attribute
        :return: The name of the attribute associated with this id
        """
        if len(self.hash_oid_name.keys()) == 0:
            self._populate_oid_attid()
        if self.hash_oid_name.has_key(self.get_oid_from_attid(attid)):
            return self.hash_oid_name[self.get_oid_from_attid(attid)]
        else:
            return None

    def _populate_oid_attid(self):
        """Populate the hash hash_oid_name.

        This hash contains the oid of the attribute as a key and
        its display name as a value
        """
        self.hash_oid_name = {}
        res = self.search(expression="objectClass=attributeSchema",
                           controls=["search_options:1:2"],
                           attrs=["attributeID",
                           "lDAPDisplayName"])
        if len(res) > 0:
            for e in res:
                strDisplay = str(e.get("lDAPDisplayName"))
                self.hash_oid_name[str(e.get("attributeID"))] = strDisplay

    def get_attribute_replmetadata_version(self, dn, att):
        """Get the version field trom the replPropertyMetaData for
        the given field

        :param dn: The on which we want to get the version
        :param att: The name of the attribute
        :return: The value of the version field in the replPropertyMetaData
            for the given attribute. None if the attribute is not replicated
        """

        res = self.search(expression="dn=%s" % dn,
                            scope=ldb.SCOPE_SUBTREE,
                            controls=["search_options:1:2"],
                            attrs=["replPropertyMetaData"])
        if len(res) == 0:
            return None

        repl = ndr_unpack(drsblobs.replPropertyMetaDataBlob,
                            str(res[0]["replPropertyMetaData"]))
        ctr = repl.ctr
        if len(self.hash_oid_name.keys()) == 0:
            self._populate_oid_attid()
        for o in ctr.array:
            # Search for Description
            att_oid = self.get_oid_from_attid(o.attid)
            if self.hash_oid_name.has_key(att_oid) and\
               att.lower() == self.hash_oid_name[att_oid].lower():
                return o.version
        return None

    def set_attribute_replmetadata_version(self, dn, att, value,
            addifnotexist=False):
        res = self.search(expression="dn=%s" % dn,
                            scope=ldb.SCOPE_SUBTREE,
                            controls=["search_options:1:2"],
                            attrs=["replPropertyMetaData"])
        if len(res) == 0:
            return None

        repl = ndr_unpack(drsblobs.replPropertyMetaDataBlob,
                            str(res[0]["replPropertyMetaData"]))
        ctr = repl.ctr
        now = samba.unix2nttime(int(time.time()))
        found = False
        if len(self.hash_oid_name.keys()) == 0:
            self._populate_oid_attid()
        for o in ctr.array:
            # Search for Description
            att_oid = self.get_oid_from_attid(o.attid)
            if self.hash_oid_name.has_key(att_oid) and\
               att.lower() == self.hash_oid_name[att_oid].lower():
                found = True
                seq = self.sequence_number(ldb.SEQ_NEXT)
                o.version = value
                o.originating_change_time = now
                o.originating_invocation_id = misc.GUID(self.get_invocation_id())
                o.originating_usn = seq
                o.local_usn = seq

        if not found and addifnotexist and len(ctr.array) >0:
            o2 = drsblobs.replPropertyMetaData1()
            o2.attid = 589914
            att_oid = self.get_oid_from_attid(o2.attid)
            seq = self.sequence_number(ldb.SEQ_NEXT)
            o2.version = value
            o2.originating_change_time = now
            o2.originating_invocation_id = misc.GUID(self.get_invocation_id())
            o2.originating_usn = seq
            o2.local_usn = seq
            found = True
            tab = ctr.array
            tab.append(o2)
            ctr.count = ctr.count + 1
            ctr.array = tab

        if found :
            replBlob = ndr_pack(repl)
            msg = ldb.Message()
            msg.dn = res[0].dn
            msg["replPropertyMetaData"] = ldb.MessageElement(replBlob,
                                                ldb.FLAG_MOD_REPLACE,
                                                "replPropertyMetaData")
            self.modify(msg, ["local_oid:1.3.6.1.4.1.7165.4.3.14:0"])

    def write_prefixes_from_schema(self):
        dsdb._dsdb_write_prefixes_from_schema_to_ldb(self)

    def get_partitions_dn(self):
        return dsdb._dsdb_get_partitions_dn(self)

    def set_minPwdAge(self, value):
        m = ldb.Message()
        m.dn = ldb.Dn(self, self.domain_dn())
        m["minPwdAge"] = ldb.MessageElement(value, ldb.FLAG_MOD_REPLACE, "minPwdAge")
        self.modify(m)

    def get_minPwdAge(self):
        res = self.search(self.domain_dn(), scope=ldb.SCOPE_BASE, attrs=["minPwdAge"])
        if len(res) == 0:
            return None
        elif not "minPwdAge" in res[0]:
            return None
        else:
            return res[0]["minPwdAge"][0]

    def set_minPwdLength(self, value):
        m = ldb.Message()
        m.dn = ldb.Dn(self, self.domain_dn())
        m["minPwdLength"] = ldb.MessageElement(value, ldb.FLAG_MOD_REPLACE, "minPwdLength")
        self.modify(m)

    def get_minPwdLength(self):
        res = self.search(self.domain_dn(), scope=ldb.SCOPE_BASE, attrs=["minPwdLength"])
        if len(res) == 0:
            return None
        elif not "minPwdLength" in res[0]:
            return None
        else:
            return res[0]["minPwdLength"][0]

    def set_pwdProperties(self, value):
        m = ldb.Message()
        m.dn = ldb.Dn(self, self.domain_dn())
        m["pwdProperties"] = ldb.MessageElement(value, ldb.FLAG_MOD_REPLACE, "pwdProperties")
        self.modify(m)

    def get_pwdProperties(self):
        res = self.search(self.domain_dn(), scope=ldb.SCOPE_BASE, attrs=["pwdProperties"])
        if len(res) == 0:
            return None
        elif not "pwdProperties" in res[0]:
            return None
        else:
            return res[0]["pwdProperties"][0]

    def set_dsheuristics(self, dsheuristics):
        m = ldb.Message()
        m.dn = ldb.Dn(self, "CN=Directory Service,CN=Windows NT,CN=Services,%s"
                      % self.get_config_basedn().get_linearized())
        if dsheuristics is not None:
            m["dSHeuristics"] = ldb.MessageElement(dsheuristics,
                ldb.FLAG_MOD_REPLACE, "dSHeuristics")
        else:
            m["dSHeuristics"] = ldb.MessageElement([], ldb.FLAG_MOD_DELETE,
                "dSHeuristics")
        self.modify(m)

    def get_dsheuristics(self):
        res = self.search("CN=Directory Service,CN=Windows NT,CN=Services,%s"
                          % self.get_config_basedn().get_linearized(),
                          scope=ldb.SCOPE_BASE, attrs=["dSHeuristics"])
        if len(res) == 0:
            dsheuristics = None
        elif "dSHeuristics" in res[0]:
            dsheuristics = res[0]["dSHeuristics"][0]
        else:
            dsheuristics = None

        return dsheuristics

    def create_ou(self, ou_dn, description=None, name=None, sd=None):
        """Creates an organizationalUnit object
        :param ou_dn: dn of the new object
        :param description: description attribute
        :param name: name atttribute
        :param sd: security descriptor of the object, can be
        an SDDL string or security.descriptor type
        """
        m = {"dn": ou_dn,
             "objectClass": "organizationalUnit"}

        if description:
            m["description"] = description
        if name:
            m["name"] = name

        if sd:
            m["nTSecurityDescriptor"] = ndr_pack(sd)
        self.add(m)
