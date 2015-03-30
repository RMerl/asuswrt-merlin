/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file command.c
 * \brief Functions for processing incoming cells.
 **/

/* In-points to command.c:
 *
 * - command_process_cell(), called from
 *   incoming cell handlers of channel_t instances;
 *   callbacks registered in command_setup_channel(),
 *   called when channels are created in circuitbuild.c
 */
#include "or.h"
#include "channel.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "command.h"
#include "connection.h"
#include "connection_or.h"
#include "config.h"
#include "control.h"
#include "cpuworker.h"
#include "hibernate.h"
#include "nodelist.h"
#include "onion.h"
#include "rephist.h"
#include "relay.h"
#include "router.h"
#include "routerlist.h"

/** How many CELL_CREATE cells have we received, ever? */
uint64_t stats_n_create_cells_processed = 0;
/** How many CELL_CREATED cells have we received, ever? */
uint64_t stats_n_created_cells_processed = 0;
/** How many CELL_RELAY cells have we received, ever? */
uint64_t stats_n_relay_cells_processed = 0;
/** How many CELL_DESTROY cells have we received, ever? */
uint64_t stats_n_destroy_cells_processed = 0;

/* Handle an incoming channel */
static void command_handle_incoming_channel(channel_listener_t *listener,
                                            channel_t *chan);

/* These are the main functions for processing cells */
static void command_process_create_cell(cell_t *cell, channel_t *chan);
static void command_process_created_cell(cell_t *cell, channel_t *chan);
static void command_process_relay_cell(cell_t *cell, channel_t *chan);
static void command_process_destroy_cell(cell_t *cell, channel_t *chan);

/** Convert the cell <b>command</b> into a lower-case, human-readable
 * string. */
const char *
cell_command_to_string(uint8_t command)
{
  switch (command) {
    case CELL_PADDING: return "padding";
    case CELL_CREATE: return "create";
    case CELL_CREATED: return "created";
    case CELL_RELAY: return "relay";
    case CELL_DESTROY: return "destroy";
    case CELL_CREATE_FAST: return "create_fast";
    case CELL_CREATED_FAST: return "created_fast";
    case CELL_VERSIONS: return "versions";
    case CELL_NETINFO: return "netinfo";
    case CELL_RELAY_EARLY: return "relay_early";
    case CELL_CREATE2: return "create2";
    case CELL_CREATED2: return "created2";
    case CELL_VPADDING: return "vpadding";
    case CELL_CERTS: return "certs";
    case CELL_AUTH_CHALLENGE: return "auth_challenge";
    case CELL_AUTHENTICATE: return "authenticate";
    case CELL_AUTHORIZE: return "authorize";
    default: return "unrecognized";
  }
}

#ifdef KEEP_TIMING_STATS
/** This is a wrapper function around the actual function that processes the
 * <b>cell</b> that just arrived on <b>conn</b>. Increment <b>*time</b>
 * by the number of microseconds used by the call to <b>*func(cell, conn)</b>.
 */
static void
command_time_process_cell(cell_t *cell, channel_t *chan, int *time,
                               void (*func)(cell_t *, channel_t *))
{
  struct timeval start, end;
  long time_passed;

  tor_gettimeofday(&start);

  (*func)(cell, chan);

  tor_gettimeofday(&end);
  time_passed = tv_udiff(&start, &end) ;

  if (time_passed > 10000) { /* more than 10ms */
    log_debug(LD_OR,"That call just took %ld ms.",time_passed/1000);
  }
  if (time_passed < 0) {
    log_info(LD_GENERAL,"That call took us back in time!");
    time_passed = 0;
  }
  *time += time_passed;
}
#endif

/** Process a <b>cell</b> that was just received on <b>chan</b>. Keep internal
 * statistics about how many of each cell we've processed so far
 * this second, and the total number of microseconds it took to
 * process each type of cell.
 */
