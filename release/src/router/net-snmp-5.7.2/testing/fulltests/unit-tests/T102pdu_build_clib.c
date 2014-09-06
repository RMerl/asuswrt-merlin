/* HEADER PDU building */

SOCK_STARTUP;

netsnmp_pdu *pdu;
u_char *packet;
size_t packet_len, offset = 0;
netsnmp_session session, *ss;
int rc;

/* prototype copied from snmp_api.c */
int             snmp_build(u_char ** pkt, size_t * pkt_len,
                           size_t * offset, netsnmp_session * pss,
                           netsnmp_pdu *pdu);

init_snmp("testing");
snmp_sess_init(&session);
session.version = SNMP_VERSION_2c;
session.peername = strdup("udp:127.0.0.1"); /* we won't actually connect */
session.community = (u_char *) strdup("bogus");
session.community_len = strlen((char *) session.community);
ss = snmp_open(&session);

OKF((ss != NULL), ("Creating a session failed"));
if (ss == NULL)
    snmp_perror("ack");

packet_len = 4096;
packet = malloc(packet_len);

pdu = snmp_pdu_create(SNMP_MSG_GET);
pdu->version = session.version;

OKF((pdu != NULL), ("Creating a GET PDU failed"));

rc = snmp_build(&packet, &packet_len, &offset, ss, pdu);

#ifndef NETSNMP_NOTIFY_ONLY
OKF((rc == SNMPERR_SUCCESS),
    ("Building a GET PDU/packet should have worked: %d", rc));
#else /* NETSNMP_NOTIFY_ONLY */
OKF((rc != SNMPERR_SUCCESS),
    ("Building a GET PDU/packet should have failed: %d", rc));
#endif /* NETSNMP_NOTIFY_ONLY */

offset = 0;
pdu->command = 163; /* a SET message */
rc = snmp_build(&packet, &packet_len, &offset, ss, pdu);

#ifndef NETSNMP_NO_WRITE_SUPPORT
OKF((rc == SNMPERR_SUCCESS),
    ("Building a SET PDU/packet should have succeeded: %d", rc));
#else /* NETSNMP_NO_WRITE_SUPPORT */
OKF((rc != SNMPERR_SUCCESS),
    ("Building a SET PDU/packet should have failed: %d", rc));
#endif /* NETSNMP_NO_WRITE_SUPPORT */



offset = 0;
pdu->command = SNMP_MSG_INFORM;
rc = snmp_build(&packet, &packet_len, &offset, ss, pdu);

OKF((rc == SNMPERR_SUCCESS),
    ("Building an INFORM PDU/packet should have succeed: %d", rc));

SOCK_CLEANUP;
