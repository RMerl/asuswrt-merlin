/* 
   Unix SMB/CIFS implementation.
   raw trans/trans2/nttrans operations

   Copyright (C) James Myers 2003 <myersjj@samba.org>
   
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
#include "../lib/util/dlinklist.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

#define TORTURE_TRANS_DATA 0

/*
  check out of bounds for incoming data
*/
static bool raw_trans_oob(struct smbcli_request *req,
			  uint_t offset, uint_t count)
{
	uint8_t *ptr;

	if (count == 0) {
		return false;
	}

	ptr = req->in.hdr + offset;
	
	/* be careful with wraparound! */
	if ((uintptr_t)ptr < (uintptr_t)req->in.data ||
	    (uintptr_t)ptr >= (uintptr_t)req->in.data + req->in.data_size ||
	    count > req->in.data_size ||
	    (uintptr_t)ptr + count > (uintptr_t)req->in.data + req->in.data_size) {
		return true;
	}
	return false;	
}

static size_t raw_trans_space_left(struct smbcli_request *req)
{
	if (req->transport->negotiate.max_xmit <= req->out.size) {
		return 0;
	}

	return req->transport->negotiate.max_xmit - req->out.size;
}

struct smb_raw_trans2_recv_state {
	uint8_t command;
	uint32_t params_total;
	uint32_t data_total;
	uint32_t params_left;
	uint32_t data_left;
	bool got_first;
	uint32_t recvd_data;
	uint32_t recvd_param;
	struct smb_trans2 io;
};

NTSTATUS smb_raw_trans2_recv(struct smbcli_request *req,
			     TALLOC_CTX *mem_ctx,
			     struct smb_trans2 *parms)
{
	struct smb_raw_trans2_recv_state *state;

	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		goto failed;
	}

	state = talloc_get_type(req->recv_helper.private_data,
				struct smb_raw_trans2_recv_state);

	parms->out = state->io.out;
	talloc_steal(mem_ctx, parms->out.setup);
	talloc_steal(mem_ctx, parms->out.params.data);
	talloc_steal(mem_ctx, parms->out.data.data);
	talloc_free(state);

	ZERO_STRUCT(req->recv_helper);

failed:
	return smbcli_request_destroy(req);
}

static enum smbcli_request_state smb_raw_trans2_ship_rest(struct smbcli_request *req,
							  struct smb_raw_trans2_recv_state *state);

/*
 * This helper returns SMBCLI_REQUEST_RECV until all data has arrived
 */
static enum smbcli_request_state smb_raw_trans2_recv_helper(struct smbcli_request *req)
{
	struct smb_raw_trans2_recv_state *state = talloc_get_type(req->recv_helper.private_data,
						  struct smb_raw_trans2_recv_state);
	uint16_t param_count, param_ofs, param_disp;
	uint16_t data_count, data_ofs, data_disp;
	uint16_t total_data, total_param;
	uint8_t setup_count;

	/*
	 * An NT RPC pipe call can return ERRDOS, ERRmoredata
	 * to a trans call. This is not an error and should not
	 * be treated as such.
	 */
	if (smbcli_request_is_error(req)) {
		goto failed;
	}

	if (state->params_left > 0 || state->data_left > 0) {
		return smb_raw_trans2_ship_rest(req, state);
	}

	SMBCLI_CHECK_MIN_WCT(req, 10);

	total_data = SVAL(req->in.vwv, VWV(1));
	total_param = SVAL(req->in.vwv, VWV(0));
	setup_count = CVAL(req->in.vwv, VWV(9));

	param_count = SVAL(req->in.vwv, VWV(3));
	param_ofs   = SVAL(req->in.vwv, VWV(4));
	param_disp  = SVAL(req->in.vwv, VWV(5));

	data_count = SVAL(req->in.vwv, VWV(6));
	data_ofs   = SVAL(req->in.vwv, VWV(7));
	data_disp  = SVAL(req->in.vwv, VWV(8));

