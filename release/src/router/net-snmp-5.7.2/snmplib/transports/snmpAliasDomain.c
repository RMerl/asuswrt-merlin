#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpAliasDomain.h>

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/utilities.h>
#include <net-snmp/config_api.h>

oid netsnmp_snmpALIASDomain[] = { 1,3,6,1,4,1,8072,3,3,7 };
static netsnmp_tdomain aliasDomain;

/* simple storage mechanism */
static netsnmp_data_list *alias_memory = NULL;

#ifdef HAVE_DMALLOC_H
static void free_wrapper(void * p)
{
    free(p);
}
#else
#define free_wrapper free
#endif

/* An alias parser */
void
parse_alias_config(const char *token, char *line) {
    char aliasname[SPRINT_MAX_LEN];
    char transportdef[SPRINT_MAX_LEN];
    /* copy the first word (the alias) out and then assume the rest is
       transport */
    line = copy_nword(line, aliasname, sizeof(aliasname));
    line = copy_nword(line, transportdef, sizeof(transportdef));
    if (line)
        config_perror("more data than expected");
    netsnmp_data_list_add_node(&alias_memory,
                               netsnmp_create_data_list(aliasname,
                                                        strdup(transportdef),
                                                        &free_wrapper));
}

void
free_alias_config(void) {
    netsnmp_free_all_list_data(alias_memory);
    alias_memory = NULL;
}

/*
 * Open a ALIAS-based transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is 
 * the remote address to send things to.  
 */

netsnmp_transport *
netsnmp_alias_create_tstring(const char *str, int local,
			   const char *default_target)
{
    const char *aliasdata;

    aliasdata = (const char*)netsnmp_get_list_data(alias_memory, str);
    if (!aliasdata) {
        snmp_log(LOG_ERR, "No alias found for %s\n", str);
        return NULL;
    }

    return netsnmp_tdomain_transport(aliasdata,local,default_target);
}



netsnmp_transport *
netsnmp_alias_create_ostring(const u_char * o, size_t o_len, int local)
{
    fprintf(stderr, "make ostring\n");
    return NULL;
}

void
netsnmp_alias_ctor(void)
{
    aliasDomain.name = netsnmp_snmpALIASDomain;
    aliasDomain.name_length = sizeof(netsnmp_snmpALIASDomain) / sizeof(oid);
    aliasDomain.prefix = (const char **)calloc(2, sizeof(char *));
    aliasDomain.prefix[0] = "alias";

    aliasDomain.f_create_from_tstring     = NULL;
    aliasDomain.f_create_from_tstring_new = netsnmp_alias_create_tstring;
    aliasDomain.f_create_from_ostring     = netsnmp_alias_create_ostring;

    netsnmp_tdomain_register(&aliasDomain);

    register_config_handler("snmp", "alias", parse_alias_config,
                            free_alias_config, "NAME TRANSPORT_DEFINITION");
}
