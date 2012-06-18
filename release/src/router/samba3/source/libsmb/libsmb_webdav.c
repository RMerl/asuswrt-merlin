//#include "includes.h"
#include "../libcli/auth/libcli_auth.h"
#include "../libcli/auth/spnego.h"
//#include "../librpc/gen_ndr/ndr_ntlmssp.h"
//#include "libsmb/ntlmssp_ndr.h"

//#define DUMP_DEBUG 1
/*==== add by Andrew to wrap cli_xxx functions ====*/
//typedef struct smb_file_s {
//	int fnum;
//	int offset;
//	int whence;
//	char *fname;
//}smb_file_t;


static const struct {
	int prot;
	const char name[24];
} prots[] = {
//	{PROTOCOL_CORE,		"PC NETWORK PROGRAM 1.0"},
//	{PROTOCOL_COREPLUS,	"MICROSOFT NETWORKS 1.03"},
//	{PROTOCOL_LANMAN1,	"MICROSOFT NETWORKS 3.0"},
//	{PROTOCOL_LANMAN1,	"LANMAN1.0"},
//	{PROTOCOL_LANMAN2,	"LM1.2X002"},
//	{PROTOCOL_LANMAN2,	"DOS LANMAN2.1"},
//	{PROTOCOL_LANMAN2,	"LANMAN2.1"},
//	{PROTOCOL_LANMAN2,	"Samba"},
	{PROTOCOL_NT1,		"NT LANMAN 1.0"},
	{PROTOCOL_NT1,		"NT LM 0.12"},
};


static bool smbc_cli_have_andx_command(const char *buf, uint16_t ofs)
{
	uint8_t wct;
printf("enter 	smbc_have_andx_command..buf=[%p]\n", buf);
	size_t buflen = talloc_get_size(buf);
printf("b1..\n");
	if ((ofs == buflen-1) || (ofs == buflen)) {
		return false;
	}

printf("b2..\n");
	wct = CVAL(buf, ofs);
	if (wct < 2) {
		/*
		 * Not enough space for the command and a following pointer
		 */
		return false;
	}
printf("b3..\n");
	uint8_t b = CVAL(buf, ofs+1);
printf("b=[%02X]\n", b);
	return (CVAL(buf, ofs+1) != 0xff);
}

static NTSTATUS smbc_cli_smb_parse(struct cli_state *cli, 
			uint8_t min_wct,
			  uint8_t *pwct, 
			  uint16_t **pvwv,
			  uint32_t *pnum_bytes, 
			  uint8_t **pbytes)
{
	NTSTATUS status = NT_STATUS_OK;
	uint8_t cmd, wct;
	uint16_t num_bytes;
	size_t wct_ofs, bytes_offset;
	int i;

	if (cli->inbuf == NULL) {
		/* This was a request without a reply */
		return NT_STATUS_OK;
	}

	wct_ofs = smb_wct;
	cmd = CVAL(cli->inbuf, smb_com);

#if 0
	for (i=0; i<state->chain_num; i++) {
		if (i < state->chain_num-1) {
			if (cmd == 0xff) {
				return NT_STATUS_REQUEST_ABORTED;
			}
			if (!is_andx_req(cmd)) {
				return NT_STATUS_INVALID_NETWORK_RESPONSE;
			}
		}

		if (!have_andx_command((char *)state->inbuf, wct_ofs)) {
			/*
			 * This request was not completed because a previous
			 * request in the chain had received an error.
			 */
			return NT_STATUS_REQUEST_ABORTED;
		}

		wct_ofs = SVAL(state->inbuf, wct_ofs + 3);

		/*
		 * Skip the all-present length field. No overflow, we've just
		 * put a 16-bit value into a size_t.
		 */
		wct_ofs += 4;

		if (wct_ofs+2 > talloc_get_size(state->inbuf)) {
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}

		cmd = CVAL(state->inbuf, wct_ofs + 1);
	}

	status = cli_pull_error((char *)cli->inbuf);
printf("a1\n");
	if (!smbc_have_andx_command((char *)cli->inbuf, wct_ofs) && NT_STATUS_IS_ERR(status)) {
		/*
		 * The last command takes the error code. All further commands
		 * down the requested chain will get a
		 * NT_STATUS_REQUEST_ABORTED.
		 */
		return -1;
	}
#endif

	wct = CVAL(cli->inbuf, wct_ofs);
	bytes_offset = wct_ofs + 1 + wct * sizeof(uint16_t);
	num_bytes = SVAL(cli->inbuf, bytes_offset);

	if (wct < min_wct) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

#if 0
	/*
	 * wct_ofs is a 16-bit value plus 4, wct is a 8-bit value, num_bytes
	 * is a 16-bit value. So bytes_offset being size_t should be far from
	 * wrapping.
	 */
	if ((bytes_offset + 2 > talloc_get_size(cli->inbuf)) || (bytes_offset > 0xffff)) {
		return -1; //NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
#endif

	if (pwct != NULL) {
		*pwct = wct;
	}
	if (pvwv != NULL) {
		*pvwv = (uint16_t *)(cli->inbuf + wct_ofs + 1);
	}
	if (pnum_bytes != NULL) {
		*pnum_bytes = num_bytes;
	}
	if (pbytes != NULL) {
		*pbytes = (uint8_t *)cli->inbuf + bytes_offset + 2;
	}

	return NT_STATUS_OK;
}

int smbc_cli_get_smb_secblob(struct cli_state *cli, unsigned char *blob)
{
	memcpy(blob, cli->secblob.data, cli->secblob.length);

	DATA_BLOB chal1 = data_blob_null;
	DATA_BLOB chal2 = data_blob_null;

	bool res = spnego_parse_challenge(cli->secblob, &chal1, &chal2);
	
	return cli->secblob.length;
}
uint32 smbc_cli_get_smb_challenge(struct cli_state *cli, char *blob)
{
	char *p;
	if (cli->protocol >= PROTOCOL_NT1) {
		if(cli->capabilities & CAP_EXTENDED_SECURITY) {
			p = strstr(cli->secblob.data, "NTLMSSP");
			memcpy(blob, &p[24], 8);
			return 8;
		} else {
			memcpy(blob, cli->secblob.data, cli->secblob.length);
			return cli->secblob.length;
		}
	} else if (cli->protocol >= PROTOCOL_LANMAN1) {
		memcpy(blob, cli->secblob.data, cli->secblob.length);
		return cli->secblob.length;
	} else {
		return 0;
	}
	return 0;
}


static void cli_set_session_key (struct cli_state *cli, const DATA_BLOB session_key) 
{
	cli->user_session_key = data_blob(session_key.data, session_key.length);
}

#ifdef DUMP_DEBUG
void dump_msg(char *buf)
{
	int i;
	int bcc=0;

	if (!DEBUGLVL(0))
		return;
	
	DEBUG(0,("size=%d\nsmb_com=0x%x\nsmb_rcls=%d\nsmb_reh=%d\nsmb_err=%d\nsmb_flg=%d\nsmb_flg2=%d\n",
			smb_len(buf),
			(int)CVAL(buf,smb_com),
			(int)CVAL(buf,smb_rcls),
			(int)CVAL(buf,smb_reh),
			(int)SVAL(buf,smb_err),
			(int)CVAL(buf,smb_flg),
			(int)SVAL(buf,smb_flg2)));
	DEBUGADD(0,("smb_tid=%d\nsmb_pid=%d\nsmb_uid=%d\nsmb_mid=%d\n",
			(int)SVAL(buf,smb_tid),
			(int)SVAL(buf,smb_pid),
			(int)SVAL(buf,smb_uid),
			(int)SVAL(buf,smb_mid)));
	DEBUGADD(0,("smt_wct=%d\n",(int)CVAL(buf,smb_wct)));

	for (i=0;i<(int)CVAL(buf,smb_wct);i++)
		DEBUGADD(0,("smb_vwv[%2d]=%5d (0x%X)\n",i,
			SVAL(buf,smb_vwv+2*i),SVAL(buf,smb_vwv+2*i)));
	
	bcc = (int)SVAL(buf,smb_vwv+2*(CVAL(buf,smb_wct)));

	DEBUGADD(0,("smb_bcc=%d\n",bcc));

	if (DEBUGLEVEL < 10)
		return;

	if (DEBUGLEVEL < 50)
		bcc = MIN(bcc, 512);
}
#endif

struct cli_state *smbc_cli_initialize()
{
	struct cli_state *cli = cli_initialise();
#if 1	
	cli->use_spnego = 1;
	cli->protocol = PROTOCOL_NT1;
#endif	
	return cli;
}

uint32 smbc_cli_connect(struct cli_state *cli, const char *desthost, int port)
{
	NTSTATUS status;
	struct sockaddr_storage ss;

	if(cli->fd >=0) {
		DEBUG(3,("already connected to server %s.\n", desthost));
		return NT_STATUS_V(NT_STATUS_OK);
	}
	zero_sockaddr(&ss);
	cli->port = port;
	status = cli_connect(cli, desthost, &ss);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3,("connected to password server %s fail\n",desthost));
		return NT_STATUS_V(status);
	}
	return NT_STATUS_V(NT_STATUS_OK);
}

void smbc_cli_shutdown(struct cli_state *cli)
{
	cli_shutdown(cli);
}

int smbc_cli_get_protocol(struct cli_state *cli)
{
	return cli->protocol;
}

int smbc_cli_get_socket(struct cli_state *cli)
{
	return cli->fd;
}

uint32 smbc_cli_get_capabilities(struct cli_state *cli)
{
	return cli->capabilities;
}

uint32 smbc_cli_send_negprot_done(struct cli_state *cli)
{
	uint8_t wct;
	uint16_t *vwv;
	uint32_t num_bytes;
	uint8_t *bytes;
	NTSTATUS status;
	uint16_t protnum;

	status = smbc_cli_smb_parse(cli, 8, &wct, &vwv, &num_bytes, &bytes);

	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_V(status);
	}

	protnum = SVAL(vwv, 0);

	if ((protnum >= ARRAY_SIZE(prots)) || (prots[protnum].prot > cli->protocol)) {
		return NT_STATUS_V(NT_STATUS_INVALID_NETWORK_RESPONSE);
	}

	cli->protocol = prots[protnum].prot;

	if ((cli->protocol < PROTOCOL_NT1) && client_is_signing_mandatory(cli)) {
		DEBUG(0,("cli_negprot: SMB signing is mandatory and the selected protocol level doesn't support it.\n"));
		return NT_STATUS_V(NT_STATUS_ACCESS_DENIED);
	}

	if (cli->protocol >= PROTOCOL_NT1) {	
		struct timespec ts;
		bool negotiated_smb_signing = false;

		/* NT protocol */
		cli->sec_mode = CVAL(vwv + 1, 0);
		cli->max_mux = SVAL(vwv + 1, 1);
		cli->max_xmit = IVAL(vwv + 3, 1);
		cli->sesskey = IVAL(vwv + 7, 1);
		cli->serverzone = SVALS(vwv + 15, 1);
		cli->serverzone *= 60;
		/* this time arrives in real GMT */
		ts = interpret_long_date(((char *)(vwv+11))+1);
		cli->servertime = ts.tv_sec;
		cli->secblob = data_blob(bytes, num_bytes);
		cli->capabilities = IVAL(vwv + 9, 1);
		if (cli->capabilities & CAP_RAW_MODE) {
			cli->readbraw_supported = True;
			cli->writebraw_supported = True;	  
		}
		/* work out if they sent us a workgroup */
		if (!(cli->capabilities & CAP_EXTENDED_SECURITY) &&
			smb_buflen(cli->inbuf) > 8) {
			clistr_pull(cli->inbuf, cli->server_domain,
					bytes+8, sizeof(cli->server_domain),
					num_bytes-8,
					STR_UNICODE|STR_NOALIGN);
		}

		/*
		 * As signing is slow we only turn it on if either the client or
		 * the server require it. JRA.
		 */

		if (cli->sec_mode & NEGOTIATE_SECURITY_SIGNATURES_REQUIRED) {
			/* Fail if server says signing is mandatory and we don't want to support it. */
			if (!client_is_signing_allowed(cli)) {
				DEBUG(0,("cli_negprot: SMB signing is mandatory and we have disabled it.\n"));
				return NT_STATUS_V(NT_STATUS_ACCESS_DENIED);
			}
			negotiated_smb_signing = true;
		} else if (client_is_signing_mandatory(cli) && client_is_signing_allowed(cli)) {
			/* Fail if client says signing is mandatory and the server doesn't support it. */
			if (!(cli->sec_mode & NEGOTIATE_SECURITY_SIGNATURES_ENABLED)) {
				DEBUG(1,("cli_negprot: SMB signing is mandatory and the server doesn't support it.\n"));
				return NT_STATUS_V(NT_STATUS_ACCESS_DENIED);
			}
			negotiated_smb_signing = true;
		} else if (cli->sec_mode & NEGOTIATE_SECURITY_SIGNATURES_ENABLED) {
			negotiated_smb_signing = true;
		}

		if (negotiated_smb_signing) {
			cli_set_signing_negotiated(cli);
		}

		if (cli->capabilities & (CAP_LARGE_READX|CAP_LARGE_WRITEX)) {
			SAFE_FREE(cli->outbuf);
			SAFE_FREE(cli->inbuf);
			cli->outbuf = (char *)SMB_MALLOC(CLI_SAMBA_MAX_LARGE_READX_SIZE+LARGE_WRITEX_HDR_SIZE+SAFETY_MARGIN);
			cli->inbuf = (char *)SMB_MALLOC(CLI_SAMBA_MAX_LARGE_READX_SIZE+LARGE_WRITEX_HDR_SIZE+SAFETY_MARGIN);
			cli->bufsize = CLI_SAMBA_MAX_LARGE_READX_SIZE + LARGE_WRITEX_HDR_SIZE;
		}

	} else if (cli->protocol >= PROTOCOL_LANMAN1) {
		cli->use_spnego = False;
		cli->sec_mode = SVAL(vwv + 1, 0);
		cli->max_xmit = SVAL(vwv + 2, 0);
		cli->max_mux = SVAL(vwv + 3, 0);
		cli->sesskey = IVAL(vwv + 6, 0);
		cli->serverzone = SVALS(vwv + 10, 0);
		cli->serverzone *= 60;
		/* this time is converted to GMT by make_unix_date */
		cli->servertime = cli_make_unix_date(cli, (char *)(vwv + 8));
		cli->readbraw_supported = ((SVAL(vwv + 5, 0) & 0x1) != 0);
		cli->writebraw_supported = ((SVAL(vwv + 5, 0) & 0x2) != 0);
		cli->secblob = data_blob(bytes, num_bytes);
	} else {
		/* the old core protocol */
		cli->use_spnego = False;
		cli->sec_mode = 0;
		cli->serverzone = get_time_zone(time(NULL));
	}

	cli->max_xmit = MIN(cli->max_xmit, CLI_BUFFER_SIZE);

	/* a way to force ascii SMB */
	if (getenv("CLI_FORCE_ASCII"))
		cli->capabilities &= ~CAP_UNICODE;

	return NT_STATUS_V(NT_STATUS_OK);
}

