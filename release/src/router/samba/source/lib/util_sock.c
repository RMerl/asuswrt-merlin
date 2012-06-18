/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

#ifdef WITH_SSL
#include <ssl.h>
#undef Realloc  /* SSLeay defines this and samba has a function of this name */
extern SSL  *ssl;
extern int  sslFd;
#endif  /* WITH_SSL */

extern int DEBUGLEVEL;

BOOL passive = False;

/* the client file descriptor */
int Client = -1;

/* the last IP received from */
struct in_addr lastip;

/* the last port received from */
int lastport=0;

int smb_read_error = 0;

/****************************************************************************
 Determine if a file descriptor is in fact a socket.
****************************************************************************/

BOOL is_a_socket(int fd)
{
  int v,l;
  l = sizeof(int);
  return(getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *)&v, &l) == 0);
}

enum SOCK_OPT_TYPES {OPT_BOOL,OPT_INT,OPT_ON};

struct
{
  char *name;
  int level;
  int option;
  int value;
  int opttype;
} socket_options[] = {
  {"SO_KEEPALIVE",      SOL_SOCKET,    SO_KEEPALIVE,    0,                 OPT_BOOL},
  {"SO_REUSEADDR",      SOL_SOCKET,    SO_REUSEADDR,    0,                 OPT_BOOL},
  {"SO_BROADCAST",      SOL_SOCKET,    SO_BROADCAST,    0,                 OPT_BOOL},
#ifdef TCP_NODELAY
  {"TCP_NODELAY",       IPPROTO_TCP,   TCP_NODELAY,     0,                 OPT_BOOL},
#endif
#ifdef IPTOS_LOWDELAY
  {"IPTOS_LOWDELAY",    IPPROTO_IP,    IP_TOS,          IPTOS_LOWDELAY,    OPT_ON},
#endif
#ifdef IPTOS_THROUGHPUT
  {"IPTOS_THROUGHPUT",  IPPROTO_IP,    IP_TOS,          IPTOS_THROUGHPUT,  OPT_ON},
#endif
#ifdef SO_REUSEPORT
  {"SO_REUSEPORT",      SOL_SOCKET,    SO_REUSEPORT,    0,                 OPT_BOOL},
#endif
#ifdef SO_SNDBUF
  {"SO_SNDBUF",         SOL_SOCKET,    SO_SNDBUF,       0,                 OPT_INT},
#endif
#ifdef SO_RCVBUF
  {"SO_RCVBUF",         SOL_SOCKET,    SO_RCVBUF,       0,                 OPT_INT},
#endif
#ifdef SO_SNDLOWAT
  {"SO_SNDLOWAT",       SOL_SOCKET,    SO_SNDLOWAT,     0,                 OPT_INT},
#endif
#ifdef SO_RCVLOWAT
  {"SO_RCVLOWAT",       SOL_SOCKET,    SO_RCVLOWAT,     0,                 OPT_INT},
#endif
#ifdef SO_SNDTIMEO
  {"SO_SNDTIMEO",       SOL_SOCKET,    SO_SNDTIMEO,     0,                 OPT_INT},
#endif
#ifdef SO_RCVTIMEO
  {"SO_RCVTIMEO",       SOL_SOCKET,    SO_RCVTIMEO,     0,                 OPT_INT},
#endif
  {NULL,0,0,0,0}};

/****************************************************************************
 Set user socket options.
****************************************************************************/

void set_socket_options(int fd, char *options)
{
	fstring tok;

	while (next_token(&options,tok," \t,", sizeof(tok))) {
		int ret=0,i;
		int value = 1;
		char *p;
		BOOL got_value = False;

		if ((p = strchr(tok,'='))) {
			*p = 0;
			value = atoi(p+1);
			got_value = True;
		}

		for (i=0;socket_options[i].name;i++)
			if (strequal(socket_options[i].name,tok))
				break;

		if (!socket_options[i].name) {
			DEBUG(0,("Unknown socket option %s\n",tok));
			continue;
		}

		switch (socket_options[i].opttype) {
		case OPT_BOOL:
		case OPT_INT:
			ret = setsockopt(fd,socket_options[i].level,
						socket_options[i].option,(char *)&value,sizeof(int));
			break;

		case OPT_ON:
			if (got_value)
				DEBUG(0,("syntax error - %s does not take a value\n",tok));

			{
				int on = socket_options[i].value;
				ret = setsockopt(fd,socket_options[i].level,
							socket_options[i].option,(char *)&on,sizeof(int));
			}
			break;	  
		}
      
		if (ret != 0)
			DEBUG(0,("Failed to set socket option %s (Error %s)\n",tok, strerror(errno) ));
	}
}

