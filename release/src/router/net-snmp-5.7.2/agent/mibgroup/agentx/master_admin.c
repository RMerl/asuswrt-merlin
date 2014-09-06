/*
 *  AgentX Administrative request handling
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <sys/types.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
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
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "agentx/protocol.h"
#include "agentx/client.h"

#include <net-snmp/agent/agent_index.h>
#include <net-snmp/agent/agent_trap.h>
#include <net-snmp/agent/agent_callbacks.h>
#include <net-snmp/agent/agent_sysORTable.h>
#include "master.h"

netsnmp_feature_require(unregister_mib_table_row)
netsnmp_feature_require(trap_vars_with_context)
netsnmp_feature_require(calculate_sectime_diff)
netsnmp_feature_require(allocate_globalcacheid)
netsnmp_feature_require(remove_index)

netsnmp_session *
find_agentx_session(netsnmp_session * session, int sessid)
{
    netsnmp_session *sp;
    for (sp = session->subsession; sp != NULL; sp = sp->next) {
        if (sp->sessid == sessid)
            return sp;
    }
    return NULL;
}


int
open_agentx_session(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;

    DEBUGMSGTL(("agentx/master", "open %8p\n", session));
    sp = (netsnmp_session *) malloc(sizeof(netsnmp_session));
    if (sp == NULL) {
        session->s_snmp_errno = AGENTX_ERR_OPEN_FAILED;
        return -1;
    }

    memcpy(sp, session, sizeof(netsnmp_session));
    sp->sessid = snmp_get_next_sessid();
    sp->version = pdu->version;
    sp->timeout = pdu->time;

    /*
     * Be careful with fields: if these aren't zeroed, they will get free()d
     * more than once when the session is closed -- once in the main session,
     * and once in each subsession.  Basically, if it's not being used for
     * some AgentX-specific purpose, it ought to be zeroed here. 
     */

    sp->community = NULL;
    sp->peername = NULL;
    sp->contextEngineID = NULL;
    sp->contextName = NULL;
    sp->securityEngineID = NULL;
    sp->securityPrivProto = NULL;

    /*
     * This next bit utilises unused SNMPv3 fields
     *   to store the subagent OID and description.
     * This really ought to use AgentX-specific fields,
     *   but it hardly seems worth it for a one-off use.
     *
     * But I'm willing to be persuaded otherwise....  */
    sp->securityAuthProto = snmp_duplicate_objid(pdu->variables->name,
                                                 pdu->variables->
                                                 name_length);
    sp->securityAuthProtoLen = pdu->variables->name_length;
    sp->securityName = strdup((char *) pdu->variables->val.string);
    sp->engineTime = (uint32_t)((netsnmp_get_agent_runtime() + 50) / 100) & 0x7fffffffL;

    sp->subsession = session;   /* link back to head */
    sp->flags |= SNMP_FLAGS_SUBSESSION;
    sp->flags &= ~AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER;
    sp->flags |= (pdu->flags & AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER);
    sp->next = session->subsession;
    session->subsession = sp;
    DEBUGMSGTL(("agentx/master", "opened %8p = %ld with flags = %02lx\n",
                sp, sp->sessid, sp->flags & AGENTX_MSG_FLAGS_MASK));

    return sp->sessid;
}

int
close_agentx_session(netsnmp_session * session, int sessid)
{
    netsnmp_session *sp, **prevNext;

    if (!session)
        return AGENTX_ERR_NOT_OPEN;

    DEBUGMSGTL(("agentx/master", "close %8p, %d\n", session, sessid));
    if (sessid == -1) {
        /*
         * The following is necessary to avoid locking up the agent when
         * a sugagent dies during a set request. We must clean up the
         * requests, so that the delegated request will be completed and
         * further requests can be processed
         */
        netsnmp_remove_delegated_requests_for_session(session);
        if (session->subsession != NULL) {
            netsnmp_session *subsession = session->subsession;
            for(; subsession; subsession = subsession->next) {
                netsnmp_remove_delegated_requests_for_session(subsession);
            }
        }
                
        unregister_mibs_by_session(session);
        unregister_index_by_session(session);
        unregister_sysORTable_by_session(session);
	SNMP_FREE(session->myvoid);
        return AGENTX_ERR_NOERROR;
    }

    prevNext = &(session->subsession);

    for (sp = session->subsession; sp != NULL; sp = sp->next) {

        if (sp->sessid == sessid) {
            unregister_mibs_by_session(sp);
            unregister_index_by_session(sp);
            unregister_sysORTable_by_session(sp);

            *prevNext = sp->next;

            if (sp->securityAuthProto != NULL) {
                free(sp->securityAuthProto);
            }
            if (sp->securityName != NULL) {
                free(sp->securityName);
            }
            free(sp);
            sp = NULL;

            DEBUGMSGTL(("agentx/master", "closed %8p, %d okay\n",
                        session, sessid));
            return AGENTX_ERR_NOERROR;
        }

        prevNext = &(sp->next);
    }

    DEBUGMSGTL(("agentx/master", "sessid %d not found\n", sessid));
    return AGENTX_ERR_NOT_OPEN;
}