uint32 smbc_cli_send_negprot(struct cli_state *cli)
{
	char *user= NULL;
	DATA_BLOB session_key = data_blob_null;
	DATA_BLOB lm_response = data_blob_null;
	NTSTATUS status;
	fstring pword;
	char *p;
	memset(cli->outbuf,'\0',smb_size);

	int numprots;

	if (cli->protocol < PROTOCOL_NT1)
		cli->use_spnego = False;

	/* setup the protocol strings */
	cli_set_message(cli->outbuf,0,0,True);

	p = smb_buf(cli->outbuf);
	for (numprots=0; numprots < ARRAY_SIZE(prots); numprots++) {
		if (prots[numprots].prot > cli->protocol) {
			break;
		}
		*p++ = 2;
		p += clistr_push(cli, p, prots[numprots].name, -1, STR_TERMINATE);
	}

	SCVAL(cli->outbuf,smb_com,SMBnegprot);
	cli_setup_bcc(cli, p);
	cli_setup_packet(cli);

	SCVAL(smb_buf(cli->outbuf),0,2);

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		return NT_STATUS_V(cli_nt_error(cli));
	}


#ifdef DUMP_DEBUG
	dump_msg(cli->inbuf);
#endif

	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);
	
	//status = cli_set_username(cli, user);
	//if (!NT_STATUS_IS_OK(status)) {
	//	return NT_STATUS_V(status);
	//}

	if (session_key.data) {
		/* Have plaintext orginal */
		cli_set_session_key(cli, session_key);
	}

	return NT_STATUS_V(NT_STATUS_OK);	
}

static uint32 smbc_cli_session_setup_capabilities(struct cli_state *cli)
{
	uint32 capabilities = CAP_NT_SMBS;

	if (!cli->force_dos_errors)
		capabilities |= CAP_STATUS32;

	if (cli->use_level_II_oplocks)
		capabilities |= CAP_LEVEL_II_OPLOCKS;

	capabilities |= (cli->capabilities & (CAP_UNICODE|CAP_LARGE_FILES|CAP_LARGE_READX|CAP_LARGE_WRITEX/*|CAP_DFS*/));
	return capabilities;
}

/****************************************************************************
 Do an old lanman2 style session setup.
****************************************************************************/
uint32 smbc_cli_session_setup_lanman2(struct cli_state *cli,
		char *ntlm_msg, 
		int ntlm_msg_len)
{
	DATA_BLOB session_key = data_blob_null;
	DATA_BLOB lm_response = data_blob_null;
	NTSTATUS status;
	fstring pword;
	char *p;
	const char *parse_string;
	struct ntlmssp_state ntlmssp_state;
	uint32 ntlmssp_command, auth_flags;
	DATA_BLOB encrypted_session_key = data_blob_null;
	DATA_BLOB blob = data_blob(ntlm_msg, ntlm_msg_len);
	TALLOC_CTX *frame = talloc_stackframe();
	
	parse_string = "CdBBUUUBd";
	bool res = msrpc_parse(frame, &blob, parse_string,
				 "NTLMSSP",
				 &ntlmssp_command,
				 &ntlmssp_state.lm_resp,
				 &ntlmssp_state.nt_resp,
				 &ntlmssp_state.domain,
				 &ntlmssp_state.user,
				 &ntlmssp_state.workstation,
				 &encrypted_session_key,
				 &auth_flags);

	uint8 *user = ntlmssp_state.user;
	uint8 *workgroup = ntlmssp_state.workstation;
	uint8 *pass = ntlmssp_state.lm_resp.data;
	int passlen = ntlmssp_state.lm_resp.length;

	if (passlen > sizeof(pword)-1) {
		TALLOC_FREE(frame);
		return NT_STATUS_V(NT_STATUS_INVALID_PARAMETER);
	}

	/* LANMAN servers predate NT status codes and Unicode and ignore those 
	   smb flags so we must disable the corresponding default capabilities  
	   that would otherwise cause the Unicode and NT Status flags to be
	   set (and even returned by the server) */

	cli->capabilities &= ~(CAP_UNICODE | CAP_STATUS32);

	/* if in share level security then don't send a password now */
	if (!(cli->sec_mode & NEGOTIATE_SECURITY_USER_LEVEL))
		passlen = 0;

	if (passlen > 0 && (cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) && passlen != 24) {
		/* Encrypted mode needed, and non encrypted password supplied. */
		lm_response = data_blob(NULL, 24);
		if (!SMBencrypt(pass, cli->secblob.data,(uchar *)lm_response.data)) {
			DEBUG(1, ("Password is > 14 chars in length, and is therefore incompatible with Lanman authentication\n"));
			TALLOC_FREE(frame);
			return NT_STATUS_V(NT_STATUS_ACCESS_DENIED);
		}
	} else if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) && passlen == 24) {
		/* Encrypted mode needed, and encrypted password supplied. */
		lm_response = data_blob(pass, passlen);
	} else if (passlen > 0) {
		/* Plaintext mode needed, assume plaintext supplied. */
		passlen = clistr_push(cli, pword, pass, sizeof(pword), STR_TERMINATE);
		lm_response = data_blob(pass, passlen);
	}

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);
	cli_set_message(cli->outbuf,10, 0, True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
	
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,cli->max_xmit);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,1);
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv7,lm_response.length);

	p = smb_buf(cli->outbuf);
	memcpy(p,lm_response.data,lm_response.length);
	p += lm_response.length;
	p += clistr_push(cli, p, user, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		TALLOC_FREE(frame);
		return NT_STATUS_V(cli_nt_error(cli));
	}

	show_msg(cli->inbuf);

	if (cli_is_error(cli)) {
		TALLOC_FREE(frame);
		return NT_STATUS_V(cli_nt_error(cli));
	}
	
	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);	
	status = cli_set_username(cli, user);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	if (session_key.data) {
		/* Have plaintext orginal */
		cli_set_session_key(cli, session_key);
	}

	TALLOC_FREE(frame);
	return NT_STATUS_V(NT_STATUS_OK);
}

/****************************************************************************
   do a NT1 NTLM/LM encrypted session setup - for when extended security
   is not negotiated.
   @param cli client state to create do session setup on
   @param user username
   @param pass *either* cleartext password (passlen !=24) or LM response.
   @param ntpass NT response, implies ntpasslen >=24, implies pass is not clear
   @param workgroup The user's domain.
****************************************************************************/

uint32 smbc_cli_session_setup_nt1(struct cli_state *cli, const char *user, 
					  const char *pass, size_t passlen,
					  const char *ntpass, size_t ntpasslen,
					  const char *workgroup)
{
	uint32 capabilities = smbc_cli_session_setup_capabilities(cli);
	DATA_BLOB lm_response = data_blob_null;
	DATA_BLOB nt_response = data_blob_null;
	DATA_BLOB session_key = data_blob_null;
	NTSTATUS result;
	char *p;
	bool ok;

	if (passlen == 0) {
		/* do nothing - guest login */
	} else if (passlen != 24) {
		if (lp_client_ntlmv2_auth()) {
			DATA_BLOB server_chal;
			DATA_BLOB names_blob;
			server_chal = data_blob(cli->secblob.data, MIN(cli->secblob.length, 8)); 

			/* note that the 'workgroup' here is a best guess - we don't know
			   the server's domain at this point.  The 'server name' is also
			   dodgy... 
			*/
			names_blob = NTLMv2_generate_names_blob(NULL, cli->called.name, workgroup);

			if (!SMBNTLMv2encrypt(NULL, user, workgroup, pass, &server_chal, 
						  &names_blob,
						  &lm_response, &nt_response, NULL, &session_key)) {
				data_blob_free(&names_blob);
				data_blob_free(&server_chal);
				return NT_STATUS_V(NT_STATUS_ACCESS_DENIED);
			}
			data_blob_free(&names_blob);
			data_blob_free(&server_chal);

		} else {
			uchar nt_hash[16];
			E_md4hash(pass, nt_hash);

#ifdef LANMAN_ONLY
			nt_response = data_blob_null;
#else
			nt_response = data_blob(NULL, 24);
			SMBNTencrypt(pass,cli->secblob.data,nt_response.data);
#endif
			/* non encrypted password supplied. Ignore ntpass. */
			if (lp_client_lanman_auth()) {
				lm_response = data_blob(NULL, 24);
				if (!SMBencrypt(pass,cli->secblob.data, lm_response.data)) {
					/* Oops, the LM response is invalid, just put 
					   the NT response there instead */
					data_blob_free(&lm_response);
					lm_response = data_blob(nt_response.data, nt_response.length);
				}
			} else {
				/* LM disabled, place NT# in LM field instead */
				lm_response = data_blob(nt_response.data, nt_response.length);
			}

			session_key = data_blob(NULL, 16);
#ifdef LANMAN_ONLY
			E_deshash(pass, session_key.data);
			memset(&session_key.data[8], '\0', 8);
#else
			SMBsesskeygen_ntv1(nt_hash, session_key.data);
#endif
		}
		cli_temp_set_signing(cli);
	} else {
		/* pre-encrypted password supplied.  Only used for 
		   security=server, can't do
		   signing because we don't have original key */

		lm_response = data_blob(pass, passlen);
		nt_response = data_blob(ntpass, ntpasslen);
	}

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);

	cli_set_message(cli->outbuf,13,0,True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
			
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,cli->pid);
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv7,lm_response.length);
	SSVAL(cli->outbuf,smb_vwv8,nt_response.length);
	SIVAL(cli->outbuf,smb_vwv11,capabilities); 
	p = smb_buf(cli->outbuf);
	if (lm_response.length) {
		memcpy(p,lm_response.data, lm_response.length); p += lm_response.length;
	}
	if (nt_response.length) {
		memcpy(p,nt_response.data, nt_response.length); p += nt_response.length;
	}
	p += clistr_push(cli, p, user, -1, STR_TERMINATE);

	/* Upper case here might help some NTLMv2 implementations */
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		result = cli_nt_error(cli);
		goto end;
	}

	/* show_msg(cli->inbuf); */

	if (cli_is_error(cli)) {
		result = cli_nt_error(cli);
		goto end;
	}

#ifdef LANMAN_ONLY
	ok = cli_simple_set_signing(cli, session_key, lm_response);
#else
	ok = cli_simple_set_signing(cli, session_key, nt_response);
#endif
	if (ok) {
		if (!cli_check_sign_mac(cli, cli->inbuf, 1)) {
			result = NT_STATUS_ACCESS_DENIED;
			goto end;
		}
	}

	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);
	
	p = smb_buf(cli->inbuf);
	p += clistr_pull(cli->inbuf, cli->server_os, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
			 -1, STR_TERMINATE);
	p += clistr_pull(cli->inbuf, cli->server_domain, p, sizeof(fstring),
			 -1, STR_TERMINATE);

	if (strstr(cli->server_type, "Samba")) {
		cli->is_samba = True;
	}

	result = cli_set_username(cli, user);
	if (!NT_STATUS_IS_OK(result)) {
		goto end;
	}

	if (session_key.data) {
		/* Have plaintext orginal */
		cli_set_session_key(cli, session_key);
	}

	result = NT_STATUS_OK;
