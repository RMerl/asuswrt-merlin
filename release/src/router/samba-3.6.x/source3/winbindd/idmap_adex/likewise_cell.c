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
#include "secrets.h"
#include "../libcli/security/dom_sid.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_IDMAP

static struct likewise_cell *_lw_cell_list = NULL;

/**********************************************************************
 Return the current HEAD of the list
 *********************************************************************/

 struct likewise_cell *cell_list_head(void)
{
	return _lw_cell_list;
}


/**********************************************************************
 *********************************************************************/

 void cell_destroy(struct likewise_cell *c)
{
	if (!c)
		return;

	if (c->conn)
		ads_destroy(&c->conn);

	talloc_destroy(c);
}

/**********************************************************************
 Free all cell entries and reset the list head to NULL
 *********************************************************************/

 void cell_list_destroy(void)
{
	struct likewise_cell *p = _lw_cell_list;

	while (p) {
		struct likewise_cell *q = p->next;

		cell_destroy(p);

		p = q;
	}

	_lw_cell_list = NULL;

	return;
}

/**********************************************************************
 Add a new cell structure to the list
 *********************************************************************/

 struct likewise_cell* cell_new(void)
{
	struct likewise_cell *c;

	/* Each cell struct is a TALLOC_CTX* */

	c = TALLOC_ZERO_P(NULL, struct likewise_cell);
	if (!c) {
		DEBUG(0,("cell_new: memory allocation failure!\n"));
		return NULL;
	}

	return c;
}

/**********************************************************************
 Add a new cell structure to the list
 *********************************************************************/

 bool cell_list_add(struct likewise_cell * cell)
{
	if (!cell) {
		return false;
	}

	/* Always add to the end */

	DLIST_ADD_END(_lw_cell_list, cell, struct likewise_cell *);

	return true;
}

/**********************************************************************
 Add a new cell structure to the list
 *********************************************************************/

 bool cell_list_remove(struct likewise_cell * cell)
{
	if (!cell) {
		return false;
	}

	/* Remove and drop the cell structure */

	DLIST_REMOVE(_lw_cell_list, cell);
	talloc_destroy(cell);

	return true;
}

/**********************************************************************
 Set the containing DNS domain for a cell
 *********************************************************************/

 void cell_set_dns_domain(struct likewise_cell *c, const char *dns_domain)
{
	c->dns_domain = talloc_strdup(c, dns_domain);
}

/**********************************************************************
 Set ADS connection for a cell
 *********************************************************************/

 void cell_set_connection(struct likewise_cell *c, ADS_STRUCT *ads)
{
	c->conn = ads;
}

/**********************************************************************
 *********************************************************************/

 void cell_set_flags(struct likewise_cell *c, uint32_t flags)
{
	c->flags |= flags;
}

/**********************************************************************
 *********************************************************************/

 void cell_clear_flags(struct likewise_cell *c, uint32_t flags)
{
	c->flags &= ~flags;
}

/**********************************************************************
 Set the Cell's DN
 *********************************************************************/

 void cell_set_dn(struct likewise_cell *c, const char *dn)
{
	if ( c->dn) {
		talloc_free(c->dn);
		c->dn = NULL;
	}

	c->dn = talloc_strdup(c, dn);
}

/**********************************************************************
 *********************************************************************/

 void cell_set_domain_sid(struct likewise_cell *c, struct dom_sid *sid)
{
	sid_copy(&c->domain_sid, sid);
}

/*
 * Query Routines
 */

/**********************************************************************
 *********************************************************************/

 const char* cell_search_base(struct likewise_cell *c)
{
	if (!c)
		return NULL;

	return talloc_asprintf(c, "cn=%s,%s", ADEX_CELL_RDN, c->dn);
}

/**********************************************************************
 *********************************************************************/

 bool cell_search_forest(struct likewise_cell *c)
{
	uint32_t test_flags = LWCELL_FLAG_SEARCH_FOREST;

	return ((c->flags & test_flags) == test_flags);
}

/**********************************************************************
 *********************************************************************/

 uint32_t cell_flags(struct likewise_cell *c)
{
	if (!c)
		return 0;

	return c->flags;
}

/**********************************************************************
 *********************************************************************/

 const char *cell_dns_domain(struct likewise_cell *c)
{
	if (!c)
		return NULL;

	return c->dns_domain;
}

/**********************************************************************
 *********************************************************************/

 ADS_STRUCT *cell_connection(struct likewise_cell *c)
{
	if (!c)
		return NULL;

	return c->conn;
}

/*
 * Connection functions
 */

