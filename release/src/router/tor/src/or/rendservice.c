/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendservice.c
 * \brief The hidden-service side of rendezvous functionality.
 **/

#define RENDSERVICE_PRIVATE

#include "or.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "config.h"
#include "control.h"
#include "directory.h"
#include "main.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "rendclient.h"
#include "rendcommon.h"
#include "rendservice.h"
#include "router.h"
#include "relay.h"
#include "rephist.h"
#include "replaycache.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"

struct rend_service_t;
static origin_circuit_t *find_intro_circuit(rend_intro_point_t *intro,
                                            const char *pk_digest);
static rend_intro_point_t *find_intro_point(origin_circuit_t *circ);
static rend_intro_point_t *find_expiring_intro_point(
    struct rend_service_t *service, origin_circuit_t *circ);

static extend_info_t *find_rp_for_intro(
    const rend_intro_cell_t *intro,
    char **err_msg_out);

static int intro_point_accepted_intro_count(rend_intro_point_t *intro);
static int intro_point_should_expire_now(rend_intro_point_t *intro,
                                         time_t now);
static int rend_service_derive_key_digests(struct rend_service_t *s);
static int rend_service_load_keys(struct rend_service_t *s);
static int rend_service_load_auth_keys(struct rend_service_t *s,
                                       const char *hfname);
static struct rend_service_t *rend_service_get_by_pk_digest(
    const char* digest);
static struct rend_service_t *rend_service_get_by_service_id(const char *id);
static const char *rend_service_escaped_dir(
    const struct rend_service_t *s);

static ssize_t rend_service_parse_intro_for_v0_or_v1(
    rend_intro_cell_t *intro,
    const uint8_t *buf,
    size_t plaintext_len,
    char **err_msg_out);
static ssize_t rend_service_parse_intro_for_v2(
    rend_intro_cell_t *intro,
    const uint8_t *buf,
    size_t plaintext_len,
    char **err_msg_out);
static ssize_t rend_service_parse_intro_for_v3(
    rend_intro_cell_t *intro,
    const uint8_t *buf,
    size_t plaintext_len,
    char **err_msg_out);

static int rend_service_check_private_dir(const or_options_t *options,
                                          const rend_service_t *s,
                                          int create);
static int rend_service_check_private_dir_impl(const or_options_t *options,
                                               const rend_service_t *s,
                                               int create);

/** Represents the mapping from a virtual port of a rendezvous service to
 * a real port on some IP.
 */
struct rend_service_port_config_s {
  /* The incoming HS virtual port we're mapping */
  uint16_t virtual_port;
  /* Is this an AF_UNIX port? */
  unsigned int is_unix_addr:1;
  /* The outgoing TCP port to use, if !is_unix_addr */
  uint16_t real_port;
  /* The outgoing IPv4 or IPv6 address to use, if !is_unix_addr */
  tor_addr_t real_addr;
  /* The socket path to connect to, if is_unix_addr */
  char unix_addr[FLEXIBLE_ARRAY_MEMBER];
};

/** Try to maintain this many intro points per service by default. */
#define NUM_INTRO_POINTS_DEFAULT 3
/** Maximum number of intro points per service. */
#define NUM_INTRO_POINTS_MAX 10
/** Number of extra intro points we launch if our set of intro nodes is
 * empty. See proposal 155, section 4. */
#define NUM_INTRO_POINTS_EXTRA 2

/** If we can't build our intro circuits, don't retry for this long. */
#define INTRO_CIRC_RETRY_PERIOD (60*5)
/** Don't try to build more than this many circuits before giving up
 * for a while.*/
#define MAX_INTRO_CIRCS_PER_PERIOD 10
/** How many times will a hidden service operator attempt to connect to
 * a requested rendezvous point before giving up? */
#define MAX_REND_FAILURES 1
/** How many seconds should we spend trying to connect to a requested
 * rendezvous point before giving up? */
#define MAX_REND_TIMEOUT 30

/* Hidden service directory file names:
 * new file names should be added to rend_service_add_filenames_to_list()
 * for sandboxing purposes. */
static const char *private_key_fname = "private_key";
static const char *hostname_fname = "hostname";
static const char *client_keys_fname = "client_keys";
static const char *sos_poison_fname = "onion_service_non_anonymous";

/** Returns a escaped string representation of the service, <b>s</b>.
 */
static const char *
rend_service_escaped_dir(const struct rend_service_t *s)
{
  return (s->directory) ? escaped(s->directory) : "[EPHEMERAL]";
}

/** A list of rend_service_t's for services run on this OP.
 */
static smartlist_t *rend_service_list = NULL;

/** Return the number of rendezvous services we have configured. */
int
num_rend_services(void)
{
  if (!rend_service_list)
    return 0;
  return smartlist_len(rend_service_list);
}

/** Helper: free storage held by a single service authorized client entry. */
void
rend_authorized_client_free(rend_authorized_client_t *client)
{
  if (!client)
    return;
  if (client->client_key)
    crypto_pk_free(client->client_key);
  if (client->client_name)
    memwipe(client->client_name, 0, strlen(client->client_name));
  tor_free(client->client_name);
  memwipe(client->descriptor_cookie, 0, sizeof(client->descriptor_cookie));
  tor_free(client);
}

/** Helper for strmap_free. */
static void
rend_authorized_client_strmap_item_free(void *authorized_client)
{
  rend_authorized_client_free(authorized_client);
}

/** Release the storage held by <b>service</b>.
 */
STATIC void
rend_service_free(rend_service_t *service)
{
  if (!service)
    return;

  tor_free(service->directory);
  if (service->ports) {
    SMARTLIST_FOREACH(service->ports, rend_service_port_config_t*, p,
                      rend_service_port_config_free(p));
    smartlist_free(service->ports);
  }
  if (service->private_key)
    crypto_pk_free(service->private_key);
  if (service->intro_nodes) {
    SMARTLIST_FOREACH(service->intro_nodes, rend_intro_point_t *, intro,
      rend_intro_point_free(intro););
    smartlist_free(service->intro_nodes);
  }
  if (service->expiring_nodes) {
    SMARTLIST_FOREACH(service->expiring_nodes, rend_intro_point_t *, intro,
                      rend_intro_point_free(intro););
    smartlist_free(service->expiring_nodes);
  }

  rend_service_descriptor_free(service->desc);
  if (service->clients) {
    SMARTLIST_FOREACH(service->clients, rend_authorized_client_t *, c,
      rend_authorized_client_free(c););
    smartlist_free(service->clients);
  }
  if (service->accepted_intro_dh_parts) {
    replaycache_free(service->accepted_intro_dh_parts);
  }
  tor_free(service);
}

/** Release all the storage held in rend_service_list.
 */
void
rend_service_free_all(void)
{
  if (!rend_service_list)
    return;

  SMARTLIST_FOREACH(rend_service_list, rend_service_t*, ptr,
                    rend_service_free(ptr));
  smartlist_free(rend_service_list);
  rend_service_list = NULL;
}

/** Validate <b>service</b> and add it to <b>service_list</b>, or to
 * the global rend_service_list if <b>service_list</b> is NULL.
 * Return 0 on success.  On failure, free <b>service</b> and return -1.
 * Takes ownership of <b>service</b>.
 */
static int
rend_add_service(smartlist_t *service_list, rend_service_t *service)
{
  int i;
  rend_service_port_config_t *p;

  smartlist_t *s_list;
  /* If no special service list is provided, then just use the global one. */
  if (!service_list) {
    if (BUG(!rend_service_list)) {
      /* No global HS list, which is a failure. */
      return -1;
    }

    s_list = rend_service_list;
  } else {
    s_list = service_list;
  }

  service->intro_nodes = smartlist_new();
  service->expiring_nodes = smartlist_new();

  if (service->max_streams_per_circuit < 0) {
    log_warn(LD_CONFIG, "Hidden service (%s) configured with negative max "
                        "streams per circuit; ignoring.",
             rend_service_escaped_dir(service));
    rend_service_free(service);
    return -1;
  }

  if (service->max_streams_close_circuit < 0 ||
      service->max_streams_close_circuit > 1) {
    log_warn(LD_CONFIG, "Hidden service (%s) configured with invalid "
                        "max streams handling; ignoring.",
             rend_service_escaped_dir(service));
    rend_service_free(service);
    return -1;
  }

  if (service->auth_type != REND_NO_AUTH &&
      (!service->clients ||
       smartlist_len(service->clients) == 0)) {
    log_warn(LD_CONFIG, "Hidden service (%s) with client authorization but no "
                        "clients; ignoring.",
             rend_service_escaped_dir(service));
    rend_service_free(service);
    return -1;
  }

  if (!service->ports || !smartlist_len(service->ports)) {
    log_warn(LD_CONFIG, "Hidden service (%s) with no ports configured; "
             "ignoring.",
             rend_service_escaped_dir(service));
    rend_service_free(service);
    return -1;
  } else {
    int dupe = 0;
    /* XXX This duplicate check has two problems:
     *
     * a) It's O(n^2), but the same comment from the bottom of
     *    rend_config_services() should apply.
     *
     * b) We only compare directory paths as strings, so we can't
     *    detect two distinct paths that specify the same directory
     *    (which can arise from symlinks, case-insensitivity, bind
     *    mounts, etc.).
     *
     * It also can't detect that two separate Tor instances are trying
     * to use the same HiddenServiceDir; for that, we would need a
     * lock file.  But this is enough to detect a simple mistake that
     * at least one person has actually made.
     */
    if (service->directory != NULL) {
      /* Skip dupe for ephemeral services. */
      SMARTLIST_FOREACH(s_list, rend_service_t*, ptr,
                        dupe = dupe ||
                               !strcmp(ptr->directory, service->directory));
      if (dupe) {
        log_warn(LD_REND, "Another hidden service is already configured for "
                 "directory %s, ignoring.",
                 rend_service_escaped_dir(service));
        rend_service_free(service);
        return -1;
      }
    }
    smartlist_add(s_list, service);
    log_debug(LD_REND,"Configuring service with directory \"%s\"",
              service->directory);
    for (i = 0; i < smartlist_len(service->ports); ++i) {
      p = smartlist_get(service->ports, i);
      if (!(p->is_unix_addr)) {
        log_debug(LD_REND,
                  "Service maps port %d to %s",
                  p->virtual_port,
                  fmt_addrport(&p->real_addr, p->real_port));
      } else {
#ifdef HAVE_SYS_UN_H
        log_debug(LD_REND,
                  "Service maps port %d to socket at \"%s\"",
                  p->virtual_port, p->unix_addr);
#else
        log_debug(LD_REND,
                  "Service maps port %d to an AF_UNIX socket, but we "
                  "have no AF_UNIX support on this platform.  This is "
                  "probably a bug.",
                  p->virtual_port);
#endif /* defined(HAVE_SYS_UN_H) */
      }
    }
    return 0;
  }
  /* NOTREACHED */
}

/** Return a new rend_service_port_config_t with its path set to
 * <b>socket_path</b> or empty if <b>socket_path</b> is NULL */
static rend_service_port_config_t *
rend_service_port_config_new(const char *socket_path)
{
  if (!socket_path)
    return tor_malloc_zero(sizeof(rend_service_port_config_t) + 1);

  const size_t pathlen = strlen(socket_path) + 1;
  rend_service_port_config_t *conf =
    tor_malloc_zero(sizeof(rend_service_port_config_t) + pathlen);
  memcpy(conf->unix_addr, socket_path, pathlen);
  conf->is_unix_addr = 1;
  return conf;
}

/** Parses a real-port to virtual-port mapping separated by the provided
 * separator and returns a new rend_service_port_config_t, or NULL and an
 * optional error string on failure.
 *
 * The format is: VirtualPort SEP (IP|RealPort|IP:RealPort|'socket':path)?
 *
 * IP defaults to 127.0.0.1; RealPort defaults to VirtualPort.
 */
rend_service_port_config_t *
rend_service_parse_port_config(const char *string, const char *sep,
                               char **err_msg_out)
{
  smartlist_t *sl;
  int virtport;
  int realport = 0;
  uint16_t p;
  tor_addr_t addr;
  rend_service_port_config_t *result = NULL;
  unsigned int is_unix_addr = 0;
  const char *socket_path = NULL;
  char *err_msg = NULL;
  char *addrport = NULL;

  sl = smartlist_new();
  smartlist_split_string(sl, string, sep,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 2);
  if (smartlist_len(sl) < 1 || BUG(smartlist_len(sl) > 2)) {
    if (err_msg_out)
      err_msg = tor_strdup("Bad syntax in hidden service port configuration.");
    goto err;
  }
  virtport = (int)tor_parse_long(smartlist_get(sl,0), 10, 1, 65535, NULL,NULL);
  if (!virtport) {
    if (err_msg_out)
      tor_asprintf(&err_msg, "Missing or invalid port %s in hidden service "
                   "port configuration", escaped(smartlist_get(sl,0)));

    goto err;
  }
  if (smartlist_len(sl) == 1) {
    /* No addr:port part; use default. */
    realport = virtport;
    tor_addr_from_ipv4h(&addr, 0x7F000001u); /* 127.0.0.1 */
  } else {
    int ret;

    const char *addrport_element = smartlist_get(sl,1);
    const char *rest = NULL;
    int is_unix;
    ret = port_cfg_line_extract_addrport(addrport_element, &addrport,
                                         &is_unix, &rest);
    if (ret < 0) {
      tor_asprintf(&err_msg, "Couldn't process address <%s> from hidden "
                   "service configuration", addrport_element);
      goto err;
    }
    if (is_unix) {
      socket_path = addrport;
      is_unix_addr = 1;
    } else if (strchr(addrport, ':') || strchr(addrport, '.')) {
      /* else try it as an IP:port pair if it has a : or . in it */
      if (tor_addr_port_lookup(addrport, &addr, &p)<0) {
        if (err_msg_out)
          err_msg = tor_strdup("Unparseable address in hidden service port "
                               "configuration.");

        goto err;
      }
      realport = p?p:virtport;
    } else {
      /* No addr:port, no addr -- must be port. */
      realport = (int)tor_parse_long(addrport, 10, 1, 65535, NULL, NULL);
      if (!realport) {
        if (err_msg_out)
          tor_asprintf(&err_msg, "Unparseable or out-of-range port %s in "
                       "hidden service port configuration.",
                       escaped(addrport));

        goto err;
      }
      tor_addr_from_ipv4h(&addr, 0x7F000001u); /* Default to 127.0.0.1 */
    }
  }

  /* Allow room for unix_addr */
  result = rend_service_port_config_new(socket_path);
  result->virtual_port = virtport;
  result->is_unix_addr = is_unix_addr;
  if (!is_unix_addr) {
    result->real_port = realport;
    tor_addr_copy(&result->real_addr, &addr);
    result->unix_addr[0] = '\0';
  }

 err:
  tor_free(addrport);
  if (err_msg_out) *err_msg_out = err_msg;
  SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
  smartlist_free(sl);

  return result;
}

/** Release all storage held in a rend_service_port_config_t. */
void
rend_service_port_config_free(rend_service_port_config_t *p)
{
  tor_free(p);
}

/* Check the directory for <b>service</b>, and add the service to
 * <b>service_list</b>, or to the global list if <b>service_list</b> is NULL.
 * Only add the service to the list if <b>validate_only</b> is false.
 * If <b>validate_only</b> is true, free the service.
 * If <b>service</b> is NULL, ignore it, and return 0.
 * Returns 0 on success, and -1 on failure.
 * Takes ownership of <b>service</b>, either freeing it, or adding it to the
 * global service list.
 */
STATIC int
rend_service_check_dir_and_add(smartlist_t *service_list,
                               const or_options_t *options,
                               rend_service_t *service,
                               int validate_only)
{
  if (!service) {
    /* It is ok for a service to be NULL, this means there are no services */
    return 0;
  }

  if (rend_service_check_private_dir(options, service, !validate_only)
      < 0) {
    rend_service_free(service);
    return -1;
  }

  if (validate_only) {
    rend_service_free(service);
    return 0;
  } else {
    /* Use service_list for unit tests */
    smartlist_t *s_list = NULL;
    /* If no special service list is provided, then just use the global one. */
    if (!service_list) {
      if (BUG(!rend_service_list)) {
        /* No global HS list, which is a failure, because we plan on adding to
         * it */
        return -1;
      }
      s_list = rend_service_list;
    } else {
      s_list = service_list;
    }
    /* s_list can not be NULL here - if both service_list and rend_service_list
     * are NULL, and validate_only is false, we exit earlier in the function
     */
    if (BUG(!s_list)) {
      return -1;
    }
    /* Ignore service failures until 030 */
    rend_add_service(s_list, service);
    return 0;
  }
}

/** Set up rend_service_list, based on the values of HiddenServiceDir and
 * HiddenServicePort in <b>options</b>.  Return 0 on success and -1 on
 * failure.  (If <b>validate_only</b> is set, parse, warn and return as
 * normal, but don't actually change the configured services.)
 */
