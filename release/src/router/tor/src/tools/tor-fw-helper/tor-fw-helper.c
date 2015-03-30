/* Copyright (c) 2010, Jacob Appelbaum, Steven J. Murdoch.
 * Copyright (c) 2010-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file tor-fw-helper.c
 * \brief The main wrapper around our firewall helper logic.
 **/

/*
 * tor-fw-helper is a tool for opening firewalls with NAT-PMP and UPnP; this
 * tool is designed to be called by hand or by Tor by way of a exec() at a
 * later date.
 */

#include "orconfig.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "container.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "tor-fw-helper.h"
#ifdef NAT_PMP
#include "tor-fw-helper-natpmp.h"
#endif
#ifdef MINIUPNPC
#include "tor-fw-helper-upnp.h"
#endif

/** This is our meta storage type - it holds information about each helper
  including the total number of helper backends, function pointers, and helper
  state. */
typedef struct backends_t {
  /** The total number of backends */
  int n_backends;
  /** The backend functions as an array */
  tor_fw_backend_t backend_ops[MAX_BACKENDS];
  /** The internal backend state */
  void *backend_state[MAX_BACKENDS];
} backends_t;

/** Initialize each backend helper with the user input stored in <b>options</b>
 * and put the results in the <b>backends</b> struct. */
static int
init_backends(tor_fw_options_t *options, backends_t *backends)
{
  int n_available = 0;
  int i, r, n;
  tor_fw_backend_t *backend_ops_list[MAX_BACKENDS];
  void *data = NULL;
  /* First, build a list of the working backends. */
  n = 0;
#ifdef MINIUPNPC
  backend_ops_list[n++] = (tor_fw_backend_t *) tor_fw_get_miniupnp_backend();
#endif
#ifdef NAT_PMP
  backend_ops_list[n++] = (tor_fw_backend_t *) tor_fw_get_natpmp_backend();
#endif
  n_available = n;

  /* Now, for each backend that might work, try to initialize it.
   * That's how we roll, initialized.
   */
  n = 0;
  for (i=0; i<n_available; ++i) {
    data = calloc(1, backend_ops_list[i]->state_len);
    if (!data) {
      perror("calloc");
      exit(1);
    }
    r = backend_ops_list[i]->init(options, data);
    if (r == 0) {
      backends->backend_ops[n] = *backend_ops_list[i];
      backends->backend_state[n] = data;
      n++;
    } else {
      free(data);
    }
  }
  backends->n_backends = n;

  return n;
}

/** Return the proper commandline switches when the user needs information. */
static void
usage(void)
{
  fprintf(stderr, "tor-fw-helper usage:\n"
          " [-h|--help]\n"
          " [-T|--test-commandline]\n"
          " [-v|--verbose]\n"
          " [-g|--fetch-public-ip]\n"
          " [-p|--forward-port ([<external port>]:<internal port>)]\n");
}

/** Log commandline options to a hardcoded file <b>tor-fw-helper.log</b> in the
 * current working directory. */
static int
log_commandline_options(int argc, char **argv)
{
  int i, retval;
  FILE *logfile;
  time_t now;

  /* Open the log file */
  logfile = fopen("tor-fw-helper.log", "a");
  if (NULL == logfile)
    return -1;

  /* Send all commandline arguments to the file */
  now = time(NULL);
  retval = fprintf(logfile, "START: %s\n", ctime(&now));
  for (i = 0; i < argc; i++) {
    retval = fprintf(logfile, "ARG: %d: %s\n", i, argv[i]);
    if (retval < 0)
      goto error;

    retval = fprintf(stderr, "ARG: %d: %s\n", i, argv[i]);
    if (retval < 0)
      goto error;
  }
  now = time(NULL);
  retval = fprintf(logfile, "END: %s\n", ctime(&now));

  /* Close and clean up */
  retval = fclose(logfile);
  return retval;

  /* If there was an error during writing */
 error:
  fclose(logfile);
  return -1;
}

/** Iterate over over each of the supported <b>backends</b> and attempt to
 * fetch the public ip. */
