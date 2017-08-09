/*
 * idmap_adex
 *
 * Provider for RFC2307 and SFU AD Forests
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
#include "../libcli/ldap/ldap_ndr.h"
#include "../libcli/security/dom_sid.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

/* Information needed by the LDAP search filters */

enum filterType { SidFilter, IdFilter, AliasFilter };

struct lwcell_filter
{
	enum filterType ftype;
	bool use2307;
	union {
		struct dom_sid sid;
		struct {
			uint32_t id;
			enum id_type type;
		} id;
		fstring alias;
	} filter;
};

/********************************************************************
 *******************************************************************/

static char* build_id_filter(TALLOC_CTX *mem_ctx,
			     uint32_t id,
			     enum id_type type,
			     uint32_t search_flags)
{
	char *filter = NULL;
	char *oc_filter, *attr_filter;
	NTSTATUS nt_status;
	TALLOC_CTX *frame = talloc_stackframe();
	bool use2307 = ((search_flags & LWCELL_FLAG_USE_RFC2307_ATTRS)
			== LWCELL_FLAG_USE_RFC2307_ATTRS);
	bool use_gc = ((search_flags & LWCELL_FLAG_SEARCH_FOREST)
		       == LWCELL_FLAG_SEARCH_FOREST);
	const char *oc;

	/* Construct search filter for objectclass and attributes */

	switch (type) {
	case ID_TYPE_UID:
		oc = ADEX_OC_USER;
		if (use2307) {
			oc = ADEX_OC_POSIX_USER;
			if (use_gc) {
				oc = AD_USER;
			}
		}
		oc_filter = talloc_asprintf(frame, "objectclass=%s", oc);
		attr_filter = talloc_asprintf(frame, "%s=%u",
					      ADEX_ATTR_UIDNUM, id);
		break;

	case ID_TYPE_GID:
		oc = ADEX_OC_GROUP;
		if (use2307) {
			oc = ADEX_OC_POSIX_GROUP;
			if (use_gc) {
				oc = AD_GROUP;
			}
		}
		oc_filter = talloc_asprintf(frame, "objectclass=%s", oc);
		attr_filter = talloc_asprintf(frame, "%s=%u",
					      ADEX_ATTR_GIDNUM, id);
		break;
	default:
		return NULL;
	}

	BAIL_ON_PTR_ERROR(oc_filter, nt_status);
	BAIL_ON_PTR_ERROR(attr_filter, nt_status);

	/* Use "keywords=%s" for non-schema cells */

	if (use2307) {
		filter = talloc_asprintf(mem_ctx,
					 "(&(%s)(%s))",
					 oc_filter,
					 attr_filter);
	} else {
		filter = talloc_asprintf(mem_ctx,
					 "(&(keywords=%s)(keywords=%s))",
					 oc_filter,
					 attr_filter);
	}

done:
	talloc_destroy(frame);

	return filter;
}

/********************************************************************
 *******************************************************************/

static char* build_alias_filter(TALLOC_CTX *mem_ctx,
				const char *alias,
				uint32_t search_flags)
{
	char *filter = NULL;
	char *user_attr_filter, *group_attr_filter;
	NTSTATUS nt_status;
	TALLOC_CTX *frame = talloc_stackframe();
	bool use2307 = ((search_flags & LWCELL_FLAG_USE_RFC2307_ATTRS)
			== LWCELL_FLAG_USE_RFC2307_ATTRS);
	bool search_forest = ((search_flags & LWCELL_FLAG_SEARCH_FOREST)
			      == LWCELL_FLAG_SEARCH_FOREST);

	/* Construct search filter for objectclass and attributes */

	user_attr_filter = talloc_asprintf(frame, "%s=%s",
					   ADEX_ATTR_UID, alias);
	group_attr_filter = talloc_asprintf(frame, "%s=%s",
					    ADEX_ATTR_DISPLAYNAME, alias);
	BAIL_ON_PTR_ERROR(user_attr_filter, nt_status);
	BAIL_ON_PTR_ERROR(group_attr_filter, nt_status);

	/* Use "keywords=%s" for non-schema cells */

	if (use2307) {
		filter = talloc_asprintf(mem_ctx,
					 "(|(&(%s)(objectclass=%s))(&(%s)(objectclass=%s)))",
					 user_attr_filter,
					 search_forest ? AD_USER : ADEX_OC_POSIX_USER,
					 group_attr_filter,
					 search_forest ? AD_GROUP : ADEX_OC_POSIX_GROUP);
	} else {
		filter = talloc_asprintf(mem_ctx,
					 "(|(keywords=%s)(keywords=%s))",
					 user_attr_filter,
					 group_attr_filter);
	}

done:
	talloc_destroy(frame);

	return filter;
}


