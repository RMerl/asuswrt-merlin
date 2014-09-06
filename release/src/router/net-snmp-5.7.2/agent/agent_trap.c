/*
 * agent_trap.c
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
/** @defgroup agent_trap Trap generation routines for mib modules to use
 *  @ingroup agent
 *
 * @{
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
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
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <net-snmp/utilities.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_trap.h>
#include <net-snmp/agent/snmp_agent.h>
#include <net-snmp/agent/agent_callbacks.h>

#include <net-snmp/agent/agent_module_config.h>
#include <net-snmp/agent/mib_module_config.h>

#ifdef USING_AGENTX_PROTOCOL_MODULE
#include "agentx/protocol.h"
#endif

netsnmp_feature_child_of(agent_trap_all, libnetsnmpagent)

netsnmp_feature_child_of(trap_vars_with_context, agent_trap_all)
netsnmp_feature_child_of(remove_trap_session, agent_trap_all)

netsnmp_feature_child_of(send_v3trap,netsnmp_unused)
netsnmp_feature_child_of(send_trap_pdu,netsnmp_unused)

struct trap_sink {
    netsnmp_session *sesp;
    struct trap_sink *next;
    int             pdutype;
    int             version;
};

struct trap_sink *sinks = NULL;

const oid       objid_enterprisetrap[] = { NETSNMP_NOTIFICATION_MIB };
const oid       trap_version_id[] = { NETSNMP_SYSTEM_MIB };
const int       enterprisetrap_len = OID_LENGTH(objid_enterprisetrap);
const int       trap_version_id_len = OID_LENGTH(trap_version_id);

#define SNMPV2_TRAPS_PREFIX	SNMP_OID_SNMPMODULES,1,1,5
const oid       trap_prefix[]    = { SNMPV2_TRAPS_PREFIX };
const oid       cold_start_oid[] = { SNMPV2_TRAPS_PREFIX, 1 };  /* SNMPv2-MIB */

#define SNMPV2_TRAP_OBJS_PREFIX	SNMP_OID_SNMPMODULES,1,1,4
const oid       snmptrap_oid[] = { SNMPV2_TRAP_OBJS_PREFIX, 1, 0 };
const oid       snmptrapenterprise_oid[] = { SNMPV2_TRAP_OBJS_PREFIX, 3, 0 };
const oid       sysuptime_oid[] = { SNMP_OID_MIB2, 1, 3, 0 };
const size_t    snmptrap_oid_len = OID_LENGTH(snmptrap_oid);
const size_t    snmptrapenterprise_oid_len = OID_LENGTH(snmptrapenterprise_oid);
const size_t    sysuptime_oid_len = OID_LENGTH(sysuptime_oid);

#define SNMPV2_COMM_OBJS_PREFIX	SNMP_OID_SNMPMODULES,18,1
const oid       agentaddr_oid[] = { SNMPV2_COMM_OBJS_PREFIX, 3, 0 };
const size_t    agentaddr_oid_len = OID_LENGTH(agentaddr_oid);
const oid       community_oid[] = { SNMPV2_COMM_OBJS_PREFIX, 4, 0 };
const size_t    community_oid_len = OID_LENGTH(community_oid);
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
char           *snmp_trapcommunity = NULL;
#endif


#define SNMP_AUTHENTICATED_TRAPS_ENABLED	1
#define SNMP_AUTHENTICATED_TRAPS_DISABLED	2

long            snmp_enableauthentraps = SNMP_AUTHENTICATED_TRAPS_DISABLED;
int             snmp_enableauthentrapsset = 0;

/*
 * Prototypes 
 */
 /*
  * static int create_v1_trap_session (const char *, u_short, const char *);
  * static int create_v2_trap_session (const char *, u_short, const char *);
  * static int create_v2_inform_session (const char *, u_short, const char *);
  * static void free_trap_session (struct trap_sink *sp);
  * static void send_v1_trap (netsnmp_session *, int, int);
  * static void send_v2_trap (netsnmp_session *, int, int, int);
  */


        /*******************
	 *
	 * Trap session handling
	 *
	 *******************/

void
init_traps(void)
{
}

static void
free_trap_session(struct trap_sink *sp)
{
    DEBUGMSGTL(("trap", "freeing callback trap session (%p, %p)\n", sp, sp->sesp));
    snmp_close(sp->sesp);
    free(sp);
}

int
add_trap_session(netsnmp_session * ss, int pdutype, int confirm,
                 int version)
{
    if (snmp_callback_available(SNMP_CALLBACK_APPLICATION,
                                SNMPD_CALLBACK_REGISTER_NOTIFICATIONS) ==
        SNMPERR_SUCCESS) {
        /*
         * something else wants to handle notification registrations 
         */
        struct agent_add_trap_args args;
        DEBUGMSGTL(("trap", "adding callback trap sink (%p)\n", ss));
        args.ss = ss;
        args.confirm = confirm;
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                            SNMPD_CALLBACK_REGISTER_NOTIFICATIONS,
                            (void *) &args);
    } else {
        /*
         * no other support exists, handle it ourselves. 
         */
        struct trap_sink *new_sink;

        DEBUGMSGTL(("trap", "adding internal trap sink\n"));
        new_sink = (struct trap_sink *) malloc(sizeof(*new_sink));
        if (new_sink == NULL)
            return 0;

        new_sink->sesp = ss;
        new_sink->pdutype = pdutype;
        new_sink->version = version;
        new_sink->next = sinks;
        sinks = new_sink;
    }
    return 1;
}

