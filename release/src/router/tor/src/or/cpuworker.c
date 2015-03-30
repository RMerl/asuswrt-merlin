/* Copyright (c) 2003-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file cpuworker.c
 * \brief Implements a farm of 'CPU worker' processes to perform
 * CPU-intensive tasks in another thread or process, to not
 * interrupt the main thread.
 *
 * Right now, we only use this for processing onionskins.
 **/
#include "or.h"
#include "buffers.h"
#include "channel.h"
#include "channeltls.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "config.h"
#include "connection.h"
#include "connection_or.h"
#include "cpuworker.h"
#include "main.h"
#include "onion.h"
#include "rephist.h"
#include "router.h"

/** The maximum number of cpuworker processes we will keep around. */
#define MAX_CPUWORKERS 16
/** The minimum number of cpuworker processes we will keep around. */
#define MIN_CPUWORKERS 1

/** The tag specifies which circuit this onionskin was from. */
#define TAG_LEN 12

/** How many cpuworkers we have running right now. */
static int num_cpuworkers=0;
/** How many of the running cpuworkers have an assigned task right now. */
static int num_cpuworkers_busy=0;
/** We need to spawn new cpuworkers whenever we rotate the onion keys
 * on platforms where execution contexts==processes.  This variable stores
 * the last time we got a key rotation event. */
static time_t last_rotation_time=0;

static void cpuworker_main(void *data) ATTR_NORETURN;
static int spawn_cpuworker(void);
static void spawn_enough_cpuworkers(void);
static void process_pending_task(connection_t *cpuworker);

/** Initialize the cpuworker subsystem.
 */
void
cpu_init(void)
{
  cpuworkers_rotate();
}

/** Called when we're done sending a request to a cpuworker. */
int
connection_cpu_finished_flushing(connection_t *conn)
{
  tor_assert(conn);
  tor_assert(conn->type == CONN_TYPE_CPUWORKER);
  return 0;
}

/** Pack global_id and circ_id; set *tag to the result. (See note on
 * cpuworker_main for wire format.) */
static void
tag_pack(uint8_t *tag, uint64_t chan_id, circid_t circ_id)
{
  /*XXXX RETHINK THIS WHOLE MESS !!!! !NM NM NM NM*/
  /*XXXX DOUBLEPLUSTHIS!!!! AS AS AS AS*/
  set_uint64(tag, chan_id);
  set_uint32(tag+8, circ_id);
}

/** Unpack <b>tag</b> into addr, port, and circ_id.
 */
static void
tag_unpack(const uint8_t *tag, uint64_t *chan_id, circid_t *circ_id)
{
  *chan_id = get_uint64(tag);
  *circ_id = get_uint32(tag+8);
}

/** Magic numbers to make sure our cpuworker_requests don't grow any
 * mis-framing bugs. */
#define CPUWORKER_REQUEST_MAGIC 0xda4afeed
#define CPUWORKER_REPLY_MAGIC 0x5eedf00d

/** A request sent to a cpuworker. */
typedef struct cpuworker_request_t {
  /** Magic number; must be CPUWORKER_REQUEST_MAGIC. */
  uint32_t magic;
  /** Opaque tag to identify the job */
  uint8_t tag[TAG_LEN];
  /** Task code. Must be one of CPUWORKER_TASK_* */
  uint8_t task;

  /** Flag: Are we timing this request? */
  unsigned timed : 1;
  /** If we're timing this request, when was it sent to the cpuworker? */
  struct timeval started_at;

  /** A create cell for the cpuworker to process. */
  create_cell_t create_cell;

  /* Turn the above into a tagged union if needed. */
} cpuworker_request_t;

