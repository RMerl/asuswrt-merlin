/*
 * wol - wake on lan client
 *
 * parse a macfile and return its tokens
 * 
 * $Id: macfile.c,v 1.8 2004/02/05 18:15:15 wol Exp $
 *
 * Copyright (C) 2000,2001,2002,2004 Thomas Krennwallner <krennwallner@aon.at>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "wol.h"
#include "macfile.h"
#include "xalloc.h"
#include "getline.h"



#define MAC_FMT  "%17[0-9a-fA-F:]"
#define HOST_FMT "%[0-9a-zA-Z._-]"
#define PORT_FMT ":%5u"
#define PASS_FMT "%17[0-9a-fA-F-]"

/* parser state */
#define MAC  0x1
#define HOST 0x2
#define PORT 0x4
#define PASS 0x8




static unsigned int
get_tokens (const char *str,
	    char *mac,
	    char *host,
	    unsigned int *port,
	    char *passwd)
{
  if (sscanf (str, " # %*s") == 1)
    {
      return 0;
    }

  if (sscanf (str, " " MAC_FMT " " HOST_FMT PORT_FMT " " PASS_FMT,
	      mac, host, port, passwd) == 4)
    {
      return MAC | HOST | PORT | PASS;
    }

  if (sscanf (str, " " MAC_FMT " " HOST_FMT " " PASS_FMT,
	      mac, host, passwd) == 3)
    {
      return MAC | HOST | PASS;
    }

  if (sscanf (str, " " MAC_FMT " " HOST_FMT PORT_FMT, mac, host, port) == 3)
    {
      return MAC | HOST | PORT;
    }

  if (sscanf (str, " " MAC_FMT " " HOST_FMT, mac, host) == 2)
    {
      return MAC | HOST;
    }

  return 0;
}



int
macfile_parse (FILE *fp,
	       char **mac_str,
	       char **host_str,
	       unsigned int *port,
	       char **passwd_str)
{
  char *willy = NULL;
  ssize_t ret;
  size_t whale = 0;
  char mac[18], host[255], passwd[18]; /* FIXME: host sufficient? */
  unsigned int parsed_tokens = 0;

  *port = 0;

  while (!feof (fp) && !ferror (fp))
    {
      if ((ret = getline (&willy, &whale, fp)) == -1)
	{
	  return -1;
	}
      
      if ((parsed_tokens = get_tokens (willy, mac, host, port, passwd)) == 0)
	continue;	

      XFREE (willy);
      break;
    }

  if (ferror (fp))
    {
      return -1;
    }

  *mac_str = strdup (mac);
  *host_str = strdup (host);

  if (parsed_tokens & PASS)
    {
      *passwd_str = strdup (passwd);
    }
  else
    {
      *passwd_str = NULL;
    }

  return 0;
}
