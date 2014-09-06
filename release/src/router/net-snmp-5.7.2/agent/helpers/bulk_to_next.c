#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/bulk_to_next.h>

/** @defgroup bulk_to_next bulk_to_next
 *  Convert GETBULK requests into GETNEXT requests for the handler.
 *  The only purpose of this handler is to convert a GETBULK request
 *  to a GETNEXT request.  It is inserted into handler chains where
 *  the handler has not set the HANDLER_CAN_GETBULK flag.
 *  @ingroup utilities
 *  @{
 */

/** returns a bulk_to_next handler that can be injected into a given
 *  handler chain.
 */
netsnmp_mib_handler *
netsnmp_get_bulk_to_next_handler(void)
{
    netsnmp_mib_handler *handler =
        netsnmp_create_handler("bulk_to_next",
                               netsnmp_bulk_to_next_helper);

    if (NULL != handler)
        handler->flags |= MIB_HANDLER_AUTO_NEXT;

    return handler;
}

/** takes answered requests and decrements the repeat count and
 *  updates the requests to the next to-do varbind in the list */
void
netsnmp_bulk_to_next_fix_requests(netsnmp_request_info *requests)
{
    netsnmp_request_info *request;
    /*
     * Make sure that:
     *    - repeats remain
     *    - last handler provided an answer
     *    - answer didn't exceed range end (ala check_getnext_results)
     *    - there is a next variable
     * then
     * update the varbinds for the next request series 
     */
    for (request = requests; request; request = request->next) {
        if (request->repeat > 0 &&
            request->requestvb->type != ASN_NULL &&
            request->requestvb->type != ASN_PRIV_RETRY &&
            (snmp_oid_compare(request->requestvb->name,
                              request->requestvb->name_length,
                              request->range_end,
                              request->range_end_len) < 0) &&
            request->requestvb->next_variable ) {
            request->repeat--;
            snmp_set_var_objid(request->requestvb->next_variable,
                               request->requestvb->name,
                               request->requestvb->name_length);
            request->requestvb = request->requestvb->next_variable;
            request->requestvb->type = ASN_PRIV_RETRY;
            /*
             * if inclusive == 2, it was set in check_getnext_results for
             * the previous requestvb. Now that we've moved on, clear it.
             */
            if (2 == request->inclusive)
                request->inclusive = 0;
        }
    }
}

/** @internal Implements the bulk_to_next handler */
int
netsnmp_bulk_to_next_helper(netsnmp_mib_handler *handler,
                            netsnmp_handler_registration *reginfo,
                            netsnmp_agent_request_info *reqinfo,
                            netsnmp_request_info *requests)
{

    int             ret = SNMP_ERR_NOERROR;

    /*
     * this code depends on AUTO_NEXT being set
     */
    netsnmp_assert(handler->flags & MIB_HANDLER_AUTO_NEXT);

    /*
     * don't do anything for any modes besides GETBULK. Just return, and
     * the agent will call the next handler (AUTO_NEXT).
     *
     * for GETBULK, we munge the mode, call the next handler ourselves
     * (setting AUTO_NEXT_OVERRRIDE so the agent knows what we did),
     * restore the mode and fix up the requests.
     */
    if(MODE_GETBULK == reqinfo->mode) {

        DEBUGIF("bulk_to_next") {
            netsnmp_request_info *req = requests;
            while(req) {
                DEBUGMSGTL(("bulk_to_next", "Got request: "));
                DEBUGMSGOID(("bulk_to_next", req->requestvb->name,
                             req->requestvb->name_length));
                DEBUGMSG(("bulk_to_next", "\n"));
                req = req->next;
            }
        }

        reqinfo->mode = MODE_GETNEXT;
        ret =
            netsnmp_call_next_handler(handler, reginfo, reqinfo, requests);
        reqinfo->mode = MODE_GETBULK;

        /*
         * update the varbinds for the next request series 
         */
        netsnmp_bulk_to_next_fix_requests(requests);

        /*
         * let agent handler know that we've already called next handler
         */
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
    }

    return ret;
}

/** initializes the bulk_to_next helper which then registers a bulk_to_next
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
netsnmp_init_bulk_to_next_helper(void)
{
    netsnmp_register_handler_by_name("bulk_to_next",
                                     netsnmp_get_bulk_to_next_handler());
}
/**  @} */

