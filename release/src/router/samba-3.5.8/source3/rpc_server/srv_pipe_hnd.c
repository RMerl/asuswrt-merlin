/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Largely re-written : 2005
 *  Copyright (C) Jeremy Allison		1998 - 2005
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
#include "librpc/gen_ndr/ndr_named_pipe_auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

static int pipes_open;

static pipes_struct *InternalPipes;

/* TODO
 * the following prototypes are declared here to avoid
 * code being moved about too much for a patch to be
 * disrupted / less obvious.
 *
 * these functions, and associated functions that they
 * call, should be moved behind a .so module-loading
 * system _anyway_.  so that's the next step...
 */

static int close_internal_rpc_pipe_hnd(struct pipes_struct *p);

/****************************************************************************
 Internal Pipe iterator functions.
****************************************************************************/

pipes_struct *get_first_internal_pipe(void)
{
	return InternalPipes;
}

pipes_struct *get_next_internal_pipe(pipes_struct *p)
{
	return p->next;
}

/****************************************************************************
 Initialise an outgoing packet.
****************************************************************************/

static bool pipe_init_outgoing_data(pipes_struct *p)
{
	output_data *o_data = &p->out_data;

	/* Reset the offset counters. */
	o_data->data_sent_length = 0;
	o_data->current_pdu_sent = 0;

	prs_mem_free(&o_data->frag);

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&o_data->rdata);

	/*
	 * Initialize the outgoing RPC data buffer.
	 * we will use this as the raw data area for replying to rpc requests.
	 */	
	if(!prs_init(&o_data->rdata, 128, p->mem_ctx, MARSHALL)) {
		DEBUG(0,("pipe_init_outgoing_data: malloc fail.\n"));
		return False;
	}

	return True;
}

/****************************************************************************
 Make an internal namedpipes structure
****************************************************************************/

static struct pipes_struct *make_internal_rpc_pipe_p(TALLOC_CTX *mem_ctx,
						     const struct ndr_syntax_id *syntax,
						     const char *client_address,
						     struct auth_serversupplied_info *server_info)
{
	pipes_struct *p;

	DEBUG(4,("Create pipe requested %s\n",
		 get_pipe_name_from_syntax(talloc_tos(), syntax)));

	p = TALLOC_ZERO_P(mem_ctx, struct pipes_struct);

	if (!p) {
		DEBUG(0,("ERROR! no memory for pipes_struct!\n"));
		return NULL;
	}

	p->mem_ctx = talloc_init("pipe %s %p",
				 get_pipe_name_from_syntax(talloc_tos(),
							   syntax), p);
	if (p->mem_ctx == NULL) {
		DEBUG(0,("open_rpc_pipe_p: talloc_init failed.\n"));
		TALLOC_FREE(p);
		return NULL;
	}

	if (!init_pipe_handle_list(p, syntax)) {
		DEBUG(0,("open_rpc_pipe_p: init_pipe_handles failed.\n"));
		talloc_destroy(p->mem_ctx);
		TALLOC_FREE(p);
		return NULL;
	}

	/*
	 * Initialize the incoming RPC data buffer with one PDU worth of memory.
	 * We cheat here and say we're marshalling, as we intend to add incoming
	 * data directly into the prs_struct and we want it to auto grow. We will
	 * change the type to UNMARSALLING before processing the stream.
	 */

	if(!prs_init(&p->in_data.data, 128, p->mem_ctx, MARSHALL)) {
		DEBUG(0,("open_rpc_pipe_p: malloc fail for in_data struct.\n"));
		talloc_destroy(p->mem_ctx);
		close_policy_by_pipe(p);
		TALLOC_FREE(p);
		return NULL;
	}

	p->server_info = copy_serverinfo(p, server_info);
	if (p->server_info == NULL) {
		DEBUG(0, ("open_rpc_pipe_p: copy_serverinfo failed\n"));
		talloc_destroy(p->mem_ctx);
		close_policy_by_pipe(p);
		TALLOC_FREE(p);
		return NULL;
	}

	DLIST_ADD(InternalPipes, p);

	memcpy(p->client_address, client_address, sizeof(p->client_address));

	p->endian = RPC_LITTLE_ENDIAN;

	/*
	 * Initialize the outgoing RPC data buffer with no memory.
	 */	
	prs_init_empty(&p->out_data.rdata, p->mem_ctx, MARSHALL);

	p->syntax = *syntax;

	DEBUG(4,("Created internal pipe %s (pipes_open=%d)\n",
		 get_pipe_name_from_syntax(talloc_tos(), syntax), pipes_open));

	talloc_set_destructor(p, close_internal_rpc_pipe_hnd);

	return p;
}

/****************************************************************************
 Sets the fault state on incoming packets.
****************************************************************************/

