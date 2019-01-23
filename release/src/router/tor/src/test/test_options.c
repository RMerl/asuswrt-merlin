/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CONFIG_PRIVATE
#include "or.h"
#include "confparse.h"
#include "config.h"
#include "test.h"
#include "geoip.h"

#define ROUTERSET_PRIVATE
#include "routerset.h"
#include "main.h"
#include "log_test_helpers.h"

#include "sandbox.h"
#include "memarea.h"
#include "policies.h"

#define NS_MODULE test_options

typedef struct {
  int severity;
  uint32_t domain;
  char *msg;
} logmsg_t;

static smartlist_t *messages = NULL;

static void
log_cback(int severity, uint32_t domain, const char *msg)
{
  logmsg_t *x = tor_malloc(sizeof(*x));
  x->severity = severity;
  x->domain = domain;
  x->msg = tor_strdup(msg);
  if (!messages)
    messages = smartlist_new();
  smartlist_add(messages, x);
}

static void
setup_log_callback(void)
{
  log_severity_list_t lst;
  memset(&lst, 0, sizeof(lst));
  lst.masks[LOG_ERR - LOG_ERR] = ~0;
  lst.masks[LOG_WARN - LOG_ERR] = ~0;
  lst.masks[LOG_NOTICE - LOG_ERR] = ~0;
  add_callback_log(&lst, log_cback);
  mark_logs_temp();
}

static char *
dump_logs(void)
{
  smartlist_t *msgs;
  char *out;
  if (! messages)
    return tor_strdup("");
  msgs = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(messages, logmsg_t *, x) {
    smartlist_add_asprintf(msgs, "[%s] %s",
                           log_level_to_string(x->severity), x->msg);
  } SMARTLIST_FOREACH_END(x);
  out = smartlist_join_strings(msgs, "", 0, NULL);
  SMARTLIST_FOREACH(msgs, char *, cp, tor_free(cp));
  smartlist_free(msgs);
  return out;
}

static void
clear_log_messages(void)
{
  if (!messages)
    return;
  SMARTLIST_FOREACH(messages, logmsg_t *, m,
                    { tor_free(m->msg); tor_free(m); });
  smartlist_free(messages);
  messages = NULL;
}

#define setup_options(opt,dflt)              \
  do {                                       \
    opt = options_new();                     \
    opt->command = CMD_RUN_TOR;              \
    options_init(opt);                       \
                                             \
    dflt = config_dup(&options_format, opt); \
    clear_log_messages();                    \
  } while (0)

#define VALID_DIR_AUTH "DirAuthority dizum orport=443 v3ident=E8A9C45"  \
  "EDE6D711294FADF8E7951F4DE6CA56B58 194.109.206.212:80 7EA6 EAD6 FD83" \
  " 083C 538F 4403 8BBF A077 587D D755\n"
#define VALID_ALT_BRIDGE_AUTH \
  "AlternateBridgeAuthority dizum orport=443 v3ident=E8A9C45"           \
  "EDE6D711294FADF8E7951F4DE6CA56B58 194.109.206.212:80 7EA6 EAD6 FD83" \
  " 083C 538F 4403 8BBF A077 587D D755\n"
#define VALID_ALT_DIR_AUTH \
  "AlternateDirAuthority dizum orport=443 v3ident=E8A9C45"           \
  "EDE6D711294FADF8E7951F4DE6CA56B58 194.109.206.212:80 7EA6 EAD6 FD83" \
  " 083C 538F 4403 8BBF A077 587D D755\n"

static void
test_options_validate_impl(const char *configuration,
                           const char *expect_errmsg,
                           int expect_log_severity,
                           const char *expect_log)
{
  or_options_t *opt=NULL;
  or_options_t *dflt;
  config_line_t *cl=NULL;
  char *msg=NULL;
  int r;

  setup_options(opt, dflt);

  r = config_get_lines(configuration, &cl, 1);
  tt_int_op(r, OP_EQ, 0);

  r = config_assign(&options_format, opt, cl, 0, &msg);
  tt_int_op(r, OP_EQ, 0);

  r = options_validate(NULL, opt, dflt, 0, &msg);
  if (expect_errmsg && !msg) {
    TT_DIE(("Expected error message <%s> from <%s>, but got none.",
            expect_errmsg, configuration));
  } else if (expect_errmsg && !strstr(msg, expect_errmsg)) {
    TT_DIE(("Expected error message <%s> from <%s>, but got <%s>.",
            expect_errmsg, configuration, msg));
  } else if (!expect_errmsg && msg) {
    TT_DIE(("Expected no error message from <%s> but got <%s>.",
            configuration, msg));
  }
  tt_int_op((r == 0), OP_EQ, (msg == NULL));

  if (expect_log) {
    int found = 0;
    if (messages) {
      SMARTLIST_FOREACH_BEGIN(messages, logmsg_t *, m) {
        if (m->severity == expect_log_severity &&
            strstr(m->msg, expect_log)) {
          found = 1;
          break;
        }
      } SMARTLIST_FOREACH_END(m);
    }
    if (!found) {
      tor_free(msg);
      msg = dump_logs();
      TT_DIE(("Expected log message [%s] %s from <%s>, but got <%s>.",
              log_level_to_string(expect_log_severity), expect_log,
              configuration, msg));
    }
  }

 done:
  escaped(NULL);
  policies_free_all();
  config_free_lines(cl);
  or_options_free(opt);
  or_options_free(dflt);
  tor_free(msg);
  clear_log_messages();
}

#define WANT_ERR(config, msg)                           \
  test_options_validate_impl((config), (msg), 0, NULL)
#define WANT_LOG(config, severity, msg)                         \
  test_options_validate_impl((config), NULL, (severity), (msg))
#define WANT_ERR_LOG(config, msg, severity, logmsg)                     \
  test_options_validate_impl((config), (msg), (severity), (logmsg))
#define OK(config)                                      \
  test_options_validate_impl((config), NULL, 0, NULL)

static void
test_options_validate(void *arg)
{
  (void)arg;
  setup_log_callback();
  sandbox_disable_getaddrinfo_cache();

  WANT_ERR("ExtORPort 500000", "Invalid ExtORPort");

  WANT_ERR_LOG("ServerTransportOptions trebuchet",
               "ServerTransportOptions did not parse",
               LOG_WARN, "Too few arguments");
  OK("ServerTransportOptions trebuchet sling=snappy");
  OK("ServerTransportOptions trebuchet sling=");
  WANT_ERR_LOG("ServerTransportOptions trebuchet slingsnappy",
               "ServerTransportOptions did not parse",
               LOG_WARN, "\"slingsnappy\" is not a k=v");

  WANT_ERR("DirPort 8080\nDirCache 0",
           "DirPort configured but DirCache disabled.");
  WANT_ERR("BridgeRelay 1\nDirCache 0",
           "We're a bridge but DirCache is disabled.");

  close_temp_logs();
  clear_log_messages();
  return;
}

#define MEGABYTEIFY(mb) (U64_LITERAL(mb) << 20)
static void
test_have_enough_mem_for_dircache(void *arg)
{
  (void)arg;
  or_options_t *opt=NULL;
  or_options_t *dflt=NULL;
  config_line_t *cl=NULL;
  char *msg=NULL;;
  int r;
  const char *configuration = "ORPort 8080\nDirCache 1", *expect_errmsg;

  setup_options(opt, dflt);
  setup_log_callback();
  (void)dflt;

  r = config_get_lines(configuration, &cl, 1);
  tt_int_op(r, OP_EQ, 0);

  r = config_assign(&options_format, opt, cl, 0, &msg);
  tt_int_op(r, OP_EQ, 0);

  /* 300 MB RAM available, DirCache enabled */
  r = have_enough_mem_for_dircache(opt, MEGABYTEIFY(300), &msg);
  tt_int_op(r, OP_EQ, 0);
  tt_assert(!msg);

  /* 200 MB RAM available, DirCache enabled */
  r = have_enough_mem_for_dircache(opt, MEGABYTEIFY(200), &msg);
  tt_int_op(r, OP_EQ, -1);
  expect_errmsg = "Being a directory cache (default) with less than ";
  if (!strstr(msg, expect_errmsg)) {
    TT_DIE(("Expected error message <%s> from <%s>, but got <%s>.",
            expect_errmsg, configuration, msg));
  }
  tor_free(msg);

  config_free_lines(cl); cl = NULL;
  configuration = "ORPort 8080\nDirCache 1\nBridgeRelay 1";
  r = config_get_lines(configuration, &cl, 1);
  tt_int_op(r, OP_EQ, 0);

  r = config_assign(&options_format, opt, cl, 0, &msg);
  tt_int_op(r, OP_EQ, 0);

  /* 300 MB RAM available, DirCache enabled, Bridge */
  r = have_enough_mem_for_dircache(opt, MEGABYTEIFY(300), &msg);
  tt_int_op(r, OP_EQ, 0);
  tt_assert(!msg);

  /* 200 MB RAM available, DirCache enabled, Bridge */
  r = have_enough_mem_for_dircache(opt, MEGABYTEIFY(200), &msg);
  tt_int_op(r, OP_EQ, -1);
  expect_errmsg = "Running a Bridge with less than ";
  if (!strstr(msg, expect_errmsg)) {
    TT_DIE(("Expected error message <%s> from <%s>, but got <%s>.",
            expect_errmsg, configuration, msg));
  }
  tor_free(msg);

  config_free_lines(cl); cl = NULL;
  configuration = "ORPort 8080\nDirCache 0";
  r = config_get_lines(configuration, &cl, 1);
  tt_int_op(r, OP_EQ, 0);

  r = config_assign(&options_format, opt, cl, 0, &msg);
  tt_int_op(r, OP_EQ, 0);

  /* 200 MB RAM available, DirCache disabled */
  r = have_enough_mem_for_dircache(opt, MEGABYTEIFY(200), &msg);
  tt_int_op(r, OP_EQ, 0);
  tt_assert(!msg);

  /* 300 MB RAM available, DirCache disabled */
  r = have_enough_mem_for_dircache(opt, MEGABYTEIFY(300), &msg);
  tt_int_op(r, OP_EQ, -1);
  expect_errmsg = "DirCache is disabled and we are configured as a ";
  if (!strstr(msg, expect_errmsg)) {
    TT_DIE(("Expected error message <%s> from <%s>, but got <%s>.",
            expect_errmsg, configuration, msg));
  }
  tor_free(msg);

  clear_log_messages();

 done:
  if (msg)
    tor_free(msg);
  or_options_free(dflt);
  or_options_free(opt);
  config_free_lines(cl);
  return;
}

static const char *fixed_get_uname_result = NULL;

static const char *
fixed_get_uname(void)
{
  return fixed_get_uname_result;
}

#define TEST_OPTIONS_OLD_VALUES   "TestingV3AuthInitialVotingInterval 1800\n" \
  "ClientBootstrapConsensusMaxDownloadTries 7\n" \
  "ClientBootstrapConsensusAuthorityOnlyMaxDownloadTries 4\n" \
  "ClientBootstrapConsensusMaxInProgressTries 3\n" \
  "TestingV3AuthInitialVoteDelay 300\n"   \
  "TestingV3AuthInitialDistDelay 300\n" \
  "TestingClientMaxIntervalWithoutRequest 600\n" \
  "TestingDirConnectionMaxStall 600\n" \
  "TestingConsensusMaxDownloadTries 8\n" \
  "TestingDescriptorMaxDownloadTries 8\n" \
  "TestingMicrodescMaxDownloadTries 8\n" \
  "TestingCertMaxDownloadTries 8\n"