static void
tor_fw_fetch_public_ip(tor_fw_options_t *tor_fw_options,
                       backends_t *backends)
{
  int i;
  int r = 0;

  if (tor_fw_options->verbose)
    fprintf(stderr, "V: tor_fw_fetch_public_ip\n");

  for (i=0; i<backends->n_backends; ++i) {
    if (tor_fw_options->verbose) {
        fprintf(stderr, "V: running backend_state now: %i\n", i);
        fprintf(stderr, "V: size of backend state: %u\n",
                (int)(backends->backend_ops)[i].state_len);
        fprintf(stderr, "V: backend state name: %s\n",
                (char *)(backends->backend_ops)[i].name);
      }
    r = backends->backend_ops[i].fetch_public_ip(tor_fw_options,
                                                 backends->backend_state[i]);
    fprintf(stderr, "tor-fw-helper: tor_fw_fetch_public_ip backend %s "
            " returned: %i\n", (char *)(backends->backend_ops)[i].name, r);
  }
}

/** Print a spec-conformant string to stdout describing the results of
 *  the TCP port forwarding operation from <b>external_port</b> to
 *  <b>internal_port</b>. */
static void
tor_fw_helper_report_port_fw_results(uint16_t internal_port,
                                     uint16_t external_port,
                                     int succeded,
                                     const char *message)
{
  char *report_string = NULL;

  tor_asprintf(&report_string, "%s %s %u %u %s %s\n",
               "tor-fw-helper",
               "tcp-forward",
               external_port, internal_port,
               succeded ? "SUCCESS" : "FAIL",
               message);
  fprintf(stdout, "%s", report_string);
  fflush(stdout);
  tor_free(report_string);
}

#define tor_fw_helper_report_port_fw_fail(i, e, m) \
  tor_fw_helper_report_port_fw_results((i), (e), 0, (m))

#define tor_fw_helper_report_port_fw_success(i, e, m) \
  tor_fw_helper_report_port_fw_results((i), (e), 1, (m))

/** Return a heap-allocated string containing the list of our
 *  backends. It can be used in log messages. Be sure to free it
 *  afterwards! */
static char *
get_list_of_backends_string(backends_t *backends)
{
  char *backend_names = NULL;
  int i;
  smartlist_t *backend_names_sl = smartlist_new();

  assert(backends->n_backends);

  for (i=0; i<backends->n_backends; ++i)
    smartlist_add(backend_names_sl, (char *) backends->backend_ops[i].name);

  backend_names = smartlist_join_strings(backend_names_sl, ", ", 0, NULL);
  smartlist_free(backend_names_sl);

  return backend_names;
}

/** Iterate over each of the supported <b>backends</b> and attempt to add a
 *  port forward for the port stored in <b>tor_fw_options</b>. */
static void
tor_fw_add_ports(tor_fw_options_t *tor_fw_options,
                 backends_t *backends)
{
  int i;
  int r = 0;
  int succeeded = 0;

  if (tor_fw_options->verbose)
    fprintf(stderr, "V: %s\n", __func__);

  /** Loop all ports that need to be forwarded, and try to use our
   *  backends for each port. If a backend succeeds, break the loop,
   *  report success and get to the next port. If all backends fail,
   *  report failure for that port. */
  SMARTLIST_FOREACH_BEGIN(tor_fw_options->ports_to_forward,
                          port_to_forward_t *, port_to_forward) {

    succeeded = 0;

    for (i=0; i<backends->n_backends; ++i) {
      if (tor_fw_options->verbose) {
        fprintf(stderr, "V: running backend_state now: %i\n", i);
        fprintf(stderr, "V: size of backend state: %u\n",
                (int)(backends->backend_ops)[i].state_len);
        fprintf(stderr, "V: backend state name: %s\n",
                (const char *) backends->backend_ops[i].name);
      }

      r =
       backends->backend_ops[i].add_tcp_mapping(port_to_forward->internal_port,
                                                port_to_forward->external_port,
                                                tor_fw_options->verbose,
                                                backends->backend_state[i]);
      if (r == 0) { /* backend success */
        tor_fw_helper_report_port_fw_success(port_to_forward->internal_port,
                                             port_to_forward->external_port,
                                             backends->backend_ops[i].name);
        succeeded = 1;
        break;
      }

      fprintf(stderr, "tor-fw-helper: tor_fw_add_port backend %s "
              "returned: %i\n",
              (const char *) backends->backend_ops[i].name, r);
    }

    if (!succeeded) { /* all backends failed */
      char *list_of_backends_str = get_list_of_backends_string(backends);
      char *fail_msg = NULL;
      tor_asprintf(&fail_msg, "All port forwarding backends (%s) failed.",
                   list_of_backends_str);
      tor_fw_helper_report_port_fw_fail(port_to_forward->internal_port,
                                        port_to_forward->external_port,
                                        fail_msg);
      tor_free(list_of_backends_str);
      tor_free(fail_msg);
    }

  } SMARTLIST_FOREACH_END(port_to_forward);
}

