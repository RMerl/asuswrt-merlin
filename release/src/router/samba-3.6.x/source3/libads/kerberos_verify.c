/*
   Unix SMB/CIFS implementation.
   kerberos utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Remus Koos 2001
   Copyright (C) Luke Howard 2003
   Copyright (C) Guenther Deschner 2003, 2005
   Copyright (C) Jim McDonough (jmcd@us.ibm.com) 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   Copyright (C) Jeremy Allison 2007

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
#include "smb_krb5.h"
#include "libads/kerberos_proto.h"
#include "secrets.h"
#include "../librpc/gen_ndr/krb5pac.h"

#ifdef HAVE_KRB5

#if !defined(HAVE_KRB5_PRINC_COMPONENT)
const krb5_data *krb5_princ_component(krb5_context, krb5_principal, int );
#endif

static bool ads_dedicated_keytab_verify_ticket(krb5_context context,
					  krb5_auth_context auth_context,
					  const DATA_BLOB *ticket,
					  krb5_ticket **pp_tkt,
					  krb5_keyblock **keyblock,
					  krb5_error_code *perr)
{
	krb5_error_code ret = 0;
	bool auth_ok = false;
	krb5_keytab keytab = NULL;
	krb5_keytab_entry kt_entry;
	krb5_ticket *dec_ticket = NULL;

	krb5_data packet;
	krb5_kvno kvno = 0;
	krb5_enctype enctype;

	*pp_tkt = NULL;
	*keyblock = NULL;
	*perr = 0;

	ZERO_STRUCT(kt_entry);

	ret = smb_krb5_open_keytab(context, lp_dedicated_keytab_file(), true,
	    &keytab);
	if (ret) {
		DEBUG(1, ("smb_krb5_open_keytab failed (%s)\n",
			error_message(ret)));
		goto out;
	}

	packet.length = ticket->length;
	packet.data = (char *)ticket->data;

	ret = krb5_rd_req(context, &auth_context, &packet, NULL, keytab,
	    NULL, &dec_ticket);
	if (ret) {
		DEBUG(0, ("krb5_rd_req failed (%s)\n", error_message(ret)));
		goto out;
	}

#ifdef HAVE_ETYPE_IN_ENCRYPTEDDATA /* Heimdal */
	enctype = dec_ticket->ticket.key.keytype;
#else /* MIT */
	enctype = dec_ticket->enc_part.enctype;
	kvno    = dec_ticket->enc_part.kvno;
#endif

	/* Get the key for checking the pac signature */
	ret = krb5_kt_get_entry(context, keytab, dec_ticket->server,
				kvno, enctype, &kt_entry);
	if (ret) {
		DEBUG(0, ("krb5_kt_get_entry failed (%s)\n",
			  error_message(ret)));
		goto out;
	}

	ret = krb5_copy_keyblock(context, KRB5_KT_KEY(&kt_entry), keyblock);
	smb_krb5_kt_free_entry(context, &kt_entry);

	if (ret) {
		DEBUG(0, ("failed to copy key: %s\n",
			  error_message(ret)));
		goto out;
	}

	auth_ok = true;
	*pp_tkt = dec_ticket;
	dec_ticket = NULL;

  out:
	if (dec_ticket)
		krb5_free_ticket(context, dec_ticket);

	if (keytab)
		krb5_kt_close(context, keytab);

	*perr = ret;
	return auth_ok;
}

/******************************************************************************
 Try to verify a ticket using the system keytab... the system keytab has
 kvno -1 entries, so it's more like what microsoft does... see comment in
 utils/net_ads.c in the ads_keytab_add_entry function for details.
******************************************************************************/

