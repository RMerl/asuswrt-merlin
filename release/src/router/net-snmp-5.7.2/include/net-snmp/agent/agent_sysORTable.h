#ifndef AGENT_SYSORTABLE_H
#define AGENT_SYSORTABLE_H

#ifdef __cplusplus
extern          "C" {
#endif

struct sysORTable;

extern void     init_agent_sysORTable(void);
extern void     shutdown_agent_sysORTable(void);

extern void     netsnmp_sysORTable_foreach(void (*)(const struct sysORTable*,
                                                    void*),
                                           void*);

extern int      register_sysORTable(oid *, size_t, const char *);
extern int      unregister_sysORTable(oid *, size_t);
extern int      register_sysORTable_sess(oid *, size_t, const char *,
                                         netsnmp_session *);
extern int      unregister_sysORTable_sess(oid *, size_t, netsnmp_session *);
extern void     unregister_sysORTable_by_session(netsnmp_session *);

#ifdef __cplusplus
}
#endif

#endif /* AGENT_SYSORTABLE_H */