int
register_agentx_list(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;
    char            buf[128];
    oid             ubound = 0;
    u_long          flags = 0;
    netsnmp_handler_registration *reg;
    int             rc = 0;
    int             cacheid;

    DEBUGMSGTL(("agentx/master", "in register_agentx_list\n"));

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    sprintf(buf, "AgentX subagent %ld, session %8p, subsession %8p",
            sp->sessid, session, sp);
    /*
     * * TODO: registration timeout
     * *   registration context
     */
    if (pdu->range_subid) {
        ubound = pdu->variables->val.objid[pdu->range_subid - 1];
    }

    if (pdu->flags & AGENTX_MSG_FLAG_INSTANCE_REGISTER) {
        flags = FULLY_QUALIFIED_INSTANCE;
    }

    reg = netsnmp_create_handler_registration(buf, agentx_master_handler, pdu->variables->name, pdu->variables->name_length, HANDLER_CAN_RWRITE | HANDLER_CAN_GETBULK); /* fake it */
    if (!session->myvoid) {
        session->myvoid = malloc(sizeof(cacheid));
        cacheid = netsnmp_allocate_globalcacheid();
        *((int *) session->myvoid) = cacheid;
    } else {
        cacheid = *((int *) session->myvoid);
    }

    reg->handler->myvoid = session;
    reg->global_cacheid = cacheid;
    if (NULL != pdu->community)
        reg->contextName = strdup((char *)pdu->community);

    /*
     * register mib. Note that for failure cases, the registration info
     * (reg) will be freed, and thus is no longer a valid pointer.
     */
    switch (netsnmp_register_mib(buf, NULL, 0, 1,
                                 pdu->variables->name,
                                 pdu->variables->name_length,
                                 pdu->priority, pdu->range_subid, ubound,
                                 sp, (char *) pdu->community, pdu->time,
                                 flags, reg, 1)) {

    case MIB_REGISTERED_OK:
        DEBUGMSGTL(("agentx/master", "registered ok\n"));
        return AGENTX_ERR_NOERROR;

    case MIB_DUPLICATE_REGISTRATION:
        DEBUGMSGTL(("agentx/master", "duplicate registration\n"));
        rc = AGENTX_ERR_DUPLICATE_REGISTRATION;
        break;

    case MIB_REGISTRATION_FAILED:
    default:
        rc = AGENTX_ERR_REQUEST_DENIED;
        DEBUGMSGTL(("agentx/master", "failed registration\n"));
    }
    return rc;
}

int
unregister_agentx_list(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;
    int             rc = 0;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL) {
        return AGENTX_ERR_NOT_OPEN;
    }

    if (pdu->range_subid != 0) {
        oid             ubound =
            pdu->variables->val.objid[pdu->range_subid - 1];
        rc = netsnmp_unregister_mib_table_row(pdu->variables->name,
                                              pdu->variables->name_length,
                                              pdu->priority,
                                              pdu->range_subid, ubound,
                                              (char *) pdu->community);
    } else {
        rc = unregister_mib_context(pdu->variables->name,
                                    pdu->variables->name_length,
                                    pdu->priority, 0, 0,
                                    (char *) pdu->community);
    }

    switch (rc) {
    case MIB_UNREGISTERED_OK:
        return AGENTX_ERR_NOERROR;
    case MIB_NO_SUCH_REGISTRATION:
        return AGENTX_ERR_UNKNOWN_REGISTRATION;
    case MIB_UNREGISTRATION_FAILED:
    default:
        return AGENTX_ERR_REQUEST_DENIED;
    }
}

