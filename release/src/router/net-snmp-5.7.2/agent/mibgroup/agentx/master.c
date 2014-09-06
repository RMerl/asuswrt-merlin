/*
 *  AgentX master agent
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#if HAVE_IO_H
#include <io.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <errno.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#define SNMP_NEED_REQUEST_LIST
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "snmpd.h"
#include "agentx/protocol.h"
#include "agentx/master_admin.h"

netsnmp_feature_require(handler_mark_requests_as_delegated)
netsnmp_feature_require(unix_socket_paths)
netsnmp_feature_require(free_agent_snmp_session_by_session)

void
real_init_master(void)
{
    netsnmp_session sess, *session = NULL;
    char *agentx_sockets;
    char *cp1;

    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE) != MASTER_AGENT)
        return;

    if (netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID,
                              NETSNMP_DS_AGENT_X_SOCKET)) {
       agentx_sockets = strdup(netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID,
                                                     NETSNMP_DS_AGENT_X_SOCKET));
#ifdef NETSNMP_AGENTX_DOM_SOCK_ONLY
       if (agentx_sockets[0] != '/') {
           /* unix:/path */
           if (agentx_sockets[5] != '/') {
               snmp_log(LOG_ERR,
                    "Error: %s transport is not supported, disabling agentx/master.\n", agentx_sockets);
               SNMP_FREE(agentx_sockets);
               return;
           }
       }
#endif
    } else {
        agentx_sockets = strdup("");
    }


    DEBUGMSGTL(("agentx/master", "initializing...\n"));
    snmp_sess_init(&sess);
    sess.version = AGENTX_VERSION_1;
    sess.flags |= SNMP_FLAGS_STREAM_SOCKET;
    sess.timeout = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                      NETSNMP_DS_AGENT_AGENTX_TIMEOUT);
    sess.retries = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                      NETSNMP_DS_AGENT_AGENTX_RETRIES);

#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
    {
	int agentx_dir_perm =
	    netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_X_DIR_PERM);
	if (agentx_dir_perm == 0)
	    agentx_dir_perm = NETSNMP_AGENT_DIRECTORY_MODE;
	netsnmp_unix_create_path_with_mode(agentx_dir_perm);
    }
#endif

    cp1 = agentx_sockets;
    while (cp1) {
        netsnmp_transport *t;
        /*
         *  If the AgentX socket string contains multiple descriptors,
         *  then pick this apart and handle them one by one.
         *
         */
        sess.peername = cp1;
        cp1 = strchr(sess.peername, ',');
        if (cp1 != NULL) {
            *cp1++ = '\0';
	}

        /*
         *  Let 'snmp_open' interpret the descriptor.
         */
        sess.local_port = AGENTX_PORT;      /* Indicate server & set default port */
        sess.remote_port = 0;
        sess.callback = handle_master_agentx_packet;
        errno = 0;
        t = netsnmp_transport_open_server("agentx", sess.peername);
        if (t == NULL) {
            /*
             * diagnose snmp_open errors with the input netsnmp_session
             * pointer.
             */
            char buf[1024];
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                        NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                snprintf(buf, sizeof(buf),
                         "Error: Couldn't open a master agentx socket to "
                         "listen on (%s)", sess.peername);
                snmp_sess_perror(buf, &sess);
                exit(1);
            } else {
                snprintf(buf, sizeof(buf),
                         "Warning: Couldn't open a master agentx socket to "
                         "listen on (%s)", sess.peername);
                netsnmp_sess_log_error(LOG_WARNING, buf, &sess);
            }
        } else {
#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
            if (t->domain == netsnmp_UnixDomain && t->local != NULL) {
                /*
                 * Apply any settings to the ownership/permissions of the
                 * AgentX socket
                 */
                int agentx_sock_perm =
                    netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                       NETSNMP_DS_AGENT_X_SOCK_PERM);
                int agentx_sock_user =
                    netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                       NETSNMP_DS_AGENT_X_SOCK_USER);
                int agentx_sock_group =
                    netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID,
                                       NETSNMP_DS_AGENT_X_SOCK_GROUP);

                char name[sizeof(struct sockaddr_un) + 1];
                memcpy(name, t->local, t->local_length);
                name[t->local_length] = '\0';

                if (agentx_sock_perm != 0)
                    chmod(name, agentx_sock_perm);

                if (agentx_sock_user || agentx_sock_group) {
                    /*
                     * If either of user or group haven't been set,
                     *  then leave them unchanged.
                     */
                    if (agentx_sock_user == 0 )
                        agentx_sock_user = -1;
                    if (agentx_sock_group == 0 )
                        agentx_sock_group = -1;
                    chown(name, agentx_sock_user, agentx_sock_group);
                }
            }
