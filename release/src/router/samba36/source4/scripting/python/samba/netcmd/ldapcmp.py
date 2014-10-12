#!/usr/bin/env python
#
# Unix SMB/CIFS implementation.
# A command to compare differences of objects and attributes between
# two LDAP servers both running at the same time. It generally compares
# one of the three pratitions DOMAIN, CONFIGURATION or SCHEMA. Users
# that have to be provided sheould be able to read objects in any of the
# above partitions.

# Copyright (C) Zahari Zahariev <zahari.zahariev@postpath.com> 2009, 2010
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

import os
import re
import sys

import samba
import samba.getopt as options
from samba import Ldb
from samba.ndr import ndr_pack, ndr_unpack
from samba.dcerpc import security
from ldb import SCOPE_SUBTREE, SCOPE_ONELEVEL, SCOPE_BASE, ERR_NO_SUCH_OBJECT, LdbError
from samba.netcmd import (
    Command,
    CommandError,
    Option,
    SuperCommand,
    )

global summary
summary = {}

class LDAPBase(object):

    def __init__(self, host, creds, lp,
                 two=False, quiet=False, descriptor=False, sort_aces=False, verbose=False,
                 view="section", base="", scope="SUB"):
        ldb_options = []
        samdb_url = host
        if not "://" in host:
            if os.path.isfile(host):
                samdb_url = "tdb://%s" % host
            else:
                samdb_url = "ldap://%s" % host
        # use 'paged_search' module when connecting remotely
        if samdb_url.lower().startswith("ldap://"):
            ldb_options = ["modules:paged_searches"]
        self.ldb = Ldb(url=samdb_url,
                       credentials=creds,
                       lp=lp,
                       options=ldb_options)
        self.search_base = base
        self.search_scope = scope
        self.two_domains = two
        self.quiet = quiet
        self.descriptor = descriptor
        self.sort_aces = sort_aces
        self.view = view
        self.verbose = verbose
        self.host = host
        self.base_dn = self.find_basedn()
        self.domain_netbios = self.find_netbios()
        self.server_names = self.find_servers()
        self.domain_name = re.sub("[Dd][Cc]=", "", self.base_dn).replace(",", ".")
        self.domain_sid = self.find_domain_sid()
        self.get_guid_map()
        self.get_sid_map()
        #
        # Log some domain controller specific place-holers that are being used
        # when compare content of two DCs. Uncomment for DEBUG purposes.
        if self.two_domains and not self.quiet:
            print "\n* Place-holders for %s:" % self.host
            print 4*" " + "${DOMAIN_DN}      => %s" % self.base_dn
            print 4*" " + "${DOMAIN_NETBIOS} => %s" % self.domain_netbios
            print 4*" " + "${SERVER_NAME}     => %s" % self.server_names
            print 4*" " + "${DOMAIN_NAME}    => %s" % self.domain_name

    def find_domain_sid(self):
        res = self.ldb.search(base=self.base_dn, expression="(objectClass=*)", scope=SCOPE_BASE)
        return ndr_unpack(security.dom_sid,res[0]["objectSid"][0])

    def find_servers(self):
        """
        """
        res = self.ldb.search(base="OU=Domain Controllers,%s" % self.base_dn, \
                scope=SCOPE_SUBTREE, expression="(objectClass=computer)", attrs=["cn"])
        assert len(res) > 0
        srv = []
        for x in res:
            srv.append(x["cn"][0])
        return srv

    def find_netbios(self):
        res = self.ldb.search(base="CN=Partitions,CN=Configuration,%s" % self.base_dn, \
                scope=SCOPE_SUBTREE, attrs=["nETBIOSName"])
        assert len(res) > 0
        for x in res:
            if "nETBIOSName" in x.keys():
                return x["nETBIOSName"][0]

    def find_basedn(self):
        res = self.ldb.search(base="", expression="(objectClass=*)", scope=SCOPE_BASE,
                attrs=["defaultNamingContext"])
        assert len(res) == 1
        return res[0]["defaultNamingContext"][0]

    def object_exists(self, object_dn):
        res = None
        try:
            res = self.ldb.search(base=object_dn, scope=SCOPE_BASE)
        except LdbError, (enum, estr):
            if enum == ERR_NO_SUCH_OBJECT:
                return False
            raise
        return len(res) == 1

    def delete_force(self, object_dn):
        try:
            self.ldb.delete(object_dn)
        except Ldb.LdbError, e:
            assert "No such object" in str(e)

    def get_attribute_name(self, key):
        """ Returns the real attribute name
            It resolved ranged results e.g. member;range=0-1499
        """

        r = re.compile("^([^;]+);range=(\d+)-(\d+|\*)$")

        m = r.match(key)
        if m is None:
            return key

        return m.group(1)

    def get_attribute_values(self, object_dn, key, vals):
        """ Returns list with all attribute values
            It resolved ranged results e.g. member;range=0-1499
        """

        r = re.compile("^([^;]+);range=(\d+)-(\d+|\*)$")

        m = r.match(key)
        if m is None:
            # no range, just return the values
            return vals

        attr = m.group(1)
        hi = int(m.group(3))

        # get additional values in a loop
        # until we get a response with '*' at the end
        while True:

            n = "%s;range=%d-*" % (attr, hi + 1)
            res = self.ldb.search(base=object_dn, scope=SCOPE_BASE, attrs=[n])
            assert len(res) == 1
            res = dict(res[0])
            del res["dn"]

            fm = None
            fvals = None

            for key in res.keys():
                m = r.match(key)

                if m is None:
                    continue

                if m.group(1) != attr:
                    continue

                fm = m
                fvals = list(res[key])
                break

            if fm is None:
                break

            vals.extend(fvals)
            if fm.group(3) == "*":
                # if we got "*" we're done
                break

            assert int(fm.group(2)) == hi + 1
            hi = int(fm.group(3))

        return vals

    def get_attributes(self, object_dn):
        """ Returns dict with all default visible attributes
        """
        res = self.ldb.search(base=object_dn, scope=SCOPE_BASE, attrs=["*"])
        assert len(res) == 1
        res = dict(res[0])
        # 'Dn' element is not iterable and we have it as 'distinguishedName'
        del res["dn"]
        for key in res.keys():
            vals = list(res[key])
            del res[key]
            name = self.get_attribute_name(key)
            res[name] = self.get_attribute_values(object_dn, key, vals)

        return res

    def get_descriptor_sddl(self, object_dn):
        res = self.ldb.search(base=object_dn, scope=SCOPE_BASE, attrs=["nTSecurityDescriptor"])
        desc = res[0]["nTSecurityDescriptor"][0]
        desc = ndr_unpack(security.descriptor, desc)
        return desc.as_sddl(self.domain_sid)

    def guid_as_string(self, guid_blob):
        """ Translate binary representation of schemaIDGUID to standard string representation.
            @gid_blob: binary schemaIDGUID
        """
        blob = "%s" % guid_blob
        stops = [4, 2, 2, 2, 6]
        index = 0
        res = ""
        x = 0
        while x < len(stops):
            tmp = ""
            y = 0
            while y < stops[x]:
                c = hex(ord(blob[index])).replace("0x", "")
                c = [None, "0" + c, c][len(c)]
                if 2 * index < len(blob):
                    tmp = c + tmp
                else:
                    tmp += c
                index += 1
                y += 1
            res += tmp + " "
            x += 1
        assert index == len(blob)
        return res.strip().replace(" ", "-")

    def get_guid_map(self):
        """ Build dictionary that maps GUID to 'name' attribute found in Schema or Extended-Rights.
        """
        self.guid_map = {}
        res = self.ldb.search(base="cn=schema,cn=configuration,%s" % self.base_dn, \
                expression="(schemaIdGuid=*)", scope=SCOPE_SUBTREE, attrs=["schemaIdGuid", "name"])
        for item in res:
            self.guid_map[self.guid_as_string(item["schemaIdGuid"]).lower()] = item["name"][0]
        #
        res = self.ldb.search(base="cn=extended-rights,cn=configuration,%s" % self.base_dn, \
                expression="(rightsGuid=*)", scope=SCOPE_SUBTREE, attrs=["rightsGuid", "name"])
        for item in res:
            self.guid_map[str(item["rightsGuid"]).lower()] = item["name"][0]

    def get_sid_map(self):
        """ Build dictionary that maps GUID to 'name' attribute found in Schema or Extended-Rights.
        """
        self.sid_map = {}
        res = self.ldb.search(base="%s" % self.base_dn, \
                expression="(objectSid=*)", scope=SCOPE_SUBTREE, attrs=["objectSid", "sAMAccountName"])
        for item in res:
            try:
                self.sid_map["%s" % ndr_unpack(security.dom_sid, item["objectSid"][0])] = item["sAMAccountName"][0]
            except KeyError:
                pass