	if (!state->got_first) {
		if (total_param > 0) {
			state->io.out.params = data_blob_talloc(state, NULL, total_param);
			if (!state->io.out.params.data) {
				goto nomem;
			}
		}

		if (total_data > 0) {
			state->io.out.data = data_blob_talloc(state, NULL, total_data);
			if (!state->io.out.data.data) {
				goto nomem;
			}
		}

		if (setup_count > 0) {
			uint16_t i;

			SMBCLI_CHECK_WCT(req, 10 + setup_count);

			state->io.out.setup_count = setup_count;
			state->io.out.setup = talloc_array(state, uint16_t, setup_count);
			if (!state->io.out.setup) {
				goto nomem;
			}
			for (i=0; i < setup_count; i++) {
				state->io.out.setup[i] = SVAL(req->in.vwv, VWV(10+i));
			}
		}

		state->got_first = true;
	}

	if (total_data > state->io.out.data.length ||
	    total_param > state->io.out.params.length) {
		/* they must *only* shrink */
		DEBUG(1,("smb_raw_trans2_recv_helper: data/params expanded!\n"));
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
		goto failed;
	}

	state->io.out.data.length = total_data;
	state->io.out.params.length = total_param;

	if (data_count + data_disp > total_data ||
	    param_count + param_disp > total_param) {
		DEBUG(1,("smb_raw_trans2_recv_helper: Buffer overflow\n"));
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
		goto failed;
	}

	/* check the server isn't being nasty */
	if (raw_trans_oob(req, param_ofs, param_count) ||
	    raw_trans_oob(req, data_ofs, data_count)) {
		DEBUG(1,("smb_raw_trans2_recv_helper: out of bounds parameters!\n"));
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
		goto failed;
	}

	if (data_count) {
		memcpy(state->io.out.data.data + data_disp,
		       req->in.hdr + data_ofs,
		       data_count);
	}

	if (param_count) {
		memcpy(state->io.out.params.data + param_disp,
		       req->in.hdr + param_ofs,
		       param_count);
	}

	state->recvd_param += param_count;
	state->recvd_data += data_count;

	if (state->recvd_data < total_data ||
	    state->recvd_param < total_param) {

		/* we don't need the in buffer any more */
		talloc_free(req->in.buffer);
		ZERO_STRUCT(req->in);

		/* we still wait for more data */
		DEBUG(10,("smb_raw_trans2_recv_helper: more data needed\n"));
		return SMBCLI_REQUEST_RECV;
	}

	DEBUG(10,("smb_raw_trans2_recv_helper: done\n"));
	return SMBCLI_REQUEST_DONE;

nomem:
	req->status = NT_STATUS_NO_MEMORY;
failed:
	return SMBCLI_REQUEST_ERROR;
}

_PUBLIC_ NTSTATUS smb_raw_trans_recv(struct smbcli_request *req,
			     TALLOC_CTX *mem_ctx,
			     struct smb_trans2 *parms)
{
	return smb_raw_trans2_recv(req, mem_ctx, parms);
}


