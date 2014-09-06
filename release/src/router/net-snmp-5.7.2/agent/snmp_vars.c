/*
 * snmp_vars.c - return a pointer to the named variable.
 */
/**
 * @addtogroup library
 *
 * @{
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
	Copyright 1988, 1989, 1990 by Carnegie Mellon University
	Copyright 1989	TGV, Incorporated

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU and TGV not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CMU AND TGV DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL CMU OR TGV BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
******************************************************************/
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/*
 * additions, fixes and enhancements for Linux by Erik Schoenfelder
 * (schoenfr@ibr.cs.tu-bs.de) 1994/1995.
 * Linux additions taken from CMU to UCD stack by Jennifer Bray of Origin
 * (jbray@origin-at.co.uk) 1997
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/*
 * XXXWWW merge todo: incl/excl range changes in differences between
 * 1.194 and 1.199 
 */

#include <net-snmp/net-snmp-config.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

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
# include <sys/socket.h>
#endif
#if HAVE_SYS_STREAM_H
#   ifdef sysv5UnixWare7
#      define _KMEMUSER 1 /* <sys/stream.h> needs this for queue_t */
#   endif
#include <sys/stream.h>
#endif
#if HAVE_SYS_SOCKETVAR_H
# include <sys/socketvar.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#ifdef NETSNMP_ENABLE_IPV6
#if HAVE_NETINET_IP6_H
#include <netinet/ip6.h>
#endif
#endif
#if HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#if HAVE_NETINET_IP_VAR_H
#include <netinet/ip_var.h>
#endif
#ifdef NETSNMP_ENABLE_IPV6
#if HAVE_NETNETSNMP_ENABLE_IPV6_IP6_VAR_H
#include <netinet6/ip6_var.h>
#endif
#endif
#if HAVE_NETINET_IN_PCB_H
#include <netinet/in_pcb.h>
#endif
#if HAVE_INET_MIB2_H
#include <inet/mib2.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/mib_modules.h>
#include <net-snmp/agent/agent_sysORTable.h>
#include "kernel.h"

#include "mibgroup/struct.h"
#include "snmpd.h"
#include "agentx/agentx_config.h"
#include "agentx/subagent.h"
#include "net-snmp/agent/all_helpers.h"
#include "agent_module_includes.h"
#include "mib_module_includes.h"
#include "net-snmp/library/container.h"

#if defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL)
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <net-snmp/library/cert_util.h>
#endif

#include "snmp_perl.h"

#ifndef  MIN
#define  MIN(a,b)                     (((a) < (b)) ? (a) : (b))
#endif

static char     done_init_agent = 0;

struct module_init_list *initlist = NULL;
struct module_init_list *noinitlist = NULL;

/*
 * mib clients are passed a pointer to a oid buffer.  Some mib clients
 * * (namely, those first noticed in mibII/vacm.c) modify this oid buffer
 * * before they determine if they really need to send results back out
 * * using it.  If the master agent determined that the client was not the
 * * right one to talk with, it will use the same oid buffer to pass to the
 * * rest of the clients, which may not longer be valid.  This should be
 * * fixed in all clients rather than the master.  However, its not a
 * * particularily easy bug to track down so this saves debugging time at
 * * the expense of a few memcpy's.
 */
#define MIB_CLIENTS_ARE_EVIL 1

extern netsnmp_subtree *subtrees;