end:	
	data_blob_free(&lm_response);
	data_blob_free(&nt_response);
	data_blob_free(&session_key);
	return NT_STATUS_V(result);
}

/****************************************************************************
 Send a extended security session setup blob
****************************************************************************/
static bool smbc_cli_session_setup_blob_send(struct cli_state *cli, DATA_BLOB blob)
{
	uint32 capabilities = smbc_cli_session_setup_capabilities(cli);
	char *p;

	capabilities |= CAP_EXTENDED_SECURITY;

	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);

	cli_set_message(cli->outbuf,12,0,True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);

	cli_setup_packet(cli);

	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,1);
	SIVAL(cli->outbuf,smb_vwv5,0);
	SSVAL(cli->outbuf,smb_vwv7,blob.length);
	SIVAL(cli->outbuf,smb_vwv10,capabilities); 
	p = smb_buf(cli->outbuf);
	memcpy(p, blob.data, blob.length);
	p += blob.length;
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);
	return cli_send_smb(cli);
}


/****************************************************************************
 Send a extended security session setup blob, returning a reply blob.
****************************************************************************/
DATA_BLOB smbc_cli_session_setup_blob_receive(struct cli_state *cli)
{
	DATA_BLOB blob2 = data_blob_null;
	char *p;
	size_t len;

	if (!cli_receive_smb(cli))
		return blob2;

	show_msg(cli->inbuf);

	if (cli_is_error(cli) && !NT_STATUS_EQUAL(cli_nt_error(cli),
						  NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		return blob2;
	}

	/* use the returned vuid from now on */
	cli->vuid = SVAL(cli->inbuf,smb_uid);

	p = smb_buf(cli->inbuf);

	blob2 = data_blob(p, SVAL(cli->inbuf, smb_vwv3));

	p += blob2.length;
	p += clistr_pull(cli->inbuf, cli->server_os, p, sizeof(fstring),
			 -1, STR_TERMINATE);

	/* w2k with kerberos doesn't properly null terminate this field */
	len = smb_bufrem(cli->inbuf, p);
	if (p + len < cli->inbuf + cli->bufsize+SAFETY_MARGIN - 2) {
		char *end_of_buf = p + len;

		SSVAL(p, len, 0);
		/* Now it's null terminated. */
		p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
			-1, STR_TERMINATE);
		/*
		 * See if there's another string. If so it's the
		 * server domain (part of the 'standard' Samba
		 * server signature).
		 */
		if (p < end_of_buf) {
			p += clistr_pull(cli->inbuf, cli->server_domain, p, sizeof(fstring),
				-1, STR_TERMINATE);
		}
	} else {
		/*
		 * No room to null terminate so we can't see if there
		 * is another string (server_domain) afterwards.
		 */
		p += clistr_pull(cli->inbuf, cli->server_type, p, sizeof(fstring),
				 len, 0);
	}
	return blob2;
}


//#define AAAAAAA_CODE_TRAC
#ifdef AAAAAAA_CODE_TRAC
static NTSTATUS smbc_cli_session_setup_ntlmssp(struct cli_state *cli, const char *user, 
					  const char *pass, const char *domain)
{
	struct ntlmssp_state *ntlmssp_state;
	NTSTATUS nt_status;
	int turn = 1;
	DATA_BLOB msg1;
	DATA_BLOB blob = data_blob_null;
	DATA_BLOB blob_in = data_blob_null;
	DATA_BLOB blob_out = data_blob_null;

	cli_temp_set_signing(cli);

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_client_start(&ntlmssp_state))) {
		return nt_status;
	}
	ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_SESSION_KEY);
	if (cli->use_ccache) {
		ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_CCACHE);
	}

	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_username(ntlmssp_state, user))) {
		return nt_status;
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_domain(ntlmssp_state, domain))) {
		return nt_status;
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_password(ntlmssp_state, pass))) {
		return nt_status;
	}

	do {
		nt_status = ntlmssp_update(ntlmssp_state, 
						  blob_in, &blob_out);
		data_blob_free(&blob_in);
		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) || NT_STATUS_IS_OK(nt_status)) {
			if (turn == 1) {
				/* and wrap it in a SPNEGO wrapper */
				msg1 = gen_negTokenInit(OID_NTLMSSP, blob_out);
			} else {
				/* wrap it in SPNEGO */
				msg1 = spnego_gen_auth(blob_out);
			}

			/* now send that blob on its way */
			if (!cli_session_setup_blob_send(cli, msg1)) {
				DEBUG(3, ("Failed to send NTLMSSP/SPNEGO blob to server!\n"));
				nt_status = NT_STATUS_UNSUCCESSFUL;
			} else {
				blob = cli_session_setup_blob_receive(cli);

				nt_status = cli_nt_error(cli);
				if (cli_is_error(cli) && NT_STATUS_IS_OK(nt_status)) {
					if (cli->smb_rw_error == SMB_READ_BAD_SIG) {
						nt_status = NT_STATUS_ACCESS_DENIED;
					} else {
						nt_status = NT_STATUS_UNSUCCESSFUL;
					}
				}
			}
			data_blob_free(&msg1);
		}

		if (!blob.length) {
			if (NT_STATUS_IS_OK(nt_status)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
			}
		} else if ((turn == 1) && 
			   NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DATA_BLOB tmp_blob = data_blob_null;
			/* the server might give us back two challenges */
			if (!spnego_parse_challenge(blob, &blob_in, 
							&tmp_blob)) {
				DEBUG(3,("Failed to parse challenges\n"));
				nt_status = NT_STATUS_INVALID_PARAMETER;
			}
			data_blob_free(&tmp_blob);
		} else {
			if (!spnego_parse_auth_response(blob, nt_status, OID_NTLMSSP, 
							&blob_in)) {
				DEBUG(3,("Failed to parse auth response\n"));
				if (NT_STATUS_IS_OK(nt_status) 
					|| NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) 
					nt_status = NT_STATUS_INVALID_PARAMETER;
			}
		}
		data_blob_free(&blob);
		data_blob_free(&blob_out);
		turn++;
	} while (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED));

	data_blob_free(&blob_in);

	if (NT_STATUS_IS_OK(nt_status)) {

		if (cli->server_domain[0] == '\0') {
			fstrcpy(cli->server_domain, ntlmssp_state->server_domain);
		}
		cli_set_session_key(cli, ntlmssp_state->session_key);

		if (cli_simple_set_signing(
				cli, ntlmssp_state->session_key, data_blob_null)) {

			if (!cli_check_sign_mac(cli, cli->inbuf, 1)) {
				nt_status = NT_STATUS_ACCESS_DENIED;
			}
		}
	}

	/* we have a reference conter on ntlmssp_state, if we are signing
	   then the state will be kept by the signing engine */

	ntlmssp_end(&ntlmssp_state);

	if (!NT_STATUS_IS_OK(nt_status)) {
		cli->vuid = 0;
	}
	return nt_status;
}
#endif

/****************************************************************************
 Do a spnego/NTLMSSP encrypted session setup.
****************************************************************************/
void *smbc_cli_ntlmssp_state_alloc()
{
	struct ntlmssp_state *ntlmssp_state = (struct ntlmssp_state *)malloc(sizeof(struct ntlmssp_state));
	memset(ntlmssp_state, 0, sizeof(struct ntlmssp_state));

	return (void *)ntlmssp_state;
}

void smbc_cli_ntlmssp_state_free(void *state)
{
	struct ntlmssp_state *ntlmssp_state = (struct ntlmssp_state *)state;
	if(ntlmssp_state)
		free(ntlmssp_state);

	return;
}

uint32 smbc_cli_session_setup_ntlmssp_auth(struct cli_state *cli, void *state, 
	char *ntlm_msg, int ntlm_msg_len)
{
	struct ntlmssp_state *ntlmssp_state = (struct ntlmssp_state *)state;
	NTSTATUS nt_status;
	DATA_BLOB msg1;
	DATA_BLOB blob = data_blob_null;
	DATA_BLOB blob_in = data_blob_null;
	DATA_BLOB blob_out = data_blob_null;

	do {
		nt_status = ntlmssp_update(ntlmssp_state, blob_in, &blob_out);
		
		data_blob_free(&blob_out);
		blob_out = data_blob_talloc(talloc_autofree_context(), ntlm_msg, ntlm_msg_len);
		
		data_blob_free(&blob_in);
		//if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) || NT_STATUS_IS_OK(nt_status)) 
		{
			/* wrap it in SPNEGO */
			msg1 = spnego_gen_auth(blob_out);

			/* now send that blob on its way */
			if (!smbc_cli_session_setup_blob_send(cli, msg1)) {
				DEBUG(3, ("Failed to send NTLMSSP/SPNEGO blob to server!\n"));
				nt_status = NT_STATUS_UNSUCCESSFUL;
			} else {
				blob = smbc_cli_session_setup_blob_receive(cli);

				nt_status = cli_nt_error(cli);
				if (cli_is_error(cli) && NT_STATUS_IS_OK(nt_status)) {
					if (cli->smb_rw_error == SMB_READ_BAD_SIG) {
						nt_status = NT_STATUS_ACCESS_DENIED;
					} else {
						nt_status = NT_STATUS_UNSUCCESSFUL;
					}
				}
			}
			data_blob_free(&msg1);
		}

		if (!blob.length) {
			if (NT_STATUS_IS_OK(nt_status)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
			}
		} else {
			if (!spnego_parse_auth_response(blob, nt_status, OID_NTLMSSP, &blob_in)) {
				DEBUG(3,("Failed to parse auth response\n"));
				if (NT_STATUS_IS_OK(nt_status) || NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) 
					nt_status = NT_STATUS_INVALID_PARAMETER;
			}
		}
		data_blob_free(&blob);
		data_blob_free(&blob_out);
	} while (0);

	data_blob_free(&blob_in);

	if (NT_STATUS_IS_OK(nt_status)) {

		if (cli->server_domain[0] == '\0') {
			fstrcpy(cli->server_domain, ntlmssp_state->server_domain);
		}
		cli_set_session_key(cli, ntlmssp_state->session_key);

		if (cli_simple_set_signing(cli, ntlmssp_state->session_key, data_blob_null)) {
			if (!cli_check_sign_mac(cli, cli->inbuf, 1)) {
				nt_status = NT_STATUS_ACCESS_DENIED;
			}
		}
	}

	/* we have a reference conter on ntlmssp_state, if we are signing
	   then the state will be kept by the signing engine */

	ntlmssp_end(&ntlmssp_state);

	if (!NT_STATUS_IS_OK(nt_status)) {
		cli->vuid = 0;
	}
	return NT_STATUS_V(nt_status);
}

uint32 smbc_cli_session_setup_ntlmssp_nego(struct cli_state *cli, void *state,
		char *ntlm_msg, int ntlm_msg_len)
{
	NTSTATUS nt_status;
	struct ntlmssp_state *ntlmssp_state = (struct ntlmssp_state *)state;
	DATA_BLOB msg1;
	DATA_BLOB blob = data_blob_null;
	DATA_BLOB blob_in = data_blob_null;
	DATA_BLOB blob_out = data_blob_null;

	cli_temp_set_signing(cli);
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_client_start(&ntlmssp_state))) {
		return NT_STATUS_V(nt_status);
	}
	ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_SESSION_KEY);
	if (cli->use_ccache) {
		ntlmssp_want_feature(ntlmssp_state, NTLMSSP_FEATURE_CCACHE);
	}

#if 0
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_username(ntlmssp_state, user))) {
		return NT_STATUS_V(nt_status);
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_domain(ntlmssp_state, domain))) {
		return NT_STATUS_V(nt_status);
	}
	if (!NT_STATUS_IS_OK(nt_status = ntlmssp_set_password(ntlmssp_state, pass))) {
		return NT_STATUS_V(nt_status);
	}