static bool ads_keytab_verify_ticket(krb5_context context,
					krb5_auth_context auth_context,
					const DATA_BLOB *ticket,
					krb5_ticket **pp_tkt,
					krb5_keyblock **keyblock,
					krb5_error_code *perr)
{
	krb5_error_code ret = 0;
	bool auth_ok = False;
	krb5_keytab keytab = NULL;
	krb5_kt_cursor kt_cursor;
	krb5_keytab_entry kt_entry;
	char *valid_princ_formats[7] = { NULL, NULL, NULL,
					 NULL, NULL, NULL, NULL };
	char *entry_princ_s = NULL;
	fstring my_name, my_fqdn;
	int i;
	int number_matched_principals = 0;
	krb5_data packet;
	int err;

	*pp_tkt = NULL;
	*keyblock = NULL;
	*perr = 0;

	/* Generate the list of principal names which we expect
	 * clients might want to use for authenticating to the file
	 * service.  We allow name$,{host,cifs}/{name,fqdn,name.REALM}. */

	fstrcpy(my_name, global_myname());

	my_fqdn[0] = '\0';
	name_to_fqdn(my_fqdn, global_myname());

	err = asprintf(&valid_princ_formats[0],
			"%s$@%s", my_name, lp_realm());
	if (err == -1) {
		goto out;
	}
	err = asprintf(&valid_princ_formats[1],
			"host/%s@%s", my_name, lp_realm());
	if (err == -1) {
		goto out;
	}
	err = asprintf(&valid_princ_formats[2],
			"host/%s@%s", my_fqdn, lp_realm());
	if (err == -1) {
		goto out;
	}
	err = asprintf(&valid_princ_formats[3],
			"host/%s.%s@%s", my_name, lp_realm(), lp_realm());
	if (err == -1) {
		goto out;
	}
	err = asprintf(&valid_princ_formats[4],
			"cifs/%s@%s", my_name, lp_realm());
	if (err == -1) {
		goto out;
	}
	err = asprintf(&valid_princ_formats[5],
			"cifs/%s@%s", my_fqdn, lp_realm());
	if (err == -1) {
		goto out;
	}
	err = asprintf(&valid_princ_formats[6],
			"cifs/%s.%s@%s", my_name, lp_realm(), lp_realm());
	if (err == -1) {
		goto out;
	}

	ZERO_STRUCT(kt_entry);
	ZERO_STRUCT(kt_cursor);

	ret = smb_krb5_open_keytab(context, NULL, False, &keytab);
	if (ret) {
		DEBUG(1, (__location__ ": smb_krb5_open_keytab failed (%s)\n",
			  error_message(ret)));
		goto out;
	}

	/* Iterate through the keytab.  For each key, if the principal
	 * name case-insensitively matches one of the allowed formats,
	 * try verifying the ticket using that principal. */

	ret = krb5_kt_start_seq_get(context, keytab, &kt_cursor);
	if (ret) {
		DEBUG(1, (__location__ ": krb5_kt_start_seq_get failed (%s)\n",
			  error_message(ret)));
		goto out;
	}
  
	while (!auth_ok &&
	       (krb5_kt_next_entry(context, keytab,
				   &kt_entry, &kt_cursor) == 0)) {
		ret = smb_krb5_unparse_name(talloc_tos(), context,
					    kt_entry.principal,
					    &entry_princ_s);
		if (ret) {
			DEBUG(1, (__location__ ": smb_krb5_unparse_name "
				  "failed (%s)\n", error_message(ret)));
			goto out;
		}

		for (i = 0; i < ARRAY_SIZE(valid_princ_formats); i++) {

			if (!strequal(entry_princ_s, valid_princ_formats[i])) {
				continue;
			}

			number_matched_principals++;
			packet.length = ticket->length;
			packet.data = (char *)ticket->data;
			*pp_tkt = NULL;

			ret = krb5_rd_req_return_keyblock_from_keytab(context,
						&auth_context, &packet,
						kt_entry.principal, keytab,
						NULL, pp_tkt, keyblock);

			if (ret) {
				DEBUG(10, (__location__ ": krb5_rd_req_return"
					   "_keyblock_from_keytab(%s) "
					   "failed: %s\n", entry_princ_s,
					   error_message(ret)));

				/* workaround for MIT:
				* as krb5_ktfile_get_entry will explicitly
				* close the krb5_keytab as soon as krb5_rd_req
				* has successfully decrypted the ticket but the
				* ticket is not valid yet (due to clockskew)
				* there is no point in querying more keytab
				* entries - Guenther */

				if (ret == KRB5KRB_AP_ERR_TKT_NYV ||
				    ret == KRB5KRB_AP_ERR_TKT_EXPIRED ||
				    ret == KRB5KRB_AP_ERR_SKEW) {
					break;
				}
			} else {
				DEBUG(3, (__location__ ": krb5_rd_req_return"
					  "_keyblock_from_keytab succeeded "
					  "for principal %s\n",
					  entry_princ_s));
				auth_ok = True;
				break;
			}
		}

		/* Free the name we parsed. */
		TALLOC_FREE(entry_princ_s);

		/* Free the entry we just read. */
		smb_krb5_kt_free_entry(context, &kt_entry);
		ZERO_STRUCT(kt_entry);
	}
	krb5_kt_end_seq_get(context, keytab, &kt_cursor);

	ZERO_STRUCT(kt_cursor);

out:

	for (i = 0; i < ARRAY_SIZE(valid_princ_formats); i++) {
		SAFE_FREE(valid_princ_formats[i]);
	}

	if (!auth_ok) {
		if (!number_matched_principals) {
			DEBUG(3, (__location__ ": no keytab principals "
				  "matched expected file service name.\n"));
		} else {
			DEBUG(3, (__location__ ": krb5_rd_req failed for "
				  "all %d matched keytab principals\n",
				  number_matched_principals));
		}
	}

	TALLOC_FREE(entry_princ_s);

	{
		krb5_keytab_entry zero_kt_entry;
		ZERO_STRUCT(zero_kt_entry);
		if (memcmp(&zero_kt_entry, &kt_entry,
			   sizeof(krb5_keytab_entry))) {
			smb_krb5_kt_free_entry(context, &kt_entry);
		}
	}

	{
		krb5_kt_cursor zero_csr;
		ZERO_STRUCT(zero_csr);
		if ((memcmp(&kt_cursor, &zero_csr,
			    sizeof(krb5_kt_cursor)) != 0) && keytab) {
			krb5_kt_end_seq_get(context, keytab, &kt_cursor);
		}
	}

	if (keytab) {
		krb5_kt_close(context, keytab);
	}
	*perr = ret;
	return auth_ok;
}

