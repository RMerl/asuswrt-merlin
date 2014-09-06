#ifndef IQUERY_H
#define IQUERY_H

config_belongs_in(agent_module)

#ifndef NETSNMP_TRANSPORT_CALLBACK_DOMAIN
config_error(utilities/iquery depends on the Callback transport)
#endif

void init_iquery(void);

netsnmp_session *netsnmp_iquery_user_session(      char* secName);
netsnmp_session *netsnmp_iquery_community_session( char* community, int version );
netsnmp_session *netsnmp_iquery_pdu_session(netsnmp_pdu* pdu);
netsnmp_session *netsnmp_iquery_session( char* secName,  int   version,
                                        int   secModel,  int   secLevel,
                                      u_char* engineID, size_t engIDLen);

#endif                          /* IQUERY_H */
