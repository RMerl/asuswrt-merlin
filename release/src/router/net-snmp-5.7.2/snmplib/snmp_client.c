/*
 * snmp_client.c - a toolkit of common functions for an SNMP client.
 *
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/**********************************************************************
	Copyright 1988, 1989, 1991, 1992 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/** @defgroup snmp_client various PDU processing routines
 *  @ingroup library
 * 
 *  @{
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <stdio.h>
#include <errno.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>

#include <net-snmp/agent/ds_agent.h>
#include <net-snmp/library/default_store.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>
#include <net-snmp/library/snmp_secmod.h>
#include <net-snmp/library/snmpusm.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/snmp_logging.h>
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/pdu_api.h>

netsnmp_feature_child_of(snmp_client_all, libnetsnmp)

netsnmp_feature_child_of(snmp_split_pdu, snmp_client_all)
netsnmp_feature_child_of(snmp_reset_var_types, snmp_client_all)
netsnmp_feature_child_of(query_set_default_session, snmp_client_all)
netsnmp_feature_child_of(row_create, snmp_client_all)

#ifndef BSD4_3
#define BSD4_2
#endif

#ifndef FD_SET

typedef long    fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)        /* bits per mask */

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	memset((p), 0, sizeof(*(p)))
#endif

/*
 * Prototype definitions 
 */
static int      snmp_synch_input(int op, netsnmp_session * session,
                                 int reqid, netsnmp_pdu *pdu, void *magic);

netsnmp_pdu    *
snmp_pdu_create(int command)
{
    netsnmp_pdu    *pdu;

    pdu = (netsnmp_pdu *) calloc(1, sizeof(netsnmp_pdu));
    if (pdu) {
        pdu->version = SNMP_DEFAULT_VERSION;
        pdu->command = command;
        pdu->errstat = SNMP_DEFAULT_ERRSTAT;
        pdu->errindex = SNMP_DEFAULT_ERRINDEX;
        pdu->securityModel = SNMP_DEFAULT_SECMODEL;
        pdu->transport_data = NULL;
        pdu->transport_data_length = 0;
        pdu->securityNameLen = 0;
        pdu->contextNameLen = 0;
        pdu->time = 0;
        pdu->reqid = snmp_get_next_reqid();
        pdu->msgid = snmp_get_next_msgid();
    }
    return pdu;

}


/*
 * Add a null variable with the requested name to the end of the list of
 * variables for this pdu.
 */
netsnmp_variable_list *
snmp_add_null_var(netsnmp_pdu *pdu, const oid * name, size_t name_length)
{
    return snmp_pdu_add_variable(pdu, name, name_length, ASN_NULL, NULL, 0);
}


#include <net-snmp/library/snmp_debug.h>
static int
snmp_synch_input(int op,
                 netsnmp_session * session,
                 int reqid, netsnmp_pdu *pdu, void *magic)
{
    struct synch_state *state = (struct synch_state *) magic;
    int             rpt_type;

    if (reqid != state->reqid && pdu && pdu->command != SNMP_MSG_REPORT) {
        DEBUGMSGTL(("snmp_synch", "Unexpected response (ReqID: %d,%d - Cmd %d)\n",
                                   reqid, state->reqid, pdu->command ));
        return 0;
    }

    state->waiting = 0;
    DEBUGMSGTL(("snmp_synch", "Response (ReqID: %d - Cmd %d)\n",
                               reqid, (pdu ? pdu->command : -1)));

    if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && pdu) {
        if (pdu->command == SNMP_MSG_REPORT) {
            rpt_type = snmpv3_get_report_type(pdu);
            if (SNMPV3_IGNORE_UNAUTH_REPORTS ||
                rpt_type == SNMPERR_NOT_IN_TIME_WINDOW) {
                state->waiting = 1;
            }
            state->pdu = NULL;
            state->status = STAT_ERROR;
            session->s_snmp_errno = rpt_type;
            SET_SNMP_ERROR(rpt_type);
        } else if (pdu->command == SNMP_MSG_RESPONSE) {
            /*
             * clone the pdu to return to snmp_synch_response 
             */
            state->pdu = snmp_clone_pdu(pdu);
            state->status = STAT_SUCCESS;
            session->s_snmp_errno = SNMPERR_SUCCESS;
        }
        else {
            char msg_buf[50];
            state->status = STAT_ERROR;
            session->s_snmp_errno = SNMPERR_PROTOCOL;
            SET_SNMP_ERROR(SNMPERR_PROTOCOL);
            snprintf(msg_buf, sizeof(msg_buf), "Expected RESPONSE-PDU but got %s-PDU",
                     snmp_pdu_type(pdu->command));
            snmp_set_detail(msg_buf);
            return 0;
        }
    } else if (op == NETSNMP_CALLBACK_OP_TIMED_OUT) {
        state->pdu = NULL;
        state->status = STAT_TIMEOUT;
        session->s_snmp_errno = SNMPERR_TIMEOUT;
        SET_SNMP_ERROR(SNMPERR_TIMEOUT);
    } else if (op == NETSNMP_CALLBACK_OP_DISCONNECT) {
        state->pdu = NULL;
        state->status = STAT_ERROR;
        session->s_snmp_errno = SNMPERR_ABORT;
        SET_SNMP_ERROR(SNMPERR_ABORT);
    }

    return 1;
}


/*
 * Clone an SNMP variable data structure.
 * Sets pointers to structure private storage, or
 * allocates larger object identifiers and values as needed.
 *
 * Caller must make list association for cloned variable.
 *
 * Returns 0 if successful.
 */
int
snmp_clone_var(netsnmp_variable_list * var, netsnmp_variable_list * newvar)
{
    if (!newvar || !var)
        return 1;

    memmove(newvar, var, sizeof(netsnmp_variable_list));
    newvar->next_variable = NULL;
    newvar->name = NULL;
    newvar->val.string = NULL;
    newvar->data = NULL;
    newvar->dataFreeHook = NULL;
    newvar->index = 0;

    /*
     * Clone the object identifier and the value.
     * Allocate memory iff original will not fit into local storage.
     */
    if (snmp_set_var_objid(newvar, var->name, var->name_length))
        return 1;

    /*
     * need a pointer to copy a string value. 
     */
    if (var->val.string) {
        if (var->val.string != &var->buf[0]) {
            if (var->val_len <= sizeof(var->buf))
                newvar->val.string = newvar->buf;
            else {
                newvar->val.string = (u_char *) malloc(var->val_len);
                if (!newvar->val.string)
                    return 1;
            }
            memmove(newvar->val.string, var->val.string, var->val_len);
        } else {                /* fix the pointer to new local store */
            newvar->val.string = newvar->buf;
            /*
             * no need for a memmove, since we copied the whole var
             * struct (and thus var->buf) at the beginning of this function.
             */
        }
    } else {
        newvar->val.string = NULL;
        newvar->val_len = 0;
    }

    return 0;
}


