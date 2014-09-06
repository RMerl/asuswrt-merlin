#ifndef AGENT_TRAP_H
#define AGENT_TRAP_H

#ifdef __cplusplus
extern          "C" {
#endif

struct agent_add_trap_args {
    netsnmp_session *ss;
    int             confirm;
};

void            init_traps(void);
void            send_easy_trap(int, int);
void            send_trap_pdu(netsnmp_pdu *);
void            send_v2trap(netsnmp_variable_list *);
void            send_v3trap(netsnmp_variable_list *vars, const char *context);
void            send_trap_vars(int, int, netsnmp_variable_list *);
void            send_trap_vars_with_context(int trap, int specific, 
                                            netsnmp_variable_list *vars,
                                            const char *context);
void            send_enterprise_trap_vars(int trap, int specific,
                                          const oid * enterprise,
                                          int enterprise_length,
                                          netsnmp_variable_list * vars);
int             netsnmp_send_traps(int trap, int specific,
                          const oid * enterprise, int enterprise_length,
                          netsnmp_variable_list * vars,
                          /* flags are currently unused */
                          const char * context, int flags);
void            snmpd_parse_config_authtrap(const char *, char *);
void            snmpd_parse_config_trapsink(const char *, char *);
void            snmpd_parse_config_trap2sink(const char *, char *);
void            snmpd_parse_config_informsink(const char *, char *);
void            snmpd_parse_config_trapsess(const char *, char *);
void            snmpd_free_trapsinks(void);
void            snmpd_parse_config_trapcommunity(const char *, char *);
void            snmpd_free_trapcommunity(void);
void            send_trap_to_sess(netsnmp_session * sess,
                                  netsnmp_pdu *template_pdu);

int             create_trap_session(char *, u_short, char *, int, int);
int             add_trap_session(netsnmp_session *, int, int, int);
int             remove_trap_session(netsnmp_session *);

void                   convert_v2_to_v1(netsnmp_variable_list *, netsnmp_pdu *);
netsnmp_variable_list *convert_v1_to_v2(netsnmp_pdu *);
netsnmp_pdu    *convert_v2pdu_to_v1(netsnmp_pdu *);
netsnmp_pdu    *convert_v1pdu_to_v2(netsnmp_pdu *);

#ifdef __cplusplus
}
#endif
#endif                          /* AGENT_TRAP_H */
