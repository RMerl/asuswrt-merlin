/* AS regular expression routine
   Copyright (C) 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "log.h"
#include "command.h"
#include "memory.h"

#include "bgpd.h"
#include "bgp_aspath.h"
#include "bgp_regex.h"

/* Character `_' has special mean.  It represents [,{}() ] and the
   beginning of the line(^) and the end of the line ($).  

   (^|[,{}() ]|$) */

regex_t *
bgp_regcomp (const char *regstr)
{
  /* Convert _ character to generic regular expression. */
  int i, j;
  int len;
  int magic = 0;
  char *magic_str;
  char magic_regexp[] = "(^|[,{}() ]|$)";
  int ret;
  regex_t *regex;

  len = strlen (regstr);
  for (i = 0; i < len; i++)
    if (regstr[i] == '_')
      magic++;

  magic_str = XMALLOC (MTYPE_TMP, len + (14 * magic) + 1);
  
  for (i = 0, j = 0; i < len; i++)
    {
      if (regstr[i] == '_')
	{
	  memcpy (magic_str + j, magic_regexp, strlen (magic_regexp));
	  j += strlen (magic_regexp);
	}
      else
	magic_str[j++] = regstr[i];
    }
  magic_str[j] = '\0';

  regex = XMALLOC (MTYPE_BGP_REGEXP, sizeof (regex_t));

  ret = regcomp (regex, magic_str, REG_EXTENDED|REG_NOSUB);

  XFREE (MTYPE_TMP, magic_str);

  if (ret != 0)
    {
      XFREE (MTYPE_BGP_REGEXP, regex);
      return NULL;
    }

  return regex;
}

int
bgp_regexec (regex_t *regex, struct aspath *aspath)
{
  return regexec (regex, aspath->str, 0, NULL, 0);
}

void
bgp_regex_free (regex_t *regex)
{
  regfree (regex);
  XFREE (MTYPE_BGP_REGEXP, regex);
}
