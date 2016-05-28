/*
  Public Interface file for Linux DNS client library implementation

  Copyright (C) 2006 Krishna Ganugapati <krishnag@centeris.com>
  Copyright (C) 2006 Gerald Carter <jerry@samba.org>

     ** NOTE! The following LGPL license applies to the libaddns
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301  USA
*/

#include "dns.h"
#include <ctype.h>


#ifdef HAVE_GSSAPI_SUPPORT

/*********************************************************************
*********************************************************************/

static int strupr( char *szDomainName )
{
	if ( !szDomainName ) {
		return ( 0 );
	}
	while ( *szDomainName != '\0' ) {
		*szDomainName = toupper( *szDomainName );
		szDomainName++;
	}
	return ( 0 );
}

#if 0
/*********************************************************************
*********************************************************************/

static void display_status_1( const char *m, OM_uint32 code, int type )
{
	OM_uint32 maj_stat, min_stat;
	gss_buffer_desc msg;
	OM_uint32 msg_ctx;

	msg_ctx = 0;
	while ( 1 ) {
		maj_stat = gss_display_status( &min_stat, code,
					       type, GSS_C_NULL_OID,
					       &msg_ctx, &msg );
		fprintf( stdout, "GSS-API error %s: %s\n", m,
			 ( char * ) msg.value );
		( void ) gss_release_buffer( &min_stat, &msg );

		if ( !msg_ctx )
			break;
	}
}

/*********************************************************************
*********************************************************************/

void display_status( const char *msg, OM_uint32 maj_stat, OM_uint32 min_stat )
{
	display_status_1( msg, maj_stat, GSS_C_GSS_CODE );
	display_status_1( msg, min_stat, GSS_C_MECH_CODE );
}
#endif

static DNS_ERROR dns_negotiate_gss_ctx_int( TALLOC_CTX *mem_ctx,
					    struct dns_connection *conn,
					    const char *keyname,
					    const gss_name_t target_name,
					    gss_ctx_id_t *ctx, 
					    enum dns_ServerType srv_type )
{
	struct gss_buffer_desc_struct input_desc, *input_ptr, output_desc;
	OM_uint32 major, minor;
	OM_uint32 ret_flags;
	DNS_ERROR err;

