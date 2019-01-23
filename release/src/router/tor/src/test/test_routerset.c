#define ROUTERSET_PRIVATE

#include "or.h"
#include "geoip.h"
#include "routerset.h"
#include "routerparse.h"
#include "policies.h"
#include "nodelist.h"
#include "test.h"

#define NS_MODULE routerset

#define NS_SUBMODULE routerset_new

/*
 * Functional (blackbox) test to determine that each member of the routerset
 * is non-NULL
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *rs;
  (void)arg;

  rs = routerset_new();

  tt_ptr_op(rs, OP_NE, NULL);
  tt_ptr_op(rs->list, OP_NE, NULL);
  tt_ptr_op(rs->names, OP_NE, NULL);
  tt_ptr_op(rs->digests, OP_NE, NULL);
  tt_ptr_op(rs->policies, OP_NE, NULL);
  tt_ptr_op(rs->country_names, OP_NE, NULL);

  done:
    routerset_free(rs);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_get_countryname

/*
 * Functional test to strip the braces from a "{xx}" country code string.
 */

static void
NS(test_main)(void *arg)
{
  const char *input;
  char *name;
  (void)arg;

  /* strlen(c) < 4 */
  input = "xxx";
  name = routerset_get_countryname(input);
  tt_ptr_op(name, OP_EQ, NULL);
  tor_free(name);

  /* c[0] != '{' */
  input = "xxx}";
  name = routerset_get_countryname(input);
  tt_ptr_op(name, OP_EQ, NULL);
  tor_free(name);

  /* c[3] != '}' */
  input = "{xxx";
  name = routerset_get_countryname(input);
  tt_ptr_op(name, OP_EQ, NULL);
  tor_free(name);

  /* tor_strlower */
  input = "{XX}";
  name = routerset_get_countryname(input);
  tt_str_op(name, OP_EQ, "xx");
  tor_free(name);

  input = "{xx}";
  name = routerset_get_countryname(input);
  tt_str_op(name, OP_EQ, "xx");
  done:
    tor_free(name);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_refresh_counties, geoip_not_loaded)

/*
 * Structural (whitebox) test for routerset_refresh_counties, when the GeoIP DB
 * is not loaded.
 */

NS_DECL(int, geoip_is_loaded, (sa_family_t family));
NS_DECL(int, geoip_get_n_countries, (void));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  (void)arg;

  NS_MOCK(geoip_is_loaded);
  NS_MOCK(geoip_get_n_countries);

  routerset_refresh_countries(set);

  tt_ptr_op(set->countries, OP_EQ, NULL);
  tt_int_op(set->n_countries, OP_EQ, 0);
  tt_int_op(CALLED(geoip_is_loaded), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_n_countries), OP_EQ, 0);

  done:
    NS_UNMOCK(geoip_is_loaded);
    NS_UNMOCK(geoip_get_n_countries);
    routerset_free(set);
}

static int
NS(geoip_is_loaded)(sa_family_t family)
{
  (void)family;
  CALLED(geoip_is_loaded)++;

  return 0;
}

static int
NS(geoip_get_n_countries)(void)
{
  CALLED(geoip_get_n_countries)++;

  return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_refresh_counties, no_countries)

/*
 * Structural test for routerset_refresh_counties, when there are no countries.
 */

NS_DECL(int, geoip_is_loaded, (sa_family_t family));
NS_DECL(int, geoip_get_n_countries, (void));
NS_DECL(country_t, geoip_get_country, (const char *country));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  (void)arg;

  NS_MOCK(geoip_is_loaded);
  NS_MOCK(geoip_get_n_countries);
  NS_MOCK(geoip_get_country);

  routerset_refresh_countries(set);

  tt_ptr_op(set->countries, OP_NE, NULL);
  tt_int_op(set->n_countries, OP_EQ, 1);
  tt_int_op((unsigned int)(*set->countries), OP_EQ, 0);
  tt_int_op(CALLED(geoip_is_loaded), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_n_countries), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_country), OP_EQ, 0);

  done:
    NS_UNMOCK(geoip_is_loaded);
    NS_UNMOCK(geoip_get_n_countries);
    NS_UNMOCK(geoip_get_country);
    routerset_free(set);
}

static int
NS(geoip_is_loaded)(sa_family_t family)
{
  (void)family;
  CALLED(geoip_is_loaded)++;

  return 1;
}

static int
NS(geoip_get_n_countries)(void)
{
  CALLED(geoip_get_n_countries)++;

  return 1;
}

static country_t
NS(geoip_get_country)(const char *countrycode)
{
  (void)countrycode;
  CALLED(geoip_get_country)++;

  return 1;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_refresh_counties, one_valid_country)

/*
 * Structural test for routerset_refresh_counties, with one valid country.
 */

NS_DECL(int, geoip_is_loaded, (sa_family_t family));
NS_DECL(int, geoip_get_n_countries, (void));
NS_DECL(country_t, geoip_get_country, (const char *country));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  (void)arg;

  NS_MOCK(geoip_is_loaded);
  NS_MOCK(geoip_get_n_countries);
  NS_MOCK(geoip_get_country);
  smartlist_add(set->country_names, tor_strndup("foo", 3));

  routerset_refresh_countries(set);

  tt_ptr_op(set->countries, OP_NE, NULL);
  tt_int_op(set->n_countries, OP_EQ, 2);
  tt_int_op(CALLED(geoip_is_loaded), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_n_countries), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_country), OP_EQ, 1);
  tt_int_op((unsigned int)(*set->countries), OP_NE, 0);

  done:
    NS_UNMOCK(geoip_is_loaded);
    NS_UNMOCK(geoip_get_n_countries);
    NS_UNMOCK(geoip_get_country);
    routerset_free(set);
}

static int
NS(geoip_is_loaded)(sa_family_t family)
{
  (void)family;
  CALLED(geoip_is_loaded)++;

  return 1;
}

static int
NS(geoip_get_n_countries)(void)
{
  CALLED(geoip_get_n_countries)++;

  return 2;
}

static country_t
NS(geoip_get_country)(const char *countrycode)
{
  (void)countrycode;
  CALLED(geoip_get_country)++;

  return 1;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_refresh_counties, one_invalid_country)

/*
 * Structural test for routerset_refresh_counties, with one invalid
 * country code..
 */

