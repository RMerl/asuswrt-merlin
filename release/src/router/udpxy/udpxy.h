/* @(#) common definitions for udpxy
 *
 * Copyright 2008-2012 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UDPXY_H_0110081654
#define UDPXY_H_0110081654

#include <sys/types.h>

/* application error codes
 *
 */
static const int ERR_PARAM      =  1;    /* invalid parameter(s) */
static const int ERR_REQ        =  2;    /* error parsing request */
static const int ERR_INTERNAL   =  3;    /* internal error */

static const int LQ_BACKLOG = 16;    /* server backlog value */
static const int RCV_LWMARK = 0;     /* low watermaek on the receiving (m-cast) socket */

/* max size of string with IPv4 address */
#define IPADDR_STR_SIZE 16

/* max size of string with TCP/UDP port */
#define PORT_STR_SIZE   6

/* max length of an HTTP command */
#define MAX_CMD_LEN     31

/* max length of a command parameter (address:port, etc.) */
#define MAX_PARAM_LEN   79

/* max length of a command's supplementary part (URI-embedded variables) */
#define MAX_TAIL_LEN    255

static const int    ETHERNET_MTU        = 1500;

/* socket timeouts in seconds */
#define RLY_SOCK_TIMEOUT   5
#define SRVSOCK_TIMEOUT    1
#define SSEL_TIMEOUT       1

/* time-out (sec) to hold buffered data
 * before sending/flushing to client(s) */
#define DHOLD_TIMEOUT      1

typedef u_short flag_t;
#if !defined( uf_TRUE ) && !defined( uf_FALSE )
    #define     uf_TRUE  ((flag_t)1)
    #define     uf_FALSE ((flag_t)0)
#else
    #error uf_TRUE or uf_FALSE already defined
#endif

#ifndef MAXPATHLEN
    #define MAXPATHLEN 1024
#endif

/* max size of string with IPv4 address */
#define IPADDR_STR_SIZE 16

typedef struct tmfd {
    int     fd;
    time_t  atime;
} tmfd_t;

#endif /* UDPXY_H_0110081654 */

/* __EOF__ */

