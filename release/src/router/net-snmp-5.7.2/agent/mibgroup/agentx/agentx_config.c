/*
 *  AgentX Configuration
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

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

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "snmpd.h"
#include "agentx/agentx_config.h"
#include "agentx/protocol.h"

netsnmp_feature_require(user_information)
netsnmp_feature_require(string_time_to_secs)

/* ---------------------------------------------------------------------
 *
 * Common master and sub-agent
 */
void
agentx_parse_agentx_socket(const char *token, char *cptr)
{
    DEBUGMSGTL(("agentx/config", "port spec: %s\n", cptr));
    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_X_SOCKET, cptr);
}

/* ---------------------------------------------------------------------
 *
 * Master agent
 */
#ifdef USING_AGENTX_MASTER_MODULE
void
agentx_parse_master(const char *token, char *cptr)
{
    int             i = -1;

    if (!strcmp(cptr, "agentx") ||
        !strcmp(cptr, "all") ||
        !strcmp(cptr, "yes") || !strcmp(cptr, "on")) {
        i = 1;
        snmp_log(LOG_INFO, "Turning on AgentX master support.\n");
    } else if (!strcmp(cptr, "no") || !strcmp(cptr, "off"))
        i = 0;
    else
        i = atoi(cptr);

    if (i < 0 || i > 1) {
	netsnmp_config_error("master '%s' unrecognised", cptr);
    } else
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_AGENTX_MASTER, i);
}

void
agentx_parse_agentx_perms(const char *token, char *cptr)
{
    char *socket_perm, *dir_perm, *socket_user, *socket_group;
    int uid = -1;
    int gid = -1;
    int s_perm = -1;
    int d_perm = -1;
    char *st;

    DEBUGMSGTL(("agentx/config", "port permissions: %s\n", cptr));
    socket_perm = strtok_r(cptr, " \t", &st);
    dir_perm    = strtok_r(NULL, " \t", &st);
    socket_user = strtok_r(NULL, " \t", &st);
    socket_group = strtok_r(NULL, " \t", &st);

    if (socket_perm) {
        s_perm = strtol(socket_perm, NULL, 8);
        netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                           NETSNMP_DS_AGENT_X_SOCK_PERM, s_perm);
        DEBUGMSGTL(("agentx/config", "socket permissions: %o (%d)\n",
                    s_perm, s_perm));
    }
    if (dir_perm) {
        d_perm = strtol(dir_perm, NULL, 8);
        netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                           NETSNMP_DS_AGENT_X_DIR_PERM, d_perm);
        DEBUGMSGTL(("agentx/config", "directory permissions: %o (%d)\n",
                    d_perm, d_perm));
    }

    /*
     * Try to handle numeric UIDs or user names for the socket owner
     */
    if (socket_user) {
        uid = netsnmp_str_to_uid(socket_user);
        if ( uid != 0 )
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_X_SOCK_USER, uid);
        DEBUGMSGTL(("agentx/config", "socket owner: %s (%d)\n",
                    socket_user, uid));
    }

    /*
     * and similarly for the socket group ownership
     */
    if (socket_group) {
        gid = netsnmp_str_to_gid(socket_group);
        if ( gid != 0 )
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_X_SOCK_GROUP, gid);
        DEBUGMSGTL(("agentx/config", "socket group: %s (%d)\n",
                    socket_group, gid));
    }
}

void
agentx_parse_agentx_timeout(const char *token, char *cptr)
{
    int x = netsnmp_string_time_to_secs(cptr);
    DEBUGMSGTL(("agentx/config/timeout", "%s\n", cptr));
    if (x == -1) {
        config_perror("Invalid timeout value");
        return;
    }
    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                       NETSNMP_DS_AGENT_AGENTX_TIMEOUT, x * ONE_SEC);
}

void
agentx_parse_agentx_retries(const char *token, char *cptr)
{
    int x = atoi(cptr);
    DEBUGMSGTL(("agentx/config/retries", "%s\n", cptr));
    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                       NETSNMP_DS_AGENT_AGENTX_RETRIES, x);
}
#endif                          /* USING_AGENTX_MASTER_MODULE */

/* ---------------------------------------------------------------------
 *
 * Sub-agent
 */


/* ---------------------------------------------------------------------
 *
 * Utility support routines
 */
void
agentx_register_config_handler(const char *token,
                              void (*parser) (const char *, char *),
                              void (*releaser) (void), const char *help)
{
    DEBUGMSGTL(("agentx_register_app_config_handler",
                "registering .conf token for \"%s\"\n", token));
    register_config_handler(":agentx", token, parser, releaser, help);
}

netsnmp_feature_child_of(agentx_unregister_config_handler, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_AGENTX_UNREGISTER_CONFIG_HANDLER
void
agentx_unregister_config_handler(const char *token)
{
    unregister_config_handler(":agentx", token);
}
#endif /* NETSNMP_FEATURE_REMOVE_AGENTX_UNREGISTER_CONFIG_HANDLER */

void
agentx_config_init(void)
{
    int agent_role = netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                            NETSNMP_DS_AGENT_ROLE);

    /*
     * Common tokens for master/subagent
     */
    netsnmp_register_default_domain("agentx", "unix tcp");
    netsnmp_register_default_target("agentx", "unix", NETSNMP_AGENTX_SOCKET);
    netsnmp_register_default_target("agentx", "tcp",
                                    "localhost:" __STRING(AGENTX_PORT));
    agentx_register_config_handler("agentxsocket",
                                  agentx_parse_agentx_socket, NULL,
                                  "AgentX bind address");
#ifdef USING_AGENTX_MASTER_MODULE
    /*
     * tokens for master agent
     */
    if (MASTER_AGENT == agent_role) {
        snmpd_register_config_handler("master",
                                      agentx_parse_master, NULL,
                                      "specify 'agentx' for AgentX support");
    agentx_register_config_handler("agentxperms",
                                  agentx_parse_agentx_perms, NULL,
                                  "AgentX socket permissions: socket_perms [directory_perms [username|userid [groupname|groupid]]]");
    agentx_register_config_handler("agentxRetries",
                                  agentx_parse_agentx_retries, NULL,
                                  "AgentX Retries");
    agentx_register_config_handler("agentxTimeout",
                                  agentx_parse_agentx_timeout, NULL,
                                  "AgentX Timeout (seconds)");
    }
#endif                          /* USING_AGENTX_MASTER_MODULE */

#ifdef USING_AGENTX_SUBAGENT_MODULE
    /*
     * tokens for master agent
     */
    if (SUB_AGENT == agent_role) {
        /*
         * set up callbacks to initiate master agent pings for this session 
         */
        netsnmp_ds_register_config(ASN_INTEGER,
                                   netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                                         NETSNMP_DS_LIB_APPTYPE),
                                   "agentxPingInterval",
                                   NETSNMP_DS_APPLICATION_ID,
                                   NETSNMP_DS_AGENT_AGENTX_PING_INTERVAL);
        /* ping and/or reconnect by default every 15 seconds */
        netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                           NETSNMP_DS_AGENT_AGENTX_PING_INTERVAL, 15);
        
    }
#endif /* USING_AGENTX_SUBAGENT_MODULE */
}
