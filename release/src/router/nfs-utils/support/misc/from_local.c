 /*
  * Check if an address belongs to the local system. Adapted from:
  * 
  * @(#)pmap_svc.c 1.32 91/03/11 Copyright 1984,1990 Sun Microsystems, Inc.
  * @(#)get_myaddress.c  2.1 88/07/29 4.0 RPCSRC.
  */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if 0
static char sccsid[] = "@(#) from_local.c 1.3 96/05/31 15:52:57";
#endif

#ifdef TEST
#undef perror
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

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
    char    buf[BUFSIZ];

    /*
     * Get list of network interfaces. We use a huge buffer to allow for the
     * presence of non-IP interfaces.
     */

    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	return (0);
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, (char *) &ifc) < 0) {
	perror("SIOCGIFCONF");
	(void) close(sock);
	return (0);
    }
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
    return (num_local);
}

/* from_local - determine whether request comes from the local system */
int
from_local(struct sockaddr_in *addr)
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
