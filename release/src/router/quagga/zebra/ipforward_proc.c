/*
 * Fetch ipforward value by reading /proc filesystem.
 * Copyright (C) 1997 Kunihiro Ishiguro
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

#include "log.h"
#include "privs.h"

#include "zebra/ipforward.h"

extern struct zebra_privs_t zserv_privs;

char proc_net_snmp[] = "/proc/net/snmp";

static void
dropline (FILE *fp)
{
  int c;

  while ((c = getc (fp)) != '\n')
    ;
}

int
ipforward (void)
{
  FILE *fp;
  int ipforwarding = 0;
  char buf[10];

  fp = fopen (proc_net_snmp, "r");

  if (fp == NULL)
    return -1;

  /* We don't care about the first line. */
  dropline (fp);
  
  /* Get ip_statistics.IpForwarding : 
     1 => ip forwarding enabled 
     2 => ip forwarding off. */
  if (fgets (buf, 6, fp))
    sscanf (buf, "Ip: %d", &ipforwarding);

  fclose(fp);
  
  if (ipforwarding == 1)
    return 1;

  return 0;
}

/* char proc_ipv4_forwarding[] = "/proc/sys/net/ipv4/conf/all/forwarding"; */
char proc_ipv4_forwarding[] = "/proc/sys/net/ipv4/ip_forward";

int
ipforward_on (void)
{
  FILE *fp;
  
  if ( zserv_privs.change(ZPRIVS_RAISE) )
  	zlog_err ("Can't raise privileges, %s", safe_strerror (errno) );

  fp = fopen (proc_ipv4_forwarding, "w");

  if (fp == NULL) {
    if ( zserv_privs.change(ZPRIVS_LOWER) )
      zlog_err ("Can't lower privileges, %s", safe_strerror (errno));
    return -1;
  }

  fprintf (fp, "1\n");

  fclose (fp);

  if ( zserv_privs.change(ZPRIVS_LOWER) )
    zlog_err ("Can't lower privileges, %s", safe_strerror (errno));

  return ipforward ();
}

int
ipforward_off (void)
{
  FILE *fp;

  if ( zserv_privs.change(ZPRIVS_RAISE) )
  	zlog_err ("Can't raise privileges, %s", safe_strerror (errno));

  fp = fopen (proc_ipv4_forwarding, "w");

  if (fp == NULL) {
    if ( zserv_privs.change(ZPRIVS_LOWER) )
      zlog_err ("Can't lower privileges, %s", safe_strerror (errno));
    return -1;
  }

  fprintf (fp, "0\n");

  fclose (fp);

  if ( zserv_privs.change(ZPRIVS_LOWER) )
    zlog_err ("Can't lower privileges, %s", safe_strerror (errno));

  return ipforward ();
}
#ifdef HAVE_IPV6

char proc_ipv6_forwarding[] = "/proc/sys/net/ipv6/conf/all/forwarding";

int
ipforward_ipv6 (void)
{
  FILE *fp;
  char buf[5];
  int ipforwarding = 0;

  fp = fopen (proc_ipv6_forwarding, "r");

  if (fp == NULL)
    return -1;

  if (fgets (buf, 2, fp))
    sscanf (buf, "%d", &ipforwarding);

  fclose (fp);
  return ipforwarding;
}

int
ipforward_ipv6_on (void)
{
  FILE *fp;

  if ( zserv_privs.change(ZPRIVS_RAISE) )
  	zlog_err ("Can't raise privileges, %s", safe_strerror (errno));

  fp = fopen (proc_ipv6_forwarding, "w");

  if (fp == NULL) {
    if ( zserv_privs.change(ZPRIVS_LOWER) )
      zlog_err ("Can't lower privileges, %s", safe_strerror (errno));
    return -1;
  }

  fprintf (fp, "1\n");

  fclose (fp);

  if ( zserv_privs.change(ZPRIVS_LOWER) )
    zlog_err ("Can't lower privileges, %s", safe_strerror (errno));

  return ipforward_ipv6 ();
}

int
ipforward_ipv6_off (void)
{
  FILE *fp;

  if ( zserv_privs.change(ZPRIVS_RAISE) )
  	zlog_err ("Can't raise privileges, %s", safe_strerror (errno));

  fp = fopen (proc_ipv6_forwarding, "w");

  if (fp == NULL) {
    if ( zserv_privs.change(ZPRIVS_LOWER) )
      zlog_err ("Can't lower privileges, %s", safe_strerror (errno));
    return -1;
  }

  fprintf (fp, "0\n");

  fclose (fp);

  if ( zserv_privs.change(ZPRIVS_LOWER) )
    zlog_err ("Can't lower privileges, %s", safe_strerror (errno));

  return ipforward_ipv6 ();
}
#endif /* HAVE_IPV6 */
