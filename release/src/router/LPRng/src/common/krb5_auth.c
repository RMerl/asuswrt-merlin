/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: krb5_auth.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";

#include "lp.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "child.h"
#include "getqueue.h"
#include "linksupport.h"
#include "gethostinfo.h"
#include "permission.h"
#include "lpd_secure.h"
#include "lpd_dispatch.h"
#include "krb5_auth.h"

#if defined(KERBEROS)

/*
 * deprecated: krb5_auth_con_initivector, krb5_get_in_tkt_with_keytab
 * WARNING: the current set of KRB5 support examples are not compatible
 * with the legacy LPRng use of the KRB utility functions.  This means
 * that sooner or later it will be necessary to severly upgrade LPRng
 * or to use an older version of Kerberos
 */
# define KRB5_DEPRECATED 1
# include <krb5.h>
# include <com_err.h>

# undef FREE_KRB_DATA
# if defined(HAVE_KRB5_FREE_DATA_CONTENTS)
#  define FREE_KRB_DATA(context,data,suffix) krb5_free_data_contents(context,&data); data suffix = 0
# else
#  if defined(HAVE_KRB5_XFREE)
#   define FREE_KRB_DATA(context,data,suffix) krb5_xfree(data suffix); data suffix = 0
#  else
#    if !defined(HAVE_KRB_XFREE)
#     define FREE_KRB_DATA(context,data,suffix) krb_xfree(data suffix); data suffix = 0
#    else
#     error missing krb_xfree value or definition
#    endif
#  endif
# endif

#if !defined(KRB5_PROTOTYPE)
#define KRB5_PROTOTYPE(X) X
#endif
 extern krb5_error_code krb5_read_message 
	KRB5_PROTOTYPE((krb5_context,
		   krb5_pointer, 
		   krb5_data *));
 extern krb5_error_code krb5_write_message 
	KRB5_PROTOTYPE((krb5_context,
		   krb5_pointer, 
		   krb5_data *));
