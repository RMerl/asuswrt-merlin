/* h2xs -b 5.0.6 -O -n NetSNMP::TrapReceiver -x perl_snmptrapd.h */

/* this file was crafted by hand from the contents of the Net-SNMP
   file: apps/snmaptrapd_handlers.h and other headers. */

typedef struct netsnmp_trapd_handler_s netsnmp_trapd_handler;

typedef int (Netsnmp_Trap_Handler)(netsnmp_pdu           *pdu,
                                   netsnmp_transport     *transport,
                                   netsnmp_trapd_handler *handler);

struct netsnmp_trapd_handler_s {
     int  *trapoid;
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

#define NETSNMPTRAPD_AUTH_HANDLER    1
#define NETSNMPTRAPD_PRE_HANDLER     2
#define NETSNMPTRAPD_POST_HANDLER    3

#define NETSNMPTRAPD_HANDLER_OK      1	/* Succeed, & keep going */
#define NETSNMPTRAPD_HANDLER_FAIL    2	/* Failed but keep going */
#define NETSNMPTRAPD_HANDLER_BREAK   3	/* Move to the next list */
#define NETSNMPTRAPD_HANDLER_FINISH  4	/* No further processing */

netsnmp_trapd_handler *netsnmp_add_global_traphandler(int list, Netsnmp_Trap_Handler handler);
netsnmp_trapd_handler *netsnmp_add_default_traphandler(Netsnmp_Trap_Handler handler);
netsnmp_trapd_handler *netsnmp_add_traphandler(Netsnmp_Trap_Handler handler,
                        oid *trapOid, int trapOidLen);
/*
netsnmp_trapd_handler *netsnmp_get_traphandler(oid *trapOid, int trapOidLen);
*/
