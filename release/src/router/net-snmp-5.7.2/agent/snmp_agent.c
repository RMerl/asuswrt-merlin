/*
 * snmp_agent.c
 *
 * Simple Network Management Protocol (RFC 1067).
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/* Portions of this file are subject to the following copyrights.  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

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
 * Copyright © 2003 Sun Microsystems, Inc. All rights 
 * reserved.  Use is subject to license terms specified in the 
 * COPYING file distributed with the Net-SNMP package.
 */
/** @defgroup snmp_agent net-snmp agent related processing 
 *  @ingroup agent
 *
 * @{
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <sys/types.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
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
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <errno.h>

#define SNMP_NEED_REQUEST_LIST
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/snmp_assert.h>

#if HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef NETSNMP_USE_LIBWRAP
#include <tcpd.h>
int             allow_severity = LOG_INFO;
int             deny_severity = LOG_WARNING;
#endif

#include "snmpd.h"
#include <net-snmp/agent/mib_module_config.h>
#include <net-snmp/agent/mib_modules.h>

#ifdef USING_AGENTX_PROTOCOL_MODULE
#include "agentx/protocol.h"
#endif

#ifdef USING_AGENTX_MASTER_MODULE
#include "agentx/master.h"
#endif

#ifdef USING_SMUX_MODULE
#include "smux/smux.h"
#endif

netsnmp_feature_child_of(snmp_agent, libnetsnmpagent)
netsnmp_feature_child_of(agent_debugging_utilities, libnetsnmpagent)

netsnmp_feature_child_of(allocate_globalcacheid, snmp_agent)
netsnmp_feature_child_of(free_agent_snmp_session_by_session, snmp_agent)
netsnmp_feature_child_of(check_all_requests_error, snmp_agent)
netsnmp_feature_child_of(check_requests_error, snmp_agent)
netsnmp_feature_child_of(request_set_error_idx, snmp_agent)
netsnmp_feature_child_of(set_agent_uptime, snmp_agent)
netsnmp_feature_child_of(agent_check_and_process, snmp_agent)

netsnmp_feature_child_of(dump_sess_list, agent_debugging_utilities)

netsnmp_feature_child_of(agent_remove_list_data, netsnmp_unused)
netsnmp_feature_child_of(set_all_requests_error, netsnmp_unused)
netsnmp_feature_child_of(addrcache_age, netsnmp_unused)
netsnmp_feature_child_of(delete_subtree_cache, netsnmp_unused)


NETSNMP_INLINE void
netsnmp_agent_add_list_data(netsnmp_agent_request_info *ari,
                            netsnmp_data_list *node)
{
    if (ari) {
	if (ari->agent_data) {
            netsnmp_add_list_data(&ari->agent_data, node);
        } else {
            ari->agent_data = node;
	}
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_AGENT_REMOVE_LIST_DATA
NETSNMP_INLINE int
netsnmp_agent_remove_list_data(netsnmp_agent_request_info *ari,
                               const char * name)
{
    if ((NULL == ari) || (NULL == ari->agent_data))
        return 1;

    return netsnmp_remove_list_node(&ari->agent_data, name);
}
#endif /* NETSNMP_FEATURE_REMOVE_AGENT_REMOVE_LIST_DATA */

NETSNMP_INLINE void    *
netsnmp_agent_get_list_data(netsnmp_agent_request_info *ari,
                            const char *name)
{
    if (ari) {
        return netsnmp_get_list_data(ari->agent_data, name);
    }
    return NULL;
}

NETSNMP_INLINE void
netsnmp_free_agent_data_set(netsnmp_agent_request_info *ari)
{
    if (ari) {
        netsnmp_free_list_data(ari->agent_data);
    }
}

NETSNMP_INLINE void
netsnmp_free_agent_data_sets(netsnmp_agent_request_info *ari)
{
    if (ari) {
        netsnmp_free_all_list_data(ari->agent_data);
    }
}

NETSNMP_INLINE void
netsnmp_free_agent_request_info(netsnmp_agent_request_info *ari)
{
    if (ari) {
        if (ari->agent_data) {
            netsnmp_free_all_list_data(ari->agent_data);
	}
        SNMP_FREE(ari);
    }
}

oid      version_sysoid[] = { NETSNMP_SYSTEM_MIB };
int      version_sysoid_len = OID_LENGTH(version_sysoid);

#define SNMP_ADDRCACHE_SIZE 10
#define SNMP_ADDRCACHE_MAXAGE 300 /* in seconds */

enum {
    SNMP_ADDRCACHE_UNUSED = 0,
    SNMP_ADDRCACHE_USED = 1
};

struct addrCache {
    char           *addr;
    int            status;
    struct timeval lastHitM;
};

static struct addrCache addrCache[SNMP_ADDRCACHE_SIZE];
int             log_addresses = 0;



typedef struct _agent_nsap {
    int             handle;
    netsnmp_transport *t;
    void           *s;          /*  Opaque internal session pointer.  */
    struct _agent_nsap *next;
} agent_nsap;

static agent_nsap *agent_nsap_list = NULL;
static netsnmp_agent_session *agent_session_list = NULL;
netsnmp_agent_session *netsnmp_processing_set = NULL;
netsnmp_agent_session *agent_delegated_list = NULL;
netsnmp_agent_session *netsnmp_agent_queued_list = NULL;


int             netsnmp_agent_check_packet(netsnmp_session *,
                                           struct netsnmp_transport_s *,
                                           void *, int);
int             netsnmp_agent_check_parse(netsnmp_session *, netsnmp_pdu *,
                                          int);
void            delete_subnetsnmp_tree_cache(netsnmp_agent_session *asp);
int             handle_pdu(netsnmp_agent_session *asp);
int             netsnmp_handle_request(netsnmp_agent_session *asp,
                                       int status);
int             netsnmp_wrap_up_request(netsnmp_agent_session *asp,
                                        int status);
int             check_delayed_request(netsnmp_agent_session *asp);
int             handle_getnext_loop(netsnmp_agent_session *asp);
int             handle_set_loop(netsnmp_agent_session *asp);

int             netsnmp_check_queued_chain_for(netsnmp_agent_session *asp);
int             netsnmp_add_queued(netsnmp_agent_session *asp);
int             netsnmp_remove_from_delegated(netsnmp_agent_session *asp);


static int      current_globalid = 0;

int      netsnmp_running = 1;

#ifndef NETSNMP_FEATURE_REMOVE_ALLOCATE_GLOBALCACHEID
int
netsnmp_allocate_globalcacheid(void)
{
    return ++current_globalid;
}
#endif /* NETSNMP_FEATURE_REMOVE_ALLOCATE_GLOBALCACHEID */

int
netsnmp_get_local_cachid(netsnmp_cachemap *cache_store, int globalid)
{
    while (cache_store != NULL) {
        if (cache_store->globalid == globalid)
            return cache_store->cacheid;
        cache_store = cache_store->next;
    }
    return -1;
}

netsnmp_cachemap *
netsnmp_get_or_add_local_cachid(netsnmp_cachemap **cache_store,
                                int globalid, int localid)
{
    netsnmp_cachemap *tmpp;

    tmpp = SNMP_MALLOC_TYPEDEF(netsnmp_cachemap);
    if (tmpp != NULL) {
        if (*cache_store) {
            tmpp->next = *cache_store;
            *cache_store = tmpp;
        } else {
            *cache_store = tmpp;
        }

        tmpp->globalid = globalid;
        tmpp->cacheid = localid;
    }
    return tmpp;
}

void
netsnmp_free_cachemap(netsnmp_cachemap *cache_store)
{
    netsnmp_cachemap *tmpp;
    while (cache_store) {
        tmpp = cache_store;
        cache_store = cache_store->next;
        SNMP_FREE(tmpp);
    }
}


typedef struct agent_set_cache_s {
    /*
     * match on these 2 
     */
    int             transID;
    netsnmp_session *sess;

    /*
     * store this info 
     */
    netsnmp_tree_cache *treecache;
    int             treecache_len;
    int             treecache_num;

    int             vbcount;
    netsnmp_request_info *requests;
    netsnmp_variable_list *saved_vars;
    netsnmp_data_list *agent_data;

    /*
     * list 
     */
    struct agent_set_cache_s *next;
} agent_set_cache;

static agent_set_cache *Sets = NULL;

agent_set_cache *
save_set_cache(netsnmp_agent_session *asp)
{
    agent_set_cache *ptr;

    if (!asp || !asp->reqinfo || !asp->pdu)
        return NULL;

    ptr = SNMP_MALLOC_TYPEDEF(agent_set_cache);
    if (ptr == NULL)
        return NULL;

    /*
     * Save the important information 
     */
    DEBUGMSGTL(("verbose:asp", "asp %p reqinfo %p saved in cache (mode %d)\n",
                asp, asp->reqinfo, asp->pdu->command));
    ptr->transID = asp->pdu->transid;
    ptr->sess = asp->session;
    ptr->treecache = asp->treecache;
    ptr->treecache_len = asp->treecache_len;
    ptr->treecache_num = asp->treecache_num;
    ptr->agent_data = asp->reqinfo->agent_data;
    ptr->requests = asp->requests;
    ptr->saved_vars = asp->pdu->variables; /* requests contains pointers to variables */
    ptr->vbcount = asp->vbcount;

    /*
     * make the agent forget about what we've saved 
     */
    asp->treecache = NULL;
    asp->reqinfo->agent_data = NULL;
    asp->pdu->variables = NULL;
    asp->requests = NULL;

    ptr->next = Sets;
    Sets = ptr;

    return ptr;
}

int
get_set_cache(netsnmp_agent_session *asp)
{
    agent_set_cache *ptr, *prev = NULL;

    for (ptr = Sets; ptr != NULL; ptr = ptr->next) {
        if (ptr->sess == asp->session && ptr->transID == asp->pdu->transid) {
            /*
             * remove this item from list
             */
            if (prev)
                prev->next = ptr->next;
            else
                Sets = ptr->next;

            /*
             * found it.  Get the needed data 
             */
            asp->treecache = ptr->treecache;
            asp->treecache_len = ptr->treecache_len;
            asp->treecache_num = ptr->treecache_num;

            /*
             * Free previously allocated requests before overwriting by
             * cached ones, otherwise memory leaks!
             */
	    if (asp->requests) {
                /*
                 * I don't think this case should ever happen. Please email
                 * the net-snmp-coders@lists.sourceforge.net if you have
                 * a test case that hits this condition. -- rstory
                 */
		int i;
                netsnmp_assert(NULL == asp->requests); /* see note above */
		for (i = 0; i < asp->vbcount; i++) {
		    netsnmp_free_request_data_sets(&asp->requests[i]);
		}
		free(asp->requests);
	    }
	    /*
	     * If we replace asp->requests with the info from the set cache,
	     * we should replace asp->pdu->variables also with the cached
	     * info, as asp->requests contains pointers to them.  And we
	     * should also free the current asp->pdu->variables list...
	     */
	    if (ptr->saved_vars) {
		if (asp->pdu->variables)
		    snmp_free_varbind(asp->pdu->variables);
		asp->pdu->variables = ptr->saved_vars;
                asp->vbcount = ptr->vbcount;
	    } else {
                /*
                 * when would we not have saved variables? someone
                 * let me know if they hit this condition. -- rstory
                 */
                netsnmp_assert(NULL != ptr->saved_vars);
            }
            asp->requests = ptr->requests;

            netsnmp_assert(NULL != asp->reqinfo);
            asp->reqinfo->asp = asp;
            asp->reqinfo->agent_data = ptr->agent_data;
            
            /*
             * update request reqinfo, if it's out of date.
             * yyy-rks: investigate when/why sometimes they match,
             * sometimes they don't.
             */
            if(asp->requests->agent_req_info != asp->reqinfo) {
                /*
                 * - one don't match case: agentx subagents. prev asp & reqinfo
                 *   freed, request reqinfo ptrs not cleared.
                 */
                netsnmp_request_info *tmp = asp->requests;
                DEBUGMSGTL(("verbose:asp",
                            "  reqinfo %p doesn't match cached reqinfo %p\n",
                            asp->reqinfo, asp->requests->agent_req_info));
                for(; tmp; tmp = tmp->next)
                    tmp->agent_req_info = asp->reqinfo;
            } else {
                /*
                 * - match case: ?
                 */
                DEBUGMSGTL(("verbose:asp",
                            "  reqinfo %p matches cached reqinfo %p\n",
                            asp->reqinfo, asp->requests->agent_req_info));
            }

            SNMP_FREE(ptr);
            return SNMP_ERR_NOERROR;
        }
        prev = ptr;
    }
    return SNMP_ERR_GENERR;
}

/* Bulkcache holds the values for the *repeating* varbinds (only),
 *   but ordered "by column" - i.e. the repetitions for each
 *   repeating varbind follow on immediately from one another,
 *   rather than being interleaved, as required by the protocol.
 *
 * So we need to rearrange the varbind list so it's ordered "by row".
 *
 * In the following code chunk:
 *     n            = # non-repeating varbinds
 *     r            = # repeating varbinds
 *     asp->vbcount = # varbinds in the incoming PDU
 *         (So asp->vbcount = n+r)
 *
 *     repeats = Desired # of repetitions (of 'r' varbinds)
 */
