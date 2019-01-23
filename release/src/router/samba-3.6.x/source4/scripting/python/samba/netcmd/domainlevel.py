#!/usr/bin/env python
#
# Raises domain and forest function levels
#
# Copyright Matthias Dieter Wallnoefer 2009
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

# Notice: At the moment we have some more checks to do here on the special
# attributes (consider attribute "msDS-Behavior-Version). This is due to the
# fact that we on s4 LDB don't implement their change policy (only certain
# values, only increments possible...) yet.

import samba.getopt as options
import ldb

from samba.auth import system_session
from samba.netcmd import (
    Command,
    CommandError,
    Option,
    )
from samba.samdb import SamDB
from samba.dsdb import (
    DS_DOMAIN_FUNCTION_2000,
    DS_DOMAIN_FUNCTION_2003,
    DS_DOMAIN_FUNCTION_2003_MIXED,
    DS_DOMAIN_FUNCTION_2008,
    DS_DOMAIN_FUNCTION_2008_R2,
    )

class cmd_domainlevel(Command):
    """Raises domain and forest function levels"""

    synopsis = "(show | raise <options>)"

    takes_optiongroups = {
        "sambaopts": options.SambaOptions,
        "credopts": options.CredentialsOptions,
        "versionopts": options.VersionOptions,
        }

    takes_options = [
        Option("-H", help="LDB URL for database or target server", type=str),
        Option("--quiet", help="Be quiet", action="store_true"),
        Option("--forest", type="choice", choices=["2003", "2008", "2008_R2"],
            help="The forest function level (2003 | 2008 | 2008_R2)"),
        Option("--domain", type="choice", choices=["2003", "2008", "2008_R2"],
            help="The domain function level (2003 | 2008 | 2008_R2)"),
        ]

    takes_args = ["subcommand"]

    def run(self, subcommand, H=None, forest=None, domain=None, quiet=False,
            credopts=None, sambaopts=None, versionopts=None):
        lp = sambaopts.get_loadparm()
        creds = credopts.get_credentials(lp, fallback_machine=True)

        samdb = SamDB(url=H, session_info=system_session(),
            credentials=creds, lp=lp)

        domain_dn = samdb.domain_dn()

        res_forest = samdb.search("CN=Partitions,CN=Configuration," + domain_dn,
          scope=ldb.SCOPE_BASE, attrs=["msDS-Behavior-Version"])
        assert len(res_forest) == 1

        res_domain = samdb.search(domain_dn, scope=ldb.SCOPE_BASE,
          attrs=["msDS-Behavior-Version", "nTMixedDomain"])
        assert len(res_domain) == 1

        res_dc_s = samdb.search("CN=Sites,CN=Configuration," + domain_dn,
          scope=ldb.SCOPE_SUBTREE, expression="(objectClass=nTDSDSA)",
          attrs=["msDS-Behavior-Version"])
        assert len(res_dc_s) >= 1

        try:
            level_forest = int(res_forest[0]["msDS-Behavior-Version"][0])
            level_domain = int(res_domain[0]["msDS-Behavior-Version"][0])
            level_domain_mixed = int(res_domain[0]["nTMixedDomain"][0])

            min_level_dc = int(res_dc_s[0]["msDS-Behavior-Version"][0]) # Init value
            for msg in res_dc_s:
                if int(msg["msDS-Behavior-Version"][0]) < min_level_dc:
                    min_level_dc = int(msg["msDS-Behavior-Version"][0])

            if level_forest < 0 or level_domain < 0:
                raise CommandError("Domain and/or forest function level(s) is/are invalid. Correct them or reprovision!")
            if min_level_dc < 0:
                raise CommandError("Lowest function level of a DC is invalid. Correct this or reprovision!")
            if level_forest > level_domain:
                raise CommandError("Forest function level is higher than the domain level(s). Correct this or reprovision!")
            if level_domain > min_level_dc:
                raise CommandError("Domain function level is higher than the lowest function level of a DC. Correct this or reprovision!")

        except KeyError:
            raise CommandError("Could not retrieve the actual domain, forest level and/or lowest DC function level!")

        if subcommand == "show":
            self.message("Domain and forest function level for domain '%s'" % domain_dn)
            if level_forest == DS_DOMAIN_FUNCTION_2000 and level_domain_mixed != 0:
                self.message("\nATTENTION: You run SAMBA 4 on a forest function level lower than Windows 2000 (Native). This isn't supported! Please raise!")
            if level_domain == DS_DOMAIN_FUNCTION_2000 and level_domain_mixed != 0:
                self.message("\nATTENTION: You run SAMBA 4 on a domain function level lower than Windows 2000 (Native). This isn't supported! Please raise!")
            if min_level_dc == DS_DOMAIN_FUNCTION_2000 and level_domain_mixed != 0:
                self.message("\nATTENTION: You run SAMBA 4 on a lowest function level of a DC lower than Windows 2003. This isn't supported! Please step-up or upgrade the concerning DC(s)!")

            self.message("")

            if level_forest == DS_DOMAIN_FUNCTION_2000:
                outstr = "2000"
            elif level_forest == DS_DOMAIN_FUNCTION_2003_MIXED:
                outstr = "2003 with mixed domains/interim (NT4 DC support)"
            elif level_forest == DS_DOMAIN_FUNCTION_2003:
                outstr = "2003"
            elif level_forest == DS_DOMAIN_FUNCTION_2008:
                outstr = "2008"
            elif level_forest == DS_DOMAIN_FUNCTION_2008_R2:
                outstr = "2008 R2"
            else:
                outstr = "higher than 2008 R2"
            self.message("Forest function level: (Windows) " + outstr)

            if level_domain == DS_DOMAIN_FUNCTION_2000 and level_domain_mixed != 0:
                outstr = "2000 mixed (NT4 DC support)"
            elif level_domain == DS_DOMAIN_FUNCTION_2000 and level_domain_mixed == 0:
                outstr = "2000"
            elif level_domain == DS_DOMAIN_FUNCTION_2003_MIXED:
                outstr = "2003 with mixed domains/interim (NT4 DC support)"
            elif level_domain == DS_DOMAIN_FUNCTION_2003:
                outstr = "2003"
            elif level_domain == DS_DOMAIN_FUNCTION_2008:
                outstr = "2008"
            elif level_domain == DS_DOMAIN_FUNCTION_2008_R2:
                outstr = "2008 R2"
            else:
                outstr = "higher than 2008 R2"
            self.message("Domain function level: (Windows) " + outstr)

            if min_level_dc == DS_DOMAIN_FUNCTION_2000:
                outstr = "2000"
            elif min_level_dc == DS_DOMAIN_FUNCTION_2003:
                outstr = "2003"
            elif min_level_dc == DS_DOMAIN_FUNCTION_2008:
                outstr = "2008"
            elif min_level_dc == DS_DOMAIN_FUNCTION_2008_R2:
                outstr = "2008 R2"
            else:
                outstr = "higher than 2008 R2"
            self.message("Lowest function level of a DC: (Windows) " + outstr)

        elif subcommand == "raise":
            msgs = []

            if domain is not None:
                if domain == "2003":
                    new_level_domain = DS_DOMAIN_FUNCTION_2003
                elif domain == "2008":
                    new_level_domain = DS_DOMAIN_FUNCTION_2008
                elif domain == "2008_R2":
                    new_level_domain = DS_DOMAIN_FUNCTION_2008_R2

                if new_level_domain <= level_domain and level_domain_mixed == 0:
                    raise CommandError("Domain function level can't be smaller equal to the actual one!")

                if new_level_domain > min_level_dc:
                    raise CommandError("Domain function level can't be higher than the lowest function level of a DC!")

                # Deactivate mixed/interim domain support
                if level_domain_mixed != 0:
                    # Directly on the base DN
                    m = ldb.Message()
                    m.dn = ldb.Dn(samdb, domain_dn)
                    m["nTMixedDomain"] = ldb.MessageElement("0",
                      ldb.FLAG_MOD_REPLACE, "nTMixedDomain")
                    samdb.modify(m)
                    # Under partitions
                    m = ldb.Message()
                    m.dn = ldb.Dn(samdb, "CN=" + lp.get("workgroup")
                      + ",CN=Partitions,CN=Configuration," + domain_dn)
                    m["nTMixedDomain"] = ldb.MessageElement("0",
                      ldb.FLAG_MOD_REPLACE, "nTMixedDomain")
                    try:
                        samdb.modify(m)
                    except ldb.LdbError, (enum, emsg):
                        if enum != ldb.ERR_UNWILLING_TO_PERFORM:
                            raise

                # Directly on the base DN
                m = ldb.Message()
                m.dn = ldb.Dn(samdb, domain_dn)
                m["msDS-Behavior-Version"]= ldb.MessageElement(
                  str(new_level_domain), ldb.FLAG_MOD_REPLACE,
                          "msDS-Behavior-Version")
                samdb.modify(m)
                # Under partitions
                m = ldb.Message()
                m.dn = ldb.Dn(samdb, "CN=" + lp.get("workgroup")
                  + ",CN=Partitions,CN=Configuration," + domain_dn)
                m["msDS-Behavior-Version"]= ldb.MessageElement(
                  str(new_level_domain), ldb.FLAG_MOD_REPLACE,
                          "msDS-Behavior-Version")
                try:
                    samdb.modify(m)
                except ldb.LdbError, (enum, emsg):
                    if enum != ldb.ERR_UNWILLING_TO_PERFORM:
                        raise

                level_domain = new_level_domain
                msgs.append("Domain function level changed!")

            if forest is not None:
                if forest == "2003":
                    new_level_forest = DS_DOMAIN_FUNCTION_2003
                elif forest == "2008":
                    new_level_forest = DS_DOMAIN_FUNCTION_2008
                elif forest == "2008_R2":
                    new_level_forest = DS_DOMAIN_FUNCTION_2008_R2
                if new_level_forest <= level_forest:
                    raise CommandError("Forest function level can't be smaller equal to the actual one!")
                if new_level_forest > level_domain:
                    raise CommandError("Forest function level can't be higher than the domain function level(s). Please raise it/them first!")
                m = ldb.Message()
                m.dn = ldb.Dn(samdb, "CN=Partitions,CN=Configuration,"
                  + domain_dn)
                m["msDS-Behavior-Version"]= ldb.MessageElement(
                  str(new_level_forest), ldb.FLAG_MOD_REPLACE,
                          "msDS-Behavior-Version")
                samdb.modify(m)
                msgs.append("Forest function level changed!")
            msgs.append("All changes applied successfully!")
            self.message("\n".join(msgs))
        else:
            raise CommandError("Wrong argument '%s'!" % subcommand)