/****************************************************************************
 Close the socket communication.
****************************************************************************/

void close_sockets(void )
{
#ifdef WITH_SSL
  sslutil_disconnect(Client);
#endif /* WITH_SSL */

  close(Client);
  Client = -1;
}

/****************************************************************************
 Read from a socket.
****************************************************************************/

ssize_t read_udp_socket(int fd,char *buf,size_t len)
{
  ssize_t ret;
  struct sockaddr_in sock;
  int socklen;
  
  socklen = sizeof(sock);
  memset((char *)&sock,'\0',socklen);
  memset((char *)&lastip,'\0',sizeof(lastip));
  ret = (ssize_t)recvfrom(fd,buf,len,0,(struct sockaddr *)&sock,&socklen);
  if (ret <= 0) {
    DEBUG(2,("read socket failed. ERRNO=%s\n",strerror(errno)));
    return(0);
  }

  lastip = sock.sin_addr;
  lastport = ntohs(sock.sin_port);

  DEBUG(10,("read_udp_socket: lastip %s lastport %d read: %d\n",
             inet_ntoa(lastip), lastport, ret));

  return(ret);
}

/****************************************************************************
 Read data from a socket with a timout in msec.
 mincount = if timeout, minimum to read before returning
 maxcount = number to be read.
 time_out = timeout in milliseconds
****************************************************************************/

static ssize_t read_socket_with_timeout(int fd,char *buf,size_t mincnt,size_t maxcnt,unsigned int time_out)
{
  fd_set fds;
  int selrtn;
  ssize_t readret;
  size_t nread = 0;
  struct timeval timeout;

  /* just checking .... */
  if (maxcnt <= 0)
    return(0);

  smb_read_error = 0;

  /* Blocking read */
  if (time_out <= 0) {
    if (mincnt == 0) mincnt = maxcnt;

    while (nread < mincnt) {
#ifdef WITH_SSL
      if(fd == sslFd){
        readret = SSL_read(ssl, buf + nread, maxcnt - nread);
      }else{
        readret = read(fd, buf + nread, maxcnt - nread);
      }
#else /* WITH_SSL */
      readret = read(fd, buf + nread, maxcnt - nread);
#endif /* WITH_SSL */

      if (readret == 0) {
        DEBUG(5,("read_socket_with_timeout: blocking read. EOF from client.\n"));
        smb_read_error = READ_EOF;
        return -1;
      }

      if (readret == -1) {
        DEBUG(0,("read_socket_with_timeout: read error = %s.\n", strerror(errno) ));
        smb_read_error = READ_ERROR;
        return -1;
      }
      nread += readret;
    }
    return((ssize_t)nread);
  }
  
  /* Most difficult - timeout read */
  /* If this is ever called on a disk file and 
     mincnt is greater then the filesize then
     system performance will suffer severely as 
     select always returns true on disk files */

  /* Set initial timeout */
  timeout.tv_sec = (time_t)(time_out / 1000);
  timeout.tv_usec = (long)(1000 * (time_out % 1000));

  for (nread=0; nread < mincnt; ) {      
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
      
    selrtn = sys_select(fd+1,&fds,&timeout);

    /* Check if error */
    if(selrtn == -1) {
      /* something is wrong. Maybe the socket is dead? */
      DEBUG(0,("read_socket_with_timeout: timeout read. select error = %s.\n", strerror(errno) ));
      smb_read_error = READ_ERROR;
      return -1;
    }

    /* Did we timeout ? */
    if (selrtn == 0) {
      DEBUG(10,("read_socket_with_timeout: timeout read. select timed out.\n"));
      smb_read_error = READ_TIMEOUT;
      return -1;
    }
      
#ifdef WITH_SSL
    if(fd == sslFd){
      readret = SSL_read(ssl, buf + nread, maxcnt - nread);
    }else{
      readret = read(fd, buf + nread, maxcnt - nread);
    }
#else /* WITH_SSL */
    readret = read(fd, buf+nread, maxcnt-nread);
#endif /* WITH_SSL */

    if (readret == 0) {
      /* we got EOF on the file descriptor */
      DEBUG(5,("read_socket_with_timeout: timeout read. EOF from client.\n"));
      smb_read_error = READ_EOF;
      return -1;
    }

    if (readret == -1) {
      /* the descriptor is probably dead */
      DEBUG(0,("read_socket_with_timeout: timeout read. read error = %s.\n", strerror(errno) ));
      smb_read_error = READ_ERROR;
      return -1;
    }
      
    nread += readret;
  }

  /* Return the number we got */
  return((ssize_t)nread);
}

