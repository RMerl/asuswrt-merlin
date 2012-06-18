/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
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

/*  this module apparently provides an implementation of DCE/RPC over a
 *  named pipe (IPC$ connection using SMBtrans).  details of DCE/RPC
 *  documentation are available (in on-line form) from the X-Open group.
 *
 *  this module should provide a level of abstraction between SMB
 *  and DCE/RPC, while minimising the amount of mallocs, unnecessary
 *  data copies, and network traffic.
 *
 *  in this version, which takes a "let's learn what's going on and
 *  get something running" approach, there is additional network
 *  traffic generated, but the code should be easier to understand...
 *
 *  ... if you read the docs.  or stare at packets for weeks on end.
 *
 */

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;

static void NTLMSSPcalc_p( pipes_struct *p, unsigned char *data, int len)
{
    unsigned char *hash = p->ntlmssp_hash;
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

/*******************************************************************
 Generate the next PDU to be returned from the data in p->rdata. 
 We cheat here as this function doesn't handle the special auth
 footers of the authenticated bind response reply.
 ********************************************************************/

BOOL create_next_pdu(pipes_struct *p)
{
	RPC_HDR_RESP hdr_resp;
	BOOL auth_verify = IS_BITS_SET_ALL(p->ntlmssp_chal_flags, NTLMSSP_NEGOTIATE_SIGN);
	BOOL auth_seal   = IS_BITS_SET_ALL(p->ntlmssp_chal_flags, NTLMSSP_NEGOTIATE_SEAL);
	uint32 data_len;
	uint32 data_space_available;
	uint32 data_len_left;
	prs_struct outgoing_pdu;
	char *data;
	char *data_from;
	uint32 data_pos;

	/*
	 * If we're in the fault state, keep returning fault PDU's until
	 * the pipe gets closed. JRA.
	 */

	if(p->fault_state) {
		setup_fault_pdu(p);
		return True;
	}

	memset((char *)&hdr_resp, '\0', sizeof(hdr_resp));

	/* Change the incoming request header to a response. */
	p->hdr.pkt_type = RPC_RESPONSE;

	/* Set up rpc header flags. */
	if (p->out_data.data_sent_length == 0)
		p->hdr.flags = RPC_FLG_FIRST;
	else
		p->hdr.flags = 0;

	/*
	 * Work out how much we can fit in a sigle PDU.
	 */

	data_space_available = sizeof(p->out_data.current_pdu) - RPC_HEADER_LEN - RPC_HDR_RESP_LEN;
	if(p->ntlmssp_auth_validated)
		data_space_available -= (RPC_HDR_AUTH_LEN + RPC_AUTH_NTLMSSP_CHK_LEN);

	/*
	 * The amount we send is the minimum of the available
	 * space and the amount left to send.
	 */

	data_len_left = prs_offset(&p->out_data.rdata) - p->out_data.data_sent_length;

	/*
	 * Ensure there really is data left to send.
	 */

	if(!data_len_left) {
		DEBUG(0,("create_next_pdu: no data left to send !\n"));
		return False;
	}

	data_len = MIN(data_len_left, data_space_available);

	/*
	 * Set up the alloc hint. This should be the data left to
	 * send.
	 */

	hdr_resp.alloc_hint = data_len_left;

	/*
	 * Set up the header lengths.
	 */

	if (p->ntlmssp_auth_validated) {
		p->hdr.frag_len = RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len +
					RPC_HDR_AUTH_LEN + RPC_AUTH_NTLMSSP_CHK_LEN;
		p->hdr.auth_len = RPC_AUTH_NTLMSSP_CHK_LEN;
	} else {
		p->hdr.frag_len = RPC_HEADER_LEN + RPC_HDR_RESP_LEN + data_len;
		p->hdr.auth_len = 0;
	}

	/*
	 * Work out if this PDU will be the last.
	 */

	if(p->out_data.data_sent_length + data_len >= prs_offset(&p->out_data.rdata))
		p->hdr.flags |= RPC_FLG_LAST;

	/*
	 * Init the parse struct to point at the outgoing
	 * data.
	 */

	prs_init( &outgoing_pdu, 0, 4, MARSHALL);
	prs_give_memory( &outgoing_pdu, (char *)p->out_data.current_pdu, sizeof(p->out_data.current_pdu), False);

	/* Store the header in the data stream. */
	if(!smb_io_rpc_hdr("hdr", &p->hdr, &outgoing_pdu, 0)) {
		DEBUG(0,("create_next_pdu: failed to marshall RPC_HDR.\n"));
		return False;
	}

	if(!smb_io_rpc_hdr_resp("resp", &hdr_resp, &outgoing_pdu, 0)) {
		DEBUG(0,("create_next_pdu: failed to marshall RPC_HDR_RESP.\n"));
		return False;
	}

	/* Store the current offset. */
	data_pos = prs_offset(&outgoing_pdu);

	/* Copy the data into the PDU. */
	data_from = prs_data_p(&p->out_data.rdata) + p->out_data.data_sent_length;

	if(!prs_append_data(&outgoing_pdu, data_from, data_len)) {
		DEBUG(0,("create_next_pdu: failed to copy %u bytes of data.\n", (unsigned int)data_len));
		return False;
	}

	/*
	 * Set data to point to where we copied the data into.
	 */

	data = prs_data_p(&outgoing_pdu) + data_pos;

	if (p->hdr.auth_len > 0) {
		uint32 crc32 = 0;

		DEBUG(5,("create_next_pdu: sign: %s seal: %s data %d auth %d\n",
			 BOOLSTR(auth_verify), BOOLSTR(auth_seal), data_len, p->hdr.auth_len));

		if (auth_seal) {
			crc32 = crc32_calc_buffer(data, data_len);
			NTLMSSPcalc_p(p, (uchar*)data, data_len);
		}

		if (auth_seal || auth_verify) {
			RPC_HDR_AUTH auth_info;

			init_rpc_hdr_auth(&auth_info, NTLMSSP_AUTH_TYPE, NTLMSSP_AUTH_LEVEL, 
					(auth_verify ? RPC_HDR_AUTH_LEN : 0), (auth_verify ? 1 : 0));
			if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, &outgoing_pdu, 0)) {
				DEBUG(0,("create_next_pdu: failed to marshall RPC_HDR_AUTH.\n"));
				return False;
			}
		}

		if (auth_verify) {
			RPC_AUTH_NTLMSSP_CHK ntlmssp_chk;
			char *auth_data = prs_data_p(&outgoing_pdu);

			p->ntlmssp_seq_num++;
			init_rpc_auth_ntlmssp_chk(&ntlmssp_chk, NTLMSSP_SIGN_VERSION,
					crc32, p->ntlmssp_seq_num++);
			auth_data = prs_data_p(&outgoing_pdu) + prs_offset(&outgoing_pdu) + 4;
			if(!smb_io_rpc_auth_ntlmssp_chk("auth_sign", &ntlmssp_chk, &outgoing_pdu, 0)) {
				DEBUG(0,("create_next_pdu: failed to marshall RPC_AUTH_NTLMSSP_CHK.\n"));
				return False;
			}
			NTLMSSPcalc_p(p, (uchar*)auth_data, RPC_AUTH_NTLMSSP_CHK_LEN - 4);
		}
	}

	/*
	 * Setup the counts for this PDU.
	 */

	p->out_data.data_sent_length += data_len;
	p->out_data.current_pdu_len = p->hdr.frag_len;
	p->out_data.current_pdu_sent = 0;

	return True;
}

