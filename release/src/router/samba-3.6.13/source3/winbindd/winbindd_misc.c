/* 
   Unix SMB/CIFS implementation.

   Winbind daemon - miscellaneous other functions

   Copyright (C) Tim Potter      2000
   Copyright (C) Andrew Bartlett 2002

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
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

/* Constants and helper functions for determining domain trust types */

enum trust_type {
	EXTERNAL = 0,
	FOREST,
	IN_FOREST,
	NONE,
};

const char *trust_type_strings[] = {"External", 
				    "Forest", 
				    "In Forest",
				    "None"};

static enum trust_type get_trust_type(struct winbindd_tdc_domain *domain)
{
	if (domain->trust_attribs == NETR_TRUST_ATTRIBUTE_QUARANTINED_DOMAIN)	
		return EXTERNAL;
	else if (domain->trust_attribs == NETR_TRUST_ATTRIBUTE_FOREST_TRANSITIVE)
		return FOREST;
	else if (((domain->trust_flags & NETR_TRUST_FLAG_IN_FOREST) == NETR_TRUST_FLAG_IN_FOREST) &&
	    ((domain->trust_flags & NETR_TRUST_FLAG_PRIMARY) == 0x0))
		return IN_FOREST;
	return NONE;	
}

static const char *get_trust_type_string(struct winbindd_tdc_domain *domain)
{
	return trust_type_strings[get_trust_type(domain)];
}

static bool trust_is_inbound(struct winbindd_tdc_domain *domain)
{
	return (domain->trust_flags == 0x0) ||
	    ((domain->trust_flags & NETR_TRUST_FLAG_IN_FOREST) ==
            NETR_TRUST_FLAG_IN_FOREST) ||           		
	    ((domain->trust_flags & NETR_TRUST_FLAG_INBOUND) ==
	    NETR_TRUST_FLAG_INBOUND);      	
}

static bool trust_is_outbound(struct winbindd_tdc_domain *domain)
{
	return (domain->trust_flags == 0x0) ||
	    ((domain->trust_flags & NETR_TRUST_FLAG_IN_FOREST) ==
            NETR_TRUST_FLAG_IN_FOREST) ||           		
	    ((domain->trust_flags & NETR_TRUST_FLAG_OUTBOUND) ==
	    NETR_TRUST_FLAG_OUTBOUND);      	
}

static bool trust_is_transitive(struct winbindd_tdc_domain *domain)
{
	if ((domain->trust_attribs == NETR_TRUST_ATTRIBUTE_NON_TRANSITIVE) ||         
	    (domain->trust_attribs == NETR_TRUST_ATTRIBUTE_QUARANTINED_DOMAIN) ||
	    (domain->trust_attribs == NETR_TRUST_ATTRIBUTE_TREAT_AS_EXTERNAL))
		return False;
	return True;
}

void winbindd_list_trusted_domains(struct winbindd_cli_state *state)
{
	struct winbindd_tdc_domain *dom_list = NULL;
	struct winbindd_tdc_domain *d = NULL;
	size_t num_domains = 0;
	int extra_data_len = 0;
	char *extra_data = NULL;
	int i = 0;

	DEBUG(3, ("[%5lu]: list trusted domains\n",
		  (unsigned long)state->pid));

	if( !wcache_tdc_fetch_list( &dom_list, &num_domains )) {
		request_error(state);	
		goto done;
	}

	extra_data = talloc_strdup(state->mem_ctx, "");
	if (extra_data == NULL) {
		request_error(state);
		goto done;
	}

	for ( i = 0; i < num_domains; i++ ) {
		struct winbindd_domain *domain;
		bool is_online = true;		

		d = &dom_list[i];
		domain = find_domain_from_name_noinit(d->domain_name);
		if (domain) {
			is_online = domain->online;
		}
		extra_data = talloc_asprintf_append_buffer(
			extra_data,
			"%s\\%s\\%s\\%s\\%s\\%s\\%s\\%s\n",
			d->domain_name,
			d->dns_name ? d->dns_name : d->domain_name,
			sid_string_talloc(state->mem_ctx, &d->sid),
			get_trust_type_string(d),
			trust_is_transitive(d) ? "Yes" : "No",
			trust_is_inbound(d) ? "Yes" : "No",
			trust_is_outbound(d) ? "Yes" : "No",
			is_online ? "Online" : "Offline" );
	}

	state->response->data.num_entries = num_domains;

	extra_data_len = strlen(extra_data);
	if (extra_data_len > 0) {

		/* Strip the last \n */
		extra_data[extra_data_len-1] = '\0';

		state->response->extra_data.data = extra_data;
		state->response->length += extra_data_len;
	}

	request_ok(state);	
done:
	TALLOC_FREE( dom_list );
}

