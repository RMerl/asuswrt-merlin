#ifndef UCD_SNMP_PROXY_H
#define UCD_SNMP_PROXY_H

struct simple_proxy {
    struct variable2 *variables;
    oid             name[MAX_OID_LEN];
    size_t          name_len;
    oid             base[MAX_OID_LEN];
    size_t          base_len;
    char           *context;
    netsnmp_session *sess;
    struct simple_proxy *next;
};

int             proxy_got_response(int, netsnmp_session *, int,
                                   netsnmp_pdu *, void *);
void            proxy_parse_config(const char *, char *);
void            init_proxy(void);
void            shutdown_proxy(void);
Netsnmp_Node_Handler proxy_handler;

#endif                          /* UCD_SNMP_PROXY_H */