class Descriptor(object):
    def __init__(self, connection, dn):
        self.con = connection
        self.dn = dn
        self.sddl = self.con.get_descriptor_sddl(self.dn)
        self.dacl_list = self.extract_dacl()
        if self.con.sort_aces:
            self.dacl_list.sort()

    def extract_dacl(self):
        """ Extracts the DACL as a list of ACE string (with the brakets).
        """
        try:
            if "S:" in self.sddl:
                res = re.search("D:(.*?)(\(.*?\))S:", self.sddl).group(2)
            else:
                res = re.search("D:(.*?)(\(.*\))", self.sddl).group(2)
        except AttributeError:
            return []
        return re.findall("(\(.*?\))", res)

    def fix_guid(self, ace):
        res = "%s" % ace
        guids = re.findall("[a-z0-9]+?-[a-z0-9]+-[a-z0-9]+-[a-z0-9]+-[a-z0-9]+", res)
        # If there are not GUIDs to replace return the same ACE
        if len(guids) == 0:
            return res
        for guid in guids:
            try:
                name = self.con.guid_map[guid.lower()]
                res = res.replace(guid, name)
            except KeyError:
                # Do not bother if the GUID is not found in
                # cn=Schema or cn=Extended-Rights
                pass
        return res

    def fix_sid(self, ace):
        res = "%s" % ace
        sids = re.findall("S-[-0-9]+", res)
        # If there are not SIDs to replace return the same ACE
        if len(sids) == 0:
            return res
        for sid in sids:
            try:
                name = self.con.sid_map[sid]
                res = res.replace(sid, name)
            except KeyError:
                # Do not bother if the SID is not found in baseDN
                pass
        return res

    def fixit(self, ace):
        """ Combine all replacement methods in one
        """
        res = "%s" % ace
        res = self.fix_guid(res)
        res = self.fix_sid(res)
        return res

    def diff_1(self, other):
        res = ""
        if len(self.dacl_list) != len(other.dacl_list):
            res += 4*" " + "Difference in ACE count:\n"
            res += 8*" " + "=> %s\n" % len(self.dacl_list)
            res += 8*" " + "=> %s\n" % len(other.dacl_list)
        #
        i = 0
        flag = True
        while True:
            self_ace = None
            other_ace = None
            try:
                self_ace = "%s" % self.dacl_list[i]
            except IndexError:
                self_ace = ""
            #
            try:
                other_ace = "%s" % other.dacl_list[i]
            except IndexError:
                other_ace = ""
            if len(self_ace) + len(other_ace) == 0:
                break
            self_ace_fixed = "%s" % self.fixit(self_ace)
            other_ace_fixed = "%s" % other.fixit(other_ace)
            if self_ace_fixed != other_ace_fixed:
                res += "%60s * %s\n" % ( self_ace_fixed, other_ace_fixed )
                flag = False
            else:
                res += "%60s | %s\n" % ( self_ace_fixed, other_ace_fixed )
            i += 1
        return (flag, res)

    def diff_2(self, other):
        res = ""
        if len(self.dacl_list) != len(other.dacl_list):
            res += 4*" " + "Difference in ACE count:\n"
            res += 8*" " + "=> %s\n" % len(self.dacl_list)
            res += 8*" " + "=> %s\n" % len(other.dacl_list)
        #
        common_aces = []
        self_aces = []
        other_aces = []
        self_dacl_list_fixed = []
        other_dacl_list_fixed = []
        [self_dacl_list_fixed.append( self.fixit(ace) ) for ace in self.dacl_list]
        [other_dacl_list_fixed.append( other.fixit(ace) ) for ace in other.dacl_list]
        for ace in self_dacl_list_fixed:
            try:
                other_dacl_list_fixed.index(ace)
            except ValueError:
                self_aces.append(ace)
            else:
                common_aces.append(ace)
        self_aces = sorted(self_aces)
        if len(self_aces) > 0:
            res += 4*" " + "ACEs found only in %s:\n" % self.con.host
            for ace in self_aces:
                res += 8*" " + ace + "\n"
        #
        for ace in other_dacl_list_fixed:
            try:
                self_dacl_list_fixed.index(ace)
            except ValueError:
                other_aces.append(ace)
            else:
                common_aces.append(ace)
        other_aces = sorted(other_aces)
        if len(other_aces) > 0:
            res += 4*" " + "ACEs found only in %s:\n" % other.con.host
            for ace in other_aces:
                res += 8*" " + ace + "\n"
        #
        common_aces = sorted(list(set(common_aces)))
        if self.con.verbose:
            res += 4*" " + "ACEs found in both:\n"
            for ace in common_aces:
                res += 8*" " + ace + "\n"
        return (self_aces == [] and other_aces == [], res)

