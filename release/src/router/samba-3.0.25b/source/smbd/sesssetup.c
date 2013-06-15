/* 
   Unix SMB/CIFS implementation.
   handle SMBsessionsetup
   Copyright (C) Andrew Tridgell 1998-2001
   Copyright (C) Andrew Bartlett      2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   Copyright (C) Luke Howard          2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

extern struct auth_context *negprot_global_auth_context;
extern BOOL global_encrypted_passwords_negotiated;
extern BOOL global_spnego_negotiated;
extern enum protocol_types Protocol;
extern int max_send;

uint32 global_client_caps = 0;

/*
  on a logon error possibly map the error to success if "map to guest"
  is set approriately
*/
static NTSTATUS do_map_to_guest(NTSTATUS status, auth_serversupplied_info **server_info,
				const char *user, const char *domain)
{
	if (NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)) {
		if ((lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_USER) || 
		    (lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_PASSWORD)) {
			DEBUG(3,("No such user %s [%s] - using guest account\n",
				 user, domain));
			status = make_server_info_guest(server_info);
		}
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_WRONG_PASSWORD)) {
		if (lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_PASSWORD) {
			DEBUG(3,("Registered username %s for guest access\n",user));
			status = make_server_info_guest(server_info);
		}
	}

	return status;
}

/****************************************************************************
 Add the standard 'Samba' signature to the end of the session setup.
****************************************************************************/

static int add_signature(char *outbuf, char *p)
{
	char *start = p;
	fstring lanman;

	fstr_sprintf( lanman, "Samba %s", SAMBA_VERSION_STRING);

	p += srvstr_push(outbuf, p, "Unix", -1, STR_TERMINATE);
	p += srvstr_push(outbuf, p, lanman, -1, STR_TERMINATE);
	p += srvstr_push(outbuf, p, lp_workgroup(), -1, STR_TERMINATE);

	return PTR_DIFF(p, start);
}

/****************************************************************************
 Start the signing engine if needed. Don't fail signing here.
****************************************************************************/

static void sessionsetup_start_signing_engine(const auth_serversupplied_info *server_info, char *inbuf)
{
	if (!server_info->guest && !srv_signing_started()) {
		/* We need to start the signing engine
		 * here but a W2K client sends the old
		 * "BSRSPYL " signature instead of the
		 * correct one. Subsequent packets will
		 * be correct.
		 */
	       	srv_check_sign_mac(inbuf, False);
	}
}

/****************************************************************************
 Send a security blob via a session setup reply.
****************************************************************************/

static BOOL reply_sesssetup_blob(connection_struct *conn, char *outbuf,
				 DATA_BLOB blob, NTSTATUS nt_status)
{
	char *p;

	if (!NT_STATUS_IS_OK(nt_status) && !NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		ERROR_NT(nt_status_squash(nt_status));
	} else {
		set_message(outbuf,4,0,True);

		nt_status = nt_status_squash(nt_status);
		SIVAL(outbuf, smb_rcls, NT_STATUS_V(nt_status));
		SSVAL(outbuf, smb_vwv0, 0xFF); /* no chaining possible */
		SSVAL(outbuf, smb_vwv3, blob.length);
		p = smb_buf(outbuf);

		/* should we cap this? */
		memcpy(p, blob.data, blob.length);
		p += blob.length;

		p += add_signature( outbuf, p );

		set_message_end(outbuf,p);
	}

	show_msg(outbuf);
	return send_smb(smbd_server_fd(),outbuf);
}

/****************************************************************************
 Do a 'guest' logon, getting back the 
****************************************************************************/

static NTSTATUS check_guest_password(auth_serversupplied_info **server_info) 
{
	struct auth_context *auth_context;
	auth_usersupplied_info *user_info = NULL;
	
	NTSTATUS nt_status;
	unsigned char chal[8];

	ZERO_STRUCT(chal);

	DEBUG(3,("Got anonymous request\n"));

	if (!NT_STATUS_IS_OK(nt_status = make_auth_context_fixed(&auth_context, chal))) {
		return nt_status;
	}

	if (!make_user_info_guest(&user_info)) {
		(auth_context->free)(&auth_context);
		return NT_STATUS_NO_MEMORY;
	}
	
	nt_status = auth_context->check_ntlm_password(auth_context, user_info, server_info);
	(auth_context->free)(&auth_context);
	free_user_info(&user_info);
	return nt_status;
}


#ifdef HAVE_KRB5

#if 0
/* Experiment that failed. See "only happens with a KDC" comment below. */
/****************************************************************************
 Cerate a clock skew error blob for a Windows client.
****************************************************************************/

static BOOL make_krb5_skew_error(DATA_BLOB *pblob_out)
{
	krb5_context context = NULL;
	krb5_error_code kerr = 0;
	krb5_data reply;
	krb5_principal host_princ = NULL;
	char *host_princ_s = NULL;
	BOOL ret = False;

	*pblob_out = data_blob(NULL,0);

	initialize_krb5_error_table();
	kerr = krb5_init_context(&context);
	if (kerr) {
		return False;
	}
	/* Create server principal. */
	asprintf(&host_princ_s, "%s$@%s", global_myname(), lp_realm());
	if (!host_princ_s) {
		goto out;
	}
	strlower_m(host_princ_s);

	kerr = smb_krb5_parse_name(context, host_princ_s, &host_princ);
	if (kerr) {
		DEBUG(10,("make_krb5_skew_error: smb_krb5_parse_name failed for name %s: Error %s\n",
			host_princ_s, error_message(kerr) ));
		goto out;
	}
	
	kerr = smb_krb5_mk_error(context, KRB5KRB_AP_ERR_SKEW, host_princ, &reply);
	if (kerr) {
		DEBUG(10,("make_krb5_skew_error: smb_krb5_mk_error failed: Error %s\n",
			error_message(kerr) ));
		goto out;
	}

	*pblob_out = data_blob(reply.data, reply.length);
	kerberos_free_data_contents(context,&reply);
	ret = True;

  out:

	if (host_princ_s) {
		SAFE_FREE(host_princ_s);
	}
	if (host_princ) {
		krb5_free_principal(context, host_princ);
	}
	krb5_free_context(context);
	return ret;
}
#endif

/****************************************************************************
 Reply to a session setup spnego negotiate packet for kerberos.
****************************************************************************/