NS_DECL(int, geoip_is_loaded, (sa_family_t family));
NS_DECL(int, geoip_get_n_countries, (void));
NS_DECL(country_t, geoip_get_country, (const char *country));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  (void)arg;

  NS_MOCK(geoip_is_loaded);
  NS_MOCK(geoip_get_n_countries);
  NS_MOCK(geoip_get_country);
  smartlist_add(set->country_names, tor_strndup("foo", 3));

  routerset_refresh_countries(set);

  tt_ptr_op(set->countries, OP_NE, NULL);
  tt_int_op(set->n_countries, OP_EQ, 2);
  tt_int_op(CALLED(geoip_is_loaded), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_n_countries), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_country), OP_EQ, 1);
  tt_int_op((unsigned int)(*set->countries), OP_EQ, 0);

  done:
    NS_UNMOCK(geoip_is_loaded);
    NS_UNMOCK(geoip_get_n_countries);
    NS_UNMOCK(geoip_get_country);
    routerset_free(set);
}

static int
NS(geoip_is_loaded)(sa_family_t family)
{
  (void)family;
  CALLED(geoip_is_loaded)++;

  return 1;
}

static int
NS(geoip_get_n_countries)(void)
{
  CALLED(geoip_get_n_countries)++;

  return 2;
}

static country_t
NS(geoip_get_country)(const char *countrycode)
{
  (void)countrycode;
  CALLED(geoip_get_country)++;

  return -1;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, malformed)

/*
 * Functional test, with a malformed string to parse.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  const char *s = "_";
  int r;
  (void)arg;

  r = routerset_parse(set, s, "");

  tt_int_op(r, OP_EQ, -1);

  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, valid_hexdigest)

/*
 * Functional test for routerset_parse, that routerset_parse returns 0
 * on a valid hexdigest entry.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  const char *s;
  int r;
  (void)arg;

  set = routerset_new();
  s = "$0000000000000000000000000000000000000000";
  r = routerset_parse(set, s, "");
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(digestmap_isempty(set->digests), OP_NE, 1);

  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, valid_nickname)

/*
 * Functional test for routerset_parse, when given a valid nickname as input.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  const char *s;
  int r;
  (void)arg;

  set = routerset_new();
  s = "fred";
  r = routerset_parse(set, s, "");
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(strmap_isempty(set->names), OP_NE, 1);

  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, get_countryname)

/*
 * Functional test for routerset_parse, when given a valid countryname.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  const char *s;
  int r;
  (void)arg;

  set = routerset_new();
  s = "{cc}";
  r = routerset_parse(set, s, "");
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(set->country_names), OP_NE, 0);

  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, policy_wildcard)

/*
 * Structural test for routerset_parse, when given a valid wildcard policy.
 */

NS_DECL(addr_policy_t *, router_parse_addr_policy_item_from_string,
    (const char *s, int assume_action, int *malformed_list));

static addr_policy_t *NS(mock_addr_policy);

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  const char *s;
  int r;
  (void)arg;

  NS_MOCK(router_parse_addr_policy_item_from_string);
  NS(mock_addr_policy) = tor_malloc_zero(sizeof(addr_policy_t));

  set = routerset_new();
  s = "*";
  r = routerset_parse(set, s, "");
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(set->policies), OP_NE, 0);
  tt_int_op(CALLED(router_parse_addr_policy_item_from_string), OP_EQ, 1);

  done:
    routerset_free(set);
}

addr_policy_t *
NS(router_parse_addr_policy_item_from_string)(const char *s,
                                              int assume_action,
                                              int *malformed_list)
{
  (void)s;
  (void)assume_action;
  (void)malformed_list;
  CALLED(router_parse_addr_policy_item_from_string)++;

  return NS(mock_addr_policy);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, policy_ipv4)

/*
 * Structural test for routerset_parse, when given a valid IPv4 address
 * literal policy.
 */

NS_DECL(addr_policy_t *, router_parse_addr_policy_item_from_string,
        (const char *s, int assume_action, int *bogus));

static addr_policy_t *NS(mock_addr_policy);

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  const char *s;
  int r;
  (void)arg;

  NS_MOCK(router_parse_addr_policy_item_from_string);
  NS(mock_addr_policy) = tor_malloc_zero(sizeof(addr_policy_t));

  set = routerset_new();
  s = "127.0.0.1";
  r = routerset_parse(set, s, "");
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(set->policies), OP_NE, 0);
  tt_int_op(CALLED(router_parse_addr_policy_item_from_string), OP_EQ, 1);

 done:
  routerset_free(set);
}

addr_policy_t *
NS(router_parse_addr_policy_item_from_string)(const char *s, int assume_action,
                                              int *bogus)
{
  (void)s;
  (void)assume_action;
  CALLED(router_parse_addr_policy_item_from_string)++;
  *bogus = 0;

  return NS(mock_addr_policy);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_parse, policy_ipv6)

/*
 * Structural test for routerset_parse, when given a valid IPv6 address
 * literal policy.
 */

NS_DECL(addr_policy_t *, router_parse_addr_policy_item_from_string,
        (const char *s, int assume_action, int *bad));

static addr_policy_t *NS(mock_addr_policy);

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  const char *s;
  int r;
  (void)arg;

  NS_MOCK(router_parse_addr_policy_item_from_string);
  NS(mock_addr_policy) = tor_malloc_zero(sizeof(addr_policy_t));

  set = routerset_new();
  s = "::1";
  r = routerset_parse(set, s, "");
  tt_int_op(r, OP_EQ, 0);
  tt_int_op(smartlist_len(set->policies), OP_NE, 0);
  tt_int_op(CALLED(router_parse_addr_policy_item_from_string), OP_EQ, 1);

 done:
  routerset_free(set);
}

addr_policy_t *
NS(router_parse_addr_policy_item_from_string)(const char *s,
                                              int assume_action, int *bad)
{
  (void)s;
  (void)assume_action;
  CALLED(router_parse_addr_policy_item_from_string)++;
  *bad = 0;

  return NS(mock_addr_policy);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_union, source_bad)

/*
 * Structural test for routerset_union, when given a bad source argument.
 */

NS_DECL(smartlist_t *, smartlist_new, (void));