/** Called before we make any calls to network-related functions.
 * (Some operating systems require their network libraries to be
 * initialized.) (from common/compat.c) */
static int
tor_fw_helper_network_init(void)
{
#ifdef _WIN32
  /* This silly exercise is necessary before windows will allow
   * gethostbyname to work. */
  WSADATA WSAData;
  int r;
  r = WSAStartup(0x101, &WSAData);
  if (r) {
    fprintf(stderr, "E: Error initializing Windows network layer "
            "- code was %d", r);
    return -1;
  }
  /* WSAData.iMaxSockets might show the max sockets we're allowed to use.
   * We might use it to complain if we're trying to be a server but have
   * too few sockets available. */
#endif
  return 0;
}

/** Parse the '-p' argument of tor-fw-helper. Its format is
 *  [<external port>]:<internal port>, and <external port> is optional.
 *  Return NULL if <b>arg</b> was c0rrupted. */
static port_to_forward_t *
parse_port(const char *arg)
{
  smartlist_t *sl = smartlist_new();
  port_to_forward_t *port_to_forward = NULL;
  char *port_str = NULL;
  int ok;
  int port;

  smartlist_split_string(sl, arg, ":", 0, 0);
  if (smartlist_len(sl) != 2)
    goto err;

  port_to_forward = tor_malloc(sizeof(port_to_forward_t));
  if (!port_to_forward)
    goto err;

  port_str = smartlist_get(sl, 0); /* macroify ? */
  port = (int)tor_parse_long(port_str, 10, 1, 65535, &ok, NULL);
  if (!ok && strlen(port_str)) /* ":1555" is valid */
    goto err;
  port_to_forward->external_port = port;

  port_str = smartlist_get(sl, 1);
  port = (int)tor_parse_long(port_str, 10, 1, 65535, &ok, NULL);
  if (!ok)
    goto err;
  port_to_forward->internal_port = port;

  goto done;

 err:
  tor_free(port_to_forward);

 done:
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);

  return port_to_forward;
}

/** Report a failure of epic proportions: We didn't manage to
 *  initialize any port forwarding backends. */
static void
report_full_fail(const smartlist_t *ports_to_forward)
{
  if (!ports_to_forward)
    return;

  SMARTLIST_FOREACH_BEGIN(ports_to_forward,
                          const port_to_forward_t *, port_to_forward) {
    tor_fw_helper_report_port_fw_fail(port_to_forward->internal_port,
                                      port_to_forward->external_port,
                                      "All backends (NAT-PMP, UPnP) failed "
                                      "to initialize!"); /* XXX hardcoded */
  } SMARTLIST_FOREACH_END(port_to_forward);
}