/*
 * server_krb5_auth(
 *  char *keytabfile,	server key tab file - /etc/lpr.keytab
 *  char *service,		service is usually "lpr"
 *  char *prinicpal,	specifically supply principal name
 *  int sock,		   socket for communications
 *  char *auth, int len authname buffer, max size
 *  char *err, int errlen error message buffer, max size
 * RETURNS: 0 if successful, non-zero otherwise, error message in err
 *   Note: there is a memory leak if authentication fails,  so this
 *   should not be done in the main or non-exiting process
 */
 extern int des_read( krb5_context context, krb5_encrypt_block *eblock,
	int fd, char *buf, int len, char *err, int errlen );
 extern int des_write( krb5_context context, krb5_encrypt_block *eblock,
	int fd, char *buf, int len, char *err, int errlen );

 /* we make these statics */
 static krb5_context context = 0;
 static krb5_auth_context auth_context = 0;
 static krb5_keytab keytab = 0;  /* Allow specification on command line */
 static krb5_principal server = 0;
 static krb5_ticket * ticket = 0;

 int server_krb5_auth( char *keytabfile, char *service, char *server_principal, int sock,
	char **auth, char *err, int errlen, char *file )
{
	int retval = 0;
	int fd = -1;
	krb5_data   inbuf, outbuf;
	struct stat statb;
	int status;
	char *cname = 0;

	DEBUG1("server_krb5_auth: keytab '%s', service '%s', principal '%s', sock %d, file '%s'",
		keytabfile, service, server_principal, sock, file );
	if( !keytabfile ){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"no server keytab file",
			Is_server?"on server":"on client" );
		retval = 1;
		goto done;
	}
	if( (fd = Checkread(keytabfile,&statb)) == -1 ){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"cannot open server keytab file '%s' - %s",
			Is_server?"on server":"on client",
			keytabfile,
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	close(fd);
	err[0] = 0;
	if ((retval = krb5_init_context(&context))){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_init_context failed - '%s' ",
			Is_server?"on server":"on client",
			error_message(retval) );
		goto done;
	}
	if( keytab == 0 && (retval = krb5_kt_resolve(context, keytabfile, &keytab) ) ){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_kt_resolve failed - file %s '%s'",
			Is_server?"on server":"on client",
			keytabfile,
			error_message(retval) );
		goto done;
	}
	if(server_principal){
		if ((retval = krb5_parse_name(context,server_principal, &server))){
			SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s",
			Is_server?"on server":"on client",
			server_principal, error_message(retval) );
			goto done;
		}
	} else {
		if ((retval = krb5_sname_to_principal(context, NULL, service,
						 KRB5_NT_SRV_HST, &server))){
			SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
				"krb5_sname_to_principal failed - service %s '%s'",
				Is_server?"on server":"on client",
				service, error_message(retval));
			goto done;
		}
	}
	if((retval = krb5_unparse_name(context, server, &cname))){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_unparse_name failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
	DEBUG1("server_krb5_auth: server '%s'", cname );

	if((retval = krb5_recvauth(context, &auth_context, (krb5_pointer)&sock,
				   service , server, 
				   0,   /* no flags */
				   keytab,  /* default keytab is NULL */
				   &ticket))){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_recvauth '%s' failed '%s'",
			Is_server?"on server":"on client",
			cname, error_message(retval));
		goto done;
	}

	/* Get client name */
	if((retval = krb5_unparse_name(context, 
		ticket->enc_part2->client, &cname))){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_unparse_name failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
	if( auth ) *auth = safestrdup( cname,__FILE__,__LINE__);
	DEBUG1( "server_krb5_auth: client '%s'", cname );
    /* initialize the initial vector */
    if((retval = krb5_auth_con_initivector(context, auth_context))){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_auth_con_initvector failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}

	krb5_auth_con_setflags(context, auth_context,
		KRB5_AUTH_CONTEXT_DO_SEQUENCE);
	if((retval = krb5_auth_con_genaddrs(context, auth_context, sock,
		KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
			KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"krb5_auth_con_genaddr failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
  
	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	fd = Checkwrite( file, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	DEBUG1( "server_krb5_auth: opened for write '%s', fd %d", file, fd );
	if( fd < 0 ){
		SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
			"file open failed: %s",
			Is_server?"on server":"on client",
			Errormsg(errno));
		retval = 1;
		goto done;
	}
	while( (retval = krb5_read_message(context,&sock,&inbuf)) == 0 ){
		if(DEBUGL5){
			char small[16];
			memcpy(small,inbuf.data,sizeof(small)-1);
			small[sizeof(small)-1] = 0;
			LOGDEBUG( "server_krb5_auth: got %d, '%s'",
				inbuf.length, small );
		}
		if((retval = krb5_rd_priv(context,auth_context,
			&inbuf,&outbuf,NULL))){
			SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
				"krb5_rd_priv failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			retval = 1;
			goto done;
		}
		if( outbuf.data ) outbuf.data[outbuf.length] = 0;
		status = Write_fd_len( fd, outbuf.data, outbuf.length );
		if( status < 0 ){
			SNPRINTF( err, errlen) "%s server_krb5_auth failed - "
				"write to file failed: %s",
				Is_server?"on server":"on client",
				Errormsg(errno));
			retval = 1;
			goto done;
		}
		/*FREE_KRB_DATA(context,inbuf.data);
		FREE_KRB_DATA(context,outbuf.data);
		*/
		FREE_KRB_DATA(context,inbuf,.data);
		FREE_KRB_DATA(context,outbuf,.data);
		inbuf.length = 0;
		outbuf.length = 0;
	}
	retval = 0;
	close(fd); fd = -1;

 done:
	if( cname )		free(cname); cname = 0;
	if( retval ){
		if( fd >= 0 )	close(fd);
		if( ticket )	krb5_free_ticket(context, ticket);
		ticket = 0;
		if( context && server )	krb5_free_principal(context, server);
		server = 0;
		if( context && auth_context)	krb5_auth_con_free(context, auth_context );
		auth_context = 0;
		if( context )	krb5_free_context(context);
		context = 0;
	}
	DEBUG1( "server_krb5_auth: retval %d, error: '%s'", retval, err );
	return(retval);
}


int server_krb5_status( int sock, char *err, int errlen, char *file )
{
	int fd = -1;
	int retval = 0;
	struct stat statb;
	char buffer[SMALLBUFFER];
	krb5_data   inbuf, outbuf;

	err[0] = 0;
	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	fd = Checkread( file, &statb );
	if( fd < 0 ){
		SNPRINTF( err, errlen)
			"file open failed: %s", Errormsg(errno));
		retval = 1;
		goto done;
	}
	DEBUG1( "server_krb5_status: sock '%d', file size %0.0f", sock, (double)(statb.st_size));

	while( (retval = read( fd,buffer,sizeof(buffer)-1)) > 0 ){
		inbuf.length = retval;
		inbuf.data = buffer;
		buffer[retval] = 0;
		DEBUG4("server_krb5_status: sending '%s'", buffer );
		if((retval = krb5_mk_priv(context,auth_context,
			&inbuf,&outbuf,NULL))){
			SNPRINTF( err, errlen) "%s server_krb5_status failed - "
				"krb5_mk_priv failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			retval = 1;
			goto done;
		}
		DEBUG4("server_krb5_status: encoded length '%d'", outbuf.length );
		if((retval= krb5_write_message(context,&sock,&outbuf))){
			SNPRINTF( err, errlen) "%s server_krb5_status failed - "
				"krb5_write_message failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			retval = 1;
			goto done;
		}
		FREE_KRB_DATA(context,outbuf,.data);
	}
	DEBUG1("server_krb5_status: done" );

 done:
	if( fd >= 0 )	close(fd);
	if( ticket )	krb5_free_ticket(context, ticket);
	ticket = 0;
	if( context && server )	krb5_free_principal(context, server);
	server = 0;
	if( context && auth_context)	krb5_auth_con_free(context, auth_context );
	auth_context = 0;
	if( context )	krb5_free_context(context);
	context = 0;
	DEBUG1( "server_krb5_status: retval %d, error: '%s'", retval, err );
	return(retval);
}

/*
 * client_krb5_auth(
 *  char * keytabfile	- keytabfile, NULL for users, file name for server
 *  char * service		-service, usually "lpr"
 *  char * host			- server host name
 *  char * principal	- server principal
 *  int options		 - options for server to server
 *  char *life			- lifetime of ticket
 *  char *renew_time	- renewal time of ticket
 *  char *err, int errlen - buffer for error messages 
 *  char *file			- file to transfer
 */ 
# define KRB5_DEFAULT_OPTIONS 0
# define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */
# define VALIDATE 0
# define RENEW 1

 extern krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	krb5_principal server, krb5_data *outbuf, int opt );

int client_krb5_auth( char *keytabfile, char *service, char *host,
	char *server_principal,
	int options, char *life, char *renew_time,
	int sock, char *err, int errlen, char *file )
{
	krb5_context context = 0;
	krb5_principal client = 0, server = 0;
	krb5_error *err_ret = 0;
	krb5_ap_rep_enc_part *rep_ret = 0;
	krb5_data cksum_data;
	krb5_ccache ccdef;
	krb5_auth_context auth_context = 0;
	krb5_timestamp now;
	krb5_deltat lifetime = KRB5_DEFAULT_LIFE;   /* -l option */
	krb5_creds my_creds;
	krb5_creds *out_creds = 0;
	krb5_keytab keytab = 0;
	krb5_deltat rlife = 0;
	krb5_address **addrs = (krb5_address **)0;
	krb5_encrypt_block eblock;	  /* eblock for encrypt/decrypt */
	krb5_data   inbuf, outbuf;
	int retval = 0;
	char *cname = 0;
	char *sname = 0;
	int fd = -1, len;
	char buffer[SMALLBUFFER];
	struct stat statb;

	err[0] = 0;
	DEBUG1( "client_krb5_auth: euid/egid %d/%d, ruid/rguid %d/%d, keytab '%s',"
		" service '%s', host '%s', sock %d, file '%s'",
		geteuid(),getegid(), getuid(),getgid(),
		keytabfile, service, host, sock, file );
	if( !safestrcasecmp(host,LOCALHOST) ){
		host = FQDNHost_FQDN;
	}
	memset((char *)&my_creds, 0, sizeof(my_creds));
	memset((char *)&outbuf, 0, sizeof(outbuf));
	memset((char *)&eblock, 0, sizeof(eblock));
	options |= KRB5_DEFAULT_OPTIONS;

	if ((retval = krb5_init_context(&context))){
		SNPRINTF( err, errlen)
			"%s krb5_init_context failed - '%s' ",
			Is_server?"on server":"on client",
			error_message(retval) );
		goto done;
	}
