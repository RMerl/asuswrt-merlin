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
 * $Id: permission.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _PERMISSION_H_
#define _PERMISSION_H_ 1

/***************************************************************************
 * Permissions keywords
 ***************************************************************************/

#define P_REJECT		-1
#define P_ACCEPT		1
#define P_NOT			2	/* invert test condition */
#define P_SERVICE		3	/* Service listed below */
#define P_USER			4	/* USER field from control file (LPR) or command */
							/* if a command, the user name is sent with command */
#define P_HOST			5	/* HOST field from control file */
							/* if not a printing operation, then host name
							sent with command */
#define P_IP			6	/* IP address of HOST */
#define P_PORT			7	/* remote connect */
#define P_REMOTEHOST	8	/* remote end of connnection host name */
							/* if printing, has the same value as HOST */
#define P_REMOTEIP		9	/* remote end of connnection IP address */
							/* if printing, has the same value as IP */
#define P_PRINTER		10	/* printer */
#define P_DEFAULT		11
#define P_FORWARD		12	/* forward - REMOTE IP != IP */
#define P_SAMEHOST		13	/* same host - REMOTE IP == IP */
#define P_SAMEUSER		14	/* remote user name on command line == user in file */
#define P_CONTROLLINE	15	/* line from control file */
#define P_GROUP	 		16	/* user is in named group - uses getpwname() */
#define P_SERVER	 	17	/* request is from the server */
#define P_REMOTEUSER 	18	/* USER from control information */
#define P_REMOTEGROUP	19	/* remote user is in named group - uses getpwname() */
#define P_IFIP			20	/* interface IP address */
#define P_LPC			21	/* LPC operations */
#define P_AUTH			22	/* authentication type - USER, SERVER, NONE */
#define P_AUTHTYPE		23	/* authentication type */
#define P_AUTHUSER		24	/* authentication user name */
#define P_AUTHFROM		25	/* from client or name */
#define P_AUTHSAMEUSER	26	/* from same authenticated user name */
#define P_AUTHJOB		27	/* job has authentication */
#define P_REMOTEPORT	28	/* alias for PORT */
#define P_UNIXSOCKET	29	/* connection via unixsocket - localhost + port 0 */
#define P_AUTHCA		30	/* Certifying authority */

/*
 * First character of protocol to letter mappings
 */

#define STARTPR     'P'  /* 1  - from lPc */
#define RECVJOB     'R'  /* 2  - from lpR, connection for printer */
#define TRANSFERJOB 'T'  /* 2  - from lpR, user information in job */
#define SHORTSTAT   'Q'  /* 3  - from lpQ */
#define LONGSTAT    'Q'  /* 4  - from lpQ */
#define REMOVEJOB   'M'  /* 5  - from lprM */
#define CONNECTION  'X'  /* connection from remote host */

struct perm_check {
	const char *user;				/* USER field from control file */
							/* or REMOTEUSER from command line */
	const char *remoteuser;		/* remote user name sent on command line */
							/* or USER field if no command line */
	struct host_information *host;	/* HOST field from control file */
							/* or REMOTEHOST if no control file */
	struct host_information *remotehost;/* remote HOST name making connection */
							/* or HOST if no control file */
	int	port;				/* port for remote connection */
	const char *printer;			/* printer name */
	// struct sockaddr addr;	/* IF address information */
	int unix_socket;		/* connection via unix socket */
	int service;			/* first character service */
	const char *lpc;				/* lpc operation */

	const char *authtype;			/* authentication type */
	const char *authfrom;			/* authentication from */
	const char *authuser;			/* user from */
	const char *authca;				/* authentication certifying authority */
};

EXTERN struct perm_check Perm_check;

/* PROTOTYPES */
char *perm_str( int n );
int perm_val( char *s );
int Perms_check( struct line_list *perms, struct perm_check *check,
	struct job *job, int job_check );
int match( struct line_list *list, const char *str, int invert );
int match_host( struct line_list *list, struct host_information *host,
	int invert );
int portmatch( char *val, int port );
int match_range( struct line_list *list, int port, int invert );
int match_char( struct line_list *list, int value, int invert );
int match_group( struct line_list *list, const char *str, int invert );
int ingroup( char *group, const char *user );
void Dump_perm_check( char *title,  struct perm_check *check );
void Perm_check_to_list( struct line_list *list, struct perm_check *check );

#endif
