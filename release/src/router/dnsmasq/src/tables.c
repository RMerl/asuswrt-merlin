/* tables.c is Copyright (c) 2014 Sven Falempin  All Rights Reserved.

   Author's email: sfalempin@citypassenger.com 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#if defined(HAVE_IPSET) && defined(HAVE_BSD_NETWORK)

#include <string.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>

#define UNUSED(x) (void)(x)

static char *pf_device = "/dev/pf";
static int dev = -1;

static char *pfr_strerror(int errnum)
{
  switch (errnum) 
    {
    case ESRCH:
      return "Table does not exist";
    case ENOENT:
      return "Anchor or Ruleset does not exist";
    default:
      return strerror(errnum);
    }
}


void ipset_init(void) 
{
  dev = open( pf_device, O_RDWR);
  if (dev == -1)
    {
      err(1, "%s", pf_device);
      die (_("failed to access pf devices: %s"), NULL, EC_MISC);
    }
}

int add_to_ipset(const char *setname, const struct all_addr *ipaddr,
		 int flags, int remove)
{
  struct pfr_addr addr;
  struct pfioc_table io;
  struct pfr_table table;

  if (dev == -1) 
    {
      my_syslog(LOG_ERR, _("warning: no opened pf devices %s"), pf_device);
      return -1;
    }

  bzero(&table, sizeof(struct pfr_table));
  table.pfrt_flags |= PFR_TFLAG_PERSIST;
  if (strlen(setname) >= PF_TABLE_NAME_SIZE)
    {
      my_syslog(LOG_ERR, _("error: cannot use table name %s"), setname);
      errno = ENAMETOOLONG;
      return -1;
    }
  
  if (strlcpy(table.pfrt_name, setname,
	      sizeof(table.pfrt_name)) >= sizeof(table.pfrt_name)) 
    {
      my_syslog(LOG_ERR, _("error: cannot strlcpy table name %s"), setname);
      return -1;
    }
  
  bzero(&io, sizeof io);
  io.pfrio_flags = 0;
  io.pfrio_buffer = &table;
  io.pfrio_esize = sizeof(table);
  io.pfrio_size = 1;
  if (ioctl(dev, DIOCRADDTABLES, &io))
    {
      my_syslog(LOG_WARNING, _("IPset: error:%s"), pfr_strerror(errno));
      
      return -1;
    }
  
  table.pfrt_flags &= ~PFR_TFLAG_PERSIST;
  if (io.pfrio_nadd)
    my_syslog(LOG_INFO, _("info: table created"));
 
  bzero(&addr, sizeof(addr));
#ifdef HAVE_IPV6
  if (flags & F_IPV6) 
    {
      addr.pfra_af = AF_INET6;
      addr.pfra_net = 0x80;
      memcpy(&(addr.pfra_ip6addr), &(ipaddr->addr), sizeof(struct in6_addr));
    } 
  else 
#endif
    {
      addr.pfra_af = AF_INET;
      addr.pfra_net = 0x20;
      addr.pfra_ip4addr.s_addr = ipaddr->addr.addr4.s_addr;
    }

  bzero(&io, sizeof(io));
  io.pfrio_flags = 0;
  io.pfrio_table = table;
  io.pfrio_buffer = &addr;
  io.pfrio_esize = sizeof(addr);
  io.pfrio_size = 1;
  if (ioctl(dev, ( remove ? DIOCRDELADDRS : DIOCRADDADDRS ), &io)) 
    {
      my_syslog(LOG_WARNING, _("warning: DIOCR%sADDRS: %s"), ( remove ? "DEL" : "ADD" ), pfr_strerror(errno));
      return -1;
    }
  
  my_syslog(LOG_INFO, _("%d addresses %s"),
            io.pfrio_nadd, ( remove ? "removed" : "added" ));
  
  return io.pfrio_nadd;
}


#endif
