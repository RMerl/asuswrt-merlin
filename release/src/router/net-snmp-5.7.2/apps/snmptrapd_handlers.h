#ifndef SNMPTRAPD_HANDLERS_H
#define SNMPTRAPD_HANDLERS_H

typedef struct netsnmp_trapd_handler_s netsnmp_trapd_handler;

typedef int (Netsnmp_Trap_Handler)(netsnmp_pdu           *pdu,
                                   netsnmp_transport     *transport,
                                   netsnmp_trapd_handler *handler);


#define NETSNMP_TRAPHANDLER_FLAG_MATCH_TREE     0x1
#define NETSNMP_TRAPHANDLER_FLAG_STRICT_SUBTREE 0x2

struct netsnmp_trapd_handler_s {
     oid  *trapoid;
     int   trapoid_len;
     char *token;		/* Or an array of tokens? */
     char *format;		/* Formatting string */
     int   version;		/* ??? */
     int   authtypes;
     int   flags;
     Netsnmp_Trap_Handler *handler;
     void *handler_data;

     netsnmp_trapd_handler *nexth;	/* Next handler for this trap */
             /* Doubly-linked list of traps with registered handlers */
     netsnmp_trapd_handler *prevt;
     netsnmp_trapd_handler *nextt;
};

Netsnmp_Trap_Handler   syslog_handler;
Netsnmp_Trap_Handler   print_handler;
Netsnmp_Trap_Handler   command_handler;
Netsnmp_Trap_Handler   event_handler;
Netsnmp_Trap_Handler   forward_handler;
Netsnmp_Trap_Handler   axforward_handler;
Netsnmp_Trap_Handler   notification_handler;
Netsnmp_Trap_Handler   mysql_handler;

void free_trap1_fmt(void);
void free_trap2_fmt(void);
extern char *print_format1;
extern char *print_format2;

#define NETSNMPTRAPD_AUTH_HANDLER    1
#define NETSNMPTRAPD_PRE_HANDLER     2
#define NETSNMPTRAPD_POST_HANDLER    3
#define NETSNMPTRAPD_DEFAULT_HANDLER 4

#define NETSNMPTRAPD_HANDLER_OK      1	/* Succeed, & keep going */
#define NETSNMPTRAPD_HANDLER_FAIL    2	/* Failed but keep going */
#define NETSNMPTRAPD_HANDLER_BREAK   3	/* Move to the next list */
#define NETSNMPTRAPD_HANDLER_FINISH  4	/* No further processing */

void snmptrapd_register_configs( void );
netsnmp_trapd_handler *netsnmp_add_global_traphandler(int list, Netsnmp_Trap_Handler* handler);
netsnmp_trapd_handler *netsnmp_add_default_traphandler(Netsnmp_Trap_Handler* handler);
netsnmp_trapd_handler *netsnmp_add_traphandler(Netsnmp_Trap_Handler* handler,
                        oid *trapOid, int trapOidLen);
netsnmp_trapd_handler *netsnmp_get_traphandler(oid *trapOid, int trapOidLen);

const char *trap_description(int trap);
void do_external(char *cmd, struct hostent *host,
            netsnmp_pdu *pdu, netsnmp_transport *transport);
int snmp_input(int op, netsnmp_session *session,
           int reqid, netsnmp_pdu *pdu, void *magic);

#endif                          /* SNMPTRAPD_HANDLERS_H */
