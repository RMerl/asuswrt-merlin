#ifndef OLD_API_H
#define OLD_API_H

#ifdef __cplusplus
extern          "C" {
#endif

#define OLD_API_NAME "old_api"

typedef struct netsnmp_old_api_info_s {
    struct variable *var;
    size_t          varsize;
    size_t          numvars;

    /*
     * old stuff 
     */
    netsnmp_session *ss;
    int             flags;
} netsnmp_old_api_info;

typedef struct old_opi_cache_s {
    u_char         *data;
    WriteMethod    *write_method;
} netsnmp_old_api_cache;

int             netsnmp_register_old_api(const char *moduleName,
                                         struct variable *var,
                                         size_t varsize,
                                         size_t numvars,
                                         const oid * mibloc,
                                         size_t mibloclen,
                                         int priority,
                                         int range_subid,
                                         oid range_ubound,
                                         netsnmp_session * ss,
                                         const char *context,
                                         int timeout, int flags);
Netsnmp_Node_Handler netsnmp_old_api_helper;

/*
 * really shouldn't be used 
 */
netsnmp_agent_session *netsnmp_get_current_agent_session(void);

#ifdef __cplusplus
}
#endif
#endif                          /* OLD_API_H */
