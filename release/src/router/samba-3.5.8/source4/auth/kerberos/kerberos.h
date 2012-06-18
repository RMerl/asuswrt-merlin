/* 
   Unix SMB/CIFS implementation.
   simple kerberos5 routines for active directory
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Luke Howard 2002-2003
   
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

#if defined(HAVE_KRB5)

#include "auth/kerberos/krb5_init_context.h"
#include "librpc/gen_ndr/krb5pac.h"

struct auth_serversupplied_info;
struct cli_credentials;

struct ccache_container {
	struct smb_krb5_context *smb_krb5_context;
	krb5_ccache ccache;
};

struct keytab_container {
	struct smb_krb5_context *smb_krb5_context;
	krb5_keytab keytab;
};

/* not really ASN.1, but RFC 1964 */
#define TOK_ID_KRB_AP_REQ	((const uint8_t *)"\x01\x00")
#define TOK_ID_KRB_AP_REP	((const uint8_t *)"\x02\x00")
#define TOK_ID_KRB_ERROR	((const uint8_t *)"\x03\x00")
#define TOK_ID_GSS_GETMIC	((const uint8_t *)"\x01\x01")
#define TOK_ID_GSS_WRAP		((const uint8_t *)"\x02\x01")

#ifdef HAVE_KRB5_KEYBLOCK_KEYVALUE
#define KRB5_KEY_TYPE(k)	((k)->keytype)
#define KRB5_KEY_LENGTH(k)	((k)->keyvalue.length)
#define KRB5_KEY_DATA(k)	((k)->keyvalue.data)
#else
#define	KRB5_KEY_TYPE(k)	((k)->enctype)
#define KRB5_KEY_LENGTH(k)	((k)->length)
#define KRB5_KEY_DATA(k)	((k)->contents)
#endif /* HAVE_KRB5_KEYBLOCK_KEYVALUE */

#ifndef HAVE_KRB5_SET_REAL_TIME
krb5_error_code krb5_set_real_time(krb5_context context, int32_t seconds, int32_t microseconds);
#endif

#ifndef HAVE_KRB5_SET_DEFAULT_TGS_KTYPES
krb5_error_code krb5_set_default_tgs_ktypes(krb5_context ctx, const krb5_enctype *enc);
#endif

#if defined(HAVE_KRB5_AUTH_CON_SETKEY) && !defined(HAVE_KRB5_AUTH_CON_SETUSERUSERKEY)
krb5_error_code krb5_auth_con_setuseruserkey(krb5_context context, krb5_auth_context auth_context, krb5_keyblock *keyblock);
#endif

#ifndef HAVE_KRB5_FREE_UNPARSED_NAME
void krb5_free_unparsed_name(krb5_context ctx, char *val);
#endif

#if defined(HAVE_KRB5_PRINCIPAL_GET_COMP_STRING) && !defined(HAVE_KRB5_PRINC_COMPONENT)
const krb5_data *krb5_princ_component(krb5_context context, krb5_principal principal, int i );
#endif

/* Samba wrapper function for krb5 functionality. */
void setup_kaddr( krb5_address *pkaddr, struct sockaddr *paddr);
int create_kerberos_key_from_string(krb5_context context, krb5_principal host_princ, krb5_data *password, krb5_keyblock *key, krb5_enctype enctype);
int create_kerberos_key_from_string_direct(krb5_context context, krb5_principal host_princ, krb5_data *password, krb5_keyblock *key, krb5_enctype enctype);
krb5_const_principal get_principal_from_tkt(krb5_ticket *tkt);
krb5_error_code get_kerberos_allowed_etypes(krb5_context context, krb5_enctype **enctypes);
void free_kerberos_etypes(krb5_context context, krb5_enctype *enctypes);
bool get_krb5_smb_session_key(krb5_context context, krb5_auth_context auth_context, DATA_BLOB *session_key, bool remote);
krb5_error_code ads_krb5_mk_req(krb5_context context, 
				krb5_auth_context *auth_context, 
				const krb5_flags ap_req_options,
				const char *principal,
				krb5_ccache ccache, 
				krb5_data *outbuf);
bool get_auth_data_from_tkt(TALLOC_CTX *mem_ctx, DATA_BLOB *auth_data, krb5_ticket *tkt);
int kerberos_kinit_password_cc(krb5_context ctx, krb5_ccache cc, 
			       krb5_principal principal, const char *password, 
			       time_t *expire_time, time_t *kdc_time);
int kerberos_kinit_keyblock_cc(krb5_context ctx, krb5_ccache cc, 
			       krb5_principal principal, krb5_keyblock *keyblock,
			       time_t *expire_time, time_t *kdc_time);
krb5_principal kerberos_fetch_salt_princ_for_host_princ(krb5_context context,
							krb5_principal host_princ,
							int enctype);
void kerberos_set_creds_enctype(krb5_creds *pcreds, int enctype);
bool kerberos_compatible_enctypes(krb5_context context, krb5_enctype enctype1, krb5_enctype enctype2);
void kerberos_free_data_contents(krb5_context context, krb5_data *pdata);
krb5_error_code smb_krb5_kt_free_entry(krb5_context context, krb5_keytab_entry *kt_entry);
char *smb_get_krb5_error_message(krb5_context context, krb5_error_code code, TALLOC_CTX *mem_ctx);
 krb5_error_code kinit_to_ccache(TALLOC_CTX *parent_ctx,
			  struct cli_credentials *credentials,
			  struct smb_krb5_context *smb_krb5_context,
				 krb5_ccache ccache);
krb5_error_code principal_from_credentials(TALLOC_CTX *parent_ctx, 
					   struct cli_credentials *credentials, 
					   struct smb_krb5_context *smb_krb5_context,
					   krb5_principal *princ);
NTSTATUS kerberos_decode_pac(TALLOC_CTX *mem_ctx,
			     struct smb_iconv_convenience *iconv_convenience,
			     struct PAC_DATA **pac_data_out,
			     DATA_BLOB blob,
			     krb5_context context,
			     const krb5_keyblock *krbtgt_keyblock,
			     const krb5_keyblock *service_keyblock,
			     krb5_const_principal client_principal,
			     time_t tgs_authtime,
			     krb5_error_code *k5ret);
 NTSTATUS kerberos_pac_logon_info(TALLOC_CTX *mem_ctx,
				  struct smb_iconv_convenience *iconv_convenience,
				  struct PAC_LOGON_INFO **logon_info,
				  DATA_BLOB blob,
				  krb5_context context,
				  const krb5_keyblock *krbtgt_keyblock,
				  const krb5_keyblock *service_keyblock,
				  krb5_const_principal client_principal,
				  time_t tgs_authtime, 
				  krb5_error_code *k5ret);
 krb5_error_code kerberos_encode_pac(TALLOC_CTX *mem_ctx,
				     struct smb_iconv_convenience *iconv_convenience,
				    struct PAC_DATA *pac_data,
				    krb5_context context,
				    const krb5_keyblock *krbtgt_keyblock,
				    const krb5_keyblock *service_keyblock,
				    DATA_BLOB *pac);
 krb5_error_code kerberos_create_pac(TALLOC_CTX *mem_ctx,
				     struct smb_iconv_convenience *iconv_convenience,
				     struct auth_serversupplied_info *server_info,
				     krb5_context context,
				     const krb5_keyblock *krbtgt_keyblock,
				     const krb5_keyblock *service_keyblock,
				     krb5_principal client_principal,
				     time_t tgs_authtime,
				     DATA_BLOB *pac);
struct loadparm_context;

#include "auth/kerberos/proto.h"

#endif /* HAVE_KRB5 */