/*
  trans/trans2 raw async interface - only BLOBs used in this interface.
*/
struct smbcli_request *smb_raw_trans_send_backend(struct smbcli_tree *tree,
						  struct smb_trans2 *parms,
						  uint8_t command)
{
	struct smb_raw_trans2_recv_state *state;
	struct smbcli_request *req;
	int i;
	int padding;
	size_t space_left;
	size_t namelen = 0;
	DATA_BLOB params_chunk;
	uint16_t ofs;
	uint16_t params_ofs = 0;
	DATA_BLOB data_chunk;
	uint16_t data_ofs = 0;

	if (parms->in.params.length > UINT16_MAX ||
	    parms->in.data.length > UINT16_MAX) {
		DEBUG(3,("Attempt to send invalid trans2 request (params %u, data %u)\n",
			 (unsigned)parms->in.params.length, (unsigned)parms->in.data.length));
		return NULL;
	}
	    

	if (command == SMBtrans)
		padding = 1;
	else
		padding = 3;
	
	req = smbcli_request_setup(tree, command,
				   14 + parms->in.setup_count,
				   padding);
	if (!req) {
		return NULL;
	}

	state = talloc_zero(req, struct smb_raw_trans2_recv_state);
	if (!state) {
		smbcli_request_destroy(req);
		return NULL;
	}

	state->command = command;

	/* make sure we don't leak data via the padding */
	memset(req->out.data, 0, padding);

	/* Watch out, this changes the req->out.* pointers */
	if (command == SMBtrans && parms->in.trans_name) {
		namelen = smbcli_req_append_string(req, parms->in.trans_name, 
						STR_TERMINATE);
	}

	ofs = PTR_DIFF(req->out.data,req->out.hdr)+padding+namelen;

	/* see how much bytes of the params block we can ship in the first request */
	space_left = raw_trans_space_left(req);

	params_chunk.length = MIN(parms->in.params.length, space_left);
	params_chunk.data = parms->in.params.data;
	params_ofs = ofs;

	state->params_left = parms->in.params.length - params_chunk.length;

	if (state->params_left > 0) {
		/* we copy the whole params block, if needed we can optimize that latter */
		state->io.in.params = data_blob_talloc(state, NULL, parms->in.params.length);
		if (!state->io.in.params.data) {
			smbcli_request_destroy(req);
			return NULL;
		}
		memcpy(state->io.in.params.data,
		       parms->in.params.data,
		       parms->in.params.length);
	}

	/* see how much bytes of the data block we can ship in the first request */
	space_left -= params_chunk.length;

#if TORTURE_TRANS_DATA
	if (space_left > 1) {
		space_left /= 2;
	}
#endif

	data_chunk.length = MIN(parms->in.data.length, space_left);
	data_chunk.data = parms->in.data.data;
	data_ofs = params_ofs + params_chunk.length;

	state->data_left = parms->in.data.length - data_chunk.length;

	if (state->data_left > 0) {
		/* we copy the whole params block, if needed we can optimize that latter */
		state->io.in.data = data_blob_talloc(state, NULL, parms->in.data.length);
		if (!state->io.in.data.data) {
			smbcli_request_destroy(req);
			return NULL;
		}
		memcpy(state->io.in.data.data,
		       parms->in.data.data,
		       parms->in.data.length);
	}

	state->params_total = parms->in.params.length;
	state->data_total = parms->in.data.length;

	/* primary request */
	SSVAL(req->out.vwv,VWV(0),parms->in.params.length);
	SSVAL(req->out.vwv,VWV(1),parms->in.data.length);
	SSVAL(req->out.vwv,VWV(2),parms->in.max_param);
	SSVAL(req->out.vwv,VWV(3),parms->in.max_data);
	SCVAL(req->out.vwv,VWV(4),parms->in.max_setup);
	SCVAL(req->out.vwv,VWV(4)+1,0); /* reserved */
	SSVAL(req->out.vwv,VWV(5),parms->in.flags);
	SIVAL(req->out.vwv,VWV(6),parms->in.timeout);
	SSVAL(req->out.vwv,VWV(8),0); /* reserved */
	SSVAL(req->out.vwv,VWV(9),params_chunk.length);
	SSVAL(req->out.vwv,VWV(10),params_ofs);
	SSVAL(req->out.vwv,VWV(11),data_chunk.length);
	SSVAL(req->out.vwv,VWV(12),data_ofs);
	SCVAL(req->out.vwv,VWV(13),parms->in.setup_count);
	SCVAL(req->out.vwv,VWV(13)+1,0); /* reserved */
	for (i=0;i<parms->in.setup_count;i++)	{
		SSVAL(req->out.vwv,VWV(14)+VWV(i),parms->in.setup[i]);
	}
	smbcli_req_append_blob(req, &params_chunk);
	smbcli_req_append_blob(req, &data_chunk);

	/* add the helper which will check that all multi-part replies are
	   in before an async client callack will be issued */
	req->recv_helper.fn = smb_raw_trans2_recv_helper;
	req->recv_helper.private_data = state;

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

static enum smbcli_request_state smb_raw_trans2_ship_next(struct smbcli_request *req,
							  struct smb_raw_trans2_recv_state *state)
{
	struct smbcli_request *req2;
	size_t space_left;
	DATA_BLOB params_chunk;
	uint16_t ofs;
	uint16_t params_ofs = 0;
	uint16_t params_disp = 0;
	DATA_BLOB data_chunk;
	uint16_t data_ofs = 0;
	uint16_t data_disp = 0;
	uint8_t wct;

	if (state->command == SMBtrans2) {
		wct = 9;
	} else {
		wct = 8;
	}

	req2 = smbcli_request_setup(req->tree, state->command+1, wct, 0);
	if (!req2) {
		goto nomem;
	}
	req2->mid = req->mid;
	SSVAL(req2->out.hdr, HDR_MID, req2->mid);

	ofs = PTR_DIFF(req2->out.data,req2->out.hdr);

	/* see how much bytes of the params block we can ship in the first request */
	space_left = raw_trans_space_left(req2);

	params_disp = state->io.in.params.length - state->params_left;
	params_chunk.length = MIN(state->params_left, space_left);
	params_chunk.data = state->io.in.params.data + params_disp;
	params_ofs = ofs;

	state->params_left -= params_chunk.length;

	/* see how much bytes of the data block we can ship in the first request */
	space_left -= params_chunk.length;

#if TORTURE_TRANS_DATA
	if (space_left > 1) {
		space_left /= 2;
	}
#endif

	data_disp = state->io.in.data.length - state->data_left;
	data_chunk.length = MIN(state->data_left, space_left);
	data_chunk.data = state->io.in.data.data + data_disp;
	data_ofs = params_ofs+params_chunk.length;

	state->data_left -= data_chunk.length;

	SSVAL(req2->out.vwv,VWV(0), state->params_total);
	SSVAL(req2->out.vwv,VWV(1), state->data_total);
	SSVAL(req2->out.vwv,VWV(2), params_chunk.length);
	SSVAL(req2->out.vwv,VWV(3), params_ofs);
	SSVAL(req2->out.vwv,VWV(4), params_disp);
	SSVAL(req2->out.vwv,VWV(5), data_chunk.length);
	SSVAL(req2->out.vwv,VWV(6), data_ofs);
	SSVAL(req2->out.vwv,VWV(7), data_disp);
	if (wct == 9) {
		SSVAL(req2->out.vwv,VWV(8), 0xFFFF);
	}

	smbcli_req_append_blob(req2, &params_chunk);
	smbcli_req_append_blob(req2, &data_chunk);

	/*
	 * it's a one way request but we need
	 * the seq_num, so we destroy req2 by hand
	 */
	if (!smbcli_request_send(req2)) {
		goto failed;
	}

	req->seq_num = req2->seq_num;
	smbcli_request_destroy(req2);

	return SMBCLI_REQUEST_RECV;

nomem:
	req->status = NT_STATUS_NO_MEMORY;
failed:
	if (req2) {
		req->status = smbcli_request_destroy(req2);
	}
	return SMBCLI_REQUEST_ERROR;
}

static enum smbcli_request_state smb_raw_trans2_ship_rest(struct smbcli_request *req,
							  struct smb_raw_trans2_recv_state *state)
{
	enum smbcli_request_state ret = SMBCLI_REQUEST_ERROR;

	while (state->params_left > 0 || state->data_left > 0) {
		ret = smb_raw_trans2_ship_next(req, state);
		if (ret != SMBCLI_REQUEST_RECV) {
			break;
		}
	}

	return ret;
}


/*
  trans/trans2 raw async interface - only BLOBs used in this interface.
  note that this doesn't yet support multi-part requests
*/
_PUBLIC_ struct smbcli_request *smb_raw_trans_send(struct smbcli_tree *tree,
				       struct smb_trans2 *parms)
{
	return smb_raw_trans_send_backend(tree, parms, SMBtrans);
}

struct smbcli_request *smb_raw_trans2_send(struct smbcli_tree *tree,
				       struct smb_trans2 *parms)
{
	return smb_raw_trans_send_backend(tree, parms, SMBtrans2);
}

/*
  trans2 synchronous blob interface
*/
NTSTATUS smb_raw_trans2(struct smbcli_tree *tree,
			TALLOC_CTX *mem_ctx,
			struct smb_trans2 *parms)
{
	struct smbcli_request *req;
	req = smb_raw_trans2_send(tree, parms);
	if (!req) return NT_STATUS_UNSUCCESSFUL;
	return smb_raw_trans2_recv(req, mem_ctx, parms);
}


/*
  trans synchronous blob interface
*/
_PUBLIC_ NTSTATUS smb_raw_trans(struct smbcli_tree *tree,
		       TALLOC_CTX *mem_ctx,
		       struct smb_trans2 *parms)
{
	struct smbcli_request *req;
	req = smb_raw_trans_send(tree, parms);
	if (!req) return NT_STATUS_UNSUCCESSFUL;
	return smb_raw_trans_recv(req, mem_ctx, parms);
}

struct smb_raw_nttrans_recv_state {
	uint32_t params_total;
	uint32_t data_total;
	uint32_t params_left;
	uint32_t data_left;
	bool got_first;
	uint32_t recvd_data;
	uint32_t recvd_param;
	struct smb_nttrans io;
};

NTSTATUS smb_raw_nttrans_recv(struct smbcli_request *req,
			      TALLOC_CTX *mem_ctx,
			      struct smb_nttrans *parms)
{
	struct smb_raw_nttrans_recv_state *state;

	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		goto failed;
	}

	state = talloc_get_type(req->recv_helper.private_data,
				struct smb_raw_nttrans_recv_state);

	parms->out = state->io.out;
	talloc_steal(mem_ctx, parms->out.setup);
	talloc_steal(mem_ctx, parms->out.params.data);
	talloc_steal(mem_ctx, parms->out.data.data);
	talloc_free(state);

	ZERO_STRUCT(req->recv_helper);

failed:
	return smbcli_request_destroy(req);
}

