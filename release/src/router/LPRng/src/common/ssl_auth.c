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
"$Id: ssl_auth.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "fileopen.h"
#include "errorcodes.h"
#include "getqueue.h"
#ifdef SSL_ENABLE
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "user_auth.h"
#include "lpd_secure.h"
#include "ssl_auth.h"

/*
   The code for the SSL support routines has been taken from
   various places, including:

   OpenSSL Example Programs 20020110
   by Eric Rescorla
   January 10, 2002 Edition

   SHAMELESS PLUG
      Extremely detailed coverage of SSL/TLS can be found in 
   
   	_SSL_and_TLS:_Designing_and_Building_Secure_Systems_
   	Eric Rescorla
   	Addison-Wesley, 2001
   	ISBN 0-201-61598-3

 The code for fetchmail:

  ftp://ftp.ccil.org/pub/esr/fetchmail
  http://www.tuxedo.org/~esr/fetchmail

 The code for Lynx
   The Lynx homepage is <URL: http://lynx.browser.org/>.
   http://lynx.isc.org/release
   ftp://lynx.isc.org/release

 The code for stunnel
   http://www.stunnel.org

 And, of course, the SSL examples
   http://www.openssl.org
    openssl-0.9.6c

 Just to make it silly,  I have used small snippets of various parts of
 all these programs,  and as far as I can tell,  do not need to include
 the GNU copyleft, licenses, or other stuff.  This allows me to distribute
 this under the BSD license or GLIB license.

 This is all much sillyness simply to be clear of encumberment and cost
 me a couple of weeks work.

  Patick Powell
  Sat May 11 16:30:07 PDT 2002
 
*/  



/*
 * The callback for getting a password for the 
 * private key.  You need to return it in the buffer.
 *  If you are a server:
 *    read from SSL_server_password_file
 *  If you are a client:
 *    check for SSL_PASSWORD (actual value)
 *    check for SSL_PASSWORD_PATH (read the file)
 */

 static char password_value[ 64 ];

 static int Password_callback( char *pass,int len,
	int rwflag,void *userdata)
{
	DEBUG1("Password_callback: returning '%s'", password_value);
	if( safestrlen( password_value ) >= len ){
		Errorcode = JABORT;
		FATAL(LOG_ERR) "Passwd_callback: password len %d longer then max len %d",
			safestrlen(password_value), len );
	}
	mystrncpy(pass,password_value,len);
    return(safestrlen(pass));
}

 static void SSL_init(void)
{
	static int ssl_init;
	if( !ssl_init ){
		/* Global system initialization*/
		SSL_library_init();
		SSL_load_error_strings();
		ssl_init = 1;
	}
}

/*
 * char *Set_ERR_str( char *header, char *errmsg, int errlen )
 *  - generate the SSL error message 
 */

char *Set_ERR_str( char *header, char *errmsg, int errlen )
{
	unsigned long e;
	errmsg[0] = 0;
	if( header ){
		snprintf(errmsg, errlen, "%s: ", header );
	}
	while(  ERR_peek_error() ){
		int n = strlen(errmsg);
		e = ERR_get_error();
		ERR_error_string_n( e, errmsg+n, errlen -n );
		if( ERR_peek_error() ){
			n = strlen(errmsg);
			SNPRINTF(errmsg+n,errlen-n) "\n");
		}
	}
	return( errmsg );
}

/*
 * static char *getuservals( char *env, char *homedir, char *file, char *buf, int maxlen )
 *  get a file name from an environment variable or create it.
 *  if env value, check to make sure file exists
 *  if make up, check and if not present, then return null
 */

 static char *getuservals( char *env, char *homedir, char *file, char *buf, int maxlen )
{
	char *s = 0;
	struct stat statb;
	if( env ) s = getenv(env);
	if( !s ){
		SNPRINTF(buf,maxlen) "%s/%s", homedir, file );
		s = buf;
		while( s && (s = strchr(s,'/')) ){
			if( cval(s+1) == '/' ){
				memmove(s,s+1,safestrlen(s+1)+1);
			} else {
				++s;
			}
		}
		s = buf;
	}
	if( s && stat(s,&statb) ){
		Errorcode = JABORT;
		FATAL(LOG_ERR)
			"getuservals: cannot stat '%s' - %s",
			s, Errormsg(errno) );
	}
	return(s);
}

