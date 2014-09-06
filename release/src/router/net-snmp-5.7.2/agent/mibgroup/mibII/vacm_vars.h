/*
 * SNMPv3 View-based Access Control Model
 */

#ifndef _MIBGROUP_VACM_H
#define _MIBGROUP_VACM_H

#include <net-snmp/library/vacm.h>

config_require(util_funcs/header_generic)
config_require(mibII/vacm_context)
config_require(mibII/vacm_conf)
config_add_mib(SNMP-VIEW-BASED-ACM-MIB)
config_add_mib(SNMP-COMMUNITY-MIB)

     void            init_vacm_vars(void);

     extern FindVarMethod var_vacm_sec2group;
     extern FindVarMethod var_vacm_access;
     extern FindVarMethod var_vacm_view;

#ifndef NETSNMP_NO_WRITE_SUPPORT 
     WriteMethod     write_vacmGroupName;
     WriteMethod     write_vacmSecurityToGroupStatus;
     WriteMethod     write_vacmSecurityToGroupStorageType;

     WriteMethod     write_vacmAccessContextMatch;
     WriteMethod     write_vacmAccessNotifyViewName;
     WriteMethod     write_vacmAccessReadViewName;
     WriteMethod     write_vacmAccessWriteViewName;
     WriteMethod     write_vacmAccessStatus;
     WriteMethod     write_vacmAccessStorageType;

     WriteMethod     write_vacmViewSpinLock;
     WriteMethod     write_vacmViewMask;
     WriteMethod     write_vacmViewStatus;
     WriteMethod     write_vacmViewStorageType;
     WriteMethod     write_vacmViewType;

     oid            *access_generate_OID(oid * prefix, size_t prefixLen,
                                         struct vacm_accessEntry *aptr,
                                         size_t * length);
     struct vacm_accessEntry *access_parse_accessEntry(oid * name,
                                                       size_t name_len);
     int             access_parse_oid(oid * oidIndex, size_t oidLen,
                                      unsigned char **groupName,
                                      size_t * groupNameLen,
                                      unsigned char **contextPrefix,
                                      size_t * contextPrefixLen,
                                      int *model, int *level);

     oid            *sec2group_generate_OID(oid * prefix, size_t prefixLen,
                                            struct vacm_groupEntry *geptr,
                                            size_t * length);
     int             sec2group_parse_oid(oid * oidIndex, size_t oidLen,
                                         int *model, unsigned char **name,
                                         size_t * nameLen);
     struct vacm_groupEntry *sec2group_parse_groupEntry(oid * name,
                                                        size_t name_len);

     oid            *view_generate_OID(oid * prefix, size_t prefixLen,
                                       struct vacm_viewEntry *vptr,
                                       size_t * length);
     int             view_parse_oid(oid * oidIndex, size_t oidLen,
                                    unsigned char **viewName,
                                    size_t * viewNameLen, oid ** subtree,
                                    size_t * subtreeLen);
     struct vacm_viewEntry *view_parse_viewEntry(oid * name,
                                                 size_t name_len);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */ 


#define OID_SNMPVACMMIB		SNMP_OID_SNMPMODULES, 16
#define OID_VACMMIBOBJECTS	OID_SNMPVACMMIB, 1

#define OID_VACMCONTEXTTABLE	OID_VACMMIBOBJECTS, 1
#define OID_VACMCONTEXTENTRY	OID_VACMCONTEXTTABLE, 1

#define OID_VACMGROUPTABLE	OID_VACMMIBOBJECTS, 2
#define OID_VACMGROUPENTRY	OID_VACMGROUPTABLE, 1

#define OID_VACMACCESSTABLE	OID_VACMMIBOBJECTS, 4
#define OID_VACMACCESSENTRY	OID_VACMACCESSTABLE, 1

#define OID_VACMMIBVIEWS	OID_VACMMIBOBJECTS, 5
#define OID_VACMVIEWTABLE	OID_VACMMIBVIEWS, 2
#define OID_VACMVIEWENTRY	OID_VACMVIEWTABLE, 1
#define SEC2GROUP_MIB_LENGTH 11
#define ACCESS_MIB_LENGTH 11
#define VIEW_MIB_LENGTH 12
#define CM_EXACT 1
#define CM_PREFIX 2

#endif                          /* _MIBGROUP_VACM_H */
