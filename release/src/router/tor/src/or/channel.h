/* * Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file channel.h
 * \brief Header file for channel.c
 **/

#ifndef TOR_CHANNEL_H
#define TOR_CHANNEL_H

#include "or.h"
#include "circuitmux.h"

/* Channel handler function pointer typedefs */
typedef void (*channel_listener_fn_ptr)(channel_listener_t *, channel_t *);
typedef void (*channel_cell_handler_fn_ptr)(channel_t *, cell_t *);
typedef void (*channel_var_cell_handler_fn_ptr)(channel_t *, var_cell_t *);

struct cell_queue_entry_s;
TOR_SIMPLEQ_HEAD(chan_cell_queue, cell_queue_entry_s) incoming_queue;
typedef struct chan_cell_queue chan_cell_queue_t;

/**
 * Channel struct; see the channel_t typedef in or.h.  A channel is an
 * abstract interface for the OR-to-OR connection, similar to connection_or_t,
 * but without the strong coupling to the underlying TLS implementation.  They
 * are constructed by calling a protocol-specific function to open a channel
 * to a particular node, and once constructed support the abstract operations
 * defined below.
 */

struct channel_s {
  /** Magic number for type-checking cast macros */
  uint32_t magic;

  /** Current channel state */
  channel_state_t state;

  /** Globally unique ID number for a channel over the lifetime of a Tor
   * process.
   */
  uint64_t global_identifier;

  /** Should we expect to see this channel in the channel lists? */
  unsigned char registered:1;

  /** has this channel ever been open? */
  unsigned int has_been_open:1;

  /** Why did we close?
   */
  enum {
    CHANNEL_NOT_CLOSING = 0,
    CHANNEL_CLOSE_REQUESTED,
    CHANNEL_CLOSE_FROM_BELOW,
    CHANNEL_CLOSE_FOR_ERROR
  } reason_for_closing;

  /** Timestamps for both cell channels and listeners */
  time_t timestamp_created; /* Channel created */
  time_t timestamp_active; /* Any activity */

  /* Methods implemented by the lower layer */

  /** Free a channel */
  void (*free)(channel_t *);
  /** Close an open channel */
  void (*close)(channel_t *);
  /** Describe the transport subclass for this channel */
  const char * (*describe_transport)(channel_t *);
  /** Optional method to dump transport-specific statistics on the channel */
  void (*dumpstats)(channel_t *, int);

  /** Registered handlers for incoming cells */
  channel_cell_handler_fn_ptr cell_handler;
  channel_var_cell_handler_fn_ptr var_cell_handler;

  /* Methods implemented by the lower layer */

  /**
   * Ask the underlying transport what the remote endpoint address is, in
   * a tor_addr_t.  This is optional and subclasses may leave this NULL.
   * If they implement it, they should write the address out to the
   * provided tor_addr_t *, and return 1 if successful or 0 if no address
   * available.
   */
  int (*get_remote_addr)(channel_t *, tor_addr_t *);
  int (*get_transport_name)(channel_t *chan, char **transport_out);

#define GRD_FLAG_ORIGINAL 1
#define GRD_FLAG_ADDR_ONLY 2
  /**
   * Get a text description of the remote endpoint; canonicalized if the flag
   * GRD_FLAG_ORIGINAL is not set, or the one we originally connected
   * to/received from if it is.  If GRD_FLAG_ADDR_ONLY is set, we return only
   * the original address.
   */
  const char * (*get_remote_descr)(channel_t *, int);
  /** Check if the lower layer has queued writes */
  int (*has_queued_writes)(channel_t *);
  /**
   * If the second param is zero, ask the lower layer if this is
   * 'canonical', for a transport-specific definition of canonical; if
   * it is 1, ask if the answer to the preceding query is safe to rely
   * on.
   */
  int (*is_canonical)(channel_t *, int);
  /** Check if this channel matches a specified extend_info_t */
  int (*matches_extend_info)(channel_t *, extend_info_t *);
  /** Check if this channel matches a target address when extending */
  int (*matches_target)(channel_t *, const tor_addr_t *);
  /** Write a cell to an open channel */
  int (*write_cell)(channel_t *, cell_t *);
  /** Write a packed cell to an open channel */
  int (*write_packed_cell)(channel_t *, packed_cell_t *);
  /** Write a variable-length cell to an open channel */
  int (*write_var_cell)(channel_t *, var_cell_t *);

