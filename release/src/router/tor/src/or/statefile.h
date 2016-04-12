/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_STATEFILE_H
#define TOR_STATEFILE_H

MOCK_DECL(or_state_t *,get_or_state,(void));
int did_last_state_file_write_fail(void);
int or_state_save(time_t now);

void save_transport_to_state(const char *transport_name,
                             const tor_addr_t *addr, uint16_t port);
char *get_stored_bindaddr_for_server_transport(const char *transport);
int or_state_load(void);
int or_state_loaded(void);
void or_state_free_all(void);

#ifdef STATEFILE_PRIVATE
STATIC config_line_t *get_transport_in_state_by_name(const char *transport);
STATIC void or_state_free(or_state_t *state);
STATIC or_state_t *or_state_new(void);
#endif

#endif

