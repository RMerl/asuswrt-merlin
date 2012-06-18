/* route-map for interface.
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

#include "hash.h"
#include "command.h"
#include "memory.h"
#include "if.h"
#include "ripng_ifrmap.h"

struct hash *ifrmaphash;

/* Hook functions. */
void (*if_rmap_add_hook) (struct if_rmap *) = NULL;
void (*if_rmap_delete_hook) (struct if_rmap *) = NULL;

struct if_rmap *
if_rmap_new ()
{
  struct if_rmap *new;

  new = XCALLOC (MTYPE_IF_RMAP, sizeof (struct if_rmap));

  return new;
}

void
if_rmap_free (struct if_rmap *if_rmap)
{
  if (if_rmap->ifname)
    free (if_rmap->ifname);

  if (if_rmap->routemap[IF_RMAP_IN])
    free (if_rmap->routemap[IF_RMAP_IN]);
  if (if_rmap->routemap[IF_RMAP_OUT])
    free (if_rmap->routemap[IF_RMAP_OUT]);

  XFREE (MTYPE_IF_RMAP, if_rmap);
}

struct if_rmap *
if_rmap_lookup (char *ifname)
{
  struct if_rmap key;
  struct if_rmap *if_rmap;

  key.ifname = ifname;

  if_rmap = hash_lookup (ifrmaphash, &key);
  
  return if_rmap;
}

void
if_rmap_hook_add (void (*func) (struct if_rmap *))
{
  if_rmap_add_hook = func;
}

void
if_rmap_hook_delete (void (*func) (struct if_rmap *))
{
  if_rmap_delete_hook = func;
}

void *
if_rmap_hash_alloc (struct if_rmap *arg)
{
  struct if_rmap *if_rmap;

  if_rmap = if_rmap_new ();
  if_rmap->ifname = strdup (arg->ifname);

  return if_rmap;
}

struct if_rmap *
if_rmap_get (char *ifname)
{
  struct if_rmap key;

  key.ifname = ifname;

  return (struct if_rmap *) hash_get (ifrmaphash, &key, if_rmap_hash_alloc);
}

unsigned int
if_rmap_hash_make (struct if_rmap *if_rmap)
{
  unsigned int key;
  int i;

  key = 0;
  for (i = 0; i < strlen (if_rmap->ifname); i++)
    key += if_rmap->ifname[i];

  return key;
}

int
if_rmap_hash_cmp (struct if_rmap *if_rmap1, struct if_rmap *if_rmap2)
{
  if (strcmp (if_rmap1->ifname, if_rmap2->ifname) == 0)
    return 1;
  return 0;
}

struct if_rmap *
if_rmap_set (char *ifname, enum if_rmap_type type, char *routemap_name)
{
  struct if_rmap *if_rmap;

  if_rmap = if_rmap_get (ifname);

  if (type == IF_RMAP_IN)
    {
      if (if_rmap->routemap[IF_RMAP_IN])
	free (if_rmap->routemap[IF_RMAP_IN]);
      if_rmap->routemap[IF_RMAP_IN] = strdup (routemap_name);
    }
  if (type == IF_RMAP_OUT)
    {
      if (if_rmap->routemap[IF_RMAP_OUT])
	free (if_rmap->routemap[IF_RMAP_OUT]);
      if_rmap->routemap[IF_RMAP_OUT] = strdup (routemap_name);
    }

  if (if_rmap_add_hook)
    (*if_rmap_add_hook) (if_rmap);
  
  return if_rmap;
}

