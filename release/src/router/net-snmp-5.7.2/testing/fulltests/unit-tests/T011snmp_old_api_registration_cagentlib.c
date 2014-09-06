/* HEADER Testing SNMP handler registration via the old API */

static oid Oid[] = { 1, 3, 6, 1, 3, 327 }; /* experimental.327 */
struct variable var_array[] = {
    { 0, 0/*type*/, 0/*acl*/, NULL/*findVar*/, 7, { 1, 3, 6, 1, 3, 327, 1 } },
    { 0, 0/*type*/, 0/*acl*/, NULL/*findVar*/, 7, { 1, 3, 6, 1, 3, 327, 2 } },
    { 0, 0/*type*/, 0/*acl*/, NULL/*findVar*/, 7, { 1, 3, 6, 1, 3, 327, 3 } },
};
netsnmp_session *sess;
int res;

init_snmp("snmp");

sess = calloc(1, sizeof(*sess));
snmp_sess_init(sess);

res = 
netsnmp_register_old_api("exp.327.a",
                         var_array,
                         sizeof(var_array[0]),
                         sizeof(var_array)/sizeof(var_array[0]),
                         Oid,
                         sizeof(Oid)/sizeof(Oid[0]),
                         2, /* priority */
                         0, /* range_subid */
                         0, /* range_ubound */
                         sess,
                         "context", 5/*timeout*/, 0/*flags - ignored*/);
OK(res == SNMPERR_SUCCESS, "Handler registration (1).");

/* Verify that duplicate registration does not cause any havoc. */
res = 
netsnmp_register_old_api("exp.327.b",
                         var_array,
                         sizeof(var_array[0]),
                         sizeof(var_array)/sizeof(var_array[0]),
                         Oid,
                         sizeof(Oid)/sizeof(Oid[0]),
                         2, /* priority */
                         0, /* range_subid */
                         0, /* range_ubound */
                         sess,
                         "context", 5/*timeout*/, 0/*flags - ignored*/);
OK(res == SNMPERR_SUCCESS, "Handler registration (2).");

snmp_shutdown("snmp");
