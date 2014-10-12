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

"""Functions for setting up a Samba configuration (LDB and LDAP backends)."""

from base64 import b64encode
import errno
import ldb
import os
import sys
import uuid
import time
import shutil
import subprocess
import urllib

from ldb import SCOPE_BASE, SCOPE_ONELEVEL, LdbError, timestring

from samba import Ldb, read_and_sub_file, setup_file
from samba.credentials import Credentials, DONT_USE_KERBEROS
from samba.schema import Schema

class SlapdAlreadyRunning(Exception):

    def __init__(self, uri):
        self.ldapi_uri = uri
        super(SlapdAlreadyRunning, self).__init__("Another slapd Instance "
            "seems already running on this host, listening to %s." %
            self.ldapi_uri)


class ProvisionBackend(object):
    def __init__(self, backend_type, paths=None, lp=None,
            credentials=None, names=None, logger=None):
        """Provision a backend for samba4"""
        self.paths = paths
        self.lp = lp
        self.credentials = credentials
        self.names = names
        self.logger = logger

        self.type = backend_type

        # Set a default - the code for "existing" below replaces this
        self.ldap_backend_type = backend_type

    def init(self):
        """Initialize the backend."""
        raise NotImplementedError(self.init)

    def start(self):
        """Start the backend."""
        raise NotImplementedError(self.start)

    def shutdown(self):
        """Shutdown the backend."""
        raise NotImplementedError(self.shutdown)

    def post_setup(self):
        """Post setup."""
        raise NotImplementedError(self.post_setup)


class LDBBackend(ProvisionBackend):

    def init(self):
        self.credentials = None
        self.secrets_credentials = None

        # Wipe the old sam.ldb databases away
        shutil.rmtree(self.paths.samdb + ".d", True)

    def start(self):
        pass

    def shutdown(self):
        pass

    def post_setup(self):
        pass


class ExistingBackend(ProvisionBackend):

    def __init__(self, backend_type, paths=None, lp=None,
            credentials=None, names=None, logger=None, ldapi_uri=None):

        super(ExistingBackend, self).__init__(backend_type=backend_type,
                paths=paths, lp=lp,
                credentials=credentials, names=names, logger=logger,
                ldap_backend_forced_uri=ldapi_uri)

    def init(self):
        # Check to see that this 'existing' LDAP backend in fact exists
        ldapi_db = Ldb(self.ldapi_uri, credentials=self.credentials)
        ldapi_db.search(base="", scope=SCOPE_BASE,
            expression="(objectClass=OpenLDAProotDSE)")

        # If we have got here, then we must have a valid connection to the LDAP
        # server, with valid credentials supplied This caused them to be set
        # into the long-term database later in the script.
        self.secrets_credentials = self.credentials

         # For now, assume existing backends at least emulate OpenLDAP
        self.ldap_backend_type = "openldap"