static enum smbcli_request_state smb_raw_nttrans_ship_rest(struct smbcli_request *req,
							   struct smb_raw_nttrans_recv_state *state);

/*
 * This helper returns SMBCLI_REQUEST_RECV until all data has arrived
 */
static enum smbcli_request_state smb_raw_nttrans_recv_helper(struct smbcli_request *req)
{
	struct smb_raw_nttrans_recv_state *state = talloc_get_type(req->recv_helper.private_data,
						   struct smb_raw_nttrans_recv_state);
	uint32_t param_count, param_ofs, param_disp;
	uint32_t data_count, data_ofs, data_disp;
	uint32_t total_data, total_param;
	uint8_t setup_count;

	/*
	 * An NT RPC pipe call can return ERRDOS, ERRmoredata
	 * to a trans call. This is not an error and should not
	 * be treated as such.
	 */
	if (smbcli_request_is_error(req)) {
		goto failed;
	}

	/* sanity check */
	if (CVAL(req->in.hdr, HDR_COM) != SMBnttrans) {
		DEBUG(0,("smb_raw_nttrans_recv_helper: Expected %s response, got command 0x%02x\n",
			 "SMBnttrans", 
			 CVAL(req->in.hdr,HDR_COM)));
		req->status = NT_STATUS_INVALID_NETWORK_RESPONSE;
		goto failed;
	}