/*****************************************************************************
 Try to verify a ticket using the secrets.tdb.
******************************************************************************/

static krb5_error_code ads_secrets_verify_ticket(krb5_context context,
						krb5_auth_context auth_context,
						krb5_principal host_princ,
						const DATA_BLOB *ticket,
						krb5_ticket **pp_tkt,
						krb5_keyblock **keyblock,
						krb5_error_code *perr)
{
	krb5_error_code ret = 0;
	bool auth_ok = False;
	bool cont = true;
	char *password_s = NULL;
	/* Let's make some room for 2 password (old and new)*/
	krb5_data passwords[2];
	krb5_enctype enctypes[] = {
#ifdef HAVE_ENCTYPE_AES256_CTS_HMAC_SHA1_96
		ENCTYPE_AES256_CTS_HMAC_SHA1_96,
#endif
#ifdef HAVE_ENCTYPE_AES128_CTS_HMAC_SHA1_96
		ENCTYPE_AES128_CTS_HMAC_SHA1_96,
#endif
		ENCTYPE_ARCFOUR_HMAC,
		ENCTYPE_DES_CBC_CRC,
		ENCTYPE_DES_CBC_MD5,
		ENCTYPE_NULL
	};
	krb5_data packet;
	int i, j;

	*pp_tkt = NULL;
	*keyblock = NULL;
	*perr = 0;

	ZERO_STRUCT(passwords);

	if (!secrets_init()) {
		DEBUG(1,("ads_secrets_verify_ticket: secrets_init failed\n"));
		*perr = KRB5_CONFIG_CANTOPEN;
		return False;
	}

	password_s = secrets_fetch_machine_password(lp_workgroup(),
						    NULL, NULL);
	if (!password_s) {
		DEBUG(1,(__location__ ": failed to fetch machine password\n"));
		*perr = KRB5_LIBOS_CANTREADPWD;
		return False;
	}

	passwords[0].data = password_s;
	passwords[0].length = strlen(password_s);

	password_s = secrets_fetch_prev_machine_password(lp_workgroup());
	if (password_s) {
		DEBUG(10, (__location__ ": found previous password\n"));
		passwords[1].data = password_s;
		passwords[1].length = strlen(password_s);
	}

	/* CIFS doesn't use addresses in tickets. This would break NAT. JRA */

	packet.length = ticket->length;
	packet.data = (char *)ticket->data;

	/* We need to setup a auth context with each possible encoding type
	 * in turn. */
	for (j=0; j<2 && passwords[j].length; j++) {

		for (i=0;enctypes[i];i++) {
			krb5_keyblock *key = NULL;

			if (!(key = SMB_MALLOC_P(krb5_keyblock))) {
				ret = ENOMEM;
				goto out;
			}

			if (create_kerberos_key_from_string(context,
						host_princ, &passwords[j],
						key, enctypes[i], false)) {
				SAFE_FREE(key);
				continue;
			}

			krb5_auth_con_setuseruserkey(context,
							auth_context, key);

			if (!(ret = krb5_rd_req(context, &auth_context,
						&packet, NULL, NULL,
						NULL, pp_tkt))) {
				DEBUG(10, (__location__ ": enc type [%u] "
					   "decrypted message !\n",
					   (unsigned int)enctypes[i]));
				auth_ok = True;
				cont = false;
				krb5_copy_keyblock(context, key, keyblock);
				krb5_free_keyblock(context, key);
				break;
			}

			DEBUG((ret != KRB5_BAD_ENCTYPE) ? 3 : 10,
				(__location__ ": enc type [%u] failed to "
				 "decrypt with error %s\n",
				 (unsigned int)enctypes[i],
				 error_message(ret)));

			/* successfully decrypted but ticket is just not
			 * valid at the moment */
			if (ret == KRB5KRB_AP_ERR_TKT_NYV ||
			    ret == KRB5KRB_AP_ERR_TKT_EXPIRED ||
			    ret == KRB5KRB_AP_ERR_SKEW) {
				krb5_free_keyblock(context, key);
				cont = false;
				break;
			}

			krb5_free_keyblock(context, key);
		}
		if (!cont) {
			/* If we found a valid pass then no need to try
			 * the next one or we have invalid ticket so no need
			 * to try next password*/
			break;
		}
	}

 out:
	SAFE_FREE(passwords[0].data);
	SAFE_FREE(passwords[1].data);
	*perr = ret;
	return auth_ok;
}

