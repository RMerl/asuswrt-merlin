/*
 * agent_read_config.c
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#else
#include <sys/types.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdio.h>
#include <ctype.h>
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
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#if HAVE_SYS_SOCKETVAR_H
#ifndef dynix
#include <sys/socketvar.h>
#else
#include <sys/param.h>
#endif
#endif
#endif
#if HAVE_SYS_STREAM_H
#   ifdef sysv5UnixWare7
#      define _KMEMUSER 1   /* <sys/stream.h> needs this for queue_t */
#   endif
#include <sys/stream.h>
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "mibgroup/struct.h"
#include <net-snmp/agent/agent_trap.h>
#include "snmpd.h"
#include <net-snmp/agent/agent_callbacks.h>
#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_iterator.h>
#include <net-snmp/agent/table_data.h>
#include <net-snmp/agent/table_dataset.h>
#include "agent_module_includes.h"
#include "mib_module_includes.h"

netsnmp_feature_child_of(agent_read_config_all, libnetsnmpagent)

netsnmp_feature_child_of(snmpd_unregister_config_handler, agent_read_config_all)

#ifdef HAVE_UNISTD_H
void
snmpd_set_agent_user(const char *token, char *cptr)
{
    if (cptr[0] == '#') {
        char           *ecp;
        int             uid;

        uid = strtoul(cptr + 1, &ecp, 10);
        if (*ecp != 0) {
            config_perror("Bad number");
	} else {
	    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_USERID, uid);
	}
#if defined(HAVE_GETPWNAM) && defined(HAVE_PWD_H)
    } else {
        struct passwd  *info;

        info = getpwnam(cptr);
        if (info)
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
                               NETSNMP_DS_AGENT_USERID, info->pw_uid);
        else
            config_perror("User not found in passwd database");
        endpwent();
#endif
    }
}

void
snmpd_set_agent_group(const char *token, char *cptr)
{
    if (cptr[0] == '#') {
        char           *ecp;
        int             gid = strtoul(cptr + 1, &ecp, 10);

        if (*ecp != 0) {
            config_perror("Bad number");
	} else {
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_GROUPID, gid);
	}
#if defined(HAVE_GETGRNAM) && defined(HAVE_GRP_H)
    } else {
        struct group   *info;

        info = getgrnam(cptr);
        if (info)
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
                               NETSNMP_DS_AGENT_GROUPID, info->gr_gid);
        else
            config_perror("Group not found in group database");
        endgrent();
#endif
    }
}
#endif

#ifndef NETSNMP_NO_LISTEN_SUPPORT
void
snmpd_set_agent_address(const char *token, char *cptr)
{
    char            buf[SPRINT_MAX_LEN];
    char           *ptr;

    /*
     * has something been specified before? 
     */
    ptr = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
				NETSNMP_DS_AGENT_PORTS);

    if (ptr) {
        /*
         * append to the older specification string 
         */
        snprintf(buf, sizeof(buf), "%s,%s", ptr, cptr);
	buf[sizeof(buf) - 1] = '\0';
    } else {
        strlcpy(buf, cptr, sizeof(buf));
    }

    DEBUGMSGTL(("snmpd_ports", "port spec: %s\n", buf));
    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, 
			  NETSNMP_DS_AGENT_PORTS, buf);
}
#endif /* NETSNMP_NO_LISTEN_SUPPORT */

void
init_agent_read_config(const char *app)
{
    if (app != NULL) {
        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, 
			      NETSNMP_DS_LIB_APPTYPE, app);
    } else {
        app = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				    NETSNMP_DS_LIB_APPTYPE);
    }

    register_app_config_handler("authtrapenable",
                                snmpd_parse_config_authtrap, NULL,
                                "1 | 2\t\t(1 = enable, 2 = disable)");
    register_app_config_handler("pauthtrapenable",
                                snmpd_parse_config_authtrap, NULL, NULL);


    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_ROLE) == MASTER_AGENT) {
#ifndef NETSNMP_DISABLE_SNMPV1
        register_app_config_handler("trapsink",
                                    snmpd_parse_config_trapsink,
                                    snmpd_free_trapsinks,
                                    "host [community] [port]");
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
        register_app_config_handler("trap2sink",
                                    snmpd_parse_config_trap2sink, 
                                    snmpd_free_trapsinks,
                                    "host [community] [port]");
        register_app_config_handler("informsink",
                                    snmpd_parse_config_informsink,
                                    snmpd_free_trapsinks,
                                    "host [community] [port]");
#endif
        register_app_config_handler("trapsess",
                                    snmpd_parse_config_trapsess,
                                    snmpd_free_trapsinks,
                                    "[snmpcmdargs] host");
    }
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    register_app_config_handler("trapcommunity",
                                snmpd_parse_config_trapcommunity,
                                snmpd_free_trapcommunity,
                                "community-string");