#if 0
	if (!valid_cksumtype(CKSUMTYPE_CRC32)) {
		SNPRINTF( err, errlen)
			"valid_cksumtype CKSUMTYPE_CRC32 - %s",
			error_message(KRB5_PROG_SUMTYPE_NOSUPP) );
		retval = 1;
		goto done;
	}
#endif
	DEBUG1( "client_krb5_auth: using host='%s', server_principal '%s'",
		host, server_principal );

	if(server_principal){
		if ((retval = krb5_parse_name(context,server_principal, &server))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s",
			Is_server?"on server":"on client",
			server_principal, error_message(retval) );
			goto done;
		}
	} else {
		/* XXX perhaps we want a better metric for determining localhost? */
		if (strncasecmp("localhost", host, sizeof(host)))
			retval = krb5_sname_to_principal(context, host, service,
							 KRB5_NT_SRV_HST, &server);
		else
			/* Let libkrb5 figure out its notion of the local host */
			retval = krb5_sname_to_principal(context, NULL, service,
							 KRB5_NT_SRV_HST, &server);
		if (retval) {
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"when parsing service/host '%s'/'%s'"
			" - %s",
			Is_server?"on server":"on client",
			service,host,error_message(retval) );
			goto done;
		}
	}

	if((retval = krb5_unparse_name(context, server, &sname))){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			" krb5_unparse_name of 'server' failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
	DEBUG1( "client_krb5_auth: server '%s'", sname );

	my_creds.server = server;

	if( keytabfile ){
		if ((retval = krb5_sname_to_principal(context, NULL, service, 
			 KRB5_NT_SRV_HST, &client))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s",
			Is_server?"on server":"on client",
			service, error_message(retval) );
			goto done;
		}
		if(cname)free(cname); cname = 0;
		if((retval = krb5_unparse_name(context, client, &cname))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_unparse_name of 'me' failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			goto done;
		}
		DEBUG1("client_krb5_auth: client '%s'", cname );
		my_creds.client = client;
		if((retval = krb5_kt_resolve(context, keytabfile, &keytab))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			 "resolving keytab '%s'"
			" '%s' - ",
			Is_server?"on server":"on client",
			keytabfile, error_message(retval) );
			goto done;
		}
		if ((retval = krb5_timeofday(context, &now))) {
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			 "getting time of day"
			" - '%s'",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
		if( life && (retval = krb5_string_to_deltat(life, &lifetime)) ){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"bad lifetime value '%s'"
			" '%s' - ",
			Is_server?"on server":"on client",
			life, error_message(retval) );
			goto done;
		}
		if( renew_time ){
			options |= KDC_OPT_RENEWABLE;
			if( (retval = krb5_string_to_deltat(renew_time, &rlife))){
				SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"bad renew time value '%s'"
				" '%s' - ",
				Is_server?"on server":"on client",
				renew_time, error_message(retval) );
				goto done;
			}
		}
		if((retval = krb5_cc_default(context, &ccdef))) {
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"while getting default ccache"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}

		my_creds.times.starttime = 0;	 /* start timer when request */
		my_creds.times.endtime = now + lifetime;

		if(options & KDC_OPT_RENEWABLE) {
			my_creds.times.renew_till = now + rlife;
		} else {
			my_creds.times.renew_till = 0;
		}

		if(options & KDC_OPT_VALIDATE){
			/* stripped down version of krb5_mk_req */
			if( (retval = krb5_tgt_gen(context,
				ccdef, server, &outbuf, VALIDATE))) {
				SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"validating tgt"
				" - %s",
				Is_server?"on server":"on client",
				error_message(retval) );
				DEBUG1("%s", err );
			}
		}

		if (options & KDC_OPT_RENEW) {
			/* stripped down version of krb5_mk_req */
			if( (retval = krb5_tgt_gen(context,
				ccdef, server, &outbuf, RENEW))) {
				SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"renewing tgt"
				" - %s",
				Is_server?"on server":"on client",
				error_message(retval) );
				DEBUG1("%s", err );
			}
		}

		if((retval = krb5_get_in_tkt_with_keytab(context, options, addrs,
					0, 0, keytab, 0, &my_creds, 0))){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"while getting initial credentials"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
		/* update the credentials */
		if( (retval = krb5_cc_initialize (context, ccdef, client)) ){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"when initializing cache"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
		if( (retval = krb5_cc_store_cred(context, ccdef, &my_creds))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"while storing credentials"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
	} else {
		/* we set RUID to user */
		if( Is_server ){
			To_ruid( DaemonUID );
		} else {
			To_ruid( OriginalRUID );
		}
		if((retval = krb5_cc_default(context, &ccdef))){
			SNPRINTF( err, errlen) "%s krb5_cc_default failed - %s",
				Is_server?"on server":"on client",
				error_message( retval ) );
			goto done;
		}
		if((retval = krb5_cc_get_principal(context, ccdef, &client))){
			SNPRINTF( err, errlen) "%s krb5_cc_get_principal failed - %s",
				Is_server?"on server":"on client",
				error_message( retval ) );
			goto done;
		}
		if( Is_server ){
			To_daemon();
		} else {
			To_user();
		}
		if(cname)free(cname); cname = 0;
		if((retval = krb5_unparse_name(context, client, &cname))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_unparse_name of 'me' failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			goto done;
		}
		DEBUG1( "client_krb5_auth: client '%s'", cname );
		my_creds.client = client;
	}

	cksum_data.data = host;
	cksum_data.length = safestrlen(host);

	if((retval = krb5_auth_con_init(context, &auth_context))){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"krb5_auth_con_init failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}

	if((retval = krb5_auth_con_genaddrs(context, auth_context, sock,
		KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
		KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"krb5_auth_con_genaddr failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
  
	retval = krb5_sendauth(context, &auth_context, (krb5_pointer) &sock,
			   service, client, server,
			   AP_OPTS_MUTUAL_REQUIRED,
			   &cksum_data,
			   &my_creds,
			   ccdef, &err_ret, &rep_ret, &out_creds);

	if (retval){
		if( err_ret == 0 ){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_sendauth failed - %s",
				Is_server?"on server":"on client",
				error_message( retval ) );
		} else {
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_sendauth - mutual authentication failed - %*s",
				Is_server?"on server":"on client",
				err_ret->text.length, err_ret->text.data);
		}
		goto done;
	} else if (rep_ret == 0) {
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"krb5_sendauth - did not do mutual authentication",
			Is_server?"on server":"on client" );
		retval = 1;
		goto done;
	} else {
		DEBUG1("client_krb5_auth: sequence number %d", rep_ret->seq_number );
	}
    /* initialize the initial vector */
    if((retval = krb5_auth_con_initivector(context, auth_context))){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"krb5_auth_con_initvector failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}

	krb5_auth_con_setflags(context, auth_context,
		KRB5_AUTH_CONTEXT_DO_SEQUENCE);

	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	fd = Checkread( file, &statb );
	if( fd < 0 ){
		SNPRINTF( err, errlen)
			"%s client_krb5_auth: could not open for reading '%s' - '%s'",
			Is_server?"on server":"on client",
			file,
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	DEBUG1( "client_krb5_auth: opened for read %s, fd %d, size %0.0f", file, fd, (double)statb.st_size );
	while( (len = read( fd, buffer, sizeof(buffer)-1 )) > 0 ){
		/* status = Write_fd_len( sock, buffer, len ); */
		inbuf.data = buffer;
		inbuf.length = len;
		if((retval = krb5_mk_priv(context, auth_context, &inbuf,
			&outbuf, NULL))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_mk_priv failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			goto done;
		}
		if((retval = krb5_write_message(context, (void *)&sock, &outbuf))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_write_message failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			goto done;
		}
		DEBUG4( "client_krb5_auth: freeing data");
		FREE_KRB_DATA(context,outbuf,.data);
	}
	if( len < 0 ){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"client_krb5_auth: file read failed '%s' - '%s'", file,
			Is_server?"on server":"on client",
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	close(fd);
	fd = -1;
	DEBUG1( "client_krb5_auth: file copy finished %s", file );
	if( shutdown(sock, 1) == -1 ){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"shutdown failed '%s'",
			Is_server?"on server":"on client",
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	fd = Checkwrite( file, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	if( fd < 0 ){
		SNPRINTF( err, errlen)
			"%s client_krb5_auth: could not open for writing '%s' - '%s'",
			Is_server?"on server":"on client",
			file,
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	while((retval = krb5_read_message( context,&sock,&inbuf))==0){
		if((retval = krb5_rd_priv(context, auth_context, &inbuf,
			&outbuf, NULL))){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"krb5_rd_priv failed - %s",
				Is_server?"on server":"on client",
				Errormsg(errno) );
			retval = 1;
			goto done;
		}
		if(Write_fd_len(fd,outbuf.data,outbuf.length) < 0){
			SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
				"write to '%s' failed - %s",
				Is_server?"on server":"on client",
				file, Errormsg(errno) );
			retval = 1;
			goto done;
		}
		FREE_KRB_DATA(context,inbuf,.data);
		FREE_KRB_DATA(context,outbuf,.data);
	}
	close(fd); fd = -1;
	fd = Checkread( file, &statb );
	err[0] = 0;
	if( fd < 0 ){
		SNPRINTF( err, errlen)
			"%s client_krb5_auth: could not open for reading '%s' - '%s'",
				Is_server?"on server":"on client",
				file,
				Errormsg(errno) );
		retval = 1;
		goto done;
	}
	DEBUG1( "client_krb5_auth: reopened for read %s, fd %d, size %0.0f", file, fd, (double)statb.st_size );
	if( dup2(fd,sock) == -1){
		SNPRINTF( err, errlen) "%s client_krb5_auth failed - "
			"dup2(%d,%d) failed - '%s'",
			Is_server?"on server":"on client",
			fd, sock, Errormsg(errno) );
	}
	retval = 0;

 done:
	if( fd >= 0 && fd != sock ) close(fd);
	DEBUG4( "client_krb5_auth: freeing my_creds");
	krb5_free_cred_contents( context, &my_creds );
	DEBUG4( "client_krb5_auth: freeing rep_ret");
	if( rep_ret )	krb5_free_ap_rep_enc_part( context, rep_ret ); rep_ret = 0;
	DEBUG4( "client_krb5_auth: freeing err_ret");
	if( err_ret )	krb5_free_error( context, err_ret ); err_ret = 0;
	DEBUG4( "client_krb5_auth: freeing auth_context");
	if( auth_context) krb5_auth_con_free(context, auth_context );
	auth_context = 0;
	DEBUG4( "client_krb5_auth: freeing context");
	if( context )	krb5_free_context(context); context = 0;
	DEBUG1( "client_krb5_auth: retval %d, error '%s'",retval, err );
	return(retval);
}

/*
 * remote_principal_krb5(
 *  char * service		-service, usually "lpr"
 *  char * host			- server host name
 *  char *buffer, int bufferlen - buffer for credentials
 *  get the principal name of the remote service
 */ 
int remote_principal_krb5( char *service, char *host, char *err, int errlen )
{
	krb5_context context = 0;
	krb5_principal server = 0;
	int retval = 0;
	char *cname = 0;

	DEBUG1("remote_principal_krb5: service '%s', host '%s'",
		service, host );
	if ((retval = krb5_init_context(&context))){
		SNPRINTF( err, errlen) "%s '%s'",
			"krb5_init_context failed - '%s' ", error_message(retval) );
		goto done;
	}
	if((retval = krb5_sname_to_principal(context, host, service,
			 KRB5_NT_SRV_HST, &server))){
		SNPRINTF( err, errlen) "krb5_sname_to_principal %s/%s failed - %s",
			service, host, error_message(retval) );
		goto done;
	}
	if((retval = krb5_unparse_name(context, server, &cname))){
		SNPRINTF( err, errlen)
			"krb5_unparse_name failed - %s", error_message(retval));
		goto done;
	}
	strncpy( err, cname, errlen );
 done:
	if( cname )		free(cname); cname = 0;
	if( server )	krb5_free_principal(context, server); server = 0;
	if( context )	krb5_free_context(context); context = 0;
	DEBUG1( "remote_principal_krb5: retval %d, result: '%s'",retval, err );
	return(retval);
}

