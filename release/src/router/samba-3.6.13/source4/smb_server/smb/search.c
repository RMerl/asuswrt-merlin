/* 
   Unix SMB/CIFS implementation.
   SMBsearch handling
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James J Myers 2003 <myersjj@samba.org>

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
/*
   This file handles the parsing of transact2 requests
*/

#include "includes.h"
#include "smb_server/smb_server.h"
#include "ntvfs/ntvfs.h"


/* a structure to encapsulate the state information about 
 * an in-progress search first/next operation */
struct search_state {
	struct smbsrv_request *req;
	union smb_search_data *file;
	uint16_t last_entry_offset;
};

/*
  fill a single entry in a search find reply 
*/
static bool find_fill_info(struct smbsrv_request *req,
			   const union smb_search_data *file)
{
	uint8_t *p;

	if (req->out.data_size + 43 > req_max_data(req)) {
		return false;
	}
	
	req_grow_data(req, req->out.data_size + 43);
	p = req->out.data + req->out.data_size - 43;

	SCVAL(p,  0, file->search.id.reserved);
	memcpy(p+1, file->search.id.name, 11);
	SCVAL(p, 12, file->search.id.handle);
	SIVAL(p, 13, file->search.id.server_cookie);
	SIVAL(p, 17, file->search.id.client_cookie);
	SCVAL(p, 21, file->search.attrib);
	srv_push_dos_date(req->smb_conn, p, 22, file->search.write_time);
	SIVAL(p, 26, file->search.size);
	memset(p+30, ' ', 12);
	memcpy(p+30, file->search.name, MIN(strlen(file->search.name)+1, 12));
	SCVAL(p,42,0);

	return true;
}

/* callback function for search first/next */
static bool find_callback(void *private_data, const union smb_search_data *file)
{
	struct search_state *state = (struct search_state *)private_data;

	return find_fill_info(state->req, file);
}

/****************************************************************************
 Reply to a search first (async reply)
****************************************************************************/
static void reply_search_first_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_search_first *sf;

	SMBSRV_CHECK_ASYNC_STATUS(sf, union smb_search_first);

	SSVAL(req->out.vwv, VWV(0), sf->search_first.out.count);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a search next (async reply)
****************************************************************************/
static void reply_search_next_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	union smb_search_next *sn;
	
	SMBSRV_CHECK_ASYNC_STATUS(sn, union smb_search_next);

	SSVAL(req->out.vwv, VWV(0), sn->search_next.out.count);

	smbsrv_send_reply(req);
}