int
rend_config_services(const or_options_t *options, int validate_only)
{
  config_line_t *line;
  rend_service_t *service = NULL;
  rend_service_port_config_t *portcfg;
  smartlist_t *old_service_list = NULL;
  int ok = 0;

  if (!validate_only) {
    old_service_list = rend_service_list;
    rend_service_list = smartlist_new();
  }

  for (line = options->RendConfigLines; line; line = line->next) {
    if (!strcasecmp(line->key, "HiddenServiceDir")) {
      /* register the service we just finished parsing
       * this code registers every service except the last one parsed,
       * which is registered below the loop */
      if (rend_service_check_dir_and_add(NULL, options, service,
                                         validate_only) < 0) {
          return -1;
      }
      service = tor_malloc_zero(sizeof(rend_service_t));
      service->directory = tor_strdup(line->value);
      service->ports = smartlist_new();
      service->intro_period_started = time(NULL);
      service->n_intro_points_wanted = NUM_INTRO_POINTS_DEFAULT;
      continue;
    }
    if (!service) {
      log_warn(LD_CONFIG, "%s with no preceding HiddenServiceDir directive",
               line->key);
      rend_service_free(service);
      return -1;
    }
    if (!strcasecmp(line->key, "HiddenServicePort")) {
      char *err_msg = NULL;
      portcfg = rend_service_parse_port_config(line->value, " ", &err_msg);
      if (!portcfg) {
        if (err_msg)
          log_warn(LD_CONFIG, "%s", err_msg);
        tor_free(err_msg);
        rend_service_free(service);
        return -1;
      }
      tor_assert(!err_msg);
      smartlist_add(service->ports, portcfg);
    } else if (!strcasecmp(line->key, "HiddenServiceAllowUnknownPorts")) {
      service->allow_unknown_ports = (int)tor_parse_long(line->value,
                                                        10, 0, 1, &ok, NULL);
      if (!ok) {
        log_warn(LD_CONFIG,
                 "HiddenServiceAllowUnknownPorts should be 0 or 1, not %s",
                 line->value);
        rend_service_free(service);
        return -1;
      }
      log_info(LD_CONFIG,
               "HiddenServiceAllowUnknownPorts=%d for %s",
               (int)service->allow_unknown_ports, service->directory);
    } else if (!strcasecmp(line->key,
                           "HiddenServiceDirGroupReadable")) {
        service->dir_group_readable = (int)tor_parse_long(line->value,
                                                        10, 0, 1, &ok, NULL);
        if (!ok) {
            log_warn(LD_CONFIG,
                     "HiddenServiceDirGroupReadable should be 0 or 1, not %s",
                     line->value);
            rend_service_free(service);
            return -1;
        }
        log_info(LD_CONFIG,
                 "HiddenServiceDirGroupReadable=%d for %s",
                 service->dir_group_readable, service->directory);
    } else if (!strcasecmp(line->key, "HiddenServiceMaxStreams")) {
      service->max_streams_per_circuit = (int)tor_parse_long(line->value,
                                                    10, 0, 65535, &ok, NULL);
      if (!ok) {
        log_warn(LD_CONFIG,
                 "HiddenServiceMaxStreams should be between 0 and %d, not %s",
                 65535, line->value);
        rend_service_free(service);
        return -1;
      }
      log_info(LD_CONFIG,
               "HiddenServiceMaxStreams=%d for %s",
               service->max_streams_per_circuit, service->directory);
    } else if (!strcasecmp(line->key, "HiddenServiceMaxStreamsCloseCircuit")) {
      service->max_streams_close_circuit = (int)tor_parse_long(line->value,
                                                        10, 0, 1, &ok, NULL);
      if (!ok) {
        log_warn(LD_CONFIG,
                 "HiddenServiceMaxStreamsCloseCircuit should be 0 or 1, "
                 "not %s",
                 line->value);
        rend_service_free(service);
        return -1;
      }
      log_info(LD_CONFIG,
               "HiddenServiceMaxStreamsCloseCircuit=%d for %s",
               (int)service->max_streams_close_circuit, service->directory);
    } else if (!strcasecmp(line->key, "HiddenServiceNumIntroductionPoints")) {
      service->n_intro_points_wanted =
        (unsigned int) tor_parse_long(line->value, 10,
                                      NUM_INTRO_POINTS_DEFAULT,
                                      NUM_INTRO_POINTS_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_CONFIG,
                 "HiddenServiceNumIntroductionPoints "
                 "should be between %d and %d, not %s",
                 NUM_INTRO_POINTS_DEFAULT, NUM_INTRO_POINTS_MAX,
                 line->value);
        rend_service_free(service);
        return -1;
      }
      log_info(LD_CONFIG, "HiddenServiceNumIntroductionPoints=%d for %s",
               service->n_intro_points_wanted, service->directory);
    } else if (!strcasecmp(line->key, "HiddenServiceAuthorizeClient")) {
      /* Parse auth type and comma-separated list of client names and add a
       * rend_authorized_client_t for each client to the service's list
       * of authorized clients. */
      smartlist_t *type_names_split, *clients;
      const char *authname;
      int num_clients;
      if (service->auth_type != REND_NO_AUTH) {
        log_warn(LD_CONFIG, "Got multiple HiddenServiceAuthorizeClient "
                 "lines for a single service.");
        rend_service_free(service);
        return -1;
      }
      type_names_split = smartlist_new();
      smartlist_split_string(type_names_split, line->value, " ", 0, 2);
      if (smartlist_len(type_names_split) < 1) {
        log_warn(LD_BUG, "HiddenServiceAuthorizeClient has no value. This "
                         "should have been prevented when parsing the "
                         "configuration.");
        smartlist_free(type_names_split);
        rend_service_free(service);
        return -1;
      }
      authname = smartlist_get(type_names_split, 0);
      if (!strcasecmp(authname, "basic")) {
        service->auth_type = REND_BASIC_AUTH;
      } else if (!strcasecmp(authname, "stealth")) {
        service->auth_type = REND_STEALTH_AUTH;
      } else {
        log_warn(LD_CONFIG, "HiddenServiceAuthorizeClient contains "
                 "unrecognized auth-type '%s'. Only 'basic' or 'stealth' "
                 "are recognized.",
                 (char *) smartlist_get(type_names_split, 0));
        SMARTLIST_FOREACH(type_names_split, char *, cp, tor_free(cp));
        smartlist_free(type_names_split);
        rend_service_free(service);
        return -1;
      }
      service->clients = smartlist_new();
      if (smartlist_len(type_names_split) < 2) {
        log_warn(LD_CONFIG, "HiddenServiceAuthorizeClient contains "
                            "auth-type '%s', but no client names.",
                 service->auth_type == REND_BASIC_AUTH ? "basic" : "stealth");
        SMARTLIST_FOREACH(type_names_split, char *, cp, tor_free(cp));
        smartlist_free(type_names_split);
        continue;
      }
      clients = smartlist_new();
      smartlist_split_string(clients, smartlist_get(type_names_split, 1),
                             ",", SPLIT_SKIP_SPACE, 0);
      SMARTLIST_FOREACH(type_names_split, char *, cp, tor_free(cp));
      smartlist_free(type_names_split);
      /* Remove duplicate client names. */
      num_clients = smartlist_len(clients);
      smartlist_sort_strings(clients);
      smartlist_uniq_strings(clients);
      if (smartlist_len(clients) < num_clients) {
        log_info(LD_CONFIG, "HiddenServiceAuthorizeClient contains %d "
                            "duplicate client name(s); removing.",
                 num_clients - smartlist_len(clients));
        num_clients = smartlist_len(clients);
      }
      SMARTLIST_FOREACH_BEGIN(clients, const char *, client_name)
      {
        rend_authorized_client_t *client;
        if (!rend_valid_client_name(client_name)) {
          log_warn(LD_CONFIG, "HiddenServiceAuthorizeClient contains an "
                              "illegal client name: '%s'. Names must be "
                              "between 1 and %d characters and contain "
                              "only [A-Za-z0-9+_-].",
                   client_name, REND_CLIENTNAME_MAX_LEN);
          SMARTLIST_FOREACH(clients, char *, cp, tor_free(cp));
          smartlist_free(clients);
          rend_service_free(service);
          return -1;
        }
        client = tor_malloc_zero(sizeof(rend_authorized_client_t));
        client->client_name = tor_strdup(client_name);
        smartlist_add(service->clients, client);
        log_debug(LD_REND, "Adding client name '%s'", client_name);
      }
      SMARTLIST_FOREACH_END(client_name);
      SMARTLIST_FOREACH(clients, char *, cp, tor_free(cp));
      smartlist_free(clients);
      /* Ensure maximum number of clients. */
      if ((service->auth_type == REND_BASIC_AUTH &&
            smartlist_len(service->clients) > 512) ||
          (service->auth_type == REND_STEALTH_AUTH &&
            smartlist_len(service->clients) > 16)) {
        log_warn(LD_CONFIG, "HiddenServiceAuthorizeClient contains %d "
                            "client authorization entries, but only a "
                            "maximum of %d entries is allowed for "
                            "authorization type '%s'.",
                 smartlist_len(service->clients),
                 service->auth_type == REND_BASIC_AUTH ? 512 : 16,
                 service->auth_type == REND_BASIC_AUTH ? "basic" : "stealth");
        rend_service_free(service);
        return -1;
      }
    } else {
      tor_assert(!strcasecmp(line->key, "HiddenServiceVersion"));
      if (strcmp(line->value, "2")) {
        log_warn(LD_CONFIG,
                 "The only supported HiddenServiceVersion is 2.");
        rend_service_free(service);
        return -1;
      }
    }
  }
  /* register the final service after we have finished parsing all services
   * this code only registers the last service, other services are registered
   * within the loop. It is ok for this service to be NULL, it is ignored. */
  if (rend_service_check_dir_and_add(NULL, options, service,
                                     validate_only) < 0) {
    return -1;
  }

  /* If this is a reload and there were hidden services configured before,
   * keep the introduction points that are still needed and close the
   * other ones. */
  if (old_service_list && !validate_only) {
    smartlist_t *surviving_services = smartlist_new();

    /* Preserve the existing ephemeral services.
     *
     * This is the ephemeral service equivalent of the "Copy introduction
     * points to new services" block, except there's no copy required since
     * the service structure isn't regenerated.
     *
     * After this is done, all ephemeral services will be:
     *  * Removed from old_service_list, so the equivalent non-ephemeral code
     *    will not attempt to preserve them.
     *  * Added to the new rend_service_list (that previously only had the
     *    services listed in the configuration).
     *  * Added to surviving_services, which is the list of services that
     *    will NOT have their intro point closed.
     */
    SMARTLIST_FOREACH(old_service_list, rend_service_t *, old, {
      if (!old->directory) {
        SMARTLIST_DEL_CURRENT(old_service_list, old);
        smartlist_add(surviving_services, old);
        smartlist_add(rend_service_list, old);
      }
    });

    /* Copy introduction points to new services. */
    /* XXXX This is O(n^2), but it's only called on reconfigure, so it's
     * probably ok? */
    SMARTLIST_FOREACH_BEGIN(rend_service_list, rend_service_t *, new) {
      SMARTLIST_FOREACH_BEGIN(old_service_list, rend_service_t *, old) {
        if (new->directory && old->directory &&
            !strcmp(old->directory, new->directory)) {
          smartlist_add_all(new->intro_nodes, old->intro_nodes);
          smartlist_clear(old->intro_nodes);
          smartlist_add_all(new->expiring_nodes, old->expiring_nodes);
          smartlist_clear(old->expiring_nodes);
          smartlist_add(surviving_services, old);
          break;
        }
      } SMARTLIST_FOREACH_END(old);
    } SMARTLIST_FOREACH_END(new);

    /* Close introduction circuits of services we don't serve anymore. */
    /* XXXX it would be nicer if we had a nicer abstraction to use here,
     * so we could just iterate over the list of services to close, but
     * once again, this isn't critical-path code. */
    SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
      if (!circ->marked_for_close &&
          circ->state == CIRCUIT_STATE_OPEN &&
          (circ->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
           circ->purpose == CIRCUIT_PURPOSE_S_INTRO)) {
        origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(circ);
        int keep_it = 0;
        tor_assert(oc->rend_data);
        SMARTLIST_FOREACH(surviving_services, rend_service_t *, ptr, {
          if (tor_memeq(ptr->pk_digest, oc->rend_data->rend_pk_digest,
                      DIGEST_LEN)) {
            keep_it = 1;
            break;
          }
        });
        if (keep_it)
          continue;
        log_info(LD_REND, "Closing intro point %s for service %s.",
                 safe_str_client(extend_info_describe(
                                            oc->build_state->chosen_exit)),
                 oc->rend_data->onion_address);
        circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
        /* XXXX Is there another reason we should use here? */
      }
    }
    SMARTLIST_FOREACH_END(circ);
    smartlist_free(surviving_services);
    SMARTLIST_FOREACH(old_service_list, rend_service_t *, ptr,
                      rend_service_free(ptr));
    smartlist_free(old_service_list);
  }

  return 0;
}

/** Add the ephemeral service <b>pk</b>/<b>ports</b> if possible, using
 * client authorization <b>auth_type</b> and an optional list of
 * rend_authorized_client_t in <b>auth_clients</b>, with
 * <b>max_streams_per_circuit</b> streams allowed per rendezvous circuit,
 * and circuit closure on max streams being exceeded set by
 * <b>max_streams_close_circuit</b>.
 *
 * Ownership of pk, ports, and auth_clients is passed to this routine.
 * Regardless of success/failure, callers should not touch these values
 * after calling this routine, and may assume that correct cleanup has
 * been done on failure.
 *
 * Return an appropriate rend_service_add_ephemeral_status_t.
 */
rend_service_add_ephemeral_status_t
rend_service_add_ephemeral(crypto_pk_t *pk,
                           smartlist_t *ports,
                           int max_streams_per_circuit,
                           int max_streams_close_circuit,
                           rend_auth_type_t auth_type,
                           smartlist_t *auth_clients,
                           char **service_id_out)
{
  *service_id_out = NULL;
  /* Allocate the service structure, and initialize the key, and key derived
   * parameters.
   */
  rend_service_t *s = tor_malloc_zero(sizeof(rend_service_t));
  s->directory = NULL; /* This indicates the service is ephemeral. */
  s->private_key = pk;
  s->auth_type = auth_type;
  s->clients = auth_clients;
  s->ports = ports;
  s->intro_period_started = time(NULL);
  s->n_intro_points_wanted = NUM_INTRO_POINTS_DEFAULT;
  s->max_streams_per_circuit = max_streams_per_circuit;
  s->max_streams_close_circuit = max_streams_close_circuit;
  if (rend_service_derive_key_digests(s) < 0) {
    rend_service_free(s);
    return RSAE_BADPRIVKEY;
  }

  if (!s->ports || smartlist_len(s->ports) == 0) {
    log_warn(LD_CONFIG, "At least one VIRTPORT/TARGET must be specified.");
    rend_service_free(s);
    return RSAE_BADVIRTPORT;
  }
  if (s->auth_type != REND_NO_AUTH &&
      (!s->clients || smartlist_len(s->clients) == 0)) {
    log_warn(LD_CONFIG, "At least one authorized client must be specified.");
    rend_service_free(s);
    return RSAE_BADAUTH;
  }

  /* Enforcing pk/id uniqueness should be done by rend_service_load_keys(), but
   * it's not, see #14828.
   */
  if (rend_service_get_by_pk_digest(s->pk_digest)) {
    log_warn(LD_CONFIG, "Onion Service private key collides with an "
             "existing service.");
    rend_service_free(s);
    return RSAE_ADDREXISTS;
  }
  if (rend_service_get_by_service_id(s->service_id)) {
    log_warn(LD_CONFIG, "Onion Service id collides with an existing service.");
    rend_service_free(s);
    return RSAE_ADDREXISTS;
  }

  /* Initialize the service. */
  if (rend_add_service(NULL, s)) {
    return RSAE_INTERNAL;
  }
  *service_id_out = tor_strdup(s->service_id);

  log_debug(LD_CONFIG, "Added ephemeral Onion Service: %s", s->service_id);
  return RSAE_OKAY;
}

/** Remove the ephemeral service <b>service_id</b> if possible.  Returns 0 on
 * success, and -1 on failure.
 */
int
rend_service_del_ephemeral(const char *service_id)
{
  rend_service_t *s;
  if (!rend_valid_service_id(service_id)) {
    log_warn(LD_CONFIG, "Requested malformed Onion Service id for removal.");
    return -1;
  }
  if ((s = rend_service_get_by_service_id(service_id)) == NULL) {
    log_warn(LD_CONFIG, "Requested non-existent Onion Service id for "
             "removal.");
    return -1;
  }
  if (s->directory) {
    log_warn(LD_CONFIG, "Requested non-ephemeral Onion Service for removal.");
    return -1;
  }

  /* Kill the intro point circuit for the Onion Service, and remove it from
   * the list.  Closing existing connections is the application's problem.
   *
   * XXX: As with the comment in rend_config_services(), a nice abstraction
   * would be ideal here, but for now just duplicate the code.
   */
  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (!circ->marked_for_close &&
        (circ->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
         circ->purpose == CIRCUIT_PURPOSE_S_INTRO)) {
      origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(circ);
      tor_assert(oc->rend_data);
      if (!tor_memeq(s->pk_digest, oc->rend_data->rend_pk_digest, DIGEST_LEN))
        continue;
      log_debug(LD_REND, "Closing intro point %s for service %s.",
                safe_str_client(extend_info_describe(
                                          oc->build_state->chosen_exit)),
                oc->rend_data->onion_address);
      circuit_mark_for_close(circ, END_CIRC_REASON_FINISHED);
    }
  } SMARTLIST_FOREACH_END(circ);
  smartlist_remove(rend_service_list, s);
  rend_service_free(s);

  log_debug(LD_CONFIG, "Removed ephemeral Onion Service: %s", service_id);

  return 0;
}

/** Replace the old value of <b>service</b>-\>desc with one that reflects
 * the other fields in service.
 */
static void
rend_service_update_descriptor(rend_service_t *service)
{
  rend_service_descriptor_t *d;
  origin_circuit_t *circ;
  int i;

  rend_service_descriptor_free(service->desc);
  service->desc = NULL;

  d = service->desc = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  d->pk = crypto_pk_dup_key(service->private_key);
  d->timestamp = time(NULL);
  d->timestamp -= d->timestamp % 3600; /* Round down to nearest hour */
  d->intro_nodes = smartlist_new();
  /* Support intro protocols 2 and 3. */
  d->protocols = (1 << 2) + (1 << 3);

  for (i = 0; i < smartlist_len(service->intro_nodes); ++i) {
    rend_intro_point_t *intro_svc = smartlist_get(service->intro_nodes, i);
    rend_intro_point_t *intro_desc;

    /* This intro point won't be listed in the descriptor... */
    intro_svc->listed_in_last_desc = 0;

    circ = find_intro_circuit(intro_svc, service->pk_digest);
    if (!circ || circ->base_.purpose != CIRCUIT_PURPOSE_S_INTRO) {
      /* This intro point's circuit isn't finished yet.  Don't list it. */
      continue;
    }

    /* ...unless this intro point is listed in the descriptor. */
    intro_svc->listed_in_last_desc = 1;

    /* We have an entirely established intro circuit.  Publish it in
     * our descriptor. */
    intro_desc = tor_malloc_zero(sizeof(rend_intro_point_t));
    intro_desc->extend_info = extend_info_dup(intro_svc->extend_info);
    if (intro_svc->intro_key)
      intro_desc->intro_key = crypto_pk_dup_key(intro_svc->intro_key);
    smartlist_add(d->intro_nodes, intro_desc);

    if (intro_svc->time_published == -1) {
      /* We are publishing this intro point in a descriptor for the
       * first time -- note the current time in the service's copy of
       * the intro point. */
      intro_svc->time_published = time(NULL);
    }
  }
}

/* Allocate and return a string containing the path to file_name in
 * service->directory. Asserts that service has a directory.
 * This function will never return NULL.
 * The caller must free this path. */
static char *
rend_service_path(const rend_service_t *service, const char *file_name)
{
  char *file_path = NULL;

  tor_assert(service->directory);

  /* Can never fail: asserts rather than leaving file_path NULL. */
  tor_asprintf(&file_path, "%s%s%s",
               service->directory, PATH_SEPARATOR, file_name);

  return file_path;
}

/* Allocate and return a string containing the path to the single onion
 * service poison file in service->directory. Asserts that service has a
 * directory.
 * The caller must free this path. */
STATIC char *
rend_service_sos_poison_path(const rend_service_t *service)
{
  return rend_service_path(service, sos_poison_fname);
}

/** Return True if hidden services <b>service> has been poisoned by single
 * onion mode. */
static int
service_is_single_onion_poisoned(const rend_service_t *service)
{
  char *poison_fname = NULL;
  file_status_t fstatus;

  /* Passing a NULL service is a bug */
  if (BUG(!service)) {
    return 0;
  }

  if (!service->directory) {
    return 0;
  }

  poison_fname = rend_service_sos_poison_path(service);

  fstatus = file_status(poison_fname);
  tor_free(poison_fname);

  /* If this fname is occupied, the hidden service has been poisoned.
   * fstatus can be FN_ERROR if the service directory does not exist, in that
   * case, there is obviously no private key. */
  if (fstatus == FN_FILE || fstatus == FN_EMPTY) {
    return 1;
  }

  return 0;
}

