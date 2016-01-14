/* key-chain for authentication.
 * Copyright (C) 2000 Kunihiro Ishiguro
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

#ifndef _ZEBRA_KEYCHAIN_H
#define _ZEBRA_KEYCHAIN_H

struct keychain
{
  char *name;

  struct list *key;
};

struct key_range
{
  time_t start;
  time_t end;

  u_char duration;
};

struct key
{
  u_int32_t index;

  char *string;

  struct key_range send;
  struct key_range accept;
};

extern void keychain_init (void);
extern struct keychain *keychain_lookup (const char *);
extern struct key *key_lookup_for_accept (const struct keychain *, u_int32_t);
extern struct key *key_match_for_accept (const struct keychain *, const char *);
extern struct key *key_lookup_for_send (const struct keychain *);

#endif /* _ZEBRA_KEYCHAIN_H */
