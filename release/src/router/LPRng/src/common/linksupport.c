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
"$Id: linksupport.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/***************************************************************************
 * MODULE: Link_support.c
 ***************************************************************************
 * Support for the inter-machine communications
 *
 * int Link_port_num(char *)
 *        gets destination port number for connection
 *
 * int Link_open(char *host, int port, int int timeout )
 *    opens a link to the remote host
 *    if timeout == 0, wait indefinitely
 *    returns socket fd (>= 0); LINK errorcode (negative) if error
 *
 * int Link_listen()
 *  1. opens a socket on the current host
 *  2. set the REUSE option on socket
 *  3. does a bind to port determined by Link_dest_port_num();
 *
 * void Link_close( int socket )
 *    closes the link to the remote host
 *
 * int Link_send( char *host, int *socket,int timeout,
 *    int ch, char *line,int lf,int *ack )
 *    sends 'ch'line'lf' to the remote host
 *    if write/read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    if timeout > 0, i.e.- local, set signal handler
 *    if ch != 0, send ch at start of line
 *    if lf != 0, send LF at end of line
 *    if ack != 0, wait for ack
 *      returns 0 if successful, LINK errorcode if failure
 *
 * int Link_copy( char *host, int *socket, int timeout,
 *	char *src, int fd, double count)
 *    copies count bytes from fd to the socket
 *    do a timeout on both reading from fd and writing to socket;
 *    if timeout == 0, wait indefinitely
 *      returns 0 if successful, LINK errorcode if failure
 *
 * int Link_line_read(char *host, int *socket, int timeout,
 *	  char *str, int *count )
 *    reads and copies characters from socket to str until '\n' read,
 *      '\n' NOT copied
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read (less 1 for '\n' )
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    if *count read and not '\n',  then error set; last character
 *       will be discarded.
 *    returns 0 if '\n' read and read <= *count characters
 *            0 if EOF and no characters read (*count == 0)
 *            LINK errorcode otherwise
 *    NOTE: socket is NOT closed on error.
 *
 * int Link_read( char *host, int *socket, int timeout,
 *	  char *str, int *count )
 *    reads and copies '*count' characters from socket to str
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    returns 0 *count not read
 *            LINK errorcode otherwise
 *
 * int Link_file_read( char *host, int *socket, int readtimeout,
 *    int writetimeout, int fd, int *count, int *ack )
 *    reads and copies '*count' characters from socket to fd
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if ack then will read an additional ACK character
 *       returns value in *ack
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    returns 0 *count not read
 *
 ***************************************************************************/

/***************************************************************************
 *  Commentary:
 * 		Patrick Powell Fri Apr 14 16:43:10 PDT 1995
 * 
 * 		These routines have evolved with experience from other systems.
 * The connection to the remote system involves 4 components:
 * < local_ip, local_port, remote_ip, remote_port >
 * 
 * Local_ip: There appears to be little reason done to restrict
 *  a system to originate a connection from a specific IP address.
 * 
 * Local_port:  The BSD UNIX networking software had a concept of reserved
 *  ports for binding, i.e.- only root could bind (accept on or originate
 *  connections from) ports 1-1023.  However,  this is now bogus given PCs
 *  that can use any port.  The requirement is specified in RFC1179, which
 *  requires origination of connections from 721-731.  Ummm... there is a
 *  problem with doing this,  as there is also a TCP/IP requirement that
 *  a port not be reused withing 2*maxLt (10 minutes).  If you spool a
 *  lot of jobs from a host,  you will have problems.  You have to use
 *  setsockopt( s, SOL_SOCKET, SO_REUSEADDR, 0, 0 ) to allow reuse of the
 *  port,  otherwise you may run out of ports to use.
 * 
 *  When using a Unix system,  you can run non-privileged or privileged.
 *  The remote system will determine if it is going to accept connections
 *  or not.  Thus,  there is the following possibilities
 * 	1. remote system wants PRIV port, and cannot open them (not root)
 * 	 - hard failure,  need root privs
 * 	2. remote system wants PRIV port, and non available
 * 	 - soft failure,  can retry after timeout
 * 	3. remote system doesnt care, cannot get priv port
 * 	 - try getting unpriv port
 * 	4. remote system doesnt care, cannot get unpriv port
 * 		 - hard failure. Something is very wrong here!
 * 
 *  Configuration options:
 *    The following options in the configuration file can be used to
 *    specify control over the local port:
 * 
 *    originate_port (default: blank same as lowportnumber = 0)
 *    originate_port lowportnumber [highportnumber]
 * 	If lowportnumber is missing or 0, then non-restricted
 * 	ports will be used.
 * 	If lowportnumber is non-zero (true),  and no high port number is
 * 	present,  then a port in the range lowportnumber-1023 will be used.
 * 	If both the lowport and highport and highport numbers
 * 	are present,  a port in the range lowportnumber-highportnumber
 * 	will be used.
 * 
 ***************************************************************************/

#include "lp.h"
#include "linksupport.h"
#include "gethostinfo.h"
#include "errorcodes.h"
/**** ENDINCLUDE ****/

/*JY1110*/
//extern char clientaddr[32];//JY1110
/**/

/***************************************************************************
 * int Link_open(char *host, int port,  int timeout );
 * 1. Set up an inet socket;  a socket has a local host/local port and
 *    remote host/remote port address part.  The routine
 *    will attempt to open a privileged port (number less than
 *    PRIV); if this fails,  it will open a non-privileged port
 * 2. Connect to the remote host on the port specified as follows:
 *    1. Lpd_port_DYN - (string) look up with getservbyname("printer","tcp")
 *    2. Lpd_port_DYN - (string) convert to integer if possible
 *    3. look up getservbyname("printer","tcp")
 *    Give up with error message
 *
 * In RFC1192, it specifies that the originating ports will be in
 * a specified range, 721-731; Since a reserved port can only
 * be accessed by UID 0 (root) processes, this would appear to prevent
 * ordinary users from directly contacting the remote daemon.
 * However,  enter the PC.  We can generate a connection from ANY port.
 * Thus,  all of these restrictions are somewhat bogus.  We use the
 * configuration file 'PORT' restrictions instead
 ***************************************************************************/
//extern char printerstatus[32];//JY1106

int Link_setreuse( int sock )
{
	int status = 0;
#ifdef SO_REUSEADDR
	int option = 1;
	status =  setsockopt( sock, SOL_SOCKET, SO_REUSEADDR,
			(char *)&option, sizeof(option) );
#endif
	return( status );
}

int Link_setkeepalive( int sock )
{
	int status = 0;
#ifdef SO_KEEPALIVE
	int option = 1;
	status = setsockopt( sock, SOL_SOCKET, SO_KEEPALIVE,
			(char *)&option, sizeof(option) );
#endif
	return( status );
}

/*
 * int getconnection ( char *hostname, int timeout, int connection_type )
 *   opens a connection to the remote host
 */


int connect_timeout( int timeout,
	int sock, struct sockaddr *name, int namelen)
{
	int status = -1;
	int err = 0;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout );
		status = connect(sock, name, namelen );
		err = errno;
	} else {
		status = -1;
		err = errno;
	}
	Clear_timeout();
	errno = err;
	return( status );
}