#define TEST_OPTIONS_DEFAULT_VALUES TEST_OPTIONS_OLD_VALUES \
  "MaxClientCircuitsPending 1\n"                                        \
  "RendPostPeriod 1000\n"                                               \
  "KeepAlivePeriod 1\n"                                                 \
  "ConnLimit 1\n"                                                       \
  "V3AuthVotingInterval 300\n"                                          \
  "V3AuthVoteDelay 20\n"                                                \
  "V3AuthDistDelay 20\n"                                                \
  "V3AuthNIntervalsValid 3\n"                                           \
  "ClientUseIPv4 1\n"                                                     \
  "VirtualAddrNetworkIPv4 127.192.0.0/10\n"                             \
  "VirtualAddrNetworkIPv6 [FE80::]/10\n"                                \
  "SchedulerHighWaterMark__ 42\n"                                       \
  "SchedulerLowWaterMark__ 10\n"

typedef struct {
  or_options_t *old_opt;
  or_options_t *opt;
  or_options_t *def_opt;
} options_test_data_t;

static void free_options_test_data(options_test_data_t *td);

static options_test_data_t *
get_options_test_data(const char *conf)
{
  int rv = -1;
  char *msg = NULL;
  config_line_t *cl=NULL;
  options_test_data_t *result = tor_malloc(sizeof(options_test_data_t));
  result->opt = options_new();
  result->old_opt = options_new();
  result->def_opt = options_new();
  rv = config_get_lines(conf, &cl, 1);
  tt_assert(rv == 0);
  rv = config_assign(&options_format, result->opt, cl, 0, &msg);
  if (msg) {
    /* Display the parse error message by comparing it with an empty string */
    tt_str_op(msg, OP_EQ, "");
  }
  tt_assert(rv == 0);
  config_free_lines(cl);
  result->opt->LogTimeGranularity = 1;
  result->opt->TokenBucketRefillInterval = 1;
  rv = config_get_lines(TEST_OPTIONS_OLD_VALUES, &cl, 1);
  tt_assert(rv == 0);
  rv = config_assign(&options_format, result->def_opt, cl, 0, &msg);
  if (msg) {
    /* Display the parse error message by comparing it with an empty string */
    tt_str_op(msg, OP_EQ, "");
  }
  tt_assert(rv == 0);

 done:
  config_free_lines(cl);
  if (rv != 0) {
    free_options_test_data(result);
    result = NULL;
    /* Callers expect a non-NULL result, so just die if we can't provide one.
     */
    tor_assert(0);
  }
  return result;
}

static void
free_options_test_data(options_test_data_t *td)
{
  if (!td) return;
  or_options_free(td->old_opt);
  or_options_free(td->opt);
  or_options_free(td->def_opt);
  tor_free(td);
}

static void
test_options_validate__uname_for_server(void *ignored)
{
  (void)ignored;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                      "ORListenAddress 127.0.0.1:5555");
  setup_capture_of_logs(LOG_WARN);

  MOCK(get_uname, fixed_get_uname);
  fixed_get_uname_result = "Windows 95";
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("Tor is running as a server, but you"
           " are running Windows 95; this probably won't work. See https://www"
           ".torproject.org/docs/faq.html#BestOSForRelay for details.\n");
  tor_free(msg);

  fixed_get_uname_result = "Windows 98";
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("Tor is running as a server, but you"
           " are running Windows 98; this probably won't work. See https://www"
           ".torproject.org/docs/faq.html#BestOSForRelay for details.\n");
  tor_free(msg);

  fixed_get_uname_result = "Windows Me";
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("Tor is running as a server, but you"
           " are running Windows Me; this probably won't work. See https://www"
           ".torproject.org/docs/faq.html#BestOSForRelay for details.\n");
  tor_free(msg);

  fixed_get_uname_result = "Windows 2000";
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_entry();
  tor_free(msg);

 done:
  UNMOCK(get_uname);
  free_options_test_data(tdata);
  tor_free(msg);
  teardown_capture_of_logs();
}

