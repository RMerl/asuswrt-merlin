#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

netsnmp_feature_provide(stash_to_next)
netsnmp_feature_child_of(stash_to_next, mib_helpers)

#ifdef NETSNMP_FEATURE_REQUIRE_STASH_TO_NEXT
netsnmp_feature_require(oid_stash)
netsnmp_feature_require(oid_stash_add_data)
#endif

#ifndef NETSNMP_FEATURE_REMOVE_STASH_TO_NEXT
#include <net-snmp/agent/stash_to_next.h>

#include <net-snmp/agent/stash_cache.h>

/** @defgroup stash_to_next stash_to_next
 *  Convert GET_STASH requests into GETNEXT requests for the handler.
 *  The purpose of this handler is to convert a GET_STASH auto-cache request
 *  to a series of GETNEXT requests.  It can be inserted into a handler chain
 *  where the lower-level handlers don't process such requests themselves.
 *  @ingroup utilities
 *  @{
 */

/** returns a stash_to_next handler that can be injected into a given
 *  handler chain.
 */
netsnmp_mib_handler *
netsnmp_get_stash_to_next_handler(void)
{
    netsnmp_mib_handler *handler =
        netsnmp_create_handler("stash_to_next",
                               netsnmp_stash_to_next_helper);

    if (NULL != handler)
        handler->flags |= MIB_HANDLER_AUTO_NEXT;

    return handler;
}

/** @internal Implements the stash_to_next handler */
int
netsnmp_stash_to_next_helper(netsnmp_mib_handler *handler,
                            netsnmp_handler_registration *reginfo,
                            netsnmp_agent_request_info *reqinfo,
                            netsnmp_request_info *requests)
{

    int             ret = SNMP_ERR_NOERROR;
    int             namelen;
    int             finished = 0;
    netsnmp_oid_stash_node **cinfo;
    netsnmp_variable_list   *vb;
    netsnmp_request_info    *reqtmp;

    /*
     * this code depends on AUTO_NEXT being set
     */
    netsnmp_assert(handler->flags & MIB_HANDLER_AUTO_NEXT);

    /*
     * Don't do anything for any modes except GET_STASH. Just return,
     * and the agent will call the next handler (AUTO_NEXT).
     *
     * If the handler chain already supports GET_STASH, we don't
     * need to do anything here either.  Once again, we just return
     * and the agent will call the next handler (AUTO_NEXT).
     *
     * Otherwise, we munge the mode to GET_NEXT, and call the
     * next handler ourselves, repeatedly until we've retrieved the
     * full contents of the table or subtree.
     *   Then restore the mode and return to the calling handler 
     * (setting AUTO_NEXT_OVERRRIDE so the agent knows what we did).
     */
    if (MODE_GET_STASH == reqinfo->mode) {
        if ( reginfo->modes & HANDLER_CAN_STASH ) {
            return ret;
        }
        cinfo  = netsnmp_extract_stash_cache( reqinfo );
        reqtmp = SNMP_MALLOC_TYPEDEF(netsnmp_request_info);
        vb = reqtmp->requestvb = SNMP_MALLOC_TYPEDEF( netsnmp_variable_list );
        vb->type = ASN_NULL;
        snmp_set_var_objid( vb, reginfo->rootoid, reginfo->rootoid_len );

        reqinfo->mode = MODE_GETNEXT;
        while (!finished) {
            ret = netsnmp_call_next_handler(handler, reginfo, reqinfo, reqtmp);
            namelen = SNMP_MIN(vb->name_length, reginfo->rootoid_len);
            if ( !snmp_oid_compare( reginfo->rootoid, reginfo->rootoid_len,
                                   vb->name, namelen) &&
                 vb->type != ASN_NULL && vb->type != SNMP_ENDOFMIBVIEW ) {
                /*
                 * This result is relevant so save it, and prepare
                 * the request varbind for the next query.
                 */
                netsnmp_oid_stash_add_data( cinfo, vb->name, vb->name_length,
                                            snmp_clone_varbind( vb ));
                    /*
                     * Tidy up the response structure,
                     *  ready for retrieving the next entry
                     */
                netsnmp_free_all_list_data(reqtmp->parent_data);
                reqtmp->parent_data = NULL;
                reqtmp->processed = 0;
                vb->type = ASN_NULL;
            } else {
                finished = 1;
            }
        }
        reqinfo->mode = MODE_GET_STASH;

        /*
         * let the handler chain processing know that we've already
         * called the next handler
         */
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
    }

    return ret;
}
/**  @} */

#else  /* ! NETSNMP_FEATURE_REMOVE_STASH_TO_NEXT */
netsnmp_feature_unused(stash_to_next);
#endif /* ! NETSNMP_FEATURE_REMOVE_STASH_TO_NEXT */
