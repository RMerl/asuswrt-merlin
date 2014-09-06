/* HEADER Testing SNMP handler registration */

static oid Oid[] = { 1, 3, 6, 1, 3, 327 }; /* experimental.327 */
netsnmp_handler_registration *handler, *handler2;
netsnmp_mib_handler *dh;
netsnmp_cache *nc, *nc2;

init_snmp("snmp");

handler = netsnmp_create_handler_registration("experimental.327", NULL,
	Oid, OID_LENGTH(Oid), HANDLER_CAN_RWRITE);
OK(handler != NULL, "Handler creation.");

nc = netsnmp_cache_create(10, NULL, NULL, Oid, OID_LENGTH(Oid));
OK(nc, "netsnmp_cache allocation");
OK(snmp_oid_compare(nc->rootoid, nc->rootoid_len, Oid, OID_LENGTH(Oid)) == 0,
   "Handler private OID.");

handler->handler->myvoid = nc;
netsnmp_cache_handler_owns_cache(handler->handler);

nc2 = handler->handler->myvoid;
OK(nc2, "Handler private data");
OK(snmp_oid_compare(nc2->rootoid, nc2->rootoid_len, Oid, OID_LENGTH(Oid)) == 0,
   "Handler private OID.");

OK(netsnmp_register_instance(handler) == MIB_REGISTERED_OK,
   "MIB registration.");

handler2 = netsnmp_create_handler_registration("experimental.327", NULL,
        Oid, OID_LENGTH(Oid), HANDLER_CAN_RWRITE);
OK(handler2 != NULL, "Second registration");

OK(netsnmp_register_instance(handler2) == MIB_DUPLICATE_REGISTRATION,
   "Duplicate MIB registration.");

dh = netsnmp_handler_dup(handler->handler);
OK(dh, "Handler duplication.");

OK(netsnmp_unregister_handler(handler) == SNMPERR_SUCCESS,
   "Handler unregistration.");

netsnmp_handler_free(dh);
OK(TRUE, "Freeing duplicate handler");

snmp_shutdown("snmp");
