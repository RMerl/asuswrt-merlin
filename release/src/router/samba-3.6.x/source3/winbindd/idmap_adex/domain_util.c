/*
 * idmap_adex: Domain search interface
 *
 * Copyright (C) Gerald (Jerry) Carter 2007-2008
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

struct dc_info {
	struct dc_info *prev, *next;
	char *dns_name;
	struct likewise_cell *domain_cell;
};

static struct dc_info *_dc_server_list = NULL;


/**********************************************************************
 *********************************************************************/

static struct dc_info *dc_list_head(void)
{
	return _dc_server_list;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS dc_add_domain(const char *domain)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct dc_info *dc = NULL;

	if (!domain) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	DEBUG(10,("dc_add_domain: Attempting to add domain %s\n", domain));

	/* Check for duplicates */

	dc = dc_list_head();
	while (dc) {
		if (strequal (dc->dns_name, domain))
			break;
		dc = dc->next;
	}

	if (dc) {
		DEBUG(10,("dc_add_domain: %s already in list\n", domain));
		return NT_STATUS_OK;
	}

	dc = TALLOC_ZERO_P(NULL, struct dc_info);
	BAIL_ON_PTR_ERROR(dc, nt_status);

	dc->dns_name = talloc_strdup(dc, domain);
	BAIL_ON_PTR_ERROR(dc->dns_name, nt_status);

	DLIST_ADD_END(_dc_server_list, dc, struct dc_info*);

	nt_status = NT_STATUS_OK;

	DEBUG(5,("dc_add_domain: Successfully added %s\n", domain));

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_destroy(dc);
		DEBUG(0,("LWI: Failed to add new DC connection for %s (%s)\n",
			 domain, nt_errstr(nt_status)));
	}

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static void dc_server_list_destroy(void)
{
	struct dc_info *dc = dc_list_head();

	while (dc) {
		struct dc_info *p = dc->next;

		cell_destroy(dc->domain_cell);
		talloc_destroy(dc);

		dc = p;
	}

	return;
}


/**********************************************************************
 *********************************************************************/

 NTSTATUS domain_init_list(void)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct winbindd_tdc_domain *domains = NULL;
	size_t num_domains = 0;
	int i;

	if (_dc_server_list != NULL) {
		dc_server_list_destroy();
	}

	/* Add our domain */

	nt_status = dc_add_domain(lp_realm());
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	if (!wcache_tdc_fetch_list(&domains, &num_domains)) {
		nt_status = NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Add all domains with an incoming trust path */

	for (i=0; i<num_domains; i++) {
		uint32_t flags = (NETR_TRUST_FLAG_INBOUND|NETR_TRUST_FLAG_IN_FOREST);

		/* We just require one of the flags to be set here */

		if (domains[i].trust_flags & flags) {
			nt_status = dc_add_domain(domains[i].dns_name);
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(2,("LWI: Failed to initialize DC list (%s)\n",
			 nt_errstr(nt_status)));
	}

	TALLOC_FREE(domains);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS dc_do_search(struct dc_info *dc,
			     const char *search_base,
			     int scope,
			     const char *expr,
			     const char **attrs,
			     LDAPMessage ** msg)
{
	ADS_STATUS status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	status = cell_do_search(dc->domain_cell, search_base,
				scope, expr, attrs, msg);
	nt_status = ads_ntstatus(status);

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static struct dc_info *dc_find_domain(const char *dns_domain)
{
	struct dc_info *dc = dc_list_head();

	if (!dc)
		return NULL;

	while (dc) {
		if (strequal(dc->dns_name, dns_domain)) {
			return dc;
		}

		dc = dc->next;
	}

	return NULL;
}

/**********************************************************************
 *********************************************************************/

 NTSTATUS dc_search_domains(struct likewise_cell **cell,
			    LDAPMessage **msg,
			    const char *dn,
			    const struct dom_sid *sid)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *frame = talloc_stackframe();
	char *dns_domain;
	const char *attrs[] = { "*", NULL };
	struct dc_info *dc = NULL;
	const char *base = NULL;

	if (!dn || !*dn) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	dns_domain = cell_dn_to_dns(dn);
	BAIL_ON_PTR_ERROR(dns_domain, nt_status);

	if ((dc = dc_find_domain(dns_domain)) == NULL) {
		nt_status = NT_STATUS_TRUSTED_DOMAIN_FAILURE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Reparse the cell settings for the domain if necessary */

	if (!dc->domain_cell) {
		char *base_dn;

		base_dn = ads_build_dn(dc->dns_name);
		BAIL_ON_PTR_ERROR(base_dn, nt_status);

		nt_status = cell_connect_dn(&dc->domain_cell, base_dn);
		SAFE_FREE(base_dn);
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		nt_status = cell_lookup_settings(dc->domain_cell);
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		/* By definition this is already part of a larger
		   forest-wide search scope */

		cell_set_flags(dc->domain_cell, LWCELL_FLAG_SEARCH_FOREST);
	}

	/* Check whether we are operating in non-schema or RFC2307
	   mode */

	if (cell_flags(dc->domain_cell) & LWCELL_FLAG_USE_RFC2307_ATTRS) {
		nt_status = dc_do_search(dc, dn, LDAP_SCOPE_BASE,
					 "(objectclass=*)", attrs, msg);
	} else {
		const char *sid_str = NULL;
		char *filter = NULL;

		sid_str = sid_string_talloc(frame, sid);
		BAIL_ON_PTR_ERROR(sid_str, nt_status);

		filter = talloc_asprintf(frame, "(keywords=backLink=%s)",
					 sid_str);
		BAIL_ON_PTR_ERROR(filter, nt_status);

		base = cell_search_base(dc->domain_cell);
		BAIL_ON_PTR_ERROR(base, nt_status);

		nt_status = dc_do_search(dc, base, LDAP_SCOPE_SUBTREE,
					 filter, attrs, msg);
	}
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	*cell = dc->domain_cell;

done:
	talloc_destroy(CONST_DISCARD(char*, base));
	talloc_destroy(frame);

	return nt_status;
}