/*
 * SSL_Initialize_ctx - initialize SSL context
 */

int SSL_Initialize_ctx(
	SSL_CTX **ctx_ret,
	char *errmsg, int errlen )
{
	char *certpath, *certfile, *cp, *cf;
	char *mycert;
    SSL_METHOD *meth = 0;
    SSL_CTX *ctx = 0;
	char header[SMALLBUFFER];
	char cabuf[MAXPATHLEN], certbuf[MAXPATHLEN], pwbuf[MAXPATHLEN]; 
	struct stat statb;
	int pid;
	char *file, *s;
	int fd = -1, n;
    
    /* Global system initialization*/
	SSL_init();

    /* Create our context*/
    meth=SSLv23_method();
    ctx=SSL_CTX_new(meth);
	*ctx_ret = ctx;
	if( ctx == 0 ){
		Set_ERR_str( "SSL_Initialize: SSL_CTX_new failed",  errmsg, errlen );
		return -1;
	}
	mycert = 0;
	/* get the directory for the SSL certificates */
	if( ISNULL(Ssl_ca_path_DYN) && !ISNULL(Ssl_ca_file_DYN) ){
		Set_DYN(&Ssl_ca_path_DYN, Ssl_ca_file_DYN);
		if( (s = strrchr(Ssl_ca_path_DYN, '/')) ) *s = 0;
	}
	cp = certpath = Ssl_ca_path_DYN;
	cf = certfile = Ssl_ca_file_DYN;
	if( Is_server ){
		mycert = Ssl_server_cert_DYN;
		file = Ssl_server_password_file_DYN;
		if( file ){
			if( (fd = Checkread( file, &statb )) < 0 ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR) "SSL_initialize: cannot open server_password_file '%s'",
					file );
			}
			if( (n = read(fd, password_value, sizeof(password_value)-1)) < 0 ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR) "SSL_initialize: cannot read server_password_file '%s'",
					file );
			}
			password_value[n] = 0;
			if( (s = safestrchr(password_value,'\n')) ) *s = 0;
			n = strlen(password_value);
			if( n == 0 ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR) "SSL_initialize: zero length server_password_file '%s'",
					file );
			}
		}
	} else {
		struct passwd *pw;
		char *homedir;
		if( (pw = getpwuid( getuid())) == 0 ){
			LOGERR_DIE(LOG_INFO) "setup_envp: getpwuid(%d) failed", getuid());
		}
		homedir = pw->pw_dir;
		mycert = getuservals("LPR_SSL_FILE",homedir, ".lpr/client.crt", certbuf,sizeof(certbuf));
		fd = -1;
		file = getuservals("LPR_SSL_PASSWORD",homedir, ".lpr/client.pwd", pwbuf, sizeof(pwbuf));
		if( file ) fd = Checkread( file, &statb );
		if( fd > 0 ){
			if( (n = read(fd, password_value, sizeof(password_value)-1)) < 0 ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_ERR) "SSL_initialize: cannot read server_password_file '%s'",
					file );
			}
			password_value[n] = 0;
			if( (s = safestrchr(password_value,'\n')) ) *s = 0;
		}
	}
	DEBUG1("SSL_Initialize_ctx: certpath '%s', certfile '%s', mycert '%s', password '%s'",
		certpath, certfile, mycert, password_value );

	/* Certificate Authority Files
	 *  - the certfile can contain a list of certificates
	 *  - the certpath has a list of files, with an index
	 */
	if( certpath && stat(certpath,&statb) ) cp = 0;
	if( certfile && stat(certfile,&statb) ) cf = 0;
	if( cf == 0 && cp == 0 ){
		SNPRINTF( errmsg,errlen)
			"SSL_initialize: Missing both CA file '%s' and CA path '%s'", certfile, certpath );
		return -1;
	}
	if( !SSL_CTX_load_verify_locations(ctx, cf, cp) ){
		DEBUG1("SSL_Initialize_ctx: verify locations failed");
		SNPRINTF( header,sizeof(header))
			"SSL_initialize: Bad CA file '%s' or CA path '%s'", cf, cp );
		Set_ERR_str( header, errmsg, errlen );
		return(-1);
	}

	/*
	 * Certificate Checking (server)
	 * we request check - this is good practice
	 */
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);

	/*
	 * we have a certificate or key or both
	 * we can assume that if you have one or the other that
	 * it is a combined key/cert file
	 * Set the password callback BEFORE we load the Private Key file
	 */

    SSL_CTX_set_default_passwd_cb(ctx, Password_callback);
	if( (cp = mycert) && stat(mycert,&statb) ) cp = 0;
	if( Is_server && !cp ){
		SNPRINTF( errmsg,errlen)
			"SSL_initialize: Missing cert file '%s'", mycert );
		return -1;
	}
	if( cp ){
		if( !SSL_CTX_use_certificate_chain_file(ctx, mycert) ){
			SNPRINTF( header,sizeof(header))
				"SSL_initialize: can't read certificate file '%s'", mycert );
			Set_ERR_str( header, errmsg, errlen );
			return(-1);
		}
		if( !SSL_CTX_use_PrivateKey_file(ctx, mycert, SSL_FILETYPE_PEM) ){
			SNPRINTF( header,sizeof(header))
				"SSL_initialize: can't read private key in '%s'", mycert );
			Set_ERR_str( header, errmsg, errlen );
			return(-1);
		}
	}

	/* we set the session id context for the server.
	 * This has no effect on clients, but appears to be
	 * harmless.
	 */
	pid = getpid();
	if( !SSL_CTX_set_session_id_context(ctx,
			(void*)&pid, sizeof pid) ){
		Set_ERR_str( "SSL_initialize: SSL_CTX_set_session_id_context failed", errmsg, errlen );
		return(-1);
	}