  /**
   * Hash of the public RSA key for the other side's identity key, or
   * zeroes if the other side hasn't shown us a valid identity key.
   */
  char identity_digest[DIGEST_LEN];
  /** Nickname of the OR on the other side, or NULL if none. */
  char *nickname;

  /**
   * Linked list of channels with the same identity digest, for the
   * digest->channel map
   */
  TOR_LIST_ENTRY(channel_s) next_with_same_id;

  /** List of incoming cells to handle */
  chan_cell_queue_t incoming_queue;

  /** List of queued outgoing cells */
  chan_cell_queue_t outgoing_queue;

  /** Circuit mux for circuits sending on this channel */
  circuitmux_t *cmux;

  /** Circuit ID generation stuff for use by circuitbuild.c */

  /**
   * When we send CREATE cells along this connection, which half of the
   * space should we use?
   */
  circ_id_type_bitfield_t circ_id_type:2;
  /** DOCDOC*/
  unsigned wide_circ_ids:1;

  /** For how many circuits are we n_chan?  What about p_chan? */
  unsigned int num_n_circuits, num_p_circuits;

  /**
   * True iff this channel shouldn't get any new circs attached to it,
   * because the connection is too old, or because there's a better one.
   * More generally, this flag is used to note an unhealthy connection;
   * for example, if a bad connection fails we shouldn't assume that the
   * router itself has a problem.
   */
  unsigned int is_bad_for_new_circs:1;

  /** True iff we have decided that the other end of this connection
   * is a client.  Channels with this flag set should never be used
   * to satisfy an EXTEND request.  */
  unsigned int is_client:1;

  /** Set if the channel was initiated remotely (came from a listener) */
  unsigned int is_incoming:1;

  /** Set by lower layer if this is local; i.e., everything it communicates
   * with for this channel returns true for is_local_addr().  This is used
   * to decide whether to declare reachability when we receive something on
   * this channel in circuitbuild.c
   */
  unsigned int is_local:1;

  /** Have we logged a warning about circID exhaustion on this channel?
   * If so, when? */
  ratelim_t last_warned_circ_ids_exhausted;

  /** Channel timestamps for cell channels */
  time_t timestamp_client; /* Client used this, according to relay.c */
  time_t timestamp_drained; /* Output queue empty */
  time_t timestamp_recv; /* Cell received from lower layer */
  time_t timestamp_xmit; /* Cell sent to lower layer */

  /** Timestamp for run_connection_housekeeping(). We update this once a
   * second when we run housekeeping and find a circuit on this channel, and
   * whenever we add a circuit to the channel. */
  time_t timestamp_last_had_circuits;

  /** Unique ID for measuring direct network status requests;vtunneled ones
   * come over a circuit_t, which has a dirreq_id field as well, but is a
   * distinct namespace. */
  uint64_t dirreq_id;

  /** Channel counters for cell channels */
  uint64_t n_cells_recved;
  uint64_t n_cells_xmitted;
};

struct channel_listener_s {
  /* Current channel listener state */
  channel_listener_state_t state;

  /* Globally unique ID number for a channel over the lifetime of a Tor
   * process.
   */
  uint64_t global_identifier;

  /** Should we expect to see this channel in the channel lists? */
  unsigned char registered:1;

  /** Why did we close?
   */
  enum {
    CHANNEL_LISTENER_NOT_CLOSING = 0,
    CHANNEL_LISTENER_CLOSE_REQUESTED,
    CHANNEL_LISTENER_CLOSE_FROM_BELOW,
    CHANNEL_LISTENER_CLOSE_FOR_ERROR
  } reason_for_closing;

  /** Timestamps for both cell channels and listeners */
  time_t timestamp_created; /* Channel created */
  time_t timestamp_active; /* Any activity */

  /* Methods implemented by the lower layer */

  /** Free a channel */
  void (*free)(channel_listener_t *);
  /** Close an open channel */
  void (*close)(channel_listener_t *);
  /** Describe the transport subclass for this channel */
  const char * (*describe_transport)(channel_listener_t *);
  /** Optional method to dump transport-specific statistics on the channel */
  void (*dumpstats)(channel_listener_t *, int);

  /** Registered listen handler to call on incoming connection */
  channel_listener_fn_ptr listener;

  /** List of pending incoming connections */
  smartlist_t *incoming_list;

  /** Timestamps for listeners */
  time_t timestamp_accepted;

  /** Counters for listeners */
  uint64_t n_accepted;
};