void
command_process_cell(channel_t *chan, cell_t *cell)
{
#ifdef KEEP_TIMING_STATS
  /* how many of each cell have we seen so far this second? needs better
   * name. */
  static int num_create=0, num_created=0, num_relay=0, num_destroy=0;
  /* how long has it taken to process each type of cell? */
  static int create_time=0, created_time=0, relay_time=0, destroy_time=0;
  static time_t current_second = 0; /* from previous calls to time */

  time_t now = time(NULL);

  if (now > current_second) { /* the second has rolled over */
    /* print stats */
    log_info(LD_OR,
         "At end of second: %d creates (%d ms), %d createds (%d ms), "
         "%d relays (%d ms), %d destroys (%d ms)",
         num_create, create_time/1000,
         num_created, created_time/1000,
         num_relay, relay_time/1000,
         num_destroy, destroy_time/1000);

    /* zero out stats */
    num_create = num_created = num_relay = num_destroy = 0;
    create_time = created_time = relay_time = destroy_time = 0;

    /* remember which second it is, for next time */
    current_second = now;
  }
#endif

#ifdef KEEP_TIMING_STATS
#define PROCESS_CELL(tp, cl, cn) STMT_BEGIN {                   \
    ++num ## tp;                                                \
    command_time_process_cell(cl, cn, & tp ## time ,            \
                              command_process_ ## tp ## _cell);  \
  } STMT_END
#else
#define PROCESS_CELL(tp, cl, cn) command_process_ ## tp ## _cell(cl, cn)
#endif

  switch (cell->command) {
    case CELL_CREATE:
    case CELL_CREATE_FAST:
    case CELL_CREATE2:
      ++stats_n_create_cells_processed;
      PROCESS_CELL(create, cell, chan);
      break;
    case CELL_CREATED:
    case CELL_CREATED_FAST:
    case CELL_CREATED2:
      ++stats_n_created_cells_processed;
      PROCESS_CELL(created, cell, chan);
      break;
    case CELL_RELAY:
    case CELL_RELAY_EARLY:
      ++stats_n_relay_cells_processed;
      PROCESS_CELL(relay, cell, chan);
      break;
    case CELL_DESTROY:
      ++stats_n_destroy_cells_processed;
      PROCESS_CELL(destroy, cell, chan);
      break;
    default:
      log_fn(LOG_INFO, LD_PROTOCOL,
             "Cell of unknown or unexpected type (%d) received.  "
             "Dropping.",
             cell->command);
      break;
  }
}

/** Process an incoming var_cell from a channel; in the current protocol all
 * the var_cells are handshake-related and handled below the channel layer,
 * so this just logs a warning and drops the cell.
 */

void
command_process_var_cell(channel_t *chan, var_cell_t *var_cell)
{
  tor_assert(chan);
  tor_assert(var_cell);

  log_info(LD_PROTOCOL,
           "Received unexpected var_cell above the channel layer of type %d"
           "; dropping it.",
           var_cell->command);
}

/** Process a 'create' <b>cell</b> that just arrived from <b>chan</b>. Make a
 * new circuit with the p_circ_id specified in cell. Put the circuit in state
 * onionskin_pending, and pass the onionskin to the cpuworker. Circ will get
 * picked up again when the cpuworker finishes decrypting it.
 */