/* Return 1 if the private key file for service exists and has a non-zero size,
 * and 0 otherwise. */
static int
rend_service_private_key_exists(const rend_service_t *service)
{
  char *private_key_path = rend_service_path(service, private_key_fname);
  const file_status_t private_key_status = file_status(private_key_path);
  tor_free(private_key_path);
  /* Only non-empty regular private key files could have been used before.
   * fstatus can be FN_ERROR if the service directory does not exist, in that
   * case, there is obviously no private key. */
  return private_key_status == FN_FILE;
}

/** Check the single onion service poison state of the directory for s:
 * - If the service is poisoned, and we are in Single Onion Mode,
 *   return 0,
 * - If the service is not poisoned, and we are not in Single Onion Mode,
 *   return 0,
 * - Otherwise, the poison state is invalid: the service was created in one
 *   mode, and is being used in the other, return -1.
 * Hidden service directories without keys are always considered consistent.
 * They will be poisoned after their directory is created (if needed). */
STATIC int
rend_service_verify_single_onion_poison(const rend_service_t* s,
                                        const or_options_t* options)
{
  /* Passing a NULL service is a bug */
  if (BUG(!s)) {
    return -1;
  }

  /* Ephemeral services are checked at ADD_ONION time */
  if (!s->directory) {
    return 0;
  }

  /* Services without keys are always ok - their keys will only ever be used
   * in the current mode */
  if (!rend_service_private_key_exists(s)) {
    return 0;
  }

  /* The key has been used before in a different mode */
  if (service_is_single_onion_poisoned(s) !=
      rend_service_non_anonymous_mode_enabled(options)) {
    return -1;
  }

  /* The key exists and is consistent with the current mode */
  return 0;
}

/*** Helper for rend_service_poison_new_single_onion_dir(). Add a file to
 * the hidden service directory for s that marks it as a single onion service.
 * Tor must be in single onion mode before calling this function, and the
 * service directory must already have been created.
 * Returns 0 when a directory is successfully poisoned, or if it is already
 * poisoned. Returns -1 on a failure to read the directory or write the poison
 * file, or if there is an existing private key file in the directory. (The
 * service should have been poisoned when the key was created.) */
static int
poison_new_single_onion_hidden_service_dir_impl(const rend_service_t *service,
                                                const or_options_t* options)
{
  /* Passing a NULL service is a bug */
  if (BUG(!service)) {
    return -1;
  }

  /* We must only poison directories if we're in Single Onion mode */
  tor_assert(rend_service_non_anonymous_mode_enabled(options));

  int fd;
  int retval = -1;
  char *poison_fname = NULL;

  if (!service->directory) {
    log_info(LD_REND, "Ephemeral HS started in non-anonymous mode.");
    return 0;
  }

  /* Make sure we're only poisoning new hidden service directories */
  if (rend_service_private_key_exists(service)) {
    log_warn(LD_BUG, "Tried to single onion poison a service directory after "
             "the private key was created.");
    return -1;
  }

  /* Make sure the directory was created before calling this function. */
  if (BUG(rend_service_check_private_dir_impl(options, service, 0) < 0))
    return -1;

  poison_fname = rend_service_sos_poison_path(service);

  switch (file_status(poison_fname)) {
  case FN_DIR:
  case FN_ERROR:
    log_warn(LD_FS, "Can't read single onion poison file \"%s\"",
             poison_fname);
    goto done;
  case FN_FILE: /* single onion poison file already exists. NOP. */
  case FN_EMPTY: /* single onion poison file already exists. NOP. */
    log_debug(LD_FS, "Tried to re-poison a single onion poisoned file \"%s\"",
              poison_fname);
    break;
  case FN_NOENT:
    fd = tor_open_cloexec(poison_fname, O_RDWR|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
      log_warn(LD_FS, "Could not create single onion poison file %s",
               poison_fname);
      goto done;
    }
    close(fd);
    break;
  default:
    tor_assert(0);
  }

  retval = 0;

 done:
  tor_free(poison_fname);

  return retval;
}

/** We just got launched in Single Onion Mode. That's a non-anoymous mode for
 * hidden services. If s is new, we should mark its hidden service
 * directory appropriately so that it is never launched as a location-private
 * hidden service. (New directories don't have private key files.)
 * Return 0 on success, -1 on fail. */
STATIC int
rend_service_poison_new_single_onion_dir(const rend_service_t *s,
                                         const or_options_t* options)
{
  /* Passing a NULL service is a bug */
  if (BUG(!s)) {
    return -1;
  }

  /* We must only poison directories if we're in Single Onion mode */
  tor_assert(rend_service_non_anonymous_mode_enabled(options));

  if (!rend_service_private_key_exists(s)) {
    if (poison_new_single_onion_hidden_service_dir_impl(s, options)
        < 0) {
      return -1;
    }
  }

  return 0;
}

/** Load and/or generate private keys for all hidden services, possibly
 * including keys for client authorization.
 * If a <b>service_list</b> is provided, treat it as the list of hidden
 * services (used in unittests). Otherwise, require that rend_service_list is
 * not NULL.
 * Return 0 on success, -1 on failure. */
int
rend_service_load_all_keys(const smartlist_t *service_list)
{
  const smartlist_t *s_list = NULL;
  /* If no special service list is provided, then just use the global one. */
  if (!service_list) {
    if (BUG(!rend_service_list)) {
      return -1;
    }
    s_list = rend_service_list;
  } else {
    s_list = service_list;
  }

  SMARTLIST_FOREACH_BEGIN(s_list, rend_service_t *, s) {
    if (s->private_key)
      continue;
    log_info(LD_REND, "Loading hidden-service keys from \"%s\"",
             s->directory);

    if (rend_service_load_keys(s) < 0)
      return -1;
  } SMARTLIST_FOREACH_END(s);

  return 0;
}

/** Add to <b>lst</b> every filename used by <b>s</b>. */
static void
rend_service_add_filenames_to_list(smartlist_t *lst, const rend_service_t *s)
{
  tor_assert(lst);
  tor_assert(s);
  tor_assert(s->directory);
  smartlist_add(lst, rend_service_path(s, private_key_fname));
  smartlist_add(lst, rend_service_path(s, hostname_fname));
  smartlist_add(lst, rend_service_path(s, client_keys_fname));
  smartlist_add(lst, rend_service_sos_poison_path(s));
}

/** Add to <b>open_lst</b> every filename used by a configured hidden service,
 * and to <b>stat_lst</b> every directory used by a configured hidden
 * service */
void
rend_services_add_filenames_to_lists(smartlist_t *open_lst,
                                     smartlist_t *stat_lst)
{
  if (!rend_service_list)
    return;
  SMARTLIST_FOREACH_BEGIN(rend_service_list, rend_service_t *, s) {
    if (s->directory) {
      rend_service_add_filenames_to_list(open_lst, s);
      smartlist_add(stat_lst, tor_strdup(s->directory));
    }
  } SMARTLIST_FOREACH_END(s);
}

/** Derive all rend_service_t internal material based on the service's key.
 * Returns 0 on sucess, -1 on failure.
 */
static int
rend_service_derive_key_digests(struct rend_service_t *s)
{
  if (rend_get_service_id(s->private_key, s->service_id)<0) {
    log_warn(LD_BUG, "Internal error: couldn't encode service ID.");
    return -1;
  }
  if (crypto_pk_get_digest(s->private_key, s->pk_digest)<0) {
    log_warn(LD_BUG, "Couldn't compute hash of public key.");
    return -1;
  }

  return 0;
}

/* Implements the directory check from rend_service_check_private_dir,
 * without doing the single onion poison checks. */
static int
rend_service_check_private_dir_impl(const or_options_t *options,
                                    const rend_service_t *s,
                                    int create)
{
  cpd_check_t  check_opts = CPD_NONE;
  if (create) {
    check_opts |= CPD_CREATE;
  } else {
    check_opts |= CPD_CHECK_MODE_ONLY;
    check_opts |= CPD_CHECK;
  }
  if (s->dir_group_readable) {
    check_opts |= CPD_GROUP_READ;
  }
  /* Check/create directory */
  if (check_private_dir(s->directory, check_opts, options->User) < 0) {
    log_warn(LD_REND, "Checking service directory %s failed.", s->directory);
    return -1;
  }

  return 0;
}

/** Make sure that the directory for <b>s</b> is private, using the config in
 * <b>options</b>.
 * If <b>create</b> is true:
 *  - if the directory exists, change permissions if needed,
 *  - if the directory does not exist, create it with the correct permissions.
 * If <b>create</b> is false:
 *  - if the directory exists, check permissions,
 *  - if the directory does not exist, check if we think we can create it.
 * Return 0 on success, -1 on failure. */
static int
rend_service_check_private_dir(const or_options_t *options,
                               const rend_service_t *s,
                               int create)
{
  /* Passing a NULL service is a bug */
  if (BUG(!s)) {
    return -1;
  }

  /* Check/create directory */
  if (rend_service_check_private_dir_impl(options, s, create) < 0) {
    return -1;
  }

  /* Check if the hidden service key exists, and was created in a different
   * single onion service mode, and refuse to launch if it has.
   * This is safe to call even when create is false, as it ignores missing
   * keys and directories: they are always valid.
   */
  if (rend_service_verify_single_onion_poison(s, options) < 0) {
    /* We can't use s->service_id here, as the key may not have been loaded */
    log_warn(LD_GENERAL, "We are configured with "
             "HiddenServiceNonAnonymousMode %d, but the hidden "
             "service key in directory %s was created in %s mode. "
             "This is not allowed.",
             rend_service_non_anonymous_mode_enabled(options) ? 1 : 0,
             rend_service_escaped_dir(s),
             rend_service_non_anonymous_mode_enabled(options) ?
             "an anonymous" : "a non-anonymous"
             );
    return -1;
  }

  /* Poison new single onion directories immediately after they are created,
   * so that we never accidentally launch non-anonymous hidden services
   * thinking they are anonymous. Any keys created later will end up with the
   * correct poisoning state.
   */
  if (create && rend_service_non_anonymous_mode_enabled(options)) {
    static int logged_warning = 0;

    if (rend_service_poison_new_single_onion_dir(s, options) < 0) {
      log_warn(LD_GENERAL,"Failed to mark new hidden services as non-anonymous"
               ".");
      return -1;
    }

    if (!logged_warning) {
      /* The keys for these services are linked to the server IP address */
      log_notice(LD_REND, "The configured onion service directories have been "
                 "used in single onion mode. They can not be used for "
                 "anonymous hidden services.");
      logged_warning = 1;
    }
  }

  return 0;
}

/** Load and/or generate private keys for the hidden service <b>s</b>,
 * possibly including keys for client authorization.  Return 0 on success, -1
 * on failure. */
static int
rend_service_load_keys(rend_service_t *s)
{
  char *fname = NULL;
  char buf[128];

  /* Make sure the directory was created and single onion poisoning was
   * checked before calling this function */
  if (BUG(rend_service_check_private_dir(get_options(), s, 0) < 0))
    goto err;

  /* Load key */
  fname = rend_service_path(s, private_key_fname);
  s->private_key = init_key_from_file(fname, 1, LOG_ERR, 0);

  if (!s->private_key)
    goto err;

  if (rend_service_derive_key_digests(s) < 0)
    goto err;

  tor_free(fname);
  /* Create service file */
  fname = rend_service_path(s, hostname_fname);

  tor_snprintf(buf, sizeof(buf),"%s.onion\n", s->service_id);
  if (write_str_to_file(fname,buf,0)<0) {
    log_warn(LD_CONFIG, "Could not write onion address to hostname file.");
    goto err;
  }
#ifndef _WIN32
  if (s->dir_group_readable) {
    /* Also verify hostname file created with group read. */
    if (chmod(fname, 0640))
      log_warn(LD_FS,"Unable to make hidden hostname file %s group-readable.",
               fname);
  }
#endif

  /* If client authorization is configured, load or generate keys. */
  if (s->auth_type != REND_NO_AUTH) {
    if (rend_service_load_auth_keys(s, fname) < 0) {
      goto err;
    }
  }

  int r = 0;
  goto done;
 err:
  r = -1;
 done:
  memwipe(buf, 0, sizeof(buf));
  tor_free(fname);
  return r;
}

/** Load and/or generate client authorization keys for the hidden service
 * <b>s</b>, which stores its hostname in <b>hfname</b>.  Return 0 on success,
 * -1 on failure. */
static int
rend_service_load_auth_keys(rend_service_t *s, const char *hfname)
{
  int r = 0;
  char *cfname = NULL;
  char *client_keys_str = NULL;
  strmap_t *parsed_clients = strmap_new();
  FILE *cfile, *hfile;
  open_file_t *open_cfile = NULL, *open_hfile = NULL;
  char desc_cook_out[3*REND_DESC_COOKIE_LEN_BASE64+1];
  char service_id[16+1];
  char buf[1500];

  /* Load client keys and descriptor cookies, if available. */
  cfname = rend_service_path(s, client_keys_fname);
  client_keys_str = read_file_to_str(cfname, RFTS_IGNORE_MISSING, NULL);
  if (client_keys_str) {
    if (rend_parse_client_keys(parsed_clients, client_keys_str) < 0) {
      log_warn(LD_CONFIG, "Previously stored client_keys file could not "
               "be parsed.");
      goto err;
    } else {
      log_info(LD_CONFIG, "Parsed %d previously stored client entries.",
               strmap_size(parsed_clients));
    }
  }

  /* Prepare client_keys and hostname files. */
  if (!(cfile = start_writing_to_stdio_file(cfname,
                                            OPEN_FLAGS_REPLACE | O_TEXT,
                                            0600, &open_cfile))) {
    log_warn(LD_CONFIG, "Could not open client_keys file %s",
             escaped(cfname));
    goto err;
  }

  if (!(hfile = start_writing_to_stdio_file(hfname,
                                            OPEN_FLAGS_REPLACE | O_TEXT,
                                            0600, &open_hfile))) {
    log_warn(LD_CONFIG, "Could not open hostname file %s", escaped(hfname));
    goto err;
  }

  /* Either use loaded keys for configured clients or generate new
   * ones if a client is new. */
  SMARTLIST_FOREACH_BEGIN(s->clients, rend_authorized_client_t *, client) {
    rend_authorized_client_t *parsed =
      strmap_get(parsed_clients, client->client_name);
    int written;
    size_t len;
    /* Copy descriptor cookie from parsed entry or create new one. */
    if (parsed) {
      memcpy(client->descriptor_cookie, parsed->descriptor_cookie,
             REND_DESC_COOKIE_LEN);
    } else {
      crypto_rand((char *) client->descriptor_cookie, REND_DESC_COOKIE_LEN);
    }
    /* For compatibility with older tor clients, this does not
     * truncate the padding characters, unlike rend_auth_encode_cookie.  */
    if (base64_encode(desc_cook_out, 3*REND_DESC_COOKIE_LEN_BASE64+1,
                      (char *) client->descriptor_cookie,
                      REND_DESC_COOKIE_LEN, 0) < 0) {
      log_warn(LD_BUG, "Could not base64-encode descriptor cookie.");
      goto err;
    }
    /* Copy client key from parsed entry or create new one if required. */
    if (parsed && parsed->client_key) {
      client->client_key = crypto_pk_dup_key(parsed->client_key);
    } else if (s->auth_type == REND_STEALTH_AUTH) {
      /* Create private key for client. */
      crypto_pk_t *prkey = NULL;
      if (!(prkey = crypto_pk_new())) {
        log_warn(LD_BUG,"Error constructing client key");
        goto err;
      }
      if (crypto_pk_generate_key(prkey)) {
        log_warn(LD_BUG,"Error generating client key");
        crypto_pk_free(prkey);
        goto err;
      }
      if (crypto_pk_check_key(prkey) <= 0) {
        log_warn(LD_BUG,"Generated client key seems invalid");
        crypto_pk_free(prkey);
        goto err;
      }
      client->client_key = prkey;
    }
    /* Add entry to client_keys file. */
    written = tor_snprintf(buf, sizeof(buf),
                           "client-name %s\ndescriptor-cookie %s\n",
                           client->client_name, desc_cook_out);
    if (written < 0) {
      log_warn(LD_BUG, "Could not write client entry.");
      goto err;
    }
    if (client->client_key) {
      char *client_key_out = NULL;
      if (crypto_pk_write_private_key_to_string(client->client_key,
                                                &client_key_out, &len) != 0) {
        log_warn(LD_BUG, "Internal error: "
                 "crypto_pk_write_private_key_to_string() failed.");
        goto err;
      }
      if (rend_get_service_id(client->client_key, service_id)<0) {
        log_warn(LD_BUG, "Internal error: couldn't encode service ID.");
        /*
         * len is string length, not buffer length, but last byte is NUL
         * anyway.
         */
        memwipe(client_key_out, 0, len);
        tor_free(client_key_out);
        goto err;
      }
      written = tor_snprintf(buf + written, sizeof(buf) - written,
                             "client-key\n%s", client_key_out);
      memwipe(client_key_out, 0, len);
      tor_free(client_key_out);
      if (written < 0) {
        log_warn(LD_BUG, "Could not write client entry.");
        goto err;
      }
    } else {
      strlcpy(service_id, s->service_id, sizeof(service_id));
    }

    if (fputs(buf, cfile) < 0) {
      log_warn(LD_FS, "Could not append client entry to file: %s",
               strerror(errno));
      goto err;
    }

    /* Add line to hostname file. This is not the same encoding as in
     * client_keys. */
    char *encoded_cookie = rend_auth_encode_cookie(client->descriptor_cookie,
                                                   s->auth_type);
    if (!encoded_cookie) {
      log_warn(LD_BUG, "Could not base64-encode descriptor cookie.");
      goto err;
    }
    tor_snprintf(buf, sizeof(buf), "%s.onion %s # client: %s\n",
                 service_id, encoded_cookie, client->client_name);
    memwipe(encoded_cookie, 0, strlen(encoded_cookie));
    tor_free(encoded_cookie);

    if (fputs(buf, hfile)<0) {
      log_warn(LD_FS, "Could not append host entry to file: %s",
               strerror(errno));
      goto err;
    }
  } SMARTLIST_FOREACH_END(client);

  finish_writing_to_file(open_cfile);
  finish_writing_to_file(open_hfile);

  goto done;
 err:
  r = -1;
  if (open_cfile)
    abort_writing_to_file(open_cfile);
  if (open_hfile)
    abort_writing_to_file(open_hfile);
 done:
  if (client_keys_str) {
    memwipe(client_keys_str, 0, strlen(client_keys_str));
    tor_free(client_keys_str);
  }
  strmap_free(parsed_clients, rend_authorized_client_strmap_item_free);

  if (cfname) {
    memwipe(cfname, 0, strlen(cfname));
    tor_free(cfname);
  }

  /* Clear stack buffers that held key-derived material. */
  memwipe(buf, 0, sizeof(buf));
  memwipe(desc_cook_out, 0, sizeof(desc_cook_out));
  memwipe(service_id, 0, sizeof(service_id));

  return r;
}

/** Return the service whose public key has a digest of <b>digest</b>, or
 * NULL if no such service exists.
 */
