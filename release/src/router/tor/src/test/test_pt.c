/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define PT_PRIVATE
#define UTIL_PRIVATE
#define STATEFILE_PRIVATE
#define CONTROL_PRIVATE
#include "or.h"
#include "config.h"
#include "confparse.h"
#include "control.h"
#include "transports.h"
#include "circuitbuild.h"
#include "util.h"
#include "statefile.h"
#include "test.h"

static void
reset_mp(managed_proxy_t *mp)
{
  mp->conf_state = PT_PROTO_LAUNCHED;
  SMARTLIST_FOREACH(mp->transports, transport_t *, t, transport_free(t));
  smartlist_clear(mp->transports);
}

static void
test_pt_parsing(void)
{
  char line[200];
  transport_t *transport = NULL;
  tor_addr_t test_addr;

  managed_proxy_t *mp = tor_malloc(sizeof(managed_proxy_t));
  mp->conf_state = PT_PROTO_INFANT;
  mp->transports = smartlist_new();

  /* incomplete cmethod */
  strlcpy(line,"CMETHOD trebuchet",sizeof(line));
  test_assert(parse_cmethod_line(line, mp) < 0);

  reset_mp(mp);

  /* wrong proxy type */
  strlcpy(line,"CMETHOD trebuchet dog 127.0.0.1:1999",sizeof(line));
  test_assert(parse_cmethod_line(line, mp) < 0);

  reset_mp(mp);

  /* wrong addrport */
  strlcpy(line,"CMETHOD trebuchet socks4 abcd",sizeof(line));
  test_assert(parse_cmethod_line(line, mp) < 0);

  reset_mp(mp);

  /* correct line */
  strlcpy(line,"CMETHOD trebuchet socks5 127.0.0.1:1999",sizeof(line));
  test_assert(parse_cmethod_line(line, mp) == 0);
  test_assert(smartlist_len(mp->transports) == 1);
  transport = smartlist_get(mp->transports, 0);
  /* test registered address of transport */
  tor_addr_parse(&test_addr, "127.0.0.1");
  test_assert(tor_addr_eq(&test_addr, &transport->addr));
  /* test registered port of transport */
  test_assert(transport->port == 1999);
  /* test registered SOCKS version of transport */
  test_assert(transport->socks_version == PROXY_SOCKS5);
  /* test registered name of transport */
  test_streq(transport->name, "trebuchet");

  reset_mp(mp);

  /* incomplete smethod */
  strlcpy(line,"SMETHOD trebuchet",sizeof(line));
  test_assert(parse_smethod_line(line, mp) < 0);

  reset_mp(mp);

  /* wrong addr type */
  strlcpy(line,"SMETHOD trebuchet abcd",sizeof(line));
  test_assert(parse_smethod_line(line, mp) < 0);

  reset_mp(mp);

  /* cowwect */
  strlcpy(line,"SMETHOD trebuchy 127.0.0.2:2999",sizeof(line));
  test_assert(parse_smethod_line(line, mp) == 0);
  test_assert(smartlist_len(mp->transports) == 1);
  transport = smartlist_get(mp->transports, 0);
  /* test registered address of transport */
  tor_addr_parse(&test_addr, "127.0.0.2");
  test_assert(tor_addr_eq(&test_addr, &transport->addr));
  /* test registered port of transport */
  test_assert(transport->port == 2999);
  /* test registered name of transport */
  test_streq(transport->name, "trebuchy");

  reset_mp(mp);

  /* Include some arguments. Good ones. */
  strlcpy(line,"SMETHOD trebuchet 127.0.0.1:9999 "
          "ARGS:counterweight=3,sling=snappy",
          sizeof(line));
  test_assert(parse_smethod_line(line, mp) == 0);
  tt_int_op(1, ==, smartlist_len(mp->transports));
  {
    const transport_t *transport = smartlist_get(mp->transports, 0);
    tt_assert(transport);
    tt_str_op(transport->name, ==, "trebuchet");
    tt_int_op(transport->port, ==, 9999);
    tt_str_op(fmt_addr(&transport->addr), ==, "127.0.0.1");
    tt_str_op(transport->extra_info_args, ==,
              "counterweight=3,sling=snappy");
  }
  reset_mp(mp);

  /* unsupported version */
  strlcpy(line,"VERSION 666",sizeof(line));
  test_assert(parse_version(line, mp) < 0);

  /* incomplete VERSION */
  strlcpy(line,"VERSION ",sizeof(line));
  test_assert(parse_version(line, mp) < 0);

  /* correct VERSION */
  strlcpy(line,"VERSION 1",sizeof(line));
  test_assert(parse_version(line, mp) == 0);

 done:
  reset_mp(mp);
  smartlist_free(mp->transports);
  tor_free(mp);
}

