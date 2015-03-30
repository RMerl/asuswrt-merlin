/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file config.h
 * \brief Header file for config.c.
 **/

#ifndef TOR_CONFIG_H
#define TOR_CONFIG_H

#include "testsupport.h"

const char *get_dirportfrontpage(void);
MOCK_DECL(const or_options_t *,get_options,(void));
or_options_t *get_options_mutable(void);
int set_options(or_options_t *new_val, char **msg);
void config_free_all(void);
const char *safe_str_client(const char *address);
const char *safe_str(const char *address);
const char *escaped_safe_str_client(const char *address);
const char *escaped_safe_str(const char *address);
const char *get_version(void);
const char *get_short_version(void);
setopt_err_t options_trial_assign(config_line_t *list, int use_defaults,
                                  int clear_first, char **msg);

uint32_t get_last_resolved_addr(void);
int resolve_my_address(int warn_severity, const or_options_t *options,
                       uint32_t *addr_out,
                       const char **method_out, char **hostname_out);
int is_local_addr(const tor_addr_t *addr);
void options_init(or_options_t *options);

#define OPTIONS_DUMP_MINIMAL 1
#define OPTIONS_DUMP_DEFAULTS 2
#define OPTIONS_DUMP_ALL 3
char *options_dump(const or_options_t *options, int how_to_dump);
int options_init_from_torrc(int argc, char **argv);
setopt_err_t options_init_from_string(const char *cf_defaults, const char *cf,
                            int command, const char *command_arg, char **msg);
int option_is_recognized(const char *key);
const char *option_get_canonical_name(const char *key);
config_line_t *option_get_assignment(const or_options_t *options,
                                     const char *key);
int options_save_current(void);
const char *get_torrc_fname(int defaults_fname);
char *options_get_datadir_fname2_suffix(const or_options_t *options,
                                        const char *sub1, const char *sub2,
                                        const char *suffix);
#define get_datadir_fname2_suffix(sub1, sub2, suffix) \
  options_get_datadir_fname2_suffix(get_options(), (sub1), (sub2), (suffix))
/** Return a newly allocated string containing datadir/sub1.  See
 * get_datadir_fname2_suffix.  */
#define get_datadir_fname(sub1) get_datadir_fname2_suffix((sub1), NULL, NULL)
/** Return a newly allocated string containing datadir/sub1/sub2.  See
 * get_datadir_fname2_suffix.  */
#define get_datadir_fname2(sub1,sub2) \
  get_datadir_fname2_suffix((sub1), (sub2), NULL)
/** Return a newly allocated string containing datadir/sub1suffix.  See
 * get_datadir_fname2_suffix. */
#define get_datadir_fname_suffix(sub1, suffix) \
  get_datadir_fname2_suffix((sub1), NULL, (suffix))

int check_or_create_data_subdir(const char *subdir);
int write_to_data_subdir(const char* subdir, const char* fname,
                         const char* str, const char* descr);

int get_num_cpus(const or_options_t *options);

const smartlist_t *get_configured_ports(void);
int get_first_advertised_port_by_type_af(int listener_type,
                                         int address_family);
#define get_primary_or_port() \
  (get_first_advertised_port_by_type_af(CONN_TYPE_OR_LISTENER, AF_INET))
#define get_primary_dir_port() \
  (get_first_advertised_port_by_type_af(CONN_TYPE_DIR_LISTENER, AF_INET))

char *get_first_listener_addrport_string(int listener_type);

int options_need_geoip_info(const or_options_t *options,
                            const char **reason_out);

smartlist_t *get_list_of_ports_to_forward(void);

int getinfo_helper_config(control_connection_t *conn,
                          const char *question, char **answer,
                          const char **errmsg);

const char *tor_get_digests(void);
uint32_t get_effective_bwrate(const or_options_t *options);
uint32_t get_effective_bwburst(const or_options_t *options);

char *get_transport_bindaddr_from_config(const char *transport);

int init_cookie_authentication(const char *fname, const char *header,
                               int cookie_len, int group_readable,
                               uint8_t **cookie_out, int *cookie_is_set_out);

or_options_t *options_new(void);

int config_parse_commandline(int argc, char **argv, int ignore_errors,
                             config_line_t **result,
                             config_line_t **cmdline_result);

void config_register_addressmaps(const or_options_t *options);
/* XXXX024 move to connection_edge.h */
int addressmap_register_auto(const char *from, const char *to,
                             time_t expires,
                             addressmap_entry_source_t addrmap_source,
                             const char **msg);

/** Represents the information stored in a torrc Bridge line. */
typedef struct bridge_line_t {
  tor_addr_t addr; /* The IP address of the bridge. */
  uint16_t port; /* The TCP port of the bridge. */
  char *transport_name; /* The name of the pluggable transport that
                           should be used to connect to the bridge. */
  char digest[DIGEST_LEN]; /* The bridge's identity key digest. */
  smartlist_t *socks_args; /* SOCKS arguments for the pluggable
                               transport proxy. */
} bridge_line_t;

void bridge_line_free(bridge_line_t *bridge_line);
bridge_line_t *parse_bridge_line(const char *line);
smartlist_t *get_options_from_transport_options_line(const char *line,
                                                     const char *transport);
smartlist_t *get_options_for_server_transport(const char *transport);

#ifdef CONFIG_PRIVATE
#ifdef TOR_UNIT_TESTS
extern struct config_format_t options_format;
#endif

STATIC void or_options_free(or_options_t *options);
STATIC int options_validate(or_options_t *old_options,
                            or_options_t *options,
                            or_options_t *default_options,
                            int from_setconf, char **msg);
#endif

#endif