class LDAPObject(object):
    def __init__(self, connection, dn, summary):
        self.con = connection
        self.two_domains = self.con.two_domains
        self.quiet = self.con.quiet
        self.verbose = self.con.verbose
        self.summary = summary
        self.dn = dn.replace("${DOMAIN_DN}", self.con.base_dn)
        self.dn = self.dn.replace("CN=${DOMAIN_NETBIOS}", "CN=%s" % self.con.domain_netbios)
        for x in self.con.server_names:
            self.dn = self.dn.replace("CN=${SERVER_NAME}", "CN=%s" % x)
        self.attributes = self.con.get_attributes(self.dn)
        # Attributes that are considered always to be different e.g based on timestamp etc.
        #
        # One domain - two domain controllers
        self.ignore_attributes =  [
                # Default Naming Context
                "lastLogon", "lastLogoff", "badPwdCount", "logonCount", "badPasswordTime", "modifiedCount",
                "operatingSystemVersion","oEMInformation",
                # Configuration Naming Context
                "repsFrom", "dSCorePropagationData", "msExchServer1HighestUSN",
                "replUpToDateVector", "repsTo", "whenChanged", "uSNChanged", "uSNCreated",
                # Schema Naming Context
                "prefixMap",]
        self.dn_attributes = []
        self.domain_attributes = []
        self.servername_attributes = []
        self.netbios_attributes = []
        self.other_attributes = []
        # Two domains - two domain controllers

        if self.two_domains:
            self.ignore_attributes +=  [
                "objectCategory", "objectGUID", "objectSid", "whenCreated", "pwdLastSet", "uSNCreated", "creationTime",
                "modifiedCount", "priorSetTime", "rIDManagerReference", "gPLink", "ipsecNFAReference",
                "fRSPrimaryMember", "fSMORoleOwner", "masteredBy", "ipsecOwnersReference", "wellKnownObjects",
                "badPwdCount", "ipsecISAKMPReference", "ipsecFilterReference", "msDs-masteredBy", "lastSetTime",
                "ipsecNegotiationPolicyReference", "subRefs", "gPCFileSysPath", "accountExpires", "invocationId",
                # After Exchange preps
                "targetAddress", "msExchMailboxGuid", "siteFolderGUID"]
            #
            # Attributes that contain the unique DN tail part e.g. 'DC=samba,DC=org'
            self.dn_attributes = [
                "distinguishedName", "defaultObjectCategory", "member", "memberOf", "siteList", "nCName",
                "homeMDB", "homeMTA", "interSiteTopologyGenerator", "serverReference",
                "msDS-HasInstantiatedNCs", "hasMasterNCs", "msDS-hasMasterNCs", "msDS-HasDomainNCs", "dMDLocation",
                "msDS-IsDomainFor", "rIDSetReferences", "serverReferenceBL",
                # After Exchange preps
                "msExchHomeRoutingGroup", "msExchResponsibleMTAServer", "siteFolderServer", "msExchRoutingMasterDN",
                "msExchRoutingGroupMembersBL", "homeMDBBL", "msExchHomePublicMDB", "msExchOwningServer", "templateRoots",
                "addressBookRoots", "msExchPolicyRoots", "globalAddressList", "msExchOwningPFTree",
                "msExchResponsibleMTAServerBL", "msExchOwningPFTreeBL",]
            self.dn_attributes = [x.upper() for x in self.dn_attributes]
            #
            # Attributes that contain the Domain name e.g. 'samba.org'
            self.domain_attributes = [
                "proxyAddresses", "mail", "userPrincipalName", "msExchSmtpFullyQualifiedDomainName",
                "dnsHostName", "networkAddress", "dnsRoot", "servicePrincipalName",]
            self.domain_attributes = [x.upper() for x in self.domain_attributes]
            #
            # May contain DOMAIN_NETBIOS and SERVER_NAME
            self.servername_attributes = [ "distinguishedName", "name", "CN", "sAMAccountName", "dNSHostName",
                "servicePrincipalName", "rIDSetReferences", "serverReference", "serverReferenceBL",
                "msDS-IsDomainFor", "interSiteTopologyGenerator",]
            self.servername_attributes = [x.upper() for x in self.servername_attributes]
            #
            self.netbios_attributes = [ "servicePrincipalName", "CN", "distinguishedName", "nETBIOSName", "name",]
            self.netbios_attributes = [x.upper() for x in self.netbios_attributes]
            #
            self.other_attributes = [ "name", "DC",]
            self.other_attributes = [x.upper() for x in self.other_attributes]
        #
        self.ignore_attributes = [x.upper() for x in self.ignore_attributes]

    def log(self, msg):
        """
        Log on the screen if there is no --quiet oprion set
        """
        if not self.quiet:
            print msg

    def fix_dn(self, s):
        res = "%s" % s
        if not self.two_domains:
            return res
        if res.upper().endswith(self.con.base_dn.upper()):
            res = res[:len(res)-len(self.con.base_dn)] + "${DOMAIN_DN}"
        return res

    def fix_domain_name(self, s):
        res = "%s" % s
        if not self.two_domains:
            return res
        res = res.replace(self.con.domain_name.lower(), self.con.domain_name.upper())
        res = res.replace(self.con.domain_name.upper(), "${DOMAIN_NAME}")
        return res

    def fix_domain_netbios(self, s):
        res = "%s" % s
        if not self.two_domains:
            return res
        res = res.replace(self.con.domain_netbios.lower(), self.con.domain_netbios.upper())
        res = res.replace(self.con.domain_netbios.upper(), "${DOMAIN_NETBIOS}")
        return res

    def fix_server_name(self, s):
        res = "%s" % s
        if not self.two_domains or len(self.con.server_names) > 1:
            return res
        for x in self.con.server_names:
            res = res.upper().replace(x, "${SERVER_NAME}")
        return res

    def __eq__(self, other):
        if self.con.descriptor:
            return self.cmp_desc(other)
        return self.cmp_attrs(other)

    def cmp_desc(self, other):
        d1 = Descriptor(self.con, self.dn)
        d2 = Descriptor(other.con, other.dn)
        if self.con.view == "section":
            res = d1.diff_2(d2)
        elif self.con.view == "collision":
            res = d1.diff_1(d2)
        else:
            raise Exception("Unknown --view option value.")
        #
        self.screen_output = res[1][:-1]
        other.screen_output = res[1][:-1]
        #
        return res[0]

    def cmp_attrs(self, other):
        res = ""
        self.unique_attrs = []
        self.df_value_attrs = []
        other.unique_attrs = []
        if self.attributes.keys() != other.attributes.keys():
            #
            title = 4*" " + "Attributes found only in %s:" % self.con.host
            for x in self.attributes.keys():
                if not x in other.attributes.keys() and \
                not x.upper() in [q.upper() for q in other.ignore_attributes]:
                    if title:
                        res += title + "\n"
                        title = None
                    res += 8*" " + x + "\n"
                    self.unique_attrs.append(x)
            #
            title = 4*" " + "Attributes found only in %s:" % other.con.host
            for x in other.attributes.keys():
                if not x in self.attributes.keys() and \
                not x.upper() in [q.upper() for q in self.ignore_attributes]:
                    if title:
                        res += title + "\n"
                        title = None
                    res += 8*" " + x + "\n"
                    other.unique_attrs.append(x)
        #
        missing_attrs = [x.upper() for x in self.unique_attrs]
        missing_attrs += [x.upper() for x in other.unique_attrs]
        title = 4*" " + "Difference in attribute values:"
        for x in self.attributes.keys():
            if x.upper() in self.ignore_attributes or x.upper() in missing_attrs:
                continue
            if isinstance(self.attributes[x], list) and isinstance(other.attributes[x], list):
                self.attributes[x] = sorted(self.attributes[x])
                other.attributes[x] = sorted(other.attributes[x])
            if self.attributes[x] != other.attributes[x]:
                p = None
                q = None
                m = None
                n = None
                # First check if the difference can be fixed but shunting the first part
                # of the DomainHostName e.g. 'mysamba4.test.local' => 'mysamba4'
                if x.upper() in self.other_attributes:
                    p = [self.con.domain_name.split(".")[0] == j for j in self.attributes[x]]
                    q = [other.con.domain_name.split(".")[0] == j for j in other.attributes[x]]
                    if p == q:
                        continue
                # Attribute values that are list that contain DN based values that may differ
                elif x.upper() in self.dn_attributes:
                    m = p
                    n = q
                    if not p and not q:
                        m = self.attributes[x]
                        n = other.attributes[x]
                    p = [self.fix_dn(j) for j in m]
                    q = [other.fix_dn(j) for j in n]
                    if p == q:
                        continue
                # Attributes that contain the Domain name in them
                if x.upper() in self.domain_attributes:
                    m = p
                    n = q
                    if not p and not q:
                        m = self.attributes[x]
                        n = other.attributes[x]
                    p = [self.fix_domain_name(j) for j in m]
                    q = [other.fix_domain_name(j) for j in n]
                    if p == q:
                        continue
                #
                if x.upper() in self.servername_attributes:
                    # Attributes with SERVER_NAME
                    m = p
                    n = q
                    if not p and not q:
                        m = self.attributes[x]
                        n = other.attributes[x]
                    p = [self.fix_server_name(j) for j in m]
                    q = [other.fix_server_name(j) for j in n]
                    if p == q:
                        continue
                #
                if x.upper() in self.netbios_attributes:
                    # Attributes with NETBIOS Domain name
                    m = p
                    n = q
                    if not p and not q:
                        m = self.attributes[x]
                        n = other.attributes[x]
                    p = [self.fix_domain_netbios(j) for j in m]
                    q = [other.fix_domain_netbios(j) for j in n]
                    if p == q:
                        continue
                #
                if title:
                    res += title + "\n"
                    title = None
                if p and q:
                    res += 8*" " + x + " => \n%s\n%s" % (p, q) + "\n"
                else:
                    res += 8*" " + x + " => \n%s\n%s" % (self.attributes[x], other.attributes[x]) + "\n"
                self.df_value_attrs.append(x)
        #
        if self.unique_attrs + other.unique_attrs != []:
            assert self.unique_attrs != other.unique_attrs
        self.summary["unique_attrs"] += self.unique_attrs
        self.summary["df_value_attrs"] += self.df_value_attrs
        other.summary["unique_attrs"] += other.unique_attrs
        other.summary["df_value_attrs"] += self.df_value_attrs # they are the same
        #
        self.screen_output = res[:-1]
        other.screen_output = res[:-1]
        #
        return res == ""


