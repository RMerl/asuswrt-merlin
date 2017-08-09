/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell               1992-1997.
 *  Copyright (C) Luke Kenneth Casson Leighton  1996-1997.
 *  Copyright (C) Paul Ashton                        1997.
 *  Copyright (C) Jeremy Allison                     2001.
 *  Copyright (C) Gerald Carter                      2002.
 *  Copyright (C) Guenther Deschner                  2008.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_dssetup.h"
#include "secrets.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

/********************************************************************
 Fill in a dssetup_DsRolePrimaryDomInfoBasic structure
 ********************************************************************/

static WERROR fill_dsrole_dominfo_basic(TALLOC_CTX *ctx,
					struct dssetup_DsRolePrimaryDomInfoBasic **info)
{
	struct dssetup_DsRolePrimaryDomInfoBasic *basic = NULL;
	char *dnsdomain = NULL;

	DEBUG(10,("fill_dsrole_dominfo_basic: enter\n"));

	basic = TALLOC_ZERO_P(ctx, struct dssetup_DsRolePrimaryDomInfoBasic);
	if (!basic) {
		DEBUG(0,("fill_dsrole_dominfo_basic: out of memory\n"));
		return WERR_NOMEM;
	}

	switch (lp_server_role()) {
		case ROLE_STANDALONE:
			basic->role = DS_ROLE_STANDALONE_SERVER;
			basic->domain = get_global_sam_name();
			break;
		case ROLE_DOMAIN_MEMBER:
			basic->role = DS_ROLE_MEMBER_SERVER;
			basic->domain = lp_workgroup();
			break;
		case ROLE_DOMAIN_BDC:
			basic->role = DS_ROLE_BACKUP_DC;
			basic->domain = get_global_sam_name();
			break;
		case ROLE_DOMAIN_PDC:
			basic->role = DS_ROLE_PRIMARY_DC;
			basic->domain = get_global_sam_name();
			break;
	}

	if (secrets_fetch_domain_guid(lp_workgroup(), &basic->domain_guid)) {
		basic->flags |= DS_ROLE_PRIMARY_DOMAIN_GUID_PRESENT;
	}

	/* fill in some additional fields if we are a member of an AD domain */

	if (lp_security() == SEC_ADS) {
		dnsdomain = talloc_strdup(ctx, lp_realm());
		if (!dnsdomain) {
			return WERR_NOMEM;
		}
		strlower_m(dnsdomain);
		basic->dns_domain = dnsdomain;

		/* FIXME!! We really should fill in the correct forest
		   name.  Should get this information from winbindd.  */
		basic->forest = dnsdomain;
	} else {
		/* security = domain should not fill in the dns or
		   forest name */
		basic->dns_domain = NULL;
		basic->forest = NULL;
	}

	*info = basic;

	return WERR_OK;
}

/********************************************************************
 Implement the _dssetup_DsRoleGetPrimaryDomainInformation() call
 ********************************************************************/

WERROR _dssetup_DsRoleGetPrimaryDomainInformation(struct pipes_struct *p,
						  struct dssetup_DsRoleGetPrimaryDomainInformation *r)
{
	WERROR werr = WERR_OK;

	switch (r->in.level) {

		case DS_ROLE_BASIC_INFORMATION: {
			struct dssetup_DsRolePrimaryDomInfoBasic *basic = NULL;
			werr = fill_dsrole_dominfo_basic(p->mem_ctx, &basic);
			if (W_ERROR_IS_OK(werr)) {
				r->out.info->basic = *basic;
			}
			break;
		}
		default:
			DEBUG(0,("_dssetup_DsRoleGetPrimaryDomainInformation: "
				"Unknown info level [%d]!\n", r->in.level));
			werr = WERR_UNKNOWN_LEVEL;
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleDnsNameToFlatName(struct pipes_struct *p,
					struct dssetup_DsRoleDnsNameToFlatName *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleDcAsDc(struct pipes_struct *p,
			     struct dssetup_DsRoleDcAsDc *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleDcAsReplica(struct pipes_struct *p,
				  struct dssetup_DsRoleDcAsReplica *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleDemoteDc(struct pipes_struct *p,
			       struct dssetup_DsRoleDemoteDc *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleGetDcOperationProgress(struct pipes_struct *p,
					     struct dssetup_DsRoleGetDcOperationProgress *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleGetDcOperationResults(struct pipes_struct *p,
					    struct dssetup_DsRoleGetDcOperationResults *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleCancel(struct pipes_struct *p,
			     struct dssetup_DsRoleCancel *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleServerSaveStateForUpgrade(struct pipes_struct *p,
						struct dssetup_DsRoleServerSaveStateForUpgrade *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleUpgradeDownlevelServer(struct pipes_struct *p,
					     struct dssetup_DsRoleUpgradeDownlevelServer *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _dssetup_DsRoleAbortDownlevelServerUpgrade(struct pipes_struct *p,
						  struct dssetup_DsRoleAbortDownlevelServerUpgrade *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}
