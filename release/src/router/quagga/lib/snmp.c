/* SNMP support
 * Copyright (C) 1999 Kunihiro Ishiguro <kunihiro@zebra.org>
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

#ifdef HAVE_SNMP
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include "smux.h"

#define min(A,B) ((A) < (B) ? (A) : (B))

int
oid_compare (oid *o1, int o1_len, oid *o2, int o2_len)
{
  int i;

  for (i = 0; i < min (o1_len, o2_len); i++)
    {
      if (o1[i] < o2[i])
	return -1;
      else if (o1[i] > o2[i])
	return 1;
    }
  if (o1_len < o2_len)
    return -1;
  if (o1_len > o2_len)
    return 1;

  return 0;
}

void *
oid_copy (void *dest, const void *src, size_t size)
{
  return memcpy (dest, src, size * sizeof (oid));
}

void
oid2in_addr (oid oid[], int len, struct in_addr *addr)
{
  int i;
  u_char *pnt;
  
  if (len == 0)
    return;

  pnt = (u_char *) addr;

  for (i = 0; i < len; i++)
    *pnt++ = oid[i];
}

void
oid_copy_addr (oid oid[], struct in_addr *addr, int len)
{
  int i;
  u_char *pnt;
  
  if (len == 0)
    return;

  pnt = (u_char *) addr;

  for (i = 0; i < len; i++)
    oid[i] = *pnt++;
}

int
smux_header_generic (struct variable *v, oid *name, size_t *length, int exact,
		     size_t *var_len, WriteMethod **write_method)
{
  oid fulloid[MAX_OID_LEN];
  int ret;

  oid_copy (fulloid, v->name, v->namelen);
  fulloid[v->namelen] = 0;
  /* Check against full instance. */
  ret = oid_compare (name, *length, fulloid, v->namelen + 1);

  /* Check single instance. */
  if ((exact && (ret != 0)) || (!exact && (ret >= 0)))
	return MATCH_FAILED;

  /* In case of getnext, fill in full instance. */
  memcpy (name, fulloid, (v->namelen + 1) * sizeof (oid));
  *length = v->namelen + 1;

  *write_method = 0;
  *var_len = sizeof(long);    /* default to 'long' results */

  return MATCH_SUCCEEDED;
}

int
smux_header_table (struct variable *v, oid *name, size_t *length, int exact,
		   size_t *var_len, WriteMethod **write_method)
{
  /* If the requested OID name is less than OID prefix we
     handle, adjust it to our prefix. */
  if ((oid_compare (name, *length, v->name, v->namelen)) < 0)
    {
      if (exact)
	return MATCH_FAILED;
      oid_copy(name, v->name, v->namelen);
      *length = v->namelen;
    }

  *write_method = 0;
  *var_len = sizeof(long);

  return MATCH_SUCCEEDED;
}
#endif /* HAVE_SNMP */