	if (state->params_left > 0 || state->data_left > 0) {
		return smb_raw_nttrans_ship_rest(req, state);
	}

	/* this is the first packet of the response */
	SMBCLI_CHECK_MIN_WCT(req, 18);

	total_param = IVAL(req->in.vwv, 3);
	total_data  = IVAL(req->in.vwv, 7);
	setup_count = CVAL(req->in.vwv, 35);

	param_count = IVAL(req->in.vwv, 11);
	param_ofs   = IVAL(req->in.vwv, 15);
	param_disp  = IVAL(req->in.vwv, 19);

	data_count = IVAL(req->in.vwv, 23);
	data_ofs   = IVAL(req->in.vwv, 27);
	data_disp  = IVAL(req->in.vwv, 31);

	if (!state->got_first) {
		if (total_param > 0) {
			state->io.out.params = data_blob_talloc(state, NULL, total_param);
			if (!state->io.out.params.data) {
				goto nomem;
			}
		}

		if (total_data > 0) {
			state->io.out.data = data_blob_talloc(state, NULL, total_data);
			if (!state->io.out.data.data) {
				goto nomem;
			}
		}

		if (setup_count > 0) {
			SMBCLI_CHECK_WCT(req, 18 + setup_count);

			state->io.out.setup_count = setup_count;
			state->io.out.setup = talloc_array(state, uint8_t,
							   setup_count * VWV(1));
			if (!state->io.out.setup) {
				goto nomem;
			}
			memcpy(state->io.out.setup, (uint8_t *)req->out.vwv + VWV(18),
			       setup_count * VWV(1));
		}

		state->got_first = true;
	}

