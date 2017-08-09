/*
   Unix SMB/CIFS implementation.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-2003
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002

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

   Revision History:

*/

#include "includes.h"
#include "../libcli/netlogon/netlogon.h"
#include "../libcli/cldap/cldap.h"
#include "../lib/tsocket/tsocket.h"
#include "../libcli/security/security.h"
#include "secrets.h"
#include "nmbd/nmbd.h"

struct sam_database_info {
        uint32 index;
        uint32 serial_lo, serial_hi;
        uint32 date_lo, date_hi;
};

/**
 * check whether the client belongs to the hosts
 * for which initial logon should be delayed...
 */
static bool delay_logon(const char *peer_name, const char *peer_addr)
{
	const char **delay_list = lp_init_logon_delayed_hosts();
	const char *peer[2];

	if (delay_list == NULL) {
		return False;
	}

	peer[0] = peer_name;
	peer[1] = peer_addr;

	return list_match(delay_list, (const char *)peer, client_match);
}

static void delayed_init_logon_handler(struct event_context *event_ctx,
				       struct timed_event *te,
				       struct timeval now,
				       void *private_data)
{
	struct packet_struct *p = (struct packet_struct *)private_data;

	DEBUG(10, ("delayed_init_logon_handler (%lx): re-queuing packet.\n",
		   (unsigned long)te));

	queue_packet(p);

	TALLOC_FREE(te);
}

struct nmbd_proxy_logon_context {
	struct cldap_socket *cldap_sock;
};

static struct nmbd_proxy_logon_context *global_nmbd_proxy_logon;

bool initialize_nmbd_proxy_logon(void)
{
	const char *cldap_server = lp_parm_const_string(-1, "nmbd_proxy_logon",
						        "cldap_server", NULL);
	struct nmbd_proxy_logon_context *ctx;
	NTSTATUS status;
	struct in_addr addr;
	char addrstr[INET_ADDRSTRLEN];
	const char *server_str;
	int ret;
	struct tsocket_address *server_addr;

	if (!cldap_server) {
		return true;
	}

	addr = interpret_addr2(cldap_server);
	server_str = inet_ntop(AF_INET, &addr,
			     addrstr, sizeof(addrstr));
	if (!server_str || strcmp("0.0.0.0", server_str) == 0) {
		DEBUG(0,("Failed to resolve[%s] for nmbd_proxy_logon\n",
			 cldap_server));
		return false;
	}

	ctx = talloc_zero(nmbd_event_context(),
			  struct nmbd_proxy_logon_context);
	if (!ctx) {
		return false;
	}

	ret = tsocket_address_inet_from_strings(ctx, "ipv4",
						server_str, LDAP_PORT,
						&server_addr);
	if (ret != 0) {
		TALLOC_FREE(ctx);
		status = map_nt_error_from_unix(errno);
		DEBUG(0,("Failed to create cldap tsocket_address for %s - %s\n",
			 server_str, nt_errstr(status)));
		return false;
	}

	/* we create a connected udp socket */
	status = cldap_socket_init(ctx, nmbd_event_context(), NULL,
				   server_addr, &ctx->cldap_sock);
	TALLOC_FREE(server_addr);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(ctx);
		DEBUG(0,("failed to create cldap socket for %s: %s\n",
			server_str, nt_errstr(status)));
		return false;
	}

	global_nmbd_proxy_logon = ctx;
	return true;
}

struct nmbd_proxy_logon_state {
	struct in_addr local_ip;
	struct packet_struct *p;
	const char *remote_name;
	uint8_t remote_name_type;
	const char *remote_mailslot;
	struct nbt_netlogon_packet req;
	struct nbt_netlogon_response resp;
	struct cldap_netlogon io;
};

static int nmbd_proxy_logon_state_destructor(struct nmbd_proxy_logon_state *s)
{
	s->p->locked = false;
	free_packet(s->p);
	return 0;
}

static void nmbd_proxy_logon_done(struct tevent_req *subreq);

