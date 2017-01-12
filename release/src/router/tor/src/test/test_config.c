/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#define CONFIG_PRIVATE
#define PT_PRIVATE
#define ROUTERSET_PRIVATE
#include "or.h"
#include "address.h"
#include "addressmap.h"
#include "circuitmux_ewma.h"
#include "circuitbuild.h"
#include "config.h"
#include "confparse.h"
#include "connection.h"
#include "connection_edge.h"
#include "test.h"
#include "util.h"
#include "address.h"
#include "connection_or.h"
#include "control.h"
#include "cpuworker.h"
#include "dirserv.h"
#include "dirvote.h"
#include "dns.h"
#include "entrynodes.h"
#include "transports.h"
#include "ext_orport.h"
#include "geoip.h"
#include "hibernate.h"
#include "main.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "rendclient.h"
#include "rendservice.h"
#include "router.h"
#include "routerlist.h"
#include "routerset.h"
#include "statefile.h"
#include "test.h"
#include "transports.h"
#include "util.h"

static void
test_config_addressmap(void *arg)
{
  char buf[1024];
  char address[256];
  time_t expires = TIME_MAX;
  (void)arg;

  strlcpy(buf, "MapAddress .invalidwildcard.com *.torserver.exit\n" // invalid
          "MapAddress *invalidasterisk.com *.torserver.exit\n" // invalid
          "MapAddress *.google.com *.torserver.exit\n"
          "MapAddress *.yahoo.com *.google.com.torserver.exit\n"
          "MapAddress *.cn.com www.cnn.com\n"
          "MapAddress *.cnn.com www.cnn.com\n"
          "MapAddress ex.com www.cnn.com\n"
          "MapAddress ey.com *.cnn.com\n"
          "MapAddress www.torproject.org 1.1.1.1\n"
          "MapAddress other.torproject.org "
            "this.torproject.org.otherserver.exit\n"
          "MapAddress test.torproject.org 2.2.2.2\n"
          "MapAddress www.google.com 3.3.3.3\n"
          "MapAddress www.example.org 4.4.4.4\n"
          "MapAddress 4.4.4.4 7.7.7.7\n"
          "MapAddress 4.4.4.4 5.5.5.5\n"
          "MapAddress www.infiniteloop.org 6.6.6.6\n"
          "MapAddress 6.6.6.6 www.infiniteloop.org\n"
          , sizeof(buf));

  config_get_lines(buf, &(get_options_mutable()->AddressMap), 0);
  config_register_addressmaps(get_options());

/* Use old interface for now, so we don't need to rewrite the unit tests */
#define addressmap_rewrite(a,s,eo,ao)                                   \
  addressmap_rewrite((a),(s), ~0, (eo),(ao))

  /* MapAddress .invalidwildcard.com .torserver.exit  - no match */
  strlcpy(address, "www.invalidwildcard.com", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  /* MapAddress *invalidasterisk.com .torserver.exit  - no match */
  strlcpy(address, "www.invalidasterisk.com", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  /* Where no mapping for FQDN match on top-level domain */
  /* MapAddress .google.com .torserver.exit */
  strlcpy(address, "reader.google.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "reader.torserver.exit");

  /* MapAddress *.yahoo.com *.google.com.torserver.exit */
  strlcpy(address, "reader.yahoo.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "reader.google.com.torserver.exit");

  /*MapAddress *.cnn.com www.cnn.com */
  strlcpy(address, "cnn.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "www.cnn.com");

  /* MapAddress .cn.com www.cnn.com */
  strlcpy(address, "www.cn.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "www.cnn.com");

  /* MapAddress ex.com www.cnn.com  - no match */
  strlcpy(address, "www.ex.com", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  /* MapAddress ey.com *.cnn.com - invalid expression */
  strlcpy(address, "ey.com", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  /* Where mapping for FQDN match on FQDN */
  strlcpy(address, "www.google.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "3.3.3.3");

  strlcpy(address, "www.torproject.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "1.1.1.1");

  strlcpy(address, "other.torproject.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "this.torproject.org.otherserver.exit");

  strlcpy(address, "test.torproject.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "2.2.2.2");

  /* Test a chain of address mappings and the order in which they were added:
          "MapAddress www.example.org 4.4.4.4"
          "MapAddress 4.4.4.4 7.7.7.7"
          "MapAddress 4.4.4.4 5.5.5.5"
  */
  strlcpy(address, "www.example.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "5.5.5.5");

  /* Test infinite address mapping results in no change */
  strlcpy(address, "www.infiniteloop.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "www.infiniteloop.org");

  /* Test we don't find false positives */
  strlcpy(address, "www.example.com", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  /* Test top-level-domain matching a bit harder */
  config_free_lines(get_options_mutable()->AddressMap);
  addressmap_clear_configured();
  strlcpy(buf, "MapAddress *.com *.torserver.exit\n"
          "MapAddress *.torproject.org 1.1.1.1\n"
          "MapAddress *.net 2.2.2.2\n"
          , sizeof(buf));
  config_get_lines(buf, &(get_options_mutable()->AddressMap), 0);
  config_register_addressmaps(get_options());

  strlcpy(address, "www.abc.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "www.abc.torserver.exit");

  strlcpy(address, "www.def.com", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "www.def.torserver.exit");

  strlcpy(address, "www.torproject.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "1.1.1.1");

  strlcpy(address, "test.torproject.org", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "1.1.1.1");

  strlcpy(address, "torproject.net", sizeof(address));
  tt_assert(addressmap_rewrite(address, sizeof(address), &expires, NULL));
  tt_str_op(address,OP_EQ, "2.2.2.2");

  /* We don't support '*' as a mapping directive */
  config_free_lines(get_options_mutable()->AddressMap);
  addressmap_clear_configured();
  strlcpy(buf, "MapAddress * *.torserver.exit\n", sizeof(buf));
  config_get_lines(buf, &(get_options_mutable()->AddressMap), 0);
  config_register_addressmaps(get_options());

  strlcpy(address, "www.abc.com", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  strlcpy(address, "www.def.net", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

  strlcpy(address, "www.torproject.org", sizeof(address));
  tt_assert(!addressmap_rewrite(address, sizeof(address), &expires, NULL));

#undef addressmap_rewrite

 done:
  config_free_lines(get_options_mutable()->AddressMap);
  get_options_mutable()->AddressMap = NULL;
  addressmap_free_all();
}

static int
is_private_dir(const char* path)
{
  struct stat st;
  int r = stat(path, &st);
  if (r) {
    return 0;
  }
#if !defined (_WIN32)
  if ((st.st_mode & (S_IFDIR | 0777)) != (S_IFDIR | 0700)) {
    return 0;
  }
#endif
  return 1;
}

static void
test_config_check_or_create_data_subdir(void *arg)
{
  or_options_t *options = get_options_mutable();
  char *datadir;
  const char *subdir = "test_stats";
  char *subpath;
  struct stat st;
  int r;
#if !defined (_WIN32)
  unsigned group_permission;
#endif
  (void)arg;

  tor_free(options->DataDirectory);
  datadir = options->DataDirectory = tor_strdup(get_fname("datadir-0"));
  subpath = get_datadir_fname(subdir);

#if defined (_WIN32)
  tt_int_op(mkdir(options->DataDirectory), OP_EQ, 0);
#else
  tt_int_op(mkdir(options->DataDirectory, 0700), OP_EQ, 0);
#endif

  r = stat(subpath, &st);

  // The subdirectory shouldn't exist yet,
  // but should be created by the call to check_or_create_data_subdir.
  tt_assert(r && (errno == ENOENT));
  tt_assert(!check_or_create_data_subdir(subdir));
  tt_assert(is_private_dir(subpath));

  // The check should return 0, if the directory already exists
  // and is private to the user.
  tt_assert(!check_or_create_data_subdir(subdir));

  r = stat(subpath, &st);
  if (r) {
    tt_abort_perror("stat");
  }

#if !defined (_WIN32)
  group_permission = st.st_mode | 0070;
  r = chmod(subpath, group_permission);

  if (r) {
    tt_abort_perror("chmod");
  }

  // If the directory exists, but its mode is too permissive
  // a call to check_or_create_data_subdir should reset the mode.
  tt_assert(!is_private_dir(subpath));
  tt_assert(!check_or_create_data_subdir(subdir));
  tt_assert(is_private_dir(subpath));
#endif

 done:
  rmdir(subpath);
  tor_free(datadir);
  tor_free(subpath);
}

static void
test_config_write_to_data_subdir(void *arg)
{
  or_options_t* options = get_options_mutable();
  char *datadir;
  char *cp = NULL;
  const char* subdir = "test_stats";
  const char* fname = "test_file";
  const char* str =
      "Lorem ipsum dolor sit amet, consetetur sadipscing\n"
      "elitr, sed diam nonumy eirmod\n"
      "tempor invidunt ut labore et dolore magna aliquyam\n"
      "erat, sed diam voluptua.\n"
      "At vero eos et accusam et justo duo dolores et ea\n"
      "rebum. Stet clita kasd gubergren,\n"
      "no sea takimata sanctus est Lorem ipsum dolor sit amet.\n"
      "Lorem ipsum dolor sit amet,\n"
      "consetetur sadipscing elitr, sed diam nonumy eirmod\n"
      "tempor invidunt ut labore et dolore\n"
      "magna aliquyam erat, sed diam voluptua. At vero eos et\n"
      "accusam et justo duo dolores et\n"
      "ea rebum. Stet clita kasd gubergren, no sea takimata\n"
      "sanctus est Lorem ipsum dolor sit amet.";
  char* filepath = NULL;
  (void)arg;

  tor_free(options->DataDirectory);
  datadir = options->DataDirectory = tor_strdup(get_fname("datadir-1"));
  filepath = get_datadir_fname2(subdir, fname);

#if defined (_WIN32)
  tt_int_op(mkdir(options->DataDirectory), OP_EQ, 0);
#else
  tt_int_op(mkdir(options->DataDirectory, 0700), OP_EQ, 0);
#endif

  // Write attempt shoudl fail, if subdirectory doesn't exist.
  tt_assert(write_to_data_subdir(subdir, fname, str, NULL));
  tt_assert(! check_or_create_data_subdir(subdir));

  // Content of file after write attempt should be
  // equal to the original string.
  tt_assert(!write_to_data_subdir(subdir, fname, str, NULL));
  cp = read_file_to_str(filepath, 0, NULL);
  tt_str_op(cp,OP_EQ, str);
  tor_free(cp);

  // A second write operation should overwrite the old content.
  tt_assert(!write_to_data_subdir(subdir, fname, str, NULL));
  cp = read_file_to_str(filepath, 0, NULL);
  tt_str_op(cp,OP_EQ, str);
  tor_free(cp);

 done:
  (void) unlink(filepath);
  rmdir(options->DataDirectory);
  tor_free(datadir);
  tor_free(filepath);
  tor_free(cp);
}

/* Test helper function: Make sure that a bridge line gets parsed
 * properly. Also make sure that the resulting bridge_line_t structure
 * has its fields set correctly. */
static void
good_bridge_line_test(const char *string, const char *test_addrport,
                      const char *test_digest, const char *test_transport,
                      const smartlist_t *test_socks_args)
{
  char *tmp = NULL;
  bridge_line_t *bridge_line = parse_bridge_line(string);
  tt_assert(bridge_line);

  /* test addrport */
  tmp = tor_strdup(fmt_addrport(&bridge_line->addr, bridge_line->port));
  tt_str_op(test_addrport,OP_EQ, tmp);
  tor_free(tmp);

  /* If we were asked to validate a digest, but we did not get a
     digest after parsing, we failed. */
  if (test_digest && tor_digest_is_zero(bridge_line->digest))
    tt_assert(0);

  /* If we were not asked to validate a digest, and we got a digest
     after parsing, we failed again. */
  if (!test_digest && !tor_digest_is_zero(bridge_line->digest))
    tt_assert(0);

  /* If we were asked to validate a digest, and we got a digest after
     parsing, make sure it's correct. */
  if (test_digest) {
    tmp = tor_strdup(hex_str(bridge_line->digest, DIGEST_LEN));
    tor_strlower(tmp);
    tt_str_op(test_digest,OP_EQ, tmp);
    tor_free(tmp);
  }

  /* If we were asked to validate a transport name, make sure tha it
     matches with the transport name that was parsed. */
  if (test_transport && !bridge_line->transport_name)
    tt_assert(0);
  if (!test_transport && bridge_line->transport_name)
    tt_assert(0);
  if (test_transport)
    tt_str_op(test_transport,OP_EQ, bridge_line->transport_name);

  /* Validate the SOCKS argument smartlist. */
  if (test_socks_args && !bridge_line->socks_args)
    tt_assert(0);
  if (!test_socks_args && bridge_line->socks_args)
    tt_assert(0);
  if (test_socks_args)
    tt_assert(smartlist_strings_eq(test_socks_args,
                                     bridge_line->socks_args));

 done:
  tor_free(tmp);
  bridge_line_free(bridge_line);
}

/* Test helper function: Make sure that a bridge line is
 * unparseable. */
static void
bad_bridge_line_test(const char *string)
{
  bridge_line_t *bridge_line = parse_bridge_line(string);
  if (bridge_line)
    TT_FAIL(("%s was supposed to fail, but it didn't.", string));
  tt_assert(!bridge_line);

 done:
  bridge_line_free(bridge_line);
}

static void
test_config_parse_bridge_line(void *arg)
{
  (void) arg;
  good_bridge_line_test("192.0.2.1:4123",
                        "192.0.2.1:4123", NULL, NULL, NULL);

  good_bridge_line_test("192.0.2.1",
                        "192.0.2.1:443", NULL, NULL, NULL);

  good_bridge_line_test("transport [::1]",
                        "[::1]:443", NULL, "transport", NULL);

  good_bridge_line_test("transport 192.0.2.1:12 "
                        "4352e58420e68f5e40bf7c74faddccd9d1349413",
                        "192.0.2.1:12",
                        "4352e58420e68f5e40bf7c74faddccd9d1349413",
                        "transport", NULL);

  {
    smartlist_t *sl_tmp = smartlist_new();
    smartlist_add_asprintf(sl_tmp, "twoandtwo=five");

    good_bridge_line_test("transport 192.0.2.1:12 "
                    "4352e58420e68f5e40bf7c74faddccd9d1349413 twoandtwo=five",
                    "192.0.2.1:12", "4352e58420e68f5e40bf7c74faddccd9d1349413",
                    "transport", sl_tmp);

    SMARTLIST_FOREACH(sl_tmp, char *, s, tor_free(s));
    smartlist_free(sl_tmp);
  }

  {
    smartlist_t *sl_tmp = smartlist_new();
    smartlist_add_asprintf(sl_tmp, "twoandtwo=five");
    smartlist_add_asprintf(sl_tmp, "z=z");

    good_bridge_line_test("transport 192.0.2.1:12 twoandtwo=five z=z",
                          "192.0.2.1:12", NULL, "transport", sl_tmp);

    SMARTLIST_FOREACH(sl_tmp, char *, s, tor_free(s));
    smartlist_free(sl_tmp);
  }

  {
    smartlist_t *sl_tmp = smartlist_new();
    smartlist_add_asprintf(sl_tmp, "dub=come");
    smartlist_add_asprintf(sl_tmp, "save=me");

    good_bridge_line_test("transport 192.0.2.1:12 "
                          "4352e58420e68f5e40bf7c74faddccd9d1349666 "
                          "dub=come save=me",

                          "192.0.2.1:12",
                          "4352e58420e68f5e40bf7c74faddccd9d1349666",
                          "transport", sl_tmp);

    SMARTLIST_FOREACH(sl_tmp, char *, s, tor_free(s));
    smartlist_free(sl_tmp);
  }

  good_bridge_line_test("192.0.2.1:1231 "
                        "4352e58420e68f5e40bf7c74faddccd9d1349413",
                        "192.0.2.1:1231",
                        "4352e58420e68f5e40bf7c74faddccd9d1349413",
                        NULL, NULL);

  /* Empty line */
  bad_bridge_line_test("");
  /* bad transport name */
  bad_bridge_line_test("tr$n_sp0r7 190.20.2.2");
  /* weird ip address */
  bad_bridge_line_test("a.b.c.d");
  /* invalid fpr */
  bad_bridge_line_test("2.2.2.2:1231 4352e58420e68f5e40bf7c74faddccd9d1349");
  /* no k=v in the end */
  bad_bridge_line_test("obfs2 2.2.2.2:1231 "
                       "4352e58420e68f5e40bf7c74faddccd9d1349413 what");
  /* no addrport */
  bad_bridge_line_test("asdw");
  /* huge k=v value that can't fit in SOCKS fields */
  bad_bridge_line_test(
           "obfs2 2.2.2.2:1231 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
           "aa=b");
}

static void
test_config_parse_transport_options_line(void *arg)
{
  smartlist_t *options_sl = NULL, *sl_tmp = NULL;

  (void) arg;

  { /* too small line */
    options_sl = get_options_from_transport_options_line("valley", NULL);
    tt_assert(!options_sl);
  }

  { /* no k=v values */
    options_sl = get_options_from_transport_options_line("hit it!", NULL);
    tt_assert(!options_sl);
  }

  { /* correct line, but wrong transport specified */
    options_sl =
      get_options_from_transport_options_line("trebuchet k=v", "rook");
    tt_assert(!options_sl);
  }

  { /* correct -- no transport specified */
    sl_tmp = smartlist_new();
    smartlist_add_asprintf(sl_tmp, "ladi=dadi");
    smartlist_add_asprintf(sl_tmp, "weliketo=party");

    options_sl =
      get_options_from_transport_options_line("rook ladi=dadi weliketo=party",
                                              NULL);
    tt_assert(options_sl);
    tt_assert(smartlist_strings_eq(options_sl, sl_tmp));

    SMARTLIST_FOREACH(sl_tmp, char *, s, tor_free(s));
    smartlist_free(sl_tmp);
    sl_tmp = NULL;
    SMARTLIST_FOREACH(options_sl, char *, s, tor_free(s));
    smartlist_free(options_sl);
    options_sl = NULL;
  }

  { /* correct -- correct transport specified */
    sl_tmp = smartlist_new();
    smartlist_add_asprintf(sl_tmp, "ladi=dadi");
    smartlist_add_asprintf(sl_tmp, "weliketo=party");

    options_sl =
      get_options_from_transport_options_line("rook ladi=dadi weliketo=party",
                                              "rook");
    tt_assert(options_sl);
    tt_assert(smartlist_strings_eq(options_sl, sl_tmp));
    SMARTLIST_FOREACH(sl_tmp, char *, s, tor_free(s));
    smartlist_free(sl_tmp);
    sl_tmp = NULL;
    SMARTLIST_FOREACH(options_sl, char *, s, tor_free(s));
    smartlist_free(options_sl);
    options_sl = NULL;
  }

 done:
  if (options_sl) {
    SMARTLIST_FOREACH(options_sl, char *, s, tor_free(s));
    smartlist_free(options_sl);
  }
  if (sl_tmp) {
    SMARTLIST_FOREACH(sl_tmp, char *, s, tor_free(s));
    smartlist_free(sl_tmp);
  }
}

/* Mocks needed for the transport plugin line test */

static void pt_kickstart_proxy_mock(const smartlist_t *transport_list,
                                    char **proxy_argv, int is_server);
static int transport_add_from_config_mock(const tor_addr_t *addr,
                                          uint16_t port, const char *name,
                                          int socks_ver);
static int transport_is_needed_mock(const char *transport_name);

static int pt_kickstart_proxy_mock_call_count = 0;
static int transport_add_from_config_mock_call_count = 0;
static int transport_is_needed_mock_call_count = 0;
static int transport_is_needed_mock_return = 0;

static void
pt_kickstart_proxy_mock(const smartlist_t *transport_list,
                        char **proxy_argv, int is_server)
{
  (void) transport_list;
  (void) proxy_argv;
  (void) is_server;
  /* XXXX check that args are as expected. */

  ++pt_kickstart_proxy_mock_call_count;

  free_execve_args(proxy_argv);
}

static int
transport_add_from_config_mock(const tor_addr_t *addr,
                               uint16_t port, const char *name,
                               int socks_ver)
{
  (void) addr;
  (void) port;
  (void) name;
  (void) socks_ver;
  /* XXXX check that args are as expected. */

  ++transport_add_from_config_mock_call_count;

  return 0;
}

static int
transport_is_needed_mock(const char *transport_name)
{
  (void) transport_name;
  /* XXXX check that arg is as expected. */

  ++transport_is_needed_mock_call_count;

  return transport_is_needed_mock_return;
}

/**
 * Test parsing for the ClientTransportPlugin and ServerTransportPlugin config
 * options.
 */

static void
test_config_parse_transport_plugin_line(void *arg)
{
  (void)arg;

  or_options_t *options = get_options_mutable();
  int r, tmp;
  int old_pt_kickstart_proxy_mock_call_count;
  int old_transport_add_from_config_mock_call_count;
  int old_transport_is_needed_mock_call_count;

  /* Bad transport lines - too short */
  r = parse_transport_line(options, "bad", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options, "bad", 1, 1);
  tt_assert(r < 0);
  r = parse_transport_line(options, "bad bad", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options, "bad bad", 1, 1);
  tt_assert(r < 0);

  /* Test transport list parsing */
  r = parse_transport_line(options,
      "transport_1 exec /usr/bin/fake-transport", 1, 0);
  tt_assert(r == 0);
  r = parse_transport_line(options,
   "transport_1 exec /usr/bin/fake-transport", 1, 1);
  tt_assert(r == 0);
  r = parse_transport_line(options,
      "transport_1,transport_2 exec /usr/bin/fake-transport", 1, 0);
  tt_assert(r == 0);
  r = parse_transport_line(options,
      "transport_1,transport_2 exec /usr/bin/fake-transport", 1, 1);
  tt_assert(r == 0);
  /* Bad transport identifiers */
  r = parse_transport_line(options,
      "transport_* exec /usr/bin/fake-transport", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options,
      "transport_* exec /usr/bin/fake-transport", 1, 1);
  tt_assert(r < 0);

  /* Check SOCKS cases for client transport */
  r = parse_transport_line(options,
      "transport_1 socks4 1.2.3.4:567", 1, 0);
  tt_assert(r == 0);
  r = parse_transport_line(options,
      "transport_1 socks5 1.2.3.4:567", 1, 0);
  tt_assert(r == 0);
  /* Proxy case for server transport */
  r = parse_transport_line(options,
      "transport_1 proxy 1.2.3.4:567", 1, 1);
  tt_assert(r == 0);
  /* Multiple-transport error exit */
  r = parse_transport_line(options,
      "transport_1,transport_2 socks5 1.2.3.4:567", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options,
      "transport_1,transport_2 proxy 1.2.3.4:567", 1, 1);
  /* No port error exit */
  r = parse_transport_line(options,
      "transport_1 socks5 1.2.3.4", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options,
     "transport_1 proxy 1.2.3.4", 1, 1);
  tt_assert(r < 0);
  /* Unparsable address error exit */
  r = parse_transport_line(options,
      "transport_1 socks5 1.2.3:6x7", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options,
      "transport_1 proxy 1.2.3:6x7", 1, 1);
  tt_assert(r < 0);

  /* "Strange {Client|Server}TransportPlugin field" error exit */
  r = parse_transport_line(options,
      "transport_1 foo bar", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options,
      "transport_1 foo bar", 1, 1);
  tt_assert(r < 0);

  /* No sandbox mode error exit */
  tmp = options->Sandbox;
  options->Sandbox = 1;
  r = parse_transport_line(options,
      "transport_1 exec /usr/bin/fake-transport", 1, 0);
  tt_assert(r < 0);
  r = parse_transport_line(options,
      "transport_1 exec /usr/bin/fake-transport", 1, 1);
  tt_assert(r < 0);
  options->Sandbox = tmp;

  /*
   * These final test cases cover code paths that only activate without
   * validate_only, so they need mocks in place.
   */
  MOCK(pt_kickstart_proxy, pt_kickstart_proxy_mock);
  old_pt_kickstart_proxy_mock_call_count =
    pt_kickstart_proxy_mock_call_count;
  r = parse_transport_line(options,
      "transport_1 exec /usr/bin/fake-transport", 0, 1);
  tt_assert(r == 0);
  tt_assert(pt_kickstart_proxy_mock_call_count ==
      old_pt_kickstart_proxy_mock_call_count + 1);
  UNMOCK(pt_kickstart_proxy);

  /* This one hits a log line in the !validate_only case only */
  r = parse_transport_line(options,
      "transport_1 proxy 1.2.3.4:567", 0, 1);
  tt_assert(r == 0);

  /* Check mocked client transport cases */
  MOCK(pt_kickstart_proxy, pt_kickstart_proxy_mock);
  MOCK(transport_add_from_config, transport_add_from_config_mock);
  MOCK(transport_is_needed, transport_is_needed_mock);

  /* Unnecessary transport case */
  transport_is_needed_mock_return = 0;
  old_pt_kickstart_proxy_mock_call_count =
    pt_kickstart_proxy_mock_call_count;
  old_transport_add_from_config_mock_call_count =
    transport_add_from_config_mock_call_count;
  old_transport_is_needed_mock_call_count =
    transport_is_needed_mock_call_count;
  r = parse_transport_line(options,
      "transport_1 exec /usr/bin/fake-transport", 0, 0);
  /* Should have succeeded */
  tt_assert(r == 0);
  /* transport_is_needed() should have been called */
  tt_assert(transport_is_needed_mock_call_count ==
      old_transport_is_needed_mock_call_count + 1);
  /*
   * pt_kickstart_proxy() and transport_add_from_config() should
   * not have been called.
   */
  tt_assert(pt_kickstart_proxy_mock_call_count ==
      old_pt_kickstart_proxy_mock_call_count);
  tt_assert(transport_add_from_config_mock_call_count ==
      old_transport_add_from_config_mock_call_count);

  /* Necessary transport case */
  transport_is_needed_mock_return = 1;
  old_pt_kickstart_proxy_mock_call_count =
    pt_kickstart_proxy_mock_call_count;
  old_transport_add_from_config_mock_call_count =
    transport_add_from_config_mock_call_count;
  old_transport_is_needed_mock_call_count =
    transport_is_needed_mock_call_count;
  r = parse_transport_line(options,
      "transport_1 exec /usr/bin/fake-transport", 0, 0);
  /* Should have succeeded */
  tt_assert(r == 0);
  /*
   * transport_is_needed() and pt_kickstart_proxy() should have been
   * called.
   */
  tt_assert(pt_kickstart_proxy_mock_call_count ==
      old_pt_kickstart_proxy_mock_call_count + 1);
  tt_assert(transport_is_needed_mock_call_count ==
      old_transport_is_needed_mock_call_count + 1);
  /* transport_add_from_config() should not have been called. */
  tt_assert(transport_add_from_config_mock_call_count ==
      old_transport_add_from_config_mock_call_count);

  /* proxy case */
  transport_is_needed_mock_return = 1;
  old_pt_kickstart_proxy_mock_call_count =
    pt_kickstart_proxy_mock_call_count;
  old_transport_add_from_config_mock_call_count =
    transport_add_from_config_mock_call_count;
  old_transport_is_needed_mock_call_count =
    transport_is_needed_mock_call_count;
  r = parse_transport_line(options,
      "transport_1 socks5 1.2.3.4:567", 0, 0);
  /* Should have succeeded */
  tt_assert(r == 0);
  /*
   * transport_is_needed() and transport_add_from_config() should have
   * been called.
   */
  tt_assert(transport_add_from_config_mock_call_count ==
      old_transport_add_from_config_mock_call_count + 1);
  tt_assert(transport_is_needed_mock_call_count ==
      old_transport_is_needed_mock_call_count + 1);
  /* pt_kickstart_proxy() should not have been called. */
  tt_assert(pt_kickstart_proxy_mock_call_count ==
      old_pt_kickstart_proxy_mock_call_count);

  /* Done with mocked client transport cases */
  UNMOCK(transport_is_needed);
  UNMOCK(transport_add_from_config);
  UNMOCK(pt_kickstart_proxy);

 done:
  /* Make sure we undo all mocks */
  UNMOCK(pt_kickstart_proxy);
  UNMOCK(transport_add_from_config);
  UNMOCK(transport_is_needed);

  return;
}

// Tests if an options with MyFamily fingerprints missing '$' normalises
// them correctly and also ensure it also works with multiple fingerprints
static void
test_config_fix_my_family(void *arg)
{
  char *err = NULL;
  const char *family = "$1111111111111111111111111111111111111111, "
                       "1111111111111111111111111111111111111112, "
                       "$1111111111111111111111111111111111111113";

  or_options_t* options = options_new();
  or_options_t* defaults = options_new();
  (void) arg;

  options_init(options);
  options_init(defaults);
  options->MyFamily = tor_strdup(family);

  options_validate(NULL, options, defaults, 0, &err) ;

  if (err != NULL) {
    TT_FAIL(("options_validate failed: %s", err));
  }

  tt_str_op(options->MyFamily,OP_EQ,
                                "$1111111111111111111111111111111111111111, "
                                "$1111111111111111111111111111111111111112, "
                                "$1111111111111111111111111111111111111113");

  done:
    if (err != NULL) {
      tor_free(err);
    }

    or_options_free(options);
    or_options_free(defaults);
}

static int n_hostname_01010101 = 0;

/** This mock function is meant to replace tor_lookup_hostname().
 * It answers with 1.1.1.1 as IP adddress that resulted from lookup.
 * This function increments <b>n_hostname_01010101</b> counter by one
 * every time it is called.
 */
static int
tor_lookup_hostname_01010101(const char *name, uint32_t *addr)
{
  n_hostname_01010101++;

  if (name && addr) {
    *addr = ntohl(0x01010101);
  }

  return 0;
}

static int n_hostname_localhost = 0;

/** This mock function is meant to replace tor_lookup_hostname().
 * It answers with 127.0.0.1 as IP adddress that resulted from lookup.
 * This function increments <b>n_hostname_localhost</b> counter by one
 * every time it is called.
 */
static int
tor_lookup_hostname_localhost(const char *name, uint32_t *addr)
{
  n_hostname_localhost++;

  if (name && addr) {
    *addr = 0x7f000001;
  }

  return 0;
}

static int n_hostname_failure = 0;

/** This mock function is meant to replace tor_lookup_hostname().
 * It pretends to fail by returning -1 to caller. Also, this function
 * increments <b>n_hostname_failure</b> every time it is called.
 */
static int
tor_lookup_hostname_failure(const char *name, uint32_t *addr)
{
  (void)name;
  (void)addr;

  n_hostname_failure++;

  return -1;
}

static int n_gethostname_replacement = 0;

/** This mock function is meant to replace tor_gethostname(). It
 * responds with string "onionrouter!" as hostname. This function
 * increments <b>n_gethostname_replacement</b> by one every time
 * it is called.
 */
static int
tor_gethostname_replacement(char *name, size_t namelen)
{
  n_gethostname_replacement++;

  if (name && namelen) {
    strlcpy(name,"onionrouter!",namelen);
  }

  return 0;
}

static int n_gethostname_localhost = 0;

/** This mock function is meant to replace tor_gethostname(). It
 * responds with string "127.0.0.1" as hostname. This function
 * increments <b>n_gethostname_localhost</b> by one every time
 * it is called.
 */
static int
tor_gethostname_localhost(char *name, size_t namelen)
{
  n_gethostname_localhost++;

  if (name && namelen) {
    strlcpy(name,"127.0.0.1",namelen);
  }

  return 0;
}

static int n_gethostname_failure = 0;

/** This mock function is meant to replace tor_gethostname.
 * It pretends to fail by returning -1. This function increments
 * <b>n_gethostname_failure</b> by one every time it is called.
 */
static int
tor_gethostname_failure(char *name, size_t namelen)
{
  (void)name;
  (void)namelen;
  n_gethostname_failure++;

  return -1;
}

static int n_get_interface_address = 0;

/** This mock function is meant to replace get_interface_address().
 * It answers with address 8.8.8.8. This function increments
 * <b>n_get_interface_address</b> by one every time it is called.
 */
static int
get_interface_address_08080808(int severity, uint32_t *addr)
{
  (void)severity;

  n_get_interface_address++;

  if (addr) {
    *addr = ntohl(0x08080808);
  }

  return 0;
}

static int n_get_interface_address6 = 0;
static sa_family_t last_address6_family;

/** This mock function is meant to replace get_interface_address6().
 * It answers with IP address 9.9.9.9 iff both of the following are true:
 *  - <b>family</b> is AF_INET
 *  - <b>addr</b> pointer is not NULL.
 * This function increments <b>n_get_interface_address6</b> by one every
 * time it is called.
 */
static int
get_interface_address6_replacement(int severity, sa_family_t family,
                                   tor_addr_t *addr)
{
  (void)severity;

  last_address6_family = family;
  n_get_interface_address6++;

  if ((family != AF_INET) || !addr) {
    return -1;
  }

  tor_addr_from_ipv4h(addr,0x09090909);

  return 0;
}

static int n_get_interface_address_failure = 0;

/**
 * This mock function is meant to replace get_interface_address().
 * It pretends to fail getting interface address by returning -1.
 * <b>n_get_interface_address_failure</b> is incremented by one
 * every time this function is called.
 */
static int
get_interface_address_failure(int severity, uint32_t *addr)
{
  (void)severity;
  (void)addr;

  n_get_interface_address_failure++;

  return -1;
}

static int n_get_interface_address6_failure = 0;

/**
 * This mock function is meant to replace get_interface_addres6().
 * It will pretend to fail by return -1.
 * <b>n_get_interface_address6_failure</b> is incremented by one
 * every time this function is called and <b>last_address6_family</b>
 * is assigned the value of <b>family</b> argument.
 */
static int
get_interface_address6_failure(int severity, sa_family_t family,
                               tor_addr_t *addr)
{
  (void)severity;
  (void)addr;
   n_get_interface_address6_failure++;
   last_address6_family = family;

   return -1;
}

static void
test_config_resolve_my_address(void *arg)
{
  or_options_t *options;
  uint32_t resolved_addr;
  const char *method_used;
  char *hostname_out = NULL;
  int retval;
  int prev_n_hostname_01010101;
  int prev_n_hostname_localhost;
  int prev_n_hostname_failure;
  int prev_n_gethostname_replacement;
  int prev_n_gethostname_failure;
  int prev_n_gethostname_localhost;
  int prev_n_get_interface_address;
  int prev_n_get_interface_address_failure;
  int prev_n_get_interface_address6;
  int prev_n_get_interface_address6_failure;

  (void)arg;

  options = options_new();

  options_init(options);

 /*
  * CASE 1:
  * If options->Address is a valid IPv4 address string, we want
  * the corresponding address to be parsed and returned.
  */

  options->Address = tor_strdup("128.52.128.105");

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(retval == 0);
  tt_want_str_op(method_used,==,"CONFIGURED");
  tt_want(hostname_out == NULL);
  tt_assert(resolved_addr == 0x80348069);

  tor_free(options->Address);

/*
 * CASE 2:
 * If options->Address is a valid DNS address, we want resolve_my_address()
 * function to ask tor_lookup_hostname() for help with resolving it
 * and return the address that was resolved (in host order).
 */

  MOCK(tor_lookup_hostname,tor_lookup_hostname_01010101);

  tor_free(options->Address);
  options->Address = tor_strdup("www.torproject.org");

  prev_n_hostname_01010101 = n_hostname_01010101;

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(retval == 0);
  tt_want(n_hostname_01010101 == prev_n_hostname_01010101 + 1);
  tt_want_str_op(method_used,==,"RESOLVED");
  tt_want_str_op(hostname_out,==,"www.torproject.org");
  tt_assert(resolved_addr == 0x01010101);

  UNMOCK(tor_lookup_hostname);

  tor_free(options->Address);
  tor_free(hostname_out);

/*
 * CASE 3:
 * Given that options->Address is NULL, we want resolve_my_address()
 * to try and use tor_gethostname() to get hostname AND use
 * tor_lookup_hostname() to get IP address.
 */

  resolved_addr = 0;
  tor_free(options->Address);
  options->Address = NULL;

  MOCK(tor_gethostname,tor_gethostname_replacement);
  MOCK(tor_lookup_hostname,tor_lookup_hostname_01010101);

  prev_n_gethostname_replacement = n_gethostname_replacement;
  prev_n_hostname_01010101 = n_hostname_01010101;

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(retval == 0);
  tt_want(n_gethostname_replacement == prev_n_gethostname_replacement + 1);
  tt_want(n_hostname_01010101 == prev_n_hostname_01010101 + 1);
  tt_want_str_op(method_used,==,"GETHOSTNAME");
  tt_want_str_op(hostname_out,==,"onionrouter!");
  tt_assert(resolved_addr == 0x01010101);

  UNMOCK(tor_gethostname);
  UNMOCK(tor_lookup_hostname);

  tor_free(hostname_out);

/*
 * CASE 4:
 * Given that options->Address is a local host address, we want
 * resolve_my_address() function to fail.
 */

  resolved_addr = 0;
  tor_free(options->Address);
  options->Address = tor_strdup("127.0.0.1");

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(resolved_addr == 0);
  tt_assert(retval == -1);

  tor_free(options->Address);
  tor_free(hostname_out);

/*
 * CASE 5:
 * We want resolve_my_address() to fail if DNS address in options->Address
 * cannot be resolved.
 */

  MOCK(tor_lookup_hostname,tor_lookup_hostname_failure);

  prev_n_hostname_failure = n_hostname_failure;

  tor_free(options->Address);
  options->Address = tor_strdup("www.tor-project.org");

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_hostname_failure == prev_n_hostname_failure + 1);
  tt_assert(retval == -1);

  UNMOCK(tor_lookup_hostname);

  tor_free(options->Address);
  tor_free(hostname_out);

/*
 * CASE 6:
 * If options->Address is NULL AND gettting local hostname fails, we want
 * resolve_my_address() to fail as well.
 */

  MOCK(tor_gethostname,tor_gethostname_failure);

  prev_n_gethostname_failure = n_gethostname_failure;

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_gethostname_failure == prev_n_gethostname_failure + 1);
  tt_assert(retval == -1);

  UNMOCK(tor_gethostname);
  tor_free(hostname_out);

/*
 * CASE 7:
 * We want resolve_my_address() to try and get network interface address via
 * get_interface_address() if hostname returned by tor_gethostname() cannot be
 * resolved into IP address.
 */

  MOCK(tor_gethostname,tor_gethostname_replacement);
  MOCK(tor_lookup_hostname,tor_lookup_hostname_failure);
  MOCK(get_interface_address,get_interface_address_08080808);

  prev_n_gethostname_replacement = n_gethostname_replacement;
  prev_n_get_interface_address = n_get_interface_address;

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(retval == 0);
  tt_want_int_op(n_gethostname_replacement, ==,
                 prev_n_gethostname_replacement + 1);
  tt_want_int_op(n_get_interface_address, ==,
                 prev_n_get_interface_address + 1);
  tt_want_str_op(method_used,==,"INTERFACE");
  tt_want(hostname_out == NULL);
  tt_assert(resolved_addr == 0x08080808);

  UNMOCK(get_interface_address);
  tor_free(hostname_out);

/*
 * CASE 8:
 * Suppose options->Address is NULL AND hostname returned by tor_gethostname()
 * is unresolvable. We want resolve_my_address to fail if
 * get_interface_address() fails.
 */

  MOCK(get_interface_address,get_interface_address_failure);

  prev_n_get_interface_address_failure = n_get_interface_address_failure;
  prev_n_gethostname_replacement = n_gethostname_replacement;

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_get_interface_address_failure ==
          prev_n_get_interface_address_failure + 1);
  tt_want(n_gethostname_replacement ==
          prev_n_gethostname_replacement + 1);
  tt_assert(retval == -1);

  UNMOCK(get_interface_address);
  tor_free(hostname_out);

/*
 * CASE 9:
 * Given that options->Address is NULL AND tor_lookup_hostname()
 * fails AND hostname returned by gethostname() resolves
 * to local IP address, we want resolve_my_address() function to
 * call get_interface_address6(.,AF_INET,.) and return IP address
 * the latter function has found.
 */

  MOCK(tor_lookup_hostname,tor_lookup_hostname_failure);
  MOCK(tor_gethostname,tor_gethostname_replacement);
  MOCK(get_interface_address6,get_interface_address6_replacement);

  prev_n_gethostname_replacement = n_gethostname_replacement;
  prev_n_hostname_failure = n_hostname_failure;
  prev_n_get_interface_address6 = n_get_interface_address6;

  retval = resolve_my_address(LOG_NOTICE,options,&resolved_addr,
                              &method_used,&hostname_out);

  tt_want(last_address6_family == AF_INET);
  tt_want(n_get_interface_address6 == prev_n_get_interface_address6 + 1);
  tt_want(n_hostname_failure == prev_n_hostname_failure + 1);
  tt_want(n_gethostname_replacement == prev_n_gethostname_replacement + 1);
  tt_want(retval == 0);
  tt_want_str_op(method_used,==,"INTERFACE");
  tt_assert(resolved_addr == 0x09090909);

  UNMOCK(tor_lookup_hostname);
  UNMOCK(tor_gethostname);
  UNMOCK(get_interface_address6);

  tor_free(hostname_out);

  /*
   * CASE 10: We want resolve_my_address() to fail if all of the following
   * are true:
   *   1. options->Address is not NULL
   *   2. ... but it cannot be converted to struct in_addr by
   *      tor_inet_aton()
   *   3. ... and tor_lookup_hostname() fails to resolve the
   *      options->Address
   */

  MOCK(tor_lookup_hostname,tor_lookup_hostname_failure);

  prev_n_hostname_failure = n_hostname_failure;

  tor_free(options->Address);
  options->Address = tor_strdup("some_hostname");

  retval = resolve_my_address(LOG_NOTICE, options, &resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_hostname_failure == prev_n_hostname_failure + 1);
  tt_assert(retval == -1);

  UNMOCK(tor_gethostname);
  UNMOCK(tor_lookup_hostname);

  tor_free(hostname_out);

  /*
   * CASE 11:
   * Suppose the following sequence of events:
   *   1. options->Address is NULL
   *   2. tor_gethostname() succeeds to get hostname of machine Tor
   *      if running on.
   *   3. Hostname from previous step cannot be converted to
   *      address by using tor_inet_aton() function.
   *   4. However, tor_lookup_hostname() succeds in resolving the
   *      hostname from step 2.
   *   5. Unfortunately, tor_addr_is_internal() deems this address
   *      to be internal.
   *   6. get_interface_address6(.,AF_INET,.) returns non-internal
   *      IPv4
   *
   *   We want resolve_my_addr() to succeed with method "INTERFACE"
   *   and address from step 6.
   */

  tor_free(options->Address);
  options->Address = NULL;

  MOCK(tor_gethostname,tor_gethostname_replacement);
  MOCK(tor_lookup_hostname,tor_lookup_hostname_localhost);
  MOCK(get_interface_address6,get_interface_address6_replacement);

  prev_n_gethostname_replacement = n_gethostname_replacement;
  prev_n_hostname_localhost = n_hostname_localhost;
  prev_n_get_interface_address6 = n_get_interface_address6;

  retval = resolve_my_address(LOG_DEBUG, options, &resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_gethostname_replacement == prev_n_gethostname_replacement + 1);
  tt_want(n_hostname_localhost == prev_n_hostname_localhost + 1);
  tt_want(n_get_interface_address6 == prev_n_get_interface_address6 + 1);

  tt_str_op(method_used,==,"INTERFACE");
  tt_assert(!hostname_out);
  tt_assert(retval == 0);

  /*
   * CASE 11b:
   *   1-5 as above.
   *   6. get_interface_address6() fails.
   *
   *   In this subcase, we want resolve_my_address() to fail.
   */

  UNMOCK(get_interface_address6);
  MOCK(get_interface_address6,get_interface_address6_failure);

  prev_n_gethostname_replacement = n_gethostname_replacement;
  prev_n_hostname_localhost = n_hostname_localhost;
  prev_n_get_interface_address6_failure = n_get_interface_address6_failure;

  retval = resolve_my_address(LOG_DEBUG, options, &resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_gethostname_replacement == prev_n_gethostname_replacement + 1);
  tt_want(n_hostname_localhost == prev_n_hostname_localhost + 1);
  tt_want(n_get_interface_address6_failure ==
          prev_n_get_interface_address6_failure + 1);

  tt_assert(retval == -1);

  UNMOCK(tor_gethostname);
  UNMOCK(tor_lookup_hostname);
  UNMOCK(get_interface_address6);

  /* CASE 12:
   * Suppose the following happens:
   *   1. options->Address is NULL AND options->DirAuthorities is non-NULL
   *   2. tor_gethostname() succeeds in getting hostname of a machine ...
   *   3. ... which is successfully parsed by tor_inet_aton() ...
   *   4. into IPv4 address that tor_addr_is_inernal() considers to be
   *      internal.
   *
   *  In this case, we want resolve_my_address() to fail.
   */

  tor_free(options->Address);
  options->Address = NULL;
  options->DirAuthorities = tor_malloc_zero(sizeof(config_line_t));

  MOCK(tor_gethostname,tor_gethostname_localhost);

  prev_n_gethostname_localhost = n_gethostname_localhost;

  retval = resolve_my_address(LOG_DEBUG, options, &resolved_addr,
                              &method_used,&hostname_out);

  tt_want(n_gethostname_localhost == prev_n_gethostname_localhost + 1);
  tt_assert(retval == -1);

  UNMOCK(tor_gethostname);

 done:
  tor_free(options->Address);
  tor_free(options->DirAuthorities);
  or_options_free(options);
  tor_free(hostname_out);

  UNMOCK(tor_gethostname);
  UNMOCK(tor_lookup_hostname);
  UNMOCK(get_interface_address);
  UNMOCK(get_interface_address6);
  UNMOCK(tor_gethostname);
}

static void
test_config_adding_trusted_dir_server(void *arg)
{
  (void)arg;

  const char digest[DIGEST_LEN] = "";
  dir_server_t *ds = NULL;
  tor_addr_port_t ipv6;
  int rv = -1;

  clear_dir_servers();
  routerlist_free_all();

  /* create a trusted ds without an IPv6 address and port */
  ds = trusted_dir_server_new("ds", "127.0.0.1", 9059, 9060, NULL, digest,
                              NULL, V3_DIRINFO, 1.0);
  tt_assert(ds);
  dir_server_add(ds);
  tt_assert(get_n_authorities(V3_DIRINFO) == 1);
  tt_assert(smartlist_len(router_get_fallback_dir_servers()) == 1);

  /* create a trusted ds with an IPv6 address and port */
  rv = tor_addr_port_parse(LOG_WARN, "[::1]:9061", &ipv6.addr, &ipv6.port, -1);
  tt_assert(rv == 0);
  ds = trusted_dir_server_new("ds", "127.0.0.1", 9059, 9060, &ipv6, digest,
                              NULL, V3_DIRINFO, 1.0);
  tt_assert(ds);
  dir_server_add(ds);
  tt_assert(get_n_authorities(V3_DIRINFO) == 2);
  tt_assert(smartlist_len(router_get_fallback_dir_servers()) == 2);

 done:
  clear_dir_servers();
  routerlist_free_all();
}

static void
test_config_adding_fallback_dir_server(void *arg)
{
  (void)arg;

  const char digest[DIGEST_LEN] = "";
  dir_server_t *ds = NULL;
  tor_addr_t ipv4;
  tor_addr_port_t ipv6;
  int rv = -1;

  clear_dir_servers();
  routerlist_free_all();

  rv = tor_addr_parse(&ipv4, "127.0.0.1");
  tt_assert(rv == AF_INET);

  /* create a trusted ds without an IPv6 address and port */
  ds = fallback_dir_server_new(&ipv4, 9059, 9060, NULL, digest, 1.0);
  tt_assert(ds);
  dir_server_add(ds);
  tt_assert(smartlist_len(router_get_fallback_dir_servers()) == 1);

  /* create a trusted ds with an IPv6 address and port */
  rv = tor_addr_port_parse(LOG_WARN, "[::1]:9061", &ipv6.addr, &ipv6.port, -1);
  tt_assert(rv == 0);
  ds = fallback_dir_server_new(&ipv4, 9059, 9060, &ipv6, digest, 1.0);
  tt_assert(ds);
  dir_server_add(ds);
  tt_assert(smartlist_len(router_get_fallback_dir_servers()) == 2);

 done:
  clear_dir_servers();
  routerlist_free_all();
}

/* No secrets here:
 * v3ident is `echo "onion" | shasum | cut -d" " -f1 | tr "a-f" "A-F"`
 * fingerprint is `echo "unionem" | shasum | cut -d" " -f1 | tr "a-f" "A-F"`
 * with added spaces
 */
#define TEST_DIR_AUTH_LINE_START                                        \
                    "foobar orport=12345 "                              \
                    "v3ident=14C131DFC5C6F93646BE72FA1401C02A8DF2E8B4 "
#define TEST_DIR_AUTH_LINE_END                                          \
                    "1.2.3.4:54321 "                                    \
                    "FDB2 FBD2 AAA5 25FA 2999 E617 5091 5A32 C777 3B17"
#define TEST_DIR_AUTH_IPV6_FLAG                                         \
                    "ipv6=[feed::beef]:9 "

static void
test_config_parsing_trusted_dir_server(void *arg)
{
  (void)arg;
  int rv = -1;

  /* parse a trusted dir server without an IPv6 address and port */
  rv = parse_dir_authority_line(TEST_DIR_AUTH_LINE_START
                                TEST_DIR_AUTH_LINE_END,
                                V3_DIRINFO, 1);
  tt_assert(rv == 0);

  /* parse a trusted dir server with an IPv6 address and port */
  rv = parse_dir_authority_line(TEST_DIR_AUTH_LINE_START
                                TEST_DIR_AUTH_IPV6_FLAG
                                TEST_DIR_AUTH_LINE_END,
                                V3_DIRINFO, 1);
  tt_assert(rv == 0);

  /* Since we are only validating, there is no cleanup. */
 done:
  ;
}

#undef TEST_DIR_AUTH_LINE_START
#undef TEST_DIR_AUTH_LINE_END
#undef TEST_DIR_AUTH_IPV6_FLAG

/* No secrets here:
 * id is `echo "syn-propanethial-S-oxide" | shasum | cut -d" " -f1`
 */
#define TEST_DIR_FALLBACK_LINE                                     \
                    "1.2.3.4:54321 orport=12345 "                  \
                    "id=50e643986f31ea1235bcc1af17a1c5c5cfc0ee54 "
#define TEST_DIR_FALLBACK_IPV6_FLAG                                \
                    "ipv6=[2015:c0de::deed]:9"

static void
test_config_parsing_fallback_dir_server(void *arg)
{
  (void)arg;
  int rv = -1;

  /* parse a trusted dir server without an IPv6 address and port */
  rv = parse_dir_fallback_line(TEST_DIR_FALLBACK_LINE, 1);
  tt_assert(rv == 0);

  /* parse a trusted dir server with an IPv6 address and port */
  rv = parse_dir_fallback_line(TEST_DIR_FALLBACK_LINE
                               TEST_DIR_FALLBACK_IPV6_FLAG,
                               1);
  tt_assert(rv == 0);

  /* Since we are only validating, there is no cleanup. */
 done:
  ;
}

#undef TEST_DIR_FALLBACK_LINE
#undef TEST_DIR_FALLBACK_IPV6_FLAG

static void
test_config_adding_default_trusted_dir_servers(void *arg)
{
  (void)arg;

  clear_dir_servers();
  routerlist_free_all();

  /* Assume we only have one bridge authority */
  add_default_trusted_dir_authorities(BRIDGE_DIRINFO);
  tt_assert(get_n_authorities(BRIDGE_DIRINFO) == 1);
  tt_assert(smartlist_len(router_get_fallback_dir_servers()) == 1);

  /* Assume we have eight V3 authorities */
  add_default_trusted_dir_authorities(V3_DIRINFO);
  tt_int_op(get_n_authorities(V3_DIRINFO), OP_EQ, 8);
  tt_int_op(smartlist_len(router_get_fallback_dir_servers()), OP_EQ, 9);

 done:
  clear_dir_servers();
  routerlist_free_all();
}

static int n_add_default_fallback_dir_servers_known_default = 0;

/**
 * This mock function is meant to replace add_default_fallback_dir_servers().
 * It will parse and add one known default fallback dir server,
 * which has a dir_port of 99.
 * <b>n_add_default_fallback_dir_servers_known_default</b> is incremented by
 * one every time this function is called.
 */
static void
add_default_fallback_dir_servers_known_default(void)
{
  int i;
  const char *fallback[] = {
    "127.0.0.1:60099 orport=9009 "
    "id=0923456789012345678901234567890123456789",
    NULL
  };
  for (i=0; fallback[i]; i++) {
    if (parse_dir_fallback_line(fallback[i], 0)<0) {
      log_err(LD_BUG, "Couldn't parse internal FallbackDir line %s",
              fallback[i]);
    }
  }
  n_add_default_fallback_dir_servers_known_default++;
}

/* Test all the different combinations of adding dir servers */
static void
test_config_adding_dir_servers(void *arg)
{
  (void)arg;

  /* allocate options */
  or_options_t *options = tor_malloc_zero(sizeof(or_options_t));

  /* Allocate and populate configuration lines:
   *
   * Use the same format as the hard-coded directories in
   * add_default_trusted_dir_authorities().
   * Zeroing the structure has the same effect as initialising to:
   * { NULL, NULL, NULL, CONFIG_LINE_NORMAL, 0};
   */
  config_line_t *test_dir_authority = tor_malloc_zero(sizeof(config_line_t));
  test_dir_authority->key = tor_strdup("DirAuthority");
  test_dir_authority->value = tor_strdup(
    "D0 orport=9000 "
    "v3ident=0023456789012345678901234567890123456789 "
    "127.0.0.1:60090 0123 4567 8901 2345 6789 0123 4567 8901 2345 6789"
    );

  config_line_t *test_alt_bridge_authority = tor_malloc_zero(
                                                      sizeof(config_line_t));
  test_alt_bridge_authority->key = tor_strdup("AlternateBridgeAuthority");
  test_alt_bridge_authority->value = tor_strdup(
    "B1 orport=9001 bridge "
    "127.0.0.1:60091 1123 4567 8901 2345 6789 0123 4567 8901 2345 6789"
    );

  config_line_t *test_alt_dir_authority = tor_malloc_zero(
                                                      sizeof(config_line_t));
  test_alt_dir_authority->key = tor_strdup("AlternateDirAuthority");
  test_alt_dir_authority->value = tor_strdup(
    "A2 orport=9002 "
    "v3ident=0223456789012345678901234567890123456789 "
    "127.0.0.1:60092 2123 4567 8901 2345 6789 0123 4567 8901 2345 6789"
    );

  /* Use the format specified in the manual page */
  config_line_t *test_fallback_directory = tor_malloc_zero(
                                                      sizeof(config_line_t));
  test_fallback_directory->key = tor_strdup("FallbackDir");
  test_fallback_directory->value = tor_strdup(
    "127.0.0.1:60093 orport=9003 id=0323456789012345678901234567890123456789"
    );

  /* We need to know if add_default_fallback_dir_servers is called,
   * whatever the size of the list in fallback_dirs.inc,
   * so we use a version of add_default_fallback_dir_servers that adds
   * one known default fallback directory. */
  MOCK(add_default_fallback_dir_servers,
       add_default_fallback_dir_servers_known_default);

  /* There are 16 different cases, covering each combination of set/NULL for:
   * DirAuthorities, AlternateBridgeAuthority, AlternateDirAuthority &
   * FallbackDir. (We always set UseDefaultFallbackDirs to 1.)
   * But validate_dir_servers() ensures that:
   *   "You cannot set both DirAuthority and Alternate*Authority."
   * This reduces the number of cases to 10.
   *
   * Let's count these cases using binary, with 1 meaning set & 0 meaning NULL
   * So 1001 or case 9 is:
   *   DirAuthorities set,
   *   AlternateBridgeAuthority NULL,
   *   AlternateDirAuthority NULL
   *   FallbackDir set
   * The valid cases are cases 0-9 counting using this method, as every case
   * greater than or equal to 10 = 1010 is invalid.
   *
   * 1. Outcome: Use Set Directory Authorities
   *   - No Default Authorities
   *   - Use AlternateBridgeAuthority, AlternateDirAuthority, and FallbackDir
   *     if they are set
   *   Cases expected to yield this outcome:
   *     8 & 9 (the 2 valid cases where DirAuthorities is set)
   *     6 & 7 (the 2 cases where DirAuthorities is NULL, and
   *           AlternateBridgeAuthority and AlternateDirAuthority are both set)
   *
   * 2. Outcome: Use Set Bridge Authority
   *  - Use Default Non-Bridge Directory Authorities
   *  - Use FallbackDir if it is set, otherwise use default FallbackDir
   *  Cases expected to yield this outcome:
   *    4 & 5 (the 2 cases where DirAuthorities is NULL,
   *           AlternateBridgeAuthority is set, and
   *           AlternateDirAuthority is NULL)
   *
   * 3. Outcome: Use Set Alternate Directory Authority
   *  - Use Default Bridge Authorities
   *  - Use FallbackDir if it is set, otherwise No Default Fallback Directories
   *  Cases expected to yield this outcome:
   *    2 & 3 (the 2 cases where DirAuthorities and AlternateBridgeAuthority
   *           are both NULL, but AlternateDirAuthority is set)
   *
   * 4. Outcome: Use Set Custom Fallback Directory
   *  - Use Default Bridge & Directory Authorities
   *  Cases expected to yield this outcome:
   *    1 (DirAuthorities, AlternateBridgeAuthority and AlternateDirAuthority
   *       are all NULL, but FallbackDir is set)
   *
   * 5. Outcome: Use All Defaults
   *  - Use Default Bridge & Directory Authorities, and
   *    Default Fallback Directories
   *  Cases expected to yield this outcome:
   *    0 (DirAuthorities, AlternateBridgeAuthority, AlternateDirAuthority
   *       and FallbackDir are all NULL)
   */

  /*
   * Find out how many default Bridge, Non-Bridge and Fallback Directories
   * are hard-coded into this build.
   * This code makes some assumptions about the implementation.
   * If they are wrong, one or more of cases 0-5 could fail.
   */
  int n_default_alt_bridge_authority = 0;
  int n_default_alt_dir_authority = 0;
  int n_default_fallback_dir = 0;
#define n_default_authorities ((n_default_alt_bridge_authority) \
                               + (n_default_alt_dir_authority))

  /* Pre-Count Number of Authorities of Each Type
   * Use 0000: No Directory Authorities or Fallback Directories Set
   */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0000 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = NULL;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 1);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();

      /* Count Bridge Authorities */
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if it's a bridge auth */
                        n_default_alt_bridge_authority +=
                        ((ds->is_authority && (ds->type & BRIDGE_DIRINFO)) ?
                         1 : 0)
                        );
      /* If we have no default bridge authority, something has gone wrong */
      tt_assert(n_default_alt_bridge_authority >= 1);

      /* Count v3 Authorities */
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment found counter if it's a v3 auth */
                        n_default_alt_dir_authority +=
                        ((ds->is_authority && (ds->type & V3_DIRINFO)) ?
                         1 : 0)
                        );
      /* If we have no default authorities, something has gone really wrong */
      tt_assert(n_default_alt_dir_authority >= 1);

      /* Calculate Fallback Directory Count */
      n_default_fallback_dir = (smartlist_len(fallback_servers) -
                                n_default_alt_bridge_authority -
                                n_default_alt_dir_authority);
      /* If we have a negative count, something has gone really wrong,
       * or some authorities aren't being added as fallback directories.
       * (networkstatus_consensus_can_use_extra_fallbacks depends on all
       * authorities being fallback directories.) */
      tt_assert(n_default_fallback_dir >= 0);
    }
  }

  /*
   * 1. Outcome: Use Set Directory Authorities
   *   - No Default Authorities
   *   - Use AlternateBridgeAuthority, AlternateDirAuthority, and FallbackDir
   *     if they are set
   *   Cases expected to yield this outcome:
   *     8 & 9 (the 2 valid cases where DirAuthorities is set)
   *     6 & 7 (the 2 cases where DirAuthorities is NULL, and
   *           AlternateBridgeAuthority and AlternateDirAuthority are both set)
   */

  /* Case 9: 1001 - DirAuthorities Set, AlternateBridgeAuthority Not Set,
     AlternateDirAuthority Not Set, FallbackDir Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 1001 */
    options->DirAuthorities = test_dir_authority;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = test_fallback_directory;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* D0, (No B1), (No A2) */
      tt_assert(smartlist_len(dir_servers) == 1);

      /* DirAuthority - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 1);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* D0, (No B1), (No A2), Custom Fallback */
      tt_assert(smartlist_len(fallback_servers) == 2);

      /* DirAuthority - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 1);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* Custom FallbackDir - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 1);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);
    }
  }

  /* Case 8: 1000 - DirAuthorities Set, Others Not Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 1000 */
    options->DirAuthorities = test_dir_authority;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = NULL;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we just have the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 0);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* D0, (No B1), (No A2) */
      tt_assert(smartlist_len(dir_servers) == 1);

      /* DirAuthority - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 1);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* D0, (No B1), (No A2), (No Fallback) */
      tt_assert(smartlist_len(fallback_servers) == 1);

      /* DirAuthority - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 1);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* (No Custom FallbackDir) - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 0);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);
    }
  }

  /* Case 7: 0111 - DirAuthorities Not Set, Others Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0111 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = test_alt_bridge_authority;
    options->AlternateDirAuthority = test_alt_dir_authority;
    options->FallbackDir = test_fallback_directory;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), B1, A2 */
      tt_assert(smartlist_len(dir_servers) == 2);

      /* (No DirAuthority) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), B1, A2, Custom Fallback */
      tt_assert(smartlist_len(fallback_servers) == 3);

      /* (No DirAuthority) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);

      /* Custom FallbackDir - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 1);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);
    }
  }

  /* Case 6: 0110 - DirAuthorities Not Set, AlternateBridgeAuthority &
     AlternateDirAuthority Set, FallbackDir Not Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0110 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = test_alt_bridge_authority;
    options->AlternateDirAuthority = test_alt_dir_authority;
    options->FallbackDir = NULL;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 0);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), B1, A2 */
      tt_assert(smartlist_len(dir_servers) == 2);

      /* (No DirAuthority) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), B1, A2, (No Fallback) */
      tt_assert(smartlist_len(fallback_servers) == 2);

      /* (No DirAuthority) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);

      /* (No Custom FallbackDir) - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 0);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);
    }
  }

  /*
   2. Outcome: Use Set Bridge Authority
     - Use Default Non-Bridge Directory Authorities
     - Use FallbackDir if it is set, otherwise use default FallbackDir
     Cases expected to yield this outcome:
       4 & 5 (the 2 cases where DirAuthorities is NULL,
              AlternateBridgeAuthority is set, and
              AlternateDirAuthority is NULL)
  */

  /* Case 5: 0101 - DirAuthorities Not Set, AlternateBridgeAuthority Set,
     AlternateDirAuthority Not Set, FallbackDir Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0101 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = test_alt_bridge_authority;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = test_fallback_directory;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), B1, (No A2), Default v3 Non-Bridge Authorities */
      tt_assert(smartlist_len(dir_servers) == 1 + n_default_alt_dir_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* There's no easy way of checking that we have included all the
       * default v3 non-Bridge directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), B1, (No A2), Default v3 Non-Bridge Authorities,
       * Custom Fallback */
      tt_assert(smartlist_len(fallback_servers) ==
                2 + n_default_alt_dir_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* Custom FallbackDir - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 1);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);

      /* There's no easy way of checking that we have included all the
       * default v3 non-Bridge directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }
  }

  /* Case 4: 0100 - DirAuthorities Not Set, AlternateBridgeAuthority Set,
   AlternateDirAuthority & FallbackDir Not Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0100 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = test_alt_bridge_authority;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = NULL;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 1);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), B1, (No A2), Default v3 Non-Bridge Authorities */
      tt_assert(smartlist_len(dir_servers) == 1 + n_default_alt_dir_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* There's no easy way of checking that we have included all the
       * default v3 non-Bridge directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), B1, (No A2), Default v3 Non-Bridge Authorities,
       * Default Fallback */
      tt_assert(smartlist_len(fallback_servers) ==
                2 + n_default_alt_dir_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* AlternateBridgeAuthority - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 1);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* (No Custom FallbackDir) - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 0);

      /* Default FallbackDir - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 1);

      /* There's no easy way of checking that we have included all the
       * default v3 non-Bridge directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }
  }

  /*
   3. Outcome: Use Set Alternate Directory Authority
     - Use Default Bridge Authorities
     - Use FallbackDir if it is set, otherwise No Default Fallback Directories
     Cases expected to yield this outcome:
       2 & 3 (the 2 cases where DirAuthorities and AlternateBridgeAuthority
              are both NULL, but AlternateDirAuthority is set)
  */

  /* Case 3: 0011 - DirAuthorities & AlternateBridgeAuthority Not Set,
     AlternateDirAuthority & FallbackDir Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0011 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = test_alt_dir_authority;
    options->FallbackDir = test_fallback_directory;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities, A2 */
      tt_assert(smartlist_len(dir_servers) ==
                1 + n_default_alt_bridge_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);

      /* There's no easy way of checking that we have included all the
       * default Bridge authorities (except for hard-coding tonga's details),
       * so let's assume that if the total count above is correct,
       * we have the right ones.
       */
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities, A2,
       * Custom Fallback Directory, (No Default Fallback Directories) */
      tt_assert(smartlist_len(fallback_servers) ==
                2 + n_default_alt_bridge_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);

      /* Custom FallbackDir - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 1);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);

      /* There's no easy way of checking that we have included all the
       * default Bridge authorities (except for hard-coding tonga's details),
       * so let's assume that if the total count above is correct,
       * we have the right ones.
       */
    }
  }

  /* Case 2: 0010 - DirAuthorities & AlternateBridgeAuthority Not Set,
   AlternateDirAuthority Set, FallbackDir Not Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0010 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = test_alt_dir_authority;
    options->FallbackDir = NULL;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we just have the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 0);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities, A2,
       * No Default or Custom Fallback Directories */
      tt_assert(smartlist_len(dir_servers) ==
                1 + n_default_alt_bridge_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);

      /* There's no easy way of checking that we have included all the
       * default Bridge authorities (except for hard-coding tonga's details),
       * so let's assume that if the total count above is correct,
       * we have the right ones.
       */
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities, A2,
       * No Custom or Default Fallback Directories */
      tt_assert(smartlist_len(fallback_servers) ==
                1 + n_default_alt_bridge_authority);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* AlternateDirAuthority - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 1);

      /* (No Custom FallbackDir) - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 0);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);

      /* There's no easy way of checking that we have included all the
       * default Bridge authorities (except for hard-coding tonga's details),
       * so let's assume that if the total count above is correct,
       * we have the right ones.
       */
    }
  }

  /*
   4. Outcome: Use Set Custom Fallback Directory
     - Use Default Bridge & Directory Authorities
     Cases expected to yield this outcome:
       1 (DirAuthorities, AlternateBridgeAuthority and AlternateDirAuthority
          are all NULL, but FallbackDir is set)
  */

  /* Case 1: 0001 - DirAuthorities, AlternateBridgeAuthority
    & AlternateDirAuthority Not Set, FallbackDir Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0001 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = test_fallback_directory;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must not have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 0);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities,
       * (No A2), Default v3 Directory Authorities */
      tt_assert(smartlist_len(dir_servers) == n_default_authorities);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* There's no easy way of checking that we have included all the
       * default Bridge & V3 Directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities,
       * (No A2), Default v3 Directory Authorities,
       * Custom Fallback Directory, (No Default Fallback Directories) */
      tt_assert(smartlist_len(fallback_servers) ==
                1 + n_default_authorities);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* Custom FallbackDir - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 1);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 0);

      /* There's no easy way of checking that we have included all the
       * default Bridge & V3 Directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }
  }

  /*
   5. Outcome: Use All Defaults
     - Use Default Bridge & Directory Authorities, Default Fallback Directories
     Cases expected to yield this outcome:
       0 (DirAuthorities, AlternateBridgeAuthority, AlternateDirAuthority
          and FallbackDir are all NULL)
  */

  /* Case 0: 0000 - All Not Set */
  {
    /* clear fallback dirs counter */
    n_add_default_fallback_dir_servers_known_default = 0;

    /* clear options*/
    memset(options, 0, sizeof(or_options_t));

    /* clear any previous dir servers:
     consider_adding_dir_servers() should do this anyway */
    clear_dir_servers();

    /* assign options: 0001 */
    options->DirAuthorities = NULL;
    options->AlternateBridgeAuthority = NULL;
    options->AlternateDirAuthority = NULL;
    options->FallbackDir = NULL;
    options->UseDefaultFallbackDirs = 1;

    /* parse options - ensure we always update by passing NULL old_options */
    consider_adding_dir_servers(options, NULL);

    /* check outcome */

    /* we must have added the default fallback dirs */
    tt_assert(n_add_default_fallback_dir_servers_known_default == 1);

    /* we have more fallbacks than just the authorities */
    tt_assert(networkstatus_consensus_can_use_extra_fallbacks(options) == 1);

    {
      /* trusted_dir_servers */
      const smartlist_t *dir_servers = router_get_trusted_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities,
       * (No A2), Default v3 Directory Authorities */
      tt_assert(smartlist_len(dir_servers) == n_default_authorities);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(dir_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* There's no easy way of checking that we have included all the
       * default Bridge & V3 Directory authorities, so let's assume that
       * if the total count above is correct, we have the right ones.
       */
    }

    {
      /* fallback_dir_servers */
      const smartlist_t *fallback_servers = router_get_fallback_dir_servers();
      /* (No D0), (No B1), Default Bridge Authorities,
       * (No A2), Default v3 Directory Authorities,
       * (No Custom Fallback Directory), Default Fallback Directories */
      tt_assert(smartlist_len(fallback_servers) ==
                n_default_authorities + n_default_fallback_dir);

      /* (No DirAuthorities) - D0 - dir_port: 60090 */
      int found_D0 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_D0 +=
                        (ds->dir_port == 60090 ?
                         1 : 0)
                        );
      tt_assert(found_D0 == 0);

      /* (No AlternateBridgeAuthority) - B1 - dir_port: 60091 */
      int found_B1 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_B1 +=
                        (ds->dir_port == 60091 ?
                         1 : 0)
                        );
      tt_assert(found_B1 == 0);

      /* (No AlternateDirAuthority) - A2 - dir_port: 60092 */
      int found_A2 = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_A2 +=
                        (ds->dir_port == 60092 ?
                         1 : 0)
                        );
      tt_assert(found_A2 == 0);

      /* Custom FallbackDir - No Nickname - dir_port: 60093 */
      int found_non_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_non_default_fallback +=
                        (ds->dir_port == 60093 ?
                         1 : 0)
                        );
      tt_assert(found_non_default_fallback == 0);

      /* (No Default FallbackDir) - No Nickname - dir_port: 60099 */
      int found_default_fallback = 0;
      SMARTLIST_FOREACH(fallback_servers,
                        dir_server_t *,
                        ds,
                        /* increment the found counter if dir_port matches */
                        found_default_fallback +=
                        (ds->dir_port == 60099 ?
                         1 : 0)
                        );
      tt_assert(found_default_fallback == 1);

      /* There's no easy way of checking that we have included all the
       * default Bridge & V3 Directory authorities, and the default
       * Fallback Directories, so let's assume that if the total count
       * above is correct, we have the right ones.
       */
    }
  }

  done:
  clear_dir_servers();

  tor_free(test_dir_authority->key);
  tor_free(test_dir_authority->value);
  tor_free(test_dir_authority);

  tor_free(test_alt_dir_authority->key);
  tor_free(test_alt_dir_authority->value);
  tor_free(test_alt_dir_authority);

  tor_free(test_alt_bridge_authority->key);
  tor_free(test_alt_bridge_authority->value);
  tor_free(test_alt_bridge_authority);

  tor_free(test_fallback_directory->key);
  tor_free(test_fallback_directory->value);
  tor_free(test_fallback_directory);

  options->DirAuthorities = NULL;
  options->AlternateBridgeAuthority = NULL;
  options->AlternateDirAuthority = NULL;
  options->FallbackDir = NULL;
  or_options_free(options);

  UNMOCK(add_default_fallback_dir_servers);
}