/* Channel state manipulations */

int channel_state_is_valid(channel_state_t state);
int channel_listener_state_is_valid(channel_listener_state_t state);

int channel_state_can_transition(channel_state_t from, channel_state_t to);
int channel_listener_state_can_transition(channel_listener_state_t from,
                                          channel_listener_state_t to);

const char * channel_state_to_string(channel_state_t state);
const char *
channel_listener_state_to_string(channel_listener_state_t state);

/* Abstract channel operations */

void channel_mark_for_close(channel_t *chan);
void channel_write_cell(channel_t *chan, cell_t *cell);
void channel_write_packed_cell(channel_t *chan, packed_cell_t *cell);
void channel_write_var_cell(channel_t *chan, var_cell_t *cell);

void channel_listener_mark_for_close(channel_listener_t *chan_l);

/* Channel callback registrations */

/* Listener callback */
channel_listener_fn_ptr
channel_listener_get_listener_fn(channel_listener_t *chan);

void channel_listener_set_listener_fn(channel_listener_t *chan,
                                      channel_listener_fn_ptr listener);

/* Incoming cell callbacks */
channel_cell_handler_fn_ptr channel_get_cell_handler(channel_t *chan);

channel_var_cell_handler_fn_ptr
channel_get_var_cell_handler(channel_t *chan);

void channel_set_cell_handlers(channel_t *chan,
                               channel_cell_handler_fn_ptr cell_handler,
                               channel_var_cell_handler_fn_ptr
                                 var_cell_handler);

/* Clean up closed channels and channel listeners periodically; these are
 * called from run_scheduled_events() in main.c.
 */
void channel_run_cleanup(void);
void channel_listener_run_cleanup(void);

/* Close all channels and deallocate everything */
void channel_free_all(void);

/* Dump some statistics in the log */
void channel_dumpstats(int severity);
void channel_listener_dumpstats(int severity);

/* Set the cmux policy on all active channels */
void channel_set_cmux_policy_everywhere(circuitmux_policy_t *pol);

#ifdef TOR_CHANNEL_INTERNAL_

/* Channel operations for subclasses and internal use only */

/* Initialize a newly allocated channel - do this first in subclass
 * constructors.
 */

void channel_init(channel_t *chan);
void channel_init_listener(channel_listener_t *chan);

/* Channel registration/unregistration */
void channel_register(channel_t *chan);
void channel_unregister(channel_t *chan);

/* Channel listener registration/unregistration */
void channel_listener_register(channel_listener_t *chan_l);
void channel_listener_unregister(channel_listener_t *chan_l);

/* Close from below */
void channel_close_from_lower_layer(channel_t *chan);
void channel_close_for_error(channel_t *chan);
void channel_closed(channel_t *chan);

void channel_listener_close_from_lower_layer(channel_listener_t *chan_l);
void channel_listener_close_for_error(channel_listener_t *chan_l);
void channel_listener_closed(channel_listener_t *chan_l);

/* Free a channel */
void channel_free(channel_t *chan);
void channel_listener_free(channel_listener_t *chan_l);

/* State/metadata setters */

void channel_change_state(channel_t *chan, channel_state_t to_state);
void channel_clear_identity_digest(channel_t *chan);
void channel_clear_remote_end(channel_t *chan);
void channel_mark_local(channel_t *chan);
void channel_mark_incoming(channel_t *chan);
void channel_mark_outgoing(channel_t *chan);
void channel_mark_remote(channel_t *chan);
void channel_set_identity_digest(channel_t *chan,
                                 const char *identity_digest);
void channel_set_remote_end(channel_t *chan,
                            const char *identity_digest,
                            const char *nickname);

void channel_listener_change_state(channel_listener_t *chan_l,
                                   channel_listener_state_t to_state);

/* Timestamp updates */
void channel_timestamp_created(channel_t *chan);
void channel_timestamp_active(channel_t *chan);
void channel_timestamp_drained(channel_t *chan);
void channel_timestamp_recv(channel_t *chan);
void channel_timestamp_xmit(channel_t *chan);

void channel_listener_timestamp_created(channel_listener_t *chan_l);
void channel_listener_timestamp_active(channel_listener_t *chan_l);
void channel_listener_timestamp_accepted(channel_listener_t *chan_l);

/* Incoming channel handling */
void channel_listener_process_incoming(channel_listener_t *listener);
void channel_listener_queue_incoming(channel_listener_t *listener,
                                     channel_t *incoming);

