/*
   Unix SMB/CIFS implementation.
   simple kerberos5 routines for active directory
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Guenther Deschner 2005-2009

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

#ifndef _INCLUDE_KRB5_PROTOS_H_
#define _INCLUDE_KRB5_PROTOS_H_

struct PAC_DATA;
struct PAC_SIGNATURE_DATA;

/* work around broken krb5.h on sles9 */
#ifdef SIZEOF_LONG
#undef SIZEOF_LONG
#endif


#if defined(HAVE_KRB5)
krb5_error_code smb_krb5_parse_name(krb5_context context,
				const char *name, /* in unix charset */
                                krb5_principal *principal);

krb5_error_code smb_krb5_unparse_name(TALLOC_CTX *mem_ctx,
				      krb5_context context,
				      krb5_const_principal principal,
				      char **unix_name);

#ifndef HAVE_KRB5_SET_REAL_TIME
krb5_error_code krb5_set_real_time(krb5_context context, int32_t seconds, int32_t microseconds);
#endif

krb5_error_code krb5_set_default_tgs_ktypes(krb5_context ctx, const krb5_enctype *enc);

#if defined(HAVE_KRB5_AUTH_CON_SETKEY) && !defined(HAVE_KRB5_AUTH_CON_SETUSERUSERKEY)
krb5_error_code krb5_auth_con_setuseruserkey(krb5_context context, krb5_auth_context auth_context, krb5_keyblock *keyblock);
#endif

#ifndef HAVE_KRB5_FREE_UNPARSED_NAME
void krb5_free_unparsed_name(krb5_context ctx, char *val);
#endif

/* Stub out initialize_krb5_error_table since it is not present in all
 * Kerberos implementations. If it's not present, it's not necessary to
 * call it.
 */
#ifndef HAVE_INITIALIZE_KRB5_ERROR_TABLE
#define initialize_krb5_error_table()
#endif

/* The following definitions come from libsmb/clikrb5.c  */

/* Samba wrapper function for krb5 functionality. */
bool setup_kaddr( krb5_address *pkaddr, struct sockaddr_storage *paddr);
int create_kerberos_key_from_string(krb5_context context, krb5_principal host_princ, krb5_data *password, krb5_keyblock *key, krb5_enctype enctype, bool no_salt);
bool get_auth_data_from_tkt(TALLOC_CTX *mem_ctx, DATA_BLOB *auth_data, krb5_ticket *tkt);
krb5_const_principal get_principal_from_tkt(krb5_ticket *tkt);
krb5_error_code get_kerberos_allowed_etypes(krb5_context context, krb5_enctype **enctypes);
bool get_krb5_smb_session_key(TALLOC_CTX *mem_ctx,
			      krb5_context context,
			      krb5_auth_context auth_context,
			      DATA_BLOB *session_key, bool remote);
krb5_error_code smb_krb5_kt_free_entry(krb5_context context, krb5_keytab_entry *kt_entry);
krb5_principal kerberos_fetch_salt_princ_for_host_princ(krb5_context context, krb5_principal host_princ, int enctype);
void kerberos_set_creds_enctype(krb5_creds *pcreds, int enctype);
bool kerberos_compatible_enctypes(krb5_context context, krb5_enctype enctype1, krb5_enctype enctype2);
void kerberos_free_data_contents(krb5_context context, krb5_data *pdata);
NTSTATUS decode_pac_data(TALLOC_CTX *mem_ctx,
			 DATA_BLOB *pac_data_blob,
			 krb5_context context,
			 krb5_keyblock *service_keyblock,
			 krb5_const_principal client_principal,
			 time_t tgs_authtime,
			 struct PAC_DATA **pac_data_out);
void smb_krb5_checksum_from_pac_sig(krb5_checksum *cksum,
				    struct PAC_SIGNATURE_DATA *sig);
krb5_error_code smb_krb5_verify_checksum(krb5_context context,
					 const krb5_keyblock *keyblock,
					 krb5_keyusage usage,
					 krb5_checksum *cksum,
					 uint8 *data,
					 size_t length);