static void
test_config_default_dir_servers(void *arg)
{
  or_options_t *opts = NULL;
  (void)arg;
  int trusted_count = 0;
  int fallback_count = 0;

  /* new set of options should stop fallback parsing */
  opts = tor_malloc_zero(sizeof(or_options_t));
  opts->UseDefaultFallbackDirs = 0;
  /* set old_options to NULL to force dir update */
  consider_adding_dir_servers(opts, NULL);
  trusted_count = smartlist_len(router_get_trusted_dir_servers());
  fallback_count = smartlist_len(router_get_fallback_dir_servers());
  or_options_free(opts);
  opts = NULL;

  /* assume a release will never go out with less than 7 authorities */
  tt_assert(trusted_count >= 7);
  /* if we disable the default fallbacks, there must not be any extra */
  tt_assert(fallback_count == trusted_count);

  opts = tor_malloc_zero(sizeof(or_options_t));
  opts->UseDefaultFallbackDirs = 1;
  consider_adding_dir_servers(opts, opts);
  trusted_count = smartlist_len(router_get_trusted_dir_servers());
  fallback_count = smartlist_len(router_get_fallback_dir_servers());
  or_options_free(opts);
  opts = NULL;

  /* assume a release will never go out with less than 7 authorities */
  tt_assert(trusted_count >= 7);
  /* XX/teor - allow for default fallbacks to be added without breaking
   * the unit tests. Set a minimum fallback count once the list is stable. */
  tt_assert(fallback_count >= trusted_count);

 done:
  or_options_free(opts);
}

