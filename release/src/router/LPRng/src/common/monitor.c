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
"$Id: monitor.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "linelist.h"
#include "getopt.h"
#include "linksupport.h"
#include "getqueue.h"
/**** ENDINCLUDE ****/

/*
 * Monitor for TCP/UDP data
 *  Opens a UDP or TCP socket and waits for data to be sent to it.
 *
 *  monitor [-t] [-u] [port]
 *   port is an integer number or a service name in the services database
 *   default is to use UDP.
 */


extern int errno;

int udp_open( int port );
int tcp_open( int port );

void Decode( char *in );

char buffer[1024*64];
int use_tcp = 1;
int use_udp = 1;
int port_num = 2001;
int udp_fd = -1;
int tcp_fd = -1;
fd_set readfds;
fd_set testfds;
int debug;

char *prog = "???";

/*****************************************************************
 * Command line options and Debugging information
 * Getopt is a modified version of the standard getopt(3) command
 *  line parsing routine. See getopt.c for details
 *****************************************************************/

/* use this before any error printing, sets up program Name */

struct info {
	char *buffer;
	int len;
	int max_len;
};

struct info *inbuffers;
int max_in_buffers;

void Add_buffer( int n )
{
	int len = max_in_buffers, count;

	if(debug)FPRINTF(STDERR, "Add_buffer: start n %d, inbuffers 0x%lx, max_in_buffers %d\n",
		n, Cast_ptr_to_long(inbuffers), max_in_buffers );
	if( max_in_buffers <= n ){
		max_in_buffers = n+1;
		count = sizeof(inbuffers[0])*max_in_buffers;
		inbuffers = realloc( inbuffers, count );
		if( inbuffers == 0 ){
			FPRINTF(STDERR,"Add_buffer: realloc %d failed\n", n );
			exit(1);
		}
		for( count = len; count < max_in_buffers; ++count ){
			memset(inbuffers+count,0,sizeof(inbuffers[0]));
		}
	}
	if(debug)FPRINTF(STDERR,"Add_buffer: end n %d, inbuffers 0x%lx, max_in_buffers %d\n",
		n, Cast_ptr_to_long(inbuffers), max_in_buffers );
}

void Clear_buffer( int n )
{
	struct info *in;
	if(debug)FPRINTF(STDERR,"Clear_buffer: n %d\n", n );
	Add_buffer( n );
	in = inbuffers+n;
	in->len = 0;
}

struct info *Save_outbuf_len( int n,  char *str, int len )
{
	struct info *in;

	if(debug)FPRINTF(STDERR,"Save_outbuf_len: n %d, len %d\n", n, len );
	Add_buffer(n);
	in = inbuffers+n;
	if(debug)FPRINTF(STDERR,
		"Save_outbuf_len: start inbuffers 0x%lx, in 0x%lx, buffer 0x%lx, len %d, max_len %d\n",
		Cast_ptr_to_long(inbuffers), Cast_ptr_to_long(in),
		Cast_ptr_to_long(in->buffer), in->len, in->max_len );
	if( len + in->len >= in->max_len ){
		in->max_len += len;
		in->buffer = realloc( in->buffer, in->max_len+1 );
		if( in->buffer == 0 ){
			FPRINTF(STDERR,"Put_oubuf_len: realloc %d failed\n", in->max_len );
			exit(1);
		}
	}
	memcpy(in->buffer+in->len, str, len+1 );
	in->len += len;
	if(debug)FPRINTF(STDERR,
		"Save_outbuf_len: start inbuffers 0x%lx, in 0x%lx, buffer 0x%lx, len %d, max_len %d\n",
		Cast_ptr_to_long(inbuffers), Cast_ptr_to_long(in),
		Cast_ptr_to_long(in->buffer), in->len, in->max_len );
	return( in );
}

void usage(void)
{
	char *s;

	if( (s = safestrrchr( prog, '/')) ){
		prog  = s+1;
	}
	FPRINTF( STDERR, "usage: %s [-u] [-t] [port]\n", prog );
	FPRINTF( STDERR, "  -u = use UDP\n" );
	FPRINTF( STDERR, "  -t = use TCP (default)\n" );
	FPRINTF( STDERR, "  -d = debug\n" );
	FPRINTF( STDERR, "  port = port to use (%d default)\n", port_num  );
	exit(1);
}
	


