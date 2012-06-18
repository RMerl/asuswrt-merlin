/*
 * Prefix list functions.
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

#define AFI_ORF_PREFIX 65535

enum prefix_list_type 
{
  PREFIX_DENY,
  PREFIX_PERMIT,
};

enum prefix_name_type
{
  PREFIX_TYPE_STRING,
  PREFIX_TYPE_NUMBER
};

struct prefix_list
{
  char *name;
  char *desc;

  struct prefix_master *master;

  enum prefix_name_type type;

  int count;
  int rangecount;

  struct prefix_list_entry *head;
  struct prefix_list_entry *tail;

  struct prefix_list *next;
  struct prefix_list *prev;
};

struct orf_prefix
{
  u_int32_t seq;
  u_char ge;
  u_char le;
  struct prefix p;
};

/* Prototypes. */
void prefix_list_init (void);
void prefix_list_reset (void);
void prefix_list_add_hook (void (*func) (struct prefix_list *));
void prefix_list_delete_hook (void (*func) (struct prefix_list *));

struct prefix_list *prefix_list_lookup (afi_t, char *);
enum prefix_list_type prefix_list_apply (struct prefix_list *, void *);

struct stream *
prefix_bgp_orf_entry (struct stream *, struct prefix_list *,
                      u_char, u_char, u_char);
int prefix_bgp_orf_set (char *, afi_t, struct orf_prefix *, int, int);
void prefix_bgp_orf_remove_all (char *);
int prefix_bgp_show_prefix_list (struct vty *, afi_t, char *);