int getconnection ( char *xhostname,
	int timeout, int connection_type, struct sockaddr *bindto, char *unix_socket_path )
{
	int sock;	         /* socket */
	int i, err;            /* ACME Generic Integers */
	struct sockaddr_in dest_sin;     /* inet socket address */
	struct sockaddr_in src_sin;     /* inet socket address */
	int maxportno, minportno;
	int euid;
	int status = -1;			/* status of operation */
	plp_block_mask oblock;
	int port_count = 0;			/* numbers of ports tried */
	int connect_count = 0;		/* number of connections tried */
	int port_number;
	int range;				/* range of ports */
	char *use_host;
	int address_count = 0;
	int incoming_port;
	char *dest_port = 0;
	static int last_port_used;
	char hostname[256];

	/*
	 * find the address
	 */
	safestrncpy(hostname,xhostname);
	if( (dest_port = strchr(hostname,'%')) ){
		*dest_port++ = 0;
	}
	DEBUGF(DNW1)("getconnection: START host %s, port %s, timeout %d, connection_type %d",
		hostname, dest_port, timeout, connection_type );
	euid = geteuid();
	sock = -1;
	memset(&dest_sin, 0, sizeof (dest_sin));
	memset(&src_sin, 0, sizeof (src_sin));
	dest_sin.sin_family = AF_Protocol();
	if( Find_fqdn( &LookupHost_IP, hostname ) ){
		/*
		 * Get the destination host address and remote port number to connect to.
		 */
		DEBUGF(DNW1)("getconnection: fqdn found %s, h_addr_list count %d",
			LookupHost_IP.fqdn, LookupHost_IP.h_addr_list.count );
		dest_sin.sin_family = LookupHost_IP.h_addrtype;
		if( LookupHost_IP.h_length > (int)sizeof( dest_sin.sin_addr ) ){
			FATAL(LOG_ALERT) "getconnection: addresslength outsize value");
		}
		memcpy( &dest_sin.sin_addr,
			LookupHost_IP.h_addr_list.list[address_count],
			LookupHost_IP.h_length );
	} else if( inet_pton( AF_Protocol(), hostname, &dest_sin.sin_addr ) != 1 ){
		DEBUGF(DNW2)("getconnection: cannot get address for '%s'", hostname );
		return( LINK_OPEN_FAIL );
	}

	/* UNIX socket connection for localhost support */
	DEBUGF(DNW1)("getconnection: unix_socket_path %s", unix_socket_path );
	/* check to see if the flag is set and the destination
		is one of the localhost addresses */
	if( !ISNULL(unix_socket_path) && safestrcasecmp( "off", unix_socket_path ) &&
			( !Same_host(&LookupHost_IP,&Host_IP)
			|| !Same_host(&LookupHost_IP,&Localhost_IP) ) ){
		/* taken from Unix Network Programming, Volume 1, 2nd Edition
		 * by W. Richard Stevens.  With great thanks to his memory
		 * and his amazingly detailed reference monographs.
		 */
		struct sockaddr_un dest_un;     /* unix socket address */
		bzero( (char *)&dest_un, sizeof(dest_un) );

		DEBUGF(DNW1)("getconnection: using unix socket");
		plp_block_all_signals( &oblock );
		if( UID_root ) (void)To_euid_root();
		safestrncpy( dest_un.sun_path, unix_socket_path );
#ifdef AF_LOCAL
		dest_un.sun_family = AF_LOCAL;
#else
		dest_un.sun_family = AF_UNIX;
#endif
		sock = socket(dest_un.sun_family, SOCK_STREAM, 0);
#ifdef WINDOW_1
{
int windowsize=1024;
//aaaaaa=fopen("/tmp/qqqqq", "a");
if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize)) == -1 ) {	
;
//fprintf(aaaaaa, "linksupport: getconnection: opensocket FAILED!!!!!\n");
} else {
;
//fprintf(aaaaaa, "linksupport: getconnection: opensocket ok!!!\n");
}
//fclose(aaaaaa);
}
#endif
		err = errno;
		if( UID_root ) (void)To_euid( euid );
		plp_set_signal_mask( &oblock, 0 );
		Max_open(sock);
		if( sock < 0 ){
			errno = err;
			LOGERR_DIE(LOG_DEBUG) "getconnection: UNIX domain socket call failed");
		}
		DEBUGF(DNW2) ("getconnection: unix domain socket %d", sock);
		/*
		 * set up timeout and then make connect call
		 */
		errno = 0;
		status = -1;
		Alarm_timed_out = 0;
		use_host = Unix_socket_path_DYN;
		DEBUGF(DNW2)("getconnection: trying connect to UNIX domain socket '%s', timeout %d",
			use_host, timeout );
		status = connect_timeout(timeout,sock,
			(struct sockaddr *) &dest_un, sizeof(dest_un));
		err = errno;

#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW2)(
			"getconnection: connect sock %d, status %d, err '%s', timedout %d",
			sock, status, Errormsg(err), Alarm_timed_out );
#endif
		if( status < 0 || Alarm_timed_out ){
			(void) close (sock);
			sock = LINK_OPEN_FAIL;
			if( Alarm_timed_out ) {
				DEBUGF(DNW1)("getconnection: connection to '%s' timed out",
					use_host);
				err = errno = ETIMEDOUT;
			} else {
#ifdef ORIGINAL_DEBUG//JY@1020
				DEBUGF(DNW1)("getconnection: connection to '%s' failed '%s'",
					use_host, Errormsg(err) );
#endif
			}
		}
		errno = err;
		return (sock);
	}
	if( ISNULL(dest_port) ) dest_port = Lpd_port_DYN;
	dest_sin.sin_port = Link_dest_port_num(dest_port);
	if( dest_sin.sin_port == 0 ){
		LOGMSG(LOG_INFO)
		"getconnection: using illegal port '%s' for connection to '%s'!\n",
			dest_port, hostname );
		return( JABORT );
	}

	/* handle multi-homed hosts with bad ideas about
		network connections, i.e. - firewalls */

 next_addr:
	DEBUGF(DNW2)("getconnection: destination IP '%s' port %d",
		inet_ntoa( dest_sin.sin_addr ), ntohs( dest_sin.sin_port ) );

	/* check on the low and high port values */
	/* we decode the minimum and  maximum range */
	maxportno = minportno = 0;
	if( Originate_port_DYN ){
		char *s, *end;
		int c;
		s = end = Originate_port_DYN;
		maxportno = strtol( s, &end, 0 );
		if( s != end ){
			s = end;
			while( (c = cval(s)) && !isdigit(c) ) ++s;
			minportno = strtol( s, 0, 0 );
		}
	}
	DEBUGF(DNW2)(
		"getconnection: Originate_port_DYN '%s' minportno %d, maxportno %d",
		Originate_port_DYN, minportno, maxportno );
	/* check for reversed order ... */
	if( maxportno < minportno ){
		i = maxportno;
		maxportno = minportno;
		minportno = i;
	}
	/* check for only one */
	if( minportno == 0 ){
		minportno = maxportno;
	}
	/*
	 * if not running SUID root or not root user
	 * check to see if we have less than reserved port
	 */
	if( !UID_root && minportno < IPPORT_RESERVED ){
		minportno = IPPORT_RESERVED;
		if( minportno > maxportno ){
			minportno = maxportno = 0;
		}
	}
	range = maxportno - minportno;

	port_count = 0;			/* numbers of ports tried */
	port_number = 0;
	connect_count = 0;		/* number of connections tried */
	port_number = 0;

	if( minportno ){
		port_number = minportno;
		if( range ){
			if( last_port_used && last_port_used >= minportno ){
				port_number = ++last_port_used;
			} else {
				srand(getpid()); 
				port_number = minportno + (abs(rand()>>8) % range);
			}
		}
		if( port_number > maxportno){
			port_number = minportno;
		}
	}

	/* we now have a range of ports and a starting position
	 * Note 1: if minportno == 0, then we use assignment by connect
	 *  and do not set SOCKREUSE.  We only try once.
	 *
     * Note 2: if minportno != 0, then we get port in range.
     *  we DO set SOCKREUSE if Reuse_addr_DYN set.  This is necessary
	 *  as some printers will not accept a new connection from the same
	 *  port as before.
	 *
     * Note 3. we try all ports in range; if none work,
     *  we get an error.
     * 
	 *
	 * do the sock et, bind and the set reuse flag operation as
	 * ROOT;  this appears to be the side effect of some
	 * very odd system implementations.  Note that you can
	 * read and write to the socket as a user.
	 */
	DEBUGF(DNW2)("getconnection: minportno %d, maxportno %d, range %d, port_number %d",
		minportno, maxportno, range, port_number );

 again:
	DEBUGF(DNW1)("getconnection: AGAIN port %d, min %d, max %d, count %d, connects %d",
		port_number, minportno, maxportno, port_count, connect_count );
	DEBUGF(DNW2)("getconnection: protocol %d, connection_type %d",
		AF_Protocol(), connection_type );
	plp_block_all_signals( &oblock );
	if( UID_root ) (void)To_euid_root();
	sock = socket(AF_Protocol(), connection_type, 0);
