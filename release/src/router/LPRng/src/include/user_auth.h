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
 * $Id: user_auth.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _USER_AUTH_H_
#define _USER_AUTH_H_ 1

/***************************************************************
 * Security stuff - needs to be in a common place
 ***************************************************************/

struct security;
typedef int (*CONNECT_PROC)( struct job *job, int *sock,
	int transfer_timeout,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info );

typedef int (*SEND_PROC)( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info );

typedef int (*GET_REPLY_PROC)( struct job *job, int *sock,
	int transfer_timeout,
	char *error, int errlen,
	struct security *security, struct line_list *info );

typedef int (*SEND_DONE_PROC)( struct job *job, int *sock,
	int transfer_timeout,
	char *error, int errlen,
	struct security *security, struct line_list *info );

typedef int (*ACCEPT_PROC)(
	int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	char *error, int errlen,
	struct line_list *info, struct line_list *header_info,
	struct security *security );

typedef int (*RECEIVE_PROC)(
	int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *error, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile );

typedef int (*REPLY_PROC)(
	int *sock, char *error, int errlen,
	struct line_list *info, struct line_list *header_info,
	struct security *security );

typedef int (*RCV_DONE_PROC)( int *sock,
	char *error, int errlen,
	struct line_list *info, struct line_list *header_info,
	struct security *security );

struct security {
	char *name;				/* authentication name */
	char *config_tag;		/* use this tag for configuration information */
	int auth_flags;				/* flags */
#define IP_SOCKET_ONLY	1 /* use TCP/IP socket only */
	CONNECT_PROC client_connect;	/* client to server connection, talk to verify */
	SEND_PROC    client_send;		/* client to server authenticate transfer, talk to transfer */
	ACCEPT_PROC server_accept;		/* server accepts the connection, sets up transfer */
	RECEIVE_PROC server_receive;	/* server to client, receive from client */
};

extern struct security SecuritySupported[];

/* PROTOTYPES */
int Test_connect( struct job *job, int *sock,
	int transfer_timeout,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int Test_accept( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	char *errmsg, int errlen,
	struct line_list *info, struct line_list *header_info,
	struct security *security );
int Test_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int Test_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile );
int md5_send( int *sock, int transfer_timeout, char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int md5_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile );
int Pgp_get_pgppassfd( struct line_list *info, char *error, int errlen );
int Pgp_decode(struct line_list *info, char *tempfile, char *pgpfile,
	struct line_list *pgp_info, char *buffer, int bufflen,
	char *error, int errlen, char *esc_to_id, struct line_list *from_info,
	int *pgp_exit_code, int *not_a_ciphertext );
int Pgp_encode(struct line_list *info, char *tempfile, char *pgpfile,
	struct line_list *pgp_info, char *buffer, int bufflen,
	char *error, int errlen, char *esc_from_id, char *esc_to_id,
	int *pgp_exit_code );
int Pgp_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info );
int Pgp_receive( int *sock,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile );

#endif