/*******************************************************************
 Process an NTLMSSP authentication response.
 If this function succeeds, the user has been authenticated
 and their domain, name and calling workstation stored in
 the pipe struct.
 The initial challenge is stored in p->challenge.
 *******************************************************************/

static BOOL api_pipe_ntlmssp_verify(pipes_struct *p, RPC_AUTH_NTLMSSP_RESP *ntlmssp_resp)
{
	uchar lm_owf[24];
	uchar nt_owf[24];
	fstring user_name;
	fstring unix_user_name;
	fstring domain;
	fstring wks;
	BOOL guest_user = False;
	struct smb_passwd *smb_pass = NULL;
	struct passwd *pass = NULL;
	uchar null_smb_passwd[16];
	uchar *smb_passwd_ptr = NULL;
	
	DEBUG(5,("api_pipe_ntlmssp_verify: checking user details\n"));

	memset(p->user_name, '\0', sizeof(p->user_name));
	memset(p->unix_user_name, '\0', sizeof(p->unix_user_name));
	memset(p->domain, '\0', sizeof(p->domain));
	memset(p->wks, '\0', sizeof(p->wks));

	/* 
	 * Setup an empty password for a guest user.
	 */

 	memset(null_smb_passwd,0,16);

	/*
	 * We always negotiate UNICODE.
	 */

	if (IS_BITS_SET_ALL(p->ntlmssp_chal_flags, NTLMSSP_NEGOTIATE_UNICODE)) {
		fstrcpy(user_name, dos_unistrn2((uint16*)ntlmssp_resp->user, ntlmssp_resp->hdr_usr.str_str_len/2));
		fstrcpy(domain, dos_unistrn2((uint16*)ntlmssp_resp->domain, ntlmssp_resp->hdr_domain.str_str_len/2));
		fstrcpy(wks, dos_unistrn2((uint16*)ntlmssp_resp->wks, ntlmssp_resp->hdr_wks.str_str_len/2));
	} else {
		fstrcpy(user_name, ntlmssp_resp->user);
		fstrcpy(domain, ntlmssp_resp->domain);
		fstrcpy(wks, ntlmssp_resp->wks);
	}

	DEBUG(5,("user: %s domain: %s wks: %s\n", user_name, domain, wks));

	memcpy(lm_owf, ntlmssp_resp->lm_resp, sizeof(lm_owf));
	memcpy(nt_owf, ntlmssp_resp->nt_resp, sizeof(nt_owf));

#ifdef DEBUG_PASSWORD
	DEBUG(100,("lm, nt owfs, chal\n"));
	dump_data(100, (char *)lm_owf, sizeof(lm_owf));
	dump_data(100, (char *)nt_owf, sizeof(nt_owf));
	dump_data(100, (char *)p->challenge, 8);
#endif

	/*
	 * Allow guest access. Patch from Shirish Kalele <kalele@veritas.com>.
	 */

	if((strlen(user_name) == 0) && 
	   (ntlmssp_resp->hdr_nt_resp.str_str_len==0))
	  {
	    guest_user = True;
	    
	    fstrcpy(unix_user_name, lp_guestaccount(-1));
	    DEBUG(100,("Null user in NTLMSSP verification. Using guest = %s\n", unix_user_name));
	    
	    smb_passwd_ptr = null_smb_passwd;
	    
	  } else {

	    /*
		 * Pass the user through the NT -> unix user mapping
		 * function.
		 */

	    fstrcpy(unix_user_name, user_name);
	    (void)map_username(unix_user_name);

	 	/* 
		 * Do the length checking only if user is not NULL.
		 */

	    if (ntlmssp_resp->hdr_lm_resp.str_str_len == 0)
	      return False;
	    if (ntlmssp_resp->hdr_nt_resp.str_str_len == 0)
	      return False;
	    if (ntlmssp_resp->hdr_usr.str_str_len == 0)
	      return False;
	    if (ntlmssp_resp->hdr_domain.str_str_len == 0)
	      return False;
	    if (ntlmssp_resp->hdr_wks.str_str_len == 0)
	      return False;

	  }

	/*
	 * Find the user in the unix password db.
	 */

	if(!(pass = Get_Pwnam(unix_user_name,True))) {
		DEBUG(1,("Couldn't find user '%s' in UNIX password database.\n",unix_user_name));
		return(False);
	}

	if(!guest_user) {

		become_root(True);

		if(!(p->ntlmssp_auth_validated = pass_check_smb(unix_user_name, domain,
		                      (uchar*)p->challenge, lm_owf, nt_owf, NULL))) {
			DEBUG(1,("api_pipe_ntlmssp_verify: User %s\\%s from machine %s \
failed authentication on named pipe %s.\n", domain, unix_user_name, wks, p->name ));
			unbecome_root(True);
			return False;
		}

		if(!(smb_pass = getsmbpwnam(unix_user_name))) {
			DEBUG(1,("api_pipe_ntlmssp_verify: Cannot find user %s in smb passwd database.\n",
				unix_user_name));
			unbecome_root(True);
			return False;
		}

		unbecome_root(True);

		if (smb_pass == NULL) {
			DEBUG(1,("api_pipe_ntlmssp_verify: Couldn't find user '%s' in smb_passwd file.\n", 
				unix_user_name));
			return(False);
		}

		/* Quit if the account was disabled. */
		if((smb_pass->acct_ctrl & ACB_DISABLED) || !smb_pass->smb_passwd) {
			DEBUG(1,("Account for user '%s' was disabled.\n", unix_user_name));
			return(False);
		}

		if(!smb_pass->smb_nt_passwd) {
			DEBUG(1,("Account for user '%s' has no NT password hash.\n", unix_user_name));
			return(False);
		}

		smb_passwd_ptr = smb_pass->smb_passwd;
	}

	/*
	 * Set up the sign/seal data.
	 */

	{
		uchar p24[24];
		NTLMSSPOWFencrypt(smb_passwd_ptr, lm_owf, p24);
		{
			unsigned char j = 0;
			int ind;

			unsigned char k2[8];

			memcpy(k2, p24, 5);
			k2[5] = 0xe5;
			k2[6] = 0x38;
			k2[7] = 0xb0;

			for (ind = 0; ind < 256; ind++)
				p->ntlmssp_hash[ind] = (unsigned char)ind;

			for( ind = 0; ind < 256; ind++) {
				unsigned char tc;

				j += (p->ntlmssp_hash[ind] + k2[ind%8]);

				tc = p->ntlmssp_hash[ind];
				p->ntlmssp_hash[ind] = p->ntlmssp_hash[j];
				p->ntlmssp_hash[j] = tc;
			}

			p->ntlmssp_hash[256] = 0;
			p->ntlmssp_hash[257] = 0;
		}
/*		NTLMSSPhash(p->ntlmssp_hash, p24); */
		p->ntlmssp_seq_num = 0;

	}

	fstrcpy(p->user_name, user_name);
	fstrcpy(p->unix_user_name, unix_user_name);
	fstrcpy(p->domain, domain);
	fstrcpy(p->wks, wks);

	/*
	 * Store the UNIX credential data (uid/gid pair) in the pipe structure.
	 */

	p->uid = pass->pw_uid;
	p->gid = pass->pw_gid;

	p->ntlmssp_auth_validated = True;
	return True;
}