static void
NS(test_main)(void *arg)
{
  routerset_t *set, *bad_set;
  (void)arg;

  set = routerset_new();
  bad_set = routerset_new();
  smartlist_free(bad_set->list);
  bad_set->list = NULL;

  NS_MOCK(smartlist_new);

  routerset_union(set, NULL);
  tt_int_op(CALLED(smartlist_new), OP_EQ, 0);

  routerset_union(set, bad_set);
  tt_int_op(CALLED(smartlist_new), OP_EQ, 0);

  done:
    NS_UNMOCK(smartlist_new);
    routerset_free(set);

    /* Just recreate list, so we can simply use routerset_free. */
    bad_set->list = smartlist_new();
    routerset_free(bad_set);
}

static smartlist_t *
NS(smartlist_new)(void)
{
  CALLED(smartlist_new)++;

  return NULL;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_union, one)

/*
 * Functional test for routerset_union.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *src = routerset_new();
  routerset_t *tgt;
  (void)arg;

  tgt = routerset_new();
  smartlist_add(src->list, tor_strdup("{xx}"));
  routerset_union(tgt, src);

  tt_int_op(smartlist_len(tgt->list), OP_NE, 0);

  done:
    routerset_free(src);
    routerset_free(tgt);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_is_list

/*
 * Functional tests for routerset_is_list.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set;
  addr_policy_t *policy;
  int is_list;
  (void)arg;

  /* len(set->country_names) == 0, len(set->policies) == 0 */
  set = routerset_new();
  is_list = routerset_is_list(set);
  routerset_free(set);
  set = NULL;
  tt_int_op(is_list, OP_NE, 0);

  /* len(set->country_names) != 0, len(set->policies) == 0 */
  set = routerset_new();
  smartlist_add(set->country_names, tor_strndup("foo", 3));
  is_list = routerset_is_list(set);
  routerset_free(set);
  set = NULL;
  tt_int_op(is_list, OP_EQ, 0);

  /* len(set->country_names) == 0, len(set->policies) != 0 */
  set = routerset_new();
  policy = tor_malloc_zero(sizeof(addr_policy_t));
  smartlist_add(set->policies, (void *)policy);
  is_list = routerset_is_list(set);
  routerset_free(set);
  set = NULL;
  tt_int_op(is_list, OP_EQ, 0);

  /* len(set->country_names) != 0, len(set->policies) != 0 */
  set = routerset_new();
  smartlist_add(set->country_names, tor_strndup("foo", 3));
  policy = tor_malloc_zero(sizeof(addr_policy_t));
  smartlist_add(set->policies, (void *)policy);
  is_list = routerset_is_list(set);
  routerset_free(set);
  set = NULL;
  tt_int_op(is_list, OP_EQ, 0);

  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_needs_geoip

/*
 * Functional tests for routerset_needs_geoip.
 */

static void
NS(test_main)(void *arg)
{
  const routerset_t *set;
  int needs_geoip;
  (void)arg;

  set = NULL;
  needs_geoip = routerset_needs_geoip(set);
  tt_int_op(needs_geoip, OP_EQ, 0);

  set = routerset_new();
  needs_geoip = routerset_needs_geoip(set);
  routerset_free((routerset_t *)set);
  tt_int_op(needs_geoip, OP_EQ, 0);
  set = NULL;

  set = routerset_new();
  smartlist_add(set->country_names, tor_strndup("xx", 2));
  needs_geoip = routerset_needs_geoip(set);
  routerset_free((routerset_t *)set);
  set = NULL;
  tt_int_op(needs_geoip, OP_NE, 0);

  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_is_empty

/*
 * Functional tests for routerset_is_empty.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = NULL;
  int is_empty;
  (void)arg;

  is_empty = routerset_is_empty(set);
  tt_int_op(is_empty, OP_NE, 0);

  set = routerset_new();
  is_empty = routerset_is_empty(set);
  routerset_free(set);
  set = NULL;
  tt_int_op(is_empty, OP_NE, 0);

  set = routerset_new();
  smartlist_add(set->list, tor_strdup("{xx}"));
  is_empty = routerset_is_empty(set);
  routerset_free(set);
  set = NULL;
  tt_int_op(is_empty, OP_EQ, 0);

  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, null_set_or_null_set_list)

/*
 * Functional test for routerset_contains, when given a NULL set or the
 * set has a NULL list.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = NULL;
  int contains;
  (void)arg;

  contains = routerset_contains(set, NULL, 0, NULL, NULL, 0);

  tt_int_op(contains, OP_EQ, 0);

  set = tor_malloc_zero(sizeof(routerset_t));
  set->list = NULL;
  contains = routerset_contains(set, NULL, 0, NULL, NULL, 0);
  tor_free(set);
  tt_int_op(contains, OP_EQ, 0);

  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_null_nickname)

/*
 * Functional test for routerset_contains, when given a valid routerset but a
 * NULL nickname.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  char *nickname = NULL;
  int contains;
  (void)arg;

  contains = routerset_contains(set, NULL, 0, nickname, NULL, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 0);

  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_nickname)

/*
 * Functional test for routerset_contains, when given a valid routerset
 * and the nickname is in the routerset.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  const char *nickname;
  int contains;
  (void)arg;

  nickname = "Foo";  /* This tests the lowercase comparison as well. */
  strmap_set_lc(set->names, nickname, (void *)1);
  contains = routerset_contains(set, NULL, 0, nickname, NULL, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 4);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_no_nickname)

/*
 * Functional test for routerset_contains, when given a valid routerset
 * and the nickname is not in the routerset.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains;
  (void)arg;

  strmap_set_lc(set->names, "bar", (void *)1);
  contains = routerset_contains(set, NULL, 0, "foo", NULL, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 0);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_digest)

/*
 * Functional test for routerset_contains, when given a valid routerset
 * and the digest is contained in the routerset.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains;
  uint8_t foo[20] = { 2, 3, 4 };
  (void)arg;

  digestmap_set(set->digests, (const char*)foo, (void *)1);
  contains = routerset_contains(set, NULL, 0, NULL, (const char*)foo, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 4);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_no_digest)

/*
 * Functional test for routerset_contains, when given a valid routerset
 * and the digest is not contained in the routerset.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains;
  uint8_t bar[20] = { 9, 10, 11, 55 };
  uint8_t foo[20] = { 1, 2, 3, 4};
  (void)arg;

  digestmap_set(set->digests, (const char*)bar, (void *)1);
  contains = routerset_contains(set, NULL, 0, NULL, (const char*)foo, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 0);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_null_digest)

/*
 * Functional test for routerset_contains, when given a valid routerset
 * and the digest is NULL.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains;
  uint8_t bar[20] = { 9, 10, 11, 55 };
  (void)arg;

  digestmap_set(set->digests, (const char*)bar, (void *)1);
  contains = routerset_contains(set, NULL, 0, NULL, NULL, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 0);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_addr)

/*
 * Structural test for routerset_contains, when given a valid routerset
 * and the address is rejected by policy.
 */

