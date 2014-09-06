/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright @ 2009 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <sys/types.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "proxy.h"

netsnmp_feature_require(handler_mark_requests_as_delegated)
netsnmp_feature_require(request_set_error_idx)

static struct simple_proxy *proxies = NULL;

oid             testoid[] = { 1, 3, 6, 1, 4, 1, 2021, 8888, 1 };

/*
 * this must be standardized somewhere, right? 
 */
#define MAX_ARGS 128

char           *context_string;

static void
proxyOptProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'n':
                optind++;
                if (optind < argc) {
                    context_string = argv[optind - 1];
                } else {
                    config_perror("No context name passed to -Cn");
                }
                break;
            case 'c':
                netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                                       NETSNMP_DS_LIB_IGNORE_NO_COMMUNITY, 1);
                break;
            default:
                config_perror("unknown argument passed to -C");
                break;
            }
        }
        break;
    default:
        break;
        /*
         * shouldn't get here 
         */
    }
}

void
proxy_parse_config(const char *token, char *line)
{
    /*
     * proxy args [base-oid] [remap-to-remote-oid] 
     */

    netsnmp_session session, *ss;
    struct simple_proxy *newp, **listpp;
    char            args[MAX_ARGS][SPRINT_MAX_LEN], *argv[MAX_ARGS];
    int             argn, arg;
    char           *cp;
    netsnmp_handler_registration *reg;

    context_string = NULL;

    DEBUGMSGTL(("proxy_config", "entering\n"));

    /*
     * create the argv[] like array 
     */
    strcpy(argv[0] = args[0], "snmpd-proxy");   /* bogus entry for getopt() */
    for (argn = 1, cp = line; cp && argn < MAX_ARGS;) {
	argv[argn] = args[argn];
        cp = copy_nword(cp, argv[argn], SPRINT_MAX_LEN);
	argn++;
    }

    for (arg = 0; arg < argn; arg++) {
        DEBUGMSGTL(("proxy_args", "final args: %d = %s\n", arg,
                    argv[arg]));
    }

    DEBUGMSGTL(("proxy_config", "parsing args: %d\n", argn));
    /* Call special parse_args that allows for no specified community string */
    arg = netsnmp_parse_args(argn, argv, &session, "C:", proxyOptProc,
                             NETSNMP_PARSE_ARGS_NOLOGGING |
                             NETSNMP_PARSE_ARGS_NOZERO);

    /* reset this in case we modified it */
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                           NETSNMP_DS_LIB_IGNORE_NO_COMMUNITY, 0);
    
    if (arg < 0) {
        config_perror("failed to parse proxy args");
        return;
    }
    DEBUGMSGTL(("proxy_config", "done parsing args\n"));

    if (arg >= argn) {
        config_perror("missing base oid");
        return;
    }

    /*
     * usm_set_reportErrorOnUnknownID(0); 
     *
     * hack, stupid v3 ASIs. 
     */
    /*
     * XXX: on a side note, we don't really need to be a reference
     * platform any more so the proper thing to do would be to fix
     * snmplib/snmpusm.c to pass in the pdu type to usm_process_incoming
     * so this isn't needed. 
     */
    ss = snmp_open(&session);
    /*
     * usm_set_reportErrorOnUnknownID(1); 
     */
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmpget", &session);
        SOCK_CLEANUP;
        return;
    }

    newp = (struct simple_proxy *) calloc(1, sizeof(struct simple_proxy));

    newp->sess = ss;
    DEBUGMSGTL(("proxy_init", "name = %s\n", args[arg]));
    newp->name_len = MAX_OID_LEN;
    if (!snmp_parse_oid(args[arg++], newp->name, &newp->name_len)) {
        snmp_perror("proxy");
        config_perror("illegal proxy oid specified\n");
        return;
    }

    if (arg < argn) {
        DEBUGMSGTL(("proxy_init", "base = %s\n", args[arg]));
        newp->base_len = MAX_OID_LEN;
        if (!snmp_parse_oid(args[arg++], newp->base, &newp->base_len)) {
            snmp_perror("proxy");
            config_perror("illegal variable name specified (base oid)\n");
            return;
        }
    }
    if ( context_string )
        newp->context = strdup(context_string);

    DEBUGMSGTL(("proxy_init", "registering at: "));
    DEBUGMSGOID(("proxy_init", newp->name, newp->name_len));
    DEBUGMSG(("proxy_init", "\n"));

    /*
     * add to our chain 
     */
    /*
     * must be sorted! 
     */
    listpp = &proxies;
    while (*listpp &&
           snmp_oid_compare(newp->name, newp->name_len,
                            (*listpp)->name, (*listpp)->name_len) > 0) {
        listpp = &((*listpp)->next);
    }

    /*
     * listpp should be next in line from us. 
     */
    if (*listpp) {
        /*
         * make our next in the link point to the current link 
         */
        newp->next = *listpp;
    }
    /*
     * replace current link with us 
     */
    *listpp = newp;

    reg = netsnmp_create_handler_registration("proxy",
                                              proxy_handler,
                                              newp->name,
                                              newp->name_len,
                                              HANDLER_CAN_RWRITE);
    reg->handler->myvoid = newp;
    if (context_string)
        reg->contextName = strdup(context_string);

    netsnmp_register_handler(reg);
}