/*
 * Initialize a credentials cache.
 */

# define KRB5_DEFAULT_OPTIONS 0
# define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */

# define VALIDATE 0
# define RENEW 1

/* stripped down version of krb5_mk_req */
 krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	 krb5_principal server, krb5_data *outbuf, int opt )
{
	krb5_error_code	   retval;
	krb5_creds		  * credsp;
	krb5_creds			creds;

	/* obtain ticket & session key */
	memset((char *)&creds, 0, sizeof(creds));
	if ((retval = krb5_copy_principal(context, server, &creds.server)))
		goto cleanup;

	if ((retval = krb5_cc_get_principal(context, ccache, &creds.client)))
		goto cleanup_creds;

	if(opt == VALIDATE) {
			if ((retval = krb5_get_credentials_validate(context, 0,
					ccache, &creds, &credsp)))
				goto cleanup_creds;
	} else {
			if ((retval = krb5_get_credentials_renew(context, 0,
					ccache, &creds, &credsp)))
				goto cleanup_creds;
	}

	/* we don't actually need to do the mk_req, just get the creds. */
 cleanup_creds:
	krb5_free_cred_contents(context, &creds);

 cleanup:

	return retval;
}


 char *storage;
 int nstored = 0;
 char *store_ptr;
 krb5_data desinbuf,desoutbuf;