static void set_incoming_fault(pipes_struct *p)
{
	prs_mem_free(&p->in_data.data);
	p->in_data.pdu_needed_len = 0;
	p->in_data.pdu_received_len = 0;
	p->fault_state = True;
	DEBUG(10, ("set_incoming_fault: Setting fault state on pipe %s\n",
		   get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
}

/****************************************************************************
 Ensures we have at least RPC_HEADER_LEN amount of data in the incoming buffer.
****************************************************************************/

static ssize_t fill_rpc_header(pipes_struct *p, char *data, size_t data_to_copy)
{
	size_t len_needed_to_complete_hdr = MIN(data_to_copy, RPC_HEADER_LEN - p->in_data.pdu_received_len);

	DEBUG(10,("fill_rpc_header: data_to_copy = %u, len_needed_to_complete_hdr = %u, receive_len = %u\n",
			(unsigned int)data_to_copy, (unsigned int)len_needed_to_complete_hdr,
			(unsigned int)p->in_data.pdu_received_len ));

	if (p->in_data.current_in_pdu == NULL) {
		p->in_data.current_in_pdu = talloc_array(p, uint8_t,
							 RPC_HEADER_LEN);
	}
	if (p->in_data.current_in_pdu == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return -1;
	}

	memcpy((char *)&p->in_data.current_in_pdu[p->in_data.pdu_received_len], data, len_needed_to_complete_hdr);
	p->in_data.pdu_received_len += len_needed_to_complete_hdr;

	return (ssize_t)len_needed_to_complete_hdr;
}

/****************************************************************************
 Unmarshalls a new PDU header. Assumes the raw header data is in current_in_pdu.
****************************************************************************/

static ssize_t unmarshall_rpc_header(pipes_struct *p)
{
	/*
	 * Unmarshall the header to determine the needed length.
	 */

	prs_struct rpc_in;

	if(p->in_data.pdu_received_len != RPC_HEADER_LEN) {
		DEBUG(0,("unmarshall_rpc_header: assert on rpc header length failed.\n"));
		set_incoming_fault(p);
		return -1;
	}

	prs_init_empty( &rpc_in, p->mem_ctx, UNMARSHALL);
	prs_set_endian_data( &rpc_in, p->endian);

	prs_give_memory( &rpc_in, (char *)&p->in_data.current_in_pdu[0],
					p->in_data.pdu_received_len, False);

	/*
	 * Unmarshall the header as this will tell us how much
	 * data we need to read to get the complete pdu.
	 * This also sets the endian flag in rpc_in.
	 */

	if(!smb_io_rpc_hdr("", &p->hdr, &rpc_in, 0)) {
		DEBUG(0,("unmarshall_rpc_header: failed to unmarshall RPC_HDR.\n"));
		set_incoming_fault(p);
		prs_mem_free(&rpc_in);
		return -1;
	}

	/*
	 * Validate the RPC header.
	 */

	if(p->hdr.major != 5 && p->hdr.minor != 0) {
		DEBUG(0,("unmarshall_rpc_header: invalid major/minor numbers in RPC_HDR.\n"));
		set_incoming_fault(p);
		prs_mem_free(&rpc_in);
		return -1;
	}

	/*
	 * If there's not data in the incoming buffer this should be the start of a new RPC.
	 */

	if(prs_offset(&p->in_data.data) == 0) {

		/*
		 * AS/U doesn't set FIRST flag in a BIND packet it seems.
		 */

		if ((p->hdr.pkt_type == DCERPC_PKT_REQUEST) && !(p->hdr.flags & DCERPC_PFC_FLAG_FIRST)) {
			/*
			 * Ensure that the FIRST flag is set. If not then we have
			 * a stream missmatch.
			 */

			DEBUG(0,("unmarshall_rpc_header: FIRST flag not set in first PDU !\n"));
			set_incoming_fault(p);
			prs_mem_free(&rpc_in);
			return -1;
		}

		/*
		 * If this is the first PDU then set the endianness
		 * flag in the pipe. We will need this when parsing all
		 * data in this RPC.
		 */

		p->endian = rpc_in.bigendian_data;

		DEBUG(5,("unmarshall_rpc_header: using %sendian RPC\n",
				p->endian == RPC_LITTLE_ENDIAN ? "little-" : "big-" ));

	} else {

		/*
		 * If this is *NOT* the first PDU then check the endianness
		 * flag in the pipe is the same as that in the PDU.
		 */

		if (p->endian != rpc_in.bigendian_data) {
			DEBUG(0,("unmarshall_rpc_header: FIRST endianness flag (%d) different in next PDU !\n", (int)p->endian));
			set_incoming_fault(p);
			prs_mem_free(&rpc_in);
			return -1;
		}
	}

	/*
	 * Ensure that the pdu length is sane.
	 */

	if((p->hdr.frag_len < RPC_HEADER_LEN) || (p->hdr.frag_len > RPC_MAX_PDU_FRAG_LEN)) {
		DEBUG(0,("unmarshall_rpc_header: assert on frag length failed.\n"));
		set_incoming_fault(p);
		prs_mem_free(&rpc_in);
		return -1;
	}

	DEBUG(10,("unmarshall_rpc_header: type = %u, flags = %u\n", (unsigned int)p->hdr.pkt_type,
			(unsigned int)p->hdr.flags ));

	p->in_data.pdu_needed_len = (uint32)p->hdr.frag_len - RPC_HEADER_LEN;

	prs_mem_free(&rpc_in);

	p->in_data.current_in_pdu = TALLOC_REALLOC_ARRAY(
		p, p->in_data.current_in_pdu, uint8_t, p->hdr.frag_len);
	if (p->in_data.current_in_pdu == NULL) {
		DEBUG(0, ("talloc failed\n"));
		set_incoming_fault(p);
		return -1;
	}

	return 0; /* No extra data processed. */
}

/****************************************************************************
 Call this to free any talloc'ed memory. Do this before and after processing
 a complete PDU.
****************************************************************************/

static void free_pipe_context(pipes_struct *p)
{
	if (p->mem_ctx) {
		DEBUG(3,("free_pipe_context: destroying talloc pool of size "
			 "%lu\n", (unsigned long)talloc_total_size(p->mem_ctx) ));
		talloc_free_children(p->mem_ctx);
	} else {
		p->mem_ctx = talloc_init(
			"pipe %s %p", get_pipe_name_from_syntax(talloc_tos(),
								&p->syntax),
			p);
		if (p->mem_ctx == NULL) {
			p->fault_state = True;
		}
	}
}

/****************************************************************************
 Processes a request pdu. This will do auth processing if needed, and
 appends the data into the complete stream if the LAST flag is not set.
****************************************************************************/

static bool process_request_pdu(pipes_struct *p, prs_struct *rpc_in_p)
{
	uint32 ss_padding_len = 0;
	size_t data_len = p->hdr.frag_len - RPC_HEADER_LEN - RPC_HDR_REQ_LEN -
				(p->hdr.auth_len ? RPC_HDR_AUTH_LEN : 0) - p->hdr.auth_len;

	if(!p->pipe_bound) {
		DEBUG(0,("process_request_pdu: rpc request with no bind.\n"));
		set_incoming_fault(p);
		return False;
	}

	/*
	 * Check if we need to do authentication processing.
	 * This is only done on requests, not binds.
	 */

	/*
	 * Read the RPC request header.
	 */

	if(!smb_io_rpc_hdr_req("req", &p->hdr_req, rpc_in_p, 0)) {
		DEBUG(0,("process_request_pdu: failed to unmarshall RPC_HDR_REQ.\n"));
		set_incoming_fault(p);
		return False;
	}

	switch(p->auth.auth_type) {
		case PIPE_AUTH_TYPE_NONE:
			break;

		case PIPE_AUTH_TYPE_SPNEGO_NTLMSSP:
		case PIPE_AUTH_TYPE_NTLMSSP:
		{
			NTSTATUS status;
			if(!api_pipe_ntlmssp_auth_process(p, rpc_in_p, &ss_padding_len, &status)) {
				DEBUG(0,("process_request_pdu: failed to do auth processing.\n"));
				DEBUG(0,("process_request_pdu: error was %s.\n", nt_errstr(status) ));
				set_incoming_fault(p);
				return False;
			}
			break;
		}

		case PIPE_AUTH_TYPE_SCHANNEL:
			if (!api_pipe_schannel_process(p, rpc_in_p, &ss_padding_len)) {
				DEBUG(3,("process_request_pdu: failed to do schannel processing.\n"));
				set_incoming_fault(p);
				return False;
			}
			break;

		default:
			DEBUG(0,("process_request_pdu: unknown auth type %u set.\n", (unsigned int)p->auth.auth_type ));
			set_incoming_fault(p);
			return False;
	}

	/* Now we've done the sign/seal we can remove any padding data. */
	if (data_len > ss_padding_len) {
		data_len -= ss_padding_len;
	}

	/*
	 * Check the data length doesn't go over the 15Mb limit.
	 * increased after observing a bug in the Windows NT 4.0 SP6a
	 * spoolsv.exe when the response to a GETPRINTERDRIVER2 RPC
	 * will not fit in the initial buffer of size 0x1068   --jerry 22/01/2002
	 */
	
	if(prs_offset(&p->in_data.data) + data_len > MAX_RPC_DATA_SIZE) {
		DEBUG(0,("process_request_pdu: rpc data buffer too large (%u) + (%u)\n",
				(unsigned int)prs_data_size(&p->in_data.data), (unsigned int)data_len ));
		set_incoming_fault(p);
		return False;
	}

	/*
	 * Append the data portion into the buffer and return.
	 */

	if(!prs_append_some_prs_data(&p->in_data.data, rpc_in_p, prs_offset(rpc_in_p), data_len)) {
		DEBUG(0,("process_request_pdu: Unable to append data size %u to parse buffer of size %u.\n",
				(unsigned int)data_len, (unsigned int)prs_data_size(&p->in_data.data) ));
		set_incoming_fault(p);
		return False;
	}

	if(p->hdr.flags & DCERPC_PFC_FLAG_LAST) {
		bool ret = False;
		/*
		 * Ok - we finally have a complete RPC stream.
		 * Call the rpc command to process it.
		 */

		/*
		 * Ensure the internal prs buffer size is *exactly* the same
		 * size as the current offset.
		 */

 		if(!prs_set_buffer_size(&p->in_data.data, prs_offset(&p->in_data.data))) {
			DEBUG(0,("process_request_pdu: Call to prs_set_buffer_size failed!\n"));
			set_incoming_fault(p);
			return False;
		}

		/*
		 * Set the parse offset to the start of the data and set the
		 * prs_struct to UNMARSHALL.
		 */

		prs_set_offset(&p->in_data.data, 0);
		prs_switch_type(&p->in_data.data, UNMARSHALL);

		/*
		 * Process the complete data stream here.
		 */

		free_pipe_context(p);

		if(pipe_init_outgoing_data(p)) {
			ret = api_pipe_request(p);
		}

		free_pipe_context(p);

		/*
		 * We have consumed the whole data stream. Set back to
		 * marshalling and set the offset back to the start of
		 * the buffer to re-use it (we could also do a prs_mem_free()
		 * and then re_init on the next start of PDU. Not sure which
		 * is best here.... JRA.
		 */

		prs_switch_type(&p->in_data.data, MARSHALL);
		prs_set_offset(&p->in_data.data, 0);
		return ret;
	}

	return True;
}

/****************************************************************************
 Processes a finished PDU stored in current_in_pdu. The RPC_HEADER has
 already been parsed and stored in p->hdr.
****************************************************************************/

static void process_complete_pdu(pipes_struct *p)
{
	prs_struct rpc_in;
	size_t data_len = p->in_data.pdu_received_len - RPC_HEADER_LEN;
	char *data_p = (char *)&p->in_data.current_in_pdu[RPC_HEADER_LEN];
	bool reply = False;

	if(p->fault_state) {
		DEBUG(10,("process_complete_pdu: pipe %s in fault state.\n",
			  get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
		set_incoming_fault(p);
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		return;
	}

	prs_init_empty( &rpc_in, p->mem_ctx, UNMARSHALL);

	/*
	 * Ensure we're using the corrent endianness for both the 
	 * RPC header flags and the raw data we will be reading from.
	 */

	prs_set_endian_data( &rpc_in, p->endian);
	prs_set_endian_data( &p->in_data.data, p->endian);

	prs_give_memory( &rpc_in, data_p, (uint32)data_len, False);

	DEBUG(10,("process_complete_pdu: processing packet type %u\n",
			(unsigned int)p->hdr.pkt_type ));

	switch (p->hdr.pkt_type) {
		case DCERPC_PKT_REQUEST:
			reply = process_request_pdu(p, &rpc_in);
			break;

		case DCERPC_PKT_PING: /* CL request - ignore... */
			DEBUG(0,("process_complete_pdu: Error. Connectionless packet type %u received on pipe %s.\n",
				(unsigned int)p->hdr.pkt_type,
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			break;

		case DCERPC_PKT_RESPONSE: /* No responses here. */
			DEBUG(0,("process_complete_pdu: Error. DCERPC_PKT_RESPONSE received from client on pipe %s.\n",
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			break;

		case DCERPC_PKT_FAULT:
		case DCERPC_PKT_WORKING: /* CL request - reply to a ping when a call in process. */
		case DCERPC_PKT_NOCALL: /* CL - server reply to a ping call. */
		case DCERPC_PKT_REJECT:
		case DCERPC_PKT_ACK:
		case DCERPC_PKT_CL_CANCEL:
		case DCERPC_PKT_FACK:
		case DCERPC_PKT_CANCEL_ACK:
			DEBUG(0,("process_complete_pdu: Error. Connectionless packet type %u received on pipe %s.\n",
				(unsigned int)p->hdr.pkt_type,
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			break;

		case DCERPC_PKT_BIND:
			/*
			 * We assume that a pipe bind is only in one pdu.
			 */
			if(pipe_init_outgoing_data(p)) {
				reply = api_pipe_bind_req(p, &rpc_in);
			}
			break;

		case DCERPC_PKT_BIND_ACK:
		case DCERPC_PKT_BIND_NAK:
			DEBUG(0,("process_complete_pdu: Error. DCERPC_PKT_BINDACK/DCERPC_PKT_BINDNACK packet type %u received on pipe %s.\n",
				(unsigned int)p->hdr.pkt_type,
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			break;


		case DCERPC_PKT_ALTER:
			/*
			 * We assume that a pipe bind is only in one pdu.
			 */
			if(pipe_init_outgoing_data(p)) {
				reply = api_pipe_alter_context(p, &rpc_in);
			}
			break;

		case DCERPC_PKT_ALTER_RESP:
			DEBUG(0,("process_complete_pdu: Error. DCERPC_PKT_ALTER_RESP on pipe %s: Should only be server -> client.\n",
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			break;

		case DCERPC_PKT_AUTH3:
			/*
			 * The third packet in an NTLMSSP auth exchange.
			 */
			if(pipe_init_outgoing_data(p)) {
				reply = api_pipe_bind_auth3(p, &rpc_in);
			}
			break;

		case DCERPC_PKT_SHUTDOWN:
			DEBUG(0,("process_complete_pdu: Error. DCERPC_PKT_SHUTDOWN on pipe %s: Should only be server -> client.\n",
				 get_pipe_name_from_syntax(talloc_tos(),
							   &p->syntax)));
			break;

		case DCERPC_PKT_CO_CANCEL:
			/* For now just free all client data and continue processing. */
			DEBUG(3,("process_complete_pdu: DCERPC_PKT_CO_CANCEL. Abandoning rpc call.\n"));
			/* As we never do asynchronous RPC serving, we can never cancel a
			   call (as far as I know). If we ever did we'd have to send a cancel_ack
			   reply. For now, just free all client data and continue processing. */
			reply = True;
			break;
#if 0
			/* Enable this if we're doing async rpc. */
			/* We must check the call-id matches the outstanding callid. */
			if(pipe_init_outgoing_data(p)) {
				/* Send a cancel_ack PDU reply. */
				/* We should probably check the auth-verifier here. */
				reply = setup_cancel_ack_reply(p, &rpc_in);
			}
			break;
#endif

		case DCERPC_PKT_ORPHANED:
			/* We should probably check the auth-verifier here.
			   For now just free all client data and continue processing. */
			DEBUG(3,("process_complete_pdu: DCERPC_PKT_ORPHANED. Abandoning rpc call.\n"));
			reply = True;
			break;

		default:
			DEBUG(0,("process_complete_pdu: Unknown rpc type = %u received.\n", (unsigned int)p->hdr.pkt_type ));
			break;
	}

	/* Reset to little endian. Probably don't need this but it won't hurt. */
	prs_set_endian_data( &p->in_data.data, RPC_LITTLE_ENDIAN);

	if (!reply) {
		DEBUG(3,("process_complete_pdu: DCE/RPC fault sent on "
			 "pipe %s\n", get_pipe_name_from_syntax(talloc_tos(),
								&p->syntax)));
		set_incoming_fault(p);
		setup_fault_pdu(p, NT_STATUS(DCERPC_FAULT_OP_RNG_ERROR));
		prs_mem_free(&rpc_in);
	} else {
		/*
		 * Reset the lengths. We're ready for a new pdu.
		 */
		TALLOC_FREE(p->in_data.current_in_pdu);
		p->in_data.pdu_needed_len = 0;
		p->in_data.pdu_received_len = 0;
	}

	prs_mem_free(&rpc_in);
}

/****************************************************************************
 Accepts incoming data on an rpc pipe. Processes the data in pdu sized units.
****************************************************************************/

static ssize_t process_incoming_data(pipes_struct *p, char *data, size_t n)
{
	size_t data_to_copy = MIN(n, RPC_MAX_PDU_FRAG_LEN - p->in_data.pdu_received_len);

	DEBUG(10,("process_incoming_data: Start: pdu_received_len = %u, pdu_needed_len = %u, incoming data = %u\n",
		(unsigned int)p->in_data.pdu_received_len, (unsigned int)p->in_data.pdu_needed_len,
		(unsigned int)n ));

	if(data_to_copy == 0) {
		/*
		 * This is an error - data is being received and there is no
		 * space in the PDU. Free the received data and go into the fault state.
		 */
		DEBUG(0,("process_incoming_data: No space in incoming pdu buffer. Current size = %u \
incoming data size = %u\n", (unsigned int)p->in_data.pdu_received_len, (unsigned int)n ));
		set_incoming_fault(p);
		return -1;
	}

	/*
	 * If we have no data already, wait until we get at least a RPC_HEADER_LEN
	 * number of bytes before we can do anything.
	 */

	if((p->in_data.pdu_needed_len == 0) && (p->in_data.pdu_received_len < RPC_HEADER_LEN)) {
		/*
		 * Always return here. If we have more data then the RPC_HEADER
		 * will be processed the next time around the loop.
		 */
		return fill_rpc_header(p, data, data_to_copy);
	}

	/*
	 * At this point we know we have at least an RPC_HEADER_LEN amount of data
	 * stored in current_in_pdu.
	 */

	/*
	 * If pdu_needed_len is zero this is a new pdu. 
	 * Unmarshall the header so we know how much more
	 * data we need, then loop again.
	 */

	if(p->in_data.pdu_needed_len == 0) {
		ssize_t rret = unmarshall_rpc_header(p);
		if (rret == -1 || p->in_data.pdu_needed_len > 0) {
			return rret;
		}
		/* If rret == 0 and pdu_needed_len == 0 here we have a PDU that consists
		   of an RPC_HEADER only. This is a DCERPC_PKT_SHUTDOWN, DCERPC_PKT_CO_CANCEL or DCERPC_PKT_ORPHANED
		   pdu type. Deal with this in process_complete_pdu(). */
	}

	/*
	 * Ok - at this point we have a valid RPC_HEADER in p->hdr.
	 * Keep reading until we have a full pdu.
	 */

	data_to_copy = MIN(data_to_copy, p->in_data.pdu_needed_len);

	/*
	 * Copy as much of the data as we need into the current_in_pdu buffer.
	 * pdu_needed_len becomes zero when we have a complete pdu.
	 */

	memcpy( (char *)&p->in_data.current_in_pdu[p->in_data.pdu_received_len], data, data_to_copy);
	p->in_data.pdu_received_len += data_to_copy;
	p->in_data.pdu_needed_len -= data_to_copy;

	/*
	 * Do we have a complete PDU ?
	 * (return the number of bytes handled in the call)
	 */

	if(p->in_data.pdu_needed_len == 0) {
		process_complete_pdu(p);
		return data_to_copy;
	}

	DEBUG(10,("process_incoming_data: not a complete PDU yet. pdu_received_len = %u, pdu_needed_len = %u\n",
		(unsigned int)p->in_data.pdu_received_len, (unsigned int)p->in_data.pdu_needed_len ));

	return (ssize_t)data_to_copy;
}

/****************************************************************************
 Accepts incoming data on an internal rpc pipe.
****************************************************************************/

static ssize_t write_to_internal_pipe(struct pipes_struct *p, char *data, size_t n)
{
	size_t data_left = n;

	while(data_left) {
		ssize_t data_used;

		DEBUG(10,("write_to_pipe: data_left = %u\n", (unsigned int)data_left ));

		data_used = process_incoming_data(p, data, data_left);

		DEBUG(10,("write_to_pipe: data_used = %d\n", (int)data_used ));

		if(data_used < 0) {
			return -1;
		}

		data_left -= data_used;
		data += data_used;
	}	

	return n;
}

/****************************************************************************
 Replies to a request to read data from a pipe.

 Headers are interspersed with the data at PDU intervals. By the time
 this function is called, the start of the data could possibly have been
 read by an SMBtrans (file_offset != 0).

 Calling create_rpc_reply() here is a hack. The data should already
 have been prepared into arrays of headers + data stream sections.
****************************************************************************/

static ssize_t read_from_internal_pipe(struct pipes_struct *p, char *data, size_t n,
				       bool *is_data_outstanding)
{
	uint32 pdu_remaining = 0;
	ssize_t data_returned = 0;

	if (!p) {
		DEBUG(0,("read_from_pipe: pipe not open\n"));
		return -1;		
	}

	DEBUG(6,(" name: %s len: %u\n",
		 get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
		 (unsigned int)n));

	/*
	 * We cannot return more than one PDU length per
	 * read request.
	 */

	/*
	 * This condition should result in the connection being closed.  
	 * Netapp filers seem to set it to 0xffff which results in domain
	 * authentications failing.  Just ignore it so things work.
	 */

	if(n > RPC_MAX_PDU_FRAG_LEN) {
                DEBUG(5,("read_from_pipe: too large read (%u) requested on "
			 "pipe %s. We can only service %d sized reads.\n",
			 (unsigned int)n,
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
			 RPC_MAX_PDU_FRAG_LEN ));
		n = RPC_MAX_PDU_FRAG_LEN;
	}

	/*
 	 * Determine if there is still data to send in the
	 * pipe PDU buffer. Always send this first. Never
	 * send more than is left in the current PDU. The
	 * client should send a new read request for a new
	 * PDU.
	 */

	pdu_remaining = prs_offset(&p->out_data.frag)
		- p->out_data.current_pdu_sent;

	if (pdu_remaining > 0) {
		data_returned = (ssize_t)MIN(n, pdu_remaining);

		DEBUG(10,("read_from_pipe: %s: current_pdu_len = %u, "
			  "current_pdu_sent = %u returning %d bytes.\n",
			  get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
			  (unsigned int)prs_offset(&p->out_data.frag),
			  (unsigned int)p->out_data.current_pdu_sent,
			  (int)data_returned));

		memcpy(data,
		       prs_data_p(&p->out_data.frag)
		       + p->out_data.current_pdu_sent,
		       data_returned);

		p->out_data.current_pdu_sent += (uint32)data_returned;
		goto out;
	}

	/*
	 * At this point p->current_pdu_len == p->current_pdu_sent (which
	 * may of course be zero if this is the first return fragment.
	 */

	DEBUG(10,("read_from_pipe: %s: fault_state = %d : data_sent_length "
		  "= %u, prs_offset(&p->out_data.rdata) = %u.\n",
		  get_pipe_name_from_syntax(talloc_tos(), &p->syntax),
		  (int)p->fault_state,
		  (unsigned int)p->out_data.data_sent_length,
		  (unsigned int)prs_offset(&p->out_data.rdata) ));

	if(p->out_data.data_sent_length >= prs_offset(&p->out_data.rdata)) {
		/*
		 * We have sent all possible data, return 0.
		 */
		data_returned = 0;
		goto out;
	}

	/*
	 * We need to create a new PDU from the data left in p->rdata.
	 * Create the header/data/footers. This also sets up the fields
	 * p->current_pdu_len, p->current_pdu_sent, p->data_sent_length
	 * and stores the outgoing PDU in p->current_pdu.
	 */

	if(!create_next_pdu(p)) {
		DEBUG(0,("read_from_pipe: %s: create_next_pdu failed.\n",
			 get_pipe_name_from_syntax(talloc_tos(), &p->syntax)));
		return -1;
	}

	data_returned = MIN(n, prs_offset(&p->out_data.frag));

	memcpy( data, prs_data_p(&p->out_data.frag), (size_t)data_returned);
	p->out_data.current_pdu_sent += (uint32)data_returned;

  out:
	(*is_data_outstanding) = prs_offset(&p->out_data.frag) > n;

	if (p->out_data.current_pdu_sent == prs_offset(&p->out_data.frag)) {
		/* We've returned everything in the out_data.frag
		 * so we're done with this pdu. Free it and reset
		 * current_pdu_sent. */
		p->out_data.current_pdu_sent = 0;
		prs_mem_free(&p->out_data.frag);
	}
	return data_returned;
}

/****************************************************************************
 Close an rpc pipe.
****************************************************************************/

static int close_internal_rpc_pipe_hnd(struct pipes_struct *p)
{
	if (!p) {
		DEBUG(0,("Invalid pipe in close_internal_rpc_pipe_hnd\n"));
		return False;
	}

	prs_mem_free(&p->out_data.frag);
	prs_mem_free(&p->out_data.rdata);
	prs_mem_free(&p->in_data.data);

	if (p->auth.auth_data_free_func) {
		(*p->auth.auth_data_free_func)(&p->auth);
	}

	TALLOC_FREE(p->mem_ctx);

	free_pipe_rpc_context( p->contexts );

	/* Free the handles database. */
	close_policy_by_pipe(p);

	DLIST_REMOVE(InternalPipes, p);

	ZERO_STRUCTP(p);

	TALLOC_FREE(p);
	
	return True;
}

bool fsp_is_np(struct files_struct *fsp)
{
	enum FAKE_FILE_TYPE type;

	if ((fsp == NULL) || (fsp->fake_file_handle == NULL)) {
		return false;
	}

	type = fsp->fake_file_handle->type;

	return ((type == FAKE_FILE_TYPE_NAMED_PIPE)
		|| (type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY));
}

struct np_proxy_state {
	struct tevent_queue *read_queue;
	struct tevent_queue *write_queue;
	int fd;

	uint8_t *msg;
	size_t sent;
};

static int np_proxy_state_destructor(struct np_proxy_state *state)
{
	if (state->fd != -1) {
		close(state->fd);
	}
	return 0;
}

static struct np_proxy_state *make_external_rpc_pipe_p(TALLOC_CTX *mem_ctx,
						       const char *pipe_name,
						       struct auth_serversupplied_info *server_info)
{
	struct np_proxy_state *result;
	struct sockaddr_un addr;
	char *socket_path;
	const char *socket_dir;

	DATA_BLOB req_blob;
	struct netr_SamInfo3 *info3;
	struct named_pipe_auth_req req;
	DATA_BLOB rep_blob;
	uint8 rep_buf[20];
	struct named_pipe_auth_rep rep;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	ssize_t written;

	result = talloc(mem_ctx, struct np_proxy_state);
	if (result == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	result->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (result->fd == -1) {
		DEBUG(10, ("socket(2) failed: %s\n", strerror(errno)));
		goto fail;
	}
	talloc_set_destructor(result, np_proxy_state_destructor);

	ZERO_STRUCT(addr);
	addr.sun_family = AF_UNIX;

	socket_dir = lp_parm_const_string(
		GLOBAL_SECTION_SNUM, "external_rpc_pipe", "socket_dir",
		get_dyn_NCALRPCDIR());
	if (socket_dir == NULL) {
		DEBUG(0, ("externan_rpc_pipe:socket_dir not set\n"));
		goto fail;
	}

	socket_path = talloc_asprintf(talloc_tos(), "%s/np/%s",
				      socket_dir, pipe_name);
	if (socket_path == NULL) {
		DEBUG(0, ("talloc_asprintf failed\n"));
		goto fail;
	}
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
	TALLOC_FREE(socket_path);

	become_root();
	if (sys_connect(result->fd, (struct sockaddr *)&addr) == -1) {
		unbecome_root();
		DEBUG(0, ("connect(%s) failed: %s\n", addr.sun_path,
			  strerror(errno)));
		goto fail;
	}
	unbecome_root();

	info3 = talloc(talloc_tos(), struct netr_SamInfo3);
	if (info3 == NULL) {
		DEBUG(0, ("talloc failed\n"));
		goto fail;
	}

	status = serverinfo_to_SamInfo3(server_info, NULL, 0, info3);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(info3);
		DEBUG(0, ("serverinfo_to_SamInfo3 failed: %s\n",
			  nt_errstr(status)));
		goto fail;
	}

	req.level = 1;
	req.info.info1 = *info3;

	ndr_err = ndr_push_struct_blob(
		&req_blob, talloc_tos(), NULL, &req,
		(ndr_push_flags_fn_t)ndr_push_named_pipe_auth_req);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(10, ("ndr_push_named_pipe_auth_req failed: %s\n",
			   ndr_errstr(ndr_err)));
		goto fail;
	}

	DEBUG(10, ("named_pipe_auth_req(client)[%u]\n", (uint32_t)req_blob.length));
	dump_data(10, req_blob.data, req_blob.length);

	written = write_data(result->fd, (char *)req_blob.data,
			     req_blob.length);
	if (written == -1) {
		DEBUG(3, ("Could not write auth req data to RPC server\n"));
		goto fail;
	}

	status = read_data(result->fd, (char *)rep_buf, sizeof(rep_buf));
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3, ("Could not read auth result\n"));
		goto fail;
	}

	rep_blob = data_blob_const(rep_buf, sizeof(rep_buf));

	DEBUG(10,("name_pipe_auth_rep(client)[%u]\n", (uint32_t)rep_blob.length));
	dump_data(10, rep_blob.data, rep_blob.length);

	ndr_err = ndr_pull_struct_blob(
		&rep_blob, talloc_tos(), NULL, &rep,
		(ndr_pull_flags_fn_t)ndr_pull_named_pipe_auth_rep);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0, ("ndr_pull_named_pipe_auth_rep failed: %s\n",
			  ndr_errstr(ndr_err)));
		goto fail;
	}

	if (rep.length != 16) {
		DEBUG(0, ("req invalid length: %u != 16\n",
			  rep.length));
		goto fail;
	}

	if (strcmp(NAMED_PIPE_AUTH_MAGIC, rep.magic) != 0) {
		DEBUG(0, ("req invalid magic: %s != %s\n",
			  rep.magic, NAMED_PIPE_AUTH_MAGIC));
		goto fail;
	}

	if (!NT_STATUS_IS_OK(rep.status)) {
		DEBUG(0, ("req failed: %s\n",
			  nt_errstr(rep.status)));
		goto fail;
	}

	if (rep.level != 1) {
		DEBUG(0, ("req invalid level: %u != 1\n",
			  rep.level));
		goto fail;
	}

	result->msg = NULL;

	result->read_queue = tevent_queue_create(result, "np_read");
	if (result->read_queue == NULL) {
		goto fail;
	}
	result->write_queue = tevent_queue_create(result, "np_write");
	if (result->write_queue == NULL) {
		goto fail;
	}

	return result;

 fail:
	TALLOC_FREE(result);
	return NULL;
}

NTSTATUS np_open(TALLOC_CTX *mem_ctx, const char *name,
		 const char *client_address,
		 struct auth_serversupplied_info *server_info,
		 struct fake_file_handle **phandle)
{
	const char **proxy_list;
	struct fake_file_handle *handle;

	proxy_list = lp_parm_string_list(-1, "np", "proxy", NULL);

	handle = talloc(mem_ctx, struct fake_file_handle);
	if (handle == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if ((proxy_list != NULL) && str_list_check_ci(proxy_list, name)) {
		struct np_proxy_state *p;

		p = make_external_rpc_pipe_p(handle, name, server_info);

		handle->type = FAKE_FILE_TYPE_NAMED_PIPE_PROXY;
		handle->private_data = p;
	} else {
		struct pipes_struct *p;
		struct ndr_syntax_id syntax;

		if (!is_known_pipename(name, &syntax)) {
			TALLOC_FREE(handle);
			return NT_STATUS_OBJECT_NAME_NOT_FOUND;
		}

		p = make_internal_rpc_pipe_p(handle, &syntax, client_address,
					     server_info);

		handle->type = FAKE_FILE_TYPE_NAMED_PIPE;
		handle->private_data = p;
	}

	if (handle->private_data == NULL) {
		TALLOC_FREE(handle);
		return NT_STATUS_PIPE_NOT_AVAILABLE;
	}

	*phandle = handle;

	return NT_STATUS_OK;
}

struct np_write_state {
	struct event_context *ev;
	struct np_proxy_state *p;
	struct iovec iov;
	ssize_t nwritten;
};

static void np_write_done(struct tevent_req *subreq);

struct tevent_req *np_write_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct fake_file_handle *handle,
				 const uint8_t *data, size_t len)
{
	struct tevent_req *req;
	struct np_write_state *state;
	NTSTATUS status;

	DEBUG(6, ("np_write_send: len: %d\n", (int)len));
	dump_data(50, data, len);

	req = tevent_req_create(mem_ctx, &state, struct np_write_state);
	if (req == NULL) {
		return NULL;
	}

	if (len == 0) {
		state->nwritten = 0;
		status = NT_STATUS_OK;
		goto post_status;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE) {
		struct pipes_struct *p = talloc_get_type_abort(
			handle->private_data, struct pipes_struct);

		state->nwritten = write_to_internal_pipe(p, (char *)data, len);

		status = (state->nwritten >= 0)
			? NT_STATUS_OK : NT_STATUS_UNEXPECTED_IO_ERROR;
		goto post_status;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY) {
		struct np_proxy_state *p = talloc_get_type_abort(
			handle->private_data, struct np_proxy_state);
		struct tevent_req *subreq;

		state->ev = ev;
		state->p = p;
		state->iov.iov_base = CONST_DISCARD(void *, data);
		state->iov.iov_len = len;

		subreq = writev_send(state, ev, p->write_queue, p->fd,
				     false, &state->iov, 1);
		if (subreq == NULL) {
			goto fail;
		}
		tevent_req_set_callback(subreq, np_write_done, req);
		return req;
	}

	status = NT_STATUS_INVALID_HANDLE;
 post_status:
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, status);
	}
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void np_write_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct np_write_state *state = tevent_req_data(
		req, struct np_write_state);
	ssize_t received;
	int err;

	received = writev_recv(subreq, &err);
	if (received < 0) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}
	state->nwritten = received;
	tevent_req_done(req);
}

NTSTATUS np_write_recv(struct tevent_req *req, ssize_t *pnwritten)
{
	struct np_write_state *state = tevent_req_data(
		req, struct np_write_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pnwritten = state->nwritten;
	return NT_STATUS_OK;
}

static ssize_t rpc_frag_more_fn(uint8_t *buf, size_t buflen, void *priv)
{
	prs_struct hdr_prs;
	struct rpc_hdr_info hdr;
	bool ret;

	if (buflen > RPC_HEADER_LEN) {
		return 0;
	}
	prs_init_empty(&hdr_prs, talloc_tos(), UNMARSHALL);
	prs_give_memory(&hdr_prs, (char *)buf, RPC_HEADER_LEN, false);
	ret = smb_io_rpc_hdr("", &hdr, &hdr_prs, 0);
	prs_mem_free(&hdr_prs);

	if (!ret) {
		return -1;
	}

	return (hdr.frag_len - RPC_HEADER_LEN);
}

struct np_read_state {
	struct event_context *ev;
	struct np_proxy_state *p;
	uint8_t *data;
	size_t len;

	size_t nread;
	bool is_data_outstanding;
};

static void np_read_trigger(struct tevent_req *req, void *private_data);
static void np_read_done(struct tevent_req *subreq);

struct tevent_req *np_read_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				struct fake_file_handle *handle,
				uint8_t *data, size_t len)
{
	struct tevent_req *req;
	struct np_read_state *state;
	NTSTATUS status;

	req = tevent_req_create(mem_ctx, &state, struct np_read_state);
	if (req == NULL) {
		return NULL;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE) {
		struct pipes_struct *p = talloc_get_type_abort(
			handle->private_data, struct pipes_struct);

		state->nread = read_from_internal_pipe(
			p, (char *)data, len, &state->is_data_outstanding);

		status = (state->nread >= 0)
			? NT_STATUS_OK : NT_STATUS_UNEXPECTED_IO_ERROR;
		goto post_status;
	}

	if (handle->type == FAKE_FILE_TYPE_NAMED_PIPE_PROXY) {
		struct np_proxy_state *p = talloc_get_type_abort(
			handle->private_data, struct np_proxy_state);

		if (p->msg != NULL) {
			size_t thistime;

			thistime = MIN(talloc_get_size(p->msg) - p->sent,
				       len);

			memcpy(data, p->msg+p->sent, thistime);
			state->nread = thistime;
			p->sent += thistime;

			if (p->sent < talloc_get_size(p->msg)) {
				state->is_data_outstanding = true;
			} else {
				state->is_data_outstanding = false;
				TALLOC_FREE(p->msg);
			}
			status = NT_STATUS_OK;
			goto post_status;
		}

		state->ev = ev;
		state->p = p;
		state->data = data;
		state->len = len;

		if (!tevent_queue_add(p->read_queue, ev, req, np_read_trigger,
				      NULL)) {
			goto fail;
		}
		return req;
	}

	status = NT_STATUS_INVALID_HANDLE;
 post_status:
	if (NT_STATUS_IS_OK(status)) {
		tevent_req_done(req);
	} else {
		tevent_req_nterror(req, status);
	}
	return tevent_req_post(req, ev);
 fail:
	TALLOC_FREE(req);
	return NULL;
}

static void np_read_trigger(struct tevent_req *req, void *private_data)
{
	struct np_read_state *state = tevent_req_data(
		req, struct np_read_state);
	struct tevent_req *subreq;

	subreq = read_packet_send(state, state->ev, state->p->fd,
				  RPC_HEADER_LEN, rpc_frag_more_fn, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, np_read_done, req);
}

static void np_read_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct np_read_state *state = tevent_req_data(
		req, struct np_read_state);
	ssize_t received;
	size_t thistime;
	int err;

	received = read_packet_recv(subreq, state->p, &state->p->msg, &err);
	TALLOC_FREE(subreq);
	if (received == -1) {
		tevent_req_nterror(req, map_nt_error_from_unix(err));
		return;
	}

	thistime = MIN(received, state->len);

	memcpy(state->data, state->p->msg, thistime);
	state->p->sent = thistime;
	state->nread = thistime;

	if (state->p->sent < received) {
		state->is_data_outstanding = true;
	} else {
		TALLOC_FREE(state->p->msg);
		state->is_data_outstanding = false;
	}

	tevent_req_done(req);
	return;
}

NTSTATUS np_read_recv(struct tevent_req *req, ssize_t *nread,
		      bool *is_data_outstanding)
{
	struct np_read_state *state = tevent_req_data(
		req, struct np_read_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*nread = state->nread;
	*is_data_outstanding = state->is_data_outstanding;
	return NT_STATUS_OK;
}

/**
 * Create a new RPC client context which uses a local dispatch function.
 */
NTSTATUS rpc_pipe_open_internal(TALLOC_CTX *mem_ctx,
				const struct ndr_syntax_id *abstract_syntax,
				NTSTATUS (*dispatch) (struct rpc_pipe_client *cli,
						      TALLOC_CTX *mem_ctx,
						      const struct ndr_interface_table *table,
						      uint32_t opnum, void *r),
				struct auth_serversupplied_info *serversupplied_info,
				struct rpc_pipe_client **presult)
{
	struct rpc_pipe_client *result;

	result = TALLOC_ZERO_P(mem_ctx, struct rpc_pipe_client);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	result->abstract_syntax = *abstract_syntax;
	result->transfer_syntax = ndr_transfer_syntax;
	result->dispatch = dispatch;

	result->pipes_struct = make_internal_rpc_pipe_p(
		result, abstract_syntax, "", serversupplied_info);
	if (result->pipes_struct == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	result->max_xmit_frag = -1;
	result->max_recv_frag = -1;

	*presult = result;
	return NT_STATUS_OK;
}

/*******************************************************************
 gets a domain user's groups from their already-calculated NT_USER_TOKEN
 ********************************************************************/

static NTSTATUS nt_token_to_group_list(TALLOC_CTX *mem_ctx,
				       const DOM_SID *domain_sid,
				       size_t num_sids,
				       const DOM_SID *sids,
				       int *numgroups,
				       struct samr_RidWithAttribute **pgids)
{
	int i;

	*numgroups=0;
	*pgids = NULL;

	for (i=0; i<num_sids; i++) {
		struct samr_RidWithAttribute gid;
		if (!sid_peek_check_rid(domain_sid, &sids[i], &gid.rid)) {
			continue;
		}
		gid.attributes = (SE_GROUP_MANDATORY|SE_GROUP_ENABLED_BY_DEFAULT|
			    SE_GROUP_ENABLED);
		ADD_TO_ARRAY(mem_ctx, struct samr_RidWithAttribute,
			     gid, pgids, numgroups);
		if (*pgids == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
	}
	return NT_STATUS_OK;
}

/****************************************************************************
 inits a netr_SamBaseInfo structure from an auth_serversupplied_info.
*****************************************************************************/

static NTSTATUS serverinfo_to_SamInfo_base(TALLOC_CTX *mem_ctx,
					   struct auth_serversupplied_info *server_info,
					   uint8_t *pipe_session_key,
					   size_t pipe_session_key_len,
					   struct netr_SamBaseInfo *base)
{
	struct samu *sampw;
	struct samr_RidWithAttribute *gids = NULL;
	const DOM_SID *user_sid = NULL;
	const DOM_SID *group_sid = NULL;
	DOM_SID domain_sid;
	uint32 user_rid, group_rid;
	NTSTATUS status;

	int num_gids = 0;
	const char *my_name;

	struct netr_UserSessionKey user_session_key;
	struct netr_LMSessionKey lm_session_key;

	NTTIME last_logon, last_logoff, acct_expiry, last_password_change;
	NTTIME allow_password_change, force_password_change;
	struct samr_RidWithAttributeArray groups;
	int i;
	struct dom_sid2 *sid = NULL;

	ZERO_STRUCT(user_session_key);
	ZERO_STRUCT(lm_session_key);

	sampw = server_info->sam_account;

	user_sid = pdb_get_user_sid(sampw);
	group_sid = pdb_get_group_sid(sampw);

	if (pipe_session_key && pipe_session_key_len != 16) {
		DEBUG(0,("serverinfo_to_SamInfo3: invalid "
			 "pipe_session_key_len[%zu] != 16\n",
			 pipe_session_key_len));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if ((user_sid == NULL) || (group_sid == NULL)) {
		DEBUG(1, ("_netr_LogonSamLogon: User without group or user SID\n"));
		return NT_STATUS_UNSUCCESSFUL;
	}

	sid_copy(&domain_sid, user_sid);
	sid_split_rid(&domain_sid, &user_rid);

	sid = sid_dup_talloc(mem_ctx, &domain_sid);
	if (!sid) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!sid_peek_check_rid(&domain_sid, group_sid, &group_rid)) {
		DEBUG(1, ("_netr_LogonSamLogon: user %s\\%s has user sid "
			  "%s\n but group sid %s.\n"
			  "The conflicting domain portions are not "
			  "supported for NETLOGON calls\n",
			  pdb_get_domain(sampw),
			  pdb_get_username(sampw),
			  sid_string_dbg(user_sid),
			  sid_string_dbg(group_sid)));
		return NT_STATUS_UNSUCCESSFUL;
	}

	if(server_info->login_server) {
		my_name = server_info->login_server;
	} else {
		my_name = global_myname();
	}

	status = nt_token_to_group_list(mem_ctx, &domain_sid,
					server_info->num_sids,
					server_info->sids,
					&num_gids, &gids);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (server_info->user_session_key.length) {
		memcpy(user_session_key.key,
		       server_info->user_session_key.data,
		       MIN(sizeof(user_session_key.key),
			   server_info->user_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(user_session_key.key, pipe_session_key, 16);
		}
	}
	if (server_info->lm_session_key.length) {
		memcpy(lm_session_key.key,
		       server_info->lm_session_key.data,
		       MIN(sizeof(lm_session_key.key),
			   server_info->lm_session_key.length));
		if (pipe_session_key) {
			arcfour_crypt(lm_session_key.key, pipe_session_key, 8);
		}
	}

	groups.count = num_gids;
	groups.rids = TALLOC_ARRAY(mem_ctx, struct samr_RidWithAttribute, groups.count);
	if (!groups.rids) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i < groups.count; i++) {
		groups.rids[i].rid = gids[i].rid;
		groups.rids[i].attributes = gids[i].attributes;
	}

	unix_to_nt_time(&last_logon, pdb_get_logon_time(sampw));
	unix_to_nt_time(&last_logoff, get_time_t_max());
	unix_to_nt_time(&acct_expiry, get_time_t_max());
	unix_to_nt_time(&last_password_change, pdb_get_pass_last_set_time(sampw));
	unix_to_nt_time(&allow_password_change, pdb_get_pass_can_change_time(sampw));
	unix_to_nt_time(&force_password_change, pdb_get_pass_must_change_time(sampw));

	base->last_logon		= last_logon;
	base->last_logoff		= last_logoff;
	base->acct_expiry		= acct_expiry;
	base->last_password_change	= last_password_change;
	base->allow_password_change	= allow_password_change;
	base->force_password_change	= force_password_change;
	base->account_name.string	= talloc_strdup(mem_ctx, pdb_get_username(sampw));
	base->full_name.string		= talloc_strdup(mem_ctx, pdb_get_fullname(sampw));
	base->logon_script.string	= talloc_strdup(mem_ctx, pdb_get_logon_script(sampw));
	base->profile_path.string	= talloc_strdup(mem_ctx, pdb_get_profile_path(sampw));
	base->home_directory.string	= talloc_strdup(mem_ctx, pdb_get_homedir(sampw));
	base->home_drive.string		= talloc_strdup(mem_ctx, pdb_get_dir_drive(sampw));
	base->logon_count		= 0; /* ?? */
	base->bad_password_count	= 0; /* ?? */
	base->rid			= user_rid;
	base->primary_gid		= group_rid;
	base->groups			= groups;
	base->user_flags		= NETLOGON_EXTRA_SIDS;
	base->key			= user_session_key;
	base->logon_server.string	= talloc_strdup(mem_ctx, my_name);
	base->domain.string		= talloc_strdup(mem_ctx, pdb_get_domain(sampw));
	base->domain_sid		= sid;
	base->LMSessKey			= lm_session_key;
	base->acct_flags		= pdb_get_acct_ctrl(sampw);

	ZERO_STRUCT(user_session_key);
	ZERO_STRUCT(lm_session_key);

	return NT_STATUS_OK;
}

/****************************************************************************
 inits a netr_SamInfo2 structure from an auth_serversupplied_info. sam2 must
 already be initialized and is used as the talloc parent for its members.
*****************************************************************************/

NTSTATUS serverinfo_to_SamInfo2(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo2 *sam2)
{
	NTSTATUS status;

	status = serverinfo_to_SamInfo_base(sam2,
					    server_info,
					    pipe_session_key,
					    pipe_session_key_len,
					    &sam2->base);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/****************************************************************************
 inits a netr_SamInfo3 structure from an auth_serversupplied_info. sam3 must
 already be initialized and is used as the talloc parent for its members.
*****************************************************************************/

NTSTATUS serverinfo_to_SamInfo3(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo3 *sam3)
{
	NTSTATUS status;

	status = serverinfo_to_SamInfo_base(sam3,
					    server_info,
					    pipe_session_key,
					    pipe_session_key_len,
					    &sam3->base);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	sam3->sidcount		= 0;
	sam3->sids		= NULL;

	return NT_STATUS_OK;
}

/****************************************************************************
 inits a netr_SamInfo6 structure from an auth_serversupplied_info. sam6 must
 already be initialized and is used as the talloc parent for its members.
*****************************************************************************/

NTSTATUS serverinfo_to_SamInfo6(struct auth_serversupplied_info *server_info,
				uint8_t *pipe_session_key,
				size_t pipe_session_key_len,
				struct netr_SamInfo6 *sam6)
{
	NTSTATUS status;
	struct pdb_domain_info *dominfo;

	if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
		DEBUG(10,("Not adding validation info level 6 "
			   "without ADS passdb backend\n"));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	dominfo = pdb_get_domain_info(sam6);
	if (dominfo == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = serverinfo_to_SamInfo_base(sam6,
					    server_info,
					    pipe_session_key,
					    pipe_session_key_len,
					    &sam6->base);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	sam6->sidcount		= 0;
	sam6->sids		= NULL;

	sam6->forest.string	= talloc_strdup(sam6, dominfo->dns_forest);
	if (sam6->forest.string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	sam6->principle.string	= talloc_asprintf(sam6, "%s@%s",
						  pdb_get_username(server_info->sam_account),
						  dominfo->dns_domain);
	if (sam6->principle.string == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}