/****************************************************************************
 Read data from a fd with a timout in msec.
 mincount = if timeout, minimum to read before returning
 maxcount = number to be read.
 time_out = timeout in milliseconds
****************************************************************************/

ssize_t read_with_timeout(int fd,char *buf,size_t mincnt,size_t maxcnt,unsigned int time_out)
{
  fd_set fds;
  int selrtn;
  ssize_t readret;
  size_t nread = 0;
  struct timeval timeout;

  /* just checking .... */
  if (maxcnt <= 0)
    return(0);

  /* Blocking read */
  if (time_out <= 0) {
    if (mincnt == 0) mincnt = maxcnt;

    while (nread < mincnt) {
#ifdef WITH_SSL
      if(fd == sslFd){
        readret = SSL_read(ssl, buf + nread, maxcnt - nread);
      }else{
        readret = read(fd, buf + nread, maxcnt - nread);
      }
#else /* WITH_SSL */
      readret = read(fd, buf + nread, maxcnt - nread);
#endif /* WITH_SSL */

      if (readret <= 0)
	return readret;

      nread += readret;
    }
    return((ssize_t)nread);
  }
  
  /* Most difficult - timeout read */
  /* If this is ever called on a disk file and 
     mincnt is greater then the filesize then
     system performance will suffer severely as 
     select always returns true on disk files */

  /* Set initial timeout */
  timeout.tv_sec = (time_t)(time_out / 1000);
  timeout.tv_usec = (long)(1000 * (time_out % 1000));

  for (nread=0; nread < mincnt; ) {      
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
      
    selrtn = sys_select(fd+1,&fds,&timeout);

    if(selrtn <= 0)
      return selrtn;
      
#ifdef WITH_SSL
    if(fd == sslFd){
      readret = SSL_read(ssl, buf + nread, maxcnt - nread);
    }else{
      readret = read(fd, buf + nread, maxcnt - nread);
    }
#else /* WITH_SSL */
    readret = read(fd, buf+nread, maxcnt-nread);
#endif /* WITH_SSL */

    if (readret <= 0)
      return readret;

    nread += readret;
  }

  /* Return the number we got */
  return((ssize_t)nread);
}

/****************************************************************************
send a keepalive packet (rfc1002)
****************************************************************************/

BOOL send_keepalive(int client)
{
  unsigned char buf[4];

  buf[0] = 0x85;
  buf[1] = buf[2] = buf[3] = 0;

  return(write_socket_data(client,(char *)buf,4) == 4);
}

/****************************************************************************
  read data from the client, reading exactly N bytes. 
****************************************************************************/

ssize_t read_data(int fd,char *buffer,size_t N)
{
  ssize_t  ret;
  size_t total=0;  
 
  smb_read_error = 0;

  while (total < N)
  {
#ifdef WITH_SSL
    if(fd == sslFd){
      ret = SSL_read(ssl, buffer + total, N - total);
    }else{
      ret = read(fd,buffer + total,N - total);
    }
#else /* WITH_SSL */
    ret = read(fd,buffer + total,N - total);
#endif /* WITH_SSL */

    if (ret == 0)
    {
      DEBUG(10,("read_data: read of %d returned 0. Error = %s\n", (int)(N - total), strerror(errno) ));
      smb_read_error = READ_EOF;
      return 0;
    }
    if (ret == -1)
    {
      DEBUG(0,("read_data: read failure for %d. Error = %s\n", (int)(N - total), strerror(errno) ));
      smb_read_error = READ_ERROR;
      return -1;
    }
    total += ret;
  }
  return (ssize_t)total;
}

/****************************************************************************
 Read data from a socket, reading exactly N bytes. 
****************************************************************************/