#endif

	do {
		nt_status = ntlmssp_update(ntlmssp_state, blob_in, &blob_out);

		data_blob_free(&blob_out);
		blob_out = data_blob_talloc(talloc_autofree_context(), ntlm_msg, ntlm_msg_len);

		data_blob_free(&blob_in);
		if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED) || NT_STATUS_IS_OK(nt_status)) {
			/* and wrap it in a SPNEGO wrapper */
			msg1 = gen_negTokenInit(OID_NTLMSSP, blob_out);

			/* now send that blob on its way */
			if (!smbc_cli_session_setup_blob_send(cli, msg1)) {
				DEBUG(3, ("Failed to send NTLMSSP/SPNEGO blob to server!\n"));
				nt_status = NT_STATUS_UNSUCCESSFUL;
			} else {
				blob = smbc_cli_session_setup_blob_receive(cli);

				nt_status = cli_nt_error(cli);
				if (cli_is_error(cli) && NT_STATUS_IS_OK(nt_status)) {
					if (cli->smb_rw_error == SMB_READ_BAD_SIG) {
						nt_status = NT_STATUS_ACCESS_DENIED;
					} else {
						nt_status = NT_STATUS_UNSUCCESSFUL;
					}
				}
			}
			data_blob_free(&msg1);
		}

		if (!blob.length) {
			if (NT_STATUS_IS_OK(nt_status)) {
				nt_status = NT_STATUS_UNSUCCESSFUL;
			}
		} else if (NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			DATA_BLOB tmp_blob = data_blob_null;
			/* the server might give us back two challenges */
			if (!spnego_parse_challenge(blob, &blob_in, &tmp_blob)) {
				DEBUG(3,("Failed to parse challenges\n"));
				nt_status = NT_STATUS_INVALID_PARAMETER;
			}

			data_blob_free(&cli->secblob);
			cli->secblob = data_blob_talloc(talloc_autofree_context(), blob_in.data, blob_in.length);
			data_blob_free(&tmp_blob);
		}
		data_blob_free(&blob);
		data_blob_free(&blob_out);
	} while (0);
	
	return NT_STATUS_V(nt_status);	
}


int smbc_cli_send_session_setup_nego(struct cli_state *cli, void *state,
	char *ntlm_msg, int ntlm_len)
{
	char *principal = NULL;
	char *OIDs[ASN1_MAX_OIDS];
	int i;
	DATA_BLOB blob;
	const char *p = NULL;
	NTSTATUS status;
	if (cli->protocol < PROTOCOL_NT1)
		return -1; 

	DEBUG(3,("Doing spnego session setup (blob length=%lu)\n", (unsigned long)cli->secblob.length));

	/* the server might not even do spnego */
	if (cli->secblob.length <= 16) {
		DEBUG(3,("server didn't supply a full spnego negprot\n"));
		goto ntlmssp;
	}

	/* there is 16 bytes of GUID before the real spnego packet starts */
	blob = data_blob(cli->secblob.data+16, cli->secblob.length-16);

	/* The server sent us the first part of the SPNEGO exchange in the
	 * negprot reply. It is WRONG to depend on the principal sent in the
	 * negprot reply, but right now we do it. If we don't receive one,
	 * we try to best guess, then fall back to NTLM.  */
	if (!spnego_parse_negTokenInit(blob, OIDs, &principal) || OIDs[0] == NULL) {
		data_blob_free(&blob);
		return NT_STATUS_V(NT_STATUS_INVALID_PARAMETER);
	}
	data_blob_free(&blob);
	
	/* make sure the server understands kerberos */
	for (i=0;OIDs[i];i++) {
		//if (i == 0)
		//	fprintf(stderr, "got OID=%s\n", OIDs[i]);
		//else
		//	fprintf(stderr, "got OID=%s\n", OIDs[i]);
		if (strcmp(OIDs[i], OID_KERBEROS5_OLD) == 0 ||strcmp(OIDs[i], OID_KERBEROS5) == 0) {
			cli->got_kerberos_mechanism = True;
		}
		talloc_free(OIDs[i]);
	}

	TALLOC_FREE(principal);

ntlmssp:
#if 0	
	account = talloc_strdup(talloc_tos(), user);
	if (!account) {
		return NT_STATUS_V(NT_STATUS_NO_MEMORY);
	}

	/* when falling back to ntlmssp while authenticating with a machine
	 * account strip off the realm - gd */

	if ((p = strchr_m(user, '@')) != NULL) {
		account[PTR_DIFF(p,user)] = '\0';
	}
#endif	
	return smbc_cli_session_setup_ntlmssp_nego(cli, state, ntlm_msg, ntlm_len);

}

#if 0
int smbc_cli_send_session_setup(struct cli_state *cli, char *user, char *workgroup, char *pass, int passlen)
{
	//struct cli_state *cli;
	uint32 capabilities = smbc_cli_session_setup_capabilities(cli);	
	struct sockaddr_storage ss;
	//char *user= NULL;
	//DATA_BLOB session_key = data_blob_null;
	DATA_BLOB lm_response = data_blob_null;
	DATA_BLOB nt_response = data_blob_null;
	NTSTATUS status;
	fstring pword;
	char *p;

	memset(cli->outbuf,'\0',smb_size);

	//int passlen = 24;
	cli->sec_mode |= NEGOTIATE_SECURITY_CHALLENGE_RESPONSE; //just for test
	//if ((cli->sec_mode & NEGOTIATE_SECURITY_CHALLENGE_RESPONSE) && passlen == 24) {
		/* Encrypted mode needed, and encrypted password supplied. */
		lm_response = data_blob(pass, passlen);
	//}

		nt_response = data_blob(pass, passlen);

#if 1
	/* send a session setup command */
	memset(cli->outbuf,'\0',smb_size);

	cli_set_message(cli->outbuf,13,0,True);
	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
			
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,CLI_BUFFER_SIZE);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,1/*cli->pid*/); //VC number
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv7,lm_response.length);
	SSVAL(cli->outbuf,smb_vwv8,nt_response.length);
	SIVAL(cli->outbuf,smb_vwv11,capabilities); 
	p = smb_buf(cli->outbuf);
	if (lm_response.length) {
		memcpy(p,lm_response.data, lm_response.length); 
		p += lm_response.length;
	}
	if (nt_response.length) {
		memcpy(p,nt_response.data, nt_response.length); 
		p += nt_response.length;
	}
	p += clistr_push(cli, p, user, -1, STR_TERMINATE|STR_UPPER);

	/* Upper case here might help some NTLMv2 implementations */
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);

#else
	/* send a session setup command */
	cli_set_message(cli->outbuf, 10, 0, True);

	SCVAL(cli->outbuf,smb_com,SMBsesssetupX);
	cli_setup_packet(cli);
	SCVAL(cli->outbuf,smb_vwv0,0xFF);
	SSVAL(cli->outbuf,smb_vwv2,cli->max_xmit);
	SSVAL(cli->outbuf,smb_vwv3,2);
	SSVAL(cli->outbuf,smb_vwv4,1);
	SIVAL(cli->outbuf,smb_vwv5,cli->sesskey);
	SSVAL(cli->outbuf,smb_vwv7,lm_response.length);

	
	p = smb_buf(cli->outbuf);
	memcpy(p,lm_response.data,lm_response.length);
	p += lm_response.length;
	p += clistr_push(cli, p, user, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, workgroup, -1, STR_TERMINATE|STR_UPPER);
	p += clistr_push(cli, p, "Unix", -1, STR_TERMINATE);
	p += clistr_push(cli, p, "Samba", -1, STR_TERMINATE);
	cli_setup_bcc(cli, p);
#endif

	if (!cli_send_smb(cli) || !cli_receive_smb(cli)) {
		//cli_shutdown(cli);
		return -1; //cli_nt_error(cli);
	}

#ifdef DUMP_DEBUG
	dump_msg(cli->inbuf);
#endif 

	return 0;	
}
#endif


/* 
 * Generate an inode number from file name for those things that need it
 */

static ino_t
generate_inode(const char *name)
{
	if (!*name) return 2; /* FIXME, why 2 ??? */
	return (ino_t)str_checksum(name);
}


/*
 * Routine to put basic stat info into a stat structure ... Used by stat and
 * fstat below.
 */

static int
setup_stat(struct stat *st, char *fname, SMB_OFF_T size, int mode)
{
	st->st_mode = 0;
		
	if (IS_DOS_DIR(mode)) {
		st->st_mode = SMBC_DIR_MODE;
	} else {
		st->st_mode = SMBC_FILE_MODE;
	}
		
	if (IS_DOS_ARCHIVE(mode)) st->st_mode |= S_IXUSR;
	if (IS_DOS_SYSTEM(mode)) st->st_mode |= S_IXGRP;
	if (IS_DOS_HIDDEN(mode)) st->st_mode |= S_IXOTH;
	if (!IS_DOS_READONLY(mode)) st->st_mode |= S_IWUSR;
		
	st->st_size = size;
#ifdef HAVE_STAT_ST_BLKSIZE
	st->st_blksize = 512;
#endif
#ifdef HAVE_STAT_ST_BLOCKS
	st->st_blocks = (size+511)/512;
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	st->st_rdev = 0;
#endif
	st->st_uid = getuid();
	st->st_gid = getgid();
		
	if (IS_DOS_DIR(mode)) {
		st->st_nlink = 2;
	} else {
		st->st_nlink = 1;
	}
		
	if (st->st_ino == 0) {
		st->st_ino = generate_inode(fname);
	}
		
	return True;  /* FIXME: Is this needed ? */
}

/****************************************************************************
 A helper for do_list.
****************************************************************************/
static void do_list_helper(const char *mntpoint, file_info *f, const char *mask, void *state)
{
#if 0
	TALLOC_CTX *ctx = talloc_tos();
	char *dir = NULL;
	char *dir_end = NULL;

	/* Work out the directory. */
	dir = talloc_strdup(ctx, mask);
	if (!dir) {
		return;
	}
	if ((dir_end = strrchr(dir, CLI_DIRSEP_CHAR)) != NULL) {
		*dir_end = '\0';
	}

	if (f->mode & aDIR) {
		if (do_list_dirs && do_this_one(f)) {
			do_list_fn(f, dir);
		}
		if (do_list_recurse &&
			f->name &&
			!strequal(f->name,".") &&
			!strequal(f->name,"..")) {
			char *mask2 = NULL;
			char *p = NULL;

			if (!f->name[0]) {
				d_printf("Empty dir name returned. Possible server misconfiguration.\n");
				TALLOC_FREE(dir);
				return;
			}

			mask2 = talloc_asprintf(ctx,
					"%s%s",
					mntpoint,
					mask);
			if (!mask2) {
				TALLOC_FREE(dir);
				return;
			}
			p = strrchr_m(mask2,CLI_DIRSEP_CHAR);
			if (p) {
				p[1] = 0;
			} else {
				mask2[0] = '\0';
			}
			mask2 = talloc_asprintf_append(mask2,
					"%s%s*",
					f->name,
					CLI_DIRSEP_STR);
			if (!mask2) {
				TALLOC_FREE(dir);
				return;
			}
			add_to_do_list_queue(mask2);
			TALLOC_FREE(mask2);
		}
		TALLOC_FREE(dir);
		return;
	}

	if (do_this_one(f)) {
		do_list_fn(f,dir);
	}
	TALLOC_FREE(dir);
#endif	
}

/****************************************************************************
 Do a directory listing, calling fn on each file found.
 This auto-switches between old and new style.
****************************************************************************/
int smbc_cli_list(struct cli_state *cli,const char *mask,uint16 attribute,
		 void (*fn)(const char *, file_info *, const char *, void *), void *state)
{
	char pass[10] = {0};
	char share[] = "andrew";
	NTSTATUS status = cli_tcon_andx(cli, share, "?????", pass, strlen(pass)+1);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		//cli_shutdown(c);
		//TALLOC_FREE(frame);
		return -1;
	}
	int total_recvd = -1;
	if (cli->protocol <= PROTOCOL_LANMAN1) {
		//total_recvd = cli_list_old(cli, mask, attribute, fn, state);
		total_recvd = cli_list_old(cli, mask, attribute, do_list_helper, state);
	} else {
		//total_recvd = cli_list_new(cli, mask, attribute, fn, state);
		total_recvd = cli_list_new(cli, mask, attribute, do_list_helper, state);
	}
	return total_recvd;
}

int smbc_cli_parse_path(const char *fname, 
			char *pWorkgroup,
			char *pServer,
			char *pShare,
			char *pPath)
{
	SMBCCTX *context = statcont;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}
		
	DEBUG(4, ("smbc_cli_parse_path(%s)\n", fname));
		
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	memcpy(pWorkgroup, workgroup, strlen(workgroup));
	memcpy(pServer, server, strlen(server));
	memcpy(pShare, share, strlen(share));
	memcpy(pPath, path, strlen(path));

	TALLOC_FREE(frame);
	return 0;
	
}
/*
 * Set file info on an SMB server.  Use setpathinfo call first.  If that
 * fails, use setattrE..
 *
 * Access and modification time parameters are always used and must be
 * provided.  Create time, if zero, will be determined from the actual create
 * time of the file.  If non-zero, the create time will be set as well.
 *
 * "mode" (attributes) parameter may be set to -1 if it is not to be set.
 */
