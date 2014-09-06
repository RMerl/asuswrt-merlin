#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/row_merge.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

netsnmp_feature_provide(row_merge)
netsnmp_feature_child_of(row_merge, row_merge_all)
netsnmp_feature_child_of(row_merge_all, mib_helpers)


#ifndef NETSNMP_FEATURE_REMOVE_ROW_MERGE
/** @defgroup row_merge row_merge
 *  Calls sub handlers with request for one row at a time.
 *  @ingroup utilities
 *  This helper splits a whole bunch of requests into chunks based on the row
 *  index that they refer to, and passes all requests for a given row to the lower handlers.
 *  This is useful for handlers that don't want to process multiple rows at the
 *  same time, but are happy to iterate through the request list for a single row.
 *  @{
 */

/** returns a row_merge handler that can be injected into a given
 *  handler chain.  
 */
netsnmp_mib_handler *
netsnmp_get_row_merge_handler(int prefix_len)
{
    netsnmp_mib_handler *ret = NULL;
    ret = netsnmp_create_handler("row_merge",
                                  netsnmp_row_merge_helper_handler);
    if (ret) {
        ret->myvoid = (void *)(intptr_t)prefix_len;
    }
    return ret;
}

/** functionally the same as calling netsnmp_register_handler() but also
 * injects a row_merge handler at the same time for you. */
netsnmp_feature_child_of(register_row_merge, row_merge_all)
#ifndef NETSNMP_FEATURE_REMOVE_REGISTER_ROW_MERGE
int
netsnmp_register_row_merge(netsnmp_handler_registration *reginfo)
{
    netsnmp_inject_handler(reginfo,
		    netsnmp_get_row_merge_handler(reginfo->rootoid_len+1));
    return netsnmp_register_handler(reginfo);
}
#endif /* NETSNMP_FEATURE_REMOVE_REGISTER_ROW_MERGE */

static void
_rm_status_free(void *mem)
{
    netsnmp_row_merge_status *rm_status = (netsnmp_row_merge_status*)mem;

    if (NULL != rm_status->saved_requests)
        free(rm_status->saved_requests);

    if (NULL != rm_status->saved_status)
        free(rm_status->saved_status);

    free(mem);
}


/** retrieve row_merge_status
 */
netsnmp_row_merge_status *
netsnmp_row_merge_status_get(netsnmp_handler_registration *reginfo,
                             netsnmp_agent_request_info *reqinfo,
                             int create_missing)
{
    netsnmp_row_merge_status *rm_status;
    char buf[64];
    int rc;

    /*
     * see if we've already been here
     */
    rc = snprintf(buf, sizeof(buf), "row_merge:%p", reginfo);
    if ((-1 == rc) || ((size_t)rc >= sizeof(buf))) {
        snmp_log(LOG_ERR,"error creating key\n");
        return NULL;
    }
    
    rm_status = (netsnmp_row_merge_status*)netsnmp_agent_get_list_data(reqinfo, buf);
    if ((NULL == rm_status) && create_missing) {
        netsnmp_data_list *data_list;
        
        rm_status = SNMP_MALLOC_TYPEDEF(netsnmp_row_merge_status);
        if (NULL == rm_status) {
            snmp_log(LOG_ERR,"error allocating memory\n");
            return NULL;
        }
        data_list = netsnmp_create_data_list(buf, rm_status,
                                             _rm_status_free);
        if (NULL == data_list) {
            free(rm_status);
            return NULL;
        }
        netsnmp_agent_add_list_data(reqinfo, data_list);
    }
    
    return rm_status;
}

/** Determine if this is the first row
 *
 * returns 1 if this is the first row for this pass of the handler.
 */
int
netsnmp_row_merge_status_first(netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo)
{
    netsnmp_row_merge_status *rm_status;

    /*
     * find status
     */
    rm_status = netsnmp_row_merge_status_get(reginfo, reqinfo, 0);
    if (NULL == rm_status)
        return 0;

    return (rm_status->count == 1) ? 1 : (rm_status->current == 1);
}

/** Determine if this is the last row
 *
 * returns 1 if this is the last row for this pass of the handler.
 */
