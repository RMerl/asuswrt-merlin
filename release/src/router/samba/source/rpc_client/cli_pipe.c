
/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                       1998.
 *  Copyright (C) Jeremy Allison                    1999.
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


#ifdef SYSLOG
#undef SYSLOG
#endif

#include "includes.h"

extern int DEBUGLEVEL;
extern struct pipe_id_info pipe_names[];
extern fstring global_myworkgroup;
extern pstring global_myname;

/********************************************************************
 Rpc pipe call id.
 ********************************************************************/

static uint32 get_rpc_call_id(void)
{
	static uint32 call_id = 0;
	return ++call_id;
}

/*******************************************************************
 Use SMBreadX to get rest of one fragment's worth of rpc data.
 ********************************************************************/

static BOOL rpc_read(struct cli_state *cli, prs_struct *rdata, uint32 data_to_read, uint32 *rdata_offset)
{
	size_t size = (size_t)cli->max_recv_frag;
	int stream_offset = 0;
	int num_read;
	char *pdata;
	uint32 err;
	int extra_data_size = ((int)*rdata_offset) + ((int)data_to_read) - (int)prs_data_size(rdata);

	DEBUG(5,("rpc_read: data_to_read: %u rdata offset: %u extra_data_size: %d\n",
		(int)data_to_read, (unsigned int)*rdata_offset, extra_data_size));

	/*
	 * Grow the buffer if needed to accommodate the data to be read.
	 */

	if (extra_data_size > 0) {
		if(!prs_force_grow(rdata, (uint32)extra_data_size)) {
			DEBUG(0,("rpc_read: Failed to grow parse struct by %d bytes.\n", extra_data_size ));
			return False;
		}
		DEBUG(5,("rpc_read: grew buffer by %d bytes to %u\n", extra_data_size, prs_data_size(rdata) ));
	}

	pdata = prs_data_p(rdata) + *rdata_offset;

	do /* read data using SMBreadX */
	{
		if (size > (size_t)data_to_read)
			size = (size_t)data_to_read;

		num_read = (int)cli_read(cli, cli->nt_pipe_fnum, pdata, (off_t)stream_offset, size);

		DEBUG(5,("rpc_read: num_read = %d, read offset: %d, to read: %d\n",
		          num_read, stream_offset, data_to_read));

		if (cli_error(cli, NULL, &err, NULL)) {
			DEBUG(0,("rpc_read: Error %u in cli_read\n", (unsigned int)err ));
			return False;
		}

		data_to_read -= num_read;
		stream_offset += num_read;
		pdata += num_read;

	} while (num_read > 0 && data_to_read > 0);
	/* && err == (0x80000000 | STATUS_BUFFER_OVERFLOW)); */

	/*
	 * Update the current offset into rdata by the amount read.
	 */
	*rdata_offset += stream_offset;

	return True;
}

/****************************************************************************
 Checks the header.
 ****************************************************************************/

static BOOL rpc_check_hdr(prs_struct *rdata, RPC_HDR *rhdr, 
                          BOOL *first, BOOL *last, uint32 *len)
{
	DEBUG(5,("rpc_check_hdr: rdata->data_size = %u\n", (uint32)prs_data_size(rdata) ));

	if(!smb_io_rpc_hdr("rpc_hdr   ", rhdr, rdata, 0)) {
		DEBUG(0,("rpc_check_hdr: Failed to unmarshall RPC_HDR.\n"));
		return False;
	}

	if (prs_offset(rdata) != RPC_HEADER_LEN) {
		DEBUG(0,("rpc_check_hdr: offset was %x, should be %x.\n", prs_offset(rdata), RPC_HEADER_LEN));
		return False;
	}

	(*first) = IS_BITS_SET_ALL(rhdr->flags, RPC_FLG_FIRST);
	(*last) = IS_BITS_SET_ALL(rhdr->flags, RPC_FLG_LAST );
	(*len) = (uint32)rhdr->frag_len - prs_data_size(rdata);

	return (rhdr->pkt_type != RPC_FAULT);
}

static void NTLMSSPcalc_ap( struct cli_state *cli, unsigned char *data, uint32 len)
{
	unsigned char *hash = cli->ntlmssp_hash;
	unsigned char index_i = hash[256];
	unsigned char index_j = hash[257];
	int ind;

	for( ind = 0; ind < len; ind++) {
		unsigned char tc;
		unsigned char t;

		index_i++;
		index_j += hash[index_i];

		tc = hash[index_i];
		hash[index_i] = hash[index_j];
		hash[index_j] = tc;

		t = hash[index_i] + hash[index_j];
		data[ind] = data[ind] ^ hash[t];
	}

	hash[256] = index_i;
	hash[257] = index_j;
}

/****************************************************************************
 Verify data on an rpc pipe.
 The VERIFY & SEAL code is only executed on packets that look like this :

 Request/Response PDU's look like the following...

 |<------------------PDU len----------------------------------------------->|
 |<-HDR_LEN-->|<--REQ LEN------>|.............|<-AUTH_HDRLEN->|<-AUTH_LEN-->|

 +------------+-----------------+-------------+---------------+-------------+
 | RPC HEADER | REQ/RESP HEADER | DATA ...... | AUTH_HDR      | AUTH DATA   |
 +------------+-----------------+-------------+---------------+-------------+

 Never on bind requests/responses.
 ****************************************************************************/