#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(ctx,1);
#endif
    return 0;
}
     
void Destroy_ctx(SSL_CTX *ctx)
{
    SSL_CTX_free(ctx);
}

/*
 * get peer certificate information
 */

void Get_cert_info( SSL *ssl, struct line_list *info )
{
	X509 *peer;
	STACK_OF(X509) *sk;
	int i;
	char *certs = 0;
	char buffer[SMALLBUFFER];

	sk=SSL_get_peer_cert_chain(ssl);
	if( sk && sk_X509_num(sk) ){
		for( i = 0; i < sk_X509_num(sk); ++i ){
			peer = sk_X509_value(sk,i);
			if( X509_NAME_oneline( X509_get_subject_name( peer ),
				buffer, sizeof(buffer) ) ){
				DEBUG1("Open_SSL_connection: level [%d] subject '%s'", i, buffer );
				if( i == 0 && info ){
					Set_str_value(info,AUTHFROM,buffer);
				}
				if( i ) certs = safeextend3( certs, buffer, "\n", __FILE__,__LINE__ );
			}
			if( X509_NAME_oneline( X509_get_issuer_name( peer ),
				buffer, sizeof(buffer) ) ){
				DEBUG1("Open_SSL_connection: level [%d] issuer '%s'", i, buffer );
				certs = safeextend3( certs, buffer, "\n", __FILE__,__LINE__ );
			}
		}
	} else {
		peer = SSL_get_peer_certificate(ssl);
		if( peer ){
			if( X509_NAME_oneline( X509_get_subject_name( peer ),
				buffer, sizeof(buffer) ) ){
				DEBUG1("Open_SSL_connection: peer subject '%s'", buffer );
				if( info ){
					Set_str_value(info,AUTHFROM,buffer);
				}
				certs = safeextend3( certs, buffer, "\n", __FILE__,__LINE__ );
			}
			if( X509_NAME_oneline( X509_get_issuer_name( peer ),
				buffer, sizeof(buffer) ) ){
				DEBUG1("Open_SSL_connection: peer issuer '%s'", buffer );
				certs = safeextend3( certs, buffer, "\n", __FILE__,__LINE__ );
			}
		}
	}
	if( info ){
		Set_str_value(info,AUTHCA,certs);
	}
	if( certs ) free( certs ); certs = 0;
}