NS_DECL(addr_policy_result_t, compare_tor_addr_to_addr_policy,
    (const tor_addr_t *addr, uint16_t port, const smartlist_t *policy));

static tor_addr_t MOCK_TOR_ADDR;
#define MOCK_TOR_ADDR_PTR (&MOCK_TOR_ADDR)

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  tor_addr_t *addr = MOCK_TOR_ADDR_PTR;
  int contains;
  (void)arg;

  NS_MOCK(compare_tor_addr_to_addr_policy);

  contains = routerset_contains(set, addr, 0, NULL, NULL, 0);
  routerset_free(set);

  tt_int_op(CALLED(compare_tor_addr_to_addr_policy), OP_EQ, 1);
  tt_int_op(contains, OP_EQ, 3);

  done:
    ;
}

addr_policy_result_t
NS(compare_tor_addr_to_addr_policy)(const tor_addr_t *addr, uint16_t port,
    const smartlist_t *policy)
{
  (void)port;
  (void)policy;
  CALLED(compare_tor_addr_to_addr_policy)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);
  return ADDR_POLICY_REJECTED;

  done:
    return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_no_addr)

/*
 * Structural test for routerset_contains, when given a valid routerset
 * and the address is not rejected by policy.
 */

NS_DECL(addr_policy_result_t, compare_tor_addr_to_addr_policy,
    (const tor_addr_t *addr, uint16_t port, const smartlist_t *policy));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  tor_addr_t *addr = MOCK_TOR_ADDR_PTR;
  int contains;
  (void)arg;

  NS_MOCK(compare_tor_addr_to_addr_policy);

  contains = routerset_contains(set, addr, 0, NULL, NULL, 0);
  routerset_free(set);

  tt_int_op(CALLED(compare_tor_addr_to_addr_policy), OP_EQ, 1);
  tt_int_op(contains, OP_EQ, 0);

  done:
    ;
}

addr_policy_result_t
NS(compare_tor_addr_to_addr_policy)(const tor_addr_t *addr, uint16_t port,
    const smartlist_t *policy)
{
  (void)port;
  (void)policy;
  CALLED(compare_tor_addr_to_addr_policy)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);

  return ADDR_POLICY_ACCEPTED;

  done:
      return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, set_and_null_addr)

/*
 * Structural test for routerset_contains, when given a valid routerset
 * and the address is NULL.
 */

NS_DECL(addr_policy_result_t, compare_tor_addr_to_addr_policy,
    (const tor_addr_t *addr, uint16_t port, const smartlist_t *policy));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains;
  (void)arg;

  NS_MOCK(compare_tor_addr_to_addr_policy);

  contains = routerset_contains(set, NULL, 0, NULL, NULL, 0);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 0);

  done:
    ;
}

addr_policy_result_t
NS(compare_tor_addr_to_addr_policy)(const tor_addr_t *addr, uint16_t port,
    const smartlist_t *policy)
{
  (void)port;
  (void)policy;
  CALLED(compare_tor_addr_to_addr_policy)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);

  return ADDR_POLICY_ACCEPTED;

  done:
    return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, countries_no_geoip)

/*
 * Structural test for routerset_contains, when there is no matching country
 * for the address.
 */

NS_DECL(addr_policy_result_t, compare_tor_addr_to_addr_policy,
    (const tor_addr_t *addr, uint16_t port, const smartlist_t *policy));
NS_DECL(int, geoip_get_country_by_addr, (const tor_addr_t *addr));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains = 1;
  (void)arg;

  NS_MOCK(compare_tor_addr_to_addr_policy);
  NS_MOCK(geoip_get_country_by_addr);

  set->countries = bitarray_init_zero(1);
  bitarray_set(set->countries, 1);
  contains = routerset_contains(set, MOCK_TOR_ADDR_PTR, 0, NULL, NULL, -1);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 0);
  tt_int_op(CALLED(compare_tor_addr_to_addr_policy), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_country_by_addr), OP_EQ, 1);

  done:
    ;
}

addr_policy_result_t
NS(compare_tor_addr_to_addr_policy)(const tor_addr_t *addr, uint16_t port,
    const smartlist_t *policy)
{
  (void)port;
  (void)policy;
  CALLED(compare_tor_addr_to_addr_policy)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);

  done:
    return ADDR_POLICY_ACCEPTED;
}

int
NS(geoip_get_country_by_addr)(const tor_addr_t *addr)
{
  CALLED(geoip_get_country_by_addr)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);

  done:
    return -1;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains, countries_geoip)

/*
 * Structural test for routerset_contains, when there a matching country
 * for the address.
 */

NS_DECL(addr_policy_result_t, compare_tor_addr_to_addr_policy,
    (const tor_addr_t *addr, uint16_t port, const smartlist_t *policy));
NS_DECL(int, geoip_get_country_by_addr, (const tor_addr_t *addr));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int contains = 1;
  (void)arg;

  NS_MOCK(compare_tor_addr_to_addr_policy);
  NS_MOCK(geoip_get_country_by_addr);

  set->n_countries = 2;
  set->countries = bitarray_init_zero(1);
  bitarray_set(set->countries, 1);
  contains = routerset_contains(set, MOCK_TOR_ADDR_PTR, 0, NULL, NULL, -1);
  routerset_free(set);

  tt_int_op(contains, OP_EQ, 2);
  tt_int_op(CALLED(compare_tor_addr_to_addr_policy), OP_EQ, 1);
  tt_int_op(CALLED(geoip_get_country_by_addr), OP_EQ, 1);

  done:
    ;
}

addr_policy_result_t
NS(compare_tor_addr_to_addr_policy)(const tor_addr_t *addr, uint16_t port,
    const smartlist_t *policy)
{
  (void)port;
  (void)policy;
  CALLED(compare_tor_addr_to_addr_policy)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);

  done:
    return ADDR_POLICY_ACCEPTED;
}