#ifdef WINDOW_1
{
int windowsize=1024;
//aaaaaa=fopen("/tmp/qqqqq", "a");
if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize)) == -1) {
//fprintf(aaaaaa, "linksupport: getconnection: opensocket: again FAILED!!!!!!\n");
} else{
//fprintf(aaaaaa, "linksupport: getconnection: opensocket: again ok!!!!!!\n");
}
//fclose(aaaaaa);
}
#endif
	err = errno;
	if( UID_root ) (void)To_euid( euid );
	plp_set_signal_mask( &oblock, 0 );
	Max_open(sock);
	if( sock < 0 ){
		errno = err;
		LOGERR_DIE(LOG_DEBUG) "getconnection: socket call failed");
	}
	DEBUGF(DNW2) ("getconnection: socket %d", sock);

	/* bind to an outgoing port if you need to */
	if( minportno || bindto ){
		incoming_port = ntohs(Link_dest_port_num(0));
		do{
			last_port_used = port_number;
			status = -1;
			if( bindto == 0 ){
				src_sin.sin_family = AF_Protocol();
				src_sin.sin_addr.s_addr = INADDR_ANY;
			} else {
				src_sin.sin_family = ((struct sockaddr_in *)bindto)->sin_family;
				src_sin.sin_addr = ((struct sockaddr_in *)bindto)->sin_addr;
			}
			if( port_number > maxportno ) port_number = minportno;

			/* make sure we don't use the port 515 */
			if( port_number == incoming_port ) continue;

			DEBUGF(DNW2) ("getconnection: trying port %d, bound to addr '%s'",
				port_number, inet_ntoa(src_sin.sin_addr) );
			src_sin.sin_port = htons((u_short)(port_number));

			/* set the reuse_addr before bind */
			status = 0;
			if( Reuse_addr_DYN ){
				/* set up the 'resuse' flag on socket, or you may not be
					able to reuse a port for up to 10 minutes */
				/* we do the next without interrupts and as root */
				plp_block_all_signals( &oblock );
				if( UID_root ) (void)To_euid_root();
				status = Link_setreuse( sock );
				err = errno;
				if( UID_root ) (void)To_euid( euid );
				plp_set_signal_mask( &oblock, 0 );
				DEBUGF(DNW2) ("getconnection: sock %d, reuse status %d",
					sock, status );
				if( status < 0 ){
					LOGERR(LOG_ERR) "getconnection: set SO_REUSEADDR failed" );
				}
			}
			if( status >= 0 ){
				/* we do the next without interrupts */
				plp_block_all_signals( &oblock );
				if( UID_root ) (void)To_euid_root();
				status = bind(sock, (struct sockaddr *)&src_sin, sizeof(src_sin));
				err = errno;
				if( UID_root ) (void)To_euid( euid );
				plp_set_signal_mask( &oblock, 0 );
				DEBUGF(DNW2) ("getconnection: bind returns %d, sock %d, port %d, src '%s'",
					status, sock, ntohs(src_sin.sin_port), inet_ntoa(src_sin.sin_addr) );
			}
			if( status >= 0 && Keepalive_DYN ){
				/* we do the next without interrupts */
				plp_block_all_signals( &oblock );
				if( UID_root ) (void)To_euid_root();
				status = Link_setkeepalive( sock );
				err = errno;
				if( UID_root ) (void)To_euid( euid );
				plp_set_signal_mask( &oblock, 0 );
				if( status < 0 ){
					LOGERR(LOG_ERR) "getconnection: set SO_KEEPALIVE failed" );
				}
			}
		} while( ++port_number && status < 0 && ++port_count < range );
		if( status < 0 ){
			close( sock );
			sock = LINK_OPEN_FAIL;
			LOGERR(LOG_DEBUG) "getconnection: cannot bind to port");
			return( sock );
		}
	}

	/*
	 * set up timeout and then make connect call
	 */
	errno = 0;
	status = -1;
	Alarm_timed_out = 0;
	use_host = hostname;
	DEBUGF(DNW2)("getconnection: trying connect to '%s', timeout %d",
		use_host, timeout );
	status = connect_timeout(timeout,sock,
		(struct sockaddr *) &dest_sin, sizeof(dest_sin));
	err = errno;

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DNW2)(
		"getconnection: connect sock %d, status %d, err '%s', timedout %d",
		sock, status, Errormsg(err), Alarm_timed_out );
#endif
	if( status < 0 || Alarm_timed_out ){
		(void) close (sock);
		sock = LINK_OPEN_FAIL;
		if( Alarm_timed_out ) {
			DEBUGF(DNW1)("getconnection: connection to '%s' timed out",
				use_host);
			err = errno = ETIMEDOUT;
		} else {
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGF(DNW1)("getconnection: connection to '%s' failed '%s'",
				use_host, Errormsg(err) );
#endif
		}
		if( err == ECONNREFUSED ){
			sock = LINK_ECONNREFUSED;
		} 
		if( err == ETIMEDOUT ){
			sock = LINK_ETIMEDOUT;
		} 
		++connect_count;
		if( connect_count < range
			&& (
				(Retry_ECONNREFUSED_DYN && err == ECONNREFUSED)
				|| err == EADDRINUSE
				|| err == EADDRNOTAVAIL
				) ){
			goto again;
		}
		/* try next address in list */
		if( ++address_count < LookupHost_IP.h_addr_list.count ){
			memcpy( &dest_sin.sin_addr,
				LookupHost_IP.h_addr_list.list[address_count],
				LookupHost_IP.h_length );
			goto next_addr;
		}
	} else {
#if defined(HAVE_SOCKLEN_T)
		socklen_t len;
#else
		int len;
#endif
		len = sizeof( src_sin );
		if( getsockname( sock, (struct sockaddr *)&src_sin, &len ) < 0 ){
			LOGERR_DIE(LOG_ERR)"getconnnection: getsockname failed" );
		}
		DEBUGF(DNW1)( "getconnection: sock %d, src ip %s, port %d", sock,
			inet_ntoa( src_sin.sin_addr ), ntohs( src_sin.sin_port ) );
		DEBUGF(DNW1)( "getconnection: dest ip %s, port %d",
			inet_ntoa( dest_sin.sin_addr ), ntohs( dest_sin.sin_port ) );
		if( Socket_linger_DYN > 0 ){
			Set_linger( sock, Socket_linger_DYN );
		}
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DNW1)("getconnection: connection to '%s' socket %d, errormsg '%s'",
		use_host, sock, Errormsg(err) );
#endif
	errno = err;
	return (sock);
}

void Set_linger( int sock, int n )
{
#ifdef SO_LINGER
#if defined(HAVE_SOCKLEN_T)
	socklen_t len;
#else
	int len;
#endif
	struct linger option;
	len = sizeof( option );

	DEBUGF(DNW2) ("Set_linger: SO_LINGER socket %d, value %d", sock, n );
	if( getsockopt( sock,SOL_SOCKET,SO_LINGER,(char *)&option, &len) == -1 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW2) ("Set_linger: getsockopt linger failed - '%s'", Errormsg(errno) );
#endif
		return;
	}
	DEBUGF(DNW4) ("Set_linger: SO_LINGER socket %d, onoff %d, linger %d",
		sock, (int)(option.l_onoff), (int)(option.l_linger));
	if( n > 0 ){
		option.l_onoff = 1;
		option.l_linger = n;
	} else {
		option.l_onoff = 0;
		option.l_linger = 0;
	}
	if( setsockopt( sock, SOL_SOCKET, SO_LINGER,
			(char *)&option, sizeof(option) ) == -1 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW2) ("Set_linger: setsockopt linger %d failed - '%s'", n, Errormsg(errno) );