static BOOL rpc_auth_pipe(struct cli_state *cli, prs_struct *rdata, int len, int auth_len)
{
	/*
	 * The following is that length of the data we must sign or seal.
	 * This doesn't include the RPC headers or the auth_len or the RPC_HDR_AUTH_LEN
	 * preceeding the auth_data.
	 */

	int data_len = len - RPC_HEADER_LEN - RPC_HDR_RESP_LEN - RPC_HDR_AUTH_LEN - auth_len;

	/*
	 * The start of the data to sign/seal is just after the RPC headers.
	 */
	char *reply_data = prs_data_p(rdata) + RPC_HEADER_LEN + RPC_HDR_REQ_LEN;

	BOOL auth_verify = IS_BITS_SET_ALL(cli->ntlmssp_srv_flgs, NTLMSSP_NEGOTIATE_SIGN);
	BOOL auth_seal = IS_BITS_SET_ALL(cli->ntlmssp_srv_flgs, NTLMSSP_NEGOTIATE_SEAL);

	DEBUG(5,("rpc_auth_pipe: len: %d auth_len: %d verify %s seal %s\n",
	          len, auth_len, BOOLSTR(auth_verify), BOOLSTR(auth_seal)));

	/*
	 * Unseal any sealed data in the PDU, not including the
	 * 8 byte auth_header or the auth_data.
	 */

	if (auth_seal) {
		DEBUG(10,("rpc_auth_pipe: unseal\n"));
		dump_data(100, reply_data, data_len);
		NTLMSSPcalc_ap(cli, (uchar*)reply_data, data_len);
		dump_data(100, reply_data, data_len);
	}

	if (auth_verify || auth_seal) {
		RPC_HDR_AUTH rhdr_auth; 
		prs_struct auth_req;
		char data[RPC_HDR_AUTH_LEN];
		/*
		 * We set dp to be the end of the packet, minus the auth_len
		 * and the length of the header that preceeds the auth_data.
		 */
		char *dp = prs_data_p(rdata) + len - auth_len - RPC_HDR_AUTH_LEN;

		if(dp - prs_data_p(rdata) > prs_data_size(rdata)) {
			DEBUG(0,("rpc_auth_pipe: auth data > data size !\n"));
			return False;
		}

		memcpy(data, dp, sizeof(data));
		
		prs_init(&auth_req , 0, 4, UNMARSHALL);
		prs_give_memory(&auth_req, data, RPC_HDR_AUTH_LEN, False);

		/*
		 * Unmarshall the 8 byte auth_header that comes before the
		 * auth data.
		 */

		if(!smb_io_rpc_hdr_auth("hdr_auth", &rhdr_auth, &auth_req, 0)) {
			DEBUG(0,("rpc_auth_pipe: unmarshalling RPC_HDR_AUTH failed.\n"));
			return False;
		}

		if (!rpc_hdr_auth_chk(&rhdr_auth)) {
			DEBUG(0,("rpc_auth_pipe: rpc_hdr_auth_chk failed.\n"));
			return False;
		}
	}

	/*
	 * Now unseal and check the auth verifier in the auth_data at
	 * then end of the packet. The 4 bytes skipped in the unseal
	 * seem to be a buffer pointer preceeding the sealed data.
	 */

	if (auth_verify) {
		RPC_AUTH_NTLMSSP_CHK chk;
		uint32 crc32;
		prs_struct auth_verf;
		char data[RPC_AUTH_NTLMSSP_CHK_LEN];
		char *dp = prs_data_p(rdata) + len - auth_len;

		if(dp - prs_data_p(rdata) > prs_data_size(rdata)) {
			DEBUG(0,("rpc_auth_pipe: auth data > data size !\n"));
			return False;
		}

		DEBUG(10,("rpc_auth_pipe: verify\n"));
		dump_data(100, dp, auth_len);
		NTLMSSPcalc_ap(cli, (uchar*)(dp+4), auth_len - 4);

		memcpy(data, dp, RPC_AUTH_NTLMSSP_CHK_LEN);
		dump_data(100, data, auth_len);

		prs_init(&auth_verf, 0, 4, UNMARSHALL);
		prs_give_memory(&auth_verf, data, RPC_AUTH_NTLMSSP_CHK_LEN, False);

		if(!smb_io_rpc_auth_ntlmssp_chk("auth_sign", &chk, &auth_verf, 0)) {
			DEBUG(0,("rpc_auth_pipe: unmarshalling RPC_AUTH_NTLMSSP_CHK failed.\n"));
			return False;
		}

		crc32 = crc32_calc_buffer(reply_data, data_len);

		if (!rpc_auth_ntlmssp_chk(&chk, crc32 , cli->ntlmssp_seq_num)) {
			DEBUG(0,("rpc_auth_pipe: rpc_auth_ntlmssp_chk failed.\n"));
			return False;
		}
		cli->ntlmssp_seq_num++;
	}
	return True;
}


/****************************************************************************
 Send data on an rpc pipe, which *must* be in one fragment.
 receive response data from an rpc pipe, which may be large...

 Read the first fragment: unfortunately have to use SMBtrans for the first
 bit, then SMBreadX for subsequent bits.

 If first fragment received also wasn't the last fragment, continue
 getting fragments until we _do_ receive the last fragment.

 Request/Response PDU's look like the following...

 |<------------------PDU len----------------------------------------------->|
 |<-HDR_LEN-->|<--REQ LEN------>|.............|<-AUTH_HDRLEN->|<-AUTH_LEN-->|

 +------------+-----------------+-------------+---------------+-------------+
 | RPC HEADER | REQ/RESP HEADER | DATA ...... | AUTH_HDR      | AUTH DATA   |
 +------------+-----------------+-------------+---------------+-------------+

 Where the presence of the AUTH_HDR and AUTH are dependent on the
 signing & sealing being neogitated.

 ****************************************************************************/

