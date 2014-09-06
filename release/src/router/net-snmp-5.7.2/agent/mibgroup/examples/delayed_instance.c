/**  @example delayed_instance.c
 *  This example implements the netSnmpExampleSleeper object.
 *
 *  It demonstrates 2 things:
 *
 *  - The instance helper, which is a way of registering an exact OID
 *    such that GENEXT requests are handled entirely by the helper.
 *
 *  - how to implement objects which normally would block the agent as
 *    it waits for external events in such a way that the agent can
 *    continue responding to other requests while this implementation
 *    waits.
 *
 *  - Added bonus: normally the nsTransactionTable is empty, since
 *    there aren't any outstanding requests generally.  When accessed,
 *    this module will create some however.  Try setting
 *    netSnmpExampleSleeper.0 to 10 and then accessing it (use
 *    "snmpget -t 15 ..." to access it), and then walk the
 *    nsTransactionTable from another shell to see that not only is
 *    the walk not blocked, but that the nsTransactionTable is not
 *    empty.
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "delayed_instance.h"

static u_long   delay_time = 1;

void
init_delayed_instance(void)
{
    static oid      my_delayed_oid[] =
        { 1, 3, 6, 1, 4, 1, 8072, 2, 1, 2, 0 };
    /*
     * delayed handler test
     */
    netsnmp_handler_registration *my_test;

    my_test =
        netsnmp_create_handler_registration("delayed_instance_example",
                                            delayed_instance_handler,
                                            my_delayed_oid,
                                            OID_LENGTH(my_delayed_oid),
                                            HANDLER_CAN_RWRITE);

    netsnmp_register_instance(my_test);
}

#define DELAYED_INSTANCE_SET_NAME "test_delayed"

int
delayed_instance_handler(netsnmp_mib_handler *handler,
                         netsnmp_handler_registration *reginfo,
                         netsnmp_agent_request_info *reqinfo,
                         netsnmp_request_info *requests)
{

    DEBUGMSGTL(("delayed_instance", "Got request, mode = %d:\n",
                reqinfo->mode));

    switch (reqinfo->mode) {
        /*
         * here we merely mention that we'll answer this request
         * later.  we don't actually care about the mode type in this
         * example, but for certain cases you may, so I'll leave in the
         * otherwise useless switch and case statements 
         */

    default:
        /*
         * mark this variable as something that can't be handled now.
         * We'll answer it later. 
         */
        requests->delegated = 1;

        /*
         * register an alarm to update the results at a later
         * time.  Normally, we might have to query something else
         * (like an external request sent to a different network
         * or system socket, etc), but for this example we'll do
         * something really simply and just insert an alarm for a
         * certain period of time 
         */
        snmp_alarm_register(delay_time, /* seconds */
                            0,  /* dont repeat. */
                            return_delayed_response,    /* the function
                                                         * to call */
                            /*
                             * here we create a "cache" of useful
                             * information that we'll want later
                             * on.  This argument is passed back
                             * to us in the callback function for
                             * an alarm 
                             */
                            (void *)
                            netsnmp_create_delegated_cache(handler,
                                                           reginfo,
                                                           reqinfo,
                                                           requests,
                                                           NULL));
        break;

    }

    return SNMP_ERR_NOERROR;
}

void
return_delayed_response(unsigned int clientreg, void *clientarg)
{
    /*
     * extract the cache from the passed argument 
     */
    netsnmp_delegated_cache *cache = (netsnmp_delegated_cache *) clientarg;

    netsnmp_request_info *requests;
    netsnmp_agent_request_info *reqinfo;
    u_long         *delay_time_cache = NULL;

    /*
     * here we double check that the cache we created earlier is still
     * * valid.  If not, the request timed out for some reason and we
     * * don't need to keep processing things.  Should never happen, but
     * * this double checks. 
     */
    cache = netsnmp_handler_check_cache(cache);

    if (!cache) {
        snmp_log(LOG_ERR, "illegal call to return delayed response\n");
        return;
    }

    /*
     * re-establish the previous pointers we are used to having 
     */
    reqinfo = cache->reqinfo;
    requests = cache->requests;

    DEBUGMSGTL(("delayed_instance",
                "continuing delayed request, mode = %d\n",
                cache->reqinfo->mode));

    /*
     * mention that it's no longer delegated, and we've now answered
     * the query (which we'll do down below). 
     */
    requests->delegated = 0;

    switch (cache->reqinfo->mode) {
        /*
         * registering as an instance means we don't need to deal with
         * getnext processing, so we don't handle it here at all.
         * 
         * However, since the instance handler already reset the mode
         * back to GETNEXT from the faked GET mode, we need to do the
         * same thing in both cases.  This should be fixed in future
         * versions of net-snmp hopefully. 
         */

    case MODE_GET:
    case MODE_GETNEXT:
        /*
         * return the currend delay time 
         */
        snmp_set_var_typed_value(cache->requests->requestvb,
                                 ASN_INTEGER,
                                 (u_char *) & delay_time,
                                 sizeof(delay_time));
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        /*
         * check type 
         */
        if (requests->requestvb->type != ASN_INTEGER) {
            /*
             * not an integer.  Bad dog, no bone. 
             */
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_WRONGTYPE);
            /*
             * we don't need the cache any longer 
             */
            netsnmp_free_delegated_cache(cache);
            return;
        }
        break;

    case MODE_SET_RESERVE2:
        /*
         * store old value for UNDO support in the future. 
         */
        memdup((u_char **) & delay_time_cache,
               (u_char *) & delay_time, sizeof(delay_time));

        /*
         * malloc failed 
         */
        if (delay_time_cache == NULL) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_RESOURCEUNAVAILABLE);
            netsnmp_free_delegated_cache(cache);
            return;
        }

        /*
         * Add our temporary information to the request itself.
         * This is then retrivable later.  The free function
         * passed auto-frees it when the request is later
         * deleted.  
         */
        netsnmp_request_add_list_data(requests,
                                      netsnmp_create_data_list
                                      (DELAYED_INSTANCE_SET_NAME,
                                       delay_time_cache, free));
        break;

    case MODE_SET_ACTION:
        /*
         * update current value 
         */
        delay_time = *(requests->requestvb->val.integer);
        DEBUGMSGTL(("testhandler", "updated delay_time -> %ld\n",
                    delay_time));
        break;

    case MODE_SET_UNDO:
        /*
         * ack, something somewhere failed.  We reset back to the
         * previously old value by extracting the previosuly
         * stored information back out of the request 
         */
        delay_time =
            *((u_long *) netsnmp_request_get_list_data(requests,
                                                       DELAYED_INSTANCE_SET_NAME));
        break;

    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
        /*
         * the only thing to do here is free the old memdup'ed
         * value, but it's auto-freed by the datalist recovery, so
         * we don't have anything to actually do here 
         */
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    }

    /*
     * free the information cache 
     */
    netsnmp_free_delegated_cache(cache);
}
