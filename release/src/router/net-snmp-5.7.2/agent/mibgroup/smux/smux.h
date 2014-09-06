/*
 * Smux module authored by Rohit Dube.
 * Rewritten by Nick Amato <naamato@merit.net>.
 */

#ifndef NETSNMP_TRANSPORT_IPV4BASE_DOMAIN
config_error(smux/smux depends on the IPv4Base transport domain)
#endif

config_belongs_in(agent_module)

#define SMUXPORT 199

#define SMUXMAXPKTSIZE 1500
#define SMUXMAXSTRLEN  1024
#define SMUXMAXPEERS   10

#define SMUX_OPEN 	(ASN_APPLICATION | ASN_CONSTRUCTOR | 0)
#define SMUX_CLOSE      (ASN_APPLICATION | ASN_PRIMITIVE | 1)
#define SMUX_RREQ       (ASN_APPLICATION | ASN_CONSTRUCTOR | 2)
#define SMUX_RRSP       (ASN_APPLICATION | ASN_PRIMITIVE | 3)
#define SMUX_SOUT       (ASN_APPLICATION | ASN_PRIMITIVE | 4)

#define SMUX_GET        (ASN_CONTEXT | ASN_CONSTRUCTOR | 0)
#define SMUX_GETNEXT    (ASN_CONTEXT | ASN_CONSTRUCTOR | 1)
#define SMUX_GETRSP     (ASN_CONTEXT | ASN_CONSTRUCTOR | 2)
#define SMUX_SET	(ASN_CONTEXT | ASN_CONSTRUCTOR | 3)
#define SMUX_TRAP	(ASN_CONTEXT | ASN_CONSTRUCTOR | 4)

#define SMUXC_GOINGDOWN                    0
#define SMUXC_UNSUPPORTEDVERSION           1
#define SMUXC_PACKETFORMAT                 2
#define SMUXC_PROTOCOLERROR                3
#define SMUXC_INTERNALERROR                4
#define SMUXC_AUTHENTICATIONFAILURE        5

#define SMUX_MAX_PEERS          10
#define SMUX_MAX_PRIORITY       2147483647

#define SMUX_REGOP_DELETE		0
#define SMUX_REGOP_REGISTER_RO		1
#define SMUX_REGOP_REGISTER_RW		2

/*
 * Authorized peers read from the config file
 */
typedef struct _smux_peer_auth {
    oid             sa_oid[MAX_OID_LEN];        /* name of peer                 */
    size_t          sa_oid_len; /* length of peer name          */
    char            sa_passwd[SMUXMAXSTRLEN];   /* configured passwd            */
    int             sa_active_fd;       /* the peer using this auth     */
} smux_peer_auth;

/*
 * Registrations
 */
typedef struct _smux_reg {
    oid             sr_name[MAX_OID_LEN];       /* name of subtree              */
    size_t          sr_name_len;        /* length of subtree name       */
    int             sr_priority;        /* priority of registration     */
    int             sr_fd;      /* descriptor of owner          */
    struct _smux_reg *sr_next;  /* next one                     */
} smux_reg;

extern void     init_smux(void);
extern void     real_init_smux(void);
extern int      smux_accept(int);
extern u_char  *smux_snmp_process(int, oid *, size_t *, size_t *, u_char *,
                                  int);
extern int      smux_process(int);
extern void     smux_parse_peer_auth(const char *, char *);
extern void     smux_free_peer_auth(void);

/* Add socket-fd to list */
int smux_snmp_select_list_add(int sd);

/* Remove socket-fd from list */
int smux_snmp_select_list_del(int sd);

/* Returns the count of added socket-fd's in the list */
int smux_snmp_select_list_get_length(void);

/* Returns the socket-fd number from the position of the list */
int smux_snmp_select_list_get_SD_from_List(int pos);

