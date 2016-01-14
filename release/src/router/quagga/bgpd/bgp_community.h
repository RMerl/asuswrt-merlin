/* Community attribute related functions.
   Copyright (C) 1998 Kunihiro Ishiguro

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

#ifndef _QUAGGA_BGP_COMMUNITY_H
#define _QUAGGA_BGP_COMMUNITY_H

/* Communities attribute.  */
struct community 
{
  /* Reference count of communities value.  */
  unsigned long refcnt;

  /* Communities value size.  */
  int size;

  /* Communities value.  */
  u_int32_t *val;

  /* String of community attribute.  This sring is used by vty output
     and expanded community-list for regular expression match.  */
  char *str;
};

/* Well-known communities value.  */
#define COMMUNITY_INTERNET              0x0
#define COMMUNITY_NO_EXPORT             0xFFFFFF01
#define COMMUNITY_NO_ADVERTISE          0xFFFFFF02
#define COMMUNITY_NO_EXPORT_SUBCONFED   0xFFFFFF03
#define COMMUNITY_LOCAL_AS              0xFFFFFF03

/* Macros of community attribute.  */
#define com_length(X)    ((X)->size * 4)
#define com_lastval(X)   ((X)->val + (X)->size - 1)
#define com_nthval(X,n)  ((X)->val + (n))

/* Prototypes of communities attribute functions.  */
extern void community_init (void);
extern void community_finish (void);
extern void community_free (struct community *);
extern struct community *community_uniq_sort (struct community *);
extern struct community *community_parse (u_int32_t *, u_short);
extern struct community *community_intern (struct community *);
extern void community_unintern (struct community **);
extern char *community_str (struct community *);
extern unsigned int community_hash_make (struct community *);
extern struct community *community_str2com (const char *);
extern int community_match (const struct community *, const struct community *);
extern int community_cmp (const struct community *, const struct community *);
extern struct community *community_merge (struct community *, struct community *);
extern struct community *community_delete (struct community *, struct community *);
extern struct community *community_dup (struct community *);
extern int community_include (struct community *, u_int32_t);
extern void community_del_val (struct community *, u_int32_t *);
extern unsigned long community_count (void);
extern struct hash *community_hash (void);

#endif /* _QUAGGA_BGP_COMMUNITY_H */
