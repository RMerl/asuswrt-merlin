/* HEADER Testing table_dataset */

/*
 * Note: the primary purpose of this unit test is to verify whether all memory
 * that is allocated for storing a table dataset is freed properly. Feel free
 * to extend this unit test such that it tests more of the table dataset code.
 */

enum { COL3 = 2, COL4 = 3, COL5 = 4, };

static const oid Oid[] = { 1, 3, 6, 1, 3, 277 };
netsnmp_table_data_set* tds;
netsnmp_handler_registration* th;
netsnmp_table_row* row;
int32_t ival;
int32_t i, j;

init_agent("snmpd");
init_snmp("snmpd");

tds = netsnmp_create_table_data_set("table_dataset unit-test");
OK(tds, "table data set creation");

netsnmp_table_dataset_add_index(tds, ASN_INTEGER);
netsnmp_table_dataset_add_index(tds, ASN_INTEGER);
netsnmp_table_set_add_default_row(tds, COL3, ASN_INTEGER,   FALSE, NULL, 0);
netsnmp_table_set_add_default_row(tds, COL4, ASN_COUNTER,   FALSE, NULL, 0);
netsnmp_table_set_add_default_row(tds, COL5, ASN_OCTET_STR, FALSE, NULL, 0);

th = netsnmp_create_handler_registration("unit-test handler", NULL, Oid,
                                         OID_LENGTH(Oid), HANDLER_CAN_RWRITE);
OK(th, "table handler registration");

OK(netsnmp_register_table_data_set(th, tds, NULL) == SNMPERR_SUCCESS,
   "table data set registration");

for (i = 1; i <= 2; i++) {
    for (j = 1; j <= 2; j++) {
        row = netsnmp_create_table_data_row();
        netsnmp_table_row_add_index(row, ASN_INTEGER, &i, sizeof(i));
        netsnmp_table_row_add_index(row, ASN_INTEGER, &j, sizeof(j));
        netsnmp_table_dataset_add_row(tds, row);

        ival = 10 * i + j;
        OK(netsnmp_set_row_column(row, COL3, ASN_INTEGER, &ival, sizeof(ival))
           == SNMPERR_SUCCESS, "set INTEGER column");
        OK(netsnmp_set_row_column(row, COL5, ASN_OCTET_STR, "test",
                                  sizeof("test") - 1)
           == SNMPERR_SUCCESS, "set OCTET_STR column");
    }
}

netsnmp_delete_table_data_set(tds);

snmp_shutdown("snmpd");
shutdown_agent();

OK(TRUE, "done");