NETSNMP_STATIC_INLINE void
_reorder_getbulk(netsnmp_agent_session *asp)
{
    int             i, n = 0, r = 0;
    int             repeats = asp->pdu->errindex;
    int             j, k;
    int             all_eoMib;
    netsnmp_variable_list *prev = NULL, *curr;
            
    if (asp->vbcount == 0)  /* Nothing to do! */
        return;

    if (asp->pdu->errstat < asp->vbcount) {
        n = asp->pdu->errstat;
    } else {
        n = asp->vbcount;
    }
    if ((r = asp->vbcount - n) < 0) {
        r = 0;
    }

    /* we do nothing if there is nothing repeated */
    if (r == 0)
        return;
            
    /* Fix endOfMibView entries. */
    for (i = 0; i < r; i++) {
        prev = NULL;
        for (j = 0; j < repeats; j++) {
	    curr = asp->bulkcache[i * repeats + j];
            /*
             *  If we don't have a valid name for a given repetition
             *   (and probably for all the ones that follow as well),
             *   extend the previous result to indicate 'endOfMibView'.
             *  Or if the repetition already has type endOfMibView make
             *   sure it has the correct objid (i.e. that of the previous
             *   entry or that of the original request).
             */
            if (curr->name_length == 0 || curr->type == SNMP_ENDOFMIBVIEW) {
		if (prev == NULL) {
		    /* Use objid from original pdu. */
		    prev = asp->orig_pdu->variables;
		    for (k = i; prev && k > 0; k--)
			prev = prev->next_variable;
		}
		if (prev) {
		    snmp_set_var_objid(curr, prev->name, prev->name_length);
		    snmp_set_var_typed_value(curr, SNMP_ENDOFMIBVIEW, NULL, 0);
		}
            }
            prev = curr;
        }
    }

    /*
     * For each of the original repeating varbinds (except the last),
     *  go through the block of results for that varbind,
     *  and link each instance to the corresponding instance
     *  in the next block.
     */
    for (i = 0; i < r - 1; i++) {
        for (j = 0; j < repeats; j++) {
            asp->bulkcache[i * repeats + j]->next_variable =
                asp->bulkcache[(i + 1) * repeats + j];
        }
    }

    /*
     * For the last of the original repeating varbinds,
     *  go through that block of results, and link each
     *  instance to the *next* instance in the *first* block.
     *
     * The very last instance of this block is left untouched
     *  since it (correctly) points to the end of the list.
     */
    for (j = 0; j < repeats - 1; j++) {
	asp->bulkcache[(r - 1) * repeats + j]->next_variable = 
	    asp->bulkcache[j + 1];
    }

    /*
     * If we've got a full row of endOfMibViews, then we
     *  can truncate the result varbind list after that.
     *
     * Look for endOfMibView exception values in the list of
     *  repetitions for the first varbind, and check the 
     *  corresponding instances for the other varbinds
     *  (following the next_variable links).
     *
     * If they're all endOfMibView too, then we can terminate
     *  the linked list there, and free any redundant varbinds.
     */
    all_eoMib = 0;
    for (i = 0; i < repeats; i++) {
        if (asp->bulkcache[i]->type == SNMP_ENDOFMIBVIEW) {
            all_eoMib = 1;
            for (j = 1, prev=asp->bulkcache[i];
                 j < r;
                 j++, prev=prev->next_variable) {
                if (prev->type != SNMP_ENDOFMIBVIEW) {
                    all_eoMib = 0;
                    break;	/* Found a real value */
                }
            }
            if (all_eoMib) {
                /*
                 * This is indeed a full endOfMibView row.
                 * Terminate the list here & free the rest.
                 */
                snmp_free_varbind( prev->next_variable );
                prev->next_variable = NULL;
                break;
            }
        }
    }
}


/* EndOfMibView replies to a GETNEXT request should according to RFC3416
 *  have the object ID set to that of the request. Our tree search 
 *  algorithm will sometimes break that requirement. This function will
 *  fix that.
 */
NETSNMP_STATIC_INLINE void
_fix_endofmibview(netsnmp_agent_session *asp)
{
    netsnmp_variable_list *vb, *ovb;

    if (asp->vbcount == 0)  /* Nothing to do! */
        return;

    for (vb = asp->pdu->variables, ovb = asp->orig_pdu->variables;
	 vb && ovb; vb = vb->next_variable, ovb = ovb->next_variable) {
	if (vb->type == SNMP_ENDOFMIBVIEW)
	    snmp_set_var_objid(vb, ovb->name, ovb->name_length);
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_AGENT_CHECK_AND_PROCESS
/**
 * This function checks for packets arriving on the SNMP port and
 * processes them(snmp_read) if some are found, using the select(). If block
 * is non zero, the function call blocks until a packet arrives
 *
 * @param block used to control blocking in the select() function, 1 = block
 *        forever, and 0 = don't block
 *
 * @return  Returns a positive integer if packets were processed, and -1 if an
 * error was found.
 *
 */
int
agent_check_and_process(int block)
{
    int             numfds;
    fd_set          fdset;
    struct timeval  timeout = { LONG_MAX, 0 }, *tvp = &timeout;
    int             count;
    int             fakeblock = 0;

    numfds = 0;
    FD_ZERO(&fdset);
    snmp_select_info(&numfds, &fdset, tvp, &fakeblock);
    if (block != 0 && fakeblock != 0) {
        /*
         * There are no alarms registered, and the caller asked for blocking, so
         * let select() block forever.  
         */

        tvp = NULL;
    } else if (block != 0 && fakeblock == 0) {
        /*
         * The caller asked for blocking, but there is an alarm due sooner than
         * LONG_MAX seconds from now, so use the modified timeout returned by
         * snmp_select_info as the timeout for select().  
         */

    } else if (block == 0) {
        /*
         * The caller does not want us to block at all.  
         */

        timerclear(tvp);
    }

    count = select(numfds, &fdset, NULL, NULL, tvp);

    if (count > 0) {
        /*
         * packets found, process them 
         */
        snmp_read(&fdset);
    } else
        switch (count) {
        case 0:
            snmp_timeout();
            break;
        case -1:
            if (errno != EINTR) {
                snmp_log_perror("select");
            }
            return -1;
        default:
            snmp_log(LOG_ERR, "select returned %d\n", count);
            return -1;
        }                       /* endif -- count>0 */

    /*
     * see if persistent store needs to be saved
     */
    snmp_store_if_needed();

    /*
     * Run requested alarms.  
     */
    run_alarms();

    netsnmp_check_outstanding_agent_requests();

    return count;
}
#endif /* NETSNMP_FEATURE_REMOVE_AGENT_CHECK_AND_PROCESS */

/*
 * Set up the address cache.  
 */
void
netsnmp_addrcache_initialise(void)
{
    int             i = 0;

    for (i = 0; i < SNMP_ADDRCACHE_SIZE; i++) {
        addrCache[i].addr = NULL;
        addrCache[i].status = SNMP_ADDRCACHE_UNUSED;
    }
}

void netsnmp_addrcache_destroy(void)
{
    int             i = 0;

    for (i = 0; i < SNMP_ADDRCACHE_SIZE; i++) {
        if (addrCache[i].status == SNMP_ADDRCACHE_USED) {
            free(addrCache[i].addr);
            addrCache[i].status = SNMP_ADDRCACHE_UNUSED;
        }
    }
}

/*
 * Adds a new entry to the cache of addresses that
 * have recently made connections to the agent.
 * Returns 0 if the entry already exists (but updates
 * the entry with a new timestamp) and 1 if the
 * entry did not previously exist.
 *
 * Implements a simple LRU cache replacement
 * policy. Uses a linear search, which should be
 * okay, as long as SNMP_ADDRCACHE_SIZE remains
 * relatively small.
 *
 * @retval 0 : updated existing entry
 * @retval 1 : added new entry
 */
int
netsnmp_addrcache_add(const char *addr)
{
    int oldest = -1; /* Index of the oldest cache entry */
    int unused = -1; /* Index of the first free cache entry */
    int i; /* Looping variable */
    int rc = -1;
    struct timeval now; /* What time is it now? */
    struct timeval aged; /* Oldest allowable cache entry */

    /*
     * First get the current and oldest allowable timestamps
     */
    netsnmp_get_monotonic_clock(&now);
    aged.tv_sec = now.tv_sec - SNMP_ADDRCACHE_MAXAGE;
    aged.tv_usec = now.tv_usec;

    /*
     * Now look for a place to put this thing
     */
    for(i = 0; i < SNMP_ADDRCACHE_SIZE; i++) {
        if (addrCache[i].status == SNMP_ADDRCACHE_UNUSED) { /* If unused */
            /*
             * remember this location, in case addr isn't in the cache
             */
            if (unused < 0)
                unused = i;
        }
        else { /* If used */
            if ((NULL != addr) && (strcmp(addrCache[i].addr, addr) == 0)) {
                /*
                 * found a match
                 */
                addrCache[i].lastHitM = now;
                if (timercmp(&addrCache[i].lastHitM, &aged, <))
		    rc = 1; /* should have expired, so is new */
		else
		    rc = 0; /* not expired, so is existing entry */
                break;
            }
            else {
                /*
                 * Used, but not this address. check if it's stale.
                 */
                if (timercmp(&addrCache[i].lastHitM, &aged, <)) {
                    /*
                     * Stale, reuse
                     */
                    SNMP_FREE(addrCache[i].addr);
                    addrCache[i].status = SNMP_ADDRCACHE_UNUSED;
                    /*
                     * remember this location, in case addr isn't in the cache
                     */
		    if (unused < 0)
                        unused = i;
                }
	        else {
                    /*
                     * Still fresh, but a candidate for LRU replacement
                     */
                    if (oldest < 0)
                        oldest = i;
                    else if (timercmp(&addrCache[i].lastHitM,
                                      &addrCache[oldest].lastHitM, <))
                        oldest = i;
                } /* fresh */
            } /* used, no match */
        } /* used */
    } /* for loop */

    if ((-1 == rc) && (NULL != addr)) {
        /*
         * We didn't find the entry in the cache
         */
        if (unused >= 0) {
            /*
             * If we have a slot free anyway, use it
             */
            addrCache[unused].addr = strdup(addr);
            addrCache[unused].status = SNMP_ADDRCACHE_USED;
            addrCache[unused].lastHitM = now;
        }
        else { /* Otherwise, replace oldest entry */
            if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                       NETSNMP_DS_AGENT_VERBOSE))
                snmp_log(LOG_INFO, "Purging address from address cache: %s",
                         addrCache[oldest].addr);
            
            free(addrCache[oldest].addr);
            addrCache[oldest].addr = strdup(addr);
            addrCache[oldest].lastHitM = now;
        }
        rc = 1;
    }
    if ((log_addresses && (1 == rc)) ||
        netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_VERBOSE)) {
        snmp_log(LOG_INFO, "Received SNMP packet(s) from %s\n", addr);
     }

    return rc;
}

/*
 * Age the entries in the address cache.  
 *
 * backwards compatability; not used anywhere
 */
#ifndef NETSNMP_FEATURE_REMOVE_ADDRCACHE_AGE
void
netsnmp_addrcache_age(void)
{
    (void)netsnmp_addrcache_add(NULL);
}
#endif /* NETSNMP_FEATURE_REMOVE_ADDRCACHE_AGE */

/*******************************************************************-o-******
 * netsnmp_agent_check_packet
 *
 * Parameters:
 *	session, transport, transport_data, transport_data_length
 *      
 * Returns:
 *	1	On success.
 *	0	On error.
 *
 * Handler for all incoming messages (a.k.a. packets) for the agent.  If using
 * the libwrap utility, log the connection and deny/allow the access. Print
 * output when appropriate, and increment the incoming counter.
 *
 */

int
netsnmp_agent_check_packet(netsnmp_session * session,
                           netsnmp_transport *transport,
                           void *transport_data, int transport_data_length)
{
    char           *addr_string = NULL;
#ifdef  NETSNMP_USE_LIBWRAP
    char *tcpudpaddr = NULL, *name;
    short not_log_connection;

    name = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                 NETSNMP_DS_LIB_APPTYPE);

    /* not_log_connection will be 1 if we should skip the messages */
    not_log_connection = netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                                NETSNMP_DS_AGENT_DONT_LOG_TCPWRAPPERS_CONNECTS);

    /*
     * handle the error case
     * default to logging the messages
     */
    if (not_log_connection == SNMPERR_GENERR) not_log_connection = 0;
#endif

    /*
     * Log the message and/or dump the message.
     * Optionally cache the network address of the sender.
     */

    if (transport != NULL && transport->f_fmtaddr != NULL) {
        /*
         * Okay I do know how to format this address for logging.  
         */
        addr_string = transport->f_fmtaddr(transport, transport_data,
                                           transport_data_length);
        /*
         * Don't forget to free() it.  
         */
    }
#ifdef  NETSNMP_USE_LIBWRAP
    /* Catch udp,udp6,tcp,tcp6 transports using "[" */
    if (addr_string)
        tcpudpaddr = strstr(addr_string, "[");
    if ( tcpudpaddr != 0 ) {
        char sbuf[64];
        char *xp;

        strlcpy(sbuf, tcpudpaddr + 1, sizeof(sbuf));
        xp = strstr(sbuf, "]");
        if (xp)
            *xp = '\0';
 
        if (hosts_ctl(name, STRING_UNKNOWN, sbuf, STRING_UNKNOWN)) {
            if (!not_log_connection) {
                snmp_log(allow_severity, "Connection from %s\n", addr_string);
            }
        } else {
            snmp_log(deny_severity, "Connection from %s REFUSED\n",
                     addr_string);
            SNMP_FREE(addr_string);
            return 0;
        }
    } else {
        /*
         * don't log callback connections.
         * What about 'Local IPC', 'IPX' and 'AAL5 PVC'?
         */
        if (0 == strncmp(addr_string, "callback", 8))
            ;
        else if (hosts_ctl(name, STRING_UNKNOWN, STRING_UNKNOWN, STRING_UNKNOWN)){
            if (!not_log_connection) {
                snmp_log(allow_severity, "Connection from <UNKNOWN> (%s)\n", addr_string);
            };
            SNMP_FREE(addr_string);
            addr_string = strdup("<UNKNOWN>");
        } else {
            snmp_log(deny_severity, "Connection from <UNKNOWN> (%s) REFUSED\n", addr_string);
            SNMP_FREE(addr_string);
            return 0;
        }
    }
#endif                          /*NETSNMP_USE_LIBWRAP */

    snmp_increment_statistic(STAT_SNMPINPKTS);

    if (addr_string != NULL) {
        netsnmp_addrcache_add(addr_string);
        SNMP_FREE(addr_string);
    }
    return 1;
}