int
main(int argc, char **argv)
{
  int r = 0;
  int c = 0;

  tor_fw_options_t tor_fw_options;
  backends_t backend_state;

  memset(&tor_fw_options, 0, sizeof(tor_fw_options));
  memset(&backend_state, 0, sizeof(backend_state));

  // Parse CLI arguments.
  while (1) {
    int option_index = 0;
    static struct option long_options[] =
      {
        {"verbose", 0, 0, 'v'},
        {"help", 0, 0, 'h'},
        {"port", 1, 0, 'p'},
        {"fetch-public-ip", 0, 0, 'g'},
        {"test-commandline", 0, 0, 'T'},
        {0, 0, 0, 0}
      };

    c = getopt_long(argc, argv, "vhp:gT",
                    long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'v': tor_fw_options.verbose = 1; break;
      case 'h': tor_fw_options.help = 1; usage(); exit(1); break;
      case 'p': {
        port_to_forward_t *port_to_forward = parse_port(optarg);
        if (!port_to_forward) {
          fprintf(stderr, "E: Failed to parse '%s'.\n", optarg);
          usage();
          exit(1);
        }

        /* If no external port was given (it's optional), set it to be
         * equal with the internal port. */
        if (!port_to_forward->external_port) {
          assert(port_to_forward->internal_port);
          if (tor_fw_options.verbose)
            fprintf(stderr, "V: No external port was given. Setting to %u.\n",
                    port_to_forward->internal_port);
          port_to_forward->external_port = port_to_forward->internal_port;
        }

        if (!tor_fw_options.ports_to_forward)
          tor_fw_options.ports_to_forward = smartlist_new();

        smartlist_add(tor_fw_options.ports_to_forward, port_to_forward);

        break;
      }
      case 'g': tor_fw_options.fetch_public_ip = 1; break;
      case 'T': tor_fw_options.test_commandline = 1; break;
      case '?': break;
      default : fprintf(stderr, "Unknown option!\n"); usage(); exit(1);
    }
  }

  { // Verbose output

    if (tor_fw_options.verbose)
      fprintf(stderr, "V: tor-fw-helper version %s\n"
              "V: We were called with the following arguments:\n"
              "V: verbose = %d, help = %d, fetch_public_ip = %u\n",
              tor_fw_version, tor_fw_options.verbose, tor_fw_options.help,
              tor_fw_options.fetch_public_ip);

    if (tor_fw_options.verbose && tor_fw_options.ports_to_forward) {
      fprintf(stderr, "V: TCP forwarding:\n");
      SMARTLIST_FOREACH(tor_fw_options.ports_to_forward,
                        const port_to_forward_t *, port_to_forward,
                        fprintf(stderr, "V: External: %u, Internal: %u\n",
                                port_to_forward->external_port,
                                port_to_forward->internal_port));
    }
  }

  if (tor_fw_options.test_commandline) {
    return log_commandline_options(argc, argv);
  }

  // See if the user actually wants us to do something.
  if (!tor_fw_options.fetch_public_ip && !tor_fw_options.ports_to_forward) {
    fprintf(stderr, "E: We require a port to be forwarded or "
            "fetch_public_ip request!\n");
    usage();
    exit(1);
  }

  // Initialize networking
  if (tor_fw_helper_network_init())
    exit(1);

  // Initalize the various fw-helper backend helpers
  r = init_backends(&tor_fw_options, &backend_state);
  if (!r) { // all backends failed:
    // report our failure
    report_full_fail(tor_fw_options.ports_to_forward);
    fprintf(stderr, "tor-fw-helper: All backends failed.\n");
    exit(1);
  } else { // some backends succeeded:
    fprintf(stderr, "tor-fw-helper: %i NAT traversal helper(s) loaded\n", r);
  }

  // Forward TCP ports.
  if (tor_fw_options.ports_to_forward) {
    tor_fw_add_ports(&tor_fw_options, &backend_state);
  }

  // Fetch our public IP.
  if (tor_fw_options.fetch_public_ip) {
    tor_fw_fetch_public_ip(&tor_fw_options, &backend_state);
  }

  // Cleanup and exit.
  if (tor_fw_options.ports_to_forward) {
    SMARTLIST_FOREACH(tor_fw_options.ports_to_forward,
                      port_to_forward_t *, port,
                      tor_free(port));
    smartlist_free(tor_fw_options.ports_to_forward);
  }

  exit(0);
}

