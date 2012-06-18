/* MTU get using kvm_read.
 * Copyright (C) 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include <kvm.h>
#include <limits.h>
#include <fcntl.h>

#include "if.h"

/* get interface MTU to use kvm_read */
void
if_kvm_get_mtu (struct interface *ifp)
{
  kvm_t *kvmd;
  struct ifnet ifnet;
  unsigned long ifnetaddr;
  int len;
 
  char ifname[IFNAMSIZ];
  char tname[INTERFACE_NAMSIZ + 1];
  char buf[_POSIX2_LINE_MAX];
 
  struct nlist nl[] = 
  {
    {"_ifnet"},
    {""}
  };

  ifp->mtu = -1;
  
  kvmd = kvm_openfiles (NULL, NULL, NULL, O_RDONLY, buf);

  if (kvmd == NULL) 
    return ;
  
  kvm_nlist(kvmd, nl);
 
  ifnetaddr = nl[0].n_value;
 
  if (kvm_read(kvmd, ifnetaddr, (char *)&ifnetaddr, sizeof ifnetaddr) < 0) 
    {
      kvm_close (kvmd);
      return ;
    }
 
  while(ifnetaddr != 0) 
    {
      if (kvm_read (kvmd, ifnetaddr, (char *)&ifnet, sizeof ifnet) < 0) 
	{
	  kvm_close (kvmd);
	  return ;
	}

      if (kvm_read (kvmd, (u_long)ifnet.if_name, ifname, IFNAMSIZ) < 0) 
	{
	  kvm_close (kvmd);
	  return ;
	}

      len = snprintf (tname, INTERFACE_NAMSIZ + 1, 
		      "%s%d", ifname, ifnet.if_unit);

      if (strncmp (tname, ifp->name, len) == 0)
	break;

      ifnetaddr = (u_long)ifnet.if_next;
    }

  kvm_close (kvmd);

  if (ifnetaddr == 0) 
    {
      return ;
    }

  ifp->mtu = ifnet.if_mtu;
}
