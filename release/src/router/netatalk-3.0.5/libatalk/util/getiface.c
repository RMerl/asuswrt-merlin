/* 
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 1999-2000 Adrian Sun. 
 * All Rights Reserved. See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef TRU64
#include <sys/mbuf.h>
#include <net/route.h>
#endif /* TRU64 */
#include <net/if.h>
#include <errno.h>

#ifdef __svr4__
#include <sys/sockio.h>
#endif

#include <atalk/util.h>

/* allocation size for interface list. */
#define IFACE_NUM 5

/* we leave all of the ioctl's to the application */
static int addname(char **list, int *i, const char *name) 

{
    /* if we've run out of room, allocate some more. just return
     * the present list if we can't. */
     
    if ((list[*i] = strdup(name)) == NULL)
      return -1;

    (*i)++;
    list[*i] = NULL; /* zero out the next entry */
    return 0;
}


static int getifaces(const int sockfd, char ***list)
{
#ifdef HAVE_IFNAMEINDEX
      struct if_nameindex *ifstart, *ifs;
      int i = 0;
	  char **new;
  
      ifs = ifstart = if_nameindex();

	  new = (char **) malloc((sizeof(ifs)/sizeof(struct if_nameindex) + 1) * sizeof(char *));
      while (ifs && ifs->if_name) {
	/* just bail if there's a problem */
	if (addname(new, &i, ifs->if_name) < 0)
	  break;
	ifs++;
      }

      if_freenameindex(ifstart);
	  *list = new;
      return i;

#else
    struct ifconf	ifc;
    struct ifreq	ifrs[ 64 ], *ifr, *nextifr;
    int			ifrsize, i = 0;
	char **new;

    if (!list)
      return 0;

    memset( &ifc, 0, sizeof( struct ifconf ));
    ifc.ifc_len = sizeof( ifrs );
    memset( ifrs, 0, sizeof( ifrs ));
    ifc.ifc_buf = (caddr_t)ifrs;
    if ( ioctl( sockfd, SIOCGIFCONF, &ifc ) < 0 ) {
	return 0;
    }

	new = (char **) malloc((ifc.ifc_len/sizeof(struct ifreq) + 1) * sizeof(char *));
    for ( ifr = ifc.ifc_req; ifc.ifc_len >= (int) sizeof( struct ifreq );
	    ifc.ifc_len -= ifrsize, ifr = nextifr ) {
#ifdef BSD4_4
 	ifrsize = sizeof(ifr->ifr_name) +
	  (ifr->ifr_addr.sa_len > sizeof(struct sockaddr)
	   ? ifr->ifr_addr.sa_len : sizeof(struct sockaddr));
#else /* !BSD4_4 */
	ifrsize = sizeof( struct ifreq );
#endif /* BSD4_4 */
	nextifr = (struct ifreq *)((caddr_t)ifr + ifrsize );

	/* just bail if there's a problem */
	if (addname(new, &i, ifr->ifr_name) < 0)
	  break;
    }
	*list = new;
    return i;
#endif
}


/*
 * Get interfaces from the kernel. we keep an extra null entry to signify
 * the end of the interface list. 
 */
char **getifacelist(void)
{
  char **list = NULL; /* FIXME */
  int  i, fd;

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    return NULL;

  if ((i = getifaces(fd, &list)) == 0) {
    free(list);
    close(fd);
    return NULL;
  }
  close(fd);

  return list;
}


/* go through and free the interface list */
void freeifacelist(char **ifacelist)
{
  char *value, **list = ifacelist;

  if (!ifacelist)
    return;

  while ((value = *list++)) {
    free(value);
  }

  free(ifacelist);
}