static void
test_options_validate__outbound_addresses(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                    "OutboundBindAddress xxyy!!!sdfaf");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__data_directory(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                                "DataDirectory longreallyl"
                                                "ongLONGLONGlongreallylong"
                                                "LONGLONGlongreallylongLON"
                                                "GLONGlongreallylongLONGLO"
                                                "NGlongreallylongLONGLONGl"
                                                "ongreallylongLONGLONGlong"
                                                "reallylongLONGLONGlongrea"
                                                "llylongLONGLONGlongreally"
                                                "longLONGLONGlongreallylon"
                                                "gLONGLONGlongreallylongLO"
                                                "NGLONGlongreallylongLONGL"
                                                "ONGlongreallylongLONGLONG"
                                                "longreallylongLONGLONGlon"
                                                "greallylongLONGLONGlongre"
                                                "allylongLONGLONGlongreall"
                                                "ylongLONGLONGlongreallylo"
                                                "ngLONGLONGlongreallylongL"
                                                "ONGLONGlongreallylongLONG"
                                                "LONG"); // 440 characters

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Invalid DataDirectory");

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__nickname(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                        "Nickname ThisNickNameIsABitTooLong");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Nickname 'ThisNickNameIsABitTooLong', nicknames must be between "
            "1 and 19 characters inclusive, and must contain only the "
            "characters [a-zA-Z0-9].");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("Nickname AMoreValidNick");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(!msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("DataDirectory /tmp/somewhere");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(!msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__contactinfo(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                "ORListenAddress 127.0.0.1:5555\nORPort 955");
  setup_capture_of_logs(LOG_DEBUG);
  tdata->opt->ContactInfo = NULL;

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg(
            "Your ContactInfo config option is not"
            " set. Please consider setting it, so we can contact you if your"
            " server is misconfigured or something else goes wrong.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ORListenAddress 127.0.0.1:5555\nORPort 955\n"
                                "ContactInfo hella@example.org");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_no_log_msg(
            "Your ContactInfo config option is not"
            " set. Please consider setting it, so we can contact you if your"
            " server is misconfigured or something else goes wrong.\n");
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__logs(void *ignored)
{
  (void)ignored;
  int ret;
  (void)ret;
  char *msg;
  int orig_quiet_level = quiet_level;
  options_test_data_t *tdata = get_options_test_data("");
  tdata->opt->Logs = NULL;
  tdata->opt->RunAsDaemon = 0;

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(tdata->opt->Logs->key, OP_EQ, "Log");
  tt_str_op(tdata->opt->Logs->value, OP_EQ, "notice stdout");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("");
  tdata->opt->Logs = NULL;
  tdata->opt->RunAsDaemon = 0;
  quiet_level = 1;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(tdata->opt->Logs->key, OP_EQ, "Log");
  tt_str_op(tdata->opt->Logs->value, OP_EQ, "warn stdout");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("");
  tdata->opt->Logs = NULL;
  tdata->opt->RunAsDaemon = 0;
  quiet_level = 2;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_assert(!tdata->opt->Logs);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("");
  tdata->opt->Logs = NULL;
  tdata->opt->RunAsDaemon = 0;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 1, &msg);
  tt_assert(!tdata->opt->Logs);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("");
  tdata->opt->Logs = NULL;
  tdata->opt->RunAsDaemon = 1;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_assert(!tdata->opt->Logs);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("");
  tdata->opt->RunAsDaemon = 0;
  config_line_t *cl=NULL;
  config_get_lines("Log foo", &cl, 1);
  tdata->opt->Logs = cl;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op((intptr_t)tdata->opt->Logs, OP_EQ, (intptr_t)cl);

 done:
  quiet_level = orig_quiet_level;
  free_options_test_data(tdata);
  tor_free(msg);
}

/* static config_line_t * */
/* mock_config_line(const char *key, const char *val) */
/* { */
/*   config_line_t *config_line = tor_malloc(sizeof(config_line_t)); */
/*   memset(config_line, 0, sizeof(config_line_t)); */
/*   config_line->key = tor_strdup(key); */
/*   config_line->value = tor_strdup(val); */
/*   return config_line; */
/* } */

static void
test_options_validate__authdir(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_INFO);
  options_test_data_t *tdata = get_options_test_data(
                                 "AuthoritativeDirectory 1\n"
                                 "Address this.should.not_exist.example.org");

  sandbox_disable_getaddrinfo_cache();

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Failed to resolve/guess local address. See logs for"
            " details.");
  expect_log_msg("Could not resolve local Address "
            "'this.should.not_exist.example.org'. Failing.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(!msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Authoritative directory servers must set ContactInfo");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "TestingTorNetwork 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "AuthoritativeDir is set, but none of (Bridge/V3)"
            "AuthoritativeDir is set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "AuthoritativeDir is set, but none of (Bridge/V3)"
            "AuthoritativeDir is set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "RecommendedVersions 1.2, 3.14\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(tdata->opt->RecommendedClientVersions->value, OP_EQ, "1.2, 3.14");
  tt_str_op(tdata->opt->RecommendedServerVersions->value, OP_EQ, "1.2, 3.14");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "RecommendedVersions 1.2, 3.14\n"
                                "RecommendedClientVersions 25\n"
                                "RecommendedServerVersions 4.18\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(tdata->opt->RecommendedClientVersions->value, OP_EQ, "25");
  tt_str_op(tdata->opt->RecommendedServerVersions->value, OP_EQ, "4.18");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "VersioningAuthoritativeDirectory 1\n"
                                "RecommendedVersions 1.2, 3.14\n"
                                "RecommendedClientVersions 25\n"
                                "RecommendedServerVersions 4.18\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ, "AuthoritativeDir is set, but none of (Bridge/V3)"
            "AuthoritativeDir is set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "VersioningAuthoritativeDirectory 1\n"
                                "RecommendedServerVersions 4.18\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ, "Versioning authoritative dir servers must set "
            "Recommended*Versions.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "VersioningAuthoritativeDirectory 1\n"
                                "RecommendedClientVersions 4.18\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ, "Versioning authoritative dir servers must set "
            "Recommended*Versions.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "UseEntryGuards 1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("Authoritative directory servers "
            "can't set UseEntryGuards. Disabling.\n");
  tt_int_op(tdata->opt->UseEntryGuards, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "V3AuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("Authoritative directories always try"
            " to download extra-info documents. Setting DownloadExtraInfo.\n");
  tt_int_op(tdata->opt->DownloadExtraInfo, OP_EQ, 1);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "DownloadExtraInfo 1\n"
                                "V3AuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_no_log_msg("Authoritative directories always try"
            " to download extra-info documents. Setting DownloadExtraInfo.\n");
  tt_int_op(tdata->opt->DownloadExtraInfo, OP_EQ, 1);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ, "AuthoritativeDir is set, but none of (Bridge/V3)"
            "AuthoritativeDir is set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "BridgeAuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "V3BandwidthsFile non-existant-file\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ,
            "Running as authoritative directory, but no DirPort set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "BridgeAuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "V3BandwidthsFile non-existant-file\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(NULL, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ,
            "Running as authoritative directory, but no DirPort set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "BridgeAuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "GuardfractionFile non-existant-file\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ,
            "Running as authoritative directory, but no DirPort set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "BridgeAuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "GuardfractionFile non-existant-file\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  options_validate(NULL, tdata->opt, tdata->def_opt, 0, &msg);
  tt_str_op(msg, OP_EQ,
            "Running as authoritative directory, but no DirPort set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "BridgeAuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Running as authoritative directory, but no DirPort set.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AuthoritativeDirectory 1\n"
                                "Address 100.200.10.1\n"
                                "DirPort 999\n"
                                "BridgeAuthoritativeDir 1\n"
                                "ContactInfo hello@hello.com\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Running as authoritative directory, but no ORPort set.");
  tor_free(msg);

  // TODO: This case can't be reached, since clientonly is used to
  // check when parsing port lines as well.
  /* free_options_test_data(tdata); */
  /* tdata = get_options_test_data("AuthoritativeDirectory 1\n" */
  /*                               "Address 100.200.10.1\n" */
  /*                               "DirPort 999\n" */
  /*                               "ORPort 888\n" */
  /*                               "ClientOnly 1\n" */
  /*                               "BridgeAuthoritativeDir 1\n" */
  /*                               "ContactInfo hello@hello.com\n" */
  /*                               "SchedulerHighWaterMark__ 42\n" */
  /*                               "SchedulerLowWaterMark__ 10\n"); */
  /* mock_clean_saved_logs(); */
  /* ret = options_validate(tdata->old_opt, tdata->opt, */
  /*                        tdata->def_opt, 0, &msg); */
  /* tt_int_op(ret, OP_EQ, -1); */
  /* tt_str_op(msg, OP_EQ, "Running as authoritative directory, " */
  /*           "but ClientOnly also set."); */

 done:
  teardown_capture_of_logs();
  //  sandbox_free_getaddrinfo_cache();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__relay_with_hidden_services(void *ignored)
{
  (void)ignored;
  char *msg;
  setup_capture_of_logs(LOG_DEBUG);
  options_test_data_t *tdata = get_options_test_data(
                                  "ORListenAddress 127.0.0.1:5555\n"
                                  "ORPort 955\n"
                                  "HiddenServiceDir "
                                  "/Library/Tor/var/lib/tor/hidden_service/\n"
                                  "HiddenServicePort 80 127.0.0.1:8080\n"
                                                     );

  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg(
            "Tor is currently configured as a relay and a hidden service. "
            "That's not very secure: you should probably run your hidden servi"
            "ce in a separate Tor process, at least -- see "
            "https://trac.torproject.org/8742\n");

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

// TODO: it doesn't seem possible to hit the case of having no port lines at
// all, since there will be a default created for SocksPort
/* static void */
/* test_options_validate__ports(void *ignored) */
/* { */
/*   (void)ignored; */
/*   int ret; */
/*   char *msg; */
/*   setup_capture_of_logs(LOG_WARN); */
/*   options_test_data_t *tdata = get_options_test_data(""); */
/*   ret = options_validate(tdata->old_opt, tdata->opt, */
/*                          tdata->def_opt, 0, &msg); */
/*   expect_log_msg("SocksPort, TransPort, NATDPort, DNSPort, and ORPort " */
/*           "are all undefined, and there aren't any hidden services " */
/*           "configured. " */
/*           " Tor will still run, but probably won't do anything.\n"); */
/*  done: */
/*   teardown_capture_of_logs(); */
/*   free_options_test_data(tdata); */
/*   tor_free(msg); */
/* } */

static void
test_options_validate__transproxy(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata;

#ifdef USE_TRANSPARENT
  // Test default trans proxy
  tdata = get_options_test_data("TransProxyType default\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->TransProxyType_parsed, OP_EQ, TPT_DEFAULT);
  tor_free(msg);

  // Test pf-divert trans proxy
  free_options_test_data(tdata);
  tdata = get_options_test_data("TransProxyType pf-divert\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);

#if !defined(__OpenBSD__) && !defined( DARWIN )
  tt_str_op(msg, OP_EQ,
          "pf-divert is a OpenBSD-specific and OS X/Darwin-specific feature.");
#else
  tt_int_op(tdata->opt->TransProxyType_parsed, OP_EQ, TPT_PF_DIVERT);
  tt_str_op(msg, OP_EQ, "Cannot use TransProxyType without "
            "any valid TransPort or TransListenAddress.");
#endif
  tor_free(msg);

  // Test tproxy trans proxy
  free_options_test_data(tdata);
  tdata = get_options_test_data("TransProxyType tproxy\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);

#if !defined(__linux__)
  tt_str_op(msg, OP_EQ, "TPROXY is a Linux-specific feature.");
#else
  tt_int_op(tdata->opt->TransProxyType_parsed, OP_EQ, TPT_TPROXY);
  tt_str_op(msg, OP_EQ, "Cannot use TransProxyType without any valid "
            "TransPort or TransListenAddress.");
#endif
  tor_free(msg);

  // Test ipfw trans proxy
  free_options_test_data(tdata);
  tdata = get_options_test_data("TransProxyType ipfw\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);

#ifndef KERNEL_MAY_SUPPORT_IPFW
  tt_str_op(msg, OP_EQ, "ipfw is a FreeBSD-specific and OS X/Darwin-specific "
            "feature.");
#else
  tt_int_op(tdata->opt->TransProxyType_parsed, OP_EQ, TPT_IPFW);
  tt_str_op(msg, OP_EQ, "Cannot use TransProxyType without any valid "
            "TransPort or TransListenAddress.");
#endif
  tor_free(msg);

  // Test unknown trans proxy
  free_options_test_data(tdata);
  tdata = get_options_test_data("TransProxyType non-existant\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Unrecognized value for TransProxyType");
  tor_free(msg);

  // Test trans proxy success
  free_options_test_data(tdata);
  tdata = NULL;

#if defined(__linux__)
  tdata = get_options_test_data("TransProxyType tproxy\n"
                                "TransPort 127.0.0.1:123\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  if (msg) {
    TT_DIE(("Expected NULL but got '%s'", msg));
  }
#elif defined(KERNEL_MAY_SUPPORT_IPFW)
  tdata = get_options_test_data("TransProxyType ipfw\n"
                                "TransPort 127.0.0.1:123\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  if (msg) {
    TT_DIE(("Expected NULL but got '%s'", msg));
  }
#elif defined(__OpenBSD__)
  tdata = get_options_test_data("TransProxyType pf-divert\n"
                                "TransPort 127.0.0.1:123\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  if (msg) {
    TT_DIE(("Expected NULL but got '%s'", msg));
  }
#elif defined(__NetBSD__)
  tdata = get_options_test_data("TransProxyType default\n"
                                "TransPort 127.0.0.1:123\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  if (msg) {
    TT_DIE(("Expected NULL but got '%s'", msg));
  }
#endif

  // Assert that a test has run for some TransProxyType
  tt_assert(tdata);

#else
  tdata = get_options_test_data("TransPort 127.0.0.1:555\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TransPort and TransListenAddress are disabled in "
            "this build.");
  tor_free(msg);
#endif

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

NS_DECL(country_t, geoip_get_country, (const char *country));

static country_t
NS(geoip_get_country)(const char *countrycode)
{
  (void)countrycode;
  CALLED(geoip_get_country)++;

  return 1;
}

static void
test_options_validate__exclude_nodes(void *ignored)
{
  (void)ignored;

  NS_MOCK(geoip_get_country);

  int ret;
  char *msg;
  setup_capture_of_logs(LOG_WARN);
  options_test_data_t *tdata = get_options_test_data(
                                                  "ExcludeExitNodes {us}\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(smartlist_len(tdata->opt->ExcludeExitNodesUnion_->list), OP_EQ, 1);
  tt_str_op((char *)
            (smartlist_get(tdata->opt->ExcludeExitNodesUnion_->list, 0)),
            OP_EQ, "{us}");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ExcludeNodes {cn}\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(smartlist_len(tdata->opt->ExcludeExitNodesUnion_->list), OP_EQ, 1);
  tt_str_op((char *)
            (smartlist_get(tdata->opt->ExcludeExitNodesUnion_->list, 0)),
            OP_EQ, "{cn}");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ExcludeNodes {cn}\n"
                                "ExcludeExitNodes {us} {cn}\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(smartlist_len(tdata->opt->ExcludeExitNodesUnion_->list), OP_EQ, 2);
  tt_str_op((char *)
            (smartlist_get(tdata->opt->ExcludeExitNodesUnion_->list, 0)),
            OP_EQ, "{us} {cn}");
  tt_str_op((char *)
            (smartlist_get(tdata->opt->ExcludeExitNodesUnion_->list, 1)),
            OP_EQ, "{cn}");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ExcludeNodes {cn}\n"
                                "StrictNodes 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg(
            "You have asked to exclude certain relays from all positions "
            "in your circuits. Expect hidden services and other Tor "
            "features to be broken in unpredictable ways.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ExcludeNodes {cn}\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_no_log_msg(
            "You have asked to exclude certain relays from all positions "
            "in your circuits. Expect hidden services and other Tor "
            "features to be broken in unpredictable ways.\n");
  tor_free(msg);

 done:
  NS_UNMOCK(geoip_get_country);
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__scheduler(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_DEBUG);
  options_test_data_t *tdata = get_options_test_data(
                                            "SchedulerLowWaterMark__ 0\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg("Bad SchedulerLowWaterMark__ option\n");
  tor_free(msg);

  // TODO: this test cannot run on platforms where UINT32_MAX == UINT64_MAX.
  // I suspect it's unlikely this branch can actually happen
  /* free_options_test_data(tdata); */
  /* tdata = get_options_test_data( */
  /*                      "SchedulerLowWaterMark 10000000000000000000\n"); */
  /* tdata->opt->SchedulerLowWaterMark__ = (uint64_t)UINT32_MAX; */
  /* tdata->opt->SchedulerLowWaterMark__++; */
  /* mock_clean_saved_logs(); */
  /* ret = options_validate(tdata->old_opt, tdata->opt, */
  /*                        tdata->def_opt, 0, &msg); */
  /* tt_int_op(ret, OP_EQ, -1); */
  /* expect_log_msg("Bad SchedulerLowWaterMark__ option\n"); */

  free_options_test_data(tdata);
  tdata = get_options_test_data("SchedulerLowWaterMark__ 42\n"
                                "SchedulerHighWaterMark__ 42\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg("Bad SchedulerHighWaterMark option\n");
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__node_families(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                     "NodeFamily flux, flax\n"
                                     "NodeFamily somewhere\n"
                                     "SchedulerHighWaterMark__ 42\n"
                                     "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(tdata->opt->NodeFamilySets);
  tt_int_op(smartlist_len(tdata->opt->NodeFamilySets), OP_EQ, 2);
  tt_str_op((char *)(smartlist_get(
    ((routerset_t *)smartlist_get(tdata->opt->NodeFamilySets, 0))->list, 0)),
            OP_EQ, "flux");
  tt_str_op((char *)(smartlist_get(
    ((routerset_t *)smartlist_get(tdata->opt->NodeFamilySets, 0))->list, 1)),
            OP_EQ, "flax");
  tt_str_op((char *)(smartlist_get(
    ((routerset_t *)smartlist_get(tdata->opt->NodeFamilySets, 1))->list, 0)),
            OP_EQ, "somewhere");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(!tdata->opt->NodeFamilySets);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("NodeFamily !flux\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(tdata->opt->NodeFamilySets);
  tt_int_op(smartlist_len(tdata->opt->NodeFamilySets), OP_EQ, 0);
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__tlsec(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_DEBUG);
  options_test_data_t *tdata = get_options_test_data(
                                 "TLSECGroup ed25519\n"
                                 "SchedulerHighWaterMark__ 42\n"
                                 "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg("Unrecognized TLSECGroup: Falling back to the default.\n");
  tt_assert(!tdata->opt->TLSECGroup);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("TLSECGroup P224\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_no_log_msg(
            "Unrecognized TLSECGroup: Falling back to the default.\n");
  tt_assert(tdata->opt->TLSECGroup);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("TLSECGroup P256\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_no_log_msg(
            "Unrecognized TLSECGroup: Falling back to the default.\n");
  tt_assert(tdata->opt->TLSECGroup);
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__token_bucket(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data("");

  tdata->opt->TokenBucketRefillInterval = 0;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "TokenBucketRefillInterval must be between 1 and 1000 inclusive.");
  tor_free(msg);

  tdata->opt->TokenBucketRefillInterval = 1001;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "TokenBucketRefillInterval must be between 1 and 1000 inclusive.");
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__recommended_packages(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_WARN);
  options_test_data_t *tdata = get_options_test_data(
            "RecommendedPackages foo 1.2 http://foo.com sha1=123123123123\n"
            "RecommendedPackages invalid-package-line\n"
            "SchedulerHighWaterMark__ 42\n"
            "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_no_log_msg("Invalid RecommendedPackage line "
            "invalid-package-line will be ignored\n");

 done:
  escaped(NULL); // This will free the leaking memory from the previous escaped
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__fetch_dir(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                            "FetchDirInfoExtraEarly 1\n"
                                            "FetchDirInfoEarly 0\n"
                                            "SchedulerHighWaterMark__ 42\n"
                                            "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "FetchDirInfoExtraEarly requires that you"
            " also set FetchDirInfoEarly");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("FetchDirInfoExtraEarly 1\n"
                                "FetchDirInfoEarly 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_NE, "FetchDirInfoExtraEarly requires that you"
            " also set FetchDirInfoEarly");
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__conn_limit(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                            "ConnLimit 0\n"
                                            "SchedulerHighWaterMark__ 42\n"
                                            "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "ConnLimit must be greater than 0, but was set to 0");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "MaxClientCircuitsPending must be between 1 and 1024, "
            "but was set to 0");
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__paths_needed(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_WARN);
  options_test_data_t *tdata = get_options_test_data(
                                      "PathsNeededToBuildCircuits 0.1\n"
                                      "ConnLimit 1\n"
                                      "SchedulerHighWaterMark__ 42\n"
                                      "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(tdata->opt->PathsNeededToBuildCircuits > 0.24 &&
            tdata->opt->PathsNeededToBuildCircuits < 0.26);
  expect_log_msg("PathsNeededToBuildCircuits is too low. "
                 "Increasing to 0.25\n");
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data("PathsNeededToBuildCircuits 0.99\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(tdata->opt->PathsNeededToBuildCircuits > 0.94 &&
            tdata->opt->PathsNeededToBuildCircuits < 0.96);
  expect_log_msg("PathsNeededToBuildCircuits is "
            "too high. Decreasing to 0.95\n");
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data("PathsNeededToBuildCircuits 0.91\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(tdata->opt->PathsNeededToBuildCircuits > 0.90 &&
            tdata->opt->PathsNeededToBuildCircuits < 0.92);
  expect_no_log_entry();
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__max_client_circuits(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                           "MaxClientCircuitsPending 0\n"
                                           "ConnLimit 1\n"
                                           "SchedulerHighWaterMark__ 42\n"
                                           "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "MaxClientCircuitsPending must be between 1 and 1024,"
            " but was set to 0");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("MaxClientCircuitsPending 1025\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "MaxClientCircuitsPending must be between 1 and 1024,"
            " but was set to 1025");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "KeepalivePeriod option must be positive.");
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__ports(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                      "FirewallPorts 65537\n"
                                      "MaxClientCircuitsPending 1\n"
                                      "ConnLimit 1\n"
                                      "SchedulerHighWaterMark__ 42\n"
                                      "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Port '65537' out of range in FirewallPorts");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("FirewallPorts 1\n"
                                "LongLivedPorts 124444\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Port '124444' out of range in LongLivedPorts");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("FirewallPorts 1\n"
                                "LongLivedPorts 2\n"
                                "RejectPlaintextPorts 112233\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Port '112233' out of range in RejectPlaintextPorts");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("FirewallPorts 1\n"
                                "LongLivedPorts 2\n"
                                "RejectPlaintextPorts 3\n"
                                "WarnPlaintextPorts 65536\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Port '65536' out of range in WarnPlaintextPorts");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("FirewallPorts 1\n"
                                "LongLivedPorts 2\n"
                                "RejectPlaintextPorts 3\n"
                                "WarnPlaintextPorts 4\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "KeepalivePeriod option must be positive.");
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__reachable_addresses(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_NOTICE);
  options_test_data_t *tdata = get_options_test_data(
                                     "FascistFirewall 1\n"
                                     "MaxClientCircuitsPending 1\n"
                                     "ConnLimit 1\n"
                                     "SchedulerHighWaterMark__ 42\n"
                                     "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg("Converting FascistFirewall config "
            "option to new format: \"ReachableDirAddresses *:80\"\n");
  tt_str_op(tdata->opt->ReachableDirAddresses->value, OP_EQ, "*:80");
  expect_log_msg("Converting FascistFirewall config "
            "option to new format: \"ReachableORAddresses *:443\"\n");
  tt_str_op(tdata->opt->ReachableORAddresses->value, OP_EQ, "*:443");
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data("FascistFirewall 1\n"
                                "ReachableDirAddresses *:81\n"
                                "ReachableORAddresses *:444\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");
  tdata->opt->FirewallPorts = smartlist_new();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_entry();
  tt_str_op(tdata->opt->ReachableDirAddresses->value, OP_EQ, "*:81");
  tt_str_op(tdata->opt->ReachableORAddresses->value, OP_EQ, "*:444");
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data("FascistFirewall 1\n"
                                "FirewallPort 123\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg("Converting FascistFirewall and "
            "FirewallPorts config options to new format: "
            "\"ReachableAddresses *:123\"\n");
  tt_str_op(tdata->opt->ReachableAddresses->value, OP_EQ, "*:123");
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data("FascistFirewall 1\n"
                                "ReachableAddresses *:82\n"
                                "ReachableAddresses *:83\n"
                                "ReachableAddresses reject *:*\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_entry();
  tt_str_op(tdata->opt->ReachableAddresses->value, OP_EQ, "*:82");
  tor_free(msg);

#define SERVERS_REACHABLE_MSG "Servers must be able to freely connect to" \
  " the rest of the Internet, so they must not set Reachable*Addresses or" \
  " FascistFirewall or FirewallPorts or ClientUseIPv4 0."

  free_options_test_data(tdata);
  tdata = get_options_test_data("ReachableAddresses *:82\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, SERVERS_REACHABLE_MSG);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ReachableORAddresses *:82\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, SERVERS_REACHABLE_MSG);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ReachableDirAddresses *:82\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, SERVERS_REACHABLE_MSG);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("ClientUseIPv4 0\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, SERVERS_REACHABLE_MSG);
  tor_free(msg);

  /* Test IPv4-only clients setting IPv6 preferences */

#define WARN_PLEASE_USE_IPV6_OR_LOG_MSG \
        "ClientPreferIPv6ORPort 1 is ignored unless tor is using IPv6. " \
        "Please set ClientUseIPv6 1, ClientUseIPv4 0, or configure bridges.\n"

#define WARN_PLEASE_USE_IPV6_DIR_LOG_MSG \
        "ClientPreferIPv6DirPort 1 is ignored unless tor is using IPv6. " \
        "Please set ClientUseIPv6 1, ClientUseIPv4 0, or configure bridges.\n"

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientUseIPv4 1\n"
                                "ClientUseIPv6 0\n"
                                "UseBridges 0\n"
                                "ClientPreferIPv6ORPort 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(WARN_PLEASE_USE_IPV6_OR_LOG_MSG);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientUseIPv4 1\n"
                                "ClientUseIPv6 0\n"
                                "UseBridges 0\n"
                                "ClientPreferIPv6DirPort 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(WARN_PLEASE_USE_IPV6_DIR_LOG_MSG);
  tor_free(msg);

  /* Now test an IPv4/IPv6 client setting IPv6 preferences */

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientUseIPv4 1\n"
                                "ClientUseIPv6 1\n"
                                "ClientPreferIPv6ORPort 1\n"
                                "ClientPreferIPv6DirPort 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);

  /* Now test an IPv6 client setting IPv6 preferences */

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientUseIPv6 1\n"
                                "ClientPreferIPv6ORPort 1\n"
                                "ClientPreferIPv6DirPort 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);

  /* And an implicit (IPv4 disabled) IPv6 client setting IPv6 preferences */

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientUseIPv4 0\n"
                                "ClientPreferIPv6ORPort 1\n"
                                "ClientPreferIPv6DirPort 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);

  /* And an implicit (bridge) client setting IPv6 preferences */

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "UseBridges 1\n"
                                "Bridge 127.0.0.1:12345\n"
                                "ClientPreferIPv6ORPort 1\n"
                                "ClientPreferIPv6DirPort 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__use_bridges(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                   "UseBridges 1\n"
                                   "ClientUseIPv4 1\n"
                                   "ORListenAddress 127.0.0.1:5555\n"
                                   "ORPort 955\n"
                                   "MaxClientCircuitsPending 1\n"
                                   "ConnLimit 1\n"
                                   "SchedulerHighWaterMark__ 42\n"
                                   "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Servers must be able to freely connect to the rest of"
            " the Internet, so they must not set UseBridges.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("UseBridges 1\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_NE, "Servers must be able to freely connect to the rest of"
            " the Internet, so they must not set UseBridges.");
  tor_free(msg);

  NS_MOCK(geoip_get_country);
  free_options_test_data(tdata);
  tdata = get_options_test_data("UseBridges 1\n"
                                "EntryNodes {cn}\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "You cannot set both UseBridges and EntryNodes.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "UseBridges 1\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "If you set UseBridges, you must specify at least one bridge.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "UseBridges 1\n"
                                "Bridge 10.0.0.1\n"
                                "Bridge !!!\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Bridge line did not parse. See logs for details.");
  tor_free(msg);

 done:
  NS_UNMOCK(geoip_get_country);
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__entry_nodes(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  NS_MOCK(geoip_get_country);
  options_test_data_t *tdata = get_options_test_data(
                                         "EntryNodes {cn}\n"
                                         "UseEntryGuards 0\n"
                                         "MaxClientCircuitsPending 1\n"
                                         "ConnLimit 1\n"
                                         "SchedulerHighWaterMark__ 42\n"
                                         "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "If EntryNodes is set, UseEntryGuards must be enabled.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("EntryNodes {cn}\n"
                                "UseEntryGuards 1\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "KeepalivePeriod option must be positive.");
  tor_free(msg);

 done:
  NS_UNMOCK(geoip_get_country);
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__invalid_nodes(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                  "AllowInvalidNodes something_stupid\n"
                                  "MaxClientCircuitsPending 1\n"
                                  "ConnLimit 1\n"
                                  "SchedulerHighWaterMark__ 42\n"
                                  "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Unrecognized value 'something_stupid' in AllowInvalidNodes");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AllowInvalidNodes entry, middle, exit\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->AllowInvalid_, OP_EQ, ALLOW_INVALID_ENTRY |
            ALLOW_INVALID_EXIT | ALLOW_INVALID_MIDDLE);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("AllowInvalidNodes introduction, rendezvous\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->AllowInvalid_, OP_EQ, ALLOW_INVALID_INTRODUCTION |
            ALLOW_INVALID_RENDEZVOUS);
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__safe_logging(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = get_options_test_data(
                                            "MaxClientCircuitsPending 1\n"
                                            "ConnLimit 1\n"
                                            "SchedulerHighWaterMark__ 42\n"
                                            "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->SafeLogging_, OP_EQ, SAFELOG_SCRUB_NONE);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("SafeLogging 0\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->SafeLogging_, OP_EQ, SAFELOG_SCRUB_NONE);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("SafeLogging Relay\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->SafeLogging_, OP_EQ, SAFELOG_SCRUB_RELAY);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("SafeLogging 1\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_int_op(tdata->opt->SafeLogging_, OP_EQ, SAFELOG_SCRUB_ALL);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("SafeLogging stuffy\n"
                                "MaxClientCircuitsPending 1\n"
                                "ConnLimit 1\n"
                                "SchedulerHighWaterMark__ 42\n"
                                "SchedulerLowWaterMark__ 10\n");

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Unrecognized value '\"stuffy\"' in SafeLogging");
  tor_free(msg);

 done:
  escaped(NULL); // This will free the leaking memory from the previous escaped
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__publish_server_descriptor(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_WARN);
  options_test_data_t *tdata = get_options_test_data(
             "PublishServerDescriptor bridge\n" TEST_OPTIONS_DEFAULT_VALUES
                                                     );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("PublishServerDescriptor humma\n"
                                TEST_OPTIONS_DEFAULT_VALUES);

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Unrecognized value in PublishServerDescriptor");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("PublishServerDescriptor bridge, v3\n"
                                TEST_OPTIONS_DEFAULT_VALUES);

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Bridges are not supposed to publish router "
            "descriptors to the directory authorities. Please correct your "
            "PublishServerDescriptor line.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("BridgeRelay 1\n"
                                "PublishServerDescriptor v3\n"
                                TEST_OPTIONS_DEFAULT_VALUES);

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Bridges are not supposed to publish router "
            "descriptors to the directory authorities. Please correct your "
            "PublishServerDescriptor line.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("BridgeRelay 1\n" TEST_OPTIONS_DEFAULT_VALUES);

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_NE, "Bridges are not supposed to publish router "
            "descriptors to the directory authorities. Please correct your "
            "PublishServerDescriptor line.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data("BridgeRelay 1\n"
                                "DirPort 999\n" TEST_OPTIONS_DEFAULT_VALUES);

  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  expect_log_msg("Can't set a DirPort on a bridge "
            "relay; disabling DirPort\n");
  tt_assert(!tdata->opt->DirPort_lines);
  tt_assert(!tdata->opt->DirPort_set);

 done:
  teardown_capture_of_logs();
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__testing(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

#define ENSURE_DEFAULT(varname, varval)                     \
  STMT_BEGIN                                                \
    free_options_test_data(tdata);                          \
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES \
                                #varname " " #varval "\n"); \
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);\
  tt_str_op(msg, OP_EQ, \
            #varname " may only be changed in testing Tor networks!");  \
  tt_int_op(ret, OP_EQ, -1);                                            \
  tor_free(msg);                                                        \
                                                \
  free_options_test_data(tdata); \
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES \
                                #varname " " #varval "\n"               \
                                VALID_DIR_AUTH                          \
                                "TestingTorNetwork 1\n");               \
                                                                        \
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);\
  if (msg) { \
    tt_str_op(msg, OP_NE, \
              #varname " may only be changed in testing Tor networks!"); \
    tor_free(msg); \
  } \
                                                                        \
  free_options_test_data(tdata);          \
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES \
                                #varname " " #varval "\n"           \
                                "___UsingTestNetworkDefaults 1\n"); \
                                                                        \
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);\
  if (msg) { \
    tt_str_op(msg, OP_NE, \
              #varname " may only be changed in testing Tor networks!"); \
    tor_free(msg); \
  } \
    STMT_END

  ENSURE_DEFAULT(TestingV3AuthInitialVotingInterval, 3600);
  ENSURE_DEFAULT(TestingV3AuthInitialVoteDelay, 3000);
  ENSURE_DEFAULT(TestingV3AuthInitialDistDelay, 3000);
  ENSURE_DEFAULT(TestingV3AuthVotingStartOffset, 3000);
  ENSURE_DEFAULT(TestingAuthDirTimeToLearnReachability, 3000);
  ENSURE_DEFAULT(TestingEstimatedDescriptorPropagationTime, 3000);
  ENSURE_DEFAULT(TestingServerDownloadSchedule, 3000);
  ENSURE_DEFAULT(TestingClientDownloadSchedule, 3000);
  ENSURE_DEFAULT(TestingServerConsensusDownloadSchedule, 3000);
  ENSURE_DEFAULT(TestingClientConsensusDownloadSchedule, 3000);
  ENSURE_DEFAULT(TestingBridgeDownloadSchedule, 3000);
  ENSURE_DEFAULT(TestingClientMaxIntervalWithoutRequest, 3000);
  ENSURE_DEFAULT(TestingDirConnectionMaxStall, 3000);
  ENSURE_DEFAULT(TestingConsensusMaxDownloadTries, 3000);
  ENSURE_DEFAULT(TestingDescriptorMaxDownloadTries, 3000);
  ENSURE_DEFAULT(TestingMicrodescMaxDownloadTries, 3000);
  ENSURE_DEFAULT(TestingCertMaxDownloadTries, 3000);
  ENSURE_DEFAULT(TestingAuthKeyLifetime, 3000);
  ENSURE_DEFAULT(TestingLinkCertLifetime, 3000);
  ENSURE_DEFAULT(TestingSigningKeySlop, 3000);
  ENSURE_DEFAULT(TestingAuthKeySlop, 3000);
  ENSURE_DEFAULT(TestingLinkKeySlop, 3000);

 done:
  escaped(NULL); // This will free the leaking memory from the previous escaped
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__hidserv(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_WARN);

  options_test_data_t *tdata = get_options_test_data(
                                                TEST_OPTIONS_DEFAULT_VALUES);
  tdata->opt->MinUptimeHidServDirectoryV2 = -1;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("MinUptimeHidServDirectoryV2 "
            "option must be at least 0 seconds. Changing to 0.\n");
  tt_int_op(tdata->opt->MinUptimeHidServDirectoryV2, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RendPostPeriod 1\n" );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("RendPostPeriod option is too short;"
            " raising to 600 seconds.\n");
  tt_int_op(tdata->opt->RendPostPeriod, OP_EQ, 600);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RendPostPeriod 302401\n" );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("RendPostPeriod is too large; "
            "clipping to 302400s.\n");
  tt_int_op(tdata->opt->RendPostPeriod, OP_EQ, 302400);
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__predicted_ports(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  setup_capture_of_logs(LOG_WARN);

  options_test_data_t *tdata = get_options_test_data(
                                     "PredictedPortsRelevanceTime 100000000\n"
                                     TEST_OPTIONS_DEFAULT_VALUES);
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("PredictedPortsRelevanceTime is too "
            "large; clipping to 3600s.\n");
  tt_int_op(tdata->opt->PredictedPortsRelevanceTime, OP_EQ, 3600);

 done:
  teardown_capture_of_logs();
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__path_bias(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;

  options_test_data_t *tdata = get_options_test_data(
                                            TEST_OPTIONS_DEFAULT_VALUES
                                            "PathBiasNoticeRate 1.1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "PathBiasNoticeRate is too high. It must be between 0 and 1.0");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "PathBiasWarnRate 1.1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "PathBiasWarnRate is too high. It must be between 0 and 1.0");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "PathBiasExtremeRate 1.1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "PathBiasExtremeRate is too high. It must be between 0 and 1.0");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "PathBiasNoticeUseRate 1.1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "PathBiasNoticeUseRate is too high. It must be between 0 and 1.0");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "PathBiasExtremeUseRate 1.1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
           "PathBiasExtremeUseRate is too high. It must be between 0 and 1.0");
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__bandwidth(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

#define ENSURE_BANDWIDTH_PARAM(p) \
  STMT_BEGIN                                                \
  free_options_test_data(tdata); \
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES #p " 3Gb\n"); \
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);\
  tt_int_op(ret, OP_EQ, -1); \
  tt_mem_op(msg, OP_EQ, #p " (3221225471) must be at most 2147483647", 40); \
  tor_free(msg); \
  STMT_END

  ENSURE_BANDWIDTH_PARAM(BandwidthRate);
  ENSURE_BANDWIDTH_PARAM(BandwidthBurst);
  ENSURE_BANDWIDTH_PARAM(MaxAdvertisedBandwidth);
  ENSURE_BANDWIDTH_PARAM(RelayBandwidthRate);
  ENSURE_BANDWIDTH_PARAM(RelayBandwidthBurst);
  ENSURE_BANDWIDTH_PARAM(PerConnBWRate);
  ENSURE_BANDWIDTH_PARAM(PerConnBWBurst);
  ENSURE_BANDWIDTH_PARAM(AuthDirFastGuarantee);
  ENSURE_BANDWIDTH_PARAM(AuthDirGuardBWGuarantee);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RelayBandwidthRate 1000\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_u64_op(tdata->opt->RelayBandwidthBurst, OP_EQ, 1000);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RelayBandwidthBurst 1001\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_u64_op(tdata->opt->RelayBandwidthRate, OP_EQ, 1001);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RelayBandwidthRate 1001\n"
                                "RelayBandwidthBurst 1000\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "RelayBandwidthBurst must be at least equal to "
            "RelayBandwidthRate.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "BandwidthRate 1001\n"
                                "BandwidthBurst 1000\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "BandwidthBurst must be at least equal to BandwidthRate.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RelayBandwidthRate 1001\n"
                                "BandwidthRate 1000\n"
                                "BandwidthBurst 1000\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_u64_op(tdata->opt->BandwidthRate, OP_EQ, 1001);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "RelayBandwidthRate 1001\n"
                                "BandwidthRate 1000\n"
                                "RelayBandwidthBurst 1001\n"
                                "BandwidthBurst 1000\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_u64_op(tdata->opt->BandwidthBurst, OP_EQ, 1001);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "BandwidthRate is set to 1 bytes/second. For servers,"
            " it must be at least 76800.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 76800\n"
                                "MaxAdvertisedBandwidth 30000\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "MaxAdvertisedBandwidth is set to 30000 bytes/second."
            " For servers, it must be at least 38400.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 76800\n"
                                "RelayBandwidthRate 1\n"
                                "MaxAdvertisedBandwidth 38400\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "RelayBandwidthRate is set to 1 bytes/second. For "
            "servers, it must be at least 76800.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 76800\n"
                                "BandwidthBurst 76800\n"
                                "RelayBandwidthRate 76800\n"
                                "MaxAdvertisedBandwidth 38400\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

 done:
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__circuits(void *ignored)
{
  (void)ignored;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "MaxCircuitDirtiness 2592001\n");
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("MaxCircuitDirtiness option is too "
            "high; setting to 30 days.\n");
  tt_int_op(tdata->opt->MaxCircuitDirtiness, OP_EQ, 2592000);
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "CircuitStreamTimeout 1\n");
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("CircuitStreamTimeout option is too"
            " short; raising to 10 seconds.\n");
  tt_int_op(tdata->opt->CircuitStreamTimeout, OP_EQ, 10);
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "CircuitStreamTimeout 111\n");
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_no_log_msg("CircuitStreamTimeout option is too"
            " short; raising to 10 seconds.\n");
  tt_int_op(tdata->opt->CircuitStreamTimeout, OP_EQ, 111);
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HeartbeatPeriod 1\n");
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("HeartbeatPeriod option is too short;"
            " raising to 1800 seconds.\n");
  tt_int_op(tdata->opt->HeartbeatPeriod, OP_EQ, 1800);
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HeartbeatPeriod 1982\n");
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_no_log_msg("HeartbeatPeriod option is too short;"
            " raising to 1800 seconds.\n");
  tt_int_op(tdata->opt->HeartbeatPeriod, OP_EQ, 1982);
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "CircuitBuildTimeout 1\n"
                                );
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_log_msg("CircuitBuildTimeout is shorter (1"
            " seconds) than the recommended minimum (10 seconds), and "
            "LearnCircuitBuildTimeout is disabled.  If tor isn't working, "
            "raise this value or enable LearnCircuitBuildTimeout.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  mock_clean_saved_logs();
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "CircuitBuildTimeout 11\n"
                                );
  options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  expect_no_log_msg("CircuitBuildTimeout is shorter (1 "
            "seconds) than the recommended minimum (10 seconds), and "
            "LearnCircuitBuildTimeout is disabled.  If tor isn't working, "
            "raise this value or enable LearnCircuitBuildTimeout.\n");
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__port_forwarding(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "PortForwarding 1\nSandbox 1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "PortForwarding is not compatible with Sandbox;"
            " at most one can be set");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "PortForwarding 1\nSandbox 0\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

 done:
  free_options_test_data(tdata);
  policies_free_all();
  tor_free(msg);
}

static void
test_options_validate__tor2web(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Tor2webRendezvousPoints 1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Tor2webRendezvousPoints cannot be set without Tor2webMode.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Tor2webRendezvousPoints 1\nTor2webMode 1\n");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

 done:
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__rend(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                 "UseEntryGuards 0\n"
                 "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
                 "HiddenServicePort 80 127.0.0.1:8080\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("UseEntryGuards is disabled, but you"
            " have configured one or more hidden services on this Tor "
            "instance.  Your hidden services will be very easy to locate using"
            " a well-known attack -- see http://freehaven.net/anonbib/#hs-"
            "attack06 for details.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(
            TEST_OPTIONS_DEFAULT_VALUES
            "UseEntryGuards 1\n"
            "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
            "HiddenServicePort 80 127.0.0.1:8080\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg("UseEntryGuards is disabled, but you"
            " have configured one or more hidden services on this Tor "
            "instance.  Your hidden services will be very easy to locate using"
            " a well-known attack -- see http://freehaven.net/anonbib/#hs-"
            "attack06 for details.\n");

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HiddenServicePort 80 127.0.0.1:8080\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Failed to configure rendezvous options. See logs for details.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HidServAuth failed\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Failed to configure client authorization for hidden "
            "services. See logs for details.");
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__single_onion(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  /* Test that HiddenServiceSingleHopMode must come with
   * HiddenServiceNonAnonymousMode */
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 0\n"
                                "HiddenServiceSingleHopMode 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HiddenServiceSingleHopMode does not provide any "
            "server anonymity. It must be used with "
            "HiddenServiceNonAnonymousMode set to 1.");
  tor_free(msg);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 0\n"
                                "HiddenServiceSingleHopMode 1\n"
                                "HiddenServiceNonAnonymousMode 0\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HiddenServiceSingleHopMode does not provide any "
            "server anonymity. It must be used with "
            "HiddenServiceNonAnonymousMode set to 1.");
  tor_free(msg);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 0\n"
                                "HiddenServiceSingleHopMode 1\n"
                                "HiddenServiceNonAnonymousMode 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);
  free_options_test_data(tdata);

  /* Test that SOCKSPort must come with Tor2webMode if
   * HiddenServiceSingleHopMode is 1 */
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 5000\n"
                                "HiddenServiceSingleHopMode 1\n"
                                "HiddenServiceNonAnonymousMode 1\n"
                                "Tor2webMode 0\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HiddenServiceNonAnonymousMode is incompatible with "
            "using Tor as an anonymous client. Please set "
            "Socks/Trans/NATD/DNSPort to 0, or HiddenServiceNonAnonymousMode "
            "to 0, or use the non-anonymous Tor2webMode.");
  tor_free(msg);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 0\n"
                                "HiddenServiceSingleHopMode 1\n"
                                "HiddenServiceNonAnonymousMode 1\n"
                                "Tor2webMode 0\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 5000\n"
                                "HiddenServiceSingleHopMode 0\n"
                                "Tor2webMode 0\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "SOCKSPort 5000\n"
                                "HiddenServiceSingleHopMode 1\n"
                                "HiddenServiceNonAnonymousMode 1\n"
                                "Tor2webMode 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);
  free_options_test_data(tdata);

  /* Test that a hidden service can't be run with Tor2web
   * Use HiddenServiceNonAnonymousMode instead of Tor2webMode, because
   * Tor2webMode requires a compilation #define */
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                  "HiddenServiceNonAnonymousMode 1\n"
                  "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
                  "HiddenServicePort 80 127.0.0.1:8080\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HiddenServiceNonAnonymousMode does not provide any "
            "server anonymity. It must be used with "
            "HiddenServiceSingleHopMode set to 1.");
  tor_free(msg);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                  "HiddenServiceNonAnonymousMode 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HiddenServiceNonAnonymousMode does not provide any "
            "server anonymity. It must be used with "
            "HiddenServiceSingleHopMode set to 1.");
  tor_free(msg);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                  "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
                  "HiddenServicePort 80 127.0.0.1:8080\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);
  free_options_test_data(tdata);

  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                  "HiddenServiceNonAnonymousMode 1\n"
                  "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
                  "HiddenServicePort 80 127.0.0.1:8080\n"
                  "HiddenServiceSingleHopMode 1\n"
                  "SOCKSPort 0\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_ptr_op(msg, OP_EQ, NULL);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__accounting(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccountingRule something_bad\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "AccountingRule must be 'sum', 'max', 'in', or 'out'");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccountingRule sum\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->AccountingRule, OP_EQ, ACCT_SUM);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccountingRule max\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->AccountingRule, OP_EQ, ACCT_MAX);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccountingStart fail\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Failed to parse accounting options. See logs for details.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccountingMax 10\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(
           TEST_OPTIONS_DEFAULT_VALUES
           "ORListenAddress 127.0.0.1:5555\n"
           "ORPort 955\n"
           "BandwidthRate 76800\n"
           "BandwidthBurst 76800\n"
           "MaxAdvertisedBandwidth 38400\n"
           "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
           "HiddenServicePort 80 127.0.0.1:8080\n"
           "AccountingMax 10\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("Using accounting with a hidden "
            "service and an ORPort is risky: your hidden service(s) and "
            "your public address will all turn off at the same time, "
            "which may alert observers that they are being run by the "
            "same party.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(
                TEST_OPTIONS_DEFAULT_VALUES
                "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
                "HiddenServicePort 80 127.0.0.1:8080\n"
                "AccountingMax 10\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg("Using accounting with a hidden "
            "service and an ORPort is risky: your hidden service(s) and "
            "your public address will all turn off at the same time, "
            "which may alert observers that they are being run by the "
            "same party.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(
             TEST_OPTIONS_DEFAULT_VALUES
             "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service/\n"
             "HiddenServicePort 80 127.0.0.1:8080\n"
             "HiddenServiceDir /Library/Tor/var/lib/tor/hidden_service2/\n"
             "HiddenServicePort 81 127.0.0.1:8081\n"
             "AccountingMax 10\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("Using accounting with multiple "
            "hidden services is risky: they will all turn off at the same"
            " time, which may alert observers that they are being run by "
            "the same party.\n");
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__proxy(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  sandbox_disable_getaddrinfo_cache();
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy 127.0.42.1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->HTTPProxyPort, OP_EQ, 80);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy 127.0.42.1:444\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->HTTPProxyPort, OP_EQ, 444);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy not_so_valid!\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HTTPProxy failed to parse or resolve. Please fix.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxyAuthenticator "
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreeonetwothreeonetwothree"

                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HTTPProxyAuthenticator is too long (>= 512 chars).");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxyAuthenticator validauth\n"

                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpsProxy 127.0.42.1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->HTTPSProxyPort, OP_EQ, 443);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpsProxy 127.0.42.1:444\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->HTTPSProxyPort, OP_EQ, 444);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpsProxy not_so_valid!\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HTTPSProxy failed to parse or resolve. Please fix.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpsProxyAuthenticator "
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreonetwothreonetwothreonetwothre"
                                "onetwothreonetwothreonetwothreonetwothreonetw"
                                "othreonetwothreeonetwothreeonetwothree"

                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "HTTPSProxyAuthenticator is too long (>= 512 chars).");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpsProxyAuthenticator validauth\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks4Proxy 127.0.42.1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->Socks4ProxyPort, OP_EQ, 1080);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks4Proxy 127.0.42.1:444\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->Socks4ProxyPort, OP_EQ, 444);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks4Proxy not_so_valid!\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Socks4Proxy failed to parse or resolve. Please fix.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5Proxy 127.0.42.1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->Socks5ProxyPort, OP_EQ, 1080);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5Proxy 127.0.42.1:444\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->Socks5ProxyPort, OP_EQ, 444);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5Proxy not_so_valid!\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Socks5Proxy failed to parse or resolve. Please fix.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks4Proxy 215.1.1.1\n"
                                "Socks5Proxy 215.1.1.2\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "You have configured more than one proxy type. "
            "(Socks4Proxy|Socks5Proxy|HTTPSProxy)");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy 215.1.1.1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("HTTPProxy configured, but no SOCKS "
            "proxy or HTTPS proxy configured. Watch out: this configuration "
            "will proxy unencrypted directory connections only.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy 215.1.1.1\n"
                                "Socks4Proxy 215.1.1.1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg("HTTPProxy configured, but no SOCKS "
            "proxy or HTTPS proxy configured. Watch out: this configuration "
            "will proxy unencrypted directory connections only.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy 215.1.1.1\n"
                                "Socks5Proxy 215.1.1.1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg("HTTPProxy configured, but no SOCKS "
            "proxy or HTTPS proxy configured. Watch out: this configuration "
            "will proxy unencrypted directory connections only.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HttpProxy 215.1.1.1\n"
                                "HttpsProxy 215.1.1.1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "HTTPProxy configured, but no SOCKS proxy or HTTPS proxy "
            "configured. Watch out: this configuration will proxy "
            "unencrypted directory connections only.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                );
  tdata->opt->Socks5ProxyUsername = tor_strdup("");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Socks5ProxyUsername must be between 1 and 255 characters.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                );
  tdata->opt->Socks5ProxyUsername =
    tor_strdup("ABCDEABCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789AB"
               "CDEABCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789ABCD"
               "EABCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789ABCDEA"
               "BCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789ABCDEABC"
               "DE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Socks5ProxyUsername must be between 1 and 255 characters.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5ProxyUsername hello_world\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Socks5ProxyPassword must be included with "
            "Socks5ProxyUsername.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5ProxyUsername hello_world\n"
                                );
  tdata->opt->Socks5ProxyPassword = tor_strdup("");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Socks5ProxyPassword must be between 1 and 255 characters.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5ProxyUsername hello_world\n"
                                );
  tdata->opt->Socks5ProxyPassword =
    tor_strdup("ABCDEABCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789AB"
               "CDEABCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789ABCD"
               "EABCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789ABCDEA"
               "BCDE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789ABCDEABC"
               "DE0123456789ABCDEABCDE0123456789ABCDEABCDE0123456789");
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Socks5ProxyPassword must be between 1 and 255 characters.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5ProxyUsername hello_world\n"
                                "Socks5ProxyPassword world_hello\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "Socks5ProxyPassword hello_world\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Socks5ProxyPassword must be included with "
            "Socks5ProxyUsername.");
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  policies_free_all();
  // sandbox_free_getaddrinfo_cache();
  tor_free(msg);
}