int
netsnmp_row_merge_status_last(netsnmp_handler_registration *reginfo,
                              netsnmp_agent_request_info *reqinfo)
{
    netsnmp_row_merge_status *rm_status;

    /*
     * find status
     */
    rm_status = netsnmp_row_merge_status_get(reginfo, reqinfo, 0);
    if (NULL == rm_status)
        return 0;

    return (rm_status->count == 1) ? 1 :
        (rm_status->current == rm_status->rows);
}


#define ROW_MERGE_WAITING 0
#define ROW_MERGE_ACTIVE  1
#define ROW_MERGE_DONE    2
#define ROW_MERGE_HEAD    3

/** Implements the row_merge handler */
int
netsnmp_row_merge_helper_handler(netsnmp_mib_handler *handler,
                                 netsnmp_handler_registration *reginfo,
                                 netsnmp_agent_request_info *reqinfo,
                                 netsnmp_request_info *requests)
{
    netsnmp_request_info *request, **saved_requests;
    char *saved_status;
    netsnmp_row_merge_status *rm_status;
    int i, j, ret, tail, count, final_rc = SNMP_ERR_NOERROR;

    /*
     * Use the prefix length as supplied during registration, rather
     *  than trying to second-guess what the MIB implementer wanted.
     */
    int SKIP_OID = (int)(intptr_t)handler->myvoid;

    DEBUGMSGTL(("helper:row_merge", "Got request (%d): ", SKIP_OID));
    DEBUGMSGOID(("helper:row_merge", reginfo->rootoid, reginfo->rootoid_len));
    DEBUGMSG(("helper:row_merge", "\n"));

    /*
     * find or create status
     */
    rm_status = netsnmp_row_merge_status_get(reginfo, reqinfo, 1);

    /*
     * Count the requests, and set up an array to keep
     *  track of the original order.
     */
    for (count = 0, request = requests; request; request = request->next) {
        DEBUGIF("helper:row_merge") {
            DEBUGMSGTL(("helper:row_merge", "  got varbind: "));
            DEBUGMSGOID(("helper:row_merge", request->requestvb->name,
                         request->requestvb->name_length));
            DEBUGMSG(("helper:row_merge", "\n"));
        }
        count++;
    }

    /*
     * Optimization: skip all this if there is just one request
     */
    if(count == 1) {
        rm_status->count = count;
        if (requests->processed)
            return SNMP_ERR_NOERROR;
        return netsnmp_call_next_handler(handler, reginfo, reqinfo, requests);
    }

    /*
     * we really should only have to do this once, instead of every pass.
     * as a precaution, we'll do it every time, but put in some asserts
     * to see if we have to.
     */
    /*
     * if the count changed, re-do everything
     */
    if ((0 != rm_status->count) && (rm_status->count != count)) {
        /*
         * ok, i know next/bulk can cause this condition. Probably
         * GET, too. need to rethink this mode counting. maybe
         * add the mode to the rm_status structure? xxx-rks
         */
        if ((reqinfo->mode != MODE_GET) &&
            (reqinfo->mode != MODE_GETNEXT) &&
            (reqinfo->mode != MODE_GETBULK)) {
            netsnmp_assert((NULL != rm_status->saved_requests) &&
                           (NULL != rm_status->saved_status));
        }
        DEBUGMSGTL(("helper:row_merge", "count changed! do over...\n"));

        SNMP_FREE(rm_status->saved_requests);
        SNMP_FREE(rm_status->saved_status);
        
        rm_status->count = 0;
        rm_status->rows = 0;
    }

    if (0 == rm_status->count) {
        /*
         * allocate memory for saved structure
         */
        rm_status->saved_requests =
            (netsnmp_request_info**)calloc(count+1,
                                           sizeof(netsnmp_request_info*));
        rm_status->saved_status = (char*)calloc(count,sizeof(char));
    }

    saved_status = rm_status->saved_status;
    saved_requests = rm_status->saved_requests;

    /*
     * set up saved requests, and set any processed requests to done
     */
    i = 0;
    for (request = requests; request; request = request->next, i++) {
        if (request->processed) {
            saved_status[i] = ROW_MERGE_DONE;
            DEBUGMSGTL(("helper:row_merge", "  skipping processed oid: "));
            DEBUGMSGOID(("helper:row_merge", request->requestvb->name,
                         request->requestvb->name_length));
            DEBUGMSG(("helper:row_merge", "\n"));
        }
        else
            saved_status[i] = ROW_MERGE_WAITING;
        if (0 != rm_status->count)
            netsnmp_assert(saved_requests[i] == request);
        saved_requests[i] = request;
        saved_requests[i]->prev = NULL;
    }
    saved_requests[i] = NULL;

    /*
     * Note that saved_requests[count] is valid
     *    (because of the 'count+1' in the calloc above),
     * but NULL (since it's past the end of the list).
     * This simplifies the re-linking later.
     */

    /*
     * Work through the (unprocessed) requests in order.
     * For each of these, search the rest of the list for any
     *   matching indexes, and link them into a new list.
     */
    for (i=0; i<count; i++) {
	if (saved_status[i] != ROW_MERGE_WAITING)
	    continue;

        if (0 == rm_status->count)
            rm_status->rows++;
        DEBUGMSGTL(("helper:row_merge", " row %d oid[%d]: ", rm_status->rows, i));
        DEBUGMSGOID(("helper:row_merge", saved_requests[i]->requestvb->name,
                     saved_requests[i]->requestvb->name_length));
        DEBUGMSG(("helper:row_merge", "\n"));

	saved_requests[i]->next = NULL;
	saved_status[i] = ROW_MERGE_HEAD;
	tail = i;
        for (j=i+1; j<count; j++) {
	    if (saved_status[j] != ROW_MERGE_WAITING)
	        continue;

            DEBUGMSGTL(("helper:row_merge", "? oid[%d]: ", j));
            DEBUGMSGOID(("helper:row_merge",
                         saved_requests[j]->requestvb->name,
                         saved_requests[j]->requestvb->name_length));
            if (!snmp_oid_compare(
                    saved_requests[i]->requestvb->name+SKIP_OID,
                    saved_requests[i]->requestvb->name_length-SKIP_OID,
                    saved_requests[j]->requestvb->name+SKIP_OID,
                    saved_requests[j]->requestvb->name_length-SKIP_OID)) {
                DEBUGMSG(("helper:row_merge", " match\n"));
                saved_requests[tail]->next = saved_requests[j];
                saved_requests[j]->next    = NULL;
                saved_requests[j]->prev = saved_requests[tail];
	        saved_status[j] = ROW_MERGE_ACTIVE;
	        tail = j;
            }
            else
                DEBUGMSG(("helper:row_merge", " no match\n"));
        }
    }

    /*
     * not that we have a list for each row, call next handler...
     */
    if (0 == rm_status->count)
        rm_status->count = count;
    rm_status->current = 0;
    for (i=0; i<count; i++) {
	if (saved_status[i] != ROW_MERGE_HEAD)
	    continue;

        /*
         * found the head of a new row,
         * call the next handler with this list
         */
        rm_status->current++;
        ret = netsnmp_call_next_handler(handler, reginfo, reqinfo,
			                saved_requests[i]);
        if (ret != SNMP_ERR_NOERROR) {
            snmp_log(LOG_WARNING,
                     "bad rc (%d) from next handler in row_merge\n", ret);
            if (SNMP_ERR_NOERROR == final_rc)
                final_rc = ret;
        }
    }

    /*
     * restore original linked list
     */
    for (i=0; i<count; i++) {
	saved_requests[i]->next = saved_requests[i+1];
        if (i>0)
	    saved_requests[i]->prev = saved_requests[i-1];
    }

    return final_rc;
}

/** 
 *  initializes the row_merge helper which then registers a row_merge
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
netsnmp_init_row_merge(void)
{
    netsnmp_register_handler_by_name("row_merge",
                                     netsnmp_get_row_merge_handler(-1));
}
#else /* NETSNMP_FEATURE_REMOVE_ROW_MERGE */
netsnmp_feature_unused(row_merge);
#endif /* NETSNMP_FEATURE_REMOVE_ROW_MERGE */


/**  @} */