static BOOL rpc_api_pipe(struct cli_state *cli, uint16 cmd, prs_struct *data, prs_struct *rdata)
{
	uint32 len;
	char *rparam = NULL;
	uint32 rparam_len = 0;
	uint16 setup[2];
	uint32 err;
	BOOL first = True;
	BOOL last  = True;
	RPC_HDR rhdr;
	char *pdata = data ? prs_data_p(data) : NULL;
	uint32 data_len = data ? prs_offset(data) : 0;
	char *prdata = NULL;
	uint32 rdata_len = 0;
	uint32 current_offset = 0;

	/* 
	 * Create setup parameters - must be in native byte order.
	 */
	setup[0] = cmd; 
	setup[1] = cli->nt_pipe_fnum; /* Pipe file handle. */

	DEBUG(5,("rpc_api_pipe: cmd:%x fnum:%x\n", (int)cmd, (int)cli->nt_pipe_fnum));

	/* send the data: receive a response. */
	if (!cli_api_pipe(cli, "\\PIPE\\\0\0\0", 8,
	          setup, 2, 0,                     /* Setup, length, max */
	          NULL, 0, 0,                      /* Params, length, max */
	          pdata, data_len, data_len,       /* data, length, max */                  
	          &rparam, &rparam_len,            /* return params, len */
	          &prdata, &rdata_len))            /* return data, len */
	{
		DEBUG(0, ("cli_pipe: return critical error. Error was %s\n", cli_errstr(cli)));
		return False;
	}

	/*
	 * Throw away returned params - we know we won't use them.
	 */

	if(rparam) {
		free(rparam);
		rparam = NULL;
	}

	if (prdata == NULL) {
		DEBUG(0,("rpc_api_pipe: cmd %x on pipe %x failed to return data.\n",
			(int)cmd, (int)cli->nt_pipe_fnum));
		return False;
	}

	/*
	 * Give this memory as dynamically allocated to the return parse struct.
	 */

	prs_give_memory(rdata, prdata, rdata_len, True);
	current_offset = rdata_len;

	if (!rpc_check_hdr(rdata, &rhdr, &first, &last, &len)) {
		prs_mem_free(rdata);
		return False;
	}

	if (rhdr.pkt_type == RPC_BINDACK) {
		if (!last && !first) {
			DEBUG(5,("rpc_api_pipe: bug in server (AS/U?), setting fragment first/last ON.\n"));
			first = True;
			last = True;
		}
	}

	if (rhdr.pkt_type == RPC_RESPONSE) {
		RPC_HDR_RESP rhdr_resp;
		if(!smb_io_rpc_hdr_resp("rpc_hdr_resp", &rhdr_resp, rdata, 0)) {
			DEBUG(5,("rpc_api_pipe: failed to unmarshal RPC_HDR_RESP.\n"));
			prs_mem_free(rdata);
			return False;
		}
	}

	DEBUG(5,("rpc_api_pipe: len left: %u smbtrans read: %u\n",
	          (unsigned int)len, (unsigned int)rdata_len ));

	/* check if data to be sent back was too large for one SMB. */
	/* err status is only informational: the _real_ check is on the length */
	if (len > 0) { 
		/* || err == (0x80000000 | STATUS_BUFFER_OVERFLOW)) */
		/*
		 * Read the rest of the first response PDU.
		 */
		if (!rpc_read(cli, rdata, len, &current_offset)) {
			prs_mem_free(rdata);
			return False;
		}
	}

	/*
	 * Now we have a complete PDU, check the auth struct if any was sent.
	 */

	if (rhdr.auth_len != 0) {
		if(!rpc_auth_pipe(cli, rdata, rhdr.frag_len, rhdr.auth_len))
			return False;
		/*
		 * Drop the auth footers from the current offset.
		 * We need this if there are more fragments.
		 * The auth footers consist of the auth_data and the
		 * preceeding 8 byte auth_header.
		 */
		current_offset -= (rhdr.auth_len + RPC_HDR_AUTH_LEN);
	}
	
	/* 
	 * Only one rpc fragment, and it has been read.
	 */

	if (first && last) {
		DEBUG(6,("rpc_api_pipe: fragment first and last both set\n"));
		return True;
	}

	/*
	 * Read more fragments until we get the last one.
	 */

	while (!last) {
		RPC_HDR_RESP rhdr_resp;
		int num_read;
		char hdr_data[RPC_HEADER_LEN+RPC_HDR_RESP_LEN];
		prs_struct hps;

		/*
		 * First read the header of the next PDU.
		 */

		prs_init(&hps, 0, 4, UNMARSHALL);
		prs_give_memory(&hps, hdr_data, sizeof(hdr_data), False);

		num_read = cli_read(cli, cli->nt_pipe_fnum, hdr_data, 0, RPC_HEADER_LEN+RPC_HDR_RESP_LEN);
		if (cli_error(cli, NULL, &err, NULL)) {
			DEBUG(0,("rpc_api_pipe: cli_read error : %d\n", err ));
			return False;
		}

		DEBUG(5,("rpc_api_pipe: read header (size:%d)\n", num_read));

		if (num_read != RPC_HEADER_LEN+RPC_HDR_RESP_LEN) {
			DEBUG(0,("rpc_api_pipe: Error : requested %d bytes, got %d.\n",
				RPC_HEADER_LEN+RPC_HDR_RESP_LEN, num_read ));
			return False;
		}

		if (!rpc_check_hdr(&hps, &rhdr, &first, &last, &len))
			return False;

		if(!smb_io_rpc_hdr_resp("rpc_hdr_resp", &rhdr_resp, &hps, 0)) {
			DEBUG(0,("rpc_api_pipe: Error in unmarshalling RPC_HDR_RESP.\n"));
			return False;
		}

		if (first) {
			DEBUG(0,("rpc_api_pipe: secondary PDU rpc header has 'first' set !\n"));
			return False;
		}

		/*
		 * Now read the rest of the PDU.
		 */

		if (!rpc_read(cli, rdata, len, &current_offset))
			return False;

		/*
		 * Verify any authentication footer.
		 */

		if (rhdr.auth_len != 0 ) {
			if(!rpc_auth_pipe(cli, rdata, rhdr.frag_len, rhdr.auth_len))
				return False;
			/*
			 * Drop the auth footers from the current offset.
			 * The auth footers consist of the auth_data and the
			 * preceeding 8 byte auth_header.
			 * We need this if there are more fragments.
			 */
			current_offset -= (rhdr.auth_len + RPC_HDR_AUTH_LEN);
		}
	}

	return True;
}