static void nmbd_proxy_logon(struct nmbd_proxy_logon_context *ctx,
			     struct in_addr local_ip,
			     struct packet_struct *p,
			     uint8_t *buf,
			     uint32_t len)
{
	struct nmbd_proxy_logon_state *state;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob = data_blob_const(buf, len);
	const char *computer_name = NULL;
	const char *mailslot_name = NULL;
	const char *user_name = NULL;
	const char *domain_sid = NULL;
	uint32_t acct_control = 0;
	uint32_t nt_version = 0;
	struct tevent_req *subreq;
	fstring source_name;
	struct dgram_packet *dgram = &p->packet.dgram;

	state = TALLOC_ZERO_P(ctx, struct nmbd_proxy_logon_state);
	if (!state) {
		DEBUG(0,("failed to allocate nmbd_proxy_logon_state\n"));
		return;
	}

	pull_ascii_nstring(source_name, sizeof(source_name), dgram->source_name.name);
	state->remote_name = talloc_strdup(state, source_name);
	state->remote_name_type = dgram->source_name.name_type,
	state->local_ip = local_ip;
	state->p = p;

	ndr_err = ndr_pull_struct_blob(
		&blob, state, &state->req,
		(ndr_pull_flags_fn_t)ndr_pull_nbt_netlogon_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("failed parse nbt_netlogon_packet: %s\n",
			nt_errstr(status)));
		TALLOC_FREE(state);
		return;
	}

	if (DEBUGLEVEL >= 10) {
		DEBUG(10, ("nmbd_proxy_logon:\n"));
		NDR_PRINT_DEBUG(nbt_netlogon_packet, &state->req);
	}

	switch (state->req.command) {
	case LOGON_SAM_LOGON_REQUEST:
		computer_name	= state->req.req.logon.computer_name;
		user_name	= state->req.req.logon.user_name;
		mailslot_name	= state->req.req.logon.mailslot_name;
		acct_control	= state->req.req.logon.acct_control;
		if (state->req.req.logon.sid_size > 0) {
			domain_sid = dom_sid_string(state,
						    &state->req.req.logon.sid);
			if (!domain_sid) {
				DEBUG(0,("failed to get a string for sid\n"));
				TALLOC_FREE(state);
				return;
			}
		}
		nt_version	= state->req.req.logon.nt_version;
		break;

	default:
		/* this can't happen as the caller already checks the command */
		break;
	}

	state->remote_mailslot = mailslot_name;

	if (user_name && strlen(user_name) == 0) {
		user_name = NULL;
	}

	if (computer_name && strlen(computer_name) == 0) {
		computer_name = NULL;
	}

	/*
	 * as the socket is connected,
	 * we don't need to specify the destination
	 */
	state->io.in.dest_address	= NULL;
	state->io.in.dest_port		= 0;
	state->io.in.realm		= NULL;
	state->io.in.host		= computer_name;
	state->io.in.user		= user_name;
	state->io.in.domain_guid	= NULL;
	state->io.in.domain_sid		= domain_sid;
	state->io.in.acct_control	= acct_control;
	state->io.in.version		= nt_version;
	state->io.in.map_response	= false;

	subreq = cldap_netlogon_send(state,
				     ctx->cldap_sock,
				     &state->io);
	if (!subreq) {
		DEBUG(0,("failed to send cldap netlogon call\n"));
		TALLOC_FREE(state);
		return;
	}
	tevent_req_set_callback(subreq, nmbd_proxy_logon_done, state);

	/* we reply async */
	state->p->locked = true;
	talloc_set_destructor(state, nmbd_proxy_logon_state_destructor);
}

static void nmbd_proxy_logon_done(struct tevent_req *subreq)
{
	struct nmbd_proxy_logon_state *state =
		tevent_req_callback_data(subreq,
		struct nmbd_proxy_logon_state);
	NTSTATUS status;
	DATA_BLOB response = data_blob_null;

	status = cldap_netlogon_recv(subreq, state, &state->io);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("failed to recv cldap netlogon call: %s\n",
			nt_errstr(status)));
		TALLOC_FREE(state);
		return;
	}

	status = push_netlogon_samlogon_response(&response, state,
						 &state->io.out.netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("failed to push netlogon_samlogon_response: %s\n",
			nt_errstr(status)));
		TALLOC_FREE(state);
		return;
	}

	send_mailslot(true, state->remote_mailslot,
		      (char *)response.data, response.length,
		      global_myname(), 0x0,
		      state->remote_name,
		      state->remote_name_type,
		      state->p->ip,
		      state->local_ip,
		      state->p->port);
	TALLOC_FREE(state);
}

