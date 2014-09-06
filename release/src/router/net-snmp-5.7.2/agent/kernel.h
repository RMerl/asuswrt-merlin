/***********************************************************************
   Net-SNMP - Simple Network Management Protocol agent library.
 ***********************************************************************/
/** @file kernel.h
 *     Net-SNMP Kernel Data Access Library - header.
 *     Provides access to kernel virtual memory for systems that
 *     support it.
 * @author   See README file for a list of contributors
 */
/* Copyrights:
 *     Copyright holders are listed in README file.
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted. License terms are specified
 *     in COPYING file distributed with the Net-SNMP package.
 */
/***********************************************************************/

#ifdef NETSNMP_CAN_USE_NLIST
extern int      init_kmem(const char *);
extern int      klookup(unsigned long, void *, size_t);
extern void     free_kmem(void);
#define NETSNMP_KLOOKUP(x,y,z) klookup((unsigned long) x,y,z)
#else
#define NETSNMP_KLOOKUP(x,y,z) (0)
#endif

#if HAVE_KVM_H
#include <kvm.h>
extern kvm_t   *kd;
#endif
/***********************************************************************/