static void
test_options_validate__control(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HashedControlPassword something_incorrect\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Bad HashedControlPassword: wrong length or bad encoding");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "HashedControlPassword 16:872860B76453A77D60CA"
                                "2BB8C1A7042072093276A3D701AD684053EC4C\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(
                   TEST_OPTIONS_DEFAULT_VALUES
                   "__HashedControlSessionPassword something_incorrect\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Bad HashedControlSessionPassword: wrong length or "
            "bad encoding");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "__HashedControlSessionPassword 16:872860B7645"
                                "3A77D60CA2BB8C1A7042072093276A3D701AD684053EC"
                                "4C\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(
                           TEST_OPTIONS_DEFAULT_VALUES
                           "__OwningControllerProcess something_incorrect\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Bad OwningControllerProcess: invalid PID");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "__OwningControllerProcess 123\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlPort 127.0.0.1:1234\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(
            "ControlPort is open, but no authentication method has been "
            "configured.  This means that any program on your computer can "
            "reconfigure your Tor.  That's bad!  You should upgrade your Tor"
            " controller as soon as possible.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlPort 127.0.0.1:1234\n"
                                "HashedControlPassword 16:872860B76453A77D60CA"
                                "2BB8C1A7042072093276A3D701AD684053EC4C\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "ControlPort is open, but no authentication method has been "
            "configured.  This means that any program on your computer can "
            "reconfigure your Tor.  That's bad!  You should upgrade your Tor "
            "controller as soon as possible.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlPort 127.0.0.1:1234\n"
                                "__HashedControlSessionPassword 16:872860B7645"
                                "3A77D60CA2BB8C1A7042072093276A3D701AD684053EC"
                                "4C\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "ControlPort is open, but no authentication method has been "
            "configured.  This means that any program on your computer can "
            "reconfigure your Tor.  That's bad!  You should upgrade your Tor "
            "controller as soon as possible.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlPort 127.0.0.1:1234\n"
                                "CookieAuthentication 1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "ControlPort is open, but no authentication method has been "
            "configured.  This means that any program on your computer can "
            "reconfigure your Tor.  That's bad!  You should upgrade your Tor "
            "controller as soon as possible.\n");
  tor_free(msg);