int
NS(geoip_get_country_by_addr)(const tor_addr_t *addr)
{
  CALLED(geoip_get_country_by_addr)++;
  tt_ptr_op(addr, OP_EQ, MOCK_TOR_ADDR_PTR);

  done:
    return 1;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_add_unknown_ccs, only_flag_and_no_ccs)

/*
 * Functional test for routerset_add_unknown_ccs, where only_if_some_cc_set
 * is set and there are no country names.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  routerset_t **setp = &set;
  int r;
  (void)arg;

  r = routerset_add_unknown_ccs(setp, 1);

  tt_int_op(r, OP_EQ, 0);

  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_add_unknown_ccs, creates_set)

/*
 * Functional test for routerset_add_unknown_ccs, where the set argument
 * is created if passed in as NULL.
 */

/* The mock is only used to stop the test from asserting erroneously. */
NS_DECL(country_t, geoip_get_country, (const char *country));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = NULL;
  routerset_t **setp = &set;
  int r;
  (void)arg;

  NS_MOCK(geoip_get_country);

  r = routerset_add_unknown_ccs(setp, 0);

  tt_ptr_op(*setp, OP_NE, NULL);
  tt_int_op(r, OP_EQ, 0);

  done:
    if (set != NULL)
      routerset_free(set);
}

country_t
NS(geoip_get_country)(const char *country)
{
  (void)country;
  CALLED(geoip_get_country)++;

  return -1;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_add_unknown_ccs, add_unknown)

/*
 * Structural test for routerset_add_unknown_ccs, that the "{??}"
 * country code is added to the list.
 */

NS_DECL(country_t, geoip_get_country, (const char *country));
NS_DECL(int, geoip_is_loaded, (sa_family_t family));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  routerset_t **setp = &set;
  int r;
  (void)arg;

  NS_MOCK(geoip_get_country);
  NS_MOCK(geoip_is_loaded);

  r = routerset_add_unknown_ccs(setp, 0);

  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_contains_string(set->country_names, "??"), OP_EQ, 1);
  tt_int_op(smartlist_contains_string(set->list, "{??}"), OP_EQ, 1);

  done:
    if (set != NULL)
      routerset_free(set);
}

country_t
NS(geoip_get_country)(const char *country)
{
  int arg_is_qq, arg_is_a1;

  CALLED(geoip_get_country)++;

  arg_is_qq = !strcmp(country, "??");
  arg_is_a1 = !strcmp(country, "A1");

  tt_int_op(arg_is_qq || arg_is_a1, OP_EQ, 1);

  if (arg_is_qq)
    return 1;

  done:
    return -1;
}

int
NS(geoip_is_loaded)(sa_family_t family)
{
  CALLED(geoip_is_loaded)++;

  tt_int_op(family, OP_EQ, AF_INET);

  done:
    return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_add_unknown_ccs, add_a1)

/*
 * Structural test for routerset_add_unknown_ccs, that the "{a1}"
 * country code is added to the list.
 */

NS_DECL(country_t, geoip_get_country, (const char *country));
NS_DECL(int, geoip_is_loaded, (sa_family_t family));

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  routerset_t **setp = &set;
  int r;
  (void)arg;

  NS_MOCK(geoip_get_country);
  NS_MOCK(geoip_is_loaded);

  r = routerset_add_unknown_ccs(setp, 0);

  tt_int_op(r, OP_EQ, 1);
  tt_int_op(smartlist_contains_string(set->country_names, "a1"), OP_EQ, 1);
  tt_int_op(smartlist_contains_string(set->list, "{a1}"), OP_EQ, 1);

  done:
    if (set != NULL)
      routerset_free(set);
}

country_t
NS(geoip_get_country)(const char *country)
{
  int arg_is_qq, arg_is_a1;

  CALLED(geoip_get_country)++;

  arg_is_qq = !strcmp(country, "??");
  arg_is_a1 = !strcmp(country, "A1");

  tt_int_op(arg_is_qq || arg_is_a1, OP_EQ, 1);

  if (arg_is_a1)
    return 1;

  done:
    return -1;
}

int
NS(geoip_is_loaded)(sa_family_t family)
{
  CALLED(geoip_is_loaded)++;

  tt_int_op(family, OP_EQ, AF_INET);

  done:
    return 0;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_contains_extendinfo

/*
 * Functional test for routerset_contains_extendinfo.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  extend_info_t ei;
  int r;
  const char *nickname = "foo";
  (void)arg;

  memset(&ei, 0, sizeof(ei));
  strmap_set_lc(set->names, nickname, (void *)1);
  strncpy(ei.nickname, nickname, sizeof(ei.nickname) - 1);
  ei.nickname[sizeof(ei.nickname) - 1] = '\0';

  r = routerset_contains_extendinfo(set, &ei);

  tt_int_op(r, OP_EQ, 4);
  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_contains_router

/*
 * Functional test for routerset_contains_router.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  routerinfo_t ri;
  country_t country = 1;
  int r;
  const char *nickname = "foo";
  (void)arg;

  memset(&ri, 0, sizeof(ri));
  strmap_set_lc(set->names, nickname, (void *)1);
  ri.nickname = (char *)nickname;

  r = routerset_contains_router(set, &ri, country);

  tt_int_op(r, OP_EQ, 4);
  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_contains_routerstatus

/*
 * Functional test for routerset_contains_routerstatus.
 */

// XXX: This is a bit brief. It only populates and tests the nickname fields
// ie., enough to make the containment check succeed. Perhaps it should do
// a bit more or test a bit more.

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  routerstatus_t rs;
  country_t country = 1;
  int r;
  const char *nickname = "foo";
  (void)arg;

  memset(&rs, 0, sizeof(rs));
  strmap_set_lc(set->names, nickname, (void *)1);
  strncpy(rs.nickname, nickname, sizeof(rs.nickname) - 1);
  rs.nickname[sizeof(rs.nickname) - 1] = '\0';

  r = routerset_contains_routerstatus(set, &rs, country);

  tt_int_op(r, OP_EQ, 4);
  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains_node, none)

/*
 * Functional test for routerset_contains_node, when the node has no
 * routerset or routerinfo.
 */

static node_t NS(mock_node);

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int r;
  (void)arg;

  NS(mock_node).ri = NULL;
  NS(mock_node).rs = NULL;

  r = routerset_contains_node(set, &NS(mock_node));
  tt_int_op(r, OP_EQ, 0);

  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains_node, routerstatus)

/*
 * Functional test for routerset_contains_node, when the node has a
 * routerset and no routerinfo.
 */

