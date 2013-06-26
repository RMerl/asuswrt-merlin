/*
 *  Unix SMB/CIFS implementation.
 *  winbindd debug helper
 *  Copyright (C) Guenther Deschner 2008
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
#include "winbindd.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../librpc/gen_ndr/ndr_security.h"
#include "librpc/ndr/util.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

/****************************************************************
****************************************************************/

void ndr_print_winbindd_child(struct ndr_print *ndr,
			      const char *name,
			      const struct winbindd_child *r)
{
	ndr_print_struct(ndr, name, "winbindd_child");
	ndr->depth++;
	ndr_print_ptr(ndr, "next", r->next);
	ndr_print_ptr(ndr, "prev", r->prev);
	ndr_print_uint32(ndr, "pid", (uint32_t)r->pid);
#if 0
	ndr_print_winbindd_domain(ndr, "domain", r->domain);
#else
	ndr_print_ptr(ndr, "domain", r->domain);
#endif
	ndr_print_string(ndr, "logfilename", r->logfilename);
	/* struct fd_event event; */
	ndr_print_ptr(ndr, "lockout_policy_event", r->lockout_policy_event);
	ndr_print_ptr(ndr, "table", r->table);
	ndr->depth--;
}

/****************************************************************
****************************************************************/

void ndr_print_winbindd_cm_conn(struct ndr_print *ndr,
				const char *name,
				const struct winbindd_cm_conn *r)
{
	ndr_print_struct(ndr, name, "winbindd_cm_conn");
	ndr->depth++;
	ndr_print_ptr(ndr, "cli", r->cli);
	ndr_print_ptr(ndr, "samr_pipe", r->samr_pipe);
	ndr_print_policy_handle(ndr, "sam_connect_handle", &r->sam_connect_handle);
	ndr_print_policy_handle(ndr, "sam_domain_handle", &r->sam_domain_handle);
	ndr_print_ptr(ndr, "lsa_pipe", r->lsa_pipe);
	ndr_print_policy_handle(ndr, "lsa_policy", &r->lsa_policy);
	ndr_print_ptr(ndr, "netlogon_pipe", r->netlogon_pipe);
	ndr->depth--;
}

/****************************************************************
****************************************************************/

#ifdef HAVE_ADS
extern struct winbindd_methods ads_methods;
#endif
extern struct winbindd_methods msrpc_methods;
extern struct winbindd_methods builtin_passdb_methods;
extern struct winbindd_methods sam_passdb_methods;
extern struct winbindd_methods reconnect_methods;
extern struct winbindd_methods cache_methods;

void ndr_print_winbindd_methods(struct ndr_print *ndr,
				const char *name,
				const struct winbindd_methods *r)
{
	ndr_print_struct(ndr, name, "winbindd_methods");
	ndr->depth++;

	if (r == NULL) {
		ndr_print_string(ndr, name, "(NULL)");
		ndr->depth--;
		return;
	}

	if (r == &msrpc_methods) {
		ndr_print_string(ndr, name, "msrpc_methods");
#ifdef HAVE_ADS
	} else if (r == &ads_methods) {
		ndr_print_string(ndr, name, "ads_methods");
#endif
	} else if (r == &builtin_passdb_methods) {
		ndr_print_string(ndr, name, "builtin_passdb_methods");
	} else if (r == &sam_passdb_methods) {
		ndr_print_string(ndr, name, "sam_passdb_methods");
	} else if (r == &reconnect_methods) {
		ndr_print_string(ndr, name, "reconnect_methods");
	} else if (r == &cache_methods) {
		ndr_print_string(ndr, name, "cache_methods");
	} else {
		ndr_print_string(ndr, name, "UNKNOWN");
	}
	ndr->depth--;
}

/****************************************************************
****************************************************************/

void ndr_print_winbindd_domain(struct ndr_print *ndr,
			       const char *name,
			       const struct winbindd_domain *r)
{
	int i;
	if (!r) {
		return;
	}

	ndr_print_struct(ndr, name, "winbindd_domain");
	ndr->depth++;
	ndr_print_string(ndr, "name", r->name);
	ndr_print_string(ndr, "alt_name", r->alt_name);
	ndr_print_string(ndr, "forest_name", r->forest_name);
	ndr_print_dom_sid(ndr, "sid", &r->sid);
	ndr_print_netr_TrustFlags(ndr, "domain_flags", r->domain_flags);
	ndr_print_netr_TrustType(ndr, "domain_type", r->domain_type);
	ndr_print_netr_TrustAttributes(ndr, "domain_trust_attribs", r->domain_trust_attribs);
	ndr_print_bool(ndr, "initialized", r->initialized);
	ndr_print_bool(ndr, "native_mode", r->native_mode);
	ndr_print_bool(ndr, "active_directory", r->active_directory);
	ndr_print_bool(ndr, "primary", r->primary);
	ndr_print_bool(ndr, "internal", r->internal);
	ndr_print_bool(ndr, "online", r->online);
	ndr_print_time_t(ndr, "startup_time", r->startup_time);
	ndr_print_bool(ndr, "startup", r->startup);
	ndr_print_winbindd_methods(ndr, "methods", r->methods);
	ndr_print_winbindd_methods(ndr, "backend", r->backend);
	ndr_print_ptr(ndr, "private_data", r->private_data);
	ndr_print_string(ndr, "dcname", r->dcname);
	ndr_print_sockaddr_storage(ndr, "dcaddr", &r->dcaddr);
	ndr_print_time_t(ndr, "last_seq_check", r->last_seq_check);
	ndr_print_uint32(ndr, "sequence_number", r->sequence_number);
	ndr_print_NTSTATUS(ndr, "last_status", r->last_status);
	ndr_print_winbindd_cm_conn(ndr, "conn", &r->conn);
	for (i=0; i<lp_winbind_max_domain_connections(); i++) {
		ndr_print_winbindd_child(ndr, "children", &r->children[i]);
	}
	ndr_print_uint32(ndr, "check_online_timeout", r->check_online_timeout);
	ndr_print_ptr(ndr, "check_online_event", r->check_online_event);
	ndr->depth--;
}