/** A reply sent by a cpuworker. */
typedef struct cpuworker_reply_t {
  /** Magic number; must be CPUWORKER_REPLY_MAGIC. */
  uint32_t magic;
  /** Opaque tag to identify the job; matches the request's tag.*/
  uint8_t tag[TAG_LEN];
  /** True iff we got a successful request. */
  uint8_t success;

  /** Are we timing this request? */
  unsigned int timed : 1;
  /** What handshake type was the request? (Used for timing) */
  uint16_t handshake_type;
  /** When did we send the request to the cpuworker? */
  struct timeval started_at;
  /** Once the cpuworker received the request, how many microseconds did it
   * take? (This shouldn't overflow; 4 billion micoseconds is over an hour,
   * and we'll never have an onion handshake that takes so long.) */
  uint32_t n_usec;

  /** Output of processing a create cell
   *
   * @{
   */
  /** The created cell to send back. */
  created_cell_t created_cell;
  /** The keys to use on this circuit. */
  uint8_t keys[CPATH_KEY_MATERIAL_LEN];
  /** Input to use for authenticating introduce1 cells. */
  uint8_t rend_auth_material[DIGEST_LEN];
} cpuworker_reply_t;

/** Called when the onion key has changed and we need to spawn new
 * cpuworkers.  Close all currently idle cpuworkers, and mark the last
 * rotation time as now.
 */
void
cpuworkers_rotate(void)
{
  connection_t *cpuworker;
  while ((cpuworker = connection_get_by_type_state(CONN_TYPE_CPUWORKER,
                                                   CPUWORKER_STATE_IDLE))) {
    connection_mark_for_close(cpuworker);
    --num_cpuworkers;
  }
  last_rotation_time = time(NULL);
  if (server_mode(get_options()))
    spawn_enough_cpuworkers();
}

/** If the cpuworker closes the connection,
 * mark it as closed and spawn a new one as needed. */
int
connection_cpu_reached_eof(connection_t *conn)
{
  log_warn(LD_GENERAL,"Read eof. CPU worker died unexpectedly.");
  if (conn->state != CPUWORKER_STATE_IDLE) {
    /* the circ associated with this cpuworker will have to wait until
     * it gets culled in run_connection_housekeeping(), since we have
     * no way to find out which circ it was. */
    log_warn(LD_GENERAL,"...and it left a circuit queued; abandoning circ.");
    num_cpuworkers_busy--;
  }
  num_cpuworkers--;
  spawn_enough_cpuworkers(); /* try to regrow. hope we don't end up
                                spinning. */
  connection_mark_for_close(conn);
  return 0;
}

/** Indexed by handshake type: how many onionskins have we processed and
 * counted of that type? */
static uint64_t onionskins_n_processed[MAX_ONION_HANDSHAKE_TYPE+1];
/** Indexed by handshake type, corresponding to the onionskins counted in
 * onionskins_n_processed: how many microseconds have we spent in cpuworkers
 * processing that kind of onionskin? */
static uint64_t onionskins_usec_internal[MAX_ONION_HANDSHAKE_TYPE+1];
/** Indexed by handshake type, corresponding to onionskins counted in
 * onionskins_n_processed: how many microseconds have we spent waiting for
 * cpuworkers to give us answers for that kind of onionskin?
 */
static uint64_t onionskins_usec_roundtrip[MAX_ONION_HANDSHAKE_TYPE+1];

/** If any onionskin takes longer than this, we clip them to this
 * time. (microseconds) */
#define MAX_BELIEVABLE_ONIONSKIN_DELAY (2*1000*1000)

static tor_weak_rng_t request_sample_rng = TOR_WEAK_RNG_INIT;

/** Return true iff we'd like to measure a handshake of type
 * <b>onionskin_type</b>. Call only from the main thread. */
static int
should_time_request(uint16_t onionskin_type)
{
  /* If we've never heard of this type, we shouldn't even be here. */
  if (onionskin_type > MAX_ONION_HANDSHAKE_TYPE)
    return 0;
  /* Measure the first N handshakes of each type, to ensure we have a
   * sample */
  if (onionskins_n_processed[onionskin_type] < 4096)
    return 1;
  /** Otherwise, measure with P=1/128.  We avoid doing this for every
   * handshake, since the measurement itself can take a little time. */
  return tor_weak_random_one_in_n(&request_sample_rng, 128);
}

