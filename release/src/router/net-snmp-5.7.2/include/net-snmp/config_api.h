#ifndef NET_SNMP_CONFIG_API_H
#define NET_SNMP_CONFIG_API_H

    /**
     *  Library API routines concerned with configuration and control
     *    of the behaviour of the library, agent and other applications.
     */

#include <net-snmp/types.h>

#ifdef __cplusplus
extern          "C" {
#endif

    /* Config Handlers */
    NETSNMP_IMPORT
    struct config_line *register_config_handler(const char *filePrefix,
                                                const char *token,
                                                void (*parser) (const char *, char *),
                                                void (*releaser) (void),
                                                const char *usageLine);
    NETSNMP_IMPORT
    struct config_line *register_const_config_handler(const char *filePrefix,
                                  const char *token,
                                  void (*parser) (const char *, const char *),
                                  void (*releaser) (void),
                                  const char *usageLine);
    NETSNMP_IMPORT
    struct config_line *register_prenetsnmp_mib_handler(const char *filePrefix,
                                                const char *token,
                                                void (*parser) (const char *, char *),
                                                void (*releaser) (void),
                                                const char *usageLine);
    NETSNMP_IMPORT
    void            unregister_config_handler(const char *filePrefix, const char *token);

    				/* Defined in mib.c, rather than read_config.c */
    void            register_mib_handlers(void);
    void            unregister_all_config_handlers(void);

    /* Application Handlers */
    NETSNMP_IMPORT
    struct config_line *register_app_config_handler(
                                                const char *token,
                                                void (*parser) (const char *, char *),
                                                void (*releaser) (void),
                                                const char *usageLine);

    NETSNMP_IMPORT
    struct config_line *register_app_prenetsnmp_mib_handler(
                                                const char *token,
                                                void (*parser) (const char *, char *),
                                                void (*releaser) (void),
                                                const char *usageLine);
    NETSNMP_IMPORT
    void            unregister_app_config_handler(                    const char *token);

    /* Reading Config Files */
    NETSNMP_IMPORT
    void            read_configs(void);
    NETSNMP_IMPORT
    void            read_premib_configs(void);

    /* Help Strings and Errors */
    NETSNMP_IMPORT
    void            read_config_print_usage(const char *lead);
    NETSNMP_IMPORT
    void            config_perror(const char *);
    NETSNMP_IMPORT
    void            config_pwarn(const char *);

#ifdef __cplusplus
}
#endif

    /*
     *    Having extracted the main ("public API") calls relevant
     *  to this area of the Net-SNMP project, the next step is to
     *  identify the related "public internal API" routines.
     *
     *    In due course, these should probably be gathered
     *  together into a companion 'library/config_api.h' header file.
     *  [Or some suitable name]
     *
     *    But for the time being, the expectation is that the
     *  traditional headers that provided the above definitions
     *  will probably also cover the relevant internal API calls.
     *  Hence they are listed here:
     */
#include <net-snmp/library/snmp_api.h>

#include <net-snmp/library/read_config.h>
#include <net-snmp/library/default_store.h>

#include <stdio.h>              /* for FILE definition */
#include <net-snmp/library/snmp_parse_args.h>
#include <net-snmp/library/snmp_enum.h>
#include <net-snmp/library/vacm.h>

#endif                          /* NET_SNMP_CONFIG_API_H */