class LDAPBundel(object):
    def __init__(self, connection, context, dn_list=None):
        self.con = connection
        self.two_domains = self.con.two_domains
        self.quiet = self.con.quiet
        self.verbose = self.con.verbose
        self.search_base = self.con.search_base
        self.search_scope = self.con.search_scope
        self.summary = {}
        self.summary["unique_attrs"] = []
        self.summary["df_value_attrs"] = []
        self.summary["known_ignored_dn"] = []
        self.summary["abnormal_ignored_dn"] = []
        if dn_list:
            self.dn_list = dn_list
        elif context.upper() in ["DOMAIN", "CONFIGURATION", "SCHEMA"]:
            self.context = context.upper()
            self.dn_list = self.get_dn_list(context)
        else:
            raise Exception("Unknown initialization data for LDAPBundel().")
        counter = 0
        while counter < len(self.dn_list) and self.two_domains:
            # Use alias reference
            tmp = self.dn_list[counter]
            tmp = tmp[:len(tmp)-len(self.con.base_dn)] + "${DOMAIN_DN}"
            tmp = tmp.replace("CN=%s" % self.con.domain_netbios, "CN=${DOMAIN_NETBIOS}")
            if len(self.con.server_names) == 1:
                for x in self.con.server_names:
                    tmp = tmp.replace("CN=%s" % x, "CN=${SERVER_NAME}")
            self.dn_list[counter] = tmp
            counter += 1
        self.dn_list = list(set(self.dn_list))
        self.dn_list = sorted(self.dn_list)
        self.size = len(self.dn_list)

    def log(self, msg):
        """
        Log on the screen if there is no --quiet oprion set
        """
        if not self.quiet:
            print msg

    def update_size(self):
        self.size = len(self.dn_list)
        self.dn_list = sorted(self.dn_list)

    def __eq__(self, other):
        res = True
        if self.size != other.size:
            self.log( "\n* DN lists have different size: %s != %s" % (self.size, other.size) )
            res = False
        #
        # This is the case where we want to explicitly compare two objects with different DNs.
        # It does not matter if they are in the same DC, in two DC in one domain or in two
        # different domains.
        if self.search_scope != SCOPE_BASE:
            title= "\n* DNs found only in %s:" % self.con.host
            for x in self.dn_list:
                if not x.upper() in [q.upper() for q in other.dn_list]:
                    if title:
                        self.log( title )
                        title = None
                        res = False
                    self.log( 4*" " + x )
                    self.dn_list[self.dn_list.index(x)] = ""
            self.dn_list = [x for x in self.dn_list if x]
            #
            title= "\n* DNs found only in %s:" % other.con.host
            for x in other.dn_list:
                if not x.upper() in [q.upper() for q in self.dn_list]:
                    if title:
                        self.log( title )
                        title = None
                        res = False
                    self.log( 4*" " + x )
                    other.dn_list[other.dn_list.index(x)] = ""
            other.dn_list = [x for x in other.dn_list if x]
            #
            self.update_size()
            other.update_size()
            assert self.size == other.size
            assert sorted([x.upper() for x in self.dn_list]) == sorted([x.upper() for x in other.dn_list])
        self.log( "\n* Objects to be compared: %s" % self.size )

        index = 0
        while index < self.size:
            skip = False
            try:
                object1 = LDAPObject(connection=self.con,
                                     dn=self.dn_list[index],
                                     summary=self.summary)
            except LdbError, (enum, estr):
                if enum == ERR_NO_SUCH_OBJECT:
                    self.log( "\n!!! Object not found: %s" % self.dn_list[index] )
                    skip = True
                raise
            try:
                object2 = LDAPObject(connection=other.con,
                        dn=other.dn_list[index],
                        summary=other.summary)
            except LdbError, (enum, estr):
                if enum == ERR_NO_SUCH_OBJECT:
                    self.log( "\n!!! Object not found: %s" % other.dn_list[index] )
                    skip = True
                raise
            if skip:
                index += 1
                continue
            if object1 == object2:
                if self.con.verbose:
                    self.log( "\nComparing:" )
                    self.log( "'%s' [%s]" % (object1.dn, object1.con.host) )
                    self.log( "'%s' [%s]" % (object2.dn, object2.con.host) )
                    self.log( 4*" " + "OK" )
            else:
                self.log( "\nComparing:" )
                self.log( "'%s' [%s]" % (object1.dn, object1.con.host) )
                self.log( "'%s' [%s]" % (object2.dn, object2.con.host) )
                self.log( object1.screen_output )
                self.log( 4*" " + "FAILED" )
                res = False
            self.summary = object1.summary
            other.summary = object2.summary
            index += 1
        #
        return res

    def get_dn_list(self, context):
        """ Query LDAP server about the DNs of certain naming self.con.ext Domain (or Default), Configuration, Schema.
            Parse all DNs and filter those that are 'strange' or abnormal.
        """
        if context.upper() == "DOMAIN":
            search_base = "%s" % self.con.base_dn
        elif context.upper() == "CONFIGURATION":
            search_base = "CN=Configuration,%s" % self.con.base_dn
        elif context.upper() == "SCHEMA":
            search_base = "CN=Schema,CN=Configuration,%s" % self.con.base_dn

        dn_list = []
        if not self.search_base:
            self.search_base = search_base
        self.search_scope = self.search_scope.upper()
        if self.search_scope == "SUB":
            self.search_scope = SCOPE_SUBTREE
        elif self.search_scope == "BASE":
            self.search_scope = SCOPE_BASE
        elif self.search_scope == "ONE":
            self.search_scope = SCOPE_ONELEVEL
        else:
            raise StandardError("Wrong 'scope' given. Choose from: SUB, ONE, BASE")
        if not self.search_base.upper().endswith(search_base.upper()):
            raise StandardError("Invalid search base specified: %s" % self.search_base)
        res = self.con.ldb.search(base=self.search_base, scope=self.search_scope, attrs=["dn"])
        for x in res:
           dn_list.append(x["dn"].get_linearized())
        #
        global summary
        #
        return dn_list

    def print_summary(self):
        self.summary["unique_attrs"] = list(set(self.summary["unique_attrs"]))
        self.summary["df_value_attrs"] = list(set(self.summary["df_value_attrs"]))
        #
        if self.summary["unique_attrs"]:
            self.log( "\nAttributes found only in %s:" % self.con.host )
            self.log( "".join([str("\n" + 4*" " + x) for x in self.summary["unique_attrs"]]) )
        #
        if self.summary["df_value_attrs"]:
            self.log( "\nAttributes with different values:" )
            self.log( "".join([str("\n" + 4*" " + x) for x in self.summary["df_value_attrs"]]) )
            self.summary["df_value_attrs"] = []