static void
command_process_create_cell(cell_t *cell, channel_t *chan)
{
  or_circuit_t *circ;
  const or_options_t *options = get_options();
  int id_is_high;
  create_cell_t *create_cell;

  tor_assert(cell);
  tor_assert(chan);

  log_debug(LD_OR,
            "Got a CREATE cell for circ_id %u on channel " U64_FORMAT
            " (%p)",
            (unsigned)cell->circ_id,
            U64_PRINTF_ARG(chan->global_identifier), chan);

  /* We check for the conditions that would make us drop the cell before
   * we check for the conditions that would make us send a DESTROY back,
   * since those conditions would make a DESTROY nonsensical. */
  if (cell->circ_id == 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Received a create cell (type %d) from %s with zero circID; "
           " ignoring.", (int)cell->command,
           channel_get_actual_remote_descr(chan));
    return;
  }

  if (circuit_id_in_use_on_channel(cell->circ_id, chan)) {
    const node_t *node = node_get_by_id(chan->identity_digest);
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Received CREATE cell (circID %u) for known circ. "
           "Dropping (age %d).",
           (unsigned)cell->circ_id,
           (int)(time(NULL) - channel_when_created(chan)));
    if (node) {
      char *p = esc_for_log(node_get_platform(node));
      log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
             "Details: router %s, platform %s.",
             node_describe(node), p);
      tor_free(p);
    }
    return;
  }

  if (we_are_hibernating()) {
    log_info(LD_OR,
             "Received create cell but we're shutting down. Sending back "
             "destroy.");
    channel_send_destroy(cell->circ_id, chan,
                               END_CIRC_REASON_HIBERNATING);
    return;
  }

  if (!server_mode(options) ||
      (!public_server_mode(options) && channel_is_outgoing(chan))) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Received create cell (type %d) from %s, but we're connected "
           "to it as a client. "
           "Sending back a destroy.",
           (int)cell->command, channel_get_canonical_remote_descr(chan));
    channel_send_destroy(cell->circ_id, chan,
                         END_CIRC_REASON_TORPROTOCOL);
    return;
  }

  /* If the high bit of the circuit ID is not as expected, close the
   * circ. */
  if (chan->wide_circ_ids)
    id_is_high = cell->circ_id & (1u<<31);
  else
    id_is_high = cell->circ_id & (1u<<15);
  if ((id_is_high &&
       chan->circ_id_type == CIRC_ID_TYPE_HIGHER) ||
      (!id_is_high &&
       chan->circ_id_type == CIRC_ID_TYPE_LOWER)) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
           "Received create cell with unexpected circ_id %u. Closing.",
           (unsigned)cell->circ_id);
    channel_send_destroy(cell->circ_id, chan,
                         END_CIRC_REASON_TORPROTOCOL);
    return;
  }

  circ = or_circuit_new(cell->circ_id, chan);
  circ->base_.purpose = CIRCUIT_PURPOSE_OR;
  circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_ONIONSKIN_PENDING);
  create_cell = tor_malloc_zero(sizeof(create_cell_t));
  if (create_cell_parse(create_cell, cell) < 0) {
    tor_free(create_cell);
    log_fn(LOG_PROTOCOL_WARN, LD_OR,
           "Bogus/unrecognized create cell; closing.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    return;
  }

  if (create_cell->handshake_type != ONION_HANDSHAKE_TYPE_FAST) {
    /* hand it off to the cpuworkers, and then return. */
    if (connection_or_digest_is_known_relay(chan->identity_digest))
      rep_hist_note_circuit_handshake_requested(create_cell->handshake_type);
    if (assign_onionskin_to_cpuworker(NULL, circ, create_cell) < 0) {
      log_debug(LD_GENERAL,"Failed to hand off onionskin. Closing.");
      circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_RESOURCELIMIT);
      return;
    }
    log_debug(LD_OR,"success: handed off onionskin.");
  } else {
    /* This is a CREATE_FAST cell; we can handle it immediately without using
     * a CPU worker. */
    uint8_t keys[CPATH_KEY_MATERIAL_LEN];
    uint8_t rend_circ_nonce[DIGEST_LEN];
    int len;
    created_cell_t created_cell;

    /* Make sure we never try to use the OR connection on which we
     * received this cell to satisfy an EXTEND request,  */
    channel_mark_client(chan);

    memset(&created_cell, 0, sizeof(created_cell));
    len = onion_skin_server_handshake(ONION_HANDSHAKE_TYPE_FAST,
                                       create_cell->onionskin,
                                       create_cell->handshake_len,
                                       NULL,
                                       created_cell.reply,
                                       keys, CPATH_KEY_MATERIAL_LEN,
                                       rend_circ_nonce);
    tor_free(create_cell);
    if (len < 0) {
      log_warn(LD_OR,"Failed to generate key material. Closing.");
      circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
      tor_free(create_cell);
      return;
    }
    created_cell.cell_type = CELL_CREATED_FAST;
    created_cell.handshake_len = len;

    if (onionskin_answer(circ, &created_cell,
                         (const char *)keys, rend_circ_nonce)<0) {
      log_warn(LD_OR,"Failed to reply to CREATE_FAST cell. Closing.");
      circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
      return;
    }
    memwipe(keys, 0, sizeof(keys));
  }
}

/** Process a 'created' <b>cell</b> that just arrived from <b>chan</b>.
 * Find the circuit
 * that it's intended for. If we're not the origin of the circuit, package
 * the 'created' cell in an 'extended' relay cell and pass it back. If we
 * are the origin of the circuit, send it to circuit_finish_handshake() to
 * finish processing keys, and then call circuit_send_next_onion_skin() to
 * extend to the next hop in the circuit if necessary.
 */
