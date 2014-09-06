#ifndef NET_SNMP_MIB_API_H
#define NET_SNMP_MIB_API_H

    /**
     *  Library API routines concerned with MIB files and objects, and OIDs
     */

#include <net-snmp/types.h>

#ifdef __cplusplus
extern          "C" {
#endif

    /* Initialisation and Shutdown */
    NETSNMP_IMPORT
    int             add_mibdir(const char *);

    NETSNMP_IMPORT
    void            netsnmp_init_mib(void);
#ifndef NETSNMP_NO_LEGACY_DEFINITIONS
    NETSNMP_IMPORT
    void            init_mib(void);
    NETSNMP_IMPORT
    void            init_mib_internals(void);
#endif
    NETSNMP_IMPORT
    void            shutdown_mib(void);

     /* Reading and Parsing MIBs */
    NETSNMP_IMPORT
    struct tree    *netsnmp_read_module(const char *);
#ifndef NETSNMP_NO_LEGACY_DEFINITIONS
    NETSNMP_IMPORT
    struct tree    *read_module(const char *);
#endif

    NETSNMP_IMPORT
    struct tree    *read_mib(const char *);
    NETSNMP_IMPORT
    struct tree    *read_all_mibs(void);

    NETSNMP_IMPORT
    void            add_module_replacement(const char *, const char *,
                                           const char *, int);

         /* from ucd-compat.h */
    NETSNMP_IMPORT
    void            snmp_set_mib_warnings(int);
    NETSNMP_IMPORT
    void            snmp_set_mib_errors(int);
    NETSNMP_IMPORT
    void            snmp_set_save_descriptions(int);


     /* Searching the MIB Tree */
    NETSNMP_IMPORT
    int             read_objid(const char *, oid *, size_t *);
    NETSNMP_IMPORT
    oid            *snmp_parse_oid(const char *, oid *, size_t *);
    NETSNMP_IMPORT
    int             get_module_node(const char *, const char *, oid *, size_t *);

     /* Output */
    NETSNMP_IMPORT
    void            print_mib(FILE * fp);

    NETSNMP_IMPORT
    void            print_objid(const oid * objid, size_t objidlen);
    NETSNMP_IMPORT
    void           fprint_objid(FILE * fp,
                                const oid * objid, size_t objidlen);
    NETSNMP_IMPORT
    int           snprint_objid(char *buf, size_t buf_len,
                                const oid * objid, size_t objidlen);

    NETSNMP_IMPORT
    void            print_description(oid * objid, size_t objidlen, int width);
    NETSNMP_IMPORT
    void           fprint_description(FILE * fp,
                                oid * objid, size_t objidlen, int width);
    NETSNMP_IMPORT
    int           snprint_description(char *buf, size_t buf_len,
                                oid * objid, size_t objidlen, int width);

#ifdef __cplusplus
}
#endif

    /*
     *    Having extracted the main ("public API") calls relevant
     *  to this area of the Net-SNMP project, the next step is to
     *  identify the related "public internal API" routines.
     *
     *    In due course, these should probably be gathered
     *  together into a companion 'library/mib_api.h' header file.
     *  [Or some suitable name]
     *
     *    But for the time being, the expectation is that the
     *  traditional headers that provided the above definitions
     *  will probably also cover the relevant internal API calls.
     *  Hence they are listed here:
     */

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/mib.h>
#ifndef NETSNMP_DISABLE_MIB_LOADING
#include <net-snmp/library/parse.h>
#endif
#include <net-snmp/library/callback.h>
#include <net-snmp/library/oid_stash.h>
#include <net-snmp/library/ucd_compat.h>

#endif                          /* NET_SNMP_MIB_API_H */
