/*
 * agent_read_config.h: reads configuration files for extensible sections.
 *
 */
#ifndef _AGENT_READ_CONFIG_H
#define _AGENT_READ_CONFIG_H

#ifdef __cplusplus
extern          "C" {
#endif

    void            init_agent_read_config(const char *);
    void            update_config(void);
    void            snmpd_register_config_handler(const char *token,
                                                  void (*parser) (const
                                                                  char *,
                                                                  char *),
                                                  void (*releaser) (void),
                                                  const char *help);
    void            snmpd_register_const_config_handler(
                                 const char *,
                                 void (*parser) (const char *, const char *),
                                 void (*releaser) (void),
                                 const char *);
    void            snmpd_unregister_config_handler(const char *);
    void            snmpd_store_config(const char *);

#ifdef __cplusplus
}
#endif
#endif                          /* _AGENT_READ_CONFIG_H */
