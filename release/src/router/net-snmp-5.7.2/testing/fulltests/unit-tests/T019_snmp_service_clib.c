/* HEADER Testing domain and target registrations from snmp_service. */

/* Setup configuration reading */
netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                      NETSNMP_DS_LIB_APPTYPE, "testprog");

netsnmp_register_service_handlers();

netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                       NETSNMP_DS_LIB_HAVE_READ_CONFIG, 1);

/* Clear all default values */
netsnmp_clear_default_domain();
netsnmp_clear_default_target();

/* Test domain lookup */

OK(netsnmp_register_default_domain("a", "") == 0,
   "register an empty token in domain 'a'");

OK(netsnmp_register_default_domain("b", "alfa") == 0,
    "register a single token in domain 'b'");

OK(netsnmp_register_default_domain("c", "alfa beta") == 0,
   "register two tokens in domain 'c'");

#define OK_STRCMP(name, value, expected)                                \
  OKF(value != NULL && strcmp(expected, value) == 0,                    \
      ("Lookup of '" #name "' expected '" expected "', got %s%s%s",     \
       value ? "'" : "", value ? value : "NULL", value ? "'" : ""))

#define OK_STR_IS_NULL(name, value)                                     \
  OKF(value == NULL,                                                    \
      ("Lookup of '" #name "' expected NULL, got %s%s%s",               \
       value ? "'" : "", value ? value : "NULL", value ? "'" : ""))

#define OK_IS_NULL(name, value)                                         \
  OKF(value == NULL, ("Lookup of '" #name "' expected NULL, got %p", value))

#define OK_IS_NOT_NULL(name, value)                                     \
  OKF(value != NULL, ("Lookup of '" #name "' expected non-NULL, got %p", value))

{
  const char *t;

  t = netsnmp_lookup_default_domain("a");
  OK_STRCMP(a, t, "");

  t = netsnmp_lookup_default_domain("b");
  OK_STRCMP(b, t, "alfa");

  t = netsnmp_lookup_default_domain("c");
  OK_STRCMP(c, t, "alfa");

  t = netsnmp_lookup_default_domain("d");
  OK_STR_IS_NULL(d, t);
}

{
  const char *const *t;

  t = netsnmp_lookup_default_domains("a");
  OK_IS_NOT_NULL(a, t);
  OK_STRCMP(a[0], t[0], "");
  OK_STR_IS_NULL(a[1], t[1]);

  t = netsnmp_lookup_default_domains("b");
  OK_IS_NOT_NULL(b, t);
  OK_STRCMP(b[0], t[0], "alfa");
  OK_STR_IS_NULL(b[1], t[1]);

  t = netsnmp_lookup_default_domains("c");
  OK_IS_NOT_NULL(c, t);
  OK_STRCMP(c[0], t[0], "alfa");
  OK_STRCMP(c[1], t[1], "beta");
  OK_STR_IS_NULL(c[2], t[2]);

  t = netsnmp_lookup_default_domains("d");
  OK_IS_NULL(d, t);
}

{
  char cfg[] = "defDomain=b gamma";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "register user domain for 'b'");
}

{
  char cfg[] = "defDomain=c gamma delta";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "register user domains for 'c'");
}

{
  char cfg[] = "defDomain=b2 gamma";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "register user domain for 'b2'");
}

{
  char cfg[] = "defDomain=c2 gamma delta";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "register user domains for 'c2'");
}

{
  const char *t;

  t = netsnmp_lookup_default_domain("b");
  OK_STRCMP(b, t, "gamma");

  t = netsnmp_lookup_default_domain("b2");
  OK_STRCMP(b2, t, "gamma");

  t = netsnmp_lookup_default_domain("c");
  OK_STRCMP(c, t, "gamma");

  t = netsnmp_lookup_default_domain("c2");
  OK_STRCMP(c2, t, "gamma");
}