static int mock_router_pick_published_address_result = 0;

static int
mock_router_pick_published_address(const or_options_t *options,
                                   uint32_t *addr, int cache_only)
{
  (void)options;
  (void)addr;
  (void)cache_only;
  return mock_router_pick_published_address_result;
}

static int mock_router_my_exit_policy_is_reject_star_result = 0;

static int
mock_router_my_exit_policy_is_reject_star(void)
{
  return mock_router_my_exit_policy_is_reject_star_result;
}

static int mock_advertised_server_mode_result = 0;

static int
mock_advertised_server_mode(void)
{
  return mock_advertised_server_mode_result;
}

static routerinfo_t *mock_router_get_my_routerinfo_result = NULL;

static const routerinfo_t *
mock_router_get_my_routerinfo(void)
{
  return mock_router_get_my_routerinfo_result;
}

static void
test_config_directory_fetch(void *arg)
{
  (void)arg;

  /* Test Setup */
  or_options_t *options = tor_malloc_zero(sizeof(or_options_t));
  routerinfo_t routerinfo;
  memset(&routerinfo, 0, sizeof(routerinfo));
  mock_router_pick_published_address_result = -1;
  mock_router_my_exit_policy_is_reject_star_result = 1;
  mock_advertised_server_mode_result = 0;
  mock_router_get_my_routerinfo_result = NULL;
  MOCK(router_pick_published_address, mock_router_pick_published_address);
  MOCK(router_my_exit_policy_is_reject_star,
       mock_router_my_exit_policy_is_reject_star);
  MOCK(advertised_server_mode, mock_advertised_server_mode);
  MOCK(router_get_my_routerinfo, mock_router_get_my_routerinfo);

  /* Clients can use multiple directory mirrors for bootstrap */
  memset(options, 0, sizeof(or_options_t));
  options->ClientOnly = 1;
  tt_assert(server_mode(options) == 0);
  tt_assert(public_server_mode(options) == 0);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 1);

  /* Bridge Clients can use multiple directory mirrors for bootstrap */
  memset(options, 0, sizeof(or_options_t));
  options->UseBridges = 1;
  tt_assert(server_mode(options) == 0);
  tt_assert(public_server_mode(options) == 0);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 1);

  /* Bridge Relays (Bridges) must act like clients, and use multiple
   * directory mirrors for bootstrap */
  memset(options, 0, sizeof(or_options_t));
  options->BridgeRelay = 1;
  options->ORPort_set = 1;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 0);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 1);

  /* Clients set to FetchDirInfoEarly must fetch it from the authorities,
   * but can use multiple authorities for bootstrap */
  memset(options, 0, sizeof(or_options_t));
  options->FetchDirInfoEarly = 1;
  tt_assert(server_mode(options) == 0);
  tt_assert(public_server_mode(options) == 0);
  tt_assert(directory_fetches_from_authorities(options) == 1);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 1);

  /* OR servers only fetch the consensus from the authorities when they don't
   * know their own address, but never use multiple directories for bootstrap
   */
  memset(options, 0, sizeof(or_options_t));
  options->ORPort_set = 1;

  mock_router_pick_published_address_result = -1;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 1);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  mock_router_pick_published_address_result = 0;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  /* Exit OR servers only fetch the consensus from the authorities when they
   * refuse unknown exits, but never use multiple directories for bootstrap
   */
  memset(options, 0, sizeof(or_options_t));
  options->ORPort_set = 1;
  options->ExitRelay = 1;
  mock_router_pick_published_address_result = 0;
  mock_router_my_exit_policy_is_reject_star_result = 0;
  mock_advertised_server_mode_result = 1;
  mock_router_get_my_routerinfo_result = &routerinfo;

  routerinfo.supports_tunnelled_dir_requests = 1;

  options->RefuseUnknownExits = 1;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 1);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  options->RefuseUnknownExits = 0;
  mock_router_pick_published_address_result = 0;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  /* Dir servers fetch the consensus from the authorities, unless they are not
   * advertising themselves (hibernating) or have no routerinfo or are not
   * advertising their dirport, and never use multiple directories for
   * bootstrap. This only applies if they are also OR servers.
   * (We don't care much about the behaviour of non-OR directory servers.) */
  memset(options, 0, sizeof(or_options_t));
  options->DirPort_set = 1;
  options->ORPort_set = 1;
  options->DirCache = 1;
  mock_router_pick_published_address_result = 0;
  mock_router_my_exit_policy_is_reject_star_result = 1;

  mock_advertised_server_mode_result = 1;
  routerinfo.dir_port = 1;
  mock_router_get_my_routerinfo_result = &routerinfo;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 1);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  mock_advertised_server_mode_result = 0;
  routerinfo.dir_port = 1;
  mock_router_get_my_routerinfo_result = &routerinfo;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  mock_advertised_server_mode_result = 1;
  mock_router_get_my_routerinfo_result = NULL;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  mock_advertised_server_mode_result = 1;
  routerinfo.dir_port = 0;
  routerinfo.supports_tunnelled_dir_requests = 0;
  mock_router_get_my_routerinfo_result = &routerinfo;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 0);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

  mock_advertised_server_mode_result = 1;
  routerinfo.dir_port = 1;
  routerinfo.supports_tunnelled_dir_requests = 1;
  mock_router_get_my_routerinfo_result = &routerinfo;
  tt_assert(server_mode(options) == 1);
  tt_assert(public_server_mode(options) == 1);
  tt_assert(directory_fetches_from_authorities(options) == 1);
  tt_assert(networkstatus_consensus_can_use_multiple_directories(options)
            == 0);

 done:
  tor_free(options);
  UNMOCK(router_pick_published_address);
  UNMOCK(router_get_my_routerinfo);
  UNMOCK(advertised_server_mode);
  UNMOCK(router_my_exit_policy_is_reject_star);
}