# define ENCBUFFERSIZE 2*LARGEBUFFER

 int des_read( krb5_context context,
	krb5_encrypt_block *eblock,
	int fd, char *buf, int len,
	char *err, int errlen )
{
	int nreturned = 0;
	long net_len,rd_len;
	int cc;
	unsigned char len_buf[4];
	
	if( len <= 0 ) return(len);

	if( desinbuf.data == 0 ){
		desinbuf.data = malloc_or_die(  ENCBUFFERSIZE, __FILE__,__LINE__ );
		storage = malloc_or_die( ENCBUFFERSIZE, __FILE__,__LINE__ );
	}
	if (nstored >= len) {
		memcpy(buf, store_ptr, len);
		store_ptr += len;
		nstored -= len;
		return(len);
	} else if (nstored) {
		memcpy(buf, store_ptr, nstored);
		nreturned += nstored;
		buf += nstored;
		len -= nstored;
		nstored = 0;
	}
	
	if ((cc = read(fd, len_buf, 4)) != 4) {
		/* XXX can't read enough, pipe must have closed */
		return(0);
	}
	rd_len =
		((len_buf[0]<<24) | (len_buf[1]<<16) | (len_buf[2]<<8) | len_buf[3]);
	net_len = krb5_encrypt_size(rd_len,eblock->crypto_entry);
	if ((net_len <= 0) || (net_len > ENCBUFFERSIZE )) {
		/* preposterous length; assume out-of-sync; only
		   recourse is to close connection, so return 0 */
		SNPRINTF( err, errlen) "des_read: "
			"read size problem");
		return(-1);
	}
	if ((cc = read( fd, desinbuf.data, net_len)) != net_len) {
		/* pipe must have closed, return 0 */
		SNPRINTF( err, errlen) "des_read: "
		"Read error: length received %d != expected %d.",
				(int)cc, (int)net_len);
		return(-1);
	}
	/* decrypt info */
	if((cc = krb5_decrypt(context, desinbuf.data, (krb5_pointer) storage,
						  net_len, eblock, 0))){
		SNPRINTF( err, errlen) "des_read: "
			"Cannot decrypt data from network - %s", error_message(cc) );
		return(-1);
	}
	store_ptr = storage;
	nstored = rd_len;
	if (nstored > len) {
		memcpy(buf, store_ptr, len);
		nreturned += len;
		store_ptr += len;
		nstored -= len;
	} else {
		memcpy(buf, store_ptr, nstored);
		nreturned += nstored;
		nstored = 0;
	}
	
	return(nreturned);
}


 int des_write( krb5_context context,
	krb5_encrypt_block *eblock,
	int fd, char *buf, int len,
	char *err, int errlen )
{
	char len_buf[4];
	int cc;

	if( len <= 0 ) return( len );
	if( desoutbuf.data == 0 ){
		desoutbuf.data = malloc_or_die( ENCBUFFERSIZE, __FILE__,__LINE__ );
	}
	desoutbuf.length = krb5_encrypt_size(len, eblock->crypto_entry);
	if (desoutbuf.length > ENCBUFFERSIZE ){
		SNPRINTF( err, errlen) "des_write: "
		"Write size problem - wanted %d", desoutbuf.length);
		return(-1);
	}
	if ((cc=krb5_encrypt(context, (krb5_pointer)buf,
					   desoutbuf.data,
					   len,
					   eblock,
					   0))){
		SNPRINTF( err, errlen) "des_write: "
		"Write encrypt problem. - %s", error_message(cc));
		return(-1);
	}
	
	len_buf[0] = (len & 0xff000000) >> 24;
	len_buf[1] = (len & 0xff0000) >> 16;
	len_buf[2] = (len & 0xff00) >> 8;
	len_buf[3] = (len & 0xff);
	if( Write_fd_len(fd, len_buf, 4) < 0 ){
		SNPRINTF( err, errlen) "des_write: "
		"Could not write len_buf - %s", Errormsg( errno ));
		return(-1);
	}
	if(Write_fd_len(fd, desoutbuf.data,desoutbuf.length) < 0 ){
		SNPRINTF( err, errlen) "des_write: "
		"Could not write data - %s", Errormsg(errno));
		return(-1);
	}
	else return(len); 
}

# if defined(MIT_KERBEROS4)
#  if defined(HAVE_KRB_H)
#   include <krb.h>
/* #   "des.h" */
#  endif
#  if defined(HAVE_KERBEROSIV_KRB_H)
#   include <kerberosIV/krb.h>
#   include <kerberosIV/des.h>
#  endif