/********************************************************************
 *******************************************************************/

 NTSTATUS cell_connect(struct likewise_cell *c)
{
	ADS_STRUCT *ads = NULL;
	ADS_STATUS ads_status;
	fstring dc_name;
	struct sockaddr_storage dcip;
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;

	/* have to at least have the AD domain name */

	if (!c->dns_domain) {
		nt_status = NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* clear out any old information */

	if (c->conn) {
		ads_destroy(&c->conn);
		c->conn = NULL;
	}

	/* now setup the new connection */

	ads = ads_init(c->dns_domain, NULL, NULL);
	BAIL_ON_PTR_ERROR(ads, nt_status);

	ads->auth.password =
	    secrets_fetch_machine_password(lp_workgroup(), NULL, NULL);
	ads->auth.realm = SMB_STRDUP(lp_realm());

	/* Make the connection.  We should already have an initial
	   TGT using the machine creds */

	if (cell_flags(c) & LWCELL_FLAG_GC_CELL) {
		ads_status = ads_connect_gc(ads);
	} else {
	  /* Set up server affinity for normal cells and the client
	     site name cache */

	  if (!get_dc_name("", c->dns_domain, dc_name, &dcip)) {
	    nt_status = NT_STATUS_DOMAIN_CONTROLLER_NOT_FOUND;
	    BAIL_ON_NTSTATUS_ERROR(nt_status);
	  }

	  ads_status = ads_connect(ads);
	}


	c->conn = ads;

	nt_status = ads_ntstatus(ads_status);

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		ads_destroy(&ads);
		c->conn = NULL;
	}

	return nt_status;
}

/********************************************************************
 *******************************************************************/

 NTSTATUS cell_connect_dn(struct likewise_cell **c, const char *dn)
{
	NTSTATUS nt_status = NT_STATUS_UNSUCCESSFUL;
	struct likewise_cell *new_cell = NULL;
	char *dns_domain = NULL;

	if (*c || !dn) {
		nt_status = NT_STATUS_INVALID_PARAMETER;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	if ((new_cell = cell_new()) == NULL) {
		nt_status = NT_STATUS_NO_MEMORY;
		BAIL_ON_NTSTATUS_ERROR(nt_status);
	}

	/* Set the DNS domain, dn, etc ... and add it to the list */

	dns_domain = cell_dn_to_dns(dn);
	cell_set_dns_domain(new_cell, dns_domain);
	SAFE_FREE(dns_domain);

	cell_set_dn(new_cell, dn);

	nt_status = cell_connect(new_cell);
	BAIL_ON_NTSTATUS_ERROR(nt_status);

	*c = new_cell;

done:
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(1,("LWI: Failled to connect to cell \"%s\" (%s)\n",
			 dn ? dn : "NULL", nt_errstr(nt_status)));
		talloc_destroy(new_cell);
	}

	return nt_status;
}


/********************************************************************
 *******************************************************************/

#define MAX_SEARCH_COUNT    2

 ADS_STATUS cell_do_search(struct likewise_cell *c,
			  const char *search_base,
			  int scope,
			  const char *expr,
			  const char **attrs,
			  LDAPMessage ** msg)
{
	int search_count = 0;
	ADS_STATUS status;
	NTSTATUS nt_status;

	/* check for a NULL connection */

	if (!c->conn) {
		nt_status = cell_connect(c);
		if (!NT_STATUS_IS_OK(nt_status)) {
			status = ADS_ERROR_NT(nt_status);
			return status;
		}
	}

	DEBUG(10, ("cell_do_search: Base = %s,  Filter = %s, Scope = %d, GC = %s\n",
		   search_base, expr, scope,
		   c->conn->server.gc ? "yes" : "no"));

	/* we try multiple times in case the ADS_STRUCT is bad
	   and we need to reconnect */

	while (search_count < MAX_SEARCH_COUNT) {
		*msg = NULL;
		status = ads_do_search(c->conn, search_base,
				       scope, expr, attrs, msg);
		if (ADS_ERR_OK(status)) {
			if (DEBUGLEVEL >= 10) {
				LDAPMessage *e = NULL;

				int n = ads_count_replies(c->conn, *msg);

				DEBUG(10,("cell_do_search: Located %d entries\n", n));

				for (e=ads_first_entry(c->conn, *msg);
				     e!=NULL;
				     e = ads_next_entry(c->conn, e))
				{
					char *dn = ads_get_dn(c->conn, talloc_tos(), e);

					DEBUGADD(10,("   dn: %s\n", dn ? dn : "<NULL>"));
					TALLOC_FREE(dn);
				}
			}

			return status;
		}


		DEBUG(5, ("cell_do_search: search[%d] failed (%s)\n",
			  search_count, ads_errstr(status)));

		search_count++;

		/* Houston, we have a problem */

		if (status.error_type == ENUM_ADS_ERROR_LDAP) {
			switch (status.err.rc) {
			case LDAP_TIMELIMIT_EXCEEDED:
			case LDAP_TIMEOUT:
			case -1:	/* we get this error if we cannot contact
					   the LDAP server */
				nt_status = cell_connect(c);
				if (!NT_STATUS_IS_OK(nt_status)) {
					status = ADS_ERROR_NT(nt_status);
					return status;
				}
				break;
			default:
				/* we're all done here */
				return status;
			}
		}
	}

	DEBUG(5, ("cell_do_search: exceeded maximum search count!\n"));

	return ADS_ERROR_NT(NT_STATUS_UNSUCCESSFUL);
}