static void
test_config_default_fallback_dirs(void *arg)
{
  const char *fallback[] = {
#include "../or/fallback_dirs.inc"
    NULL
  };

  int n_included_fallback_dirs = 0;
  int n_added_fallback_dirs = 0;

  (void)arg;
  clear_dir_servers();

  while (fallback[n_included_fallback_dirs])
    n_included_fallback_dirs++;

  add_default_fallback_dir_servers();

  n_added_fallback_dirs = smartlist_len(router_get_fallback_dir_servers());

  tt_assert(n_included_fallback_dirs == n_added_fallback_dirs);

  done:
  clear_dir_servers();
}

static void
test_config_port_cfg_line_extract_addrport(void *arg)
{
  (void)arg;
  int unixy = 0;
  const char *rest = NULL;
  char *a = NULL;

  tt_int_op(port_cfg_line_extract_addrport("", &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("hello", &a, &unixy, &rest),
            OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "hello");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport(" flipperwalt gersplut",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "flipperwalt");;
  tt_str_op(rest, OP_EQ, "gersplut");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport(" flipperwalt \t gersplut",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "flipperwalt");;
  tt_str_op(rest, OP_EQ, "gersplut");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("flipperwalt \t gersplut",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "flipperwalt");;
  tt_str_op(rest, OP_EQ, "gersplut");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:flipperwalt \t gersplut",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "flipperwalt");;
  tt_str_op(rest, OP_EQ, "gersplut");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("lolol",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:lolol",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:lolol ",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport(" unix:lolol",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("foobar:lolol",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, "foobar:lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport(":lolol",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 0);
  tt_str_op(a, OP_EQ, ":lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lolol\"",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lolol\" ",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lolol\" foo ",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lolol");;
  tt_str_op(rest, OP_EQ, "foo ");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lol ol\" foo ",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lol ol");;
  tt_str_op(rest, OP_EQ, "foo ");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lol\\\" ol\" foo ",
                                           &a, &unixy, &rest), OP_EQ, 0);
  tt_int_op(unixy, OP_EQ, 1);
  tt_str_op(a, OP_EQ, "lol\" ol");;
  tt_str_op(rest, OP_EQ, "foo ");
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lol\\\" ol foo ",
                                           &a, &unixy, &rest), OP_EQ, -1);
  tor_free(a);

  tt_int_op(port_cfg_line_extract_addrport("unix:\"lol\\0\" ol foo ",
                                           &a, &unixy, &rest), OP_EQ, -1);
  tor_free(a);

 done:
  tor_free(a);
}

static config_line_t *
mock_config_line(const char *key, const char *val)
{
  config_line_t *config_line = tor_malloc(sizeof(config_line_t));
  memset(config_line, 0, sizeof(config_line_t));
  config_line->key = tor_strdup(key);
  config_line->value = tor_strdup(val);
  return config_line;
}

static void
test_config_parse_port_config__listenaddress(void *data)
{
  (void)data;
  int ret;
  config_line_t *config_listen_address = NULL, *config_listen_address2 = NULL,
    *config_listen_address3 = NULL;
  config_line_t *config_port1 = NULL, *config_port2 = NULL,
    *config_port3 = NULL, *config_port4 = NULL, *config_port5 = NULL;
  smartlist_t *slout = NULL;
  port_cfg_t *port_cfg = NULL;

  // Test basic invocation with no arguments
  ret = parse_port_config(NULL, NULL, NULL, NULL, 0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Setup some test data
  config_listen_address = mock_config_line("DNSListenAddress", "127.0.0.1");
  config_listen_address2 = mock_config_line("DNSListenAddress", "x$$$:::345");
  config_listen_address3 = mock_config_line("DNSListenAddress",
                                            "127.0.0.1:1442");
  config_port1 = mock_config_line("DNSPort", "42");
  config_port2 = mock_config_line("DNSPort", "43");
  config_port1->next = config_port2;
  config_port3 = mock_config_line("DNSPort", "auto");
  config_port4 = mock_config_line("DNSPort", "55542");
  config_port5 = mock_config_line("DNSPort", "666777");

  // Test failure when we have a ListenAddress line and several
  // Port lines for the same portname
  ret = parse_port_config(NULL, config_port1, config_listen_address, "DNS", 0,
                          NULL, 0, 0);

  tt_int_op(ret, OP_EQ, -1);

  // Test case when we have a listen address, no default port and allow
  // spurious listen address lines
  ret = parse_port_config(NULL, NULL, config_listen_address, "DNS", 0, NULL,
                          0, CL_PORT_ALLOW_EXTRA_LISTENADDR);
  tt_int_op(ret, OP_EQ, 1);

  // Test case when we have a listen address, no default port but doesn't
  // allow spurious listen address lines
  ret = parse_port_config(NULL, NULL, config_listen_address, "DNS", 0, NULL,
                          0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test case when we have a listen address, and a port that points to auto,
  // should use the AUTO port
  slout = smartlist_new();
  ret = parse_port_config(slout, config_port3, config_listen_address, "DNS",
                          0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, CFG_AUTO_PORT);

  // Test when we have a listen address and a custom port
  ret = parse_port_config(slout, config_port4, config_listen_address, "DNS",
                          0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 2);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 1);
  tt_int_op(port_cfg->port, OP_EQ, 55542);

  // Test when we have a listen address and an invalid custom port
  ret = parse_port_config(slout, config_port5, config_listen_address, "DNS",
                          0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test we get a server port configuration when asked for it
  ret = parse_port_config(slout, NULL, config_listen_address, "DNS", 0, NULL,
                          123, CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 4);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 2);
  tt_int_op(port_cfg->port, OP_EQ, 123);
  tt_int_op(port_cfg->server_cfg.no_listen, OP_EQ, 1);
  tt_int_op(port_cfg->server_cfg.bind_ipv4_only, OP_EQ, 1);

  // Test an invalid ListenAddress configuration
  ret = parse_port_config(NULL, NULL, config_listen_address2, "DNS", 0, NULL,
                          222, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test default to the port in the listen address if available
  ret = parse_port_config(slout, config_port2, config_listen_address3, "DNS",
                          0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 5);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 4);
  tt_int_op(port_cfg->port, OP_EQ, 1442);

  // Test we work correctly without an out, but with a listen address
  // and a port
  ret = parse_port_config(NULL, config_port2, config_listen_address, "DNS",
                          0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test warning nonlocal control
  ret = parse_port_config(slout, config_port2, config_listen_address, "DNS",
                          CONN_TYPE_CONTROL_LISTENER, NULL, 0,
                          CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test warning nonlocal ext or listener
  ret = parse_port_config(slout, config_port2, config_listen_address, "DNS",
                          CONN_TYPE_EXT_OR_LISTENER, NULL, 0,
                          CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test warning nonlocal other
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, config_port2, config_listen_address, "DNS",
                          0, NULL, 0, CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test warning nonlocal control without an out
  ret = parse_port_config(NULL, config_port2, config_listen_address, "DNS",
                          CONN_TYPE_CONTROL_LISTENER, NULL, 0,
                          CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

 done:
  config_free_lines(config_listen_address);
  config_free_lines(config_listen_address2);
  config_free_lines(config_listen_address3);
  config_free_lines(config_port1);
  /* 2 was linked from 1. */
  config_free_lines(config_port3);
  config_free_lines(config_port4);
  config_free_lines(config_port5);
  if (slout)
    SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_free(slout);
}

static void
test_config_parse_port_config__ports__no_ports_given(void *data)
{
  (void)data;
  int ret;
  smartlist_t *slout = NULL;
  port_cfg_t *port_cfg = NULL;

  slout = smartlist_new();

  // Test no defaultport, no defaultaddress and no out
  ret = parse_port_config(NULL, NULL, NULL, "DNS", 0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test with defaultport, no defaultaddress and no out
  ret = parse_port_config(NULL, NULL, NULL, "DNS", 0, NULL, 42, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test no defaultport, with defaultaddress and no out
  ret = parse_port_config(NULL, NULL, NULL, "DNS", 0, "127.0.0.2", 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test with defaultport, with defaultaddress and no out
  ret = parse_port_config(NULL, NULL, NULL, "DNS", 0, "127.0.0.2", 42, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test no defaultport, no defaultaddress and with out
  ret = parse_port_config(slout, NULL, NULL, "DNS", 0, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 0);

  // Test with defaultport, no defaultaddress and with out
  ret = parse_port_config(slout, NULL, NULL, "DNS", 0, NULL, 42, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 0);

  // Test no defaultport, with defaultaddress and with out
  ret = parse_port_config(slout, NULL, NULL, "DNS", 0, "127.0.0.2", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 0);

  // Test with defaultport, with defaultaddress and out, adds a new port cfg
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, NULL, NULL, "DNS", 0, "127.0.0.2", 42, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, 42);
  tt_int_op(port_cfg->is_unix_addr, OP_EQ, 0);

  // Test with defaultport, with defaultaddress and out, adds a new port cfg
  // for a unix address
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, NULL, NULL, "DNS", 0, "/foo/bar/unixdomain",
                          42, CL_PORT_IS_UNIXSOCKET);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, 0);
  tt_int_op(port_cfg->is_unix_addr, OP_EQ, 1);
  tt_str_op(port_cfg->unix_addr, OP_EQ, "/foo/bar/unixdomain");

 done:
  if (slout)
    SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_free(slout);
}

static void
test_config_parse_port_config__ports__ports_given(void *data)
{
  (void)data;
  int ret;
  smartlist_t *slout = NULL;
  port_cfg_t *port_cfg = NULL;
  config_line_t *config_port_invalid = NULL, *config_port_valid = NULL;
  tor_addr_t addr;

  slout = smartlist_new();

  // Test error when encounters an invalid Port specification
  config_port_invalid = mock_config_line("DNSPort", "");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS", 0, NULL,
                          0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test error when encounters an empty unix domain specification
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("DNSPort", "unix:");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS", 0, NULL,
                          0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test error when encounters a unix domain specification but the listener
  // doesn't support domain sockets
  config_port_valid = mock_config_line("DNSPort", "unix:/tmp/foo/bar");
  ret = parse_port_config(NULL, config_port_valid, NULL, "DNS",
                          CONN_TYPE_AP_DNS_LISTENER, NULL, 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test valid unix domain
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0, 0);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, 0);
  tt_int_op(port_cfg->is_unix_addr, OP_EQ, 1);
  tt_str_op(port_cfg->unix_addr, OP_EQ, "/tmp/foo/bar");
  /* Test entry port defaults as initialised in parse_port_config */
  tt_int_op(port_cfg->entry_cfg.dns_request, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.onion_traffic, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.cache_ipv4_answers, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.prefer_ipv6_virtaddr, OP_EQ, 1);
#endif

  // Test failure if we have no ipv4 and no ipv6 and no onion (DNS only)
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("SOCKSPort",
                                         "unix:/tmp/foo/bar NoIPv4Traffic "
                                         "NoOnionTraffic");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure if we have no DNS and we're a DNSPort
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("DNSPort",
                                         "127.0.0.1:80 NoDNSRequest");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS",
                          CONN_TYPE_AP_DNS_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, -1);

  // If we're a DNSPort, DNS only is ok
  // Use a port because DNSPort doesn't support sockets
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "127.0.0.1:80 "
                                       "NoIPv4Traffic NoOnionTraffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS",
                          CONN_TYPE_AP_DNS_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.dns_request, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.onion_traffic, OP_EQ, 0);

  // Test failure if we have DNS but no ipv4 and no ipv6
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("SOCKSPort",
                                         "unix:/tmp/foo/bar NoIPv4Traffic");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with no DNS, no ipv4, no ipv6 (only onion, using separate
  // options)
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:/tmp/foo/bar "
                                       "NoDNSRequest NoIPv4Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.dns_request, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.onion_traffic, OP_EQ, 1);
#endif

  // Test success with quoted unix: address.
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:\"/tmp/foo/ bar\" "
                                       "NoDNSRequest NoIPv4Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.dns_request, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.onion_traffic, OP_EQ, 1);
#endif

  // Test failure with broken quoted unix: address.
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:\"/tmp/foo/ bar "
                                       "NoDNSRequest NoIPv4Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure with empty quoted unix: address.
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:\"\" "
                                       "NoDNSRequest NoIPv4Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with OnionTrafficOnly (no DNS, no ipv4, no ipv6)
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:/tmp/foo/bar "
                                       "OnionTrafficOnly");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.dns_request, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.onion_traffic, OP_EQ, 1);
#endif

  // Test success with no ipv4 but take ipv6
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:/tmp/foo/bar "
                                       "NoIPv4Traffic IPv6Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 1);
#endif

  // Test success with both ipv4 and ipv6
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:/tmp/foo/bar "
                                       "IPv4Traffic IPv6Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, NULL, 0,
                          CL_PORT_TAKES_HOSTNAMES);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 1);
#endif

  // Test failure if we specify world writable for an IP Port
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("DNSPort", "42 WorldWritable");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure if we specify group writable for an IP Port
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("DNSPort", "42 GroupWritable");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure if we specify group writable for an IP Port
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("DNSPort", "42 RelaxDirModeCheck");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with only a port (this will fail without a default address)
  config_free_lines(config_port_valid); config_port_valid = NULL;
  config_port_valid = mock_config_line("DNSPort", "42");
  ret = parse_port_config(NULL, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test success with only a port and isolate destination port
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 IsolateDestPort");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.isolation_flags, OP_EQ,
            ISO_DEFAULT | ISO_DESTPORT);

  // Test success with a negative isolate destination port, and plural
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 NoIsolateDestPorts");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.isolation_flags, OP_EQ,
            ISO_DEFAULT & ~ISO_DESTPORT);

  // Test success with isolate destination address
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 IsolateDestAddr");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.isolation_flags, OP_EQ,
            ISO_DEFAULT | ISO_DESTADDR);

  // Test success with isolate socks AUTH
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 IsolateSOCKSAuth");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.isolation_flags, OP_EQ,
            ISO_DEFAULT | ISO_SOCKSAUTH);

  // Test success with isolate client protocol
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 IsolateClientProtocol");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.isolation_flags, OP_EQ,
            ISO_DEFAULT | ISO_CLIENTPROTO);

  // Test success with isolate client address
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 IsolateClientAddr");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.isolation_flags, OP_EQ,
            ISO_DEFAULT | ISO_CLIENTADDR);

  // Test success with ignored unknown options
  config_free_lines(config_port_valid); config_port_valid = NULL;
  config_port_valid = mock_config_line("DNSPort", "42 ThisOptionDoesntExist");
  ret = parse_port_config(NULL, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  // Test success with no isolate socks AUTH
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 NoIsolateSOCKSAuth");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.3", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.socks_prefer_no_auth, OP_EQ, 1);

  // Test success with prefer ipv6
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort",
                                       "42 IPv6Traffic PreferIPv6");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, "127.0.0.42", 0,
                          CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.prefer_ipv6, OP_EQ, 1);

  // Test success with cache ipv4 DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 CacheIPv4DNS");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.cache_ipv4_answers, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.cache_ipv6_answers, OP_EQ, 0);

  // Test success with cache ipv6 DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 CacheIPv6DNS");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.cache_ipv4_answers, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.cache_ipv6_answers, OP_EQ, 1);

  // Test success with no cache ipv4 DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 NoCacheIPv4DNS");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.cache_ipv4_answers, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.cache_ipv6_answers, OP_EQ, 0);

  // Test success with cache DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 CacheDNS");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, CL_PORT_TAKES_HOSTNAMES);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.cache_ipv4_answers, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.cache_ipv6_answers, OP_EQ, 1);

  // Test success with use cached ipv4 DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 UseIPv4Cache");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.use_cached_ipv4_answers, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.use_cached_ipv6_answers, OP_EQ, 0);

  // Test success with use cached ipv6 DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 UseIPv6Cache");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.use_cached_ipv4_answers, OP_EQ, 0);
  tt_int_op(port_cfg->entry_cfg.use_cached_ipv6_answers, OP_EQ, 1);

  // Test success with use cached DNS
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 UseDNSCache");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.use_cached_ipv4_answers, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.use_cached_ipv6_answers, OP_EQ, 1);

  // Test success with not preferring ipv6 automap
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 NoPreferIPv6Automap");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.prefer_ipv6_virtaddr, OP_EQ, 0);

  // Test success with prefer SOCKS no auth
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 PreferSOCKSNoAuth");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.socks_prefer_no_auth, OP_EQ, 1);

  // Test failure with both a zero port and a non-zero port
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "0");
  config_port_valid = mock_config_line("DNSPort", "42");
  config_port_invalid->next = config_port_valid;
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.42", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with warn non-local control
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, config_port_valid, NULL, "Control",
                          CONN_TYPE_CONTROL_LISTENER, "127.0.0.42", 0,
                          CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test success with warn non-local listener
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, config_port_valid, NULL, "ExtOR",
                          CONN_TYPE_EXT_OR_LISTENER, "127.0.0.42", 0,
                          CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test success with warn non-local other
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test success with warn non-local other without out
  ret = parse_port_config(NULL, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.42", 0, CL_PORT_WARN_NONLOCAL);
  tt_int_op(ret, OP_EQ, 0);

  // Test success with both ipv4 and ipv6 but without stream options
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 IPv4Traffic "
                                       "IPv6Traffic");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.44", 0,
                          CL_PORT_TAKES_HOSTNAMES |
                          CL_PORT_NO_STREAM_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.ipv4_traffic, OP_EQ, 1);
  tt_int_op(port_cfg->entry_cfg.ipv6_traffic, OP_EQ, 0);

  // Test failure for a SessionGroup argument with invalid value
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "42 SessionGroup=invalid");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.44", 0, CL_PORT_NO_STREAM_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

  // TODO: this seems wrong. Shouldn't it be the other way around?
  // Potential bug.
  // Test failure for a SessionGroup argument with valid value but with stream
  // options allowed
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "42 SessionGroup=123");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.44", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure for more than one SessionGroup argument
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "42 SessionGroup=123 "
                                         "SessionGroup=321");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.44", 0, CL_PORT_NO_STREAM_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with a sessiongroup options
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "42 SessionGroup=1111122");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.44", 0, CL_PORT_NO_STREAM_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->entry_cfg.session_group, OP_EQ, 1111122);

  // Test success with a zero unix domain socket, and doesnt add it to out
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "0");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.45", 0, CL_PORT_IS_UNIXSOCKET);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 0);

  // Test success with a one unix domain socket, and doesnt add it to out
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "something");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.45", 0, CL_PORT_IS_UNIXSOCKET);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->is_unix_addr, OP_EQ, 1);
  tt_str_op(port_cfg->unix_addr, OP_EQ, "something");

  // Test success with a port of auto - it uses the default address
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "auto");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.46", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, CFG_AUTO_PORT);
  tor_addr_parse(&addr, "127.0.0.46");
  tt_assert(tor_addr_eq(&port_cfg->addr, &addr))

  // Test success with parsing both an address and an auto port
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "127.0.0.122:auto");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.46", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, CFG_AUTO_PORT);
  tor_addr_parse(&addr, "127.0.0.122");
  tt_assert(tor_addr_eq(&port_cfg->addr, &addr))

  // Test failure when asked to parse an invalid address followed by auto
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_port_invalid = mock_config_line("DNSPort", "invalidstuff!!:auto");
  ret = parse_port_config(NULL, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.46", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with parsing both an address and a real port
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "127.0.0.123:656");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0,
                          "127.0.0.46", 0, 0);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->port, OP_EQ, 656);
  tor_addr_parse(&addr, "127.0.0.123");
  tt_assert(tor_addr_eq(&port_cfg->addr, &addr))

  // Test failure if we can't parse anything at all
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "something wrong");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.46", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure if we find both an address, a port and an auto
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "127.0.1.0:123:auto");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0,
                          "127.0.0.46", 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  // Test that default to group writeable default sets group writeable for
  // domain socket
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("SOCKSPort", "unix:/tmp/somewhere");
  ret = parse_port_config(slout, config_port_valid, NULL, "SOCKS",
                          CONN_TYPE_AP_LISTENER, "127.0.0.46", 0,
                          CL_PORT_DFLT_GROUP_WRITABLE);
