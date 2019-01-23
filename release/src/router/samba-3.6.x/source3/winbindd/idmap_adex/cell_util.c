/*
 * idmap_adex: Support for AD Forests
 *
 * Copyright (C) Gerald (Jerry) Carter 2006-2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "ads.h"
#include "idmap.h"
#include "idmap_adex.h"
#include "../libds/common/flags.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/**********************************************************************
**********************************************************************/

 char *find_attr_string(char **list, size_t num_lines, const char *substr)
{
	int i;
	int cmplen = strlen(substr);

	for (i = 0; i < num_lines; i++) {
		/* make sure to avoid substring matches like uid
		   and uidNumber */
		if ((StrnCaseCmp(list[i], substr, cmplen) == 0) &&
		    (list[i][cmplen] == '=')) {
			/* Don't return an empty string */
			if (list[i][cmplen + 1] != '\0')
				return &(list[i][cmplen + 1]);

			return NULL;
		}
	}

	return NULL;
}

/**********************************************************************
**********************************************************************/

 bool is_object_class(char **list, size_t num_lines, const char *substr)
{
	int i;

	for (i = 0; i < num_lines; i++) {
		if (strequal(list[i], substr)) {
			return true;
		}
	}

	return false;
}

/**********************************************************************
 Find out about the cell (e.g. use2307Attrs, etc...)
**********************************************************************/

 NTSTATUS cell_lookup_settings(struct likewise_cell * cell)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	/* Parameter check */

	if (!cell) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Only supporting Forest-wide, schema based searches */

	cell_set_flags(cell, LWCELL_FLAG_USE_RFC2307_ATTRS);
	cell_set_flags(cell, LWCELL_FLAG_SEARCH_FOREST);

	cell->provider = &ccp_unified;

	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1,("LWI: Failed to obtain cell settings (%s)\n",
			 nt_errstr(nt_status)));
	}

	return nt_status;
}


static NTSTATUS cell_lookup_forest(struct likewise_cell *c)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct gc_info *gc = NULL;

	if (!c) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if ((gc = TALLOC_ZERO_P(NULL, struct gc_info)) == NULL) {
		nt_status = NT_STATUS_NO_MEMORY;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Query the rootDSE for the forest root naming conect first.
           Check that the a GC server for the forest has not already
	   been added */

	nt_status = gc_find_forest_root(gc, cell_dns_domain(c));
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	c->forest_name = talloc_strdup(c, gc->forest_name);
	BAIL_ON_PTR_ERROR(c->forest_name, nt_status);

done:
	if (gc) {
		talloc_free(gc);
	}

	return nt_status;
}

/**********************************************************************
**********************************************************************/

 NTSTATUS cell_locate_membership(ADS_STRUCT * ads)
{
	ADS_STATUS status;
	char *domain_dn = ads_build_dn(lp_realm());
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct dom_sid sid;
	struct likewise_cell *cell = NULL;

	/* In the Likewise plugin, I had to support the concept of cells
	   based on the machine's membership in an OU.  However, now I'll
	   just assume our membership in the forest cell */

	DEBUG(2, ("locate_cell_membership: Located membership "
		  "in cell \"%s\"\n", domain_dn));

	if ((cell = cell_new()) == NULL) {
		nt_status = NT_STATUS_NO_MEMORY;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	status = ads_domain_sid(ads, &sid);
	if (!ADS_ERR_OK(status)) {
		DEBUG(3,("locate_cell_membership: Failed to find "
			 "domain SID for %s\n", domain_dn));
	}

	/* save the SID and search base for our domain */

	cell_set_dns_domain(cell, lp_realm());
	cell_set_connection(cell, ads);
	cell_set_dn(cell, domain_dn);
	cell_set_domain_sid(cell, &sid);

	/* Now save our forest root */

	cell_lookup_forest(cell);

	/* Add the cell to the list */

	if (!cell_list_add(cell)) {
		nt_status = NT_STATUS_INSUFFICIENT_RESOURCES;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Done! */
	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(0,("LWI: Failed to locate cell membership (%s)\n",
			 nt_errstr(nt_status)));
	}

	SAFE_FREE(domain_dn);

	return nt_status;
}

/*********************************************************************
 ********************************************************************/

 int min_id_value(void)
{
	int id_val;

	id_val = lp_parm_int(-1, "lwidentity", "min_id_value", MIN_ID_VALUE);

	/* Still don't let it go below 50 */

	return MAX(50, id_val);
}

/********************************************************************
 *******************************************************************/

 char *cell_dn_to_dns(const char *dn)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *domain = NULL;
	char *dns_name = NULL;
	const char *tmp_dn;
	char *buffer = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!dn || !*dn) {
		goto done;
	}

	tmp_dn = talloc_strdup(frame, dn);
	BAIL_ON_PTR_ERROR(tmp_dn, nt_status);

	while (next_token_talloc(frame, &tmp_dn, &buffer, ",")) {

		/* skip everything up the where DC=... begins */
		if (StrnCaseCmp(buffer, "DC=", 3) != 0)
			continue;

		if (!domain) {
			domain = talloc_strdup(frame, &buffer[3]);
		} else {
			domain = talloc_asprintf_append(domain, ".%s",
							&buffer[3]);
		}
		BAIL_ON_PTR_ERROR(domain, nt_status);
	}

	dns_name = SMB_STRDUP(domain);
	BAIL_ON_PTR_ERROR(dns_name, nt_status);

	nt_status = NT_STATUS_OK;

done:
	PRINT_NTSTATUS_ERROR(nt_status, "cell_dn_to_dns", 1);

	talloc_destroy(frame);

	return dns_name;
}

/*********************************************************************
 ********************************************************************/

 NTSTATUS get_sid_type(ADS_STRUCT *ads,
		       LDAPMessage *msg,
		       enum lsa_SidType *type)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	uint32_t atype;

	if (!ads_pull_uint32(ads, msg, "sAMAccountType", &atype)) {
		nt_status = NT_STATUS_INVALID_USER_BUFFER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	switch (atype &0xF0000000) {
	case ATYPE_SECURITY_GLOBAL_GROUP:
		*type = SID_NAME_DOM_GRP;
		break;
	case ATYPE_SECURITY_LOCAL_GROUP:
		*type = SID_NAME_ALIAS;
		break;
	case ATYPE_NORMAL_ACCOUNT:
	case ATYPE_WORKSTATION_TRUST:
	case ATYPE_INTERDOMAIN_TRUST:
		*type = SID_NAME_USER;
		break;
	default:
		*type = SID_NAME_USE_NONE;
		nt_status = NT_STATUS_INVALID_ACCOUNT_NAME;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	nt_status = NT_STATUS_OK;

done:
	return nt_status;
}