/*
 * Possibly make a copy of source memory buffer.
 * Will reset destination pointer if source pointer is NULL.
 * Returns 0 if successful, 1 if memory allocation fails.
 */
int
snmp_clone_mem(void **dstPtr, const void *srcPtr, unsigned len)
{
    *dstPtr = NULL;
    if (srcPtr) {
        *dstPtr = malloc(len + 1);
        if (!*dstPtr) {
            return 1;
        }
        memmove(*dstPtr, srcPtr, len);
        /*
         * this is for those routines that expect 0-terminated strings!!!
         * someone should rather have called strdup
         */
        ((char *) *dstPtr)[len] = 0;
    }
    return 0;
}


/*
 * Walks through a list of varbinds and frees and allocated memory,
 * restoring pointers to local buffers
 */
void
snmp_reset_var_buffers(netsnmp_variable_list * var)
{
    while (var) {
        if (var->name != var->name_loc) {
            if(NULL != var->name)
                free(var->name);
            var->name = var->name_loc;
            var->name_length = 0;
        }
        if (var->val.string != var->buf) {
            if (NULL != var->val.string)
                free(var->val.string);
            var->val.string = var->buf;
            var->val_len = 0;
        }
        var = var->next_variable;
    }
}

/*
 * Creates and allocates a clone of the input PDU,
 * but does NOT copy the variables.
 * This function should be used with another function,
 * such as _copy_pdu_vars.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
static
netsnmp_pdu    *
_clone_pdu_header(netsnmp_pdu *pdu)
{
    netsnmp_pdu    *newpdu;
    struct snmp_secmod_def *sptr;
    int ret;

    newpdu = (netsnmp_pdu *) malloc(sizeof(netsnmp_pdu));
    if (!newpdu)
        return NULL;
    memmove(newpdu, pdu, sizeof(netsnmp_pdu));

    /*
     * reset copied pointers if copy fails 
     */
    newpdu->variables = NULL;
    newpdu->enterprise = NULL;
    newpdu->community = NULL;
    newpdu->securityEngineID = NULL;
    newpdu->securityName = NULL;
    newpdu->contextEngineID = NULL;
    newpdu->contextName = NULL;
    newpdu->transport_data = NULL;

    /*
     * copy buffers individually. If any copy fails, all are freed. 
     */
    if (snmp_clone_mem((void **) &newpdu->enterprise, pdu->enterprise,
                       sizeof(oid) * pdu->enterprise_length) ||
        snmp_clone_mem((void **) &newpdu->community, pdu->community,
                       pdu->community_len) ||
        snmp_clone_mem((void **) &newpdu->contextEngineID,
                       pdu->contextEngineID, pdu->contextEngineIDLen)
        || snmp_clone_mem((void **) &newpdu->securityEngineID,
                          pdu->securityEngineID, pdu->securityEngineIDLen)
        || snmp_clone_mem((void **) &newpdu->contextName, pdu->contextName,
                          pdu->contextNameLen)
        || snmp_clone_mem((void **) &newpdu->securityName,
                          pdu->securityName, pdu->securityNameLen)
        || snmp_clone_mem((void **) &newpdu->transport_data,
                          pdu->transport_data,
                          pdu->transport_data_length)) {
        snmp_free_pdu(newpdu);
        return NULL;
    }

    if (pdu != NULL && pdu->securityStateRef &&
        pdu->command == SNMP_MSG_TRAP2) {

        ret = usm_clone_usmStateReference((struct usmStateReference *) pdu->securityStateRef,
                (struct usmStateReference **) &newpdu->securityStateRef );

        if (ret)
        {
            snmp_free_pdu(newpdu);
            return 0;
        }
    }

    if ((sptr = find_sec_mod(newpdu->securityModel)) != NULL &&
        sptr->pdu_clone != NULL) {
        /*
         * call security model if it needs to know about this 
         */
        (*sptr->pdu_clone) (pdu, newpdu);
    }

    return newpdu;
}

static
netsnmp_variable_list *
_copy_varlist(netsnmp_variable_list * var,      /* source varList */
              int errindex,     /* index of variable to drop (if any) */
              int copy_count)
{                               /* !=0 number variables to copy */
    netsnmp_variable_list *newhead, *newvar, *oldvar;
    int             ii = 0;

    newhead = NULL;
    oldvar = NULL;

    while (var && (copy_count-- > 0)) {
        /*
         * Drop the specified variable (if applicable) 
         */
        if (++ii == errindex) {
            var = var->next_variable;
            continue;
        }

        /*
         * clone the next variable. Cleanup if alloc fails 
         */
        newvar = (netsnmp_variable_list *)
            malloc(sizeof(netsnmp_variable_list));
        if (snmp_clone_var(var, newvar)) {
            if (newvar)
                free((char *) newvar);
            snmp_free_varbind(newhead);
            return NULL;
        }

        /*
         * add cloned variable to new list  
         */
        if (NULL == newhead)
            newhead = newvar;
        if (oldvar)
            oldvar->next_variable = newvar;
        oldvar = newvar;

        var = var->next_variable;
    }
    return newhead;
}


/*
 * Copy some or all variables from source PDU to target PDU.
 * This function consolidates many of the needs of PDU variables:
 * Clone PDU : copy all the variables.
 * Split PDU : skip over some variables to copy other variables.
 * Fix PDU   : remove variable associated with error index.
 *
 * Designed to work with _clone_pdu_header.
 *
 * If drop_err is set, drop any variable associated with errindex.
 * If skip_count is set, skip the number of variable in pdu's list.
 * While copy_count is greater than zero, copy pdu variables to newpdu.
 *
 * If an error occurs, newpdu is freed and pointer is set to 0.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
static
netsnmp_pdu    *
_copy_pdu_vars(netsnmp_pdu *pdu,        /* source PDU */
               netsnmp_pdu *newpdu,     /* target PDU */
               int drop_err,    /* !=0 drop errored variable */
               int skip_count,  /* !=0 number of variables to skip */
               int copy_count)
{                               /* !=0 number of variables to copy */
    netsnmp_variable_list *var;
#if TEMPORARILY_DISABLED
    int             copied;
#endif
    int             drop_idx;

    if (!newpdu)
        return NULL;            /* where is PDU to copy to ? */

    if (drop_err)
        drop_idx = pdu->errindex - skip_count;
    else
        drop_idx = 0;

    var = pdu->variables;
    while (var && (skip_count-- > 0))   /* skip over pdu variables */
        var = var->next_variable;

#if TEMPORARILY_DISABLED
    copied = 0;
    if (pdu->flags & UCD_MSG_FLAG_FORCE_PDU_COPY)
        copied = 1;             /* We're interested in 'empty' responses too */
#endif

    newpdu->variables = _copy_varlist(var, drop_idx, copy_count);
#if TEMPORARILY_DISABLED
    if (newpdu->variables)
        copied = 1;
#endif

#if ALSO_TEMPORARILY_DISABLED
    /*
     * Error if bad errindex or if target PDU has no variables copied 
     */
    if ((drop_err && (ii < pdu->errindex))
#if TEMPORARILY_DISABLED
        /*
         * SNMPv3 engineID probes are allowed to be empty.
         * See the comment in snmp_api.c for further details 
         */
        || copied == 0
#endif
        ) {
        snmp_free_pdu(newpdu);
        return 0;
    }
#endif
    return newpdu;
}