static rend_service_t *
rend_service_get_by_pk_digest(const char* digest)
{
  SMARTLIST_FOREACH(rend_service_list, rend_service_t*, s,
                    if (tor_memeq(s->pk_digest,digest,DIGEST_LEN))
                        return s);
  return NULL;
}

/** Return the service whose service id is <b>id</b>, or NULL if no such
 * service exists.
 */
static struct rend_service_t *
rend_service_get_by_service_id(const char *id)
{
  tor_assert(strlen(id) == REND_SERVICE_ID_LEN_BASE32);
  SMARTLIST_FOREACH(rend_service_list, rend_service_t*, s, {
    if (tor_memeq(s->service_id, id, REND_SERVICE_ID_LEN_BASE32))
      return s;
  });
  return NULL;
}

/** Return 1 if any virtual port in <b>service</b> wants a circuit
 * to have good uptime. Else return 0.
 */
static int
rend_service_requires_uptime(rend_service_t *service)
{
  int i;
  rend_service_port_config_t *p;

  for (i=0; i < smartlist_len(service->ports); ++i) {
    p = smartlist_get(service->ports, i);
    if (smartlist_contains_int_as_string(get_options()->LongLivedPorts,
                                  p->virtual_port))
      return 1;
  }
  return 0;
}

/** Check client authorization of a given <b>descriptor_cookie</b> of
 * length <b>cookie_len</b> for <b>service</b>. Return 1 for success
 * and 0 for failure. */
static int
rend_check_authorization(rend_service_t *service,
                         const char *descriptor_cookie,
                         size_t cookie_len)
{
  rend_authorized_client_t *auth_client = NULL;
  tor_assert(service);
  tor_assert(descriptor_cookie);
  if (!service->clients) {
    log_warn(LD_BUG, "Can't check authorization for a service that has no "
                     "authorized clients configured.");
    return 0;
  }

  if (cookie_len != REND_DESC_COOKIE_LEN) {
    log_info(LD_REND, "Descriptor cookie is %lu bytes, but we expected "
                      "%lu bytes. Dropping cell.",
             (unsigned long)cookie_len, (unsigned long)REND_DESC_COOKIE_LEN);
    return 0;
  }

  /* Look up client authorization by descriptor cookie. */
  SMARTLIST_FOREACH(service->clients, rend_authorized_client_t *, client, {
    if (tor_memeq(client->descriptor_cookie, descriptor_cookie,
                REND_DESC_COOKIE_LEN)) {
      auth_client = client;
      break;
    }
  });
  if (!auth_client) {
    char descriptor_cookie_base64[3*REND_DESC_COOKIE_LEN_BASE64];
    base64_encode(descriptor_cookie_base64, sizeof(descriptor_cookie_base64),
                  descriptor_cookie, REND_DESC_COOKIE_LEN, 0);
    log_info(LD_REND, "No authorization found for descriptor cookie '%s'! "
                      "Dropping cell!",
             descriptor_cookie_base64);
    return 0;
  }

  /* Allow the request. */
  log_info(LD_REND, "Client %s authorized for service %s.",
           auth_client->client_name, service->service_id);
  return 1;
}

/* Can this service make a direct connection to ei?
 * It must be a single onion service, and the firewall rules must allow ei. */
static int
rend_service_use_direct_connection(const or_options_t* options,
                                   const extend_info_t* ei)
{
  /* We'll connect directly all reachable addresses, whether preferred or not.
   * The prefer_ipv6 argument to fascist_firewall_allows_address_addr is
   * ignored, because pref_only is 0. */
  return (rend_service_allow_non_anonymous_connection(options) &&
          fascist_firewall_allows_address_addr(&ei->addr, ei->port,
                                               FIREWALL_OR_CONNECTION, 0, 0));
}

/* Like rend_service_use_direct_connection, but to a node. */
static int
rend_service_use_direct_connection_node(const or_options_t* options,
                                        const node_t* node)
{
  /* We'll connect directly all reachable addresses, whether preferred or not.
   */
  return (rend_service_allow_non_anonymous_connection(options) &&
          fascist_firewall_allows_node(node, FIREWALL_OR_CONNECTION, 0));
}

/******
 * Handle cells
 ******/

/** Respond to an INTRODUCE2 cell by launching a circuit to the chosen
 * rendezvous point.
 */
int
rend_service_receive_introduction(origin_circuit_t *circuit,
                                  const uint8_t *request,
                                  size_t request_len)
{
  /* Global status stuff */
  int status = 0, result;
  const or_options_t *options = get_options();
  char *err_msg = NULL;
  int err_msg_severity = LOG_WARN;
  const char *stage_descr = NULL;
  int reason = END_CIRC_REASON_TORPROTOCOL;
  /* Service/circuit/key stuff we can learn before parsing */
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  rend_service_t *service = NULL;
  rend_intro_point_t *intro_point = NULL;
  crypto_pk_t *intro_key = NULL;
  /* Parsed cell */
  rend_intro_cell_t *parsed_req = NULL;
  /* Rendezvous point */
  extend_info_t *rp = NULL;
  /* XXX not handled yet */
  char buf[RELAY_PAYLOAD_SIZE];
  char keys[DIGEST_LEN+CPATH_KEY_MATERIAL_LEN]; /* Holds KH, Df, Db, Kf, Kb */
  int i;
  crypto_dh_t *dh = NULL;
  origin_circuit_t *launched = NULL;
  crypt_path_t *cpath = NULL;
  char hexcookie[9];
  int circ_needs_uptime;
  time_t now = time(NULL);
  time_t elapsed;
  int replay;
  size_t keylen;

  /* Do some initial validation and logging before we parse the cell */
  if (circuit->base_.purpose != CIRCUIT_PURPOSE_S_INTRO) {
    log_warn(LD_PROTOCOL,
             "Got an INTRODUCE2 over a non-introduction circuit %u.",
             (unsigned) circuit->base_.n_circ_id);
    goto err;
  }

  assert_circ_anonymity_ok(circuit, options);
  tor_assert(circuit->rend_data);

  /* We'll use this in a bazillion log messages */
  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32+1,
                circuit->rend_data->rend_pk_digest, REND_SERVICE_ID_LEN);

  /* look up service depending on circuit. */
  service =
    rend_service_get_by_pk_digest(circuit->rend_data->rend_pk_digest);
  if (!service) {
    log_warn(LD_BUG,
             "Internal error: Got an INTRODUCE2 cell on an intro "
             "circ for an unrecognized service %s.",
             escaped(serviceid));
    goto err;
  }

  intro_point = find_intro_point(circuit);
  if (intro_point == NULL) {
    intro_point = find_expiring_intro_point(service, circuit);
    if (intro_point == NULL) {
      log_warn(LD_BUG,
               "Internal error: Got an INTRODUCE2 cell on an "
               "intro circ (for service %s) with no corresponding "
               "rend_intro_point_t.",
               escaped(serviceid));
      goto err;
    }
  }

  log_info(LD_REND, "Received INTRODUCE2 cell for service %s on circ %u.",
           escaped(serviceid), (unsigned)circuit->base_.n_circ_id);

  /* use intro key instead of service key. */
  intro_key = circuit->intro_key;

  tor_free(err_msg);
  stage_descr = NULL;

  stage_descr = "early parsing";
  /* Early parsing pass (get pk, ciphertext); type 2 is INTRODUCE2 */
  parsed_req =
    rend_service_begin_parse_intro(request, request_len, 2, &err_msg);
  if (!parsed_req) {
    goto log_error;
  } else if (err_msg) {
    log_info(LD_REND, "%s on circ %u.", err_msg,
             (unsigned)circuit->base_.n_circ_id);
    tor_free(err_msg);
  }

  /* make sure service replay caches are present */
  if (!service->accepted_intro_dh_parts) {
    service->accepted_intro_dh_parts =
      replaycache_new(REND_REPLAY_TIME_INTERVAL,
                      REND_REPLAY_TIME_INTERVAL);
  }

  if (!intro_point->accepted_intro_rsa_parts) {
    intro_point->accepted_intro_rsa_parts = replaycache_new(0, 0);
  }

  /* check for replay of PK-encrypted portion. */
  keylen = crypto_pk_keysize(intro_key);
  replay = replaycache_add_test_and_elapsed(
    intro_point->accepted_intro_rsa_parts,
    parsed_req->ciphertext, MIN(parsed_req->ciphertext_len, keylen),
    &elapsed);

  if (replay) {
    log_warn(LD_REND,
             "Possible replay detected! We received an "
             "INTRODUCE2 cell with same PK-encrypted part %d "
             "seconds ago.  Dropping cell.",
             (int)elapsed);
    goto err;
  }

  stage_descr = "decryption";
  /* Now try to decrypt it */
  result = rend_service_decrypt_intro(parsed_req, intro_key, &err_msg);
  if (result < 0) {
    goto log_error;
  } else if (err_msg) {
    log_info(LD_REND, "%s on circ %u.", err_msg,
             (unsigned)circuit->base_.n_circ_id);
    tor_free(err_msg);
  }

  stage_descr = "late parsing";
  /* Parse the plaintext */
  result = rend_service_parse_intro_plaintext(parsed_req, &err_msg);
  if (result < 0) {
    goto log_error;
  } else if (err_msg) {
    log_info(LD_REND, "%s on circ %u.", err_msg,
             (unsigned)circuit->base_.n_circ_id);
    tor_free(err_msg);
  }

  stage_descr = "late validation";
  /* Validate the parsed plaintext parts */
  result = rend_service_validate_intro_late(parsed_req, &err_msg);
  if (result < 0) {
    goto log_error;
  } else if (err_msg) {
    log_info(LD_REND, "%s on circ %u.", err_msg,
             (unsigned)circuit->base_.n_circ_id);
    tor_free(err_msg);
  }
  stage_descr = NULL;

  /* Increment INTRODUCE2 counter */
  ++(intro_point->accepted_introduce2_count);

  /* Find the rendezvous point */
  rp = find_rp_for_intro(parsed_req, &err_msg);
  if (!rp) {
    err_msg_severity = LOG_PROTOCOL_WARN;
    goto log_error;
  }

  /* Check if we'd refuse to talk to this router */
  if (options->StrictNodes &&
      routerset_contains_extendinfo(options->ExcludeNodes, rp)) {
    log_warn(LD_REND, "Client asked to rendezvous at a relay that we "
             "exclude, and StrictNodes is set. Refusing service.");
    reason = END_CIRC_REASON_INTERNAL; /* XXX might leak why we refused */
    goto err;
  }

  base16_encode(hexcookie, 9, (const char *)(parsed_req->rc), 4);

  /* Check whether there is a past request with the same Diffie-Hellman,
   * part 1. */
  replay = replaycache_add_test_and_elapsed(
      service->accepted_intro_dh_parts,
      parsed_req->dh, DH_KEY_LEN,
      &elapsed);

  if (replay) {
    /* A Tor client will send a new INTRODUCE1 cell with the same rend
     * cookie and DH public key as its previous one if its intro circ
     * times out while in state CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT .
     * If we received the first INTRODUCE1 cell (the intro-point relay
     * converts it into an INTRODUCE2 cell), we are already trying to
     * connect to that rend point (and may have already succeeded);
     * drop this cell. */
    log_info(LD_REND, "We received an "
             "INTRODUCE2 cell with same first part of "
             "Diffie-Hellman handshake %d seconds ago. Dropping "
             "cell.",
             (int) elapsed);
    goto err;
  }

  /* If the service performs client authorization, check included auth data. */
  if (service->clients) {
    if (parsed_req->version == 3 && parsed_req->u.v3.auth_len > 0) {
      if (rend_check_authorization(service,
                                   (const char*)parsed_req->u.v3.auth_data,
                                   parsed_req->u.v3.auth_len)) {
        log_info(LD_REND, "Authorization data in INTRODUCE2 cell are valid.");
      } else {
        log_info(LD_REND, "The authorization data that are contained in "
                 "the INTRODUCE2 cell are invalid. Dropping cell.");
        reason = END_CIRC_REASON_CONNECTFAILED;
        goto err;
      }
    } else {
      log_info(LD_REND, "INTRODUCE2 cell does not contain authentication "
               "data, but we require client authorization. Dropping cell.");
      reason = END_CIRC_REASON_CONNECTFAILED;
      goto err;
    }
  }

  /* Try DH handshake... */
  dh = crypto_dh_new(DH_TYPE_REND);
  if (!dh || crypto_dh_generate_public(dh)<0) {
    log_warn(LD_BUG,"Internal error: couldn't build DH state "
             "or generate public key.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }
  if (crypto_dh_compute_secret(LOG_PROTOCOL_WARN, dh,
                               (char *)(parsed_req->dh),
                               DH_KEY_LEN, keys,
                               DIGEST_LEN+CPATH_KEY_MATERIAL_LEN)<0) {
    log_warn(LD_BUG, "Internal error: couldn't complete DH handshake");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }

  circ_needs_uptime = rend_service_requires_uptime(service);

  /* help predict this next time */
  rep_hist_note_used_internal(now, circ_needs_uptime, 1);

  /* Launch a circuit to the client's chosen rendezvous point.
   */
  for (i=0;i<MAX_REND_FAILURES;i++) {
    int flags = CIRCLAUNCH_NEED_CAPACITY | CIRCLAUNCH_IS_INTERNAL;
    if (circ_needs_uptime) flags |= CIRCLAUNCH_NEED_UPTIME;
    /* A Single Onion Service only uses a direct connection if its
     * firewall rules permit direct connections to the address. */
    if (rend_service_use_direct_connection(options, rp)) {
      flags = flags | CIRCLAUNCH_ONEHOP_TUNNEL;
    }
    launched = circuit_launch_by_extend_info(
                        CIRCUIT_PURPOSE_S_CONNECT_REND, rp, flags);

    if (launched)
      break;
  }
  if (!launched) { /* give up */
    log_warn(LD_REND, "Giving up launching first hop of circuit to rendezvous "
             "point %s for service %s.",
             safe_str_client(extend_info_describe(rp)),
             serviceid);
    reason = END_CIRC_REASON_CONNECTFAILED;
    goto err;
  }
  log_info(LD_REND,
           "Accepted intro; launching circuit to %s "
           "(cookie %s) for service %s.",
           safe_str_client(extend_info_describe(rp)),
           hexcookie, serviceid);
  tor_assert(launched->build_state);
  /* Fill in the circuit's state. */

  launched->rend_data =
    rend_data_service_create(service->service_id,
                             circuit->rend_data->rend_pk_digest,
                             parsed_req->rc, service->auth_type);

  launched->build_state->service_pending_final_cpath_ref =
    tor_malloc_zero(sizeof(crypt_path_reference_t));
  launched->build_state->service_pending_final_cpath_ref->refcount = 1;

  launched->build_state->service_pending_final_cpath_ref->cpath = cpath =
    tor_malloc_zero(sizeof(crypt_path_t));
  cpath->magic = CRYPT_PATH_MAGIC;
  launched->build_state->expiry_time = now + MAX_REND_TIMEOUT;

  cpath->rend_dh_handshake_state = dh;
  dh = NULL;
  if (circuit_init_cpath_crypto(cpath,keys+DIGEST_LEN,1)<0)
    goto err;
  memcpy(cpath->rend_circ_nonce, keys, DIGEST_LEN);

  goto done;

 log_error:
  if (!err_msg) {
    if (stage_descr) {
      tor_asprintf(&err_msg,
                   "unknown %s error for INTRODUCE2", stage_descr);
    } else {
      err_msg = tor_strdup("unknown error for INTRODUCE2");
    }
  }

  log_fn(err_msg_severity, LD_REND, "%s on circ %u", err_msg,
           (unsigned)circuit->base_.n_circ_id);
 err:
  status = -1;
  if (dh) crypto_dh_free(dh);
  if (launched) {
    circuit_mark_for_close(TO_CIRCUIT(launched), reason);
  }
  tor_free(err_msg);

 done:
  memwipe(keys, 0, sizeof(keys));
  memwipe(buf, 0, sizeof(buf));
  memwipe(serviceid, 0, sizeof(serviceid));
  memwipe(hexcookie, 0, sizeof(hexcookie));

  /* Free the parsed cell */
  rend_service_free_intro(parsed_req);

  /* Free rp */
  extend_info_free(rp);

  return status;
}

/** Given a parsed and decrypted INTRODUCE2, find the rendezvous point or
 * return NULL and an error string if we can't. Return a newly allocated
 * extend_info_t* for the rendezvous point. */
static extend_info_t *
find_rp_for_intro(const rend_intro_cell_t *intro,
                  char **err_msg_out)
{
  extend_info_t *rp = NULL;
  char *err_msg = NULL;
  const char *rp_nickname = NULL;
  const node_t *node = NULL;

  if (!intro) {
    if (err_msg_out)
      err_msg = tor_strdup("Bad parameters to find_rp_for_intro()");

    goto err;
  }

  if (intro->version == 0 || intro->version == 1) {
    rp_nickname = (const char *)(intro->u.v0_v1.rp);

    node = node_get_by_nickname(rp_nickname, 0);
    if (!node) {
      if (err_msg_out) {
        tor_asprintf(&err_msg,
                     "Couldn't find router %s named in INTRODUCE2 cell",
                     escaped_safe_str_client(rp_nickname));
      }

      goto err;
    }

    /* Are we in single onion mode? */
    const int allow_direct = rend_service_allow_non_anonymous_connection(
                                                                get_options());
    rp = extend_info_from_node(node, allow_direct);
    if (!rp) {
      if (err_msg_out) {
        tor_asprintf(&err_msg,
                     "Couldn't build extend_info_t for router %s named "
                     "in INTRODUCE2 cell",
                     escaped_safe_str_client(rp_nickname));
      }

      goto err;
    }
  } else if (intro->version == 2) {
    rp = extend_info_dup(intro->u.v2.extend_info);
  } else if (intro->version == 3) {
    rp = extend_info_dup(intro->u.v3.extend_info);
  } else {
    if (err_msg_out) {
      tor_asprintf(&err_msg,
                   "Unknown version %d in INTRODUCE2 cell",
                   (int)(intro->version));
    }

    goto err;
  }

  /* rp is always set here: extend_info_dup guarantees a non-NULL result, and
   * the other cases goto err. */
  tor_assert(rp);

  /* Make sure the RP we are being asked to connect to is _not_ a private
   * address unless it's allowed. Let's avoid to build a circuit to our
   * second middle node and fail right after when extending to the RP. */
  if (!extend_info_addr_is_allowed(&rp->addr)) {
    if (err_msg_out) {
      tor_asprintf(&err_msg,
                   "Relay IP in INTRODUCE2 cell is private address.");
    }
    extend_info_free(rp);
    rp = NULL;
    goto err;
  }
  goto done;

 err:
  if (err_msg_out)
    *err_msg_out = err_msg;
  else
    tor_free(err_msg);

 done:
  return rp;
}

/** Free a parsed INTRODUCE1 or INTRODUCE2 cell that was allocated by
 * rend_service_parse_intro().
 */
void
rend_service_free_intro(rend_intro_cell_t *request)
{
  if (!request) {
    return;
  }

  /* Free ciphertext */
  tor_free(request->ciphertext);
  request->ciphertext_len = 0;

  /* Have plaintext? */
  if (request->plaintext) {
    /* Zero it out just to be safe */
    memwipe(request->plaintext, 0, request->plaintext_len);
    tor_free(request->plaintext);
    request->plaintext_len = 0;
  }

  /* Have parsed plaintext? */
  if (request->parsed) {
    switch (request->version) {
      case 0:
      case 1:
        /*
         * Nothing more to do; these formats have no further pointers
         * in them.
         */
        break;
      case 2:
        extend_info_free(request->u.v2.extend_info);
        request->u.v2.extend_info = NULL;
        break;
      case 3:
        if (request->u.v3.auth_data) {
          memwipe(request->u.v3.auth_data, 0, request->u.v3.auth_len);
          tor_free(request->u.v3.auth_data);
        }

        extend_info_free(request->u.v3.extend_info);
        request->u.v3.extend_info = NULL;
        break;
      default:
        log_info(LD_BUG,
                 "rend_service_free_intro() saw unknown protocol "
                 "version %d.",
                 request->version);
    }
  }

  /* Zero it out to make sure sensitive stuff doesn't hang around in memory */
  memwipe(request, 0, sizeof(*request));

  tor_free(request);
}

/** Parse an INTRODUCE1 or INTRODUCE2 cell into a newly allocated
 * rend_intro_cell_t structure.  Free it with rend_service_free_intro()
 * when finished.  The type parameter should be 1 or 2 to indicate whether
 * this is INTRODUCE1 or INTRODUCE2.  This parses only the non-encrypted
 * parts; after this, call rend_service_decrypt_intro() with a key, then
 * rend_service_parse_intro_plaintext() to finish parsing.  The optional
 * err_msg_out parameter is set to a string suitable for log output
 * if parsing fails.  This function does some validation, but only
 * that which depends solely on the contents of the cell and the
 * key; it can be unit-tested.  Further validation is done in
 * rend_service_validate_intro().
 */

rend_intro_cell_t *
rend_service_begin_parse_intro(const uint8_t *request,
                               size_t request_len,
                               uint8_t type,
                               char **err_msg_out)
{
  rend_intro_cell_t *rv = NULL;
  char *err_msg = NULL;

  if (!request || request_len <= 0) goto err;
  if (!(type == 1 || type == 2)) goto err;

  /* First, check that the cell is long enough to be a sensible INTRODUCE */

  /* min key length plus digest length plus nickname length */
  if (request_len <
        (DIGEST_LEN + REND_COOKIE_LEN + (MAX_NICKNAME_LEN + 1) +
         DH_KEY_LEN + 42)) {
    if (err_msg_out) {
      tor_asprintf(&err_msg,
                   "got a truncated INTRODUCE%d cell",
                   (int)type);
    }
    goto err;
  }

  /* Allocate a new parsed cell structure */
  rv = tor_malloc_zero(sizeof(*rv));

  /* Set the type */
  rv->type = type;

  /* Copy in the ID */
  memcpy(rv->pk, request, DIGEST_LEN);

  /* Copy in the ciphertext */
  rv->ciphertext = tor_malloc(request_len - DIGEST_LEN);
  memcpy(rv->ciphertext, request + DIGEST_LEN, request_len - DIGEST_LEN);
  rv->ciphertext_len = request_len - DIGEST_LEN;

  goto done;

 err:
  rend_service_free_intro(rv);
  rv = NULL;

  if (err_msg_out && !err_msg) {
    tor_asprintf(&err_msg,
                 "unknown INTRODUCE%d error",
                 (int)type);
  }

 done:
  if (err_msg_out) *err_msg_out = err_msg;
  else tor_free(err_msg);

  return rv;
}

/** Parse the version-specific parts of a v0 or v1 INTRODUCE1 or INTRODUCE2
 * cell
 */

static ssize_t
rend_service_parse_intro_for_v0_or_v1(
    rend_intro_cell_t *intro,
    const uint8_t *buf,
    size_t plaintext_len,
    char **err_msg_out)
{
  const char *rp_nickname, *endptr;
  size_t nickname_field_len, ver_specific_len;

  if (intro->version == 1) {
    ver_specific_len = MAX_HEX_NICKNAME_LEN + 2;
    rp_nickname = ((const char *)buf) + 1;
    nickname_field_len = MAX_HEX_NICKNAME_LEN + 1;
  } else if (intro->version == 0) {
    ver_specific_len = MAX_NICKNAME_LEN + 1;
    rp_nickname = (const char *)buf;
    nickname_field_len = MAX_NICKNAME_LEN + 1;
  } else {
    if (err_msg_out)
      tor_asprintf(err_msg_out,
                   "rend_service_parse_intro_for_v0_or_v1() called with "
                   "bad version %d on INTRODUCE%d cell (this is a bug)",
                   intro->version,
                   (int)(intro->type));
    goto err;
  }

  if (plaintext_len < ver_specific_len) {
    if (err_msg_out)
      tor_asprintf(err_msg_out,
                   "short plaintext of encrypted part in v1 INTRODUCE%d "
                   "cell (%lu bytes, needed %lu)",
                   (int)(intro->type),
                   (unsigned long)plaintext_len,
                   (unsigned long)ver_specific_len);
    goto err;
  }

  endptr = memchr(rp_nickname, 0, nickname_field_len);
  if (!endptr || endptr == rp_nickname) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "couldn't find a nul-padded nickname in "
                   "INTRODUCE%d cell",
                   (int)(intro->type));
    }
    goto err;
  }

  if ((intro->version == 0 &&
       !is_legal_nickname(rp_nickname)) ||
      (intro->version == 1 &&
       !is_legal_nickname_or_hexdigest(rp_nickname))) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "bad nickname in INTRODUCE%d cell",
                   (int)(intro->type));
    }
    goto err;
  }

  memcpy(intro->u.v0_v1.rp, rp_nickname, endptr - rp_nickname + 1);

  return ver_specific_len;

 err:
  return -1;
}