#ifndef NETSNMP_FEATURE_REMOVE_REMOVE_TRAP_SESSION
int
remove_trap_session(netsnmp_session * ss)
{
    struct trap_sink *sp = sinks, *prev = NULL;

    DEBUGMSGTL(("trap", "removing trap sessions\n"));
    while (sp) {
        if (sp->sesp == ss) {
            if (prev) {
                prev->next = sp->next;
            } else {
                sinks = sp->next;
            }
            /*
             * I don't believe you *really* want to close the session here;
             * it may still be in use for other purposes.  In particular this
             * is awkward for AgentX, since we want to call this function
             * from the session's callback.  Let's just free the trapsink
             * data structure.  [jbpn]  
             */
            /*
             * free_trap_session(sp);  
             */
            DEBUGMSGTL(("trap", "removing trap session (%p, %p)\n", sp, sp->sesp));
            free(sp);
            return 1;
        }
        prev = sp;
        sp = sp->next;
    }
    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_REMOVE_TRAP_SESSION */

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
static int
create_trap_session2(const char *sink, const char* sinkport,
		     char *com, int version, int pdutype)
{
    netsnmp_transport *t;
    netsnmp_session session, *sesp;

    memset(&session, 0, sizeof(netsnmp_session));
    session.version = version;
    if (com) {
        session.community = (u_char *) com;
        session.community_len = strlen(com);
    }

    /*
     * for informs, set retries to default
     */
    if (SNMP_MSG_INFORM == pdutype) {
        session.timeout = SNMP_DEFAULT_TIMEOUT;
        session.retries = SNMP_DEFAULT_RETRIES;
    }

    /*
     * if the sink is localhost, bind to localhost, to reduce open ports.
     */
    if ((NULL == netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                       NETSNMP_DS_LIB_CLIENT_ADDR)) && 
        ((0 == strcmp("localhost",sink)) || (0 == strcmp("127.0.0.1",sink))))
        session.localname = strdup("localhost");

    t = netsnmp_tdomain_transport_full("snmptrap", sink, 0, NULL, sinkport);
    if (t != NULL) {
	sesp = snmp_add(&session, t, NULL, NULL);

	if (sesp) {
	    return add_trap_session(sesp, pdutype,
				    (pdutype == SNMP_MSG_INFORM), version);
	}
    }
    /*
     * diagnose snmp_open errors with the input netsnmp_session pointer 
     */
    snmp_sess_perror("snmpd: create_trap_session", &session);
    return 0;
}

int
create_trap_session(char *sink, u_short sinkport,
		    char *com, int version, int pdutype)
{
    char buf[sizeof(sinkport) * 3 + 2];
    if (sinkport != 0) {
	sprintf(buf, ":%hu", sinkport);
	snmp_log(LOG_NOTICE,
		 "Using a separate port number is deprecated, please correct "
		 "the sink specification instead");
    }
    return create_trap_session2(sink, sinkport ? buf : NULL, com, version,
				pdutype);
}

#endif /* support for community based SNMP */

#ifndef NETSNMP_DISABLE_SNMPV1
static int
create_v1_trap_session(char *sink, const char *sinkport, char *com)
{
    return create_trap_session2(sink, sinkport, com,
				SNMP_VERSION_1, SNMP_MSG_TRAP);
}
#endif

#ifndef NETSNMP_DISABLE_SNMPV2C
static int
create_v2_trap_session(const char *sink, const char *sinkport, char *com)
{
    return create_trap_session2(sink, sinkport, com,
				SNMP_VERSION_2c, SNMP_MSG_TRAP2);
}

static int
create_v2_inform_session(const char *sink, const char *sinkport, char *com)
{
    return create_trap_session2(sink, sinkport, com,
				SNMP_VERSION_2c, SNMP_MSG_INFORM);
}
#endif

void
snmpd_free_trapsinks(void)
{
    struct trap_sink *sp = sinks;
    DEBUGMSGTL(("trap", "freeing trap sessions\n"));
    while (sp) {
        sinks = sinks->next;
        free_trap_session(sp);
        sp = sinks;
    }
}

        /*******************
	 *
	 * Trap handling
	 *
	 *******************/