/*******************************************************************
 The switch table for the pipe names and the functions to handle them.
 *******************************************************************/

struct api_cmd
{
  char * pipe_clnt_name;
  char * pipe_srv_name;
  BOOL (*fn) (pipes_struct *, prs_struct *);
};

static struct api_cmd api_fd_commands[] =
{
    { "lsarpc",   "lsass",   api_ntlsa_rpc },
    { "samr",     "lsass",   api_samr_rpc },
    { "srvsvc",   "ntsvcs",  api_srvsvc_rpc },
    { "wkssvc",   "ntsvcs",  api_wkssvc_rpc },
    { "NETLOGON", "lsass",   api_netlog_rpc },
#if 1 /* DISABLED_IN_2_0 JRATEST */
    { "winreg",   "winreg",  api_reg_rpc },
#endif
    { NULL,       NULL,      NULL }
};

/*******************************************************************
 This is the client reply to our challenge for an authenticated 
 bind request. The challenge we sent is in p->challenge.
*******************************************************************/

BOOL api_pipe_bind_auth_resp(pipes_struct *p, prs_struct *rpc_in_p)
{
	RPC_HDR_AUTHA autha_info;
	RPC_AUTH_VERIFIER auth_verifier;
	RPC_AUTH_NTLMSSP_RESP ntlmssp_resp;

	DEBUG(5,("api_pipe_bind_auth_resp: decode request. %d\n", __LINE__));

	if (p->hdr.auth_len == 0) {
		DEBUG(0,("api_pipe_bind_auth_resp: No auth field sent !\n"));
		return False;
	}

	/*
	 * Decode the authentication verifier response.
	 */

	if(!smb_io_rpc_hdr_autha("", &autha_info, rpc_in_p, 0)) {
		DEBUG(0,("api_pipe_bind_auth_resp: unmarshall of RPC_HDR_AUTHA failed.\n"));
		return False;
	}

	if (autha_info.auth_type != NTLMSSP_AUTH_TYPE || autha_info.auth_level != NTLMSSP_AUTH_LEVEL) {
		DEBUG(0,("api_pipe_bind_auth_resp: incorrect auth type (%d) or level (%d).\n",
			(int)autha_info.auth_type, (int)autha_info.auth_level ));
		return False;
	}

	if(!smb_io_rpc_auth_verifier("", &auth_verifier, rpc_in_p, 0)) {
		DEBUG(0,("api_pipe_bind_auth_resp: unmarshall of RPC_AUTH_VERIFIER failed.\n"));
		return False;
	}

	/*
	 * Ensure this is a NTLMSSP_AUTH packet type.
	 */

	if (!rpc_auth_verifier_chk(&auth_verifier, "NTLMSSP", NTLMSSP_AUTH)) {
		DEBUG(0,("api_pipe_bind_auth_resp: rpc_auth_verifier_chk failed.\n"));
		return False;
	}

	if(!smb_io_rpc_auth_ntlmssp_resp("", &ntlmssp_resp, rpc_in_p, 0)) {
		DEBUG(0,("api_pipe_bind_auth_resp: Failed to unmarshall RPC_AUTH_NTLMSSP_RESP.\n"));
		return False;
	}

	/*
	 * The following call actually checks the challenge/response data.
	 * for correctness against the given DOMAIN\user name.
	 */
	
	if (!api_pipe_ntlmssp_verify(p, &ntlmssp_resp))
		return False;

	p->pipe_bound = True
;
	return True;
}