/*
 * Creates (allocates and copies) a clone of the input PDU.
 * If drop_err is set, don't copy any variable associated with errindex.
 * This function is called by snmp_clone_pdu and snmp_fix_pdu.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
static
netsnmp_pdu    *
_clone_pdu(netsnmp_pdu *pdu, int drop_err)
{
    netsnmp_pdu    *newpdu;
    newpdu = _clone_pdu_header(pdu);
    newpdu = _copy_pdu_vars(pdu, newpdu, drop_err, 0, 10000);   /* skip none, copy all */

    return newpdu;
}


/*
 * This function will clone a full varbind list
 *
 * Returns a pointer to the cloned varbind list if successful.
 * Returns 0 if failure
 */
netsnmp_variable_list *
snmp_clone_varbind(netsnmp_variable_list * varlist)
{
    return _copy_varlist(varlist, 0, 10000);    /* skip none, copy all */
}

/*
 * This function will clone a PDU including all of its variables.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure
 */
netsnmp_pdu    *
snmp_clone_pdu(netsnmp_pdu *pdu)
{
    return _clone_pdu(pdu, 0);  /* copies all variables */
}


/*
 * This function will clone a PDU including some of its variables.
 *
 * If skip_count is not zero, it defines the number of variables to skip.
 * If copy_count is not zero, it defines the number of variables to copy.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
#ifndef NETSNMP_FEATURE_REMOVE_SNMP_SPLIT_PDU
netsnmp_pdu    *
snmp_split_pdu(netsnmp_pdu *pdu, int skip_count, int copy_count)
{
    netsnmp_pdu    *newpdu;
    newpdu = _clone_pdu_header(pdu);
    newpdu = _copy_pdu_vars(pdu, newpdu, 0,     /* don't drop any variables */
                            skip_count, copy_count);

    return newpdu;
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMP_SPLIT_PDU */


/*
 * If there was an error in the input pdu, creates a clone of the pdu
 * that includes all the variables except the one marked by the errindex.
 * The command is set to the input command and the reqid, errstat, and
 * errindex are set to default values.
 * If the error status didn't indicate an error, the error index didn't
 * indicate a variable, the pdu wasn't a get response message, the
 * marked variable was not present in the initial request, or there
 * would be no remaining variables, this function will return 0.
 * If everything was successful, a pointer to the fixed cloned pdu will
 * be returned.
 */
netsnmp_pdu    *
snmp_fix_pdu(netsnmp_pdu *pdu, int command)
{
    netsnmp_pdu    *newpdu;

    if ((pdu->command != SNMP_MSG_RESPONSE)
        || (pdu->errstat == SNMP_ERR_NOERROR)
        || (NULL == pdu->variables)
        || (pdu->errindex > (int)snmp_varbind_len(pdu))
        || (pdu->errindex <= 0)) {
        return NULL;            /* pre-condition tests fail */
    }

    newpdu = _clone_pdu(pdu, 1);        /* copies all except errored variable */
    if (!newpdu)
        return NULL;
    if (!newpdu->variables) {
        snmp_free_pdu(newpdu);
        return NULL;            /* no variables. "should not happen" */
    }
    newpdu->command = command;
    newpdu->reqid = snmp_get_next_reqid();
    newpdu->msgid = snmp_get_next_msgid();
    newpdu->errstat = SNMP_DEFAULT_ERRSTAT;
    newpdu->errindex = SNMP_DEFAULT_ERRINDEX;

    return newpdu;
}


/*
 * Returns the number of variables bound to a PDU structure
 */
unsigned long
snmp_varbind_len(netsnmp_pdu *pdu)
{
    register netsnmp_variable_list *vars;
    unsigned long   retVal = 0;
    if (pdu)
        for (vars = pdu->variables; vars; vars = vars->next_variable) {
            retVal++;
        }

    return retVal;
}

/*
 * Add object identifier name to SNMP variable.
 * If the name is large, additional memory is allocated.
 * Returns 0 if successful.
 */

int
snmp_set_var_objid(netsnmp_variable_list * vp,
                   const oid * objid, size_t name_length)
{
    size_t          len = sizeof(oid) * name_length;

    if (vp->name != vp->name_loc && vp->name != NULL) {
        /*
         * Probably previously-allocated "big storage".  Better free it
         * else memory leaks possible.  
         */
        free(vp->name);
    }

    /*
     * use built-in storage for smaller values 
     */
    if (len <= sizeof(vp->name_loc)) {
        vp->name = vp->name_loc;
    } else {
        vp->name = (oid *) malloc(len);
        if (!vp->name)
            return 1;
    }
    if (objid)
        memmove(vp->name, objid, len);
    vp->name_length = name_length;
    return 0;
}

/**
 * snmp_set_var_typed_value is used to set data into the netsnmp_variable_list
 * structure.  Used to return data to the snmp request via the
 * netsnmp_request_info structure's requestvb pointer.
 *
 * @param newvar   the structure gets populated with the given data, type,
 *                 val_str, and val_len.
 * @param type     is the asn data type to be copied
 * @param val_str  is a buffer containing the value to be copied into the
 *                 newvar structure. 
 * @param val_len  the length of val_str
 * 
 * @return returns 0 on success and 1 on a malloc error
 */

int
snmp_set_var_typed_value(netsnmp_variable_list * newvar, u_char type,
                         const void * val_str, size_t val_len)
{
    newvar->type = type;
    return snmp_set_var_value(newvar, val_str, val_len);
}

int
snmp_set_var_typed_integer(netsnmp_variable_list * newvar,
                           u_char type, long val)
{
    newvar->type = type;
    return snmp_set_var_value(newvar, &val, sizeof(long));
}

int
count_varbinds(netsnmp_variable_list * var_ptr)
{
    int             count = 0;

    for (; var_ptr != NULL; var_ptr = var_ptr->next_variable)
        count++;

    return count;
}

