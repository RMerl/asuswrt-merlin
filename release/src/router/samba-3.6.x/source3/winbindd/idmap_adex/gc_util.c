/*
 * idmap_adex: Global Catalog search interface
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
#include "libads/cldap.h"
#include "../libcli/ldap/ldap_ndr.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

static struct gc_info *_gc_server_list = NULL;


/**********************************************************************
 *********************************************************************/

static struct gc_info *gc_list_head(void)
{
	return _gc_server_list;
}

/**********************************************************************
 Checks if either of the domains is a subdomain of the other
 *********************************************************************/

static bool is_subdomain(const char* a, const char *b)
{
	char *s;
	TALLOC_CTX *frame = talloc_stackframe();
	char *x, *y;
	bool ret = false;

	/* Trivial cases */

	if (!a && !b)
		return true;

	if (!a || !b)
		return false;

	/* Normalize the case */

	x = talloc_strdup(frame, a);
	y = talloc_strdup(frame, b);
	if (!x || !y) {
		ret = false;
		goto done;
	}

	strupper_m(x);
	strupper_m(y);

	/* Exact match */

	if (strcmp(x, y) == 0) {
		ret = true;
		goto done;
	}

	/* Check for trailing substrings */

	s = strstr_m(x, y);
	if (s && (strlen(s) == strlen(y))) {
		ret = true;
		goto done;
	}

	s = strstr_m(y, x);
	if (s && (strlen(s) == strlen(x))) {
		ret = true;
		goto done;
	}

done:
	talloc_destroy(frame);

	return ret;
}