/** Return an estimate of how many microseconds we will need for a single
 * cpuworker to to process <b>n_requests</b> onionskins of type
 * <b>onionskin_type</b>. */
uint64_t
estimated_usec_for_onionskins(uint32_t n_requests, uint16_t onionskin_type)
{
  if (onionskin_type > MAX_ONION_HANDSHAKE_TYPE) /* should be impossible */
    return 1000 * (uint64_t)n_requests;
  if (PREDICT_UNLIKELY(onionskins_n_processed[onionskin_type] < 100)) {
    /* Until we have 100 data points, just asssume everything takes 1 msec. */
    return 1000 * (uint64_t)n_requests;
  } else {
    /* This can't overflow: we'll never have more than 500000 onionskins
     * measured in onionskin_usec_internal, and they won't take anything near
     * 1 sec each, and we won't have anything like 1 million queued
     * onionskins.  But that's 5e5 * 1e6 * 1e6, which is still less than
     * UINT64_MAX. */
    return (onionskins_usec_internal[onionskin_type] * n_requests) /
      onionskins_n_processed[onionskin_type];
  }
}

/** Compute the absolute and relative overhead of using the cpuworker
 * framework for onionskins of type <b>onionskin_type</b>.*/
static int
get_overhead_for_onionskins(uint32_t *usec_out, double *frac_out,
                            uint16_t onionskin_type)
{
  uint64_t overhead;

  *usec_out = 0;
  *frac_out = 0.0;

  if (onionskin_type > MAX_ONION_HANDSHAKE_TYPE) /* should be impossible */
    return -1;
  if (onionskins_n_processed[onionskin_type] == 0 ||
      onionskins_usec_internal[onionskin_type] == 0 ||
      onionskins_usec_roundtrip[onionskin_type] == 0)
    return -1;

  overhead = onionskins_usec_roundtrip[onionskin_type] -
    onionskins_usec_internal[onionskin_type];

  *usec_out = (uint32_t)(overhead / onionskins_n_processed[onionskin_type]);
  *frac_out = U64_TO_DBL(overhead) / onionskins_usec_internal[onionskin_type];

  return 0;
}

/** If we've measured overhead for onionskins of type <b>onionskin_type</b>,
 * log it. */
void
cpuworker_log_onionskin_overhead(int severity, int onionskin_type,
                                 const char *onionskin_type_name)
{
  uint32_t overhead;
  double relative_overhead;
  int r;

  r = get_overhead_for_onionskins(&overhead,  &relative_overhead,
                                  onionskin_type);
  if (!overhead || r<0)
    return;

  log_fn(severity, LD_OR,
         "%s onionskins have averaged %u usec overhead (%.2f%%) in "
         "cpuworker code ",
         onionskin_type_name, (unsigned)overhead, relative_overhead*100);
}

/** Called when we get data from a cpuworker.  If the answer is not complete,
 * wait for a complete answer. If the answer is complete,
 * process it as appropriate.
 */
