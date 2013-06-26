#!/usr/bin/env python
#
# Changes a FSMO role owner
#
# Copyright Nadezhda Ivanova 2009
# Copyright Jelmer Vernooij 2009
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

import samba.getopt as options
import ldb
from ldb import LdbError

from samba.auth import system_session
from samba.netcmd import (
    Command,
    CommandError,
    Option,
    )
from samba.samdb import SamDB

class cmd_fsmo(Command):
    """Makes the targer DC transfer or seize a fsmo role [server connection needed]"""

    synopsis = "(show | transfer <options> | seize <options>)"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_options = [
        Option("--host", help="LDB URL for database or target server", type=str),
        Option("--force", help="Force seizing of the role without attempting to transfer first.", action="store_true"),
        Option("--role", type="choice", choices=["rid", "pdc", "infrastructure","schema","naming","all"],
               help="""The FSMO role to seize or transfer.\n
rid=RidAllocationMasterRole\n
schema=SchemaMasterRole\n
pdc=PdcEmulationMasterRole\n
naming=DomainNamingMasterRole\n
infrastructure=InfrastructureMasterRole\n
all=all of the above"""),
        ]

    takes_args = ["subcommand"]

    def transfer_role(self, role, samdb):
        m = ldb.Message()
        m.dn = ldb.Dn(samdb, "")
        if role == "rid":
            m["becomeRidMaster"]= ldb.MessageElement(
                "1", ldb.FLAG_MOD_REPLACE,
                "becomeRidMaster")
        elif role == "pdc":
            domain_dn = samdb.domain_dn()
            res = samdb.search(domain_dn,
                               scope=ldb.SCOPE_BASE, attrs=["objectSid"])
            assert len(res) == 1
            sid = res[0]["objectSid"][0]
            m["becomePdc"]= ldb.MessageElement(
                sid, ldb.FLAG_MOD_REPLACE,
                "becomePdc")
        elif role == "naming":
            m["becomeDomainMaster"]= ldb.MessageElement(
                "1", ldb.FLAG_MOD_REPLACE,
                "becomeDomainMaster")
            samdb.modify(m)
        elif role == "infrastructure":
            m["becomeInfrastructureMaster"]= ldb.MessageElement(
                "1", ldb.FLAG_MOD_REPLACE,
                "becomeInfrastructureMaster")
        elif role == "schema":
            m["becomeSchemaMaster"]= ldb.MessageElement(
                "1", ldb.FLAG_MOD_REPLACE,
                "becomeSchemaMaster")
        else:
            raise CommandError("Invalid FSMO role.")
        samdb.modify(m)

    def seize_role(self, role, samdb, force):
        res = samdb.search("",
                           scope=ldb.SCOPE_BASE, attrs=["dsServiceName"])
        assert len(res) == 1
        serviceName = res[0]["dsServiceName"][0]
        domain_dn = samdb.domain_dn()
        m = ldb.Message()
        if role == "rid":
            m.dn = ldb.Dn(samdb, self.rid_dn)
        elif role == "pdc":
            m.dn = ldb.Dn(samdb, domain_dn)
        elif role == "naming":
            m.dn = ldb.Dn(samdb, self.naming_dn)
        elif role == "infrastructure":
            m.dn = ldb.Dn(samdb, self.infrastructure_dn)
        elif role == "schema":
            m.dn = ldb.Dn(samdb, self.schema_dn)
        else:
            raise CommandError("Invalid FSMO role.")
        #first try to transfer to avoid problem if the owner is still active
        if force is None:
            self.message("Attempting transfer...")
            try:
                self.transfer_role(role, samdb)
            except LdbError, (num, _):
            #transfer failed, use the big axe...
                self.message("Transfer unsuccessfull, seizing...")
                m["fSMORoleOwner"]= ldb.MessageElement(
                    serviceName, ldb.FLAG_MOD_REPLACE,
                    "fSMORoleOwner")
                samdb.modify(m)
            else:
                self.message("Transfer succeeded.")
        else:
            self.message("Will not attempt transfer, seizing...")
            m["fSMORoleOwner"]= ldb.MessageElement(
                serviceName, ldb.FLAG_MOD_REPLACE,
                "fSMORoleOwner")
            samdb.modify(m)

    def run(self, subcommand, force=None, host=None, role=None,
            credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)

        samdb = SamDB(url=host, session_info=system_session(),
            credentials=creds, lp=lp)

        domain_dn = samdb.domain_dn()
        self.infrastructure_dn = "CN=Infrastructure," + domain_dn
        self.naming_dn = "CN=Partitions,CN=Configuration," + domain_dn
        self.schema_dn = "CN=Schema,CN=Configuration," + domain_dn
        self.rid_dn = "CN=RID Manager$,CN=System," + domain_dn

        res = samdb.search(self.infrastructure_dn,
                           scope=ldb.SCOPE_BASE, attrs=["fSMORoleOwner"])
        assert len(res) == 1
        self.infrastructureMaster = res[0]["fSMORoleOwner"][0]

        res = samdb.search(domain_dn,
                           scope=ldb.SCOPE_BASE, attrs=["fSMORoleOwner"])
        assert len(res) == 1
        self.pdcEmulator = res[0]["fSMORoleOwner"][0]

        res = samdb.search(self.naming_dn,
                           scope=ldb.SCOPE_BASE, attrs=["fSMORoleOwner"])
        assert len(res) == 1
        self.namingMaster = res[0]["fSMORoleOwner"][0]

        res = samdb.search(self.schema_dn,
                           scope=ldb.SCOPE_BASE, attrs=["fSMORoleOwner"])
        assert len(res) == 1
        self.schemaMaster = res[0]["fSMORoleOwner"][0]

        res = samdb.search(self.rid_dn,
                           scope=ldb.SCOPE_BASE, attrs=["fSMORoleOwner"])
        assert len(res) == 1
        self.ridMaster = res[0]["fSMORoleOwner"][0]

        if subcommand == "show":
            self.message("InfrastructureMasterRole owner: " + self.infrastructureMaster)
            self.message("RidAllocationMasterRole owner: " + self.ridMaster)
            self.message("PdcEmulationMasterRole owner: " + self.pdcEmulator)
            self.message("DomainNamingMasterRole owner: " + self.namingMaster)
            self.message("SchemaMasterRole owner: " + self.schemaMaster)
        elif subcommand == "transfer":
            if role == "all":
                self.transfer_role("rid", samdb)
                self.transfer_role("pdc", samdb)
                self.transfer_role("naming", samdb)
                self.transfer_role("infrastructure", samdb)
                self.transfer_role("schema", samdb)
            else:
                self.transfer_role(role, samdb)
        elif subcommand == "seize":
            if role == "all":
                self.seize_role("rid", samdb, force)
                self.seize_role("pdc", samdb, force)
                self.seize_role("naming", samdb, force)
                self.seize_role("infrastructure", samdb, force)
                self.seize_role("schema", samdb, force)
            else:
                self.seize_role(role, samdb, force)
        else:
            raise CommandError("Wrong argument '%s'!" % subcommand)