#  if !defined(HAVE_KRB_AUTH_DEF)

 extern int  krb_sendauth(
  long options,            /* bit-pattern of options */
  int fd,              /* file descriptor to write onto */
  KTEXT ticket,   /* where to put ticket (return) or supplied in case of KOPT_DONT_MK_REQ */
  char *service, char *inst, char *realm,    /* service name, instance, realm */
  u_long checksum,         /* checksum to include in request */
  MSG_DAT *msg_data,       /* mutual auth MSG_DAT (return) */
  CREDENTIALS *cred,       /* credentials (return) */
  Key_schedule schedule,       /* key schedule (return) */
  struct sockaddr_in *laddr,   /* local address */
  struct sockaddr_in *faddr,   /* address of foreign host on fd */
  char *version);           /* version string */
 extern char* krb_realmofhost( char *host );

 extern int krb_recvauth(
	long options,            /* bit-pattern of options */
	int fd,              /* file descr. to read from */
	KTEXT ticket,            /* storage for client's ticket */
	char *service,           /* service expected */
	char *instance,          /* inst expected (may be filled in) */
	struct sockaddr_in *faddr,   /* address of foreign host on fd */
	struct sockaddr_in *laddr,   /* local address */
	AUTH_DAT *kdata,         /* kerberos data (returned) */
	char *filename,          /* name of file with service keys */
	Key_schedule schedule,       /* key schedule (return) */
	char *version);           /* version string (filled in) */

#  endif


 struct krberr{
	int err; char *code;
 } krb4_errs[] = {
	{1, "KDC_NAME_EXP 'Principal expired'"},
	{2, "KDC_SERVICE_EXP 'Service expired'"},
	{2, "K_LOCK_EX 'Exclusive lock'"},
	{3, "KDC_AUTH_EXP 'Auth expired'"},
	{4, "CLIENT_KRB_TIMEOUT 'time between retries'"},
	{4, "KDC_PKT_VER 'Protocol version unknown'"},
	{5, "KDC_P_MKEY_VER 'Wrong master key version'"},
	{6, "KDC_S_MKEY_VER 'Wrong master key version'"},
	{7, "KDC_BYTE_ORDER 'Byte order unknown'"},
	{8, "KDC_PR_UNKNOWN 'Principal unknown'"},
	{9, "KDC_PR_N_UNIQUE 'Principal not unique'"},
	{10, "KDC_NULL_KEY 'Principal has null key'"},
	{20, "KDC_GEN_ERR 'Generic error from KDC'"},
	{21, "GC_TKFIL 'Can't read ticket file'"},
	{21, "RET_TKFIL 'Can't read ticket file'"},
	{22, "GC_NOTKT 'Can't find ticket or TGT'"},
	{22, "RET_NOTKT 'Can't find ticket or TGT'"},
	{26, "DATE_SZ 'RTI date output'"},
	{26, "MK_AP_TGTEXP 'TGT Expired'"},
	{31, "RD_AP_UNDEC 'Can't decode authenticator'"},
	{32, "RD_AP_EXP 'Ticket expired'"},
	{33, "RD_AP_NYV 'Ticket not yet valid'"},
	{34, "RD_AP_REPEAT 'Repeated request'"},
	{35, "RD_AP_NOT_US 'The ticket isn't for us'"},
	{36, "RD_AP_INCON 'Request is inconsistent'"},
	{37, "RD_AP_TIME 'delta_t too big'"},
	{38, "RD_AP_BADD 'Incorrect net address'"},
	{39, "RD_AP_VERSION 'protocol version mismatch'"},
	{40, "RD_AP_MSG_TYPE 'invalid msg type'"},
	{41, "RD_AP_MODIFIED 'message stream modified'"},
	{42, "RD_AP_ORDER 'message out of order'"},
	{43, "RD_AP_UNAUTHOR 'unauthorized request'"},
	{51, "GT_PW_NULL 'Current PW is null'"},
	{52, "GT_PW_BADPW 'Incorrect current password'"},
	{53, "GT_PW_PROT 'Protocol Error'"},
	{54, "GT_PW_KDCERR 'Error returned by KDC'"},
	{55, "GT_PW_NULLTKT 'Null tkt returned by KDC'"},
	{56, "SKDC_RETRY 'Retry count exceeded'"},
	{57, "SKDC_CANT 'Can't send request'"},
	{61, "INTK_W_NOTALL 'Not ALL tickets returned'"},
	{62, "INTK_BADPW 'Incorrect password'"},
	{63, "INTK_PROT 'Protocol Error'"},
	{70, "INTK_ERR 'Other error'"},
	{71, "AD_NOTGT 'Don't have tgt'"},
	{72, "AD_INTR_RLM_NOTGT 'Can't get inter-realm tgt'"},
	{76, "NO_TKT_FIL 'No ticket file found'"},
	{77, "TKT_FIL_ACC 'Couldn't access tkt file'"},
	{78, "TKT_FIL_LCK 'Couldn't lock ticket file'"},
	{79, "TKT_FIL_FMT 'Bad ticket file format'"},
	{80, "TKT_FIL_INI 'tf_init not called first'"},
	{81, "KNAME_FMT 'Bad kerberos name format'"},
	{100, "MAX_HSTNM 'for compatibility'"},
	{512, "CLIENT_KRB_BUFLEN 'max unfragmented packet'"},
	{0,0}
	};

char *krb4_err_str( int err )
{
	int i;
	char *s = 0;
	for( i = 0; (s = krb4_errs[i].code) && err != krb4_errs[i].err; ++i );
	if( s == 0 ){
		static char msg[24];
		SNPRINTF(msg,sizeof(msg))"UNKNOWN %d", err );
		s = msg;
	}
	return(s);
}

int Send_krb4_auth( struct job *job, int *sock,
	int connect_timeout, char *errmsg, int errlen,
	struct security *security, struct line_list *info )
{
	int status = JFAIL;
	int ack, i;
	KTEXT_ST ticket;
	char buffer[1], *host;
	char line[LINEBUFFER];
	struct sockaddr_in sinaddr;

	errmsg[0] = 0;
	status = 0;

	sinaddr.sin_family = Host_IP.h_addrtype;
	memmove( &sinaddr.sin_addr, Host_IP.h_addr_list.list[0],
		Host_IP.h_length );

	if( *sock < 0 ){
		/* this is to fix up the error message */
		return(JSUCC);
	}