/*
 * Make the connection an SSL connnection
 *  - we also set up a BIO so that we can do buffered line reads
 *    and writes
 */

int Open_SSL_connection( int sock, SSL_CTX *ctx, SSL **ssl_ret,
	struct line_list *info, char *errmsg, int errlen )
{
	SSL *ssl = 0;
	BIO *bio;
	int ret;
	long verify_result;
	char buffer[SMALLBUFFER];
	int status = 0;

	/* we get the SSL context.  No connection yet */
	ssl = SSL_new(ctx);
	if( !ssl ){
		Set_ERR_str( "Open_SSL_connection: SSL_new failed", errmsg, errlen );
		status = -1;
		goto done;
	}
	/* we get the magic BIO wrapper, and put it around
	 * our socket.  Note that we want to make sure that
	 * the socket is not closed by the BIO functions
	 */
	if( !(bio = BIO_new_socket(sock, BIO_NOCLOSE)) ){
		Set_ERR_str( "Open_SSL_connection: BIO_new_socket failed", errmsg, errlen );
		status = -1;
		goto done;
	}
    SSL_set_bio(ssl,bio,bio);

	/* if you get any sort of error, give up */
	ret = SSL_connect( ssl );
	DEBUG1("Open_SSL_connection: SSL_connect returned %d, SSL_get_error = %d",
		ret, SSL_get_error(ssl, ret) );
	switch( SSL_get_error(ssl, ret) ){
		case SSL_ERROR_NONE:
			break;
		default:
		SNPRINTF(buffer,sizeof(buffer))
		 	"SSL_connect failed, err %d, SSL_get_error %d",
				ret, SSL_get_error(ssl, ret) );
			Set_ERR_str( buffer, errmsg, errlen );
			status = -1;
			goto done;
			break;
	}


	/* now we check to see which server we talked to */
	verify_result = SSL_get_verify_result(ssl);
	DEBUG1("Open_SSL_connection: SSL_get_verify_result '%s'",
		X509_verify_cert_error_string(verify_result) );

	if( verify_result != X509_V_OK ){
		SNPRINTF(errmsg,errlen)
		 	"SSL_connect failed, peer certificat not verified: '%s'",
				X509_verify_cert_error_string(verify_result) );
		status = -1;
		goto done;
	}
	Get_cert_info( ssl, info );

 done:
	if( status ){
		if( ssl ) SSL_free(ssl); ssl = 0;
	}
	*ssl_ret = ssl;

	return( status );
}

/*
 * accept an incoming SSL connection
 */


int Accept_SSL_connection( int sock, int timeout, SSL_CTX *ctx, SSL **ssl_ret,
	struct line_list *info, char *errmsg, int errlen )
{
	SSL *ssl = 0;
	BIO *bio;
	int ret, n, finished;
	long verify_result;
	char buffer[SMALLBUFFER];
	int wait_for_read, wait_for_write;	/* for select */
	int status = 0;

	/* we get the SSL context.  No connection yet */
	DEBUG1("Accept_ssl_connection: starting, ctx 0x%lx, sock %d", Cast_ptr_to_long(ctx), sock);
	