#endif
            session =
                snmp_add_full(&sess, t, NULL, agentx_parse, NULL, NULL,
                              agentx_realloc_build, agentx_check_packet, NULL);
        }
        if (session == NULL) {
            netsnmp_transport_free(t);
        }
    }

#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
    netsnmp_unix_dont_create_path();
#endif

    SNMP_FREE(agentx_sockets);
    DEBUGMSGTL(("agentx/master", "initializing...   DONE\n"));
}

        /*
         * Handle the response from an AgentX subagent,
         *   merging the answers back into the original query
         */
int
agentx_got_response(int operation,
                    netsnmp_session * session,
                    int reqid, netsnmp_pdu *pdu, void *magic)
{
    netsnmp_delegated_cache *cache = (netsnmp_delegated_cache *) magic;
    int             i, ret;
    netsnmp_request_info *requests, *request;
    netsnmp_variable_list *var;
    netsnmp_session *ax_session;

    cache = netsnmp_handler_check_cache(cache);
    if (!cache) {
        DEBUGMSGTL(("agentx/master", "response too late on session %8p\n",
                    session));
        return 0;
    }
    requests = cache->requests;

    switch (operation) {
    case NETSNMP_CALLBACK_OP_TIMED_OUT:{
            void           *s = snmp_sess_pointer(session);
            DEBUGMSGTL(("agentx/master", "timeout on session %8p req=0x%x\n",
                        session, (unsigned)reqid));

            netsnmp_handler_mark_requests_as_delegated(requests,
                                       REQUEST_IS_NOT_DELEGATED);
            netsnmp_set_request_error(cache->reqinfo, requests,
                                      /* XXXWWW: should be index=0 */
                                      SNMP_ERR_GENERR);

            /*
             * This is a bit sledgehammer because the other sessions on this
             * transport may be okay (e.g. some thread in the subagent has
             * wedged, but the others are alright).  OTOH the overwhelming
             * probability is that the whole agent has died somehow.  
             */

            if (s != NULL) {
                netsnmp_transport *t = snmp_sess_transport(s);
                close_agentx_session(session, -1);

                if (t != NULL) {
                    DEBUGMSGTL(("agentx/master", "close transport\n"));
                    t->f_close(t);
                } else {
                    DEBUGMSGTL(("agentx/master", "NULL transport??\n"));
                }
            } else {
                DEBUGMSGTL(("agentx/master", "NULL sess_pointer??\n"));
            }
            ax_session = (netsnmp_session *) cache->localinfo;
            netsnmp_free_agent_snmp_session_by_session(ax_session, NULL);
            netsnmp_free_delegated_cache(cache);
            return 0;
        }

    case NETSNMP_CALLBACK_OP_DISCONNECT:
    case NETSNMP_CALLBACK_OP_SEND_FAILED:
        if (operation == NETSNMP_CALLBACK_OP_DISCONNECT) {
            DEBUGMSGTL(("agentx/master", "disconnect on session %8p\n",
                        session));
        } else {
            DEBUGMSGTL(("agentx/master", "send failed on session %8p\n",
                        session));
        }
        close_agentx_session(session, -1);
        netsnmp_handler_mark_requests_as_delegated(requests,
                                                   REQUEST_IS_NOT_DELEGATED);
        netsnmp_set_request_error(cache->reqinfo, requests,     /* XXXWWW: should be index=0 */
                                  SNMP_ERR_GENERR);
        netsnmp_free_delegated_cache(cache);
        return 0;

    case NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE:
        /*
         * This session is alive 
         */
        CLEAR_SNMP_STRIKE_FLAGS(session->flags);
        break;
    default:
        snmp_log(LOG_ERR, "Unknown operation %d in agentx_got_response\n",
                 operation);
        netsnmp_free_delegated_cache(cache);
        return 0;
    }

    DEBUGMSGTL(("agentx/master", "got response errstat=%ld, (req=0x%x,trans="
                "0x%x,sess=0x%x)\n",
                pdu->errstat, (unsigned)pdu->reqid, (unsigned)pdu->transid,
		(unsigned)pdu->sessid));

    if (pdu->errstat != AGENTX_ERR_NOERROR) {
        /* [RFC 2471 - 7.2.5.2.]
         *
         *   1) For any received AgentX response PDU, if res.error is
         *      not `noError', the SNMP response PDU's error code is
         *      set to this value.  If res.error contains an AgentX
         *      specific value (e.g.  `parseError'), the SNMP response
         *      PDU's error code is set to a value of genErr instead.
         *      Also, the SNMP response PDU's error index is set to
         *      the index of the variable binding corresponding to the
         *      failed VarBind in the subagent's AgentX response PDU.
         *
         *      All other AgentX response PDUs received due to
         *      processing this SNMP request are ignored.  Processing
         *      is complete; the SNMP Response PDU is ready to be sent
         *      (see section 7.2.6, "Sending the SNMP Response-PDU").
         */
        int err;

        DEBUGMSGTL(("agentx/master",
                    "agentx_got_response() error branch\n"));

        switch (pdu->errstat) {
        case AGENTX_ERR_PARSE_FAILED:
        case AGENTX_ERR_REQUEST_DENIED:
        case AGENTX_ERR_PROCESSING_ERROR:
            err = SNMP_ERR_GENERR;
            break;
        default:
            err = pdu->errstat;
        }

        ret = 0;
        for (request = requests, i = 1; request;
             request = request->next, i++) {
            if (i == pdu->errindex) {
                /*
                 * Mark this varbind as the one generating the error.
                 * Note that the AgentX errindex may not match the
                 * position in the original SNMP PDU (request->index)
                 */
                netsnmp_set_request_error(cache->reqinfo, request,
                                          err);
                ret = 1;
            }
            request->delegated = REQUEST_IS_NOT_DELEGATED;
        }
        if (!ret) {
            /*
             * ack, unknown, mark the first one
             */
            netsnmp_set_request_error(cache->reqinfo, requests,
                                      SNMP_ERR_GENERR);
        }
        netsnmp_free_delegated_cache(cache);
        DEBUGMSGTL(("agentx/master", "end error branch\n"));
        return 1;
    } else if (cache->reqinfo->mode == MODE_GET ||
               cache->reqinfo->mode == MODE_GETNEXT ||
               cache->reqinfo->mode == MODE_GETBULK) {
        /*
         * Replace varbinds for data request types, but not SETs.  
         */
        DEBUGMSGTL(("agentx/master",
                    "agentx_got_response() beginning...\n"));
        for (var = pdu->variables, request = requests; request && var;
             request = request->next, var = var->next_variable) {
            /*
             * Otherwise, process successful requests
             */
            DEBUGMSGTL(("agentx/master",
                        "  handle_agentx_response: processing: "));
            DEBUGMSGOID(("agentx/master", var->name, var->name_length));
            DEBUGMSG(("agentx/master", "\n"));
            if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_VERBOSE)) {
                DEBUGMSGTL(("agentx/master", "    >> "));
                DEBUGMSGVAR(("agentx/master", var));
                DEBUGMSG(("agentx/master", "\n"));
            }

            /*
             * update the oid in the original request 
             */
            if (var->type != SNMP_ENDOFMIBVIEW) {
                snmp_set_var_typed_value(request->requestvb, var->type,
                                         var->val.string, var->val_len);
                snmp_set_var_objid(request->requestvb, var->name,
                                   var->name_length);
            }
            request->delegated = REQUEST_IS_NOT_DELEGATED;
        }

        if (request || var) {
            /*
             * ack, this is bad.  The # of varbinds don't match and
             * there is no way to fix the problem 
             */
            snmp_log(LOG_ERR,
                     "response to agentx request illegal.  bailing out.\n");
            netsnmp_set_request_error(cache->reqinfo, requests,
                                      SNMP_ERR_GENERR);
        }

        if (cache->reqinfo->mode == MODE_GETBULK)
            netsnmp_bulk_to_next_fix_requests(requests);
    } else {
        /*
         * mark set requests as handled 
         */
        for (request = requests; request; request = request->next) {
            request->delegated = REQUEST_IS_NOT_DELEGATED;
        }
    }
    DEBUGMSGTL(("agentx/master",
                "handle_agentx_response() finishing...\n"));
    netsnmp_free_delegated_cache(cache);
    return 1;
}