static void
command_process_created_cell(cell_t *cell, channel_t *chan)
{
  circuit_t *circ;
  extended_cell_t extended_cell;

  circ = circuit_get_by_circid_channel(cell->circ_id, chan);

  if (!circ) {
    log_info(LD_OR,
             "(circID %u) unknown circ (probably got a destroy earlier). "
             "Dropping.", (unsigned)cell->circ_id);
    return;
  }

  if (circ->n_circ_id != cell->circ_id || circ->n_chan != chan) {
    log_fn(LOG_PROTOCOL_WARN,LD_PROTOCOL,
           "got created cell from Tor client? Closing.");
    circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
    return;
  }

  if (created_cell_parse(&extended_cell.created_cell, cell) < 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_OR, "Unparseable created cell.");
    circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
    return;
  }

  if (CIRCUIT_IS_ORIGIN(circ)) { /* we're the OP. Handshake this. */
    origin_circuit_t *origin_circ = TO_ORIGIN_CIRCUIT(circ);
    int err_reason = 0;
    log_debug(LD_OR,"at OP. Finishing handshake.");
    if ((err_reason = circuit_finish_handshake(origin_circ,
                                        &extended_cell.created_cell)) < 0) {
      log_warn(LD_OR,"circuit_finish_handshake failed.");
      circuit_mark_for_close(circ, -err_reason);
      return;
    }
    log_debug(LD_OR,"Moving to next skin.");
    if ((err_reason = circuit_send_next_onion_skin(origin_circ)) < 0) {
      log_info(LD_OR,"circuit_send_next_onion_skin failed.");
      /* XXX push this circuit_close lower */
      circuit_mark_for_close(circ, -err_reason);
      return;
    }
  } else { /* pack it into an extended relay cell, and send it. */
    uint8_t command=0;
    uint16_t len=0;
    uint8_t payload[RELAY_PAYLOAD_SIZE];
    log_debug(LD_OR,
              "Converting created cell to extended relay cell, sending.");
    memset(payload, 0, sizeof(payload));
    if (extended_cell.created_cell.cell_type == CELL_CREATED2)
      extended_cell.cell_type = RELAY_COMMAND_EXTENDED2;
    else
      extended_cell.cell_type = RELAY_COMMAND_EXTENDED;
    if (extended_cell_format(&command, &len, payload, &extended_cell) < 0) {
      log_fn(LOG_PROTOCOL_WARN, LD_OR, "Can't format extended cell.");
      circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
      return;
    }

    relay_send_command_from_edge(0, circ, command,
                                 (const char*)payload, len, NULL);
  }
}

/** Process a 'relay' or 'relay_early' <b>cell</b> that just arrived from
 * <b>conn</b>. Make sure it came in with a recognized circ_id. Pass it on to
 * circuit_receive_relay_cell() for actual processing.
 */
static void
command_process_relay_cell(cell_t *cell, channel_t *chan)
{
  circuit_t *circ;
  int reason, direction;

  circ = circuit_get_by_circid_channel(cell->circ_id, chan);

  if (!circ) {
    log_debug(LD_OR,
              "unknown circuit %u on connection from %s. Dropping.",
              (unsigned)cell->circ_id,
              channel_get_canonical_remote_descr(chan));
    return;
  }

  if (circ->state == CIRCUIT_STATE_ONIONSKIN_PENDING) {
    log_fn(LOG_PROTOCOL_WARN,LD_PROTOCOL,"circuit in create_wait. Closing.");
    circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
    return;
  }

  if (CIRCUIT_IS_ORIGIN(circ)) {
    /* if we're a relay and treating connections with recent local
     * traffic better, then this is one of them. */
    channel_timestamp_client(chan);
  }

  if (!CIRCUIT_IS_ORIGIN(circ) &&
      chan == TO_OR_CIRCUIT(circ)->p_chan &&
      cell->circ_id == TO_OR_CIRCUIT(circ)->p_circ_id)
    direction = CELL_DIRECTION_OUT;
  else
    direction = CELL_DIRECTION_IN;

  /* If we have a relay_early cell, make sure that it's outbound, and we've
   * gotten no more than MAX_RELAY_EARLY_CELLS_PER_CIRCUIT of them. */
  if (cell->command == CELL_RELAY_EARLY) {
    if (direction == CELL_DIRECTION_IN) {
      /* Inbound early cells could once be encountered as a result of
       * bug 1038; but relays running versions before 0.2.1.19 are long
       * gone from the network, so any such cells now are surprising. */
      log_warn(LD_OR,
               "Received an inbound RELAY_EARLY cell on circuit %u."
               " Closing circuit. Please report this event,"
               " along with the following message.",
               (unsigned)cell->circ_id);
      if (CIRCUIT_IS_ORIGIN(circ)) {
        circuit_log_path(LOG_WARN, LD_OR, TO_ORIGIN_CIRCUIT(circ));
      } else if (circ->n_chan) {
        log_warn(LD_OR, " upstream=%s",
                 channel_get_actual_remote_descr(circ->n_chan));
      }
      circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
      return;
    } else {
      or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
      if (or_circ->remaining_relay_early_cells == 0) {
        log_fn(LOG_PROTOCOL_WARN, LD_OR,
               "Received too many RELAY_EARLY cells on circ %u from %s."
               "  Closing circuit.",
               (unsigned)cell->circ_id,
               safe_str(channel_get_canonical_remote_descr(chan)));
        circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
        return;
      }
      --or_circ->remaining_relay_early_cells;
    }
  }

  if ((reason = circuit_receive_relay_cell(cell, circ, direction)) < 0) {
    log_fn(LOG_PROTOCOL_WARN,LD_PROTOCOL,"circuit_receive_relay_cell "
           "(%s) failed. Closing.",
           direction==CELL_DIRECTION_OUT?"forward":"backward");
    circuit_mark_for_close(circ, -reason);
  }
}