/*******************************************************************
 creates a DCE/RPC bind request

 - initialises the parse structure.
 - dynamically allocates the header data structure
 - caller is expected to free the header data structure once used.

 ********************************************************************/

static BOOL create_rpc_bind_req(prs_struct *rpc_out, BOOL do_auth, uint32 rpc_call_id,
                                RPC_IFACE *abstract, RPC_IFACE *transfer,
                                char *my_name, char *domain, uint32 neg_flags)
{
	RPC_HDR hdr;
	RPC_HDR_RB hdr_rb;
	char buffer[4096];
	prs_struct auth_info;
	int auth_len = 0;

	prs_init(&auth_info, 0, 4, MARSHALL);

	if (do_auth) {
		RPC_HDR_AUTH hdr_auth;
		RPC_AUTH_VERIFIER auth_verifier;
		RPC_AUTH_NTLMSSP_NEG ntlmssp_neg;

		/*
		 * Create the auth structs we will marshall.
		 */

		init_rpc_hdr_auth(&hdr_auth, NTLMSSP_AUTH_TYPE, NTLMSSP_AUTH_LEVEL, 0x00, 1);
		init_rpc_auth_verifier(&auth_verifier, "NTLMSSP", NTLMSSP_NEGOTIATE);
		init_rpc_auth_ntlmssp_neg(&ntlmssp_neg, neg_flags, my_name, domain);

		/*
		 * Use the 4k buffer to store the auth info.
		 */

		prs_give_memory( &auth_info, buffer, sizeof(buffer), False);

		/*
		 * Now marshall the data into the temporary parse_struct.
		 */

		if(!smb_io_rpc_hdr_auth("hdr_auth", &hdr_auth, &auth_info, 0)) {
			DEBUG(0,("create_rpc_bind_req: failed to marshall RPC_HDR_AUTH.\n"));
			return False;
		}

		if(!smb_io_rpc_auth_verifier("auth_verifier", &auth_verifier, &auth_info, 0)) {
			DEBUG(0,("create_rpc_bind_req: failed to marshall RPC_AUTH_VERIFIER.\n"));
			return False;
		}

		if(!smb_io_rpc_auth_ntlmssp_neg("ntlmssp_neg", &ntlmssp_neg, &auth_info, 0)) {
			DEBUG(0,("create_rpc_bind_req: failed to marshall RPC_AUTH_NTLMSSP_NEG.\n"));
			return False;
		}

		/* Auth len in the rpc header doesn't include auth_header. */
		auth_len = prs_offset(&auth_info) - RPC_HDR_AUTH_LEN;
	}

	/* create the request RPC_HDR */
	init_rpc_hdr(&hdr, RPC_BIND, 0x0, rpc_call_id, 
		RPC_HEADER_LEN + RPC_HDR_RB_LEN + prs_offset(&auth_info),
		auth_len);

	if(!smb_io_rpc_hdr("hdr"   , &hdr, rpc_out, 0)) {
		DEBUG(0,("create_rpc_bind_req: failed to marshall RPC_HDR.\n"));
		return False;
	}

	/* create the bind request RPC_HDR_RB */
	init_rpc_hdr_rb(&hdr_rb, MAX_PDU_FRAG_LEN, MAX_PDU_FRAG_LEN, 0x0,
			0x1, 0x0, 0x1, abstract, transfer);

	/* Marshall the bind request data */
	if(!smb_io_rpc_hdr_rb("", &hdr_rb, rpc_out, 0)) {
		DEBUG(0,("create_rpc_bind_req: failed to marshall RPC_HDR_RB.\n"));
		return False;
	}

	/*
	 * Grow the outgoing buffer to store any auth info.
	 */

	if(hdr.auth_len != 0) {
		if(!prs_append_prs_data( rpc_out, &auth_info)) {
			DEBUG(0,("create_rpc_bind_req: failed to grow parse struct to add auth.\n"));
			return False;
		}
	}

	return True;
}

/*******************************************************************
 Creates a DCE/RPC bind authentication response.
 This is the packet that is sent back to the server once we
 have received a BIND-ACK, to finish the third leg of
 the authentication handshake.
 ********************************************************************/