static void
test_pt_get_transport_options(void *arg)
{
  char **execve_args;
  smartlist_t *transport_list = smartlist_new();
  managed_proxy_t *mp;
  or_options_t *options = get_options_mutable();
  char *opt_str = NULL;
  config_line_t *cl = NULL;
  (void)arg;

  execve_args = tor_malloc(sizeof(char*)*2);
  execve_args[0] = tor_strdup("cheeseshop");
  execve_args[1] = NULL;

  mp = managed_proxy_create(transport_list, execve_args, 1);
  tt_ptr_op(mp, !=, NULL);
  opt_str = get_transport_options_for_server_proxy(mp);
  tt_ptr_op(opt_str, ==, NULL);

  smartlist_add(mp->transports_to_launch, tor_strdup("gruyere"));
  smartlist_add(mp->transports_to_launch, tor_strdup("roquefort"));
  smartlist_add(mp->transports_to_launch, tor_strdup("stnectaire"));

  tt_assert(options);

  cl = tor_malloc_zero(sizeof(config_line_t));
  cl->value = tor_strdup("gruyere melty=10 hardness=se;ven");
  options->ServerTransportOptions = cl;

  cl = tor_malloc_zero(sizeof(config_line_t));
  cl->value = tor_strdup("stnectaire melty=4 hardness=three");
  cl->next = options->ServerTransportOptions;
  options->ServerTransportOptions = cl;

  cl = tor_malloc_zero(sizeof(config_line_t));
  cl->value = tor_strdup("pepperjack melty=12 hardness=five");
  cl->next = options->ServerTransportOptions;
  options->ServerTransportOptions = cl;

  opt_str = get_transport_options_for_server_proxy(mp);
  tt_str_op(opt_str, ==,
            "gruyere:melty=10;gruyere:hardness=se\\;ven;"
            "stnectaire:melty=4;stnectaire:hardness=three");

 done:
  tor_free(opt_str);
  config_free_lines(cl);
  managed_proxy_destroy(mp, 0);
  smartlist_free(transport_list);
}

static void
test_pt_protocol(void)
{
  char line[200];

  managed_proxy_t *mp = tor_malloc_zero(sizeof(managed_proxy_t));
  mp->conf_state = PT_PROTO_LAUNCHED;
  mp->transports = smartlist_new();
  mp->argv = tor_malloc_zero(sizeof(char*)*2);
  mp->argv[0] = tor_strdup("<testcase>");

  /* various wrong protocol runs: */

  strlcpy(line,"VERSION 1",sizeof(line));
  handle_proxy_line(line, mp);
  test_assert(mp->conf_state == PT_PROTO_ACCEPTING_METHODS);

  strlcpy(line,"VERSION 1",sizeof(line));
  handle_proxy_line(line, mp);
  test_assert(mp->conf_state == PT_PROTO_BROKEN);

  reset_mp(mp);

  strlcpy(line,"CMETHOD trebuchet socks5 127.0.0.1:1999",sizeof(line));
  handle_proxy_line(line, mp);
  test_assert(mp->conf_state == PT_PROTO_BROKEN);

  reset_mp(mp);

  /* correct protocol run: */
  strlcpy(line,"VERSION 1",sizeof(line));
  handle_proxy_line(line, mp);
  test_assert(mp->conf_state == PT_PROTO_ACCEPTING_METHODS);

  strlcpy(line,"CMETHOD trebuchet socks5 127.0.0.1:1999",sizeof(line));
  handle_proxy_line(line, mp);
  test_assert(mp->conf_state == PT_PROTO_ACCEPTING_METHODS);

  strlcpy(line,"CMETHODS DONE",sizeof(line));
  handle_proxy_line(line, mp);
  test_assert(mp->conf_state == PT_PROTO_CONFIGURED);

 done:
  reset_mp(mp);
  smartlist_free(mp->transports);
  tor_free(mp->argv[0]);
  tor_free(mp->argv);
  tor_free(mp);
}

