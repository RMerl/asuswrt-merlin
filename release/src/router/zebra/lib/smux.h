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

#define SMUX_PORT_DEFAULT 199

#define SMUXMAXPKTSIZE    1500
#define SMUXMAXSTRLEN      256

#define SMUX_OPEN       (ASN_APPLICATION | ASN_CONSTRUCTOR | 0)
#define SMUX_CLOSE      (ASN_APPLICATION | ASN_PRIMITIVE | 1)
#define SMUX_RREQ       (ASN_APPLICATION | ASN_CONSTRUCTOR | 2)
#define SMUX_RRSP       (ASN_APPLICATION | ASN_PRIMITIVE | 3)
#define SMUX_SOUT       (ASN_APPLICATION | ASN_PRIMITIVE | 4)

#define SMUX_GET        (ASN_CONTEXT | ASN_CONSTRUCTOR | 0)
#define SMUX_GETNEXT    (ASN_CONTEXT | ASN_CONSTRUCTOR | 1)
#define SMUX_GETRSP     (ASN_CONTEXT | ASN_CONSTRUCTOR | 2)
#define SMUX_SET	(ASN_CONTEXT | ASN_CONSTRUCTOR | 3)
#define SMUX_TRAP	(ASN_CONTEXT | ASN_CONSTRUCTOR | 4)

#define SMUX_MAX_FAILURE 3

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

struct variable;

#define REGISTER_MIB(descr, var, vartype, theoid)		\
    smux_register_mib(descr, (struct variable *)var, sizeof(struct vartype), \
    sizeof(var)/sizeof(struct vartype),			\
    theoid, sizeof(theoid)/sizeof(oid))

typedef int (WriteMethod)(int action,
			  u_char  *var_val,
			  u_char   var_val_type,
			  size_t   var_val_len,
			  u_char  *statP,
			  oid     *name,
			  size_t   length,
			  struct variable *v);

typedef u_char *(FindVarMethod)(struct variable *v,
				oid     *name,
				size_t  *length,
				int      exact,
				size_t  *var_len,
				WriteMethod   **write_method);

/* SNMP variable */
struct variable
{
  /* Index of the MIB.*/
  u_char magic;

  /* Type of variable. */
  char type;

  /* Access control list. */
  u_short acl;

  /* Callback function. */
  FindVarMethod *findVar;

  /* Suffix of the MIB. */
  u_char namelen;
  oid name[MAX_OID_LEN];
};

/* SNMP tree. */
struct subtree
{
  /* Tree's oid. */
  oid name[MAX_OID_LEN];
  u_char name_len;

  /* List of the variables. */
  struct variable *variables;

  /* Length of the variables list. */
  int variables_num;

  /* Width of the variables list. */
  int variables_width;

  /* Registered flag. */
  int registered;
};

struct trap_object
{
  FindVarMethod *findVar;
  u_char namelen;
  oid name[MAX_OID_LEN];
};

/* Declare SMUX return value. */
#define SNMP_LOCAL_VARIABLES \
  static int32_t snmp_int_val; \
  static struct in_addr snmp_in_addr_val;

#define SNMP_INTEGER(V) \
  ( \
    *var_len = sizeof (int32_t), \
    snmp_int_val = V, \
    (u_char *) &snmp_int_val \
  )

#define SNMP_IPADDRESS(V) \
  ( \
    *var_len = sizeof (struct in_addr), \
    snmp_in_addr_val = V, \
    (u_char *) &snmp_in_addr_val \
  )

void smux_init (oid [], size_t);
void smux_start (void);
void smux_register_mib(char *, struct variable *, size_t, int, oid [], size_t);
int smux_header_generic (struct variable *, oid [], size_t *, int, size_t *, 
    WriteMethod **);
int smux_trap (oid *, size_t, oid *, size_t, struct trap_object *, size_t, unsigned int, u_char);

int oid_compare (oid *, int, oid *, int);
void oid2in_addr (oid [], int, struct in_addr *);
void *oid_copy (void *, void *, size_t);
void oid_copy_addr (oid [], struct in_addr *, int);

#endif /* _ZEBRA_SNMP_H */