int
if_rmap_unset (char *ifname, enum if_rmap_type type, char *routemap_name)
{
  struct if_rmap *if_rmap;

  if_rmap = if_rmap_lookup (ifname);
  if (!if_rmap)
    return 0;

  if (type == IF_RMAP_IN)
    {
      if (!if_rmap->routemap[IF_RMAP_IN])
	return 0;
      if (strcmp (if_rmap->routemap[IF_RMAP_IN], routemap_name) != 0)
	return 0;

      free (if_rmap->routemap[IF_RMAP_IN]);
      if_rmap->routemap[IF_RMAP_IN] = NULL;      
    }

  if (type == IF_RMAP_OUT)
    {
      if (!if_rmap->routemap[IF_RMAP_OUT])
	return 0;
      if (strcmp (if_rmap->routemap[IF_RMAP_OUT], routemap_name) != 0)
	return 0;

      free (if_rmap->routemap[IF_RMAP_OUT]);
      if_rmap->routemap[IF_RMAP_OUT] = NULL;      
    }

  if (if_rmap_delete_hook)
    (*if_rmap_delete_hook) (if_rmap);

  if (if_rmap->routemap[IF_RMAP_IN] == NULL &&
      if_rmap->routemap[IF_RMAP_OUT] == NULL)
    {
      hash_release (ifrmaphash, if_rmap);
      if_rmap_free (if_rmap);
    }

  return 1;
}

DEFUN (ripng_if_rmap,
       ripng_if_rmap_cmd,
       "route-map RMAP_NAME (in|out) IFNAME",
       "Route map set\n"
       "Route map name\n"
       "Route map set for input filtering\n"
       "Route map set for output filtering\n"
       "Route map interface name\n")
{
  enum if_rmap_type type;
  struct if_rmap *if_rmap;

  if (strncmp (argv[1], "i", 1) == 0)
    type = IF_RMAP_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = IF_RMAP_OUT;
  else
    {
      vty_out (vty, "route-map direction must be [in|out]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if_rmap = if_rmap_set (argv[2], type, argv[0]);

  return CMD_SUCCESS;
}       

DEFUN (no_ripng_if_rmap,
       no_ripng_if_rmap_cmd,
       "no route-map ROUTEMAP_NAME (in|out) IFNAME",
       NO_STR
       "Route map unset\n"
       "Route map name\n"
       "Route map for input filtering\n"
       "Route map for output filtering\n"
       "Route map interface name\n")
{
  int ret;
  enum if_rmap_type type;

  if (strncmp (argv[1], "i", 1) == 0)
    type = IF_RMAP_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = IF_RMAP_OUT;
  else
    {
      vty_out (vty, "route-map direction must be [in|out]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = if_rmap_unset (argv[2], type, argv[0]);
  if (! ret)
    {
      vty_out (vty, "route-map doesn't exist%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}       

/* Configuration write function. */
int
config_write_if_rmap (struct vty *vty)
{
  int i;
  struct hash_backet *mp;
  int write = 0;

  for (i = 0; i < ifrmaphash->size; i++)
    for (mp = ifrmaphash->index[i]; mp; mp = mp->next)
      {
	struct if_rmap *if_rmap;

	if_rmap = mp->data;

	if (if_rmap->routemap[IF_RMAP_IN])
	  {
	    vty_out (vty, " route-map %s in %s%s", 
		     if_rmap->routemap[IF_RMAP_IN],
		     if_rmap->ifname,
		     VTY_NEWLINE);
	    write++;
	  }

	if (if_rmap->routemap[IF_RMAP_OUT])
	  {
	    vty_out (vty, " route-map %s out %s%s", 
		     if_rmap->routemap[IF_RMAP_OUT],
		     if_rmap->ifname,
		     VTY_NEWLINE);
	    write++;
	  }
      }
  return write;
}

void
if_rmap_reset ()
{
  hash_clean (ifrmaphash, (void (*) (void *)) if_rmap_free);
}

void
if_rmap_init (void)
{
  ifrmaphash = hash_create (if_rmap_hash_make, if_rmap_hash_cmp);

  install_element (RIPNG_NODE, &ripng_if_rmap_cmd);
  install_element (RIPNG_NODE, &no_ripng_if_rmap_cmd);
}