/*
 *
 * AgentX State diagram.  [mode] = internal mode it's mapped from:
 *
 * TESTSET -success-> COMMIT -success-> CLEANUP
 * [RESERVE1]         [ACTION]          [COMMIT]
 *    |                 |
 *    |                 \--failure-> UNDO
 *    |                              [UNDO]
 *    |
 *     --failure-> CLEANUP
 *                 [FREE]
 */
int
agentx_master_handler(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests)
{
    netsnmp_session *ax_session = (netsnmp_session *) handler->myvoid;
    netsnmp_request_info *request = requests;
    netsnmp_pdu    *pdu;
    void           *cb_data;
    int             result;

    DEBUGMSGTL(("agentx/master",
                "agentx master handler starting, mode = 0x%02x\n",
                reqinfo->mode));

    if (!ax_session) {
        netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
        return SNMP_ERR_NOERROR;
    }        

    /*
     * build a new pdu based on the pdu type coming in 
     */
    switch (reqinfo->mode) {
    case MODE_GET:
        pdu = snmp_pdu_create(AGENTX_MSG_GET);
        break;

    case MODE_GETNEXT:
        pdu = snmp_pdu_create(AGENTX_MSG_GETNEXT);
        break;

    case MODE_GETBULK:         /* WWWXXX */
        pdu = snmp_pdu_create(AGENTX_MSG_GETNEXT);
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        pdu = snmp_pdu_create(AGENTX_MSG_TESTSET);
        break;

    case MODE_SET_RESERVE2:
        /*
         * don't do anything here for AgentX.  Assume all is fine
         * and go on since AgentX only has one test phase. 
         */
        return SNMP_ERR_NOERROR;

    case MODE_SET_ACTION:
        pdu = snmp_pdu_create(AGENTX_MSG_COMMITSET);
        break;

    case MODE_SET_UNDO:
        pdu = snmp_pdu_create(AGENTX_MSG_UNDOSET);
        break;

    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
        pdu = snmp_pdu_create(AGENTX_MSG_CLEANUPSET);
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

    default:
        snmp_log(LOG_WARNING,
                 "unsupported mode for agentx/master called\n");
        return SNMP_ERR_NOERROR;
    }

    if (!pdu) {
        netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_GENERR);
        return SNMP_ERR_NOERROR;
    }

    pdu->version = AGENTX_VERSION_1;
    pdu->reqid = snmp_get_next_transid();
    pdu->transid = reqinfo->asp->pdu->transid;
    pdu->sessid = ax_session->subsession->sessid;
    if (reginfo->contextName) {
        pdu->community = (u_char *) strdup(reginfo->contextName);
        pdu->community_len = strlen(reginfo->contextName);
        pdu->flags |= AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT;
    }
    if (ax_session->subsession->flags & AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER)
        pdu->flags |= AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER;

    while (request) {

        size_t nlen = request->requestvb->name_length;
        oid   *nptr = request->requestvb->name;
        
        DEBUGMSGTL(("agentx/master","request for variable ("));
        DEBUGMSGOID(("agentx/master", nptr, nlen));
        DEBUGMSG(("agentx/master", ")\n"));
        
        /*
         * loop through all the requests and create agentx ones out of them 
         */

        if (reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK) {

            if (snmp_oid_compare(nptr, nlen, request->subtree->start_a,
                                 request->subtree->start_len) < 0) {
                DEBUGMSGTL(("agentx/master","inexact request preceeding region ("));
                DEBUGMSGOID(("agentx/master", request->subtree->start_a,
                             request->subtree->start_len));
                DEBUGMSG(("agentx/master", ")\n"));
                nptr = request->subtree->start_a;
                nlen = request->subtree->start_len;
                request->inclusive = 1;
            }

            if (request->inclusive) {
                DEBUGMSGTL(("agentx/master", "INCLUSIVE varbind "));
                DEBUGMSGOID(("agentx/master", nptr, nlen));
                DEBUGMSG(("agentx/master", " scoped to "));
                DEBUGMSGOID(("agentx/master", request->range_end,
                             request->range_end_len));
                DEBUGMSG(("agentx/master", "\n"));
                snmp_pdu_add_variable(pdu, nptr, nlen, ASN_PRIV_INCL_RANGE,
                                      (u_char *) request->range_end,
                                      request->range_end_len *
                                      sizeof(oid));
                request->inclusive = 0;
            } else {
                DEBUGMSGTL(("agentx/master", "EXCLUSIVE varbind "));
                DEBUGMSGOID(("agentx/master", nptr, nlen));
                DEBUGMSG(("agentx/master", " scoped to "));
                DEBUGMSGOID(("agentx/master", request->range_end,
                             request->range_end_len));
                DEBUGMSG(("agentx/master", "\n"));
                snmp_pdu_add_variable(pdu, nptr, nlen, ASN_PRIV_EXCL_RANGE,
                                      (u_char *) request->range_end,
                                      request->range_end_len *
                                      sizeof(oid));
            }
        } else {
            snmp_pdu_add_variable(pdu, request->requestvb->name,
                                  request->requestvb->name_length,
                                  request->requestvb->type,
                                  request->requestvb->val.string,
                                  request->requestvb->val_len);
        }

        /*
         * mark the request as delayed 
         */
        if (pdu->command != AGENTX_MSG_CLEANUPSET)
            request->delegated = REQUEST_IS_DELEGATED;
        else
            request->delegated = REQUEST_IS_NOT_DELEGATED;

        /*
         * next... 
         */
        request = request->next;
    }

    /*
     * When the master sends a CleanupSet PDU, it will never get a response
     * back from the subagent. So we shouldn't allocate the
     * netsnmp_delegated_cache structure in this case.
     */
    if (pdu->command != AGENTX_MSG_CLEANUPSET)
        cb_data = netsnmp_create_delegated_cache(handler, reginfo,
                                                 reqinfo, requests,
                                                 (void *) ax_session);
    else
        cb_data = NULL;

    /*
     * send the requests out.
     */
    DEBUGMSGTL(("agentx/master", "sending pdu (req=0x%x,trans=0x%x,sess=0x%x)\n",
                (unsigned)pdu->reqid, (unsigned)pdu->transid, (unsigned)pdu->sessid));
    result = snmp_async_send(ax_session, pdu, agentx_got_response, cb_data);
    if (result == 0) {
        snmp_free_pdu(pdu);
    }

    return SNMP_ERR_NOERROR;
}