/** Process a 'destroy' <b>cell</b> that just arrived from
 * <b>chan</b>. Find the circ that it refers to (if any).
 *
 * If the circ is in state
 * onionskin_pending, then call onion_pending_remove() to remove it
 * from the pending onion list (note that if it's already being
 * processed by the cpuworker, it won't be in the list anymore; but
 * when the cpuworker returns it, the circuit will be gone, and the
 * cpuworker response will be dropped).
 *
 * Then mark the circuit for close (which marks all edges for close,
 * and passes the destroy cell onward if necessary).
 */
static void
command_process_destroy_cell(cell_t *cell, channel_t *chan)
{
  circuit_t *circ;
  int reason;

  circ = circuit_get_by_circid_channel(cell->circ_id, chan);
  if (!circ) {
    log_info(LD_OR,"unknown circuit %u on connection from %s. Dropping.",
             (unsigned)cell->circ_id,
             channel_get_canonical_remote_descr(chan));
    return;
  }
  log_debug(LD_OR,"Received for circID %u.",(unsigned)cell->circ_id);

  reason = (uint8_t)cell->payload[0];
  circ->received_destroy = 1;

  if (!CIRCUIT_IS_ORIGIN(circ) &&
      chan == TO_OR_CIRCUIT(circ)->p_chan &&
      cell->circ_id == TO_OR_CIRCUIT(circ)->p_circ_id) {
    /* the destroy came from behind */
    circuit_set_p_circid_chan(TO_OR_CIRCUIT(circ), 0, NULL);
    circuit_mark_for_close(circ, reason|END_CIRC_REASON_FLAG_REMOTE);
  } else { /* the destroy came from ahead */
    circuit_set_n_circid_chan(circ, 0, NULL);
    if (CIRCUIT_IS_ORIGIN(circ)) {
      circuit_mark_for_close(circ, reason|END_CIRC_REASON_FLAG_REMOTE);
    } else {
      char payload[1];
      log_debug(LD_OR, "Delivering 'truncated' back.");
      payload[0] = (char)reason;
      relay_send_command_from_edge(0, circ, RELAY_COMMAND_TRUNCATED,
                                   payload, sizeof(payload), NULL);
    }
  }
}

/** Callback to handle a new channel; call command_setup_channel() to give
 * it the right cell handlers.
 */

static void
command_handle_incoming_channel(channel_listener_t *listener, channel_t *chan)
{
  tor_assert(listener);
  tor_assert(chan);

  command_setup_channel(chan);
}

/** Given a channel, install the right handlers to process incoming
 * cells on it.
 */

void
command_setup_channel(channel_t *chan)
{
  tor_assert(chan);

  channel_set_cell_handlers(chan,
                            command_process_cell,
                            command_process_var_cell);
}

/** Given a listener, install the right handler to process incoming
 * channels on it.
 */

void
command_setup_listener(channel_listener_t *listener)
{
  tor_assert(listener);
  tor_assert(listener->state == CHANNEL_LISTENER_STATE_LISTENING);

  channel_listener_set_listener_fn(listener, command_handle_incoming_channel);
}