static int reply_spnego_kerberos(connection_struct *conn, 
				 char *inbuf, char *outbuf,
				 int length, int bufsize,
				 DATA_BLOB *secblob,
				 BOOL *p_invalidate_vuid)
{
	TALLOC_CTX *mem_ctx;
	DATA_BLOB ticket;
	char *client, *p, *domain;
	fstring netbios_domain_name;
	struct passwd *pw;
	fstring user;
	int sess_vuid;
	NTSTATUS ret;
	PAC_DATA *pac_data;
	DATA_BLOB ap_rep, ap_rep_wrapped, response;
	auth_serversupplied_info *server_info = NULL;
	DATA_BLOB session_key = data_blob(NULL, 0);
	uint8 tok_id[2];
	DATA_BLOB nullblob = data_blob(NULL, 0);
	fstring real_username;
	BOOL map_domainuser_to_guest = False;
	BOOL username_was_mapped;
	PAC_LOGON_INFO *logon_info = NULL;

	ZERO_STRUCT(ticket);
	ZERO_STRUCT(pac_data);
	ZERO_STRUCT(ap_rep);
	ZERO_STRUCT(ap_rep_wrapped);
	ZERO_STRUCT(response);

	/* Normally we will always invalidate the intermediate vuid. */
	*p_invalidate_vuid = True;

	mem_ctx = talloc_init("reply_spnego_kerberos");
	if (mem_ctx == NULL) {
		return ERROR_NT(nt_status_squash(NT_STATUS_NO_MEMORY));
	}

	if (!spnego_parse_krb5_wrap(*secblob, &ticket, tok_id)) {
		talloc_destroy(mem_ctx);
		return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
	}

	ret = ads_verify_ticket(mem_ctx, lp_realm(), 0, &ticket, &client, &pac_data, &ap_rep, &session_key);

	data_blob_free(&ticket);

	if (!NT_STATUS_IS_OK(ret)) {
#if 0
		/* Experiment that failed. See "only happens with a KDC" comment below. */

		if (NT_STATUS_EQUAL(ret, NT_STATUS_TIME_DIFFERENCE_AT_DC)) {

			/*
			 * Windows in this case returns NT_STATUS_MORE_PROCESSING_REQUIRED
			 * with a negTokenTarg blob containing an krb5_error struct ASN1 encoded
			 * containing KRB5KRB_AP_ERR_SKEW. The client then fixes its
			 * clock and continues rather than giving an error. JRA.
			 * -- Looks like this only happens with a KDC. JRA.
			 */

			BOOL ok = make_krb5_skew_error(&ap_rep);
			if (!ok) {
				talloc_destroy(mem_ctx);
				return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
			}
			ap_rep_wrapped = spnego_gen_krb5_wrap(ap_rep, TOK_ID_KRB_ERROR);
			response = spnego_gen_auth_response(&ap_rep_wrapped, ret, OID_KERBEROS5_OLD);
			reply_sesssetup_blob(conn, outbuf, response, NT_STATUS_MORE_PROCESSING_REQUIRED);

			/*
			 * In this one case we don't invalidate the intermediate vuid.
			 * as we're expecting the client to re-use it for the next
			 * sessionsetupX packet. JRA.
			 */

			*p_invalidate_vuid = False;

			data_blob_free(&ap_rep);
			data_blob_free(&ap_rep_wrapped);
			data_blob_free(&response);
			talloc_destroy(mem_ctx);
			return -1; /* already replied */
		}
#else
		if (!NT_STATUS_EQUAL(ret, NT_STATUS_TIME_DIFFERENCE_AT_DC)) {
			ret = NT_STATUS_LOGON_FAILURE;
		}
#endif
		DEBUG(1,("Failed to verify incoming ticket with error %s!\n", nt_errstr(ret)));	
		talloc_destroy(mem_ctx);
		return ERROR_NT(nt_status_squash(ret));
	}

	DEBUG(3,("Ticket name is [%s]\n", client));

	p = strchr_m(client, '@');
	if (!p) {
		DEBUG(3,("Doesn't look like a valid principal\n"));
		data_blob_free(&ap_rep);
		data_blob_free(&session_key);
		SAFE_FREE(client);
		talloc_destroy(mem_ctx);
		return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
	}

	*p = 0;

	/* save the PAC data if we have it */

	if (pac_data) {
		logon_info = get_logon_info_from_pac(pac_data);
		if (logon_info) {
			netsamlogon_cache_store( client, &logon_info->info3 );
		}
	}

	if (!strequal(p+1, lp_realm())) {
		DEBUG(3,("Ticket for foreign realm %s@%s\n", client, p+1));
		if (!lp_allow_trusted_domains()) {
			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
			SAFE_FREE(client);
			talloc_destroy(mem_ctx);
			return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
		}
	}

	/* this gives a fully qualified user name (ie. with full realm).
	   that leads to very long usernames, but what else can we do? */

	domain = p+1;

	if (logon_info && logon_info->info3.hdr_logon_dom.uni_str_len) {

		unistr2_to_ascii(netbios_domain_name, &logon_info->info3.uni_logon_dom, -1);
		domain = netbios_domain_name;
		DEBUG(10, ("Mapped to [%s] (using PAC)\n", domain));

	} else {

		/* If we have winbind running, we can (and must) shorten the
		   username by using the short netbios name. Otherwise we will
		   have inconsistent user names. With Kerberos, we get the
		   fully qualified realm, with ntlmssp we get the short
		   name. And even w2k3 does use ntlmssp if you for example
		   connect to an ip address. */

		struct winbindd_request wb_request;
		struct winbindd_response wb_response;
		NSS_STATUS wb_result;

		ZERO_STRUCT(wb_request);
		ZERO_STRUCT(wb_response);

		DEBUG(10, ("Mapping [%s] to short name\n", domain));

		fstrcpy(wb_request.domain_name, domain);

		wb_result = winbindd_request_response(WINBINDD_DOMAIN_INFO,
					     &wb_request, &wb_response);

		if (wb_result == NSS_STATUS_SUCCESS) {

			fstrcpy(netbios_domain_name,
				wb_response.data.domain_info.name);
			domain = netbios_domain_name;

			DEBUG(10, ("Mapped to [%s] (using Winbind)\n", domain));
		} else {
			DEBUG(3, ("Could not find short name -- winbind "
				  "not running?\n"));
		}
	}

	fstr_sprintf(user, "%s%c%s", domain, *lp_winbind_separator(), client);
	
	/* lookup the passwd struct, create a new user if necessary */

	username_was_mapped = map_username( user );

	pw = smb_getpwnam( mem_ctx, user, real_username, True );

	if (pw) {
		/* if a real user check pam account restrictions */
		/* only really perfomed if "obey pam restriction" is true */
		/* do this before an eventual mappign to guest occurs */
		ret = smb_pam_accountcheck(pw->pw_name);
		if (  !NT_STATUS_IS_OK(ret)) {
			DEBUG(1, ("PAM account restriction prevents user login\n"));
			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
			TALLOC_FREE(mem_ctx);
			return ERROR_NT(nt_status_squash(ret));
		}
	}

	if (!pw) {

		/* this was originally the behavior of Samba 2.2, if a user
		   did not have a local uid but has been authenticated, then 
		   map them to a guest account */

		if (lp_map_to_guest() == MAP_TO_GUEST_ON_BAD_UID){ 
			map_domainuser_to_guest = True;
			fstrcpy(user,lp_guestaccount());
			pw = smb_getpwnam( mem_ctx, user, real_username, True );
		} 

		/* extra sanity check that the guest account is valid */

		if ( !pw ) {
			DEBUG(1,("Username %s is invalid on this system\n", user));
			SAFE_FREE(client);
			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
			TALLOC_FREE(mem_ctx);
			return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
		}
	}

	/* setup the string used by %U */
	
	sub_set_smb_name( real_username );
	reload_services(True);

	if ( map_domainuser_to_guest ) {
		make_server_info_guest(&server_info);
	} else if (logon_info) {
		/* pass the unmapped username here since map_username() 
		   will be called again from inside make_server_info_info3() */
		
		ret = make_server_info_info3(mem_ctx, client, domain, 
					     &server_info, &logon_info->info3);
		if ( !NT_STATUS_IS_OK(ret) ) {
			DEBUG(1,("make_server_info_info3 failed: %s!\n",
				 nt_errstr(ret)));
			SAFE_FREE(client);
			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
			TALLOC_FREE(mem_ctx);
			return ERROR_NT(nt_status_squash(ret));
		}

	} else {
		ret = make_server_info_pw(&server_info, real_username, pw);

		if ( !NT_STATUS_IS_OK(ret) ) {
			DEBUG(1,("make_server_info_pw failed: %s!\n",
				 nt_errstr(ret)));
			SAFE_FREE(client);
			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
			TALLOC_FREE(mem_ctx);
			return ERROR_NT(nt_status_squash(ret));
		}

	        /* make_server_info_pw does not set the domain. Without this
		 * we end up with the local netbios name in substitutions for
		 * %D. */

		if (server_info->sam_account != NULL) {
			pdb_set_domain(server_info->sam_account, domain, PDB_SET);
		}
	}

	server_info->was_mapped |= username_was_mapped;
	
	/* we need to build the token for the user. make_server_info_guest()
	   already does this */
	
	if ( !server_info->ptok ) {
		ret = create_local_token( server_info );
		if ( !NT_STATUS_IS_OK(ret) ) {
			SAFE_FREE(client);
			data_blob_free(&ap_rep);
			data_blob_free(&session_key);
			TALLOC_FREE( mem_ctx );
			TALLOC_FREE( server_info );
			return ERROR_NT(nt_status_squash(ret));
		}
	}

	/* register_vuid keeps the server info */
	/* register_vuid takes ownership of session_key, no need to free after this.
 	   A better interface would copy it.... */
	sess_vuid = register_vuid(server_info, session_key, nullblob, client);

	SAFE_FREE(client);

	if (sess_vuid == UID_FIELD_INVALID ) {
		ret = NT_STATUS_LOGON_FAILURE;
	} else {
		/* current_user_info is changed on new vuid */
		reload_services( True );

		set_message(outbuf,4,0,True);
		SSVAL(outbuf, smb_vwv3, 0);
			
		if (server_info->guest) {
			SSVAL(outbuf,smb_vwv2,1);
		}
		
		SSVAL(outbuf, smb_uid, sess_vuid);

		sessionsetup_start_signing_engine(server_info, inbuf);
	}

        /* wrap that up in a nice GSS-API wrapping */
	if (NT_STATUS_IS_OK(ret)) {
		ap_rep_wrapped = spnego_gen_krb5_wrap(ap_rep, TOK_ID_KRB_AP_REP);
	} else {
		ap_rep_wrapped = data_blob(NULL, 0);
	}
	response = spnego_gen_auth_response(&ap_rep_wrapped, ret, OID_KERBEROS5_OLD);
	reply_sesssetup_blob(conn, outbuf, response, ret);

	data_blob_free(&ap_rep);
	data_blob_free(&ap_rep_wrapped);
	data_blob_free(&response);
	TALLOC_FREE(mem_ctx);

	return -1; /* already replied */
}
#endif