int
allocate_idx_list(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;
    netsnmp_variable_list *vp, *vp2, *next, *res;
    int             flags = 0;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    if (pdu->flags & AGENTX_MSG_FLAG_ANY_INSTANCE)
        flags |= ALLOCATE_ANY_INDEX;
    if (pdu->flags & AGENTX_MSG_FLAG_NEW_INSTANCE)
        flags |= ALLOCATE_NEW_INDEX;

    /*
     * XXX - what about errors?
     *
     *  If any allocations fail, then we need to
     *    *fully* release the earlier ones.
     *  (i.e. remove them completely from the index registry,
     *    not simply mark them as available for re-use)
     *
     * For now - assume they all succeed.
     */
    for (vp = pdu->variables; vp != NULL; vp = next) {
        next = vp->next_variable;
        res = register_index(vp, flags, session);
        if (res == NULL) {
            /*
             *  If any allocations fail, we need to *fully* release
             *      all previous ones (i.e. remove them completely
             *      from the index registry)
             */
            for (vp2 = pdu->variables; vp2 != vp; vp2 = vp2->next_variable) {
                remove_index(vp2, session);
            }
            return AGENTX_ERR_INDEX_NONE_AVAILABLE;     /* XXX */
        } else {
            (void) snmp_clone_var(res, vp);
            free(res);
        }
        vp->next_variable = next;
    }
    return AGENTX_ERR_NOERROR;
}

int
release_idx_list(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;
    netsnmp_variable_list *vp, *vp2, *rv = NULL;
    int             res;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    for (vp = pdu->variables; vp != NULL; vp = vp->next_variable) {
        res = unregister_index(vp, TRUE, session);
        /*
         *  If any releases fail,
         *      we need to reinstate all previous ones.
         */
        if (res != SNMP_ERR_NOERROR) {
            for (vp2 = pdu->variables; vp2 != vp; vp2 = vp2->next_variable) {
                rv = register_index(vp2, ALLOCATE_THIS_INDEX, session);
                free(rv);
            }
            return AGENTX_ERR_INDEX_NOT_ALLOCATED;      /* Probably */
        }
    }
    return AGENTX_ERR_NOERROR;
}

int
add_agent_caps_list(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;
    char* description;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    description = netsnmp_strdup_and_null(pdu->variables->val.string,
                                          pdu->variables->val_len);
    register_sysORTable_sess(pdu->variables->name, pdu->variables->name_length,
                             description, sp);
    free(description);
    return AGENTX_ERR_NOERROR;
}

int
remove_agent_caps_list(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;
    int rc;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    rc = unregister_sysORTable_sess(pdu->variables->name,
                                    pdu->variables->name_length, sp);

    if (rc < 0)
      return AGENTX_ERR_UNKNOWN_AGENTCAPS;

    return AGENTX_ERR_NOERROR;
}

int
agentx_notify(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session       *sp;
    netsnmp_variable_list *var;
    extern const oid       sysuptime_oid[], snmptrap_oid[];
    extern const size_t    sysuptime_oid_len, snmptrap_oid_len;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;

    var = pdu->variables;
    if (!var)
        return AGENTX_ERR_PROCESSING_ERROR;

    if (snmp_oid_compare(var->name, var->name_length,
                         sysuptime_oid, sysuptime_oid_len) == 0) {
        var = var->next_variable;
    }

    if (!var || snmp_oid_compare(var->name, var->name_length,
                                 snmptrap_oid, snmptrap_oid_len) != 0)
        return AGENTX_ERR_PROCESSING_ERROR;

    /*
     *  If sysUptime isn't the first varbind, don't worry.  
     *     send_trap_vars() will add it if necessary.
     *
     *  Note that if this behaviour is altered, it will
     *     be necessary to add sysUptime here,
     *     as this is valid AgentX syntax.
     */

	/* If a context name was specified, send the trap using that context.
	 * Otherwise, send the trap without the context using the old method */
	if (pdu->contextName != NULL)
	{
        send_trap_vars_with_context(-1, -1, pdu->variables, 
                       pdu->contextName);
	}
	else
	{
        send_trap_vars(-1, -1, pdu->variables);
	}

    return AGENTX_ERR_NOERROR;
}