{
  const char *const *t;

  t = netsnmp_lookup_default_domains("b");
  OK_IS_NOT_NULL(b, t);
  OK_STRCMP(b[0], t[0], "gamma");
  OK_STR_IS_NULL(b[1], t[1]);

  t = netsnmp_lookup_default_domains("b2");
  OK_IS_NOT_NULL(b2, t);
  OK_STRCMP(b2[0], t[0], "gamma");
  OK_STR_IS_NULL(b2[1], t[1]);

  t = netsnmp_lookup_default_domains("c");
  OK_IS_NOT_NULL(c, t);
  OK_STRCMP(c[0], t[0], "gamma");
  OK_STRCMP(c[1], t[1], "delta");
  OK_STR_IS_NULL(c[2], t[2]);

  t = netsnmp_lookup_default_domains("c2");
  OK_IS_NOT_NULL(c2, t);
  OK_STRCMP(c2[0], t[0], "gamma");
  OK_STRCMP(c2[1], t[1], "delta");
  OK_STR_IS_NULL(c2[2], t[2]);
}

{
  char cfg[] = "defDomain=";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "Empty config line");
}

{
  const char* t;
  char cfg[] = "defDomain=e";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "Incomplete config line");
  t = netsnmp_lookup_default_domain("e");
  OK_STR_IS_NULL(e, t);
}

/* Test target lookup */

OK(netsnmp_register_default_target("b", "alfa", "alfa-domain:b") == 0,
   "register a 'alfa:alfa-domain:b' for application 'b'");

OK(netsnmp_register_default_target("b", "beta", "beta-domain:b") == 0,
   "register a 'beta:beta-domain:b' for application 'b'");

OK(netsnmp_register_default_target("c", "alfa", "alfa-domain:c") == 0,
   "register a 'alfa:alfa-domain:c' for application 'c'");

{
  const char *t;

  t = netsnmp_lookup_default_target("b", "alfa");
  OK_STRCMP(b:alfa, t, "alfa-domain:b");

  t = netsnmp_lookup_default_target("b", "beta");
  OK_STRCMP(b:beta, t, "beta-domain:b");

  t = netsnmp_lookup_default_target("c", "alfa");
  OK_STRCMP(c:alfa, t, "alfa-domain:c");

  t = netsnmp_lookup_default_target("a", "alfa");
  OK_STR_IS_NULL(a:alfa, t);

  t = netsnmp_lookup_default_target("b", "gamma");
  OK_STR_IS_NULL(b:gamma, t);

  t = netsnmp_lookup_default_target("c", "beta");
  OK_STR_IS_NULL(c:beta, t);
}

{
  char cfg[] = "defTarget=b alfa user-alfa:b";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS,
     "register user target 'alfa:user-alfa:b' for application 'b'");
}

{
  char cfg[] = "defTarget=b gamma user-gamma:b";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS,
     "register user target 'gamma:user-gamma:b' for application 'b'");
}

{
  char cfg[] = "defTarget=c alfa user-alfa:c";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS,
     "register user target 'alfa:user-alfa:c' for application 'c'");
}


{
  const char *t;

  t = netsnmp_lookup_default_target("b", "alfa");
  OK_STRCMP(b:alfa, t, "user-alfa:b");

  t = netsnmp_lookup_default_target("b", "beta");
  OK_STRCMP(b:beta, t, "beta-domain:b");

  t = netsnmp_lookup_default_target("c", "alfa");
  OK_STRCMP(c:alfa, t, "user-alfa:c");

  t = netsnmp_lookup_default_target("a", "alfa");
  OK_STR_IS_NULL(a:alfa, t);

  t = netsnmp_lookup_default_target("b", "gamma");
  OK_STRCMP(b:gamma, t, "user-gamma:b");

  t = netsnmp_lookup_default_target("b", "delta");
  OK_STR_IS_NULL(b:delta, t);

  t = netsnmp_lookup_default_target("c", "beta");
  OK_STR_IS_NULL(c:beta, t);
}

{
  char cfg[] = "defTarget=";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "Empty config line");
}

{
  const char* t;
  char cfg[] = "defTarget=e1";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "Incomplete config line #1");
  t = netsnmp_lookup_default_target("e1", "");
  OK_STR_IS_NULL(e1:, t);
}

{
  const char* t;
  char cfg[] = "defTarget=e2 omega";
  OK(netsnmp_config(cfg) == SNMPERR_SUCCESS, "Incomplete config line #2");
  t = netsnmp_lookup_default_target("e2", "omega");
  OK_STR_IS_NULL(e2:omega, t);
}