static node_t NS(mock_node);

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int r;
  const char *nickname = "foo";
  routerstatus_t rs;
  (void)arg;

  strmap_set_lc(set->names, nickname, (void *)1);

  strncpy(rs.nickname, nickname, sizeof(rs.nickname) - 1);
  rs.nickname[sizeof(rs.nickname) - 1] = '\0';
  NS(mock_node).ri = NULL;
  NS(mock_node).rs = &rs;

  r = routerset_contains_node(set, &NS(mock_node));

  tt_int_op(r, OP_EQ, 4);
  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_contains_node, routerinfo)

/*
 * Functional test for routerset_contains_node, when the node has no
 * routerset and a routerinfo.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  int r;
  const char *nickname = "foo";
  routerinfo_t ri;
  node_t mock_node;
  (void)arg;

  strmap_set_lc(set->names, nickname, (void *)1);

  ri.nickname = (char *)nickname;
  mock_node.ri = &ri;
  mock_node.rs = NULL;

  r = routerset_contains_node(set, &mock_node);

  tt_int_op(r, OP_EQ, 4);
  done:
    routerset_free(set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_get_all_nodes, no_routerset)

/*
 * Functional test for routerset_get_all_nodes, when routerset is NULL or
 * the routerset list is NULL.
 */

static void
NS(test_main)(void *arg)
{
  smartlist_t *out = smartlist_new();
  routerset_t *set = NULL;
  (void)arg;

  tt_int_op(smartlist_len(out), OP_EQ, 0);
  routerset_get_all_nodes(out, NULL, NULL, 0);

  tt_int_op(smartlist_len(out), OP_EQ, 0);

  set = routerset_new();
  smartlist_free(set->list);
  routerset_get_all_nodes(out, NULL, NULL, 0);
  tt_int_op(smartlist_len(out), OP_EQ, 0);

  /* Just recreate list, so we can simply use routerset_free. */
  set->list = smartlist_new();

 done:
  routerset_free(set);
  smartlist_free(out);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_get_all_nodes, list_with_no_nodes)

/*
 * Structural test for routerset_get_all_nodes, when the routerset list
 * is empty.
 */

NS_DECL(const node_t *, node_get_by_nickname,
    (const char *nickname, int warn_if_unused));
static const char *NS(mock_nickname);

static void
NS(test_main)(void *arg)
{
  smartlist_t *out = smartlist_new();
  routerset_t *set = routerset_new();
  int out_len;
  (void)arg;

  NS_MOCK(node_get_by_nickname);

  NS(mock_nickname) = "foo";
  smartlist_add(set->list, tor_strdup(NS(mock_nickname)));

  routerset_get_all_nodes(out, set, NULL, 0);
  out_len = smartlist_len(out);

  smartlist_free(out);
  routerset_free(set);

  tt_int_op(out_len, OP_EQ, 0);
  tt_int_op(CALLED(node_get_by_nickname), OP_EQ, 1);

  done:
    ;
}

const node_t *
NS(node_get_by_nickname)(const char *nickname, int warn_if_unused)
{
  CALLED(node_get_by_nickname)++;
  tt_str_op(nickname, OP_EQ, NS(mock_nickname));
  tt_int_op(warn_if_unused, OP_EQ, 1);

  done:
    return NULL;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_get_all_nodes, list_flag_not_running)

/*
 * Structural test for routerset_get_all_nodes, with the running_only flag
 * is set but the nodes are not running.
 */

NS_DECL(const node_t *, node_get_by_nickname,
    (const char *nickname, int warn_if_unused));
static const char *NS(mock_nickname);
static node_t NS(mock_node);

static void
NS(test_main)(void *arg)
{
  smartlist_t *out = smartlist_new();
  routerset_t *set = routerset_new();
  int out_len;
  (void)arg;

  NS_MOCK(node_get_by_nickname);

  NS(mock_node).is_running = 0;
  NS(mock_nickname) = "foo";
  smartlist_add(set->list, tor_strdup(NS(mock_nickname)));

  routerset_get_all_nodes(out, set, NULL, 1);
  out_len = smartlist_len(out);

  smartlist_free(out);
  routerset_free(set);

  tt_int_op(out_len, OP_EQ, 0);
  tt_int_op(CALLED(node_get_by_nickname), OP_EQ, 1);

  done:
    ;
}

const node_t *
NS(node_get_by_nickname)(const char *nickname, int warn_if_unused)
{
  CALLED(node_get_by_nickname)++;
  tt_str_op(nickname, OP_EQ, NS(mock_nickname));
  tt_int_op(warn_if_unused, OP_EQ, 1);

  done:
    return &NS(mock_node);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_get_all_nodes, list)

/*
 * Structural test for routerset_get_all_nodes.
 */

NS_DECL(const node_t *, node_get_by_nickname,
    (const char *nickname, int warn_if_unused));
static char *NS(mock_nickname);
static node_t NS(mock_node);

static void
NS(test_main)(void *arg)
{
  smartlist_t *out = smartlist_new();
  routerset_t *set = routerset_new();
  int out_len;
  node_t *ent;
  (void)arg;

  NS_MOCK(node_get_by_nickname);

  NS(mock_nickname) = tor_strdup("foo");
  smartlist_add(set->list, NS(mock_nickname));

  routerset_get_all_nodes(out, set, NULL, 0);
  out_len = smartlist_len(out);
  ent = (node_t *)smartlist_get(out, 0);

  smartlist_free(out);
  routerset_free(set);

  tt_int_op(out_len, OP_EQ, 1);
  tt_ptr_op(ent, OP_EQ, &NS(mock_node));
  tt_int_op(CALLED(node_get_by_nickname), OP_EQ, 1);

  done:
    ;
}

const node_t *
NS(node_get_by_nickname)(const char *nickname, int warn_if_unused)
{
  CALLED(node_get_by_nickname)++;
  tt_str_op(nickname, OP_EQ, NS(mock_nickname));
  tt_int_op(warn_if_unused, OP_EQ, 1);

  done:
    return &NS(mock_node);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_get_all_nodes, nodelist_with_no_nodes)

/*
 * Structural test for routerset_get_all_nodes, when the nodelist has no nodes.
 */

NS_DECL(smartlist_t *, nodelist_get_list, (void));