/****************************************************************************
 Reply to a search.
****************************************************************************/
void smbsrv_reply_search(struct smbsrv_request *req)
{
	union smb_search_first *sf;
	uint16_t resume_key_length;
	struct search_state *state;
	uint8_t *p;
	enum smb_search_level level = RAW_SEARCH_SEARCH;
	uint8_t op = CVAL(req->in.hdr,HDR_COM);

	if (op == SMBffirst) {
		level = RAW_SEARCH_FFIRST;
	} else if (op == SMBfunique) {
		level = RAW_SEARCH_FUNIQUE;
	}

	/* parse request */
	if (req->in.wct != 2) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	SMBSRV_TALLOC_IO_PTR(sf, union smb_search_first);

	p = req->in.data;
	p += req_pull_ascii4(&req->in.bufinfo, &sf->search_first.in.pattern, 
			     p, STR_TERMINATE);
	if (!sf->search_first.in.pattern) {
		smbsrv_send_error(req, NT_STATUS_OBJECT_NAME_NOT_FOUND);
		return;
	}

	if (req_data_oob(&req->in.bufinfo, p, 3)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	if (*p != 5) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	resume_key_length = SVAL(p, 1);
	p += 3;
	
	/* setup state for callback */
	state = talloc(req, struct search_state);
	if (!state) {
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
		return;
	}

	state->req = req;
	state->file = NULL;
	state->last_entry_offset = 0;

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);
	SSVAL(req->out.vwv, VWV(0), 0);
	req_append_var_block(req, NULL, 0);

	if (resume_key_length != 0) {
		union smb_search_next *sn;

		if (resume_key_length != 21 || 
		    req_data_oob(&req->in.bufinfo, p, 21) ||
		    level == RAW_SEARCH_FUNIQUE) {
			smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
			return;
		}

		/* do a search next operation */
		SMBSRV_TALLOC_IO_PTR(sn, union smb_search_next);
		SMBSRV_SETUP_NTVFS_REQUEST(reply_search_next_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

		sn->search_next.in.id.reserved      = CVAL(p, 0);
		memcpy(sn->search_next.in.id.name,    p+1, 11);
		sn->search_next.in.id.handle        = CVAL(p, 12);
		sn->search_next.in.id.server_cookie = IVAL(p, 13);
		sn->search_next.in.id.client_cookie = IVAL(p, 17);

		sn->search_next.level = level;
		sn->search_next.data_level = RAW_SEARCH_DATA_SEARCH;
		sn->search_next.in.max_count     = SVAL(req->in.vwv, VWV(0));
		sn->search_next.in.search_attrib = SVAL(req->in.vwv, VWV(1));

		/* call backend */
		SMBSRV_CALL_NTVFS_BACKEND(ntvfs_search_next(req->ntvfs, sn, state, find_callback));
	} else {
		SMBSRV_SETUP_NTVFS_REQUEST(reply_search_first_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

		/* do a search first operation */
		sf->search_first.level = level;
		sf->search_first.data_level = RAW_SEARCH_DATA_SEARCH;
		sf->search_first.in.search_attrib = SVAL(req->in.vwv, VWV(1));
		sf->search_first.in.max_count     = SVAL(req->in.vwv, VWV(0));

		SMBSRV_CALL_NTVFS_BACKEND(ntvfs_search_first(req->ntvfs, sf, state, find_callback));
	}
}


/****************************************************************************
 Reply to a fclose (async reply)
****************************************************************************/
static void reply_fclose_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;

	SMBSRV_CHECK_ASYNC_STATUS_SIMPLE;

	/* construct reply */
	smbsrv_setup_reply(req, 1, 0);

	SSVAL(req->out.vwv, VWV(0), 0);

	smbsrv_send_reply(req);
}


/****************************************************************************
 Reply to fclose (stop directory search).
****************************************************************************/
void smbsrv_reply_fclose(struct smbsrv_request *req)
{
	union smb_search_close *sc;
	uint16_t resume_key_length;
	uint8_t *p;
	const char *pattern;

	/* parse request */
	if (req->in.wct != 2) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	SMBSRV_TALLOC_IO_PTR(sc, union smb_search_close);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_fclose_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	p = req->in.data;
	p += req_pull_ascii4(&req->in.bufinfo, &pattern, p, STR_TERMINATE);
	if (pattern && *pattern) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	
	if (req_data_oob(&req->in.bufinfo, p, 3)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	if (*p != 5) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}
	resume_key_length = SVAL(p, 1);
	p += 3;

	if (resume_key_length != 21) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	if (req_data_oob(&req->in.bufinfo, p, 21)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	sc->fclose.level               = RAW_FINDCLOSE_FCLOSE;
	sc->fclose.in.max_count        = SVAL(req->in.vwv, VWV(0));
	sc->fclose.in.search_attrib    = SVAL(req->in.vwv, VWV(1));
	sc->fclose.in.id.reserved      = CVAL(p, 0);
	memcpy(sc->fclose.in.id.name,    p+1, 11);
	sc->fclose.in.id.handle        = CVAL(p, 12);
	sc->fclose.in.id.server_cookie = IVAL(p, 13);
	sc->fclose.in.id.client_cookie = IVAL(p, 17);

	SMBSRV_CALL_NTVFS_BACKEND(ntvfs_search_close(req->ntvfs, sc));

}