int
connection_cpu_process_inbuf(connection_t *conn)
{
  uint64_t chan_id;
  circid_t circ_id;
  channel_t *p_chan = NULL;
  circuit_t *circ;

  tor_assert(conn);
  tor_assert(conn->type == CONN_TYPE_CPUWORKER);

  if (!connection_get_inbuf_len(conn))
    return 0;

  if (conn->state == CPUWORKER_STATE_BUSY_ONION) {
    cpuworker_reply_t rpl;
    if (connection_get_inbuf_len(conn) < sizeof(cpuworker_reply_t))
      return 0; /* not yet */
    tor_assert(connection_get_inbuf_len(conn) == sizeof(cpuworker_reply_t));

    connection_fetch_from_buf((void*)&rpl,sizeof(cpuworker_reply_t),conn);

    tor_assert(rpl.magic == CPUWORKER_REPLY_MAGIC);

    if (rpl.timed && rpl.success &&
        rpl.handshake_type <= MAX_ONION_HANDSHAKE_TYPE) {
      /* Time how long this request took. The handshake_type check should be
         needless, but let's leave it in to be safe. */
      struct timeval tv_end, tv_diff;
      int64_t usec_roundtrip;
      tor_gettimeofday(&tv_end);
      timersub(&tv_end, &rpl.started_at, &tv_diff);
      usec_roundtrip = ((int64_t)tv_diff.tv_sec)*1000000 + tv_diff.tv_usec;
      if (usec_roundtrip >= 0 &&
          usec_roundtrip < MAX_BELIEVABLE_ONIONSKIN_DELAY) {
        ++onionskins_n_processed[rpl.handshake_type];
        onionskins_usec_internal[rpl.handshake_type] += rpl.n_usec;
        onionskins_usec_roundtrip[rpl.handshake_type] += usec_roundtrip;
        if (onionskins_n_processed[rpl.handshake_type] >= 500000) {
          /* Scale down every 500000 handshakes.  On a busy server, that's
           * less impressive than it sounds. */
          onionskins_n_processed[rpl.handshake_type] /= 2;
          onionskins_usec_internal[rpl.handshake_type] /= 2;
          onionskins_usec_roundtrip[rpl.handshake_type] /= 2;
        }
      }
    }
    /* parse out the circ it was talking about */
    tag_unpack(rpl.tag, &chan_id, &circ_id);
    circ = NULL;
    log_debug(LD_OR,
              "Unpacking cpuworker reply, chan_id is " U64_FORMAT
              ", circ_id is %u",
              U64_PRINTF_ARG(chan_id), (unsigned)circ_id);
    p_chan = channel_find_by_global_id(chan_id);

    if (p_chan)
      circ = circuit_get_by_circid_channel(circ_id, p_chan);

    if (rpl.success == 0) {
      log_debug(LD_OR,
                "decoding onionskin failed. "
                "(Old key or bad software.) Closing.");
      if (circ)
        circuit_mark_for_close(circ, END_CIRC_REASON_TORPROTOCOL);
      goto done_processing;
    }
    if (!circ) {
      /* This happens because somebody sends us a destroy cell and the
       * circuit goes away, while the cpuworker is working. This is also
       * why our tag doesn't include a pointer to the circ, because we'd
       * never know if it's still valid.
       */
      log_debug(LD_OR,"processed onion for a circ that's gone. Dropping.");
      goto done_processing;
    }
    tor_assert(! CIRCUIT_IS_ORIGIN(circ));
    if (onionskin_answer(TO_OR_CIRCUIT(circ),
                         &rpl.created_cell,
                         (const char*)rpl.keys,
                         rpl.rend_auth_material) < 0) {
      log_warn(LD_OR,"onionskin_answer failed. Closing.");
      circuit_mark_for_close(circ, END_CIRC_REASON_INTERNAL);
      goto done_processing;
    }
    log_debug(LD_OR,"onionskin_answer succeeded. Yay.");
  } else {
    tor_assert(0); /* don't ask me to do handshakes yet */
  }

 done_processing:
  conn->state = CPUWORKER_STATE_IDLE;
  num_cpuworkers_busy--;
  if (conn->timestamp_created < last_rotation_time) {
    connection_mark_for_close(conn);
    num_cpuworkers--;
    spawn_enough_cpuworkers();
  } else {
    process_pending_task(conn);
  }
  return 0;
}

/** Implement a cpuworker.  'data' is an fdarray as returned by socketpair.
 * Read and writes from fdarray[1].  Reads requests, writes answers.
 *
 *   Request format:
 *          cpuworker_request_t.
 *   Response format:
 *          cpuworker_reply_t
 */
