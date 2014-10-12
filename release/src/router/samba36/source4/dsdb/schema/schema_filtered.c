/* 
   Unix SMB/CIFS mplementation.
   API for determining af an attribute belongs to the filtered set.
   
   Copyright (C) Nadezhda Ivanova <nivanova@samba.org> 2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   
*/
#include "includes.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/common/util.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "param/param.h"

static const char * const never_in_filtered_attrs[] = {
				     "accountExpires",
				     "codePage",
				     "creationTime",
				     "dNSHostName",
				     "displayName",
				     "domainReplica",
				     "fSMORoleOwner",
				     "flatName",
				     "isCriticalSystemObject",
				     "lockOutObservationWindow",
				     "lockoutDuration",
				     "lockoutTime",
				     "logonHours",
				     "maxPwdAge",
				     "minPwdAge",
				     "minPwdLength",
				     "msDS-AdditionalDnsHostName",
				     "msDS-AdditionalSamAccountName",
				     "msDS-AllowedToDelegateTo",
				     "msDS-AuthenticatedAtDC",
				     "msDS-ExecuteScriptPassword",
				     "msDS-KrbTgtLink",
				     "msDS-SPNSuffixes",
				     "msDS-SupportedEncryptionTypes",
				     "msDS-TrustForestTrustInfo",
				     "nETBIOSName",
				     "nTMixedDomain",
				     "notFiltlockoutThreshold",
				     "operatingSystem",
				     "operatingSystemServicePack",
				     "operatingSystemVersion",
				     "pwdHistoryLength",
				     "pwdLastSet",
				     "pwdProperties",
				     "rid",
				     "sIDHistory",
				     "securityIdentifier",
				     "servicePrincipalName",
				     "trustAttributes",
				     "trustDirection",
				     "trustParent",
				     "trustPartner",
				     "trustPosixOffset",
				     "trustType",
				     DSDB_SECRET_ATTRIBUTES
};

/* returns true if the attribute can be in a filtered replica */

bool dsdb_attribute_is_attr_in_filtered_replica(struct dsdb_attribute *attribute)
{
	int i, size = sizeof(never_in_filtered_attrs)/sizeof(char *);
	if (attribute->systemOnly ||
	    attribute->schemaFlagsEx & SCHEMA_FLAG_ATTR_IS_CRITICAL) {
		return false;
	}
	if (attribute->systemFlags & (DS_FLAG_ATTR_NOT_REPLICATED |
				      DS_FLAG_ATTR_REQ_PARTIAL_SET_MEMBER |
				      DS_FLAG_ATTR_IS_CONSTRUCTED)) {
		return false;
	}

	for (i=0; i < size; i++) {
		if (strcmp(attribute->lDAPDisplayName, never_in_filtered_attrs[i]) == 0) {
			return false;
		}
	}

	if (attribute->searchFlags & SEARCH_FLAG_RODC_ATTRIBUTE) {
		return false;
	}
	return true;
}