	if (total_data > state->io.out.data.length ||
	    total_param > state->io.out.params.length) {
		/* they must *only* shrink */
		DEBUG(1,("smb_raw_nttrans_recv_helper: data/params expanded!\n"));
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
		goto failed;
	}

	state->io.out.data.length = total_data;
	state->io.out.params.length = total_param;

	if (data_count + data_disp > total_data ||
	    param_count + param_disp > total_param) {
		DEBUG(1,("smb_raw_nttrans_recv_helper: Buffer overflow\n"));
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
		goto failed;
	}

	/* check the server isn't being nasty */
	if (raw_trans_oob(req, param_ofs, param_count) ||
	    raw_trans_oob(req, data_ofs, data_count)) {
		DEBUG(1,("smb_raw_nttrans_recv_helper: out of bounds parameters!\n"));
		req->status = NT_STATUS_BUFFER_TOO_SMALL;
		goto failed;
	}

	if (data_count) {
		memcpy(state->io.out.data.data + data_disp,
		       req->in.hdr + data_ofs,
		       data_count);
	}

	if (param_count) {
		memcpy(state->io.out.params.data + param_disp,
		       req->in.hdr + param_ofs,
		       param_count);
	}

	state->recvd_param += param_count;
	state->recvd_data += data_count;

	if (state->recvd_data < total_data ||
	    state->recvd_param < total_param) {

		/* we don't need the in buffer any more */
		talloc_free(req->in.buffer);
		ZERO_STRUCT(req->in);

		/* we still wait for more data */
		DEBUG(10,("smb_raw_nttrans_recv_helper: more data needed\n"));
		return SMBCLI_REQUEST_RECV;
	}

	DEBUG(10,("smb_raw_nttrans_recv_helper: done\n"));
	return SMBCLI_REQUEST_DONE;

nomem:
	req->status = NT_STATUS_NO_MEMORY;
failed:
	return SMBCLI_REQUEST_ERROR;
}

/****************************************************************************
 nttrans raw - only BLOBs used in this interface.
 at the moment we only handle a single primary request 
****************************************************************************/
struct smbcli_request *smb_raw_nttrans_send(struct smbcli_tree *tree,
					 struct smb_nttrans *parms)
{
	struct smbcli_request *req; 
	struct smb_raw_nttrans_recv_state *state;
	uint32_t ofs;
	size_t space_left;
	DATA_BLOB params_chunk;
	uint32_t params_ofs;
	DATA_BLOB data_chunk;
	uint32_t data_ofs;
	int align = 0;

