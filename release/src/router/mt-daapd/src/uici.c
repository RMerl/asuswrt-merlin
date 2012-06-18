/*
 * $Id: uici.c,v 1.1 2009-06-30 02:31:09 steven Exp $
 * Sockets Library
 *
 * ** NOTICE **
 *
 * This code is written by (and is therefore copyright) Dr Kay Robbins 
 * (krobbins@cs.utsa.edu) and Dr. Steve Robbins (srobbins@cs.utsa.edu), 
 * and was released with unspecified licensing as part of their book 
 * _UNIX_Systems_Programming_ (Prentice Hall, ISBN: 0130424110).
 *
 * Dr. Steve Robbins was kind enough to allow me to re-license this 
 * software as GPL.  I would request that any bugs or problems with 
 * this code be brought to my attention (ron@pedde.com), and I will 
 * submit appropriate patches upstream, should the problem be with
 * the original code.
 *
 * ** NOTICE **
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "uici.h"

#ifndef MAXBACKLOG
#define MAXBACKLOG 50
#endif

#ifndef INADDR_NONE
#  define INADDR_NONE 0xFFFFFFFFU
#endif


/*
 *                           u_igniore_sigpipe
 * Ignore SIGPIPE if the default action is in effect.
 *
 * returns: 0 if successful
 *          -1 on error and sets errno
 */
static int u_ignore_sigpipe() {
   struct sigaction act;

   if (sigaction(SIGPIPE, (struct sigaction *)NULL, &act) == -1)
      return -1;
   if (act.sa_handler == SIG_DFL) {
      act.sa_handler = SIG_IGN;
      if (sigaction(SIGPIPE, &act, (struct sigaction *)NULL) == -1)
         return -1;
   }   
   return 0;
}

/*
 *                           u_open
 * Return a file descriptor, which is bound to the given port.
 *
 * parameter:
 *        s = number of port to bind to
 * returns:  file descriptor if successful
 *           -1 on error and sets errno
 */
int u_open(u_port_t port) {
   int error;  
   struct sockaddr_in server;
   int sock;
   int true = 1;

   if ((u_ignore_sigpipe() == -1) ||
        ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1))
      return -1; 

   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&true,
                  sizeof(true)) == -1) {
      error = errno;
      while ((close(sock) == -1) && (errno == EINTR)); 
      errno = error;
      return -1;
   }
 
   server.sin_family = AF_INET;
   server.sin_addr.s_addr = htonl(INADDR_ANY);
   server.sin_port = htons((short)port);
   if ((bind(sock, (struct sockaddr *)&server, sizeof(server)) == -1) ||
        (listen(sock, MAXBACKLOG) == -1)) {
      error = errno;
      while ((close(sock) == -1) && (errno == EINTR)); 
      errno = error;
      return -1;
   }
   return sock;
}

/*
 *                           u_accept
 * Wait for a connection request from a host on a specified port.
 *
 * parameters:
 *      fd = file descriptor previously bound to listening port
 *      hostn = a buffer that will hold the name of the remote host
 *      hostnsize = size of hostn buffer
 * returns:  a communication file descriptor on success
 *              hostn is filled with the name of the remote host.
 *           -1 on error with errno set
 *
 * comments: This function is used by the server to wait for a
 * communication.  It blocks until a remote request is received
 * from the port bound to the given file descriptor.
 * hostn is filled with an ASCII string containing the remote
 * host name.  It must point to a buffer of size at least hostnsize.
 * If the name does not fit, as much of the name as is possible is put
 * into the buffer.
 * If hostn is NULL or hostnsize <= 0, no hostname is copied.
 */
int u_accept(int fd, char *hostn, int hostnsize) {
   int len = sizeof(struct sockaddr);
   struct sockaddr_in netclient;
   int retval;

   while (((retval =
           accept(fd, (struct sockaddr *)(&netclient), &len)) == -1) &&
          (errno == EINTR))
      ;  
   if ((retval == -1) || (hostn == NULL) || (hostnsize <= 0))
      return retval;
   
   strncpy(hostn,inet_ntoa(netclient.sin_addr),hostnsize);
   return retval;
}

/*
 *                           u_connect
 * Initiate communication with a remote server.
 *
 * parameters:
 *     port  = well-known port on remote server
 *     hostn = character string giving the Internet name of remote host
 * returns:  a communication file descriptor if successful
 *           -1 on error with errno set
 */
int u_connect(u_port_t port, char *hostn) {
   int error;
   int retval;
   struct sockaddr_in server;
   int sock;
   fd_set sockset;
   struct hostent *phe;


   /* fix broken name resolution for hosts beginning with a 
    * digit, plus get rid of hostname lookup stuff
    */
   if(inet_addr(hostn) == INADDR_NONE) {
       phe=gethostbyname(hostn);
       if(phe == NULL) {
	   errno = EINVAL;
	   return -1;
       }

       memcpy((char*)&server.sin_addr,(char*)(phe->h_addr_list[0]),
	      sizeof(struct in_addr));
   } else {
       server.sin_addr.s_addr=inet_addr(hostn);
   }

   server.sin_port = htons((short)port);
   server.sin_family = AF_INET;

   if ((u_ignore_sigpipe() == -1) ||
        ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1))
      return -1;

   if (((retval =
       connect(sock, (struct sockaddr *)&server, sizeof(server))) == -1) &&
       ((errno == EINTR) || (errno == EALREADY))) {
       FD_ZERO(&sockset);
       FD_SET(sock, &sockset);
       while ( ((retval = select(sock+1, NULL, &sockset, NULL, NULL)) == -1) &&
               (errno == EINTR) ) {
          FD_ZERO(&sockset);
          FD_SET(sock, &sockset);
       } 
   }
   if (retval == -1) {
        error = errno;
        while ((close(sock) == -1) && (errno == EINTR)); 
        errno = error;
        return -1;
   }
   return sock;
}
