/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Jeremy Allison				    1999.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"


#define	PIPE		"\\PIPE\\"
#define	PIPELEN		strlen(PIPE)

extern int DEBUGLEVEL;
static pipes_struct *chain_p;
static int pipes_open;

#ifndef MAX_OPEN_PIPES
#define MAX_OPEN_PIPES 64
#endif

static pipes_struct *Pipes;
static struct bitmap *bmap;

/* this must be larger than the sum of the open files and directories */
static int pipe_handle_offset;

/****************************************************************************
 Set the pipe_handle_offset. Called from smbd/files.c
****************************************************************************/

void set_pipe_handle_offset(int max_open_files)
{
  if(max_open_files < 0x7000)
    pipe_handle_offset = 0x7000;
  else
    pipe_handle_offset = max_open_files + 10; /* For safety. :-) */
}

/****************************************************************************
 Reset pipe chain handle number.
****************************************************************************/
void reset_chain_p(void)
{
	chain_p = NULL;
}

/****************************************************************************
 Initialise pipe handle states.
****************************************************************************/

void init_rpc_pipe_hnd(void)
{
	bmap = bitmap_allocate(MAX_OPEN_PIPES);
	if (!bmap)
		exit_server("out of memory in init_rpc_pipe_hnd\n");
}

/****************************************************************************
 Initialise an outgoing packet.
****************************************************************************/

static BOOL pipe_init_outgoing_data(output_data *o_data)
{
	/* Reset the offset counters. */
	o_data->data_sent_length = 0;
	o_data->current_pdu_len = 0;
	o_data->current_pdu_sent = 0;

	memset(o_data->current_pdu, '\0', sizeof(o_data->current_pdu));

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&o_data->rdata);

	/*
	 * Initialize the outgoing RPC data buffer.
	 * we will use this as the raw data area for replying to rpc requests.
	 */	
	if(!prs_init(&o_data->rdata, MAX_PDU_FRAG_LEN, 4, MARSHALL)) {
		DEBUG(0,("pipe_init_outgoing_data: malloc fail.\n"));
		return False;
	}

	return True;
}

/****************************************************************************
 Find first available pipe slot.
****************************************************************************/

