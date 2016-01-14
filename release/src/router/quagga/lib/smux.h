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

#ifndef _ZEBRA_SNMP_H
#define _ZEBRA_SNMP_H

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/snmp_vars.h>

/* Structures here are mostly compatible with UCD SNMP 4.1.1 */
#define MATCH_FAILED     (-1)
#define MATCH_SUCCEEDED  0

/* SYNTAX TruthValue from SNMPv2-TC. */
#define SNMP_TRUE  1
#define SNMP_FALSE 2

/* SYNTAX RowStatus from SNMPv2-TC. */
#define SNMP_VALID  1
#define SNMP_INVALID 2

#define IN_ADDR_SIZE sizeof(struct in_addr)

#undef REGISTER_MIB
#define REGISTER_MIB(descr, var, vartype, theoid)		\
    smux_register_mib(descr, (struct variable *)var, sizeof(struct vartype), \
    sizeof(var)/sizeof(struct vartype),			\
    theoid, sizeof(theoid)/sizeof(oid))

struct trap_object
{
  int namelen; /* Negative if the object is not indexed */
  oid name[MAX_OID_LEN];
};

/* Declare SMUX return value. */
#define SNMP_LOCAL_VARIABLES \
  static long snmp_int_val; \
  static struct in_addr snmp_in_addr_val;

#define SNMP_INTEGER(V) \
  ( \
    *var_len = sizeof (snmp_int_val), \
    snmp_int_val = V, \
    (u_char *) &snmp_int_val \
  )

#define SNMP_IPADDRESS(V) \
  ( \
    *var_len = sizeof (struct in_addr), \
    snmp_in_addr_val = V, \
    (u_char *) &snmp_in_addr_val \
  )

extern void smux_init (struct thread_master *tm);
extern void smux_register_mib(const char *, struct variable *, 
                              size_t, int, oid [], size_t);
extern int smux_header_generic (struct variable *, oid [], size_t *, 
                                int, size_t *, WriteMethod **);
extern int smux_header_table (struct variable *, oid *, size_t *, 
			      int, size_t *, WriteMethod **);

/* For traps, three OID are provided:

 1. The enterprise OID to use (the last argument will be appended to
    it to form the SNMP trap OID)

 2. The base OID for objects to be sent in traps.

 3. The index OID for objects to be sent in traps. This index is used
    to designate a particular instance of a column.

 The provided trap object contains the bindings to be sent with the
 trap. The base OID will be prefixed to the provided OID and, if the
 length is positive, the requested OID is assumed to be a columnar
 object and the index OID will be appended.

 The two first arguments are the MIB registry used to locate the trap
 objects.

 The use of the arguments may differ depending on the implementation
 used.
*/
extern int smux_trap (struct variable *, size_t,
		      const oid *, size_t,
		      const oid *, size_t,
		      const oid *, size_t,
		      const struct trap_object *, size_t,
		      u_char);

extern int oid_compare (oid *, int, oid *, int);
extern void oid2in_addr (oid [], int, struct in_addr *);
extern void *oid_copy (void *, const void *, size_t);
extern void oid_copy_addr (oid [], struct in_addr *, int);

#endif /* _ZEBRA_SNMP_H */
