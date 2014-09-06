/*
 * snmpv3.h
 */

#ifndef SNMPV3_H
#define SNMPV3_H

#ifdef __cplusplus
extern          "C" {
#endif

#define MAX_ENGINEID_LENGTH 32 /* per SNMP-FRAMEWORK-MIB SnmpEngineID TC */

#define ENGINEID_TYPE_IPV4    1
#define ENGINEID_TYPE_IPV6    2
#define ENGINEID_TYPE_MACADDR 3
#define ENGINEID_TYPE_TEXT    4
#define ENGINEID_TYPE_EXACT   5
#define ENGINEID_TYPE_NETSNMP_RND 128

#define	DEFAULT_NIC "eth0"

    NETSNMP_IMPORT
    int             setup_engineID(u_char ** eidp, const char *text);
    void            engineID_conf(const char *word, char *cptr);
    void            engineBoots_conf(const char *, char *);
    void            engineIDType_conf(const char *, char *);
    void            engineIDNic_conf(const char *, char *);
    NETSNMP_IMPORT
    void            init_snmpv3(const char *);
    int             init_snmpv3_post_config(int majorid, int minorid,
                                            void *serverarg,
                                            void *clientarg);
    int             init_snmpv3_post_premib_config(int majorid,
                                                   int minorid,
                                                   void *serverarg,
                                                   void *clientarg);
    void            shutdown_snmpv3(const char *type);
    int             snmpv3_store(int majorID, int minorID, void *serverarg,
                                 void *clientarg);
    NETSNMP_IMPORT
    u_long          snmpv3_local_snmpEngineBoots(void);
    int             snmpv3_clone_engineID(u_char **, size_t *, u_char *,
                                          size_t);
    NETSNMP_IMPORT
    size_t          snmpv3_get_engineID(u_char * buf, size_t buflen);
    NETSNMP_IMPORT
    u_char         *snmpv3_generate_engineID(size_t *);
    NETSNMP_IMPORT
    u_long          snmpv3_local_snmpEngineTime(void);
    int             get_default_secLevel(void);
    void            snmpv3_set_engineBootsAndTime(int boots, int ttime);
    int             free_engineID(int majorid, int minorid, void *serverarg,
				  void *clientarg);
    NETSNMP_IMPORT
    int             parse_secLevel_conf(const char* word, char *cptr);

#ifdef __cplusplus
}
#endif
#endif                          /* SNMPV3_H */