	ssl = SSL_new(ctx);
	DEBUG1("Accept_ssl_connection: SSL_new 0x%lx", Cast_ptr_to_long(ssl) );
	if( !ssl ){
		Set_ERR_str( "Accept_SSL_connection: SSL_new failed", errmsg, errlen );
		DEBUG1("Accept_ssl_connection: '%s'", errmsg);
		status = -1;
		goto done;
	}
	/* we get the magic BIO wrapper, and put it around
	 * our socket.  Note that we want to make sure that
	 * the socket is not closed by the BIO functions
	 */
	if( !(bio = BIO_new_socket(sock, BIO_NOCLOSE)) ){
		Set_ERR_str( "Accept_SSL_connection: BIO_new_socket failed", errmsg, errlen );
		DEBUG1("Accept_ssl_connection: '%s'", errmsg);
		status = -1;
		goto done;
	}
    SSL_set_bio(ssl,bio,bio);

	/* if you get any sort of error, give up */
	finished = wait_for_read = wait_for_write = 0;
	DEBUG1("Accept_ssl_connection: loop");
	while(!finished){
		if( wait_for_read || wait_for_write ){
			fd_set readfds, writefds, exceptfds;	/* for select() */
			struct timeval tv, *tm = 0;

			DEBUG1("Accept_ssl_connection: need to wait for IO");
			memset(&tv,0,sizeof(tv));
			tv.tv_sec = timeout;
			if( timeout ) tm = &tv;
			FD_ZERO( &readfds ); FD_ZERO( &writefds ); FD_ZERO( &exceptfds );
			if( wait_for_read ) FD_SET( sock, &readfds );
			if( wait_for_write ) FD_SET( sock, &writefds );
			FD_SET( sock, &exceptfds );
			wait_for_read = wait_for_write = 0;
			n = select( sock+1,&readfds, &writefds, &exceptfds, tm );
			if( n == 0 ){
				SNPRINTF(errmsg, errlen)
				"Accept_SSL_connection: timeout");
				status = -1;
				goto done;
			}
		}
		ret = SSL_accept( ssl );
		n = SSL_get_error(ssl, ret);
		DEBUG1("Accept_SSL_connection: SSL_accept returned %d, SSL_get_error = %d",
			ret, n  );
		switch( n ){
			case SSL_ERROR_NONE: finished = 1; break;
			case SSL_ERROR_WANT_READ: wait_for_read = 1; break;
			case SSL_ERROR_WANT_WRITE: wait_for_write = 1; break;
			default:
			SNPRINTF(buffer,sizeof(buffer))
				"SSL_accept failed, err %d, SSL_get_error %d '%s'",
					ret, n, Error_SSL_name(n) );
				Set_ERR_str( buffer, errmsg, errlen );
				status = -1;
				DEBUG1("Accept_SSL_connection: '%s'", errmsg );
				goto done;
				break;
		}
	}
	/* now we check to see which server we talked to */
	verify_result = SSL_get_verify_result(ssl);
	DEBUG1("Accept_SSL_connection: SSL_get_verify_result '%s'",
		X509_verify_cert_error_string(verify_result) );

	if( verify_result != X509_V_OK ){
		SNPRINTF(errmsg,errlen)
		 	"SSL_connect failed, peer certificat not verified: '%s'",
				X509_verify_cert_error_string(verify_result) );
		status = -1;
		goto done;
	}
	Get_cert_info( ssl, info );

 done:
	if( status ){
		if( ssl ) SSL_free(ssl); ssl = 0;
	}
	*ssl_ret = ssl;

	return( status );
}

/*
 * write a buffer to the SSL connection
 *  return: 0 if successful
 *         != 0 if failure
 */

int Write_SSL_connection( int timeout, SSL *ssl, char *buffer, int len,
	char *errmsg, int errlen )
{
	int done = 0, n = 0;
	int status = 0;
	while( !status && done < len ){
		Set_timeout_break( timeout );
		n = SSL_write( ssl, buffer+done, len - done );
		Clear_timeout();
		switch( SSL_get_error(ssl, n) ){
			case SSL_ERROR_NONE:
				done += n;
				break;
			case SSL_ERROR_ZERO_RETURN:
				status = LINK_TRANSFER_FAIL;
				break;
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				break;
			default:
				SNPRINTF(buffer,sizeof(buffer))
					"SSL_write failed, err %d, SSL_get_error %d",
					n, SSL_get_error(ssl, n) );
				Set_ERR_str( buffer, errmsg, errlen );
				status = LINK_TRANSFER_FAIL;
				break;
		}
	}
	return( status );
}