int
netsnmp_agent_check_parse(netsnmp_session * session, netsnmp_pdu *pdu,
                          int result)
{
    if (result == 0) {
        if (snmp_get_do_logging() &&
	    netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_AGENT_VERBOSE)) {
            netsnmp_variable_list *var_ptr;

            switch (pdu->command) {
            case SNMP_MSG_GET:
                snmp_log(LOG_DEBUG, "  GET message\n");
                break;
            case SNMP_MSG_GETNEXT:
                snmp_log(LOG_DEBUG, "  GETNEXT message\n");
                break;
            case SNMP_MSG_RESPONSE:
                snmp_log(LOG_DEBUG, "  RESPONSE message\n");
                break;
#ifndef NETSNMP_NO_WRITE_SUPPORT
            case SNMP_MSG_SET:
                snmp_log(LOG_DEBUG, "  SET message\n");
                break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            case SNMP_MSG_TRAP:
                snmp_log(LOG_DEBUG, "  TRAP message\n");
                break;
            case SNMP_MSG_GETBULK:
                snmp_log(LOG_DEBUG, "  GETBULK message, non-rep=%ld, max_rep=%ld\n",
                         pdu->errstat, pdu->errindex);
                break;
            case SNMP_MSG_INFORM:
                snmp_log(LOG_DEBUG, "  INFORM message\n");
                break;
            case SNMP_MSG_TRAP2:
                snmp_log(LOG_DEBUG, "  TRAP2 message\n");
                break;
            case SNMP_MSG_REPORT:
                snmp_log(LOG_DEBUG, "  REPORT message\n");
                break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
            case SNMP_MSG_INTERNAL_SET_RESERVE1:
                snmp_log(LOG_DEBUG, "  INTERNAL RESERVE1 message\n");
                break;

            case SNMP_MSG_INTERNAL_SET_RESERVE2:
                snmp_log(LOG_DEBUG, "  INTERNAL RESERVE2 message\n");
                break;

            case SNMP_MSG_INTERNAL_SET_ACTION:
                snmp_log(LOG_DEBUG, "  INTERNAL ACTION message\n");
                break;

            case SNMP_MSG_INTERNAL_SET_COMMIT:
                snmp_log(LOG_DEBUG, "  INTERNAL COMMIT message\n");
                break;

            case SNMP_MSG_INTERNAL_SET_FREE:
                snmp_log(LOG_DEBUG, "  INTERNAL FREE message\n");
                break;

            case SNMP_MSG_INTERNAL_SET_UNDO:
                snmp_log(LOG_DEBUG, "  INTERNAL UNDO message\n");
                break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

            default:
                snmp_log(LOG_DEBUG, "  UNKNOWN message, type=%02X\n",
                         pdu->command);
                snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
                return 0;
            }

            for (var_ptr = pdu->variables; var_ptr != NULL;
                 var_ptr = var_ptr->next_variable) {
                size_t          c_oidlen = 256, c_outlen = 0;
                u_char         *c_oid = (u_char *) malloc(c_oidlen);

                if (c_oid) {
                    if (!sprint_realloc_objid
                        (&c_oid, &c_oidlen, &c_outlen, 1, var_ptr->name,
                         var_ptr->name_length)) {
                        snmp_log(LOG_DEBUG, "    -- %s [TRUNCATED]\n",
                                 c_oid);
                    } else {
                        snmp_log(LOG_DEBUG, "    -- %s\n", c_oid);
                    }
                    SNMP_FREE(c_oid);
                }
            }
        }
        return 1;
    }
    return 0;                   /* XXX: does it matter what the return value
                                 * is?  Yes: if we return 0, then the PDU is
                                 * dumped.  */
}


/*
 * Global access to the primary session structure for this agent.
 * for Index Allocation use initially. 
 */

/*
 * I don't understand what this is for at the moment.  AFAICS as long as it
 * gets set and points at a session, that's fine.  ???  
 */

netsnmp_session *main_session = NULL;



/*
 * Set up an agent session on the given transport.  Return a handle
 * which may later be used to de-register this transport.  A return
 * value of -1 indicates an error.  
 */

int
netsnmp_register_agent_nsap(netsnmp_transport *t)
{
    netsnmp_session *s, *sp = NULL;
    agent_nsap     *a = NULL, *n = NULL, **prevNext = &agent_nsap_list;
    int             handle = 0;
    void           *isp = NULL;

    if (t == NULL) {
        return -1;
    }

    DEBUGMSGTL(("netsnmp_register_agent_nsap", "fd %d\n", t->sock));

    n = (agent_nsap *) malloc(sizeof(agent_nsap));
    if (n == NULL) {
        return -1;
    }
    s = (netsnmp_session *) malloc(sizeof(netsnmp_session));
    if (s == NULL) {
        SNMP_FREE(n);
        return -1;
    }
    memset(s, 0, sizeof(netsnmp_session));
    snmp_sess_init(s);

    /*
     * Set up the session appropriately for an agent.  
     */

    s->version = SNMP_DEFAULT_VERSION;
    s->callback = handle_snmp_packet;
    s->authenticator = NULL;
    s->flags = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 
				  NETSNMP_DS_AGENT_FLAGS);
    s->isAuthoritative = SNMP_SESS_AUTHORITATIVE;

    /* Optional supplimental transport configuration information and
       final call to actually open the transport */
    if (netsnmp_sess_config_transport(s->transport_configuration, t)
        != SNMPERR_SUCCESS) {
        SNMP_FREE(s);
        SNMP_FREE(n);
        return -1;
    }


    if (t->f_open)
        t = t->f_open(t);

    if (NULL == t) {
        SNMP_FREE(s);
        SNMP_FREE(n);
        return -1;
    }

    t->flags |= NETSNMP_TRANSPORT_FLAG_OPENED;

    sp = snmp_add(s, t, netsnmp_agent_check_packet,
                  netsnmp_agent_check_parse);
    if (sp == NULL) {
        SNMP_FREE(s);
        SNMP_FREE(n);
        return -1;
    }

    isp = snmp_sess_pointer(sp);
    if (isp == NULL) {          /*  over-cautious  */
        SNMP_FREE(s);
        SNMP_FREE(n);
        return -1;
    }

    n->s = isp;
    n->t = t;

    if (main_session == NULL) {
        main_session = snmp_sess_session(isp);
    }

    for (a = agent_nsap_list; a != NULL && handle + 1 >= a->handle;
         a = a->next) {
        handle = a->handle;
        prevNext = &(a->next);
    }

    if (handle < INT_MAX) {
        n->handle = handle + 1;
        n->next = a;
        *prevNext = n;
        SNMP_FREE(s);
        return n->handle;
    } else {
        SNMP_FREE(s);
        SNMP_FREE(n);
        return -1;
    }
}

void
netsnmp_deregister_agent_nsap(int handle)
{
    agent_nsap     *a = NULL, **prevNext = &agent_nsap_list;
    int             main_session_deregistered = 0;

    DEBUGMSGTL(("netsnmp_deregister_agent_nsap", "handle %d\n", handle));

    for (a = agent_nsap_list; a != NULL && a->handle < handle; a = a->next) {
        prevNext = &(a->next);
    }

    if (a != NULL && a->handle == handle) {
        *prevNext = a->next;
	if (snmp_sess_session_lookup(a->s)) {
            if (main_session == snmp_sess_session(a->s)) {
                main_session_deregistered = 1;
            }
            snmp_close(snmp_sess_session(a->s));
            /*
             * The above free()s the transport and session pointers.  
             */
        }
        SNMP_FREE(a);
    }

    /*
     * If we've deregistered the session that main_session used to point to,
     * then make it point to another one, or in the last resort, make it equal
     * to NULL.  Basically this shouldn't ever happen in normal operation
     * because main_session starts off pointing at the first session added by
     * init_master_agent(), which then discards the handle.  
     */

    if (main_session_deregistered) {
        if (agent_nsap_list != NULL) {
            DEBUGMSGTL(("snmp_agent",
			"WARNING: main_session ptr changed from %p to %p\n",
                        main_session, snmp_sess_session(agent_nsap_list->s)));
            main_session = snmp_sess_session(agent_nsap_list->s);
        } else {
            DEBUGMSGTL(("snmp_agent",
			"WARNING: main_session ptr changed from %p to NULL\n",
                        main_session));
            main_session = NULL;
        }
    }
}



/*
 * 
 * This function has been modified to use the experimental
 * netsnmp_register_agent_nsap interface.  The major responsibility of this
 * function now is to interpret a string specified to the agent (via -p on the
 * command line, or from a configuration file) as a list of agent NSAPs on
 * which to listen for SNMP packets.  Typically, when you add a new transport
 * domain "foo", you add code here such that if the "foo" code is compiled
 * into the agent (SNMP_TRANSPORT_FOO_DOMAIN is defined), then a token of the
 * form "foo:bletch-3a0054ef%wob&wob" gets turned into the appropriate
 * transport descriptor.  netsnmp_register_agent_nsap is then called with that
 * transport descriptor and sets up a listening agent session on it.
 * 
 * Everything then works much as normal: the agent runs in an infinite loop
 * (in the snmpd.c/receive()routine), which calls snmp_read() when a request
 * is readable on any of the given transports.  This routine then traverses
 * the library 'Sessions' list to identify the relevant session and eventually
 * invokes '_sess_read'.  This then processes the incoming packet, calling the
 * pre_parse, parse, post_parse and callback routines in turn.
 * 
 * JBPN 20001117
 */

int
init_master_agent(void)
{
    netsnmp_transport *transport;
    char           *cptr;
    char           *buf = NULL;
    char           *st;

    /* default to a default cache size */
    netsnmp_set_lookup_cache_size(-1);

    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_ROLE) != MASTER_AGENT) {
        DEBUGMSGTL(("snmp_agent",
                    "init_master_agent; not master agent\n"));

        netsnmp_assert("agent role !master && !sub_agent");
        
        return 0;               /*  No error if ! MASTER_AGENT  */
    }

#ifndef NETSNMP_NO_LISTEN_SUPPORT
    /*
     * Have specific agent ports been specified?  
     */
    cptr = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
				 NETSNMP_DS_AGENT_PORTS);

    if (cptr) {
        buf = strdup(cptr);
        if (!buf) {
            snmp_log(LOG_ERR,
                     "Error processing transport \"%s\"\n", cptr);
            return 1;
        }
    } else {
        /*
         * No, so just specify the default port.  
         */
        buf = strdup("");
    }

    DEBUGMSGTL(("snmp_agent", "final port spec: \"%s\"\n", buf));
    st = buf;
    do {
        /*
         * Specification format: 
         * 
         * NONE:                      (a pseudo-transport)
         * UDP:[address:]port        (also default if no transport is specified)
         * TCP:[address:]port         (if supported)
         * Unix:pathname              (if supported)
         * AAL5PVC:itf.vpi.vci        (if supported)
         * IPX:[network]:node[/port] (if supported)
         * 
         */

	cptr = st;
	st = strchr(st, ',');
	if (st)
	    *st++ = '\0';

        DEBUGMSGTL(("snmp_agent", "installing master agent on port %s\n",
                    cptr));

        if (strncasecmp(cptr, "none", 4) == 0) {
            DEBUGMSGTL(("snmp_agent",
                        "init_master_agent; pseudo-transport \"none\" "
			"requested\n"));
            break;
        }
        transport = netsnmp_transport_open_server("snmp", cptr);

        if (transport == NULL) {
            snmp_log(LOG_ERR, "Error opening specified endpoint \"%s\"\n",
                     cptr);
            return 1;
        }

        if (netsnmp_register_agent_nsap(transport) == 0) {
            snmp_log(LOG_ERR,
                     "Error registering specified transport \"%s\" as an "
		     "agent NSAP\n", cptr);
            return 1;
        } else {
            DEBUGMSGTL(("snmp_agent",
                        "init_master_agent; \"%s\" registered as an agent "
			"NSAP\n", cptr));
        }
    } while(st && *st != '\0');
    SNMP_FREE(buf);
#endif /* NETSNMP_NO_LISTEN_SUPPORT */

#ifdef USING_AGENTX_MASTER_MODULE
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_AGENTX_MASTER) == 1)
        real_init_master();
#endif
#ifdef USING_SMUX_MODULE
    if(should_init("smux"))
    real_init_smux();
#endif

    return 0;
}

void
clear_nsap_list(void)
{
    DEBUGMSGTL(("clear_nsap_list", "clear the nsap list\n"));

    while (agent_nsap_list != NULL)
	netsnmp_deregister_agent_nsap(agent_nsap_list->handle);
}

void
shutdown_master_agent(void)
{
    clear_nsap_list();
}


netsnmp_agent_session *
init_agent_snmp_session(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_agent_session *asp = (netsnmp_agent_session *)
        calloc(1, sizeof(netsnmp_agent_session));

    if (asp == NULL) {
        return NULL;
    }

    DEBUGMSGTL(("snmp_agent","agent_sesion %8p created\n", asp));
    asp->session = session;
    asp->pdu = snmp_clone_pdu(pdu);
    asp->orig_pdu = snmp_clone_pdu(pdu);
    asp->rw = READ;
    asp->exact = TRUE;
    asp->next = NULL;
    asp->mode = RESERVE1;
    asp->status = SNMP_ERR_NOERROR;
    asp->index = 0;
    asp->oldmode = 0;
    asp->treecache_num = -1;
    asp->treecache_len = 0;
    asp->reqinfo = SNMP_MALLOC_TYPEDEF(netsnmp_agent_request_info);
    DEBUGMSGTL(("verbose:asp", "asp %p reqinfo %p created\n",
                asp, asp->reqinfo));

    return asp;
}

void
free_agent_snmp_session(netsnmp_agent_session *asp)
{
    if (!asp)
        return;

    DEBUGMSGTL(("snmp_agent","agent_session %8p released\n", asp));

    netsnmp_remove_from_delegated(asp);
    
    DEBUGMSGTL(("verbose:asp", "asp %p reqinfo %p freed\n",
                asp, asp->reqinfo));
    if (asp->orig_pdu)
        snmp_free_pdu(asp->orig_pdu);
    if (asp->pdu)
        snmp_free_pdu(asp->pdu);
    if (asp->reqinfo)
        netsnmp_free_agent_request_info(asp->reqinfo);
    SNMP_FREE(asp->treecache);
    SNMP_FREE(asp->bulkcache);
    if (asp->requests) {
        int             i;
        for (i = 0; i < asp->vbcount; i++) {
            netsnmp_free_request_data_sets(&asp->requests[i]);
        }
        SNMP_FREE(asp->requests);
    }
    if (asp->cache_store) {
        netsnmp_free_cachemap(asp->cache_store);
        asp->cache_store = NULL;
    }
    SNMP_FREE(asp);
}