int
agentx_ping_response(netsnmp_session * session, netsnmp_pdu *pdu)
{
    netsnmp_session *sp;

    sp = find_agentx_session(session, pdu->sessid);
    if (sp == NULL)
        return AGENTX_ERR_NOT_OPEN;
    else
        return AGENTX_ERR_NOERROR;
}

int
handle_master_agentx_packet(int operation,
                            netsnmp_session * session,
                            int reqid, netsnmp_pdu *pdu, void *magic)
{
    netsnmp_agent_session *asp;

    if (operation == NETSNMP_CALLBACK_OP_DISCONNECT) {
        DEBUGMSGTL(("agentx/master",
                    "transport disconnect on session %8p\n", session));
        /*
         * Shut this session down gracefully.  
         */
        close_agentx_session(session, -1);
        return 1;
    } else if (operation == NETSNMP_CALLBACK_OP_CONNECT) {
        DEBUGMSGTL(("agentx/master",
                    "transport connect on session %8p\n", session));
        return 1;
    } else if (operation != NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        DEBUGMSGTL(("agentx/master", "unexpected callback op %d\n",
                    operation));
        return 1;
    }

    /*
     * Okay, it's a NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE op.  
     */

    if (magic) {
        asp = (netsnmp_agent_session *) magic;
    } else {
        asp = init_agent_snmp_session(session, pdu);
    }

    DEBUGMSGTL(("agentx/master", "handle pdu (req=0x%lx,trans=0x%lx,sess=0x%lx)\n",
                (unsigned long)pdu->reqid, (unsigned long)pdu->transid,
		(unsigned long)pdu->sessid));
    
    switch (pdu->command) {
    case AGENTX_MSG_OPEN:
        asp->pdu->sessid = open_agentx_session(session, pdu);
        if (asp->pdu->sessid == -1)
            asp->status = session->s_snmp_errno;
        break;

    case AGENTX_MSG_CLOSE:
        asp->status = close_agentx_session(session, pdu->sessid);
        break;

    case AGENTX_MSG_REGISTER:
        asp->status = register_agentx_list(session, pdu);
        break;

    case AGENTX_MSG_UNREGISTER:
        asp->status = unregister_agentx_list(session, pdu);
        break;

    case AGENTX_MSG_INDEX_ALLOCATE:
        asp->status = allocate_idx_list(session, asp->pdu);
        if (asp->status != AGENTX_ERR_NOERROR) {
            snmp_free_pdu(asp->pdu);
            asp->pdu = snmp_clone_pdu(pdu);
        }
        break;

    case AGENTX_MSG_INDEX_DEALLOCATE:
        asp->status = release_idx_list(session, pdu);
        break;

    case AGENTX_MSG_ADD_AGENT_CAPS:
        asp->status = add_agent_caps_list(session, pdu);
        break;

    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        asp->status = remove_agent_caps_list(session, pdu);
        break;

    case AGENTX_MSG_NOTIFY:
        asp->status = agentx_notify(session, pdu);
        break;

    case AGENTX_MSG_PING:
        asp->status = agentx_ping_response(session, pdu);
        break;

        /*
         * TODO: Other admin packets 
         */

    case AGENTX_MSG_GET:
    case AGENTX_MSG_GETNEXT:
    case AGENTX_MSG_GETBULK:
    case AGENTX_MSG_TESTSET:
    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_CLEANUPSET:
    case AGENTX_MSG_RESPONSE:
        /*
         * Shouldn't be handled here 
         */
        break;

    default:
        asp->status = AGENTX_ERR_PARSE_FAILED;
        break;
    }

    asp->pdu->time = netsnmp_get_agent_uptime();
    asp->pdu->command = AGENTX_MSG_RESPONSE;
    asp->pdu->errstat = asp->status;
    DEBUGMSGTL(("agentx/master", "send response, stat %d (req=0x%lx,trans="
                "0x%lx,sess=0x%lx)\n",
                asp->status, (unsigned long)pdu->reqid,
		(unsigned long)pdu->transid, (unsigned long)pdu->sessid));
    if (!snmp_send(asp->session, asp->pdu)) {
        char           *eb = NULL;
        int             pe, pse;
        snmp_error(asp->session, &pe, &pse, &eb);
        snmp_free_pdu(asp->pdu);
        DEBUGMSGTL(("agentx/master", "FAILED %d %d %s\n", pe, pse, eb));
        free(eb);
    }
    asp->pdu = NULL;
    free_agent_snmp_session(asp);

    return 1;
}
