/*
 *  UCD compatability definitions & declarations
 *
 */

#ifndef NET_SNMP_UCD_COMPAT_H
#define NET_SNMP_UCD_COMPAT_H

#ifdef __cplusplus
extern          "C" {
#endif

        /*
         * from snmp_api.h 
         */
NETSNMP_IMPORT
void            snmp_set_dump_packet(int);
NETSNMP_IMPORT
int             snmp_get_dump_packet(void);
NETSNMP_IMPORT
void            snmp_set_quick_print(int);
NETSNMP_IMPORT
int             snmp_get_quick_print(void);
NETSNMP_IMPORT
void            snmp_set_suffix_only(int);
NETSNMP_IMPORT
int             snmp_get_suffix_only(void);
NETSNMP_IMPORT
void            snmp_set_full_objid(int);
int             snmp_get_full_objid(void);
NETSNMP_IMPORT
void            snmp_set_random_access(int);
NETSNMP_IMPORT
int             snmp_get_random_access(void);

        /*
         * from parse.h 
         */
NETSNMP_IMPORT
void            snmp_set_mib_warnings(int);
NETSNMP_IMPORT
void            snmp_set_mib_errors(int);
NETSNMP_IMPORT
void            snmp_set_save_descriptions(int);
void            snmp_set_mib_comment_term(int);
void            snmp_set_mib_parse_label(int);

#ifdef __cplusplus
}
#endif
#endif                          /* NET_SNMP_UCD_COMPAT_H */
