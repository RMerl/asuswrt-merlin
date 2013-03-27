/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#include <stdio.h>

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "misc.h"
#include "filter.h"

struct _filter {
  char *flag;
  filter *next;
};


filter *
new_filter(const char *filt,
	   filter *filters)
{
  while (strlen(filt) > 0) {
    filter *new_filter;
    /* break up the filt list */
    char *end = strchr(filt, ',');
    char *next;
    int len;
    if (end == NULL) {
      end = strchr(filt, '\0');
      next = end;
    }
    else {
      next = end + 1;
    }
    len = end - filt;
    /* add to filter list */
    new_filter = ZALLOC(filter);
    new_filter->flag = (char*)zalloc(len + 1);
    strncpy(new_filter->flag, filt, len);
    new_filter->next = filters;
    filters = new_filter;
    filt = next;
  }
  return filters;
}


int
is_filtered_out(const char *flags,
		filter *filters)
{
  while (strlen(flags) > 0) {
    int present;
    filter *filt = filters;
    /* break the string up */
    char *end = strchr(flags, ',');
    char *next;
    int len;
    if (end == NULL) {
      end = strchr(flags, '\0');
      next = end;
    }
    else {
      next = end + 1;
    }
    len = end - flags;
    /* check that it is present */
    present = 0;
    filt = filters;
    while (filt != NULL) {
      if (strncmp(flags, filt->flag, len) == 0
	  && strlen(filt->flag) == len) {
	present = 1;
	break;
      }
      filt = filt->next;
    }
    if (!present)
      return 1;
    flags = next;
  }
  return 0;
}


int
it_is(const char *flag,
      const char *flags)
{
  int flag_len = strlen(flag);
  while (*flags != '\0') {
    if (!strncmp(flags, flag, flag_len)
	&& (flags[flag_len] == ',' || flags[flag_len] == '\0'))
      return 1;
    while (*flags != ',') {
      if (*flags == '\0')
	return 0;
      flags++;
    }
    flags++;
  }
  return 0;
}


#ifdef MAIN
int
main(int argc, char **argv)
{
  filter *filters = NULL;
  int i;
  if (argc < 2) {
    printf("Usage: filter <flags> <filter> ...\n");
    exit (1);
  }
  /* load the filter up */
  for (i = 2; i < argc; i++) 
    filters = new_filter(argv[i], filters);
  if (is_filtered_out(argv[1], filters))
    printf("fail\n");
  else
    printf("pass\n");
  return 0;
}
#endif