bool
smbc_cli_setatr(struct cli_state *cli, char *path, 
			time_t create_time,
			time_t access_time,
			time_t write_time,
			time_t change_time,
			uint16 mode)
{
	SMBCCTX *context = statcont;
	uint16_t fd;
	int ret;
	TALLOC_CTX *frame = talloc_stackframe();
		
		/*
		 * First, try setpathinfo (if qpathinfo succeeded), for it is the
		 * modern function for "new code" to be using, and it works given a
		 * filename rather than requiring that the file be opened to have its
		 * attributes manipulated.
		 */
		if (! cli_setpathinfo(cli, path,
				create_time,
				access_time,
				write_time,
				change_time,
				mode)) 
	{
				
				/*
				 * setpathinfo is not supported; go to plan B. 
				 *
				 * cli_setatr() does not work on win98, and it also doesn't
				 * support setting the access time (only the modification
				 * time), so in all cases, we open the specified file and use
				 * cli_setattrE() which should work on all OS versions, and
				 * supports both times.
				 */
				
#ifdef SHOULD_TODO
				/* Don't try {q,set}pathinfo() again, with this server */
				srv->no_pathinfo = True;
#endif

				/* Open the file */
				if (!NT_STATUS_IS_OK(cli_open(cli, path, O_RDWR, DENY_NONE, &fd))) {
						
						errno = SMBC_errno(context, cli);
			TALLOC_FREE(frame);
						return -1;
				}
				
				/* Set the new attributes */
				ret = NT_STATUS_IS_OK(cli_setattrE(cli, fd,
								   change_time,
								   access_time,
								   write_time));
				
				/* Close the file */
				cli_close(cli, fd);
				
				/*
				 * Unfortunately, setattrE() doesn't have a provision for
				 * setting the access mode (attributes).  We'll have to try
				 * cli_setatr() for that, and with only this parameter, it
				 * seems to work on win98.
				 */
				if (ret && mode != (uint16) -1) {
						ret = NT_STATUS_IS_OK(cli_setatr(cli, path, mode, 0));
				}
				
				if (! ret) {
						errno = SMBC_errno(context, cli);
			TALLOC_FREE(frame);
						return False;
				}
		}
		
	TALLOC_FREE(frame);
		return True;
}


/*
 * Get info from an SMB server on a file. Use a qpathinfo call first
 * and if that fails, use getatr, as Win95 sometimes refuses qpathinfo
 */
bool
smbc_cli_getatr(struct cli_state *cli,
			char *path,
			uint16 *mode,
			SMB_OFF_T *size,
			struct timespec *create_time_ts,
			struct timespec *access_time_ts,
			struct timespec *write_time_ts,
			struct timespec *change_time_ts,
			SMB_INO_T *ino)
{
	SMBCCTX *context = statcont;
	char *fixedpath = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	time_t write_time;
	TALLOC_CTX *frame = talloc_stackframe();
		
	if (!context || !context->internal->initialized) {
				
		errno = EINVAL;
		TALLOC_FREE(frame);
 		return False;
 	}
		
	/* path fixup for . and .. */
	if (strequal(path, ".") || strequal(path, "..")) {
		fixedpath = talloc_strdup(frame, "\\");
		if (!fixedpath) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return False;
		}
	} else {
		fixedpath = talloc_strdup(frame, path);
		if (!fixedpath) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return False;
		}
		trim_string(fixedpath, NULL, "\\..");
		trim_string(fixedpath, NULL, "\\.");
	}
	DEBUG(4,("SMBC_getatr: sending qpathinfo\n"));
		
	if (!cli_resolve_path(frame, "", context->internal->auth_info,
			cli, fixedpath,
			&targetcli, &targetpath)) {
		d_printf("Couldn't resolve %s\n", path);
				errno = ENOENT;
		TALLOC_FREE(frame);
		return False;
	}

#ifdef SHOULD_TODO		
	if (!srv->no_pathinfo2 &&
			cli_qpathinfo2(targetcli, targetpath,
						   create_time_ts,
						   access_time_ts,
						   write_time_ts,
						   change_time_ts,
						   size, mode, ino)) {
		TALLOC_FREE(frame);
		return True;
		}
#endif

	/* if this is NT then don't bother with the getatr */
	if (targetcli->capabilities & CAP_NT_SMBS) {
		errno = EPERM;
		TALLOC_FREE(frame);
		return False;
		}
		
	if (NT_STATUS_IS_OK(cli_getatr(targetcli, targetpath, mode, size, &write_time))) {
				
				struct timespec w_time_ts;
				
				w_time_ts = convert_time_t_to_timespec(write_time);
				
				if (write_time_ts != NULL) {
			*write_time_ts = w_time_ts;
				}
				
				if (create_time_ts != NULL) {
						*create_time_ts = w_time_ts;
				}
				
				if (access_time_ts != NULL) {
						*access_time_ts = w_time_ts;
				}
				
				if (change_time_ts != NULL) {
						*change_time_ts = w_time_ts;
				}
				
#ifdef SHOULD_TODO		
		srv->no_pathinfo2 = True;
#endif
		TALLOC_FREE(frame);
		return True;
	}
		
		errno = EPERM;
	TALLOC_FREE(frame);
	return False;
		
}
int fprintf_string(char *path)
{
	fprintf(stderr, "file:[%s]\n", path);
	int i, len=strlen(path);
	for(i=0;i<len;i++) 
		fprintf(stderr, "%02x", path[i]&0xff);
	fprintf(stderr, "\n");		
}

uint32_t smbc_cli_tree_connect(struct cli_state *cli, char *fname)
{
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *spath = NULL;
	char *dpath = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NT_STATUS_V(status);
	}

	if (!fname) {
		errno = EINVAL;
		return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_tree_connect(%s)\n", fname));
	frame = talloc_stackframe();	
	
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&dpath,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	/* must be a normal share */
	char pass[10] = {0};

	status = cli_tcon_andx(cli, share, "?????", pass, strlen(pass)+1);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	DEBUG(4,(" tconx ok\n"));
	
	TALLOC_FREE(frame);
	return NT_STATUS_V(status);
}

/*
 * Routine to stat a file given a name
 */
int smbc_cli_stat(struct cli_state *cli, const char *fname, struct stat *st)
{
	SMBCCTX *context = statcont;
	SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	struct timespec write_time_ts;
	struct timespec access_time_ts;
	struct timespec change_time_ts;
	SMB_OFF_T size = 0;
	uint16 mode = 0;
	SMB_INO_T ino = 0;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}
		
	DEBUG(4, ("smbc_cli_stat(%s)\n", fname));
		
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}
#if 0	
	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}
#endif

#if 0
	/* must be a normal share */
	char pass[10] = {0};

	NTSTATUS status = cli_tcon_andx(cli, share, "?????", pass, strlen(pass)+1);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4,(" tconx ok\n"));
#endif

	srv = SMB_MALLOC_P(SMBCSRV);
	if (!srv) {
		TALLOC_FREE(frame);
		errno = ENOMEM;
		return -1;
	}

	ZERO_STRUCTP(srv);
	srv->cli = cli;
	srv->dev = (dev_t)(str_checksum(server) ^ str_checksum(share));
	srv->no_pathinfo = False;
	srv->no_pathinfo2 = False;
	srv->no_nt_session = False;

	if (!SMBC_getatr(context, srv, path, &mode, &size,
			 NULL,
			 &access_time_ts,
			 &write_time_ts,
			 &change_time_ts,
			 &ino)) 
	{
		SAFE_FREE(srv);
		errno = SMBC_errno(context, cli);
		TALLOC_FREE(frame);
		return -1;
	}

	st->st_ino = ino;
		
	setup_stat(st, (char *) fname, size, mode);
		
	st->st_atime = convert_timespec_to_time_t(access_time_ts);
	st->st_ctime = convert_timespec_to_time_t(change_time_ts);
	st->st_mtime = convert_timespec_to_time_t(write_time_ts);
	st->st_dev   = srv->dev;

	SAFE_FREE(srv);

	//cli_tdis(cli);

	TALLOC_FREE(frame);
	return 0;
		
}

/*
 * Routine to open a directory
 * We accept the URL syntax explained in SMBC_parse_path(), above.
 */

static void smbc_remove_dir(SMBCFILE *dir)
{
	struct smbc_dir_list *d,*f;

	d = dir->dir_list;
	while (d) {

		f = d; d = d->next;

		SAFE_FREE(f->dirent);
		SAFE_FREE(f);

	}

	dir->dir_list = dir->dir_end = dir->dir_next = NULL;

}

static int smbc_add_dirent(SMBCFILE *dir, const char *name, const char *comment, uint32 type)
{
	struct smbc_dirent *dirent;
	int size;
	int name_length = (name == NULL ? 0 : strlen(name));
	int comment_len = (comment == NULL ? 0 : strlen(comment));

	/*
	 * Allocate space for the dirent, which must be increased by the
	 * size of the name and the comment and 1 each for the null terminator.
	 */

	size = sizeof(struct smbc_dirent) + name_length + comment_len + 2;

	dirent = (struct smbc_dirent *)SMB_MALLOC(size);

	if (!dirent) {

		dir->dir_error = ENOMEM;
		return -1;

	}

	ZERO_STRUCTP(dirent);

	if (dir->dir_list == NULL) {

		dir->dir_list = SMB_MALLOC_P(struct smbc_dir_list);
		if (!dir->dir_list) {

			SAFE_FREE(dirent);
			dir->dir_error = ENOMEM;
			return -1;

		}
		ZERO_STRUCTP(dir->dir_list);

		dir->dir_end = dir->dir_next = dir->dir_list;
	}
	else {

		dir->dir_end->next = SMB_MALLOC_P(struct smbc_dir_list);

		if (!dir->dir_end->next) {

			SAFE_FREE(dirent);
			dir->dir_error = ENOMEM;
			return -1;

		}
		ZERO_STRUCTP(dir->dir_end->next);

		dir->dir_end = dir->dir_end->next;
	}

	dir->dir_end->next = NULL;
	dir->dir_end->dirent = dirent;

	dirent->smbc_type = type;
	dirent->namelen = name_length;
	dirent->commentlen = comment_len;
	dirent->dirlen = size;

	/*
	 * dirent->namelen + 1 includes the null (no null termination needed)
	 * Ditto for dirent->commentlen.
	 * The space for the two null bytes was allocated.
	 */
	strncpy(dirent->name, (name?name:""), dirent->namelen + 1);
	dirent->comment = (char *)(&dirent->name + dirent->namelen + 1);
	strncpy(dirent->comment, (comment?comment:""), dirent->commentlen + 1);

	return 0;

}

