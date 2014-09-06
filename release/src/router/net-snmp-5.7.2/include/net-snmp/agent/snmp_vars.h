/*
 * Definitions for SNMP (RFC 1067) agent variable finder.
 *
 */

#ifndef _SNMP_VARS_H_
#define _SNMP_VARS_H_

#ifdef __cplusplus
extern          "C" {
#endif

/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University
	Copyright 1989	TGV, Incorporated

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU and TGV not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CMU AND TGV DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL CMU OR TGV BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
******************************************************************/
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

    struct variable;

    /**
     * Duplicates a variable.
     *
     * @return Pointer to the duplicate variable upon success; NULL upon
     *   failure.
     *
     * @see struct variable
     * @see struct variable1
     * @see struct variable2
     * @see struct variable3
     * @see struct variable4
     * @see struct variable7
     * @see struct variable8
     * @see struct variable13
     */
    struct variable *netsnmp_duplicate_variable(struct variable *var);

    /*
     * Function pointer called by the master agent for writes. 
     */
    typedef int     (WriteMethod) (int action,
                                   u_char * var_val,
                                   u_char var_val_type,
                                   size_t var_val_len,
                                   u_char * statP,
                                   oid * name, size_t length);

    /*
     * Function pointer called by the master agent for mib information retrieval 
     */
    typedef u_char *(FindVarMethod) (struct variable * vp,
                                     oid * name,
                                     size_t * length,
                                     int exact,
                                     size_t * var_len,
                                     WriteMethod ** write_method);

    /*
     * Function pointer called by the master agent for setting up subagent requests 
     */
    typedef int     (AddVarMethod) (netsnmp_agent_session *asp,
                                    netsnmp_variable_list * vbp);

    struct nlist;

    extern long     long_return;
    extern u_char   return_buf[];

    extern oid      nullOid[];
    extern int      nullOidLen;

#define INST	0xFFFFFFFF      /* used to fill out the instance field of the variables table */

    struct variable {
        u_char          magic;  /* passed to function as a hint */
        char            type;   /* type of variable */
        /*
         * See important comment in snmp_vars.c relating to acl 
         */
        u_short         acl;    /* access control list for variable */
        FindVarMethod  *findVar;        /* function that finds variable */
        u_char          namelen;        /* length of above */
        oid             name[MAX_OID_LEN];      /* object identifier of variable */
    };

    int             init_agent(const char *);
    void            shutdown_agent(void);

    int             should_init(const char *module_name);
    void            add_to_init_list(char *module_list);

#ifdef USING_AGENTX_SUBAGENT_MODULE
    void            netsnmp_enable_subagent(void);
#endif

#ifndef _AGENT_REGISTRY_H
#include <net-snmp/agent/agent_handler.h>
#include <net-snmp/agent/var_struct.h>
#include <net-snmp/agent/agent_registry.h>
#endif

    /*
     * fail overloads non-negative integer value. it must be -1 ! 
     */
#define MATCH_FAILED	(-1)
#define MATCH_SUCCEEDED	0

#ifdef __cplusplus
}
#endif
#endif                          /* _SNMP_VARS_H_ */