netsnmp_pdu*
convert_v2pdu_to_v1( netsnmp_pdu* template_v2pdu )
{
    netsnmp_pdu           *template_v1pdu;
    netsnmp_variable_list *first_vb, *vblist;
    netsnmp_variable_list *var;

    /*
     * Make a copy of the v2 Trap PDU
     *   before starting to convert this
     *   into a v1 Trap PDU.
     */
    template_v1pdu = snmp_clone_pdu( template_v2pdu);
    if (!template_v1pdu) {
        snmp_log(LOG_WARNING,
                 "send_trap: failed to copy v1 template PDU\n");
        return NULL;
    }
    template_v1pdu->command = SNMP_MSG_TRAP;
    first_vb = template_v1pdu->variables;
    vblist   = template_v1pdu->variables;

    /*
     * The first varbind should be the system uptime.
     */
    if (!vblist ||
        snmp_oid_compare(vblist->name,  vblist->name_length,
                         sysuptime_oid, sysuptime_oid_len)) {
        snmp_log(LOG_WARNING,
                 "send_trap: no v2 sysUptime varbind to set from\n");
        snmp_free_pdu(template_v1pdu);
        return NULL;
    }
    template_v1pdu->time = *vblist->val.integer;
    vblist = vblist->next_variable;
            
    /*
     * The second varbind should be the snmpTrapOID.
     */
    if (!vblist ||
        snmp_oid_compare(vblist->name, vblist->name_length,
                         snmptrap_oid, snmptrap_oid_len)) {
        snmp_log(LOG_WARNING,
                 "send_trap: no v2 trapOID varbind to set from\n");
        snmp_free_pdu(template_v1pdu);
        return NULL;
    }

    /*
     * Check the v2 varbind list for any varbinds
     *  that are not valid in an SNMPv1 trap.
     *  This basically means Counter64 values.
     *
     * RFC 2089 said to omit such varbinds from the list.
     * RFC 2576/3584 say to drop the trap completely.
     */
    for (var = vblist->next_variable; var; var = var->next_variable) {
        if ( var->type == ASN_COUNTER64 ) {
            snmp_log(LOG_WARNING,
                     "send_trap: v1 traps can't carry Counter64 varbinds\n");
            snmp_free_pdu(template_v1pdu);
            return NULL;
        }
    }

    /*
     * Set the generic & specific trap types,
     *    and the enterprise field from the v2 varbind list.
     * If there's an agentIPAddress varbind, set the agent_addr too
     */
    if (!snmp_oid_compare(vblist->val.objid, OID_LENGTH(trap_prefix),
                          trap_prefix,       OID_LENGTH(trap_prefix))) {
        /*
         * For 'standard' traps, extract the generic trap type
         *   from the snmpTrapOID value, and take the enterprise
         *   value from the 'snmpEnterprise' varbind.
         */
        template_v1pdu->trap_type =
            vblist->val.objid[OID_LENGTH(trap_prefix)] - 1;
        template_v1pdu->specific_type = 0;

        var = find_varbind_in_list( vblist,
                             snmptrapenterprise_oid,
                             snmptrapenterprise_oid_len);
        if (var) {
            template_v1pdu->enterprise_length = var->val_len/sizeof(oid);
            template_v1pdu->enterprise =
                snmp_duplicate_objid(var->val.objid,
                                     template_v1pdu->enterprise_length);
        } else {
            template_v1pdu->enterprise        = NULL;
            template_v1pdu->enterprise_length = 0;		/* XXX ??? */
        }
    } else {
        /*
         * For enterprise-specific traps, split the snmpTrapOID value
         *   into enterprise and specific trap
         */
        size_t len = vblist->val_len / sizeof(oid);
        if ( len <= 2 ) {
            snmp_log(LOG_WARNING,
                     "send_trap: v2 trapOID too short (%d)\n", (int)len);
            snmp_free_pdu(template_v1pdu);
            return NULL;
        }
        template_v1pdu->trap_type     = SNMP_TRAP_ENTERPRISESPECIFIC;
        template_v1pdu->specific_type = vblist->val.objid[len - 1];
        len--;
        if (vblist->val.objid[len-1] == 0)
            len--;
        SNMP_FREE(template_v1pdu->enterprise);
        template_v1pdu->enterprise =
            snmp_duplicate_objid(vblist->val.objid, len);
        template_v1pdu->enterprise_length = len;
    }
    var = find_varbind_in_list( vblist, agentaddr_oid,
                                        agentaddr_oid_len);
    if (var) {
        memcpy(template_v1pdu->agent_addr,
               var->val.string, 4);
    }

    /*
     * The remainder of the v2 varbind list is kept
     * as the v2 varbind list.  Update the PDU and
     * free the two redundant varbinds.
     */
    template_v1pdu->variables = vblist->next_variable;
    vblist->next_variable = NULL;
    snmp_free_varbind( first_vb );
            
    return template_v1pdu;
}