int
netsnmp_check_for_delegated(netsnmp_agent_session *asp)
{
    int             i;
    netsnmp_request_info *request;

    if (NULL == asp->treecache)
        return 0;
    
    for (i = 0; i <= asp->treecache_num; i++) {
        for (request = asp->treecache[i].requests_begin; request;
             request = request->next) {
            if (request->delegated)
                return 1;
        }
    }
    return 0;
}

int
netsnmp_check_delegated_chain_for(netsnmp_agent_session *asp)
{
    netsnmp_agent_session *asptmp;
    for (asptmp = agent_delegated_list; asptmp; asptmp = asptmp->next) {
        if (asptmp == asp)
            return 1;
    }
    return 0;
}

int
netsnmp_check_for_delegated_and_add(netsnmp_agent_session *asp)
{
    if (netsnmp_check_for_delegated(asp)) {
        if (!netsnmp_check_delegated_chain_for(asp)) {
            /*
             * add to delegated request chain 
             */
            asp->next = agent_delegated_list;
            agent_delegated_list = asp;
            DEBUGMSGTL(("snmp_agent", "delegate session == %8p\n", asp));
        }
        return 1;
    }
    return 0;
}

int
netsnmp_remove_from_delegated(netsnmp_agent_session *asp)
{
    netsnmp_agent_session *curr, *prev = NULL;
    
    for (curr = agent_delegated_list; curr; prev = curr, curr = curr->next) {
        /*
         * is this us?
         */
        if (curr != asp)
            continue;
        
        /*
         * remove from queue 
         */
        if (prev != NULL)
            prev->next = asp->next;
        else
            agent_delegated_list = asp->next;

        DEBUGMSGTL(("snmp_agent", "remove delegated session == %8p\n", asp));

        return 1;
    }

    return 0;
}

/*
 * netsnmp_remove_delegated_requests_for_session
 *
 * called when a session is being closed. Check all delegated requests to
 * see if the are waiting on this session, and if set, set the status for
 * that request to GENERR.
 */
int
netsnmp_remove_delegated_requests_for_session(netsnmp_session *sess)
{
    netsnmp_agent_session *asp;
    int count = 0;
    
    for (asp = agent_delegated_list; asp; asp = asp->next) {
        /*
         * check each request
         */
        netsnmp_request_info *request;
        for(request = asp->requests; request; request = request->next) {
            /*
             * check session
             */
            netsnmp_assert(NULL!=request->subtree);
            if(request->subtree->session != sess)
                continue;

            /*
             * matched! mark request as done
             */
            netsnmp_request_set_error(request, SNMP_ERR_GENERR);
            ++count;
        }
    }

    /*
     * if we found any, that request may be finished now
     */
    if(count) {
        DEBUGMSGTL(("snmp_agent", "removed %d delegated request(s) for session "
                    "%8p\n", count, sess));
        netsnmp_check_outstanding_agent_requests();
    }
    
    return count;
}

int
netsnmp_check_queued_chain_for(netsnmp_agent_session *asp)
{
    netsnmp_agent_session *asptmp;
    for (asptmp = netsnmp_agent_queued_list; asptmp; asptmp = asptmp->next) {
        if (asptmp == asp)
            return 1;
    }
    return 0;
}

int
netsnmp_add_queued(netsnmp_agent_session *asp)
{
    netsnmp_agent_session *asp_tmp;

    /*
     * first item?
     */
    if (NULL == netsnmp_agent_queued_list) {
        netsnmp_agent_queued_list = asp;
        return 1;
    }


    /*
     * add to end of queued request chain 
     */
    asp_tmp = netsnmp_agent_queued_list;
    for (; asp_tmp; asp_tmp = asp_tmp->next) {
        /*
         * already in queue?
         */
        if (asp_tmp == asp)
            break;

        /*
         * end of queue?
         */
        if (NULL == asp_tmp->next)
            asp_tmp->next = asp;
    }
    return 1;
}


int
netsnmp_wrap_up_request(netsnmp_agent_session *asp, int status)
{
    /*
     * if this request was a set, clear the global now that we are
     * done.
     */
    if (asp == netsnmp_processing_set) {
        DEBUGMSGTL(("snmp_agent", "SET request complete, asp = %8p\n",
                    asp));
        netsnmp_processing_set = NULL;
    }

    if (asp->pdu) {
        /*
         * If we've got an error status, then this needs to be
         *  passed back up to the higher levels....
         */
        if ( status != 0  && asp->status == 0 )
            asp->status = status;

        switch (asp->pdu->command) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
            case SNMP_MSG_INTERNAL_SET_BEGIN:
            case SNMP_MSG_INTERNAL_SET_RESERVE1:
            case SNMP_MSG_INTERNAL_SET_RESERVE2:
            case SNMP_MSG_INTERNAL_SET_ACTION:
                /*
                 * some stuff needs to be saved in special subagent cases 
                 */
                save_set_cache(asp);
                break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

            case SNMP_MSG_GETNEXT:
                _fix_endofmibview(asp);
                break;

            case SNMP_MSG_GETBULK:
                /*
                 * for a GETBULK response we need to rearrange the varbinds 
                 */
                _reorder_getbulk(asp);
                break;
        }

        /*
         * May need to "dumb down" a SET error status for a
         * v1 query.  See RFC2576 - section 4.3
         */
#ifndef NETSNMP_DISABLE_SNMPV1
#ifndef NETSNMP_NO_WRITE_SUPPORT
        if ((asp->pdu->command == SNMP_MSG_SET) &&
            (asp->pdu->version == SNMP_VERSION_1)) {
            switch (asp->status) {
                case SNMP_ERR_WRONGVALUE:
                case SNMP_ERR_WRONGENCODING:
                case SNMP_ERR_WRONGTYPE:
                case SNMP_ERR_WRONGLENGTH:
                case SNMP_ERR_INCONSISTENTVALUE:
                    status = SNMP_ERR_BADVALUE;
                    asp->status = SNMP_ERR_BADVALUE;
                    break;
                case SNMP_ERR_NOACCESS:
                case SNMP_ERR_NOTWRITABLE:
                case SNMP_ERR_NOCREATION:
                case SNMP_ERR_INCONSISTENTNAME:
                case SNMP_ERR_AUTHORIZATIONERROR:
                    status = SNMP_ERR_NOSUCHNAME;
                    asp->status = SNMP_ERR_NOSUCHNAME;
                    break;
                case SNMP_ERR_RESOURCEUNAVAILABLE:
                case SNMP_ERR_COMMITFAILED:
                case SNMP_ERR_UNDOFAILED:
                    status = SNMP_ERR_GENERR;
                    asp->status = SNMP_ERR_GENERR;
                    break;
            }
        }
        /*
         * Similarly we may need to "dumb down" v2 exception
         *  types to throw an error for a v1 query.
         *  See RFC2576 - section 4.1.2.3
         */
        if ((asp->pdu->command != SNMP_MSG_SET) &&
            (asp->pdu->version == SNMP_VERSION_1)) {
            netsnmp_variable_list *var_ptr = asp->pdu->variables;
            int                    i = 1;

            while (var_ptr != NULL) {
                switch (var_ptr->type) {
                    case SNMP_NOSUCHOBJECT:
                    case SNMP_NOSUCHINSTANCE:
                    case SNMP_ENDOFMIBVIEW:
                    case ASN_COUNTER64:
                        status = SNMP_ERR_NOSUCHNAME;
                        asp->status = SNMP_ERR_NOSUCHNAME;
                        asp->index = i;
                        break;
                }
                var_ptr = var_ptr->next_variable;
                ++i;
            }
        }
#endif /* NETSNMP_NO_WRITE_SUPPORT */
#endif /* snmpv1 support */
    } /** if asp->pdu */

    /*
     * Update the snmp error-count statistics
     *   XXX - should we include the V2 errors in this or not?
     */
#define INCLUDE_V2ERRORS_IN_V1STATS

    switch (status) {
#ifdef INCLUDE_V2ERRORS_IN_V1STATS
    case SNMP_ERR_WRONGVALUE:
    case SNMP_ERR_WRONGENCODING:
    case SNMP_ERR_WRONGTYPE:
    case SNMP_ERR_WRONGLENGTH:
    case SNMP_ERR_INCONSISTENTVALUE:
#endif
    case SNMP_ERR_BADVALUE:
        snmp_increment_statistic(STAT_SNMPOUTBADVALUES);
        break;
#ifdef INCLUDE_V2ERRORS_IN_V1STATS
    case SNMP_ERR_NOACCESS:
    case SNMP_ERR_NOTWRITABLE:
    case SNMP_ERR_NOCREATION:
    case SNMP_ERR_INCONSISTENTNAME:
    case SNMP_ERR_AUTHORIZATIONERROR:
#endif
    case SNMP_ERR_NOSUCHNAME:
        snmp_increment_statistic(STAT_SNMPOUTNOSUCHNAMES);
        break;
#ifdef INCLUDE_V2ERRORS_IN_V1STATS
    case SNMP_ERR_RESOURCEUNAVAILABLE:
    case SNMP_ERR_COMMITFAILED:
    case SNMP_ERR_UNDOFAILED:
#endif
    case SNMP_ERR_GENERR:
        snmp_increment_statistic(STAT_SNMPOUTGENERRS);
        break;

    case SNMP_ERR_TOOBIG:
        snmp_increment_statistic(STAT_SNMPOUTTOOBIGS);
        break;
    }

    if ((status == SNMP_ERR_NOERROR) && (asp->pdu)) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
        snmp_increment_statistic_by((asp->pdu->command == SNMP_MSG_SET ?
                                     STAT_SNMPINTOTALSETVARS :
                                     STAT_SNMPINTOTALREQVARS),
                                    count_varbinds(asp->pdu->variables));
#else /* NETSNMP_NO_WRITE_SUPPORT */
        snmp_increment_statistic_by(STAT_SNMPINTOTALREQVARS,
                                    count_varbinds(asp->pdu->variables));
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    } else {
        /*
         * Use a copy of the original request
         *   to report failures.
         */
        snmp_free_pdu(asp->pdu);
        asp->pdu = asp->orig_pdu;
        asp->orig_pdu = NULL;
    }
    if (asp->pdu) {
        asp->pdu->command = SNMP_MSG_RESPONSE;
        asp->pdu->errstat = asp->status;
        asp->pdu->errindex = asp->index;
        if (!snmp_send(asp->session, asp->pdu) &&
             asp->session->s_snmp_errno != SNMPERR_SUCCESS) {
            netsnmp_variable_list *var_ptr;
            snmp_perror("send response");
            for (var_ptr = asp->pdu->variables; var_ptr != NULL;
                     var_ptr = var_ptr->next_variable) {
                size_t  c_oidlen = 256, c_outlen = 0;
                u_char *c_oid = (u_char *) malloc(c_oidlen);

                if (c_oid) {
                    if (!sprint_realloc_objid (&c_oid, &c_oidlen, &c_outlen, 1,
 		                               var_ptr->name,
                                               var_ptr->name_length)) {
                        snmp_log(LOG_ERR, "    -- %s [TRUNCATED]\n", c_oid);
                    } else {
                        snmp_log(LOG_ERR, "    -- %s\n", c_oid);
                    }
                    SNMP_FREE(c_oid);
                }
            }
            snmp_free_pdu(asp->pdu);
            asp->pdu = NULL;
        }
        snmp_increment_statistic(STAT_SNMPOUTPKTS);
        snmp_increment_statistic(STAT_SNMPOUTGETRESPONSES);
        asp->pdu = NULL; /* yyy-rks: redundant, no? */
        netsnmp_remove_and_free_agent_snmp_session(asp);
    }
    return 1;
}

#ifndef NETSNMP_FEATURE_REMOVE_DUMP_SESS_LIST
void
dump_sess_list(void)
{
    netsnmp_agent_session *a;

    DEBUGMSGTL(("snmp_agent", "DUMP agent_sess_list -> "));
    for (a = agent_session_list; a != NULL; a = a->next) {
        DEBUGMSG(("snmp_agent", "%8p[session %8p] -> ", a, a->session));
    }
    DEBUGMSG(("snmp_agent", "[NIL]\n"));
}
#endif /* NETSNMP_FEATURE_REMOVE_DUMP_SESS_LIST */