enum winbindd_result winbindd_dual_list_trusted_domains(struct winbindd_domain *domain,
							struct winbindd_cli_state *state)
{
	int i;
	int extra_data_len = 0;
	char *extra_data;
	NTSTATUS result;
	bool have_own_domain = False;
	struct netr_DomainTrustList trusts;

	DEBUG(3, ("[%5lu]: list trusted domains\n",
		  (unsigned long)state->pid));

	result = domain->methods->trusted_domains(domain, state->mem_ctx,
						  &trusts);

	if (!NT_STATUS_IS_OK(result)) {
		DEBUG(3, ("winbindd_dual_list_trusted_domains: trusted_domains returned %s\n",
			nt_errstr(result) ));
		return WINBINDD_ERROR;
	}

	extra_data = talloc_strdup(state->mem_ctx, "");

	for (i=0; i<trusts.count; i++) {
		extra_data = talloc_asprintf_append_buffer(
			extra_data, "%s\\%s\\%s\n",
			trusts.array[i].netbios_name,
			trusts.array[i].dns_name,
			sid_string_talloc(state->mem_ctx,
					  trusts.array[i].sid));
	}

	/* add our primary domain */

	for (i=0; i<trusts.count; i++) {
		if (strequal(trusts.array[i].netbios_name, domain->name)) {
			have_own_domain = True;
			break;
		}
	}

	if (state->request->data.list_all_domains && !have_own_domain) {
		extra_data = talloc_asprintf_append_buffer(
			extra_data, "%s\\%s\\%s\n", domain->name,
			domain->alt_name ? domain->alt_name : domain->name,
			sid_string_talloc(state->mem_ctx, &domain->sid));
	}

	extra_data_len = strlen(extra_data);
	if (extra_data_len > 0) {

		/* Strip the last \n */
		extra_data[extra_data_len-1] = '\0';

		state->response->extra_data.data = extra_data;
		state->response->length += extra_data_len+1;
	}

	return WINBINDD_OK;
}

struct domain_info_state {
	struct winbindd_domain *domain;
	struct winbindd_cli_state *cli;
	struct winbindd_request ping_request;
};

static void domain_info_done(struct tevent_req *req);

void winbindd_domain_info(struct winbindd_cli_state *cli)
{
	struct domain_info_state *state;
	struct winbindd_domain *domain;
	struct tevent_req *req;

	DEBUG(3, ("[%5lu]: domain_info [%s]\n", (unsigned long)cli->pid,
		  cli->request->domain_name));

	domain = find_domain_from_name_noinit(cli->request->domain_name);

	if (domain == NULL) {
		DEBUG(3, ("Did not find domain [%s]\n",
			  cli->request->domain_name));
		request_error(cli);
		return;
	}

	if (domain->initialized) {
		fstrcpy(cli->response->data.domain_info.name,
			domain->name);
		fstrcpy(cli->response->data.domain_info.alt_name,
			domain->alt_name);
		sid_to_fstring(cli->response->data.domain_info.sid,
			       &domain->sid);
		cli->response->data.domain_info.native_mode =
			domain->native_mode;
		cli->response->data.domain_info.active_directory =
			domain->active_directory;
		cli->response->data.domain_info.primary =
			domain->primary;
		request_ok(cli);
		return;
	}

	state = talloc_zero(cli->mem_ctx, struct domain_info_state);
	if (state == NULL) {
		DEBUG(0, ("talloc failed\n"));
		request_error(cli);
		return;
	}

	state->cli = cli;
	state->domain = domain;
	state->ping_request.cmd = WINBINDD_PING;

	/*
	 * Send a ping down. This implicitly initializes the domain.
	 */

	req = wb_domain_request_send(state, winbind_event_context(),
				     domain, &state->ping_request);
	if (req == NULL) {
		DEBUG(3, ("wb_domain_request_send failed\n"));
		request_error(cli);
		return;
	}
	tevent_req_set_callback(req, domain_info_done, state);
}