static smartlist_t *NS(mock_smartlist);

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  smartlist_t *out = smartlist_new();
  int r;
  (void)arg;

  NS_MOCK(nodelist_get_list);

  smartlist_add(set->country_names, tor_strdup("{xx}"));
  NS(mock_smartlist) = smartlist_new();

  routerset_get_all_nodes(out, set, NULL, 1);
  r = smartlist_len(out);
  routerset_free(set);
  smartlist_free(out);
  smartlist_free(NS(mock_smartlist));

  tt_int_op(r, OP_EQ, 0);
  tt_int_op(CALLED(nodelist_get_list), OP_EQ, 1);

  done:
    ;
}

smartlist_t *
NS(nodelist_get_list)(void)
{
  CALLED(nodelist_get_list)++;

  return NS(mock_smartlist);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_get_all_nodes, nodelist_flag_not_running)

/*
 * Structural test for routerset_get_all_nodes, with a non-list routerset
 * the running_only flag is set, but the nodes are not running.
 */

NS_DECL(smartlist_t *, nodelist_get_list, (void));

static smartlist_t *NS(mock_smartlist);
static node_t NS(mock_node);

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  smartlist_t *out = smartlist_new();
  int r;
  (void)arg;

  NS_MOCK(nodelist_get_list);

  smartlist_add(set->country_names, tor_strdup("{xx}"));
  NS(mock_smartlist) = smartlist_new();
  NS(mock_node).is_running = 0;
  smartlist_add(NS(mock_smartlist), (void *)&NS(mock_node));

  routerset_get_all_nodes(out, set, NULL, 1);
  r = smartlist_len(out);
  routerset_free(set);
  smartlist_free(out);
  smartlist_free(NS(mock_smartlist));

  tt_int_op(r, OP_EQ, 0);
  tt_int_op(CALLED(nodelist_get_list), OP_EQ, 1);

  done:
    ;
}

