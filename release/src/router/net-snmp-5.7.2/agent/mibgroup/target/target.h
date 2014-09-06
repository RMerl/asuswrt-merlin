#ifndef SNMP_TARGET_H
#define SNMP_TARGET_H

/*
 * optional filtering function.  Return either TARGET_SKIP or TARGET_KEEP 
 */
typedef int     (TargetFilterFunction) (struct targetAddrTable_struct *
                                        targaddrs,
                                        struct targetParamTable_struct *
                                        param, void *);
#define TARGET_KEEP 0
#define TARGET_SKIP 1


/*
 * utility functions 
 */

netsnmp_session *get_target_sessions(char *taglist, TargetFilterFunction *,
                                     void *filterArg);

config_require(target/snmpTargetAddrEntry target/snmpTargetParamsEntry)

#endif                          /* SNMP_TARGET_H */