/********************************************************************
 *******************************************************************/

static NTSTATUS search_cell(struct likewise_cell *c,
			    LDAPMessage **msg,
			    const struct lwcell_filter *fdata)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX* frame = talloc_stackframe();
	char *filter = NULL;
	const char *base = NULL;
	ADS_STATUS ads_status = ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
        const char *attrs[] = { "*", NULL };
	int count;
	char *sid_str;

	/* get the filter and other search parameters */

	switch (fdata->ftype) {
	case SidFilter:
		sid_str = sid_string_talloc(frame, &fdata->filter.sid);
		BAIL_ON_PTR_ERROR(sid_str, nt_status);

		filter = talloc_asprintf(frame, "(keywords=backLink=%s)",
					 sid_str);
		break;
	case IdFilter:
		filter = build_id_filter(frame,
					 fdata->filter.id.id,
					 fdata->filter.id.type,
					 cell_flags(c));
		break;
	case AliasFilter:
		filter = build_alias_filter(frame,
					    fdata->filter.alias,
					    cell_flags(c));
		break;
	default:
		nt_status = NT_STATUS_INVALID_PARAMETER;
		break;
	}
	BAIL_ON_PTR_ERROR(filter, nt_status);

	base = cell_search_base(c);
	BAIL_ON_PTR_ERROR(base, nt_status);

	ads_status = cell_do_search(c, base, LDAP_SCOPE_SUBTREE,
				    filter, attrs, msg);

	nt_status = ads_ntstatus(ads_status);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Now check that we got only one reply */

	count = ads_count_replies(c->conn, *msg);
	if (count < 1) {
		nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	if ( count > 1) {
		nt_status = NT_STATUS_DUPLICATE_NAME;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

done:
	PRINT_NTSTATUS_ERROR(nt_status, "search_cell", 4);

	talloc_destroy(CONST_DISCARD(char*, base));
	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS search_domain(struct likewise_cell **cell,
			      LDAPMessage **msg,
			      const char *dn,
			      const struct dom_sid *sid)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX* frame = talloc_stackframe();
	int count;

	nt_status = dc_search_domains(cell, msg, dn, sid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Now check that we got only one reply */

	count = ads_count_replies(cell_connection(*cell), *msg);
	if (count < 1) {
		nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	if ( count > 1) {
		nt_status = NT_STATUS_DUPLICATE_NAME;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

done:
	PRINT_NTSTATUS_ERROR(nt_status, "search_domain", 4);
	talloc_destroy(frame);

	return nt_status;
}


/********************************************************************
 Check that a DN is within the forest scope.
 *******************************************************************/

static bool check_forest_scope(const char *dn)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *frame = talloc_stackframe();
	char *p = NULL;
	char *q = NULL;
	char *dns_domain = NULL;
	struct winbindd_tdc_domain *domain;

	/* If the DN does *not* contain "$LikewiseIdentityCell",
	   assume this is a schema mode forest and it is in the
	   forest scope by definition. */

	if ((p = strstr_m(dn, ADEX_CELL_RDN)) == NULL) {
		nt_status = NT_STATUS_OK;
		goto done;
	}

	/* If this is a non-schema forest, then make sure that the DN
	   is in the form "...,cn=$LikewiseIdentityCell,DC=..." */

	if ((q = strchr_m(p, ',')) == NULL) {
		nt_status = NT_STATUS_OBJECT_NAME_INVALID;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	q++;
	if (StrnCaseCmp(q, "dc=", 3) != 0) {
		nt_status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}


	dns_domain = cell_dn_to_dns(q);
	BAIL_ON_PTR_ERROR(dns_domain, nt_status);

	domain = wcache_tdc_fetch_domain(frame, dns_domain);
	if (!domain) {
		nt_status = NT_STATUS_TRUSTED_DOMAIN_FAILURE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	nt_status = NT_STATUS_OK;

done:
	talloc_destroy(frame);
	SAFE_FREE(dns_domain);

	return NT_STATUS_IS_OK(nt_status);
}



/********************************************************************
 Check that only one result was returned within the forest cell
 scope.
 *******************************************************************/

static NTSTATUS check_result_unique_scoped(ADS_STRUCT **ads_list,
					   LDAPMessage **msg_list,
					   int num_resp,
					   char **dn,
					   struct dom_sid *user_sid)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	int i;
	ADS_STRUCT *ads = NULL;
	LDAPMessage *msg = NULL;
	int count = 0;
	char *entry_dn = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!dn || !user_sid) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	*dn = NULL;

	if (!ads_list || !msg_list || (num_resp == 0)) {
		nt_status = NT_STATUS_NO_SUCH_FILE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Loop over all msgs */

	for (i=0; i<num_resp; i++) {
		LDAPMessage *e = ads_first_entry(ads_list[i], msg_list[i]);

		while (e) {
			entry_dn = ads_get_dn(ads_list[i], talloc_tos(), e);
			BAIL_ON_PTR_ERROR(entry_dn, nt_status);

			if (check_forest_scope(entry_dn)) {
				count++;

				/* If we've already broken the condition, no
				   need to continue */

				if (count > 1) {
					nt_status = NT_STATUS_DUPLICATE_NAME;
					BAIL_ON_NTSTATUS_ERROR(nt_status);
				}

				ads = ads_list[i];
				msg = e;
				*dn = SMB_STRDUP(entry_dn);
				BAIL_ON_PTR_ERROR((*dn), nt_status);
			}

			e = ads_next_entry(ads_list[i], e);
			TALLOC_FREE(entry_dn);
		}
	}

	if (!ads || !msg) {
		nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* If we made is through the loop, then grab the user_sid and
	   run home to base */

	/*
	   Try and get the SID from either objectSid or keywords.
	   We cannot use pull_sid() here since we want to try
	   both methods and not only one or the other (and we
	   have no full likewise_cell struct.

	   Fail if both are unavailable
	*/

	if (!ads_pull_sid(ads, msg, "objectSid", user_sid)) {
		char **keywords;
		char *s;
		size_t num_lines = 0;

		keywords = ads_pull_strings(ads, frame, msg, "keywords",
					    &num_lines);
		BAIL_ON_PTR_ERROR(keywords, nt_status);

		s = find_attr_string(keywords, num_lines, "backLink");
		if (!s) {
			nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}

		if (!string_to_sid(user_sid, s)) {
			nt_status = NT_STATUS_INVALID_SID;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		SAFE_FREE(*dn);
	}

	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 Search all forests.  Each forest can have it's own forest-cell
 settings so we have to generate the filter for each search.
 We don't use gc_search_all_forests() since we may have a different
 schema model in each forest and need to construct the search
 filter for each GC search.
 *******************************************************************/

static NTSTATUS search_forest(struct likewise_cell *forest_cell,
			      LDAPMessage **msg,
			      const struct lwcell_filter *fdata)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *frame = talloc_stackframe();
	char *filter = NULL;
	char *dn = NULL;
	struct gc_info *gc = NULL;
	ADS_STRUCT **ads_list = NULL;
	LDAPMessage **msg_list = NULL;
	int num_resp = 0;
	LDAPMessage *m;
	struct dom_sid user_sid;
	struct likewise_cell *domain_cell = NULL;

	if ((gc = gc_search_start()) == NULL) {
		nt_status = NT_STATUS_INVALID_DOMAIN_STATE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	while (gc) {
		char *sid_binstr = NULL;
		uint32_t flags = LWCELL_FLAG_SEARCH_FOREST;

		m = NULL;

		flags |= cell_flags(gc->forest_cell);

		switch (fdata->ftype) {
		case SidFilter:
			sid_binstr = ldap_encode_ndr_dom_sid(frame, &fdata->filter.sid);
			BAIL_ON_PTR_ERROR(sid_binstr, nt_status);

			filter = talloc_asprintf(frame, "(objectSid=%s)", sid_binstr);
			TALLOC_FREE(sid_binstr);
			break;
		case IdFilter:
			filter = build_id_filter(frame,
						 fdata->filter.id.id,
						 fdata->filter.id.type, flags);
			break;
		case AliasFilter:
			filter = build_alias_filter(frame,
						    fdata->filter.alias,
						    flags);
			break;
		}

		/* First find the sparse object in GC */
		nt_status = gc_search_forest(gc, &m, filter);
		if (!NT_STATUS_IS_OK(nt_status)) {
			gc = gc->next;
			continue;
		}

		nt_status = add_ads_result_to_array(cell_connection(gc->forest_cell),
						    m, &ads_list, &msg_list,
						    &num_resp);
		BAIL_ON_NTSTATUS_ERROR(nt_status);

		gc = gc->next;
	}

	/* Uniqueness check across forests */

	nt_status = check_result_unique_scoped(ads_list, msg_list, num_resp,
					       &dn, &user_sid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = search_domain(&domain_cell, &m, dn, &user_sid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Save the connection and results in the return parameters */

	forest_cell->gc_search_cell = domain_cell;
	*msg = m;

done:
	PRINT_NTSTATUS_ERROR(nt_status, "search_forest", 4);

	SAFE_FREE(dn);

	free_result_array(ads_list, msg_list, num_resp);
	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS search_cell_list(struct likewise_cell **c,
				 LDAPMessage **m,
				 const struct lwcell_filter *fdata)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct likewise_cell *cell = NULL;
	LDAPMessage *msg = NULL;
	struct likewise_cell *result_cell = NULL;

	if ((cell = cell_list_head()) == NULL) {
		nt_status = NT_STATUS_INVALID_SERVER_STATE;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	while (cell) {
		/* Clear any previous GC search results */

		cell->gc_search_cell = NULL;

		if (cell_search_forest(cell)) {
			nt_status = search_forest(cell, &msg, fdata);
		} else {
			nt_status = search_cell(cell, &msg, fdata);
		}

		/* Always point to the search result cell.
		   In forests this might be for another domain
		   which means the schema model may be different */

		result_cell = cell->gc_search_cell ?
			cell->gc_search_cell : cell;

		/* Check if we are done */

		if (NT_STATUS_IS_OK(nt_status)) {
			break;
		}

		/* No luck.  Free memory and hit the next cell.
		   Forest searches always set the gc_search_cell
		   so give preference to that connection if possible. */

		ads_msgfree(cell_connection(result_cell), msg);
		msg = NULL;

		cell = cell->next;
	}

	/* This might be assigning NULL but that is ok as long as we
	   give back the proper error code */

	*c = result_cell;
	*m = msg;

done:
	PRINT_NTSTATUS_ERROR(nt_status, "search_cell_list", 3);

	return nt_status;
}

/********************************************************************
 Pull the SID from an object which is always stored in the keywords
 attribute as "backLink=S-1-5-21-..."
 *******************************************************************/

static NTSTATUS pull_sid(struct likewise_cell *c,
			 LDAPMessage *msg,
			 struct dom_sid *sid)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	TALLOC_CTX *frame = talloc_stackframe();
	ADS_STRUCT *ads = NULL;

	ads = cell_connection(c);

	/*
	   We have two ways of getting the sid:
	   (a) from the objectSID in case of a GC search,
	   (b) from backLink in the case of a cell search.
	   Pull the keywords attributes and grab the backLink.
	*/

	if (!ads_pull_sid(ads, msg, "objectSid", sid)) {
		char **keywords;
		char *s;
		size_t num_lines = 0;

		keywords = ads_pull_strings(ads, frame, msg,
					    "keywords", &num_lines);
		BAIL_ON_PTR_ERROR(keywords, nt_status);

		s = find_attr_string(keywords, num_lines, "backLink");
		if (!s) {
			nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}

		if (!string_to_sid(sid, s)) {
			nt_status = NT_STATUS_INVALID_SID;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}
	}

	nt_status = NT_STATUS_OK;

done:
	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS get_object_type(struct likewise_cell *c,
				LDAPMessage *msg,
				enum id_type *type)
{
	TALLOC_CTX *ctx = talloc_stackframe();
	char **oc_list = NULL;
	NTSTATUS nt_status = NT_STATUS_OK;
	size_t list_size = 0;
	char *s = NULL;
	ADS_STRUCT *ads = NULL;

	ads = cell_connection(c);

	/* Deal with RFC 2307 support first */

	if (cell_flags(c) & LWCELL_FLAG_USE_RFC2307_ATTRS) {
		oc_list = ads_pull_strings(ads, ctx, msg,
					   "objectClass", &list_size);
		if (!oc_list) {
			nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto done;
		}

		/* Check for posix classes and AD classes */

		if (is_object_class(oc_list, list_size, ADEX_OC_POSIX_USER)
		    || is_object_class(oc_list, list_size, AD_USER)) {
			*type = ID_TYPE_UID;
		} else if (is_object_class(oc_list, list_size, ADEX_OC_POSIX_GROUP)
			   || is_object_class(oc_list, list_size, AD_GROUP)) {
			*type = ID_TYPE_GID;
		} else {
			*type = ID_TYPE_NOT_SPECIFIED;
			nt_status = NT_STATUS_INVALID_PARAMETER;
		}
	} else {
		/* Default to non-schema mode */

		oc_list = ads_pull_strings(ads, ctx, msg,
					   "keywords", &list_size);
		if (!oc_list) {
			nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto done;
		}

		s = find_attr_string(oc_list, list_size, "objectClass");
		if (!s) {
			nt_status = NT_STATUS_INTERNAL_DB_CORRUPTION;
			goto done;
		}

		if (strequal(s, ADEX_OC_USER)) {
			*type = ID_TYPE_UID;
		} else if (strequal(s, ADEX_OC_GROUP)) {
			*type = ID_TYPE_GID;
		} else {
			*type = ID_TYPE_NOT_SPECIFIED;
			nt_status = NT_STATUS_INVALID_PARAMETER;
		}
	}

	nt_status = NT_STATUS_OK;

done:
	talloc_destroy(ctx);

	return nt_status;
}

/********************************************************************
 Pull an attribute uint32_t  value
 *******************************************************************/

static NTSTATUS get_object_uint32(struct likewise_cell *c,
				  LDAPMessage *msg,
				  const char *attrib,
				  uint32_t *x)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char **keywords = NULL;
	size_t list_size = 0;
	TALLOC_CTX *frame = talloc_stackframe();
	ADS_STRUCT *ads = NULL;

	ads = cell_connection(c);

	/* Deal with RFC2307 schema */

	if (cell_flags(c) & LWCELL_FLAG_USE_RFC2307_ATTRS) {
		if (!ads_pull_uint32(ads, msg, attrib, x)) {
			nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}
	} else {
		/* Non-schema mode */
		char *s = NULL;
		uint32_t num;

		keywords = ads_pull_strings(ads, frame, msg, "keywords",
					    &list_size);
		BAIL_ON_PTR_ERROR(keywords, nt_status);

		s = find_attr_string(keywords, list_size, attrib);
		if (!s) {
			nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}

		num = strtoll(s, NULL, 10);
		if (errno == ERANGE) {
			nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}
		*x = num;
	}

	nt_status = NT_STATUS_OK;

done:
	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS get_object_id(struct likewise_cell *c,
			      LDAPMessage *msg,
			      enum id_type type,
			      uint32_t *id)
{
	NTSTATUS nt_status = NT_STATUS_OK;
	const char *id_attr;

	/* Figure out which attribute we need to pull */

	switch (type) {
	case ID_TYPE_UID:
		id_attr = ADEX_ATTR_UIDNUM;
		break;
	case ID_TYPE_GID:
		id_attr = ADEX_ATTR_GIDNUM;
		break;
	default:
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
		break;
	}

	nt_status = get_object_uint32(c, msg, id_attr, id);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	return nt_status;
}

/********************************************************************
 Pull the uid/gid and type from an object.  This differs depending on
 the cell flags.
 *******************************************************************/

static NTSTATUS pull_id(struct likewise_cell *c,
			LDAPMessage *msg,
			uint32_t *id,
			enum id_type *type)
{
	NTSTATUS nt_status;

	nt_status = get_object_type(c, msg, type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = get_object_id(c, msg, *type, id);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	return nt_status;
}

/********************************************************************
 Pull an attribute string value
 *******************************************************************/

static NTSTATUS get_object_string(struct likewise_cell *c,
				  LDAPMessage *msg,
				  TALLOC_CTX *ctx,
				  const char *attrib,
				  char **string)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	char **keywords = NULL;
	size_t list_size = 0;
	TALLOC_CTX *frame = talloc_stackframe();
	ADS_STRUCT *ads = NULL;

	*string = NULL;

	ads = cell_connection(c);

	/* Deal with RFC2307 schema */

	if (cell_flags(c) & LWCELL_FLAG_USE_RFC2307_ATTRS) {
		*string = ads_pull_string(ads, ctx, msg, attrib);
	} else {
		/* Non-schema mode */

		char *s = NULL;

		keywords = ads_pull_strings(ads, frame, msg,
					    "keywords", &list_size);
		if (!keywords) {
			nt_status = NT_STATUS_NO_MEMORY;
			BAIL_ON_NTSTATUS_ERROR(nt_status);
		}
		s = find_attr_string(keywords, list_size, attrib);
		if (s) {
			*string = talloc_strdup(ctx, s);
		}
	}

	if (!*string) {
		nt_status = NT_STATUS_OBJECT_NAME_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	nt_status = NT_STATUS_OK;

done:
	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 Pull the struct passwd fields for a user
 *******************************************************************/

static NTSTATUS pull_nss_info(struct likewise_cell *c,
			      LDAPMessage *msg,
			      TALLOC_CTX *ctx,
			      const char **homedir,
			      const char **shell,
			      const char **gecos,
			      gid_t *p_gid)
{
	NTSTATUS nt_status;
	char *tmp;

	nt_status = get_object_string(c, msg, ctx, ADEX_ATTR_HOMEDIR, &tmp);
	BAIL_ON_NTSTATUS_ERROR(nt_status);
	*homedir = tmp;

	nt_status = get_object_string(c, msg, ctx, ADEX_ATTR_SHELL, &tmp);
	BAIL_ON_NTSTATUS_ERROR(nt_status);
	*shell = tmp;

	nt_status = get_object_string(c, msg, ctx, ADEX_ATTR_GECOS, &tmp);
	/* Gecos is often not set so ignore failures */
	*gecos = tmp;

	nt_status = get_object_uint32(c, msg, ADEX_ATTR_GIDNUM, p_gid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	return nt_status;
}

/********************************************************************
 Pull the struct passwd fields for a user
 *******************************************************************/

static NTSTATUS pull_alias(struct likewise_cell *c,
			   LDAPMessage *msg,
			   TALLOC_CTX *ctx,
			   char **alias)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	enum id_type type;
	const char *attr = NULL;

	/* Figure out if this is a user or a group */

	nt_status = get_object_type(c, msg, &type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	switch (type) {
	case ID_TYPE_UID:
		attr = ADEX_ATTR_UID;
		break;
	case ID_TYPE_GID:
		/* What is the group attr for RFC2307 Forests? */
		attr = ADEX_ATTR_DISPLAYNAME;
		break;
	default:
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
		break;
	}

	nt_status = get_object_string(c, msg, ctx, attr, alias);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS _ccp_get_sid_from_id(struct dom_sid * sid,
				     uint32_t id, enum id_type type)
{
	struct likewise_cell *cell = NULL;
	LDAPMessage *msg = NULL;
	NTSTATUS nt_status;
	struct lwcell_filter filter;

	filter.ftype = IdFilter;
	filter.filter.id.id = id;
	filter.filter.id.type = type;

	nt_status = search_cell_list(&cell, &msg, &filter);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = pull_sid(cell, msg, sid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	ads_msgfree(cell->conn, msg);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS _ccp_get_id_from_sid(uint32_t * id,
				     enum id_type *type,
				     const struct dom_sid * sid)
{
	struct likewise_cell *cell = NULL;
	LDAPMessage *msg = NULL;
	NTSTATUS nt_status;
	struct lwcell_filter filter;

	filter.ftype = SidFilter;
	sid_copy(&filter.filter.sid, sid);

	nt_status = search_cell_list(&cell, &msg, &filter);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = pull_id(cell, msg, id, type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	if (*id < min_id_value()) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

done:
	ads_msgfree(cell->conn, msg);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

static NTSTATUS _ccp_nss_get_info(const struct dom_sid * sid,
				  TALLOC_CTX * ctx,
				  const char **homedir,
				  const char **shell,
				  const char **gecos, gid_t * p_gid)
{
	struct likewise_cell *cell = NULL;
	LDAPMessage *msg = NULL;
	NTSTATUS nt_status;
	struct lwcell_filter filter;
	enum id_type type;

	filter.ftype = SidFilter;
	sid_copy(&filter.filter.sid, sid);

	nt_status = search_cell_list(&cell, &msg, &filter);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = get_object_type(cell, msg, &type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	if (type != ID_TYPE_UID) {
		nt_status = NT_STATUS_NO_SUCH_USER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	nt_status = pull_nss_info(cell, msg, ctx, homedir, shell, gecos,
				  (uint32_t*) p_gid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	ads_msgfree(cell->conn, msg);

	return nt_status;
}

/**********************************************************************
 *********************************************************************/

static NTSTATUS _ccp_map_to_alias(TALLOC_CTX *ctx,
				  const char *domain,
				  const char *name, char **alias)
{
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct dom_sid sid;
	struct likewise_cell *cell = NULL;
	LDAPMessage *msg = NULL;
	struct lwcell_filter filter;
	enum lsa_SidType sid_type;

	/* Convert the name to a SID */

	nt_status = gc_name_to_sid(domain, name, &sid, &sid_type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Find the user/group */

	filter.ftype = SidFilter;
	sid_copy(&filter.filter.sid, &sid);

	nt_status = search_cell_list(&cell, &msg, &filter);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Pull the alias and return */

	nt_status = pull_alias(cell, msg, ctx, alias);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

done:
	PRINT_NTSTATUS_ERROR(nt_status, "map_to_alias", 3);

	talloc_destroy(frame);
	ads_msgfree(cell_connection(cell), msg);

	return nt_status;
}

/**********************************************************************
 Map from an alias name to the canonical, qualified name.
 Ensure that the alias is only pull from the closest in which
 the user or gorup is enabled in
 *********************************************************************/

static NTSTATUS _ccp_map_from_alias(TALLOC_CTX *mem_ctx,
				    const char *domain,
				    const char *alias, char **name)
{
	TALLOC_CTX *frame = talloc_stackframe();
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct dom_sid sid;
	struct likewise_cell *cell_alias = NULL;
	LDAPMessage *msg_alias = NULL;
	struct likewise_cell *cell_sid = NULL;
	LDAPMessage *msg_sid = NULL;
	struct lwcell_filter filter;
	char *canonical_name = NULL;
	enum lsa_SidType type;

	/* Find the user/group */

	filter.ftype = AliasFilter;
	fstrcpy(filter.filter.alias, alias);

	nt_status = search_cell_list(&cell_alias, &msg_alias, &filter);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	nt_status = pull_sid(cell_alias, msg_alias, &sid);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	/* Now search again for the SID according to the cell list.
	   Verify that the cell of both search results is the same
	   so that we only match an alias from the closest cell
	   in which a user/group has been instantied. */

	filter.ftype = SidFilter;
	sid_copy(&filter.filter.sid, &sid);

	nt_status = search_cell_list(&cell_sid, &msg_sid, &filter);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	if (cell_alias != cell_sid) {
		nt_status = NT_STATUS_OBJECT_PATH_NOT_FOUND;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Finally do the GC sid/name conversion */

	nt_status = gc_sid_to_name(&sid, &canonical_name, &type);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	*name = talloc_strdup(mem_ctx, canonical_name);
	BAIL_ON_PTR_ERROR((*name), nt_status);

	nt_status = NT_STATUS_OK;

done:
	PRINT_NTSTATUS_ERROR(nt_status, "map_from_alias", 3);

	ads_msgfree(cell_connection(cell_alias), msg_alias);
	ads_msgfree(cell_connection(cell_sid), msg_sid);

	SAFE_FREE(canonical_name);

	talloc_destroy(frame);

	return nt_status;
}

/********************************************************************
 *******************************************************************/

struct cell_provider_api ccp_unified = {
	.get_sid_from_id = _ccp_get_sid_from_id,
	.get_id_from_sid = _ccp_get_id_from_sid,
	.get_nss_info    = _ccp_nss_get_info,
	.map_to_alias    = _ccp_map_to_alias,
	.map_from_alias  = _ccp_map_from_alias
};