time_t get_authtime_from_tkt(krb5_ticket *tkt);
krb5_error_code smb_krb5_get_keyinfo_from_ap_req(krb5_context context,
						 const krb5_data *inbuf,
						 krb5_kvno *kvno,
						 krb5_enctype *enctype);
krb5_error_code krb5_rd_req_return_keyblock_from_keytab(krb5_context context,
							krb5_auth_context *auth_context,
							const krb5_data *inbuf,
							krb5_const_principal server,
							krb5_keytab keytab,
							krb5_flags *ap_req_options,
							krb5_ticket **ticket,
							krb5_keyblock **keyblock);
krb5_error_code smb_krb5_parse_name_norealm(krb5_context context,
					    const char *name,
					    krb5_principal *principal);
bool smb_krb5_principal_compare_any_realm(krb5_context context,
					  krb5_const_principal princ1,
					  krb5_const_principal princ2);
krb5_error_code smb_krb5_renew_ticket(const char *ccache_string, const char *client_string, const char *service_string, time_t *expire_time);
krb5_error_code kpasswd_err_to_krb5_err(krb5_error_code res_code);
krb5_error_code smb_krb5_gen_netbios_krb5_address(smb_krb5_addresses **kerb_addr);
krb5_error_code smb_krb5_free_addresses(krb5_context context, smb_krb5_addresses *addr);
NTSTATUS krb5_to_nt_status(krb5_error_code kerberos_error);
krb5_error_code nt_status_to_krb5(NTSTATUS nt_status);
void smb_krb5_free_error(krb5_context context, krb5_error *krberror);
krb5_error_code handle_krberror_packet(krb5_context context,
                                         krb5_data *packet);

void smb_krb5_get_init_creds_opt_free(krb5_context context,
				    krb5_get_init_creds_opt *opt);
krb5_error_code smb_krb5_get_init_creds_opt_alloc(krb5_context context,
				    krb5_get_init_creds_opt **opt);
krb5_error_code smb_krb5_mk_error(krb5_context context,
					krb5_error_code error_code,
					const krb5_principal server,
					krb5_data *reply);
krb5_enctype smb_get_enctype_from_kt_entry(krb5_keytab_entry *kt_entry);
krb5_error_code smb_krb5_enctype_to_string(krb5_context context,
					    krb5_enctype enctype,
					    char **etype_s);
krb5_error_code smb_krb5_open_keytab(krb5_context context,
				      const char *keytab_name,
				      bool write_access,
				      krb5_keytab *keytab);
krb5_error_code smb_krb5_keytab_name(TALLOC_CTX *mem_ctx,
				     krb5_context context,
				     krb5_keytab keytab,
				     const char **keytab_name);
krb5_error_code smb_krb5_get_credentials(krb5_context context,
					 krb5_ccache ccache,
					 krb5_principal me,
					 krb5_principal server,
					 krb5_principal impersonate_princ,
					 krb5_creds **out_creds);
krb5_error_code smb_krb5_get_creds(const char *server_s,
				   time_t time_offset,
				   const char *cc,
				   const char *impersonate_princ_s,
				   krb5_creds **creds_p);
char *smb_krb5_principal_get_realm(krb5_context context,
				   krb5_principal principal);


#endif /* HAVE_KRB5 */

int cli_krb5_get_ticket(TALLOC_CTX *mem_ctx,
			const char *principal, time_t time_offset,
			DATA_BLOB *ticket, DATA_BLOB *session_key_krb5,
			uint32_t extra_ap_opts, const char *ccname,
			time_t *tgs_expire,
			const char *impersonate_princ_s);

bool unwrap_edata_ntstatus(TALLOC_CTX *mem_ctx,
			   DATA_BLOB *edata,
			   DATA_BLOB *edata_out);
bool unwrap_pac(TALLOC_CTX *mem_ctx, DATA_BLOB *auth_data, DATA_BLOB *unwrapped_pac_data);

#endif /* _INCLUDE_KRB5_PROTOS_H_ */