	host = RemoteHost_DYN;
	if( !safestrcasecmp( host, LOCALHOST ) ){
		host = FQDNHost_FQDN;
	}
	if( !safestrchr( host, '.' ) ){
		if( !(host = Find_fqdn(&LookupHost_IP, host)) ){
			SETSTATUS(job) "cannot find FQDN for '%s'", host );
			return JFAIL;
		}
	}
	DEBUG1("Send_krb4_auth: FQND host '%s'", host );
	SETSTATUS(job) "sending krb4 auth to %s@%s",
		RemotePrinter_DYN, host);
	SNPRINTF(line, sizeof(line)) "%c%s\n", REQ_K4AUTH, RemotePrinter_DYN);
	status = Link_send(host, sock, connect_timeout, line,
		safestrlen(line), &ack);
	DEBUG1("Send_krb4_auth: krb4 auth request ACK status %d, ack %d", status, ack );
	if( status ){
		SETSTATUS(job) "Printer %s@%s does not support krb4 authentication",
			RemotePrinter_DYN, host);
		shutdown(*sock,1);
		return JFAIL;
	}
	memset(&ticket,0,sizeof(ticket));
	status = krb_sendauth(0, *sock, &ticket, KLPR_SERVICE, host,
		krb_realmofhost(host), 0, NULL, NULL,
		NULL, NULL, NULL, "KLPRV0.1");
	DEBUG1("Send_krb4_auth: krb_sendauth status %d, '%s'",
		status, krb4_err_str(status) );
	if( status != KSUCCESS ){
		SNPRINTF(errmsg, errlen) "krb4 authentication failed to %s@%s - %s",
			RemotePrinter_DYN, host, krb4_err_str(status));
		shutdown(*sock,1);
		return JFAIL;
	}
	buffer[0] = 0;
	i = Read_fd_len_timeout(connect_timeout, *sock, buffer, 1);
	if (i <= 0 || Alarm_timed_out){
		status = LINK_TRANSFER_FAIL;
	} else if(buffer[0]){
		status = LINK_ACK_FAIL;
	}
	if(status){
		SNPRINTF(errmsg, errlen) "cannot read status from %s@%s - %s",
			RemotePrinter_DYN, host, krb4_err_str(status));
		shutdown(*sock,1);
		return JFAIL;
	} else {
		SETSTATUS(job)"krb4 authentication succeeded to %s@%s",
			RemotePrinter_DYN, host);
	}
	return(status);
}

/***************************************************************************
 * Commentary:
 * MIT Athena extension  --mwhitson@mit.edu 12/2/98
 * 
 * The protocol used to send a krb4 authenticator consists of:
 * 
 * Client                                   Server
 * kprintername\n - receive authenticator
 *                                          \0  (ack)
 * <krb4_credentials>
 *                                          \0
 * 
 * The server will note validity of the credentials for future service
 * requests in this session.  No capacity for TCP stream encryption or
 * verification is provided.
 * 
 ***************************************************************************/

/* A bunch of this has been ripped from the Athena lpd, and, as such,
 * isn't as nice as Patrick's code.  I'll clean it up eventually.
 *   -mwhitson
 *   Ran it through the drunk tank and detox center,
 *      and cleaned the dog's teeth.
 *     -papowell ("sigh...") Powell
 */
int Receive_k4auth( int *sock, char *input )
{
	int status = 0;
	char error_msg[LINEBUFFER];
	char cmd[LINEBUFFER];
	struct line_list values;
	int ack = ACK_SUCCESS;
	int k4error=0;
#if defined(HAVE_SOCKLEN_T)
	socklen_t sin_len;
#else
	int sin_len;
#endif

	struct sockaddr_in faddr;
	int len;
	uid_t euid;
	KTEXT_ST k4ticket;
	AUTH_DAT k4data;
	char k4principal[ANAME_SZ];
	char k4instance[INST_SZ];
	char k4realm[REALM_SZ];
	char k4version[9];
	char k4name[ANAME_SZ + INST_SZ + REALM_SZ + 3];

	sin_len = sizeof(struct sockaddr_in);
	Init_line_list(&values);
	error_msg[0] = '\0';
	DEBUG1("Receive_k4auth: doing '%s'", ++input);

	k4principal[0] = k4realm[0] = '\0';
	memset(&k4ticket, 0, sizeof(k4ticket));
	memset(&k4data, 0, sizeof(k4data));
	if (getpeername(*sock, (struct sockaddr *) &faddr, &sin_len) <0) {
	  	status = JFAIL;
		SNPRINTF( error_msg, sizeof(error_msg)) 
			      "Receive_k4auth: couldn't get peername" );
		goto error;
	}
    DEBUG1("Receive_k4auth: remote host IP '%s'",
        inet_ntoa( faddr.sin_addr ) );
	status = Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN,"",1,0 );
	if( status ){
		ack = ACK_FAIL;
		SNPRINTF( error_msg, sizeof(error_msg))
			      "Receive_k4auth: sending ACK 0 failed" );
		goto error;
	}
	strcpy(k4instance, "*");
	euid = geteuid();
	/* this is a nasty hack to get around krb4 library problems
	 * it appears to use the EUID/UID values to get the service
	 * to read, so you need to set it to uid root/euid root
	 */
	if( Is_server ) setuid(0);  /* Need root here to read srvtab */
	k4error = krb_recvauth(0, *sock, &k4ticket, KLPR_SERVICE,
			      k4instance,
			      &faddr,
			      (struct sockaddr_in *)NULL,
			      &k4data, "", NULL,
			      k4version);
	/* now we can set the euid back */
	if( Is_server ) (void)To_euid(euid);
	DEBUG1("Receive_k4auth: krb_recvauth returned %d, '%s'",
		k4error, krb_err_txt[k4error] );
	if (k4error != KSUCCESS) {
		/* erroring out here if the auth failed. */
	  	status = JFAIL;
		SNPRINTF( error_msg, sizeof(error_msg))
		    "kerberos 4 receive authentication failed - '%s'",
			krb_err_txt[k4error] );
	  	goto error;
	}

	strncpy(k4principal, k4data.pname, ANAME_SZ);
	strncpy(k4instance, k4data.pinst, INST_SZ);
	strncpy(k4realm, k4data.prealm, REALM_SZ);

	/* Okay, we got auth.  Note it. */

	if (k4instance[0]) {
		SNPRINTF( k4name, sizeof(k4name)) "%s.%s@%s", k4principal,
			      k4instance, k4realm );
	} else {
		SNPRINTF( k4name, sizeof(k4name)) "%s@%s", k4principal, k4realm );
	}
	DEBUG1("Receive_k4auth: auth for %s", k4name);
	Perm_check.authtype = "kerberos4";
	Set_str_value(&values,FROM,k4name);
	/* we will only use this for client to server authentication */
	if( (status = Check_secure_perms( &values, 0, error_msg, sizeof(error_msg)) )){
		DEBUGF(DRECV1)("Receive_k4auth: Check_secure_perms failed - '%s'",
			error_msg );
		goto error;
	}

	/* ACK the credentials  */
	status = Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN,"",1,0 );
	if( status ){
		ack = ACK_FAIL;
		SNPRINTF(error_msg, sizeof(error_msg))
			     "Receive_k4auth: sending ACK 0 failed" );
		goto error;
	}
	len = sizeof(cmd)-1;
    status = Link_line_read(ShortRemote_FQDN,sock,
        Send_job_rw_timeout_DYN,cmd,&len);
	if( len >= 0 ) cmd[len] = 0;
    DEBUG1( "Receive_k4auth: read status %d, len %d, '%s'",
        status, len, cmd );
    if( len == 0 ){
        DEBUG3( "Receive_k4auth: zero length read" );
		cleanup(0);
    }
    if( status ){
        LOGERR_DIE(LOG_DEBUG) "Service_connection: cannot read request" );
    }
    if( len < 3 ){
        FATAL(LOG_INFO) "Service_connection: bad request line '%s'", cmd );
    }
	Free_line_list(&values);
    Dispatch_input(sock,cmd);

	status = ack = 0;


 error:	
	DEBUG1("Receive_k4auth: error - status %d, ack %d, k4error %d, error '%s'",
		status, ack, k4error, error_msg );
	if( status || ack ){
		cmd[0] = ack;
		if(ack) Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN, cmd, 1, 0 );
		if( status == 0 ) status = JFAIL;
		/* shut down reception from the remote file */
		if( error_msg[0] ){
			safestrncat( error_msg, "\n" );
			Write_fd_str( *sock, error_msg );
		}
	}

	DEBUG1("Receive_k4auth: done");
	Free_line_list(&values);
	return(status);
}
# endif


