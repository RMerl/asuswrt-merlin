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
 * $Id: ssl_auth.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _SSL_AUTH_H_
#define _SSL_AUTH_H_ 1

#include <openssl/ssl.h>
#include <openssl/err.h>


/* PROTOTYPES */
char *Set_ERR_str( char *header, char *errmsg, int errlen );
int SSL_Initialize_ctx(
	SSL_CTX **ctx_ret,
	char *errmsg, int errlen );
void Destroy_ctx(SSL_CTX *ctx);
int Open_SSL_connection( int sock, SSL_CTX *ctx, SSL **ssl_ret,
	struct line_list *info, char *errmsg, int errlen );
int Accept_SSL_connection( int sock, int timeout, SSL_CTX *ctx, SSL **ssl_ret,
	struct line_list *info, char *errmsg, int errlen );
int Write_SSL_connection( int timeout, SSL *ssl, char *buffer, int len,
	char *errmsg, int errlen );
int Gets_SSL_connection( int timeout, SSL *ssl, char *inbuffer, int len,
	char *errmsg, int errlen );
int Read_SSL_connection( int timeout, SSL *ssl, char *inbuffer, int *len,
	char *errmsg, int errlen );
int Close_SSL_connection( int sock, SSL *ssl );
const char * Error_SSL_name( int i );
int Ssl_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int Ssl_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile );

#endif