/** Parse the version-specific parts of a v2 INTRODUCE1 or INTRODUCE2 cell
 */

static ssize_t
rend_service_parse_intro_for_v2(
    rend_intro_cell_t *intro,
    const uint8_t *buf,
    size_t plaintext_len,
    char **err_msg_out)
{
  unsigned int klen;
  extend_info_t *extend_info = NULL;
  ssize_t ver_specific_len;

  /*
   * We accept version 3 too so that the v3 parser can call this with
   * an adjusted buffer for the latter part of a v3 cell, which is
   * identical to a v2 cell.
   */
  if (!(intro->version == 2 ||
        intro->version == 3)) {
    if (err_msg_out)
      tor_asprintf(err_msg_out,
                   "rend_service_parse_intro_for_v2() called with "
                   "bad version %d on INTRODUCE%d cell (this is a bug)",
                   intro->version,
                   (int)(intro->type));
    goto err;
  }

  /* 7 == version, IP and port, DIGEST_LEN == id, 2 == key length */
  if (plaintext_len < 7 + DIGEST_LEN + 2) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "truncated plaintext of encrypted parted of "
                   "version %d INTRODUCE%d cell",
                   intro->version,
                   (int)(intro->type));
    }

    goto err;
  }

  extend_info = tor_malloc_zero(sizeof(extend_info_t));
  tor_addr_from_ipv4n(&extend_info->addr, get_uint32(buf + 1));
  extend_info->port = ntohs(get_uint16(buf + 5));
  memcpy(extend_info->identity_digest, buf + 7, DIGEST_LEN);
  extend_info->nickname[0] = '$';
  base16_encode(extend_info->nickname + 1, sizeof(extend_info->nickname) - 1,
                extend_info->identity_digest, DIGEST_LEN);
  klen = ntohs(get_uint16(buf + 7 + DIGEST_LEN));

  /* 7 == version, IP and port, DIGEST_LEN == id, 2 == key length */
  if (plaintext_len < 7 + DIGEST_LEN + 2 + klen) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "truncated plaintext of encrypted parted of "
                   "version %d INTRODUCE%d cell",
                   intro->version,
                   (int)(intro->type));
    }

    goto err;
  }

  extend_info->onion_key =
    crypto_pk_asn1_decode((const char *)(buf + 7 + DIGEST_LEN + 2), klen);
  if (!extend_info->onion_key) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "error decoding onion key in version %d "
                   "INTRODUCE%d cell",
                   intro->version,
                   (intro->type));
    }

    goto err;
  }
  if (128 != crypto_pk_keysize(extend_info->onion_key)) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "invalid onion key size in version %d INTRODUCE%d cell",
                   intro->version,
                   (intro->type));
    }

    goto err;
  }

  ver_specific_len = 7+DIGEST_LEN+2+klen;

  if (intro->version == 2) intro->u.v2.extend_info = extend_info;
  else intro->u.v3.extend_info = extend_info;

  return ver_specific_len;

 err:
  extend_info_free(extend_info);

  return -1;
}

/** Parse the version-specific parts of a v3 INTRODUCE1 or INTRODUCE2 cell
 */

static ssize_t
rend_service_parse_intro_for_v3(
    rend_intro_cell_t *intro,
    const uint8_t *buf,
    size_t plaintext_len,
    char **err_msg_out)
{
  ssize_t adjust, v2_ver_specific_len, ts_offset;

  /* This should only be called on v3 cells */
  if (intro->version != 3) {
    if (err_msg_out)
      tor_asprintf(err_msg_out,
                   "rend_service_parse_intro_for_v3() called with "
                   "bad version %d on INTRODUCE%d cell (this is a bug)",
                   intro->version,
                   (int)(intro->type));
    goto err;
  }

  /*
   * Check that we have at least enough to get auth_len:
   *
   * 1 octet for version, 1 for auth_type, 2 for auth_len
   */
  if (plaintext_len < 4) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "truncated plaintext of encrypted parted of "
                   "version %d INTRODUCE%d cell",
                   intro->version,
                   (int)(intro->type));
    }

    goto err;
  }

  /*
   * The rend_client_send_introduction() function over in rendclient.c is
   * broken (i.e., fails to match the spec) in such a way that we can't
   * change it without breaking the protocol.  Specifically, it doesn't
   * emit auth_len when auth-type is REND_NO_AUTH, so everything is off
   * by two bytes after that.  Calculate ts_offset and do everything from
   * the timestamp on relative to that to handle this dain bramage.
   */

  intro->u.v3.auth_type = buf[1];
  if (intro->u.v3.auth_type != REND_NO_AUTH) {
    intro->u.v3.auth_len = ntohs(get_uint16(buf + 2));
    ts_offset = 4 + intro->u.v3.auth_len;
  } else {
    intro->u.v3.auth_len = 0;
    ts_offset = 2;
  }

  /* Check that auth len makes sense for this auth type */
  if (intro->u.v3.auth_type == REND_BASIC_AUTH ||
      intro->u.v3.auth_type == REND_STEALTH_AUTH) {
      if (intro->u.v3.auth_len != REND_DESC_COOKIE_LEN) {
        if (err_msg_out) {
          tor_asprintf(err_msg_out,
                       "wrong auth data size %d for INTRODUCE%d cell, "
                       "should be %d",
                       (int)(intro->u.v3.auth_len),
                       (int)(intro->type),
                       REND_DESC_COOKIE_LEN);
        }

        goto err;
      }
  }

  /* Check that we actually have everything up through the timestamp */
  if (plaintext_len < (size_t)(ts_offset)+4) {
    if (err_msg_out) {
      tor_asprintf(err_msg_out,
                   "truncated plaintext of encrypted parted of "
                   "version %d INTRODUCE%d cell",
                   intro->version,
                   (int)(intro->type));
    }

    goto err;
  }

  if (intro->u.v3.auth_type != REND_NO_AUTH &&
      intro->u.v3.auth_len > 0) {
    /* Okay, we can go ahead and copy auth_data */
    intro->u.v3.auth_data = tor_malloc(intro->u.v3.auth_len);
    /*
     * We know we had an auth_len field in this case, so 4 is
     * always right.
     */
    memcpy(intro->u.v3.auth_data, buf + 4, intro->u.v3.auth_len);
  }

  /*
   * From here on, the format is as in v2, so we call the v2 parser with
   * adjusted buffer and length.  We are 4 + ts_offset octets in, but the
   * v2 parser expects to skip over a version byte at the start, so we
   * adjust by 3 + ts_offset.
   */
  adjust = 3 + ts_offset;

  v2_ver_specific_len =
    rend_service_parse_intro_for_v2(intro,
                                    buf + adjust, plaintext_len - adjust,
                                    err_msg_out);

  /* Success in v2 parser */
  if (v2_ver_specific_len >= 0) return v2_ver_specific_len + adjust;
  /* Failure in v2 parser; it will have provided an err_msg */
  else return v2_ver_specific_len;

 err:
  return -1;
}

/** Table of parser functions for version-specific parts of an INTRODUCE2
 * cell.
 */

static ssize_t
  (*intro_version_handlers[])(
    rend_intro_cell_t *,
    const uint8_t *,
    size_t,
    char **) =
{ rend_service_parse_intro_for_v0_or_v1,
  rend_service_parse_intro_for_v0_or_v1,
  rend_service_parse_intro_for_v2,
  rend_service_parse_intro_for_v3 };

/** Decrypt the encrypted part of an INTRODUCE1 or INTRODUCE2 cell,
 * return 0 if successful, or < 0 and write an error message to
 * *err_msg_out if provided.
 */

int
rend_service_decrypt_intro(
    rend_intro_cell_t *intro,
    crypto_pk_t *key,
    char **err_msg_out)
{
  char *err_msg = NULL;
  uint8_t key_digest[DIGEST_LEN];
  char service_id[REND_SERVICE_ID_LEN_BASE32+1];
  ssize_t key_len;
  uint8_t buf[RELAY_PAYLOAD_SIZE];
  int result, status = -1;

  if (!intro || !key) {
    if (err_msg_out) {
      err_msg =
        tor_strdup("rend_service_decrypt_intro() called with bad "
                   "parameters");
    }

    status = -2;
    goto err;
  }

  /* Make sure we have ciphertext */
  if (!(intro->ciphertext) || intro->ciphertext_len <= 0) {
    if (err_msg_out) {
      tor_asprintf(&err_msg,
                   "rend_intro_cell_t was missing ciphertext for "
                   "INTRODUCE%d cell",
                   (int)(intro->type));
    }
    status = -3;
    goto err;
  }

  /* Check that this cell actually matches this service key */

  /* first DIGEST_LEN bytes of request is intro or service pk digest */
  crypto_pk_get_digest(key, (char *)key_digest);
  if (tor_memneq(key_digest, intro->pk, DIGEST_LEN)) {
    if (err_msg_out) {
      base32_encode(service_id, REND_SERVICE_ID_LEN_BASE32 + 1,
                    (char*)(intro->pk), REND_SERVICE_ID_LEN);
      tor_asprintf(&err_msg,
                   "got an INTRODUCE%d cell for the wrong service (%s)",
                   (int)(intro->type),
                   escaped(service_id));
    }

    status = -4;
    goto err;
  }

  /* Make sure the encrypted part is long enough to decrypt */

  key_len = crypto_pk_keysize(key);
  if (intro->ciphertext_len < key_len) {
    if (err_msg_out) {
      tor_asprintf(&err_msg,
                   "got an INTRODUCE%d cell with a truncated PK-encrypted "
                   "part",
                   (int)(intro->type));
    }

    status = -5;
    goto err;
  }

  /* Decrypt the encrypted part */

  note_crypto_pk_op(REND_SERVER);
  result =
    crypto_pk_private_hybrid_decrypt(
       key, (char *)buf, sizeof(buf),
       (const char *)(intro->ciphertext), intro->ciphertext_len,
       PK_PKCS1_OAEP_PADDING, 1);
  if (result < 0) {
    if (err_msg_out) {
      tor_asprintf(&err_msg,
                   "couldn't decrypt INTRODUCE%d cell",
                   (int)(intro->type));
    }
    status = -6;
    goto err;
  }
  intro->plaintext_len = result;
  intro->plaintext = tor_malloc(intro->plaintext_len);
  memcpy(intro->plaintext, buf, intro->plaintext_len);

  status = 0;

  goto done;

 err:
  if (err_msg_out && !err_msg) {
    tor_asprintf(&err_msg,
                 "unknown INTRODUCE%d error decrypting encrypted part",
                 intro ? (int)(intro->type) : -1);
  }

 done:
  if (err_msg_out) *err_msg_out = err_msg;
  else tor_free(err_msg);

  /* clean up potentially sensitive material */
  memwipe(buf, 0, sizeof(buf));
  memwipe(key_digest, 0, sizeof(key_digest));
  memwipe(service_id, 0, sizeof(service_id));

  return status;
}

/** Parse the plaintext of the encrypted part of an INTRODUCE1 or
 * INTRODUCE2 cell, return 0 if successful, or < 0 and write an error
 * message to *err_msg_out if provided.
 */

int
rend_service_parse_intro_plaintext(
    rend_intro_cell_t *intro,
    char **err_msg_out)
{
  char *err_msg = NULL;
  ssize_t ver_specific_len, ver_invariant_len;
  uint8_t version;
  int status = -1;

  if (!intro) {
    if (err_msg_out) {
      err_msg =
        tor_strdup("rend_service_parse_intro_plaintext() called with NULL "
                   "rend_intro_cell_t");
    }

    status = -2;
    goto err;
  }

  /* Check that we have plaintext */
  if (!(intro->plaintext) || intro->plaintext_len <= 0) {
    if (err_msg_out) {
      err_msg = tor_strdup("rend_intro_cell_t was missing plaintext");
    }
    status = -3;
    goto err;
  }

  /* In all formats except v0, the first byte is a version number */
  version = intro->plaintext[0];

  /* v0 has no version byte (stupid...), so handle it as a fallback */
  if (version > 3) version = 0;

  /* Copy the version into the parsed cell structure */
  intro->version = version;

  /* Call the version-specific parser from the table */
  ver_specific_len =
    intro_version_handlers[version](intro,
                                    intro->plaintext, intro->plaintext_len,
                                    &err_msg);
  if (ver_specific_len < 0) {
    status = -4;
    goto err;
  }

  /** The rendezvous cookie and Diffie-Hellman stuff are version-invariant
   * and at the end of the plaintext of the encrypted part of the cell.
   */

  ver_invariant_len = intro->plaintext_len - ver_specific_len;
  if (ver_invariant_len < REND_COOKIE_LEN + DH_KEY_LEN) {
    tor_asprintf(&err_msg,
        "decrypted plaintext of INTRODUCE%d cell was truncated (%ld bytes)",
        (int)(intro->type),
        (long)(intro->plaintext_len));
    status = -5;
    goto err;
  } else if (ver_invariant_len > REND_COOKIE_LEN + DH_KEY_LEN) {
    tor_asprintf(&err_msg,
        "decrypted plaintext of INTRODUCE%d cell was too long (%ld bytes)",
        (int)(intro->type),
        (long)(intro->plaintext_len));
    status = -6;
    goto err;
  } else {
    memcpy(intro->rc,
           intro->plaintext + ver_specific_len,
           REND_COOKIE_LEN);
    memcpy(intro->dh,
           intro->plaintext + ver_specific_len + REND_COOKIE_LEN,
           DH_KEY_LEN);
  }

  /* Flag it as being fully parsed */
  intro->parsed = 1;

  status = 0;
  goto done;

 err:
  if (err_msg_out && !err_msg) {
    tor_asprintf(&err_msg,
                 "unknown INTRODUCE%d error parsing encrypted part",
                 intro ? (int)(intro->type) : -1);
  }

 done:
  if (err_msg_out) *err_msg_out = err_msg;
  else tor_free(err_msg);

  return status;
}