class cmd_ldapcmp(Command):
    """compare two ldap databases"""
    synopsis = "ldapcmp URL1 URL2 <domain|configuration|schema> [options]"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "versionopts": options.VersionOptions,
        "credopts": options.CredentialsOptionsDouble,
    }

    takes_args = ["URL1", "URL2", "context1?", "context2?", "context3?"]

    takes_options = [
        Option("-w", "--two", dest="two", action="store_true", default=False,
            help="Hosts are in two different domains"),
        Option("-q", "--quiet", dest="quiet", action="store_true", default=False,
            help="Do not print anything but relay on just exit code"),
        Option("-v", "--verbose", dest="verbose", action="store_true", default=False,
            help="Print all DN pairs that have been compared"),
        Option("--sd", dest="descriptor", action="store_true", default=False,
            help="Compare nTSecurityDescriptor attibutes only"),
        Option("--sort-aces", dest="sort_aces", action="store_true", default=False,
            help="Sort ACEs before comparison of nTSecurityDescriptor attribute"),
        Option("--view", dest="view", default="section",
            help="Display mode for nTSecurityDescriptor results. Possible values: section or collision."),
        Option("--base", dest="base", default="",
            help="Pass search base that will build DN list for the first DC."),
        Option("--base2", dest="base2", default="",
            help="Pass search base that will build DN list for the second DC. Used when --two or when compare two different DNs."),
        Option("--scope", dest="scope", default="SUB",
            help="Pass search scope that builds DN list. Options: SUB, ONE, BASE"),
        ]

    def run(self, URL1, URL2,
            context1=None, context2=None, context3=None,
            two=False, quiet=False, verbose=False, descriptor=False, sort_aces=False, view="section",
            base="", base2="", scope="SUB", credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)
        creds2 = credopts.get_credentials2(lp, guess=False)
        if creds2.is_anonymous():
            creds2 = creds
        else:
            creds2.set_domain("")
            creds2.set_workstation("")
        if not creds.authentication_requested():
            raise CommandError("You must supply at least one username/password pair")

        # make a list of contexts to compare in
        contexts = []
        if context1 is None:
            if base and base2:
                # If search bases are specified context is defaulted to
                # DOMAIN so the given search bases can be verified.
                contexts = ["DOMAIN"]
            else:
                # if no argument given, we compare all contexts
                contexts = ["DOMAIN", "CONFIGURATION", "SCHEMA"]
        else:
            for c in [context1, context2, context3]:
                if c is None:
                    continue
                if not c.upper() in ["DOMAIN", "CONFIGURATION", "SCHEMA"]:
                    raise CommandError("Incorrect argument: %s" % c)
                contexts.append(c.upper())

        if verbose and quiet:
            raise CommandError("You cannot set --verbose and --quiet together")
        if (not base and base2) or (base and not base2):
            raise CommandError("You need to specify both --base and --base2 at the same time")
        if descriptor and view.upper() not in ["SECTION", "COLLISION"]:
            raise CommandError("Invalid --view value. Choose from: section or collision")
        if not scope.upper() in ["SUB", "ONE", "BASE"]:
            raise CommandError("Invalid --scope value. Choose from: SUB, ONE, BASE")

        con1 = LDAPBase(URL1, creds, lp,
                        two=two, quiet=quiet, descriptor=descriptor, sort_aces=sort_aces,
                        verbose=verbose,view=view, base=base, scope=scope)
        assert len(con1.base_dn) > 0

        con2 = LDAPBase(URL2, creds2, lp,
                        two=two, quiet=quiet, descriptor=descriptor, sort_aces=sort_aces,
                        verbose=verbose, view=view, base=base2, scope=scope)
        assert len(con2.base_dn) > 0

        status = 0
        for context in contexts:
            if not quiet:
                print "\n* Comparing [%s] context..." % context

            b1 = LDAPBundel(con1, context=context)
            b2 = LDAPBundel(con2, context=context)

            if b1 == b2:
                if not quiet:
                    print "\n* Result for [%s]: SUCCESS" % context
            else:
                if not quiet:
                    print "\n* Result for [%s]: FAILURE" % context
                    if not descriptor:
                        assert len(b1.summary["df_value_attrs"]) == len(b2.summary["df_value_attrs"])
                        b2.summary["df_value_attrs"] = []
                        print "\nSUMMARY"
                        print "---------"
                        b1.print_summary()
                        b2.print_summary()
                # mark exit status as FAILURE if a least one comparison failed
                status = -1
        if status != 0:
            raise CommandError("Compare failed: %d" % status)