static void
test_pt_get_extrainfo_string(void *arg)
{
  managed_proxy_t *mp1 = NULL, *mp2 = NULL;
  char **argv1, **argv2;
  smartlist_t *t1 = smartlist_new(), *t2 = smartlist_new();
  int r;
  char *s = NULL;
  (void) arg;

  argv1 = tor_malloc_zero(sizeof(char*)*3);
  argv1[0] = tor_strdup("ewige");
  argv1[1] = tor_strdup("Blumenkraft");
  argv1[2] = NULL;
  argv2 = tor_malloc_zero(sizeof(char*)*4);
  argv2[0] = tor_strdup("und");
  argv2[1] = tor_strdup("ewige");
  argv2[2] = tor_strdup("Schlangenkraft");
  argv2[3] = NULL;

  mp1 = managed_proxy_create(t1, argv1, 1);
  mp2 = managed_proxy_create(t2, argv2, 1);

  r = parse_smethod_line("SMETHOD hagbard 127.0.0.1:5555", mp1);
  tt_int_op(r, ==, 0);
  r = parse_smethod_line("SMETHOD celine 127.0.0.1:1723 ARGS:card=no-enemy",
                         mp2);
  tt_int_op(r, ==, 0);

  /* Force these proxies to look "completed" or they won't generate output. */
  mp1->conf_state = mp2->conf_state = PT_PROTO_COMPLETED;

  s = pt_get_extra_info_descriptor_string();
  tt_assert(s);
  tt_str_op(s, ==,
            "transport hagbard 127.0.0.1:5555\n"
            "transport celine 127.0.0.1:1723 card=no-enemy\n");

 done:
  /* XXXX clean up better */
  smartlist_free(t1);
  smartlist_free(t2);
  tor_free(s);
}

#ifdef _WIN32
#define STDIN_HANDLE HANDLE
#else
#define STDIN_HANDLE FILE
#endif

static smartlist_t *
tor_get_lines_from_handle_replacement(STDIN_HANDLE *handle,
                                      enum stream_status *stream_status_out)
{
  static int times_called = 0;
  smartlist_t *retval_sl = smartlist_new();

  (void) handle;
  (void) stream_status_out;

  /* Generate some dummy CMETHOD lines the first 5 times. The 6th
     time, send 'CMETHODS DONE' to finish configuring the proxy. */
  if (times_called++ != 5) {
    smartlist_add_asprintf(retval_sl, "SMETHOD mock%d 127.0.0.1:555%d",
                           times_called, times_called);
  } else {
    smartlist_add(retval_sl, tor_strdup("SMETHODS DONE"));
  }

  return retval_sl;
}

/* NOP mock */
static void
tor_process_handle_destroy_replacement(process_handle_t *process_handle,
                                       int also_terminate_process)
{
  (void) process_handle;
  (void) also_terminate_process;
}

static or_state_t *dummy_state = NULL;

static or_state_t *
get_or_state_replacement(void)
{
  return dummy_state;
}

static int controlevent_n = 0;
static uint16_t controlevent_event = 0;
static smartlist_t *controlevent_msgs = NULL;

static void
send_control_event_string_replacement(uint16_t event, event_format_t which,
                                      const char *msg)
{
  (void) which;
  ++controlevent_n;
  controlevent_event = event;
  if (!controlevent_msgs)
    controlevent_msgs = smartlist_new();
  smartlist_add(controlevent_msgs, tor_strdup(msg));
}

