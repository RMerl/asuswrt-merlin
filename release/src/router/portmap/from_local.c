 /*
  * Check if an address belongs to the local system. Adapted from:
  * 
  * pmap_svc.c 1.32 91/03/11 Copyright 1984,1990 Sun Microsystems, Inc.
  * get_myaddress.c  2.1 88/07/29 4.0 RPCSRC.
  */

/*
 * Copyright (c) 2009, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Sun Microsystems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#ifndef TRUE
#define	TRUE	1
#define FALSE	0
#endif

 /*
  * With virtual hosting, each hardware network interface can have multiple
  * network addresses. On such machines the number of machine addresses can
  * be surprisingly large.
  */
static int num_local;
static int num_addrs;
static struct in_addr *addrs;

/* grow_addrs - extend list of local interface addresses */

static int grow_addrs(void)
{
    struct in_addr *new_addrs;
    int     new_num;

    /*
     * Keep the previous result if we run out of memory. The system would
     * really get hosed if we simply give up.
     */
    new_num = (addrs == 0) ? 1 : num_addrs + num_addrs;
    new_addrs = (struct in_addr *) malloc(sizeof(*addrs) * new_num);
    if (new_addrs == 0) {
	perror("portmap: out of memory");
	return (0);
    } else {
	if (addrs != 0) {
	    memcpy((char *) new_addrs, (char *) addrs,
		   sizeof(*addrs) * num_addrs);
	    free((char *) addrs);
	}
	num_addrs = new_num;
	addrs = new_addrs;
	return (1);
    }
}

/* find_local - find all IP addresses for this host */

static int
find_local(void)
{
    struct ifconf ifc;
    struct ifreq ifreq;
    struct ifreq *ifr;
    struct ifreq *the_end;
    int     sock;
    char    *buf=NULL;

    /*
     * Get list of network interfaces. We use a huge buffer to allow for the
     * presence of non-IP interfaces.
     */

    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	return (0);
    }

    num_local=0;
    do {
	num_local++;
	buf=realloc(buf,num_local * BUFSIZ);
	if(buf == NULL) {
	    perror("portmap: out of memory");
	    (void) close(sock);
	    num_local=0;
	    return (0);
	}
	ifc.ifc_len = (num_local * BUFSIZ);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, (char *) &ifc) < 0) {
	    perror("SIOCGIFCONF");
	    (void) close(sock);
	    free(buf);
	    num_local=0;
	    return (0);
	}
    } while (ifc.ifc_len > ((num_local * BUFSIZ)-sizeof(struct ifreq)));
    /* Get IP address of each active IP network interface. */

    the_end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    num_local = 0;
    for (ifr = ifc.ifc_req; ifr < the_end; ifr++) {
	if (ifr->ifr_addr.sa_family == AF_INET) {	/* IP net interface */
	    ifreq = *ifr;
	    if (ioctl(sock, SIOCGIFFLAGS, (char *) &ifreq) < 0) {
		perror("SIOCGIFFLAGS");
	    } else if (ifreq.ifr_flags & IFF_UP) {	/* active interface */
		if (ioctl(sock, SIOCGIFADDR, (char *) &ifreq) < 0) {
		    perror("SIOCGIFADDR");
		} else {
		    if (num_local >= num_addrs)
			if (grow_addrs() == 0)
			    break;
		    addrs[num_local++] = ((struct sockaddr_in *)
					  & ifreq.ifr_addr)->sin_addr;
		}
	    }
	}
	/* Support for variable-length addresses. */
#ifdef HAS_SA_LEN
	ifr = (struct ifreq *) ((caddr_t) ifr
		      + ifr->ifr_addr.sa_len - sizeof(struct sockaddr));
#endif
    }
    (void) close(sock);
    free(buf);
    return (num_local);
}

/* from_local - determine whether request comes from the local system */

int from_local(struct sockaddr_in *addr)
{
    int     i;

    if (addrs == 0 && find_local() == 0)
	syslog(LOG_ERR, "cannot find any active local network interfaces");

    for (i = 0; i < num_local; i++) {
	if (memcmp((char *) &(addr->sin_addr), (char *) &(addrs[i]),
		   sizeof(struct in_addr)) == 0)
	    return (TRUE);
    }
    return (FALSE);
}

#ifdef TEST

main()
{
    char   *inet_ntoa();
    int     i;

    find_local();
    for (i = 0; i < num_local; i++)
	printf("%s\n", inet_ntoa(addrs[i]));
}

#endif
