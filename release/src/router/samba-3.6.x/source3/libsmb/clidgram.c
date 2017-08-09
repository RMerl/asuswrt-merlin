/* 
   Unix SMB/CIFS implementation.
   client dgram calls
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Richard Sharpe 2001
   Copyright (C) John Terpstra 2001

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
#include "libsmb/libsmb.h"
#include "../lib/util/tevent_ntstatus.h"
#include "libsmb/clidgram.h"
#include "libsmb/nmblib.h"
#include "messages.h"

/*
 * cli_send_mailslot, send a mailslot for client code ...
 */

static bool cli_prep_mailslot(bool unique, const char *mailslot,
		       uint16 priority,
		       char *buf, int len,
		       const char *srcname, int src_type,
		       const char *dstname, int dest_type,
		       const struct sockaddr_storage *dest_ss,
		       int dgm_id,
		       struct packet_struct *p)
{
	struct dgram_packet *dgram = &p->packet.dgram;
	char *ptr, *p2;
	char tmp[4];
	char addr[INET6_ADDRSTRLEN];

	ZERO_STRUCTP(p);

	/*
	 * Next, build the DGRAM ...
	 */

	/* DIRECT GROUP or UNIQUE datagram. */
	dgram->header.msg_type = unique ? 0x10 : 0x11;
	dgram->header.flags.node_type = M_NODE;
	dgram->header.flags.first = True;
	dgram->header.flags.more = False;
	dgram->header.dgm_id = dgm_id;
	/* source ip is filled by nmbd */
	dgram->header.dgm_length = 0; /* Let build_dgram() handle this. */
	dgram->header.packet_offset = 0;

	make_nmb_name(&dgram->source_name,srcname,src_type);
	make_nmb_name(&dgram->dest_name,dstname,dest_type);

	ptr = &dgram->data[0];

	/* Setup the smb part. */
	ptr -= 4; /* XXX Ugliness because of handling of tcp SMB length. */
	memcpy(tmp,ptr,4);

	if (smb_size + 17*2 + strlen(mailslot) + 1 + len > MAX_DGRAM_SIZE) {
		DEBUG(0, ("cli_send_mailslot: Cannot write beyond end of packet\n"));
		return False;
	}

	cli_set_message(ptr,17,strlen(mailslot) + 1 + len,True);
	memcpy(ptr,tmp,4);

	SCVAL(ptr,smb_com,SMBtrans);
	SSVAL(ptr,smb_vwv1,len);
	SSVAL(ptr,smb_vwv11,len);
	SSVAL(ptr,smb_vwv12,70 + strlen(mailslot));
	SSVAL(ptr,smb_vwv13,3);
	SSVAL(ptr,smb_vwv14,1);
	SSVAL(ptr,smb_vwv15,priority);
	SSVAL(ptr,smb_vwv16,2);
	p2 = smb_buf(ptr);
	fstrcpy(p2,mailslot);
	p2 = skip_string(ptr,MAX_DGRAM_SIZE,p2);
	if (!p2) {
		return False;
	}

	memcpy(p2,buf,len);
	p2 += len;

	dgram->datasize = PTR_DIFF(p2,ptr+4); /* +4 for tcp length. */

	p->packet_type = DGRAM_PACKET;
	p->ip = ((const struct sockaddr_in *)dest_ss)->sin_addr;
	p->timestamp = time(NULL);

	DEBUG(4,("send_mailslot: Sending to mailslot %s from %s ",
		 mailslot, nmb_namestr(&dgram->source_name)));
	print_sockaddr(addr, sizeof(addr), dest_ss);

	DEBUGADD(4,("to %s IP %s\n", nmb_namestr(&dgram->dest_name), addr));

	return true;
}

static char *mailslot_name(TALLOC_CTX *mem_ctx, struct in_addr dc_ip)
{
	return talloc_asprintf(mem_ctx, "%s%X",
			       NBT_MAILSLOT_GETDC, dc_ip.s_addr);
}

static bool prep_getdc_request(const struct sockaddr_storage *dc_ss,
			       const char *domain_name,
			       const struct dom_sid *sid,
			       uint32_t nt_version,
			       const char *my_mailslot,
			       int dgm_id,
			       struct packet_struct *p)
{
	TALLOC_CTX *frame = talloc_stackframe();
	const char *my_acct_name;
	struct nbt_netlogon_packet packet;
	struct NETLOGON_SAM_LOGON_REQUEST *s;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob = data_blob_null;
	struct dom_sid my_sid;
	bool ret = false;

	ZERO_STRUCT(packet);
	ZERO_STRUCT(my_sid);

	if (sid != NULL) {
		my_sid = *sid;
	}

