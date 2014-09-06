#ifndef _AGENTX_MASTER_H
#define _AGENTX_MASTER_H

config_belongs_in(agent_module)

config_require(agentx/protocol)
config_require(agentx/client)
config_require(agentx/master_admin)
config_require(agentx/agentx_config)

     void            init_master(void);
     void            real_init_master(void);
     Netsnmp_Node_Handler agentx_master_handler;

#endif                          /* _AGENTX_MASTER_H */