	gss_OID_desc krb5_oid_desc =
		{ 9, (char *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02" };

	*ctx = GSS_C_NO_CONTEXT;
	input_ptr = NULL;

	do {
		major = gss_init_sec_context(
			&minor, NULL, ctx, target_name, &krb5_oid_desc,
			GSS_C_REPLAY_FLAG | GSS_C_MUTUAL_FLAG |
			GSS_C_SEQUENCE_FLAG | GSS_C_CONF_FLAG |
			GSS_C_INTEG_FLAG | GSS_C_DELEG_FLAG,
			0, NULL, input_ptr, NULL, &output_desc,
			&ret_flags, NULL );

		if (input_ptr != NULL) {
			TALLOC_FREE(input_desc.value);
		}

		if (output_desc.length != 0) {

			struct dns_request *req;
			struct dns_rrec *rec;
			struct dns_buffer *buf;

			time_t t = time(NULL);

			err = dns_create_query(mem_ctx, keyname, QTYPE_TKEY,
					       DNS_CLASS_IN, &req);
			if (!ERR_DNS_IS_OK(err)) goto error;

			err = dns_create_tkey_record(
				req, keyname, "gss.microsoft.com", t,
				t + 86400, DNS_TKEY_MODE_GSSAPI, 0,
				output_desc.length, (uint8 *)output_desc.value,
				&rec );
			if (!ERR_DNS_IS_OK(err)) goto error;

			/* Windows 2000 DNS is broken and requires the
			   TKEY payload in the Answer section instead
			   of the Additional seciton like Windows 2003 */

			if ( srv_type == DNS_SRV_WIN2000 ) {
				err = dns_add_rrec(req, rec, &req->num_answers,
						   &req->answers);
			} else {
				err = dns_add_rrec(req, rec, &req->num_additionals,
						   &req->additionals);
			}
			
			if (!ERR_DNS_IS_OK(err)) goto error;

			err = dns_marshall_request(req, req, &buf);
			if (!ERR_DNS_IS_OK(err)) goto error;

			err = dns_send(conn, buf);
			if (!ERR_DNS_IS_OK(err)) goto error;

			TALLOC_FREE(req);
		}

		gss_release_buffer(&minor, &output_desc);

		if ((major != GSS_S_COMPLETE) &&
		    (major != GSS_S_CONTINUE_NEEDED)) {
			return ERROR_DNS_GSS_ERROR;
		}

		if (major == GSS_S_CONTINUE_NEEDED) {

			struct dns_request *resp;
			struct dns_buffer *buf;
			struct dns_tkey_record *tkey;

			err = dns_receive(mem_ctx, conn, &buf);
			if (!ERR_DNS_IS_OK(err)) goto error;

			err = dns_unmarshall_request(buf, buf, &resp);
			if (!ERR_DNS_IS_OK(err)) goto error;

			/*
			 * TODO: Compare id and keyname
			 */
			
			if ((resp->num_additionals != 1) ||
			    (resp->num_answers == 0) ||
			    (resp->answers[0]->type != QTYPE_TKEY)) {
				err = ERROR_DNS_INVALID_MESSAGE;
				goto error;
			}

			err = dns_unmarshall_tkey_record(
				mem_ctx, resp->answers[0], &tkey);
			if (!ERR_DNS_IS_OK(err)) goto error;

			input_desc.length = tkey->key_length;
			input_desc.value = talloc_move(mem_ctx, &tkey->key);

			input_ptr = &input_desc;

			TALLOC_FREE(buf);
		}

	} while ( major == GSS_S_CONTINUE_NEEDED );

	/* If we arrive here, we have a valid security context */

	err = ERROR_DNS_SUCCESS;

      error:

	return err;
}

DNS_ERROR dns_negotiate_sec_ctx( const char *target_realm,
				 const char *servername,
				 const char *keyname,
				 gss_ctx_id_t *gss_ctx,
				 enum dns_ServerType srv_type )
{
	OM_uint32 major, minor;

	char *upcaserealm, *targetname;
	DNS_ERROR err;

	gss_buffer_desc input_name;
	struct dns_connection *conn;

	gss_name_t targ_name;

	krb5_principal host_principal;
	krb5_context krb_ctx = NULL;

	gss_OID_desc nt_host_oid_desc =
		{ 10, (char *)"\052\206\110\206\367\022\001\002\002\002" };

	TALLOC_CTX *mem_ctx;

	if (!(mem_ctx = talloc_init("dns_negotiate_sec_ctx"))) {
		return ERROR_DNS_NO_MEMORY;
	}

	err = dns_open_connection( servername, DNS_TCP, mem_ctx, &conn );
	if (!ERR_DNS_IS_OK(err)) goto error;

	if (!(upcaserealm = talloc_strdup(mem_ctx, target_realm))) {
		err = ERROR_DNS_NO_MEMORY;
		goto error;
	}

	strupr(upcaserealm);

	if (!(targetname = talloc_asprintf(mem_ctx, "dns/%s@%s",
					   servername, upcaserealm))) {
		err = ERROR_DNS_NO_MEMORY;
		goto error;
	}

	krb5_init_context( &krb_ctx );
	krb5_parse_name( krb_ctx, targetname, &host_principal );

	/* don't free the principal until after you call
	   gss_release_name() or else you'll get a segv
	   as the krb5_copy_principal() does a structure 
	   copy and not a deep copy.    --jerry*/

	input_name.value = &host_principal;
	input_name.length = sizeof( host_principal );

	major = gss_import_name( &minor, &input_name,
				 &nt_host_oid_desc, &targ_name );

	if (major) {
		krb5_free_principal( krb_ctx, host_principal );
		krb5_free_context( krb_ctx );
		err = ERROR_DNS_GSS_ERROR;
		goto error;
	}

	err = dns_negotiate_gss_ctx_int(mem_ctx, conn, keyname, 
					targ_name, gss_ctx, srv_type );
	
	gss_release_name( &minor, &targ_name );

	/* now we can free the principal */

	krb5_free_principal( krb_ctx, host_principal );
	krb5_free_context( krb_ctx );

 error:
	TALLOC_FREE(mem_ctx);

	return err;
}

DNS_ERROR dns_sign_update(struct dns_update_request *req,
			  gss_ctx_id_t gss_ctx,
			  const char *keyname,
			  const char *algorithmname,
			  time_t time_signed, uint16 fudge)
{
	struct dns_buffer *buf;
	DNS_ERROR err;
	struct dns_domain_name *key, *algorithm;
	struct gss_buffer_desc_struct msg, mic;
	OM_uint32 major, minor;
	struct dns_rrec *rec;

	err = dns_marshall_update_request(req, req, &buf);
	if (!ERR_DNS_IS_OK(err)) return err;

	err = dns_domain_name_from_string(buf, keyname, &key);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_domain_name_from_string(buf, algorithmname, &algorithm);
	if (!ERR_DNS_IS_OK(err)) goto error;

	dns_marshall_domain_name(buf, key);
	dns_marshall_uint16(buf, DNS_CLASS_ANY);
	dns_marshall_uint32(buf, 0); /* TTL */
	dns_marshall_domain_name(buf, algorithm);
	dns_marshall_uint16(buf, 0); /* Time prefix for 48-bit time_t */
	dns_marshall_uint32(buf, time_signed);
	dns_marshall_uint16(buf, fudge);
	dns_marshall_uint16(buf, 0); /* error */
	dns_marshall_uint16(buf, 0); /* other len */

	err = buf->error;
	if (!ERR_DNS_IS_OK(buf->error)) goto error;

	msg.value = (void *)buf->data;
	msg.length = buf->offset;

	major = gss_get_mic(&minor, gss_ctx, 0, &msg, &mic);
	if (major != 0) {
		err = ERROR_DNS_GSS_ERROR;
		goto error;
	}

	if (mic.length > 0xffff) {
		gss_release_buffer(&minor, &mic);
		err = ERROR_DNS_GSS_ERROR;
		goto error;
	}

	err = dns_create_tsig_record(buf, keyname, algorithmname, time_signed,
				     fudge, mic.length, (uint8 *)mic.value,
				     req->id, 0, &rec);
	gss_release_buffer(&minor, &mic);
	if (!ERR_DNS_IS_OK(err)) goto error;

	err = dns_add_rrec(req, rec, &req->num_additionals, &req->additionals);

 error:
	TALLOC_FREE(buf);
	return err;
}

#endif	/* HAVE_GSSAPI_SUPPORT */
