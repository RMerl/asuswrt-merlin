#ifndef _AGENTX_SUBAGENT_H
#define _AGENTX_SUBAGENT_H

config_belongs_in(agent_module)

config_require(agentx/protocol)
config_require(agentx/client)
config_require(agentx/agentx_config)

#ifndef NETSNMP_TRANSPORT_CALLBACK_DOMAIN
config_error(agentx/subagent depends on the Callback transport)
#endif

     int             subagent_init(void);
     int             handle_agentx_packet(int, netsnmp_session *, int,
                                          netsnmp_pdu *, void *);
     SNMPCallback    agentx_register_callback;
     SNMPCallback    agentx_unregister_callback;
     SNMPAlarmCallback agentx_check_session;

#endif                          /* _AGENTX_SUBAGENT_H */