	/* only align if there are parameters or data */
	if (parms->in.params.length || parms->in.data.length) {
		align = 3;
	}
	
	req = smbcli_request_setup(tree, SMBnttrans, 
				19 + parms->in.setup_count, align);
	if (!req) {
		return NULL;
	}

	state = talloc_zero(req, struct smb_raw_nttrans_recv_state);
	if (!state) {
		talloc_free(req);
		return NULL;
	}

	/* fill in SMB parameters */

	if (align != 0) {
		memset(req->out.data, 0, align);
	}

	ofs = PTR_DIFF(req->out.data,req->out.hdr)+align;

	/* see how much bytes of the params block we can ship in the first request */
	space_left = raw_trans_space_left(req);

	params_chunk.length = MIN(parms->in.params.length, space_left);
	params_chunk.data = parms->in.params.data;
	params_ofs = ofs;

	state->params_left = parms->in.params.length - params_chunk.length;

	if (state->params_left > 0) {
		/* we copy the whole params block, if needed we can optimize that latter */
		state->io.in.params = data_blob_talloc(state, NULL, parms->in.params.length);
		if (!state->io.in.params.data) {
			smbcli_request_destroy(req);
			return NULL;
		}
		memcpy(state->io.in.params.data,
		       parms->in.params.data,
		       parms->in.params.length);
	}

	/* see how much bytes of the data block we can ship in the first request */
	space_left -= params_chunk.length;

#if TORTURE_TRANS_DATA
	if (space_left > 1) {
		space_left /= 2;
	}
#endif

	data_chunk.length = MIN(parms->in.data.length, space_left);
	data_chunk.data = parms->in.data.data;
	data_ofs = params_ofs + params_chunk.length;

	state->data_left = parms->in.data.length - data_chunk.length;

	if (state->data_left > 0) {
		/* we copy the whole params block, if needed we can optimize that latter */
		state->io.in.data = data_blob_talloc(state, NULL, parms->in.data.length);
		if (!state->io.in.data.data) {
			smbcli_request_destroy(req);
			return NULL;
		}
		memcpy(state->io.in.data.data,
		       parms->in.data.data,
		       parms->in.data.length);
	}

	state->params_total = parms->in.params.length;
	state->data_total = parms->in.data.length;

	SCVAL(req->out.vwv,  0, parms->in.max_setup);
	SSVAL(req->out.vwv,  1, 0); /* reserved */
	SIVAL(req->out.vwv,  3, parms->in.params.length);
	SIVAL(req->out.vwv,  7, parms->in.data.length);
	SIVAL(req->out.vwv, 11, parms->in.max_param);
	SIVAL(req->out.vwv, 15, parms->in.max_data);
	SIVAL(req->out.vwv, 19, params_chunk.length);
	SIVAL(req->out.vwv, 23, params_ofs);
	SIVAL(req->out.vwv, 27, data_chunk.length);
	SIVAL(req->out.vwv, 31, data_ofs);
	SCVAL(req->out.vwv, 35, parms->in.setup_count);
	SSVAL(req->out.vwv, 36, parms->in.function);
	memcpy(req->out.vwv + VWV(19), parms->in.setup,
	       sizeof(uint16_t) * parms->in.setup_count);

	smbcli_req_append_blob(req, &params_chunk);
	smbcli_req_append_blob(req, &data_chunk);