void
proxy_free_config(void)
{
    struct simple_proxy *rm;

    DEBUGMSGTL(("proxy_free_config", "Free config\n"));
    while (proxies) {
        rm = proxies;
        proxies = rm->next;

        DEBUGMSGTL(( "proxy_free_config", "freeing "));
        DEBUGMSGOID(("proxy_free_config", rm->name, rm->name_len));
        DEBUGMSG((   "proxy_free_config", " (%s)\n", rm->context));
        unregister_mib_context(rm->name, rm->name_len,
                               DEFAULT_MIB_PRIORITY, 0, 0,
                               rm->context);
        SNMP_FREE(rm->variables);
        SNMP_FREE(rm->context);
        snmp_close(rm->sess);
        SNMP_FREE(rm);
    }
}

/*
 * Configure special parameters on the session.
 * Currently takes the parameter configured and changes it if something 
 * was configured.  It becomes "-c" if the community string from the pdu
 * is placed on the session.
 */
int
proxy_fill_in_session(netsnmp_mib_handler *handler,
                      netsnmp_agent_request_info *reqinfo,
                      void **configured)
{
    netsnmp_session *session;
    struct simple_proxy *sp;

    sp = (struct simple_proxy *) handler->myvoid;
    if (!sp) {
        return 0;
    }
    session = sp->sess;
    if (!session) {
        return 0;
    }

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
#if defined(NETSNMP_DISABLE_SNMPV1)
    if (session->version == SNMP_VERSION_2c) {
#else
#if defined(NETSNMP_DISABLE_SNMPV2C)
    if (session->version == SNMP_VERSION_1) {
#else
    if (session->version == SNMP_VERSION_1 ||
        session->version == SNMP_VERSION_2c) {
#endif
#endif

        /*
         * Check if session has community string defined for it.
         * If not, need to extract community string from the pdu.
         * Copy to session and set 'configured' to indicate this.
         */
        if (session->community_len == 0) {
            DEBUGMSGTL(("proxy", "session has no community string\n"));
            if (reqinfo->asp == NULL || reqinfo->asp->pdu == NULL ||
                reqinfo->asp->pdu->community_len == 0) {
                return 0;
            }

            *configured = strdup("-c");
            DEBUGMSGTL(("proxy", "pdu has community string\n"));
            session->community_len = reqinfo->asp->pdu->community_len;
            session->community = malloc(session->community_len + 1);
            sprintf((char *)session->community, "%.*s",
                    (int) session->community_len,
                    (const char *)reqinfo->asp->pdu->community);
        }
    }
#endif

    return 1;
}

/*
 * Free any specially configured parameters used on the session.
 */
void
proxy_free_filled_in_session_args(netsnmp_session *session, void **configured)
{

    /* Only do comparisions, etc., if something was configured */
    if (*configured == NULL) {
        return;
    }

    /* If used community string from pdu, release it from session now */
    if (strcmp((const char *)(*configured), "-c") == 0) {
        free(session->community);
        session->community = NULL;
        session->community_len = 0;
    }

    free((u_char *)(*configured));
    *configured = NULL;
}

void
init_proxy(void)
{
    snmpd_register_config_handler("proxy", proxy_parse_config,
                                  proxy_free_config,
                                  "[snmpcmd args] host oid [remoteoid]");
}

void
shutdown_proxy(void)
{
    proxy_free_config();
}

int
proxy_handler(netsnmp_mib_handler *handler,
              netsnmp_handler_registration *reginfo,
              netsnmp_agent_request_info *reqinfo,
              netsnmp_request_info *requests)
{

    netsnmp_pdu    *pdu;
    struct simple_proxy *sp;
    oid            *ourname;
    size_t          ourlength;
    netsnmp_request_info *request = requests;
    u_char         *configured = NULL;

    DEBUGMSGTL(("proxy", "proxy handler starting, mode = %d\n",
                reqinfo->mode));

    switch (reqinfo->mode) {
    case MODE_GET:
    case MODE_GETNEXT:
    case MODE_GETBULK:         /* WWWXXX */
        pdu = snmp_pdu_create(reqinfo->mode);
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_ACTION:
        pdu = snmp_pdu_create(SNMP_MSG_SET);
        break;

    case MODE_SET_UNDO:
        /*
         *  If we set successfully (status == NOERROR),
         *     we can't back out again, so need to report the fact.
         *  If we failed to set successfully, then we're fine.
         */
        for (request = requests; request; request=request->next) {
            if (request->status == SNMP_ERR_NOERROR) {
                netsnmp_set_request_error(reqinfo, requests,
                                          SNMP_ERR_UNDOFAILED);
                return SNMP_ERR_UNDOFAILED;
	    }
	}
        return SNMP_ERR_NOERROR;

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_FREE:
    case MODE_SET_COMMIT:
        /*
         *  Nothing to do in this pass
         */
        return SNMP_ERR_NOERROR;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    default:
        snmp_log(LOG_WARNING, "unsupported mode for proxy called (%d)\n",
                               reqinfo->mode);
        return SNMP_ERR_NOERROR;
    }

    sp = (struct simple_proxy *) handler->myvoid;

    if (!pdu || !sp) {
        netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
        if (pdu)
            snmp_free_pdu(pdu);
        return SNMP_ERR_NOERROR;
    }

    while (request) {
        ourname = request->requestvb->name;
        ourlength = request->requestvb->name_length;

        if (sp->base_len &&
            reqinfo->mode == MODE_GETNEXT &&
            (snmp_oid_compare(ourname, ourlength,
                              sp->base, sp->base_len) < 0)) {
            DEBUGMSGTL(( "proxy", "request is out of registered range\n"));
            /*
             * Create GETNEXT request with an OID so the
             * master returns the first OID in the registered range.
             */
            memcpy(ourname, sp->base, sp->base_len * sizeof(oid));
            ourlength = sp->base_len;
            if (ourname[ourlength-1] <= 1) {
                /*
                 * The registered range ends with x.y.z.1
                 * -> ask for the next of x.y.z
                 */
                ourlength--;
            } else {
                /*
                 * The registered range ends with x.y.z.A
                 * -> ask for the next of x.y.z.A-1.MAX_SUBID
                 */
                ourname[ourlength-1]--;
                ourname[ourlength] = MAX_SUBID;
                ourlength++;
            }
        } else if (sp->base_len > 0) {
            if ((ourlength - sp->name_len + sp->base_len) > MAX_OID_LEN) {
                /*
                 * too large 
                 */
                if (pdu)
                    snmp_free_pdu(pdu);
                snmp_log(LOG_ERR,
                         "proxy oid request length is too long\n");
                return SNMP_ERR_NOERROR;
            }
            /*
             * suffix appended? 
             */
            DEBUGMSGTL(("proxy", "length=%d, base_len=%d, name_len=%d\n",
                        (int)ourlength, (int)sp->base_len, (int)sp->name_len));
            if (ourlength > sp->name_len)
                memcpy(&(sp->base[sp->base_len]), &(ourname[sp->name_len]),
                       sizeof(oid) * (ourlength - sp->name_len));
            ourlength = ourlength - sp->name_len + sp->base_len;
            ourname = sp->base;
        }

        snmp_pdu_add_variable(pdu, ourname, ourlength,
                              request->requestvb->type,
                              request->requestvb->val.string,
                              request->requestvb->val_len);
        request->delegated = 1;
        request = request->next;
    }

    /*
     * Customize session parameters based on request information
     */
    if (!proxy_fill_in_session(handler, reqinfo, (void **)&configured)) {
        netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
        if (pdu)
            snmp_free_pdu(pdu);
        return SNMP_ERR_NOERROR;
    }

    /*
     * send the request out 
     */
    DEBUGMSGTL(("proxy", "sending pdu\n"));
    snmp_async_send(sp->sess, pdu, proxy_got_response,
                    netsnmp_create_delegated_cache(handler, reginfo,
                                                   reqinfo, requests,
                                                   (void *) sp));

    /* Free any special parameters generated on the session */
    proxy_free_filled_in_session_args(sp->sess, (void **)&configured);

    return SNMP_ERR_NOERROR;
}

int
proxy_got_response(int operation, netsnmp_session * sess, int reqid,
                   netsnmp_pdu *pdu, void *cb_data)
{
    netsnmp_delegated_cache *cache = (netsnmp_delegated_cache *) cb_data;
    netsnmp_request_info  *requests, *request = NULL;
    netsnmp_variable_list *vars,     *var     = NULL;

    struct simple_proxy *sp;
    oid             myname[MAX_OID_LEN];
    size_t          myname_len = MAX_OID_LEN;

    cache = netsnmp_handler_check_cache(cache);

    if (!cache) {
        DEBUGMSGTL(("proxy", "a proxy request was no longer valid.\n"));
        return SNMP_ERR_NOERROR;
    }

    requests = cache->requests;


    sp = (struct simple_proxy *) cache->localinfo;

    if (!sp) {
        DEBUGMSGTL(("proxy", "a proxy request was no longer valid.\n"));
        return SNMP_ERR_NOERROR;
    }

    switch (operation) {
    case NETSNMP_CALLBACK_OP_TIMED_OUT:
        /*
         * WWWXXX: don't leave requests delayed if operation is
         * something like TIMEOUT 
         */
        DEBUGMSGTL(("proxy", "got timed out... requests = %8p\n", requests));

        netsnmp_handler_mark_requests_as_delegated(requests,
                                                   REQUEST_IS_NOT_DELEGATED);
        if(cache->reqinfo->mode != MODE_GETNEXT) {
            DEBUGMSGTL(("proxy", "  ignoring timeout\n"));
            netsnmp_set_request_error(cache->reqinfo, requests, /* XXXWWW: should be index = 0 */
                                      SNMP_ERR_GENERR);
        }
        netsnmp_free_delegated_cache(cache);
        return 0;

    case NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE:
        vars = pdu->variables;

        if (pdu->errstat != SNMP_ERR_NOERROR) {
            /*
             *  If we receive an error from the proxy agent, pass it on up.
             *  The higher-level processing seems to Do The Right Thing.
             *
             * 2005/06 rks: actually, it doesn't do the right thing for
             * a get-next request that returns NOSUCHNAME. If we do nothing,
             * it passes that error back to the comman initiator. What it should
             * do is ignore the error and move on to the next tree. To
             * accomplish that, all we need to do is clear the delegated flag.
             * Not sure if any other error codes need the same treatment. Left
             * as an exercise to the reader...
             */
            DEBUGMSGTL(("proxy", "got error response (%ld)\n", pdu->errstat));
            if((cache->reqinfo->mode == MODE_GETNEXT) &&
               (SNMP_ERR_NOSUCHNAME == pdu->errstat)) {
                DEBUGMSGTL(("proxy", "  ignoring error response\n"));
                netsnmp_handler_mark_requests_as_delegated(requests,
                                                           REQUEST_IS_NOT_DELEGATED);
            }
#ifndef NETSNMP_NO_WRITE_SUPPORT
	    else if (cache->reqinfo->mode == MODE_SET_ACTION) {
		/*
		 * In order for netsnmp_wrap_up_request to consider the
		 * SET request complete,
		 * there must be no delegated requests pending.
		 * https://sourceforge.net/tracker/
		 *	?func=detail&atid=112694&aid=1554261&group_id=12694
		 */
		DEBUGMSGTL(("proxy",
		    "got SET error %s, index %ld\n",
		    snmp_errstring(pdu->errstat), pdu->errindex));
		netsnmp_handler_mark_requests_as_delegated(
		    requests, REQUEST_IS_NOT_DELEGATED);
		netsnmp_request_set_error_idx(requests, pdu->errstat,
                                                        pdu->errindex);
	    }
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
            else {
		netsnmp_handler_mark_requests_as_delegated( requests,
                                             REQUEST_IS_NOT_DELEGATED);
		netsnmp_request_set_error_idx(requests, pdu->errstat,
                                                        pdu->errindex);
            }

        /*
         * update the original request varbinds with the results 
         */
	} else for (var = vars, request = requests;
             request && var;
             request = request->next, var = var->next_variable) {
            /*
             * XXX - should this be done here?
             *       Or wait until we know it's OK?
             */
            snmp_set_var_typed_value(request->requestvb, var->type,
                                     var->val.string, var->val_len);

            DEBUGMSGTL(("proxy", "got response... "));
            DEBUGMSGOID(("proxy", var->name, var->name_length));
            DEBUGMSG(("proxy", "\n"));
            request->delegated = 0;

            /*
             * Check the response oid is legitimate,
             *   and discard the value if not.
             *
             * XXX - what's the difference between these cases?
             */
            if (sp->base_len &&
                (var->name_length < sp->base_len ||
                 snmp_oid_compare(var->name, sp->base_len, sp->base,
                                  sp->base_len) != 0)) {
                DEBUGMSGTL(( "proxy", "out of registered range... "));
                DEBUGMSGOID(("proxy", var->name, sp->base_len));
                DEBUGMSG((   "proxy", " (%d) != ", (int)sp->base_len));
                DEBUGMSGOID(("proxy", sp->base, sp->base_len));
                DEBUGMSG((   "proxy", "\n"));
                snmp_set_var_typed_value(request->requestvb, ASN_NULL, NULL, 0);

                continue;
            } else if (!sp->base_len &&
                       (var->name_length < sp->name_len ||
                        snmp_oid_compare(var->name, sp->name_len, sp->name,
                                         sp->name_len) != 0)) {
                DEBUGMSGTL(( "proxy", "out of registered base range... "));
                DEBUGMSGOID(("proxy", var->name, sp->name_len));
                DEBUGMSG((   "proxy", " (%d) != ", (int)sp->name_len));
                DEBUGMSGOID(("proxy", sp->name, sp->name_len));
                DEBUGMSG((   "proxy", "\n"));
                snmp_set_var_typed_value(request->requestvb, ASN_NULL, NULL, 0);
                continue;
            } else {
                /*
                 * If the returned OID is legitimate, then update
                 *   the original request varbind accordingly.
                 */
                if (sp->base_len) {
                    /*
                     * XXX: oid size maxed? 
                     */
                    memcpy(myname, sp->name, sizeof(oid) * sp->name_len);
                    myname_len =
                        sp->name_len + var->name_length - sp->base_len;
                    if (myname_len > MAX_OID_LEN) {
                        snmp_log(LOG_WARNING,
                                 "proxy OID return length too long.\n");
                        netsnmp_set_request_error(cache->reqinfo, requests,
                                                  SNMP_ERR_GENERR);
                        if (pdu)
                            snmp_free_pdu(pdu);
                        netsnmp_free_delegated_cache(cache);
                        return 1;
                    }

                    if (var->name_length > sp->base_len)
                        memcpy(&myname[sp->name_len],
                               &var->name[sp->base_len],
                               sizeof(oid) * (var->name_length -
                                              sp->base_len));
                    snmp_set_var_objid(request->requestvb, myname,
                                       myname_len);
                } else {
                    snmp_set_var_objid(request->requestvb, var->name,
                                       var->name_length);
                }
            }
        }

        if (request || var) {
            /*
             * ack, this is bad.  The # of varbinds don't match and
             * there is no way to fix the problem 
             */
            if (pdu)
                snmp_free_pdu(pdu);
            snmp_log(LOG_ERR,
                     "response to proxy request illegal.  We're screwed.\n");
            netsnmp_set_request_error(cache->reqinfo, requests,
                                      SNMP_ERR_GENERR);
        }

        /* fix bulk_to_next operations */
        if (cache->reqinfo->mode == MODE_GETBULK)
            netsnmp_bulk_to_next_fix_requests(requests);
        
        /*
         * free the response 
         */
        if (pdu && 0)
            snmp_free_pdu(pdu);
	break;

    default:
        DEBUGMSGTL(("proxy", "no response received: op = %d\n",
                    operation));
	break;
    }

    netsnmp_free_delegated_cache(cache);
    return 1;
}