	my_acct_name = talloc_asprintf(talloc_tos(), "%s$", global_myname());
	if (my_acct_name == NULL) {
		goto fail;
	}

	packet.command	= LOGON_SAM_LOGON_REQUEST;
	s		= &packet.req.logon;

	s->request_count	= 0;
	s->computer_name	= global_myname();
	s->user_name		= my_acct_name;
	s->mailslot_name	= my_mailslot;
	s->acct_control		= ACB_WSTRUST;
	s->sid			= my_sid;
	s->nt_version		= nt_version;
	s->lmnt_token		= 0xffff;
	s->lm20_token		= 0xffff;

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(nbt_netlogon_packet, &packet);
	}

	ndr_err = ndr_push_struct_blob(&blob, talloc_tos(), &packet,
		       (ndr_push_flags_fn_t)ndr_push_nbt_netlogon_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		goto fail;
	}

	ret = cli_prep_mailslot(false, NBT_MAILSLOT_NTLOGON, 0,
				(char *)blob.data, blob.length,
				global_myname(), 0, domain_name, 0x1c,
				dc_ss, dgm_id, p);
fail:
	TALLOC_FREE(frame);
	return ret;
}

static bool parse_getdc_response(
	struct packet_struct *packet,
	TALLOC_CTX *mem_ctx,
	const char *domain_name,
	uint32_t *nt_version,
	const char **dc_name,
	struct netlogon_samlogon_response **samlogon_response)
{
	DATA_BLOB blob;
	struct netlogon_samlogon_response *r;
	union dgram_message_body p;
	enum ndr_err_code ndr_err;
	NTSTATUS status;

	const char *returned_dc = NULL;
	const char *returned_domain = NULL;

	blob = data_blob_const(packet->packet.dgram.data,
			       packet->packet.dgram.datasize);
	if (blob.length < 4) {
		DEBUG(1, ("invalid length: %d\n", (int)blob.length));
		return false;
	}

	if (RIVAL(blob.data,0) != DGRAM_SMB) {
		DEBUG(1, ("invalid packet\n"));
		return false;
	}

	blob.data += 4;
	blob.length -= 4;

	ndr_err = ndr_pull_union_blob_all(&blob, mem_ctx, &p, DGRAM_SMB,
		       (ndr_pull_flags_fn_t)ndr_pull_dgram_smb_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1, ("failed to parse packet\n"));
		return false;
	}

	if (p.smb.smb_command != SMB_TRANSACTION) {
		DEBUG(1, ("invalid smb_command: %d\n", p.smb.smb_command));
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(dgram_smb_packet, &p);
	}

	blob = p.smb.body.trans.data;

	r = TALLOC_ZERO_P(mem_ctx, struct netlogon_samlogon_response);
	if (!r) {
		return false;
	}

	status = pull_netlogon_samlogon_response(&blob, r, r);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(r);
		return false;
	}

	map_netlogon_samlogon_response(r);

	/* do we still need this ? */
	*nt_version = r->ntver;

	returned_domain = r->data.nt5_ex.domain_name;
	returned_dc = r->data.nt5_ex.pdc_name;

	if (!strequal(returned_domain, domain_name)) {
		DEBUG(3, ("GetDC: Expected domain %s, got %s\n",
			  domain_name, returned_domain));
		TALLOC_FREE(r);
		return false;
	}

	if (*returned_dc == '\\') returned_dc += 1;
	if (*returned_dc == '\\') returned_dc += 1;

	*dc_name = talloc_strdup(mem_ctx, returned_dc);
	if (!*dc_name) {
		TALLOC_FREE(r);
		return false;
	}

	if (samlogon_response) {
		*samlogon_response = r;
	} else {
		TALLOC_FREE(r);
	}

	DEBUG(10, ("GetDC gave name %s for domain %s\n",
		   *dc_name, returned_domain));

	return True;
}

struct nbt_getdc_state {
	struct tevent_context *ev;
	struct messaging_context *msg_ctx;
	struct nb_packet_reader *reader;
	const char *my_mailslot;
	pid_t nmbd_pid;

	const struct sockaddr_storage *dc_addr;
	const char *domain_name;
	const struct dom_sid *sid;
	uint32_t nt_version;
	const char *dc_name;
	struct netlogon_samlogon_response *samlogon_response;

	struct packet_struct p;
};

static void nbt_getdc_got_reader(struct tevent_req *subreq);
static void nbt_getdc_got_response(struct tevent_req *subreq);