pipes_struct *open_rpc_pipe_p(char *pipe_name, 
			      connection_struct *conn, uint16 vuid)
{
	int i;
	pipes_struct *p;
	static int next_pipe;

	DEBUG(4,("Open pipe requested %s (pipes_open=%d)\n",
		 pipe_name, pipes_open));
	
	/* not repeating pipe numbers makes it easier to track things in 
	   log files and prevents client bugs where pipe numbers are reused
	   over connection restarts */
	if (next_pipe == 0)
		next_pipe = (getpid() ^ time(NULL)) % MAX_OPEN_PIPES;

	i = bitmap_find(bmap, next_pipe);

	if (i == -1) {
		DEBUG(0,("ERROR! Out of pipe structures\n"));
		return NULL;
	}

	next_pipe = (i+1) % MAX_OPEN_PIPES;

	for (p = Pipes; p; p = p->next)
		DEBUG(5,("open pipes: name %s pnum=%x\n", p->name, p->pnum));  

	p = (pipes_struct *)malloc(sizeof(*p));

	if (!p)
		return NULL;

	ZERO_STRUCTP(p);

	DLIST_ADD(Pipes, p);

	/*
	 * Initialize the incoming RPC data buffer with one PDU worth of memory.
	 * We cheat here and say we're marshalling, as we intend to add incoming
	 * data directly into the prs_struct and we want it to auto grow. We will
	 * change the type to UNMARSALLING before processing the stream.
	 */

	if(!prs_init(&p->in_data.data, MAX_PDU_FRAG_LEN, 4, MARSHALL)) {
		DEBUG(0,("open_rpc_pipe_p: malloc fail for in_data struct.\n"));
		return NULL;
	}

	bitmap_set(bmap, i);
	i += pipe_handle_offset;

	pipes_open++;

	p->pnum = i;

	p->open = True;
	p->device_state = 0;
	p->priority = 0;
	p->conn = conn;
	p->vuid  = vuid;

	p->max_trans_reply = 0;
	
	p->ntlmssp_chal_flags = 0;
	p->ntlmssp_auth_validated = False;
	p->ntlmssp_auth_requested = False;

	p->pipe_bound = False;
	p->fault_state = False;

	/*
	 * Initialize the incoming RPC struct.
	 */

	p->in_data.pdu_needed_len = 0;
	p->in_data.pdu_received_len = 0;

	/*
	 * Initialize the outgoing RPC struct.
	 */

	p->out_data.current_pdu_len = 0;
	p->out_data.current_pdu_sent = 0;
	p->out_data.data_sent_length = 0;

	/*
	 * Initialize the outgoing RPC data buffer with no memory.
	 */	
	prs_init(&p->out_data.rdata, 0, 4, MARSHALL);
	
	p->uid = (uid_t)-1;
	p->gid = (gid_t)-1;
	
	fstrcpy(p->name, pipe_name);
	
	DEBUG(4,("Opened pipe %s with handle %x (pipes_open=%d)\n",
		 pipe_name, i, pipes_open));
	
	chain_p = p;
	
	/* OVERWRITE p as a temp variable, to display all open pipes */ 
	for (p = Pipes; p; p = p->next)
		DEBUG(5,("open pipes: name %s pnum=%x\n", p->name, p->pnum));  

	return chain_p;
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
	DEBUG(10,("set_incoming_fault: Setting fault state on pipe %s : pnum = 0x%x\n",
		p->name, p->pnum ));
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

	prs_init( &rpc_in, 0, 4, UNMARSHALL);
	prs_give_memory( &rpc_in, (char *)&p->in_data.current_in_pdu[0],
					p->in_data.pdu_received_len, False);

	/*
	 * Unmarshall the header as this will tell us how much
	 * data we need to read to get the complete pdu.
	 */

	if(!smb_io_rpc_hdr("", &p->hdr, &rpc_in, 0)) {
		DEBUG(0,("unmarshall_rpc_header: failed to unmarshall RPC_HDR.\n"));
		set_incoming_fault(p);
		return -1;
	}

	/*
	 * Validate the RPC header.
	 */

	if(p->hdr.major != 5 && p->hdr.minor != 0) {
		DEBUG(0,("unmarshall_rpc_header: invalid major/minor numbers in RPC_HDR.\n"));
		set_incoming_fault(p);
		return -1;
	}

	/*
	 * If there is no data in the incoming buffer and it's a requst pdu then
	 * ensure that the FIRST flag is set. If not then we have
	 * a stream missmatch.
	 */

	if((p->hdr.pkt_type == RPC_REQUEST) && (prs_offset(&p->in_data.data) == 0) && !(p->hdr.flags & RPC_FLG_FIRST)) {
		DEBUG(0,("unmarshall_rpc_header: FIRST flag not set in first PDU !\n"));
		set_incoming_fault(p);
		return -1;
	}

	/*
	 * Ensure that the pdu length is sane.
	 */

	if((p->hdr.frag_len < RPC_HEADER_LEN) || (p->hdr.frag_len > MAX_PDU_FRAG_LEN)) {
		DEBUG(0,("unmarshall_rpc_header: assert on frag length failed.\n"));
		set_incoming_fault(p);
		return -1;
	}

	DEBUG(10,("unmarshall_rpc_header: type = %u, flags = %u\n", (unsigned int)p->hdr.pkt_type,
			(unsigned int)p->hdr.flags ));

	/*
	 * Adjust for the header we just ate.
	 */
	p->in_data.pdu_received_len = 0;
	p->in_data.pdu_needed_len = (uint32)p->hdr.frag_len - RPC_HEADER_LEN;

	/*
	 * Null the data we just ate.
	 */

	memset((char *)&p->in_data.current_in_pdu[0], '\0', RPC_HEADER_LEN);

	return 0; /* No extra data processed. */
}

/****************************************************************************
 Processes a request pdu. This will do auth processing if needed, and
 appends the data into the complete stream if the LAST flag is not set.
****************************************************************************/