netsnmp_pdu*
convert_v1pdu_to_v2( netsnmp_pdu* template_v1pdu )
{
    netsnmp_pdu           *template_v2pdu;
    netsnmp_variable_list *var;
    oid                    enterprise[MAX_OID_LEN];
    size_t                 enterprise_len;

    /*
     * Make a copy of the v1 Trap PDU
     *   before starting to convert this
     *   into a v2 Trap PDU.
     */
    template_v2pdu = snmp_clone_pdu( template_v1pdu);
    if (!template_v2pdu) {
        snmp_log(LOG_WARNING,
                 "send_trap: failed to copy v2 template PDU\n");
        return NULL;
    }
    template_v2pdu->command = SNMP_MSG_TRAP2;

    /*
     * Insert an snmpTrapOID varbind before the original v1 varbind list
     *   either using one of the standard defined trap OIDs,
     *   or constructing this from the PDU enterprise & specific trap fields
     */
    if (template_v1pdu->trap_type == SNMP_TRAP_ENTERPRISESPECIFIC) {
        memcpy(enterprise, template_v1pdu->enterprise,
                           template_v1pdu->enterprise_length*sizeof(oid));
        enterprise_len               = template_v1pdu->enterprise_length;
        enterprise[enterprise_len++] = 0;
        enterprise[enterprise_len++] = template_v1pdu->specific_type;
    } else {
        memcpy(enterprise, cold_start_oid, sizeof(cold_start_oid));
	enterprise[9]  = template_v1pdu->trap_type+1;
        enterprise_len = sizeof(cold_start_oid)/sizeof(oid);
    }

    var = NULL;
    if (!snmp_varlist_add_variable( &var,
             snmptrap_oid, snmptrap_oid_len,
             ASN_OBJECT_ID,
             (u_char*)enterprise, enterprise_len*sizeof(oid))) {
        snmp_log(LOG_WARNING,
                 "send_trap: failed to insert copied snmpTrapOID varbind\n");
        snmp_free_pdu(template_v2pdu);
        return NULL;
    }
    var->next_variable        = template_v2pdu->variables;
    template_v2pdu->variables = var;

    /*
     * Insert a sysUptime varbind at the head of the v2 varbind list
     */
    var = NULL;
    if (!snmp_varlist_add_variable( &var,
             sysuptime_oid, sysuptime_oid_len,
             ASN_TIMETICKS,
             (u_char*)&(template_v1pdu->time), 
             sizeof(template_v1pdu->time))) {
        snmp_log(LOG_WARNING,
                 "send_trap: failed to insert copied sysUptime varbind\n");
        snmp_free_pdu(template_v2pdu);
        return NULL;
    }
    var->next_variable        = template_v2pdu->variables;
    template_v2pdu->variables = var;

    /*
     * Append the other three conversion varbinds,
     *  (snmpTrapAgentAddr, snmpTrapCommunity & snmpTrapEnterprise)
     *  if they're not already present.
     *  But don't bomb out completely if there are problems.
     */
    var = find_varbind_in_list( template_v2pdu->variables,
                                agentaddr_oid, agentaddr_oid_len);
    if (!var && (template_v1pdu->agent_addr[0]
              || template_v1pdu->agent_addr[1]
              || template_v1pdu->agent_addr[2]
              || template_v1pdu->agent_addr[3])) {
        if (!snmp_varlist_add_variable( &(template_v2pdu->variables),
                 agentaddr_oid, agentaddr_oid_len,
                 ASN_IPADDRESS,
                 (u_char*)&(template_v1pdu->agent_addr), 
                 sizeof(template_v1pdu->agent_addr)))
            snmp_log(LOG_WARNING,
                 "send_trap: failed to append snmpTrapAddr varbind\n");
    }
    var = find_varbind_in_list( template_v2pdu->variables,
                                community_oid, community_oid_len);
    if (!var && template_v1pdu->community) {
        if (!snmp_varlist_add_variable( &(template_v2pdu->variables),
                 community_oid, community_oid_len,
                 ASN_OCTET_STR,
                 template_v1pdu->community, 
                 template_v1pdu->community_len))
            snmp_log(LOG_WARNING,
                 "send_trap: failed to append snmpTrapCommunity varbind\n");
    }
    var = find_varbind_in_list( template_v2pdu->variables,
                                snmptrapenterprise_oid,
                                snmptrapenterprise_oid_len);
    if (!var) {
        if (!snmp_varlist_add_variable( &(template_v2pdu->variables),
                 snmptrapenterprise_oid, snmptrapenterprise_oid_len,
                 ASN_OBJECT_ID,
                 (u_char*)template_v1pdu->enterprise, 
                 template_v1pdu->enterprise_length*sizeof(oid)))
            snmp_log(LOG_WARNING,
                 "send_trap: failed to append snmpEnterprise varbind\n");
    }
    return template_v2pdu;
}

/**
 * This function allows you to make a distinction between generic 
 * traps from different classes of equipment. For example, you may want 
 * to handle a SNMP_TRAP_LINKDOWN trap for a particular device in a 
 * different manner to a generic system SNMP_TRAP_LINKDOWN trap.
 *   
 *
 * @param trap is the generic trap type.  The trap types are:
 *		- SNMP_TRAP_COLDSTART:
 *			cold start
 *		- SNMP_TRAP_WARMSTART:
 *			warm start
 *		- SNMP_TRAP_LINKDOWN:
 *			link down
 *		- SNMP_TRAP_LINKUP:
 *			link up
 *		- SNMP_TRAP_AUTHFAIL:
 *			authentication failure
 *		- SNMP_TRAP_EGPNEIGHBORLOSS:
 *			egp neighbor loss
 *		- SNMP_TRAP_ENTERPRISESPECIFIC:
 *			enterprise specific
 *			
 * @param specific is the specific trap value.
 *
 * @param enterprise is an enterprise oid in which you want to send specific 
 *	traps from. 
 *
 * @param enterprise_length is the length of the enterprise oid, use macro,
 *	OID_LENGTH, to compute length.
 *
 * @param vars is used to supply list of variable bindings to form an SNMPv2 
 *	trap.
 *
 * @param context currently unused 
 *
 * @param flags currently unused 
 *
 * @return void
 *
 * @see send_easy_trap
 * @see send_v2trap
 */
