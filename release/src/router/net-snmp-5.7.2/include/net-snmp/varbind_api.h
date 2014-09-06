#ifndef NET_SNMP_VARBIND_API_H
#define NET_SNMP_VARBIND_API_H

    /**
     *  Library API routines concerned with variable bindings and values.
     */

#include <net-snmp/types.h>

#ifdef __cplusplus
extern          "C" {
#endif

    /* Creation */
    NETSNMP_IMPORT
    netsnmp_variable_list *
       snmp_pdu_add_variable(netsnmp_pdu *pdu,
                                 const oid * name, size_t name_length,
                                 u_char type,
                                 const void * value, size_t len);
    NETSNMP_IMPORT
    netsnmp_variable_list *
       snmp_varlist_add_variable(netsnmp_variable_list ** varlist,
                                 const oid * name, size_t name_length,
                                 u_char type,
                                 const void * value, size_t len);
    NETSNMP_IMPORT
    netsnmp_variable_list *
       snmp_add_null_var(netsnmp_pdu *pdu,
                                 const oid * name, size_t name_length);
    NETSNMP_IMPORT
    netsnmp_variable_list *
       snmp_clone_varbind(netsnmp_variable_list * varlist);

    /* Setting Values */
    NETSNMP_IMPORT
    int             snmp_set_var_objid(netsnmp_variable_list * var,
                                       const oid * name, size_t name_length);
    NETSNMP_IMPORT
    int             snmp_set_var_value(netsnmp_variable_list * var,
                                       const void * value, size_t len);
    NETSNMP_IMPORT
    int             snmp_set_var_typed_value(netsnmp_variable_list * var,
                                       u_char type,
                                       const void * value, size_t len);
    NETSNMP_IMPORT
    int             snmp_set_var_typed_integer(netsnmp_variable_list * var,
                                       u_char type, long val);

     /* Output */
    NETSNMP_IMPORT
    void            print_variable(const oid * objid, size_t objidlen,
                                   const netsnmp_variable_list * variable);
    NETSNMP_IMPORT
    void           fprint_variable(FILE * fp,
                                   const oid * objid, size_t objidlen,
                                   const netsnmp_variable_list * variable);
    NETSNMP_IMPORT
    int           snprint_variable(char *buf, size_t buf_len,
                                   const oid * objid, size_t objidlen,
                                   const netsnmp_variable_list * variable);

    NETSNMP_IMPORT
    void             print_value(const oid * objid, size_t objidlen,
                                 const netsnmp_variable_list * variable);
    NETSNMP_IMPORT
    void            fprint_value(FILE * fp,
                                 const oid * objid, size_t objidlen,
                                 const netsnmp_variable_list * variable);
    NETSNMP_IMPORT
    int            snprint_value(char *buf, size_t buf_len,
                                 const oid * objid, size_t objidlen,
                                 const netsnmp_variable_list * variable);

           /* See mib_api.h for {,f,sn}print_objid */

    /* Deletion */
    NETSNMP_IMPORT
    void            snmp_free_var(    netsnmp_variable_list *var);     /* frees just this one */
    NETSNMP_IMPORT
    void            snmp_free_varbind(netsnmp_variable_list *varlist); /* frees all in list */

#ifdef __cplusplus
}
#endif

    /*
     *    Having extracted the main ("public API") calls relevant
     *  to this area of the Net-SNMP project, the next step is to
     *  identify the related "public internal API" routines.
     *
     *    In due course, these should probably be gathered
     *  together into a companion 'library/varbind_api.h' header file.
     *  [Or some suitable name]
     *
     *    But for the time being, the expectation is that the
     *  traditional headers that provided the above definitions
     *  will probably also cover the relevant internal API calls.
     *  Hence they are listed here:
     */
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>
#include <net-snmp/library/mib.h>

#endif                          /* NET_SNMP_VARBIND_API_H */