/* Incoming cell handling */
void channel_process_cells(channel_t *chan);
void channel_queue_cell(channel_t *chan, cell_t *cell);
void channel_queue_var_cell(channel_t *chan, var_cell_t *var_cell);

/* Outgoing cell handling */
void channel_flush_cells(channel_t *chan);

/* Request from lower layer for more cells if available */
ssize_t channel_flush_some_cells(channel_t *chan, ssize_t num_cells);

/* Query if data available on this channel */
int channel_more_to_flush(channel_t *chan);

/* Notify flushed outgoing for dirreq handling */
void channel_notify_flushed(channel_t *chan);

/* Handle stuff we need to do on open like notifying circuits */
void channel_do_open_actions(channel_t *chan);

#endif

/* Helper functions to perform operations on channels */

int channel_send_destroy(circid_t circ_id, channel_t *chan,
                         int reason);

/*
 * Outside abstract interfaces that should eventually get turned into
 * something transport/address format independent.
 */

channel_t * channel_connect(const tor_addr_t *addr, uint16_t port,
                            const char *id_digest);

channel_t * channel_get_for_extend(const char *digest,
                                   const tor_addr_t *target_addr,
                                   const char **msg_out,
                                   int *launch_out);

/* Ask which of two channels is better for circuit-extension purposes */
int channel_is_better(time_t now,
                      channel_t *a, channel_t *b,
                      int forgive_new_connections);

/** Channel lookups
 */

channel_t * channel_find_by_global_id(uint64_t global_identifier);
channel_t * channel_find_by_remote_digest(const char *identity_digest);

/** For things returned by channel_find_by_remote_digest(), walk the list.
 */
channel_t * channel_next_with_digest(channel_t *chan);

/*
 * Metadata queries/updates
 */

const char * channel_describe_transport(channel_t *chan);
void channel_dump_statistics(channel_t *chan, int severity);
void channel_dump_transport_statistics(channel_t *chan, int severity);
const char * channel_get_actual_remote_descr(channel_t *chan);
const char * channel_get_actual_remote_address(channel_t *chan);
int channel_get_addr_if_possible(channel_t *chan, tor_addr_t *addr_out);
const char * channel_get_canonical_remote_descr(channel_t *chan);
int channel_has_queued_writes(channel_t *chan);
int channel_is_bad_for_new_circs(channel_t *chan);
void channel_mark_bad_for_new_circs(channel_t *chan);
int channel_is_canonical(channel_t *chan);
int channel_is_canonical_is_reliable(channel_t *chan);
int channel_is_client(channel_t *chan);
int channel_is_local(channel_t *chan);
int channel_is_incoming(channel_t *chan);
int channel_is_outgoing(channel_t *chan);
void channel_mark_client(channel_t *chan);
int channel_matches_extend_info(channel_t *chan, extend_info_t *extend_info);
int channel_matches_target_addr_for_extend(channel_t *chan,
                                           const tor_addr_t *target);
unsigned int channel_num_circuits(channel_t *chan);
void channel_set_circid_type(channel_t *chan, crypto_pk_t *identity_rcvd,
                             int consider_identity);
void channel_timestamp_client(channel_t *chan);

const char * channel_listener_describe_transport(channel_listener_t *chan_l);
void channel_listener_dump_statistics(channel_listener_t *chan_l,
                                      int severity);
void channel_listener_dump_transport_statistics(channel_listener_t *chan_l,
                                                int severity);

/* Timestamp queries */
time_t channel_when_created(channel_t *chan);
time_t channel_when_last_active(channel_t *chan);
time_t channel_when_last_client(channel_t *chan);
time_t channel_when_last_drained(channel_t *chan);
time_t channel_when_last_recv(channel_t *chan);
time_t channel_when_last_xmit(channel_t *chan);

time_t channel_listener_when_created(channel_listener_t *chan_l);
time_t channel_listener_when_last_active(channel_listener_t *chan_l);
time_t channel_listener_when_last_accepted(channel_listener_t *chan_l);

/* Counter queries */
uint64_t channel_count_recved(channel_t *chan);
uint64_t channel_count_xmitted(channel_t *chan);

uint64_t channel_listener_count_accepted(channel_listener_t *chan_l);

int packed_cell_is_destroy(channel_t *chan,
                           const packed_cell_t *packed_cell,
                           circid_t *circid_out);

#endif