static BOOL create_rpc_bind_resp(struct pwd_info *pwd,
				char *domain, char *user_name, char *my_name,
				uint32 ntlmssp_cli_flgs,
				uint32 rpc_call_id,
				prs_struct *rpc_out)
{
	unsigned char lm_owf[24];
	unsigned char nt_owf[24];
	RPC_HDR hdr;
	RPC_HDR_AUTHA hdr_autha;
	RPC_AUTH_VERIFIER auth_verifier;
	RPC_AUTH_NTLMSSP_RESP ntlmssp_resp;
	char buffer[4096];
	prs_struct auth_info;

	/*
	 * Marshall the variable length data into a temporary parse
	 * struct, pointing into a 4k local buffer.
	 */
        prs_init(&auth_info, 0, 4, MARSHALL);

	/*
	 * Use the 4k buffer to store the auth info.
	 */

	prs_give_memory( &auth_info, buffer, sizeof(buffer), False);

	/*
	 * Create the variable length auth_data.
	 */

 	init_rpc_auth_verifier(&auth_verifier, "NTLMSSP", NTLMSSP_AUTH);

	pwd_get_lm_nt_owf(pwd, lm_owf, nt_owf);
			
	init_rpc_auth_ntlmssp_resp(&ntlmssp_resp,
			         lm_owf, nt_owf,
			         domain, user_name, my_name,
			         ntlmssp_cli_flgs);

	/*
	 * Marshall the variable length auth_data into a temp parse_struct.
	 */

	if(!smb_io_rpc_auth_verifier("auth_verifier", &auth_verifier, &auth_info, 0)) {
		DEBUG(0,("create_rpc_bind_resp: failed to marshall RPC_AUTH_VERIFIER.\n"));
		return False;
	}

	if(!smb_io_rpc_auth_ntlmssp_resp("ntlmssp_resp", &ntlmssp_resp, &auth_info, 0)) {
		DEBUG(0,("create_rpc_bind_resp: failed to marshall RPC_AUTH_NTLMSSP_RESP.\n"));
		return False;
	}

	/* Create the request RPC_HDR */
	init_rpc_hdr(&hdr, RPC_BINDRESP, 0x0, rpc_call_id,
			RPC_HEADER_LEN + RPC_HDR_AUTHA_LEN + prs_offset(&auth_info),
			prs_offset(&auth_info) );

	/* Marshall it. */
	if(!smb_io_rpc_hdr("hdr", &hdr, rpc_out, 0)) {
		DEBUG(0,("create_rpc_bind_resp: failed to marshall RPC_HDR.\n"));
		return False;
	}

	/* Create the request RPC_HDR_AUTHA */
	init_rpc_hdr_autha(&hdr_autha, MAX_PDU_FRAG_LEN, MAX_PDU_FRAG_LEN,
			NTLMSSP_AUTH_TYPE, NTLMSSP_AUTH_LEVEL, 0x00);

	if(!smb_io_rpc_hdr_autha("hdr_autha", &hdr_autha, rpc_out, 0)) {
		DEBUG(0,("create_rpc_bind_resp: failed to marshall RPC_HDR_AUTHA.\n"));
		return False;
	}

	/*
	 * Append the auth data to the outgoing buffer.
	 */

	if(!prs_append_prs_data(rpc_out, &auth_info)) {
		DEBUG(0,("create_rpc_bind_req: failed to grow parse struct to add auth.\n"));
		return False;
	}

	return True;
}


/*******************************************************************
 Creates a DCE/RPC request.
 ********************************************************************/

static BOOL create_rpc_request(prs_struct *rpc_out, uint8 op_num, int data_len, int auth_len)
{
	uint32 alloc_hint;
	RPC_HDR     hdr;
	RPC_HDR_REQ hdr_req;

	DEBUG(5,("create_rpc_request: opnum: 0x%x data_len: 0x%x\n", op_num, data_len));

	/* create the rpc header RPC_HDR */
	init_rpc_hdr(&hdr, RPC_REQUEST, RPC_FLG_FIRST | RPC_FLG_LAST,
	             get_rpc_call_id(), data_len, auth_len);

	/*
	 * The alloc hint should be the amount of data, not including 
	 * RPC headers & footers.
	 */

	if (auth_len != 0)
		alloc_hint = data_len - RPC_HEADER_LEN - RPC_HDR_AUTH_LEN - auth_len;
	else
		alloc_hint = data_len - RPC_HEADER_LEN;

	DEBUG(10,("create_rpc_request: data_len: %x auth_len: %x alloc_hint: %x\n",
	           data_len, auth_len, alloc_hint));

	/* Create the rpc request RPC_HDR_REQ */
	init_rpc_hdr_req(&hdr_req, alloc_hint, op_num);

	/* stream-time... */
	if(!smb_io_rpc_hdr("hdr    ", &hdr, rpc_out, 0))
		return False;

	if(!smb_io_rpc_hdr_req("hdr_req", &hdr_req, rpc_out, 0))
		return False;

	if (prs_offset(rpc_out) != RPC_HEADER_LEN + RPC_HDR_REQ_LEN)
		return False;

	return True;
}


/****************************************************************************
 Send a request on an rpc pipe.
 ****************************************************************************/

