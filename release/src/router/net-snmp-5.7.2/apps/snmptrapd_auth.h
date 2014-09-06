#ifndef SNMPTRAPD_AUTH_H
#define SNMPTRAPD_AUTH_H

void init_netsnmp_trapd_auth(void);
int netsnmp_trapd_auth(netsnmp_pdu *pdu, netsnmp_transport *transport,
                       netsnmp_trapd_handler *handler);
int netsnmp_trapd_check_auth(int authtypes);

#define TRAP_AUTH_LOG (1 << VACM_VIEW_LOG)      /* displaying and logging */
#define TRAP_AUTH_EXE (1 << VACM_VIEW_EXECUTE)  /* executing code or binaries */
#define TRAP_AUTH_NET (1 << VACM_VIEW_NET)      /* forwarding and net access */

#define TRAP_AUTH_ALL (TRAP_AUTH_LOG | TRAP_AUTH_EXE | TRAP_AUTH_NET)
#define TRAP_AUTH_NONE 0

#endif /* SNMPTRAPD_AUTH_H */
