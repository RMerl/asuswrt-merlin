#ifndef NETSNMP_SYSORTABLE_H
#define NETSNMP_SYSORTABLE_H

struct sysORTable {
    char            *OR_descr;
    oid             *OR_oid;
    size_t           OR_oidlen;
    netsnmp_session *OR_sess;
    u_long           OR_uptime;
};

struct register_sysOR_parameters {
    char            *descr;
    oid             *name;
    size_t           namelen;
};

#define SYS_ORTABLE_REGISTERED_OK              0
#define SYS_ORTABLE_REGISTRATION_FAILED       -1
#define SYS_ORTABLE_UNREGISTERED_OK            0
#define SYS_ORTABLE_NO_SUCH_REGISTRATION      -1

#include <net-snmp/agent/agent_callbacks.h>

#define REGISTER_SYSOR_TABLE(theoid, len, descr)           \
  do {                                                     \
    struct sysORTable t;                                   \
    t.OR_descr = NETSNMP_REMOVE_CONST(char *, descr);      \
    t.OR_oid = theoid;                                     \
    t.OR_oidlen = len;                                     \
    t.OR_sess = NULL;                                      \
    t.OR_uptime = 0;                                       \
    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,         \
                        SNMPD_CALLBACK_REQ_REG_SYSOR, &t); \
  } while(0);

#define REGISTER_SYSOR_ENTRY(theoid, descr)                     \
  REGISTER_SYSOR_TABLE(theoid, sizeof(theoid) / sizeof(oid),    \
                       descr)

#define UNREGISTER_SYSOR_TABLE(theoid, len)                     \
  do {                                                          \
    struct sysORTable t;                                        \
    t.OR_descr = NULL;                                          \
    t.OR_oid = theoid;                                          \
    t.OR_oidlen = len;                                          \
    t.OR_sess = NULL;                                           \
    t.OR_uptime = 0;                                            \
    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,              \
                        SNMPD_CALLBACK_REQ_UNREG_SYSOR, &t);    \
  } while(0);

#define UNREGISTER_SYSOR_ENTRY(theoid)                          \
  UNREGISTER_SYSOR_TABLE(theoid, sizeof(theoid) / sizeof(oid))

#define UNREGISTER_SYSOR_SESS(sess)                             \
  snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,                \
                      SNMPD_CALLBACK_REQ_UNREG_SYSOR_SESS,      \
                      sess);


#endif /* NETSNMP_SYSORTABLE_H */