#endif
	}
#else
	DEBUGF(DNW2) ("Set_linger: NO SO_LINGER, socket %d, value %d", sock, n);
#endif
}

/*
 * int Link_listen(port)
 *  1. opens a socket on the current host
 *  2. set the REUSE option on socket
 *  3. does a bind to port determined by Link_dest_port_num();
 */

int Link_listen( char *port_name )
{
	int sock;                   /* socket */
	int status;                   /* socket */
	struct sockaddr_in sinaddr;     /* inet socket address */
	int euid;					/* euid at time of call*/
	int port;
	int err;
	char *s;

	/*
	 * Zero out the sockaddr_in struct
	 */
	memset(&sinaddr, 0, sizeof (sinaddr));
	/*
	 * Get the destination host address and remote port number to connect to.
	 */
	sinaddr.sin_family = AF_Protocol();
	sinaddr.sin_addr.s_addr = INADDR_ANY;
	if( (s = safestrchr( port_name, '%')) ){
		*s = 0;
		if( Find_fqdn( &LookupHost_IP, port_name ) ){
			/*
			 * Get the destination host address and remote port number to connect to.
			 */
			DEBUGF(DNW1)("Link_listen: fqdn found %s, h_addr_list count %d",
				LookupHost_IP.fqdn, LookupHost_IP.h_addr_list.count );
			sinaddr.sin_family = LookupHost_IP.h_addrtype;
			if( LookupHost_IP.h_length > (int)sizeof( sinaddr.sin_addr ) ){
				FATAL(LOG_ALERT) "getconnection: addresslength outsize value");
			}
			/* use the first address in the list */
			memcpy( &sinaddr.sin_addr,
				LookupHost_IP.h_addr_list.list[0],
				LookupHost_IP.h_length );
		} else if( inet_pton( AF_Protocol(), port_name, &sinaddr.sin_addr ) != 1 ){
			*s = '%';
			Errorcode = JABORT;
			FATAL(LOG_ERR) "Link_listen: bad lpd_port value, cannot resolve IP address '%s'",
				port_name );
		}
		sinaddr.sin_port = Link_dest_port_num(s+1);
		*s = '%';
	} else if( port_name ){
		sinaddr.sin_port = Link_dest_port_num(port_name);
	}
	port = ntohs( sinaddr.sin_port );
	DEBUGF(DNW2)("Link_listen: bind to IP '%s' port %d",
		inet_ntoa( sinaddr.sin_addr ), ntohs( sinaddr.sin_port ) );
	if( port == 0 ){
		errno = 0;
		return( 0 );
	}

	euid = geteuid();
	if( UID_root ) (void)To_euid_root();
	errno = 0;
	status = (sock = socket (AF_Protocol(), SOCK_STREAM, 0)) < 0
		|| Link_setreuse( sock ) < 0
		|| (Keepalive_DYN && Link_setkeepalive( sock ) < 0)
		|| bind(sock, (struct sockaddr *)&sinaddr, sizeof(sinaddr)) < 0;
#ifdef WINDOW_1
{
int windowsize=1024;
//aaaaaa=fopen("/tmp/qqqqq", "a");
if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize)) == -1) {
//fprintf(aaaaaa, "linksupport: Link_listen: opensocket FAILED!!!!!!\n");
} else{
//fprintf(aaaaaa, "linksupport: Link_listen: opensocket ok!!!!!!\n");
}
//fclose(aaaaaa);
}
#endif
	err = errno;
	if( UID_root ) (void)To_euid( euid );
	if( status ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW4)("Link_listen: bind to lpd port %d failed '%s'",
			port, Errormsg(err));
#endif
		if( sock >= 0 ){
			(void)close( sock );
			sock = -1;
		}
		errno = err;
		return( LINK_BIND_FAIL );
	}
	status = listen(sock, 64 );	/* backlog of 10 is inadequate */
	err = errno;
	if( status ){
		LOGERR_DIE(LOG_ERR) "Link_listen: listen failed");
		(void)close( sock );
		sock = -1;
		err = errno;
		return( LINK_OPEN_FAIL );
	}
	DEBUGF(DNW4)("Link_listen: port %d, socket %d", ntohs( sinaddr.sin_port ), sock);
	errno = err;
	return (sock);
}

int Unix_link_listen( char *unix_socket_path )
#ifdef ORIGINAL_DEBUG//JY@1020
{
#else
{}
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
	int sock;                   /* socket */
	int status;                 /* socket */
	struct sockaddr_un sunaddr;     /* inet socket address */
	int euid;					/* euid at time of call*/
	int err;
	int omask;

	euid = geteuid();

	DEBUGF(DNW2)("Unix_link_listen: path %s", unix_socket_path );
	
	/*
	 * Zero out the sunaddr struct
	 */
	memset(&sunaddr, 0, sizeof (sunaddr));
	/*
	 * Get the destination host address and remote port number to connect to.
	 */
	DEBUGF(DNW1)("Unix_link_listen: using unix socket");
	safestrncpy( sunaddr.sun_path, Unix_socket_path_DYN );
#ifdef AF_LOCAL
	sunaddr.sun_family = AF_LOCAL;
#else
	sunaddr.sun_family = AF_UNIX;
#endif
	if( UID_root ) (void)To_euid_root();
	unlink( sunaddr.sun_path );
	status = ((sock = socket (sunaddr.sun_family, SOCK_STREAM, 0)) < 0);
#ifdef WINDOW_1
int windowsize=1024;
//aaaaaa=fopen("/tmp/qqqqq", "a");
if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&windowsize, sizeof(windowsize)) == -1) {
//fprintf(aaaaaa, "linksupport: Unix_link_listen: opensocket FAILED!!!!!!\n");
}else{
//fprintf(aaaaaa, "linksupport: Unix_link_listen: opensocket ok!!!!!\n");
}
//fclose(aaaaaa);
#endif
	err = errno;
	if( UID_root ) (void)To_euid( euid );
	Max_open(sock);
	if( sock < 0 ){
		errno = err;
		LOGERR_DIE(LOG_DEBUG) "Unix_link_listen: UNIX domain socket call failed");
	}

	omask = umask(0);
	if( UID_root ) (void)To_euid_root();
	status = bind(sock, (struct sockaddr *)&sunaddr, sizeof(sunaddr)) < 0;
	err = errno;
	if( UID_root ) (void)To_euid( euid );
	umask(omask);
	if( status ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW4)("Unix_link_listen: bind to unix port %s failed '%s'",
			Unix_socket_path_DYN, Errormsg(err));
#endif
		if( sock >= 0 ){
			(void)close( sock );
			sock = -1;
		}
		errno = err;
		return( LINK_BIND_FAIL );
	}
	if( UID_root ) (void)To_euid_root();
	status = listen(sock, 64 );	/* backlog of 10 is inadequate */
	err = errno;
	if( UID_root ) (void)To_euid( euid );
	if( status ){
		LOGERR_DIE(LOG_ERR) "Unix_link_listen: listen failed");
		(void)close( sock );
		sock = -1;
		err = errno;
		return( LINK_OPEN_FAIL );
	}
	DEBUGF(DNW4)("Unix_link_listen: socket %d", sock);
	errno = err;
	return (sock);
}
#endif

int Link_open(char *host, int timeout, struct sockaddr *bindto,
	char *unix_socket_path )
{
	int sock;
	DEBUGF(DNW4) ("Link_open: host '%s', timeout %d",
		host, timeout);
	sock = Link_open_type( host, timeout, SOCK_STREAM, bindto, unix_socket_path );
	DEBUGF(DNW4) ("Link_open: socket %d", sock );
	return(sock);
}