/*******************************************************************
 Marshall a bind_nak pdu.
*******************************************************************/

static BOOL setup_bind_nak(pipes_struct *p)
{
	prs_struct outgoing_rpc;
	RPC_HDR nak_hdr;
	uint16 zero = 0;

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&p->out_data.rdata);

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	prs_init( &outgoing_rpc, 0, 4, MARSHALL);
	prs_give_memory( &outgoing_rpc, (char *)p->out_data.current_pdu, sizeof(p->out_data.current_pdu), False);


	/*
	 * Initialize a bind_nak header.
	 */

	init_rpc_hdr(&nak_hdr, RPC_BINDNACK, RPC_FLG_FIRST | RPC_FLG_LAST,
            p->hdr.call_id, RPC_HEADER_LEN + sizeof(uint16), 0);

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &nak_hdr, &outgoing_rpc, 0)) {
		DEBUG(0,("setup_bind_nak: marshalling of RPC_HDR failed.\n"));
		return False;
	}

	/*
	 * Now add the reject reason.
	 */

	if(!prs_uint16("reject code", &outgoing_rpc, 0, &zero))
        return False;

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_len = prs_offset(&outgoing_rpc);
	p->out_data.current_pdu_sent = 0;

	p->pipe_bound = False;

	return True;
}