static void
cpuworker_main(void *data)
{
  /* For talking to the parent thread/process */
  tor_socket_t *fdarray = data;
  tor_socket_t fd;

  /* variables for onion processing */
  server_onion_keys_t onion_keys;
  cpuworker_request_t req;
  cpuworker_reply_t rpl;

  fd = fdarray[1]; /* this side is ours */
#ifndef TOR_IS_MULTITHREADED
  tor_close_socket(fdarray[0]); /* this is the side of the socketpair the
                                 * parent uses */
  tor_free_all(1); /* so the child doesn't hold the parent's fd's open */
  handle_signals(0); /* ignore interrupts from the keyboard, etc */
#endif
  tor_free(data);

  setup_server_onion_keys(&onion_keys);

  for (;;) {
    if (read_all(fd, (void *)&req, sizeof(req), 1) != sizeof(req)) {
      log_info(LD_OR, "read request failed. Exiting.");
      goto end;
    }
    tor_assert(req.magic == CPUWORKER_REQUEST_MAGIC);

    memset(&rpl, 0, sizeof(rpl));

    if (req.task == CPUWORKER_TASK_ONION) {
      const create_cell_t *cc = &req.create_cell;
      created_cell_t *cell_out = &rpl.created_cell;
      struct timeval tv_start = {0,0}, tv_end;
      int n;
      rpl.timed = req.timed;
      rpl.started_at = req.started_at;
      rpl.handshake_type = cc->handshake_type;
      if (req.timed)
        tor_gettimeofday(&tv_start);
      n = onion_skin_server_handshake(cc->handshake_type,
                                      cc->onionskin, cc->handshake_len,
                                      &onion_keys,
                                      cell_out->reply,
                                      rpl.keys, CPATH_KEY_MATERIAL_LEN,
                                      rpl.rend_auth_material);
      if (n < 0) {
        /* failure */
        log_debug(LD_OR,"onion_skin_server_handshake failed.");
        memset(&rpl, 0, sizeof(rpl));
        memcpy(rpl.tag, req.tag, TAG_LEN);
        rpl.success = 0;
      } else {
        /* success */
        log_debug(LD_OR,"onion_skin_server_handshake succeeded.");
        memcpy(rpl.tag, req.tag, TAG_LEN);
        cell_out->handshake_len = n;
        switch (cc->cell_type) {
        case CELL_CREATE:
          cell_out->cell_type = CELL_CREATED; break;
        case CELL_CREATE2:
          cell_out->cell_type = CELL_CREATED2; break;
        case CELL_CREATE_FAST:
          cell_out->cell_type = CELL_CREATED_FAST; break;
        default:
          tor_assert(0);
          goto end;
        }
        rpl.success = 1;
      }
      rpl.magic = CPUWORKER_REPLY_MAGIC;
      if (req.timed) {
        struct timeval tv_diff;
        int64_t usec;
        tor_gettimeofday(&tv_end);
        timersub(&tv_end, &tv_start, &tv_diff);
        usec = ((int64_t)tv_diff.tv_sec)*1000000 + tv_diff.tv_usec;
        if (usec < 0 || usec > MAX_BELIEVABLE_ONIONSKIN_DELAY)
          rpl.n_usec = MAX_BELIEVABLE_ONIONSKIN_DELAY;
        else
          rpl.n_usec = (uint32_t) usec;
      }
      if (write_all(fd, (void*)&rpl, sizeof(rpl), 1) != sizeof(rpl)) {
        log_err(LD_BUG,"writing response buf failed. Exiting.");
        goto end;
      }
      log_debug(LD_OR,"finished writing response.");
    } else if (req.task == CPUWORKER_TASK_SHUTDOWN) {
      log_info(LD_OR,"Clean shutdown: exiting");
      goto end;
    }
    memwipe(&req, 0, sizeof(req));
    memwipe(&rpl, 0, sizeof(req));
  }
 end:
  memwipe(&req, 0, sizeof(req));
  memwipe(&rpl, 0, sizeof(req));
  release_server_onion_keys(&onion_keys);
  tor_close_socket(fd);
  crypto_thread_cleanup();
  spawn_exit();
}