int Link_open_type(char *host, int timeout, int connection_type,
	struct sockaddr *bindto, char * unix_socket_path )
{
	int sock = -1;
	DEBUGF(DNW4)(
		"Link_open_type: host '%s', timeout %d, type %d",
		host, timeout, connection_type );
	sock = getconnection( host, timeout, connection_type, bindto, unix_socket_path );
	DEBUGF(DNW4) ("Link_open_type: socket %d", sock );
	return( sock );
}

#ifdef ORIGINAL_DEBUG//JY@1020
int Link_open_list( char *hostlist, char **result,
	int timeout, struct sockaddr *bindto, char *unix_socket_path )
{
	int sock = -1, i, err = 0;
	struct line_list list;

	Init_line_list( &list );
	DEBUGFC(DNW4){
		LOGDEBUG(
	"Link_open_line_list_type: hostlist '%s', timeout %d, bindto 0x%lx",
		hostlist, timeout, Cast_ptr_to_long(bindto) );
	}
	if( result ) *result = 0;
	Split(&list,hostlist,Host_sep,0,0,0,0,0,0);
	err = errno = 0;
	for( i = 0; sock < 0 && i < list.count; ++i ){
		DEBUGF(DNW4) ("Link_open_list: host trying '%s'", list.list[i] );
		sock = getconnection( list.list[i], timeout, SOCK_STREAM, bindto, unix_socket_path );
		err = errno;
		DEBUGF(DNW4) ("Link_open_list: result host '%s' socket %d", list.list[i], sock );
		if( sock >= 0 ){
			if( result ){
				*result = safestrdup( list.list[i], __FILE__, __LINE__ );
			}
			break;
		}
	}
	errno = err;
	Free_line_list(&list);
	return( sock );
}
#endif

/***************************************************************************
 * void Link_close( int socket )
 *    closes the link to the remote host
 *  We first do a shutdown(*sock,1) and THEN do a close
 ***************************************************************************/

void Link_close( int *sock )
{
	char buf[SMALLBUFFER];
	DEBUGF(DNW4) ("Link_close: closing socket %d", *sock );
	if( *sock >= 0 ){
		shutdown(*sock,1);
		while( read(*sock,buf,sizeof(buf)) > 0 );
		(void)close(*sock);
	}
	*sock = -1;
}

/***************************************************************************
 * int Link_send( char *host, int *socket, int timeout,
 *			 char ch, char *str, int lf, int *ack )
 *    sends 'ch'str to the remote host
 *    if write/read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    if ch != 0, send ch at start of line
 *    if lf != 0, send LF at end of line
 *    if ack != 0, wait for ack, and report it
 *      returns 0 if successful, LINK errorcode if failure
 *      closes socket and sets to LINK errorcode if a failure
 * NOTE: if timeout > 0, local to this function;
 *       if timeout < 0, global to all functions
 *
 * Note: several implementations of LPD expect the line to be written/read
 *     with a single system call (i.e.- TPC/IP does not have the PSH flag
 *     set in the output stream until the last byte, for those in the know)
 *     After having gnashed my teeth and pulled my hair,  I have modified
 *     this code to try to use a single write.  Note that timeouts, errors,
 *     interrupts, etc., may render this impossible on some systems.
 *     Tue Jul 25 05:50:54 PDT 1995 Patrick Powell
 ***************************************************************************/

int Link_send( char *host, int *sock, int timeout,
	char *sendstr, int count, int *ack )
{
	int i;      /* Watch out for longjmp * ACME Integers, Inc. */
	int status;		/* return status */
	int err = 0;

	/*
	 * set up initial conditions
	 */
	i = status = 0;	/* shut up GCC */
	if(*sock < 0) {
		DEBUGF(DNW1)( "Link_send: bad socket" );
		return( LINK_TRANSFER_FAIL );
	}
	if( ack ){
		*ack = 0;
	}

	DEBUGF(DNW1)( "Link_send: host '%s' socket %d, timeout %d",
		host, *sock, timeout );
	DEBUGF(DNW1)( "Link_send: str '%s', count %d, ack 0x%x",
		sendstr, count , ack );

	/*
	 * set up timeout and then write
	 */
	
	i = Write_fd_len_timeout( timeout, *sock, sendstr, count );

	/* now decode the results */
	DEBUGF(DNW2)("Link_send: final write status %d", i );
	if( i < 0 || Alarm_timed_out ){
		if( Alarm_timed_out ){
			DEBUGF(DNW2)("Link_send: write to '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else {
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGF(DNW2)("Link_send: write to '%s' failed '%s'",
				host, Errormsg(err) );
#endif
			status = LINK_TRANSFER_FAIL;
		}
	}

	/* check for an ACK to be received */
	if( status == 0 && ack ){
		char buffer[1];

		DEBUGF(DNW2)("Link_send: ack required" );
		buffer[0] = 0;
		i = Read_fd_len_timeout(timeout, *sock, buffer, 1 );
		err = errno;
		DEBUGF(DNW2)("Link_send: read status '%d'", i );
		if( i < 0 || Alarm_timed_out ){
			if( Alarm_timed_out ){
				DEBUGF(DNW2)("Link_send: ack read from '%s' timed out", host);
				status = LINK_TRANSFER_FAIL;
			} else {
#ifdef ORIGINAL_DEBUG//JY@1020
				DEBUGF(DNW2)("Link_send: ack read from '%s' failed - %s",
					host, Errormsg(err) );
#endif
				status = LINK_TRANSFER_FAIL;
			}
		} else if( i == 0 ){
			DEBUGF(DNW2)("Link_send: ack read EOF from '%s'", host );
			status = LINK_TRANSFER_FAIL;
		} else if( buffer[0] ){
			*ack = buffer[0];
			status = LINK_ACK_FAIL;
		}
		DEBUGF(DNW2)("Link_send: read %d, status %s, ack=%s",
			i, Link_err_str(status), Ack_err_str(*ack) );

		if( Check_for_protocol_violations_DYN && status == 0 && *ack == 0 ){
			/* check to see if you have some additional stuff pending */
			fd_set readfds;
			struct timeval delay;

			memset( &delay,0,sizeof(delay));
			/* delay.tv_usec = 1000; */
			FD_ZERO( &readfds );
			FD_SET( *sock, &readfds );
			i = select( (*sock)+1,
				FD_SET_FIX((fd_set *))&readfds,
				FD_SET_FIX((fd_set *))0,
				FD_SET_FIX((fd_set *))0, &delay );
			if( i > 0 ){
				LOGMSG( LOG_ERR)
				"Link_send: PROTOCOL ERROR - pending input from '%s' after ACK received",
				host );
			}
		}
	}
	DEBUGF(DNW1)("Link_send: final status %s", Link_err_str(status) );
	return (status);
}

#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * int Link_copy( char *host, int *socket, int timeout,
 *  char *src, int fd, int count)
 *    copies count bytes from fd to the socket
 *    if write does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    if count < 0, will read until end of file
 *      returns 0 if successful, LINK errorcode if failure
 ***************************************************************************/