#ifdef HAVE_SYS_UN_H
  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlSocket unix:/tmp WorldWritable\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(
            "ControlSocket is world writable, but no authentication method has"
            " been configured.  This means that any program on your computer "
            "can reconfigure your Tor.  That's bad!  You should upgrade your "
            "Tor controller as soon as possible.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlSocket unix:/tmp WorldWritable\n"
                                "HashedControlPassword 16:872860B76453A77D60CA"
                                "2BB8C1A7042072093276A3D701AD684053EC4C\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "ControlSocket is world writable, but no authentication method has"
            " been configured.  This means that any program on your computer "
            "can reconfigure your Tor.  That's bad!  You should upgrade your "
            "Tor controller as soon as possible.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlSocket unix:/tmp WorldWritable\n"
                                "__HashedControlSessionPassword 16:872860B7645"
                                "3A77D60CA2BB8C1A7042072093276A3D701AD684053EC"
                                "4C\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "ControlSocket is world writable, but no authentication method has"
            " been configured.  This means that any program on your computer "
            "can reconfigure your Tor.  That's bad!  You should upgrade your "
            "Tor controller as soon as possible.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ControlSocket unix:/tmp WorldWritable\n"
                                "CookieAuthentication 1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "ControlSocket is world writable, but no authentication method has"
            " been configured.  This means that any program on your computer "
            "can reconfigure your Tor.  That's bad!  You should upgrade your "
            "Tor controller as soon as possible.\n");
  tor_free(msg);