int main(int argc, char *argv[] )
{
	int n, i, c, err, max_port = 0;
	char *portname, *s;
	struct servent *servent;
	struct info *in;

	prog = argv[0];
	Opterr = 1;
	while( (n = Getopt(argc, argv, "dut")) != EOF ){
		switch(n){
		default:  usage(); break;
		case 'u': use_udp = !use_udp; break;
		case 't': use_tcp = !use_tcp; break;
		case 'd': debug = 1; break;
		}
	}
	i = argc - Optind;
	if( i > 1 ) usage();
	if( i == 1 ){
		portname = argv[Optind];
		n = atoi( portname );
		if( n <= 0 ){
			servent = getservbyname( portname, "udp" );
			if( servent ){
				n = ntohs( servent->s_port );
			}
		}
		if( n <= 0 ){
			FPRINTF( STDERR, "udp_open: bad port number '%s'\n",portname );
			usage();
		}
		port_num = n;
	}

	if( !use_tcp && !use_udp ) use_udp = 1;
	if( debug ){
		FPRINTF(STDERR,"monitor: udp %d, tcp %d, port %d\n",
			use_udp, use_tcp, port_num );
	}

	max_port = 0;
	FD_ZERO( &readfds );
	if( use_udp && (udp_fd = udp_open( port_num )) >= 0){
		if( debug ) FPRINTF(STDERR,"monitor: udp port %d\n", udp_fd );
		FD_SET(udp_fd, &readfds);
		if( udp_fd >= max_port ) max_port = udp_fd+1;
	}
	if( use_tcp && (tcp_fd = tcp_open( port_num )) >= 0){
		if( debug ) FPRINTF(STDERR,"monitor: tcp port %d\n", tcp_fd );
		FD_SET(tcp_fd, &readfds);
		if( tcp_fd >= max_port ) max_port = tcp_fd+1;
	}
	if( debug ){
		FPRINTF(STDERR,"monitor: max_port %d\n", max_port );
		for( i = 0; i < max_port; ++i ){
			if( FD_ISSET(i, &readfds) ){
				FPRINTF(STDERR,"monitor: initial on %d\n", i );
			}
		}
	}


	while(1){
		FD_ZERO( &testfds );
		for( i = 0; i < max_port; ++i ){
			if( FD_ISSET(i, &readfds) ){
				if( debug ) FPRINTF(STDERR,"monitor: waiting on %d\n", i );
				FD_SET(i, &testfds);
			}
		}
		if( debug ) FPRINTF(STDERR,"monitor: starting wait, max %d\n", i );
		n = select( i,
			FD_SET_FIX((fd_set *))&testfds,
			FD_SET_FIX((fd_set *))0, FD_SET_FIX((fd_set *))0,
			(struct timeval *)0 );
		err = errno;
		if( debug ) FPRINTF(STDERR,"monitor: select returned %d\n", n );
		if( n < 0 ){
			FPRINTF( STDERR, "select error - %s\n", Errormsg(errno) );
			if( err != EINTR ) break;
		}
		if( n > 0 ) for( i = 0; i < max_port; ++i ){
			if( FD_ISSET(i, &testfds) ){
				if( debug ) FPRINTF(STDERR,"monitor: input on %d\n", i );
				if( i == tcp_fd ){
#if defined(HAVE_SOCKLEN_T)
					socklen_t len;
#else
					int len;
#endif
					struct sockaddr_in sinaddr;
					len = sizeof( sinaddr );
					i = accept( tcp_fd, (struct sockaddr *)&sinaddr, &len );
					if( i < 0 ){
						FPRINTF( STDERR, "accept error - %s\n",
							Errormsg(errno) );
						continue;
					}
					FPRINTF( STDOUT, "connection from %s\n",
						inet_ntoa( sinaddr.sin_addr ) );
					if( i >= max_port ) max_port = i+1;
					FD_SET(i, &readfds);
				} else {
					c = read( i, buffer, sizeof(buffer)-1 );
					if( c == 0 ){
						/* closed connection */
						FPRINTF(STDOUT, "closed connection %d\n", i );
						close( i );
						FD_CLR(i, &readfds );
						Clear_buffer(i);
					} else if( c > 0 ){
						buffer[c] = 0;
						if(debug)FPRINTF( STDOUT, "recv port %d: %s\n", i, buffer );
						Add_buffer(i);
						in = Save_outbuf_len( i, buffer, c );
						while( (s = safestrchr(in->buffer,'\n')) ){
							*s++ = 0;
							Decode(in->buffer);
							memmove(in->buffer,s, safestrlen(s)+1 );
							in->len = safestrlen(in->buffer);
						}
					} else {
						FPRINTF( STDERR, "read error - %s\n",
							Errormsg(errno) );
						close( i );
						FD_CLR(i, &readfds );
					}
				}
			}
		}
	}
	return(0);
}