/** Do validity checks on a parsed intro cell after decryption; some of
 * these are not done in rend_service_parse_intro_plaintext() itself because
 * they depend on a lot of other state and would make it hard to unit test.
 * Returns >= 0 if successful or < 0 if the intro cell is invalid, and
 * optionally writes out an error message for logging.  If an err_msg
 * pointer is provided, it is the caller's responsibility to free any
 * provided message.
 */

int
rend_service_validate_intro_late(const rend_intro_cell_t *intro,
                                 char **err_msg_out)
{
  int status = 0;

  if (!intro) {
    if (err_msg_out)
      *err_msg_out =
        tor_strdup("NULL intro cell passed to "
                   "rend_service_validate_intro_late()");

    status = -1;
    goto err;
  }

  if (intro->version == 3 && intro->parsed) {
    if (!(intro->u.v3.auth_type == REND_NO_AUTH ||
          intro->u.v3.auth_type == REND_BASIC_AUTH ||
          intro->u.v3.auth_type == REND_STEALTH_AUTH)) {
      /* This is an informative message, not an error, as in the old code */
      if (err_msg_out)
        tor_asprintf(err_msg_out,
                     "unknown authorization type %d",
                     intro->u.v3.auth_type);
    }
  }

 err:
  return status;
}

/** Called when we fail building a rendezvous circuit at some point other
 * than the last hop: launches a new circuit to the same rendezvous point.
 */
void
rend_service_relaunch_rendezvous(origin_circuit_t *oldcirc)
{
  origin_circuit_t *newcirc;
  cpath_build_state_t *newstate, *oldstate;

  tor_assert(oldcirc->base_.purpose == CIRCUIT_PURPOSE_S_CONNECT_REND);

  /* Don't relaunch the same rend circ twice. */
  if (oldcirc->hs_service_side_rend_circ_has_been_relaunched) {
    log_info(LD_REND, "Rendezvous circuit to %s has already been relaunched; "
             "not relaunching it again.",
             oldcirc->build_state ?
             safe_str(extend_info_describe(oldcirc->build_state->chosen_exit))
             : "*unknown*");
    return;
  }
  oldcirc->hs_service_side_rend_circ_has_been_relaunched = 1;

  if (!oldcirc->build_state ||
      oldcirc->build_state->failure_count > MAX_REND_FAILURES ||
      oldcirc->build_state->expiry_time < time(NULL)) {
    log_info(LD_REND,
             "Attempt to build circuit to %s for rendezvous has failed "
             "too many times or expired; giving up.",
             oldcirc->build_state ?
             safe_str(extend_info_describe(oldcirc->build_state->chosen_exit))
             : "*unknown*");
    return;
  }

  oldstate = oldcirc->build_state;
  tor_assert(oldstate);

  if (oldstate->service_pending_final_cpath_ref == NULL) {
    log_info(LD_REND,"Skipping relaunch of circ that failed on its first hop. "
             "Initiator will retry.");
    return;
  }

  log_info(LD_REND,"Reattempting rendezvous circuit to '%s'",
           safe_str(extend_info_describe(oldstate->chosen_exit)));

  /* You'd think Single Onion Services would want to retry the rendezvous
   * using a direct connection. But if it's blocked by a firewall, or the
   * service is IPv6-only, or the rend point avoiding becoming a one-hop
   * proxy, we need a 3-hop connection. */
  newcirc = circuit_launch_by_extend_info(CIRCUIT_PURPOSE_S_CONNECT_REND,
                            oldstate->chosen_exit,
                            CIRCLAUNCH_NEED_CAPACITY|CIRCLAUNCH_IS_INTERNAL);

  if (!newcirc) {
    log_warn(LD_REND,"Couldn't relaunch rendezvous circuit to '%s'.",
             safe_str(extend_info_describe(oldstate->chosen_exit)));
    return;
  }
  newstate = newcirc->build_state;
  tor_assert(newstate);
  newstate->failure_count = oldstate->failure_count+1;
  newstate->expiry_time = oldstate->expiry_time;
  newstate->service_pending_final_cpath_ref =
    oldstate->service_pending_final_cpath_ref;
  ++(newstate->service_pending_final_cpath_ref->refcount);

  newcirc->rend_data = rend_data_dup(oldcirc->rend_data);
}

/** Launch a circuit to serve as an introduction point for the service
 * <b>service</b> at the introduction point <b>nickname</b>
 */
static int
rend_service_launch_establish_intro(rend_service_t *service,
                                    rend_intro_point_t *intro)
{
  origin_circuit_t *launched;
  int flags = CIRCLAUNCH_NEED_UPTIME|CIRCLAUNCH_IS_INTERNAL;
  const or_options_t *options = get_options();
  extend_info_t *launch_ei = intro->extend_info;
  extend_info_t *direct_ei = NULL;

  /* Are we in single onion mode? */
  if (rend_service_allow_non_anonymous_connection(options)) {
    /* Do we have a descriptor for the node?
     * We've either just chosen it from the consensus, or we've just reviewed
     * our intro points to see which ones are still valid, and deleted the ones
     * that aren't in the consensus any more. */
    const node_t *node = node_get_by_id(launch_ei->identity_digest);
    if (BUG(!node)) {
      /* The service has kept an intro point after it went missing from the
       * consensus. If we did anything else here, it would be a consensus
       * distinguisher. Which are less of an issue for single onion services,
       * but still a bug. */
      return -1;
    }
    /* Can we connect to the node directly? If so, replace launch_ei
     * (a multi-hop extend_info) with one suitable for direct connection. */
    if (rend_service_use_direct_connection_node(options, node)) {
      direct_ei = extend_info_from_node(node, 1);
      if (BUG(!direct_ei)) {
        /* rend_service_use_direct_connection_node and extend_info_from_node
         * disagree about which addresses on this node are permitted. This
         * should never happen. Avoiding the connection is a safe response. */
        return -1;
      }
      flags = flags | CIRCLAUNCH_ONEHOP_TUNNEL;
      launch_ei = direct_ei;
    }
  }
  /* launch_ei is either intro->extend_info, or has been replaced with a valid
   * extend_info for single onion service direct connection. */
  tor_assert(launch_ei);
  /* We must have the same intro when making a direct connection. */
  tor_assert(tor_memeq(intro->extend_info->identity_digest,
                       launch_ei->identity_digest,
                       DIGEST_LEN));

  log_info(LD_REND,
           "Launching circuit to introduction point %s%s%s for service %s",
           safe_str_client(extend_info_describe(intro->extend_info)),
           direct_ei ? " via direct address " : "",
           direct_ei ? safe_str_client(extend_info_describe(direct_ei)) : "",
           service->service_id);

  rep_hist_note_used_internal(time(NULL), 1, 0);

  ++service->n_intro_circuits_launched;
  launched = circuit_launch_by_extend_info(CIRCUIT_PURPOSE_S_ESTABLISH_INTRO,
                             launch_ei, flags);

  if (!launched) {
    log_info(LD_REND,
             "Can't launch circuit to establish introduction at %s%s%s.",
             safe_str_client(extend_info_describe(intro->extend_info)),
             direct_ei ? " via direct address " : "",
             direct_ei ? safe_str_client(extend_info_describe(direct_ei)) : ""
             );
    extend_info_free(direct_ei);
    return -1;
  }
  /* We must have the same exit node even if cannibalized or direct connection.
   */
  tor_assert(tor_memeq(intro->extend_info->identity_digest,
                       launched->build_state->chosen_exit->identity_digest,
                       DIGEST_LEN));

  launched->rend_data = rend_data_service_create(service->service_id,
                                                 service->pk_digest, NULL,
                                                 service->auth_type);
  launched->intro_key = crypto_pk_dup_key(intro->intro_key);
  if (launched->base_.state == CIRCUIT_STATE_OPEN)
    rend_service_intro_has_opened(launched);
  extend_info_free(direct_ei);
  return 0;
}

/** Return the number of introduction points that are established for the
 * given service. */
static unsigned int
count_established_intro_points(const rend_service_t *service)
{
  unsigned int num = 0;

  SMARTLIST_FOREACH(service->intro_nodes, rend_intro_point_t *, intro,
    num += intro->circuit_established
  );
  return num;
}

/** Return the number of introduction points that are or are being
 * established for the given service. This function iterates over all
 * circuit and count those that are linked to the service and are waiting
 * for the intro point to respond. */
static unsigned int
count_intro_point_circuits(const rend_service_t *service)
{
  unsigned int num_ipos = 0;
  SMARTLIST_FOREACH_BEGIN(circuit_get_global_list(), circuit_t *, circ) {
    if (!circ->marked_for_close &&
        circ->state == CIRCUIT_STATE_OPEN &&
        (circ->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
         circ->purpose == CIRCUIT_PURPOSE_S_INTRO)) {
      origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(circ);
      if (oc->rend_data &&
          !rend_cmp_service_ids(service->service_id,
                                oc->rend_data->onion_address))
        num_ipos++;
    }
  }
  SMARTLIST_FOREACH_END(circ);
  return num_ipos;
}

/** Called when we're done building a circuit to an introduction point:
 *  sends a RELAY_ESTABLISH_INTRO cell.
 */
