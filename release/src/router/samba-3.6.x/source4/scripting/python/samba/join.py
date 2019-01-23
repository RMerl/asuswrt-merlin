#!/usr/bin/env python
#
# python join code
# Copyright Andrew Tridgell 2010
# Copyright Andrew Bartlett 2010
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

"""Joining a domain."""

from samba.auth import system_session
from samba.samdb import SamDB
from samba import gensec, Ldb, drs_utils
import ldb, samba, sys, os, uuid
from samba.ndr import ndr_pack
from samba.dcerpc import security, drsuapi, misc, nbt
from samba.credentials import Credentials, DONT_USE_KERBEROS
from samba.provision import secretsdb_self_join, provision, FILL_DRS
from samba.schema import Schema
from samba.net import Net
import logging
import talloc

# this makes debugging easier
talloc.enable_null_tracking()


class dc_join(object):
    '''perform a DC join'''

    def __init__(ctx, server=None, creds=None, lp=None, site=None,
            netbios_name=None, targetdir=None, domain=None):
        ctx.creds = creds
        ctx.lp = lp
        ctx.site = site
        ctx.netbios_name = netbios_name
        ctx.targetdir = targetdir

        ctx.creds.set_gensec_features(creds.get_gensec_features() | gensec.FEATURE_SEAL)
        ctx.net = Net(creds=ctx.creds, lp=ctx.lp)

        if server is not None:
            ctx.server = server
        else:
            print("Finding a writeable DC for domain '%s'" % domain)
            ctx.server = ctx.find_dc(domain)
            print("Found DC %s" % ctx.server)

        ctx.samdb = SamDB(url="ldap://%s" % ctx.server,
                          session_info=system_session(),
                          credentials=ctx.creds, lp=ctx.lp)

        ctx.myname = netbios_name
        ctx.samname = "%s$" % ctx.myname
        ctx.base_dn = str(ctx.samdb.get_default_basedn())
        ctx.root_dn = str(ctx.samdb.get_root_basedn())
        ctx.schema_dn = str(ctx.samdb.get_schema_basedn())
        ctx.config_dn = str(ctx.samdb.get_config_basedn())
        ctx.domsid = ctx.samdb.get_domain_sid()
        ctx.domain_name = ctx.get_domain_name()

        lp.set("workgroup", ctx.domain_name)
        print("workgroup is %s" % ctx.domain_name)

        ctx.dc_ntds_dn = ctx.get_dsServiceName()
        ctx.dc_dnsHostName = ctx.get_dnsHostName()
        ctx.behavior_version = ctx.get_behavior_version()

        ctx.acct_pass = samba.generate_random_password(32, 40)

        # work out the DNs of all the objects we will be adding
        ctx.server_dn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (ctx.myname, ctx.site, ctx.config_dn)
        ctx.ntds_dn = "CN=NTDS Settings,%s" % ctx.server_dn
        topology_base = "CN=Topology,CN=Domain System Volume,CN=DFSR-GlobalSettings,CN=System,%s" % ctx.base_dn
        if ctx.dn_exists(topology_base):
            ctx.topology_dn = "CN=%s,%s" % (ctx.myname, topology_base)
        else:
            ctx.topology_dn = None

        ctx.dnsdomain = ldb.Dn(ctx.samdb, ctx.base_dn).canonical_str().split('/')[0]

        ctx.realm = ctx.dnsdomain
        lp.set("realm", ctx.realm)

        print("realm is %s" % ctx.realm)

        ctx.dnshostname = "%s.%s" % (ctx.myname.lower(), ctx.dnsdomain)

        ctx.acct_dn = "CN=%s,OU=Domain Controllers,%s" % (ctx.myname, ctx.base_dn)

        ctx.tmp_samdb = None

        ctx.SPNs = [ "HOST/%s" % ctx.myname,
                     "HOST/%s" % ctx.dnshostname,
                     "GC/%s/%s" % (ctx.dnshostname, ctx.dnsdomain) ]

        # these elements are optional
        ctx.never_reveal_sid = None
        ctx.reveal_sid = None
        ctx.connection_dn = None
        ctx.RODC = False
        ctx.krbtgt_dn = None
        ctx.drsuapi = None
        ctx.managedby = None


    def del_noerror(ctx, dn, recursive=False):
        if recursive:
            try:
                res = ctx.samdb.search(base=dn, scope=ldb.SCOPE_ONELEVEL, attrs=["dn"])
            except Exception:
                return
            for r in res:
                ctx.del_noerror(r.dn, recursive=True)
        try:
            ctx.samdb.delete(dn)
            print "Deleted %s" % dn
        except Exception:
            pass

    def cleanup_old_join(ctx):
        '''remove any DNs from a previous join'''
        try:
            # find the krbtgt link
            print("checking samaccountname")
            res = ctx.samdb.search(base=ctx.samdb.get_default_basedn(),
                                   expression='samAccountName=%s' % ctx.samname,
                                   attrs=["msDS-krbTgtLink"])
            if res:
                ctx.del_noerror(res[0].dn, recursive=True)
            if ctx.connection_dn is not None:
                ctx.del_noerror(ctx.connection_dn)
            if ctx.krbtgt_dn is not None:
                ctx.del_noerror(ctx.krbtgt_dn)
            ctx.del_noerror(ctx.ntds_dn)
            ctx.del_noerror(ctx.server_dn, recursive=True)
            if ctx.topology_dn:
                ctx.del_noerror(ctx.topology_dn)
            if res:
                ctx.new_krbtgt_dn = res[0]["msDS-Krbtgtlink"][0]
                ctx.del_noerror(ctx.new_krbtgt_dn)
        except Exception:
            pass

    def find_dc(ctx, domain):
        '''find a writeable DC for the given domain'''
        try:
            ctx.cldap_ret = ctx.net.finddc(domain, nbt.NBT_SERVER_LDAP | nbt.NBT_SERVER_DS | nbt.NBT_SERVER_WRITABLE)
        except Exception:
            raise Exception("Failed to find a writeable DC for domain '%s'" % domain)
        if ctx.cldap_ret.client_site is not None and ctx.cldap_ret.client_site != "":
            ctx.site = ctx.cldap_ret.client_site
        return ctx.cldap_ret.pdc_dns_name


    def get_dsServiceName(ctx):
        res = ctx.samdb.search(base="", scope=ldb.SCOPE_BASE, attrs=["dsServiceName"])
        return res[0]["dsServiceName"][0]

    def get_behavior_version(ctx):
        res = ctx.samdb.search(base=ctx.base_dn, scope=ldb.SCOPE_BASE, attrs=["msDS-Behavior-Version"])
        if "msDS-Behavior-Version" in res[0]:
            return int(res[0]["msDS-Behavior-Version"][0])
        else:
            return samba.dsdb.DS_DOMAIN_FUNCTION_2000

    def get_dnsHostName(ctx):
        res = ctx.samdb.search(base="", scope=ldb.SCOPE_BASE, attrs=["dnsHostName"])
        return res[0]["dnsHostName"][0]

    def get_domain_name(ctx):
        '''get netbios name of the domain from the partitions record'''
        partitions_dn = ctx.samdb.get_partitions_dn()
        res = ctx.samdb.search(base=partitions_dn, scope=ldb.SCOPE_ONELEVEL, attrs=["nETBIOSName"],
                               expression='ncName=%s' % ctx.samdb.get_default_basedn())
        return res[0]["nETBIOSName"][0]

    def get_mysid(ctx):
        '''get the SID of the connected user. Only works with w2k8 and later,
           so only used for RODC join'''
        res = ctx.samdb.search(base="", scope=ldb.SCOPE_BASE, attrs=["tokenGroups"])
        binsid = res[0]["tokenGroups"][0]
        return ctx.samdb.schema_format_value("objectSID", binsid)

    def dn_exists(ctx, dn):
        '''check if a DN exists'''
        try:
            res = ctx.samdb.search(base=dn, scope=ldb.SCOPE_BASE, attrs=[])
        except ldb.LdbError, (enum, estr):
            if enum == ldb.ERR_NO_SUCH_OBJECT:
                return False
            raise
        return True

    def add_krbtgt_account(ctx):
        '''RODCs need a special krbtgt account'''
        print "Adding %s" % ctx.krbtgt_dn
        rec = {
            "dn" : ctx.krbtgt_dn,
            "objectclass" : "user",
            "useraccountcontrol" : str(samba.dsdb.UF_NORMAL_ACCOUNT |
                                       samba.dsdb.UF_ACCOUNTDISABLE),
            "showinadvancedviewonly" : "TRUE",
            "description" : "krbtgt for %s" % ctx.samname}
        ctx.samdb.add(rec, ["rodc_join:1:1"])

        # now we need to search for the samAccountName attribute on the krbtgt DN,
        # as this will have been magically set to the krbtgt number
        res = ctx.samdb.search(base=ctx.krbtgt_dn, scope=ldb.SCOPE_BASE, attrs=["samAccountName"])
        ctx.krbtgt_name = res[0]["samAccountName"][0]

        print "Got krbtgt_name=%s" % ctx.krbtgt_name

        m = ldb.Message()
        m.dn = ldb.Dn(ctx.samdb, ctx.acct_dn)
        m["msDS-krbTgtLink"] = ldb.MessageElement(ctx.krbtgt_dn,
                                                  ldb.FLAG_MOD_REPLACE, "msDS-krbTgtLink")
        ctx.samdb.modify(m)

        ctx.new_krbtgt_dn = "CN=%s,CN=Users,%s" % (ctx.krbtgt_name, ctx.base_dn)
        print "Renaming %s to %s" % (ctx.krbtgt_dn, ctx.new_krbtgt_dn)
        ctx.samdb.rename(ctx.krbtgt_dn, ctx.new_krbtgt_dn)

    def drsuapi_connect(ctx):
        '''make a DRSUAPI connection to the server'''
        binding_options = "seal"
        if ctx.lp.get("log level") >= 5:
            binding_options += ",print"
        binding_string = "ncacn_ip_tcp:%s[%s]" % (ctx.server, binding_options)
        ctx.drsuapi = drsuapi.drsuapi(binding_string, ctx.lp, ctx.creds)
        (ctx.drsuapi_handle, ctx.bind_supported_extensions) = drs_utils.drs_DsBind(ctx.drsuapi)

    def create_tmp_samdb(ctx):
        '''create a temporary samdb object for schema queries'''
        ctx.tmp_schema = Schema(security.dom_sid(ctx.domsid),
                                schemadn=ctx.schema_dn)
        ctx.tmp_samdb = SamDB(session_info=system_session(), url=None, auto_connect=False,
                              credentials=ctx.creds, lp=ctx.lp, global_schema=False,
                              am_rodc=False)
        ctx.tmp_samdb.set_schema(ctx.tmp_schema)

    def build_DsReplicaAttribute(ctx, attrname, attrvalue):
        '''build a DsReplicaAttributeCtr object'''
        r = drsuapi.DsReplicaAttribute()
        r.attid = ctx.tmp_samdb.get_attid_from_lDAPDisplayName(attrname)
        r.value_ctr = 1


    def DsAddEntry(ctx, rec):
        '''add a record via the DRSUAPI DsAddEntry call'''
        if ctx.drsuapi is None:
            ctx.drsuapi_connect()
        if ctx.tmp_samdb is None:
            ctx.create_tmp_samdb()

        id = drsuapi.DsReplicaObjectIdentifier()
        id.dn = rec['dn']

        attrs = []
        for a in rec:
            if a == 'dn':
                continue
            if not isinstance(rec[a], list):
                v = [rec[a]]
            else:
                v = rec[a]
            rattr = ctx.tmp_samdb.dsdb_DsReplicaAttribute(ctx.tmp_samdb, a, v)
            attrs.append(rattr)

        attribute_ctr = drsuapi.DsReplicaAttributeCtr()
        attribute_ctr.num_attributes = len(attrs)
        attribute_ctr.attributes = attrs

        object = drsuapi.DsReplicaObject()
        object.identifier = id
        object.attribute_ctr = attribute_ctr

        first_object = drsuapi.DsReplicaObjectListItem()
        first_object.object = object

        req2 = drsuapi.DsAddEntryRequest2()
        req2.first_object = first_object

        (level, ctr) = ctx.drsuapi.DsAddEntry(ctx.drsuapi_handle, 2, req2)
        if ctr.err_ver != 1:
            raise RuntimeError("expected err_ver 1, got %u" % ctr.err_ver)
        if ctr.err_data.status != (0, 'WERR_OK'):
            print("DsAddEntry failed with status %s info %s" % (ctr.err_data.status,
                                                                ctr.err_data.info.extended_err))
            raise RuntimeError("DsAddEntry failed")

    def join_add_objects(ctx):
        '''add the various objects needed for the join'''
        print "Adding %s" % ctx.acct_dn
        rec = {
            "dn" : ctx.acct_dn,
            "objectClass": "computer",
            "displayname": ctx.samname,
            "samaccountname" : ctx.samname,
            "userAccountControl" : str(ctx.userAccountControl | samba.dsdb.UF_ACCOUNTDISABLE),
            "dnshostname" : ctx.dnshostname}
        if ctx.behavior_version >= samba.dsdb.DS_DOMAIN_FUNCTION_2008:
            rec['msDS-SupportedEncryptionTypes'] = str(samba.dsdb.ENC_ALL_TYPES)
        if ctx.managedby:
            rec["managedby"] = ctx.managedby
        if ctx.never_reveal_sid:
            rec["msDS-NeverRevealGroup"] = ctx.never_reveal_sid
        if ctx.reveal_sid:
            rec["msDS-RevealOnDemandGroup"] = ctx.reveal_sid
        ctx.samdb.add(rec)

        if ctx.krbtgt_dn:
            ctx.add_krbtgt_account()

        print "Adding %s" % ctx.server_dn
        rec = {
            "dn": ctx.server_dn,
            "objectclass" : "server",
            "systemFlags" : str(samba.dsdb.SYSTEM_FLAG_CONFIG_ALLOW_RENAME |
                                samba.dsdb.SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE |
                                samba.dsdb.SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE),
            "serverReference" : ctx.acct_dn,
            "dnsHostName" : ctx.dnshostname}
        ctx.samdb.add(rec)

        # FIXME: the partition (NC) assignment has to be made dynamic
        print "Adding %s" % ctx.ntds_dn
        rec = {
            "dn" : ctx.ntds_dn,
            "objectclass" : "nTDSDSA",
            "systemFlags" : str(samba.dsdb.SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE),
            "dMDLocation" : ctx.schema_dn}

        if ctx.behavior_version >= samba.dsdb.DS_DOMAIN_FUNCTION_2003:
            rec["msDS-Behavior-Version"] = str(ctx.behavior_version)

        if ctx.behavior_version >= samba.dsdb.DS_DOMAIN_FUNCTION_2003:
            rec["msDS-HasDomainNCs"] = ctx.base_dn

        if ctx.RODC:
            rec["objectCategory"] = "CN=NTDS-DSA-RO,%s" % ctx.schema_dn
            rec["msDS-HasFullReplicaNCs"] = [ ctx.base_dn, ctx.config_dn, ctx.schema_dn ]
            rec["options"] = "37"
            ctx.samdb.add(rec, ["rodc_join:1:1"])
        else:
            rec["objectCategory"] = "CN=NTDS-DSA,%s" % ctx.schema_dn
            rec["HasMasterNCs"]      = [ ctx.base_dn, ctx.config_dn, ctx.schema_dn ]
            if ctx.behavior_version >= samba.dsdb.DS_DOMAIN_FUNCTION_2003:
                rec["msDS-HasMasterNCs"] = [ ctx.base_dn, ctx.config_dn, ctx.schema_dn ]
            rec["options"] = "1"
            rec["invocationId"] = ndr_pack(misc.GUID(str(uuid.uuid4())))
            ctx.DsAddEntry(rec)

        # find the GUID of our NTDS DN
        res = ctx.samdb.search(base=ctx.ntds_dn, scope=ldb.SCOPE_BASE, attrs=["objectGUID"])
        ctx.ntds_guid = misc.GUID(ctx.samdb.schema_format_value("objectGUID", res[0]["objectGUID"][0]))

        if ctx.connection_dn is not None:
            print "Adding %s" % ctx.connection_dn
            rec = {
                "dn" : ctx.connection_dn,
                "objectclass" : "nTDSConnection",
                "enabledconnection" : "TRUE",
                "options" : "65",
                "fromServer" : ctx.dc_ntds_dn}
            ctx.samdb.add(rec)

        if ctx.topology_dn:
            print "Adding %s" % ctx.topology_dn
            rec = {
                "dn" : ctx.topology_dn,
                "objectclass" : "msDFSR-Member",
                "msDFSR-ComputerReference" : ctx.acct_dn,
                "serverReference" : ctx.ntds_dn}
            ctx.samdb.add(rec)

        print "Adding SPNs to %s" % ctx.acct_dn
        m = ldb.Message()
        m.dn = ldb.Dn(ctx.samdb, ctx.acct_dn)
        for i in range(len(ctx.SPNs)):
            ctx.SPNs[i] = ctx.SPNs[i].replace("$NTDSGUID", str(ctx.ntds_guid))
        m["servicePrincipalName"] = ldb.MessageElement(ctx.SPNs,
                                                       ldb.FLAG_MOD_ADD,
                                                       "servicePrincipalName")
        ctx.samdb.modify(m)

        print "Setting account password for %s" % ctx.samname
        ctx.samdb.setpassword("(&(objectClass=user)(sAMAccountName=%s))" % ctx.samname,
                              ctx.acct_pass,
                              force_change_at_next_login=False,
                              username=ctx.samname)
        res = ctx.samdb.search(base=ctx.acct_dn, scope=ldb.SCOPE_BASE, attrs=["msDS-keyVersionNumber"])
        ctx.key_version_number = int(res[0]["msDS-keyVersionNumber"][0])

        print("Enabling account")
        m = ldb.Message()
        m.dn = ldb.Dn(ctx.samdb, ctx.acct_dn)
        m["userAccountControl"] = ldb.MessageElement(str(ctx.userAccountControl),
                                                     ldb.FLAG_MOD_REPLACE,
                                                     "userAccountControl")
        ctx.samdb.modify(m)

    def join_provision(ctx):
        '''provision the local SAM'''

        print "Calling bare provision"

        logger = logging.getLogger("provision")
        logger.addHandler(logging.StreamHandler(sys.stdout))
        smbconf = ctx.lp.configfile

        presult = provision(logger, system_session(), None,
                            smbconf=smbconf, targetdir=ctx.targetdir, samdb_fill=FILL_DRS,
                            realm=ctx.realm, rootdn=ctx.root_dn, domaindn=ctx.base_dn,
                            schemadn=ctx.schema_dn,
                            configdn=ctx.config_dn,
                            serverdn=ctx.server_dn, domain=ctx.domain_name,
                            hostname=ctx.myname, domainsid=ctx.domsid,
                            machinepass=ctx.acct_pass, serverrole="domain controller",
                            sitename=ctx.site, lp=ctx.lp)
        print "Provision OK for domain DN %s" % presult.domaindn
        ctx.local_samdb = presult.samdb
        ctx.lp          = presult.lp
        ctx.paths       = presult.paths


    def join_replicate(ctx):
        '''replicate the SAM'''

        print "Starting replication"
        ctx.local_samdb.transaction_start()
        try:
            source_dsa_invocation_id = misc.GUID(ctx.samdb.get_invocation_id())
            destination_dsa_guid = ctx.ntds_guid

            if ctx.RODC:
                repl_creds = Credentials()
                repl_creds.guess(ctx.lp)
                repl_creds.set_kerberos_state(DONT_USE_KERBEROS)
                repl_creds.set_username(ctx.samname)
                repl_creds.set_password(ctx.acct_pass)
            else:
                repl_creds = ctx.creds

            binding_options = "seal"
            if ctx.lp.get("debug level") >= 5:
                binding_options += ",print"
            repl = drs_utils.drs_Replicate(
                "ncacn_ip_tcp:%s[%s]" % (ctx.server, binding_options),
                ctx.lp, repl_creds, ctx.local_samdb)

            repl.replicate(ctx.schema_dn, source_dsa_invocation_id,
                    destination_dsa_guid, schema=True, rodc=ctx.RODC,
                    replica_flags=ctx.replica_flags)
            repl.replicate(ctx.config_dn, source_dsa_invocation_id,
                    destination_dsa_guid, rodc=ctx.RODC,
                    replica_flags=ctx.replica_flags)
            repl.replicate(ctx.base_dn, source_dsa_invocation_id,
                    destination_dsa_guid, rodc=ctx.RODC,
                    replica_flags=ctx.replica_flags)
            if ctx.RODC:
                repl.replicate(ctx.acct_dn, source_dsa_invocation_id,
                        destination_dsa_guid,
                        exop=drsuapi.DRSUAPI_EXOP_REPL_SECRET, rodc=True)
                repl.replicate(ctx.new_krbtgt_dn, source_dsa_invocation_id,
                        destination_dsa_guid,
                        exop=drsuapi.DRSUAPI_EXOP_REPL_SECRET, rodc=True)

            print "Committing SAM database"
        except:
            ctx.local_samdb.transaction_cancel()
            raise
        else:
            ctx.local_samdb.transaction_commit()


    def join_finalise(ctx):
        '''finalise the join, mark us synchronised and setup secrets db'''

        print "Setting isSynchronized"
        m = ldb.Message()
        m.dn = ldb.Dn(ctx.samdb, '@ROOTDSE')
        m["isSynchronized"] = ldb.MessageElement("TRUE", ldb.FLAG_MOD_REPLACE, "isSynchronized")
        ctx.samdb.modify(m)

        secrets_ldb = Ldb(ctx.paths.secrets, session_info=system_session(), lp=ctx.lp)

        print "Setting up secrets database"
        secretsdb_self_join(secrets_ldb, domain=ctx.domain_name,
                            realm=ctx.realm,
                            dnsdomain=ctx.dnsdomain,
                            netbiosname=ctx.myname,
                            domainsid=security.dom_sid(ctx.domsid),
                            machinepass=ctx.acct_pass,
                            secure_channel_type=ctx.secure_channel_type,
                            key_version_number=ctx.key_version_number)

    def do_join(ctx):
        ctx.cleanup_old_join()
        try:
            ctx.join_add_objects()
            ctx.join_provision()
            ctx.join_replicate()
            ctx.join_finalise()
        except Exception:
            print "Join failed - cleaning up"
            ctx.cleanup_old_join()
            raise