/****************************************************************************
Process a domain logon packet
**************************************************************************/

void process_logon_packet(struct packet_struct *p, const char *buf,int len,
                          const char *mailslot)
{
	fstring source_name;
	struct dgram_packet *dgram = &p->packet.dgram;
	struct sockaddr_storage ss;
	const struct sockaddr_storage *pss;
	struct in_addr ip;

	DATA_BLOB blob_in, blob_out;
	enum ndr_err_code ndr_err;
	struct nbt_netlogon_packet request;
	struct nbt_netlogon_response response;
	NTSTATUS status;
	const char *pdc_name;

#ifndef NETLOGON_SUPPORT
	return;
#endif

	in_addr_to_sockaddr_storage(&ss, p->ip);
	pss = iface_ip((struct sockaddr *)&ss);
	if (!pss) {
		DEBUG(5,("process_logon_packet:can't find outgoing interface "
			"for packet from IP %s\n",
			inet_ntoa(p->ip) ));
		return;
	}
	ip = ((struct sockaddr_in *)pss)->sin_addr;

	if (!lp_domain_logons()) {
		DEBUG(5,("process_logon_packet: Logon packet received from IP %s and domain \
logons are not enabled.\n", inet_ntoa(p->ip) ));
		return;
	}

	pull_ascii_nstring(source_name, sizeof(source_name), dgram->source_name.name);

	pdc_name = talloc_asprintf(talloc_tos(), "\\\\%s", global_myname());
	if (!pdc_name) {
		return;
	}

	ZERO_STRUCT(request);

	blob_in = data_blob_const(buf, len);

	ndr_err = ndr_pull_struct_blob(&blob_in, talloc_tos(), &request,
		(ndr_pull_flags_fn_t)ndr_pull_nbt_netlogon_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("process_logon_packet: Failed to pull logon packet\n"));
		return;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(nbt_netlogon_packet, &request);
	}

	DEBUG(4,("process_logon_packet: Logon from %s: code = 0x%x\n",
		inet_ntoa(p->ip), request.command));

	switch (request.command) {
	case LOGON_REQUEST: {

		struct nbt_netlogon_response2 response2;

		DEBUG(5,("process_logon_packet: Domain login request from %s at IP %s user=%s token=%x\n",
			request.req.logon0.computer_name, inet_ntoa(p->ip),
			request.req.logon0.user_name,
			request.req.logon0.lm20_token));

		response2.command	= LOGON_RESPONSE2;
		response2.pdc_name	= pdc_name;
		response2.lm20_token	= 0xffff;

		response.response_type = NETLOGON_RESPONSE2;
		response.data.response2 = response2;

		status = push_nbt_netlogon_response(&blob_out, talloc_tos(), &response);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("process_logon_packet: failed to push packet\n"));
			return;
		}

		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(nbt_netlogon_response2, &response.data.response2);
		}

		send_mailslot(True, request.req.logon0.mailslot_name,
				(char *)blob_out.data,
				blob_out.length,
				global_myname(), 0x0,
				source_name,
				dgram->source_name.name_type,
				p->ip, ip, p->port);
		break;
	}

	case LOGON_PRIMARY_QUERY: {

		struct nbt_netlogon_response_from_pdc get_pdc;

		if (!lp_domain_master()) {
			/* We're not Primary Domain Controller -- ignore this */
			return;
		}

		DEBUG(5,("process_logon_packet: GETDC request from %s at IP %s, "
			"reporting %s domain %s 0x%x ntversion=%x lm_nt token=%x lm_20 token=%x\n",
			request.req.pdc.computer_name,
			inet_ntoa(p->ip),
			global_myname(),
			lp_workgroup(),
			NETLOGON_RESPONSE_FROM_PDC,
			request.req.pdc.nt_version,
			request.req.pdc.lmnt_token,
			request.req.pdc.lm20_token));

		get_pdc.command			= NETLOGON_RESPONSE_FROM_PDC;
		get_pdc.pdc_name		= global_myname();
		get_pdc._pad			= data_blob_null;
		get_pdc.unicode_pdc_name	= global_myname();
		get_pdc.domain_name		= lp_workgroup();
		get_pdc.nt_version		= NETLOGON_NT_VERSION_1;
		get_pdc.lmnt_token		= 0xffff;
		get_pdc.lm20_token		= 0xffff;

		response.response_type = NETLOGON_GET_PDC;
		response.data.get_pdc = get_pdc;

		status = push_nbt_netlogon_response(&blob_out, talloc_tos(), &response);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("process_logon_packet: failed to push packet\n"));
			return;
		}

		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(nbt_netlogon_response_from_pdc, &response.data.get_pdc);
		}

		send_mailslot(True, request.req.pdc.mailslot_name,
			(char *)blob_out.data,
			blob_out.length,
			global_myname(), 0x0,
			source_name,
			dgram->source_name.name_type,
			p->ip, ip, p->port);

		return;
	}

	case LOGON_SAM_LOGON_REQUEST: {
		char *source_addr;
		bool user_unknown = false;

		struct netlogon_samlogon_response samlogon;

		if (global_nmbd_proxy_logon) {
			nmbd_proxy_logon(global_nmbd_proxy_logon,
					 ip, p, (uint8_t *)buf, len);
			return;
		}

		source_addr = SMB_STRDUP(inet_ntoa(dgram->header.source_ip));
		if (source_addr == NULL) {
			DEBUG(3, ("out of memory copying client"
				  " address string\n"));
			return;
		}

		DEBUG(5,("process_logon_packet: LOGON_SAM_LOGON_REQUEST request from %s(%s) for %s, returning logon svr %s domain %s code %x token=%x\n",
			request.req.logon.computer_name,
			inet_ntoa(p->ip),
			request.req.logon.user_name,
			pdc_name,
			lp_workgroup(),
			LOGON_SAM_LOGON_RESPONSE,
			request.req.logon.lmnt_token));

		if (!request.req.logon.user_name) {
			user_unknown = true;
		}

		/* we want the simple version unless we are an ADS PDC..which means  */
		/* never, at least for now */

		if ((request.req.logon.nt_version < (NETLOGON_NT_VERSION_1 | NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX_WITH_IP)) ||
		    (SEC_ADS != lp_security()) || (ROLE_DOMAIN_PDC != lp_server_role())) {

			struct NETLOGON_SAM_LOGON_RESPONSE_NT40 nt4;

			nt4.command		= user_unknown ? LOGON_SAM_LOGON_USER_UNKNOWN :
								 LOGON_SAM_LOGON_RESPONSE;
			nt4.pdc_name		= pdc_name;
			nt4.user_name		= request.req.logon.user_name;
			nt4.domain_name		= lp_workgroup();
			nt4.nt_version		= NETLOGON_NT_VERSION_1;
			nt4.lmnt_token		= 0xffff;
			nt4.lm20_token		= 0xffff;

			samlogon.ntver = NETLOGON_NT_VERSION_1;
			samlogon.data.nt4 = nt4;

			if (DEBUGLEVEL >= 10) {
				NDR_PRINT_DEBUG(NETLOGON_SAM_LOGON_RESPONSE_NT40, &nt4);
			}
		}
#ifdef HAVE_ADS
		else {

			struct NETLOGON_SAM_LOGON_RESPONSE_EX nt5_ex;
			struct GUID domain_guid;
			struct nbt_sockaddr saddr;
			char *domain;
			const char *hostname;

			saddr.sockaddr_family	= 2; /* AF_INET */
			saddr.pdc_ip		= inet_ntoa(ip);
			saddr.remaining		= data_blob_talloc_zero(talloc_tos(), 8); /* ??? */

			domain = get_mydnsdomname(talloc_tos());
			if (!domain) {
				DEBUG(2,("get_mydnsdomname failed.\n"));
				return;
			}

			hostname = get_mydnsfullname();
			if (!hostname) {
				DEBUG(2,("get_mydnsfullname failed.\n"));
				return;
			}

			if (!secrets_fetch_domain_guid(domain, &domain_guid)) {
				DEBUG(2,("Could not fetch DomainGUID for %s\n", domain));
				return;
			}

			nt5_ex.command		= user_unknown ? LOGON_SAM_LOGON_USER_UNKNOWN_EX :
								 LOGON_SAM_LOGON_RESPONSE_EX;
			nt5_ex.sbz		= 0;
			nt5_ex.server_type	= NBT_SERVER_PDC |
						  NBT_SERVER_GC |
						  NBT_SERVER_LDAP |
						  NBT_SERVER_DS |
						  NBT_SERVER_KDC |
						  NBT_SERVER_TIMESERV |
						  NBT_SERVER_CLOSEST |
						  NBT_SERVER_WRITABLE;
			nt5_ex.domain_uuid	= domain_guid;
			nt5_ex.forest		= domain;
			nt5_ex.dns_domain	= domain;
			nt5_ex.pdc_dns_name	= hostname;
			nt5_ex.domain_name	= lp_workgroup();
			nt5_ex.pdc_name		= global_myname();
			nt5_ex.user_name	= request.req.logon.user_name;
			nt5_ex.server_site	= "Default-First-Site-Name";
			nt5_ex.client_site	= "Default-First-Site-Name";
			nt5_ex.sockaddr_size	= 0x10; /* the w32 winsock addr size */
			nt5_ex.sockaddr		= saddr;
			nt5_ex.next_closest_site= NULL;
			nt5_ex.nt_version	= NETLOGON_NT_VERSION_1 |
						  NETLOGON_NT_VERSION_5EX |
						  NETLOGON_NT_VERSION_5EX_WITH_IP;
			nt5_ex.lmnt_token	= 0xffff;
			nt5_ex.lm20_token	= 0xffff;

			samlogon.ntver = NETLOGON_NT_VERSION_1 |
					 NETLOGON_NT_VERSION_5EX |
					 NETLOGON_NT_VERSION_5EX_WITH_IP;
			samlogon.data.nt5_ex = nt5_ex;

			if (DEBUGLEVEL >= 10) {
				NDR_PRINT_DEBUG(NETLOGON_SAM_LOGON_RESPONSE_EX, &nt5_ex);
			}
		}
#endif /* HAVE_ADS */

		response.response_type = NETLOGON_SAMLOGON;
		response.data.samlogon = samlogon;

		status = push_nbt_netlogon_response(&blob_out, talloc_tos(), &response);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("process_logon_packet: failed to push packet\n"));
			return;
		}

		/*
		 * handle delay.
		 * packets requeued after delay are marked as
		 * locked.
		 */
		if ((p->locked == False) &&
		    (strlen(request.req.logon.user_name) == 0) &&
		    delay_logon(source_name, source_addr))
		{
			struct timeval when;

			DEBUG(3, ("process_logon_packet: "
				  "delaying initial logon "
				  "reply for client %s(%s) for "
				  "%u milliseconds\n",
				  source_name, source_addr,
				  lp_init_logon_delay()));

			when = timeval_current_ofs(0,
				lp_init_logon_delay() * 1000);
			p->locked = true;
			event_add_timed(nmbd_event_context(),
					NULL,
					when,
					delayed_init_logon_handler,
					p);
		} else {
			DEBUG(3, ("process_logon_packet: "
				  "processing delayed initial "
				  "logon reply for client "
				  "%s(%s)\n",
				  source_name, source_addr));
			p->locked = false;
			send_mailslot(true, request.req.logon.mailslot_name,
				(char *)blob_out.data,
				blob_out.length,
				global_myname(), 0x0,
				source_name,
				dgram->source_name.name_type,
				p->ip, ip, p->port);
		}

		SAFE_FREE(source_addr);

		break;
	}

	/* Announce change to UAS or SAM.  Send by the domain controller when a
	replication event is required. */

	case NETLOGON_ANNOUNCE_UAS:
		DEBUG(5, ("Got NETLOGON_ANNOUNCE_UAS\n"));
		break;

	default:
		DEBUG(3,("process_logon_packet: Unknown domain request %d\n",
			request.command));
		return;
	}
}