void
rend_service_intro_has_opened(origin_circuit_t *circuit)
{
  rend_service_t *service;
  size_t len;
  int r;
  char buf[RELAY_PAYLOAD_SIZE];
  char auth[DIGEST_LEN + 9];
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  int reason = END_CIRC_REASON_TORPROTOCOL;

  tor_assert(circuit->base_.purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO);
  assert_circ_anonymity_ok(circuit, get_options());
  tor_assert(circuit->cpath);
  tor_assert(circuit->rend_data);

  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32+1,
                circuit->rend_data->rend_pk_digest, REND_SERVICE_ID_LEN);

  service = rend_service_get_by_pk_digest(
                circuit->rend_data->rend_pk_digest);
  if (!service) {
    log_warn(LD_REND, "Unrecognized service ID %s on introduction circuit %u.",
             safe_str_client(serviceid), (unsigned)circuit->base_.n_circ_id);
    reason = END_CIRC_REASON_NOSUCHSERVICE;
    goto err;
  }

  /* If we already have enough introduction circuits for this service,
   * redefine this one as a general circuit or close it, depending.
   * Substract the amount of expiring nodes here since the circuits are
   * still opened. */
  if ((count_intro_point_circuits(service) -
       smartlist_len(service->expiring_nodes)) >
      service->n_intro_points_wanted) {
    const or_options_t *options = get_options();
    /* Remove the intro point associated with this circuit, it's being
     * repurposed or closed thus cleanup memory. */
    rend_intro_point_t *intro = find_intro_point(circuit);
    if (intro != NULL) {
      smartlist_remove(service->intro_nodes, intro);
      rend_intro_point_free(intro);
    }

    if (options->ExcludeNodes) {
      /* XXXX in some future version, we can test whether the transition is
         allowed or not given the actual nodes in the circuit.  But for now,
         this case, we might as well close the thing. */
      log_info(LD_CIRC|LD_REND, "We have just finished an introduction "
               "circuit, but we already have enough.  Closing it.");
      reason = END_CIRC_REASON_NONE;
      goto err;
    } else {
      tor_assert(circuit->build_state->is_internal);
      log_info(LD_CIRC|LD_REND, "We have just finished an introduction "
               "circuit, but we already have enough. Redefining purpose to "
               "general; leaving as internal.");

      circuit_change_purpose(TO_CIRCUIT(circuit), CIRCUIT_PURPOSE_C_GENERAL);

      {
        rend_data_t *rend_data = circuit->rend_data;
        circuit->rend_data = NULL;
        rend_data_free(rend_data);
      }
      {
        crypto_pk_t *intro_key = circuit->intro_key;
        circuit->intro_key = NULL;
        crypto_pk_free(intro_key);
      }

      circuit_has_opened(circuit);
      goto done;
    }
  }

  log_info(LD_REND,
           "Established circuit %u as introduction point for service %s",
           (unsigned)circuit->base_.n_circ_id, serviceid);
  circuit_log_path(LOG_INFO, LD_REND, circuit);

  /* Use the intro key instead of the service key in ESTABLISH_INTRO. */
  crypto_pk_t *intro_key = circuit->intro_key;
  /* Build the payload for a RELAY_ESTABLISH_INTRO cell. */
  r = crypto_pk_asn1_encode(intro_key, buf+2,
                            RELAY_PAYLOAD_SIZE-2);
  if (r < 0) {
    log_warn(LD_BUG, "Internal error; failed to establish intro point.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }
  len = r;
  set_uint16(buf, htons((uint16_t)len));
  len += 2;
  memcpy(auth, circuit->cpath->prev->rend_circ_nonce, DIGEST_LEN);
  memcpy(auth+DIGEST_LEN, "INTRODUCE", 9);
  if (crypto_digest(buf+len, auth, DIGEST_LEN+9))
    goto err;
  len += 20;
  note_crypto_pk_op(REND_SERVER);
  r = crypto_pk_private_sign_digest(intro_key, buf+len, sizeof(buf)-len,
                                    buf, len);
  if (r<0) {
    log_warn(LD_BUG, "Internal error: couldn't sign introduction request.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }
  len += r;

  if (relay_send_command_from_edge(0, TO_CIRCUIT(circuit),
                                   RELAY_COMMAND_ESTABLISH_INTRO,
                                   buf, len, circuit->cpath->prev)<0) {
    log_info(LD_GENERAL,
             "Couldn't send introduction request for service %s on circuit %u",
             serviceid, (unsigned)circuit->base_.n_circ_id);
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }

  /* We've attempted to use this circuit */
  pathbias_count_use_attempt(circuit);

  goto done;

 err:
  circuit_mark_for_close(TO_CIRCUIT(circuit), reason);
 done:
  memwipe(buf, 0, sizeof(buf));
  memwipe(auth, 0, sizeof(auth));
  memwipe(serviceid, 0, sizeof(serviceid));

  return;
}

/** Called when we get an INTRO_ESTABLISHED cell; mark the circuit as a
 * live introduction point, and note that the service descriptor is
 * now out-of-date. */
int
rend_service_intro_established(origin_circuit_t *circuit,
                               const uint8_t *request,
                               size_t request_len)
{
  rend_service_t *service;
  rend_intro_point_t *intro;
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  (void) request;
  (void) request_len;

  if (circuit->base_.purpose != CIRCUIT_PURPOSE_S_ESTABLISH_INTRO) {
    log_warn(LD_PROTOCOL,
             "received INTRO_ESTABLISHED cell on non-intro circuit.");
    goto err;
  }
  tor_assert(circuit->rend_data);
  service = rend_service_get_by_pk_digest(
                circuit->rend_data->rend_pk_digest);
  if (!service) {
    log_warn(LD_REND, "Unknown service on introduction circuit %u.",
             (unsigned)circuit->base_.n_circ_id);
    goto err;
  }
  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32 + 1,
                circuit->rend_data->rend_pk_digest, REND_SERVICE_ID_LEN);
  /* We've just successfully established a intro circuit to one of our
   * introduction point, account for it. */
  intro = find_intro_point(circuit);
  if (intro == NULL) {
    log_warn(LD_REND,
             "Introduction circuit established without a rend_intro_point_t "
             "object for service %s on circuit %u",
             safe_str_client(serviceid), (unsigned)circuit->base_.n_circ_id);
    goto err;
  }
  intro->circuit_established = 1;
  /* We might not have every introduction point ready but at this point we
   * know that the descriptor needs to be uploaded. */
  service->desc_is_dirty = time(NULL);
  circuit_change_purpose(TO_CIRCUIT(circuit), CIRCUIT_PURPOSE_S_INTRO);

  log_info(LD_REND,
           "Received INTRO_ESTABLISHED cell on circuit %u for service %s",
           (unsigned)circuit->base_.n_circ_id, serviceid);

  /* Getting a valid INTRODUCE_ESTABLISHED means we've successfully
   * used the circ */
  pathbias_mark_use_success(circuit);

  return 0;
 err:
  circuit_mark_for_close(TO_CIRCUIT(circuit), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}

/** Called once a circuit to a rendezvous point is established: sends a
 *  RELAY_COMMAND_RENDEZVOUS1 cell.
 */
void
rend_service_rendezvous_has_opened(origin_circuit_t *circuit)
{
  rend_service_t *service;
  char buf[RELAY_PAYLOAD_SIZE];
  crypt_path_t *hop;
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  char hexcookie[9];
  int reason;

  tor_assert(circuit->base_.purpose == CIRCUIT_PURPOSE_S_CONNECT_REND);
  tor_assert(circuit->cpath);
  tor_assert(circuit->build_state);
  assert_circ_anonymity_ok(circuit, get_options());
  tor_assert(circuit->rend_data);

  /* Declare the circuit dirty to avoid reuse, and for path-bias */
  if (!circuit->base_.timestamp_dirty)
    circuit->base_.timestamp_dirty = time(NULL);

  /* This may be redundant */
  pathbias_count_use_attempt(circuit);

  hop = circuit->build_state->service_pending_final_cpath_ref->cpath;

  base16_encode(hexcookie,9,circuit->rend_data->rend_cookie,4);
  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32+1,
                circuit->rend_data->rend_pk_digest, REND_SERVICE_ID_LEN);

  log_info(LD_REND,
           "Done building circuit %u to rendezvous with "
           "cookie %s for service %s",
           (unsigned)circuit->base_.n_circ_id, hexcookie, serviceid);
  circuit_log_path(LOG_INFO, LD_REND, circuit);

  /* Clear the 'in-progress HS circ has timed out' flag for
   * consistency with what happens on the client side; this line has
   * no effect on Tor's behaviour. */
  circuit->hs_circ_has_timed_out = 0;

  /* If hop is NULL, another rend circ has already connected to this
   * rend point.  Close this circ. */
  if (hop == NULL) {
    log_info(LD_REND, "Another rend circ has already reached this rend point; "
             "closing this rend circ.");
    reason = END_CIRC_REASON_NONE;
    goto err;
  }

  /* Remove our final cpath element from the reference, so that no
   * other circuit will try to use it.  Store it in
   * pending_final_cpath for now to ensure that it will be freed if
   * our rendezvous attempt fails. */
  circuit->build_state->pending_final_cpath = hop;
  circuit->build_state->service_pending_final_cpath_ref->cpath = NULL;

  service = rend_service_get_by_pk_digest(
                circuit->rend_data->rend_pk_digest);
  if (!service) {
    log_warn(LD_GENERAL, "Internal error: unrecognized service ID on "
             "rendezvous circuit.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }

  /* All we need to do is send a RELAY_RENDEZVOUS1 cell... */
  memcpy(buf, circuit->rend_data->rend_cookie, REND_COOKIE_LEN);
  if (crypto_dh_get_public(hop->rend_dh_handshake_state,
                           buf+REND_COOKIE_LEN, DH_KEY_LEN)<0) {
    log_warn(LD_GENERAL,"Couldn't get DH public key.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }
  memcpy(buf+REND_COOKIE_LEN+DH_KEY_LEN, hop->rend_circ_nonce,
         DIGEST_LEN);

  /* Send the cell */
  if (relay_send_command_from_edge(0, TO_CIRCUIT(circuit),
                                   RELAY_COMMAND_RENDEZVOUS1,
                                   buf, REND_COOKIE_LEN+DH_KEY_LEN+DIGEST_LEN,
                                   circuit->cpath->prev)<0) {
    log_warn(LD_GENERAL, "Couldn't send RENDEZVOUS1 cell.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }

  crypto_dh_free(hop->rend_dh_handshake_state);
  hop->rend_dh_handshake_state = NULL;

  /* Append the cpath entry. */
  hop->state = CPATH_STATE_OPEN;
  /* set the windows to default. these are the windows
   * that the service thinks the client has.
   */
  hop->package_window = circuit_initial_package_window();
  hop->deliver_window = CIRCWINDOW_START;

  onion_append_to_cpath(&circuit->cpath, hop);
  circuit->build_state->pending_final_cpath = NULL; /* prevent double-free */

  /* Change the circuit purpose. */
  circuit_change_purpose(TO_CIRCUIT(circuit), CIRCUIT_PURPOSE_S_REND_JOINED);

  goto done;

 err:
  circuit_mark_for_close(TO_CIRCUIT(circuit), reason);
 done:
  memwipe(buf, 0, sizeof(buf));
  memwipe(serviceid, 0, sizeof(serviceid));
  memwipe(hexcookie, 0, sizeof(hexcookie));

  return;
}

/*
 * Manage introduction points
 */

/** Return the (possibly non-open) introduction circuit ending at
 * <b>intro</b> for the service whose public key is <b>pk_digest</b>.
 * (<b>desc_version</b> is ignored). Return NULL if no such service is
 * found.
 */
static origin_circuit_t *
find_intro_circuit(rend_intro_point_t *intro, const char *pk_digest)
{
  origin_circuit_t *circ = NULL;

  tor_assert(intro);
  while ((circ = circuit_get_next_by_pk_and_purpose(circ,pk_digest,
                                                  CIRCUIT_PURPOSE_S_INTRO))) {
    if (tor_memeq(circ->build_state->chosen_exit->identity_digest,
                intro->extend_info->identity_digest, DIGEST_LEN) &&
        circ->rend_data) {
      return circ;
    }
  }

  circ = NULL;
  while ((circ = circuit_get_next_by_pk_and_purpose(circ,pk_digest,
                                        CIRCUIT_PURPOSE_S_ESTABLISH_INTRO))) {
    if (tor_memeq(circ->build_state->chosen_exit->identity_digest,
                intro->extend_info->identity_digest, DIGEST_LEN) &&
        circ->rend_data) {
      return circ;
    }
  }
  return NULL;
}

/** Return the corresponding introdution point using the circuit <b>circ</b>
 * found in the <b>service</b>. NULL is returned if not found. */
static rend_intro_point_t *
find_expiring_intro_point(rend_service_t *service, origin_circuit_t *circ)
{
  tor_assert(service);
  tor_assert(TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
             TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_S_INTRO);

  SMARTLIST_FOREACH(service->expiring_nodes, rend_intro_point_t *,
                    intro_point,
    if (crypto_pk_eq_keys(intro_point->intro_key, circ->intro_key)) {
      return intro_point;
  });

  return NULL;
}

/** Return a pointer to the rend_intro_point_t corresponding to the
 * service-side introduction circuit <b>circ</b>. */
static rend_intro_point_t *
find_intro_point(origin_circuit_t *circ)
{
  const char *serviceid;
  rend_service_t *service = NULL;

  tor_assert(TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_S_ESTABLISH_INTRO ||
             TO_CIRCUIT(circ)->purpose == CIRCUIT_PURPOSE_S_INTRO);
  tor_assert(circ->rend_data);
  serviceid = circ->rend_data->onion_address;

  SMARTLIST_FOREACH(rend_service_list, rend_service_t *, s,
    if (tor_memeq(s->service_id, serviceid, REND_SERVICE_ID_LEN_BASE32)) {
      service = s;
      break;
    });

  if (service == NULL) return NULL;

  SMARTLIST_FOREACH(service->intro_nodes, rend_intro_point_t *, intro_point,
    if (crypto_pk_eq_keys(intro_point->intro_key, circ->intro_key)) {
      return intro_point;
    });

  return NULL;
}

/** Upload the rend_encoded_v2_service_descriptor_t's in <b>descs</b>
 * associated with the rend_service_descriptor_t <b>renddesc</b> to
 * the responsible hidden service directories OR the hidden service
 * directories specified by <b>hs_dirs</b>; <b>service_id</b> and
 * <b>seconds_valid</b> are only passed for logging purposes.
 */
void
directory_post_to_hs_dir(rend_service_descriptor_t *renddesc,
                         smartlist_t *descs, smartlist_t *hs_dirs,
                         const char *service_id, int seconds_valid)
{
  int i, j, failed_upload = 0;
  smartlist_t *responsible_dirs = smartlist_new();
  smartlist_t *successful_uploads = smartlist_new();
  routerstatus_t *hs_dir;
  for (i = 0; i < smartlist_len(descs); i++) {
    rend_encoded_v2_service_descriptor_t *desc = smartlist_get(descs, i);
    /** If any HSDirs are specified, they should be used instead of
     *  the responsible directories */
    if (hs_dirs && smartlist_len(hs_dirs) > 0) {
      smartlist_add_all(responsible_dirs, hs_dirs);
    } else {
      /* Determine responsible dirs. */
      if (hid_serv_get_responsible_directories(responsible_dirs,
                                               desc->desc_id) < 0) {
        log_warn(LD_REND, "Could not determine the responsible hidden service "
                          "directories to post descriptors to.");
        control_event_hs_descriptor_upload(service_id,
                                           "UNKNOWN",
                                           "UNKNOWN");
        goto done;
      }
    }
    for (j = 0; j < smartlist_len(responsible_dirs); j++) {
      char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
      char *hs_dir_ip;
      const node_t *node;
      rend_data_t *rend_data;
      hs_dir = smartlist_get(responsible_dirs, j);
      if (smartlist_contains_digest(renddesc->successful_uploads,
                                hs_dir->identity_digest))
        /* Don't upload descriptor if we succeeded in doing so last time. */
        continue;
      node = node_get_by_id(hs_dir->identity_digest);
      if (!node || !node_has_descriptor(node)) {
        log_info(LD_REND, "Not launching upload for for v2 descriptor to "
                          "hidden service directory %s; we don't have its "
                          "router descriptor. Queuing for later upload.",
                 safe_str_client(routerstatus_describe(hs_dir)));
        failed_upload = -1;
        continue;
      }
      /* Send publish request. */

      /* We need the service ID to identify which service did the upload
       * request. Lookup is made in rend_service_desc_has_uploaded(). */
      rend_data = rend_data_client_create(service_id, desc->desc_id, NULL,
                                          REND_NO_AUTH);
      directory_initiate_command_routerstatus_rend(hs_dir,
                                              DIR_PURPOSE_UPLOAD_RENDDESC_V2,
                                                   ROUTER_PURPOSE_GENERAL,
                                                   DIRIND_ANONYMOUS, NULL,
                                                   desc->desc_str,
                                                   strlen(desc->desc_str),
                                                   0, rend_data);
      rend_data_free(rend_data);
      base32_encode(desc_id_base32, sizeof(desc_id_base32),
                    desc->desc_id, DIGEST_LEN);
      hs_dir_ip = tor_dup_ip(hs_dir->addr);
      log_info(LD_REND, "Launching upload for v2 descriptor for "
                        "service '%s' with descriptor ID '%s' with validity "
                        "of %d seconds to hidden service directory '%s' on "
                        "%s:%d.",
               safe_str_client(service_id),
               safe_str_client(desc_id_base32),
               seconds_valid,
               hs_dir->nickname,
               hs_dir_ip,
               hs_dir->or_port);
      control_event_hs_descriptor_upload(service_id,
                                         hs_dir->identity_digest,
                                         desc_id_base32);
      tor_free(hs_dir_ip);
      /* Remember successful upload to this router for next time. */
      if (!smartlist_contains_digest(successful_uploads,
                                     hs_dir->identity_digest))
        smartlist_add(successful_uploads, hs_dir->identity_digest);
    }
    smartlist_clear(responsible_dirs);
  }
  if (!failed_upload) {
    if (renddesc->successful_uploads) {
      SMARTLIST_FOREACH(renddesc->successful_uploads, char *, c, tor_free(c););
      smartlist_free(renddesc->successful_uploads);
      renddesc->successful_uploads = NULL;
    }
    renddesc->all_uploads_performed = 1;
  } else {
    /* Remember which routers worked this time, so that we don't upload the
     * descriptor to them again. */
    if (!renddesc->successful_uploads)
      renddesc->successful_uploads = smartlist_new();
    SMARTLIST_FOREACH(successful_uploads, const char *, c, {
      if (!smartlist_contains_digest(renddesc->successful_uploads, c)) {
        char *hsdir_id = tor_memdup(c, DIGEST_LEN);
        smartlist_add(renddesc->successful_uploads, hsdir_id);
      }
    });
  }
 done:
  smartlist_free(responsible_dirs);
  smartlist_free(successful_uploads);
}

/** Encode and sign an up-to-date service descriptor for <b>service</b>,
 * and upload it/them to the responsible hidden service directories.
 */
static void
upload_service_descriptor(rend_service_t *service)
{
  time_t now = time(NULL);
  int rendpostperiod;
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  int uploaded = 0;

  rendpostperiod = get_options()->RendPostPeriod;

  networkstatus_t *c = networkstatus_get_latest_consensus();
  if (c && smartlist_len(c->routerstatus_list) > 0) {
    int seconds_valid, i, j, num_descs;
    smartlist_t *descs = smartlist_new();
    smartlist_t *client_cookies = smartlist_new();
    /* Either upload a single descriptor (including replicas) or one
     * descriptor for each authorized client in case of authorization
     * type 'stealth'. */
    num_descs = service->auth_type == REND_STEALTH_AUTH ?
                    smartlist_len(service->clients) : 1;
    for (j = 0; j < num_descs; j++) {
      crypto_pk_t *client_key = NULL;
      rend_authorized_client_t *client = NULL;
      smartlist_clear(client_cookies);
      switch (service->auth_type) {
        case REND_NO_AUTH:
          /* Do nothing here. */
          break;
        case REND_BASIC_AUTH:
          SMARTLIST_FOREACH(service->clients, rend_authorized_client_t *,
              cl, smartlist_add(client_cookies, cl->descriptor_cookie));
          break;
        case REND_STEALTH_AUTH:
          client = smartlist_get(service->clients, j);
          client_key = client->client_key;
          smartlist_add(client_cookies, client->descriptor_cookie);
          break;
      }
      /* Encode the current descriptor. */
      seconds_valid = rend_encode_v2_descriptors(descs, service->desc,
                                                 now, 0,
                                                 service->auth_type,
                                                 client_key,
                                                 client_cookies);
      if (seconds_valid < 0) {
        log_warn(LD_BUG, "Internal error: couldn't encode service "
                 "descriptor; not uploading.");
        smartlist_free(descs);
        smartlist_free(client_cookies);
        return;
      }
      rend_get_service_id(service->desc->pk, serviceid);
      if (get_options()->PublishHidServDescriptors) {
        /* Post the current descriptors to the hidden service directories. */
        log_info(LD_REND, "Launching upload for hidden service %s",
                     serviceid);
        directory_post_to_hs_dir(service->desc, descs, NULL, serviceid,
                                 seconds_valid);
      }
      /* Free memory for descriptors. */
      for (i = 0; i < smartlist_len(descs); i++)
        rend_encoded_v2_service_descriptor_free(smartlist_get(descs, i));
      smartlist_clear(descs);
      /* Update next upload time. */
      if (seconds_valid - REND_TIME_PERIOD_OVERLAPPING_V2_DESCS
          > rendpostperiod)
        service->next_upload_time = now + rendpostperiod;
      else if (seconds_valid < REND_TIME_PERIOD_OVERLAPPING_V2_DESCS)
        service->next_upload_time = now + seconds_valid + 1;
      else
        service->next_upload_time = now + seconds_valid -
            REND_TIME_PERIOD_OVERLAPPING_V2_DESCS + 1;
      /* Post also the next descriptors, if necessary. */
      if (seconds_valid < REND_TIME_PERIOD_OVERLAPPING_V2_DESCS) {
        seconds_valid = rend_encode_v2_descriptors(descs, service->desc,
                                                   now, 1,
                                                   service->auth_type,
                                                   client_key,
                                                   client_cookies);
        if (seconds_valid < 0) {
          log_warn(LD_BUG, "Internal error: couldn't encode service "
                   "descriptor; not uploading.");
          smartlist_free(descs);
          smartlist_free(client_cookies);
          return;
        }
        if (get_options()->PublishHidServDescriptors) {
          directory_post_to_hs_dir(service->desc, descs, NULL, serviceid,
                                   seconds_valid);
        }
        /* Free memory for descriptors. */
        for (i = 0; i < smartlist_len(descs); i++)
          rend_encoded_v2_service_descriptor_free(smartlist_get(descs, i));
        smartlist_clear(descs);
      }
    }
    smartlist_free(descs);
    smartlist_free(client_cookies);
    uploaded = 1;
    if (get_options()->PublishHidServDescriptors) {
      log_info(LD_REND, "Successfully uploaded v2 rend descriptors!");
    } else {
      log_info(LD_REND, "Successfully stored created v2 rend descriptors!");
    }
  }

  /* If not uploaded, try again in one minute. */
  if (!uploaded)
    service->next_upload_time = now + 60;

  /* Unmark dirty flag of this service. */
  service->desc_is_dirty = 0;
}

/** Return the number of INTRODUCE2 cells this hidden service has received
 * from this intro point. */
static int
intro_point_accepted_intro_count(rend_intro_point_t *intro)
{
  return intro->accepted_introduce2_count;
}

/** Return non-zero iff <b>intro</b> should 'expire' now (i.e. we
 * should stop publishing it in new descriptors and eventually close
 * it). */
static int
intro_point_should_expire_now(rend_intro_point_t *intro,
                              time_t now)
{
  tor_assert(intro != NULL);

  if (intro->time_published == -1) {
    /* Don't expire an intro point if we haven't even published it yet. */
    return 0;
  }

  if (intro_point_accepted_intro_count(intro) >=
      intro->max_introductions) {
    /* This intro point has been used too many times.  Expire it now. */
    return 1;
  }

  if (intro->time_to_expire == -1) {
    /* This intro point has been published, but we haven't picked an
     * expiration time for it.  Pick one now. */
    int intro_point_lifetime_seconds =
      crypto_rand_int_range(INTRO_POINT_LIFETIME_MIN_SECONDS,
                            INTRO_POINT_LIFETIME_MAX_SECONDS);

    /* Start the expiration timer now, rather than when the intro
     * point was first published.  There shouldn't be much of a time
     * difference. */
    intro->time_to_expire = now + intro_point_lifetime_seconds;

    return 0;
  }

  /* This intro point has a time to expire set already.  Use it. */
  return (now >= intro->time_to_expire);
}

/** Iterate over intro points in the given service and remove the invalid
 * ones. For an intro point object to be considered invalid, the circuit
 * _and_ node need to have disappeared.
 *
 * If the intro point should expire, it's placed into the expiring_nodes
 * list of the service and removed from the active intro nodes list.
 *
 * If <b>exclude_nodes</b> is not NULL, add the valid nodes to it.
 *
 * If <b>retry_nodes</b> is not NULL, add the valid node to it if the
 * circuit disappeared but the node is still in the consensus. */
static void
remove_invalid_intro_points(rend_service_t *service,
                            smartlist_t *exclude_nodes,
                            smartlist_t *retry_nodes, time_t now)
{
  tor_assert(service);

  SMARTLIST_FOREACH_BEGIN(service->intro_nodes, rend_intro_point_t *,
                          intro) {
    /* Find the introduction point node object. */
    const node_t *node =
      node_get_by_id(intro->extend_info->identity_digest);
    /* Find the intro circuit, this might be NULL. */
    origin_circuit_t *intro_circ =
      find_intro_circuit(intro, service->pk_digest);

    /* Add the valid node to the exclusion list so we don't try to establish
     * an introduction point to it again. */
    if (node && exclude_nodes) {
      smartlist_add(exclude_nodes, (void*) node);
    }

    /* First, make sure we still have a valid circuit for this intro point.
     * If we dont, we'll give up on it and make a new one. */
    if (intro_circ == NULL) {
      log_info(LD_REND, "Attempting to retry on %s as intro point for %s"
               " (circuit disappeared).",
               safe_str_client(extend_info_describe(intro->extend_info)),
               safe_str_client(service->service_id));
      /* We've lost the circuit for this intro point, flag it so it can be
       * accounted for when considiring uploading a descriptor. */
      intro->circuit_established = 0;

      /* Node is gone or we've reached our maximum circuit creationg retry
       * count, clean up everything, we'll find a new one. */
      if (node == NULL ||
          intro->circuit_retries >= MAX_INTRO_POINT_CIRCUIT_RETRIES) {
        rend_intro_point_free(intro);
        SMARTLIST_DEL_CURRENT(service->intro_nodes, intro);
        /* We've just killed the intro point, nothing left to do. */
        continue;
      }

      /* The intro point is still alive so let's try to use it again because
       * we have a published descriptor containing it. Keep the intro point
       * in the intro_nodes list because it's still valid, we are rebuilding
       * a circuit to it. */
      if (retry_nodes) {
        smartlist_add(retry_nodes, intro);
      }
    }
    /* else, the circuit is valid so in both cases, node being alive or not,
     * we leave the circuit and intro point object as is. Closing the
     * circuit here would leak new consensus timing and freeing the intro
     * point object would make the intro circuit unusable. */

    /* Now, check if intro point should expire. If it does, queue it so
     * it can be cleaned up once it has been replaced properly. */
    if (intro_point_should_expire_now(intro, now)) {
      log_info(LD_REND, "Expiring %s as intro point for %s.",
               safe_str_client(extend_info_describe(intro->extend_info)),
               safe_str_client(service->service_id));
      /* We might have put it in the retry list if so, undo. */
      if (retry_nodes) {
        smartlist_remove(retry_nodes, intro);
      }
      smartlist_add(service->expiring_nodes, intro);
      SMARTLIST_DEL_CURRENT(service->intro_nodes, intro);
      /* Intro point is expired, we need a new one thus don't consider it
       * anymore has a valid established intro point. */
      intro->circuit_established = 0;
    }
  } SMARTLIST_FOREACH_END(intro);
}

/** A new descriptor has been successfully uploaded for the given
 * <b>rend_data</b>. Remove and free the expiring nodes from the associated
 * service. */
void
rend_service_desc_has_uploaded(const rend_data_t *rend_data)
{
  rend_service_t *service;

  tor_assert(rend_data);

  service = rend_service_get_by_service_id(rend_data->onion_address);
  if (service == NULL) {
    return;
  }

  SMARTLIST_FOREACH_BEGIN(service->expiring_nodes, rend_intro_point_t *,
                          intro) {
    origin_circuit_t *intro_circ =
      find_intro_circuit(intro, service->pk_digest);
    if (intro_circ != NULL) {
      circuit_mark_for_close(TO_CIRCUIT(intro_circ),
                             END_CIRC_REASON_FINISHED);
    }
    SMARTLIST_DEL_CURRENT(service->expiring_nodes, intro);
    rend_intro_point_free(intro);
  } SMARTLIST_FOREACH_END(intro);
}

/** For every service, check how many intro points it currently has, and:
 *  - Invalidate introdution points based on specific criteria, see
 *  remove_invalid_intro_points comments.
 *  - Pick new intro points as necessary.
 *  - Launch circuits to any new intro points.
 *
 * This is called once a second by the main loop.
 */
void
rend_consider_services_intro_points(void)
{
  int i;
  time_t now;
  const or_options_t *options = get_options();
  /* Are we in single onion mode? */
  const int allow_direct = rend_service_allow_non_anonymous_connection(
                                                                get_options());
  /* List of nodes we need to _exclude_ when choosing a new node to
   * establish an intro point to. */
  smartlist_t *exclude_nodes;
  /* List of nodes we need to retry to build a circuit on them because the
   * node is valid but circuit died. */
  smartlist_t *retry_nodes;

  if (!have_completed_a_circuit())
    return;

  exclude_nodes = smartlist_new();
  retry_nodes = smartlist_new();
  now = time(NULL);

  SMARTLIST_FOREACH_BEGIN(rend_service_list, rend_service_t *, service) {
    int r;
    /* Number of intro points we want to open and add to the intro nodes
     * list of the service. */
    unsigned int n_intro_points_to_open;
    /* Have an unsigned len so we can use it to compare values else gcc is
     * not happy with unmatching signed comparaison. */
    unsigned int intro_nodes_len;
    /* Different service are allowed to have the same introduction point as
     * long as they are on different circuit thus why we clear this list. */
    smartlist_clear(exclude_nodes);
    smartlist_clear(retry_nodes);

    /* This retry period is important here so we don't stress circuit
     * creation. */
    if (now > service->intro_period_started + INTRO_CIRC_RETRY_PERIOD) {
      /* One period has elapsed; we can try building circuits again. */
      service->intro_period_started = now;
      service->n_intro_circuits_launched = 0;
    } else if (service->n_intro_circuits_launched >=
               MAX_INTRO_CIRCS_PER_PERIOD) {
      /* We have failed too many times in this period; wait for the next
       * one before we try again. */
      continue;
    }

    /* Cleanup the invalid intro points and save the node objects, if apply,
     * in the exclude_nodes and retry_nodes list. */
    remove_invalid_intro_points(service, exclude_nodes, retry_nodes, now);

    /* Let's try to rebuild circuit on the nodes we want to retry on. */
    SMARTLIST_FOREACH_BEGIN(retry_nodes, rend_intro_point_t *, intro) {
      r = rend_service_launch_establish_intro(service, intro);
      if (r < 0) {
        log_warn(LD_REND, "Error launching circuit to node %s for service %s.",
                 safe_str_client(extend_info_describe(intro->extend_info)),
                 safe_str_client(service->service_id));
        /* Unable to launch a circuit to that intro point, remove it from
         * the valid list so we can create a new one. */
        smartlist_remove(service->intro_nodes, intro);
        rend_intro_point_free(intro);
        continue;
      }
      intro->circuit_retries++;
    } SMARTLIST_FOREACH_END(intro);

    /* Avoid mismatched signed comparaison below. */
    intro_nodes_len = (unsigned int) smartlist_len(service->intro_nodes);

    /* Quiescent state, no node expiring and we have more or the amount of
     * wanted node for this service. Proceed to the next service. Could be
     * more because we launch two preemptive circuits if our intro nodes
     * list is empty. */
    if (smartlist_len(service->expiring_nodes) == 0 &&
        intro_nodes_len >= service->n_intro_points_wanted) {
      continue;
    }

    /* Number of intro points we want to open which is the wanted amount
     * minus the current amount of valid nodes. */
    n_intro_points_to_open = service->n_intro_points_wanted - intro_nodes_len;
    if (intro_nodes_len == 0) {
      /* We want to end up with n_intro_points_wanted intro points, but if
       * we have no intro points at all (chances are they all cycled or we
       * are starting up), we launch NUM_INTRO_POINTS_EXTRA extra circuits
       * and use the first n_intro_points_wanted that complete. See proposal
       * #155, section 4 for the rationale of this which is purely for
       * performance.
       *
       * The ones after the first n_intro_points_to_open will be converted
       * to 'general' internal circuits in rend_service_intro_has_opened(),
       * and then we'll drop them from the list of intro points. */
      n_intro_points_to_open += NUM_INTRO_POINTS_EXTRA;
    }

    for (i = 0; i < (int) n_intro_points_to_open; i++) {
      const node_t *node;
      rend_intro_point_t *intro;
      router_crn_flags_t flags = CRN_NEED_UPTIME|CRN_NEED_DESC;
      if (get_options()->AllowInvalid_ & ALLOW_INVALID_INTRODUCTION)
        flags |= CRN_ALLOW_INVALID;
      router_crn_flags_t direct_flags = flags;
      direct_flags |= CRN_PREF_ADDR;
      direct_flags |= CRN_DIRECT_CONN;

      node = router_choose_random_node(exclude_nodes,
                                       options->ExcludeNodes,
                                       allow_direct ? direct_flags : flags);
      /* If we are in single onion mode, retry node selection for a 3-hop
       * path */
      if (allow_direct && !node) {
        log_info(LD_REND,
                 "Unable to find an intro point that we can connect to "
                 "directly for %s, falling back to a 3-hop path.",
                 safe_str_client(service->service_id));
        node = router_choose_random_node(exclude_nodes,
                                         options->ExcludeNodes, flags);
      }

      if (!node) {
        log_warn(LD_REND,
                 "We only have %d introduction points established for %s; "
                 "wanted %u.",
                 smartlist_len(service->intro_nodes),
                 safe_str_client(service->service_id),
                 n_intro_points_to_open);
        break;
      }
      /* Add the choosen node to the exclusion list in order to avoid picking
       * it again in the next iteration. */
      smartlist_add(exclude_nodes, (void*)node);
      intro = tor_malloc_zero(sizeof(rend_intro_point_t));
      /* extend_info is for clients, so we want the multi-hop primary ORPort,
       * even if we are a single onion service and intend to connect to it
       * directly ourselves. */
      intro->extend_info = extend_info_from_node(node, 0);
      intro->intro_key = crypto_pk_new();
      const int fail = crypto_pk_generate_key(intro->intro_key);
      tor_assert(!fail);
      intro->time_published = -1;
      intro->time_to_expire = -1;
      intro->max_introductions =
        crypto_rand_int_range(INTRO_POINT_MIN_LIFETIME_INTRODUCTIONS,
                              INTRO_POINT_MAX_LIFETIME_INTRODUCTIONS);
      smartlist_add(service->intro_nodes, intro);
      log_info(LD_REND, "Picked router %s as an intro point for %s.",
               safe_str_client(node_describe(node)),
               safe_str_client(service->service_id));
      /* Establish new introduction circuit to our chosen intro point. */
      r = rend_service_launch_establish_intro(service, intro);
      if (r < 0) {
        log_warn(LD_REND, "Error launching circuit to node %s for service %s.",
                 safe_str_client(extend_info_describe(intro->extend_info)),
                 safe_str_client(service->service_id));
        /* This funcion will be called again by the main loop so this intro
         * point without a intro circuit will be retried on or removed after
         * a maximum number of attempts. */
      }
    }
  } SMARTLIST_FOREACH_END(service);
  smartlist_free(exclude_nodes);
  smartlist_free(retry_nodes);
}

#define MIN_REND_INITIAL_POST_DELAY (30)
#define MIN_REND_INITIAL_POST_DELAY_TESTING (5)

/** Regenerate and upload rendezvous service descriptors for all
 * services, if necessary. If the descriptor has been dirty enough
 * for long enough, definitely upload; else only upload when the
 * periodic timeout has expired.
 *
 * For the first upload, pick a random time between now and two periods
 * from now, and pick it independently for each service.
 */
void
rend_consider_services_upload(time_t now)
{
  int i;
  rend_service_t *service;
  const or_options_t *options = get_options();
  int rendpostperiod = options->RendPostPeriod;
  int rendinitialpostdelay = (options->TestingTorNetwork ?
                              MIN_REND_INITIAL_POST_DELAY_TESTING :
                              MIN_REND_INITIAL_POST_DELAY);

  for (i=0; i < smartlist_len(rend_service_list); ++i) {
    service = smartlist_get(rend_service_list, i);
    if (!service->next_upload_time) { /* never been uploaded yet */
      /* The fixed lower bound of rendinitialpostdelay seconds ensures that
       * the descriptor is stable before being published. See comment below. */
      service->next_upload_time =
        now + rendinitialpostdelay + crypto_rand_int(2*rendpostperiod);
      /* Single Onion Services prioritise availability over hiding their
       * startup time, as their IP address is publicly discoverable anyway.
       */
      if (rend_service_reveal_startup_time(options)) {
        service->next_upload_time = now + rendinitialpostdelay;
      }
    }
    /* Does every introduction points have been established? */
    unsigned int intro_points_ready =
      count_established_intro_points(service) >=
        service->n_intro_points_wanted;
    if (intro_points_ready &&
        (service->next_upload_time < now ||
        (service->desc_is_dirty &&
         service->desc_is_dirty < now-rendinitialpostdelay))) {
      /* if it's time, or if the directory servers have a wrong service
       * descriptor and ours has been stable for rendinitialpostdelay seconds,
       * upload a new one of each format. */
      rend_service_update_descriptor(service);
      upload_service_descriptor(service);
    }
  }
}

/** True if the list of available router descriptors might have changed so
 * that we should have a look whether we can republish previously failed
 * rendezvous service descriptors. */
static int consider_republishing_rend_descriptors = 1;

/** Called when our internal view of the directory has changed, so that we
 * might have router descriptors of hidden service directories available that
 * we did not have before. */
void
rend_hsdir_routers_changed(void)
{
  consider_republishing_rend_descriptors = 1;
}

/** Consider republication of v2 rendezvous service descriptors that failed
 * previously, but without regenerating descriptor contents.
 */
void
rend_consider_descriptor_republication(void)
{
  int i;
  rend_service_t *service;

  if (!consider_republishing_rend_descriptors)
    return;
  consider_republishing_rend_descriptors = 0;

  if (!get_options()->PublishHidServDescriptors)
    return;

  for (i=0; i < smartlist_len(rend_service_list); ++i) {
    service = smartlist_get(rend_service_list, i);
    if (service->desc && !service->desc->all_uploads_performed) {
      /* If we failed in uploading a descriptor last time, try again *without*
       * updating the descriptor's contents. */
      upload_service_descriptor(service);
    }
  }
}

/** Log the status of introduction points for all rendezvous services
 * at log severity <b>severity</b>.
 */
void
rend_service_dump_stats(int severity)
{
  int i,j;
  rend_service_t *service;
  rend_intro_point_t *intro;
  const char *safe_name;
  origin_circuit_t *circ;

  for (i=0; i < smartlist_len(rend_service_list); ++i) {
    service = smartlist_get(rend_service_list, i);
    tor_log(severity, LD_GENERAL, "Service configured in \"%s\":",
        service->directory);
    for (j=0; j < smartlist_len(service->intro_nodes); ++j) {
      intro = smartlist_get(service->intro_nodes, j);
      safe_name = safe_str_client(intro->extend_info->nickname);

      circ = find_intro_circuit(intro, service->pk_digest);
      if (!circ) {
        tor_log(severity, LD_GENERAL, "  Intro point %d at %s: no circuit",
            j, safe_name);
        continue;
      }
      tor_log(severity, LD_GENERAL, "  Intro point %d at %s: circuit is %s",
          j, safe_name, circuit_state_to_string(circ->base_.state));
    }
  }
}

#ifdef HAVE_SYS_UN_H

/** Given <b>ports</b>, a smarlist containing rend_service_port_config_t,
 * add the given <b>p</b>, a AF_UNIX port to the list. Return 0 on success
 * else return -ENOSYS if AF_UNIX is not supported (see function in the
 * #else statement below). */
static int
add_unix_port(smartlist_t *ports, rend_service_port_config_t *p)
{
  tor_assert(ports);
  tor_assert(p);
  tor_assert(p->is_unix_addr);

  smartlist_add(ports, p);
  return 0;
}

/** Given <b>conn</b> set it to use the given port <b>p</b> values. Return 0
 * on success else return -ENOSYS if AF_UNIX is not supported (see function
 * in the #else statement below). */
static int
set_unix_port(edge_connection_t *conn, rend_service_port_config_t *p)
{
  tor_assert(conn);
  tor_assert(p);
  tor_assert(p->is_unix_addr);

  conn->base_.socket_family = AF_UNIX;
  tor_addr_make_unspec(&conn->base_.addr);
  conn->base_.port = 1;
  conn->base_.address = tor_strdup(p->unix_addr);
  return 0;
}

#else /* defined(HAVE_SYS_UN_H) */

static int
set_unix_port(edge_connection_t *conn, rend_service_port_config_t *p)
{
  (void) conn;
  (void) p;
  return -ENOSYS;
}

static int
add_unix_port(smartlist_t *ports, rend_service_port_config_t *p)
{
  (void) ports;
  (void) p;
  return -ENOSYS;
}

#endif /* HAVE_SYS_UN_H */

/** Given <b>conn</b>, a rendezvous exit stream, look up the hidden service for
 * 'circ', and look up the port and address based on conn-\>port.
 * Assign the actual conn-\>addr and conn-\>port. Return -2 on failure
 * for which the circuit should be closed, -1 on other failure,
 * or 0 for success.
 */
int
rend_service_set_connection_addr_port(edge_connection_t *conn,
                                      origin_circuit_t *circ)
{
  rend_service_t *service;
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  smartlist_t *matching_ports;
  rend_service_port_config_t *chosen_port;
  unsigned int warn_once = 0;

  tor_assert(circ->base_.purpose == CIRCUIT_PURPOSE_S_REND_JOINED);
  tor_assert(circ->rend_data);
  log_debug(LD_REND,"beginning to hunt for addr/port");
  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32+1,
                circ->rend_data->rend_pk_digest, REND_SERVICE_ID_LEN);
  service = rend_service_get_by_pk_digest(
                circ->rend_data->rend_pk_digest);
  if (!service) {
    log_warn(LD_REND, "Couldn't find any service associated with pk %s on "
             "rendezvous circuit %u; closing.",
             serviceid, (unsigned)circ->base_.n_circ_id);
    return -2;
  }
  if (service->max_streams_per_circuit > 0) {
    /* Enforce the streams-per-circuit limit, and refuse to provide a
     * mapping if this circuit will exceed the limit. */
#define MAX_STREAM_WARN_INTERVAL 600
    static struct ratelim_t stream_ratelim =
        RATELIM_INIT(MAX_STREAM_WARN_INTERVAL);
    if (circ->rend_data->nr_streams >= service->max_streams_per_circuit) {
      log_fn_ratelim(&stream_ratelim, LOG_WARN, LD_REND,
                     "Maximum streams per circuit limit reached on rendezvous "
                     "circuit %u; %s.  Circuit has %d out of %d streams.",
                     (unsigned)circ->base_.n_circ_id,
                     service->max_streams_close_circuit ?
                       "closing circuit" :
                       "ignoring open stream request",
                     circ->rend_data->nr_streams,
                     service->max_streams_per_circuit);
      return service->max_streams_close_circuit ? -2 : -1;
    }
  }
  matching_ports = smartlist_new();
  SMARTLIST_FOREACH(service->ports, rend_service_port_config_t *, p,
  {
    if (conn->base_.port != p->virtual_port) {
      continue;
    }
    if (!(p->is_unix_addr)) {
      smartlist_add(matching_ports, p);
    } else {
      if (add_unix_port(matching_ports, p)) {
        if (!warn_once) {
         /* Unix port not supported so warn only once. */
          log_warn(LD_REND,
              "Saw AF_UNIX virtual port mapping for port %d on service "
              "%s, which is unsupported on this platform. Ignoring it.",
              conn->base_.port, serviceid);
        }
        warn_once++;
      }
    }
  });
  chosen_port = smartlist_choose(matching_ports);
  smartlist_free(matching_ports);
  if (chosen_port) {
    if (!(chosen_port->is_unix_addr)) {
      /* Get a non-AF_UNIX connection ready for connection_exit_connect() */
      tor_addr_copy(&conn->base_.addr, &chosen_port->real_addr);
      conn->base_.port = chosen_port->real_port;
    } else {
      if (set_unix_port(conn, chosen_port)) {
        /* Simply impossible to end up here else we were able to add a Unix
         * port without AF_UNIX support... ? */
        tor_assert(0);
      }
    }
    return 0;
  }

  log_info(LD_REND,
           "No virtual port mapping exists for port %d on service %s",
           conn->base_.port, serviceid);

  if (service->allow_unknown_ports)
    return -1;
  else
    return -2;
}

/* Are HiddenServiceSingleHopMode and HiddenServiceNonAnonymousMode consistent?
 */
static int
rend_service_non_anonymous_mode_consistent(const or_options_t *options)
{
  /* !! is used to make these options boolean */
  return (!! options->HiddenServiceSingleHopMode ==
          !! options->HiddenServiceNonAnonymousMode);
}

/* Do the options allow onion services to make direct (non-anonymous)
 * connections to introduction or rendezvous points?
 * Must only be called after options_validate_single_onion() has successfully
 * checked onion service option consistency.
 * Returns true if tor is in HiddenServiceSingleHopMode. */
int
rend_service_allow_non_anonymous_connection(const or_options_t *options)
{
  tor_assert(rend_service_non_anonymous_mode_consistent(options));
  return options->HiddenServiceSingleHopMode ? 1 : 0;
}

/* Do the options allow us to reveal the exact startup time of the onion
 * service?
 * Single Onion Services prioritise availability over hiding their
 * startup time, as their IP address is publicly discoverable anyway.
 * Must only be called after options_validate_single_onion() has successfully
 * checked onion service option consistency.
 * Returns true if tor is in non-anonymous hidden service mode. */
int
rend_service_reveal_startup_time(const or_options_t *options)
{
  tor_assert(rend_service_non_anonymous_mode_consistent(options));
  return rend_service_non_anonymous_mode_enabled(options);
}

/* Is non-anonymous mode enabled using the HiddenServiceNonAnonymousMode
 * config option?
 * Must only be called after options_validate_single_onion() has successfully
 * checked onion service option consistency.
 */
int
rend_service_non_anonymous_mode_enabled(const or_options_t *options)
{
  tor_assert(rend_service_non_anonymous_mode_consistent(options));
  return options->HiddenServiceNonAnonymousMode ? 1 : 0;
}