int Link_copy( char *host, int *sock, int readtimeout, int writetimeout,
	char *src, int fd, double pcount)
{
	char buf[LARGEBUFFER];      /* buffer */
	int len;              /* ACME Integer, Inc. */
	int status;				/* status of operation */
	double count;	/* might be clobbered by longjmp */
	int err;					/* saved error status */

	count = pcount;
	len = status = 0;	/* shut up GCC */
	DEBUGF(DNW4)("Link_copy: sending %0.0f of '%s' to %s, rdtmo %d, wrtmo %d, fd %d",
		count, src, host, readtimeout, writetimeout, fd );
	/* check for valid sock */
	if(*sock < 0) {
		DEBUGF(DNW4)( "Link_copy: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/* do the read */
	while( status == 0 && (count > 0 || pcount == 0) ){
		len = sizeof(buf);
		if( pcount && len > count ) len = count;
		/* do the read with timeout */
		len = Read_fd_len_timeout( readtimeout, fd, buf, len );
		err = errno;
		DEBUGF(DNW4)("Link_copy: read %d bytes", len );

		if( pcount && len > 0 ) count -= len;

		if( Alarm_timed_out || len <= 0 ){
			/* EOF on input */
			if( pcount && count > 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
				DEBUGF(DNW4)(
					"Link_copy: read from '%s' failed, %0.0f bytes left - %s",
					src, count, Errormsg(err) );
#endif
				status = LINK_TRANSFER_FAIL;
			} else {
				DEBUGF(DNW4)("Link_copy: read status %d count %0.0f", len, count );
				status = 0;
			}
			break;
		}

		len = Write_fd_len_timeout(writetimeout, *sock, buf, len );

		DEBUGF(DNW4)("Link_copy: write done, status %d", len );
		if( len < 0 || Alarm_timed_out ){
			if( Alarm_timed_out ){
				DEBUGF(DNW4)("Link_copy: write to '%s' timed out", host);
				status = LINK_TRANSFER_FAIL;
			} else {
#ifdef ORIGINAL_DEBUG//JY@1020
				DEBUGF(DNW4)("Link_copy: write to '%s' failed - %s",
					host, Errormsg(err) );
#endif
				status = LINK_TRANSFER_FAIL;
			}
		}
	}
	
	if( status == 0 ){
		/* check to see if you have some additional stuff pending */
		fd_set readfds;
		struct timeval delay;
		int i;

		memset( &delay,0,sizeof(delay));
		FD_ZERO( &readfds );
		FD_SET( *sock, &readfds );
		i = select( *sock+1,
			FD_SET_FIX((fd_set *))&readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))0, &delay );
		if( i != 0 ){
			LOGMSG( LOG_ERR)
			"Link_copy: PROTOCOL ERROR - pending input from '%s' after transfer",
			host );
		}
	}
	DEBUGF(DNW4)("Link_copy: status %d", status );
	return( status );
}
#endif


/***************************************************************************
 * Link_dest_port_num ( char *port )
 * Get the destination port number
 * look up the service in the service directory using getservent
 * returned in the network standard order
 *  The port specification can be host%port
 *  if none is specified the Lpd_port_DYN used
 ***************************************************************************/
int Link_dest_port_num( char *port )
{
	struct servent *sp;
	int port_num = 0;
	char *s;

	if( port == 0 ) port = Lpd_port_DYN;
	if( port == 0 ){
		Errorcode = JABORT;
		FATAL( LOG_ERR)
			"Link_dest_port_num: LOGIC ERROR! no port number!");
	}
	if( (s = strchr(port, '%')) ) port = s+1;

	s = 0;
	port_num = strtol(port,&s,0);
	port_num = htons(port_num);
	if( s == 0 || *s ){
		if( (sp = getservbyname(port, "tcp")) == 0) {
			DEBUGF(DNW4)("getservbyname(\"%s\",tcp) failed", port);
			/* try integer value */
			port_num = 0;
		} else {
			port_num = sp->s_port;
		}
	}
	DEBUGF(DNW1)("Link_dest_port_num: port %s = %d", port, ntohs( port_num ) );
	return (port_num);
}

/***************************************************************************
 * int Link_line_read(char *host, int *sock, int timeout,
 *	  char *str, int *count )
 *    reads and copies characters from socket to str until '\n' read,
 *      '\n' NOT copied. \r\n -> \n
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read (less 1 for '\n' )
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    if *count read and not '\n',  then error set; last character
 *       will be discarded.
 *    returns 0 if '\n' read at or before *count characters
 *            0 if EOF and no characters read (*count == 0)
 *            LINK errorcode otherwise
 ***************************************************************************/

int Link_line_read(char *host, int *sock, int timeout,
	  char *buf, int *count )
{
	int i, len, max, err = 0;	/* ACME Integer, Inc. */
	int status;				/* status of operation */
	int cr, lf;					/* lf found */

	len = i = status = 0;	/* shut up GCC */
	max = *count;
	*count = 0;
	buf[0] = 0;
	DEBUGF(DNW1) ("Link_line_read: reading %d from '%s' on %d, timeout %d",
		max, host, *sock, timeout );
	/* check for valid socket */
	if(*sock < 0) {
		DEBUGF(DNW4)("Link_line_read: bad socket" );
		*count = 0;
		return (LINK_OPEN_FAIL);
	}
	/*
	 * set up timeout and then do operation
	 */
	cr = lf = len = 0;
	errno = 0;
	while( len < max-1
		&&(i = Read_fd_len_timeout(timeout, *sock, &buf[len], 1 )) > 0
		&& !Alarm_timed_out ){
		if( (lf = (buf[len] == '\n')) ){
			break;
		} if( (cr = (buf[len] == '\r')) ){
			cr = 1;
		} else {
			++len;
		}
	}
	err = errno;
	buf[len] = 0;
	DEBUGF(DNW2)( "Link_line_read: read %d, timeout %d, '%s'", len, Alarm_timed_out, buf);
	/*
	 * conditions will be:
	 * long line, timeout, error, or OK
	 */
	if( Alarm_timed_out ){
		DEBUGF(DNW4)( "Link_line_read: read from '%s' timed out", host);
		status = LINK_TRANSFER_FAIL;
	} else if( i == 0 ){
		DEBUGF(DNW4)("Link_line_read: EOF from '%s'", host );
		if( len != 0 ){
			status = LINK_TRANSFER_FAIL;
		}
	} else if( i < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW4)("Link_line_read: read from '%s' failed - %s", host,
			Errormsg(err) );
#endif
		status = LINK_TRANSFER_FAIL;
	} else if( lf == 0 ){
		DEBUGF(DNW4)("Link_line_read: no LF on line from '%s'", host );
		status = LINK_LONG_LINE_FAIL;
	}
	*count = len;

	DEBUGF(DNW4)("Link_line_read: status %d, len %d", status, len );
	errno = err;
	return( status );
}


#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * int Link_line_peek(char *host, int *sock, int timeout,
 *	  char *str, int *count )
 *    peeks at input on socket
 *       and copies characters from socket to str until '\n'
 *        or max chars found.
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if \n not found within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    returns 0 if '\n' read or *count characters
 *            LINK errorcode otherwise
 ***************************************************************************/

int Link_line_peek(char *host, int *sock, int timeout,
	  char *buf, int *count )
{
	int err = 0;	/* ACME Integer, Inc. */
	int status, max, len;				/* status of operation */

	status = 0;	/* shut up GCC */
	max = *count;
	*count = 0;
	buf[0] = 0;
	DEBUGF(DNW1) ("Link_line_peek: peeking for %d from '%s' on %d, timeout %d",
		max, host, *sock, timeout );
	/* check for valid socket */
	if(*sock < 0) {
		DEBUGF(DNW1)("Link_line_peek: bad socket" );
		*count = 0;
		return (LINK_OPEN_FAIL);
	}
	/*
	 * set up timeout and then do operation
	 */
	len = -1;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout );
		len = recv( *sock, buf, max, MSG_PEEK );
	} else {
		len = -1;
	}
	err = errno;
	DEBUGF(DNW1)( "Link_line_peek: read %d, timeout %d, '%s'", len, Alarm_timed_out, buf);
	/*
	 * conditions will be:
	 * long line, timeout, error, or OK
	 */
	if( len <= 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW1)("Link_line_peek: read from '%s' failed - %s", host,
			Errormsg(err) );
#endif
		status = LINK_TRANSFER_FAIL;
	} else {
		*count = len;
		status = 0;
	}

	DEBUGF(DNW1)("Link_line_peek: status %d, len %d", status, len );
	errno = err;
	return( status );
}
#endif

/***************************************************************************
 * int Link_read(char *host, int *sock, int timeout,
 *	  char *str, int *count )
 *    reads and copies '*count' characters from socket to str
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    returns 0 *count not read
 *        LINK errorcode otherwise
 ***************************************************************************/

