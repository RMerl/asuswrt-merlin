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
"$Id: sclient.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


/*
 * Simple Kerberos client tester.
 * Derived from the Kerberos 5 SCLIENT code -
 *   Note carefully that there are NO MIT copyrights here.
 */

#include "lp.h"
#include "child.h"
#include "krb5_auth.h"

char *msg[] = {
	"[-D options] [-p port] [-s service] [-k keytab] [-P principal] host file",
	"  -D turns debugging on",
	0
};

char *progname;

void send_to_logger( int sfd, int mfd, struct job *job, const char *header, char *msg ){;}

void
usage()
{   
	int i;
	FPRINTF(STDERR, "usage: %s %s\n", progname, msg[0]);
	for( i = 1; msg[i]; ++i ){
		FPRINTF(STDERR, "%s\n", msg[i]);
	}
}  


int
main(argc, argv)
int argc;
char *argv[];
{
    struct hostent *host_ent;
    struct sockaddr_in sin;
    int sock;
	int port = 1234;
	char *host;
	int c;
	extern int opterr, optind, getopt();
	extern char * optarg;
	char buffer[SMALLBUFFER];
	char msg[SMALLBUFFER];
	char *file;
	char *keytab = 0;
	char *principal = 0;

	progname = argv[0];
	while( (c = getopt(argc, argv, "D:p:s:k:P:")) != EOF){
		switch(c){
		default: 
			FPRINTF(STDERR,"bad option '%c'\n", c );
			usage(progname); exit(1); break;
		case 'k': keytab = optarg; break;
		case 'D': Parse_debug(optarg,1); break;
		case 'p': port= atoi(optarg); break;
		case 's': Set_DYN(&Kerberos_service_DYN,optarg); break;
		case 'P': principal = optarg; break;
		}
	}
	if( argc - optind != 2 ){
		FPRINTF(STDERR,"missing host or file name\n" );
		usage(progname);
		exit(1);
	}
	host = argv[optind++];
	file = argv[optind++];

    /* clear out the structure first */
    (void) memset((char *)&sin, 0, sizeof(sin));
	if(Kerberos_service_DYN == 0 ) Set_DYN(&Kerberos_service_DYN,"lpr");
	if( principal ){
		FPRINTF(STDERR, "using '%s'\n", principal );
	} else {
		remote_principal_krb5( Kerberos_service_DYN, host, buffer, sizeof(buffer) );
		FPRINTF(STDERR, "default remote principal '%s'\n", buffer );
		principal = buffer;
	}

    /* look up the server host */
    host_ent = gethostbyname(host);
    if(host_ent == 0){
		FPRINTF(STDERR, "unknown host %s\n",host);
		exit(1);
    }

    /* set up the address of the foreign socket for connect() */
    sin.sin_family = host_ent->h_addrtype;
    (void) memcpy((char *)&sin.sin_addr, (char *)host_ent->h_addr,
		 sizeof(host_ent->h_addr));
	sin.sin_port = htons(port);

    /* open a TCP socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
#ifdef WINDOW_1
int windowsize=1024;
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize));
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "sclient: main\n");
fclose(aaaaaa);
#endif
	Max_open(sock);
    if( sock < 0 ){
		perror("socket");
		exit(1);
    }

    /* connect to the server */
    if( connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0 ){
		perror("connect");
		close(sock);
		exit(1);
    }

	buffer[0] = 0;
	if( client_krb5_auth( keytab, Kerberos_service_DYN, host,
		0, 0, 0, 0,
		sock, buffer, sizeof(buffer), file ) ){
		FPRINTF( STDERR, "client_krb5_auth failed: %s\n", buffer );
		exit(1);
	}
	fflush(STDOUT);
	fflush(STDERR);
	SNPRINTF(msg, sizeof(msg))"starting read from %d\n", sock );
	write(1,msg, safestrlen(msg) );
	while( (c = read( sock, buffer, sizeof(buffer) ) ) > 0 ){
		buffer[c] = 0;
		SNPRINTF(msg, sizeof(msg))
			"read %d from fd %d '%s'\n", c, sock, buffer );
		write( 1, msg, safestrlen(msg) );
	}
	SNPRINTF(msg, sizeof(msg))
		"last read status %d from fd %d\n", c, sock );
	write( 1, msg, safestrlen(msg) );
    return(0);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
void setstatus (struct job *job,char *fmt,...)
#else
void setstatus (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) VSNPRINTF( msg, sizeof(msg)-2) fmt, ap);
		strcat( msg,"\n" );
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}


/* VARARGS2 */
#ifdef HAVE_STDARGS
void setmessage (struct job *job,const char *header, char *fmt,...)
#else
void setmessage (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt, *header;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (header, char * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) VSNPRINTF( msg, sizeof(msg)-2) fmt, ap);
		strcat( msg,"\n" );
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}

