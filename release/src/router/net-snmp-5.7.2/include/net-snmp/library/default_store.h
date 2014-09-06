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
/*
 * Note:
 *    If new default_store entries are added to this header file,
 *    then remember to run 'perl/default_store/gen' to update the
 *    corresponding perl interface.
 */
/*
 * @file default_store.h: storage space for defaults
 *
 * @addtogroup default_store
 *
 * @{
 */
#ifndef DEFAULT_STORE_H
#define DEFAULT_STORE_H

#include <net-snmp/net-snmp-config.h>

#ifdef __cplusplus
extern          "C" {
#endif

#define NETSNMP_DS_MAX_IDS 3
#define NETSNMP_DS_MAX_SUBIDS 48        /* needs to be a multiple of 8 */

    /*
     * begin storage definitions 
     */
/**
 * @def NETSNMP_DS_LIBRARY_ID
 * These definitions correspond with the "storid" argument to the API.
 */
#define NETSNMP_DS_LIBRARY_ID     0
#define NETSNMP_DS_APPLICATION_ID 1
#define NETSNMP_DS_TOKEN_ID       2

    /*
     * These definitions correspond with the "which" argument to the API,
     * when the storeid argument is NETSNMP_DS_LIBRARY_ID 
     */
    /*
     * library booleans 
     */
#define NETSNMP_DS_LIB_MIB_ERRORS          0
#define NETSNMP_DS_LIB_SAVE_MIB_DESCRS     1
#define NETSNMP_DS_LIB_MIB_COMMENT_TERM    2
#define NETSNMP_DS_LIB_MIB_PARSE_LABEL     3
#define NETSNMP_DS_LIB_DUMP_PACKET         4
#define NETSNMP_DS_LIB_LOG_TIMESTAMP       5
#define NETSNMP_DS_LIB_DONT_READ_CONFIGS   6    /* don't read normal config files */
#define NETSNMP_DS_LIB_DISABLE_CONFIG_LOAD      NETSNMP_DS_LIB_DONT_READ_CONFIGS
#define NETSNMP_DS_LIB_MIB_REPLACE         7    /* replace objects from latest module */
#define NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM  8    /* print only numeric enum values */
#define NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS  9    /* print only numeric enum values */
#define NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS 10   /* dont print oid indexes specially */
#define NETSNMP_DS_LIB_ALARM_DONT_USE_SIG  11   /* don't use the alarm() signal */
#define NETSNMP_DS_LIB_PRINT_FULL_OID      12   /* print fully qualified oids */
#define NETSNMP_DS_LIB_QUICK_PRINT         13   /* print very brief output for parsing */
#define NETSNMP_DS_LIB_RANDOM_ACCESS	   14   /* random access to oid labels */
#define NETSNMP_DS_LIB_REGEX_ACCESS	   15   /* regex matching to oid labels */
#define NETSNMP_DS_LIB_DONT_CHECK_RANGE    16   /* don't check values for ranges on send */
#define NETSNMP_DS_LIB_NO_TOKEN_WARNINGS   17   /* no warn about unknown config tokens */
#define NETSNMP_DS_LIB_NUMERIC_TIMETICKS   18   /* print timeticks as a number */
#define NETSNMP_DS_LIB_ESCAPE_QUOTES       19   /* shell escape quote marks in oids */
#define NETSNMP_DS_LIB_REVERSE_ENCODE      20   /* encode packets from back to front */
#define NETSNMP_DS_LIB_PRINT_BARE_VALUE	   21   /* just print value (not OID = value) */
#define NETSNMP_DS_LIB_EXTENDED_INDEX	   22   /* print extended index format [x1][x2] */
#define NETSNMP_DS_LIB_PRINT_HEX_TEXT      23   /* print ASCII text along with hex strings */
#define NETSNMP_DS_LIB_PRINT_UCD_STYLE_OID 24   /* print OID's using the UCD-style prefix suppression */
#define NETSNMP_DS_LIB_READ_UCD_STYLE_OID  25   /* require top-level OIDs to be prefixed with a dot */
#define NETSNMP_DS_LIB_HAVE_READ_PREMIB_CONFIG 26       /* have the pre-mib parsing config tokens been processed */
#define NETSNMP_DS_LIB_HAVE_READ_CONFIG    27   /* have the config tokens been processed */
#define NETSNMP_DS_LIB_QUICKE_PRINT        28   
#define NETSNMP_DS_LIB_DONT_PRINT_UNITS    29 /* don't print UNITS suffix */
#define NETSNMP_DS_LIB_NO_DISPLAY_HINT     30 /* don't apply DISPLAY-HINTs */
#define NETSNMP_DS_LIB_16BIT_IDS           31   /* restrict requestIDs, etc to 16-bit values */
#define NETSNMP_DS_LIB_DONT_PERSIST_STATE  32	/* don't load config and don't load/save persistent file */
#define NETSNMP_DS_LIB_2DIGIT_HEX_OUTPUT   33	/* print a leading 0 on hex values <= 'f' */
#define NETSNMP_DS_LIB_IGNORE_NO_COMMUNITY 34	/* don't complain if no community is specified in the command arguments */
#define NETSNMP_DS_LIB_DISABLE_PERSISTENT_LOAD  35 /* don't load persistent file */
#define NETSNMP_DS_LIB_DISABLE_PERSISTENT_SAVE  36 /* don't save persistent file */
#define NETSNMP_DS_LIB_APPEND_LOGFILES     37 /* append, don't overwrite, log files */
#define NETSNMP_DS_LIB_NO_DISCOVERY        38 /* don't support RFC5343 contextEngineID discovery */
#define NETSNMP_DS_LIB_TSM_USE_PREFIX      39 /* TSM's simple security name mapping */
#define NETSNMP_DS_LIB_DONT_LOAD_HOST_FILES 40 /* don't read host.conf files */
#define NETSNMP_DS_LIB_DNSSEC_WARN_ONLY     41 /* tread DNSSEC errors as warnings */
#define NETSNMP_DS_LIB_MAX_BOOL_ID          48 /* match NETSNMP_DS_MAX_SUBIDS */

    /*
     * library integers 
     */
#define NETSNMP_DS_LIB_MIB_WARNINGS         0
#define NETSNMP_DS_LIB_SECLEVEL             1
#define NETSNMP_DS_LIB_SNMPVERSION          2
#define NETSNMP_DS_LIB_DEFAULT_PORT         3
#define NETSNMP_DS_LIB_OID_OUTPUT_FORMAT    4
#define NETSNMP_DS_LIB_PRINT_SUFFIX_ONLY    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT
#define NETSNMP_DS_LIB_STRING_OUTPUT_FORMAT 5
#define NETSNMP_DS_LIB_HEX_OUTPUT_LENGTH    6
#define NETSNMP_DS_LIB_SERVERSENDBUF        7 /* send buffer (server) */
#define NETSNMP_DS_LIB_SERVERRECVBUF        8 /* receive buffer (server) */
#define NETSNMP_DS_LIB_CLIENTSENDBUF        9 /* send buffer (client) */
#define NETSNMP_DS_LIB_CLIENTRECVBUF       10 /* receive buffer (client) */
#define NETSNMP_DS_SSHDOMAIN_SOCK_PERM     11
#define NETSNMP_DS_SSHDOMAIN_DIR_PERM      12
#define NETSNMP_DS_SSHDOMAIN_SOCK_USER     12
#define NETSNMP_DS_SSHDOMAIN_SOCK_GROUP    13
#define NETSNMP_DS_LIB_TIMEOUT             14
#define NETSNMP_DS_LIB_RETRIES             15
#define NETSNMP_DS_LIB_MAX_INT_ID          48 /* match NETSNMP_DS_MAX_SUBIDS */
    
    /*
     * special meanings for the default SNMP version slot (NETSNMP_DS_LIB_SNMPVERSION) 
     */
#ifndef NETSNMP_DISABLE_SNMPV1
#define NETSNMP_DS_SNMP_VERSION_1    128        /* bogus */
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
#define NETSNMP_DS_SNMP_VERSION_2c   1  /* real */
#endif
#define NETSNMP_DS_SNMP_VERSION_3    3  /* real */


    /*
     * library strings 
     */
#define NETSNMP_DS_LIB_SECNAME           0
#define NETSNMP_DS_LIB_CONTEXT           1
#define NETSNMP_DS_LIB_PASSPHRASE        2
#define NETSNMP_DS_LIB_AUTHPASSPHRASE    3
#define NETSNMP_DS_LIB_PRIVPASSPHRASE    4
#define NETSNMP_DS_LIB_OPTIONALCONFIG    5
#define NETSNMP_DS_LIB_APPTYPE           6
#define NETSNMP_DS_LIB_COMMUNITY         7
#define NETSNMP_DS_LIB_PERSISTENT_DIR    8
#define NETSNMP_DS_LIB_CONFIGURATION_DIR 9
#define NETSNMP_DS_LIB_SECMODEL          10
#define NETSNMP_DS_LIB_MIBDIRS           11
#define NETSNMP_DS_LIB_OIDSUFFIX         12
#define NETSNMP_DS_LIB_OIDPREFIX         13
#define NETSNMP_DS_LIB_CLIENT_ADDR       14
#define NETSNMP_DS_LIB_TEMP_FILE_PATTERN 15
#define NETSNMP_DS_LIB_AUTHMASTERKEY     16
#define NETSNMP_DS_LIB_PRIVMASTERKEY     17
#define NETSNMP_DS_LIB_AUTHLOCALIZEDKEY  18
#define NETSNMP_DS_LIB_PRIVLOCALIZEDKEY  19
#define NETSNMP_DS_LIB_APPTYPES          20
#define NETSNMP_DS_LIB_KSM_KEYTAB        21
#define NETSNMP_DS_LIB_KSM_SERVICE_NAME  22
#define NETSNMP_DS_LIB_X509_CLIENT_PUB   23
#define NETSNMP_DS_LIB_X509_SERVER_PUB   24
#define NETSNMP_DS_LIB_SSHTOSNMP_SOCKET  25
#define NETSNMP_DS_LIB_CERT_EXTRA_SUBDIR 26
#define NETSNMP_DS_LIB_HOSTNAME          27
#define NETSNMP_DS_LIB_X509_CRL_FILE     28
#define NETSNMP_DS_LIB_TLS_ALGORITMS     29
#define NETSNMP_DS_LIB_TLS_LOCAL_CERT    30
#define NETSNMP_DS_LIB_TLS_PEER_CERT     31
#define NETSNMP_DS_LIB_SSH_USERNAME      32
#define NETSNMP_DS_LIB_SSH_PUBKEY        33
#define NETSNMP_DS_LIB_SSH_PRIVKEY       34
#define NETSNMP_DS_LIB_MAX_STR_ID        48 /* match NETSNMP_DS_MAX_SUBIDS */

    /*
     * end storage definitions 
     */

    NETSNMP_IMPORT
    int             netsnmp_ds_set_boolean(int storeid, int which, int value);
    NETSNMP_IMPORT
    int             netsnmp_ds_get_boolean(int storeid, int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_toggle_boolean(int storeid, int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_set_int(int storeid, int which, int value);
    NETSNMP_IMPORT
    int             netsnmp_ds_get_int(int storeid, int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_set_string(int storeid, int which,
                                  const char *value);
    NETSNMP_IMPORT
    char           *netsnmp_ds_get_string(int storeid, int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_set_void(int storeid, int which, void *value);
    NETSNMP_IMPORT
    void           *netsnmp_ds_get_void(int storeid, int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_register_config(u_char type, const char *ftype,
                                       const char *token, int storeid,
                                       int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_register_premib(u_char type, const char *ftype,
                                       const char *token, int storeid,
                                       int which);
    NETSNMP_IMPORT
    int             netsnmp_ds_parse_boolean(char *line);
    NETSNMP_IMPORT
    void            netsnmp_ds_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif                          /* DEFAULT_STORE_H */
/** @} */