BOOL rpc_api_pipe_req(struct cli_state *cli, uint8 op_num,
                      prs_struct *data, prs_struct *rdata)
{
	prs_struct outgoing_packet;
	uint32 data_len;
	uint32 auth_len;
	BOOL ret;
	BOOL auth_verify;
	BOOL auth_seal;
	uint32 crc32 = 0;
	char *pdata_out = NULL;

	auth_verify = IS_BITS_SET_ALL(cli->ntlmssp_srv_flgs, NTLMSSP_NEGOTIATE_SIGN);
	auth_seal   = IS_BITS_SET_ALL(cli->ntlmssp_srv_flgs, NTLMSSP_NEGOTIATE_SEAL);

	/*
	 * The auth_len doesn't include the RPC_HDR_AUTH_LEN.
	 */

	auth_len = (auth_verify ? RPC_AUTH_NTLMSSP_CHK_LEN : 0);

	/*
	 * PDU len is header, plus request header, plus data, plus
	 * auth_header_len (if present), plus auth_len (if present).
	 * NB. The auth stuff should be aligned on an 8 byte boundary
	 * to be totally DCE/RPC spec complient. For now we cheat and
	 * hope that the data structs defined are a multiple of 8 bytes.
	 */

	if((prs_offset(data) % 8) != 0) {
		DEBUG(5,("rpc_api_pipe_req: Outgoing data not a multiple of 8 bytes....\n"));
	}

	data_len = RPC_HEADER_LEN + RPC_HDR_REQ_LEN + prs_offset(data) +
			(auth_verify ? RPC_HDR_AUTH_LEN : 0) + auth_len;

	/*
	 * Malloc a parse struct to hold it (and enough for alignments).
	 */

	if(!prs_init(&outgoing_packet, data_len + 8, 4, MARSHALL)) {
		DEBUG(0,("rpc_api_pipe_req: Failed to malloc %u bytes.\n", (unsigned int)data_len ));
		return False;
	}

	pdata_out = prs_data_p(&outgoing_packet);
	
	/*
	 * Write out the RPC header and the request header.
	 */

	if(!create_rpc_request(&outgoing_packet, op_num, data_len, auth_len)) {
		DEBUG(0,("rpc_api_pipe_req: Failed to create RPC request.\n"));
		prs_mem_free(&outgoing_packet);
		return False;
	}

	/*
	 * Seal the outgoing data if requested.
	 */

	if (auth_seal) {
		crc32 = crc32_calc_buffer(prs_data_p(data), prs_offset(data));
		NTLMSSPcalc_ap(cli, (unsigned char*)prs_data_p(data), prs_offset(data));
	}

	/*
	 * Now copy the data into the outgoing packet.
	 */

	if(!prs_append_prs_data( &outgoing_packet, data)) {
		DEBUG(0,("rpc_api_pipe_req: Failed to append data to outgoing packet.\n"));
		prs_mem_free(&outgoing_packet);
		return False;
	}

	/*
	 * Add a trailing auth_verifier if needed.
	 */

	if (auth_seal || auth_verify) {
		RPC_HDR_AUTH hdr_auth;

		init_rpc_hdr_auth(&hdr_auth, NTLMSSP_AUTH_TYPE,
			NTLMSSP_AUTH_LEVEL, 0x08, (auth_verify ? 1 : 0));
		if(!smb_io_rpc_hdr_auth("hdr_auth", &hdr_auth, &outgoing_packet, 0)) {
			DEBUG(0,("rpc_api_pipe_req: Failed to marshal RPC_HDR_AUTH.\n"));
			prs_mem_free(&outgoing_packet);
			return False;
		}
	}

	/*
	 * Finally the auth data itself.
	 */

	if (auth_verify) {
		RPC_AUTH_NTLMSSP_CHK chk;
		uint32 current_offset = prs_offset(&outgoing_packet);

		init_rpc_auth_ntlmssp_chk(&chk, NTLMSSP_SIGN_VERSION, crc32, cli->ntlmssp_seq_num++);
		if(!smb_io_rpc_auth_ntlmssp_chk("auth_sign", &chk, &outgoing_packet, 0)) {
			DEBUG(0,("rpc_api_pipe_req: Failed to marshal RPC_AUTH_NTLMSSP_CHK.\n"));
			prs_mem_free(&outgoing_packet);
			return False;
		}
		NTLMSSPcalc_ap(cli, (unsigned char*)&pdata_out[current_offset+4], RPC_AUTH_NTLMSSP_CHK_LEN - 4);
	}

	DEBUG(100,("data_len: %x data_calc_len: %x\n", data_len, prs_offset(&outgoing_packet)));

	ret = rpc_api_pipe(cli, 0x0026, &outgoing_packet, rdata);

	prs_mem_free(&outgoing_packet);

	return ret;
}

/****************************************************************************
 Set the handle state.
****************************************************************************/

static BOOL rpc_pipe_set_hnd_state(struct cli_state *cli, char *pipe_name, uint16 device_state)
{
	BOOL state_set = False;
	char param[2];
	uint16 setup[2]; /* only need 2 uint16 setup parameters */
	char *rparam = NULL;
	char *rdata = NULL;
	uint32 rparam_len, rdata_len;

	if (pipe_name == NULL)
		return False;

	DEBUG(5,("Set Handle state Pipe[%x]: %s - device state:%x\n",
	cli->nt_pipe_fnum, pipe_name, device_state));

	/* create parameters: device state */
	SSVAL(param, 0, device_state);

	/* create setup parameters. */
	setup[0] = 0x0001; 
	setup[1] = cli->nt_pipe_fnum; /* pipe file handle.  got this from an SMBOpenX. */

	/* send the data on \PIPE\ */
	if (cli_api_pipe(cli, "\\PIPE\\\0\0\0", 8,
	            setup, 2, 0,                /* setup, length, max */
	            param, 2, 0,                /* param, length, max */
	            NULL, 0, 1024,              /* data, length, max */
	            &rparam, &rparam_len,        /* return param, length */
	            &rdata, &rdata_len))         /* return data, length */
	{
		DEBUG(5, ("Set Handle state: return OK\n"));
		state_set = True;
	}

	if (rparam)
		free(rparam);
	if (rdata)
		free(rdata );

	return state_set;
}

/****************************************************************************
 check the rpc bind acknowledge response
****************************************************************************/

static BOOL valid_pipe_name(char *pipe_name, RPC_IFACE *abstract, RPC_IFACE *transfer)
{
	int pipe_idx = 0;

	while (pipe_names[pipe_idx].client_pipe != NULL) {
		if (strequal(pipe_name, pipe_names[pipe_idx].client_pipe )) {
			DEBUG(5,("Bind Abstract Syntax: "));	
			dump_data(5, (char*)&(pipe_names[pipe_idx].abstr_syntax), 
			          sizeof(pipe_names[pipe_idx].abstr_syntax));
			DEBUG(5,("Bind Transfer Syntax: "));
			dump_data(5, (char*)&(pipe_names[pipe_idx].trans_syntax),
			          sizeof(pipe_names[pipe_idx].trans_syntax));

			/* copy the required syntaxes out so we can do the right bind */
			*transfer = pipe_names[pipe_idx].trans_syntax;
			*abstract = pipe_names[pipe_idx].abstr_syntax;

			return True;
		}
		pipe_idx++;
	};

	DEBUG(5,("Bind RPC Pipe[%s] unsupported\n", pipe_name));
	return False;
}