int
netsnmp_send_traps(int trap, int specific,
                          const oid * enterprise, int enterprise_length,
                          netsnmp_variable_list * vars,
                          const char * context, int flags)
{
    netsnmp_pdu           *template_v1pdu;
    netsnmp_pdu           *template_v2pdu;
    netsnmp_variable_list *vblist = NULL;
    netsnmp_variable_list *trap_vb;
    netsnmp_variable_list *var;
    in_addr_t             *pdu_in_addr_t;
    u_long                 uptime;
    struct trap_sink *sink;
    const char            *v1trapaddress;
    int                    res = 0;

    DEBUGMSGTL(( "trap", "send_trap %d %d ", trap, specific));
    DEBUGMSGOID(("trap", enterprise, enterprise_length));
    DEBUGMSG(( "trap", "\n"));

    if (vars) {
        vblist = snmp_clone_varbind( vars );
        if (!vblist) {
            snmp_log(LOG_WARNING,
                     "send_trap: failed to clone varbind list\n");
            return -1;
        }
    }

    if ( trap == -1 ) {
        /*
         * Construct the SNMPv2-style notification PDU
         */
        if (!vblist) {
            snmp_log(LOG_WARNING,
                     "send_trap: called with NULL v2 information\n");
            return -1;
        }
        template_v2pdu = snmp_pdu_create(SNMP_MSG_TRAP2);
        if (!template_v2pdu) {
            snmp_log(LOG_WARNING,
                     "send_trap: failed to construct v2 template PDU\n");
            snmp_free_varbind(vblist);
            return -1;
        }

        /*
         * Check the varbind list we've been given.
         * If it starts with a 'sysUptime.0' varbind, then use that.
         * Otherwise, prepend a suitable 'sysUptime.0' varbind.
         */
        if (!snmp_oid_compare( vblist->name,    vblist->name_length,
                               sysuptime_oid, sysuptime_oid_len )) {
            template_v2pdu->variables = vblist;
            trap_vb  = vblist->next_variable;
        } else {
            uptime   = netsnmp_get_agent_uptime();
            var = NULL;
            snmp_varlist_add_variable( &var,
                           sysuptime_oid, sysuptime_oid_len,
                           ASN_TIMETICKS, (u_char*)&uptime, sizeof(uptime));
            if (!var) {
                snmp_log(LOG_WARNING,
                     "send_trap: failed to insert sysUptime varbind\n");
                snmp_free_pdu(template_v2pdu);
                snmp_free_varbind(vblist);
                return -1;
            }
            template_v2pdu->variables = var;
            var->next_variable        = vblist;
            trap_vb  = vblist;
        }

        /*
         * 'trap_vb' should point to the snmpTrapOID.0 varbind,
         *   identifying the requested trap.  If not then bomb out.
         * If it's a 'standard' trap, then we need to append an
         *   snmpEnterprise varbind (if there isn't already one).
         */
        if (!trap_vb ||
            snmp_oid_compare(trap_vb->name, trap_vb->name_length,
                             snmptrap_oid,  snmptrap_oid_len)) {
            snmp_log(LOG_WARNING,
                     "send_trap: no v2 trapOID varbind provided\n");
            snmp_free_pdu(template_v2pdu);
            return -1;
        }
        if (!snmp_oid_compare(vblist->val.objid, OID_LENGTH(trap_prefix),
                              trap_prefix,       OID_LENGTH(trap_prefix))) {
            var = find_varbind_in_list( template_v2pdu->variables,
                                        snmptrapenterprise_oid,
                                        snmptrapenterprise_oid_len);
            if (!var &&
                !snmp_varlist_add_variable( &(template_v2pdu->variables),
                     snmptrapenterprise_oid, snmptrapenterprise_oid_len,
                     ASN_OBJECT_ID,
                     enterprise, enterprise_length*sizeof(oid))) {
                snmp_log(LOG_WARNING,
                     "send_trap: failed to add snmpEnterprise to v2 trap\n");
                snmp_free_pdu(template_v2pdu);
                return -1;
            }
        }
            

        /*
         * If everything's OK, convert the v2 template into an SNMPv1 trap PDU.
         */
        template_v1pdu = convert_v2pdu_to_v1( template_v2pdu );
        if (!template_v1pdu) {
            snmp_log(LOG_WARNING,
                     "send_trap: failed to convert v2->v1 template PDU\n");
        }

    } else {
        /*
         * Construct the SNMPv1 trap PDU....
         */
        template_v1pdu = snmp_pdu_create(SNMP_MSG_TRAP);
        if (!template_v1pdu) {
            snmp_log(LOG_WARNING,
                     "send_trap: failed to construct v1 template PDU\n");
            snmp_free_varbind(vblist);
            return -1;
        }
        template_v1pdu->trap_type     = trap;
        template_v1pdu->specific_type = specific;
        template_v1pdu->time          = netsnmp_get_agent_uptime();

        if (snmp_clone_mem((void **) &template_v1pdu->enterprise,
                       enterprise, enterprise_length * sizeof(oid))) {
            snmp_log(LOG_WARNING,
                     "send_trap: failed to set v1 enterprise OID\n");
            snmp_free_varbind(vblist);
            snmp_free_pdu(template_v1pdu);
            return -1;
        }
        template_v1pdu->enterprise_length = enterprise_length;

        template_v1pdu->flags    |= UCD_MSG_FLAG_FORCE_PDU_COPY;
        template_v1pdu->variables = vblist;

        /*
         * ... and convert it into an SNMPv2-style notification PDU.
         */

        template_v2pdu = convert_v1pdu_to_v2( template_v1pdu );
        if (!template_v2pdu) {
            snmp_log(LOG_WARNING,
                     "send_trap: failed to convert v1->v2 template PDU\n");
        }
    }

    /*
     * Check whether we're ignoring authFail traps
     */
    if (template_v1pdu) {
      if (template_v1pdu->trap_type == SNMP_TRAP_AUTHFAIL &&
        snmp_enableauthentraps == SNMP_AUTHENTICATED_TRAPS_DISABLED) {
        snmp_free_pdu(template_v1pdu);
        snmp_free_pdu(template_v2pdu);
        return 0;
      }

    /*
     * Ensure that the v1 trap PDU includes the local IP address
     */
       pdu_in_addr_t = (in_addr_t *) template_v1pdu->agent_addr;
       v1trapaddress = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID,
                                             NETSNMP_DS_AGENT_TRAP_ADDR);
       if (v1trapaddress != NULL) {
           /* "v1trapaddress" was specified in config, try to resolve it */
           res = netsnmp_gethostbyname_v4(v1trapaddress, pdu_in_addr_t);
       }
       if (v1trapaddress == NULL || res < 0) {
           /* "v1trapaddress" was not specified in config or the resolution failed,
            * try any local address */
           *pdu_in_addr_t = get_myaddr();
       }

    }

    if (template_v2pdu) {
	/* A context name was provided, so copy it and its length to the v2 pdu
	 * template. */
	if (context != NULL)
	{
		template_v2pdu->contextName    = strdup(context);
		template_v2pdu->contextNameLen = strlen(context);
	}
    }

    /*
     *  Now loop through the list of trap sinks
     *   and call the trap callback routines,
     *   providing an appropriately formatted PDU in each case
     */
    for (sink = sinks; sink; sink = sink->next) {
#ifndef NETSNMP_DISABLE_SNMPV1
        if (sink->version == SNMP_VERSION_1) {
          if (template_v1pdu) {
            send_trap_to_sess(sink->sesp, template_v1pdu);
          }
        } else {
#endif
          if (template_v2pdu) {
            template_v2pdu->command = sink->pdutype;
            send_trap_to_sess(sink->sesp, template_v2pdu);
          }
#ifndef NETSNMP_DISABLE_SNMPV1
        }
#endif
    }
    if (template_v1pdu)
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                        SNMPD_CALLBACK_SEND_TRAP1, template_v1pdu);
    if (template_v2pdu)
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                        SNMPD_CALLBACK_SEND_TRAP2, template_v2pdu);
    snmp_free_pdu(template_v1pdu);
    snmp_free_pdu(template_v2pdu);
    return 0;
}