/**********************************************************************
 *********************************************************************/

 NTSTATUS gc_find_forest_root(struct gc_info *gc, const char *domain)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS ads_status;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct NETLOGON_SAM_LOGON_RESPONSE_EX cldap_reply;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!gc || !domain) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	ZERO_STRUCT(cldap_reply);

	ads = ads_init(domain, NULL, NULL);
	BAIL_ON_PTR_ERROR(ads, nt_status);

	ads->auth.flags = ADS_AUTH_NO_BIND;
	ads_status = ads_connect(ads);
	if (!ADS_ERR_OK(ads_status)) {
		DEBUG(4, ("find_forest_root: ads_connect(%s) failed! (%s)\n",
			  domain, ads_errstr(ads_status)));
	}
	nt_status = ads_ntstatus(ads_status);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	if (!ads_cldap_netlogon_5(frame,
				  ads->config.ldap_server_name,
				  ads->config.realm,
				  &cldap_reply))
	{
		DEBUG(4,("find_forest_root: Failed to get a CLDAP reply from %s!\n",
			 ads->server.ldap_server));
		nt_status = NT_STATUS_IO_TIMEOUT;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	gc->forest_name = talloc_strdup(gc, cldap_reply.forest);
	BAIL_ON_PTR_ERROR(gc->forest_name, nt_status);

done:
	if (ads) {
		ads_destroy(&ads);
	}

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS gc_add_forest(const char *domain)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct gc_info *gc = NULL;
	struct gc_info *find_gc = NULL;
	char *dn;
	ADS_STRUCT *ads = NULL;
	struct likewise_cell *primary_cell = NULL;

	primary_cell = cell_list_head();
	if (!primary_cell) {
		nt_status = NT_STATUS_INVALID_SERVER_STATE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Check for duplicates based on domain name first as this
           requires no connection */

	find_gc = gc_list_head();
	while (find_gc) {
		if (strequal (find_gc->forest_name, domain))
			break;
		find_gc = find_gc->next;
	}

	if (find_gc) {
		DEBUG(10,("gc_add_forest: %s already in list\n", find_gc->forest_name));
		return NT_STATUS_OK;
	}

	if ((gc = TALLOC_ZERO_P(NULL, struct gc_info)) == NULL) {
		nt_status = NT_STATUS_NO_MEMORY;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Query the rootDSE for the forest root naming conect first.
           Check that the a GC server for the forest has not already
	   been added */

	nt_status = gc_find_forest_root(gc, domain);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	find_gc = gc_list_head();
	while (find_gc) {
		if (strequal (find_gc->forest_name, gc->forest_name))
			break;
		find_gc = find_gc->next;
	}

	if (find_gc) {
		DEBUG(10,("gc_add_forest: Forest %s already in list\n",
			  find_gc->forest_name));
		return NT_STATUS_OK;
	}

	/* Not found, so add it here.  Make sure we connect to
	   a DC in _this_ domain and not the forest root. */

	dn = ads_build_dn(gc->forest_name);
	BAIL_ON_PTR_ERROR(dn, nt_status);

	gc->search_base = talloc_strdup(gc, dn);
	SAFE_FREE(dn);
	BAIL_ON_PTR_ERROR(gc->search_base, nt_status);

#if 0
	/* Can't use cell_connect_dn() here as there is no way to
	   specifiy the LWCELL_FLAG_GC_CELL flag setting for cell_connect() */

	nt_status = cell_connect_dn(&gc->forest_cell, gc->search_base);
	BAIL_ON_NTSTATUS_ERROR(nt_status);
#else

	gc->forest_cell = cell_new();
	BAIL_ON_PTR_ERROR(gc->forest_cell, nt_status);

	/* Set the DNS domain, dn, etc ... and add it to the list */

	cell_set_dns_domain(gc->forest_cell, gc->forest_name);
	cell_set_dn(gc->forest_cell, gc->search_base);
	cell_set_flags(gc->forest_cell, LWCELL_FLAG_GC_CELL);
#endif

	/* It is possible to belong to a non-forest cell and a
	   non-provisioned forest (at our domain levele). In that
	   case, we should just inherit the flags from our primary
	   cell since the GC searches will match our own schema
	   model. */

	if (strequal(primary_cell->forest_name, gc->forest_name)
	    || is_subdomain(primary_cell->dns_domain, gc->forest_name))
	{
		cell_set_flags(gc->forest_cell, cell_flags(primary_cell));
	} else {
		/* outside of our domain */

		nt_status = cell_connect(gc->forest_cell);
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		nt_status = cell_lookup_settings(gc->forest_cell);
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		/* Drop the connection now that we have the settings */

		ads = cell_connection(gc->forest_cell);
		ads_destroy(&ads);
		cell_set_connection(gc->forest_cell, NULL);
	}

	DLIST_ADD_END(_gc_server_list, gc, struct gc_info*);

	DEBUG(10,("gc_add_forest: Added %s to Global Catalog list of servers\n",
		  gc->forest_name));

	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_destroy(gc);
		DEBUG(3,("LWI: Failed to add new GC connection for %s (%s)\n",
			 domain, nt_errstr(nt_status)));
	}

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static void gc_server_list_destroy(void)
{
	struct gc_info *gc = gc_list_head();

	while (gc) {
		struct gc_info *p = gc->next;

		cell_destroy(gc->forest_cell);
		talloc_destroy(gc);

		gc = p;
	}

	_gc_server_list = NULL;

	return;
}

/**********************************************************************
 Setup the initial list of forests and initial the forest cell
 settings for each.  FIXME!!!
 *********************************************************************/

 NTSTATUS gc_init_list(void)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct winbindd_tdc_domain *domains = NULL;
	size_t num_domains = 0;
	int i;

	if (_gc_server_list != NULL) {
		gc_server_list_destroy();
	}

	if (!wcache_tdc_fetch_list(&domains, &num_domains)) {
		nt_status = NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Find our forest first.  Have to try all domains here starting
	   with our own.  gc_add_forest() filters duplicates */

	nt_status = gc_add_forest(lp_realm());
	WARN_ON_NTSTATUS_ERROR(nt_status);

	for (i=0; i<num_domains; i++) {
		uint32_t flags = (NETR_TRUST_FLAG_IN_FOREST);

		/* I think we should be able to break out of loop once
		   we add a GC for our forest and not have to test every one.
		   In fact, this entire loop is probably irrelevant since
		   the GC location code should always find a GC given lp_realm().
		   Will have to spend time testing before making the change.
		   --jerry */

		if ((domains[i].trust_flags & flags) == flags) {
			nt_status = gc_add_forest(domains[i].dns_name);
			WARN_ON_NTSTATUS_ERROR(nt_status);
			/* Don't BAIL here since not every domain may
			   have a GC server */
		}
	}

	/* Now add trusted forests.  gc_add_forest() will filter out
	   duplicates. Check everything with an incoming trust path
	   that is not in our own forest.  */

	for (i=0; i<num_domains; i++) {
		uint32_t flags = domains[i].trust_flags;
		uint32_t attribs = domains[i].trust_attribs;

		/* Skip non_AD domains */

		if (strlen(domains[i].dns_name) == 0) {
			continue;
		}

		/* Only add a GC for a forest outside of our own.
		   Ignore QUARANTINED/EXTERNAL trusts */

		if ((flags & NETR_TRUST_FLAG_INBOUND)
		    && !(flags & NETR_TRUST_FLAG_IN_FOREST)
		    && (attribs & NETR_TRUST_ATTRIBUTE_FOREST_TRANSITIVE))
		{
			nt_status = gc_add_forest(domains[i].dns_name);
			WARN_ON_NTSTATUS_ERROR(nt_status);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(2,("LWI: Failed to initialized GC list (%s)\n",
			 nt_errstr(nt_status)));
	}

	TALLOC_FREE(domains);

	return nt_status;
}


/**********************************************************************
 *********************************************************************/

 struct gc_info *gc_search_start(void)
{
	NTSTATUS nt_status = NT_STATUS_OK;
	struct gc_info *gc = gc_list_head();

	if (!gc) {
		nt_status = gc_init_list();
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		gc = gc_list_head();
	}

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(2,("LWI: Failed to initialize GC list (%s)\n",
			 nt_errstr(nt_status)));
	}

	return gc;
}

/**********************************************************************
 Search Global Catalog.  Always search our own forest.  The flags set
 controls whether or not we search cross forest.  Assume that the
 resulting set is always returned from one GC so that we don't have to
 both combining the LDAPMessage * results
 *********************************************************************/

 NTSTATUS gc_search_forest(struct gc_info *gc,
			   LDAPMessage **msg,
			   const char *filter)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	ADS_STATUS ads_status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
	const char *attrs[] = {"*", NULL};
	LDAPMessage *m = NULL;

	if (!gc || !msg || !filter) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* When you have multiple domain trees in a forest, the
	   GC will search all naming contexts when you send it
	   and empty ("") base search suffix.   Tested against
	   Windows 2003.  */

	ads_status = cell_do_search(gc->forest_cell, "",
				   LDAP_SCOPE_SUBTREE, filter, attrs, &m);
	nt_status = ads_ntstatus(ads_status);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	*msg = m;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(2,("LWI: Forest wide search %s failed (%s)\n",
			 filter, nt_errstr(nt_status)));
	}

	return nt_status;
}