class LDAPBackend(ProvisionBackend):

    def __init__(self, backend_type, paths=None, lp=None,
                 credentials=None, names=None, logger=None, domainsid=None,
                 schema=None, hostname=None, ldapadminpass=None,
                 slapd_path=None, ldap_backend_extra_port=None,
                 ldap_backend_forced_uri=None, ldap_dryrun_mode=False):

        super(LDAPBackend, self).__init__(backend_type=backend_type,
                paths=paths, lp=lp,
                credentials=credentials, names=names, logger=logger)

        self.domainsid = domainsid
        self.schema = schema
        self.hostname = hostname

        self.ldapdir = os.path.join(paths.private_dir, "ldap")
        self.ldapadminpass = ldapadminpass

        self.slapd_path = slapd_path
        self.slapd_command = None
        self.slapd_command_escaped = None
        self.slapd_pid = os.path.join(self.ldapdir, "slapd.pid")

        self.ldap_backend_extra_port = ldap_backend_extra_port
        self.ldap_dryrun_mode = ldap_dryrun_mode

        if ldap_backend_forced_uri is not None:
            self.ldap_uri = ldap_backend_forced_uri
        else:
            self.ldap_uri = "ldapi://%s" % urllib.quote(
                os.path.join(self.ldapdir, "ldapi"), safe="")

        if not os.path.exists(self.ldapdir):
            os.mkdir(self.ldapdir)

    def init(self):
        from samba.provision import ProvisioningError
        # we will shortly start slapd with ldapi for final provisioning. first
        # check with ldapsearch -> rootDSE via self.ldap_uri if another
        # instance of slapd is already running
        try:
            ldapi_db = Ldb(self.ldap_uri)
            ldapi_db.search(base="", scope=SCOPE_BASE,
                expression="(objectClass=OpenLDAProotDSE)")
            try:
                f = open(self.slapd_pid, "r")
            except IOError, err:
                if err != errno.ENOENT:
                    raise
            else:
                p = f.read()
                f.close()
                self.logger.info("Check for slapd Process with PID: %s and terminate it manually." % p)
            raise SlapdAlreadyRunning(self.ldap_uri)
        except LdbError:
            # XXX: We should never be catching all Ldb errors
            pass

        # Try to print helpful messages when the user has not specified the
        # path to slapd
        if self.slapd_path is None:
            raise ProvisioningError("Warning: LDAP-Backend must be setup with path to slapd, e.g. --slapd-path=\"/usr/local/libexec/slapd\"!")
        if not os.path.exists(self.slapd_path):
            self.logger.warning("Path (%s) to slapd does not exist!",
                self.slapd_path)

        if not os.path.isdir(self.ldapdir):
            os.makedirs(self.ldapdir, 0700)

        # Put the LDIF of the schema into a database so we can search on
        # it to generate schema-dependent configurations in Fedora DS and
        # OpenLDAP
        schemadb_path = os.path.join(self.ldapdir, "schema-tmp.ldb")
        try:
            os.unlink(schemadb_path)
        except OSError:
            pass

        self.schema.write_to_tmp_ldb(schemadb_path)

        self.credentials = Credentials()
        self.credentials.guess(self.lp)
        # Kerberos to an ldapi:// backend makes no sense
        self.credentials.set_kerberos_state(DONT_USE_KERBEROS)
        self.credentials.set_password(self.ldapadminpass)

        self.secrets_credentials = Credentials()
        self.secrets_credentials.guess(self.lp)
        # Kerberos to an ldapi:// backend makes no sense
        self.secrets_credentials.set_kerberos_state(DONT_USE_KERBEROS)
        self.secrets_credentials.set_username("samba-admin")
        self.secrets_credentials.set_password(self.ldapadminpass)

        self.provision()

    def provision(self):
        pass

    def start(self):
        from samba.provision import ProvisioningError
        self.slapd_command_escaped = "\'" + "\' \'".join(self.slapd_command) + "\'"
        f = open(os.path.join(self.ldapdir, "ldap_backend_startup.sh"), 'w')
        try:
            f.write("#!/bin/sh\n" + self.slapd_command_escaped + "\n")
        finally:
            f.close()

        # Now start the slapd, so we can provision onto it.  We keep the
        # subprocess context around, to kill this off at the successful
        # end of the script
        self.slapd = subprocess.Popen(self.slapd_provision_command,
            close_fds=True, shell=False)

        count = 0
        while self.slapd.poll() is None:
            # Wait until the socket appears
            try:
                ldapi_db = Ldb(self.ldap_uri, lp=self.lp, credentials=self.credentials)
                ldapi_db.search(base="", scope=SCOPE_BASE,
                    expression="(objectClass=OpenLDAProotDSE)")
                # If we have got here, then we must have a valid connection to
                # the LDAP server!
                return
            except LdbError:
                time.sleep(1)
                count = count + 1

                if count > 15:
                    self.logger.error("Could not connect to slapd started with: %s" %  "\'" + "\' \'".join(self.slapd_provision_command) + "\'")
                    raise ProvisioningError("slapd never accepted a connection within 15 seconds of starting")

        self.logger.error("Could not start slapd with: %s" % "\'" + "\' \'".join(self.slapd_provision_command) + "\'")
        raise ProvisioningError("slapd died before we could make a connection to it")

    def shutdown(self):
        # if an LDAP backend is in use, terminate slapd after final provision
        # and check its proper termination
        if self.slapd.poll() is None:
            # Kill the slapd
            if getattr(self.slapd, "terminate", None) is not None:
                self.slapd.terminate()
            else:
                # Older python versions don't have .terminate()
                import signal
                os.kill(self.slapd.pid, signal.SIGTERM)

            # and now wait for it to die
            self.slapd.communicate()

    def post_setup(self):
        pass


