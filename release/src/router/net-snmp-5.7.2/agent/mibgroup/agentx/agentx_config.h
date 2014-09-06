#ifndef __AGENTX_CONFIG_H__
#define __AGENTX_CONFIG_H__

config_belongs_in(agent_module)

#ifdef __cplusplus
extern          "C" {
#endif

    void            agentx_parse_master(const char *token, char *cptr);
    void            agentx_parse_agentx_socket(const char *token,
                                               char *cptr);
    void            agentx_config_init(void);

#ifdef __cplusplus
}
#endif
#endif                          /* __AGENTX_CONFIG_H__ */