/**********************************************************************
 Search all forests via GC and return the results in an array of
 ADS_STRUCT/LDAPMessage pairs.
 *********************************************************************/

 NTSTATUS gc_search_all_forests(const char *filter,
				ADS_STRUCT ***ads_list,
				LDAPMessage ***msg_list,
				int *num_resp, uint32_t flags)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct gc_info *gc = NULL;
	uint32_t test_flags = ADEX_GC_SEARCH_CHECK_UNIQUE;

	*ads_list = NULL;
	*msg_list = NULL;
	*num_resp = 0;

	if ((gc = gc_search_start()) == NULL) {
		nt_status = NT_STATUS_INVALID_DOMAIN_STATE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	while (gc) {
		LDAPMessage *m = NULL;

		nt_status = gc_search_forest(gc, &m, filter);
		if (!NT_STATUS_IS_OK(nt_status)) {
			gc = gc->next;
			continue;
		}

		nt_status = add_ads_result_to_array(cell_connection(gc->forest_cell),
						    m, ads_list, msg_list,
						    num_resp);
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		/* If there can only be one match, then we are done */

		if ((*num_resp > 0) && ((flags & test_flags) == test_flags)) {
			break;
		}

		gc = gc->next;
	}

	if (*num_resp == 0) {
		nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	nt_status = NT_STATUS_OK;

done:
	return nt_status;
}

/**********************************************************************
 Search all forests via GC and return the results in an array of
 ADS_STRUCT/LDAPMessage pairs.
 *********************************************************************/

 NTSTATUS gc_search_all_forests_unique(const char *filter,
				       ADS_STRUCT **ads,
				       LDAPMessage **msg)
{
	ADS_STRUCT **ads_list = NULL;
	LDAPMessage **msg_list = NULL;
	int num_resp;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	nt_status = gc_search_all_forests(filter, &ads_list,
					  &msg_list, &num_resp,
					  ADEX_GC_SEARCH_CHECK_UNIQUE);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = check_result_unique(ads_list[0], msg_list[0]);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	*ads = ads_list[0];
	*msg = msg_list[0];

done:
	/* Be care that we don't free the msg result being returned */

	if (!NT_STATUS_IS_OK(nt_status)) {
		free_result_array(ads_list, msg_list, num_resp);
	} else {
		talloc_destroy(ads_list);
		talloc_destroy(msg_list);
	}

	return nt_status;
}

/*********************************************************************
 ********************************************************************/

 NTSTATUS gc_name_to_sid(const char *domain,
			 const char *name,
			 struct dom_sid *sid,
			 enum lsa_SidType *sid_type)
{
	TALLOC_CTX *frame = talloc_stackframe();
	char *p, *name_user;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *name_filter;
	ADS_STRUCT *ads = NULL;
	LDAPMessage *msg = NULL;
	LDAPMessage *e = NULL;
	char *dn = NULL;
	char *dns_domain = NULL;
	ADS_STRUCT **ads_list = NULL;
	LDAPMessage **msg_list = NULL;
	int num_resp = 0;
	int i;

	/* Strip the "DOMAIN\" prefix if necessary and search for
	   a matching sAMAccountName in the forest */

	if ((p = strchr_m( name, '\\' )) == NULL)
		name_user = talloc_strdup( frame, name );
	else
		name_user = talloc_strdup( frame, p+1 );
	BAIL_ON_PTR_ERROR(name_user, nt_status);

	name_filter = talloc_asprintf(frame, "(sAMAccountName=%s)", name_user);
	BAIL_ON_PTR_ERROR(name_filter, nt_status);

	nt_status = gc_search_all_forests(name_filter, &ads_list,
					  &msg_list, &num_resp, 0);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Assume failure until we know otherwise*/

	nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;

	/* Match the domain name from the DN */

	for (i=0; i<num_resp; i++) {
		ads = ads_list[i];
		msg = msg_list[i];

		e = ads_first_entry(ads, msg);
		while (e) {
			struct winbindd_tdc_domain *domain_rec;

			dn = ads_get_dn(ads, frame, e);
			BAIL_ON_PTR_ERROR(dn, nt_status);

			dns_domain = cell_dn_to_dns(dn);
			TALLOC_FREE(dn);
			BAIL_ON_PTR_ERROR(dns_domain, nt_status);

			domain_rec = wcache_tdc_fetch_domain(frame, dns_domain);
			SAFE_FREE(dns_domain);

			/* Ignore failures and continue the search */

			if (!domain_rec) {
				e = ads_next_entry(ads, e);
				continue;
			}

			/* Check for a match on the domain name */

			if (strequal(domain, domain_rec->domain_name)) {
				if (!ads_pull_sid(ads, e, "objectSid", sid)) {
					nt_status = NT_STATUS_INVALID_SID;
					BAIL_ON_NTSTATUS_ERROR(nt_status);
				}

				talloc_destroy(domain_rec);

				nt_status = get_sid_type(ads, msg, sid_type);
				BAIL_ON_NTSTATUS_ERROR(nt_status);

				/* We're done! */
				nt_status = NT_STATUS_OK;
				break;
			}

			/* once more around thew merry-go-round */

			talloc_destroy(domain_rec);
			e = ads_next_entry(ads, e);
		}
	}

done:
	free_result_array(ads_list, msg_list, num_resp);
	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 Pull an attribute string value
 *******************************************************************/

static NTSTATUS get_object_account_name(ADS_STRUCT *ads,
					LDAPMessage *msg,
					char **name)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *sam_name = NULL;
	struct winbindd_tdc_domain *domain_rec = NULL;
	char *dns_domain = NULL;
	char *dn = NULL;
	TALLOC_CTX *frame = talloc_stackframe();
	int len;

	/* Check parameters */

	if (!ads || !msg || !name) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* get the name and domain */

	dn = ads_get_dn(ads, frame, msg);
	BAIL_ON_PTR_ERROR(dn, nt_status);

	DEBUG(10,("get_object_account_name: dn = \"%s\"\n", dn));

	dns_domain = cell_dn_to_dns(dn);
	TALLOC_FREE(dn);
	BAIL_ON_PTR_ERROR(dns_domain, nt_status);

	domain_rec = wcache_tdc_fetch_domain(frame, dns_domain);
	SAFE_FREE(dns_domain);

	if (!domain_rec) {
		nt_status = NT_STATUS_TRUSTED_DOMAIN_FAILURE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	sam_name = ads_pull_string(ads, frame, msg, "sAMAccountName");
	BAIL_ON_PTR_ERROR(sam_name, nt_status);

	len = asprintf(name, "%s\\%s", domain_rec->domain_name, sam_name);
	if (len == -1) {
		*name = NULL;
		BAIL_ON_PTR_ERROR((*name), nt_status);
	}

	nt_status = NT_STATUS_OK;

done:
	talloc_destroy(frame);

	return nt_status;
}

/*********************************************************************
 ********************************************************************/

 NTSTATUS gc_sid_to_name(const struct dom_sid *sid,
			 char **name,
			 enum lsa_SidType *sid_type)
{
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char *filter;
	ADS_STRUCT *ads = NULL;
	LDAPMessage *msg = NULL;
	char *sid_string;

	*name = NULL;

	sid_string = ldap_encode_ndr_dom_sid(frame, sid);
	BAIL_ON_PTR_ERROR(sid_string, nt_status);

	filter = talloc_asprintf(frame, "(objectSid=%s)", sid_string);
	TALLOC_FREE(sid_string);
	BAIL_ON_PTR_ERROR(filter, nt_status);

	nt_status = gc_search_all_forests_unique(filter, &ads, &msg);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = get_object_account_name(ads, msg, name);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = get_sid_type(ads, msg, sid_type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	ads_msgfree(ads, msg);
	talloc_destroy(frame);

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

 NTSTATUS add_ads_result_to_array(ADS_STRUCT *ads,
				  LDAPMessage *msg,
				  ADS_STRUCT ***ads_list,
				  LDAPMessage ***msg_list,
				  int *size)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	ADS_STRUCT **ads_tmp = NULL;
	LDAPMessage **msg_tmp = NULL;
	int count = *size;

	if (!ads || !msg) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

#if 0
	/* Don't add a response with no entries */

	if (ads_count_replies(ads, msg) == 0) {
		return NT_STATUS_OK;
	}
#endif

	if (count == 0) {
		ads_tmp = TALLOC_ARRAY(NULL, ADS_STRUCT*, 1);
		BAIL_ON_PTR_ERROR(ads_tmp, nt_status);

		msg_tmp = TALLOC_ARRAY(NULL, LDAPMessage*, 1);
		BAIL_ON_PTR_ERROR(msg_tmp, nt_status);
	} else {
		ads_tmp = TALLOC_REALLOC_ARRAY(*ads_list, *ads_list, ADS_STRUCT*,
					       count+1);
		BAIL_ON_PTR_ERROR(ads_tmp, nt_status);

		msg_tmp = TALLOC_REALLOC_ARRAY(*msg_list, *msg_list, LDAPMessage*,
					       count+1);
		BAIL_ON_PTR_ERROR(msg_tmp, nt_status);
	}

	ads_tmp[count] = ads;
	msg_tmp[count] = msg;
	count++;

	*ads_list = ads_tmp;
	*msg_list = msg_tmp;
	*size = count;

	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_destroy(ads_tmp);
		talloc_destroy(msg_tmp);
	}

	return nt_status;
}

/**********************************************************************
 Frees search results.  Do not free the ads_list as these are
 references back to the GC search structures.
 *********************************************************************/

 void free_result_array(ADS_STRUCT **ads_list,
			LDAPMessage **msg_list,
			int num_resp)
{
	int i;

	for (i=0; i<num_resp; i++) {
		ads_msgfree(ads_list[i], msg_list[i]);
	}

	talloc_destroy(ads_list);
	talloc_destroy(msg_list);
}

/**********************************************************************
 Check that we have exactly one entry from the search
 *********************************************************************/

 NTSTATUS check_result_unique(ADS_STRUCT *ads, LDAPMessage *msg)
{
	NTSTATUS nt_status;
	int count;

	count = ads_count_replies(ads, msg);

	if (count <= 0) {
		nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	if (count > 1) {
		nt_status = NT_STATUS_DUPLICATE_NAME;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	nt_status = NT_STATUS_OK;

done:
	return nt_status;
}