#endif

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "CookieAuthFileGroupReadable 1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(
            "CookieAuthFileGroupReadable is set, but will have no effect: you "
            "must specify an explicit CookieAuthFile to have it "
            "group-readable.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "CookieAuthFileGroupReadable 1\n"
                                "CookieAuthFile /tmp/somewhere\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "CookieAuthFileGroupReadable is set, but will have no effect: you "
            "must specify an explicit CookieAuthFile to have it "
            "group-readable.\n");
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__families(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "MyFamily home\n"
                                "BridgeRelay 1\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 51300\n"
                                "BandwidthBurst 51300\n"
                                "MaxAdvertisedBandwidth 25700\n"
                                "DirCache 1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(
            "Listing a family for a bridge relay is not supported: it can "
            "reveal bridge fingerprints to censors. You should also make sure "
            "you aren't listing this bridge's fingerprint in any other "
            "MyFamily.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "MyFamily home\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "Listing a family for a bridge relay is not supported: it can "
            "reveal bridge fingerprints to censors. You should also make sure "
            "you aren't listing this bridge's fingerprint in any other "
            "MyFamily.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "MyFamily !\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Invalid nickname '!' in MyFamily line");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "NodeFamily foo\n"
                                "NodeFamily !\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_assert(!msg);
  tor_free(msg);

 done:
  teardown_capture_of_logs();
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__addr_policies(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ExitPolicy !!!\n"
                                "ExitRelay 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Error in ExitPolicy entry.");
  tor_free(msg);

 done:
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__dir_auth(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                VALID_DIR_AUTH
                                VALID_ALT_DIR_AUTH
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Directory authority/fallback line did not parse. See logs for "
            "details.");
  expect_log_msg(
            "You cannot set both DirAuthority and Alternate*Authority.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "TestingTorNetwork may only be configured in combination with a "
            "non-default set of DirAuthority or both of AlternateDirAuthority "
            "and AlternateBridgeAuthority configured.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingTorNetwork 1\n"
                                VALID_ALT_DIR_AUTH
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "TestingTorNetwork may only be configured in combination with a "
            "non-default set of DirAuthority or both of AlternateDirAuthority "
            "and AlternateBridgeAuthority configured.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingTorNetwork 1\n"
                                VALID_ALT_BRIDGE_AUTH
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingTorNetwork may only be configured in "
            "combination with a non-default set of DirAuthority or both of "
            "AlternateDirAuthority and AlternateBridgeAuthority configured.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                VALID_ALT_DIR_AUTH
                                VALID_ALT_BRIDGE_AUTH
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__transport(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_NOTICE);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientTransportPlugin !!\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Invalid client transport line. See logs for details.");
  expect_log_msg(
            "Too few arguments on ClientTransportPlugin line.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ClientTransportPlugin foo exec bar\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ServerTransportPlugin !!\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Invalid server transport line. See logs for details.");
  expect_log_msg(
            "Too few arguments on ServerTransportPlugin line.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ServerTransportPlugin foo exec bar\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(
            "Tor is not configured as a relay but you specified a "
            "ServerTransportPlugin line (\"foo exec bar\"). The "
            "ServerTransportPlugin line will be ignored.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ServerTransportPlugin foo exec bar\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 76900\n"
                                "BandwidthBurst 76900\n"
                                "MaxAdvertisedBandwidth 38500\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "Tor is not configured as a relay but you specified a "
            "ServerTransportPlugin line (\"foo exec bar\"). The "
            "ServerTransportPlugin line will be ignored.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ServerTransportListenAddr foo 127.0.0.42:55\n"
                                "ServerTransportListenAddr !\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "ServerTransportListenAddr did not parse. See logs for details.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ServerTransportListenAddr foo 127.0.0.42:55\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg(
            "You need at least a single managed-proxy to specify a transport "
            "listen address. The ServerTransportListenAddr line will be "
            "ignored.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ServerTransportListenAddr foo 127.0.0.42:55\n"
                                "ServerTransportPlugin foo exec bar\n"
                                "ORListenAddress 127.0.0.1:5555\n"
                                "ORPort 955\n"
                                "BandwidthRate 76900\n"
                                "BandwidthBurst 76900\n"
                                "MaxAdvertisedBandwidth 38500\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "You need at least a single managed-proxy to specify a transport "
            "listen address. The ServerTransportListenAddr line will be "
            "ignored.\n");

 done:
  escaped(NULL); // This will free the leaking memory from the previous escaped
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__constrained_sockets(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ConstrainedSockets 1\n"
                                "ConstrainedSockSize 0\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "ConstrainedSockSize is invalid.  Must be a value "
            "between 2048 and 262144 in 1024 byte increments.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ConstrainedSockets 1\n"
                                "ConstrainedSockSize 263168\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "ConstrainedSockSize is invalid.  Must be a value "
            "between 2048 and 262144 in 1024 byte increments.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ConstrainedSockets 1\n"
                                "ConstrainedSockSize 2047\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "ConstrainedSockSize is invalid.  Must be a value "
            "between 2048 and 262144 in 1024 byte increments.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ConstrainedSockets 1\n"
                                "ConstrainedSockSize 2048\n"
                                "DirPort 999\n"
                                "DirCache 1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("You have requested constrained "
            "socket buffers while also serving directory entries via DirPort."
            "  It is strongly suggested that you disable serving directory"
            " requests when system TCP buffer resources are scarce.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "ConstrainedSockets 1\n"
                                "ConstrainedSockSize 2048\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg(
            "You have requested constrained socket buffers while also serving"
            " directory entries via DirPort.  It is strongly suggested that "
            "you disable serving directory requests when system TCP buffer "
            "resources are scarce.\n");
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__v3_auth(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 1000\n"
                                "V3AuthDistDelay 1000\n"
                                "V3AuthVotingInterval 1000\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "V3AuthVoteDelay plus V3AuthDistDelay must be less than half "
            "V3AuthVotingInterval");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthVoteDelay is way too low.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 1\n"
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthVoteDelay is way too low.");
  tor_free(msg);

  // TODO: we can't reach the case of v3authvotedelay lower
  // than MIN_VOTE_SECONDS but not lower than MIN_VOTE_SECONDS_TESTING,
  // since they are the same

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthDistDelay 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthDistDelay is way too low.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthDistDelay 1\n"
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthDistDelay is way too low.");
  tor_free(msg);

  // TODO: we can't reach the case of v3authdistdelay lower than
  // MIN_DIST_SECONDS but not lower than MIN_DIST_SECONDS_TESTING,
  // since they are the same

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthNIntervalsValid 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthNIntervalsValid must be at least 2.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 49\n"
                                "V3AuthDistDelay 49\n"
                                "V3AuthVotingInterval 200\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthVotingInterval is insanely low.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 49\n"
                                "V3AuthDistDelay 49\n"
                                "V3AuthVotingInterval 200000\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "V3AuthVotingInterval is insanely high.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 49\n"
                                "V3AuthDistDelay 49\n"
                                "V3AuthVotingInterval 1441\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("V3AuthVotingInterval does not divide"
            " evenly into 24 hours.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 49\n"
                                "V3AuthDistDelay 49\n"
                                "V3AuthVotingInterval 1440\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg("V3AuthVotingInterval does not divide"
            " evenly into 24 hours.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "V3AuthVoteDelay 49\n"
                                "V3AuthDistDelay 49\n"
                                "V3AuthVotingInterval 299\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("V3AuthVotingInterval is very low. "
            "This may lead to failure to synchronise for a consensus.\n");
  tor_free(msg);

  // TODO: It is impossible to reach the case of testingtor network, with
  // v3authvotinginterval too low
  /* free_options_test_data(tdata); */
  /* tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES */
  /*                               "V3AuthVoteDelay 1\n" */
  /*                               "V3AuthDistDelay 1\n" */
  /*                               "V3AuthVotingInterval 9\n" */
                                   /* VALID_DIR_AUTH */
  /*                               "TestingTorNetwork 1\n" */
  /*                               ); */
  /* ret = options_validate(tdata->old_opt, tdata->opt, */
  /*                        tdata->def_opt, 0, &msg); */
  /* tt_int_op(ret, OP_EQ, -1); */
  /* tt_str_op(msg, OP_EQ, "V3AuthVotingInterval is insanely low."); */

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingV3AuthInitialVoteDelay 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingV3AuthInitialVoteDelay is way too low.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingV3AuthInitialDistDelay 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingV3AuthInitialDistDelay is way too low.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                );
  tdata->opt->TestingV3AuthVotingStartOffset = 100000;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingV3AuthVotingStartOffset is higher than the "
            "voting interval.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                );
  tdata->opt->TestingV3AuthVotingStartOffset = -1;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "TestingV3AuthVotingStartOffset must be non-negative.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                "TestingV3AuthInitialVotingInterval 4\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingV3AuthInitialVotingInterval is insanely low.");
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__virtual_addr(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "VirtualAddrNetworkIPv4 !!"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Error parsing VirtualAddressNetwork !!");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "VirtualAddrNetworkIPv6 !!"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "Error parsing VirtualAddressNetworkIPv6 !!");
  tor_free(msg);

 done:
  escaped(NULL); // This will free the leaking memory from the previous escaped
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__exits(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AllowSingleHopExits 1"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_log_msg("You have set AllowSingleHopExits; "
            "now your relay will allow others to make one-hop exits. However,"
            " since by default most clients avoid relays that set this option,"
            " most clients will ignore you.\n");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AllowSingleHopExits 1\n"
                                VALID_DIR_AUTH
                                );
  mock_clean_saved_logs();
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_msg("You have set AllowSingleHopExits; "
            "now your relay will allow others to make one-hop exits. However,"
            " since by default most clients avoid relays that set this option,"
            " most clients will ignore you.\n");
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__testing_options(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;
  setup_capture_of_logs(LOG_WARN);

#define TEST_TESTING_OPTION(name, low_val, high_val, err_low)           \
  STMT_BEGIN                                                            \
    free_options_test_data(tdata);                                      \
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES             \
                                VALID_DIR_AUTH                          \
                                "TestingTorNetwork 1\n"                 \
                                );                                      \
  tdata->opt-> name = low_val;                                       \
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);\
  tt_int_op(ret, OP_EQ, -1);                                            \
  tt_str_op(msg, OP_EQ, #name " " err_low);                \
  tor_free(msg); \
                                                                        \
  free_options_test_data(tdata);                                        \
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES             \
                                VALID_DIR_AUTH                          \
                                "TestingTorNetwork 1\n"                 \
                                );                                      \
  tdata->opt->  name = high_val;                                      \
  mock_clean_saved_logs();                                              \
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);\
  tt_int_op(ret, OP_EQ, 0);                                             \
  expect_log_msg( #name " is insanely high.\n"); \
  tor_free(msg); \
  STMT_END

  TEST_TESTING_OPTION(TestingAuthDirTimeToLearnReachability, -1, 8000,
                      "must be non-negative.");
  TEST_TESTING_OPTION(TestingEstimatedDescriptorPropagationTime, -1, 3601,
                      "must be non-negative.");
  TEST_TESTING_OPTION(TestingClientMaxIntervalWithoutRequest, -1, 3601,
                      "is way too low.");
  TEST_TESTING_OPTION(TestingDirConnectionMaxStall, 1, 3601,
                      "is way too low.");
  // TODO: I think this points to a bug/regression in options_validate
  TEST_TESTING_OPTION(TestingConsensusMaxDownloadTries, 1, 801,
                      "must be greater than 2.");
  TEST_TESTING_OPTION(TestingDescriptorMaxDownloadTries, 1, 801,
                      "must be greater than 1.");
  TEST_TESTING_OPTION(TestingMicrodescMaxDownloadTries, 1, 801,
                      "must be greater than 1.");
  TEST_TESTING_OPTION(TestingCertMaxDownloadTries, 1, 801,
                      "must be greater than 1.");

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableConnBwEvent 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingEnableConnBwEvent may only be changed in "
            "testing Tor networks!");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableConnBwEvent 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                "___UsingTestNetworkDefaults 0\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableConnBwEvent 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 0\n"
                                "___UsingTestNetworkDefaults 1\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableCellStatsEvent 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingEnableCellStatsEvent may only be changed in "
            "testing Tor networks!");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableCellStatsEvent 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                "___UsingTestNetworkDefaults 0\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableCellStatsEvent 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 0\n"
                                "___UsingTestNetworkDefaults 1\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableTbEmptyEvent 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ, "TestingEnableTbEmptyEvent may only be changed "
            "in testing Tor networks!");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableTbEmptyEvent 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 1\n"
                                "___UsingTestNetworkDefaults 0\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "TestingEnableTbEmptyEvent 1\n"
                                VALID_DIR_AUTH
                                "TestingTorNetwork 0\n"
                                "___UsingTestNetworkDefaults 1\n"
                                );

  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(!msg);
  tor_free(msg);

 done:
  policies_free_all();
  teardown_capture_of_logs();
  free_options_test_data(tdata);
  tor_free(msg);
}

