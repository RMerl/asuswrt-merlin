/* Distribute list functions header
 * Copyright (C) 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _ZEBRA_DISTRIBUTE_H
#define _ZEBRA_DISTRIBUTE_H

/* Disctirubte list types. */
enum distribute_type
{
  DISTRIBUTE_IN,
  DISTRIBUTE_OUT,
  DISTRIBUTE_MAX
};

struct distribute
{
  /* Name of the interface. */
  char *ifname;

  /* Filter name of `in' and `out' */
  char *list[DISTRIBUTE_MAX];

  /* prefix-list name of `in' and `out' */
  char *prefix[DISTRIBUTE_MAX];
};

/* Prototypes for distribute-list. */
void distribute_list_init (int);
void distribute_list_reset (void);
void distribute_list_add_hook (void (*) (struct distribute *));
void distribute_list_delete_hook (void (*) (struct distribute *));
struct distribute *distribute_lookup (char *);
int config_write_distribute (struct vty *);
int config_show_distribute (struct vty *);

enum filter_type distribute_apply_in (struct interface *, struct prefix *);
enum filter_type distribute_apply_out (struct interface *, struct prefix *);

#endif /* _ZEBRA_DISTRIBUTE_H */
