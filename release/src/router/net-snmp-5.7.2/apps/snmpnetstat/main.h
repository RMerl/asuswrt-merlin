
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

extern  netsnmp_session *ss;
NETSNMP_IMPORT
int netsnmp_query_get(    netsnmp_variable_list *list,
                          netsnmp_session       *session);
NETSNMP_IMPORT
int netsnmp_query_getnext(netsnmp_variable_list *list,
                          netsnmp_session       *session);
NETSNMP_IMPORT
int netsnmp_query_walk(   netsnmp_variable_list *list,
                          netsnmp_session       *session);

#ifndef AF_INET6
#define AF_INET6	10
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST      1025
#endif