void
send_enterprise_trap_vars(int trap,
                          int specific,
                          const oid * enterprise, int enterprise_length,
                          netsnmp_variable_list * vars)
{
    netsnmp_send_traps(trap, specific,
                       enterprise, enterprise_length,
                       vars, NULL, 0);
    return;
}

/**
 * Captures responses or the lack there of from INFORMs that were sent
 * 1) a response is received from an INFORM
 * 2) one isn't received and the retries/timeouts have failed
*/
int
handle_inform_response(int op, netsnmp_session * session,
                       int reqid, netsnmp_pdu *pdu,
                       void *magic)
{
    /* XXX: possibly stats update */
    switch (op) {

    case NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE:
        snmp_increment_statistic(STAT_SNMPINPKTS);
        DEBUGMSGTL(("trap", "received the inform response for reqid=%d\n",
                    reqid));
        break;

    case NETSNMP_CALLBACK_OP_TIMED_OUT:
        DEBUGMSGTL(("trap",
                    "received a timeout sending an inform for reqid=%d\n",
                    reqid));
        break;

    case NETSNMP_CALLBACK_OP_SEND_FAILED:
        DEBUGMSGTL(("trap",
                    "failed to send an inform for reqid=%d\n",
                    reqid));
        break;

    default:
        DEBUGMSGTL(("trap", "received op=%d for reqid=%d when trying to send an inform\n", op, reqid));
    }

    return 1;
}


/*
 * send_trap_to_sess: sends a trap to a session but assumes that the
 * pdu is constructed correctly for the session type. 
 */
void
send_trap_to_sess(netsnmp_session * sess, netsnmp_pdu *template_pdu)
{
    netsnmp_pdu    *pdu;
    int            result;

    if (!sess || !template_pdu)
        return;

    DEBUGMSGTL(("trap", "sending trap type=%d, version=%ld\n",
                template_pdu->command, sess->version));

#ifndef NETSNMP_DISABLE_SNMPV1
    if (sess->version == SNMP_VERSION_1 &&
        (template_pdu->command != SNMP_MSG_TRAP))
        return;                 /* Skip v1 sinks for v2 only traps */
    if (sess->version != SNMP_VERSION_1 &&
        (template_pdu->command == SNMP_MSG_TRAP))
        return;                 /* Skip v2+ sinks for v1 only traps */
#endif
    template_pdu->version = sess->version;
    pdu = snmp_clone_pdu(template_pdu);
    pdu->sessid = sess->sessid; /* AgentX only ? */

    if ( template_pdu->command == SNMP_MSG_INFORM
#ifdef USING_AGENTX_PROTOCOL_MODULE
         || template_pdu->command == AGENTX_MSG_NOTIFY
#endif
       ) {
        result =
            snmp_async_send(sess, pdu, &handle_inform_response, NULL);
        
    } else {
        if ((sess->version == SNMP_VERSION_3) &&
                (pdu->command == SNMP_MSG_TRAP2) &&
                (sess->securityEngineIDLen == 0)) {
            u_char          tmp[SPRINT_MAX_LEN];

            int len = snmpv3_get_engineID(tmp, sizeof(tmp));
            memdup(&pdu->securityEngineID, tmp, len);
            pdu->securityEngineIDLen = len;
        }

        result = snmp_send(sess, pdu);
    }

    if (result == 0) {
        snmp_sess_perror("snmpd: send_trap", sess);
        snmp_free_pdu(pdu);
    } else {
        snmp_increment_statistic(STAT_SNMPOUTTRAPS);
        snmp_increment_statistic(STAT_SNMPOUTPKTS);
    }
}

void
send_trap_vars(int trap, int specific, netsnmp_variable_list * vars)
{
    if (trap == SNMP_TRAP_ENTERPRISESPECIFIC)
        send_enterprise_trap_vars(trap, specific, objid_enterprisetrap,
                                  OID_LENGTH(objid_enterprisetrap), vars);
    else
        send_enterprise_trap_vars(trap, specific, trap_version_id,
                                  OID_LENGTH(trap_version_id), vars);
}