/****************************************************************************
 check the rpc bind acknowledge response
****************************************************************************/

static BOOL check_bind_response(RPC_HDR_BA *hdr_ba, char *pipe_name, RPC_IFACE *transfer)
{
	int i = 0;

	while ((pipe_names[i].client_pipe != NULL) && hdr_ba->addr.len > 0) {
		DEBUG(6,("bind_rpc_pipe: searching pipe name: client:%s server:%s\n",
		pipe_names[i].client_pipe , pipe_names[i].server_pipe ));

		if ((strequal(pipe_name, pipe_names[i].client_pipe ))) {
			if (strequal(hdr_ba->addr.str, pipe_names[i].server_pipe )) {
				DEBUG(5,("bind_rpc_pipe: server pipe_name found: %s\n",
				         pipe_names[i].server_pipe ));
				break;
			} else {
				DEBUG(4,("bind_rpc_pipe: pipe_name %s != expected pipe %s.  oh well!\n",
				         pipe_names[i].server_pipe ,
				         hdr_ba->addr.str));
				break;
			}
		} else {
			i++;
		}
	}

	if (pipe_names[i].server_pipe == NULL) {
		DEBUG(2,("bind_rpc_pipe: pipe name %s unsupported\n", hdr_ba->addr.str));
		return False;
	}

	/* check the transfer syntax */
	if ((hdr_ba->transfer.version != transfer->version) ||
	     (memcmp(&hdr_ba->transfer.uuid, &transfer->uuid, sizeof(transfer->uuid)) !=0)) {
		DEBUG(0,("bind_rpc_pipe: transfer syntax differs\n"));
		return False;
	}

	/* lkclXXXX only accept one result: check the result(s) */
	if (hdr_ba->res.num_results != 0x1 || hdr_ba->res.result != 0) {
		DEBUG(2,("bind_rpc_pipe: bind denied results: %d reason: %x\n",
		          hdr_ba->res.num_results, hdr_ba->res.reason));
	}

	DEBUG(5,("bind_rpc_pipe: accepted!\n"));
	return True;
}

/****************************************************************************
 Create and send the third packet in an RPC auth.
****************************************************************************/

static BOOL rpc_send_auth_reply(struct cli_state *cli, prs_struct *rdata, uint32 rpc_call_id)
{
	RPC_HDR_AUTH rhdr_auth;
	RPC_AUTH_VERIFIER rhdr_verf;
	RPC_AUTH_NTLMSSP_CHAL rhdr_chal;
	char buffer[MAX_PDU_FRAG_LEN];
	prs_struct rpc_out;
	ssize_t ret;

	unsigned char p24[24];
	unsigned char lm_owf[24];
	unsigned char lm_hash[16];

	if(!smb_io_rpc_hdr_auth("", &rhdr_auth, rdata, 0)) {
		DEBUG(0,("rpc_send_auth_reply: Failed to unmarshall RPC_HDR_AUTH.\n"));
		return False;
	}
	if(!smb_io_rpc_auth_verifier("", &rhdr_verf, rdata, 0)) {
		DEBUG(0,("rpc_send_auth_reply: Failed to unmarshall RPC_AUTH_VERIFIER.\n"));
		return False;
	}
	if(!smb_io_rpc_auth_ntlmssp_chal("", &rhdr_chal, rdata, 0)) {
		DEBUG(0,("rpc_send_auth_reply: Failed to unmarshall RPC_AUTH_NTLMSSP_CHAL.\n"));
		return False;
	}

	cli->ntlmssp_cli_flgs = rhdr_chal.neg_flags;

	pwd_make_lm_nt_owf(&cli->pwd, rhdr_chal.challenge);

	prs_init(&rpc_out, 0, 4, MARSHALL);

	prs_give_memory( &rpc_out, buffer, sizeof(buffer), False);

	create_rpc_bind_resp(&cli->pwd, cli->domain,
	                     cli->user_name, global_myname, 
	                     cli->ntlmssp_cli_flgs, rpc_call_id,
	                     &rpc_out);
			                    
	pwd_get_lm_nt_owf(&cli->pwd, lm_owf, NULL);
	pwd_get_lm_nt_16(&cli->pwd, lm_hash, NULL);

	NTLMSSPOWFencrypt(lm_hash, lm_owf, p24);

	{
		unsigned char j = 0;
		int ind;
		unsigned char k2[8];

		memcpy(k2, p24, 5);
		k2[5] = 0xe5;
		k2[6] = 0x38;
		k2[7] = 0xb0;

		for (ind = 0; ind < 256; ind++)
			cli->ntlmssp_hash[ind] = (unsigned char)ind;

		for( ind = 0; ind < 256; ind++) {
			unsigned char tc;

			j += (cli->ntlmssp_hash[ind] + k2[ind%8]);

			tc = cli->ntlmssp_hash[ind];
			cli->ntlmssp_hash[ind] = cli->ntlmssp_hash[j];
			cli->ntlmssp_hash[j] = tc;
		}

		cli->ntlmssp_hash[256] = 0;
		cli->ntlmssp_hash[257] = 0;
	}

	memset((char *)lm_hash, '\0', sizeof(lm_hash));

	if ((ret = cli_write(cli, cli->nt_pipe_fnum, 0x8, prs_data_p(&rpc_out), 
			0, (size_t)prs_offset(&rpc_out))) != (ssize_t)prs_offset(&rpc_out)) {
		DEBUG(0,("rpc_send_auth_reply: cli_write failed. Return was %d\n", (int)ret));
		return False;
	}

	cli->ntlmssp_srv_flgs = rhdr_chal.neg_flags;
	return True;
}

