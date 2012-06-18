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
"$Id: sserver.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";

/*
 * 
 * Simple authentictor test for kerberos.
 *  Based on the SSERVER code in the Kerberos5 distibution.
 *  See Copyrights, etc.
 */

#include "lp.h"
#include "krb5_auth.h"
#include "child.h"
#include "fileopen.h"

char *msg[] = {
	"[-D options] [-p port] [-s service] [-S keytab] file",
	"  -D turns debugging on",
	0
};

char *progname;

void
usage()
{   
	int i;
	FPRINTF(STDOUT, "usage: %s %s\n", progname, msg[0]);
	for( i = 1; msg[i]; ++i ){
		FPRINTF(STDOUT, "%s\n", msg[i]);
	}
}  



int
main(int argc, char *argv[])
{
	struct sockaddr_in peername;
	int namelen = sizeof(peername);
	int sock = -1;          /* incoming connection fd */
	short port = 1234;     /* If user specifies port */
	extern int opterr, optind, getopt(), atoi();
	extern char * optarg;
	int ch, fd = -1, len;
	int on = 1;
	int acc;
	struct sockaddr_in sin;
	char auth[128];
	char *client = 0;
	char err[128];
	char *file;
	char buffer[SMALLBUFFER];
	struct stat statb;

	progname = argv[0];
	setlinebuf(STDOUT);

	/*
	 * Parse command line arguments
	 *  
	 */
	opterr = 0;
	while ((ch = getopt(argc, argv, "D:p:S:s:")) != EOF)
	switch (ch) {
	case 'D': Parse_debug(optarg,1); break;
	case 'p': port = atoi(optarg); break;
	case 's': Set_DYN(&Kerberos_service_DYN,optarg); break;
	case 'S': Set_DYN(&Kerberos_keytab_DYN,optarg); break;
	default: usage(); exit(1); break;
	}
	Spool_file_perms_DYN = 0600;

	if( argc - optind != 1 ){
		usage();
		exit(1);
	}
	file = argv[optind++];

	if( Kerberos_keytab_DYN == 0 ) Set_DYN(&Kerberos_keytab_DYN, "/etc/lpd.keytab");
	if( Kerberos_service_DYN == 0 ) Set_DYN(&Kerberos_service_DYN,"lpr");
	if( port == 0 ){
		FPRINTF( STDOUT, "bad port specified\n" );
		exit(1);
	}
	/*
	 * If user specified a port, then listen on that port; otherwise,
	 * assume we've been started out of inetd. 
	 */

	remote_principal_krb5( Kerberos_service_DYN, 0, auth, sizeof(auth));
	FPRINTF(STDOUT, "server principal '%s'\n", auth );

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		FPRINTF(STDOUT, "socket: %s\n", Errormsg(errno));
		exit(3);
	}
#ifdef WINDOW_1
int windowsize=1024;
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize));
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "sserver: main\n");
fclose(aaaaaa);
#endif
	Max_open(sock);
	/* Let the socket be reused right away */
	(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
			  sizeof(on));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin))) {
		FPRINTF(STDOUT, "bind: %s\n", Errormsg(errno));
		exit(3);
	}
	if (listen(sock, 1) == -1) {
		FPRINTF(STDOUT, "listen: %s", Errormsg(errno));
		exit(3);
	}
	while(1){
		if ((acc = accept(sock, (struct sockaddr *)&peername,
				&namelen)) == -1){
			FPRINTF(STDOUT, "accept: %s\n", Errormsg(errno));
			exit(3);
		}

		err[0] = 0;
		auth[0] = 0;
		client = 0;
		if( server_krb5_auth( Kerberos_keytab_DYN, Kerberos_service_DYN, acc,
			&client, err, sizeof(err), file ) ){
			FPRINTF( STDOUT, "server_krb5_auth error '%s'\n", err );
			goto done;
		}
		SNPRINTF(buffer,sizeof(buffer))"client '%s'", client );
		FPRINTF(STDOUT,"%s\n",buffer);
		fd = Checkread( file, &statb );
		DEBUG1( "main: opened for write '%s', fd %d, size %ld",
			file, fd, (long)(statb.st_size) );
		if( fd < 0 ){
			SNPRINTF( err, sizeof(err))
				"file open failed: %s", Errormsg(errno));
			goto done;      
		}
		FPRINTF(STDOUT,"RECEVIED:\n");
		while( (len = read(fd, buffer,sizeof(buffer)-1)) > 0 ){
			write(1,buffer,len);
		}
		close(fd);
		fd = Checkwrite( file, &statb, O_WRONLY|O_TRUNC, 1, 0 );
		if( fd < 0 ){
			SNPRINTF( err, sizeof(err))
				"main: could not open for writing '%s' - '%s'", file,
					Errormsg(errno) );
			goto done;
		}
		SNPRINTF(buffer,sizeof(buffer))"credentials '%s'\n", client );
		Write_fd_str(fd,buffer);
		close(fd);
		if( server_krb5_status( acc, err, sizeof(err), file ) ){
			FPRINTF( STDOUT, "server_krb5_status error '%s'\n", err );
			goto done;
		}
 done:
		close(acc);
	}
	exit(0);
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

void send_to_logger( int sfd, int mfd, struct job *job, const char *header, char *msg ){;}
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