int Krb5_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile )
{
	int status = 0;
	char *from = 0;
	char *keytab = 0;
	char *service = 0;
	char *principal = 0;

	errmsg[0] = 0;
	keytab = Find_str_value(info,"keytab",Value_sep);
	service = Find_str_value(info,"service",Value_sep);
	if( !(principal = Find_str_value(info,"server_principal",Value_sep)) ){
		principal = Find_str_value(info,"id",Value_sep);
	}
	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		SNPRINTF( errmsg, errlen) "Krb5_receive: ACK 0 write errmsg - %s",
			Errormsg(errno) );
	} else if( (status = server_krb5_auth( keytab, service, principal, *sock,
		&from, errmsg, errlen, tempfile )) ){
		if( errmsg[0] == 0 ){
			SNPRINTF( errmsg, errlen) "Krb5_receive: server_krb5_auth failed - no reason given" );
		}
	} else {
		DEBUGF(DRECV1)("Krb5_receive: from '%s'", from );
		Set_str_value( header_info, FROM, from );
		status = Do_secure_work( jobsize, from_server, tempfile, header_info );
		if( server_krb5_status( *sock, errmsg, errlen, tempfile ) ){
			SNPRINTF( errmsg, errlen) "Krb5_receive: status send failed - '%s'",
				errmsg );
		}
	}
#if 0
	if( *errmsg ){
		LOGMSG(LOG_INFO)"%s", errmsg );
	}
#endif
	if( from ) free(from); from = 0;
	return(status);
}



/*
 * 
 * The following routines simply implement the encryption and transfer of
 * the files and/or values
 * 
 * By default, when sending a command,  the file will contain:
 *   key=value lines.
 *   KEY           PURPOSE
 *   client        client or user name
 *   from          originator - server if forwarding, client otherwise
 *   command       command to send
 * 
 */

int Krb5_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info )
{
	char *keyfile = 0;
	int status = 0, fd = -1;
	struct stat statb;
	char *principal = 0;
	char *service = 0;
	char *life,  *renew;

	DEBUG1("Krb5_send: tempfile '%s'", tempfile );
	life = renew = 0;
	if( Is_server ){
		if( !(keyfile = Find_str_value(info,"keytab",Value_sep)) ){
			SNPRINTF( error, errlen) "no server keytab file" );
			status = JFAIL;
			goto error;
		}
		DEBUG1("Krb5_send: keyfile '%s'", keyfile );
		if( (fd = Checkread(keyfile,&statb)) == -1 ){
			SNPRINTF( error, errlen)
				"cannot open server keytab file - %s",
				Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
		close(fd);
		principal = Find_str_value(info,"forward_principal",Value_sep);
	} else {
		if( !(principal = Find_str_value(info,"server_principal",Value_sep)) ){
			principal = Find_str_value(info,"id",Value_sep);
		}
	}
	service = Find_str_value(info, "service", Value_sep );
	life = Find_str_value(info, "life", Value_sep );
	renew = Find_str_value(info, "renew", Value_sep );
	status= client_krb5_auth( keyfile, service,
		RemoteHost_DYN, /* remote host */
		principal,	/* principle name of the remote server */
		0,	/* options */
		life,	/* lifetime of server ticket */
		renew,	/* renewable time of server ticket */
		*sock, error, errlen, tempfile );
	DEBUG1("Krb5_send: client_krb5_auth returned '%d' - error '%s'",
		status, error );
	if( status && error[0] == 0 ){
		SNPRINTF( error, errlen)
		"krb5 authenticated transfer to remote host failed");
	}
#if 0
	if( error[0] ){
		DEBUG2("Krb5_send: writing error to file '%s'", error );
		if( safestrlen(error) < errlen-2 ){
			memmove( error+1, error, safestrlen(error)+1 );
			error[0] = ' ';
		}
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			SNPRINTF(error,errlen)
			"Krb5_send: open '%s' for write failed - %s",
				tempfile, Errormsg(errno));
		}
		Write_fd_str(fd,error);
		close( fd ); fd = -1;
		error[0] = 0;
	}
#endif
  error:
	return(status);
}

#endif