/*
 *      Each variable name is placed in the variable table, without the
 * terminating substring that determines the instance of the variable.  When
 * a string is found that is lexicographicly preceded by the input string,
 * the function for that entry is called to find the method of access of the
 * instance of the named variable.  If that variable is not found, NULL is
 * returned, and the search through the table continues (it will probably
 * stop at the next entry).  If it is found, the function returns a character
 * pointer and a length or a function pointer.  The former is the address
 * of the operand, the latter is a write routine for the variable.
 *
 * u_char *
 * findVar(name, length, exact, var_len, write_method)
 * oid      *name;          IN/OUT - input name requested, output name found
 * int      length;         IN/OUT - number of sub-ids in the in and out oid's
 * int      exact;          IN - TRUE if an exact match was requested.
 * int      len;            OUT - length of variable or 0 if function returned.
 * int      write_method;   OUT - pointer to function to set variable,
 *                                otherwise 0
 *
 *     The writeVar function is returned to handle row addition or complex
 * writes that require boundary checking or executing an action.
 * This routine will be called three times for each varbind in the packet.
 * The first time for each varbind, action is set to RESERVE1.  The type
 * and value should be checked during this pass.  If any other variables
 * in the MIB depend on this variable, this variable will be stored away
 * (but *not* committed!) in a place where it can be found by a call to
 * writeVar for a dependent variable, even in the same PDU.  During
 * the second pass, action is set to RESERVE2.  If this variable is dependent
 * on any other variables, it will check them now.  It must check to see
 * if any non-committed values have been stored for variables in the same
 * PDU that it depends on.  Sometimes resources will need to be reserved
 * in the first two passes to guarantee that the operation can proceed
 * during the third pass.  During the third pass, if there were no errors
 * in the first two passes, writeVar is called for every varbind with action
 * set to COMMIT.  It is now that the values should be written.  If there
 * were errors during the first two passes, writeVar is called in the third
 * pass once for each varbind, with the action set to FREE.  An opportunity
 * is thus provided to free those resources reserved in the first two passes.
 * 
 * writeVar(action, var_val, var_val_type, var_val_len, statP, name, name_len)
 * int      action;         IN - RESERVE1, RESERVE2, COMMIT, or FREE
 * u_char   *var_val;       IN - input or output buffer space
 * u_char   var_val_type;   IN - type of input buffer
 * int      var_val_len;    IN - input and output buffer len
 * u_char   *statP;         IN - pointer to local statistic
 * oid      *name           IN - pointer to name requested
 * int      name_len        IN - number of sub-ids in the name
 */

long            long_return;
#ifndef ibm032
u_char          return_buf[258];
#else
u_char          return_buf[256];        /* nee 64 */
#endif

int             callback_master_num = -1;

#ifdef NETSNMP_TRANSPORT_CALLBACK_DOMAIN
netsnmp_session *callback_master_sess = NULL;

static void
_init_agent_callback_transport(void)
{
    /*
     * always register a callback transport for internal use 
     */
    callback_master_sess = netsnmp_callback_open(0, handle_snmp_packet,
                                                 netsnmp_agent_check_packet,
                                                 netsnmp_agent_check_parse);
    if (callback_master_sess)
        callback_master_num = callback_master_sess->local_port;
}
#else
#define _init_agent_callback_transport()
#endif

/**
 * Initialize the agent.  Calls into init_agent_read_config to set tha app's
 * configuration file in the appropriate default storage space,
 *  NETSNMP_DS_LIB_APPTYPE.  Need to call init_agent before calling init_snmp.
 *
 * @param app the configuration file to be read in, gets stored in default
 *        storage
 *
 * @return Returns non-zero on failure and zero on success.
 *
 * @see init_snmp
 */
int
init_agent(const char *app)
{
    int             r = 0;

    if(++done_init_agent > 1) {
        snmp_log(LOG_WARNING, "ignoring extra call to init_agent (%d)\n", 
                 done_init_agent);
        return r;
    }

    /*
     * get current time (ie, the time the agent started) 
     */
    netsnmp_set_agent_starttime(NULL);

    /*
     * we handle alarm signals ourselves in the select loop 
     */
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_ALARM_DONT_USE_SIG, 1);

#ifdef NETSNMP_CAN_USE_NLIST
    r = init_kmem("/dev/kmem") ? 0 : -EACCES;
#endif

    setup_tree();

    init_agent_read_config(app);

#ifdef TESTING
    auto_nlist_print_tree(-2, 0);
