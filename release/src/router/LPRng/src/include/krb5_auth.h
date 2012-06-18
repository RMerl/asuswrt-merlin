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
 * $Id: krb5_auth.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _KRB5_AUTH_H
#define _KRB5_AUTH_H 1

/* PROTOTYPES */
int server_krb5_status( int sock, char *err, int errlen, char *file );
int client_krb5_auth( char *keytabfile, char *service, char *host,
	char *server_principal,
	int options, char *life, char *renew_time,
	int sock, char *err, int errlen, char *file );
int remote_principal_krb5( char *service, char *host, char *err, int errlen );
char *krb4_err_str( int err );
int Send_krb4_auth( struct job *job, int *sock,
	int connect_timeout, char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int Receive_k4auth( int *sock, char *input );
int Krb5_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile );
int Krb5_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info );

#endif