static BOOL process_request_pdu(pipes_struct *p, prs_struct *rpc_in_p)
{
	BOOL auth_verify = IS_BITS_SET_ALL(p->ntlmssp_chal_flags, NTLMSSP_NEGOTIATE_SIGN);
	size_t data_len = p->hdr.frag_len - RPC_HEADER_LEN - RPC_HDR_REQ_LEN -
				(auth_verify ? RPC_HDR_AUTH_LEN : 0) - p->hdr.auth_len;

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

	if(p->ntlmssp_auth_validated && !api_pipe_auth_process(p, rpc_in_p)) {
		DEBUG(0,("process_request_pdu: failed to do auth processing.\n"));
		set_incoming_fault(p);
		return False;
	}

	if (p->ntlmssp_auth_requested && !p->ntlmssp_auth_validated) {

		/*
		 * Authentication _was_ requested and it already failed.
		 */

		DEBUG(0,("process_request_pdu: RPC request received on pipe %s where \
authentication failed. Denying the request.\n", p->name));
		set_incoming_fault(p);
        return False;
    }

	/*
	 * Check the data length doesn't go over the 1Mb limit.
	 */
	
	if(prs_data_size(&p->in_data.data) + data_len > 1024*1024) {
		DEBUG(0,("process_request_pdu: rpc data buffer too large (%u) + (%u)\n",
				(unsigned int)prs_data_size(&p->in_data.data), (unsigned int)data_len ));
		set_incoming_fault(p);
		return False;
	}

	/*
	 * Append the data portion into the buffer and return.
	 */

	{
		char *data_from = prs_data_p(rpc_in_p) + prs_offset(rpc_in_p);

		if(!prs_append_data(&p->in_data.data, data_from, data_len)) {
			DEBUG(0,("process_request_pdu: Unable to append data size %u to parse buffer of size %u.\n",
					(unsigned int)data_len, (unsigned int)prs_data_size(&p->in_data.data) ));
			set_incoming_fault(p);
			return False;
		}

	}

	if(p->hdr.flags & RPC_FLG_LAST) {
		BOOL ret = False;
		/*
		 * Ok - we finally have a complete RPC stream.
		 * Call the rpc command to process it.
		 */

		/*
		 * Set the parse offset to the start of the data and set the
		 * prs_struct to UNMARSHALL.
		 */

		prs_set_offset(&p->in_data.data, 0);
		prs_switch_type(&p->in_data.data, UNMARSHALL);

		/*
		 * Process the complete data stream here.
		 */

		if(pipe_init_outgoing_data(&p->out_data))
			ret = api_pipe_request(p);

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

static ssize_t process_complete_pdu(pipes_struct *p)
{
	prs_struct rpc_in;
	size_t data_len = p->in_data.pdu_received_len;
	char *data_p = (char *)&p->in_data.current_in_pdu[0];
	BOOL reply = False;

	if(p->fault_state) {
		DEBUG(10,("process_complete_pdu: pipe %s in fault state.\n",
			p->name ));
		set_incoming_fault(p);
		setup_fault_pdu(p);
		return (ssize_t)data_len;
	}

	prs_init( &rpc_in, 0, 4, UNMARSHALL);
	prs_give_memory( &rpc_in, data_p, (uint32)data_len, False);

	DEBUG(10,("process_complete_pdu: processing packet type %u\n",
			(unsigned int)p->hdr.pkt_type ));

	switch (p->hdr.pkt_type) {
		case RPC_BIND:
		case RPC_ALTCONT:
			/*
			 * We assume that a pipe bind is only in one pdu.
			 */
			if(pipe_init_outgoing_data(&p->out_data))
				reply = api_pipe_bind_req(p, &rpc_in);
			break;
		case RPC_BINDRESP:
			/*
			 * We assume that a pipe bind_resp is only in one pdu.
			 */
			if(pipe_init_outgoing_data(&p->out_data))
				reply = api_pipe_bind_auth_resp(p, &rpc_in);
			break;
		case RPC_REQUEST:
			reply = process_request_pdu(p, &rpc_in);
			break;
		default:
			DEBUG(0,("process_complete_pdu: Unknown rpc type = %u received.\n", (unsigned int)p->hdr.pkt_type ));
			break;
	}

	if (!reply) {
		DEBUG(3,("process_complete_pdu: DCE/RPC fault sent on pipe %s\n", p->pipe_srv_name));
		set_incoming_fault(p);
		setup_fault_pdu(p);
	} else {
		/*
		 * Reset the lengths. We're ready for a new pdu.
		 */
		p->in_data.pdu_needed_len = 0;
		p->in_data.pdu_received_len = 0;
	}

	return (ssize_t)data_len;
}

/****************************************************************************
 Accepts incoming data on an rpc pipe. Processes the data in pdu sized units.
****************************************************************************/

static ssize_t process_incoming_data(pipes_struct *p, char *data, size_t n)
{
	size_t data_to_copy = MIN(n, MAX_PDU_FRAG_LEN - p->in_data.pdu_received_len);

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

	if(p->in_data.pdu_needed_len == 0)
		return unmarshall_rpc_header(p);

	/*
	 * Ok - at this point we have a valid RPC_HEADER in p->hdr.
	 * Keep reading until we have a full pdu.
	 */

	data_to_copy = MIN(data_to_copy, p->in_data.pdu_needed_len);

	/*
	 * Copy as much of the data as we need into the current_in_pdu buffer.
	 */

	memcpy( (char *)&p->in_data.current_in_pdu[p->in_data.pdu_received_len], data, data_to_copy);
	p->in_data.pdu_received_len += data_to_copy;

	/*
	 * Do we have a complete PDU ?
	 */

	if(p->in_data.pdu_received_len == p->in_data.pdu_needed_len)
		return process_complete_pdu(p);

	DEBUG(10,("process_incoming_data: not a complete PDU yet. pdu_received_len = %u, pdu_needed_len = %u\n",
		(unsigned int)p->in_data.pdu_received_len, (unsigned int)p->in_data.pdu_needed_len ));

	return (ssize_t)data_to_copy;

}

/****************************************************************************
 Accepts incoming data on an rpc pipe.
****************************************************************************/

ssize_t write_to_pipe(pipes_struct *p, char *data, size_t n)
{
	size_t data_left = n;

	DEBUG(6,("write_to_pipe: %x", p->pnum));

	DEBUG(6,(" name: %s open: %s len: %d\n",
		 p->name, BOOLSTR(p->open), (int)n));

	dump_data(50, data, n);

	while(data_left) {
		ssize_t data_used;

		DEBUG(10,("write_to_pipe: data_left = %u\n", (unsigned int)data_left ));

		data_used = process_incoming_data(p, data, data_left);

		DEBUG(10,("write_to_pipe: data_used = %d\n", (int)data_used ));

		if(data_used < 0)
			return -1;

		data_left -= data_used;
		data += data_used;
	}	

	return n;
}


/****************************************************************************
 Replyies to a request to read data from a pipe.

 Headers are interspersed with the data at PDU intervals. By the time
 this function is called, the start of the data could possibly have been
 read by an SMBtrans (file_offset != 0).

 Calling create_rpc_reply() here is a hack. The data should already
 have been prepared into arrays of headers + data stream sections.

 ****************************************************************************/

ssize_t read_from_pipe(pipes_struct *p, char *data, size_t n)
{
	uint32 pdu_remaining = 0;
	ssize_t data_returned = 0;

	if (!p || !p->open) {
		DEBUG(0,("read_from_pipe: pipe not open\n"));
		return -1;		
	}

	DEBUG(6,("read_from_pipe: %x", p->pnum));

	DEBUG(6,(" name: %s len: %u\n", p->name, (unsigned int)n));

	/*
	 * We cannot return more than one PDU length per
	 * read request.
	 */

	if(n > MAX_PDU_FRAG_LEN) {
		DEBUG(0,("read_from_pipe: loo large read (%u) requested on pipe %s. We can \
only service %d sized reads.\n", (unsigned int)n, p->name, MAX_PDU_FRAG_LEN ));
		return -1;
	}

	/*
 	 * Determine if there is still data to send in the
	 * pipe PDU buffer. Always send this first. Never
	 * send more than is left in the current PDU. The
	 * client should send a new read request for a new
	 * PDU.
	 */

	if((pdu_remaining = p->out_data.current_pdu_len - p->out_data.current_pdu_sent) > 0) {
		data_returned = (ssize_t)MIN(n, pdu_remaining);

		DEBUG(10,("read_from_pipe: %s: current_pdu_len = %u, current_pdu_sent = %u \
returning %d bytes.\n", p->name, (unsigned int)p->out_data.current_pdu_len, 
			(unsigned int)p->out_data.current_pdu_sent, (int)data_returned));

		memcpy( data, &p->out_data.current_pdu[p->out_data.current_pdu_sent], (size_t)data_returned);
		p->out_data.current_pdu_sent += (uint32)data_returned;
		return data_returned;
	}

	/*
	 * At this point p->current_pdu_len == p->current_pdu_sent (which
	 * may of course be zero if this is the first return fragment.
	 */

	DEBUG(10,("read_from_pipe: %s: fault_state = %d : data_sent_length \
= %u, prs_offset(&p->out_data.rdata) = %u.\n",
		p->name, (int)p->fault_state, (unsigned int)p->out_data.data_sent_length, (unsigned int)prs_offset(&p->out_data.rdata) ));

	if(p->out_data.data_sent_length >= prs_offset(&p->out_data.rdata)) {
		/*
		 * We have sent all possible data. Return 0.
		 */
		return 0;
	}

	/*
	 * We need to create a new PDU from the data left in p->rdata.
	 * Create the header/data/footers. This also sets up the fields
	 * p->current_pdu_len, p->current_pdu_sent, p->data_sent_length
	 * and stores the outgoing PDU in p->current_pdu.
	 */

	if(!create_next_pdu(p)) {
		DEBUG(0,("read_from_pipe: %s: create_next_pdu failed.\n",
			 p->name));
		return -1;
	}

	data_returned = MIN(n, p->out_data.current_pdu_len);

	memcpy( data, p->out_data.current_pdu, (size_t)data_returned);
	p->out_data.current_pdu_sent += (uint32)data_returned;
	return data_returned;
}

/****************************************************************************
 Wait device state on a pipe. Exactly what this is for is unknown...
****************************************************************************/

BOOL wait_rpc_pipe_hnd_state(pipes_struct *p, uint16 priority)
{
	if (p == NULL)
		return False;

	if (p->open) {
		DEBUG(3,("wait_rpc_pipe_hnd_state: Setting pipe wait state priority=%x on pipe (name=%s)\n",
		         priority, p->name));

		p->priority = priority;
		
		return True;
	} 

	DEBUG(3,("wait_rpc_pipe_hnd_state: Error setting pipe wait state priority=%x (name=%s)\n",
		 priority, p->name));
	return False;
}


/****************************************************************************
 Set device state on a pipe. Exactly what this is for is unknown...
****************************************************************************/

BOOL set_rpc_pipe_hnd_state(pipes_struct *p, uint16 device_state)
{
	if (p == NULL)
		return False;

	if (p->open) {
		DEBUG(3,("set_rpc_pipe_hnd_state: Setting pipe device state=%x on pipe (name=%s)\n",
		         device_state, p->name));

		p->device_state = device_state;
		
		return True;
	} 

	DEBUG(3,("set_rpc_pipe_hnd_state: Error setting pipe device state=%x (name=%s)\n",
		 device_state, p->name));
	return False;
}


/****************************************************************************
 Close an rpc pipe.
****************************************************************************/

BOOL close_rpc_pipe_hnd(pipes_struct *p, connection_struct *conn)
{
	if (!p) {
		DEBUG(0,("Invalid pipe in close_rpc_pipe_hnd\n"));
		return False;
	}

	prs_mem_free(&p->out_data.rdata);
	prs_mem_free(&p->in_data.data);

	bitmap_clear(bmap, p->pnum - pipe_handle_offset);

	pipes_open--;

	DEBUG(4,("closed pipe name %s pnum=%x (pipes_open=%d)\n", 
		 p->name, p->pnum, pipes_open));  

	DLIST_REMOVE(Pipes, p);

	ZERO_STRUCTP(p);

	free(p);
	
	return True;
}

/****************************************************************************
 Find an rpc pipe given a pipe handle in a buffer and an offset.
****************************************************************************/

pipes_struct *get_rpc_pipe_p(char *buf, int where)
{
	int pnum = SVAL(buf,where);

	if (chain_p)
		return chain_p;

	return get_rpc_pipe(pnum);
}

/****************************************************************************
 Find an rpc pipe given a pipe handle.
****************************************************************************/

pipes_struct *get_rpc_pipe(int pnum)
{
	pipes_struct *p;

	DEBUG(4,("search for pipe pnum=%x\n", pnum));

	for (p=Pipes;p;p=p->next)
		DEBUG(5,("pipe name %s pnum=%x (pipes_open=%d)\n", 
		          p->name, p->pnum, pipes_open));  

	for (p=Pipes;p;p=p->next) {
		if (p->pnum == pnum) {
			chain_p = p;
			return p;
		}
	}

	return NULL;
}