class OpenLDAPBackend(LDAPBackend):

    def __init__(self, backend_type, paths=None, lp=None,
            credentials=None, names=None, logger=None, domainsid=None,
            schema=None, hostname=None, ldapadminpass=None, slapd_path=None,
            ldap_backend_extra_port=None, ldap_dryrun_mode=False,
            ol_mmr_urls=None, nosync=False, ldap_backend_forced_uri=None):
        from samba.provision import setup_path
        super(OpenLDAPBackend, self).__init__( backend_type=backend_type,
                paths=paths, lp=lp,
                credentials=credentials, names=names, logger=logger,
                domainsid=domainsid, schema=schema, hostname=hostname,
                ldapadminpass=ldapadminpass, slapd_path=slapd_path,
                ldap_backend_extra_port=ldap_backend_extra_port,
                ldap_backend_forced_uri=ldap_backend_forced_uri,
                ldap_dryrun_mode=ldap_dryrun_mode)

        self.ol_mmr_urls = ol_mmr_urls
        self.nosync = nosync

        self.slapdconf          = os.path.join(self.ldapdir, "slapd.conf")
        self.modulesconf        = os.path.join(self.ldapdir, "modules.conf")
        self.memberofconf       = os.path.join(self.ldapdir, "memberof.conf")
        self.olmmrserveridsconf = os.path.join(self.ldapdir, "mmr_serverids.conf")
        self.olmmrsyncreplconf  = os.path.join(self.ldapdir, "mmr_syncrepl.conf")
        self.olcdir             = os.path.join(self.ldapdir, "slapd.d")
        self.olcseedldif        = os.path.join(self.ldapdir, "olc_seed.ldif")

        self.schema = Schema(self.domainsid,
                             schemadn=self.names.schemadn, files=[
                setup_path("schema_samba4.ldif")])

    def setup_db_config(self, dbdir):
        """Setup a Berkeley database.

        :param dbdir: Database directory.
        """
        from samba.provision import setup_path
        if not os.path.isdir(os.path.join(dbdir, "bdb-logs")):
            os.makedirs(os.path.join(dbdir, "bdb-logs"), 0700)
            if not os.path.isdir(os.path.join(dbdir, "tmp")):
                os.makedirs(os.path.join(dbdir, "tmp"), 0700)

        setup_file(setup_path("DB_CONFIG"),
            os.path.join(dbdir, "DB_CONFIG"), {"LDAPDBDIR": dbdir})

    def provision(self):
        from samba.provision import ProvisioningError, setup_path
        # Wipe the directories so we can start
        shutil.rmtree(os.path.join(self.ldapdir, "db"), True)

        # Allow the test scripts to turn off fsync() for OpenLDAP as for TDB
        # and LDB
        nosync_config = ""
        if self.nosync:
            nosync_config = "dbnosync"

        lnkattr = self.schema.linked_attributes()
        refint_attributes = ""
        memberof_config = "# Generated from Samba4 schema\n"
        for att in  lnkattr.keys():
            if lnkattr[att] is not None:
                refint_attributes = refint_attributes + " " + att

                memberof_config += read_and_sub_file(
                    setup_path("memberof.conf"), {
                        "MEMBER_ATTR": att,
                        "MEMBEROF_ATTR" : lnkattr[att] })

        refint_config = read_and_sub_file(setup_path("refint.conf"),
                                      { "LINK_ATTRS" : refint_attributes})

        attrs = ["linkID", "lDAPDisplayName"]
        res = self.schema.ldb.search(expression="(&(objectclass=attributeSchema)(searchFlags:1.2.840.113556.1.4.803:=1))", base=self.names.schemadn, scope=SCOPE_ONELEVEL, attrs=attrs)
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

        if self.ol_mmr_urls is not None:
            # For now, make these equal
            mmr_pass = self.ldapadminpass

            url_list = filter(None,self.ol_mmr_urls.split(','))
            for url in url_list:
                self.logger.info("Using LDAP-URL: "+url)
            if len(url_list) == 1:
                raise ProvisioningError("At least 2 LDAP-URLs needed for MMR!")

            mmr_on_config = "MirrorMode On"
            mmr_replicator_acl = "  by dn=cn=replicator,cn=samba read"
            serverid = 0
            for url in url_list:
                serverid = serverid + 1
                mmr_serverids_config += read_and_sub_file(
                    setup_path("mmr_serverids.conf"), {
                        "SERVERID": str(serverid),
                        "LDAPSERVER": url })
                rid = serverid * 10
                rid = rid + 1
                mmr_syncrepl_schema_config += read_and_sub_file(
                        setup_path("mmr_syncrepl.conf"), {
                            "RID" : str(rid),
                           "MMRDN": self.names.schemadn,
                           "LDAPSERVER" : url,
                           "MMR_PASSWORD": mmr_pass})

                rid = rid + 1
                mmr_syncrepl_config_config += read_and_sub_file(
                    setup_path("mmr_syncrepl.conf"), {
                        "RID" : str(rid),
                        "MMRDN": self.names.configdn,
                        "LDAPSERVER" : url,
                        "MMR_PASSWORD": mmr_pass})

                rid = rid + 1
                mmr_syncrepl_user_config += read_and_sub_file(
                    setup_path("mmr_syncrepl.conf"), {
                        "RID" : str(rid),
                        "MMRDN": self.names.domaindn,
                        "LDAPSERVER" : url,
                        "MMR_PASSWORD": mmr_pass })
        # OpenLDAP cn=config initialisation
        olc_syncrepl_config = ""
        olc_mmr_config = ""
        # if mmr = yes, generate cn=config-replication directives
        # and olc_seed.lif for the other mmr-servers
        if self.ol_mmr_urls is not None:
            serverid = 0
            olc_serverids_config = ""
            olc_syncrepl_seed_config = ""
            olc_mmr_config += read_and_sub_file(
                setup_path("olc_mmr.conf"), {})
            rid = 500
            for url in url_list:
                serverid = serverid + 1
                olc_serverids_config += read_and_sub_file(
                    setup_path("olc_serverid.conf"), {
                        "SERVERID" : str(serverid), "LDAPSERVER" : url })

                rid = rid + 1
                olc_syncrepl_config += read_and_sub_file(
                    setup_path("olc_syncrepl.conf"), {
                        "RID" : str(rid), "LDAPSERVER" : url,
                        "MMR_PASSWORD": mmr_pass})

                olc_syncrepl_seed_config += read_and_sub_file(
                    setup_path("olc_syncrepl_seed.conf"), {
                        "RID" : str(rid), "LDAPSERVER" : url})

            setup_file(setup_path("olc_seed.ldif"), self.olcseedldif,
                       {"OLC_SERVER_ID_CONF": olc_serverids_config,
                        "OLC_PW": self.ldapadminpass,
                        "OLC_SYNCREPL_CONF": olc_syncrepl_seed_config})
        # end olc

        setup_file(setup_path("slapd.conf"), self.slapdconf,
                   {"DNSDOMAIN": self.names.dnsdomain,
                    "LDAPDIR": self.ldapdir,
                    "DOMAINDN": self.names.domaindn,
                    "CONFIGDN": self.names.configdn,
                    "SCHEMADN": self.names.schemadn,
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

        self.setup_db_config(os.path.join(self.ldapdir, "db", "user"))
        self.setup_db_config(os.path.join(self.ldapdir, "db", "config"))
        self.setup_db_config(os.path.join(self.ldapdir, "db", "schema"))

        if not os.path.exists(os.path.join(self.ldapdir, "db", "samba", "cn=samba")):
            os.makedirs(os.path.join(self.ldapdir, "db", "samba", "cn=samba"), 0700)

        setup_file(setup_path("cn=samba.ldif"),
                   os.path.join(self.ldapdir, "db", "samba", "cn=samba.ldif"),
                   { "UUID": str(uuid.uuid4()),
                     "LDAPTIME": timestring(int(time.time()))} )
        setup_file(setup_path("cn=samba-admin.ldif"),
                   os.path.join(self.ldapdir, "db", "samba", "cn=samba", "cn=samba-admin.ldif"),
                   {"LDAPADMINPASS_B64": b64encode(self.ldapadminpass),
                    "UUID": str(uuid.uuid4()),
                    "LDAPTIME": timestring(int(time.time()))} )

        if self.ol_mmr_urls is not None:
            setup_file(setup_path("cn=replicator.ldif"),
                       os.path.join(self.ldapdir, "db", "samba", "cn=samba", "cn=replicator.ldif"),
                       {"MMR_PASSWORD_B64": b64encode(mmr_pass),
                        "UUID": str(uuid.uuid4()),
                        "LDAPTIME": timestring(int(time.time()))} )

        mapping = "schema-map-openldap-2.3"
        backend_schema = "backend-schema.schema"

        f = open(setup_path(mapping), 'r')
        backend_schema_data = self.schema.convert_to_openldap(
                "openldap", f.read())
        assert backend_schema_data is not None
        f = open(os.path.join(self.ldapdir, backend_schema), 'w')
        try:
            f.write(backend_schema_data)
        finally:
            f.close()

        # now we generate the needed strings to start slapd automatically,
        if self.ldap_backend_extra_port is not None:
            # When we use MMR, we can't use 0.0.0.0 as it uses the name
            # specified there as part of it's clue as to it's own name,
            # and not to replicate to itself
            if self.ol_mmr_urls is None:
                server_port_string = "ldap://0.0.0.0:%d" % self.ldap_backend_extra_port
            else:
                server_port_string = "ldap://%s.%s:%d" (self.names.hostname,
                    self.names.dnsdomain, self.ldap_backend_extra_port)
        else:
            server_port_string = ""

        # Prepare the 'result' information - the commands to return in
        # particular
        self.slapd_provision_command = [self.slapd_path, "-F" + self.olcdir,
            "-h"]

        # copy this command so we have two version, one with -d0 and only
        # ldapi (or the forced ldap_uri), and one with all the listen commands
        self.slapd_command = list(self.slapd_provision_command)

        self.slapd_provision_command.extend([self.ldap_uri, "-d0"])

        uris = self.ldap_uri
        if server_port_string is not "":
            uris = uris + " " + server_port_string

        self.slapd_command.append(uris)

        # Set the username - done here because Fedora DS still uses the admin
        # DN and simple bind
        self.credentials.set_username("samba-admin")

        # Wipe the old sam.ldb databases away
        shutil.rmtree(self.olcdir, True)
        os.makedirs(self.olcdir, 0770)

        # If we were just looking for crashes up to this point, it's a
        # good time to exit before we realise we don't have OpenLDAP on
        # this system
        if self.ldap_dryrun_mode:
            sys.exit(0)

        slapd_cmd = [self.slapd_path, "-Ttest", "-n", "0", "-f",
                         self.slapdconf, "-F", self.olcdir]
        retcode = subprocess.call(slapd_cmd, close_fds=True, shell=False)

        if retcode != 0:
            self.logger.error("conversion from slapd.conf to cn=config failed slapd started with: %s" %  "\'" + "\' \'".join(slapd_cmd) + "\'")
            raise ProvisioningError("conversion from slapd.conf to cn=config failed")

        if not os.path.exists(os.path.join(self.olcdir, "cn=config.ldif")):
            raise ProvisioningError("conversion from slapd.conf to cn=config failed")

        # Don't confuse the admin by leaving the slapd.conf around
        os.remove(self.slapdconf)


class FDSBackend(LDAPBackend):

    def __init__(self, backend_type, paths=None, lp=None,
            credentials=None, names=None, logger=None, domainsid=None,
            schema=None, hostname=None, ldapadminpass=None, slapd_path=None,
            ldap_backend_extra_port=None, ldap_dryrun_mode=False, root=None,
            setup_ds_path=None):

        from samba.provision import setup_path

        super(FDSBackend, self).__init__(backend_type=backend_type,
                paths=paths, lp=lp,
                credentials=credentials, names=names, logger=logger,
                domainsid=domainsid, schema=schema, hostname=hostname,
                ldapadminpass=ldapadminpass, slapd_path=slapd_path,
                ldap_backend_extra_port=ldap_backend_extra_port,
                ldap_backend_forced_uri=ldap_backend_forced_uri,
                ldap_dryrun_mode=ldap_dryrun_mode)

        self.root = root
        self.setup_ds_path = setup_ds_path
        self.ldap_instance = self.names.netbiosname.lower()

        self.sambadn = "CN=Samba"

        self.fedoradsinf = os.path.join(self.ldapdir, "fedorads.inf")
        self.partitions_ldif = os.path.join(self.ldapdir,
            "fedorads-partitions.ldif")
        self.sasl_ldif = os.path.join(self.ldapdir, "fedorads-sasl.ldif")
        self.dna_ldif = os.path.join(self.ldapdir, "fedorads-dna.ldif")
        self.pam_ldif = os.path.join(self.ldapdir, "fedorads-pam.ldif")
        self.refint_ldif = os.path.join(self.ldapdir, "fedorads-refint.ldif")
        self.linked_attrs_ldif = os.path.join(self.ldapdir,
            "fedorads-linked-attributes.ldif")
        self.index_ldif = os.path.join(self.ldapdir, "fedorads-index.ldif")
        self.samba_ldif = os.path.join(self.ldapdir, "fedorads-samba.ldif")

        self.samba3_schema = setup_path(
            "../../examples/LDAP/samba.schema")
        self.samba3_ldif = os.path.join(self.ldapdir, "samba3.ldif")

        self.retcode = subprocess.call(["bin/oLschema2ldif",
                "-I", self.samba3_schema,
                "-O", self.samba3_ldif,
                "-b", self.names.domaindn],
                close_fds=True, shell=False)

        if self.retcode != 0:
            raise Exception("Unable to convert Samba 3 schema.")

        self.schema = Schema(
                self.domainsid,
                schemadn=self.names.schemadn,
                files=[setup_path("schema_samba4.ldif"), self.samba3_ldif],
                additional_prefixmap=["1000:1.3.6.1.4.1.7165.2.1",
                                      "1001:1.3.6.1.4.1.7165.2.2"])

    def provision(self):
        from samba.provision import ProvisioningError, setup_path
        if self.ldap_backend_extra_port is not None:
            serverport = "ServerPort=%d" % self.ldap_backend_extra_port
        else:
            serverport = ""

        setup_file(setup_path("fedorads.inf"), self.fedoradsinf,
                   {"ROOT": self.root,
                    "HOSTNAME": self.hostname,
                    "DNSDOMAIN": self.names.dnsdomain,
                    "LDAPDIR": self.ldapdir,
                    "DOMAINDN": self.names.domaindn,
                    "LDAP_INSTANCE": self.ldap_instance,
                    "LDAPMANAGERDN": self.names.ldapmanagerdn,
                    "LDAPMANAGERPASS": self.ldapadminpass,
                    "SERVERPORT": serverport})

        setup_file(setup_path("fedorads-partitions.ldif"),
            self.partitions_ldif,
                   {"CONFIGDN": self.names.configdn,
                    "SCHEMADN": self.names.schemadn,
                    "SAMBADN": self.sambadn,
                    })

        setup_file(setup_path("fedorads-sasl.ldif"), self.sasl_ldif,
                   {"SAMBADN": self.sambadn,
                    })

        setup_file(setup_path("fedorads-dna.ldif"), self.dna_ldif,
                   {"DOMAINDN": self.names.domaindn,
                    "SAMBADN": self.sambadn,
                    "DOMAINSID": str(self.domainsid),
                    })

        setup_file(setup_path("fedorads-pam.ldif"), self.pam_ldif)

        lnkattr = self.schema.linked_attributes()

        refint_config = open(setup_path("fedorads-refint-delete.ldif"), 'r').read()
        memberof_config = ""
        index_config = ""
        argnum = 3

        for attr in lnkattr.keys():
            if lnkattr[attr] is not None:
                refint_config += read_and_sub_file(
                    setup_path("fedorads-refint-add.ldif"),
                         { "ARG_NUMBER" : str(argnum),
                           "LINK_ATTR" : attr })
                memberof_config += read_and_sub_file(
                    setup_path("fedorads-linked-attributes.ldif"),
                         { "MEMBER_ATTR" : attr,
                           "MEMBEROF_ATTR" : lnkattr[attr] })
                index_config += read_and_sub_file(
                    setup_path("fedorads-index.ldif"), { "ATTR" : attr })
                argnum += 1

        open(self.refint_ldif, 'w').write(refint_config)
        open(self.linked_attrs_ldif, 'w').write(memberof_config)

        attrs = ["lDAPDisplayName"]
        res = self.schema.ldb.search(expression="(&(objectclass=attributeSchema)(searchFlags:1.2.840.113556.1.4.803:=1))", base=self.names.schemadn, scope=SCOPE_ONELEVEL, attrs=attrs)

        for i in range (0, len(res)):
            attr = res[i]["lDAPDisplayName"][0]

            if attr == "objectGUID":
                attr = "nsUniqueId"

            index_config += read_and_sub_file(
                setup_path("fedorads-index.ldif"), { "ATTR" : attr })

        open(self.index_ldif, 'w').write(index_config)

        setup_file(setup_path("fedorads-samba.ldif"), self.samba_ldif, {
            "SAMBADN": self.sambadn,
            "LDAPADMINPASS": self.ldapadminpass
            })

        mapping = "schema-map-fedora-ds-1.0"
        backend_schema = "99_ad.ldif"

        # Build a schema file in Fedora DS format
        backend_schema_data = self.schema.convert_to_openldap("fedora-ds",
            open(setup_path(mapping), 'r').read())
        assert backend_schema_data is not None
        f = open(os.path.join(self.ldapdir, backend_schema), 'w')
        try:
            f.write(backend_schema_data)
        finally:
            f.close()

        self.credentials.set_bind_dn(self.names.ldapmanagerdn)

        # Destory the target directory, or else setup-ds.pl will complain
        fedora_ds_dir = os.path.join(self.ldapdir,
            "slapd-" + self.ldap_instance)
        shutil.rmtree(fedora_ds_dir, True)

        self.slapd_provision_command = [self.slapd_path, "-D", fedora_ds_dir,
                "-i", self.slapd_pid]
        # In the 'provision' command line, stay in the foreground so we can
        # easily kill it
        self.slapd_provision_command.append("-d0")

        #the command for the final run is the normal script
        self.slapd_command = [os.path.join(self.ldapdir,
            "slapd-" + self.ldap_instance, "start-slapd")]

        # If we were just looking for crashes up to this point, it's a
        # good time to exit before we realise we don't have Fedora DS on
        if self.ldap_dryrun_mode:
            sys.exit(0)

        # Try to print helpful messages when the user has not specified the
        # path to the setup-ds tool
        if self.setup_ds_path is None:
            raise ProvisioningError("Fedora DS LDAP-Backend must be setup with path to setup-ds, e.g. --setup-ds-path=\"/usr/sbin/setup-ds.pl\"!")
        if not os.path.exists(self.setup_ds_path):
            self.logger.warning("Path (%s) to slapd does not exist!",
                self.setup_ds_path)

        # Run the Fedora DS setup utility
        retcode = subprocess.call([self.setup_ds_path, "--silent", "--file",
            self.fedoradsinf], close_fds=True, shell=False)
        if retcode != 0:
            raise ProvisioningError("setup-ds failed")

        # Load samba-admin
        retcode = subprocess.call([
            os.path.join(self.ldapdir, "slapd-" + self.ldap_instance, "ldif2db"), "-s", self.sambadn, "-i", self.samba_ldif],
            close_fds=True, shell=False)
        if retcode != 0:
            raise ProvisioningError("ldif2db failed")

    def post_setup(self):
        ldapi_db = Ldb(self.ldap_uri, credentials=self.credentials)

        # configure in-directory access control on Fedora DS via the aci
        # attribute (over a direct ldapi:// socket)
        aci = """(targetattr = "*") (version 3.0;acl "full access to all by samba-admin";allow (all)(userdn = "ldap:///CN=samba-admin,%s");)""" % self.sambadn

        m = ldb.Message()
        m["aci"] = ldb.MessageElement([aci], ldb.FLAG_MOD_REPLACE, "aci")

        for dnstring in (self.names.domaindn, self.names.configdn,
                         self.names.schemadn):
            m.dn = ldb.Dn(ldapi_db, dnstring)
            ldapi_db.modify(m)