int Link_read(char *host, int *sock, int timeout,
	  char *buf, int *count )
{
	int len, i, status;      /* ACME Integer, Inc. */
	char *str;				/* input buffer pointer */
	int err;

	status = 0;	/* shut up GCC */
	DEBUGF(DNW1) ("Link_read: reading %d from '%s' on socket %d",
		*count, host, *sock );
	/* check for valid socket */
	if(*sock < 0) {
		DEBUGF(DNW1)( "Link_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	if( *count < 0 ) *count = 0;
	len = *count;
	*count = 0;
	str = buf;
	/*
	 * set up timeout and then do operation
	 */
	i = Read_fd_len_timeout(timeout, *sock, str, len );
	err = errno;
	if( i >= 0 ){
		*count = i;
	}
	DEBUGFC(DNW2){
		char shortpart[32];
		shortpart[0] = 0;
		if( i > 0 ){
			safestrncpy( shortpart, str );
		}
		LOGDEBUG( "Link_read: wanted %d, got %d, start='%s'",
			len, i, shortpart );
	}

	if( Alarm_timed_out ){
		DEBUGF(DNW2)("Link_read: read %d from '%s' timed out",
			len, host, i );
		status = LINK_TRANSFER_FAIL;
	} else if( i < 0 ) {
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DNW2)("Link_read: read %d from '%s' failed, returned %d - %s",
			len, host, i, Errormsg(err) );
#endif
		status = LINK_TRANSFER_FAIL;
	}

	errno = err;
	return( status );
}
#ifdef TEST_WRITE//JYWeng

/***************************************************************************
 * int Link_file_read( char *host, int *sock, int readtimeout,
 *    int writetimeout, int fd, int *count, int *ack )
 *    reads and copies '*count' characters from socket to fd
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if ack then will read an additional ACK character
 *       returns value in *ack
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    returns 0 *count not read
 *
 ***************************************************************************/

int Link_file_read_test(char *host, int *sock, int readtimeout, int writetimeout,
	  int fd, double *count, int *ack )
{
	char str[LARGEBUFFER];		/* input buffer pointer */
	int i, l, cnt;			/* number to read or write */
	int status;				/* status of operation */
	int err;					/* error */
	double len;
	double readcount;

	currten_sock = sock;//JY1120
	len = i = status = cnt = 0;	/* shut up GCC */
	readcount = 0;
	*ack = 0;
	DEBUGF(DNW1) ("Link_file_read: reading %0.0f from '%s' on %d",
		*count, host, *sock );
	/* check for valid socket */
	if(*sock < 0) {
		DEBUGF(DNW2)( "Link_file_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/*
	 * set up timeout and then do the transfer
	 */

	/* do the read */
	len = *count;
/*1106test status*/
//plp_signal_break(SIGUSR, );
check_prn_status("Printing", clientaddr);
/**/
	while( status == 0 && (*count == 0 || len > 0) ){
		DEBUGF(DNW2)("Link_file_read: doing data read" );
		l = sizeof(str);
		if( *count && l > len ) l = len;
		i = Read_fd_len_timeout( readtimeout, *sock, str, l );
		err = errno;
		if( Alarm_timed_out ){
			DEBUGF(DNW2)( "Link_file_read: read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DEBUGF(DNW2)("Link_file_read: len %0.0f, readlen %d, read %d", len, l, i );
			if( *count ) len -= i;
			readcount += i;
#if 1 //JYWeng
/*1106test status*/
//strcpy(printerstatus, "Printing");
/**/

writetimeout = 600;//JY1110
//JY1110			cnt = write(fd_print, str, i );
			cnt = Write_fd_len_timeout(writetimeout, fd_print, str, i);

#else
aaaaaa=fopen("/tmp/a12345", "a");
fprintf(aaaaaa, "Linksupport: write_fd_len_timeout\n");
fclose(aaaaaa);
			cnt = Write_fd_len_timeout(writetimeout, fd, str, i );
sleep(10);
#endif
			err = errno;
			if( Alarm_timed_out || cnt < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
				DEBUGF(DNW2)( "Link_file_read: write %d to fd %d failed - %s",
					i, fd, Errormsg(err) );
#endif
				status = LINK_TRANSFER_FAIL; 
			}
		} else if( *count ){
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGF(DNW2)("Link_file_read: read from '%s' failed - %s",
				host, Errormsg(err) );
#endif
			status = LINK_TRANSFER_FAIL;
		} else {
			break;
		}
	}
#if JYDEBUG //JYWeng
aaaaaa=fopen("/tmp/a12345", "a");
fprintf(aaaaaa, "Linksupport: loop end!\n");
fclose(aaaaaa);
//close(fd_print);
#endif
	if( *count ){
		*count -= len;
	} else {
		*count = readcount;
	}

	if( *count && status == 0 && ack ){
		DEBUGF(DNW2)("Link_file_read: doing end marker byte read" );
		i = Read_fd_len_timeout(readtimeout, *sock, str, 1 );
		err = errno;

		if( Alarm_timed_out ){
			DEBUGF(DNW2)( "Link_file_read: end marker byte read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DEBUGF(DNW2)("Link_file_read: end marker read count %d value %d", i, *str );
			*ack = *str;
			if( *ack ){
				DEBUGF(DNW2)( "Link_file_read: nonzero end marker '%d'", *ack );
				status = LINK_ACK_FAIL;
			}
		} else if( i == 0 ){
			DEBUGF(DNW2)("Link_file_read: EOF and no end marker" );
		} else {
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGF(DNW2)("Link_file_read: end marker read from '%s' failed - %s",
				host, Errormsg(err) );
#endif
			status = LINK_TRANSFER_FAIL;
		}
	}

	DEBUGF(DNW2)("Link_file_read: status %d", status );
	return( status );
}
#endif

/***************************************************************************
 * int Link_file_read( char *host, int *sock, int readtimeout,
 *    int writetimeout, int fd, int *count, int *ack )
 *    reads and copies '*count' characters from socket to fd
 *    *count points to maximum number of bytes to read;
 *      updated with actual value read
 *    if ack then will read an additional ACK character
 *       returns value in *ack
 *    if read does not complete within timeout seconds,
 *      terminate action with error.
 *    if timeout == 0, wait indefinitely
 *    returns 0 *count not read
 *
 ***************************************************************************/

int Link_file_read(char *host, int *sock, int readtimeout, int writetimeout,
	  int fd, double *count, int *ack )
{
	char str[LARGEBUFFER];		/* input buffer pointer */
	int i, l, cnt;			/* number to read or write */
	int status;				/* status of operation */
	int err;					/* error */
	double len;
	double readcount;

	len = i = status = cnt = 0;	/* shut up GCC */
	readcount = 0;
	*ack = 0;
	DEBUGF(DNW1) ("Link_file_read: reading %0.0f from '%s' on %d",
		*count, host, *sock );
	/* check for valid socket */
	if(*sock < 0) {
		DEBUGF(DNW2)( "Link_file_read: bad socket" );
		return (LINK_OPEN_FAIL);
	}
	/*
	 * set up timeout and then do the transfer
	 */

	/* do the read */
	len = *count;
	while( status == 0 && (*count == 0 || len > 0) ){
		DEBUGF(DNW2)("Link_file_read: doing data read" );
		l = sizeof(str);
		if( *count && l > len ) l = len;
		i = Read_fd_len_timeout( readtimeout, *sock, str, l );
		err = errno;
		if( Alarm_timed_out ){
			DEBUGF(DNW2)( "Link_file_read: read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DEBUGF(DNW2)("Link_file_read: len %0.0f, readlen %d, read %d", len, l, i );
			if( *count ) len -= i;
			readcount += i;
//JY1111			cnt = Write_fd_len_timeout(writetimeout, fd, str, i );
sleep(10);
			err = errno;
			if( Alarm_timed_out || cnt < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
				DEBUGF(DNW2)( "Link_file_read: write %d to fd %d failed - %s",
					i, fd, Errormsg(err) );
#endif
				status = LINK_TRANSFER_FAIL; 
			}
		} else if( *count ){
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGF(DNW2)("Link_file_read: read from '%s' failed - %s",
				host, Errormsg(err) );
#endif
			status = LINK_TRANSFER_FAIL;
		} else {
			break;
		}
	}
	if( *count ){
		*count -= len;
	} else {
		*count = readcount;
	}

	if( *count && status == 0 && ack ){
		DEBUGF(DNW2)("Link_file_read: doing end marker byte read" );
		i = Read_fd_len_timeout(readtimeout, *sock, str, 1 );
		err = errno;

		if( Alarm_timed_out ){
			DEBUGF(DNW2)( "Link_file_read: end marker byte read from '%s' timed out", host);
			status = LINK_TRANSFER_FAIL;
		} else if( i > 0 ){
			DEBUGF(DNW2)("Link_file_read: end marker read count %d value %d", i, *str );
			*ack = *str;
			if( *ack ){
				DEBUGF(DNW2)( "Link_file_read: nonzero end marker '%d'", *ack );
				status = LINK_ACK_FAIL;
			}
		} else if( i == 0 ){
			DEBUGF(DNW2)("Link_file_read: EOF and no end marker" );
		} else {
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUGF(DNW2)("Link_file_read: end marker read from '%s' failed - %s",
				host, Errormsg(err) );
#endif
			status = LINK_TRANSFER_FAIL;
		}
	}

	DEBUGF(DNW2)("Link_file_read: status %d", status );
	return( status );
}

#undef PAIR
#ifndef _UNPROTO_
# define PAIR(X) { #X, X }
#else
# define __string(X) "X"
# define PAIR(X) { __string(X), X }
#endif


/* PAIR(LINK_OPEN_FAIL), */
/* PAIR(LINK_TRANSFER_FAIL), */
/* PAIR(LINK_ACK_FAIL), */
/* PAIR(LINK_FILE_READ_FAIL), */
/* PAIR(LINK_LONG_LINE_FAIL), */
/* PAIR(LINK_BIND_FAIL), */
/* PAIR(LINK_PERM_FAIL), */

 static struct link_err {
    char *str;
    int value;
} link_err[] = {
{ "NO ERROR", 0 },
{"ERROR OPENING CONNECTION",LINK_OPEN_FAIL},
{"ERROR TRANSFERRING DATA",LINK_TRANSFER_FAIL},
{"NONZERO RFC1179 ERROR CODE FROM SERVER",LINK_ACK_FAIL},
{"ERROR TRANSFERRING FILE",LINK_FILE_READ_FAIL},
{"COMMAND TOO LONG FOR RFC1179, POSSIBLE BUFFER OVERRUN ATTEMPT", LINK_LONG_LINE_FAIL},
{"CANNOT BIND TO PORT - POSSIBLE DENIAL OF SERVICE ATTACK", LINK_BIND_FAIL},
{"NO PERMISSIONS TO MANANGE SOCKET- MAY NEED SUID INSTALLATION", LINK_BIND_FAIL},
{0,0}
};

const char *Link_err_str (int n)
{
    int i;
    static char buf[40];
	const char *s = 0;

	/* DEBUG1("Link_err_str: %d\n", n ); */
	for( i = 0; link_err[i].str && link_err[i].value != n; ++i ){
		/* DEBUG1("Link_err_str: %d = '%s'\n", link_err[i].value,
			link_err[i].str	); */
	}
	/* DEBUG1("Link_err_str: %d = '%s'\n", link_err[i].value,
		link_err[i].str	); */
	s = link_err[i].str;
	if( s == 0 ){
		s = buf;
		(void) SNPRINTF (buf, sizeof(buf)) "link error %d", n);
	}
    return(s);
}

 static struct link_err ack_err[] = {
 PAIR(ACK_SUCCESS),
 PAIR(ACK_STOP_Q),
 PAIR(ACK_RETRY),
 PAIR(ACK_FAIL),
 {0,0}
};


const char *Ack_err_str (int n)
{
    int i;
    static char buf[40];
	const char *s = 0;

	for( i = 0; ack_err[i].str && ack_err[i].value != n; ++i );
	s = ack_err[i].str;
	if( s == 0 ){
		s = buf;
		(void) SNPRINTF (buf, sizeof(buf)) "ack error %d", n);
	}
    return(s);
}

int AF_Protocol(void)
{
	/* we now know if we are using IPV4 or IPV6 from configuration */
	int af_protocol = AF_INET;
#if defined(IPV6)
	if( IPV6Protocol_DYN ){
		af_protocol = AF_INET6;
# if defined(HAVE_RESOLV_H) && defined(RES_USE_INET6) && defined(HAVE_RES)
		_res.options |= RES_USE_INET6;
# endif
	}
#endif
	DEBUG4("AF_Protocol: af_protocol %d, AF_INET %d", af_protocol, AF_INET );
	return( af_protocol );
}

#if !defined(HAVE_INET_PTON)
/***************************************************************************
 * inet_pton( int family, const char *strptr, void *addrptr
 *  p means presentation, i.e. - ASCII C string
 *  n means numeric, i.e. - network byte ordered address
 * family = AF_Protocol or AF_Protocol6
 * strptr = string to convert
 * addrprt = destination
 ***************************************************************************/

 int inet_pton( int family, const char *strptr, void *addr )
{
	if( family != AF_INET ){
		FATAL(LOG_ERR) "inet_pton: bad family '%d'", family );
	}
#if defined(HAVE_INET_ATON)
	return( inet_aton( strptr, addr ) );
#else
#if !defined(INADDR_NONE)
#define INADDR_NONE 0xffffffff
#endif
	if( inet_addr( strptr ) != INADDR_NONE ){
		((struct in_addr *)addr)->s_addr = inet_addr(strptr);
		return(1);
	}
	return(0);
#endif
}
#endif


#if !defined(HAVE_INET_NTOP)
/***************************************************************************
 * inet_ntop( int family, const void *addr,
 *    const char *strptr, int len )
 *  p means presentation, i.e. - ASCII C string
 *  n means numeric, i.e. - network byte ordered address
 * family = AF_Protocol or AF_Protocol6
 * addr   = binary to convert
 * strptr = string where to place
 * len    = length
 ***************************************************************************/
 const char *inet_ntop( int family, const void *addr,
	char *str, size_t len )
{
	char *s;
	if( family != AF_INET ){
		FATAL(LOG_ERR) "inet_ntop: bad family '%d'", family );
	}
	s = inet_ntoa(((struct in_addr *)addr)[0]);
	mystrncpy( str, s, len );
	return(str);
}
#endif

/***************************************************************************
 * inet_ntop_sockaddr( struct sockaddr *addr,
 *    const char *strptr, int len )
 * get the address type and format it correctly
 *  p means presentation, i.e. - ASCII C string
 *  n means numeric, i.e. - network byte ordered address
 * family = AF_Protocol or AF_Protocol6
 * addr   = binary to convert
 * strptr = string where to place
 * len    = length
 ***************************************************************************/
const char *inet_ntop_sockaddr( struct sockaddr *addr,
	char *str, int len )
{
	void *a = 0;
	/* we get the printable from the socket address */
	if( addr->sa_family == AF_INET ){
		a = &((struct sockaddr_in *)addr)->sin_addr;
#if defined(IN6_ADDR)
	} else if( addr->sa_family == AF_INET6 ){
		a = &((struct sockaddr_in6 *)addr)->sin6_addr;
#endif
	} else if( addr->sa_family == 0 
#if defined(AF_LOCAL)
		|| addr->sa_family == AF_LOCAL 
#endif
#if defined(AF_UNIX)
		|| addr->sa_family == AF_UNIX 
#endif
		){
		SNPRINTF (str, len) "%s", Unix_socket_path_DYN );
		return( str );
	} else {
		FATAL(LOG_ERR) "inet_ntop_sockaddr: bad family '%d'",
			addr->sa_family );
	}
	return( inet_ntop( addr->sa_family, a, str, len ) );
}