smartlist_t *
NS(nodelist_get_list)(void)
{
  CALLED(nodelist_get_list)++;

  return NS(mock_smartlist);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_subtract_nodes

/*
 * Functional test for routerset_subtract_nodes.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = routerset_new();
  smartlist_t *list = smartlist_new();
  const char *nickname = "foo";
  routerinfo_t ri;
  node_t mock_node;
  (void)arg;

  strmap_set_lc(set->names, nickname, (void *)1);

  ri.nickname = (char *)nickname;
  mock_node.rs = NULL;
  mock_node.ri = &ri;
  smartlist_add(list, (void *)&mock_node);

  tt_int_op(smartlist_len(list), OP_NE, 0);
  routerset_subtract_nodes(list, set);

  tt_int_op(smartlist_len(list), OP_EQ, 0);
  done:
    routerset_free(set);
    smartlist_free(list);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_subtract_nodes, null_routerset)

/*
 * Functional test for routerset_subtract_nodes, with a NULL routerset.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = NULL;
  smartlist_t *list = smartlist_new();
  const char *nickname = "foo";
  routerinfo_t ri;
  node_t mock_node;
  (void)arg;

  ri.nickname = (char *)nickname;
  mock_node.ri = &ri;
  smartlist_add(list, (void *)&mock_node);

  tt_int_op(smartlist_len(list), OP_NE, 0);
  routerset_subtract_nodes(list, set);

  tt_int_op(smartlist_len(list), OP_NE, 0);
  done:
    routerset_free(set);
    smartlist_free(list);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_to_string

/*
 * Functional test for routerset_to_string.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *set = NULL;
  char *s = NULL;
  (void)arg;

  set = NULL;
  s = routerset_to_string(set);
  tt_str_op(s, OP_EQ, "");
  tor_free(s);

  set = routerset_new();
  s = routerset_to_string(set);
  tt_str_op(s, OP_EQ, "");
  tor_free(s);
  routerset_free(set); set = NULL;

  set = routerset_new();
  smartlist_add(set->list, tor_strndup("a", 1));
  s = routerset_to_string(set);
  tt_str_op(s, OP_EQ, "a");
  tor_free(s);
  routerset_free(set); set = NULL;

  set = routerset_new();
  smartlist_add(set->list, tor_strndup("a", 1));
  smartlist_add(set->list, tor_strndup("b", 1));
  s = routerset_to_string(set);
  tt_str_op(s, OP_EQ, "a,b");
  tor_free(s);
  routerset_free(set); set = NULL;

 done:
  tor_free(s);
  routerset_free((routerset_t *)set);
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_equal, empty_empty)

/*
 * Functional test for routerset_equal, with both routersets empty.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *a = routerset_new(), *b = routerset_new();
  int r;
  (void)arg;

  r = routerset_equal(a, b);
  routerset_free(a);
  routerset_free(b);

  tt_int_op(r, OP_EQ, 1);

  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_equal, empty_not_empty)

/*
 * Functional test for routerset_equal, with one routersets empty.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *a = routerset_new(), *b = routerset_new();
  int r;
  (void)arg;

  smartlist_add(b->list, tor_strdup("{xx}"));
  r = routerset_equal(a, b);
  routerset_free(a);
  routerset_free(b);

  tt_int_op(r, OP_EQ, 0);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_equal, differing_lengths)

/*
 * Functional test for routerset_equal, with the routersets having
 * differing lengths.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *a = routerset_new(), *b = routerset_new();
  int r;
  (void)arg;

  smartlist_add(a->list, tor_strdup("{aa}"));
  smartlist_add(b->list, tor_strdup("{b1}"));
  smartlist_add(b->list, tor_strdup("{b2}"));
  r = routerset_equal(a, b);
  routerset_free(a);
  routerset_free(b);

  tt_int_op(r, OP_EQ, 0);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_equal, unequal)

/*
 * Functional test for routerset_equal, with the routersets being
 * different.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *a = routerset_new(), *b = routerset_new();
  int r;
  (void)arg;

  smartlist_add(a->list, tor_strdup("foo"));
  smartlist_add(b->list, tor_strdup("bar"));
  r = routerset_equal(a, b);
  routerset_free(a);
  routerset_free(b);

  tt_int_op(r, OP_EQ, 0);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_equal, equal)

/*
 * Functional test for routerset_equal, with the routersets being
 * equal.
 */

static void
NS(test_main)(void *arg)
{
  routerset_t *a = routerset_new(), *b = routerset_new();
  int r;
  (void)arg;

  smartlist_add(a->list, tor_strdup("foo"));
  smartlist_add(b->list, tor_strdup("foo"));
  r = routerset_equal(a, b);
  routerset_free(a);
  routerset_free(b);

  tt_int_op(r, OP_EQ, 1);
  done:
    ;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE ASPECT(routerset_free, null_routerset)

/*
 * Structural test for routerset_free, where the routerset is NULL.
 */

NS_DECL(void, smartlist_free, (smartlist_t *sl));

static void
NS(test_main)(void *arg)
{
  (void)arg;

  NS_MOCK(smartlist_free);

  routerset_free(NULL);

  tt_int_op(CALLED(smartlist_free), OP_EQ, 0);

  done:
    ;
}

void
NS(smartlist_free)(smartlist_t *s)
{
  (void)s;
  CALLED(smartlist_free)++;
}

#undef NS_SUBMODULE
#define NS_SUBMODULE routerset_free

/*
 * Structural test for routerset_free.
 */

NS_DECL(void, smartlist_free, (smartlist_t *sl));
NS_DECL(void, strmap_free,(strmap_t *map, void (*free_val)(void*)));
NS_DECL(void, digestmap_free, (digestmap_t *map, void (*free_val)(void*)));

static void
NS(test_main)(void *arg)
{
  routerset_t *routerset = routerset_new();
  (void)arg;

  NS_MOCK(smartlist_free);
  NS_MOCK(strmap_free);
  NS_MOCK(digestmap_free);

  routerset_free(routerset);

  tt_int_op(CALLED(smartlist_free), OP_NE, 0);
  tt_int_op(CALLED(strmap_free), OP_NE, 0);
  tt_int_op(CALLED(digestmap_free), OP_NE, 0);

  done:
    ;
}

void
NS(smartlist_free)(smartlist_t *s)
{
  CALLED(smartlist_free)++;
  smartlist_free__real(s);
}

void
NS(strmap_free)(strmap_t *map, void (*free_val)(void*))
{
  CALLED(strmap_free)++;
  strmap_free__real(map, free_val);
}

void
NS(digestmap_free)(digestmap_t *map, void (*free_val)(void*))
{
  CALLED(digestmap_free)++;
  digestmap_free__real(map, free_val);
}

#undef NS_SUBMODULE

struct testcase_t routerset_tests[] = {
  TEST_CASE(routerset_new),
  TEST_CASE(routerset_get_countryname),
  TEST_CASE(routerset_is_list),
  TEST_CASE(routerset_needs_geoip),
  TEST_CASE(routerset_is_empty),
  TEST_CASE_ASPECT(routerset_contains, null_set_or_null_set_list),
  TEST_CASE_ASPECT(routerset_contains, set_and_nickname),
  TEST_CASE_ASPECT(routerset_contains, set_and_null_nickname),
  TEST_CASE_ASPECT(routerset_contains, set_and_no_nickname),
  TEST_CASE_ASPECT(routerset_contains, set_and_digest),
  TEST_CASE_ASPECT(routerset_contains, set_and_no_digest),
  TEST_CASE_ASPECT(routerset_contains, set_and_null_digest),
  TEST_CASE_ASPECT(routerset_contains, set_and_addr),
  TEST_CASE_ASPECT(routerset_contains, set_and_no_addr),
  TEST_CASE_ASPECT(routerset_contains, set_and_null_addr),
  TEST_CASE_ASPECT(routerset_contains, countries_no_geoip),
  TEST_CASE_ASPECT(routerset_contains, countries_geoip),
  TEST_CASE_ASPECT(routerset_add_unknown_ccs, only_flag_and_no_ccs),
  TEST_CASE_ASPECT(routerset_add_unknown_ccs, creates_set),
  TEST_CASE_ASPECT(routerset_add_unknown_ccs, add_unknown),
  TEST_CASE_ASPECT(routerset_add_unknown_ccs, add_a1),
  TEST_CASE(routerset_contains_extendinfo),
  TEST_CASE(routerset_contains_router),
  TEST_CASE(routerset_contains_routerstatus),
  TEST_CASE_ASPECT(routerset_contains_node, none),
  TEST_CASE_ASPECT(routerset_contains_node, routerinfo),
  TEST_CASE_ASPECT(routerset_contains_node, routerstatus),
  TEST_CASE_ASPECT(routerset_get_all_nodes, no_routerset),
  TEST_CASE_ASPECT(routerset_get_all_nodes, list_with_no_nodes),
  TEST_CASE_ASPECT(routerset_get_all_nodes, list_flag_not_running),
  TEST_CASE_ASPECT(routerset_get_all_nodes, list),
  TEST_CASE_ASPECT(routerset_get_all_nodes, nodelist_with_no_nodes),
  TEST_CASE_ASPECT(routerset_get_all_nodes, nodelist_flag_not_running),
  TEST_CASE_ASPECT(routerset_refresh_counties, geoip_not_loaded),
  TEST_CASE_ASPECT(routerset_refresh_counties, no_countries),
  TEST_CASE_ASPECT(routerset_refresh_counties, one_valid_country),
  TEST_CASE_ASPECT(routerset_refresh_counties, one_invalid_country),
  TEST_CASE_ASPECT(routerset_union, source_bad),
  TEST_CASE_ASPECT(routerset_union, one),
  TEST_CASE_ASPECT(routerset_parse, malformed),
  TEST_CASE_ASPECT(routerset_parse, valid_hexdigest),
  TEST_CASE_ASPECT(routerset_parse, valid_nickname),
  TEST_CASE_ASPECT(routerset_parse, get_countryname),
  TEST_CASE_ASPECT(routerset_parse, policy_wildcard),
  TEST_CASE_ASPECT(routerset_parse, policy_ipv4),
  TEST_CASE_ASPECT(routerset_parse, policy_ipv6),
  TEST_CASE(routerset_subtract_nodes),
  TEST_CASE_ASPECT(routerset_subtract_nodes, null_routerset),
  TEST_CASE(routerset_to_string),
  TEST_CASE_ASPECT(routerset_equal, empty_empty),
  TEST_CASE_ASPECT(routerset_equal, empty_not_empty),
  TEST_CASE_ASPECT(routerset_equal, differing_lengths),
  TEST_CASE_ASPECT(routerset_equal, unequal),
  TEST_CASE_ASPECT(routerset_equal, equal),
  TEST_CASE_ASPECT(routerset_free, null_routerset),
  TEST_CASE(routerset_free),
  END_OF_TESTCASES
};