static void domain_info_done(struct tevent_req *req)
{
	struct domain_info_state *state = tevent_req_callback_data(
		req, struct domain_info_state);
	struct winbindd_response *response;
	int ret, err;

	ret = wb_domain_request_recv(req, req, &response, &err);
	TALLOC_FREE(req);
	if (ret == -1) {
		DEBUG(10, ("wb_domain_request failed: %s\n", strerror(errno)));
		request_error(state->cli);
		return;
	}
	if (!state->domain->initialized) {
		DEBUG(5, ("wb_domain_request did not initialize domain %s\n",
			  state->domain->name));
		request_error(state->cli);
		return;
	}

	fstrcpy(state->cli->response->data.domain_info.name,
		state->domain->name);
	fstrcpy(state->cli->response->data.domain_info.alt_name,
		state->domain->alt_name);
	sid_to_fstring(state->cli->response->data.domain_info.sid,
		       &state->domain->sid);

	state->cli->response->data.domain_info.native_mode =
		state->domain->native_mode;
	state->cli->response->data.domain_info.active_directory =
		state->domain->active_directory;
	state->cli->response->data.domain_info.primary =
		state->domain->primary;

	request_ok(state->cli);
}

void winbindd_dc_info(struct winbindd_cli_state *cli)
{
	struct winbindd_domain *domain;
	char *dc_name, *dc_ip;

	cli->request->domain_name[sizeof(cli->request->domain_name)-1] = '\0';

	DEBUG(3, ("[%5lu]: domain_info [%s]\n", (unsigned long)cli->pid,
		  cli->request->domain_name));

	if (cli->request->domain_name[0] != '\0') {
		domain = find_domain_from_name_noinit(
			cli->request->domain_name);
		DEBUG(10, ("Could not find domain %s\n",
			   cli->request->domain_name));
		if (domain == NULL) {
			request_error(cli);
			return;
		}
	} else {
		domain = find_our_domain();
	}

	if (!fetch_current_dc_from_gencache(
		    talloc_tos(), domain->name, &dc_name, &dc_ip)) {
		DEBUG(10, ("fetch_current_dc_from_gencache(%s) failed\n",
			   domain->name));
		request_error(cli);
		return;
	}

	cli->response->data.num_entries = 1;
	cli->response->extra_data.data = talloc_asprintf(
		cli->mem_ctx, "%s\n%s\n", dc_name, dc_ip);

	TALLOC_FREE(dc_name);
	TALLOC_FREE(dc_ip);

	if (cli->response->extra_data.data == NULL) {
		request_error(cli);
		return;
	}

	/* must add one to length to copy the 0 for string termination */
	cli->response->length +=
		strlen((char *)cli->response->extra_data.data) + 1;

	request_ok(cli);
}

/* List various tidbits of information */

void winbindd_info(struct winbindd_cli_state *state)
{

	DEBUG(3, ("[%5lu]: request misc info\n", (unsigned long)state->pid));

	state->response->data.info.winbind_separator = *lp_winbind_separator();
	fstrcpy(state->response->data.info.samba_version, samba_version_string());
	request_ok(state);
}

/* Tell the client the current interface version */

void winbindd_interface_version(struct winbindd_cli_state *state)
{
	DEBUG(3, ("[%5lu]: request interface version\n",
		  (unsigned long)state->pid));

	state->response->data.interface_version = WINBIND_INTERFACE_VERSION;
	request_ok(state);
}

/* What domain are we a member of? */

void winbindd_domain_name(struct winbindd_cli_state *state)
{
	DEBUG(3, ("[%5lu]: request domain name\n", (unsigned long)state->pid));

	fstrcpy(state->response->data.domain_name, lp_workgroup());
	request_ok(state);
}

/* What's my name again? */

void winbindd_netbios_name(struct winbindd_cli_state *state)
{
	DEBUG(3, ("[%5lu]: request netbios name\n",
		  (unsigned long)state->pid));

	fstrcpy(state->response->data.netbios_name, global_myname());
	request_ok(state);
}

/* Where can I find the privileged pipe? */

void winbindd_priv_pipe_dir(struct winbindd_cli_state *state)
{
	char *priv_dir;
	DEBUG(3, ("[%5lu]: request location of privileged pipe\n",
		  (unsigned long)state->pid));

	priv_dir = get_winbind_priv_pipe_dir();
	state->response->extra_data.data = talloc_move(state->mem_ctx,
						      &priv_dir);

	/* must add one to length to copy the 0 for string termination */
	state->response->length +=
		strlen((char *)state->response->extra_data.data) + 1;

	request_ok(state);
}