static void
test_options_validate__accel(void *ignored)
{
  (void)ignored;
  int ret;
  char *msg;
  options_test_data_t *tdata = NULL;

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccelName foo\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->HardwareAccel, OP_EQ, 1);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccelName foo\n"
                                );
  tdata->opt->HardwareAccel = 2;
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tdata->opt->HardwareAccel, OP_EQ, 2);
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccelDir 1\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, -1);
  tt_str_op(msg, OP_EQ,
            "Can't use hardware crypto accelerator dir without engine name.");
  tor_free(msg);

  free_options_test_data(tdata);
  tdata = get_options_test_data(TEST_OPTIONS_DEFAULT_VALUES
                                "AccelDir 1\n"
                                "AccelName something\n"
                                );
  ret = options_validate(tdata->old_opt, tdata->opt, tdata->def_opt, 0, &msg);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(msg);

 done:
  policies_free_all();
  free_options_test_data(tdata);
  tor_free(msg);
}

#define LOCAL_VALIDATE_TEST(name) \
  { "validate__" #name, test_options_validate__ ## name, TT_FORK, NULL, NULL }

struct testcase_t options_tests[] = {
  { "validate", test_options_validate, TT_FORK, NULL, NULL },
  { "mem_dircache", test_have_enough_mem_for_dircache, TT_FORK, NULL, NULL },
  LOCAL_VALIDATE_TEST(uname_for_server),
  LOCAL_VALIDATE_TEST(outbound_addresses),
  LOCAL_VALIDATE_TEST(data_directory),
  LOCAL_VALIDATE_TEST(nickname),
  LOCAL_VALIDATE_TEST(contactinfo),
  LOCAL_VALIDATE_TEST(logs),
  LOCAL_VALIDATE_TEST(authdir),
  LOCAL_VALIDATE_TEST(relay_with_hidden_services),
  LOCAL_VALIDATE_TEST(transproxy),
  LOCAL_VALIDATE_TEST(exclude_nodes),
  LOCAL_VALIDATE_TEST(scheduler),
  LOCAL_VALIDATE_TEST(node_families),
  LOCAL_VALIDATE_TEST(tlsec),
  LOCAL_VALIDATE_TEST(token_bucket),
  LOCAL_VALIDATE_TEST(recommended_packages),
  LOCAL_VALIDATE_TEST(fetch_dir),
  LOCAL_VALIDATE_TEST(conn_limit),
  LOCAL_VALIDATE_TEST(paths_needed),
  LOCAL_VALIDATE_TEST(max_client_circuits),
  LOCAL_VALIDATE_TEST(ports),
  LOCAL_VALIDATE_TEST(reachable_addresses),
  LOCAL_VALIDATE_TEST(use_bridges),
  LOCAL_VALIDATE_TEST(entry_nodes),
  LOCAL_VALIDATE_TEST(invalid_nodes),
  LOCAL_VALIDATE_TEST(safe_logging),
  LOCAL_VALIDATE_TEST(publish_server_descriptor),
  LOCAL_VALIDATE_TEST(testing),
  LOCAL_VALIDATE_TEST(hidserv),
  LOCAL_VALIDATE_TEST(predicted_ports),
  LOCAL_VALIDATE_TEST(path_bias),
  LOCAL_VALIDATE_TEST(bandwidth),
  LOCAL_VALIDATE_TEST(circuits),
  LOCAL_VALIDATE_TEST(port_forwarding),
  LOCAL_VALIDATE_TEST(tor2web),
  LOCAL_VALIDATE_TEST(rend),
  LOCAL_VALIDATE_TEST(single_onion),
  LOCAL_VALIDATE_TEST(accounting),
  LOCAL_VALIDATE_TEST(proxy),
  LOCAL_VALIDATE_TEST(control),
  LOCAL_VALIDATE_TEST(families),
  LOCAL_VALIDATE_TEST(addr_policies),
  LOCAL_VALIDATE_TEST(dir_auth),
  LOCAL_VALIDATE_TEST(transport),
  LOCAL_VALIDATE_TEST(constrained_sockets),
  LOCAL_VALIDATE_TEST(v3_auth),
  LOCAL_VALIDATE_TEST(virtual_addr),
  LOCAL_VALIDATE_TEST(exits),
  LOCAL_VALIDATE_TEST(testing_options),
  LOCAL_VALIDATE_TEST(accel),
  END_OF_TESTCASES              /*  */
};