/*******************************************************************
 Marshall a fault pdu.
*******************************************************************/

BOOL setup_fault_pdu(pipes_struct *p)
{
	prs_struct outgoing_pdu;
	RPC_HDR fault_hdr;
	RPC_HDR_RESP hdr_resp;
	RPC_HDR_FAULT fault_resp;

	/* Free any memory in the current return data buffer. */
	prs_mem_free(&p->out_data.rdata);

	/*
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	prs_init( &outgoing_pdu, 0, 4, MARSHALL);
	prs_give_memory( &outgoing_pdu, (char *)p->out_data.current_pdu, sizeof(p->out_data.current_pdu), False);

	/*
	 * Initialize a fault header.
	 */

	init_rpc_hdr(&fault_hdr, RPC_FAULT, RPC_FLG_FIRST | RPC_FLG_LAST | RPC_FLG_NOCALL,
            p->hdr.call_id, RPC_HEADER_LEN + RPC_HDR_RESP_LEN + RPC_HDR_FAULT_LEN, 0);

	/*
	 * Initialize the HDR_RESP and FAULT parts of the PDU.
	 */

	memset((char *)&hdr_resp, '\0', sizeof(hdr_resp));

	fault_resp.status = 0x1c010002;
	fault_resp.reserved = 0;

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &fault_hdr, &outgoing_pdu, 0)) {
		DEBUG(0,("setup_fault_pdu: marshalling of RPC_HDR failed.\n"));
		return False;
	}

	if(!smb_io_rpc_hdr_resp("resp", &hdr_resp, &outgoing_pdu, 0)) {
		DEBUG(0,("setup_fault_pdu: failed to marshall RPC_HDR_RESP.\n"));
		return False;
	}

	if(!smb_io_rpc_hdr_fault("fault", &fault_resp, &outgoing_pdu, 0)) {
		DEBUG(0,("setup_fault_pdu: failed to marshall RPC_HDR_FAULT.\n"));
		return False;
	}

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_len = prs_offset(&outgoing_pdu);
	p->out_data.current_pdu_sent = 0;

	return True;
}

/*******************************************************************
 Respond to a pipe bind request.
*******************************************************************/