netsnmp_feature_child_of(count_varbinds_of_type, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_COUNT_VARBINDS_OF_TYPE
int
count_varbinds_of_type(netsnmp_variable_list * var_ptr, u_char type)
{
    int             count = 0;

    for (; var_ptr != NULL; var_ptr = var_ptr->next_variable)
        if (var_ptr->type == type)
            count++;

    return count;
}
#endif /* NETSNMP_FEATURE_REMOVE_COUNT_VARBINDS_OF_TYPE */

netsnmp_feature_child_of(find_varind_of_type, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_FIND_VARIND_OF_TYPE
netsnmp_variable_list *
find_varbind_of_type(netsnmp_variable_list * var_ptr, u_char type)
{
    for (; var_ptr != NULL && var_ptr->type != type;
         var_ptr = var_ptr->next_variable);

    return var_ptr;
}
#endif /* NETSNMP_FEATURE_REMOVE_FIND_VARIND_OF_TYPE */

netsnmp_variable_list*
find_varbind_in_list( netsnmp_variable_list *vblist,
                      const oid *name, size_t len)
{
    for (; vblist != NULL; vblist = vblist->next_variable)
        if (!snmp_oid_compare(vblist->name, vblist->name_length, name, len))
            return vblist;

    return NULL;
}

/*
 * Add some value to SNMP variable.
 * If the value is large, additional memory is allocated.
 * Returns 0 if successful.
 */

int
snmp_set_var_value(netsnmp_variable_list * vars,
                   const void * value, size_t len)
{
    int             largeval = 1;

    /*
     * xxx-rks: why the unconditional free? why not use existing
     * memory, if len < vars->val_len ?
     */
    if (vars->val.string && vars->val.string != vars->buf) {
        free(vars->val.string);
    }
    vars->val.string = NULL;
    vars->val_len = 0;

    if (value == NULL && len > 0) {
        snmp_log(LOG_ERR, "bad size for NULL value\n");
        return 1;
    }

    /*
     * use built-in storage for smaller values 
     */
    if (len <= sizeof(vars->buf)) {
        vars->val.string = (u_char *) vars->buf;
        largeval = 0;
    }

    if ((0 == len) || (NULL == value)) {
        vars->val.string[0] = 0;
        return 0;
    }

    vars->val_len = len;
    switch (vars->type) {
    case ASN_INTEGER:
    case ASN_UNSIGNED:
    case ASN_TIMETICKS:
    case ASN_COUNTER:
    case ASN_UINTEGER:
        if (vars->val_len == sizeof(int)) {
            if (ASN_INTEGER == vars->type) {
                const int      *val_int 
                    = (const int *) value;
                *(vars->val.integer) = (long) *val_int;
            } else {
                const u_int    *val_uint
                    = (const u_int *) value;
                *(vars->val.integer) = (unsigned long) *val_uint;
            }
        }
#if SIZEOF_LONG != SIZEOF_INT
        else if (vars->val_len == sizeof(long)){
            const u_long   *val_ulong
                = (const u_long *) value;
            *(vars->val.integer) = *val_ulong;
            if (*(vars->val.integer) > 0xffffffff) {
                snmp_log(LOG_ERR,"truncating integer value > 32 bits\n");
                *(vars->val.integer) &= 0xffffffff;
            }
        }
#endif
#if defined(SIZEOF_LONG_LONG) && (SIZEOF_LONG != SIZEOF_LONG_LONG)
#if !defined(SIZEOF_INTMAX_T) || (SIZEOF_LONG_LONG != SIZEOF_INTMAX_T)
        else if (vars->val_len == sizeof(long long)){
            const unsigned long long   *val_ullong
                = (const unsigned long long *) value;
            *(vars->val.integer) = (long) *val_ullong;
            if (*(vars->val.integer) > 0xffffffff) {
                snmp_log(LOG_ERR,"truncating integer value > 32 bits\n");
                *(vars->val.integer) &= 0xffffffff;
            }
        }
#endif
#endif
#if defined(SIZEOF_INTMAX_T) && (SIZEOF_LONG != SIZEOF_INTMAX_T)
        else if (vars->val_len == sizeof(intmax_t)){
            const uintmax_t *val_uintmax_t
                = (const uintmax_t *) value;
            *(vars->val.integer) = (long) *val_uintmax_t;
            if (*(vars->val.integer) > 0xffffffff) {
                snmp_log(LOG_ERR,"truncating integer value > 32 bits\n");
                *(vars->val.integer) &= 0xffffffff;
            }
        }
#endif
#if SIZEOF_SHORT != SIZEOF_INT
        else if (vars->val_len == sizeof(short)) {
            if (ASN_INTEGER == vars->type) {
                const short      *val_short 
                    = (const short *) value;
                *(vars->val.integer) = (long) *val_short;
            } else {
                const u_short    *val_ushort
                    = (const u_short *) value;
                *(vars->val.integer) = (unsigned long) *val_ushort;
            }
        }
#endif
        else if (vars->val_len == sizeof(char)) {
            if (ASN_INTEGER == vars->type) {
                const char      *val_char 
                    = (const char *) value;
                *(vars->val.integer) = (long) *val_char;
            } else {
                    const u_char    *val_uchar
                    = (const u_char *) value;
                *(vars->val.integer) = (unsigned long) *val_uchar;
            }
        }
        else {
            snmp_log(LOG_ERR,"bad size for integer-like type (%d)\n",
                     (int)vars->val_len);
            return (1);
        }
        vars->val_len = sizeof(long);
        break;

    case ASN_OBJECT_ID:
    case ASN_PRIV_IMPLIED_OBJECT_ID:
    case ASN_PRIV_INCL_RANGE:
    case ASN_PRIV_EXCL_RANGE:
        if (largeval) {
            vars->val.objid = (oid *) malloc(vars->val_len);
        }
        if (vars->val.objid == NULL) {
            snmp_log(LOG_ERR,"no storage for OID\n");
            return 1;
        }
        memmove(vars->val.objid, value, vars->val_len);
        break;

    case ASN_IPADDRESS: /* snmp_build_var_op treats IPADDR like a string */
        if (4 != vars->val_len) {
            netsnmp_assert("ipaddress length == 4");
        }
        /** FALL THROUGH */
    case ASN_PRIV_IMPLIED_OCTET_STR:
    case ASN_OCTET_STR:
    case ASN_BIT_STR:
    case ASN_OPAQUE:
    case ASN_NSAP:
        if (vars->val_len >= sizeof(vars->buf)) {
            vars->val.string = (u_char *) malloc(vars->val_len + 1);
        }
        if (vars->val.string == NULL) {
            snmp_log(LOG_ERR,"no storage for string\n");
            return 1;
        }
        memmove(vars->val.string, value, vars->val_len);
        /*
         * Make sure the string is zero-terminated; some bits of code make
         * this assumption.  Easier to do this here than fix all these wrong
         * assumptions.  
         */
        vars->val.string[vars->val_len] = '\0';
        break;

    case SNMP_NOSUCHOBJECT:
    case SNMP_NOSUCHINSTANCE:
    case SNMP_ENDOFMIBVIEW:
    case ASN_NULL:
        vars->val_len = 0;
        vars->val.string = NULL;
        break;

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case ASN_OPAQUE_U64:
    case ASN_OPAQUE_I64:
#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */
    case ASN_COUNTER64:
        if (largeval) {
            snmp_log(LOG_ERR,"bad size for counter 64 (%d)\n",
                     (int)vars->val_len);
            return (1);
        }
        vars->val_len = sizeof(struct counter64);
        memmove(vars->val.counter64, value, vars->val_len);
        break;

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case ASN_OPAQUE_FLOAT:
        if (largeval) {
            snmp_log(LOG_ERR,"bad size for opaque float (%d)\n",
                     (int)vars->val_len);
            return (1);
        }
        vars->val_len = sizeof(float);
        memmove(vars->val.floatVal, value, vars->val_len);
        break;

    case ASN_OPAQUE_DOUBLE:
        if (largeval) {
            snmp_log(LOG_ERR,"bad size for opaque double (%d)\n",
                     (int)vars->val_len);
            return (1);
        }
        vars->val_len = sizeof(double);
        memmove(vars->val.doubleVal, value, vars->val_len);
        break;

#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */

    default:
        snmp_log(LOG_ERR,"Internal error in type switching\n");
        snmp_set_detail("Internal error in type switching\n");
        return (1);
    }

    return 0;
}

void
snmp_replace_var_types(netsnmp_variable_list * vbl, u_char old_type,
                       u_char new_type)
{
    while (vbl) {
        if (vbl->type == old_type) {
            snmp_set_var_typed_value(vbl, new_type, NULL, 0);
        }
        vbl = vbl->next_variable;
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_SNMP_RESET_VAR_TYPES
void
snmp_reset_var_types(netsnmp_variable_list * vbl, u_char new_type)
{
    while (vbl) {
        snmp_set_var_typed_value(vbl, new_type, NULL, 0);
        vbl = vbl->next_variable;
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMP_RESET_VAR_TYPES */

int
snmp_synch_response_cb(netsnmp_session * ss,
                       netsnmp_pdu *pdu,
                       netsnmp_pdu **response, snmp_callback pcb)
{
    struct synch_state lstate, *state;
    snmp_callback   cbsav;
    void           *cbmagsav;
    int             numfds, count;
    fd_set          fdset;
    struct timeval  timeout, *tvp;
    int             block;

    memset((void *) &lstate, 0, sizeof(lstate));
    state = &lstate;
    cbsav = ss->callback;
    cbmagsav = ss->callback_magic;
    ss->callback = pcb;
    ss->callback_magic = (void *) state;

    if ((state->reqid = snmp_send(ss, pdu)) == 0) {
        snmp_free_pdu(pdu);
        state->status = STAT_ERROR;
    } else
        state->waiting = 1;

    while (state->waiting) {
        numfds = 0;
        FD_ZERO(&fdset);
        block = NETSNMP_SNMPBLOCK;
        tvp = &timeout;
        timerclear(tvp);
        snmp_sess_select_info_flags(0, &numfds, &fdset, tvp, &block,
                                    NETSNMP_SELECT_NOALARMS);
        if (block == 1)
            tvp = NULL;         /* block without timeout */
        count = select(numfds, &fdset, NULL, NULL, tvp);
        if (count > 0) {
            snmp_read(&fdset);
        } else {
            switch (count) {
            case 0:
                snmp_timeout();
                break;
            case -1:
                if (errno == EINTR) {
                    continue;
                } else {
                    snmp_errno = SNMPERR_GENERR;    /*MTCRITICAL_RESOURCE */
                    /*
                     * CAUTION! if another thread closed the socket(s)
                     * waited on here, the session structure was freed.
                     * It would be nice, but we can't rely on the pointer.
                     * ss->s_snmp_errno = SNMPERR_GENERR;
                     * ss->s_errno = errno;
                     */
                    snmp_set_detail(strerror(errno));
                }
                /*
                 * FALLTHRU 
                 */
            default:
                state->status = STAT_ERROR;
                state->waiting = 0;
            }
        }

        if ( ss->flags & SNMP_FLAGS_RESP_CALLBACK ) {
            void (*cb)(void);
            cb = (void (*)(void))(ss->myvoid);
            cb();        /* Used to invoke 'netsnmp_check_outstanding_agent_requests();'
                            on internal AgentX queries.  */
        }
    }
    *response = state->pdu;
    ss->callback = cbsav;
    ss->callback_magic = cbmagsav;
    return state->status;
}

int
snmp_synch_response(netsnmp_session * ss,
                    netsnmp_pdu *pdu, netsnmp_pdu **response)
{
    return snmp_synch_response_cb(ss, pdu, response, snmp_synch_input);
}

int
snmp_sess_synch_response(void *sessp,
                         netsnmp_pdu *pdu, netsnmp_pdu **response)
{
    netsnmp_session *ss;
    struct synch_state lstate, *state;
    snmp_callback   cbsav;
    void           *cbmagsav;
    int             numfds, count;
    fd_set          fdset;
    struct timeval  timeout, *tvp;
    int             block;

    ss = snmp_sess_session(sessp);
    if (ss == NULL) {
        return STAT_ERROR;
    }

    memset((void *) &lstate, 0, sizeof(lstate));
    state = &lstate;
    cbsav = ss->callback;
    cbmagsav = ss->callback_magic;
    ss->callback = snmp_synch_input;
    ss->callback_magic = (void *) state;

    if ((state->reqid = snmp_sess_send(sessp, pdu)) == 0) {
        snmp_free_pdu(pdu);
        state->status = STAT_ERROR;
    } else
        state->waiting = 1;

    while (state->waiting) {
        numfds = 0;
        FD_ZERO(&fdset);
        block = NETSNMP_SNMPBLOCK;
        tvp = &timeout;
        timerclear(tvp);
        snmp_sess_select_info_flags(sessp, &numfds, &fdset, tvp, &block,
                                    NETSNMP_SELECT_NOALARMS);
        if (block == 1)
            tvp = NULL;         /* block without timeout */
        count = select(numfds, &fdset, NULL, NULL, tvp);
        if (count > 0) {
            snmp_sess_read(sessp, &fdset);
        } else
            switch (count) {
            case 0:
                snmp_sess_timeout(sessp);
                break;
            case -1:
                if (errno == EINTR) {
                    continue;
                } else {
                    snmp_errno = SNMPERR_GENERR;    /*MTCRITICAL_RESOURCE */
                    /*
                     * CAUTION! if another thread closed the socket(s)
                     * waited on here, the session structure was freed.
                     * It would be nice, but we can't rely on the pointer.
                     * ss->s_snmp_errno = SNMPERR_GENERR;
                     * ss->s_errno = errno;
                     */
                    snmp_set_detail(strerror(errno));
                }
                /*
                 * FALLTHRU 
                 */
            default:
                state->status = STAT_ERROR;
                state->waiting = 0;
            }
    }
    *response = state->pdu;
    ss->callback = cbsav;
    ss->callback_magic = cbmagsav;
    return state->status;
}


const char     *
snmp_errstring(int errstat)
{
    const char * const error_string[19] = {
        "(noError) No Error",
        "(tooBig) Response message would have been too large.",
        "(noSuchName) There is no such variable name in this MIB.",
        "(badValue) The value given has the wrong type or length.",
        "(readOnly) The two parties used do not have access to use the specified SNMP PDU.",
        "(genError) A general failure occured",
        "noAccess",
        "wrongType (The set datatype does not match the data type the agent expects)",
        "wrongLength (The set value has an illegal length from what the agent expects)",
        "wrongEncoding",
        "wrongValue (The set value is illegal or unsupported in some way)",
        "noCreation (That table does not support row creation or that object can not ever be created)",
        "inconsistentValue (The set value is illegal or unsupported in some way)",
        "resourceUnavailable (This is likely a out-of-memory failure within the agent)",
        "commitFailed",
        "undoFailed",
        "authorizationError (access denied to that object)",
        "notWritable (That object does not support modification)",
        "inconsistentName (That object can not currently be created)"
    };

    if (errstat <= MAX_SNMP_ERR && errstat >= SNMP_ERR_NOERROR) {
        return error_string[errstat];
    } else {
        return "Unknown Error";
    }
}



/*
 *
 *  Convenience routines to make various requests
 *  over the specified SNMP session.
 *
 */
#include <net-snmp/library/snmp_debug.h>

static netsnmp_session *_def_query_session = NULL;

#ifndef NETSNMP_FEATURE_REMOVE_QUERY_SET_DEFAULT_SESSION
void
netsnmp_query_set_default_session( netsnmp_session *sess) {
    DEBUGMSGTL(("iquery", "set default session %p\n", sess));
    _def_query_session = sess;
}
#endif /* NETSNMP_FEATURE_REMOVE_QUERY_SET_DEFAULT_SESSION */

/**
 * Return a pointer to the default internal query session.
 */
netsnmp_session *
netsnmp_query_get_default_session_unchecked( void ) {
    DEBUGMSGTL(("iquery", "get default session %p\n", _def_query_session));
    return _def_query_session;
}

/**
 * Return a pointer to the default internal query session and log a
 * warning message once if this session does not exist.
 */
netsnmp_session *
netsnmp_query_get_default_session( void ) {
    static int warning_logged = 0;

    if (! _def_query_session && ! warning_logged) {
        if (! netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID,
                                    NETSNMP_DS_AGENT_INTERNAL_SECNAME)) {
            snmp_log(LOG_WARNING,
                     "iquerySecName has not been configured - internal queries will fail\n");
        } else {
            snmp_log(LOG_WARNING,
                     "default session is not available - internal queries will fail\n");
        }
        warning_logged = 1;
    }

    return netsnmp_query_get_default_session_unchecked();
}


/*
 * Internal utility routine to actually send the query
 */
static int _query(netsnmp_variable_list *list,
                  int                    request,
                  netsnmp_session       *session) {

    netsnmp_pdu *pdu      = snmp_pdu_create( request );
    netsnmp_pdu *response = NULL;
    netsnmp_variable_list *vb1, *vb2, *vtmp;
    int ret, count;

    DEBUGMSGTL(("iquery", "query on session %p\n", session));
    /*
     * Clone the varbind list into the request PDU...
     */
    pdu->variables = snmp_clone_varbind( list );
retry:
    if ( session )
        ret = snmp_synch_response(            session, pdu, &response );
    else if (_def_query_session)
        ret = snmp_synch_response( _def_query_session, pdu, &response );
    else {
        /* No session specified */
        snmp_free_pdu(pdu);
        return SNMP_ERR_GENERR;
    }
    DEBUGMSGTL(("iquery", "query returned %d\n", ret));

    /*
     * ....then copy the results back into the
     * list (assuming the request succeeded!).
     * This avoids having to worry about how this
     * list was originally allocated.
     */
    if ( ret == SNMP_ERR_NOERROR ) {
        if ( response->errstat != SNMP_ERR_NOERROR ) {
            DEBUGMSGT(("iquery", "Error in packet: %s\n",
                       snmp_errstring(response->errstat)));
            /*
             * If the request failed, then remove the
             *  offending varbind and try again.
             *  (all except SET requests)
             *
             * XXX - implement a library version of
             *       NETSNMP_DS_APP_DONT_FIX_PDUS ??
             */
            ret = response->errstat;
            if (response->errindex != 0) {
                DEBUGMSGT(("iquery:result", "Failed object:\n"));
                for (count = 1, vtmp = response->variables;
                     vtmp && count != response->errindex;
                     vtmp = vtmp->next_variable, count++)
                    /*EMPTY*/;
                if (vtmp)
                    DEBUGMSGVAR(("iquery:result", vtmp));
                DEBUGMSG(("iquery:result", "\n"));
            }
#ifndef NETSNMP_NO_WRITE_SUPPORT
            if (request != SNMP_MSG_SET &&
                response->errindex != 0) {
                DEBUGMSGTL(("iquery", "retrying query (%d, %ld)\n", ret, response->errindex));
                pdu = snmp_fix_pdu( response, request );
                snmp_free_pdu( response );
                response = NULL;
                if ( pdu != NULL )
                    goto retry;
            }
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        } else {
            for (vb1 = response->variables, vb2 = list;
                 vb1;
                 vb1 = vb1->next_variable,  vb2 = vb2->next_variable) {
                DEBUGMSGVAR(("iquery:result", vb1));
                DEBUGMSG(("iquery:results", "\n"));
                if ( !vb2 ) {
                    ret = SNMP_ERR_GENERR;
                    break;
                }
                vtmp = vb2->next_variable;
                snmp_free_var_internals( vb2 );
                snmp_clone_var( vb1, vb2 );
                vb2->next_variable = vtmp;
            }
        }
    } else {
        /* Distinguish snmp_send errors from SNMP errStat errors */
        ret = -ret;
    }
    snmp_free_pdu( response );
    return ret;
}

/*
 * These are simple wrappers round the internal utility routine
 */
int netsnmp_query_get(netsnmp_variable_list *list,
                      netsnmp_session       *session){
    return _query( list, SNMP_MSG_GET, session );
}


int netsnmp_query_getnext(netsnmp_variable_list *list,
                          netsnmp_session       *session){
    return _query( list, SNMP_MSG_GETNEXT, session );
}


#ifndef NETSNMP_NO_WRITE_SUPPORT
int netsnmp_query_set(netsnmp_variable_list *list,
                      netsnmp_session       *session){
    return _query( list, SNMP_MSG_SET, session );
}
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

/*
 * A walk needs a bit more work.
 */
int netsnmp_query_walk(netsnmp_variable_list *list,
                       netsnmp_session       *session) {
    /*
     * Create a working copy of the original (single)
     * varbind, so we can use this varbind parameter
     * to check when we've finished walking this subtree.
     */
    netsnmp_variable_list *vb = snmp_clone_varbind( list );
    netsnmp_variable_list *res_list = NULL;
    netsnmp_variable_list *res_last = NULL;
    int ret;

    /*
     * Now walk the tree as usual
     */
    ret = _query( vb, SNMP_MSG_GETNEXT, session );
    while ( ret == SNMP_ERR_NOERROR &&
        snmp_oidtree_compare( list->name, list->name_length,
                                vb->name,   vb->name_length ) == 0) {

        /*
         * Copy each response varbind to the end of the result list
         * and then re-use this to ask for the next entry.
         */
        if ( res_last ) {
            res_last->next_variable = snmp_clone_varbind( vb );
            res_last = res_last->next_variable;
        } else {
            res_list = snmp_clone_varbind( vb );
            res_last = res_list;
        }
        ret = _query( vb, SNMP_MSG_GETNEXT, session );
    }
    /*
     * Copy the first result back into the original varbind parameter,
     * add the rest of the results (if any), and clean up.
     */
    if ( res_list ) {
        snmp_clone_var( res_list, list );
        list->next_variable = res_list->next_variable;
        res_list->next_variable = NULL;
        snmp_free_varbind( res_list );
    }
    snmp_free_varbind( vb );
    return ret;
}

/** **************************************************************************
 *
 * state machine
 *
 */
int
netsnmp_state_machine_run( netsnmp_state_machine_input *input)
{
    netsnmp_state_machine_step *current, *last;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->steps, SNMPERR_GENERR );
    last = current = input->steps;

    DEBUGMSGT(("state_machine:run", "starting step: %s\n", current->name));

    while (current) {

        /*
         * log step and check for required data
         */
        DEBUGMSGT(("state_machine:run", "at step: %s\n", current->name));
        if (NULL == current->run) {
            DEBUGMSGT(("state_machine:run", "no run step\n"));
            current->result = last->result;
            break;
        }

        /*
         * run step
         */
        DEBUGMSGT(("state_machine:run", "running step: %s\n", current->name));
        current->result = (*current->run)( input, current );
        ++input->steps_so_far;
        
        /*
         * log result and move to next step
         */
        input->last_run = current;
        DEBUGMSGT(("state_machine:run:result", "step %s returned %d\n",
                   current->name, current->result));
        if (SNMPERR_SUCCESS == current->result)
            current = current->on_success;
        else if (SNMPERR_ABORT == current->result) {
            DEBUGMSGT(("state_machine:run:result", "ABORT from %s\n",
                       current->name));
            break;
        }
        else
            current = current->on_error;
    }

    /*
     * run cleanup
     */
    if ((input->cleanup) && (input->cleanup->run))
        (*input->cleanup->run)( input, input->last_run );

    return input->last_run->result;
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
#ifndef NETSNMP_FEATURE_REMOVE_ROW_CREATE
/** **************************************************************************
 *
 * row create state machine steps
 *
 */
typedef struct rowcreate_state_s {

    netsnmp_session        *session;
    netsnmp_variable_list  *vars;
    int                     row_status_index;
} rowcreate_state;

static netsnmp_variable_list *
_get_vb_num(netsnmp_variable_list *vars, int index)
{
    for (; vars && index > 0; --index)
        vars = vars->next_variable;

    if (!vars || index > 0)
        return NULL;
    
    return vars;
}


/*
 * cleanup
 */
static int 
_row_status_state_cleanup(netsnmp_state_machine_input *input,
                 netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;

    netsnmp_require_ptr_LRV( input, SNMPERR_ABORT );
    netsnmp_require_ptr_LRV( step, SNMPERR_ABORT );

    DEBUGMSGT(("row_create:called", "_row_status_state_cleanup, last run step was %s rc %d\n",
               step->name, step->result));

    ctx = (rowcreate_state *)input->input_context;
    if (ctx && ctx->vars)
        snmp_free_varbind( ctx->vars );

    return SNMPERR_SUCCESS;
}

/*
 * send a request to activate the row
 */
static int 
_row_status_state_activate(netsnmp_state_machine_input *input,
                  netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;
    netsnmp_variable_list *rs_var, *var = NULL;
    int32_t                rc, val = RS_ACTIVE;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( step, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->input_context, SNMPERR_GENERR );

    ctx = (rowcreate_state *)input->input_context;

    DEBUGMSGT(("row_create:called", "called %s\n", step->name));

    /*
     * just send the rowstatus varbind
     */
    rs_var = _get_vb_num(ctx->vars, ctx->row_status_index);
    netsnmp_require_ptr_LRV(rs_var, SNMPERR_GENERR);

    var = snmp_varlist_add_variable(&var, rs_var->name, rs_var->name_length,
                                    rs_var->type, &val, sizeof(val));
    netsnmp_require_ptr_LRV( var, SNMPERR_GENERR );

    /*
     * send set
     */
    rc = netsnmp_query_set( var, ctx->session );
    if (-2 == rc)
        rc = SNMPERR_ABORT;

    snmp_free_varbind(var);

    return rc;
}

/*
 * send each non-row status column, one at a time
 */
static int 
_row_status_state_single_value_cols(netsnmp_state_machine_input *input,
                                    netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;
    netsnmp_variable_list *var, *tmp_next, *row_status;
    int                    rc = SNMPERR_GENERR;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( step, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->input_context, SNMPERR_GENERR );

    ctx = (rowcreate_state *)input->input_context;

    DEBUGMSGT(("row_create:called", "called %s\n", step->name));

    row_status = _get_vb_num(ctx->vars, ctx->row_status_index);
    netsnmp_require_ptr_LRV(row_status, SNMPERR_GENERR);

    /*
     * try one varbind at a time
     */
    for (var = ctx->vars; var; var = var->next_variable) {
        if (var == row_status)
            continue;

        tmp_next = var->next_variable;
        var->next_variable = NULL;

        /*
         * send set
         */
        rc = netsnmp_query_set( var, ctx->session );
        var->next_variable = tmp_next;
        if (-2 == rc)
            rc = SNMPERR_ABORT;
        if (rc != SNMPERR_SUCCESS)
            break;
    }

    return rc;
}

/*
 * send all values except row status
 */
static int 
_row_status_state_multiple_values_cols(netsnmp_state_machine_input *input,
                                       netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;
    netsnmp_variable_list *vars, *var, *last, *row_status;
    int                    rc;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( step, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->input_context, SNMPERR_GENERR );

    ctx = (rowcreate_state *)input->input_context;

    DEBUGMSGT(("row_create:called", "called %s\n", step->name));

    vars = snmp_clone_varbind(ctx->vars);
    netsnmp_require_ptr_LRV(vars, SNMPERR_GENERR);

    row_status = _get_vb_num(vars, ctx->row_status_index);
    if (NULL == row_status) {
        snmp_free_varbind(vars);
        return SNMPERR_GENERR;
    }

    /*
     * remove row status varbind
     */
    if (row_status == vars) {
        vars = row_status->next_variable;
        row_status->next_variable = NULL;
    }
    else {
        for (last=vars, var=last->next_variable;
             var;
             last=var, var = var->next_variable) {
            if (var == row_status) {
                last->next_variable = var->next_variable;
                break;
            }
        }
    }
    snmp_free_var(row_status);

    /*
     * send set
     */
    rc = netsnmp_query_set( vars, ctx->session );
    if (-2 == rc)
        rc = SNMPERR_ABORT;

    snmp_free_varbind(vars);

    return rc;
}

/*
 * send a createAndWait request with no other values
 */
static int 
_row_status_state_single_value_createAndWait(netsnmp_state_machine_input *input,
                                             netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;
    netsnmp_variable_list *var = NULL, *rs_var;
    int32_t                rc, val = RS_CREATEANDWAIT;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( step, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->input_context, SNMPERR_GENERR );

    ctx = (rowcreate_state *)input->input_context;

    DEBUGMSGT(("row_create:called", "called %s\n", step->name));

    rs_var = _get_vb_num(ctx->vars, ctx->row_status_index);
    netsnmp_require_ptr_LRV(rs_var, SNMPERR_GENERR);

    var = snmp_varlist_add_variable(&var, rs_var->name, rs_var->name_length,
                                    rs_var->type, &val, sizeof(val));
    netsnmp_require_ptr_LRV(var, SNMPERR_GENERR);

    /*
     * send set
     */
    rc = netsnmp_query_set( var, ctx->session );
    if (-2 == rc)
        rc = SNMPERR_ABORT;

    snmp_free_varbind(var);

    return rc;
}

/*
 * send a creatAndWait request with all values
 */
static int 
_row_status_state_all_values_createAndWait(netsnmp_state_machine_input *input,
                                           netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;
    netsnmp_variable_list *vars, *rs_var;
    int                    rc;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( step, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->input_context, SNMPERR_GENERR );

    ctx = (rowcreate_state *)input->input_context;

    DEBUGMSGT(("row_create:called", "called %s\n", step->name));

    vars = snmp_clone_varbind(ctx->vars);
    netsnmp_require_ptr_LRV(vars, SNMPERR_GENERR);

    /*
     * make sure row stats is createAndWait
     */
    rs_var = _get_vb_num(vars, ctx->row_status_index);
    if (NULL == rs_var) {
        snmp_free_varbind(vars);
        return SNMPERR_GENERR;
    }

    if (*rs_var->val.integer != RS_CREATEANDWAIT)
        *rs_var->val.integer = RS_CREATEANDWAIT;

    /*
     * send set
     */
    rc = netsnmp_query_set( vars, ctx->session );
    if (-2 == rc)
        rc = SNMPERR_ABORT;

    snmp_free_varbind(vars);

    return rc;
}


/**
 * send createAndGo request with all values
 */
static int 
_row_status_state_all_values_createAndGo(netsnmp_state_machine_input *input,
                                         netsnmp_state_machine_step *step)
{
    rowcreate_state       *ctx;
    netsnmp_variable_list *vars, *rs_var;
    int                    rc;

    netsnmp_require_ptr_LRV( input, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( step, SNMPERR_GENERR );
    netsnmp_require_ptr_LRV( input->input_context, SNMPERR_GENERR );

    ctx = (rowcreate_state *)input->input_context;

    DEBUGMSGT(("row_create:called", "called %s\n", step->name));

    vars = snmp_clone_varbind(ctx->vars);
    netsnmp_require_ptr_LRV(vars, SNMPERR_GENERR);

    /*
     * make sure row stats is createAndGo
     */
    rs_var = _get_vb_num(vars, ctx->row_status_index + 1);
    if (NULL == rs_var) {
        snmp_free_varbind(vars);
        return SNMPERR_GENERR;
    }

    if (*rs_var->val.integer != RS_CREATEANDGO)
        *rs_var->val.integer = RS_CREATEANDGO;

    /*
     * send set
     */
    rc = netsnmp_query_set( vars, ctx->session );
    if (-2 == rc)
        rc = SNMPERR_ABORT;

    snmp_free_varbind(vars);

    return rc;
}

/** **************************************************************************
 *
 * row api
 *
 */
int
netsnmp_row_create(netsnmp_session *sess, netsnmp_variable_list *vars,
                   int row_status_index)
{
    netsnmp_state_machine_step rc_cleanup =
        { "row_create_cleanup", 0, _row_status_state_cleanup,
          0, NULL, NULL, 0, NULL };
    netsnmp_state_machine_step rc_activate =
        { "row_create_activate", 0, _row_status_state_activate,
          0, NULL, NULL, 0, NULL };
    netsnmp_state_machine_step rc_sv_cols =
        { "row_create_single_value_cols", 0,
          _row_status_state_single_value_cols, 0, &rc_activate,NULL, 0, NULL };
    netsnmp_state_machine_step rc_mv_cols =
        { "row_create_multiple_values_cols", 0,
          _row_status_state_multiple_values_cols, 0, &rc_activate, &rc_sv_cols,
          0, NULL };
    netsnmp_state_machine_step rc_sv_caw =
        { "row_create_single_value_createAndWait", 0,
          _row_status_state_single_value_createAndWait, 0, &rc_mv_cols, NULL,
          0, NULL };
    netsnmp_state_machine_step rc_av_caw =
        { "row_create_all_values_createAndWait", 0,
          _row_status_state_all_values_createAndWait, 0, &rc_activate,
          &rc_sv_caw, 0, NULL };
    netsnmp_state_machine_step rc_av_cag =
        { "row_create_all_values_createAndGo", 0,
          _row_status_state_all_values_createAndGo, 0, NULL, &rc_av_caw, 0,
          NULL };

    netsnmp_state_machine_input sm_input = { "row_create_machine", 0,
                                             &rc_av_cag, &rc_cleanup };
    rowcreate_state state;

    netsnmp_require_ptr_LRV( sess, SNMPERR_GENERR);
    netsnmp_require_ptr_LRV( vars, SNMPERR_GENERR);

    state.session = sess;
    state.vars = vars;

    state.row_status_index = row_status_index;
    sm_input.input_context = &state;

    netsnmp_state_machine_run( &sm_input);

    return SNMPERR_SUCCESS;
}
#endif /* NETSNMP_FEATURE_REMOVE_ROW_CREATE */
#endif /* NETSNMP_NO_WRITE_SUPPORT */


/** @} */