	/* add the helper which will check that all multi-part replies are
	   in before an async client callack will be issued */
	req->recv_helper.fn = smb_raw_nttrans_recv_helper;
	req->recv_helper.private_data = state;

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

static enum smbcli_request_state smb_raw_nttrans_ship_next(struct smbcli_request *req,
							   struct smb_raw_nttrans_recv_state *state)
{
	struct smbcli_request *req2;
	size_t space_left;
	DATA_BLOB params_chunk;
	uint32_t ofs;
	uint32_t params_ofs = 0;
	uint32_t params_disp = 0;
	DATA_BLOB data_chunk;
	uint32_t data_ofs = 0;
	uint32_t data_disp = 0;

	req2 = smbcli_request_setup(req->tree, SMBnttranss, 18, 0);
	if (!req2) {
		goto nomem;
	}
	req2->mid = req->mid;
	SSVAL(req2->out.hdr, HDR_MID, req2->mid);

	ofs = PTR_DIFF(req2->out.data,req2->out.hdr);

	/* see how much bytes of the params block we can ship in the first request */
	space_left = raw_trans_space_left(req2);

	params_disp = state->io.in.params.length - state->params_left;
	params_chunk.length = MIN(state->params_left, space_left);
	params_chunk.data = state->io.in.params.data + params_disp;
	params_ofs = ofs;

	state->params_left -= params_chunk.length;

	/* see how much bytes of the data block we can ship in the first request */
	space_left -= params_chunk.length;

#if TORTURE_TRANS_DATA
	if (space_left > 1) {
		space_left /= 2;
	}
#endif

	data_disp = state->io.in.data.length - state->data_left;
	data_chunk.length = MIN(state->data_left, space_left);
	data_chunk.data = state->io.in.data.data + data_disp;
	data_ofs = params_ofs+params_chunk.length;

	state->data_left -= data_chunk.length;

	SSVAL(req2->out.vwv,0, 0); /* reserved */
	SCVAL(req2->out.vwv,2, 0); /* reserved */
	SIVAL(req2->out.vwv,3, state->params_total);
	SIVAL(req2->out.vwv,7, state->data_total);
	SIVAL(req2->out.vwv,11, params_chunk.length);
	SIVAL(req2->out.vwv,15, params_ofs);
	SIVAL(req2->out.vwv,19, params_disp);
	SIVAL(req2->out.vwv,23, data_chunk.length);
	SIVAL(req2->out.vwv,27, data_ofs);
	SIVAL(req2->out.vwv,31, data_disp);
	SCVAL(req2->out.vwv,35, 0); /* reserved */

	smbcli_req_append_blob(req2, &params_chunk);
	smbcli_req_append_blob(req2, &data_chunk);

	/*
	 * it's a one way request but we need
	 * the seq_num, so we destroy req2 by hand
	 */
	if (!smbcli_request_send(req2)) {
		goto failed;
	}

	req->seq_num = req2->seq_num;
	smbcli_request_destroy(req2);

	return SMBCLI_REQUEST_RECV;

nomem:
	req->status = NT_STATUS_NO_MEMORY;
failed:
	if (req2) {
		req->status = smbcli_request_destroy(req2);
	}
	return SMBCLI_REQUEST_ERROR;
}

static enum smbcli_request_state smb_raw_nttrans_ship_rest(struct smbcli_request *req,
							   struct smb_raw_nttrans_recv_state *state)
{
	enum smbcli_request_state ret = SMBCLI_REQUEST_ERROR;

	while (state->params_left > 0 || state->data_left > 0) {
		ret = smb_raw_nttrans_ship_next(req, state);
		if (ret != SMBCLI_REQUEST_RECV) {
			break;
		}
	}

	return ret;
}


/****************************************************************************
  receive a SMB nttrans response allocating the necessary memory
  ****************************************************************************/
NTSTATUS smb_raw_nttrans(struct smbcli_tree *tree,
			 TALLOC_CTX *mem_ctx,
			 struct smb_nttrans *parms)
{
	struct smbcli_request *req;

	req = smb_raw_nttrans_send(tree, parms);
	if (!req) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	return smb_raw_nttrans_recv(req, mem_ctx, parms);
}