/****************************************************************************
 Send a session setup reply, wrapped in SPNEGO.
 Get vuid and check first.
 End the NTLMSSP exchange context if we are OK/complete fail
 This should be split into two functions, one to handle each
 leg of the NTLM auth steps.
***************************************************************************/

static BOOL reply_spnego_ntlmssp(connection_struct *conn, char *inbuf, char *outbuf,
				 uint16 vuid,
				 AUTH_NTLMSSP_STATE **auth_ntlmssp_state,
				 DATA_BLOB *ntlmssp_blob, NTSTATUS nt_status, 
				 BOOL wrap) 
{
	BOOL ret;
	DATA_BLOB response;
	struct auth_serversupplied_info *server_info = NULL;

	if (NT_STATUS_IS_OK(nt_status)) {
		server_info = (*auth_ntlmssp_state)->server_info;
	} else {
		nt_status = do_map_to_guest(nt_status, 
					    &server_info, 
					    (*auth_ntlmssp_state)->ntlmssp_state->user, 
					    (*auth_ntlmssp_state)->ntlmssp_state->domain);
	}

	if (NT_STATUS_IS_OK(nt_status)) {
		int sess_vuid;
		DATA_BLOB nullblob = data_blob(NULL, 0);
		DATA_BLOB session_key = data_blob((*auth_ntlmssp_state)->ntlmssp_state->session_key.data, (*auth_ntlmssp_state)->ntlmssp_state->session_key.length);

		/* register_vuid keeps the server info */
		sess_vuid = register_vuid(server_info, session_key, nullblob, (*auth_ntlmssp_state)->ntlmssp_state->user);
		(*auth_ntlmssp_state)->server_info = NULL;

		if (sess_vuid == UID_FIELD_INVALID ) {
			nt_status = NT_STATUS_LOGON_FAILURE;
		} else {
			
			/* current_user_info is changed on new vuid */
			reload_services( True );

			set_message(outbuf,4,0,True);
			SSVAL(outbuf, smb_vwv3, 0);
			
			if (server_info->guest) {
				SSVAL(outbuf,smb_vwv2,1);
			}
			
			SSVAL(outbuf,smb_uid,sess_vuid);

			sessionsetup_start_signing_engine(server_info, inbuf);
		}
	}

	if (wrap) {
		response = spnego_gen_auth_response(ntlmssp_blob, nt_status, OID_NTLMSSP);
	} else {
		response = *ntlmssp_blob;
	}

	ret = reply_sesssetup_blob(conn, outbuf, response, nt_status);
	if (wrap) {
		data_blob_free(&response);
	}

	/* NT_STATUS_MORE_PROCESSING_REQUIRED from our NTLMSSP code tells us,
	   and the other end, that we are not finished yet. */

	if (!ret || !NT_STATUS_EQUAL(nt_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		/* NB. This is *NOT* an error case. JRA */
		auth_ntlmssp_end(auth_ntlmssp_state);
		/* Kill the intermediate vuid */
		invalidate_vuid(vuid);
	}

	return ret;
}

/****************************************************************************
 Is this a krb5 mechanism ?
****************************************************************************/

static NTSTATUS parse_spnego_mechanisms(DATA_BLOB blob_in, DATA_BLOB *pblob_out, BOOL *p_is_krb5)
{
	char *OIDs[ASN1_MAX_OIDS];
	int i;

	*p_is_krb5 = False;

	/* parse out the OIDs and the first sec blob */
	if (!parse_negTokenTarg(blob_in, OIDs, pblob_out)) {
		return NT_STATUS_LOGON_FAILURE;
	}

	/* only look at the first OID for determining the mechToken --
	   accoirding to RFC2478, we should choose the one we want 
	   and renegotiate, but i smell a client bug here..  
	   
	   Problem observed when connecting to a member (samba box) 
	   of an AD domain as a user in a Samba domain.  Samba member 
	   server sent back krb5/mskrb5/ntlmssp as mechtypes, but the 
	   client (2ksp3) replied with ntlmssp/mskrb5/krb5 and an 
	   NTLMSSP mechtoken.                 --jerry              */

#ifdef HAVE_KRB5	
	if (strcmp(OID_KERBEROS5, OIDs[0]) == 0 ||
	    strcmp(OID_KERBEROS5_OLD, OIDs[0]) == 0) {
		*p_is_krb5 = True;
	}
#endif
		
	for (i=0;OIDs[i];i++) {
		DEBUG(5,("parse_spnego_mechanisms: Got OID %s\n", OIDs[i]));
		free(OIDs[i]);
	}
	return NT_STATUS_OK;
}

/****************************************************************************
 Reply to a session setup spnego negotiate packet.
****************************************************************************/

static int reply_spnego_negotiate(connection_struct *conn, 
				  char *inbuf,
				  char *outbuf,
				  uint16 vuid,
				  int length, int bufsize,
				  DATA_BLOB blob1,
				  AUTH_NTLMSSP_STATE **auth_ntlmssp_state)
{
	DATA_BLOB secblob;
	DATA_BLOB chal;
	BOOL got_kerberos_mechanism = False;
	NTSTATUS status;

	status = parse_spnego_mechanisms(blob1, &secblob, &got_kerberos_mechanism);
	if (!NT_STATUS_IS_OK(status)) {
		/* Kill the intermediate vuid */
		invalidate_vuid(vuid);
		return ERROR_NT(nt_status_squash(status));
	}

	DEBUG(3,("reply_spnego_negotiate: Got secblob of size %lu\n", (unsigned long)secblob.length));

#ifdef HAVE_KRB5
	if ( got_kerberos_mechanism && ((lp_security()==SEC_ADS) || lp_use_kerberos_keytab()) ) {
		BOOL destroy_vuid = True;
		int ret = reply_spnego_kerberos(conn, inbuf, outbuf, 
						length, bufsize, &secblob, &destroy_vuid);
		data_blob_free(&secblob);
		if (destroy_vuid) {
			/* Kill the intermediate vuid */
			invalidate_vuid(vuid);
		}
		return ret;
	}
#endif

	if (*auth_ntlmssp_state) {
		auth_ntlmssp_end(auth_ntlmssp_state);
	}

	status = auth_ntlmssp_start(auth_ntlmssp_state);
	if (!NT_STATUS_IS_OK(status)) {
		/* Kill the intermediate vuid */
		invalidate_vuid(vuid);
		return ERROR_NT(nt_status_squash(status));
	}

	status = auth_ntlmssp_update(*auth_ntlmssp_state, 
					secblob, &chal);

	data_blob_free(&secblob);

	reply_spnego_ntlmssp(conn, inbuf, outbuf, vuid, auth_ntlmssp_state,
			     &chal, status, True);

	data_blob_free(&chal);

	/* already replied */
	return -1;
}

/****************************************************************************
 Reply to a session setup spnego auth packet.
****************************************************************************/

static int reply_spnego_auth(connection_struct *conn, char *inbuf, char *outbuf,
			     uint16 vuid,
			     int length, int bufsize,
			     DATA_BLOB blob1,
			     AUTH_NTLMSSP_STATE **auth_ntlmssp_state)
{
	DATA_BLOB auth = data_blob(NULL,0);
	DATA_BLOB auth_reply = data_blob(NULL,0);
	DATA_BLOB secblob = data_blob(NULL,0);
	NTSTATUS status = NT_STATUS_INVALID_PARAMETER;

	if (!spnego_parse_auth(blob1, &auth)) {
#if 0
		file_save("auth.dat", blob1.data, blob1.length);
#endif
		/* Kill the intermediate vuid */
		invalidate_vuid(vuid);

		return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
	}

	if (auth.data[0] == ASN1_APPLICATION(0)) {
		/* Might be a second negTokenTarg packet */

		BOOL got_krb5_mechanism = False;
		status = parse_spnego_mechanisms(auth, &secblob, &got_krb5_mechanism);
		if (NT_STATUS_IS_OK(status)) {
			DEBUG(3,("reply_spnego_auth: Got secblob of size %lu\n", (unsigned long)secblob.length));
#ifdef HAVE_KRB5
			if ( got_krb5_mechanism && ((lp_security()==SEC_ADS) || lp_use_kerberos_keytab()) ) {
				BOOL destroy_vuid = True;
				int ret = reply_spnego_kerberos(conn, inbuf, outbuf, 
								length, bufsize, &secblob, &destroy_vuid);
				data_blob_free(&secblob);
				data_blob_free(&auth);
				if (destroy_vuid) {
					/* Kill the intermediate vuid */
					invalidate_vuid(vuid);
				}
				return ret;
			}
#endif
		}
	}

	/* If we get here it wasn't a negTokenTarg auth packet. */
	data_blob_free(&secblob);
	
	if (!*auth_ntlmssp_state) {
		/* Kill the intermediate vuid */
		invalidate_vuid(vuid);

		/* auth before negotiatiate? */
		return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
	}
	
	status = auth_ntlmssp_update(*auth_ntlmssp_state, 
					auth, &auth_reply);

	data_blob_free(&auth);

	reply_spnego_ntlmssp(conn, inbuf, outbuf, vuid, 
			     auth_ntlmssp_state,
			     &auth_reply, status, True);
		
	data_blob_free(&auth_reply);

	/* and tell smbd that we have already replied to this packet */
	return -1;
}

/****************************************************************************
 List to store partial SPNEGO auth fragments.
****************************************************************************/

static struct pending_auth_data *pd_list;

/****************************************************************************
 Delete an entry on the list.
****************************************************************************/

static void delete_partial_auth(struct pending_auth_data *pad)
{
	if (!pad) {
		return;
	}
	DLIST_REMOVE(pd_list, pad);
	data_blob_free(&pad->partial_data);
	SAFE_FREE(pad);
}

/****************************************************************************
 Search for a partial SPNEGO auth fragment matching an smbpid.
****************************************************************************/

static struct pending_auth_data *get_pending_auth_data(uint16 smbpid)
{
	struct pending_auth_data *pad;

	for (pad = pd_list; pad; pad = pad->next) {
		if (pad->smbpid == smbpid) {
			break;
		}
	}
	return pad;
}

/****************************************************************************
 Check the size of an SPNEGO blob. If we need more return NT_STATUS_MORE_PROCESSING_REQUIRED,
 else return NT_STATUS_OK. Don't allow the blob to be more than 64k.
****************************************************************************/

static NTSTATUS check_spnego_blob_complete(uint16 smbpid, uint16 vuid, DATA_BLOB *pblob)
{
	struct pending_auth_data *pad = NULL;
	ASN1_DATA data;
	size_t needed_len = 0;

	pad = get_pending_auth_data(smbpid);

	/* Ensure we have some data. */
	if (pblob->length == 0) {
		/* Caller can cope. */
		DEBUG(2,("check_spnego_blob_complete: zero blob length !\n"));
		delete_partial_auth(pad);
		return NT_STATUS_OK;
	}

	/* Were we waiting for more data ? */
	if (pad) {
		DATA_BLOB tmp_blob;
		size_t copy_len = MIN(65536, pblob->length);

		/* Integer wrap paranoia.... */

		if (pad->partial_data.length + copy_len < pad->partial_data.length ||
		    pad->partial_data.length + copy_len < copy_len) {

			DEBUG(2,("check_spnego_blob_complete: integer wrap "
				"pad->partial_data.length = %u, "
				"copy_len = %u\n",
				(unsigned int)pad->partial_data.length,
				(unsigned int)copy_len ));

			delete_partial_auth(pad);
			return NT_STATUS_INVALID_PARAMETER;
		}

		DEBUG(10,("check_spnego_blob_complete: "
			"pad->partial_data.length = %u, "
			"pad->needed_len = %u, "
			"copy_len = %u, "
			"pblob->length = %u,\n",
			(unsigned int)pad->partial_data.length,
			(unsigned int)pad->needed_len,
			(unsigned int)copy_len,
			(unsigned int)pblob->length ));

		tmp_blob = data_blob(NULL,
				pad->partial_data.length + copy_len);

		/* Concatenate the two (up to copy_len) bytes. */
		memcpy(tmp_blob.data,
			pad->partial_data.data,
			pad->partial_data.length);
		memcpy(tmp_blob.data + pad->partial_data.length,
			pblob->data,
			copy_len);

		/* Replace the partial data. */
		data_blob_free(&pad->partial_data);
		pad->partial_data = tmp_blob;
		ZERO_STRUCT(tmp_blob);

		/* Are we done ? */
		if (pblob->length >= pad->needed_len) {
			/* Yes, replace pblob. */
			data_blob_free(pblob);
			*pblob = pad->partial_data;
			ZERO_STRUCT(pad->partial_data);
			delete_partial_auth(pad);
			return NT_STATUS_OK;
		}

		/* Still need more data. */
		pad->needed_len -= copy_len;
		return NT_STATUS_MORE_PROCESSING_REQUIRED;
	}

	if ((pblob->data[0] != ASN1_APPLICATION(0)) &&
	    (pblob->data[0] != ASN1_CONTEXT(1))) {
		/* Not something we can determine the
		 * length of.
		 */
		return NT_STATUS_OK;
	}

	/* This is a new SPNEGO sessionsetup - see if
	 * the data given in this blob is enough.
	 */

	asn1_load(&data, *pblob);
	asn1_start_tag(&data, pblob->data[0]);
	if (data.has_error || data.nesting == NULL) {
		asn1_free(&data);
		/* Let caller catch. */
		return NT_STATUS_OK;
	}

	/* Integer wrap paranoia.... */

	if (data.nesting->taglen + data.nesting->start < data.nesting->taglen ||
	    data.nesting->taglen + data.nesting->start < data.nesting->start) {

		DEBUG(2,("check_spnego_blob_complete: integer wrap "
			"data.nesting->taglen = %u, "
			"data.nesting->start = %u\n",
			(unsigned int)data.nesting->taglen,
			(unsigned int)data.nesting->start ));

		asn1_free(&data);
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Total length of the needed asn1 is the tag length
	 * plus the current offset. */

	needed_len = data.nesting->taglen + data.nesting->start;
	asn1_free(&data);

	DEBUG(10,("check_spnego_blob_complete: needed_len = %u, "
		"pblob->length = %u\n",
		(unsigned int)needed_len,
		(unsigned int)pblob->length ));

	if (needed_len <= pblob->length) {
		/* Nothing to do - blob is complete. */
		return NT_STATUS_OK;
	}

	/* Refuse the blob if it's bigger than 64k. */
	if (needed_len > 65536) {
		DEBUG(2,("check_spnego_blob_complete: needed_len too large (%u)\n",
			(unsigned int)needed_len ));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* We must store this blob until complete. */
	pad = SMB_MALLOC(sizeof(struct pending_auth_data));
	if (!pad) {
		return NT_STATUS_NO_MEMORY;
	}
	pad->needed_len = needed_len - pblob->length;
	pad->partial_data = data_blob(pblob->data, pblob->length);
	if (pad->partial_data.data == NULL) {
		SAFE_FREE(pad);
		return NT_STATUS_NO_MEMORY;
	}
	pad->smbpid = smbpid;
	pad->vuid = vuid;
	DLIST_ADD(pd_list, pad);

	return NT_STATUS_MORE_PROCESSING_REQUIRED;
}

/****************************************************************************
 Reply to a session setup command.
 conn POINTER CAN BE NULL HERE !
****************************************************************************/

static int reply_sesssetup_and_X_spnego(connection_struct *conn, char *inbuf,
					char *outbuf,
					int length,int bufsize)
{
	uint8 *p;
	DATA_BLOB blob1;
	int ret;
	size_t bufrem;
	fstring native_os, native_lanman, primary_domain;
	char *p2;
	uint16 data_blob_len = SVAL(inbuf, smb_vwv7);
	enum remote_arch_types ra_type = get_remote_arch();
	int vuid = SVAL(inbuf,smb_uid);
	user_struct *vuser = NULL;
	NTSTATUS status = NT_STATUS_OK;
	uint16 smbpid = SVAL(inbuf,smb_pid);

	DEBUG(3,("Doing spnego session setup\n"));

	if (global_client_caps == 0) {
		global_client_caps = IVAL(inbuf,smb_vwv10);

		if (!(global_client_caps & CAP_STATUS32)) {
			remove_from_common_flags2(FLAGS2_32_BIT_ERROR_CODES);
		}

	}
		
	p = (uint8 *)smb_buf(inbuf);

	if (data_blob_len == 0) {
		/* an invalid request */
		return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
	}

	bufrem = smb_bufrem(inbuf, p);
	/* pull the spnego blob */
	blob1 = data_blob(p, MIN(bufrem, data_blob_len));

#if 0
	file_save("negotiate.dat", blob1.data, blob1.length);
#endif

	p2 = inbuf + smb_vwv13 + data_blob_len;
	p2 += srvstr_pull_buf(inbuf, native_os, p2, sizeof(native_os), STR_TERMINATE);
	p2 += srvstr_pull_buf(inbuf, native_lanman, p2, sizeof(native_lanman), STR_TERMINATE);
	p2 += srvstr_pull_buf(inbuf, primary_domain, p2, sizeof(primary_domain), STR_TERMINATE);
	DEBUG(3,("NativeOS=[%s] NativeLanMan=[%s] PrimaryDomain=[%s]\n", 
		native_os, native_lanman, primary_domain));

	if ( ra_type == RA_WIN2K ) {
		/* Vista sets neither the OS or lanman strings */

		if ( !strlen(native_os) && !strlen(native_lanman) )
			set_remote_arch(RA_VISTA);
		
		/* Windows 2003 doesn't set the native lanman string, 
		   but does set primary domain which is a bug I think */
			   
		if ( !strlen(native_lanman) ) {
			ra_lanman_string( primary_domain );
		} else {
			ra_lanman_string( native_lanman );
		}
	}
		
	vuser = get_partial_auth_user_struct(vuid);
	if (!vuser) {
		struct pending_auth_data *pad = get_pending_auth_data(smbpid);
		if (pad) {
			DEBUG(10,("reply_sesssetup_and_X_spnego: found pending vuid %u\n",
				(unsigned int)pad->vuid ));
			vuid = pad->vuid;
			vuser = get_partial_auth_user_struct(vuid);
		}
	}

	if (!vuser) {
		vuid = register_vuid(NULL, data_blob(NULL, 0), data_blob(NULL, 0), NULL);
		if (vuid == UID_FIELD_INVALID ) {
			data_blob_free(&blob1);
			return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
		}
	
		vuser = get_partial_auth_user_struct(vuid);
	}

	if (!vuser) {
		data_blob_free(&blob1);
		return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
	}
	
	SSVAL(outbuf,smb_uid,vuid);

	/* Large (greater than 4k) SPNEGO blobs are split into multiple
	 * sessionsetup requests as the Windows limit on the security blob
	 * field is 4k. Bug #4400. JRA.
	 */

	status = check_spnego_blob_complete(smbpid, vuid, &blob1);
	if (!NT_STATUS_IS_OK(status)) {
		if (!NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
			/* Real error - kill the intermediate vuid */
			invalidate_vuid(vuid);
		}
		data_blob_free(&blob1);
		return ERROR_NT(nt_status_squash(status));
	}

	if (blob1.data[0] == ASN1_APPLICATION(0)) {
		/* its a negTokenTarg packet */
		ret = reply_spnego_negotiate(conn, inbuf, outbuf, vuid, length, bufsize, blob1,
					     &vuser->auth_ntlmssp_state);
		data_blob_free(&blob1);
		return ret;
	}

	if (blob1.data[0] == ASN1_CONTEXT(1)) {
		/* its a auth packet */
		ret = reply_spnego_auth(conn, inbuf, outbuf, vuid, length, bufsize, blob1,
					&vuser->auth_ntlmssp_state);
		data_blob_free(&blob1);
		return ret;
	}

	if (strncmp((char *)(blob1.data), "NTLMSSP", 7) == 0) {
		DATA_BLOB chal;
		if (!vuser->auth_ntlmssp_state) {
			status = auth_ntlmssp_start(&vuser->auth_ntlmssp_state);
			if (!NT_STATUS_IS_OK(status)) {
				/* Kill the intermediate vuid */
				invalidate_vuid(vuid);
				data_blob_free(&blob1);
				return ERROR_NT(nt_status_squash(status));
			}
		}

		status = auth_ntlmssp_update(vuser->auth_ntlmssp_state,
						blob1, &chal);
		
		data_blob_free(&blob1);
		
		reply_spnego_ntlmssp(conn, inbuf, outbuf, vuid, 
					   &vuser->auth_ntlmssp_state,
					   &chal, status, False);
		data_blob_free(&chal);
		return -1;
	}

	/* what sort of packet is this? */
	DEBUG(1,("Unknown packet in reply_sesssetup_and_X_spnego\n"));

	data_blob_free(&blob1);

	return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
}

/****************************************************************************
 On new VC == 0, shutdown *all* old connections and users.
 It seems that only NT4.x does this. At W2K and above (XP etc.).
 a new session setup with VC==0 is ignored.
****************************************************************************/

static int shutdown_other_smbds(TDB_CONTEXT *tdb, TDB_DATA kbuf, TDB_DATA dbuf,
				void *p)
{
	struct sessionid *sessionid = (struct sessionid *)dbuf.dptr;
	const char *ip = (const char *)p;

	if (!process_exists(pid_to_procid(sessionid->pid))) {
		return 0;
	}

	if (sessionid->pid == sys_getpid()) {
		return 0;
	}

	if (strcmp(ip, sessionid->ip_addr) != 0) {
		return 0;
	}

	message_send_pid(pid_to_procid(sessionid->pid), MSG_SHUTDOWN,
			 NULL, 0, True);
	return 0;
}

static void setup_new_vc_session(void)
{
	DEBUG(2,("setup_new_vc_session: New VC == 0, if NT4.x compatible we would close all old resources.\n"));
#if 0
	conn_close_all();
	invalidate_all_vuids();
#endif
	if (lp_reset_on_zero_vc()) {
		session_traverse(shutdown_other_smbds, client_addr());
	}
}

/****************************************************************************
 Reply to a session setup command.
****************************************************************************/

int reply_sesssetup_and_X(connection_struct *conn, char *inbuf,char *outbuf,
			  int length,int bufsize)
{
	int sess_vuid;
	int   smb_bufsize;    
	DATA_BLOB lm_resp;
	DATA_BLOB nt_resp;
	DATA_BLOB plaintext_password;
	fstring user;
	fstring sub_user; /* Sainitised username for substituion */
	fstring domain;
	fstring native_os;
	fstring native_lanman;
	fstring primary_domain;
	static BOOL done_sesssetup = False;
	auth_usersupplied_info *user_info = NULL;
	auth_serversupplied_info *server_info = NULL;

	NTSTATUS nt_status;

	BOOL doencrypt = global_encrypted_passwords_negotiated;

	DATA_BLOB session_key;
	
	START_PROFILE(SMBsesssetupX);

	ZERO_STRUCT(lm_resp);
	ZERO_STRUCT(nt_resp);
	ZERO_STRUCT(plaintext_password);

	DEBUG(3,("wct=%d flg2=0x%x\n", CVAL(inbuf, smb_wct), SVAL(inbuf, smb_flg2)));

	/* a SPNEGO session setup has 12 command words, whereas a normal
	   NT1 session setup has 13. See the cifs spec. */
	if (CVAL(inbuf, smb_wct) == 12 &&
	    (SVAL(inbuf, smb_flg2) & FLAGS2_EXTENDED_SECURITY)) {
		if (!global_spnego_negotiated) {
			DEBUG(0,("reply_sesssetup_and_X:  Rejecting attempt at SPNEGO session setup when it was not negoitiated.\n"));
			return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
		}

		if (SVAL(inbuf,smb_vwv4) == 0) {
			setup_new_vc_session();
		}
		return reply_sesssetup_and_X_spnego(conn, inbuf, outbuf, length, bufsize);
	}

	smb_bufsize = SVAL(inbuf,smb_vwv2);

	if (Protocol < PROTOCOL_NT1) {
		uint16 passlen1 = SVAL(inbuf,smb_vwv7);

		/* Never do NT status codes with protocols before NT1 as we don't get client caps. */
		remove_from_common_flags2(FLAGS2_32_BIT_ERROR_CODES);

		if ((passlen1 > MAX_PASS_LEN) || (passlen1 > smb_bufrem(inbuf, smb_buf(inbuf)))) {
			return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
		}

		if (doencrypt) {
			lm_resp = data_blob(smb_buf(inbuf), passlen1);
		} else {
			plaintext_password = data_blob(smb_buf(inbuf), passlen1+1);
			/* Ensure null termination */
			plaintext_password.data[passlen1] = 0;
		}

		srvstr_pull_buf(inbuf, user, smb_buf(inbuf)+passlen1, sizeof(user), STR_TERMINATE);
		*domain = 0;

	} else {
		uint16 passlen1 = SVAL(inbuf,smb_vwv7);
		uint16 passlen2 = SVAL(inbuf,smb_vwv8);
		enum remote_arch_types ra_type = get_remote_arch();
		char *p = smb_buf(inbuf);    
		char *save_p = smb_buf(inbuf);
		uint16 byte_count;
			

		if(global_client_caps == 0) {
			global_client_caps = IVAL(inbuf,smb_vwv11);
		
			if (!(global_client_caps & CAP_STATUS32)) {
				remove_from_common_flags2(FLAGS2_32_BIT_ERROR_CODES);
			}

			/* client_caps is used as final determination if client is NT or Win95. 
			   This is needed to return the correct error codes in some
			   circumstances.
			*/
		
			if(ra_type == RA_WINNT || ra_type == RA_WIN2K || ra_type == RA_WIN95) {
				if(!(global_client_caps & (CAP_NT_SMBS | CAP_STATUS32))) {
					set_remote_arch( RA_WIN95);
				}
			}
		}

		if (!doencrypt) {
			/* both Win95 and WinNT stuff up the password lengths for
			   non-encrypting systems. Uggh. 
			   
			   if passlen1==24 its a win95 system, and its setting the
			   password length incorrectly. Luckily it still works with the
			   default code because Win95 will null terminate the password
			   anyway 
			   
			   if passlen1>0 and passlen2>0 then maybe its a NT box and its
			   setting passlen2 to some random value which really stuffs
			   things up. we need to fix that one.  */
			
			if (passlen1 > 0 && passlen2 > 0 && passlen2 != 24 && passlen2 != 1)
				passlen2 = 0;
		}
		
		/* check for nasty tricks */
		if (passlen1 > MAX_PASS_LEN || passlen1 > smb_bufrem(inbuf, p)) {
			return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
		}

		if (passlen2 > MAX_PASS_LEN || passlen2 > smb_bufrem(inbuf, p+passlen1)) {
			return ERROR_NT(nt_status_squash(NT_STATUS_INVALID_PARAMETER));
		}

		/* Save the lanman2 password and the NT md4 password. */
		
		if ((doencrypt) && (passlen1 != 0) && (passlen1 != 24)) {
			doencrypt = False;
		}

		if (doencrypt) {
			lm_resp = data_blob(p, passlen1);
			nt_resp = data_blob(p+passlen1, passlen2);
		} else {
			pstring pass;
			BOOL unic=SVAL(inbuf, smb_flg2) & FLAGS2_UNICODE_STRINGS;

#if 0
			/* This was the previous fix. Not sure if it's still valid. JRA. */
			if ((ra_type == RA_WINNT) && (passlen2 == 0) && unic && passlen1) {
				/* NT4.0 stuffs up plaintext unicode password lengths... */
				srvstr_pull(inbuf, pass, smb_buf(inbuf) + 1,
					sizeof(pass), passlen1, STR_TERMINATE);
#endif

			if (unic && (passlen2 == 0) && passlen1) {
				/* Only a ascii plaintext password was sent. */
				srvstr_pull(inbuf, pass, smb_buf(inbuf), sizeof(pass),
					passlen1, STR_TERMINATE|STR_ASCII);
			} else {
				srvstr_pull(inbuf, pass, smb_buf(inbuf), 
					sizeof(pass),  unic ? passlen2 : passlen1, 
					STR_TERMINATE);
			}
			plaintext_password = data_blob(pass, strlen(pass)+1);
		}
		
		p += passlen1 + passlen2;
		p += srvstr_pull_buf(inbuf, user, p, sizeof(user), STR_TERMINATE);
		p += srvstr_pull_buf(inbuf, domain, p, sizeof(domain), STR_TERMINATE);
		p += srvstr_pull_buf(inbuf, native_os, p, sizeof(native_os), STR_TERMINATE);
		p += srvstr_pull_buf(inbuf, native_lanman, p, sizeof(native_lanman), STR_TERMINATE);

		/* not documented or decoded by Ethereal but there is one more string 
		   in the extra bytes which is the same as the PrimaryDomain when using 
		   extended security.  Windows NT 4 and 2003 use this string to store 
		   the native lanman string. Windows 9x does not include a string here 
		   at all so we have to check if we have any extra bytes left */
		
		byte_count = SVAL(inbuf, smb_vwv13);
		if ( PTR_DIFF(p, save_p) < byte_count)
			p += srvstr_pull_buf(inbuf, primary_domain, p, sizeof(primary_domain), STR_TERMINATE);
		else 
			fstrcpy( primary_domain, "null" );

		DEBUG(3,("Domain=[%s]  NativeOS=[%s] NativeLanMan=[%s] PrimaryDomain=[%s]\n",
			 domain, native_os, native_lanman, primary_domain));

		if ( ra_type == RA_WIN2K ) {
			if ( strlen(native_lanman) == 0 )
				ra_lanman_string( primary_domain );
			else
				ra_lanman_string( native_lanman );
		}

	}

	if (SVAL(inbuf,smb_vwv4) == 0) {
		setup_new_vc_session();
	}

	DEBUG(3,("sesssetupX:name=[%s]\\[%s]@[%s]\n", domain, user, get_remote_machine_name()));

	if (*user) {
		if (global_spnego_negotiated) {
			
			/* This has to be here, because this is a perfectly valid behaviour for guest logons :-( */
			
			DEBUG(0,("reply_sesssetup_and_X:  Rejecting attempt at 'normal' session setup after negotiating spnego.\n"));
			return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
		}
		fstrcpy(sub_user, user);
	} else {
		fstrcpy(sub_user, lp_guestaccount());
	}

	sub_set_smb_name(sub_user);

	reload_services(True);
	
	if (lp_security() == SEC_SHARE) {
		/* in share level we should ignore any passwords */

		data_blob_free(&lm_resp);
		data_blob_free(&nt_resp);
		data_blob_clear_free(&plaintext_password);

		map_username(sub_user);
		add_session_user(sub_user);
		add_session_workgroup(domain);
		/* Then force it to null for the benfit of the code below */
		*user = 0;
	}
	
	if (!*user) {

		nt_status = check_guest_password(&server_info);

	} else if (doencrypt) {
		if (!negprot_global_auth_context) {
			DEBUG(0, ("reply_sesssetup_and_X:  Attempted encrypted session setup without negprot denied!\n"));
			return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
		}
		nt_status = make_user_info_for_reply_enc(&user_info, user, domain,
		                                         lm_resp, nt_resp);
		if (NT_STATUS_IS_OK(nt_status)) {
			nt_status = negprot_global_auth_context->check_ntlm_password(negprot_global_auth_context, 
										     user_info, 
										     &server_info);
		}
	} else {
		struct auth_context *plaintext_auth_context = NULL;
		const uint8 *chal;

		nt_status = make_auth_context_subsystem(&plaintext_auth_context);

		if (NT_STATUS_IS_OK(nt_status)) {
			chal = plaintext_auth_context->get_ntlm_challenge(plaintext_auth_context);
			
			if (!make_user_info_for_reply(&user_info, 
						      user, domain, chal,
						      plaintext_password)) {
				nt_status = NT_STATUS_NO_MEMORY;
			}
		
			if (NT_STATUS_IS_OK(nt_status)) {
				nt_status = plaintext_auth_context->check_ntlm_password(plaintext_auth_context, 
											user_info, 
											&server_info); 
				
				(plaintext_auth_context->free)(&plaintext_auth_context);
			}
		}
	}

	free_user_info(&user_info);
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		nt_status = do_map_to_guest(nt_status, &server_info, user, domain);
	}
	
	if (!NT_STATUS_IS_OK(nt_status)) {
		data_blob_free(&nt_resp);
		data_blob_free(&lm_resp);
		data_blob_clear_free(&plaintext_password);
		return ERROR_NT(nt_status_squash(nt_status));
	}

	/* Ensure we can't possible take a code path leading to a null defref. */
	if (!server_info) {
		return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
	}

	nt_status = create_local_token(server_info);
	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(10, ("create_local_token failed: %s\n",
			   nt_errstr(nt_status)));
		data_blob_free(&nt_resp);
		data_blob_free(&lm_resp);
		data_blob_clear_free(&plaintext_password);
		return ERROR_NT(nt_status_squash(nt_status));
	}

	if (server_info->user_session_key.data) {
		session_key = data_blob(server_info->user_session_key.data, server_info->user_session_key.length);
	} else {
		session_key = data_blob(NULL, 0);
	}

	data_blob_clear_free(&plaintext_password);
	
	/* it's ok - setup a reply */
	set_message(outbuf,3,0,True);
	if (Protocol >= PROTOCOL_NT1) {
		char *p = smb_buf( outbuf );
		p += add_signature( outbuf, p );
		set_message_end( outbuf, p );
		/* perhaps grab OS version here?? */
	}
	
	if (server_info->guest) {
		SSVAL(outbuf,smb_vwv2,1);
	}

	/* register the name and uid as being validated, so further connections
	   to a uid can get through without a password, on the same VC */

	if (lp_security() == SEC_SHARE) {
		sess_vuid = UID_FIELD_INVALID;
		data_blob_free(&session_key);
		TALLOC_FREE(server_info);
	} else {
		/* register_vuid keeps the server info */
		sess_vuid = register_vuid(server_info, session_key,
					  nt_resp.data ? nt_resp : lm_resp,
					  sub_user);
		if (sess_vuid == UID_FIELD_INVALID) {
			data_blob_free(&nt_resp);
			data_blob_free(&lm_resp);
			return ERROR_NT(nt_status_squash(NT_STATUS_LOGON_FAILURE));
		}

		/* current_user_info is changed on new vuid */
		reload_services( True );

		sessionsetup_start_signing_engine(server_info, inbuf);
	}

	data_blob_free(&nt_resp);
	data_blob_free(&lm_resp);
	
	SSVAL(outbuf,smb_uid,sess_vuid);
	SSVAL(inbuf,smb_uid,sess_vuid);
	
	if (!done_sesssetup)
		max_send = MIN(max_send,smb_bufsize);
	
	done_sesssetup = True;
	
	END_PROFILE(SMBsesssetupX);
	return chain_reply(inbuf,outbuf,length,bufsize);
}