/* Test the configure_proxy() function. */
static void
test_pt_configure_proxy(void *arg)
{
  int i, retval;
  managed_proxy_t *mp = NULL;
  (void) arg;

  dummy_state = tor_malloc_zero(sizeof(or_state_t));

  MOCK(tor_get_lines_from_handle,
       tor_get_lines_from_handle_replacement);
  MOCK(tor_process_handle_destroy,
       tor_process_handle_destroy_replacement);
  MOCK(get_or_state,
       get_or_state_replacement);
  MOCK(send_control_event_string,
       send_control_event_string_replacement);

  control_testing_set_global_event_mask(EVENT_TRANSPORT_LAUNCHED);

  mp = tor_malloc(sizeof(managed_proxy_t));
  mp->conf_state = PT_PROTO_ACCEPTING_METHODS;
  mp->transports = smartlist_new();
  mp->transports_to_launch = smartlist_new();
  mp->process_handle = tor_malloc_zero(sizeof(process_handle_t));
  mp->argv = tor_malloc_zero(sizeof(char*)*2);
  mp->argv[0] = tor_strdup("<testcase>");
  mp->is_server = 1;

  /* Test the return value of configure_proxy() by calling it some
     times while it is uninitialized and then finally finalizing its
     configuration. */
  for (i = 0 ; i < 5 ; i++) {
    retval = configure_proxy(mp);
    /* retval should be zero because proxy hasn't finished configuring yet */
    test_assert(retval == 0);
    /* check the number of registered transports */
    test_assert(smartlist_len(mp->transports) == i+1);
    /* check that the mp is still waiting for transports */
    test_assert(mp->conf_state == PT_PROTO_ACCEPTING_METHODS);
  }

  /* this last configure_proxy() should finalize the proxy configuration. */
  retval = configure_proxy(mp);
  /* retval should be 1 since the proxy finished configuring */
  test_assert(retval == 1);
  /* check the mp state */
  test_assert(mp->conf_state == PT_PROTO_COMPLETED);

  tt_int_op(controlevent_n, ==, 5);
  tt_int_op(controlevent_event, ==, EVENT_TRANSPORT_LAUNCHED);
  tt_int_op(smartlist_len(controlevent_msgs), ==, 5);
  smartlist_sort_strings(controlevent_msgs);
  tt_str_op(smartlist_get(controlevent_msgs, 0), ==,
            "650 TRANSPORT_LAUNCHED server mock1 127.0.0.1 5551\r\n");
  tt_str_op(smartlist_get(controlevent_msgs, 1), ==,
            "650 TRANSPORT_LAUNCHED server mock2 127.0.0.1 5552\r\n");
  tt_str_op(smartlist_get(controlevent_msgs, 2), ==,
            "650 TRANSPORT_LAUNCHED server mock3 127.0.0.1 5553\r\n");
  tt_str_op(smartlist_get(controlevent_msgs, 3), ==,
            "650 TRANSPORT_LAUNCHED server mock4 127.0.0.1 5554\r\n");
  tt_str_op(smartlist_get(controlevent_msgs, 4), ==,
            "650 TRANSPORT_LAUNCHED server mock5 127.0.0.1 5555\r\n");

  { /* check that the transport info were saved properly in the tor state */
    config_line_t *transport_in_state = NULL;
    smartlist_t *transport_info_sl = smartlist_new();
    char *name_of_transport = NULL;
    char *bindaddr = NULL;

    /* Get the bindaddr for "mock1" and check it against the bindaddr
       that the mocked tor_get_lines_from_handle() generated. */
    transport_in_state = get_transport_in_state_by_name("mock1");
    test_assert(transport_in_state);
    smartlist_split_string(transport_info_sl, transport_in_state->value,
                           NULL, 0, 0);
    name_of_transport = smartlist_get(transport_info_sl, 0);
    bindaddr = smartlist_get(transport_info_sl, 1);
    tt_str_op(name_of_transport, ==, "mock1");
    tt_str_op(bindaddr, ==, "127.0.0.1:5551");

    SMARTLIST_FOREACH(transport_info_sl, char *, cp, tor_free(cp));
    smartlist_free(transport_info_sl);
  }

 done:
  or_state_free(dummy_state);
  UNMOCK(tor_get_lines_from_handle);
  UNMOCK(tor_process_handle_destroy);
  UNMOCK(get_or_state);
  UNMOCK(send_control_event_string);
  if (controlevent_msgs) {
    SMARTLIST_FOREACH(controlevent_msgs, char *, cp, tor_free(cp));
    smartlist_free(controlevent_msgs);
    controlevent_msgs = NULL;
  }
  if (mp->transports) {
    SMARTLIST_FOREACH(mp->transports, transport_t *, t, transport_free(t));
    smartlist_free(mp->transports);
  }
  smartlist_free(mp->transports_to_launch);
  tor_free(mp->process_handle);
  tor_free(mp->argv[0]);
  tor_free(mp->argv);
  tor_free(mp);
}

#define PT_LEGACY(name)                                               \
  { #name, legacy_test_helper, 0, &legacy_setup, test_pt_ ## name }

struct testcase_t pt_tests[] = {
  PT_LEGACY(parsing),
  PT_LEGACY(protocol),
  { "get_transport_options", test_pt_get_transport_options, TT_FORK,
    NULL, NULL },
  { "get_extrainfo_string", test_pt_get_extrainfo_string, TT_FORK,
    NULL, NULL },
  { "configure_proxy",test_pt_configure_proxy, TT_FORK,
    NULL, NULL },
  END_OF_TESTCASES
};