BOOL api_pipe_bind_req(pipes_struct *p, prs_struct *rpc_in_p)
{
	RPC_HDR_BA hdr_ba;
	RPC_HDR_RB hdr_rb;
	RPC_HDR_AUTH auth_info;
	uint16 assoc_gid;
	fstring ack_pipe_name;
	prs_struct out_hdr_ba;
	prs_struct out_auth;
	prs_struct outgoing_rpc;
	int i = 0;
	int auth_len = 0;
	enum RPC_PKT_TYPE reply_pkt_type;

	p->ntlmssp_auth_requested = False;

	DEBUG(5,("api_pipe_bind_req: decode request. %d\n", __LINE__));

	/*
	 * Try and find the correct pipe name to ensure
	 * that this is a pipe name we support.
	 */

	for (i = 0; api_fd_commands[i].pipe_clnt_name; i++) {
		if (strequal(api_fd_commands[i].pipe_clnt_name, p->name) &&
		    api_fd_commands[i].fn != NULL) {
			DEBUG(3,("api_pipe_bind_req: \\PIPE\\%s -> \\PIPE\\%s\n",
			           api_fd_commands[i].pipe_clnt_name,
			           api_fd_commands[i].pipe_srv_name));
			fstrcpy(p->pipe_srv_name, api_fd_commands[i].pipe_srv_name);
			break;
		}
	}

	if (api_fd_commands[i].fn == NULL) {
		DEBUG(3,("api_pipe_bind_req: Unknown pipe name %s in bind request.\n",
			p->name ));
		if(!setup_bind_nak(p))
			return False;
		return True;
	}

	/* decode the bind request */
	if(!smb_io_rpc_hdr_rb("", &hdr_rb, rpc_in_p, 0))  {
		DEBUG(0,("api_pipe_bind_req: unable to unmarshall RPC_HDR_RB struct.\n"));
		return False;
	}

	/*
	 * Check if this is an authenticated request.
	 */

	if (p->hdr.auth_len != 0) {
		RPC_AUTH_VERIFIER auth_verifier;
		RPC_AUTH_NTLMSSP_NEG ntlmssp_neg;

		/* 
		 * Decode the authentication verifier.
		 */

		if(!smb_io_rpc_hdr_auth("", &auth_info, rpc_in_p, 0)) {
			DEBUG(0,("api_pipe_bind_req: unable to unmarshall RPC_HDR_AUTH struct.\n"));
			return False;
		}

		/*
		 * We only support NTLMSSP_AUTH_TYPE requests.
		 */

		if(auth_info.auth_type != NTLMSSP_AUTH_TYPE) {
			DEBUG(0,("api_pipe_bind_req: unknown auth type %x requested.\n",
				auth_info.auth_type ));
			return False;
		}

		if(!smb_io_rpc_auth_verifier("", &auth_verifier, rpc_in_p, 0)) {
			DEBUG(0,("api_pipe_bind_req: unable to unmarshall RPC_HDR_AUTH struct.\n"));
			return False;
		}

		if(!strequal(auth_verifier.signature, "NTLMSSP")) {
			DEBUG(0,("api_pipe_bind_req: auth_verifier.signature != NTLMSSP\n"));
			return False;
		}

		if(auth_verifier.msg_type != NTLMSSP_NEGOTIATE) {
			DEBUG(0,("api_pipe_bind_req: auth_verifier.msg_type (%d) != NTLMSSP_NEGOTIATE\n",
				auth_verifier.msg_type));
			return False;
		}

		if(!smb_io_rpc_auth_ntlmssp_neg("", &ntlmssp_neg, rpc_in_p, 0)) {
			DEBUG(0,("api_pipe_bind_req: Failed to unmarshall RPC_AUTH_NTLMSSP_NEG.\n"));
			return False;
		}

		p->ntlmssp_chal_flags = SMBD_NTLMSSP_NEG_FLAGS;
		p->ntlmssp_auth_requested = True;
	}

	switch(p->hdr.pkt_type) {
		case RPC_BIND:
			/* name has to be \PIPE\xxxxx */
			fstrcpy(ack_pipe_name, "\\PIPE\\");
			fstrcat(ack_pipe_name, p->pipe_srv_name);
			reply_pkt_type = RPC_BINDACK;
			break;
		case RPC_ALTCONT:
			/* secondary address CAN be NULL
			 * as the specs say it's ignored.
			 * It MUST NULL to have the spoolss working.
			 */
			fstrcpy(ack_pipe_name,"");
			reply_pkt_type = RPC_ALTCONTRESP;
			break;
		default:
			return False;
	}

	DEBUG(5,("api_pipe_bind_req: make response. %d\n", __LINE__));

	/* 
	 * Marshall directly into the outgoing PDU space. We
	 * must do this as we need to set to the bind response
	 * header and are never sending more than one PDU here.
	 */

	prs_init( &outgoing_rpc, 0, 4, MARSHALL);
	prs_give_memory( &outgoing_rpc, (char *)p->out_data.current_pdu, sizeof(p->out_data.current_pdu), False);

	/*
	 * Setup the memory to marshall the ba header, and the
	 * auth footers.
	 */

	if(!prs_init(&out_hdr_ba, 1024, 4, MARSHALL)) {
		DEBUG(0,("api_pipe_bind_req: malloc out_hdr_ba failed.\n"));
		return False;
	}

	if(!prs_init(&out_auth, 1024, 4, MARSHALL)) {
		DEBUG(0,("pi_pipe_bind_req: malloc out_auth failed.\n"));
		prs_mem_free(&out_hdr_ba);
		return False;
	}

	if (p->ntlmssp_auth_requested)
		assoc_gid = 0x7a77;
	else
		assoc_gid = hdr_rb.bba.assoc_gid ? hdr_rb.bba.assoc_gid : 0x53f0;

	/*
	 * Create the bind response struct.
	 */

	init_rpc_hdr_ba(&hdr_ba,
	                MAX_PDU_FRAG_LEN,
	                MAX_PDU_FRAG_LEN,
	                assoc_gid,
	                ack_pipe_name,
	                0x1, 0x0, 0x0,
	                &hdr_rb.transfer);

	/*
	 * and marshall it.
	 */

	if(!smb_io_rpc_hdr_ba("", &hdr_ba, &out_hdr_ba, 0)) {
		DEBUG(0,("api_pipe_bind_req: marshalling of RPC_HDR_BA failed.\n"));
		goto err_exit;
	}

	/*
	 * Now the authentication.
	 */

	if (p->ntlmssp_auth_requested) {
		RPC_AUTH_VERIFIER auth_verifier;
		RPC_AUTH_NTLMSSP_CHAL ntlmssp_chal;

		generate_random_buffer(p->challenge, 8, False);

		/*** Authentication info ***/

		init_rpc_hdr_auth(&auth_info, NTLMSSP_AUTH_TYPE, NTLMSSP_AUTH_LEVEL, RPC_HDR_AUTH_LEN, 1);
		if(!smb_io_rpc_hdr_auth("", &auth_info, &out_auth, 0)) {
			DEBUG(0,("api_pipe_bind_req: marshalling of RPC_HDR_AUTH failed.\n"));
			goto err_exit;
		}

		/*** NTLMSSP verifier ***/

		init_rpc_auth_verifier(&auth_verifier, "NTLMSSP", NTLMSSP_CHALLENGE);
		if(!smb_io_rpc_auth_verifier("", &auth_verifier, &out_auth, 0)) {
			DEBUG(0,("api_pipe_bind_req: marshalling of RPC_AUTH_VERIFIER failed.\n"));
			goto err_exit;
		}

		/* NTLMSSP challenge ***/

		init_rpc_auth_ntlmssp_chal(&ntlmssp_chal, p->ntlmssp_chal_flags, p->challenge);
		if(!smb_io_rpc_auth_ntlmssp_chal("", &ntlmssp_chal, &out_auth, 0)) {
			DEBUG(0,("api_pipe_bind_req: marshalling of RPC_AUTH_NTLMSSP_CHAL failed.\n"));
			goto err_exit;
		}

		/* Auth len in the rpc header doesn't include auth_header. */
		auth_len = prs_offset(&out_auth) - RPC_HDR_AUTH_LEN;
	}

	/*
	 * Create the header, now we know the length.
	 */

	init_rpc_hdr(&p->hdr, reply_pkt_type, RPC_FLG_FIRST | RPC_FLG_LAST,
			p->hdr.call_id,
			RPC_HEADER_LEN + prs_offset(&out_hdr_ba) + prs_offset(&out_auth),
			auth_len);

	/*
	 * Marshall the header into the outgoing PDU.
	 */

	if(!smb_io_rpc_hdr("", &p->hdr, &outgoing_rpc, 0)) {
		DEBUG(0,("pi_pipe_bind_req: marshalling of RPC_HDR failed.\n"));
		goto err_exit;
	}

	/*
	 * Now add the RPC_HDR_BA and any auth needed.
	 */

	if(!prs_append_prs_data( &outgoing_rpc, &out_hdr_ba)) {
		DEBUG(0,("api_pipe_bind_req: append of RPC_HDR_BA failed.\n"));
		goto err_exit;
	}

	if(p->ntlmssp_auth_requested && !prs_append_prs_data( &outgoing_rpc, &out_auth)) {
		DEBUG(0,("api_pipe_bind_req: append of auth info failed.\n"));
		goto err_exit;
	}

	if(!p->ntlmssp_auth_requested)
		p->pipe_bound = True;

	/*
	 * Setup the lengths for the initial reply.
	 */

	p->out_data.data_sent_length = 0;
	p->out_data.current_pdu_len = prs_offset(&outgoing_rpc);
	p->out_data.current_pdu_sent = 0;

	prs_mem_free(&out_hdr_ba);
	prs_mem_free(&out_auth);

	return True;

  err_exit:

	prs_mem_free(&out_hdr_ba);
	prs_mem_free(&out_auth);
	return False;
}