/** Launch a new cpuworker. Return 0 if we're happy, -1 if we failed.
 */
static int
spawn_cpuworker(void)
{
  tor_socket_t *fdarray;
  tor_socket_t fd;
  connection_t *conn;
  int err;

  fdarray = tor_malloc(sizeof(tor_socket_t)*2);
  if ((err = tor_socketpair(AF_UNIX, SOCK_STREAM, 0, fdarray)) < 0) {
    log_warn(LD_NET, "Couldn't construct socketpair for cpuworker: %s",
             tor_socket_strerror(-err));
    tor_free(fdarray);
    return -1;
  }

  tor_assert(SOCKET_OK(fdarray[0]));
  tor_assert(SOCKET_OK(fdarray[1]));

  fd = fdarray[0];
  if (spawn_func(cpuworker_main, (void*)fdarray) < 0) {
    tor_close_socket(fdarray[0]);
    tor_close_socket(fdarray[1]);
    tor_free(fdarray);
    return -1;
  }
  log_debug(LD_OR,"just spawned a cpu worker.");
#ifndef TOR_IS_MULTITHREADED
  tor_close_socket(fdarray[1]); /* don't need the worker's side of the pipe */
  tor_free(fdarray);
#endif

  conn = connection_new(CONN_TYPE_CPUWORKER, AF_UNIX);

  /* set up conn so it's got all the data we need to remember */
  conn->s = fd;
  conn->address = tor_strdup("localhost");
  tor_addr_make_unspec(&conn->addr);

  if (set_socket_nonblocking(fd) == -1) {
    connection_free(conn); /* this closes fd */
    return -1;
  }

  if (connection_add(conn) < 0) { /* no space, forget it */
    log_warn(LD_NET,"connection_add for cpuworker failed. Giving up.");
    connection_free(conn); /* this closes fd */
    return -1;
  }

  conn->state = CPUWORKER_STATE_IDLE;
  connection_start_reading(conn);

  return 0; /* success */
}

/** If we have too few or too many active cpuworkers, try to spawn new ones
 * or kill idle ones.
 */
static void
spawn_enough_cpuworkers(void)
{
  int num_cpuworkers_needed = get_num_cpus(get_options());
  int reseed = 0;

  if (num_cpuworkers_needed < MIN_CPUWORKERS)
    num_cpuworkers_needed = MIN_CPUWORKERS;
  if (num_cpuworkers_needed > MAX_CPUWORKERS)
    num_cpuworkers_needed = MAX_CPUWORKERS;

  while (num_cpuworkers < num_cpuworkers_needed) {
    if (spawn_cpuworker() < 0) {
      log_warn(LD_GENERAL,"Cpuworker spawn failed. Will try again later.");
      return;
    }
    num_cpuworkers++;
    reseed++;
  }

  if (reseed)
    crypto_seed_weak_rng(&request_sample_rng);
}

/** Take a pending task from the queue and assign it to 'cpuworker'. */
static void
process_pending_task(connection_t *cpuworker)
{
  or_circuit_t *circ;
  create_cell_t *onionskin = NULL;

  tor_assert(cpuworker);

  /* for now only process onion tasks */

  circ = onion_next_task(&onionskin);
  if (!circ)
    return;
  if (assign_onionskin_to_cpuworker(cpuworker, circ, onionskin))
    log_warn(LD_OR,"assign_to_cpuworker failed. Ignoring.");
}

/** How long should we let a cpuworker stay busy before we give
 * up on it and decide that we have a bug or infinite loop?
 * This value is high because some servers with low memory/cpu
 * sometimes spend an hour or more swapping, and Tor starves. */
#define CPUWORKER_BUSY_TIMEOUT (60*60*12)