void Decode( char *in )
{
	struct line_list l, cf, info;
	char *s, *t, *header, *value;
	int i;

	Init_line_list(&l);
	Init_line_list(&cf);
	Init_line_list(&info);

	FPRINTF(STDOUT,"****\n");
	if( debug )FPRINTF(STDOUT, "Decode: %s\n", in );
	if((s = safestrpbrk(in,Value_sep)) ){
		*s++ = 0;
		Unescape(s);
		Free_line_list(&l);
		Split(&l,s,Line_ends,1,Value_sep,1,1,1,0);
		for( i = 0; i < l.count; ++i ){
			t = l.list[i];
			if( debug || safestrncasecmp(t,VALUE,5) ){
				FPRINTF(STDOUT,"%s\n", t );
			}
		}
		s = Find_str_value(&l,VALUE,Value_sep);
		if( s ) Unescape(s);
		if( !safestrcasecmp(in,TRACE) ){
			FPRINTF(STDOUT,"TRACE: '%s'\n", s );
		} else if( !safestrcasecmp(in,UPDATE) ){
			FPRINTF(STDOUT,"UPDATE: '%s'\n", s );
			Split(&info,s,Line_ends,0,0,0,0,0,0);
			for( i = 0; i < info.count; ++i ){
				header = info.list[i];
				if( (value = safestrchr(header,'=')) ) *value++ = 0;
				Unescape(value);
				FPRINTF(STDOUT," [%d] %s='%s'\n", i, header, value );
			}
		} else if( !safestrcasecmp(in,PRSTATUS) ){
			FPRINTF(STDOUT,"PRSTATUS: '%s'\n", s );
		} else if( !safestrcasecmp(in,QUEUE) ){
			FPRINTF(STDOUT,"QUEUE: '%s'\n", s );
			Split(&info,s,Line_ends,0,0,0,0,0,0);
			for( i = 0; i < info.count; ++i ){
				header = info.list[i];
				if( (value = safestrchr(header,'=')) ) *value++ = 0;
				Unescape(value);
				FPRINTF(STDOUT," [%d] %s='%s'\n", i, header, value );
			}
			Free_line_list(&info);
		} else if( !safestrcasecmp(in,DUMP) ){
			FPRINTF(STDOUT,"DUMP:\n");
			Split(&info,s,Line_ends,0,0,0,0,0,0);
			for( i = 0; i < info.count; ++i ){
				header = info.list[i];
				if( (value = safestrchr(header,'=')) ) *value++ = 0;
				Unescape(value);
				if(debug) FPRINTF(STDOUT," [%d] %s='%s'\n", i, header, value );
				if( !safestrcasecmp(header,QUEUE) ){
					FPRINTF(STDOUT," EXTRACT QUEUE '%s'\n",value);
				} else if( !safestrcasecmp(header,UPDATE) ){
					FPRINTF(STDOUT," EXTRACT UPDATE '%s'\n",value);
				} else {
					FPRINTF(STDOUT," EXTRACT '%s' '%s'\n",header, value);
				}
			}
			Free_line_list(&info);
		} else {
			FPRINTF(STDOUT,"%s: '%s'\n", in, s );
		}
	}
	Free_line_list(&l);
	Free_line_list(&cf);
	Free_line_list(&info);
	FPRINTF(STDOUT,"\n");
}
int udp_open( int port )
{
	int i, fd, err;
	struct sockaddr_in sinaddr;

	sinaddr.sin_family = AF_INET;
	sinaddr.sin_addr.s_addr = INADDR_ANY;
	sinaddr.sin_port = htons( port );

	fd = socket( AF_INET, SOCK_DGRAM, 0 );
	Max_open(fd);
	err = errno;
	if( fd < 0 ){
		FPRINTF(STDERR,"udp_open: socket call failed - %s\n", Errormsg(err) );
		return( -1 );
	}
	i = -1;
	i = bind( fd, (struct sockaddr *) & sinaddr, sizeof (sinaddr) );
	err = errno;

	if( i < 0 ){
		FPRINTF(STDERR,"udp_open: bind to '%s port %d' failed - %s\n",
			inet_ntoa( sinaddr.sin_addr ), ntohs( sinaddr.sin_port ),
			Errormsg(errno) );
		close(fd);
		fd = -1;
	}
	if( fd == 0 ){
		fd = dup(fd);
		if( fd < 0 ){
			FPRINTF(STDERR,"udp_open: dup failed - %s\n",
				Errormsg(errno) );
			close(fd);
			fd = -1;
		}
	}
	return( fd );
}


int tcp_open( int port )
{
	int i, fd, err;
	struct sockaddr_in sinaddr;

	sinaddr.sin_family = AF_INET;
	sinaddr.sin_addr.s_addr = INADDR_ANY;
	sinaddr.sin_port = htons( port );

	fd = socket( AF_INET, SOCK_STREAM, 0 );
#ifdef WINDOW_1
int windowsize=1024;
setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize));
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, " monitor: tcp_send\n");
fclose(aaaaaa);
#endif
	Max_open(fd);
	err = errno;
	if( fd < 0 ){
		FPRINTF(STDERR,"tcp_open: socket call failed - %s\n", Errormsg(err) );
		return( -1 );
	}
	i = Link_setreuse( fd );
	if( i >= 0 ) i = bind( fd, (struct sockaddr *) & sinaddr, sizeof (sinaddr) );
	if( i >= 0 ) i = listen( fd, 10 );
	err = errno;

	if( i < 0 ){
		FPRINTF(STDERR,"tcp_open: connect to '%s port %d' failed - %s\n",
			inet_ntoa( sinaddr.sin_addr ), ntohs( sinaddr.sin_port ),
			Errormsg(errno) );
		close(fd);
		fd = -1;
	}
	if( fd == 0 ){
		fd = dup(fd);
		if( fd < 0 ){
			FPRINTF(STDERR,"tcp_open: dup failed - %s\n",
				Errormsg(errno) );
			close(fd);
			fd = -1;
		}
	}
	return( fd );
}