void
netsnmp_remove_and_free_agent_snmp_session(netsnmp_agent_session *asp)
{
    netsnmp_agent_session *a, **prevNext = &agent_session_list;

    DEBUGMSGTL(("snmp_agent", "REMOVE session == %8p\n", asp));

    for (a = agent_session_list; a != NULL; a = *prevNext) {
        if (a == asp) {
            *prevNext = a->next;
            a->next = NULL;
            free_agent_snmp_session(a);
            asp = NULL;
            break;
        } else {
            prevNext = &(a->next);
        }
    }

    if (a == NULL && asp != NULL) {
        /*
         * We coulnd't find it on the list, so free it anyway.  
         */
        free_agent_snmp_session(asp);
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_FREE_AGENT_SNMP_SESSION_BY_SESSION
void
netsnmp_free_agent_snmp_session_by_session(netsnmp_session * sess,
                                           void (*free_request)
                                           (netsnmp_request_list *))
{
    netsnmp_agent_session *a, *next, **prevNext = &agent_session_list;

    DEBUGMSGTL(("snmp_agent", "REMOVE session == %8p\n", sess));

    for (a = agent_session_list; a != NULL; a = next) {
        if (a->session == sess) {
            *prevNext = a->next;
            next = a->next;
            free_agent_snmp_session(a);
        } else {
            prevNext = &(a->next);
            next = a->next;
        }
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_FREE_AGENT_SNMP_SESSION_BY_SESSION */

/** handles an incoming SNMP packet into the agent */
int
handle_snmp_packet(int op, netsnmp_session * session, int reqid,
                   netsnmp_pdu *pdu, void *magic)
{
    netsnmp_agent_session *asp;
    int             status, access_ret, rc;

    /*
     * We only support receiving here.  
     */
    if (op != NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        return 1;
    }

    /*
     * RESPONSE messages won't get this far, but TRAP-like messages
     * might.  
     */
    if (pdu->command == SNMP_MSG_TRAP || pdu->command == SNMP_MSG_INFORM ||
        pdu->command == SNMP_MSG_TRAP2) {
        DEBUGMSGTL(("snmp_agent", "received trap-like PDU (%02x)\n",
                    pdu->command));
        pdu->command = SNMP_MSG_TRAP2;
        snmp_increment_statistic(STAT_SNMPUNKNOWNPDUHANDLERS);
        return 1;
    }

    /*
     * send snmpv3 authfail trap.
     */
    if (pdu->version  == SNMP_VERSION_3 && 
        session->s_snmp_errno == SNMPERR_USM_AUTHENTICATIONFAILURE) {
           send_easy_trap(SNMP_TRAP_AUTHFAIL, 0);
           return 1;
    } 
	
    if (magic == NULL) {
        asp = init_agent_snmp_session(session, pdu);
        status = SNMP_ERR_NOERROR;
    } else {
        asp = (netsnmp_agent_session *) magic;
        status = asp->status;
    }

    if ((access_ret = check_access(asp->pdu)) != 0) {
        if (access_ret == VACM_NOSUCHCONTEXT) {
            /*
             * rfc3413 section 3.2, step 5 says that we increment the
             * counter but don't return a response of any kind 
             */

            /*
             * we currently don't support unavailable contexts, as
             * there is no reason to that I currently know of 
             */
            snmp_increment_statistic(STAT_SNMPUNKNOWNCONTEXTS);

            /*
             * drop the request 
             */
            netsnmp_remove_and_free_agent_snmp_session(asp);
            return 0;
        } else {
            /*
             * access control setup is incorrect 
             */
            send_easy_trap(SNMP_TRAP_AUTHFAIL, 0);
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
#if defined(NETSNMP_DISABLE_SNMPV1)
            if (asp->pdu->version != SNMP_VERSION_2c) {
#else
#if defined(NETSNMP_DISABLE_SNMPV2C)
            if (asp->pdu->version != SNMP_VERSION_1) {
#else
            if (asp->pdu->version != SNMP_VERSION_1
                && asp->pdu->version != SNMP_VERSION_2c) {
#endif
#endif
                asp->pdu->errstat = SNMP_ERR_AUTHORIZATIONERROR;
                asp->pdu->command = SNMP_MSG_RESPONSE;
                snmp_increment_statistic(STAT_SNMPOUTPKTS);
                if (!snmp_send(asp->session, asp->pdu))
                    snmp_free_pdu(asp->pdu);
                asp->pdu = NULL;
                netsnmp_remove_and_free_agent_snmp_session(asp);
                return 1;
            } else {
#endif /* support for community based SNMP */
                /*
                 * drop the request 
                 */
                netsnmp_remove_and_free_agent_snmp_session(asp);
                return 0;
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
            }
#endif /* support for community based SNMP */
        }
    }

    rc = netsnmp_handle_request(asp, status);

    /*
     * done 
     */
    DEBUGMSGTL(("snmp_agent", "end of handle_snmp_packet, asp = %8p\n",
                asp));
    return rc;
}

netsnmp_request_info *
netsnmp_add_varbind_to_cache(netsnmp_agent_session *asp, int vbcount,
                             netsnmp_variable_list * varbind_ptr,
                             netsnmp_subtree *tp)
{
    netsnmp_request_info *request = NULL;

    DEBUGMSGTL(("snmp_agent", "add_vb_to_cache(%8p, %d, ", asp, vbcount));
    DEBUGMSGOID(("snmp_agent", varbind_ptr->name,
                 varbind_ptr->name_length));
    DEBUGMSG(("snmp_agent", ", %8p)\n", tp));

    if (tp &&
        (asp->pdu->command == SNMP_MSG_GETNEXT ||
         asp->pdu->command == SNMP_MSG_GETBULK)) {
        int result;
        int prefix_len;

        prefix_len = netsnmp_oid_find_prefix(tp->start_a,
                                             tp->start_len,
                                             tp->end_a, tp->end_len);
        if (prefix_len < 1) {
            result = VACM_NOTINVIEW; /* ack...  bad bad thing happened */
        } else {
            result =
                netsnmp_acm_check_subtree(asp->pdu, tp->start_a, prefix_len);
        }

        while (result == VACM_NOTINVIEW) {
            /* the entire subtree is not in view. Skip it. */
            /** @todo make this be more intelligent about ranges.
                Right now we merely take the highest level
                commonality of a registration range and use that.
                At times we might be able to be smarter about
                checking the range itself as opposed to the node
                above where the range exists, but I doubt this will
                come up all that frequently. */
            tp = tp->next;
            if (tp) {
                prefix_len = netsnmp_oid_find_prefix(tp->start_a,
                                                     tp->start_len,
                                                     tp->end_a,
                                                     tp->end_len);
                if (prefix_len < 1) {
                    /* ack...  bad bad thing happened */
                    result = VACM_NOTINVIEW;
                } else {
                    result =
                        netsnmp_acm_check_subtree(asp->pdu,
                                                  tp->start_a, prefix_len);
                }
            }
            else
                break;
        }
    }
    if (tp == NULL) {
        /*
         * no appropriate registration found 
         */
        /*
         * make up the response ourselves 
         */
        switch (asp->pdu->command) {
        case SNMP_MSG_GETNEXT:
        case SNMP_MSG_GETBULK:
            varbind_ptr->type = SNMP_ENDOFMIBVIEW;
            break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
        case SNMP_MSG_SET:
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        case SNMP_MSG_GET:
            varbind_ptr->type = SNMP_NOSUCHOBJECT;
            break;

        default:
            return NULL;        /* shouldn't get here */
        }
    } else {
        int cacheid;

        DEBUGMSGTL(("snmp_agent", "tp->start "));
        DEBUGMSGOID(("snmp_agent", tp->start_a, tp->start_len));
        DEBUGMSG(("snmp_agent", ", tp->end "));
        DEBUGMSGOID(("snmp_agent", tp->end_a, tp->end_len));
        DEBUGMSG(("snmp_agent", ", \n"));

        /*
         * malloc the request structure 
         */
        request = &(asp->requests[vbcount - 1]);
        request->index = vbcount;
        request->delegated = 0;
        request->processed = 0;
        request->status = 0;
        request->subtree = tp;
        request->agent_req_info = asp->reqinfo;
        if (request->parent_data) {
            netsnmp_free_request_data_sets(request);
        }
        DEBUGMSGTL(("verbose:asp", "asp %p reqinfo %p assigned to request\n",
                    asp, asp->reqinfo));

        /*
         * for non-SET modes, set the type to NULL 
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
        if (!MODE_IS_SET(asp->pdu->command)) {
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            DEBUGMSGTL(("verbose:asp", "asp %p reqinfo %p assigned to request\n",
                    asp, asp->reqinfo));
            if (varbind_ptr->type == ASN_PRIV_INCL_RANGE) {
                DEBUGMSGTL(("snmp_agent", "varbind %d is inclusive\n",
                            request->index));
                request->inclusive = 1;
            }
            varbind_ptr->type = ASN_NULL;
#ifndef NETSNMP_NO_WRITE_SUPPORT
        }
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        /*
         * place them in a cache 
         */
        if (tp->global_cacheid) {
            /*
             * we need to merge all marked subtrees together 
             */
            if (asp->cache_store && -1 !=
                (cacheid = netsnmp_get_local_cachid(asp->cache_store,
                                                    tp->global_cacheid))) {
            } else {
                cacheid = ++(asp->treecache_num);
                netsnmp_get_or_add_local_cachid(&asp->cache_store,
                                                tp->global_cacheid,
                                                cacheid);
                goto mallocslot;        /* XXX: ick */
            }
        } else if (tp->cacheid > -1 && tp->cacheid <= asp->treecache_num &&
                   asp->treecache[tp->cacheid].subtree == tp) {
            /*
             * we have already added a request to this tree
             * pointer before 
             */
            cacheid = tp->cacheid;
        } else {
            cacheid = ++(asp->treecache_num);
          mallocslot:
            /*
             * new slot needed 
             */
            if (asp->treecache_num >= asp->treecache_len) {
                /*
                 * exapand cache array 
                 */
                /*
                 * WWW: non-linear expansion needed (with cap) 
                 */
#define CACHE_GROW_SIZE 16
                asp->treecache_len =
                    (asp->treecache_len + CACHE_GROW_SIZE);
                asp->treecache =
                    (netsnmp_tree_cache *)realloc(asp->treecache,
                            sizeof(netsnmp_tree_cache) *
                            asp->treecache_len);
                if (asp->treecache == NULL)
                    return NULL;
                memset(&(asp->treecache[cacheid]), 0x00,
                       sizeof(netsnmp_tree_cache) * (CACHE_GROW_SIZE));
            }
            asp->treecache[cacheid].subtree = tp;
            asp->treecache[cacheid].requests_begin = request;
            tp->cacheid = cacheid;
        }

        /*
         * if this is a search type, get the ending range oid as well 
         */
        if (asp->pdu->command == SNMP_MSG_GETNEXT ||
            asp->pdu->command == SNMP_MSG_GETBULK) {
            request->range_end = tp->end_a;
            request->range_end_len = tp->end_len;
        } else {
            request->range_end = NULL;
            request->range_end_len = 0;
        }

        /*
         * link into chain 
         */
        if (asp->treecache[cacheid].requests_end)
            asp->treecache[cacheid].requests_end->next = request;
        request->next = NULL;
        request->prev = asp->treecache[cacheid].requests_end;
        asp->treecache[cacheid].requests_end = request;

        /*
         * add the given request to the list of requests they need
         * to handle results for 
         */
        request->requestvb = request->requestvb_start = varbind_ptr;
    }
    return request;
}

/*
 * check the ACM(s) for the results on each of the varbinds.
 * If ACM disallows it, replace the value with type
 * 
 * Returns number of varbinds with ACM errors
 */
int
check_acm(netsnmp_agent_session *asp, u_char type)
{
    int             view;
    int             i, j, k;
    netsnmp_request_info *request;
    int             ret = 0;
    netsnmp_variable_list *vb, *vb2, *vbc;
    int             earliest = 0;

    for (i = 0; i <= asp->treecache_num; i++) {
        for (request = asp->treecache[i].requests_begin;
             request; request = request->next) {
            /*
             * for each request, run it through in_a_view() 
             */
            earliest = 0;
            for(j = request->repeat, vb = request->requestvb_start;
                vb && j > -1;
                j--, vb = vb->next_variable) {
                if (vb->type != ASN_NULL &&
                    vb->type != ASN_PRIV_RETRY) { /* not yet processed */
                    view =
                        in_a_view(vb->name, &vb->name_length,
                                  asp->pdu, vb->type);

                    /*
                     * if a ACM error occurs, mark it as type passed in 
                     */
                    if (view != VACM_SUCCESS) {
                        ret++;
                        if (request->repeat < request->orig_repeat) {
                            /* basically this means a GETBULK */
                            request->repeat++;
                            if (!earliest) {
                                request->requestvb = vb;
                                earliest = 1;
                            }

                            /* ugh.  if a whole now exists, we need to
                               move the contents up the chain and fill
                               in at the end else we won't end up
                               lexographically sorted properly */
                            if (j > -1 && vb->next_variable &&
                                vb->next_variable->type != ASN_NULL &&
                                vb->next_variable->type != ASN_PRIV_RETRY) {
                                for(k = j, vbc = vb, vb2 = vb->next_variable;
                                    k > -2 && vbc && vb2;
                                    k--, vbc = vb2, vb2 = vb2->next_variable) {
                                    /* clone next into the current */
                                    snmp_clone_var(vb2, vbc);
                                    vbc->next_variable = vb2;
                                }
                            }
                        }
                        snmp_set_var_typed_value(vb, type, NULL, 0);
                    }
                }
            }
        }
    }
    return ret;
}


int
netsnmp_create_subtree_cache(netsnmp_agent_session *asp)
{
    netsnmp_subtree *tp;
    netsnmp_variable_list *varbind_ptr, *vbsave, *vbptr, **prevNext;
    int             view;
    int             vbcount = 0;
    int             bulkcount = 0, bulkrep = 0;
    int             i = 0, n = 0, r = 0;
    netsnmp_request_info *request;

    if (asp->treecache == NULL && asp->treecache_len == 0) {
        asp->treecache_len = SNMP_MAX(1 + asp->vbcount / 4, 16);
        asp->treecache =
            (netsnmp_tree_cache *)calloc(asp->treecache_len, sizeof(netsnmp_tree_cache));
        if (asp->treecache == NULL)
            return SNMP_ERR_GENERR;
    }
    asp->treecache_num = -1;

    if (asp->pdu->command == SNMP_MSG_GETBULK) {
        /*
         * getbulk prep 
         */
        int             count = count_varbinds(asp->pdu->variables);
        if (asp->pdu->errstat < 0) {
            asp->pdu->errstat = 0;
        }
        if (asp->pdu->errindex < 0) {
            asp->pdu->errindex = 0;
        }

        if (asp->pdu->errstat < count) {
            n = asp->pdu->errstat;
        } else {
            n = count;
        }
        if ((r = count - n) <= 0) {
            r = 0;
            asp->bulkcache = NULL;
        } else {
            int           maxbulk =
                netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                   NETSNMP_DS_AGENT_MAX_GETBULKREPEATS);
            int maxresponses =
                netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                   NETSNMP_DS_AGENT_MAX_GETBULKRESPONSES);

            if (maxresponses == 0)
                maxresponses = 100;   /* more than reasonable default */

            /* ensure that the total number of responses fits in a mallocable
             * result vector
             */
            if (maxresponses < 0 ||
                maxresponses > (int)(INT_MAX / sizeof(struct varbind_list *)))
                maxresponses = (int)(INT_MAX / sizeof(struct varbind_list *));

            /* ensure that the maximum number of repetitions will fit in the
             * result vector
             */
            if (maxbulk <= 0 || maxbulk > maxresponses / r)
                maxbulk = maxresponses / r;

            /* limit getbulk number of repeats to a configured size */
            if (asp->pdu->errindex > maxbulk) {
                asp->pdu->errindex = maxbulk;
                DEBUGMSGTL(("snmp_agent",
                            "truncating number of getbulk repeats to %ld\n",
                            asp->pdu->errindex));
            }

            asp->bulkcache =
                (netsnmp_variable_list **) malloc(
                    (n + asp->pdu->errindex * r) * sizeof(struct varbind_list *));

            if (!asp->bulkcache) {
                DEBUGMSGTL(("snmp_agent", "Bulkcache malloc failed\n"));
                return SNMP_ERR_GENERR;
            }
        }
        DEBUGMSGTL(("snmp_agent", "GETBULK N = %d, M = %ld, R = %d\n",
                    n, asp->pdu->errindex, r));
    }

    /*
     * collect varbinds into their registered trees 
     */
    prevNext = &(asp->pdu->variables);
    for (varbind_ptr = asp->pdu->variables; varbind_ptr;
         varbind_ptr = vbsave) {

        /*
         * getbulk mess with this pointer, so save it 
         */
        vbsave = varbind_ptr->next_variable;

        if (asp->pdu->command == SNMP_MSG_GETBULK) {
            if (n > 0) {
                n--;
            } else {
                /*
                 * repeate request varbinds on GETBULK.  These will
                 * have to be properly rearranged later though as
                 * responses are supposed to actually be interlaced
                 * with each other.  This is done with the asp->bulkcache. 
                 */
                bulkrep = asp->pdu->errindex - 1;
                if (asp->pdu->errindex > 0) {
                    vbptr = varbind_ptr;
                    asp->bulkcache[bulkcount++] = vbptr;

                    for (i = 1; i < asp->pdu->errindex; i++) {
                        vbptr->next_variable =
                            SNMP_MALLOC_STRUCT(variable_list);
                        /*
                         * don't clone the oid as it's got to be
                         * overwritten anyway 
                         */
                        if (!vbptr->next_variable) {
                            /*
                             * XXXWWW: ack!!! 
                             */
                            DEBUGMSGTL(("snmp_agent", "NextVar malloc failed\n"));
                        } else {
                            vbptr = vbptr->next_variable;
                            vbptr->name_length = 0;
                            vbptr->type = ASN_NULL;
                            asp->bulkcache[bulkcount++] = vbptr;
                        }
                    }
                    vbptr->next_variable = vbsave;
                } else {
                    /*
                     * 0 repeats requested for this varbind, so take it off
                     * the list.  
                     */
                    vbptr = varbind_ptr;
                    *prevNext = vbptr->next_variable;
                    vbptr->next_variable = NULL;
                    snmp_free_varbind(vbptr);
                    asp->vbcount--;
                    continue;
                }
            }
        }

        /*
         * count the varbinds 
         */
        ++vbcount;

        /*
         * find the owning tree 
         */
        tp = netsnmp_subtree_find(varbind_ptr->name, varbind_ptr->name_length,
				  NULL, asp->pdu->contextName);

        /*
         * check access control 
         */
        switch (asp->pdu->command) {
        case SNMP_MSG_GET:
            view = in_a_view(varbind_ptr->name, &varbind_ptr->name_length,
                             asp->pdu, varbind_ptr->type);
            if (view != VACM_SUCCESS)
                snmp_set_var_typed_value(varbind_ptr, SNMP_NOSUCHOBJECT,
                                         NULL, 0);
            break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
        case SNMP_MSG_SET:
            view = in_a_view(varbind_ptr->name, &varbind_ptr->name_length,
                             asp->pdu, varbind_ptr->type);
            if (view != VACM_SUCCESS) {
                asp->index = vbcount;
                return SNMP_ERR_NOACCESS;
            }
            break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        case SNMP_MSG_GETNEXT:
        case SNMP_MSG_GETBULK:
        default:
            view = VACM_SUCCESS;
            /*
             * XXXWWW: check VACM here to see if "tp" is even worthwhile 
             */
        }
        if (view == VACM_SUCCESS) {
            request = netsnmp_add_varbind_to_cache(asp, vbcount, varbind_ptr,
						   tp);
            if (request && asp->pdu->command == SNMP_MSG_GETBULK) {
                request->repeat = request->orig_repeat = bulkrep;
            }
        }

        prevNext = &(varbind_ptr->next_variable);
    }

    return SNMPERR_SUCCESS;
}

/*
 * this function is only applicable in getnext like contexts 
 */
int
netsnmp_reassign_requests(netsnmp_agent_session *asp)
{
    /*
     * assume all the requests have been filled or rejected by the
     * subtrees, so reassign the rejected ones to the next subtree in
     * the chain 
     */

    int             i;

    /*
     * get old info 
     */
    netsnmp_tree_cache *old_treecache = asp->treecache;

    /*
     * malloc new space 
     */
    asp->treecache =
        (netsnmp_tree_cache *) calloc(asp->treecache_len,
                                      sizeof(netsnmp_tree_cache));

    if (asp->treecache == NULL)
        return SNMP_ERR_GENERR;

    asp->treecache_num = -1;
    if (asp->cache_store) {
        netsnmp_free_cachemap(asp->cache_store);
        asp->cache_store = NULL;
    }

    for (i = 0; i < asp->vbcount; i++) {
        if (asp->requests[i].requestvb == NULL) {
            /*
             * Occurs when there's a mixture of still active
             *   and "endOfMibView" repetitions
             */
            continue;
        }
        if (asp->requests[i].requestvb->type == ASN_NULL) {
            if (!netsnmp_add_varbind_to_cache(asp, asp->requests[i].index,
                                              asp->requests[i].requestvb,
                                              asp->requests[i].subtree->next)) {
                SNMP_FREE(old_treecache);
            }
        } else if (asp->requests[i].requestvb->type == ASN_PRIV_RETRY) {
            /*
             * re-add the same subtree 
             */
            asp->requests[i].requestvb->type = ASN_NULL;
            if (!netsnmp_add_varbind_to_cache(asp, asp->requests[i].index,
                                              asp->requests[i].requestvb,
                                              asp->requests[i].subtree)) {
                SNMP_FREE(old_treecache);
            }
        }
    }

    SNMP_FREE(old_treecache);
    return SNMP_ERR_NOERROR;
}

void
netsnmp_delete_request_infos(netsnmp_request_info *reqlist)
{
    while (reqlist) {
        netsnmp_free_request_data_sets(reqlist);
        reqlist = reqlist->next;
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_DELETE_SUBTREE_CACHE
void
netsnmp_delete_subtree_cache(netsnmp_agent_session *asp)
{
    while (asp->treecache_num >= 0) {
        /*
         * don't delete subtrees 
         */
        netsnmp_delete_request_infos(asp->treecache[asp->treecache_num].
                                     requests_begin);
        asp->treecache_num--;
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_DELETE_SUBTREE_CACHE */

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_ALL_REQUESTS_ERROR
/*
 * check all requests for errors
 *
 * @Note:
 * This function is a little different from the others in that
 * it does not use any linked lists, instead using the original
 * asp requests array. This is of particular importance for
 * cases where the linked lists are unreliable. One known instance
 * of this scenario occurs when the row_merge helper is used, which
 * may temporarily disrupts linked lists during its (and its childrens)
 * handling of requests.
 */
int
netsnmp_check_all_requests_error(netsnmp_agent_session *asp,
                                 int look_for_specific)
{
    int i;

    /*
     * find any errors marked in the requests 
     */
    for( i = 0; i < asp->vbcount; ++i ) {
        if ((SNMP_ERR_NOERROR != asp->requests[i].status) &&
            (!look_for_specific ||
             asp->requests[i].status == look_for_specific))
            return asp->requests[i].status;
    }

    return SNMP_ERR_NOERROR;
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_ALL_REQUESTS_ERROR */

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_REQUESTS_ERROR
int
netsnmp_check_requests_error(netsnmp_request_info *requests)
{
    /*
     * find any errors marked in the requests 
     */
    for (;requests;requests = requests->next) {
        if (requests->status != SNMP_ERR_NOERROR)
            return requests->status;
    }
    return SNMP_ERR_NOERROR;
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_REQUESTS_ERROR */

int
netsnmp_check_requests_status(netsnmp_agent_session *asp,
                              netsnmp_request_info *requests,
                              int look_for_specific)
{
    /*
     * find any errors marked in the requests 
     */
    while (requests) {
        if(requests->agent_req_info != asp->reqinfo) {
            DEBUGMSGTL(("verbose:asp",
                        "**reqinfo %p doesn't match cached reqinfo %p\n",
                        asp->reqinfo, requests->agent_req_info));
        }
        if (requests->status != SNMP_ERR_NOERROR &&
            (!look_for_specific || requests->status == look_for_specific)
            && (look_for_specific || asp->index == 0
                || requests->index < asp->index)) {
            asp->index = requests->index;
            asp->status = requests->status;
        }
        requests = requests->next;
    }
    return asp->status;
}

int
netsnmp_check_all_requests_status(netsnmp_agent_session *asp,
                                  int look_for_specific)
{
    int             i;
    for (i = 0; i <= asp->treecache_num; i++) {
        netsnmp_check_requests_status(asp,
                                      asp->treecache[i].requests_begin,
                                      look_for_specific);
    }
    return asp->status;
}

int
handle_var_requests(netsnmp_agent_session *asp)
{
    int             i, retstatus = SNMP_ERR_NOERROR,
        status = SNMP_ERR_NOERROR, final_status = SNMP_ERR_NOERROR;
    netsnmp_handler_registration *reginfo;

    asp->reqinfo->asp = asp;
    asp->reqinfo->mode = asp->mode;

    /*
     * now, have the subtrees in the cache go search for their results 
     */
    for (i = 0; i <= asp->treecache_num; i++) {
        /*
         * don't call handlers w/null reginfo.
         * - when is this? sub agent disconnected while request processing?
         * - should this case encompass more of this subroutine?
         *   - does check_request_status make send if handlers weren't called?
         */
        if(NULL != asp->treecache[i].subtree->reginfo) {
            reginfo = asp->treecache[i].subtree->reginfo;
            status = netsnmp_call_handlers(reginfo, asp->reqinfo,
                                           asp->treecache[i].requests_begin);
        }
        else
            status = SNMP_ERR_GENERR;

        /*
         * find any errors marked in the requests.  For later parts of
         * SET processing, only check for new errors specific to that
         * set processing directive (which must superceed the previous
         * errors).
         */
        switch (asp->mode) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
        case MODE_SET_COMMIT:
            retstatus = netsnmp_check_requests_status(asp,
						      asp->treecache[i].
						      requests_begin,
						      SNMP_ERR_COMMITFAILED);
            break;

        case MODE_SET_UNDO:
            retstatus = netsnmp_check_requests_status(asp,
						      asp->treecache[i].
						      requests_begin,
						      SNMP_ERR_UNDOFAILED);
            break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        default:
            retstatus = netsnmp_check_requests_status(asp,
						      asp->treecache[i].
						      requests_begin, 0);
            break;
        }

        /*
         * always take lowest varbind if possible 
         */
        if (retstatus != SNMP_ERR_NOERROR) {
            status = retstatus;
	}

        /*
         * other things we know less about (no index) 
         */
        /*
         * WWW: drop support for this? 
         */
        if (final_status == SNMP_ERR_NOERROR && status != SNMP_ERR_NOERROR) {
            /*
             * we can't break here, since some processing needs to be
             * done for all requests anyway (IE, SET handling for UNDO
             * needs to be called regardless of previous status
             * results.
             * WWW:  This should be predictable though and
             * breaking should be possible in some cases (eg GET,
             * GETNEXT, ...) 
             */
            final_status = status;
        }
    }

    return final_status;
}

/*
 * loop through our sessions known delegated sessions and check to see
 * if they've completed yet. If there are no more delegated sessions,
 * check for and process any queued requests
 */
void
netsnmp_check_outstanding_agent_requests(void)
{
    netsnmp_agent_session *asp, *prev_asp = NULL, *next_asp = NULL;

    /*
     * deal with delegated requests
     */
    for (asp = agent_delegated_list; asp; asp = next_asp) {
        next_asp = asp->next;   /* save in case we clean up asp */
        if (!netsnmp_check_for_delegated(asp)) {

            /*
             * we're done with this one, remove from queue 
             */
            if (prev_asp != NULL)
                prev_asp->next = asp->next;
            else
                agent_delegated_list = asp->next;
            asp->next = NULL;

            /*
             * check request status
             */
            netsnmp_check_all_requests_status(asp, 0);
            
            /*
             * continue processing or finish up 
             */
            check_delayed_request(asp);

            /*
             * if head was removed, don't drop it if it
             * was it re-queued
             */
            if ((prev_asp == NULL) && (agent_delegated_list == asp)) {
                prev_asp = asp;
            }
        } else {

            /*
             * asp is still on the queue
             */
            prev_asp = asp;
        }
    }

    /*
     * if we are processing a set and there are more delegated
     * requests, keep waiting before getting to queued requests.
     */
    if (netsnmp_processing_set && (NULL != agent_delegated_list))
        return;

    while (netsnmp_agent_queued_list) {

        /*
         * if we are processing a set, the first item better be
         * the set being (or waiting to be) processed.
         */
        netsnmp_assert((!netsnmp_processing_set) ||
                       (netsnmp_processing_set == netsnmp_agent_queued_list));

        /*
         * if the top request is a set, don't pop it
         * off if there are delegated requests
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
        if ((netsnmp_agent_queued_list->pdu->command == SNMP_MSG_SET) &&
            (agent_delegated_list)) {

            netsnmp_assert(netsnmp_processing_set == NULL);

            netsnmp_processing_set = netsnmp_agent_queued_list;
            DEBUGMSGTL(("snmp_agent", "SET request remains queued while "
                        "delegated requests finish, asp = %8p\n", asp));
            break;
        }
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        /*
         * pop the first request and process it
         */
        asp = netsnmp_agent_queued_list;
        netsnmp_agent_queued_list = asp->next;
        DEBUGMSGTL(("snmp_agent",
                    "processing queued request, asp = %8p\n", asp));

        netsnmp_handle_request(asp, asp->status);

        /*
         * if we hit a set, stop
         */
        if (NULL != netsnmp_processing_set)
            break;
    }
}

/** Decide if the requested transaction_id is still being processed
   within the agent.  This is used to validate whether a delayed cache
   (containing possibly freed pointers) is still usable.

   returns SNMPERR_SUCCESS if it's still valid, or SNMPERR_GENERR if not. */
int
netsnmp_check_transaction_id(int transaction_id)
{
    netsnmp_agent_session *asp;

    for (asp = agent_delegated_list; asp; asp = asp->next) {
        if (asp->pdu->transid == transaction_id)
            return SNMPERR_SUCCESS;
    }
    return SNMPERR_GENERR;
}


/*
 * check_delayed_request(asp)
 *
 * Called to rexamine a set of requests and continue processing them
 * once all the previous (delayed) requests have been handled one way
 * or another.
 */

int
check_delayed_request(netsnmp_agent_session *asp)
{
    int             status = SNMP_ERR_NOERROR;

    DEBUGMSGTL(("snmp_agent", "processing delegated request, asp = %8p\n",
                asp));

    switch (asp->mode) {
    case SNMP_MSG_GETBULK:
    case SNMP_MSG_GETNEXT:
        netsnmp_check_all_requests_status(asp, 0);
        handle_getnext_loop(asp);
        if (netsnmp_check_for_delegated(asp) &&
            netsnmp_check_transaction_id(asp->pdu->transid) !=
            SNMPERR_SUCCESS) {
            /*
             * add to delegated request chain 
             */
            if (!netsnmp_check_delegated_chain_for(asp)) {
                asp->next = agent_delegated_list;
                agent_delegated_list = asp;
            }
        }
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_COMMIT:
        netsnmp_check_all_requests_status(asp, SNMP_ERR_COMMITFAILED);
        goto settop;

    case MODE_SET_UNDO:
        netsnmp_check_all_requests_status(asp, SNMP_ERR_UNDOFAILED);
        goto settop;

    case MODE_SET_BEGIN:
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_FREE:
      settop:
        /* If we should do only one pass, this mean we */
        /* should not reenter this function */
        if ((asp->pdu->flags & UCD_MSG_FLAG_ONE_PASS_ONLY)) {
            /* We should have finished the processing after the first */
            /* handle_set_loop, so just wrap up */
            break;
        }
        handle_set_loop(asp);
        if (asp->mode != FINISHED_SUCCESS && asp->mode != FINISHED_FAILURE) {

            if (netsnmp_check_for_delegated_and_add(asp)) {
                /*
                 * add to delegated request chain 
                 */
                if (!asp->status)
                    asp->status = status;
            }

            return SNMP_ERR_NOERROR;
        }
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    default:
        break;
    }

    /*
     * if we don't have anything outstanding (delegated), wrap up 
     */
    if (!netsnmp_check_for_delegated(asp))
        return netsnmp_wrap_up_request(asp, status);

    return 1;
}

/** returns 1 if there are valid GETNEXT requests left.  Returns 0 if not. */
int
check_getnext_results(netsnmp_agent_session *asp)
{
    /*
     * get old info 
     */
    netsnmp_tree_cache *old_treecache = asp->treecache;
    int             old_treecache_num = asp->treecache_num;
    int             count = 0;
    int             i, special = 0;
    netsnmp_request_info *request;

    if (asp->mode == SNMP_MSG_GET) {
        /*
         * Special case for doing INCLUSIVE getNext operations in
         * AgentX subagents.  
         */
        DEBUGMSGTL(("snmp_agent",
                    "asp->mode == SNMP_MSG_GET in ch_getnext\n"));
        asp->mode = asp->oldmode;
        special = 1;
    }

    for (i = 0; i <= old_treecache_num; i++) {
        for (request = old_treecache[i].requests_begin; request;
             request = request->next) {

            /*
             * If we have just done the special case AgentX GET, then any
             * requests which were not INCLUSIVE will now have a wrong
             * response, so junk them and retry from the same place (except
             * that this time the handler will be called in "inexact"
             * mode).  
             */

            if (special) {
                if (!request->inclusive) {
                    DEBUGMSGTL(("snmp_agent",
                                "request %d wasn't inclusive\n",
                                request->index));
                    snmp_set_var_typed_value(request->requestvb,
                                             ASN_PRIV_RETRY, NULL, 0);
                } else if (request->requestvb->type == ASN_NULL ||
                           request->requestvb->type == SNMP_NOSUCHINSTANCE ||
                           request->requestvb->type == SNMP_NOSUCHOBJECT) {
                    /*
                     * it was inclusive, but no results.  Still retry this
                     * search. 
                     */
                    snmp_set_var_typed_value(request->requestvb,
                                             ASN_PRIV_RETRY, NULL, 0);
                }
            }

            /*
             * out of range? 
             */
            if (snmp_oid_compare(request->requestvb->name,
                                 request->requestvb->name_length,
                                 request->range_end,
                                 request->range_end_len) >= 0) {
                /*
                 * ack, it's beyond the accepted end of range. 
                 */
                /*
                 * fix it by setting the oid to the end of range oid instead 
                 */
                DEBUGMSGTL(("check_getnext_results",
                            "request response %d out of range\n",
                            request->index));
                /*
                 * I'm not sure why inclusive is set unconditionally here (see
                 * comments for revision 1.161), but it causes a problem for
                 * GETBULK over an overridden variable. The bulk-to-next
                 * handler re-uses the same request for multiple varbinds,
                 * and once inclusive was set, it was never cleared. So, a
                 * hack. Instead of setting it to 1, set it to 2, so bulk-to
                 * next can clear it later. As of the time of this hack, all
                 * checks of this var are boolean checks (not == 1), so this
                 * should be safe. Cross your fingers.
                 */
                request->inclusive = 2;
                /*
                 * XXX: should set this to the original OID? 
                 */
                snmp_set_var_objid(request->requestvb,
                                   request->range_end,
                                   request->range_end_len);
                snmp_set_var_typed_value(request->requestvb, ASN_NULL,
                                         NULL, 0);
            }

            /*
             * mark any existent requests with illegal results as NULL 
             */
            if (request->requestvb->type == SNMP_ENDOFMIBVIEW) {
                /*
                 * illegal response from a subagent.  Change it back to NULL 
                 *  xxx-rks: err, how do we know this is a subagent?
                 */
                request->requestvb->type = ASN_NULL;
                request->inclusive = 1;
            }

            if (request->requestvb->type == ASN_NULL ||
                request->requestvb->type == ASN_PRIV_RETRY ||
                (asp->reqinfo->mode == MODE_GETBULK
                 && request->repeat > 0))
                count++;
        }
    }
    return count;
}

/** repeatedly calls getnext handlers looking for an answer till all
   requests are satisified.  It's expected that one pass has been made
   before entering this function */
int
handle_getnext_loop(netsnmp_agent_session *asp)
{
    int             status;
    netsnmp_variable_list *var_ptr;

    /*
     * loop 
     */
    while (netsnmp_running) {

        /*
         * bail for now if anything is delegated. 
         */
        if (netsnmp_check_for_delegated(asp)) {
            return SNMP_ERR_NOERROR;
        }

        /*
         * check vacm against results 
         */
        check_acm(asp, ASN_PRIV_RETRY);

        /*
         * need to keep going we're not done yet. 
         */
        if (!check_getnext_results(asp))
            /*
             * nothing left, quit now 
             */
            break;

        /*
         * never had a request (empty pdu), quit now 
         */
        /*
         * XXXWWW: huh?  this would be too late, no?  shouldn't we
         * catch this earlier? 
         */
        /*
         * if (count == 0)
         * break; 
         */

        DEBUGIF("results") {
            DEBUGMSGTL(("results",
                        "getnext results, before next pass:\n"));
            for (var_ptr = asp->pdu->variables; var_ptr;
                 var_ptr = var_ptr->next_variable) {
                DEBUGMSGTL(("results", "\t"));
                DEBUGMSGVAR(("results", var_ptr));
                DEBUGMSG(("results", "\n"));
            }
        }

        netsnmp_reassign_requests(asp);
        status = handle_var_requests(asp);
        if (status != SNMP_ERR_NOERROR) {
            return status;      /* should never really happen */
        }
    }
    return SNMP_ERR_NOERROR;
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
int
handle_set(netsnmp_agent_session *asp)
{
    int             status;
    /*
     * SETS require 3-4 passes through the var_op_list.
     * The first two
     * passes verify that all types, lengths, and values are valid
     * and may reserve resources and the third does the set and a
     * fourth executes any actions.  Then the identical GET RESPONSE
     * packet is returned.
     * If either of the first two passes returns an error, another
     * pass is made so that any reserved resources can be freed.
     * If the third pass returns an error, another pass is
     * made so that
     * any changes can be reversed.
     * If the fourth pass (or any of the error handling passes)
     * return an error, we'd rather not know about it!
     */
    if (!(asp->pdu->flags & UCD_MSG_FLAG_ONE_PASS_ONLY)) {
        switch (asp->mode) {
        case MODE_SET_BEGIN:
            snmp_increment_statistic(STAT_SNMPINSETREQUESTS);
            asp->rw = WRITE;    /* WWW: still needed? */
            asp->mode = MODE_SET_RESERVE1;
            asp->status = SNMP_ERR_NOERROR;
            break;

        case MODE_SET_RESERVE1:

            if (asp->status != SNMP_ERR_NOERROR)
                asp->mode = MODE_SET_FREE;
            else
                asp->mode = MODE_SET_RESERVE2;
            break;

        case MODE_SET_RESERVE2:
            if (asp->status != SNMP_ERR_NOERROR)
                asp->mode = MODE_SET_FREE;
            else
                asp->mode = MODE_SET_ACTION;
            break;

        case MODE_SET_ACTION:
            if (asp->status != SNMP_ERR_NOERROR)
                asp->mode = MODE_SET_UNDO;
            else
                asp->mode = MODE_SET_COMMIT;
            break;

        case MODE_SET_COMMIT:
            if (asp->status != SNMP_ERR_NOERROR) {
                asp->mode = FINISHED_FAILURE;
            } else {
                asp->mode = FINISHED_SUCCESS;
            }
            break;

        case MODE_SET_UNDO:
            asp->mode = FINISHED_FAILURE;
            break;

        case MODE_SET_FREE:
            asp->mode = FINISHED_FAILURE;
            break;
        }
    }

    if (asp->mode != FINISHED_SUCCESS && asp->mode != FINISHED_FAILURE) {
        DEBUGMSGTL(("agent_set", "doing set mode = %d (%s)\n", asp->mode,
                    se_find_label_in_slist("agent_mode", asp->mode)));
        status = handle_var_requests(asp);
        DEBUGMSGTL(("agent_set", "did set mode = %d, status = %d\n",
                    asp->mode, status));
        if ((status != SNMP_ERR_NOERROR && asp->status == SNMP_ERR_NOERROR) ||
	    status == SNMP_ERR_COMMITFAILED || 
	    status == SNMP_ERR_UNDOFAILED) {
            asp->status = status;
        }
    }
    return asp->status;
}

int
handle_set_loop(netsnmp_agent_session *asp)
{
    while (asp->mode != FINISHED_FAILURE && asp->mode != FINISHED_SUCCESS) {
        handle_set(asp);
        if (netsnmp_check_for_delegated(asp)) {
            return SNMP_ERR_NOERROR;
	}
        if (asp->pdu->flags & UCD_MSG_FLAG_ONE_PASS_ONLY) {
            return asp->status;
	}
    }
    return asp->status;
}
#endif /* NETSNMP_NO_WRITE_SUPPORT */

int
netsnmp_handle_request(netsnmp_agent_session *asp, int status)
{
    /*
     * if this isn't a delegated request trying to finish,
     * processing of a set request should not start until all
     * delegated requests have completed, and no other new requests
     * should be processed until the set request completes.
     */
    if ((0 == netsnmp_check_delegated_chain_for(asp)) &&
        (asp != netsnmp_processing_set)) {
        /*
         * if we are processing a set and this is not a delegated
         * request, queue the request
         */
        if (netsnmp_processing_set) {
            netsnmp_add_queued(asp);
            DEBUGMSGTL(("snmp_agent",
                        "request queued while processing set, "
                        "asp = %8p\n", asp));
            return 1;
        }

        /*
         * check for set request
         */
#ifndef NETSNMP_NO_WRITE_SUPPORT
        if (asp->pdu->command == SNMP_MSG_SET) {
            netsnmp_processing_set = asp;

            /*
             * if there are delegated requests, we must wait for them
             * to finish.
             */
            if (agent_delegated_list) {
                DEBUGMSGTL(("snmp_agent", "SET request queued while "
                            "delegated requests finish, asp = %8p\n",
                            asp));
                netsnmp_add_queued(asp);
                return 1;
            }
        }
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }

    /*
     * process the request 
     */
    status = handle_pdu(asp);

    /*
     * print the results in appropriate debugging mode 
     */
    DEBUGIF("results") {
        netsnmp_variable_list *var_ptr;
        DEBUGMSGTL(("results", "request results (status = %d):\n",
                    status));
        for (var_ptr = asp->pdu->variables; var_ptr;
             var_ptr = var_ptr->next_variable) {
            DEBUGMSGTL(("results", "\t"));
            DEBUGMSGVAR(("results", var_ptr));
            DEBUGMSG(("results", "\n"));
        }
    }

    /*
     * check for uncompleted requests 
     */
    if (netsnmp_check_for_delegated_and_add(asp)) {
        /*
         * add to delegated request chain 
         */
        asp->status = status;
    } else {
        /*
         * if we don't have anything outstanding (delegated), wrap up
         */
        return netsnmp_wrap_up_request(asp, status);
    }

    return 1;
}

int
handle_pdu(netsnmp_agent_session *asp)
{
    int             status, inclusives = 0;
    netsnmp_variable_list *v = NULL;

    /*
     * for illegal requests, mark all nodes as ASN_NULL 
     */
    switch (asp->pdu->command) {

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_INTERNAL_SET_RESERVE2:
    case SNMP_MSG_INTERNAL_SET_ACTION:
    case SNMP_MSG_INTERNAL_SET_COMMIT:
    case SNMP_MSG_INTERNAL_SET_FREE:
    case SNMP_MSG_INTERNAL_SET_UNDO:
        status = get_set_cache(asp);
        if (status != SNMP_ERR_NOERROR)
            return status;
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    case SNMP_MSG_GET:
    case SNMP_MSG_GETNEXT:
    case SNMP_MSG_GETBULK:
        for (v = asp->pdu->variables; v != NULL; v = v->next_variable) {
            if (v->type == ASN_PRIV_INCL_RANGE) {
                /*
                 * Leave the type for now (it gets set to
                 * ASN_NULL in netsnmp_add_varbind_to_cache,
                 * called by create_subnetsnmp_tree_cache below).
                 * If we set it to ASN_NULL now, we wouldn't be
                 * able to distinguish INCLUSIVE search
                 * ranges.  
                 */
                inclusives++;
            } else {
                snmp_set_var_typed_value(v, ASN_NULL, NULL, 0);
            }
        }
        /*
         * fall through 
         */

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_INTERNAL_SET_BEGIN:
    case SNMP_MSG_INTERNAL_SET_RESERVE1:
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    default:
        asp->vbcount = count_varbinds(asp->pdu->variables);
        if (asp->vbcount) /* efence doesn't like 0 size allocs */
            asp->requests = (netsnmp_request_info *)
                calloc(asp->vbcount, sizeof(netsnmp_request_info));
        /*
         * collect varbinds 
         */
        status = netsnmp_create_subtree_cache(asp);
        if (status != SNMP_ERR_NOERROR)
            return status;
    }

    asp->mode = asp->pdu->command;
    switch (asp->mode) {
    case SNMP_MSG_GET:
        /*
         * increment the message type counter 
         */
        snmp_increment_statistic(STAT_SNMPINGETREQUESTS);

        /*
         * check vacm ahead of time 
         */
        check_acm(asp, SNMP_NOSUCHOBJECT);

        /*
         * get the results 
         */
        status = handle_var_requests(asp);

        /*
         * Deal with unhandled results -> noSuchInstance (rather
         * than noSuchObject -- in that case, the type will
         * already have been set to noSuchObject when we realised
         * we couldn't find an appropriate tree).  
         */
        if (status == SNMP_ERR_NOERROR)
            snmp_replace_var_types(asp->pdu->variables, ASN_NULL,
                                   SNMP_NOSUCHINSTANCE);
        break;

    case SNMP_MSG_GETNEXT:
        snmp_increment_statistic(STAT_SNMPINGETNEXTS);
        /*
         * fall through 
         */

    case SNMP_MSG_GETBULK:     /* note: there is no getbulk stat */
        /*
         * loop through our mib tree till we find an
         * appropriate response to return to the caller. 
         */

        if (inclusives) {
            /*
             * This is a special case for AgentX INCLUSIVE getNext
             * requests where a result lexi-equal to the request is okay
             * but if such a result does not exist, we still want the
             * lexi-next one.  So basically we do a GET first, and if any
             * of the INCLUSIVE requests are satisfied, we use that
             * value.  Then, unsatisfied INCLUSIVE requests, and
             * non-INCLUSIVE requests get done as normal.  
             */

            DEBUGMSGTL(("snmp_agent", "inclusive range(s) in getNext\n"));
            asp->oldmode = asp->mode;
            asp->mode = SNMP_MSG_GET;
        }

        /*
         * first pass 
         */
        status = handle_var_requests(asp);
        if (status != SNMP_ERR_NOERROR) {
            if (!inclusives)
                return status;  /* should never really happen */
            else
                asp->status = SNMP_ERR_NOERROR;
        }

        /*
         * loop through our mib tree till we find an
         * appropriate response to return to the caller. 
         */

        status = handle_getnext_loop(asp);
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_SET:
#ifdef NETSNMP_DISABLE_SET_SUPPORT
        return SNMP_ERR_NOTWRITABLE;
#else
        /*
         * check access permissions first 
         */
        if (check_acm(asp, SNMP_NOSUCHOBJECT))
            return SNMP_ERR_NOTWRITABLE;

        asp->mode = MODE_SET_BEGIN;
        status = handle_set_loop(asp);
#endif
        break;

    case SNMP_MSG_INTERNAL_SET_BEGIN:
    case SNMP_MSG_INTERNAL_SET_RESERVE1:
    case SNMP_MSG_INTERNAL_SET_RESERVE2:
    case SNMP_MSG_INTERNAL_SET_ACTION:
    case SNMP_MSG_INTERNAL_SET_COMMIT:
    case SNMP_MSG_INTERNAL_SET_FREE:
    case SNMP_MSG_INTERNAL_SET_UNDO:
        asp->pdu->flags |= UCD_MSG_FLAG_ONE_PASS_ONLY;
        status = handle_set_loop(asp);
        /*
         * asp related cache is saved in cleanup 
         */
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    case SNMP_MSG_RESPONSE:
        snmp_increment_statistic(STAT_SNMPINGETRESPONSES);
        return SNMP_ERR_NOERROR;

    case SNMP_MSG_TRAP:
    case SNMP_MSG_TRAP2:
        snmp_increment_statistic(STAT_SNMPINTRAPS);
        return SNMP_ERR_NOERROR;

    default:
        /*
         * WWW: are reports counted somewhere ? 
         */
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        return SNMPERR_GENERR;  /* shouldn't get here */
        /*
         * WWW 
         */
    }
    return status;
}

/** set error for a request
 * \internal external interface: netsnmp_request_set_error
 */
NETSNMP_STATIC_INLINE int
_request_set_error(netsnmp_request_info *request, int mode, int error_value)
{
    if (!request)
        return SNMPERR_NO_VARS;

    request->processed = 1;
    request->delegated = REQUEST_IS_NOT_DELEGATED;

    switch (error_value) {
    case SNMP_NOSUCHOBJECT:
    case SNMP_NOSUCHINSTANCE:
    case SNMP_ENDOFMIBVIEW:
        /*
         * these are exceptions that should be put in the varbind
         * in the case of a GET but should be translated for a SET
         * into a real error status code and put in the request 
         */
        switch (mode) {
        case MODE_GET:
        case MODE_GETNEXT:
        case MODE_GETBULK:
            request->requestvb->type = error_value;
            break;

            /*
             * These are technically illegal to set by the
             * client APIs for these modes.  But accepting
             * them here allows the 'sparse_table' helper to
             * provide some common table handling processing
             *
            snmp_log(LOG_ERR, "Illegal error_value %d for mode %d ignored\n",
                     error_value, mode);
            return SNMPERR_VALUE;
             */

#ifndef NETSNMP_NO_WRITE_SUPPORT
        case SNMP_MSG_INTERNAL_SET_RESERVE1:
            request->status = SNMP_ERR_NOCREATION;
            break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

        default:
            request->status = SNMP_ERR_NOSUCHNAME;      /* WWW: correct? */
            break;
        }
        break;                  /* never get here */

    default:
        if (error_value < 0) {
            /*
             * illegal local error code.  translate to generr 
             */
            /*
             * WWW: full translation map? 
             */
            snmp_log(LOG_ERR, "Illegal error_value %d translated to %d\n",
                     error_value, SNMP_ERR_GENERR);
            request->status = SNMP_ERR_GENERR;
        } else {
            /*
             * WWW: translations and mode checking? 
             */
            request->status = error_value;
        }
        break;
    }
    return SNMPERR_SUCCESS;
}

/** set error for a request
 * @param request request which has error
 * @param error_value error value for request
 */
int
netsnmp_request_set_error(netsnmp_request_info *request, int error_value)
{
    if (!request || !request->agent_req_info)
        return SNMPERR_NO_VARS;

    return _request_set_error(request, request->agent_req_info->mode,
                              error_value);
}

#ifndef NETSNMP_FEATURE_REMOVE_REQUEST_SET_ERROR_IDX
/** set error for a request within a request list
 * @param request head of the request list
 * @param error_value error value for request
 * @param idx index of the request which has the error
 */
int
netsnmp_request_set_error_idx(netsnmp_request_info *request,
                              int error_value, int idx)
{
    int i;
    netsnmp_request_info *req = request;

    if (!request || !request->agent_req_info)
        return SNMPERR_NO_VARS;

    /*
     * Skip to the indicated varbind
     */
    for ( i=2; i<idx; i++) {
        req = req->next;
        if (!req)
            return SNMPERR_NO_VARS;
    }
    
    return _request_set_error(req, request->agent_req_info->mode,
                              error_value);
}
#endif /* NETSNMP_FEATURE_REMOVE_REQUEST_SET_ERROR_IDX */

/** set error for all requests
 * @param requests request list
 * @param error error value for requests
 * @return SNMPERR_SUCCESS, or an error code
 */
NETSNMP_INLINE int
netsnmp_request_set_error_all( netsnmp_request_info *requests, int error)
{
    int mode, rc, result = SNMPERR_SUCCESS;

    if((NULL == requests) || (NULL == requests->agent_req_info))
        return SNMPERR_NO_VARS;
    
    mode = requests->agent_req_info->mode; /* every req has same mode */
    
    for(; requests ; requests = requests->next) {

        /** paranoid sanity checks */
        netsnmp_assert(NULL != requests->agent_req_info);
        netsnmp_assert(mode == requests->agent_req_info->mode);

        /*
         * set error for this request. Log any errors, save the last
         * to return to the user.
         */
        if((rc = _request_set_error(requests, mode, error))) {
            snmp_log(LOG_WARNING,"got %d while setting request error\n", rc);
            result = rc;
        }
    }
    return result;
}

/**
 * Return the difference between pm and the agent start time in hundredths of
 * a second.
 * \deprecated Don't use in new code.
 *
 * @param[in] pm An absolute time as e.g. reported by gettimeofday().
 */
u_long
netsnmp_marker_uptime(marker_t pm)
{
    u_long          res;
    const_marker_t  start = netsnmp_get_agent_starttime();

    res = uatime_hdiff(start, pm);
    return res;
}

/**
 * Return the difference between tv and the agent start time in hundredths of
 * a second.
 *
 * \deprecated Use netsnmp_get_agent_uptime() instead.
 *
 * @param[in] tv An absolute time as e.g. reported by gettimeofday().
 */
u_long
netsnmp_timeval_uptime(struct timeval * tv)
{
    return netsnmp_marker_uptime((marker_t) tv);
}


struct timeval  starttime;
static struct timeval starttimeM;

/**
 * Return a pointer to the variable in which the Net-SNMP start time has
 * been stored.
 *
 * @note Use netsnmp_get_agent_runtime() instead of this function if you need
 *   to know how much time elapsed since netsnmp_set_agent_starttime() has been
 *   called.
 */
const_marker_t        
netsnmp_get_agent_starttime(void)
{
    return &starttime;
}

/**
 * Report the time that elapsed since the agent start time in hundredths of a
 * second.
 *
 * @see See also netsnmp_set_agent_starttime().
 */
uint64_t
netsnmp_get_agent_runtime(void)
{
    struct timeval now, delta;

    netsnmp_get_monotonic_clock(&now);
    NETSNMP_TIMERSUB(&now, &starttimeM, &delta);
    return delta.tv_sec * (uint64_t)100 + delta.tv_usec / 10000;
}

/**
 * Set the time at which Net-SNMP started either to the current time
 * (if s == NULL) or to *s (if s is not NULL).
 *
 * @see See also netsnmp_set_agent_uptime().
 */
void            
netsnmp_set_agent_starttime(marker_t s)
{
    if (s) {
        struct timeval nowA, nowM;

        starttime = *(struct timeval*)s;
        gettimeofday(&nowA, NULL);
        netsnmp_get_monotonic_clock(&nowM);
        NETSNMP_TIMERSUB(&starttime, &nowA, &starttimeM);
        NETSNMP_TIMERADD(&starttimeM, &nowM, &starttimeM);
    } else {
        gettimeofday(&starttime, NULL);
        netsnmp_get_monotonic_clock(&starttimeM);
    }
}


/**
 * Return the current value of 'sysUpTime' 
 */
u_long
netsnmp_get_agent_uptime(void)
{
    struct timeval now, delta;

    netsnmp_get_monotonic_clock(&now);
    NETSNMP_TIMERSUB(&now, &starttimeM, &delta);
    return delta.tv_sec * 100UL + delta.tv_usec / 10000;
}

#ifndef NETSNMP_FEATURE_REMOVE_SET_AGENT_UPTIME
/**
 * Set the start time from which 'sysUpTime' is computed.
 *
 * @param[in] hsec New sysUpTime in hundredths of a second.
 *
 * @see See also netsnmp_set_agent_starttime().
 */
void
netsnmp_set_agent_uptime(u_long hsec)
{
    struct timeval  nowA, nowM;
    struct timeval  new_uptime;

    gettimeofday(&nowA, NULL);
    netsnmp_get_monotonic_clock(&nowM);
    new_uptime.tv_sec = hsec / 100;
    new_uptime.tv_usec = (uint32_t)(hsec - new_uptime.tv_sec * 100) * 10000L;
    NETSNMP_TIMERSUB(&nowA, &new_uptime, &starttime);
    NETSNMP_TIMERSUB(&nowM, &new_uptime, &starttimeM);
}
#endif /* NETSNMP_FEATURE_REMOVE_SET_AGENT_UPTIME */


/*************************************************************************
 *
 * deprecated functions
 *
 */

/** set error for a request
 * \deprecated, use netsnmp_request_set_error instead
 * @param reqinfo agent_request_info pointer for request
 * @param request request_info pointer
 * @param error_value error value for requests
 * @return error_value
 */
int
netsnmp_set_request_error(netsnmp_agent_request_info *reqinfo,
                          netsnmp_request_info *request, int error_value)
{
    if (!request || !reqinfo)
        return error_value;

    _request_set_error(request, reqinfo->mode, error_value);
    
    return error_value;
}

/** set error for a request
 * \deprecated, use netsnmp_request_set_error instead
 * @param mode Net-SNMP agent processing mode
 * @param request request_info pointer
 * @param error_value error value for requests
 * @return error_value
 */
int
netsnmp_set_mode_request_error(int mode, netsnmp_request_info *request,
                               int error_value)
{
    _request_set_error(request, mode, error_value);
    
    return error_value;
}

/** set error for all request
 * \deprecated use netsnmp_request_set_error_all
 * @param reqinfo agent_request_info pointer for requests
 * @param requests request list
 * @param error_value error value for requests
 * @return error_value
 */
#ifndef NETSNMP_FEATURE_REMOVE_SET_ALL_REQUESTS_ERROR
int
netsnmp_set_all_requests_error(netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests,
                               int error_value)
{
    netsnmp_request_set_error_all(requests, error_value);
    return error_value;
}
#endif /* NETSNMP_FEATURE_REMOVE_SET_ALL_REQUESTS_ERROR */
/** @} */