/*
 * get a line termianted with \n into the buffer of size len
 *  - reads at most len-1 chars to make sure a terminating 0
 * can be appended
 *  returns: 0 - success
 *           -1 - error
 *           1  - EOF
 */
int Gets_SSL_connection( int timeout, SSL *ssl, char *inbuffer, int len,
	char *errmsg, int errlen )
{
	int n, ret, status = 0, done = 0;
	char buffer[SMALLBUFFER];
	buffer[0] = 0;
	for( n = 0; !done && n < len-1; ){
		Set_timeout_break( timeout );
		ret = SSL_read( ssl, inbuffer+n, 1 );
		Clear_timeout();
		if( ret > 0 ){
			n += ret;
			inbuffer[n] = 0;
			if( cval(inbuffer+n-1) == '\n' ) done = 1;
		}
		DEBUG1("Gets_SSL_connection: ret %d, n %d, '%s'\n", ret, n, inbuffer );
		switch( SSL_get_error( ssl, ret ) ){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				done = status = 1;
				break;
			default:
				done = status = -1;
				SNPRINTF(buffer,sizeof(buffer))
					"SSL_read failed, err %d, SSL_get_error %d",
					ret, SSL_get_error(ssl, ret) );
				Set_ERR_str( buffer, errmsg, errlen );
				break;
		}
	}
	return( status );
}


/*
 * read a buffer of size len
 *  returns: 0 - success
 *           -1 - error
 *           1  - EOF
 *  *len = number of bytes read
 */
int Read_SSL_connection( int timeout, SSL *ssl, char *inbuffer, int *len,
	char *errmsg, int errlen )
{
	char buffer[SMALLBUFFER];
	int n, ret, status = 0;
	n = *len; *len = 0;
	Set_timeout_break( timeout );
	ret = SSL_read( ssl, inbuffer, n );
	Clear_timeout();
	switch( SSL_get_error( ssl, ret ) ){
		case SSL_ERROR_NONE:
			*len = n;
			break;
		case SSL_ERROR_ZERO_RETURN:
			status = 1;
			break;
		default:
			status = -1;
			SNPRINTF(buffer,sizeof(buffer))
				"SSL_read failed, err %d, SSL_get_error %d",
				ret, SSL_get_error(ssl, ret) );
			Set_ERR_str( buffer, errmsg, errlen );
			status = LINK_TRANSFER_FAIL;
			break;
	}
	return( status );
}

/*
 * close and kill off the SSL connection with undue violence
 */

int Close_SSL_connection( int sock, SSL *ssl )
{
	int ret;
	BIO *bio = 0;
	char buffer[SMALLBUFFER];
	int status = 0;
	/* do the shutdown and brutally at that */
	if( ssl ){
		ret = SSL_shutdown(ssl);
		DEBUG1( "SSL_shutdown returned %d, SSL_get_error %d - '%s'",
			ret, SSL_get_error(ssl, ret), Error_SSL_name(SSL_get_error(ssl,ret)) );
		if( ret == 0 ){
			shutdown( sock,1 );
			ret = SSL_shutdown(ssl);
			DEBUG1( "SSL_shutdown (second) returned %d, SSL_get_error %d",
				ret, SSL_get_error(ssl, ret) );
		}
		if( ret != 1 ){
			SNPRINTF(buffer,sizeof(buffer))
				"SSL_shutdown failed, err %d, SSL_get_error %d",
				ret, SSL_get_error(ssl, ret) );
		}
	}
	return( status );
}

