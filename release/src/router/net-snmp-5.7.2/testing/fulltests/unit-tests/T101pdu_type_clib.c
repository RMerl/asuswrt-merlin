/* HEADER PDU type descriptions */
const char *pdu_description;
pdu_description = snmp_pdu_type(SNMP_MSG_GET);
OKF((strcmp(pdu_description, "GET") == 0),
    ("SNMP GET PDU type did not return the expected identifier (GET): %s",
     pdu_description));

#ifdef NETSNMP_NO_WRITE_SUPPORT
pdu_description = snmp_pdu_type(163);
OKF((strcmp(pdu_description, "?0xA3?") == 0),
    ("SNMP SET PDU type did not return the expected unknown identifier (?0xA3?): %s",
     pdu_description));
#endif /* NETSNMP_NO_WRITE_SUPPORT */