/****************************************************************************
 Deal with sign & seal processing on an RPC request.
****************************************************************************/

BOOL api_pipe_auth_process(pipes_struct *p, prs_struct *rpc_in)
{
	/*
	 * We always negotiate the following two bits....
	 */
	BOOL auth_verify = IS_BITS_SET_ALL(p->ntlmssp_chal_flags, NTLMSSP_NEGOTIATE_SIGN);
	BOOL auth_seal   = IS_BITS_SET_ALL(p->ntlmssp_chal_flags, NTLMSSP_NEGOTIATE_SEAL);
	int data_len;
	int auth_len;
	uint32 old_offset;
	uint32 crc32 = 0;

	auth_len = p->hdr.auth_len;

	if ((auth_len != RPC_AUTH_NTLMSSP_CHK_LEN) && auth_verify) {
		DEBUG(0,("api_pipe_auth_process: Incorrect auth_len %d.\n", auth_len ));
		return False;
	}

	/*
	 * The following is that length of the data we must verify or unseal.
	 * This doesn't include the RPC headers or the auth_len or the RPC_HDR_AUTH_LEN
	 * preceeding the auth_data.
	 */

	data_len = p->hdr.frag_len - RPC_HEADER_LEN - RPC_HDR_REQ_LEN - 
			(auth_verify ? RPC_HDR_AUTH_LEN : 0) - auth_len;
	
	DEBUG(5,("api_pipe_auth_process: sign: %s seal: %s data %d auth %d\n",
	         BOOLSTR(auth_verify), BOOLSTR(auth_seal), data_len, auth_len));

	if (auth_seal) {
		/*
		 * The data in rpc_in doesn't contain the RPC_HEADER as this
		 * has already been consumed.
		 */
		char *data = prs_data_p(rpc_in) + RPC_HDR_REQ_LEN;
		NTLMSSPcalc_p(p, (uchar*)data, data_len);
		crc32 = crc32_calc_buffer(data, data_len);
	}

	old_offset = prs_offset(rpc_in);

	if (auth_seal || auth_verify) {
		RPC_HDR_AUTH auth_info;

		if(!prs_set_offset(rpc_in, old_offset + data_len)) {
			DEBUG(0,("api_pipe_auth_process: cannot move offset to %u.\n",
				(unsigned int)old_offset + data_len ));
			return False;
		}

		if(!smb_io_rpc_hdr_auth("hdr_auth", &auth_info, rpc_in, 0)) {
			DEBUG(0,("api_pipe_auth_process: failed to unmarshall RPC_HDR_AUTH.\n"));
			return False;
		}
	}

	if (auth_verify) {
		RPC_AUTH_NTLMSSP_CHK ntlmssp_chk;
		char *req_data = prs_data_p(rpc_in) + prs_offset(rpc_in) + 4;

		DEBUG(5,("api_pipe_auth_process: auth %d\n", prs_offset(rpc_in) + 4));

		/*
		 * Ensure we have RPC_AUTH_NTLMSSP_CHK_LEN - 4 more bytes in the
		 * incoming buffer.
		 */
		if(prs_mem_get(rpc_in, RPC_AUTH_NTLMSSP_CHK_LEN - 4) == NULL) {
			DEBUG(0,("api_pipe_auth_process: missing %d bytes in buffer.\n",
				RPC_AUTH_NTLMSSP_CHK_LEN - 4 ));
			return False;
		}

		NTLMSSPcalc_p(p, (uchar*)req_data, RPC_AUTH_NTLMSSP_CHK_LEN - 4);
		if(!smb_io_rpc_auth_ntlmssp_chk("auth_sign", &ntlmssp_chk, rpc_in, 0)) {
			DEBUG(0,("api_pipe_auth_process: failed to unmarshall RPC_AUTH_NTLMSSP_CHK.\n"));
			return False;
		}

		if (!rpc_auth_ntlmssp_chk(&ntlmssp_chk, crc32, p->ntlmssp_seq_num)) {
			DEBUG(0,("api_pipe_auth_process: NTLMSSP check failed.\n"));
			return False;
		}
	}

	/*
	 * Return the current pointer to the data offset.
	 */

	if(!prs_set_offset(rpc_in, old_offset)) {
		DEBUG(0,("api_pipe_auth_process: failed to set offset back to %u\n",
			(unsigned int)old_offset ));
		return False;
	}

	return True;
}