const char * Error_SSL_name( int i )
{
	char *s = "Unknown";
	switch(i){
	case SSL_ERROR_NONE: s = "SSL_ERROR_NONE"; break;
	case SSL_ERROR_SSL: s = "SSL_ERROR_SSL"; break;
	case SSL_ERROR_WANT_READ: s = "SSL_ERROR_WANT_READ"; break;
	case SSL_ERROR_WANT_WRITE: s = "SSL_ERROR_WANT_WRITE"; break;
	case SSL_ERROR_WANT_X509_LOOKUP: s = "SSL_ERROR_WANT_X509_LOOKUP"; break;
	case SSL_ERROR_SYSCALL: s = "SSL_ERROR_SYSCALL"; break;
	case SSL_ERROR_ZERO_RETURN: s = "SSL_ERROR_ZERO_RETURN"; break;
	case SSL_ERROR_WANT_CONNECT: s = "SSL_ERROR_WANT_CONNECT"; break;
	}
	return(s);
}

int Ssl_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info )
{
	char buffer[LARGEBUFFER];
	struct stat statb;
	int tempfd = -1, len;
	int status = 0;
	double size;
	SSL_CTX *ctx = 0;
	SSL *ssl = 0;

	errmsg[0] = 0;

	if(DEBUGL1)Dump_line_list("Ssl_send: info", info );
	DEBUG1("Ssl_send: sending on socket %d", *sock );

	if( SSL_Initialize_ctx(&ctx, errmsg, errlen ) ){
		status = JFAIL;
		goto t_error;
	}
	if( Open_SSL_connection( *sock, ctx, &ssl, info, errmsg, errlen ) ){
		status = JFAIL;
		goto t_error;
	}

	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		SNPRINTF(errmsg, errlen)
			"Ssl_send: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto t_error;
	}
	size = statb.st_size;
	SNPRINTF(buffer,sizeof(buffer)) "%0.0f\n", size );
	DEBUG1("Ssl_send: writing '%s'", buffer );
	if( Write_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, strlen(buffer), errmsg, errlen) ){
		status = JFAIL;
		goto t_error;
	}

	DEBUG1("Ssl_send: starting send");
	while( (len = read( tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("Ssl_send: file information '%s'", buffer );
		if( Write_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, len, errmsg, errlen) ){
			status = JFAIL;
			goto t_error;
		}
	}
	if( len < 0 ){
		SNPRINTF(errmsg, errlen)
			"Ssl_send: read from '%s' failed - %s", tempfile, Errormsg(errno) );
		status = JFAIL;
		goto t_error;
	}

	close(tempfd); tempfd = -1;
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		Errorcode = JABORT;
		FATAL(LOG_ERR)
			"Ssl_send: open '%s' for write failed - %s",
			tempfile, Errormsg(errno) );
	}

	DEBUG1("Ssl_send: sent file" );

	DEBUG1("Ssl_send: getting read size");
	if( Gets_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, sizeof(buffer), errmsg, errlen) ){
		status = JFAIL;
		goto error;
	}
	size = strtod( buffer, 0 );

	DEBUG1("Ssl_send: starting read of %0.0f bytes", size);

	while( size > 0 ){
		len = sizeof(buffer)-1;
		if( len > size ) len = size;
		if( Read_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, &len, errmsg, errlen) ){
			status = JFAIL;
			goto error;
		}
		buffer[len] = 0;
		DEBUG1("Ssl_send: rcvd %d bytes '%s'", len, buffer);
		if( write(tempfd,buffer,len) != len ){
			SNPRINTF(errmsg, errlen)
				"Ssl_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
		size -= len;
	}
	DEBUG1("Ssl_send: finished read");
	close( tempfd ); tempfd = -1;
	goto done;

 t_error:
	DEBUG1("Ssl_send: t_error - status %d, errmsg '%s'", status, errmsg);
	close(tempfd); tempfd = -1;
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		Errorcode = JFAIL;
		FATAL(LOG_ERR)
			"Ssl_send: open '%s' for write failed - %s",
			tempfile, Errormsg(errno) );
	}

 error:

	DEBUG1("Ssl_send: error - status %d, errmsg '%s'", status, errmsg);
	Write_fd_str(tempfd, errmsg );
	Write_fd_str(tempfd, "\n" );

 done:
	DEBUG1("Ssl_send: done - status %d, errmsg '%s'", status, errmsg);
	if( ssl ){
		Close_SSL_connection( *sock, ssl );
	}
	if( ssl ) SSL_free( ssl );
	if( ctx ) Destroy_ctx( ctx );
	return(status);
}