/****************************************************************************
 Do an rpc bind.
****************************************************************************/

static BOOL rpc_pipe_bind(struct cli_state *cli, char *pipe_name, char *my_name)
{
	RPC_IFACE abstract;
	RPC_IFACE transfer;
	prs_struct rpc_out;
	prs_struct rdata;
	BOOL do_auth = (cli->ntlmssp_cli_flgs != 0);
	uint32 rpc_call_id;
	char buffer[MAX_PDU_FRAG_LEN];

	DEBUG(5,("Bind RPC Pipe[%x]: %s\n", cli->nt_pipe_fnum, pipe_name));

	if (!valid_pipe_name(pipe_name, &abstract, &transfer))
		return False;

	prs_init(&rpc_out, 0, 4, MARSHALL);

	/*
	 * Use the MAX_PDU_FRAG_LEN buffer to store the bind request.
	 */

	prs_give_memory( &rpc_out, buffer, sizeof(buffer), False);

	rpc_call_id = get_rpc_call_id();

	/* Marshall the outgoing data. */
	create_rpc_bind_req(&rpc_out, do_auth, rpc_call_id,
	                    &abstract, &transfer,
	                    global_myname, cli->domain, cli->ntlmssp_cli_flgs);

	/* Initialize the incoming data struct. */
	prs_init(&rdata, 0, 4, UNMARSHALL);

	/* send data on \PIPE\.  receive a response */
	if (rpc_api_pipe(cli, 0x0026, &rpc_out, &rdata)) {
		RPC_HDR_BA   hdr_ba;

		DEBUG(5, ("rpc_pipe_bind: rpc_api_pipe returned OK.\n"));

		if(!smb_io_rpc_hdr_ba("", &hdr_ba, &rdata, 0)) {
			DEBUG(0,("rpc_pipe_bind: Failed to unmarshall RPC_HDR_BA.\n"));
			prs_mem_free(&rdata);
			return False;
		}

		if(!check_bind_response(&hdr_ba, pipe_name, &transfer)) {
			DEBUG(0,("rpc_pipe_bind: check_bind_response failed.\n"));
			prs_mem_free(&rdata);
			return False;
		}

		cli->max_xmit_frag = hdr_ba.bba.max_tsize;
		cli->max_recv_frag = hdr_ba.bba.max_rsize;

		/*
		 * If we're doing NTLMSSP auth we need to send a reply to
		 * the bind-ack to complete the 3-way challenge response
		 * handshake.
		 */

		if (do_auth && !rpc_send_auth_reply(cli, &rdata, rpc_call_id)) {
			DEBUG(0,("rpc_pipe_bind: rpc_send_auth_reply failed.\n"));
			prs_mem_free(&rdata);
			return False;
		}
	}

	prs_mem_free(&rdata);
	return True;
}

/****************************************************************************
 Set ntlmssp negotiation flags.
 ****************************************************************************/

void cli_nt_set_ntlmssp_flgs(struct cli_state *cli, uint32 ntlmssp_flgs)
{
	cli->ntlmssp_cli_flgs = ntlmssp_flgs;
}


/****************************************************************************
 Open a session.
 ****************************************************************************/

BOOL cli_nt_session_open(struct cli_state *cli, char *pipe_name)
{
	int fnum;

	if (IS_BITS_SET_ALL(cli->capabilities, CAP_NT_SMBS)) {
		if ((fnum = cli_nt_create(cli, &(pipe_name[5]))) == -1) {
			DEBUG(0,("cli_nt_session_open: cli_nt_create failed on pipe %s to machine %s.  Error was %s\n",
				 &(pipe_name[5]), cli->desthost, cli_errstr(cli)));
			return False;
		}

		cli->nt_pipe_fnum = (uint16)fnum;
	} else {
		if ((fnum = cli_open(cli, pipe_name, O_CREAT|O_RDWR, DENY_NONE)) == -1) {
			DEBUG(0,("cli_nt_session_open: cli_open failed on pipe %s to machine %s.  Error was %s\n",
				 pipe_name, cli->desthost, cli_errstr(cli)));
			return False;
		}

		cli->nt_pipe_fnum = (uint16)fnum;

		/**************** Set Named Pipe State ***************/
		if (!rpc_pipe_set_hnd_state(cli, pipe_name, 0x4300)) {
			DEBUG(0,("cli_nt_session_open: pipe hnd state failed.  Error was %s\n",
				  cli_errstr(cli)));
			cli_close(cli, cli->nt_pipe_fnum);
			return False;
		}
	}

	/******************* bind request on pipe *****************/

	if (!rpc_pipe_bind(cli, pipe_name, global_myname)) {
		DEBUG(0,("cli_nt_session_open: rpc bind failed. Error was %s\n",
		          cli_errstr(cli)));
		cli_close(cli, cli->nt_pipe_fnum);
		return False;
	}

	/* 
	 * Setup the remote server name prefixed by \ and the machine account name.
	 */

	fstrcpy(cli->srv_name_slash, "\\\\");
	fstrcat(cli->srv_name_slash, cli->desthost);
	strupper(cli->srv_name_slash);

	fstrcpy(cli->clnt_name_slash, "\\\\");
	fstrcat(cli->clnt_name_slash, global_myname);
	strupper(cli->clnt_name_slash);

	fstrcpy(cli->mach_acct, global_myname);
	fstrcat(cli->mach_acct, "$");
	strupper(cli->mach_acct);

	return True;
}

/****************************************************************************
close the session
****************************************************************************/

void cli_nt_session_close(struct cli_state *cli)
{
	cli_close(cli, cli->nt_pipe_fnum);
}
