/*
 * usmUser.h
 *
 */

#ifndef _MIBGROUP_USMUSER_H
#define _MIBGROUP_USMUSER_H

#include <net-snmp/library/snmpusm.h>

/*
 * <...prefix>.<engineID_length>.<engineID>.<user_name_length>.<user_name>
 * = 1 + 32 + 1 + 32 
 */
#define USM_LENGTH_OID_MAX	66

/*
 * we use header_generic from the util_funcs module
 */

config_require(util_funcs/header_generic)
config_add_mib(SNMP-USER-BASED-SM-MIB)

    /*
     * Magic number definitions: 
     */
#define   USMUSERSPINLOCK       1
#define   USMUSERSECURITYNAME   2
#define   USMUSERCLONEFROM      3
#define   USMUSERAUTHPROTOCOL   4
#define   USMUSERAUTHKEYCHANGE  5
#define   USMUSEROWNAUTHKEYCHANGE  6
#define   USMUSERPRIVPROTOCOL   7
#define   USMUSERPRIVKEYCHANGE  8
#define   USMUSEROWNPRIVKEYCHANGE  9
#define   USMUSERPUBLIC         10
#define   USMUSERSTORAGETYPE    11
#define   USMUSERSTATUS         12
    /*
     * function definitions 
     */
     extern void     init_usmUser(void);
     extern FindVarMethod var_usmUser;
     void init_register_usmUser_context(const char *contextName);

     void            shutdown_usmUser(void);
     int             store_usmUser(int majorID, int minorID,
                                   void *serverarg, void *clientarg);
     oid            *usm_generate_OID(oid * prefix, size_t prefixLen,
                                      struct usmUser *uptr,
                                      size_t * length);
     int             usm_parse_oid(oid * oidIndex, size_t oidLen,
                                   unsigned char **engineID,
                                   size_t * engineIDLen,
                                   unsigned char **name, size_t * nameLen);

#ifndef NETSNMP_NO_WRITE_SUPPORT 
     WriteMethod     write_usmUserSpinLock;
     WriteMethod     write_usmUserCloneFrom;
     WriteMethod     write_usmUserAuthProtocol;
     WriteMethod     write_usmUserAuthKeyChange;
     WriteMethod     write_usmUserPrivProtocol;
     WriteMethod     write_usmUserPrivKeyChange;
     WriteMethod     write_usmUserPublic;
     WriteMethod     write_usmUserStorageType;
     WriteMethod     write_usmUserStatus;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 

#endif                          /* _MIBGROUP_USMUSER_H */