/****************************************************************************
 Find the correct RPC function to call for this request.
 If the pipe is authenticated then become the correct UNIX user
 before doing the call.
****************************************************************************/

BOOL api_pipe_request(pipes_struct *p)
{
	int i = 0;
	BOOL ret = False;
	BOOL changed_user_id = False;

	if (p->ntlmssp_auth_validated) {

		if(!become_authenticated_pipe_user(p)) {
			prs_mem_free(&p->out_data.rdata);
			return False;
		}

		changed_user_id = True;
	}

	for (i = 0; api_fd_commands[i].pipe_clnt_name; i++) {
		if (strequal(api_fd_commands[i].pipe_clnt_name, p->name) &&
		    api_fd_commands[i].fn != NULL) {
			DEBUG(3,("Doing \\PIPE\\%s\n", api_fd_commands[i].pipe_clnt_name));
			ret = api_fd_commands[i].fn(p, &p->in_data.data);
		}
	}

	if(changed_user_id)
		unbecome_authenticated_pipe_user(p);

	return ret;
}

/*******************************************************************
 Calls the underlying RPC function for a named pipe.
 ********************************************************************/

BOOL api_rpcTNP(pipes_struct *p, char *rpc_name, struct api_struct *api_rpc_cmds,
				prs_struct *rpc_in)
{
	int fn_num;

	/* interpret the command */
	DEBUG(4,("api_rpcTNP: %s op 0x%x - ", rpc_name, p->hdr_req.opnum));

	for (fn_num = 0; api_rpc_cmds[fn_num].name; fn_num++) {
		if (api_rpc_cmds[fn_num].opnum == p->hdr_req.opnum && api_rpc_cmds[fn_num].fn != NULL) {
			DEBUG(3,("api_rpcTNP: rpc command: %s\n", api_rpc_cmds[fn_num].name));
			break;
		}
	}

	if (api_rpc_cmds[fn_num].name == NULL) {
		/*
		 * For an unknown RPC just return a fault PDU but
		 * return True to allow RPC's on the pipe to continue
		 * and not put the pipe into fault state. JRA.
		 */
		DEBUG(4, ("unknown\n"));
		setup_fault_pdu(p);
		return True;
	}

	/* do the actual command */
	if(!api_rpc_cmds[fn_num].fn(p->vuid, rpc_in, &p->out_data.rdata)) {
		DEBUG(0,("api_rpcTNP: %s: failed.\n", rpc_name));
		prs_mem_free(&p->out_data.rdata);
		return False;
	}

	DEBUG(5,("api_rpcTNP: called %s successfully\n", rpc_name));

	return True;
}
