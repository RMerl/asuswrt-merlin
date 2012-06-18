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
/*
 * tcpsend [-t timeout] host port [files]
 *
 * open a TCP/IP connection to port on host and send files
 *  Super lightweight LPR ???
 * tcpsend.c,v 1.1 2000/10/14 21:11:23 papowell Exp
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

char *Prog = "???";

char *msg[] = {
	"use: %s [-t timeout] host port [files]",
	"  -t timeout  - try connecting for timeout seconds",
	0
};

int max_try = 10;

void do_send( int sock, int fd, char *name );
void Set_linger( int sock, int n );

void usage(void)
{
	int i;
	fprintf( stderr, msg[0], Prog );
	fprintf( stderr, "\n");
	for( i = 1; msg[i]; ++i ){
		fprintf( stderr, "%s\n", msg[i] );
	}
	exit(1);
}

char buffer[10*1024];

int main( int argc, char **argv )
{
	int n, try;
	int sock;	         /* socket */
	char *s, *host, *port;
	struct sockaddr_in dest_sin;     /* inet socket address */
	struct hostent *host_ent = 0;

	(void)signal( SIGPIPE, SIG_IGN );
	if( argv[0] ) Prog = argv[0];
	if( (s = strrchr(Prog,'/')) ){
		Prog = s+1;
	}
	while( (n = getopt(argc, argv, "t:")) != EOF ){
		switch(n){
		case 't': max_try = atoi(optarg); break;
		default: usage(); break;
		}
	}
	if( argc - optind < 2 ) usage();


	sock = -1;
	memset(&dest_sin, 0, sizeof (dest_sin));

	host = argv[optind++];
	port = argv[optind++];

	if( (host_ent = gethostbyname( host )) ){
		dest_sin.sin_family = host_ent->h_addrtype;
		if( host_ent->h_length > sizeof( dest_sin.sin_addr ) ){
			fprintf(stderr,"%s: address length wrong\n", Prog);
			exit(1);
		}
		memcpy(&dest_sin.sin_addr, host_ent->h_addr_list[0], host_ent->h_length );
	} else 
		/* if( inet_pton( AF_INET, host, &dest_sin.sin_addr ) != 1 ){ */
		if( (dest_sin.sin_addr.s_addr = inet_addr( host )) == -1 ){
		fprintf(stderr,"%s: cannot find address for '%s'\n", Prog, host );
		exit(1);
	}
	n = atoi(port);
	if( n <= 0 ){
		fprintf(stderr,"%s: bad port '%s'\n", Prog, port );
		exit(1);
	}
	dest_sin.sin_port = htons(n);
    /* we get the printable from the socket address */
#if 0
    fprintf(stderr, "connect to '%s',port %d\n",
		inet_ntoa( dest_sin.sin_addr )),
		(int) ntohs(dest_sin.sin_port) );
#endif
	try = 0;
	do{ 
		if( sock != -1 ) close(sock );
		sock = -1;
		sock = socket(AF_INET, SOCK_STREAM, 0 );
#ifdef WINDOW_1
int windowsize=1024;
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize));
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "tcp_send: main\n");
fclose(aaaaaa);
#endif
		if( sock == -1 ){
			fprintf(stderr,"%s: socket() failed '%s'\n", Prog, strerror(errno) );
			exit(1);
		}
		n = connect(sock, (void *)&dest_sin, sizeof(dest_sin) );
		if( n == -1 && ++try < max_try ){
			sleep(1);
		} else {
			break;
		}
	}while( n == -1 );
	if( n == -1 ){
		fprintf(stderr, "%s: connect to '%s',port %d failed after %d tries - %s\n",
			Prog,
			inet_ntoa( dest_sin.sin_addr),
			(int) ntohs(dest_sin.sin_port), try, strerror(errno) );
		exit(1);
	}
	if( optind == argc ){
		do_send( sock, 0, "stdin" );
	} else for(; optind < argc; ++optind ){
		if( (n = open(argv[optind], O_RDONLY, 0)) == -1 ){
			fprintf(stderr, "%s: cannot open '%s' - %s\n",
				Prog, argv[optind], strerror(errno) );
			exit(1);
		}
		do_send( sock, n, argv[optind] );
		close(n);
	}
	/* we shut down the connection */
	shutdown(sock,1);
	while( (n = read(sock,&buffer,sizeof(buffer))) > 0 );
	close(sock);
	return(0);
}

void do_send( int sock, int fd, char *name )
{
	int cnt, n, i;
	while( (n = read(fd,buffer,sizeof(buffer))) > 0 ){
		for( cnt = i = 0; cnt >= 0 && i < n; i += cnt ){
			cnt = write(sock,buffer+i,n-i);
		}
		if( cnt < 0 ){
			fprintf(stderr, "%s: cannot write to remote host - %s\n",
				Prog, strerror(errno) );
			exit(1);
		}
	}
	if( n < 0 ){
		fprintf(stderr, "%s: cannot read from '%s' - %s\n",
			Prog, name, strerror(errno) );
		exit(1);
	}
}