/*****************************************************************************
 Verify an incoming ticket and parse out the principal name and
 authorization_data if available.
******************************************************************************/

NTSTATUS ads_verify_ticket(TALLOC_CTX *mem_ctx,
			   const char *realm,
			   time_t time_offset,
			   const DATA_BLOB *ticket,
			   char **principal,
			   struct PAC_LOGON_INFO **logon_info,
			   DATA_BLOB *ap_rep,
			   DATA_BLOB *session_key,
			   bool use_replay_cache)
{
	NTSTATUS sret = NT_STATUS_LOGON_FAILURE;
	NTSTATUS pac_ret;
	DATA_BLOB auth_data;
	krb5_context context = NULL;
	krb5_auth_context auth_context = NULL;
	krb5_data packet;
	krb5_ticket *tkt = NULL;
	krb5_rcache rcache = NULL;
	krb5_keyblock *keyblock = NULL;
	time_t authtime;
	krb5_error_code ret = 0;
	int flags = 0;
	krb5_principal host_princ = NULL;
	krb5_const_principal client_principal = NULL;
	char *host_princ_s = NULL;
	bool auth_ok = False;
	bool got_auth_data = False;
	struct named_mutex *mutex = NULL;

	ZERO_STRUCT(packet);
	ZERO_STRUCT(auth_data);

	*principal = NULL;
	*logon_info = NULL;
	*ap_rep = data_blob_null;
	*session_key = data_blob_null;

	initialize_krb5_error_table();
	ret = krb5_init_context(&context);
	if (ret) {
		DEBUG(1, (__location__ ": krb5_init_context failed (%s)\n",
			  error_message(ret)));
		return NT_STATUS_LOGON_FAILURE;
	}

	if (time_offset != 0) {
		krb5_set_real_time(context, time(NULL) + time_offset, 0);
	}

	ret = krb5_set_default_realm(context, realm);
	if (ret) {
		DEBUG(1, (__location__ ": krb5_set_default_realm "
			  "failed (%s)\n", error_message(ret)));
		goto out;
	}

	/* This whole process is far more complex than I would
           like. We have to go through all this to allow us to store
           the secret internally, instead of using /etc/krb5.keytab */

	ret = krb5_auth_con_init(context, &auth_context);
	if (ret) {
		DEBUG(1, (__location__ ": krb5_auth_con_init failed (%s)\n",
			  error_message(ret)));
		goto out;
	}

	krb5_auth_con_getflags( context, auth_context, &flags );
	if ( !use_replay_cache ) {
		/* Disable default use of a replay cache */
		flags &= ~KRB5_AUTH_CONTEXT_DO_TIME;
		krb5_auth_con_setflags( context, auth_context, flags );
	}

	if (asprintf(&host_princ_s, "%s$", global_myname()) == -1) {
		goto out;
	}

	strlower_m(host_princ_s);
	ret = smb_krb5_parse_name(context, host_princ_s, &host_princ);
	if (ret) {
		DEBUG(1, (__location__ ": smb_krb5_parse_name(%s) "
			  "failed (%s)\n", host_princ_s, error_message(ret)));
		goto out;
	}


	if (use_replay_cache) {

		/* Lock a mutex surrounding the replay as there is no
		   locking in the MIT krb5 code surrounding the replay
		   cache... */

		mutex = grab_named_mutex(talloc_tos(),
					 "replay cache mutex", 10);
		if (mutex == NULL) {
			DEBUG(1, (__location__ ": unable to protect replay "
				  "cache with mutex.\n"));
			ret = KRB5_CC_IO;
			goto out;
		}

		/* JRA. We must set the rcache here. This will prevent
		   replay attacks. */

		ret = krb5_get_server_rcache(
				context,
				krb5_princ_component(context, host_princ, 0),
				&rcache);
		if (ret) {
			DEBUG(1, (__location__ ": krb5_get_server_rcache "
				  "failed (%s)\n", error_message(ret)));
			goto out;
		}

		ret = krb5_auth_con_setrcache(context, auth_context, rcache);
		if (ret) {
			DEBUG(1, (__location__ ": krb5_auth_con_setrcache "
				  "failed (%s)\n", error_message(ret)));
			goto out;
		}
	}

	switch (lp_kerberos_method()) {
	default:
	case KERBEROS_VERIFY_SECRETS:
		auth_ok = ads_secrets_verify_ticket(context, auth_context,
		    host_princ, ticket, &tkt, &keyblock, &ret);
		break;
	case KERBEROS_VERIFY_SYSTEM_KEYTAB:
		auth_ok = ads_keytab_verify_ticket(context, auth_context,
		    ticket, &tkt, &keyblock, &ret);
		break;
	case KERBEROS_VERIFY_DEDICATED_KEYTAB:
		auth_ok = ads_dedicated_keytab_verify_ticket(context,
		    auth_context, ticket, &tkt, &keyblock, &ret);
		break;
	case KERBEROS_VERIFY_SECRETS_AND_KEYTAB:
		/* First try secrets.tdb and fallback to the krb5.keytab if
		   necessary.  This is the pre 3.4 behavior when
		   "use kerberos keytab" was true.*/
		auth_ok = ads_secrets_verify_ticket(context, auth_context,
		    host_princ, ticket, &tkt, &keyblock, &ret);

		if (!auth_ok) {
			/* Only fallback if we failed to decrypt the ticket */
			if (ret != KRB5KRB_AP_ERR_TKT_NYV &&
			    ret != KRB5KRB_AP_ERR_TKT_EXPIRED &&
			    ret != KRB5KRB_AP_ERR_SKEW) {
				auth_ok = ads_keytab_verify_ticket(context,
				    auth_context, ticket, &tkt, &keyblock,
				    &ret);
			}
		}
		break;
	}

	if (use_replay_cache) {
		TALLOC_FREE(mutex);
#if 0
		/* Heimdal leaks here, if we fix the leak, MIT crashes */
		if (rcache) {
			krb5_rc_close(context, rcache);
		}
#endif
	}

	if (!auth_ok) {
		DEBUG(3, (__location__ ": krb5_rd_req with auth "
			  "failed (%s)\n", error_message(ret)));
		/* Try map the error return in case it's something like
		 * a clock skew error.
		 */
		sret = krb5_to_nt_status(ret);
		if (NT_STATUS_IS_OK(sret) ||
		    NT_STATUS_EQUAL(sret,NT_STATUS_UNSUCCESSFUL)) {
			sret = NT_STATUS_LOGON_FAILURE;
		}
		DEBUG(10, (__location__ ": returning error %s\n",
			   nt_errstr(sret) ));
		goto out;
	}

	authtime = get_authtime_from_tkt(tkt);
	client_principal = get_principal_from_tkt(tkt);

	ret = krb5_mk_rep(context, auth_context, &packet);
	if (ret) {
		DEBUG(3, (__location__ ": Failed to generate mutual "
			  "authentication reply (%s)\n", error_message(ret)));
		goto out;
	}

	*ap_rep = data_blob(packet.data, packet.length);
	if (packet.data) {
		kerberos_free_data_contents(context, &packet);
		ZERO_STRUCT(packet);
	}

	get_krb5_smb_session_key(mem_ctx, context,
				 auth_context, session_key, true);
	dump_data_pw("SMB session key (from ticket)\n",
		     session_key->data, session_key->length);

#if 0
	file_save("/tmp/ticket.dat", ticket->data, ticket->length);
#endif

	/* continue when no PAC is retrieved or we couldn't decode the PAC
	   (like accounts that have the UF_NO_AUTH_DATA_REQUIRED flag set, or
	   Kerberos tickets encrypted using a DES key) - Guenther */

	got_auth_data = get_auth_data_from_tkt(mem_ctx, &auth_data, tkt);
	if (!got_auth_data) {
		DEBUG(3, (__location__ ": did not retrieve auth data. "
			  "continuing without PAC\n"));
	}

	if (got_auth_data) {
		struct PAC_DATA *pac_data;
		pac_ret = decode_pac_data(mem_ctx, &auth_data, context,
					  keyblock, client_principal,
					  authtime, &pac_data);
		data_blob_free(&auth_data);
		if (!NT_STATUS_IS_OK(pac_ret)) {
			DEBUG(3, (__location__ ": failed to decode "
				  "PAC_DATA: %s\n", nt_errstr(pac_ret)));
		} else {
			uint32_t i;
			for (i = 0; i < pac_data->num_buffers; i++) {

				if (pac_data->buffers[i].type != PAC_TYPE_LOGON_INFO) {
					continue;
				}

				*logon_info = pac_data->buffers[i].info->logon_info.info;
			}

			if (!*logon_info) {
				DEBUG(1, ("correctly decoded PAC but found "
					  "no logon_info! "
					  "This should not happen\n"));
				return NT_STATUS_INVALID_USER_BUFFER;
			}
		}
	}

#if 0
#if defined(HAVE_KRB5_TKT_ENC_PART2)
	/* MIT */
	if (tkt->enc_part2) {
		file_save("/tmp/authdata.dat",
			  tkt->enc_part2->authorization_data[0]->contents,
			  tkt->enc_part2->authorization_data[0]->length);
	}
#else
	/* Heimdal */
	if (tkt->ticket.authorization_data) {
		file_save("/tmp/authdata.dat",
			  tkt->ticket.authorization_data->val->ad_data.data,
			  tkt->ticket.authorization_data->val->ad_data.length);
	}
#endif
#endif

	ret = smb_krb5_unparse_name(mem_ctx, context,
				    client_principal, principal);
	if (ret) {
		DEBUG(3, (__location__ ": smb_krb5_unparse_name "
			  "failed (%s)\n", error_message(ret)));
		sret = NT_STATUS_LOGON_FAILURE;
		goto out;
	}

	sret = NT_STATUS_OK;

out:

	TALLOC_FREE(mutex);

	if (!NT_STATUS_IS_OK(sret)) {
		data_blob_free(&auth_data);
	}

	if (!NT_STATUS_IS_OK(sret)) {
		data_blob_free(ap_rep);
	}

	if (host_princ) {
		krb5_free_principal(context, host_princ);
	}

	if (keyblock) {
		krb5_free_keyblock(context, keyblock);
	}

	if (tkt != NULL) {
		krb5_free_ticket(context, tkt);
	}

	SAFE_FREE(host_princ_s);

	if (auth_context) {
		krb5_auth_con_free(context, auth_context);
	}

	if (context) {
		krb5_free_context(context);
	}

	return sret;
}

#endif /* HAVE_KRB5 */