struct tevent_req *nbt_getdc_send(TALLOC_CTX *mem_ctx,
				  struct tevent_context *ev,
				  struct messaging_context *msg_ctx,
				  const struct sockaddr_storage *dc_addr,
				  const char *domain_name,
				  const struct dom_sid *sid,
				  uint32_t nt_version)
{
	struct tevent_req *req, *subreq;
	struct nbt_getdc_state *state;
	uint16_t dgm_id;

	req = tevent_req_create(mem_ctx, &state, struct nbt_getdc_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->msg_ctx = msg_ctx;
	state->dc_addr = dc_addr;
	state->domain_name = domain_name;
	state->sid = sid;
	state->nt_version = nt_version;

	if (dc_addr->ss_family != AF_INET) {
		tevent_req_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return tevent_req_post(req, ev);
	}
	state->my_mailslot = mailslot_name(
		state, ((struct sockaddr_in *)dc_addr)->sin_addr);
	if (tevent_req_nomem(state->my_mailslot, req)) {
		return tevent_req_post(req, ev);
	}
	state->nmbd_pid = pidfile_pid("nmbd");
	if (state->nmbd_pid == 0) {
		DEBUG(3, ("No nmbd found\n"));
		tevent_req_nterror(req, NT_STATUS_NOT_SUPPORTED);
		return tevent_req_post(req, ev);
	}

	generate_random_buffer((uint8_t *)(void *)&dgm_id, sizeof(dgm_id));

	if (!prep_getdc_request(dc_addr, domain_name, sid, nt_version,
				state->my_mailslot, dgm_id & 0x7fff,
				&state->p)) {
		DEBUG(3, ("prep_getdc_request failed\n"));
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	subreq = nb_packet_reader_send(state, ev, DGRAM_PACKET, -1,
				       state->my_mailslot);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, nbt_getdc_got_reader, req);
	return req;
}

static void nbt_getdc_got_reader(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nbt_getdc_state *state = tevent_req_data(
		req, struct nbt_getdc_state);
	NTSTATUS status;

	status = nb_packet_reader_recv(subreq, state, &state->reader);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		DEBUG(10, ("nb_packet_reader_recv returned %s\n",
			   nt_errstr(status)));
		return;
	}

	status = messaging_send_buf(
		state->msg_ctx, pid_to_procid(state->nmbd_pid),
		MSG_SEND_PACKET, (uint8_t *)&state->p, sizeof(state->p));

	if (tevent_req_nterror(req, status)) {
		DEBUG(10, ("messaging_send_buf returned %s\n",
			   nt_errstr(status)));
		return;
	}
	subreq = nb_packet_read_send(state, state->ev, state->reader);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, nbt_getdc_got_response, req);
}

static void nbt_getdc_got_response(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nbt_getdc_state *state = tevent_req_data(
		req, struct nbt_getdc_state);
	struct packet_struct *p;
	NTSTATUS status;
	bool ret;

	status = nb_packet_read_recv(subreq, &p);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	ret = parse_getdc_response(p, state, state->domain_name,
				   &state->nt_version, &state->dc_name,
				   &state->samlogon_response);
	free_packet(p);
	if (!ret) {
		tevent_req_nterror(req, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS nbt_getdc_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			uint32_t *nt_version, const char **dc_name,
			struct netlogon_samlogon_response **samlogon_response)
{
	struct nbt_getdc_state *state = tevent_req_data(
		req, struct nbt_getdc_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	if (nt_version != NULL) {
		*nt_version = state->nt_version;
	}
	if (dc_name != NULL) {
		*dc_name = talloc_move(mem_ctx, &state->dc_name);
	}
	if (samlogon_response != NULL) {
		*samlogon_response = talloc_move(
			mem_ctx, &state->samlogon_response);
	}
	return NT_STATUS_OK;
}

NTSTATUS nbt_getdc(struct messaging_context *msg_ctx,
		   uint32_t timeout_in_seconds,
		   const struct sockaddr_storage *dc_addr,
		   const char *domain_name,
		   const struct dom_sid *sid,
		   uint32_t nt_version,
		   TALLOC_CTX *mem_ctx,
		   uint32_t *pnt_version,
		   const char **dc_name,
		   struct netlogon_samlogon_response **samlogon_response)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_NO_MEMORY;

	ev = tevent_context_init(frame);
	if (ev == NULL) {
		goto fail;
	}
	req = nbt_getdc_send(ev, ev, msg_ctx, dc_addr, domain_name,
			     sid, nt_version);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_set_endtime(req, ev,
			timeval_current_ofs(timeout_in_seconds, 0))) {
		goto fail;
	}
	if (!tevent_req_poll_ntstatus(req, ev, &status)) {
		goto fail;
	}
	status = nbt_getdc_recv(req, mem_ctx, pnt_version, dc_name,
				samlogon_response);
 fail:
	TALLOC_FREE(frame);
	return status;
}