#endif

    _init_agent_callback_transport();
    
    netsnmp_init_helpers();
    init_traps();
    netsnmp_container_init_list();
    init_agent_sysORTable();

#if defined(USING_AGENTX_SUBAGENT_MODULE) || defined(USING_AGENTX_MASTER_MODULE)
    /*
     * initialize agentx configs
     */
    agentx_config_init();
#if defined(USING_AGENTX_SUBAGENT_MODULE)
    if(netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                              NETSNMP_DS_AGENT_ROLE) == SUB_AGENT)
        subagent_init();
#endif
#endif

    /*
     * Register configuration tokens from transport modules.  
     */
#ifdef NETSNMP_TRANSPORT_UDP_DOMAIN
    netsnmp_udp_agent_config_tokens_register();
#endif
#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
    netsnmp_udp6_agent_config_tokens_register();
#endif
#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
    netsnmp_unix_agent_config_tokens_register();
#endif

#ifdef NETSNMP_EMBEDDED_PERL
    init_perl();
#endif

#if defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL)
    /** init secname mapping */
    netsnmp_certs_agent_init();
#endif

#ifdef USING_AGENTX_SUBAGENT_MODULE
    /*
     * don't init agent modules for a sub-agent
     */
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_ROLE) == SUB_AGENT)
        return r;
#endif

#  include "agent_module_inits.h"

    return r;
}                               /* end init_agent() */

oid             nullOid[] = { 0, 0 };
int             nullOidLen = sizeof(nullOid);

void
shutdown_agent(void) {

    /* probably some of this can be called as shutdown callback */
    shutdown_tree();
    clear_context();
    netsnmp_clear_callback_list();
    netsnmp_clear_tdomain_list();
    netsnmp_clear_handler_list();
    shutdown_agent_sysORTable();
    netsnmp_container_free_list();
    clear_sec_mod();
    clear_snmp_enum();
    clear_callback();
    shutdown_secmod();
    netsnmp_addrcache_destroy();
#ifdef NETSNMP_CAN_USE_NLIST
    free_kmem();
#endif

    done_init_agent = 0;
}


void
add_to_init_list(char *module_list)
{
    struct module_init_list *newitem, **list;
    char           *cp;
    char           *st;

    if (module_list == NULL) {
        return;
    } else {
        cp = (char *) module_list;
    }

    if (*cp == '-' || *cp == '!') {
        cp++;
        list = &noinitlist;
    } else {
        list = &initlist;
    }

    cp = strtok_r(cp, ", :", &st);
    while (cp) {
        newitem = (struct module_init_list *) calloc(1, sizeof(*initlist));
        newitem->module_name = strdup(cp);
        newitem->next = *list;
        *list = newitem;
        cp = strtok_r(NULL, ", :", &st);
    }
}

int
should_init(const char *module_name)
{
    struct module_init_list *listp;

    /*
     * a definitive list takes priority 
     */
    if (initlist) {
        listp = initlist;
        while (listp) {
            if (strcmp(listp->module_name, module_name) == 0) {
                DEBUGMSGTL(("mib_init", "initializing: %s\n",
                            module_name));
                return DO_INITIALIZE;
            }
            listp = listp->next;
        }
        DEBUGMSGTL(("mib_init", "skipping:     %s\n", module_name));
        return DONT_INITIALIZE;
    }

    /*
     * initialize it only if not on the bad list (bad module, no bone) 
     */
    if (noinitlist) {
        listp = noinitlist;
        while (listp) {
            if (strcmp(listp->module_name, module_name) == 0) {
                DEBUGMSGTL(("mib_init", "skipping:     %s\n",
                            module_name));
                return DONT_INITIALIZE;
            }
            listp = listp->next;
        }
    }
    DEBUGMSGTL(("mib_init", "initializing: %s\n", module_name));

    /*
     * initialize it 
     */
    return DO_INITIALIZE;
}
/**  @} */