#ifdef _WIN32
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->is_group_writable, OP_EQ, 1);
#endif

 done:
  if (slout)
    SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_free(slout);
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_free_lines(config_port_valid); config_port_valid = NULL;
}

static void
test_config_parse_port_config__ports__server_options(void *data)
{
  (void)data;
  int ret;
  smartlist_t *slout = NULL;
  port_cfg_t *port_cfg = NULL;
  config_line_t *config_port_invalid = NULL, *config_port_valid = NULL;

  slout = smartlist_new();

  // Test success with NoAdvertise option
  config_free_lines(config_port_valid); config_port_valid = NULL;
  config_port_valid = mock_config_line("DNSPort",
                                       "127.0.0.124:656 NoAdvertise");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0, NULL, 0,
                          CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->server_cfg.no_advertise, OP_EQ, 1);
  tt_int_op(port_cfg->server_cfg.no_listen, OP_EQ, 0);

  // Test success with NoListen option
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "127.0.0.124:656 NoListen");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0, NULL, 0,
                          CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->server_cfg.no_advertise, OP_EQ, 0);
  tt_int_op(port_cfg->server_cfg.no_listen, OP_EQ, 1);

  // Test failure with both NoAdvertise and NoListen option
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "127.0.0.124:656 NoListen "
                                         "NoAdvertise");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0, NULL,
                          0, CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with IPv4Only
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "127.0.0.124:656 IPv4Only");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0, NULL, 0,
                          CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->server_cfg.bind_ipv4_only, OP_EQ, 1);
  tt_int_op(port_cfg->server_cfg.bind_ipv6_only, OP_EQ, 0);

  // Test success with IPv6Only
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "[::1]:656 IPv6Only");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0, NULL, 0,
                          CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);
  port_cfg = (port_cfg_t *)smartlist_get(slout, 0);
  tt_int_op(port_cfg->server_cfg.bind_ipv4_only, OP_EQ, 0);
  tt_int_op(port_cfg->server_cfg.bind_ipv6_only, OP_EQ, 1);

  // Test failure with both IPv4Only and IPv6Only
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "127.0.0.124:656 IPv6Only "
                                         "IPv4Only");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0, NULL,
                          0, CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

  // Test success with invalid parameter
  config_free_lines(config_port_valid); config_port_valid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_valid = mock_config_line("DNSPort", "127.0.0.124:656 unknown");
  ret = parse_port_config(slout, config_port_valid, NULL, "DNS", 0, NULL, 0,
                          CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(smartlist_len(slout), OP_EQ, 1);

  // Test failure when asked to bind only to ipv6 but gets an ipv4 address
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort",
                                         "127.0.0.124:656 IPv6Only");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0, NULL,
                          0, CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

  // Test failure when asked to bind only to ipv4 but gets an ipv6 address
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("DNSPort", "[::1]:656 IPv4Only");
  ret = parse_port_config(slout, config_port_invalid, NULL, "DNS", 0, NULL,
                          0, CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

  // Check for failure with empty unix: address.
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_clear(slout);
  config_port_invalid = mock_config_line("ORPort", "unix:\"\"");
  ret = parse_port_config(slout, config_port_invalid, NULL, "ORPort", 0, NULL,
                          0, CL_PORT_SERVER_OPTIONS);
  tt_int_op(ret, OP_EQ, -1);

 done:
  if (slout)
    SMARTLIST_FOREACH(slout,port_cfg_t *,pf,port_cfg_free(pf));
  smartlist_free(slout);
  config_free_lines(config_port_invalid); config_port_invalid = NULL;
  config_free_lines(config_port_valid); config_port_valid = NULL;
}

