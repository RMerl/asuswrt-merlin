/*
 * SNMPv3 View-based Access Control Model
 */

#ifndef _MIBGROUP_VACM_CONF_H
#define _MIBGROUP_VACM_CONF_H

#include <net-snmp/library/vacm.h>

config_belongs_in(agent_module)

#define VACM_CREATE_SIMPLE_V3       1
#define VACM_CREATE_SIMPLE_COM      2
#define VACM_CREATE_SIMPLE_COMIPV4  3
#define VACM_CREATE_SIMPLE_COMIPV6  4
#define VACM_CREATE_SIMPLE_COMUNIX  5

     void            init_vacm_conf(void);
     void            init_vacm_config_tokens(void);
     void            vacm_free_group(void);
     void            vacm_free_access(void);
     void            vacm_free_view(void);
     void            vacm_parse_group(const char *, char *);
     void            vacm_parse_access(const char *, char *);
     void            vacm_parse_setaccess(const char *, char *);
     void            vacm_parse_view(const char *, char *);
     void            vacm_parse_rocommunity(const char *, char *);
     void            vacm_parse_rwcommunity(const char *, char *);
     void            vacm_parse_rocommunity6(const char *, char *);
     void            vacm_parse_rwcommunity6(const char *, char *);
     void            vacm_parse_rouser(const char *, char *);
     void            vacm_parse_rwuser(const char *, char *);
     void            vacm_create_simple(const char *, char *, int, int);
     void            vacm_parse_authcommunity(const char *, char *);
     void            vacm_parse_authuser(const char *, char *);
     void            vacm_parse_authgroup(const char *, char *);
     void            vacm_parse_authaccess(const char *, char *);

     SNMPCallback    vacm_in_view_callback;
     SNMPCallback    vacm_warn_if_not_configured;
     SNMPCallback    vacm_standard_views;

     int             vacm_in_view(netsnmp_pdu *, oid *, size_t, int);
     int             vacm_check_view(netsnmp_pdu *, oid *, size_t, int, int);
     int             vacm_check_view_contents(netsnmp_pdu *, oid *, size_t,
                                              int, int, int);

#define VACM_CHECK_VIEW_CONTENTS_NO_FLAGS        0
#define VACM_CHECK_VIEW_CONTENTS_DNE_CONTEXT_OK  1

#endif                          /* _MIBGROUP_VACM_CONF_H */