#endif /* support for community based SNMP */
    netsnmp_ds_register_config(ASN_OCTET_STR, app, "v1trapaddress", 
                               NETSNMP_DS_APPLICATION_ID, 
                               NETSNMP_DS_AGENT_TRAP_ADDR);
#ifdef HAVE_UNISTD_H
    register_app_config_handler("agentuser",
                                snmpd_set_agent_user, NULL, "userid");
    register_app_config_handler("agentgroup",
                                snmpd_set_agent_group, NULL, "groupid");
#endif
#ifndef NETSNMP_NO_LISTEN_SUPPORT
    register_app_config_handler("agentaddress",
                                snmpd_set_agent_address, NULL,
                                "SNMP bind address");
#endif /* NETSNMP_NO_LISTEN_SUPPORT */
    netsnmp_ds_register_config(ASN_BOOLEAN, app, "quit", 
			       NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_QUIT_IMMEDIATELY);
    netsnmp_ds_register_config(ASN_BOOLEAN, app, "leave_pidfile", 
			       NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_LEAVE_PIDFILE);
    netsnmp_ds_register_config(ASN_BOOLEAN, app, "dontLogTCPWrappersConnects",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DONT_LOG_TCPWRAPPERS_CONNECTS);
    netsnmp_ds_register_config(ASN_INTEGER, app, "maxGetbulkRepeats",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_MAX_GETBULKREPEATS);
    netsnmp_ds_register_config(ASN_INTEGER, app, "maxGetbulkResponses",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_MAX_GETBULKRESPONSES);
    netsnmp_init_handler_conf();

#include "agent_module_dot_conf.h"
#include "mib_module_dot_conf.h"
#ifdef TESTING
    print_config_handlers();
#endif
}

void
update_config(void)
{
    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                        SNMPD_CALLBACK_PRE_UPDATE_CONFIG, NULL);
    free_config();
    read_configs();
}


void
snmpd_register_config_handler(const char *token,
                              void (*parser) (const char *, char *),
                              void (*releaser) (void), const char *help)
{
    DEBUGMSGTL(("snmpd_register_app_config_handler",
                "registering .conf token for \"%s\"\n", token));
    register_app_config_handler(token, parser, releaser, help);
}

void
snmpd_register_const_config_handler(const char *token,
                                    void (*parser) (const char *, const char *),
                                    void (*releaser) (void), const char *help)
{
    DEBUGMSGTL(("snmpd_register_app_config_handler",
                "registering .conf token for \"%s\"\n", token));
    register_app_config_handler(token, (void(*)(const char *, char *))parser,
                                releaser, help);
}

#ifdef NETSNMP_FEATURE_REQUIRE_SNMPD_UNREGISTER_CONFIG_HANDLER
netsnmp_feature_require(unregister_app_config_handler)
#endif /* NETSNMP_FEATURE_REQUIRE_SNMPD_UNREGISTER_CONFIG_HANDLER */

#ifndef NETSNMP_FEATURE_REMOVE_SNMPD_UNREGISTER_CONFIG_HANDLER
void
snmpd_unregister_config_handler(const char *token)
{
    unregister_app_config_handler(token);
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMPD_UNREGISTER_CONFIG_HANDLER */

/*
 * this function is intended for use by mib-modules to store permenant
 * configuration information generated by sets or persistent counters 
 */
void
snmpd_store_config(const char *line)
{
    read_app_config_store(line);
}