#define CONFIG_TEST(name, flags)                          \
  { #name, test_config_ ## name, flags, NULL, NULL }

struct testcase_t config_tests[] = {
  CONFIG_TEST(adding_trusted_dir_server, TT_FORK),
  CONFIG_TEST(adding_fallback_dir_server, TT_FORK),
  CONFIG_TEST(parsing_trusted_dir_server, 0),
  CONFIG_TEST(parsing_fallback_dir_server, 0),
  CONFIG_TEST(adding_default_trusted_dir_servers, TT_FORK),
  CONFIG_TEST(adding_dir_servers, TT_FORK),
  CONFIG_TEST(default_dir_servers, TT_FORK),
  CONFIG_TEST(default_fallback_dirs, 0),
  CONFIG_TEST(resolve_my_address, TT_FORK),
  CONFIG_TEST(addressmap, 0),
  CONFIG_TEST(parse_bridge_line, 0),
  CONFIG_TEST(parse_transport_options_line, 0),
  CONFIG_TEST(parse_transport_plugin_line, TT_FORK),
  CONFIG_TEST(check_or_create_data_subdir, TT_FORK),
  CONFIG_TEST(write_to_data_subdir, TT_FORK),
  CONFIG_TEST(fix_my_family, 0),
  CONFIG_TEST(directory_fetch, 0),
  CONFIG_TEST(port_cfg_line_extract_addrport, 0),
  CONFIG_TEST(parse_port_config__listenaddress, 0),
  CONFIG_TEST(parse_port_config__ports__no_ports_given, 0),
  CONFIG_TEST(parse_port_config__ports__server_options, 0),
  CONFIG_TEST(parse_port_config__ports__ports_given, 0),
  END_OF_TESTCASES
};

