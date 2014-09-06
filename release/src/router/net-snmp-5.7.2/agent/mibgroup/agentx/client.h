#ifndef AGENTX_CLIENT_H
#define AGENTX_CLIENT_H

config_belongs_in(agent_module)

#ifdef __cplusplus
extern          "C" {
#endif
    /*
     *  Utility functions for Agent Extensibility Protocol (RFC 2257)
     *
     */


    int             agentx_open_session(netsnmp_session *);
    int             agentx_close_session(netsnmp_session *, int);
    int             agentx_register(netsnmp_session *, oid *, size_t, int,
                                    int, oid, int, u_char, const char *);
    int             agentx_unregister(netsnmp_session *, oid *, size_t,
                                      int, int, oid, const char *);
    netsnmp_variable_list *agentx_register_index(netsnmp_session *,
                                                 netsnmp_variable_list *,
                                                 int);
    int             agentx_unregister_index(netsnmp_session *,
                                            netsnmp_variable_list *);
    int             agentx_add_agentcaps(netsnmp_session *, const oid *, size_t,
                                         const char *);
    int             agentx_remove_agentcaps(netsnmp_session *, const oid *,
                                            size_t);
    int             agentx_send_ping(netsnmp_session *);

#define AGENTX_CLOSE_OTHER    1
#define AGENTX_CLOSE_PARSE    2
#define AGENTX_CLOSE_PROTOCOL 3
#define AGENTX_CLOSE_TIMEOUT  4
#define AGENTX_CLOSE_SHUTDOWN 5
#define AGENTX_CLOSE_MANAGER  6

#ifdef __cplusplus
}
#endif
#endif                          /* AGENTX_CLIENT_H */