int Ssl_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile )
{
	int tempfd, status, n, len;
	char buffer[LARGEBUFFER];
	struct stat statb;
	double size;
	SSL_CTX *ctx = 0;
	SSL *ssl = 0;

	DEBUGFC(DRECV1)Dump_line_list("Ssl_receive: info", info );
	DEBUGFC(DRECV1)Dump_line_list("Ssl_receive: header_info", header_info );
	/* do validation and then write 0 */
	tempfd = -1;
	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		SNPRINTF( errmsg, errlen) "Ssl_receive: ACK 0 write error - %s",
			Errormsg(errno) );
		goto error;
	}

	if( SSL_Initialize_ctx(&ctx, errmsg, errlen ) ){
		status = JFAIL;
		goto error;
	}
	if( Accept_SSL_connection( *sock, Send_job_rw_timeout_DYN, ctx, &ssl, header_info, errmsg, errlen ) ){
		status = JFAIL;
		goto error;
	}
	DEBUGFC(DRECV1)Dump_line_list("Ssl_receive: after accept info", header_info );

	/* open a file for the output */
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Ssl_receive: open of '%s' for write failed",
			tempfile );
	}

	DEBUGF(DRECV1)("Ssl_receive: getting read size");
	if( Gets_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, sizeof(buffer), errmsg, errlen) ){
		status = JFAIL;
		goto error;
	}
	size = strtod( buffer, 0 );
	DEBUGF(DRECV1)("Ssl_receive: read size '%s'", buffer );

	while( size > 0 ){
		len = sizeof(buffer);
		if( len > size ) len = size;
		if( Read_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, &len, errmsg, errlen) ){
			status = JFAIL;
			goto error;
		}
		DEBUGF(DRECV1)("Ssl_receive: rcvd '%d' '%s'", len, buffer );
		if( write( tempfd,buffer,len ) != len ){
			status = JFAIL;
			LOGERR_DIE(LOG_ERR)
				"Ssl_receive: bad write to '%s' - '%s'",
					tempfile, Errormsg(errno) );
		}
		size -= len;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV1)("Ssl_receive: end read" );

	/*** at this point you can check the format of the received file, etc.
     *** if you have an error message at this point, you should write it
	 *** to the socket,  and arrange protocol can handle this.
	 ***/

	status = Do_secure_work( jobsize, from_server, tempfile, header_info );

	/*** if an error message is returned, you should write this
	 *** message to the tempfile and the proceed to send the contents
	 ***/
	DEBUGF(DRECV1)("Ssl_receive: doing reply" );
	if( (tempfd = Checkread(tempfile,&statb)) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Ssl_receive: reopen of '%s' for write failed",
			tempfile );
	}
	size = statb.st_size;
	SNPRINTF(buffer,sizeof(buffer)) "%0.0f\n", size );
	DEBUG1("Ssl_receive: writing '%s'", buffer );
	if( Write_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, strlen(buffer), errmsg, errlen) ){
		status = JFAIL;
		goto error;
	}

	while( (n = read(tempfd, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV1)("Ssl_receive: sending '%d' '%s'", n, buffer );
		if( Write_SSL_connection( Send_job_rw_timeout_DYN, ssl, buffer, n, errmsg, errlen) ){
			status = JFAIL;
			goto error;
		}
	}
	if( n < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Ssl_receive: read of '%s'",
			tempfile );
	}
	DEBUGF(DRECV1)("Ssl_receive: reply done" );
	if( tempfd > 0 ) close( tempfd ); tempfd = -1;
	return( status );

 error:

	FATAL(LOG_ERR) "Ssl_receive: %s", errmsg );
	return(status);
}

#endif