static ssize_t read_socket_data(int fd,char *buffer,size_t N)
{
  ssize_t  ret;
  size_t total=0;  
 
  smb_read_error = 0;

  while (total < N)
  {
#ifdef WITH_SSL
    if(fd == sslFd){
      ret = SSL_read(ssl, buffer + total, N - total);
    }else{
      ret = read(fd,buffer + total,N - total);
    }
#else /* WITH_SSL */
    ret = read(fd,buffer + total,N - total);
#endif /* WITH_SSL */

    if (ret == 0)
    {
      DEBUG(10,("read_socket_data: recv of %d returned 0. Error = %s\n", (int)(N - total), strerror(errno) ));
      smb_read_error = READ_EOF;
      return 0;
    }
    if (ret == -1)
    {
      DEBUG(0,("read_socket_data: recv failure for %d. Error = %s\n", (int)(N - total), strerror(errno) ));
      smb_read_error = READ_ERROR;
      return -1;
    }
    total += ret;
  }
  return (ssize_t)total;
}

/****************************************************************************
 Write data to a fd.
****************************************************************************/

ssize_t write_data(int fd,char *buffer,size_t N)
{
  size_t total=0;
  ssize_t ret;

  while (total < N)
  {
#ifdef WITH_SSL
    if(fd == sslFd){
      ret = SSL_write(ssl,buffer + total,N - total);
    }else{
      ret = write(fd,buffer + total,N - total);
    }
#else /* WITH_SSL */
    ret = write(fd,buffer + total,N - total);
#endif /* WITH_SSL */

    if (ret == -1) {
      DEBUG(0,("write_data: write failure. Error = %s\n", strerror(errno) ));
      return -1;
    }
    if (ret == 0) return total;

    total += ret;
  }
  return (ssize_t)total;
}

/****************************************************************************
 Write data to a socket - use send rather than write.
****************************************************************************/

ssize_t write_socket_data(int fd,char *buffer,size_t N)
{
  size_t total=0;
  ssize_t ret;

  while (total < N)
  {
#ifdef WITH_SSL
    if(fd == sslFd){
      ret = SSL_write(ssl,buffer + total,N - total);
    }else{
      ret = send(fd,buffer + total,N - total, 0);
    }
#else /* WITH_SSL */
    ret = send(fd,buffer + total,N - total,0);
#endif /* WITH_SSL */

    if (ret == -1) {
      DEBUG(0,("write_socket_data: write failure. Error = %s\n", strerror(errno) ));
      return -1;
    }
    if (ret == 0) return total;

    total += ret;
  }
  return (ssize_t)total;
}

/****************************************************************************
write to a socket
****************************************************************************/

ssize_t write_socket(int fd,char *buf,size_t len)
{
  ssize_t ret=0;

  if (passive)
    return(len);
  DEBUG(6,("write_socket(%d,%d)\n",fd,(int)len));
  ret = write_socket_data(fd,buf,len);
      
  DEBUG(6,("write_socket(%d,%d) wrote %d\n",fd,(int)len,(int)ret));
  if(ret <= 0)
    DEBUG(0,("write_socket: Error writing %d bytes to socket %d: ERRNO = %s\n", 
       (int)len, fd, strerror(errno) ));

  return(ret);
}

/****************************************************************************
read 4 bytes of a smb packet and return the smb length of the packet
store the result in the buffer
This version of the function will return a length of zero on receiving
a keepalive packet.
timeout is in milliseconds.
****************************************************************************/

static ssize_t read_smb_length_return_keepalive(int fd,char *inbuf,unsigned int timeout)
{
  ssize_t len=0;
  int msg_type;
  BOOL ok = False;

  while (!ok)
  {
    if (timeout > 0)
      ok = (read_socket_with_timeout(fd,inbuf,4,4,timeout) == 4);
    else 
      ok = (read_socket_data(fd,inbuf,4) == 4);

    if (!ok)
      return(-1);

    len = smb_len(inbuf);
    msg_type = CVAL(inbuf,0);

    if (msg_type == 0x85) 
      DEBUG(5,("Got keepalive packet\n"));
  }

  DEBUG(10,("got smb length of %d\n",len));

  return(len);
}

/****************************************************************************
read 4 bytes of a smb packet and return the smb length of the packet
store the result in the buffer. This version of the function will
never return a session keepalive (length of zero).
timeout is in milliseconds.
****************************************************************************/