#ifndef NETSNMP_FEATURE_REMOVE_TRAP_VARS_WITH_CONTEXT
/* Send a trap under a context */
void send_trap_vars_with_context(int trap, int specific, 
              netsnmp_variable_list *vars, const char *context)
{
    if (trap == SNMP_TRAP_ENTERPRISESPECIFIC)
        netsnmp_send_traps(trap, specific, objid_enterprisetrap,
                                  OID_LENGTH(objid_enterprisetrap), vars,
								  context, 0);
    else
        netsnmp_send_traps(trap, specific, trap_version_id,
                                  OID_LENGTH(trap_version_id), vars, 
								  context, 0);
    	
}
#endif /* NETSNMP_FEATURE_REMOVE_TRAP_VARS_WITH_CONTEXT */

/**
 * Sends an SNMPv1 trap (or the SNMPv2 equivalent) to the list of  
 * configured trap destinations (or "sinks"), using the provided 
 * values for the generic trap type and specific trap value.
 *
 * This function eventually calls send_enterprise_trap_vars.  If the
 * trap type is not set to SNMP_TRAP_ENTERPRISESPECIFIC the enterprise 
 * and enterprise_length paramater is set to the pre defined NETSNMP_SYSTEM_MIB 
 * oid and length respectively.  If the trap type is set to 
 * SNMP_TRAP_ENTERPRISESPECIFIC the enterprise and enterprise_length 
 * parameters are set to the pre-defined NETSNMP_NOTIFICATION_MIB oid and length 
 * respectively.
 *
 * @param trap is the generic trap type.
 *
 * @param specific is the specific trap value.
 *
 * @return void
 *
 * @see send_enterprise_trap_vars
 * @see send_v2trap
 */
       	
void
send_easy_trap(int trap, int specific)
{
    send_trap_vars(trap, specific, NULL);
}

/**
 * Uses the supplied list of variable bindings to form an SNMPv2 trap, 
 * which is sent to SNMPv2-capable sinks  on  the  configured  list.  
 * An equivalent INFORM is sent to the configured list of inform sinks.  
 * Sinks that can only handle SNMPv1 traps are skipped.
 *
 * This function eventually calls send_enterprise_trap_vars.  If the
 * trap type is not set to SNMP_TRAP_ENTERPRISESPECIFIC the enterprise 
 * and enterprise_length paramater is set to the pre defined NETSNMP_SYSTEM_MIB 
 * oid and length respectively.  If the trap type is set to 
 * SNMP_TRAP_ENTERPRISESPECIFIC the enterprise and enterprise_length 
 * parameters are set to the pre-defined NETSNMP_NOTIFICATION_MIB oid and length 
 * respectively.
 *
 * @param vars is used to supply list of variable bindings to form an SNMPv2 
 *	trap.
 *
 * @return void
 *
 * @see send_easy_trap
 * @see send_enterprise_trap_vars
 */

void
send_v2trap(netsnmp_variable_list * vars)
{
    send_trap_vars(-1, -1, vars);
}

/**
 * Similar to send_v2trap(), with the added ability to specify a context.  If
 * the last parameter is NULL, then this call is equivalent to send_v2trap().
 *
 * @param vars is used to supply the list of variable bindings for the trap.
 * 
 * @param context is used to specify the context of the trap.
 *
 * @return void
 *
 * @see send_v2trap
 */
#ifndef NETSNMP_FEATURE_REMOVE_SEND_V3TRAP
void send_v3trap(netsnmp_variable_list *vars, const char *context)
{
    netsnmp_send_traps(-1, -1, 
                       trap_version_id, OID_LENGTH(trap_version_id),
                       vars, context, 0);
}
#endif /* NETSNMP_FEATURE_REMOVE_SEND_V3TRAP */

#ifndef NETSNMP_FEATURE_REMOVE_SEND_TRAP_PDU
void
send_trap_pdu(netsnmp_pdu *pdu)
{
    send_trap_vars(-1, -1, pdu->variables);
}
#endif /* NETSNMP_FEATURE_REMOVE_SEND_TRAP_PDU */



        /*******************
	 *
	 * Config file handling
	 *
	 *******************/

void
snmpd_parse_config_authtrap(const char *token, char *cptr)
{
    int             i;

    i = atoi(cptr);
    if (i == 0) {
        if (strcmp(cptr, "enable") == 0) {
            i = SNMP_AUTHENTICATED_TRAPS_ENABLED;
        } else if (strcmp(cptr, "disable") == 0) {
            i = SNMP_AUTHENTICATED_TRAPS_DISABLED;
        }
    }
    if (i < 1 || i > 2) {
        config_perror("authtrapenable must be 1 or 2");
    } else {
        if (strcmp(token, "pauthtrapenable") == 0) {
            if (snmp_enableauthentrapsset < 0) {
                /*
                 * This is bogus (and shouldn't happen anyway) -- the value
                 * of snmpEnableAuthenTraps.0 is already configured
                 * read-only.  
                 */
                snmp_log(LOG_WARNING,
                         "ignoring attempted override of read-only snmpEnableAuthenTraps.0\n");
                return;
            } else {
                snmp_enableauthentrapsset++;
            }
        } else {
            if (snmp_enableauthentrapsset > 0) {
                /*
                 * This is bogus (and shouldn't happen anyway) -- we already
                 * read a persistent value of snmpEnableAuthenTraps.0, which
                 * we should ignore in favour of this one.  
                 */
                snmp_log(LOG_WARNING,
                         "ignoring attempted override of read-only snmpEnableAuthenTraps.0\n");
                /*
                 * Fall through and copy in this value.  
                 */
            }
            snmp_enableauthentrapsset = -1;
        }
        snmp_enableauthentraps = i;
    }
}