static void smbc_dir_list_fn(const char *mnt, file_info *finfo, const char *mask, void *state)
{

	if (smbc_add_dirent((SMBCFILE *)state, finfo->name, "",
			   (finfo->mode&aDIR?SMBC_DIR:SMBC_FILE)) < 0) {

		/* Handle an error ... */

		/* FIXME: Add some code ... */

	}

}
uint32 smbc_cli_rmdir(struct cli_state *cli, const char *dname) 
{
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NT_STATUS_V(status);
	}

	if (!dname) {
		errno = EINVAL;
		return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_rmdir(%s)\n", dname));
	frame = talloc_stackframe();
		
	if (SMBC_parse_path(frame,
						context,
						dname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_rmdir(cli, path);

	TALLOC_FREE(frame);
	return NT_STATUS_V(status);
}

uint32 smbc_cli_mkdir(struct cli_state *cli, const char *dname)
{
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NT_STATUS_V(status);
	}

	if (!dname) {
		errno = EINVAL;
		return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_mkdir(%s)\n", dname));
	frame = talloc_stackframe();
		
	if (SMBC_parse_path(frame,
						context,
						dname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_mkdir(cli, path);

	TALLOC_FREE(frame);
	return NT_STATUS_V(status);
}


static void
smbc_list_unique_wg_fn(const char *name,
                  uint32 type,
                  const char *comment,
                  void *state)
{
	SMBCFILE *dir = (SMBCFILE *)state;
        struct smbc_dir_list *dir_list;
        struct smbc_dirent *dirent;
	int dirent_type;
        int do_remove = 0;

	dirent_type = dir->dir_type;

	if (smbc_add_dirent(dir, name, comment, dirent_type) < 0) {
		/* An error occurred, what do we do? */
		/* FIXME: Add some code here */
	}

        /* Point to the one just added */
        dirent = dir->dir_end->dirent;

        /* See if this was a duplicate */
        for (dir_list = dir->dir_list; dir_list != dir->dir_end; dir_list = dir_list->next) {
                if (! do_remove &&
                    strcmp(dir_list->dirent->name, dirent->name) == 0) {
                        /* Duplicate.  End end of list need to be removed. */
                        do_remove = 1;
                }

                if (do_remove && dir_list->next == dir->dir_end) {
                        /* Found the end of the list.  Remove it. */
                        dir->dir_end = dir_list;
                        free(dir_list->next);
                        free(dirent);
                        dir_list->next = NULL;
                        break;
                }
        }
}

static void
smbc_list_fn(const char *name,
        uint32 type,
        const char *comment,
        void *state)
{
	SMBCFILE *dir = (SMBCFILE *)state;
	int dirent_type;
	/*
         * We need to process the type a little ...
         *
         * Disk share     = 0x00000000
         * Print share    = 0x00000001
         * Comms share    = 0x00000002 (obsolete?)
         * IPC$ share     = 0x00000003
         *
         * administrative shares:
         * ADMIN$, IPC$, C$, D$, E$ ...  are type |= 0x80000000
         */

	if (dir->dir_type == SMBC_FILE_SHARE) {
		switch (type) {
		case 0 | 0x80000000:
		case 0:
			dirent_type = SMBC_FILE_SHARE;
			break;

		case 1:
			dirent_type = SMBC_PRINTER_SHARE;
			break;

		case 2:
			dirent_type = SMBC_COMMS_SHARE;
			break;

                case 3 | 0x80000000:
		case 3:
			dirent_type = SMBC_IPC_SHARE;
			break;

		default:
			dirent_type = SMBC_FILE_SHARE; /* FIXME, error? */
			break;
		}
	}
	else {
		dirent_type = dir->dir_type;
        }

	if (smbc_add_dirent(dir, name, comment, dirent_type) < 0) {
		/* An error occurred, what do we do? */
		/* FIXME: Add some code here */
	}
}

SMBCFILE *smbc_cli_opendir2(const char *fname)
{
fprintf(stderr, "enter smbc_cli_opendir2..with fname=[%s]\n", fname);
	SMBCCTX *context = statcont;
	int saved_errno;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *options = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	uint16 mode;
	char *p = NULL;
	SMBCSRV *srv  = NULL;
	SMBCFILE *dir = NULL;
	struct sockaddr_storage rem_ss;

	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		DEBUG(4, ("no valid context\n"));
		errno = EINVAL + 8192;
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!fname) {
		DEBUG(4, ("no valid fname\n"));
		errno = EINVAL + 8193;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (SMBC_parse_path(frame,
							context,
							fname,
							&workgroup,
							&server,
							&share,
							&path,
							&user,
							&password,
							&options)) 
	{
		DEBUG(4, ("no valid path\n"));
		errno = EINVAL + 8194;
		TALLOC_FREE(frame);
		return NULL;
	}

	DEBUG(4, ("parsed path: fname='%s' server='%s' share='%s' "
				  "path='%s' options='%s'\n",
				  fname, server, share, path, options));

	/* Ensure the options are valid */
	if (SMBC_check_options(server, share, path, options)) {
		DEBUG(4, ("unacceptable options (%s)\n", options));
		errno = EINVAL + 8195;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	dir = SMB_MALLOC_P(SMBCFILE);

	if (!dir) {
		errno = ENOMEM;
		TALLOC_FREE(frame);
		return NULL;
	}

	ZERO_STRUCTP(dir);

	dir->cli_fd   = 0;
	dir->fname	= SMB_STRDUP(fname);
	dir->srv	  = NULL;
	dir->offset   = 0;
	dir->file	 = False;
	dir->dir_list = dir->dir_next = dir->dir_end = NULL;

	//query string: smb://
	if (server[0] == (char)0) {
		int i;
		int count;
		int max_lmb_count;
		struct ip_service *ip_list;
		struct ip_service server_addr;
		struct user_auth_info u_info;

		if (share[0] != (char)0 || path[0] != (char)0) {

			errno = EINVAL + 8196;
			if (dir) {
				SAFE_FREE(dir->fname);
				SAFE_FREE(dir);
			}
			TALLOC_FREE(frame);
			return NULL;
		}

		/* Determine how many local master browsers to query */
		max_lmb_count = (smbc_getOptionBrowseMaxLmbCount(context) == 0
								 ? INT_MAX
								 : smbc_getOptionBrowseMaxLmbCount(context));
		memset(&u_info, '\0', sizeof(u_info));
		u_info.username = talloc_strdup(frame,user);
		u_info.password = talloc_strdup(frame,password);
		if (!u_info.username || !u_info.password) {
			if (dir) {
				SAFE_FREE(dir->fname);
				SAFE_FREE(dir);
			}
			TALLOC_FREE(frame);
			return NULL;
		}

		/*
		  * We have server and share and path empty but options
		  * requesting that we scan all master browsers for their list
		  * of workgroups/domains.  This implies that we must first try
		  * broadcast queries to find all master browsers, and if that
		  * doesn't work, then try our other methods which return only
		  * a single master browser.
		  */

		ip_list = NULL;
		if (!NT_STATUS_IS_OK(name_resolve_bcast(MSBROWSE, 1, &ip_list, &count))) {
			SAFE_FREE(ip_list);
			if (!find_master_ip(workgroup, &server_addr.ss)) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				errno = ENOENT;
				TALLOC_FREE(frame);
				return NULL;
			}

			ip_list = (struct ip_service *)memdup(&server_addr, sizeof(server_addr));
			if (ip_list == NULL) {
				errno = ENOMEM;
				TALLOC_FREE(frame);
				return NULL;
			}
			count = 1;
		}
		for (i = 0; i < count && i < max_lmb_count; i++) {
			char addr[INET6_ADDRSTRLEN];
			char *wg_ptr = NULL;
			struct cli_state *cli = NULL;

			print_sockaddr(addr, sizeof(addr), &ip_list[i].ss);
			DEBUG(99, ("Found master browser %d of %d: %s\n",
					   i+1, MAX(count, max_lmb_count), addr));

			cli = get_ipc_connect_master_ip(talloc_tos(), &ip_list[i], &u_info, &wg_ptr);
			/* cli == NULL is the master browser refused to talk or could not be found */
			if (!cli) {
				continue;
			}

			workgroup = talloc_strdup(frame, wg_ptr);
			server = talloc_strdup(frame, cli->desthost);

			cli_shutdown(cli);

			if (!workgroup || !server) {
				errno = ENOMEM;
				TALLOC_FREE(frame);
				return NULL;
			}

			DEBUG(4, ("using workgroup %s %s\n", workgroup, server));

			/*
			 * For each returned master browser IP address, get a
			 * connection to IPC$ on the server if we do not
			 * already have one, and determine the
			 * workgroups/domains that it knows about.
			 */

			srv = SMBC_server(frame, context, True, server, "IPC$",
							  &workgroup, &user, &password);
			if (!srv) {
					continue;
			}

			dir->srv = srv;
			dir->dir_type = SMBC_WORKGROUP;

			/* Now, list the stuff ... */

			if (!cli_NetServerEnum(srv->cli,
							   workgroup,
							   SV_TYPE_DOMAIN_ENUM,
							   smbc_list_unique_wg_fn,
							   (void *)dir)) 
			{
				continue;
			}
		}

		SAFE_FREE(ip_list);
	}
	else {
		/*
		 * Server not an empty string ... Check the rest and see what gives
		 */
		//query string: smb://host/
		if (*share == '\0') {
			if (*path != '\0') {
				/* Should not have empty share with path */
				errno = EINVAL + 8197;
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			/*
			 * We don't know if <server> is really a server name
			 * or is a workgroup/domain name.  If we already have
			 * a server structure for it, we'll use it.
			 * Otherwise, check to see if <server><1D>,
			 * <server><1B>, or <server><20> translates.  We check
			 * to see if <server> is an IP address first.
			 */

			/*
			 * See if we have an existing server.  Do not
			 * establish a connection if one does not already
			 * exist.
			 */
			srv = SMBC_server(frame, context, False,
							  server, "IPC$",
							  &workgroup, &user, &password);

			/*
			 * If no existing server and not an IP addr, look for
			 * LMB or DMB
			 */
			if (!srv && !is_ipaddress(server) &&
				(resolve_name(server, &rem_ss, 0x1d, false) ||   /* LMB */
				 resolve_name(server, &rem_ss, 0x1b, false) )) /* DMB */
			{ 
				/*
				 * "server" is actually a workgroup name,
				 * not a server. Make this clear.
				 */
				char *wgroup = server;
				fstring buserver;

				dir->dir_type = SMBC_SERVER;

				/*
				 * Get the backup list ...
				 */
				if (!name_status_find(wgroup, 0, 0, &rem_ss, buserver)) {
					char addr[INET6_ADDRSTRLEN];
					print_sockaddr(addr, sizeof(addr), &rem_ss);
					DEBUG(0,("Could not get name of "
							"local/domain master browser "
							"for workgroup %s from "
							"address %s\n",
							wgroup,
							addr));
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					errno = EPERM;
					TALLOC_FREE(frame);
					return NULL;
				}

				/*
				 * Get a connection to IPC$ on the server if
				 * we do not already have one
				 */
				srv = SMBC_server(frame, context, True,
								  buserver, "IPC$",
								  &workgroup,
								  &user, &password);
				if (!srv) {
					DEBUG(0, ("got no contact to IPC$\n"));
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;

				}

				dir->srv = srv;

				/* Now, list the servers ... */
				if (!cli_NetServerEnum(srv->cli, wgroup,
							   0x0000FFFE, smbc_list_fn,
							   (void *)dir)) 
				{
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;
				}
			} else if (srv ||  (resolve_name(server, &rem_ss, 0x20, false))) {
				/*
				 * If we hadn't found the server, get one now
				 */
				if (!srv) {
					srv = SMBC_server(frame, context, True,
									  server, "IPC$",
									  &workgroup,
									  &user, &password);
				}

				if (!srv) {
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;
				}

				dir->dir_type = SMBC_FILE_SHARE;
				dir->srv = srv;

				/* List the shares ... */

				if (net_share_enum_rpc(srv->cli, smbc_list_fn, (void *) dir) < 0 &&
					cli_RNetShareEnum(srv->cli, smbc_list_fn, (void *)dir) < 0) 
				{
					errno = cli_errno(srv->cli);
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;
				}
			} else {
				/* Neither the workgroup nor server exists */
				errno = ECONNREFUSED;
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}
		}

		//query string: smb://host/share/
		else {
			/*
			 * The server and share are specified ... work from
			 * there ...
			 */
			char *targetpath;
			struct cli_state *targetcli;

			/* We connect to the server and list the directory */
			dir->dir_type = SMBC_FILE_SHARE;

			srv = SMBC_server(frame, context, True, server, share,
							  &workgroup, &user, &password);

			if (!srv) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			dir->srv = srv;

			/* Now, list the files ... */
			p = path + strlen(path);
			path = talloc_asprintf_append(path, "\\*");
			if (!path) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			if (!cli_resolve_path(frame, "", context->internal->auth_info,
						srv->cli, path,
						&targetcli, &targetpath)) 
			{
				d_printf("Could not resolve %s\n", path);
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			if (cli_list(targetcli, targetpath,
					 aDIR | aSYSTEM | aHIDDEN,
					 smbc_dir_list_fn, (void *)dir) < 0) {

				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				saved_errno = SMBC_errno(context, targetcli);

				if (saved_errno == EINVAL) {
					/*
					 * See if they asked to opendir
					 * something other than a directory.
					 * If so, the converted error value we
					 * got would have been EINVAL rather
					 * than ENOTDIR.
					 */
					*p = '\0'; /* restore original path */

					if (SMBC_getatr(context, srv, path,
									&mode, NULL,
									NULL, NULL, NULL, NULL,
									NULL) &&
						! IS_DOS_DIR(mode)) 
					{
						/* It is.  Correct the error value */
						saved_errno = ENOTDIR;
					}
				}

				/*
				 * If there was an error and the server is no
				 * good any more...
				 */
				if (cli_is_error(targetcli) && smbc_getFunctionCheckServer(context)(context, srv)) {
					/* ... then remove it. */
					if (smbc_getFunctionRemoveUnusedServer(context)(context, srv)) {
						/*
						 * We could not remove the
						 * server completely, remove
						 * it from the cache so we
						 * will not get it again. It
						 * will be removed when the
						 * last file/dir is closed.
						 */
						smbc_getFunctionRemoveCachedServer(context)(context, srv);
					}
				}
				errno = saved_errno;
				TALLOC_FREE(frame);
				return NULL;
			}
		}

	}

	DLIST_ADD(context->internal->files, dir);
	TALLOC_FREE(frame);

	return dir;
}

SMBCFILE *smbc_cli_open_share(struct cli_state *cli, const char *fname)
{
	SMBCCTX *context = statcont;
	int saved_errno;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *options = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	uint16 mode;
	char *p = NULL;
	SMBCSRV *srv  = NULL;
	SMBCFILE *dir = NULL;
	struct sockaddr_storage rem_ss;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		DEBUG(4, ("no valid context\n"));
		errno = EINVAL + 8192;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!fname) {
		DEBUG(4, ("no valid fname\n"));
		errno = EINVAL + 8193;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						&options)) {
		DEBUG(4, ("no valid path\n"));
		errno = EINVAL + 8194;
		TALLOC_FREE(frame);
		return NULL;
	}

	DEBUG(4, ("parsed path: fname='%s' server='%s' share='%s' "
				  "path='%s' options='%s'\n",
				  fname, server, share, path, options));

	/* Ensure the options are valid */
	if (SMBC_check_options(server, share, path, options)) {
		DEBUG(4, ("unacceptable options (%s)\n", options));
		errno = EINVAL + 8195;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	dir = SMB_MALLOC_P(SMBCFILE);

	if (!dir) {
		errno = ENOMEM;
		TALLOC_FREE(frame);
		return NULL;
	}

	ZERO_STRUCTP(dir);

	dir->cli_fd   = 0;
	dir->fname	= SMB_STRDUP(fname);
	dir->srv	  = NULL;
	dir->offset   = 0;
	dir->file	 = False;
	dir->dir_list = dir->dir_next = dir->dir_end = NULL;

	/*
	 * The server and share are specified ... work from
	 * there ...
	 */
	char *targetpath;
	struct cli_state *targetcli;

	/* We connect to the server and list the directory */
	dir->dir_type = SMBC_FILE_SHARE;

	srv = SMB_MALLOC_P(SMBCSRV);
	if (!srv) {
		TALLOC_FREE(frame);
		errno = ENOMEM;
		return NULL;
	}

	ZERO_STRUCTP(srv);
	srv->cli = cli;
	srv->dev = (dev_t)(str_checksum(server) ^ str_checksum(share));
	srv->no_pathinfo = False;
	srv->no_pathinfo2 = False;
	srv->no_nt_session = False;

	dir->srv = srv;

#if 1
	//try to open pipe to machine and bound anonymously
	cli->user_name = "";
	cli->domain = "";	
	
	/* List the shares ... */
	if (net_share_enum_rpc(srv->cli, smbc_list_fn, (void *) dir) < 0 &&
		cli_RNetShareEnum(srv->cli, smbc_list_fn, (void *)dir) < 0) 
	{
		errno = cli_errno(srv->cli);
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		TALLOC_FREE(frame);
		return NULL;
	}
#else
	/* Now, list the files ... */

	p = path + strlen(path);
	path = talloc_asprintf_append(path, "\\*");
	if (!path) {
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!cli_resolve_path(frame, "", context->internal->auth_info, srv->cli, path, &targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		TALLOC_FREE(frame);
		return NULL;
	}
	if (cli_list(targetcli, targetpath, aDIR | aSYSTEM | aHIDDEN, smbc_dir_list_fn, (void *)dir) < 0) {
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		saved_errno = SMBC_errno(context, targetcli);

		if (saved_errno == EINVAL) {
			/*
			 * See if they asked to opendir
			 * something other than a directory.
			 * If so, the converted error value we
			 * got would have been EINVAL rather
			 * than ENOTDIR.
			 */

			*p = '\0'; /* restore original path */
			if (SMBC_getatr(context, srv, path, &mode, NULL, NULL, NULL, NULL, NULL, NULL) &&
					! IS_DOS_DIR(mode)) 
			{

				/* It is.  Correct the error value */
				saved_errno = ENOTDIR;
			}
		}

		/*
		 * If there was an error and the server is no
		 * good any more...
		 */
		if (cli_is_error(targetcli) && smbc_getFunctionCheckServer(context)(context, srv)) {
			/* ... then remove it. */
			if (smbc_getFunctionRemoveUnusedServer(context)(context, srv)) {
				/*
				 * We could not remove the
				 * server completely, remove
				 * it from the cache so we
				 * will not get it again. It
				 * will be removed when the
				 * last file/dir is closed.
				 */
				smbc_getFunctionRemoveCachedServer(context)(context, srv);
			}
		}

		errno = saved_errno;
		TALLOC_FREE(frame);
		return NULL;
	}
#endif
	DLIST_ADD(context->internal->files, dir);
	TALLOC_FREE(frame);

	return dir;
}

SMBCFILE *smbc_cli_opendir(struct cli_state *cli, const char *fname)
{
	SMBCCTX *context = statcont;
	int saved_errno;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *options = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	uint16 mode;
	char *p = NULL;
	SMBCSRV *srv  = NULL;
	SMBCFILE *dir = NULL;
	struct sockaddr_storage rem_ss;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		DEBUG(4, ("no valid context\n"));
		errno = EINVAL + 8192;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!fname) {
		DEBUG(4, ("no valid fname\n"));
		errno = EINVAL + 8193;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						&options)) {
		DEBUG(4, ("no valid path\n"));
		errno = EINVAL + 8194;
		TALLOC_FREE(frame);
		return NULL;
	}

	DEBUG(4, ("parsed path: fname='%s' server='%s' share='%s' "
				  "path='%s' options='%s'\n",
				  fname, server, share, path, options));

	/* Ensure the options are valid */
	if (SMBC_check_options(server, share, path, options)) {
		DEBUG(4, ("unacceptable options (%s)\n", options));
		errno = EINVAL + 8195;
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	dir = SMB_MALLOC_P(SMBCFILE);

	if (!dir) {
		errno = ENOMEM;
		TALLOC_FREE(frame);
		return NULL;
	}

	ZERO_STRUCTP(dir);

	dir->cli_fd   = 0;
	dir->fname	= SMB_STRDUP(fname);
	dir->srv	  = NULL;
	dir->offset   = 0;
	dir->file	 = False;
	dir->dir_list = dir->dir_next = dir->dir_end = NULL;

	/*
	 * The server and share are specified ... work from
	 * there ...
	 */
	char *targetpath;
	struct cli_state *targetcli;

	/* We connect to the server and list the directory */
	dir->dir_type = SMBC_FILE_SHARE;
#if 0
	srv = SMBC_server(frame, context, True, server, share,
								  &workgroup, &user, &password);

	if (!srv) {
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		TALLOC_FREE(frame);
		return NULL;
	}
#else			
	srv = SMB_MALLOC_P(SMBCSRV);
	if (!srv) {
		TALLOC_FREE(frame);
		errno = ENOMEM;
		return NULL;
	}

	ZERO_STRUCTP(srv);
	srv->cli = cli;
	srv->dev = (dev_t)(str_checksum(server) ^ str_checksum(share));
	srv->no_pathinfo = False;
	srv->no_pathinfo2 = False;
	srv->no_nt_session = False;
#endif
	dir->srv = srv;

	/* Now, list the files ... */

	p = path + strlen(path);
	path = talloc_asprintf_append(path, "\\*");
	if (!path) {
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		TALLOC_FREE(frame);
		return NULL;
	}

	if (!cli_resolve_path(frame, "", context->internal->auth_info, srv->cli, path, &targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		TALLOC_FREE(frame);
		return NULL;
	}
	if (cli_list(targetcli, targetpath, aDIR | aSYSTEM | aHIDDEN, smbc_dir_list_fn, (void *)dir) < 0) {
		if (dir) {
			SAFE_FREE(dir->fname);
			SAFE_FREE(dir);
		}
		saved_errno = SMBC_errno(context, targetcli);

		if (saved_errno == EINVAL) {
			/*
			 * See if they asked to opendir
			 * something other than a directory.
			 * If so, the converted error value we
			 * got would have been EINVAL rather
			 * than ENOTDIR.
			 */

			*p = '\0'; /* restore original path */
			if (SMBC_getatr(context, srv, path, &mode, NULL, NULL, NULL, NULL, NULL, NULL) &&
					! IS_DOS_DIR(mode)) 
			{

				/* It is.  Correct the error value */
				saved_errno = ENOTDIR;
			}
		}

		/*
		 * If there was an error and the server is no
		 * good any more...
		 */
		if (cli_is_error(targetcli) && smbc_getFunctionCheckServer(context)(context, srv)) {
			/* ... then remove it. */
			if (smbc_getFunctionRemoveUnusedServer(context)(context, srv)) {
				/*
				 * We could not remove the
				 * server completely, remove
				 * it from the cache so we
				 * will not get it again. It
				 * will be removed when the
				 * last file/dir is closed.
				 */
				smbc_getFunctionRemoveCachedServer(context)(context, srv);
			}
		}

		errno = saved_errno;
		TALLOC_FREE(frame);
		return NULL;
	}

	DLIST_ADD(context->internal->files, dir);
	TALLOC_FREE(frame);

	return dir;
}

static void
smbc_cli_readdir_internal(SMBCCTX * context,
					  struct smbc_dirent *dest,
					  struct smbc_dirent *src,
					  int max_namebuf_len)
{
	if (smbc_getOptionUrlEncodeReaddirEntries(context)) {

		/* url-encode the name.  get back remaining buffer space */
		max_namebuf_len =
			smbc_urlencode(dest->name, src->name, max_namebuf_len);

		/* We now know the name length */
		dest->namelen = strlen(dest->name);

		/* Save the pointer to the beginning of the comment */
		dest->comment = dest->name + dest->namelen + 1;

		/* Copy the comment */
		strncpy(dest->comment, src->comment, max_namebuf_len - 1);
		dest->comment[max_namebuf_len - 1] = '\0';

		/* Save other fields */
		dest->smbc_type = src->smbc_type;
		dest->commentlen = strlen(dest->comment);
		dest->dirlen = ((dest->comment + dest->commentlen + 1) - (char *) dest);
	} else {

		/* No encoding.  Just copy the entry as is. */
		memcpy(dest, src, src->dirlen);
		dest->comment = (char *)(&dest->name + src->namelen + 1);
	}

}

/*
 * Routine to get a directory entry
 */
struct smbc_dirent *smbc_cli_readdir(SMBCFILE *dir)
{
	SMBCCTX *context = statcont;
	int maxlen;
	struct smbc_dirent *dirp, *dirent;
	TALLOC_CTX *frame = talloc_stackframe();

	/* Check that all is ok first ... */

	if (!context || !context->internal->initialized) {

		errno = EINVAL;
		DEBUG(0, ("Invalid context in SMBC_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!dir || !SMBC_dlist_contains(context->internal->files, dir)) {

		errno = EBADF;
		DEBUG(0, ("Invalid dir in SMBC_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		DEBUG(0, ("Found file vs directory in SMBC_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!dir->dir_next) {
		TALLOC_FREE(frame);
		return NULL;
	}

	dirent = dir->dir_next->dirent;
	if (!dirent) {

		errno = ENOENT;
		TALLOC_FREE(frame);
		return NULL;

	}

	dirp = &context->internal->dirent;
	maxlen = sizeof(context->internal->_dirent_name);

	smbc_cli_readdir_internal(context, dirp, dirent, maxlen);

	dir->dir_next = dir->dir_next->next;

	TALLOC_FREE(frame);
	return dirp;
}


/*
 * Routine to close a directory
 */
int
smbc_cli_closedir(SMBCFILE *dir)
{
	SMBCCTX *context = statcont;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;
		return -1;
	}

	if (!dir || !SMBC_dlist_contains(context->internal->files, dir)) {
		errno = EBADF;
		return -1;
	}

	smbc_remove_dir(dir); /* Clean it up */

	DLIST_REMOVE(context->internal->files, dir);

	if (dir) {

		SAFE_FREE(dir->fname);
		SAFE_FREE(dir);	/* Free the space too */
	}

	return 0;

}

uint32 smbc_cli_unlink(struct cli_state *cli, const char *fname, uint16_t mayhave_attrs)
{
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NT_STATUS_V(status);
	}

	if (!fname) {
		errno = EINVAL;
		return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_unlink(%s)\n", fname));
	frame = talloc_stackframe();
		
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_unlink(cli, path, mayhave_attrs);

	TALLOC_FREE(frame);

	return NT_STATUS_V(status);
}

smb_file_t *smbc_cli_ntcreate(struct cli_state *cli, char *fname,
	uint32_t desired_access,
	uint32_t create_disposition,
	uint32_t create_options)
{
	uint16_t fnum = (uint16_t)-1;
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	errno = 0;
	
	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NULL;
		//return NT_STATUS_V(status);
	}

	if (!fname) {
		errno = EINVAL;
		return NULL;
		//return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_ntcreate(%s)\n", fname));
	frame = talloc_stackframe();
		
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NULL;
		//return NT_STATUS_V(status);
	}


//	if (create_options & FILE_DIRECTORY_FILE) {
//		desired_access = FILE_READ_DATA;
//	} else {
//		desired_access = FILE_READ_DATA | FILE_WRITE_DATA;
//	}

//	status = cli_ntcreate(cli, path, 0,
//			FILE_READ_DATA|FILE_WRITE_DATA, 0,
//			FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0x0, 0x0, &fnum);

//	status = cli_ntcreate(cli, path, 
//			0x16, 	//CreateFlags
//			0x6019f, //DesiredAccess
//			0x20, 	//fileAttributes
//			0, 		//shareAccess
//			0x02, 	//CreateDisposition
//			0x44,	//CreateOptions
//			0x0, 	//SecurityFlags
//			&fnum);
	status = cli_ntcreate(cli, path, 
			0,	 								//CreateFlags
			desired_access, 						//DesiredAccess
			0x00, 								//fileAttributes
			FILE_SHARE_READ|FILE_SHARE_WRITE, 	//shareAccess
			create_disposition, 					//CreateDisposition
			create_options,						//CreateOptions
			0x0, 								//SecurityFlags
			&fnum);
	if (!NT_STATUS_IS_OK(status)) {
#if 0
		status = cli_ntcreate(cli, path, 
			0,
			FILE_READ_DATA, 
			0,
			FILE_SHARE_READ|FILE_SHARE_WRITE, 
			FILE_OPEN, 
			0x0, 
			0x0, 
			&fnum);
		if (NT_STATUS_IS_OK(status)) {
			d_printf("open file %s: for read fnum %d\n", path, fnum);
		} else {
			d_printf("Failed to open file %s. %s\n", path, cli_errstr(cli));
			TALLOC_FREE(frame);
			return NULL;
		}
#else
		//if ( NT_STATUS_V(status) == NT_STATUS_V(NT_STATUS_OBJECT_NAME_NOT_FOUND))
			errno = ENOENT;
		//else
		//	errno = NT_STATUS_V(status);
		d_printf("Failed to open file %s. %s\n", path, cli_errstr(cli));
		TALLOC_FREE(frame);
		return NULL;
#endif
	} else {
		d_printf("open file %s: for read/write fnum %d\n", path, fnum);
	}

	smb_file_t *smbf = (smb_file_t *)malloc(sizeof(smb_file_t));
	smbf->fnum = fnum;
	smbf->offset  = 0; 
	smbf->whence = 0;
	int len = strlen(fname);
	smbf->fname = (char *)malloc(len+1);
	memset(smbf->fname, 0, len);
	memcpy(smbf->fname, fname, len);

	TALLOC_FREE(frame);
	return smbf;
	//return NT_STATUS_V(status);
}

uint32 smbc_cli_get(struct cli_state *cli, char *fname,
	NTSTATUS (*sink)(char *buf, size_t n, void *priv),
	void *priv)
{
	int io_bufsize = 524288;
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	uint16_t fnum;
	uint16 attr;
	SMB_OFF_T size;
	off_t start = 0;
	SMB_OFF_T nread = 0;

	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NT_STATUS_V(status);
	}

	if (!fname) {
		errno = EINVAL;
		return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_stat(%s)\n", fname));
	frame = talloc_stackframe();
		
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	uint32_t desired_access = FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES | READ_CONTROL_ACCESS;
	uint32_t file_attribute = FILE_ATTRIBUTE_NORMAL;
	uint32_t create_disposition = FILE_OPEN;
	uint32_t create_options = FILE_NON_DIRECTORY_FILE;
#if 1
	status = cli_ntcreate(cli, path, 
			0x16, 								//CreateFlags
			desired_access, 						//DesiredAccess
			file_attribute,							//fileAttributes
			FILE_SHARE_READ|FILE_SHARE_WRITE, 	//shareAccess
			create_disposition, 					//CreateDisposition
			create_options,						//CreateOptions
			0x0, 								//SecurityFlags
			&fnum);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s opening remote file %s\n", cli_errstr(cli), fname);
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	if (!cli_qfileinfo(cli, fnum, &attr, &size, NULL, NULL, NULL, NULL, NULL) &&
		!NT_STATUS_IS_OK(status = cli_getattrE(cli, fnum, &attr, &size, NULL, NULL, NULL))) 
	{
		d_printf("getattrib: %s\n",cli_errstr(cli));
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_pull(cli, fnum, start, size, io_bufsize, sink, (void *)priv, &nread);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("parallel_read returned %s\n", nt_errstr(status)));
		cli_close(cli, fnum);
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_close(cli, fnum);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Error %s closing remote file\n",cli_errstr(cli)));
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}
#else
	status = cli_open(cli, path, O_RDONLY, DENY_NONE, &fnum);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s opening remote file %s\n", cli_errstr(cli), fname);
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}


	if (!cli_qfileinfo(cli, fnum, &attr, &size, NULL, NULL, NULL, NULL, NULL) &&
		!NT_STATUS_IS_OK(status = cli_getattrE(cli, fnum, &attr, &size, NULL, NULL, NULL))) 
	{
		d_printf("getattrib: %s\n",cli_errstr(cli));
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_pull(cli, fnum, start, size, io_bufsize, sink, (void *)priv, &nread);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("parallel_read returned %s\n", nt_errstr(status)));
		cli_close(cli, fnum);
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_close(cli, fnum);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1,("Error %s closing remote file\n",cli_errstr(cli)));
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}
#endif
	TALLOC_FREE(frame);
	return NT_STATUS_V(status);
}

uint32 smbc_cli_rename(struct cli_state *cli, char *src, char *dst)
{
	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *spath = NULL;
	char *dpath = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NT_STATUS_V(status);
	}

	if (!src || !dst) {
		errno = EINVAL;
		return NT_STATUS_V(status);
	}
		
	DEBUG(4, ("smbc_cli_rename(%s, %s)\n", src, dst));
	frame = talloc_stackframe();	
	
	if (SMBC_parse_path(frame,
						context,
						src,
						&workgroup,
						&server,
						&share,
						&spath,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	if (SMBC_parse_path(frame,
						context,
						dst,
						&workgroup,
						&server,
						&share,
						&dpath,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NT_STATUS_V(status);
	}

	status = cli_rename(cli, spath, dpath);
	
	TALLOC_FREE(frame);
	return NT_STATUS_V(status);
}

smb_file_t *smbc_cli_open(struct cli_state *cli, char *fname, int flags)
{
	SMB_OFF_T start = 0;
	uint16 fnum = -1;
	errno =  0; //init to OK

	NTSTATUS status = NT_STATUS_FILE_INVALID;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
	SMBCCTX *context = statcont;
	TALLOC_CTX *frame;

	if (!context || !context->internal->initialized) {
		errno = EINVAL;  /* Best I can think of ... */
		return NULL;
	}

	if (!fname) {
		errno = EINVAL;
		return NULL;
	}
		
	DEBUG(4, ("smbc_cli_stat(%s)\n", fname));
	frame = talloc_stackframe();
		
	if (SMBC_parse_path(frame,
						context,
						fname,
						&workgroup,
						&server,
						&share,
						&path,
						&user,
						&password,
						NULL)) 
	{
		errno = EINVAL;
		TALLOC_FREE(frame);
		return NULL;
	}

	
	status = cli_open(cli, path, flags, DENY_NONE, &fnum);
	if (NT_STATUS_IS_OK(status)) {
		if (!cli_qfileinfo(cli, fnum, NULL, &start, NULL, NULL, NULL, NULL, NULL) &&
			!NT_STATUS_IS_OK(status = cli_getattrE(cli, fnum, NULL, &start, NULL, NULL, NULL))) 
		{
			errno = NT_STATUS_V(status);
			d_printf("getattrib: %s\n",cli_errstr(cli));
			TALLOC_FREE(frame);
			return NULL;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		if ( NT_STATUS_V(status) == NT_STATUS_V(NT_STATUS_OBJECT_NAME_NOT_FOUND))
			errno = ENOENT;
		else
			errno = NT_STATUS_V(status);
		DEBUG(4, ("%s opening remote file %s\n",cli_errstr(cli),fname));
		TALLOC_FREE(frame);
		return NULL;
	}
	
	smb_file_t *smbf = (smb_file_t *)malloc(sizeof(smb_file_t));
	smbf->fnum = fnum;
	smbf->offset  = 0; 
	smbf->whence = 0;
	int len = strlen(fname);
	smbf->fname = (char *)malloc(len+1);
	memset(smbf->fname, 0, len);
	memcpy(smbf->fname, fname, len);
	
	TALLOC_FREE(frame);
	return smbf;
}

uint32 smbc_cli_lseek(struct cli_state *cli, smb_file_t *smbf, off_t offset, int whence)
{
	SMB_OFF_T size;
			  
	switch (whence) {
	case SEEK_SET:
		smbf->offset = offset;
		break;
				
	case SEEK_CUR:
		smbf->offset += offset;
		break;
				
	case SEEK_END:
		/*d_printf(">>>lseek: resolved path as %s\n", targetpath);*/
				
		if (!cli_qfileinfo(cli, smbf->fnum, NULL, &size, NULL, NULL, NULL, NULL, NULL))
		{
			SMB_OFF_T b_size = size;
			if (!NT_STATUS_IS_OK(cli_getattrE(cli, smbf->fnum, NULL, &b_size, NULL, NULL, NULL))) {
								errno = EINVAL;
								return -1;
						} else
								size = b_size;
		}
		smbf->offset = size + offset;
		break;
				
	default:
		errno = EINVAL;
		break;
				
	}
		
	return smbf->offset;
		
}

size_t smbc_cli_write(struct cli_state *cli, smb_file_t *smbf, uint16_t write_mode, const char *buf, size_t size)
{
	size_t bwr = cli_write(cli, smbf->fnum, write_mode, buf, smbf->offset, size);
	smbf->offset += bwr;
	
	return bwr;
}

size_t smbc_cli_read(struct cli_state *cli, smb_file_t *smbf, char *buf, size_t size)
{
	size_t bwr = cli_read(cli, smbf->fnum, buf, smbf->offset, size);
	smbf->offset += bwr;
	return bwr;
}

uint32 smbc_cli_close(struct cli_state *cli, smb_file_t *smbf)
{
	if(!smbf)
		return 0;
	
	int fnum = smbf->fnum;
	free(smbf->fname);
	free(smbf);
	
	NTSTATUS status = NT_STATUS_OK;
	if (!NT_STATUS_IS_OK(status = cli_close(cli, fnum))) {
		return NT_STATUS_V(status);
	}
	return NT_STATUS_V(status);
}

uint32 smbc_cli_put(struct cli_state *cli, char *rname, int reput,
	 size_t (*push_source)(uint8 *buf, size_t n, void *priv),
	void *priv)
{
	int io_bufsize = 524288;
	NTSTATUS status = NT_STATUS_OK;
	uint16_t fnum;
	XFILE *f;
	SMB_OFF_T start = 0;

	if (reput) {
		status = cli_open(cli, rname, O_RDWR|O_CREAT, DENY_NONE, &fnum);
		if (NT_STATUS_IS_OK(status)) {
			if (!cli_qfileinfo(cli, fnum, NULL, &start, NULL, NULL, NULL, NULL, NULL) &&
				!NT_STATUS_IS_OK(status = cli_getattrE(cli, fnum, NULL, &start, NULL, NULL, NULL))) 
			{
				d_printf("getattrib: %s\n",cli_errstr(cli));
				return NT_STATUS_V(status);
			}
		}
	} else {
		status = cli_open(cli, rname, O_RDWR|O_CREAT|O_TRUNC, DENY_NONE, &fnum);
	}

	if (!NT_STATUS_IS_OK(status)) {
		return NT_STATUS_V(status);
	}

	status = cli_push(cli, fnum, 0, 0, io_bufsize, push_source, priv);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("cli_push returned %s\n", nt_errstr(status));
	}

	if (!NT_STATUS_IS_OK(status = cli_close(cli, fnum))) {
		d_printf("%s closing remote file %s\n",cli_errstr(cli),rname);
		return NT_STATUS_V(status);
	}

	return NT_STATUS_V(status);
}

int nmb_terminated = 0;
void *smbc_cli_nmb_lookup( void *priv )
{
	SMBCFILE *smbf = (SMBCFILE *)priv;
	SMBCCTX *context = statcont;
	char *uri = NULL;
	//struct smbc_dir_list *dir_list;
	SMBCFILE *dir;
	struct smbc_dirent *de;
	SMBCFILE *dir2, *dir3;
	struct smbc_dirent *de2, *de3;
	int res;

	int rnd = 0;

	while(!nmb_terminated) {
		res = asprintf(&uri, "smb://");		
		dir = smbc_cli_opendir2(uri);
		SAFE_FREE(uri);
		/* See if this was a duplicate*/
		fprintf(stderr, "[type]:\t\t[share]\t\t[comment]\n");
		while(NULL != (de = smbc_cli_readdir(dir))) {
			if(nmb_terminated) return;
			res = asprintf(&uri, "smb://%s/", de->name);
			dir2 = smbc_cli_opendir2(uri);
			SAFE_FREE(uri);
			fprintf(stderr, "\t[%d]:\t\t[%s]\t\t[%s]\n", de->smbc_type, de->name, de->comment);

			fprintf(stderr, "\t[type]:\t\t[share]\t\t[comment]\n");
			while(NULL != (de2 = smbc_cli_readdir(dir2))) {
				fprintf(stderr, "\t\t[%d]:\t\t[%s]\t\t[%s]\n", de2->smbc_type, de2->name, de2->comment);
				if(nmb_terminated) return;

				res = asprintf(&uri, "smb://%s/", de2->name);
				dir3 = smbc_cli_opendir2(uri);
				SAFE_FREE(uri);
				while(NULL != (de3 = smbc_cli_readdir(dir3))) {
					fprintf(stderr, "\t\t[%d]:\t\t[%s]\t\t[%s]\n", de3->smbc_type, de3->name, de3->comment);
					if(nmb_terminated) return;
				}
				smbc_cli_closedir(dir3);
			}
			smbc_cli_closedir(dir2);
		}
		smbc_cli_closedir(dir);
		sleep(10);
	}
	
	return;
}

void smbc_cli_nmb_terminate(int term) 
{
	nmb_terminated = term;
}
/*==== end ====*/