ssize_t read_smb_length(int fd,char *inbuf,unsigned int timeout)
{
  ssize_t len;

  for(;;)
  {
    len = read_smb_length_return_keepalive(fd, inbuf, timeout);

    if(len < 0)
      return len;

    /* Ignore session keepalives. */
    if(CVAL(inbuf,0) != 0x85)
      break;
  }

  DEBUG(10,("read_smb_length: got smb length of %d\n",len));

  return len;
}

/****************************************************************************
  read an smb from a fd. Note that the buffer *MUST* be of size
  BUFFER_SIZE+SAFETY_MARGIN.
  The timeout is in milliseconds. 
  This function will return on a
  receipt of a session keepalive packet.
****************************************************************************/

BOOL receive_smb(int fd,char *buffer, unsigned int timeout)
{
  ssize_t len,ret;

  smb_read_error = 0;

  memset(buffer,'\0',smb_size + 100);

  len = read_smb_length_return_keepalive(fd,buffer,timeout);
	if (len < 0) {
    DEBUG(10,("receive_smb: length < 0!\n"));
    return(False);
  }

	/*
	 * A WRITEX with CAP_LARGE_WRITEX can be 64k worth of data plus 65 bytes
     * of header. Don't print the error if this fits.... JRA.
	 */

	if (len > (BUFFER_SIZE + LARGE_WRITEX_HDR_SIZE)) {
    DEBUG(0,("Invalid packet length! (%d bytes).\n",len));
    if (len > BUFFER_SIZE + (SAFETY_MARGIN/2))
	exit(1);
    }

  if(len > 0) {
    ret = read_socket_data(fd,buffer+4,len);
    if (ret != len) {
      smb_read_error = READ_ERROR;
      return False;
    }
  }
  return(True);
}

/****************************************************************************
  read an smb from a fd ignoring all keepalive packets. Note that the buffer 
  *MUST* be of size BUFFER_SIZE+SAFETY_MARGIN.
  The timeout is in milliseconds

  This is exactly the same as receive_smb except that it never returns
  a session keepalive packet (just as receive_smb used to do).
  receive_smb was changed to return keepalives as the oplock processing means this call
  should never go into a blocking read.
****************************************************************************/

BOOL client_receive_smb(int fd,char *buffer, unsigned int timeout)
{
  BOOL ret;

  for(;;)
  {
    ret = receive_smb(fd, buffer, timeout);

    if (!ret)
    {
      DEBUG(10,("client_receive_smb failed\n"));
      show_msg(buffer);
      return ret;
    }

    /* Ignore session keepalive packets. */
    if(CVAL(buffer,0) != 0x85)
      break;
  }
  show_msg(buffer);
  return ret;
}

/****************************************************************************
  send an null session message to a fd
****************************************************************************/

BOOL send_null_session_msg(int fd)
{
  ssize_t ret;
  uint32 blank = 0;
  size_t len = 4;
  size_t nwritten=0;
  char *buffer = (char *)&blank;

  while (nwritten < len)
  {
    ret = write_socket(fd,buffer+nwritten,len - nwritten);
    if (ret <= 0)
    {
      DEBUG(0,("send_null_session_msg: Error writing %d bytes to client. %d. Exiting\n",(int)len,(int)ret));
      close_sockets();
      exit(1);
    }
    nwritten += ret;
  }

  DEBUG(10,("send_null_session_msg: sent 4 null bytes to client.\n"));
  return True;
}

/****************************************************************************
  send an smb to a fd 
****************************************************************************/

BOOL send_smb(int fd,char *buffer)
{
  size_t len;
  size_t nwritten=0;
  ssize_t ret;
  len = smb_len(buffer) + 4;

  while (nwritten < len)
  {
    ret = write_socket(fd,buffer+nwritten,len - nwritten);
    if (ret <= 0)
    {
      DEBUG(0,("Error writing %d bytes to client. %d. Exiting\n",(int)len,(int)ret));
      close_sockets();
      exit(1);
    }
    nwritten += ret;
  }

  return True;
}

/****************************************************************************
send a single packet to a port on another machine
****************************************************************************/