#ifndef NETSNMP_DISABLE_SNMPV1
void
snmpd_parse_config_trapsink(const char *token, char *cptr)
{
    char           *sp, *cp, *pp = NULL;
    char            *st;

    if (!snmp_trapcommunity)
        snmp_trapcommunity = strdup("public");
    sp = strtok_r(cptr, " \t\n", &st);
    cp = strtok_r(NULL, " \t\n", &st);
    if (cp)
        pp = strtok_r(NULL, " \t\n", &st);
    if (pp)
	config_pwarn("The separate port argument to trapsink is deprecated");
    if (create_v1_trap_session(sp, pp, cp ? cp : snmp_trapcommunity) == 0) {
	netsnmp_config_error("cannot create trapsink: %s", cptr);
    }
}
#endif

#ifndef NETSNMP_DISABLE_SNMPV2C
void
snmpd_parse_config_trap2sink(const char *word, char *cptr)
{
    char           *st, *sp, *cp, *pp = NULL;

    if (!snmp_trapcommunity)
        snmp_trapcommunity = strdup("public");
    sp = strtok_r(cptr, " \t\n", &st);
    cp = strtok_r(NULL, " \t\n", &st);
    if (cp)
        pp = strtok_r(NULL, " \t\n", &st);
    if (pp)
	config_pwarn("The separate port argument to trapsink2 is deprecated");
    if (create_v2_trap_session(sp, pp, cp ? cp : snmp_trapcommunity) == 0) {
	netsnmp_config_error("cannot create trap2sink: %s", cptr);
    }
}

void
snmpd_parse_config_informsink(const char *word, char *cptr)
{
    char           *st, *sp, *cp, *pp = NULL;

    if (!snmp_trapcommunity)
        snmp_trapcommunity = strdup("public");
    sp = strtok_r(cptr, " \t\n", &st);
    cp = strtok_r(NULL, " \t\n", &st);
    if (cp)
        pp = strtok_r(NULL, " \t\n", &st);
    if (pp)
	config_pwarn("The separate port argument to informsink is deprecated");
    if (create_v2_inform_session(sp, pp, cp ? cp : snmp_trapcommunity) == 0) {
	netsnmp_config_error("cannot create informsink: %s", cptr);
    }
}
#endif

/*
 * this must be standardized somewhere, right? 
 */
#define MAX_ARGS 128

static int      traptype;

static void
trapOptProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'i':
                traptype = SNMP_MSG_INFORM;
                break;
            default:
                config_perror("unknown argument passed to -C");
                break;
            }
        }
        break;
    }
}


void
snmpd_parse_config_trapsess(const char *word, char *cptr)
{
    char           *argv[MAX_ARGS], *cp = cptr;
    int             argn, rc;
    netsnmp_session session, *ss;
    netsnmp_transport *transport;
    size_t          len;

    /*
     * inform or trap?  default to trap 
     */
    traptype = SNMP_MSG_TRAP2;

    /*
     * create the argv[] like array 
     */
    argv[0] = strdup("snmpd-trapsess"); /* bogus entry for getopt() */
    for (argn = 1; cp && argn < MAX_ARGS; argn++) {
        char            tmp[SPRINT_MAX_LEN];

        cp = copy_nword(cp, tmp, SPRINT_MAX_LEN);
        argv[argn] = strdup(tmp);
    }

    netsnmp_parse_args(argn, argv, &session, "C:", trapOptProc,
                       NETSNMP_PARSE_ARGS_NOLOGGING |
                       NETSNMP_PARSE_ARGS_NOZERO);

    transport = netsnmp_transport_open_client("snmptrap", session.peername);
    if (transport == NULL) {
        config_perror("snmpd: failed to parse this line.");
        return;
    }
    if ((rc = netsnmp_sess_config_and_open_transport(&session, transport))
        != SNMPERR_SUCCESS) {
        session.s_snmp_errno = rc;
        session.s_errno = 0;
        return;
    }
    ss = snmp_add(&session, transport, NULL, NULL);
    for (; argn > 0; argn--) {
        free(argv[argn - 1]);
    }

    if (!ss) {
        config_perror
            ("snmpd: failed to parse this line or the remote trap receiver is down.  Possible cause:");
        snmp_sess_perror("snmpd: snmpd_parse_config_trapsess()", &session);
        return;
    }

    /*
     * If this is an SNMPv3 TRAP session, then the agent is
     *   the authoritative engine, so set the engineID accordingly
     */
    if (ss->version == SNMP_VERSION_3 &&
        traptype != SNMP_MSG_INFORM   &&
        ss->securityEngineIDLen == 0) {
            u_char          tmp[SPRINT_MAX_LEN];

            len = snmpv3_get_engineID( tmp, sizeof(tmp));
            memdup(&ss->securityEngineID, tmp, len);
            ss->securityEngineIDLen = len;
    }

#ifndef NETSNMP_DISABLE_SNMPV1
    if (ss->version == SNMP_VERSION_1)
        traptype = SNMP_MSG_TRAP;
#endif
    add_trap_session(ss, traptype, (traptype == SNMP_MSG_INFORM), ss->version);
}

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
void
snmpd_parse_config_trapcommunity(const char *word, char *cptr)
{
    if (snmp_trapcommunity != NULL) {
        free(snmp_trapcommunity);
    }
    snmp_trapcommunity = (char *) malloc(strlen(cptr) + 1);
    if (snmp_trapcommunity != NULL) {
        copy_nword(cptr, snmp_trapcommunity, strlen(cptr) + 1);
    }
}

void
snmpd_free_trapcommunity(void)
{
    if (snmp_trapcommunity) {
        free(snmp_trapcommunity);
        snmp_trapcommunity = NULL;
    }
}
#endif
/** @} */