def join_RODC(server=None, creds=None, lp=None, site=None, netbios_name=None,
              targetdir=None, domain=None):
    """join as a RODC"""

    ctx = dc_join(server, creds, lp, site, netbios_name, targetdir, domain)

    ctx.krbtgt_dn = "CN=krbtgt_%s,CN=Users,%s" % (ctx.myname, ctx.base_dn)

    # setup some defaults for accounts that should be replicated to this RODC
    ctx.never_reveal_sid = [ "<SID=%s-%s>" % (ctx.domsid, security.DOMAIN_RID_RODC_DENY),
                             "<SID=%s>" % security.SID_BUILTIN_ADMINISTRATORS,
                             "<SID=%s>" % security.SID_BUILTIN_SERVER_OPERATORS,
                             "<SID=%s>" % security.SID_BUILTIN_BACKUP_OPERATORS,
                             "<SID=%s>" % security.SID_BUILTIN_ACCOUNT_OPERATORS ]
    ctx.reveal_sid = "<SID=%s-%s>" % (ctx.domsid, security.DOMAIN_RID_RODC_ALLOW)

    mysid = ctx.get_mysid()
    admin_dn = "<SID=%s>" % mysid
    ctx.managedby = admin_dn

    ctx.userAccountControl = (samba.dsdb.UF_WORKSTATION_TRUST_ACCOUNT |
                              samba.dsdb.UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION |
                              samba.dsdb.UF_PARTIAL_SECRETS_ACCOUNT)

    ctx.SPNs.extend([ "RestrictedKrbHost/%s" % ctx.myname,
                      "RestrictedKrbHost/%s" % ctx.dnshostname ])

    ctx.connection_dn = "CN=RODC Connection (FRS),%s" % ctx.ntds_dn
    ctx.secure_channel_type = misc.SEC_CHAN_RODC
    ctx.RODC = True
    ctx.replica_flags  =  (drsuapi.DRSUAPI_DRS_INIT_SYNC |
                           drsuapi.DRSUAPI_DRS_PER_SYNC |
                           drsuapi.DRSUAPI_DRS_GET_ANC |
                           drsuapi.DRSUAPI_DRS_NEVER_SYNCED |
                           drsuapi.DRSUAPI_DRS_SPECIAL_SECRET_PROCESSING |
                           drsuapi.DRSUAPI_DRS_GET_ALL_GROUP_MEMBERSHIP)
    ctx.do_join()


    print "Joined domain %s (SID %s) as an RODC" % (ctx.domain_name, ctx.domsid)


def join_DC(server=None, creds=None, lp=None, site=None, netbios_name=None,
            targetdir=None, domain=None):
    """join as a DC"""
    ctx = dc_join(server, creds, lp, site, netbios_name, targetdir, domain)

    ctx.userAccountControl = samba.dsdb.UF_SERVER_TRUST_ACCOUNT | samba.dsdb.UF_TRUSTED_FOR_DELEGATION

    ctx.SPNs.append('E3514235-4B06-11D1-AB04-00C04FC2DCD2/$NTDSGUID/%s' % ctx.dnsdomain)
    ctx.secure_channel_type = misc.SEC_CHAN_BDC

    ctx.replica_flags = (drsuapi.DRSUAPI_DRS_WRIT_REP |
                         drsuapi.DRSUAPI_DRS_INIT_SYNC |
                         drsuapi.DRSUAPI_DRS_PER_SYNC |
                         drsuapi.DRSUAPI_DRS_FULL_SYNC_IN_PROGRESS |
                         drsuapi.DRSUAPI_DRS_NEVER_SYNCED)

    ctx.do_join()
    print "Joined domain %s (SID %s) as a DC" % (ctx.domain_name, ctx.domsid)