BOOL send_one_packet(char *buf,int len,struct in_addr ip,int port,int type)
{
  BOOL ret;
  int out_fd;
  struct sockaddr_in sock_out;

  if (passive)
    return(True);

  /* create a socket to write to */
  out_fd = socket(AF_INET, type, 0);
  if (out_fd == -1) 
    {
      DEBUG(0,("socket failed"));
      return False;
    }

  /* set the address and port */
  memset((char *)&sock_out,'\0',sizeof(sock_out));
  putip((char *)&sock_out.sin_addr,(char *)&ip);
  sock_out.sin_port = htons( port );
  sock_out.sin_family = AF_INET;
  
  if (DEBUGLEVEL > 0)
    DEBUG(3,("sending a packet of len %d to (%s) on port %d of type %s\n",
	     len,inet_ntoa(ip),port,type==SOCK_DGRAM?"DGRAM":"STREAM"));
	
  /* send it */
  ret = (sendto(out_fd,buf,len,0,(struct sockaddr *)&sock_out,sizeof(sock_out)) >= 0);

  if (!ret)
    DEBUG(0,("Packet send to %s(%d) failed ERRNO=%s\n",
	     inet_ntoa(ip),port,strerror(errno)));

  close(out_fd);
  return(ret);
}

/****************************************************************************
open a socket of the specified type, port and address for incoming data
****************************************************************************/

int open_socket_in(int type, int port, int dlevel,uint32 socket_addr, BOOL rebind)
{
  struct hostent *hp;
  struct sockaddr_in sock;
  pstring host_name;
  int res;

  /* get my host name */
  if (gethostname(host_name, MAXHOSTNAMELEN) == -1) 
    { DEBUG(0,("gethostname failed\n")); return -1; } 

  /* get host info */
  if ((hp = Get_Hostbyname(host_name)) == 0) 
    {
      DEBUG(0,( "Get_Hostbyname: Unknown host %s\n",host_name));
      return -1;
    }
  
  memset((char *)&sock,'\0',sizeof(sock));
  memcpy((char *)&sock.sin_addr,(char *)hp->h_addr, hp->h_length);

#ifdef HAVE_SOCK_SIN_LEN
  sock.sin_len = sizeof(sock);
#endif
  sock.sin_port = htons( port );
  sock.sin_family = hp->h_addrtype;
  sock.sin_addr.s_addr = socket_addr;
  res = socket(hp->h_addrtype, type, 0);
  if (res == -1) 
    { DEBUG(0,("socket failed\n")); return -1; }

  {
    int val=1;
	if(rebind)
		val=1;
	else
		val=0;
    if(setsockopt(res,SOL_SOCKET,SO_REUSEADDR,(char *)&val,sizeof(val)) == -1)
		DEBUG(dlevel,("setsockopt: SO_REUSEADDR=%d on port %d failed with error = %s\n",
			val, port, strerror(errno) ));
#ifdef SO_REUSEPORT
    if(setsockopt(res,SOL_SOCKET,SO_REUSEPORT,(char *)&val,sizeof(val)) == -1)
		DEBUG(dlevel,("setsockopt: SO_REUSEPORT=%d on port %d failed with error = %s\n",
			val, port, strerror(errno) ));
#endif /* SO_REUSEPORT */
  }

  /* now we've got a socket - we need to bind it */
  if (bind(res, (struct sockaddr * ) &sock,sizeof(sock)) < 0) 
    { 
      if (port) {
	if (port == SMB_PORT || port == NMB_PORT)
	  DEBUG(dlevel,("bind failed on port %d socket_addr=%s (%s)\n",
			port,inet_ntoa(sock.sin_addr),strerror(errno))); 
	close(res); 

	if (dlevel > 0 && port < 1000)
	  port = 7999;

	if (port >= 1000 && port < 9000)
	  return(open_socket_in(type,port+1,dlevel,socket_addr,rebind));
      }

      return(-1); 
    }
  DEBUG(3,("bind succeeded on port %d\n",port));

  return res;
}

/****************************************************************************
  create an outgoing socket. timeout is in milliseconds.
  **************************************************************************/

