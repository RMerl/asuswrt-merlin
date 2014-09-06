/* HEADER Parsing of PDUs */
netsnmp_pdu pdu;
int rc;
u_char data[] = { 
    0xA2, 0x1D, 0x02, 0x04, 0x4E, 0x39,
    0xB2, 0x8E, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00,
    0x30, 0x0F, 0x30, 0x0D, 0x06, 0x08, 0x2B, 0x06,
    0x01, 0x02, 0x01, 0x01, 0x04, 0x00, 0x04, 0x01,
    0x66
};
size_t data_length=sizeof(data);

rc = snmp_pdu_parse(&pdu, data, &data_length);

OKF((rc == 0), ("Parsing of a generic PDU failed"));

#ifdef NETSNMP_NO_WRITE_SUPPORT
data[0] = 0xA3; /* changes it to a SET pdu */
rc = snmp_pdu_parse(&pdu, data, &data_length);

OKF((rc != 0), ("Parsing of a generic SET PDU succeeded"));
#endif /* NETSNMP_NO_WRITE_SUPPORT */