/** We have a bug that I can't find. Sometimes, very rarely, cpuworkers get
 * stuck in the 'busy' state, even though the cpuworker process thinks of
 * itself as idle. I don't know why. But here's a workaround to kill any
 * cpuworker that's been busy for more than CPUWORKER_BUSY_TIMEOUT.
 */
static void
cull_wedged_cpuworkers(void)
{
  time_t now = time(NULL);
  smartlist_t *conns = get_connection_array();
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (!conn->marked_for_close &&
        conn->type == CONN_TYPE_CPUWORKER &&
        conn->state == CPUWORKER_STATE_BUSY_ONION &&
        conn->timestamp_lastwritten + CPUWORKER_BUSY_TIMEOUT < now) {
      log_notice(LD_BUG,
                 "closing wedged cpuworker. Can somebody find the bug?");
      num_cpuworkers_busy--;
      num_cpuworkers--;
      connection_mark_for_close(conn);
    }
  } SMARTLIST_FOREACH_END(conn);
}

/** Try to tell a cpuworker to perform the public key operations necessary to
 * respond to <b>onionskin</b> for the circuit <b>circ</b>.
 *
 * If <b>cpuworker</b> is defined, assert that he's idle, and use him. Else,
 * look for an idle cpuworker and use him. If none idle, queue task onto the
 * pending onion list and return.  Return 0 if we successfully assign the
 * task, or -1 on failure.
 */
int
assign_onionskin_to_cpuworker(connection_t *cpuworker,
                              or_circuit_t *circ,
                              create_cell_t *onionskin)
{
  cpuworker_request_t req;
  time_t now = approx_time();
  static time_t last_culled_cpuworkers = 0;
  int should_time;

  /* Checking for wedged cpuworkers requires a linear search over all
   * connections, so let's do it only once a minute.
   */
#define CULL_CPUWORKERS_INTERVAL 60

  if (last_culled_cpuworkers + CULL_CPUWORKERS_INTERVAL <= now) {
    cull_wedged_cpuworkers();
    spawn_enough_cpuworkers();
    last_culled_cpuworkers = now;
  }

  if (1) {
    if (num_cpuworkers_busy == num_cpuworkers) {
      log_debug(LD_OR,"No idle cpuworkers. Queuing.");
      if (onion_pending_add(circ, onionskin) < 0) {
        tor_free(onionskin);
        return -1;
      }
      return 0;
    }

    if (!cpuworker)
      cpuworker = connection_get_by_type_state(CONN_TYPE_CPUWORKER,
                                               CPUWORKER_STATE_IDLE);

    tor_assert(cpuworker);

    if (!circ->p_chan) {
      log_info(LD_OR,"circ->p_chan gone. Failing circ.");
      tor_free(onionskin);
      return -1;
    }

    if (connection_or_digest_is_known_relay(circ->p_chan->identity_digest))
      rep_hist_note_circuit_handshake_assigned(onionskin->handshake_type);

    should_time = should_time_request(onionskin->handshake_type);
    memset(&req, 0, sizeof(req));
    req.magic = CPUWORKER_REQUEST_MAGIC;
    tag_pack(req.tag, circ->p_chan->global_identifier,
             circ->p_circ_id);
    req.timed = should_time;

    cpuworker->state = CPUWORKER_STATE_BUSY_ONION;
    /* touch the lastwritten timestamp, since that's how we check to
     * see how long it's been since we asked the question, and sometimes
     * we check before the first call to connection_handle_write(). */
    cpuworker->timestamp_lastwritten = now;
    num_cpuworkers_busy++;

    req.task = CPUWORKER_TASK_ONION;
    memcpy(&req.create_cell, onionskin, sizeof(create_cell_t));

    tor_free(onionskin);

    if (should_time)
      tor_gettimeofday(&req.started_at);

    connection_write_to_buf((void*)&req, sizeof(req), cpuworker);
    memwipe(&req, 0, sizeof(req));
  }
  return 0;
}