int open_socket_out(int type, struct in_addr *addr, int port ,int timeout)
{
  struct sockaddr_in sock_out;
  int res,ret;
  int connect_loop = 250; /* 250 milliseconds */
  int loops = (timeout) / connect_loop;

  /* create a socket to write to */
  res = socket(PF_INET, type, 0);
  if (res == -1) 
    { DEBUG(0,("socket error\n")); return -1; }

  if (type != SOCK_STREAM) return(res);
  
  memset((char *)&sock_out,'\0',sizeof(sock_out));
  putip((char *)&sock_out.sin_addr,(char *)addr);
  
  sock_out.sin_port = htons( port );
  sock_out.sin_family = PF_INET;

  /* set it non-blocking */
  set_blocking(res,False);

  DEBUG(3,("Connecting to %s at port %d\n",inet_ntoa(*addr),port));
  
  /* and connect it to the destination */
connect_again:
  ret = connect(res,(struct sockaddr *)&sock_out,sizeof(sock_out));

  /* Some systems return EAGAIN when they mean EINPROGRESS */
  if (ret < 0 && (errno == EINPROGRESS || errno == EALREADY ||
        errno == EAGAIN) && loops--) {
    msleep(connect_loop);
    goto connect_again;
  }

  if (ret < 0 && (errno == EINPROGRESS || errno == EALREADY ||
         errno == EAGAIN)) {
      DEBUG(1,("timeout connecting to %s:%d\n",inet_ntoa(*addr),port));
      close(res);
      return -1;
  }

#ifdef EISCONN
  if (ret < 0 && errno == EISCONN) {
    errno = 0;
    ret = 0;
  }
#endif

  if (ret < 0) {
    DEBUG(1,("error connecting to %s:%d (%s)\n",
	     inet_ntoa(*addr),port,strerror(errno)));
    close(res);
    return -1;
  }

  /* set it blocking again */
  set_blocking(res,True);

  return res;
}


/*******************************************************************
 Reset the 'done' variables so after a client process is created
 from a fork call these calls will be re-done. This should be
 expanded if more variables need reseting.
 ******************************************************************/

static BOOL global_client_name_done = False;
static BOOL global_client_addr_done = False;

void reset_globals_after_fork(void)
{
  global_client_name_done = False;
  global_client_addr_done = False;

  /*
   * Re-seed the random crypto generator, so all smbd's
   * started from the same parent won't generate the same
   * sequence.
   */
  {
    unsigned char dummy;
    generate_random_buffer( &dummy, 1, True);
  } 
}
 
/*******************************************************************
 return the DNS name of the client 
 ******************************************************************/

char *client_name(int fd)
{
	struct sockaddr sa;
	struct sockaddr_in *sockin = (struct sockaddr_in *) (&sa);
	int     length = sizeof(sa);
	static pstring name_buf;
	struct hostent *hp;
	static int last_fd=-1;
	
	if (global_client_name_done && last_fd == fd) 
		return name_buf;
	
	last_fd = fd;
	global_client_name_done = False;
	
	pstrcpy(name_buf,"UNKNOWN");
	
	if (fd == -1) {
		return name_buf;
	}
	
	if (getpeername(fd, &sa, &length) < 0) {
		DEBUG(0,("getpeername failed. Error was %s\n", strerror(errno) ));
		return name_buf;
	}
	
	/* Look up the remote host name. */
	if ((hp = gethostbyaddr((char *) &sockin->sin_addr,
				sizeof(sockin->sin_addr),
				AF_INET)) == 0) {
		DEBUG(1,("Gethostbyaddr failed for %s\n",client_addr(fd)));
		StrnCpy(name_buf,client_addr(fd),sizeof(name_buf) - 1);
	} else {
		StrnCpy(name_buf,(char *)hp->h_name,sizeof(name_buf) - 1);
		if (!matchname(name_buf, sockin->sin_addr)) {
			DEBUG(0,("Matchname failed on %s %s\n",name_buf,client_addr(fd)));
			pstrcpy(name_buf,"UNKNOWN");
		}
	}
	global_client_name_done = True;
	return name_buf;
}

/*******************************************************************
 return the IP addr of the client as a string 
 ******************************************************************/

char *client_addr(int fd)
{
	struct sockaddr sa;
	struct sockaddr_in *sockin = (struct sockaddr_in *) (&sa);
	int     length = sizeof(sa);
	static fstring addr_buf;
	static int last_fd = -1;

	if (global_client_addr_done && fd == last_fd) 
		return addr_buf;

	last_fd = fd;
	global_client_addr_done = False;

	fstrcpy(addr_buf,"0.0.0.0");

	if (fd == -1) {
		return addr_buf;
	}
	
	if (getpeername(fd, &sa, &length) < 0) {
		DEBUG(0,("getpeername failed. Error was %s\n", strerror(errno) ));
		return addr_buf;
	}
	
	fstrcpy(addr_buf,(char *)inet_ntoa(sockin->sin_addr));
	
	global_client_addr_done = True;
	return addr_buf;
}
